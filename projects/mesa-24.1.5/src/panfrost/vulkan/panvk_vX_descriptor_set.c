/*
 * Copyright © 2021 Collabora Ltd.
 *
 * Derived from:
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "genxml/gen_macros.h"

#include "panvk_buffer_view.h"
#include "panvk_device.h"
#include "panvk_entrypoints.h"
#include "panvk_image.h"
#include "panvk_image_view.h"
#include "panvk_priv_bo.h"

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "util/mesa-sha1.h"
#include "vk_alloc.h"
#include "vk_descriptor_update_template.h"
#include "vk_descriptors.h"
#include "vk_format.h"
#include "vk_log.h"
#include "vk_util.h"

#include "panvk_buffer.h"
#include "panvk_descriptor_set.h"
#include "panvk_descriptor_set_layout.h"
#include "panvk_sampler.h"

VKAPI_ATTR VkResult VKAPI_CALL
panvk_per_arch(CreateDescriptorPool)(
   VkDevice _device, const VkDescriptorPoolCreateInfo *pCreateInfo,
   const VkAllocationCallbacks *pAllocator, VkDescriptorPool *pDescriptorPool)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   struct panvk_descriptor_pool *pool;

   pool = vk_object_zalloc(&device->vk, pAllocator,
                           sizeof(struct panvk_descriptor_pool),
                           VK_OBJECT_TYPE_DESCRIPTOR_POOL);
   if (!pool)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   pool->max.sets = pCreateInfo->maxSets;

   for (unsigned i = 0; i < pCreateInfo->poolSizeCount; ++i) {
      unsigned desc_count = pCreateInfo->pPoolSizes[i].descriptorCount;

      switch (pCreateInfo->pPoolSizes[i].type) {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
         pool->max.samplers += desc_count;
         break;
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         pool->max.combined_image_samplers += desc_count;
         break;
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
         pool->max.sampled_images += desc_count;
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
         pool->max.storage_images += desc_count;
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
         pool->max.uniform_texel_bufs += desc_count;
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
         pool->max.storage_texel_bufs += desc_count;
         break;
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
         pool->max.input_attachments += desc_count;
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
         pool->max.uniform_bufs += desc_count;
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
         pool->max.storage_bufs += desc_count;
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
         pool->max.uniform_dyn_bufs += desc_count;
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         pool->max.storage_dyn_bufs += desc_count;
         break;
      default:
         unreachable("Invalid descriptor type");
      }
   }

   *pDescriptorPool = panvk_descriptor_pool_to_handle(pool);
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(DestroyDescriptorPool)(VkDevice _device, VkDescriptorPool _pool,
                                      const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_descriptor_pool, pool, _pool);

   if (pool)
      vk_object_free(&device->vk, pAllocator, pool);
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_per_arch(ResetDescriptorPool)(VkDevice _device, VkDescriptorPool _pool,
                                    VkDescriptorPoolResetFlags flags)
{
   VK_FROM_HANDLE(panvk_descriptor_pool, pool, _pool);
   memset(&pool->cur, 0, sizeof(pool->cur));
   return VK_SUCCESS;
}

static void
panvk_descriptor_set_destroy(struct panvk_device *device,
                             struct panvk_descriptor_pool *pool,
                             struct panvk_descriptor_set *set)
{
   if (set->desc_ubo.bo)
      panvk_priv_bo_destroy(set->desc_ubo.bo, NULL);

   vk_object_free(&device->vk, NULL, set);
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_per_arch(FreeDescriptorSets)(VkDevice _device,
                                   VkDescriptorPool descriptorPool,
                                   uint32_t count,
                                   const VkDescriptorSet *pDescriptorSets)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_descriptor_pool, pool, descriptorPool);

   for (unsigned i = 0; i < count; i++) {
      VK_FROM_HANDLE(panvk_descriptor_set, set, pDescriptorSets[i]);

      if (set)
         panvk_descriptor_set_destroy(device, pool, set);
   }
   return VK_SUCCESS;
}

static void
panvk_fill_bview_desc(struct panvk_bview_desc *desc,
                      struct panvk_buffer_view *view)
{
   desc->elems = view->vk.elements;
}

static void
panvk_fill_image_desc(struct panvk_image_desc *desc,
                      struct panvk_image_view *view)
{
   desc->width = view->vk.extent.width - 1;
   desc->height = view->vk.extent.height - 1;
   desc->depth = view->vk.extent.depth - 1;
   desc->levels = view->vk.level_count;
   desc->samples = view->vk.image->samples;

   /* Stick array layer count after the last valid size component */
   if (view->vk.image->image_type == VK_IMAGE_TYPE_1D)
      desc->height = view->vk.layer_count - 1;
   else if (view->vk.image->image_type == VK_IMAGE_TYPE_2D)
      desc->depth = view->vk.layer_count - 1;
}

