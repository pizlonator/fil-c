/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "vk_descriptors.h"
#include "vk_log.h"

#include "panvk_descriptor_set.h"
#include "panvk_descriptor_set_layout.h"
#include "panvk_device.h"
#include "panvk_entrypoints.h"
#include "panvk_pipeline_layout.h"
#include "panvk_sampler.h"

#define PANVK_DESCRIPTOR_ALIGN 8

/* FIXME: make sure those values are correct */
#define PANVK_MAX_TEXTURES (1 << 16)
#define PANVK_MAX_IMAGES   (1 << 8)
#define PANVK_MAX_SAMPLERS (1 << 16)
#define PANVK_MAX_UBOS     255

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(GetDescriptorSetLayoutSupport)(
   VkDevice _device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
   VkDescriptorSetLayoutSupport *pSupport)
{
   VK_FROM_HANDLE(panvk_device, device, _device);

   pSupport->supported = false;

   VkDescriptorSetLayoutBinding *bindings;
   VkResult result = vk_create_sorted_bindings(
      pCreateInfo->pBindings, pCreateInfo->bindingCount, &bindings);
   if (result != VK_SUCCESS) {
      vk_error(device, result);
      return;
   }

   unsigned sampler_idx = 0, tex_idx = 0, ubo_idx = 0;
   unsigned img_idx = 0;
   UNUSED unsigned dynoffset_idx = 0;

   for (unsigned i = 0; i < pCreateInfo->bindingCount; i++) {
      const VkDescriptorSetLayoutBinding *binding = &bindings[i];

      switch (binding->descriptorType) {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
         sampler_idx += binding->descriptorCount;
         break;
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         sampler_idx += binding->descriptorCount;
         tex_idx += binding->descriptorCount;
         break;
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
         tex_idx += binding->descriptorCount;
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
         dynoffset_idx += binding->descriptorCount;
         FALLTHROUGH;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
         ubo_idx += binding->descriptorCount;
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         dynoffset_idx += binding->descriptorCount;
         FALLTHROUGH;
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
         img_idx += binding->descriptorCount;
         break;
      default:
         unreachable("Invalid descriptor type");
      }
   }

   /* The maximum values apply to all sets attached to a pipeline since all
    * sets descriptors have to be merged in a single array.
    */
   if (tex_idx > PANVK_MAX_TEXTURES / MAX_SETS ||
       sampler_idx > PANVK_MAX_SAMPLERS / MAX_SETS ||
       ubo_idx > PANVK_MAX_UBOS / MAX_SETS ||
       img_idx > PANVK_MAX_IMAGES / MAX_SETS)
      return;

   pSupport->supported = true;
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_per_arch(CreateDescriptorSetLayout)(
   VkDevice _device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
   const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   struct panvk_descriptor_set_layout *set_layout;
   VkDescriptorSetLayoutBinding *bindings = NULL;
   unsigned num_bindings = 0;
   VkResult result;

   if (pCreateInfo->bindingCount) {
      result = vk_create_sorted_bindings(pCreateInfo->pBindings,
                                         pCreateInfo->bindingCount, &bindings);
      if (result != VK_SUCCESS)
         return vk_error(device, result);

      num_bindings = bindings[pCreateInfo->bindingCount - 1].binding + 1;
   }

   unsigned num_immutable_samplers = 0;
   for (unsigned i = 0; i < pCreateInfo->bindingCount; i++) {
      if (bindings[i].pImmutableSamplers)
         num_immutable_samplers += bindings[i].descriptorCount;
   }

   size_t size =
      sizeof(*set_layout) +
      (sizeof(struct panvk_descriptor_set_binding_layout) * num_bindings) +
      (sizeof(struct panvk_sampler *) * num_immutable_samplers);
   set_layout = vk_descriptor_set_layout_zalloc(&device->vk, size);
   if (!set_layout) {
      result = VK_ERROR_OUT_OF_HOST_MEMORY;
      goto err_free_bindings;
   }

   set_layout->flags = pCreateInfo->flags;

   struct panvk_sampler **immutable_samplers =
      (struct panvk_sampler **)((uint8_t *)set_layout + sizeof(*set_layout) +
                                (sizeof(
                                    struct panvk_descriptor_set_binding_layout) *
                                 num_bindings));

   set_layout->binding_count = num_bindings;

   unsigned sampler_idx = 0, tex_idx = 0, ubo_idx = 0;
   unsigned dyn_ubo_idx = 0, dyn_ssbo_idx = 0, img_idx = 0;
   uint32_t desc_ubo_size = 0;

   for (unsigned i = 0; i < pCreateInfo->bindingCount; i++) {
      const VkDescriptorSetLayoutBinding *binding = &bindings[i];
      struct panvk_descriptor_set_binding_layout *binding_layout =
         &set_layout->bindings[binding->binding];

      binding_layout->type = binding->descriptorType;
      binding_layout->array_size = binding->descriptorCount;
      binding_layout->shader_stages = binding->stageFlags;
      binding_layout->desc_ubo_stride = 0;
      if (binding->pImmutableSamplers) {
         binding_layout->immutable_samplers = immutable_samplers;
         immutable_samplers += binding_layout->array_size;
         for (unsigned j = 0; j < binding_layout->array_size; j++) {
            VK_FROM_HANDLE(panvk_sampler, sampler,
                           binding->pImmutableSamplers[j]);
            binding_layout->immutable_samplers[j] = sampler;
         }
      }

      switch (binding_layout->type) {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
         binding_layout->sampler_idx = sampler_idx;
         sampler_idx += binding_layout->array_size;
         break;
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         binding_layout->sampler_idx = sampler_idx;
         binding_layout->tex_idx = tex_idx;
         sampler_idx += binding_layout->array_size;
         tex_idx += binding_layout->array_size;
         binding_layout->desc_ubo_stride = sizeof(struct panvk_image_desc);
         break;
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
         binding_layout->tex_idx = tex_idx;
         tex_idx += binding_layout->array_size;
         binding_layout->desc_ubo_stride = sizeof(struct panvk_image_desc);
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
         binding_layout->tex_idx = tex_idx;
         tex_idx += binding_layout->array_size;
         binding_layout->desc_ubo_stride = sizeof(struct panvk_bview_desc);
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
         binding_layout->dyn_ubo_idx = dyn_ubo_idx;
         dyn_ubo_idx += binding_layout->array_size;
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
         binding_layout->ubo_idx = ubo_idx;
         ubo_idx += binding_layout->array_size;
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         binding_layout->dyn_ssbo_idx = dyn_ssbo_idx;
         dyn_ssbo_idx += binding_layout->array_size;
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
         binding_layout->desc_ubo_stride = sizeof(struct panvk_ssbo_addr);
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
         binding_layout->img_idx = img_idx;
         img_idx += binding_layout->array_size;
         binding_layout->desc_ubo_stride = sizeof(struct panvk_image_desc);
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
         binding_layout->img_idx = img_idx;
         img_idx += binding_layout->array_size;
         binding_layout->desc_ubo_stride = sizeof(struct panvk_bview_desc);
         break;
      default:
         unreachable("Invalid descriptor type");
      }

      desc_ubo_size = ALIGN_POT(desc_ubo_size, PANVK_DESCRIPTOR_ALIGN);
      binding_layout->desc_ubo_offset = desc_ubo_size;
      desc_ubo_size +=
         binding_layout->desc_ubo_stride * binding_layout->array_size;
   }

   set_layout->desc_ubo_size = desc_ubo_size;
   if (desc_ubo_size > 0)
      set_layout->desc_ubo_index = ubo_idx++;

   set_layout->num_samplers = sampler_idx;
   set_layout->num_textures = tex_idx;
   set_layout->num_ubos = ubo_idx;
   set_layout->num_dyn_ubos = dyn_ubo_idx;
   set_layout->num_dyn_ssbos = dyn_ssbo_idx;
   set_layout->num_imgs = img_idx;

   free(bindings);
   *pSetLayout = panvk_descriptor_set_layout_to_handle(set_layout);
   return VK_SUCCESS;

err_free_bindings:
   free(bindings);
   return vk_error(device, result);
}
