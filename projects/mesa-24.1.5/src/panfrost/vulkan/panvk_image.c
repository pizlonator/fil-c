/*
 * Copyright © 2021 Collabora Ltd.
 *
 * Derived from tu_image.c which is:
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 * Copyright © 2015 Intel Corporation
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

#include "pan_props.h"

#include "panvk_device.h"
#include "panvk_device_memory.h"
#include "panvk_entrypoints.h"
#include "panvk_image.h"
#include "panvk_instance.h"
#include "panvk_physical_device.h"

#include "drm-uapi/drm_fourcc.h"
#include "util/u_atomic.h"
#include "util/u_debug.h"
#include "vk_format.h"
#include "vk_log.h"
#include "vk_object.h"
#include "vk_util.h"

#define PANVK_MAX_PLANES 1

static unsigned
panvk_image_get_total_size(const struct panvk_image *image)
{
   assert(util_format_get_num_planes(image->pimage.layout.format) == 1);
   return image->pimage.layout.data_size;
}

static enum mali_texture_dimension
panvk_image_type_to_mali_tex_dim(VkImageType type)
{
   switch (type) {
   case VK_IMAGE_TYPE_1D:
      return MALI_TEXTURE_DIMENSION_1D;
   case VK_IMAGE_TYPE_2D:
      return MALI_TEXTURE_DIMENSION_2D;
   case VK_IMAGE_TYPE_3D:
      return MALI_TEXTURE_DIMENSION_3D;
   default:
      unreachable("Invalid image type");
   }
}

static VkResult
panvk_image_create(VkDevice _device, const VkImageCreateInfo *pCreateInfo,
                   const VkAllocationCallbacks *alloc, VkImage *pImage,
                   uint64_t modifier, const VkSubresourceLayout *plane_layouts)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   struct panvk_physical_device *phys_dev =
      to_panvk_physical_device(device->vk.physical);
   struct panvk_image *image = NULL;

   image = vk_image_create(&device->vk, pCreateInfo, alloc, sizeof(*image));
   if (!image)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   image->pimage.layout = (struct pan_image_layout){
      .modifier = modifier,
      .format = vk_format_to_pipe_format(image->vk.format),
      .dim = panvk_image_type_to_mali_tex_dim(image->vk.image_type),
      .width = image->vk.extent.width,
      .height = image->vk.extent.height,
      .depth = image->vk.extent.depth,
      .array_size = image->vk.array_layers,
      .nr_samples = image->vk.samples,
      .nr_slices = image->vk.mip_levels,
   };

   unsigned arch = pan_arch(phys_dev->kmod.props.gpu_prod_id);
   pan_image_layout_init(arch, &image->pimage.layout, NULL);

   *pImage = panvk_image_to_handle(image);
   return VK_SUCCESS;
}

static uint64_t
panvk_image_select_mod(VkDevice _device, const VkImageCreateInfo *pCreateInfo,
                       const VkSubresourceLayout **plane_layouts)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   struct panvk_instance *instance =
      to_panvk_instance(device->vk.physical->instance);
   struct panvk_physical_device *phys_dev =
      to_panvk_physical_device(device->vk.physical);
   enum pipe_format fmt = vk_format_to_pipe_format(pCreateInfo->format);
   bool noafbc = !(instance->debug_flags & PANVK_DEBUG_AFBC);
   bool linear = instance->debug_flags & PANVK_DEBUG_LINEAR;

   *plane_layouts = NULL;

   if (pCreateInfo->tiling == VK_IMAGE_TILING_LINEAR)
      return DRM_FORMAT_MOD_LINEAR;

   if (pCreateInfo->tiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT) {
      const VkImageDrmFormatModifierListCreateInfoEXT *mod_info =
         vk_find_struct_const(pCreateInfo->pNext,
                              IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT);
      const VkImageDrmFormatModifierExplicitCreateInfoEXT *drm_explicit_info =
         vk_find_struct_const(
            pCreateInfo->pNext,
            IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT);

      assert(mod_info || drm_explicit_info);

      uint64_t modifier;

      if (mod_info) {
         modifier = DRM_FORMAT_MOD_LINEAR;
         for (unsigned i = 0; i < mod_info->drmFormatModifierCount; i++) {
            if (drm_is_afbc(mod_info->pDrmFormatModifiers[i]) && !noafbc) {
               modifier = mod_info->pDrmFormatModifiers[i];
               break;
            }
         }
      } else {
         modifier = drm_explicit_info->drmFormatModifier;
         assert(modifier == DRM_FORMAT_MOD_LINEAR ||
                modifier == DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED ||
                (drm_is_afbc(modifier) && !noafbc));
         *plane_layouts = drm_explicit_info->pPlaneLayouts;
      }

      return modifier;
   }

   const struct wsi_image_create_info *wsi_info =
      vk_find_struct_const(pCreateInfo->pNext, WSI_IMAGE_CREATE_INFO_MESA);
   if (wsi_info && wsi_info->scanout)
      return DRM_FORMAT_MOD_LINEAR;

   assert(pCreateInfo->tiling == VK_IMAGE_TILING_OPTIMAL);

   if (linear)
      return DRM_FORMAT_MOD_LINEAR;

   /* Image store don't work on AFBC images */
   if (pCreateInfo->usage & VK_IMAGE_USAGE_STORAGE_BIT)
      return DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED;

   /* AFBC does not support layered multisampling */
   if (pCreateInfo->samples > 1)
      return DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED;

   if (!panfrost_query_afbc(&phys_dev->kmod.props))
      return DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED;

   /* Only a small selection of formats are AFBC'able */
   unsigned arch = pan_arch(phys_dev->kmod.props.gpu_prod_id);
   if (!panfrost_format_supports_afbc(arch, fmt))
      return DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED;

   /* 3D AFBC is only supported on Bifrost v7+. It's supposed to
    * be supported on Midgard but it doesn't seem to work.
    */
   if (pCreateInfo->imageType == VK_IMAGE_TYPE_3D && arch < 7)
      return DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED;

   /* For one tile, AFBC is a loss compared to u-interleaved */
   if (pCreateInfo->extent.width <= 16 && pCreateInfo->extent.height <= 16)
      return DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED;

   if (noafbc)
      return DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED;

   uint64_t afbc_type =
      AFBC_FORMAT_MOD_BLOCK_SIZE_16x16 | AFBC_FORMAT_MOD_SPARSE;

   if (panfrost_afbc_can_ytr(fmt))
      afbc_type |= AFBC_FORMAT_MOD_YTR;

   return DRM_FORMAT_MOD_ARM_AFBC(afbc_type);
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_CreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo,
                  const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
   const VkSubresourceLayout *plane_layouts;
   uint64_t modifier =
      panvk_image_select_mod(device, pCreateInfo, &plane_layouts);

   return panvk_image_create(device, pCreateInfo, pAllocator, pImage, modifier,
                             plane_layouts);
}