static void panvk_write_sampler_desc_raw(struct panvk_descriptor_set *set,
                                         uint32_t binding, uint32_t elem,
                                         struct panvk_sampler *sampler);

static struct panvk_descriptor_set *
panvk_descriptor_set_alloc(const struct panvk_descriptor_set_layout *layout,
                           const VkAllocationCallbacks *alloc,
                           VkSystemAllocationScope scope)
{
   VK_MULTIALLOC(ma);
   VK_MULTIALLOC_DECL(&ma, struct panvk_descriptor_set, set, 1);
   VK_MULTIALLOC_DECL(&ma, struct panvk_buffer_desc, dyn_ssbos,
                      layout->num_dyn_ssbos);
   VK_MULTIALLOC_DECL(&ma, struct mali_uniform_buffer_packed, ubos,
                      layout->num_ubos);
   VK_MULTIALLOC_DECL(&ma, struct panvk_buffer_desc, dyn_ubos,
                      layout->num_dyn_ubos);
   VK_MULTIALLOC_DECL(&ma, struct mali_sampler_packed, samplers,
                      layout->num_samplers);
   VK_MULTIALLOC_DECL(&ma, struct mali_texture_packed, textures,
                      layout->num_textures);
   VK_MULTIALLOC_DECL(&ma, struct mali_attribute_buffer_packed, img_attrib_bufs,
                      layout->num_imgs * 2);
   VK_MULTIALLOC_DECL(&ma, uint32_t, img_fmts, layout->num_imgs);

   if (!vk_multialloc_zalloc(&ma, alloc, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
      return NULL;

   set->layout = layout;

   if (layout->num_dyn_ssbos)
      set->dyn_ssbos = dyn_ssbos;

   if (layout->num_ubos)
      set->ubos = ubos;

   if (layout->num_dyn_ubos)
      set->dyn_ubos = dyn_ubos;

   if (layout->num_samplers)
      set->samplers = samplers;

   if (layout->num_textures)
      set->textures = textures;

   if (layout->num_imgs) {
      set->img_attrib_bufs = img_attrib_bufs;
      set->img_fmts = img_fmts;
   }

   return set;
}

static VkResult
panvk_per_arch(descriptor_set_create)(
   struct panvk_device *device, struct panvk_descriptor_pool *pool,
   const struct panvk_descriptor_set_layout *layout,
   struct panvk_descriptor_set **out_set)
{
   /* TODO: Allocate from the pool! */
   struct panvk_descriptor_set *set = panvk_descriptor_set_alloc(
      layout, &device->vk.alloc, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!set)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_object_base_init(&device->vk, &set->base, VK_OBJECT_TYPE_DESCRIPTOR_SET);

   if (layout->desc_ubo_size) {
      set->desc_ubo.bo =
         panvk_priv_bo_create(device, layout->desc_ubo_size, 0, NULL,
                              VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      if (!set->desc_ubo.bo)
         goto err_free_set;

      struct mali_uniform_buffer_packed *ubos = set->ubos;

      set->desc_ubo.addr.dev = set->desc_ubo.bo->addr.dev;
      set->desc_ubo.addr.host = set->desc_ubo.bo->addr.host;
      pan_pack(&ubos[layout->desc_ubo_index], UNIFORM_BUFFER, cfg) {
         cfg.pointer = set->desc_ubo.addr.dev;
         cfg.entries = DIV_ROUND_UP(layout->desc_ubo_size, 16);
      }
   }

   for (unsigned i = 0; i < layout->binding_count; i++) {
      if (!layout->bindings[i].immutable_samplers)
         continue;

      for (unsigned j = 0; j < layout->bindings[i].array_size; j++) {
         struct panvk_sampler *sampler =
            layout->bindings[i].immutable_samplers[j];
         panvk_write_sampler_desc_raw(set, i, j, sampler);
      }
   }

   *out_set = set;
   return VK_SUCCESS;

err_free_set:
   if (set->desc_ubo.bo)
      panvk_priv_bo_destroy(set->desc_ubo.bo, NULL);
   vk_object_free(&device->vk, NULL, set);
   return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_per_arch(AllocateDescriptorSets)(
   VkDevice _device, const VkDescriptorSetAllocateInfo *pAllocateInfo,
   VkDescriptorSet *pDescriptorSets)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_descriptor_pool, pool, pAllocateInfo->descriptorPool);
   VkResult result;
   unsigned i;

   for (i = 0; i < pAllocateInfo->descriptorSetCount; i++) {
      VK_FROM_HANDLE(panvk_descriptor_set_layout, layout,
                     pAllocateInfo->pSetLayouts[i]);
      struct panvk_descriptor_set *set = NULL;

      result =
         panvk_per_arch(descriptor_set_create)(device, pool, layout, &set);
      if (result != VK_SUCCESS)
         goto err_free_sets;

      pDescriptorSets[i] = panvk_descriptor_set_to_handle(set);
   }

   return VK_SUCCESS;

err_free_sets:
   panvk_FreeDescriptorSets(_device, pAllocateInfo->descriptorPool, i,
                            pDescriptorSets);
   for (i = 0; i < pAllocateInfo->descriptorSetCount; i++)
      pDescriptorSets[i] = VK_NULL_HANDLE;

   return result;
}

static void *
panvk_desc_ubo_data(struct panvk_descriptor_set *set, uint32_t binding,
                    uint32_t elem)
{
   const struct panvk_descriptor_set_binding_layout *binding_layout =
      &set->layout->bindings[binding];

   return (char *)set->desc_ubo.addr.host + binding_layout->desc_ubo_offset +
          elem * binding_layout->desc_ubo_stride;
}

static struct mali_sampler_packed *
panvk_sampler_desc(struct panvk_descriptor_set *set, uint32_t binding,
                   uint32_t elem)
{
   const struct panvk_descriptor_set_binding_layout *binding_layout =
      &set->layout->bindings[binding];

   uint32_t sampler_idx = binding_layout->sampler_idx + elem;

   return &((struct mali_sampler_packed *)set->samplers)[sampler_idx];
}

static void
panvk_write_sampler_desc_raw(struct panvk_descriptor_set *set, uint32_t binding,
                             uint32_t elem, struct panvk_sampler *sampler)
{
   memcpy(panvk_sampler_desc(set, binding, elem), &sampler->desc,
          sizeof(sampler->desc));
}

static void
panvk_write_sampler_desc(struct panvk_descriptor_set *set, uint32_t binding,
                         uint32_t elem,
                         const VkDescriptorImageInfo *const pImageInfo)
{
   const struct panvk_descriptor_set_binding_layout *binding_layout =
      &set->layout->bindings[binding];
   bool push_set = set->layout->flags &
                   VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

   if (binding_layout->immutable_samplers && !push_set)
      return;

   struct panvk_sampler *sampler =
      binding_layout->immutable_samplers
         ? binding_layout->immutable_samplers[elem]
         : panvk_sampler_from_handle(pImageInfo->sampler);

   panvk_write_sampler_desc_raw(set, binding, elem, sampler);
}

static void
panvk_copy_sampler_desc(struct panvk_descriptor_set *dst_set,
                        uint32_t dst_binding, uint32_t dst_elem,
                        struct panvk_descriptor_set *src_set,
                        uint32_t src_binding, uint32_t src_elem)
{
   const struct panvk_descriptor_set_binding_layout *dst_binding_layout =
      &dst_set->layout->bindings[dst_binding];

   if (dst_binding_layout->immutable_samplers)
      return;

   memcpy(panvk_sampler_desc(dst_set, dst_binding, dst_elem),
          panvk_sampler_desc(src_set, src_binding, src_elem),
          sizeof(struct mali_sampler_packed));
}

static struct mali_texture_packed *
panvk_tex_desc(struct panvk_descriptor_set *set, uint32_t binding,
               uint32_t elem)
{
   const struct panvk_descriptor_set_binding_layout *binding_layout =
      &set->layout->bindings[binding];

   unsigned tex_idx = binding_layout->tex_idx + elem;

   return &((struct mali_texture_packed *)set->textures)[tex_idx];
}

static void
panvk_write_tex_desc(struct panvk_descriptor_set *set, uint32_t binding,
                     uint32_t elem,
                     const VkDescriptorImageInfo *const pImageInfo)
{
   VK_FROM_HANDLE(panvk_image_view, view, pImageInfo->imageView);

   memcpy(panvk_tex_desc(set, binding, elem), view->descs.tex.opaque,
          pan_size(TEXTURE));

   panvk_fill_image_desc(panvk_desc_ubo_data(set, binding, elem), view);
}

static void
panvk_copy_tex_desc(struct panvk_descriptor_set *dst_set, uint32_t dst_binding,
                    uint32_t dst_elem, struct panvk_descriptor_set *src_set,
                    uint32_t src_binding, uint32_t src_elem)
{
   *panvk_tex_desc(dst_set, dst_binding, dst_elem) =
      *panvk_tex_desc(src_set, src_binding, src_elem);

   /* Descriptor UBO data gets copied automatically */
}

static void
panvk_write_tex_buf_desc(struct panvk_descriptor_set *set, uint32_t binding,
                         uint32_t elem, const VkBufferView bufferView)
{
   VK_FROM_HANDLE(panvk_buffer_view, view, bufferView);

   memcpy(panvk_tex_desc(set, binding, elem), view->descs.tex.opaque,
          pan_size(TEXTURE));

   panvk_fill_bview_desc(panvk_desc_ubo_data(set, binding, elem), view);
}

static uint32_t
panvk_img_idx(struct panvk_descriptor_set *set, uint32_t binding, uint32_t elem)
{
   const struct panvk_descriptor_set_binding_layout *binding_layout =
      &set->layout->bindings[binding];

   return binding_layout->img_idx + elem;
}

static void
panvk_write_img_desc(struct panvk_descriptor_set *set, uint32_t binding,
                     uint32_t elem, const VkDescriptorImageInfo *pImageInfo)
{
   VK_FROM_HANDLE(panvk_image_view, view, pImageInfo->imageView);

   unsigned img_idx = panvk_img_idx(set, binding, elem);
   void *attrib_buf = (uint8_t *)set->img_attrib_bufs +
                      (pan_size(ATTRIBUTE_BUFFER) * 2 * img_idx);

   set->img_fmts[img_idx] =
      GENX(panfrost_format_from_pipe_format)(view->pview.format)->hw;
   memcpy(attrib_buf, view->descs.img_attrib_buf,
          pan_size(ATTRIBUTE_BUFFER) * 2);

   panvk_fill_image_desc(panvk_desc_ubo_data(set, binding, elem), view);
}

static void
panvk_copy_img_desc(struct panvk_descriptor_set *dst_set, uint32_t dst_binding,
                    uint32_t dst_elem, struct panvk_descriptor_set *src_set,
                    uint32_t src_binding, uint32_t src_elem)
{
   unsigned dst_img_idx = panvk_img_idx(dst_set, dst_binding, dst_elem);
   unsigned src_img_idx = panvk_img_idx(src_set, src_binding, src_elem);

   void *dst_attrib_buf = (uint8_t *)dst_set->img_attrib_bufs +
                          (pan_size(ATTRIBUTE_BUFFER) * 2 * dst_img_idx);
   void *src_attrib_buf = (uint8_t *)src_set->img_attrib_bufs +
                          (pan_size(ATTRIBUTE_BUFFER) * 2 * src_img_idx);

   dst_set->img_fmts[dst_img_idx] = src_set->img_fmts[src_img_idx];
   memcpy(dst_attrib_buf, src_attrib_buf, pan_size(ATTRIBUTE_BUFFER) * 2);

   /* Descriptor UBO data gets copied automatically */
}

static void
panvk_write_img_buf_desc(struct panvk_descriptor_set *set, uint32_t binding,
                         uint32_t elem, const VkBufferView bufferView)
{
   VK_FROM_HANDLE(panvk_buffer_view, view, bufferView);

   unsigned img_idx = panvk_img_idx(set, binding, elem);
   void *attrib_buf = (uint8_t *)set->img_attrib_bufs +
                      (pan_size(ATTRIBUTE_BUFFER) * 2 * img_idx);
   enum pipe_format pfmt = vk_format_to_pipe_format(view->vk.format);

   set->img_fmts[img_idx] = GENX(panfrost_format_from_pipe_format)(pfmt)->hw;
   memcpy(attrib_buf, view->descs.img_attrib_buf,
          pan_size(ATTRIBUTE_BUFFER) * 2);

   panvk_fill_bview_desc(panvk_desc_ubo_data(set, binding, elem), view);
}

static struct mali_uniform_buffer_packed *
panvk_ubo_desc(struct panvk_descriptor_set *set, uint32_t binding,
               uint32_t elem)
{
   const struct panvk_descriptor_set_binding_layout *binding_layout =
      &set->layout->bindings[binding];

   unsigned ubo_idx = binding_layout->ubo_idx + elem;

   return &((struct mali_uniform_buffer_packed *)set->ubos)[ubo_idx];
}

static void
panvk_write_ubo_desc(struct panvk_descriptor_set *set, uint32_t binding,
                     uint32_t elem, const VkDescriptorBufferInfo *pBufferInfo)
{
   VK_FROM_HANDLE(panvk_buffer, buffer, pBufferInfo->buffer);

   mali_ptr ptr = panvk_buffer_gpu_ptr(buffer, pBufferInfo->offset);
   size_t size =
      panvk_buffer_range(buffer, pBufferInfo->offset, pBufferInfo->range);

   pan_pack(panvk_ubo_desc(set, binding, elem), UNIFORM_BUFFER, cfg) {
      cfg.pointer = ptr;
      cfg.entries = DIV_ROUND_UP(size, 16);
   }
}

static void
panvk_copy_ubo_desc(struct panvk_descriptor_set *dst_set, uint32_t dst_binding,
                    uint32_t dst_elem, struct panvk_descriptor_set *src_set,
                    uint32_t src_binding, uint32_t src_elem)
{
   *panvk_ubo_desc(dst_set, dst_binding, dst_elem) =
      *panvk_ubo_desc(src_set, src_binding, src_elem);
}

static struct panvk_buffer_desc *
panvk_dyn_ubo_desc(struct panvk_descriptor_set *set, uint32_t binding,
                   uint32_t elem)
{
   const struct panvk_descriptor_set_binding_layout *binding_layout =
      &set->layout->bindings[binding];

   return &set->dyn_ubos[binding_layout->dyn_ubo_idx + elem];
}

static void
panvk_write_dyn_ubo_desc(struct panvk_descriptor_set *set, uint32_t binding,
                         uint32_t elem,
                         const VkDescriptorBufferInfo *pBufferInfo)
{
   VK_FROM_HANDLE(panvk_buffer, buffer, pBufferInfo->buffer);

   *panvk_dyn_ubo_desc(set, binding, elem) = (struct panvk_buffer_desc){
      .buffer = buffer,
      .offset = pBufferInfo->offset,
      .size = pBufferInfo->range,
   };
}

static void
panvk_copy_dyn_ubo_desc(struct panvk_descriptor_set *dst_set,
                        uint32_t dst_binding, uint32_t dst_elem,
                        struct panvk_descriptor_set *src_set,
                        uint32_t src_binding, uint32_t src_elem)
{
   *panvk_dyn_ubo_desc(dst_set, dst_binding, dst_elem) =
      *panvk_dyn_ubo_desc(src_set, src_binding, src_elem);
}

static void
panvk_write_ssbo_desc(struct panvk_descriptor_set *set, uint32_t binding,
                      uint32_t elem, const VkDescriptorBufferInfo *pBufferInfo)
{
   VK_FROM_HANDLE(panvk_buffer, buffer, pBufferInfo->buffer);

   struct panvk_ssbo_addr *desc = panvk_desc_ubo_data(set, binding, elem);
   *desc = (struct panvk_ssbo_addr){
      .base_addr = panvk_buffer_gpu_ptr(buffer, pBufferInfo->offset),
      .size =
         panvk_buffer_range(buffer, pBufferInfo->offset, pBufferInfo->range),
   };
}

static void
panvk_copy_ssbo_desc(struct panvk_descriptor_set *dst_set, uint32_t dst_binding,
                     uint32_t dst_elem, struct panvk_descriptor_set *src_set,
                     uint32_t src_binding, uint32_t src_elem)
{
   /* Descriptor UBO data gets copied automatically */
}

static struct panvk_buffer_desc *
panvk_dyn_ssbo_desc(struct panvk_descriptor_set *set, uint32_t binding,
                    uint32_t elem)
{
   const struct panvk_descriptor_set_binding_layout *binding_layout =
      &set->layout->bindings[binding];

   return &set->dyn_ssbos[binding_layout->dyn_ssbo_idx + elem];
}

static void
panvk_write_dyn_ssbo_desc(struct panvk_descriptor_set *set, uint32_t binding,
                          uint32_t elem,
                          const VkDescriptorBufferInfo *pBufferInfo)
{
   VK_FROM_HANDLE(panvk_buffer, buffer, pBufferInfo->buffer);

   *panvk_dyn_ssbo_desc(set, binding, elem) = (struct panvk_buffer_desc){
      .buffer = buffer,
      .offset = pBufferInfo->offset,
      .size = pBufferInfo->range,
   };
}

static void
panvk_copy_dyn_ssbo_desc(struct panvk_descriptor_set *dst_set,
                         uint32_t dst_binding, uint32_t dst_elem,
                         struct panvk_descriptor_set *src_set,
                         uint32_t src_binding, uint32_t src_elem)
{
   *panvk_dyn_ssbo_desc(dst_set, dst_binding, dst_elem) =
      *panvk_dyn_ssbo_desc(src_set, src_binding, src_elem);
}

static void
panvk_descriptor_set_write(struct panvk_descriptor_set *set,
                           const VkWriteDescriptorSet *write)
{
   switch (write->descriptorType) {
   case VK_DESCRIPTOR_TYPE_SAMPLER:
      for (uint32_t j = 0; j < write->descriptorCount; j++) {
         panvk_write_sampler_desc(set, write->dstBinding,
                                  write->dstArrayElement + j,
                                  &write->pImageInfo[j]);
      }
      break;

   case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      for (uint32_t j = 0; j < write->descriptorCount; j++) {
         panvk_write_sampler_desc(set, write->dstBinding,
                                  write->dstArrayElement + j,
                                  &write->pImageInfo[j]);
         panvk_write_tex_desc(set, write->dstBinding,
                              write->dstArrayElement + j,
                              &write->pImageInfo[j]);
      }
      break;

   case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
   case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      for (uint32_t j = 0; j < write->descriptorCount; j++) {
         panvk_write_tex_desc(set, write->dstBinding,
                              write->dstArrayElement + j,
                              &write->pImageInfo[j]);
      }
      break;

   case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      for (uint32_t j = 0; j < write->descriptorCount; j++) {
         panvk_write_img_desc(set, write->dstBinding,
                              write->dstArrayElement + j,
                              &write->pImageInfo[j]);
      }
      break;

   case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      for (uint32_t j = 0; j < write->descriptorCount; j++) {
         panvk_write_tex_buf_desc(set, write->dstBinding,
                                  write->dstArrayElement + j,
                                  write->pTexelBufferView[j]);
      }
      break;

   case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      for (uint32_t j = 0; j < write->descriptorCount; j++) {
         panvk_write_img_buf_desc(set, write->dstBinding,
                                  write->dstArrayElement + j,
                                  write->pTexelBufferView[j]);
      }
      break;

   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      for (uint32_t j = 0; j < write->descriptorCount; j++) {
         panvk_write_ubo_desc(set, write->dstBinding,
                              write->dstArrayElement + j,
                              &write->pBufferInfo[j]);
      }
      break;

   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      for (uint32_t j = 0; j < write->descriptorCount; j++) {
         panvk_write_dyn_ubo_desc(set, write->dstBinding,
                                  write->dstArrayElement + j,
                                  &write->pBufferInfo[j]);
      }
      break;

   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      for (uint32_t j = 0; j < write->descriptorCount; j++) {
         panvk_write_ssbo_desc(set, write->dstBinding,
                               write->dstArrayElement + j,
                               &write->pBufferInfo[j]);
      }
      break;

   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      for (uint32_t j = 0; j < write->descriptorCount; j++) {
         panvk_write_dyn_ssbo_desc(set, write->dstBinding,
                                   write->dstArrayElement + j,
                                   &write->pBufferInfo[j]);
      }
      break;

   default:
      unreachable("Unsupported descriptor type");
   }
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(UpdateDescriptorSets)(
   VkDevice _device, uint32_t descriptorWriteCount,
   const VkWriteDescriptorSet *pDescriptorWrites, uint32_t descriptorCopyCount,
   const VkCopyDescriptorSet *pDescriptorCopies)
{
   for (unsigned i = 0; i < descriptorWriteCount; i++) {
      const VkWriteDescriptorSet *write = &pDescriptorWrites[i];
      VK_FROM_HANDLE(panvk_descriptor_set, set, write->dstSet);

      panvk_descriptor_set_write(set, write);
   }

   for (unsigned i = 0; i < descriptorCopyCount; i++) {
      const VkCopyDescriptorSet *copy = &pDescriptorCopies[i];
      VK_FROM_HANDLE(panvk_descriptor_set, src_set, copy->srcSet);
      VK_FROM_HANDLE(panvk_descriptor_set, dst_set, copy->dstSet);

      const struct panvk_descriptor_set_binding_layout *dst_binding_layout =
         &dst_set->layout->bindings[copy->dstBinding];
      const struct panvk_descriptor_set_binding_layout *src_binding_layout =
         &src_set->layout->bindings[copy->srcBinding];

      assert(dst_binding_layout->type == src_binding_layout->type);

      if (dst_binding_layout->desc_ubo_stride > 0 &&
          src_binding_layout->desc_ubo_stride > 0) {
         for (uint32_t j = 0; j < copy->descriptorCount; j++) {
            memcpy(panvk_desc_ubo_data(dst_set, copy->dstBinding,
                                       copy->dstArrayElement + j),
                   panvk_desc_ubo_data(src_set, copy->srcBinding,
                                       copy->srcArrayElement + j),
                   MIN2(dst_binding_layout->desc_ubo_stride,
                        src_binding_layout->desc_ubo_stride));
         }
      }

      switch (src_binding_layout->type) {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
         for (uint32_t j = 0; j < copy->descriptorCount; j++) {
            panvk_copy_sampler_desc(
               dst_set, copy->dstBinding, copy->dstArrayElement + j, src_set,
               copy->srcBinding, copy->srcArrayElement + j);
         }
         break;

      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         for (uint32_t j = 0; j < copy->descriptorCount; j++) {
            panvk_copy_sampler_desc(
               dst_set, copy->dstBinding, copy->dstArrayElement + j, src_set,
               copy->srcBinding, copy->srcArrayElement + j);
            panvk_copy_tex_desc(dst_set, copy->dstBinding,
                                copy->dstArrayElement + j, src_set,
                                copy->srcBinding, copy->srcArrayElement + j);
         }
         break;

      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
         for (uint32_t j = 0; j < copy->descriptorCount; j++) {
            panvk_copy_tex_desc(dst_set, copy->dstBinding,
                                copy->dstArrayElement + j, src_set,
                                copy->srcBinding, copy->srcArrayElement + j);
         }
         break;

      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
         for (uint32_t j = 0; j < copy->descriptorCount; j++) {
            panvk_copy_img_desc(dst_set, copy->dstBinding,
                                copy->dstArrayElement + j, src_set,
                                copy->srcBinding, copy->srcArrayElement + j);
         }
         break;

      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
         for (uint32_t j = 0; j < copy->descriptorCount; j++) {
            panvk_copy_ubo_desc(dst_set, copy->dstBinding,
                                copy->dstArrayElement + j, src_set,
                                copy->srcBinding, copy->srcArrayElement + j);
         }
         break;

      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
         for (uint32_t j = 0; j < copy->descriptorCount; j++) {
            panvk_copy_dyn_ubo_desc(
               dst_set, copy->dstBinding, copy->dstArrayElement + j, src_set,
               copy->srcBinding, copy->srcArrayElement + j);
         }
         break;

      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
         for (uint32_t j = 0; j < copy->descriptorCount; j++) {
            panvk_copy_ssbo_desc(dst_set, copy->dstBinding,
                                 copy->dstArrayElement + j, src_set,
                                 copy->srcBinding, copy->srcArrayElement + j);
         }
         break;

      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         for (uint32_t j = 0; j < copy->descriptorCount; j++) {
            panvk_copy_dyn_ssbo_desc(
               dst_set, copy->dstBinding, copy->dstArrayElement + j, src_set,
               copy->srcBinding, copy->srcArrayElement + j);
         }
         break;

      default:
         unreachable("Unsupported descriptor type");
      }
   }
}

static void
panvk_descriptor_set_update_with_template(struct panvk_descriptor_set *set,
                                          VkDescriptorUpdateTemplate templ,
                                          const void *data)
{
   VK_FROM_HANDLE(vk_descriptor_update_template, template, templ);

   for (uint32_t i = 0; i < template->entry_count; i++) {
      const struct vk_descriptor_template_entry *entry = &template->entries[i];

      switch (entry->type) {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
         for (unsigned j = 0; j < entry->array_count; j++) {
            const VkDescriptorImageInfo *info =
               data + entry->offset + j * entry->stride;

            if (entry->type == VK_DESCRIPTOR_TYPE_SAMPLER ||
                entry->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
               panvk_write_sampler_desc(set, entry->binding,
                                        entry->array_element + j, info);
            }

            if (entry->type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                entry->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                entry->type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
               panvk_write_tex_desc(set, entry->binding,
                                    entry->array_element + j, info);
            }
         }
         break;

      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
         for (unsigned j = 0; j < entry->array_count; j++) {
            const VkDescriptorImageInfo *info =
               data + entry->offset + j * entry->stride;

            panvk_write_img_desc(set, entry->binding, entry->array_element + j,
                                 info);
         }
         break;

      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
         for (unsigned j = 0; j < entry->array_count; j++) {
            const VkBufferView *view = data + entry->offset + j * entry->stride;

            panvk_write_tex_buf_desc(set, entry->binding,
                                     entry->array_element + j, *view);
         }
         break;

      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
         for (unsigned j = 0; j < entry->array_count; j++) {
            const VkBufferView *view = data + entry->offset + j * entry->stride;

            panvk_write_img_buf_desc(set, entry->binding,
                                     entry->array_element + j, *view);
         }
         break;

      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
         for (unsigned j = 0; j < entry->array_count; j++) {
            const VkDescriptorBufferInfo *info =
               data + entry->offset + j * entry->stride;

            panvk_write_ubo_desc(set, entry->binding, entry->array_element + j,
                                 info);
         }
         break;

      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
         for (unsigned j = 0; j < entry->array_count; j++) {
            const VkDescriptorBufferInfo *info =
               data + entry->offset + j * entry->stride;

            panvk_write_dyn_ubo_desc(set, entry->binding,
                                     entry->array_element + j, info);
         }
         break;

      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
         for (unsigned j = 0; j < entry->array_count; j++) {
            const VkDescriptorBufferInfo *info =
               data + entry->offset + j * entry->stride;

            panvk_write_ssbo_desc(set, entry->binding, entry->array_element + j,
                                  info);
         }
         break;

      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         for (unsigned j = 0; j < entry->array_count; j++) {
            const VkDescriptorBufferInfo *info =
               data + entry->offset + j * entry->stride;

            panvk_write_dyn_ssbo_desc(set, entry->binding,
                                      entry->array_element + j, info);
         }
         break;
      default:
         unreachable("Invalid type");
      }
   }
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(UpdateDescriptorSetWithTemplate)(
   VkDevice _device, VkDescriptorSet descriptorSet,
   VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *data)
{
   VK_FROM_HANDLE(panvk_descriptor_set, set, descriptorSet);

   panvk_descriptor_set_update_with_template(set, descriptorUpdateTemplate, data);
}

void
panvk_per_arch(push_descriptor_set_assign_layout)(
   struct panvk_push_descriptor_set *push_set,
   const struct panvk_descriptor_set_layout *layout)
{
   ASSERTED unsigned num_descs = layout->num_samplers + layout->num_textures +
                                 layout->num_ubos + layout->num_imgs;
   struct panvk_descriptor_set *set = &push_set->set;
   unsigned desc_offset = 0;

   set->layout = layout;
   assert(layout->num_dyn_ubos == 0);
   assert(layout->num_dyn_ssbos == 0);
   assert(num_descs <= PANVK_MAX_PUSH_DESCS);
   assert(layout->desc_ubo_size <= sizeof(push_set->storage.desc_ubo));

   if (layout->num_ubos) {
      set->ubos = (void *)(push_set->storage.descs + desc_offset);
      desc_offset += PANVK_MAX_DESC_SIZE * layout->num_ubos;
   }

   if (layout->num_samplers) {
      set->samplers = (void *)(push_set->storage.descs + desc_offset);
      desc_offset += PANVK_MAX_DESC_SIZE * layout->num_samplers;
   }

   if (layout->num_textures) {
      set->textures = (void *)(push_set->storage.descs + desc_offset);
      desc_offset += PANVK_MAX_DESC_SIZE * layout->num_textures;
   }

   if (layout->num_imgs) {
      set->img_attrib_bufs = (void *)(push_set->storage.descs + desc_offset);
      desc_offset += PANVK_MAX_DESC_SIZE * layout->num_imgs;
      set->img_fmts = push_set->storage.img_fmts;
   }

   if (layout->desc_ubo_size)
      set->desc_ubo.addr.host = push_set->storage.desc_ubo;
}

void
panvk_per_arch(push_descriptor_set)(
   struct panvk_push_descriptor_set *push_set,
   const struct panvk_descriptor_set_layout *layout,
   uint32_t write_count, const VkWriteDescriptorSet *writes)
{
   panvk_per_arch(push_descriptor_set_assign_layout)(push_set, layout);
   for (unsigned i = 0; i < write_count; i++) {
      const VkWriteDescriptorSet *write = &writes[i];

      panvk_descriptor_set_write(&push_set->set, write);
   }
}

void
panvk_per_arch(push_descriptor_set_with_template)(
   struct panvk_push_descriptor_set *push_set,
   const struct panvk_descriptor_set_layout *layout,
   VkDescriptorUpdateTemplate templ, const void *data)
{
   panvk_per_arch(push_descriptor_set_assign_layout)(push_set, layout);
   panvk_descriptor_set_update_with_template(&push_set->set, templ, data);
}
