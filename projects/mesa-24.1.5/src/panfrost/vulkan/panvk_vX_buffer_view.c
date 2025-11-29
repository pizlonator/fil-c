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

#include "genxml/gen_macros.h"

#include "panvk_buffer.h"
#include "panvk_buffer_view.h"
#include "panvk_device.h"
#include "panvk_entrypoints.h"
#include "panvk_priv_bo.h"

#include "vk_format.h"
#include "vk_log.h"

VKAPI_ATTR VkResult VKAPI_CALL
panvk_per_arch(CreateBufferView)(VkDevice _device,
                                 const VkBufferViewCreateInfo *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkBufferView *pView)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_buffer, buffer, pCreateInfo->buffer);

   struct panvk_buffer_view *view = vk_object_zalloc(
      &device->vk, pAllocator, sizeof(*view), VK_OBJECT_TYPE_BUFFER_VIEW);

   if (!view)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_buffer_view_init(&device->vk, &view->vk, pCreateInfo);

   enum pipe_format pfmt = vk_format_to_pipe_format(view->vk.format);

   mali_ptr address = panvk_buffer_gpu_ptr(buffer, pCreateInfo->offset);
   unsigned blksz = vk_format_get_blocksize(pCreateInfo->format);

   assert(!(address & 63));

   if (buffer->vk.usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) {
      unsigned bo_size = pan_size(SURFACE_WITH_STRIDE);
      view->bo = panvk_priv_bo_create(device, bo_size, 0, pAllocator,
                                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

      pan_pack(view->bo->addr.host, SURFACE_WITH_STRIDE, cfg) {
         cfg.pointer = address;
      }

      pan_pack(view->descs.tex.opaque, TEXTURE, cfg) {
         cfg.dimension = MALI_TEXTURE_DIMENSION_1D;
         cfg.format = GENX(panfrost_format_from_pipe_format)(pfmt)->hw;
         cfg.width = view->vk.elements;
         cfg.depth = cfg.height = 1;
         cfg.swizzle = PAN_V6_SWIZZLE(R, G, B, A);
         cfg.texel_ordering = MALI_TEXTURE_LAYOUT_LINEAR;
         cfg.levels = 1;
         cfg.array_size = 1;
         cfg.surfaces = view->bo->addr.dev;
         cfg.maximum_lod = cfg.minimum_lod = 0;
      }
   }

   if (buffer->vk.usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) {
      pan_pack(view->descs.img_attrib_buf[0].opaque, ATTRIBUTE_BUFFER, cfg) {
         cfg.type = MALI_ATTRIBUTE_TYPE_3D_LINEAR;
         cfg.pointer = address;
         cfg.stride = blksz;
         cfg.size = view->vk.elements * blksz;
      }

      pan_pack(view->descs.img_attrib_buf[1].opaque,
               ATTRIBUTE_BUFFER_CONTINUATION_3D, cfg) {
         cfg.s_dimension = view->vk.elements;
         cfg.t_dimension = 1;
         cfg.r_dimension = 1;
         cfg.row_stride = view->vk.elements * blksz;
      }
   }

   *pView = panvk_buffer_view_to_handle(view);
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(DestroyBufferView)(VkDevice _device, VkBufferView bufferView,
                                  const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_buffer_view, view, bufferView);

   if (!view)
      return;

   panvk_priv_bo_destroy(view->bo, pAllocator);
   vk_buffer_view_destroy(&device->vk, pAllocator, &view->vk);
}
