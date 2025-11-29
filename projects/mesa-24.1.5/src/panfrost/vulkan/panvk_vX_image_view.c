/*
 * Copyright © 2021 Collabora Ltd.
 *
 * Derived from tu_image.c which is:
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "vk_format.h"
#include "vk_log.h"

#include "panvk_device.h"
#include "panvk_entrypoints.h"
#include "panvk_image.h"
#include "panvk_image_view.h"
#include "panvk_priv_bo.h"

#include "genxml/gen_macros.h"

static enum mali_texture_dimension
panvk_view_type_to_mali_tex_dim(VkImageViewType type)
{
   switch (type) {
   case VK_IMAGE_VIEW_TYPE_1D:
   case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
      return MALI_TEXTURE_DIMENSION_1D;
   case VK_IMAGE_VIEW_TYPE_2D:
   case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
      return MALI_TEXTURE_DIMENSION_2D;
   case VK_IMAGE_VIEW_TYPE_3D:
      return MALI_TEXTURE_DIMENSION_3D;
   case VK_IMAGE_VIEW_TYPE_CUBE:
   case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
      return MALI_TEXTURE_DIMENSION_CUBE;
   default:
      unreachable("Invalid view type");
   }
}

static void
panvk_convert_swizzle(const VkComponentMapping *in, unsigned char *out)
{
   const VkComponentSwizzle *comp = &in->r;
   for (unsigned i = 0; i < 4; i++) {
      switch (comp[i]) {
      case VK_COMPONENT_SWIZZLE_ZERO:
         out[i] = PIPE_SWIZZLE_0;
         break;
      case VK_COMPONENT_SWIZZLE_ONE:
         out[i] = PIPE_SWIZZLE_1;
         break;
      case VK_COMPONENT_SWIZZLE_R:
         out[i] = PIPE_SWIZZLE_X;
         break;
      case VK_COMPONENT_SWIZZLE_G:
         out[i] = PIPE_SWIZZLE_Y;
         break;
      case VK_COMPONENT_SWIZZLE_B:
         out[i] = PIPE_SWIZZLE_Z;
         break;
      case VK_COMPONENT_SWIZZLE_A:
         out[i] = PIPE_SWIZZLE_W;
         break;
      default:
         unreachable("Invalid swizzle");
      }
   }
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_per_arch(CreateImageView)(VkDevice _device,
                                const VkImageViewCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkImageView *pView)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_image, image, pCreateInfo->image);
   struct panvk_image_view *view;

   view = vk_image_view_create(&device->vk, false, pCreateInfo, pAllocator,
                               sizeof(*view));
   if (view == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   view->pview = (struct pan_image_view){
      .planes[0] = &image->pimage,
      .format = vk_format_to_pipe_format(view->vk.view_format),
      .dim = panvk_view_type_to_mali_tex_dim(view->vk.view_type),
      .nr_samples = image->pimage.layout.nr_samples,
      .first_level = view->vk.base_mip_level,
      .last_level = view->vk.base_mip_level + view->vk.level_count - 1,
      .first_layer = view->vk.base_array_layer,
      .last_layer = view->vk.base_array_layer + view->vk.layer_count - 1,
   };
   panvk_convert_swizzle(&view->vk.swizzle, view->pview.swizzle);

   /* We need to patch the view format when the image contains both
    * depth and stencil but the view only contains one of these components, so
    * we can ignore the component we don't use.
    */
   if (image->vk.format == VK_FORMAT_D32_SFLOAT_S8_UINT &&
       view->vk.view_format != VK_FORMAT_D32_SFLOAT_S8_UINT)
      view->pview.format = view->vk.view_format == VK_FORMAT_D32_SFLOAT
                              ? PIPE_FORMAT_Z32_FLOAT_S8X24_UINT
                              : PIPE_FORMAT_X32_S8X24_UINT;

   if (image->vk.format == VK_FORMAT_D24_UNORM_S8_UINT &&
       view->vk.view_format == VK_FORMAT_S8_UINT)
      view->pview.format = PIPE_FORMAT_X24S8_UINT;

   if (image->vk.format == VK_FORMAT_D32_SFLOAT_S8_UINT &&
       view->vk.view_format == VK_FORMAT_S8_UINT)
      view->pview.format = PIPE_FORMAT_X32_S8X24_UINT;

   if (view->vk.usage &
       (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) {
      /* Use a temporary pan_image_view so we can tweak it for texture
       * descriptor emission without changing the original definition.
       */
      struct pan_image_view pview = view->pview;

      if (util_format_is_depth_or_stencil(view->pview.format)) {
         /* Vulkan wants R001, where the depth/stencil is stored in the red
          * component, but the pan_format/texture logic gives us RRRR.
          * Tweak the swizzle so we get what Vulkan wants.
          */
         static const unsigned char r001[4] = {
            PIPE_SWIZZLE_X,
            PIPE_SWIZZLE_0,
            PIPE_SWIZZLE_0,
            PIPE_SWIZZLE_1,
         };

         util_format_compose_swizzles(r001, view->pview.swizzle, pview.swizzle);
      }

      unsigned bo_size =
         GENX(panfrost_estimate_texture_payload_size)(&pview);

      view->bo = panvk_priv_bo_create(device, bo_size, 0, pAllocator,
                                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

      struct panfrost_ptr ptr = {
         .gpu = view->bo->addr.dev,
         .cpu = view->bo->addr.host,
      };

      GENX(panfrost_new_texture)(&pview, view->descs.tex.opaque, &ptr);
   }

   if (view->vk.usage & VK_IMAGE_USAGE_STORAGE_BIT) {
      bool is_3d = image->pimage.layout.dim == MALI_TEXTURE_DIMENSION_3D;
      unsigned offset = image->pimage.data.offset;
      offset +=
         panfrost_texture_offset(&image->pimage.layout, view->pview.first_level,
                                 is_3d ? 0 : view->pview.first_layer,
                                 is_3d ? view->pview.first_layer : 0);

      pan_pack(view->descs.img_attrib_buf[0].opaque, ATTRIBUTE_BUFFER, cfg) {
         cfg.type = image->pimage.layout.modifier == DRM_FORMAT_MOD_LINEAR
                       ? MALI_ATTRIBUTE_TYPE_3D_LINEAR
                       : MALI_ATTRIBUTE_TYPE_3D_INTERLEAVED;
         cfg.pointer = image->pimage.data.base + offset;
         cfg.stride = util_format_get_blocksize(view->pview.format);
         cfg.size = pan_kmod_bo_size(image->bo) - offset;
      }

      pan_pack(view->descs.img_attrib_buf[1].opaque,
               ATTRIBUTE_BUFFER_CONTINUATION_3D, cfg) {
         unsigned level = view->pview.first_level;

         cfg.s_dimension = u_minify(image->pimage.layout.width, level);
         cfg.t_dimension = u_minify(image->pimage.layout.height, level);
         cfg.r_dimension =
            view->pview.dim == MALI_TEXTURE_DIMENSION_3D
               ? u_minify(image->pimage.layout.depth, level)
               : (view->pview.last_layer - view->pview.first_layer + 1);
         cfg.row_stride = image->pimage.layout.slices[level].row_stride;
         if (cfg.r_dimension > 1) {
            cfg.slice_stride =
               panfrost_get_layer_stride(&image->pimage.layout, level);
         }
      }
   }

   *pView = panvk_image_view_to_handle(view);
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(DestroyImageView)(VkDevice _device, VkImageView _view,
                                 const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_image_view, view, _view);

   if (!view)
      return;

   panvk_priv_bo_destroy(view->bo, NULL);
   vk_image_view_destroy(&device->vk, pAllocator, &view->vk);
}
