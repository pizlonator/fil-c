/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "gfx10_format_table.h"

#include "radv_buffer.h"
#include "radv_buffer_view.h"
#include "radv_entrypoints.h"
#include "radv_formats.h"
#include "radv_image.h"

#include "vk_log.h"

void
radv_make_texel_buffer_descriptor(struct radv_device *device, uint64_t va, VkFormat vk_format, unsigned offset,
                                  unsigned range, uint32_t *state)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct util_format_description *desc;
   unsigned stride;
   unsigned num_format, data_format;
   int first_non_void;
   enum pipe_swizzle swizzle[4];
   unsigned rsrc_word3;

   desc = vk_format_description(vk_format);
   first_non_void = vk_format_get_first_non_void_channel(vk_format);
   stride = desc->block.bits / 8;

   radv_compose_swizzle(desc, NULL, swizzle);

   va += offset;

   if (pdev->info.gfx_level != GFX8 && stride) {
      range /= stride;
   }

   rsrc_word3 = S_008F0C_DST_SEL_X(radv_map_swizzle(swizzle[0])) | S_008F0C_DST_SEL_Y(radv_map_swizzle(swizzle[1])) |
                S_008F0C_DST_SEL_Z(radv_map_swizzle(swizzle[2])) | S_008F0C_DST_SEL_W(radv_map_swizzle(swizzle[3]));

   if (pdev->info.gfx_level >= GFX10) {
      const struct gfx10_format *fmt = &ac_get_gfx10_format_table(&pdev->info)[vk_format_to_pipe_format(vk_format)];

      /* OOB_SELECT chooses the out-of-bounds check.
       *
       * GFX10:
       *  - 0: (index >= NUM_RECORDS) || (offset >= STRIDE)
       *  - 1: index >= NUM_RECORDS
       *  - 2: NUM_RECORDS == 0
       *  - 3: if SWIZZLE_ENABLE:
       *          swizzle_address >= NUM_RECORDS
       *       else:
       *          offset >= NUM_RECORDS
       *
       * GFX11:
       *  - 0: (index >= NUM_RECORDS) || (offset+payload > STRIDE)
       *  - 1: index >= NUM_RECORDS
       *  - 2: NUM_RECORDS == 0
       *  - 3: if SWIZZLE_ENABLE && STRIDE:
       *          (index >= NUM_RECORDS) || ( offset+payload > STRIDE)
       *       else:
       *          offset+payload > NUM_RECORDS
       */
      rsrc_word3 |= S_008F0C_FORMAT(fmt->img_format) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_STRUCTURED_WITH_OFFSET) |
                    S_008F0C_RESOURCE_LEVEL(pdev->info.gfx_level < GFX11);
   } else {
      num_format = radv_translate_buffer_numformat(desc, first_non_void);
      data_format = radv_translate_buffer_dataformat(desc, first_non_void);

      assert(data_format != V_008F0C_BUF_DATA_FORMAT_INVALID);
      assert(num_format != ~0);

      rsrc_word3 |= S_008F0C_NUM_FORMAT(num_format) | S_008F0C_DATA_FORMAT(data_format);
   }

   state[0] = va;
   state[1] = S_008F04_BASE_ADDRESS_HI(va >> 32) | S_008F04_STRIDE(stride);
   state[2] = range;
   state[3] = rsrc_word3;
}

void
radv_buffer_view_init(struct radv_buffer_view *view, struct radv_device *device,
                      const VkBufferViewCreateInfo *pCreateInfo)
{
   VK_FROM_HANDLE(radv_buffer, buffer, pCreateInfo->buffer);
   uint64_t va = radv_buffer_get_va(buffer->bo) + buffer->offset;

   vk_buffer_view_init(&device->vk, &view->vk, pCreateInfo);

   view->bo = buffer->bo;

   radv_make_texel_buffer_descriptor(device, va, view->vk.format, view->vk.offset, view->vk.range, view->state);
}

void
radv_buffer_view_finish(struct radv_buffer_view *view)
{
   vk_buffer_view_finish(&view->vk);
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CreateBufferView(VkDevice _device, const VkBufferViewCreateInfo *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator, VkBufferView *pView)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   struct radv_buffer_view *view;

   view = vk_alloc2(&device->vk.alloc, pAllocator, sizeof(*view), 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!view)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   radv_buffer_view_init(view, device, pCreateInfo);

   *pView = radv_buffer_view_to_handle(view);

   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
radv_DestroyBufferView(VkDevice _device, VkBufferView bufferView, const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   VK_FROM_HANDLE(radv_buffer_view, view, bufferView);

   if (!view)
      return;

   radv_buffer_view_finish(view);
   vk_free2(&device->vk.alloc, pAllocator, view);
}