VKAPI_ATTR void VKAPI_CALL
panvk_DestroyImage(VkDevice _device, VkImage _image,
                   const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_image, image, _image);

   if (!image)
      return;

   if (image->bo)
      pan_kmod_bo_put(image->bo);

   vk_image_destroy(&device->vk, pAllocator, &image->vk);
}

static unsigned
panvk_plane_index(VkFormat format, VkImageAspectFlags aspect_mask)
{
   switch (aspect_mask) {
   default:
      return 0;
   case VK_IMAGE_ASPECT_PLANE_1_BIT:
      return 1;
   case VK_IMAGE_ASPECT_PLANE_2_BIT:
      return 2;
   case VK_IMAGE_ASPECT_STENCIL_BIT:
      return format == VK_FORMAT_D32_SFLOAT_S8_UINT;
   }
}

VKAPI_ATTR void VKAPI_CALL
panvk_GetImageSubresourceLayout(VkDevice _device, VkImage _image,
                                const VkImageSubresource *pSubresource,
                                VkSubresourceLayout *pLayout)
{
   VK_FROM_HANDLE(panvk_image, image, _image);

   unsigned plane =
      panvk_plane_index(image->vk.format, pSubresource->aspectMask);
   assert(plane < PANVK_MAX_PLANES);

   const struct pan_image_slice_layout *slice_layout =
      &image->pimage.layout.slices[pSubresource->mipLevel];

   pLayout->offset = slice_layout->offset + (pSubresource->arrayLayer *
                                             image->pimage.layout.array_stride);
   pLayout->size = slice_layout->size;
   pLayout->rowPitch = slice_layout->row_stride;
   pLayout->arrayPitch = image->pimage.layout.array_stride;
   pLayout->depthPitch = slice_layout->surface_stride;
}

