/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "genxml/gen_macros.h"

#include "panvk_device.h"
#include "panvk_entrypoints.h"
#include "panvk_sampler.h"

#include "pan_encoder.h"
#include "pan_format.h"

#include "vk_format.h"
#include "vk_log.h"

static enum mali_mipmap_mode
panvk_translate_sampler_mipmap_mode(VkSamplerMipmapMode mode)
{
   switch (mode) {
   case VK_SAMPLER_MIPMAP_MODE_NEAREST:
      return MALI_MIPMAP_MODE_NEAREST;
   case VK_SAMPLER_MIPMAP_MODE_LINEAR:
      return MALI_MIPMAP_MODE_TRILINEAR;
   default:
      unreachable("Invalid mipmap mode");
   }
}

static unsigned
panvk_translate_sampler_address_mode(VkSamplerAddressMode mode)
{
   switch (mode) {
   case VK_SAMPLER_ADDRESS_MODE_REPEAT:
      return MALI_WRAP_MODE_REPEAT;
   case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
      return MALI_WRAP_MODE_MIRRORED_REPEAT;
   case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
      return MALI_WRAP_MODE_CLAMP_TO_EDGE;
   case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
      return MALI_WRAP_MODE_CLAMP_TO_BORDER;
   case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
      return MALI_WRAP_MODE_MIRRORED_CLAMP_TO_EDGE;
   default:
      unreachable("Invalid wrap");
   }
}

static enum mali_func
panvk_translate_sampler_compare_func(const VkSamplerCreateInfo *pCreateInfo)
{
   if (!pCreateInfo->compareEnable)
      return MALI_FUNC_NEVER;

   return panfrost_flip_compare_func((enum mali_func)pCreateInfo->compareOp);
}

static void
swizzle_border_color(VkClearColorValue *border_color, VkFormat fmt)
{
   if (PAN_ARCH != 7)
      return;

   enum pipe_format pfmt = vk_format_to_pipe_format(fmt);
   if (panfrost_format_is_yuv(pfmt) || util_format_is_depth_or_stencil(pfmt))
      return;

   const struct util_format_description *fdesc = util_format_description(pfmt);
   if (fdesc->swizzle[0] == PIPE_SWIZZLE_Z &&
       fdesc->swizzle[2] == PIPE_SWIZZLE_X) {
      uint32_t red = border_color->uint32[0];

      border_color->uint32[0] = border_color->uint32[2];
      border_color->uint32[2] = red;
   }
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_per_arch(CreateSampler)(VkDevice _device,
                              const VkSamplerCreateInfo *pCreateInfo,
                              const VkAllocationCallbacks *pAllocator,
                              VkSampler *pSampler)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   struct panvk_sampler *sampler;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);

   sampler =
      vk_sampler_create(&device->vk, pCreateInfo, pAllocator, sizeof(*sampler));
   if (!sampler)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   STATIC_ASSERT(sizeof(sampler->desc) >= pan_size(SAMPLER));

   VkFormat fmt;
   VkClearColorValue border_color =
      vk_sampler_border_color_value(pCreateInfo, &fmt);

   swizzle_border_color(&border_color, fmt);

   pan_pack(sampler->desc.opaque, SAMPLER, cfg) {
      cfg.magnify_nearest = pCreateInfo->magFilter == VK_FILTER_NEAREST;
      cfg.minify_nearest = pCreateInfo->minFilter == VK_FILTER_NEAREST;
      cfg.mipmap_mode =
         panvk_translate_sampler_mipmap_mode(pCreateInfo->mipmapMode);
      cfg.normalized_coordinates = !pCreateInfo->unnormalizedCoordinates;

      cfg.lod_bias = pCreateInfo->mipLodBias;
      cfg.minimum_lod = pCreateInfo->minLod;
      cfg.maximum_lod = pCreateInfo->maxLod;
      cfg.wrap_mode_s =
         panvk_translate_sampler_address_mode(pCreateInfo->addressModeU);
      cfg.wrap_mode_t =
         panvk_translate_sampler_address_mode(pCreateInfo->addressModeV);

      /* "
       * When unnormalizedCoordinates is VK_TRUE, images the sampler is used
       * with in the shader have the following requirements:
       * - The viewType must be either VK_IMAGE_VIEW_TYPE_1D or
       *   VK_IMAGE_VIEW_TYPE_2D.
       * - The image view must have a single layer and a single mip level.
       * "
       *
       * This means addressModeW should be ignored. We pick a default value
       * that works for normalized_coordinates=false.
       */
      cfg.wrap_mode_r =
         pCreateInfo->unnormalizedCoordinates ?
         MALI_WRAP_MODE_CLAMP_TO_EDGE :
         panvk_translate_sampler_address_mode(pCreateInfo->addressModeW);
      cfg.compare_function = panvk_translate_sampler_compare_func(pCreateInfo);
      cfg.border_color_r = border_color.uint32[0];
      cfg.border_color_g = border_color.uint32[1];
      cfg.border_color_b = border_color.uint32[2];
      cfg.border_color_a = border_color.uint32[3];
   }

   *pSampler = panvk_sampler_to_handle(sampler);
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(DestroySampler)(VkDevice _device, VkSampler _sampler,
                               const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_sampler, sampler, _sampler);

   if (!sampler)
      return;

   vk_sampler_destroy(&device->vk, pAllocator, &sampler->vk);
}