VKAPI_ATTR void VKAPI_CALL
panvk_GetImageMemoryRequirements2(VkDevice device,
                                  const VkImageMemoryRequirementsInfo2 *pInfo,
                                  VkMemoryRequirements2 *pMemoryRequirements)
{
   VK_FROM_HANDLE(panvk_image, image, pInfo->image);

   const uint64_t alignment = 4096;
   const uint64_t size = panvk_image_get_total_size(image);

   pMemoryRequirements->memoryRequirements.memoryTypeBits = 1;
   pMemoryRequirements->memoryRequirements.alignment = alignment;
   pMemoryRequirements->memoryRequirements.size = size;
}

VKAPI_ATTR void VKAPI_CALL
panvk_GetImageSparseMemoryRequirements2(
   VkDevice device, const VkImageSparseMemoryRequirementsInfo2 *pInfo,
   uint32_t *pSparseMemoryRequirementCount,
   VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
   panvk_stub();
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_BindImageMemory2(VkDevice device, uint32_t bindInfoCount,
                       const VkBindImageMemoryInfo *pBindInfos)
{
   for (uint32_t i = 0; i < bindInfoCount; ++i) {
      VK_FROM_HANDLE(panvk_image, image, pBindInfos[i].image);
      VK_FROM_HANDLE(panvk_device_memory, mem, pBindInfos[i].memory);
      struct pan_kmod_bo *old_bo = image->bo;

      assert(mem);
      image->bo = pan_kmod_bo_get(mem->bo);
      image->pimage.data.base = mem->addr.dev;
      image->pimage.data.offset = pBindInfos[i].memoryOffset;
      /* Reset the AFBC headers */
      if (drm_is_afbc(image->pimage.layout.modifier)) {
         /* Transient CPU mapping */
         void *base = pan_kmod_bo_mmap(mem->bo, 0, pan_kmod_bo_size(mem->bo),
                                       PROT_WRITE, MAP_SHARED, NULL);

         assert(base != MAP_FAILED);

         for (unsigned layer = 0; layer < image->pimage.layout.array_size;
              layer++) {
            for (unsigned level = 0; level < image->pimage.layout.nr_slices;
                 level++) {
               void *header = base + image->pimage.data.offset +
                              (layer * image->pimage.layout.array_stride) +
                              image->pimage.layout.slices[level].offset;
               memset(header, 0,
                      image->pimage.layout.slices[level].afbc.header_size);
            }
         }

         ASSERTED int ret = os_munmap(base, pan_kmod_bo_size(mem->bo));
         assert(!ret);
      }

      pan_kmod_bo_put(old_bo);
   }

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_GetImageDrmFormatModifierPropertiesEXT(
   VkDevice device, VkImage _image,
   VkImageDrmFormatModifierPropertiesEXT *pProperties)
{
   VK_FROM_HANDLE(panvk_image, image, _image);

   assert(pProperties->sType ==
          VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT);

   pProperties->drmFormatModifier = image->pimage.layout.modifier;
   return VK_SUCCESS;
}
