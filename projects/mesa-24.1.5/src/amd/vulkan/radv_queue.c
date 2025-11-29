/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "radv_queue.h"
#include "radv_buffer.h"
#include "radv_cp_reg_shadowing.h"
#include "radv_cs.h"
#include "radv_debug.h"
#include "radv_device_memory.h"
#include "radv_image.h"
#include "radv_printf.h"
#include "radv_rmv.h"
#include "vk_semaphore.h"
#include "vk_sync.h"

#include "ac_debug.h"

enum radeon_ctx_priority
radv_get_queue_global_priority(const VkDeviceQueueGlobalPriorityCreateInfoKHR *pObj)
{
   /* Default to MEDIUM when a specific global priority isn't requested */
   if (!pObj)
      return RADEON_CTX_PRIORITY_MEDIUM;

   switch (pObj->globalPriority) {
   case VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR:
      return RADEON_CTX_PRIORITY_REALTIME;
   case VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR:
      return RADEON_CTX_PRIORITY_HIGH;
   case VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR:
      return RADEON_CTX_PRIORITY_MEDIUM;
   case VK_QUEUE_GLOBAL_PRIORITY_LOW_KHR:
      return RADEON_CTX_PRIORITY_LOW;
   default:
      unreachable("Illegal global priority value");
      return RADEON_CTX_PRIORITY_INVALID;
   }
}

static VkResult
radv_sparse_buffer_bind_memory(struct radv_device *device, const VkSparseBufferMemoryBindInfo *bind)
{
   VK_FROM_HANDLE(radv_buffer, buffer, bind->buffer);
   VkResult result = VK_SUCCESS;

   struct radv_device_memory *mem = NULL;
   VkDeviceSize resourceOffset = 0;
   VkDeviceSize size = 0;
   VkDeviceSize memoryOffset = 0;
   for (uint32_t i = 0; i < bind->bindCount; ++i) {
      struct radv_device_memory *cur_mem = NULL;

      if (bind->pBinds[i].memory != VK_NULL_HANDLE)
         cur_mem = radv_device_memory_from_handle(bind->pBinds[i].memory);
      if (i && mem == cur_mem) {
         if (mem) {
            if (bind->pBinds[i].resourceOffset == resourceOffset + size &&
                bind->pBinds[i].memoryOffset == memoryOffset + size) {
               size += bind->pBinds[i].size;
               continue;
            }
         } else {
            if (bind->pBinds[i].resourceOffset == resourceOffset + size) {
               size += bind->pBinds[i].size;
               continue;
            }
         }
      }
      if (size) {
         result = radv_bo_virtual_bind(device, &buffer->vk.base, buffer->bo, resourceOffset, size, mem ? mem->bo : NULL,
                                       memoryOffset);
         if (result != VK_SUCCESS)
            return result;
      }
      mem = cur_mem;
      resourceOffset = bind->pBinds[i].resourceOffset;
      size = bind->pBinds[i].size;
      memoryOffset = bind->pBinds[i].memoryOffset;
   }
   if (size) {
      result = radv_bo_virtual_bind(device, &buffer->vk.base, buffer->bo, resourceOffset, size, mem ? mem->bo : NULL,
                                    memoryOffset);
   }

   return result;
}

static VkResult
radv_sparse_image_opaque_bind_memory(struct radv_device *device, const VkSparseImageOpaqueMemoryBindInfo *bind)
{
   VK_FROM_HANDLE(radv_image, image, bind->image);
   VkResult result;

   for (uint32_t i = 0; i < bind->bindCount; ++i) {
      struct radv_device_memory *mem = NULL;

      if (bind->pBinds[i].memory != VK_NULL_HANDLE)
         mem = radv_device_memory_from_handle(bind->pBinds[i].memory);

      result = radv_bo_virtual_bind(device, &image->vk.base, image->bindings[0].bo, bind->pBinds[i].resourceOffset,
                                    bind->pBinds[i].size, mem ? mem->bo : NULL, bind->pBinds[i].memoryOffset);
      if (result != VK_SUCCESS)
         return result;
   }

   return VK_SUCCESS;
}

static VkResult
radv_sparse_image_bind_memory(struct radv_device *device, const VkSparseImageMemoryBindInfo *bind)
{
   VK_FROM_HANDLE(radv_image, image, bind->image);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_surf *surface = &image->planes[0].surface;
   uint32_t bs = vk_format_get_blocksize(image->vk.format);
   VkResult result;

   for (uint32_t i = 0; i < bind->bindCount; ++i) {
      struct radv_device_memory *mem = NULL;
      uint64_t offset, depth_pitch;
      uint32_t pitch;
      uint64_t mem_offset = bind->pBinds[i].memoryOffset;
      const uint32_t layer = bind->pBinds[i].subresource.arrayLayer;
      const uint32_t level = bind->pBinds[i].subresource.mipLevel;

      VkExtent3D bind_extent = bind->pBinds[i].extent;
      bind_extent.width = DIV_ROUND_UP(bind_extent.width, vk_format_get_blockwidth(image->vk.format));
      bind_extent.height = DIV_ROUND_UP(bind_extent.height, vk_format_get_blockheight(image->vk.format));

      VkOffset3D bind_offset = bind->pBinds[i].offset;
      bind_offset.x /= vk_format_get_blockwidth(image->vk.format);
      bind_offset.y /= vk_format_get_blockheight(image->vk.format);

      if (bind->pBinds[i].memory != VK_NULL_HANDLE)
         mem = radv_device_memory_from_handle(bind->pBinds[i].memory);

      if (pdev->info.gfx_level >= GFX9) {
         offset = surface->u.gfx9.surf_slice_size * layer + surface->u.gfx9.prt_level_offset[level];
         pitch = surface->u.gfx9.prt_level_pitch[level];
         depth_pitch = surface->u.gfx9.surf_slice_size;
      } else {
         depth_pitch = surface->u.legacy.level[level].slice_size_dw * 4;
         offset = (uint64_t)surface->u.legacy.level[level].offset_256B * 256 + depth_pitch * layer;
         pitch = surface->u.legacy.level[level].nblk_x;
      }

      offset +=
         bind_offset.z * depth_pitch + ((uint64_t)bind_offset.y * pitch * surface->prt_tile_depth +
                                        (uint64_t)bind_offset.x * surface->prt_tile_height * surface->prt_tile_depth) *
                                          bs;

      uint32_t aligned_extent_width = ALIGN(bind_extent.width, surface->prt_tile_width);
      uint32_t aligned_extent_height = ALIGN(bind_extent.height, surface->prt_tile_height);
      uint32_t aligned_extent_depth = ALIGN(bind_extent.depth, surface->prt_tile_depth);

      bool whole_subres = (bind_extent.height <= surface->prt_tile_height || aligned_extent_width == pitch) &&
                          (bind_extent.depth <= surface->prt_tile_depth ||
                           (uint64_t)aligned_extent_width * aligned_extent_height * bs == depth_pitch);

      if (whole_subres) {
         uint64_t size = (uint64_t)aligned_extent_width * aligned_extent_height * aligned_extent_depth * bs;
         result = radv_bo_virtual_bind(device, &image->vk.base, image->bindings[0].bo, offset, size,
                                       mem ? mem->bo : NULL, mem_offset);
         if (result != VK_SUCCESS)
            return result;
      } else {
         uint32_t img_y_increment = pitch * bs * surface->prt_tile_depth;
         uint32_t mem_y_increment = aligned_extent_width * bs * surface->prt_tile_depth;
         uint64_t mem_z_increment = (uint64_t)aligned_extent_width * aligned_extent_height * bs;
         uint64_t size = mem_y_increment * surface->prt_tile_height;
         for (unsigned z = 0; z < bind_extent.depth;
              z += surface->prt_tile_depth, offset += depth_pitch * surface->prt_tile_depth) {
            for (unsigned y = 0; y < bind_extent.height; y += surface->prt_tile_height) {
               uint64_t bo_offset = offset + (uint64_t)img_y_increment * y;

               result = radv_bo_virtual_bind(device, &image->vk.base, image->bindings[0].bo, bo_offset, size,
                                             mem ? mem->bo : NULL,
                                             mem_offset + (uint64_t)mem_y_increment * y + mem_z_increment * z);
               if (result != VK_SUCCESS)
                  return result;
            }
         }
      }
   }

   return VK_SUCCESS;
}

static VkResult
radv_queue_submit_bind_sparse_memory(struct radv_device *device, struct vk_queue_submit *submission)
{
   for (uint32_t i = 0; i < submission->buffer_bind_count; ++i) {
      VkResult result = radv_sparse_buffer_bind_memory(device, submission->buffer_binds + i);
      if (result != VK_SUCCESS)
         return result;
   }

   for (uint32_t i = 0; i < submission->image_opaque_bind_count; ++i) {
      VkResult result = radv_sparse_image_opaque_bind_memory(device, submission->image_opaque_binds + i);
      if (result != VK_SUCCESS)
         return result;
   }

   for (uint32_t i = 0; i < submission->image_bind_count; ++i) {
      VkResult result = radv_sparse_image_bind_memory(device, submission->image_binds + i);
      if (result != VK_SUCCESS)
         return result;
   }

   return VK_SUCCESS;
}

static VkResult
radv_queue_submit_empty(struct radv_queue *queue, struct vk_queue_submit *submission)
{
   struct radv_device *device = radv_queue_device(queue);
   struct radeon_winsys_ctx *ctx = queue->hw_ctx;
   struct radv_winsys_submit_info submit = {
      .ip_type = radv_queue_ring(queue),
      .queue_index = queue->vk.index_in_family,
   };

   return device->ws->cs_submit(ctx, &submit, submission->wait_count, submission->waits, submission->signal_count,
                                submission->signals);
}

static void
radv_fill_shader_rings(struct radv_device *device, uint32_t *desc, struct radeon_winsys_bo *scratch_bo,
                       uint32_t esgs_ring_size, struct radeon_winsys_bo *esgs_ring_bo, uint32_t gsvs_ring_size,
                       struct radeon_winsys_bo *gsvs_ring_bo, struct radeon_winsys_bo *tess_rings_bo,
                       struct radeon_winsys_bo *task_rings_bo, struct radeon_winsys_bo *mesh_scratch_ring_bo,
                       uint32_t attr_ring_size, struct radeon_winsys_bo *attr_ring_bo)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);

   if (scratch_bo) {
      uint64_t scratch_va = radv_buffer_get_va(scratch_bo);
      uint32_t rsrc1 = S_008F04_BASE_ADDRESS_HI(scratch_va >> 32);

      if (pdev->info.gfx_level >= GFX11)
         rsrc1 |= S_008F04_SWIZZLE_ENABLE_GFX11(1);
      else
         rsrc1 |= S_008F04_SWIZZLE_ENABLE_GFX6(1);

      desc[0] = scratch_va;
      desc[1] = rsrc1;
   }

   desc += 4;

   if (esgs_ring_bo) {
      uint64_t esgs_va = radv_buffer_get_va(esgs_ring_bo);

      /* stride 0, num records - size, add tid, swizzle, elsize4,
         index stride 64 */
      desc[0] = esgs_va;
      desc[1] = S_008F04_BASE_ADDRESS_HI(esgs_va >> 32);
      desc[2] = esgs_ring_size;
      desc[3] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) | S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
                S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) | S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W) |
                S_008F0C_INDEX_STRIDE(3) | S_008F0C_ADD_TID_ENABLE(1);

      if (pdev->info.gfx_level >= GFX11)
         desc[1] |= S_008F04_SWIZZLE_ENABLE_GFX11(1);
      else
         desc[1] |= S_008F04_SWIZZLE_ENABLE_GFX6(1);

      if (pdev->info.gfx_level >= GFX11) {
         desc[3] |= S_008F0C_FORMAT(V_008F0C_GFX11_FORMAT_32_FLOAT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED);
      } else if (pdev->info.gfx_level >= GFX10) {
         desc[3] |= S_008F0C_FORMAT(V_008F0C_GFX10_FORMAT_32_FLOAT) |
                    S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED) | S_008F0C_RESOURCE_LEVEL(1);
      } else if (pdev->info.gfx_level >= GFX8) {
         /* DATA_FORMAT is STRIDE[14:17] for MUBUF with ADD_TID_ENABLE=1 */
         desc[3] |=
            S_008F0C_NUM_FORMAT(V_008F0C_BUF_NUM_FORMAT_FLOAT) | S_008F0C_DATA_FORMAT(0) | S_008F0C_ELEMENT_SIZE(1);
      } else {
         desc[3] |= S_008F0C_NUM_FORMAT(V_008F0C_BUF_NUM_FORMAT_FLOAT) |
                    S_008F0C_DATA_FORMAT(V_008F0C_BUF_DATA_FORMAT_32) | S_008F0C_ELEMENT_SIZE(1);
      }

      /* GS entry for ES->GS ring */
      /* stride 0, num records - size, elsize0,
         index stride 0 */
      desc[4] = esgs_va;
      desc[5] = S_008F04_BASE_ADDRESS_HI(esgs_va >> 32);
      desc[6] = esgs_ring_size;
      desc[7] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) | S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
                S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) | S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W);

      if (pdev->info.gfx_level >= GFX11) {
         desc[7] |= S_008F0C_FORMAT(V_008F0C_GFX11_FORMAT_32_FLOAT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED);
      } else if (pdev->info.gfx_level >= GFX10) {
         desc[7] |= S_008F0C_FORMAT(V_008F0C_GFX10_FORMAT_32_FLOAT) |
                    S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED) | S_008F0C_RESOURCE_LEVEL(1);
      } else {
         desc[7] |=
            S_008F0C_NUM_FORMAT(V_008F0C_BUF_NUM_FORMAT_FLOAT) | S_008F0C_DATA_FORMAT(V_008F0C_BUF_DATA_FORMAT_32);
      }
   }

   desc += 8;

   if (gsvs_ring_bo) {
      uint64_t gsvs_va = radv_buffer_get_va(gsvs_ring_bo);

      /* VS entry for GS->VS ring */
      /* stride 0, num records - size, elsize0,
         index stride 0 */
      desc[0] = gsvs_va;
      desc[1] = S_008F04_BASE_ADDRESS_HI(gsvs_va >> 32);
      desc[2] = gsvs_ring_size;
      desc[3] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) | S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
                S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) | S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W);

      if (pdev->info.gfx_level >= GFX11) {
         desc[3] |= S_008F0C_FORMAT(V_008F0C_GFX11_FORMAT_32_FLOAT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED);
      } else if (pdev->info.gfx_level >= GFX10) {
         desc[3] |= S_008F0C_FORMAT(V_008F0C_GFX10_FORMAT_32_FLOAT) |
                    S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED) | S_008F0C_RESOURCE_LEVEL(1);
      } else {
         desc[3] |=
            S_008F0C_NUM_FORMAT(V_008F0C_BUF_NUM_FORMAT_FLOAT) | S_008F0C_DATA_FORMAT(V_008F0C_BUF_DATA_FORMAT_32);
      }

      /* stride gsvs_itemsize, num records 64
         elsize 4, index stride 16 */
      /* shader will patch stride and desc[2] */
      desc[4] = gsvs_va;
      desc[5] = S_008F04_BASE_ADDRESS_HI(gsvs_va >> 32);
      desc[6] = 0;
      desc[7] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) | S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
                S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) | S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W) |
                S_008F0C_INDEX_STRIDE(1) | S_008F0C_ADD_TID_ENABLE(true);

      if (pdev->info.gfx_level >= GFX11)
         desc[5] |= S_008F04_SWIZZLE_ENABLE_GFX11(1);
      else
         desc[5] |= S_008F04_SWIZZLE_ENABLE_GFX6(1);

      if (pdev->info.gfx_level >= GFX11) {
         desc[7] |= S_008F0C_FORMAT(V_008F0C_GFX11_FORMAT_32_FLOAT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED);
      } else if (pdev->info.gfx_level >= GFX10) {
         desc[7] |= S_008F0C_FORMAT(V_008F0C_GFX10_FORMAT_32_FLOAT) |
                    S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED) | S_008F0C_RESOURCE_LEVEL(1);
      } else if (pdev->info.gfx_level >= GFX8) {
         /* DATA_FORMAT is STRIDE[14:17] for MUBUF with ADD_TID_ENABLE=1 */
         desc[7] |=
            S_008F0C_NUM_FORMAT(V_008F0C_BUF_NUM_FORMAT_FLOAT) | S_008F0C_DATA_FORMAT(0) | S_008F0C_ELEMENT_SIZE(1);
      } else {
         desc[7] |= S_008F0C_NUM_FORMAT(V_008F0C_BUF_NUM_FORMAT_FLOAT) |
                    S_008F0C_DATA_FORMAT(V_008F0C_BUF_DATA_FORMAT_32) | S_008F0C_ELEMENT_SIZE(1);
      }
   }

   desc += 8;

   if (tess_rings_bo) {
      uint64_t tess_va = radv_buffer_get_va(tess_rings_bo);
      uint64_t tess_offchip_va = tess_va + pdev->hs.tess_offchip_ring_offset;

      desc[0] = tess_va;
      desc[1] = S_008F04_BASE_ADDRESS_HI(tess_va >> 32);
      desc[2] = pdev->hs.tess_factor_ring_size;
      desc[3] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) | S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
                S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) | S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W);

      if (pdev->info.gfx_level >= GFX11) {
         desc[3] |= S_008F0C_FORMAT(V_008F0C_GFX11_FORMAT_32_FLOAT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_RAW);
      } else if (pdev->info.gfx_level >= GFX10) {
         desc[3] |= S_008F0C_FORMAT(V_008F0C_GFX10_FORMAT_32_FLOAT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_RAW) |
                    S_008F0C_RESOURCE_LEVEL(1);
      } else {
         desc[3] |=
            S_008F0C_NUM_FORMAT(V_008F0C_BUF_NUM_FORMAT_FLOAT) | S_008F0C_DATA_FORMAT(V_008F0C_BUF_DATA_FORMAT_32);
      }

      desc[4] = tess_offchip_va;
      desc[5] = S_008F04_BASE_ADDRESS_HI(tess_offchip_va >> 32);
      desc[6] = pdev->hs.tess_offchip_ring_size;
      desc[7] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) | S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
                S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) | S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W);

      if (pdev->info.gfx_level >= GFX11) {
         desc[7] |= S_008F0C_FORMAT(V_008F0C_GFX11_FORMAT_32_FLOAT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_RAW);
      } else if (pdev->info.gfx_level >= GFX10) {
         desc[7] |= S_008F0C_FORMAT(V_008F0C_GFX10_FORMAT_32_FLOAT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_RAW) |
                    S_008F0C_RESOURCE_LEVEL(1);
      } else {
         desc[7] |=
            S_008F0C_NUM_FORMAT(V_008F0C_BUF_NUM_FORMAT_FLOAT) | S_008F0C_DATA_FORMAT(V_008F0C_BUF_DATA_FORMAT_32);
      }
   }

   desc += 8;

   if (task_rings_bo) {
      uint64_t task_va = radv_buffer_get_va(task_rings_bo);
      uint64_t task_draw_ring_va = task_va + pdev->task_info.draw_ring_offset;
      uint64_t task_payload_ring_va = task_va + pdev->task_info.payload_ring_offset;

      desc[0] = task_draw_ring_va;
      desc[1] = S_008F04_BASE_ADDRESS_HI(task_draw_ring_va >> 32);
      desc[2] = pdev->task_info.num_entries * AC_TASK_DRAW_ENTRY_BYTES;
      desc[3] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) | S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
                S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) | S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W);

      if (pdev->info.gfx_level >= GFX11) {
         desc[3] |= S_008F0C_FORMAT(V_008F0C_GFX11_FORMAT_32_UINT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED);
      } else {
         assert(pdev->info.gfx_level >= GFX10_3);
         desc[3] |= S_008F0C_FORMAT(V_008F0C_GFX10_FORMAT_32_UINT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED) |
                    S_008F0C_RESOURCE_LEVEL(1);
      }

      desc[4] = task_payload_ring_va;
      desc[5] = S_008F04_BASE_ADDRESS_HI(task_payload_ring_va >> 32);
      desc[6] = pdev->task_info.num_entries * AC_TASK_PAYLOAD_ENTRY_BYTES;
      desc[7] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) | S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
                S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) | S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W);

      if (pdev->info.gfx_level >= GFX11) {
         desc[7] |= S_008F0C_FORMAT(V_008F0C_GFX11_FORMAT_32_UINT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED);
      } else {
         assert(pdev->info.gfx_level >= GFX10_3);
         desc[7] |= S_008F0C_FORMAT(V_008F0C_GFX10_FORMAT_32_UINT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED) |
                    S_008F0C_RESOURCE_LEVEL(1);
      }
   }

   desc += 8;

   if (mesh_scratch_ring_bo) {
      uint64_t va = radv_buffer_get_va(mesh_scratch_ring_bo);

      desc[0] = va;
      desc[1] = S_008F04_BASE_ADDRESS_HI(va >> 32);
      desc[2] = RADV_MESH_SCRATCH_NUM_ENTRIES * RADV_MESH_SCRATCH_ENTRY_BYTES;
      desc[3] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) | S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
                S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) | S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W);

      if (pdev->info.gfx_level >= GFX11) {
         desc[3] |= S_008F0C_FORMAT(V_008F0C_GFX11_FORMAT_32_UINT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED);
      } else {
         assert(pdev->info.gfx_level >= GFX10_3);
         desc[3] |= S_008F0C_FORMAT(V_008F0C_GFX10_FORMAT_32_UINT) | S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_DISABLED) |
                    S_008F0C_RESOURCE_LEVEL(1);
      }
   }

   desc += 4;

   if (attr_ring_bo) {
      assert(pdev->info.gfx_level >= GFX11);

      uint64_t va = radv_buffer_get_va(attr_ring_bo);

      desc[0] = va;
      desc[1] = S_008F04_BASE_ADDRESS_HI(va >> 32) | S_008F04_SWIZZLE_ENABLE_GFX11(3) /* 16B */;
      desc[2] = attr_ring_size;
      desc[3] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) | S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
                S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) | S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W) |
                S_008F0C_FORMAT(V_008F0C_GFX11_FORMAT_32_32_32_32_FLOAT) | S_008F0C_INDEX_STRIDE(2) /* 32 elements */;
   }

   desc += 4;

   /* add sample positions after all rings */
   memcpy(desc, device->sample_locations_1x, 8);
   desc += 2;
   memcpy(desc, device->sample_locations_2x, 16);
   desc += 4;
   memcpy(desc, device->sample_locations_4x, 32);
   desc += 8;
   memcpy(desc, device->sample_locations_8x, 64);
}

static void
radv_emit_gs_ring_sizes(struct radv_device *device, struct radeon_cmdbuf *cs, struct radeon_winsys_bo *esgs_ring_bo,
                        uint32_t esgs_ring_size, struct radeon_winsys_bo *gsvs_ring_bo, uint32_t gsvs_ring_size)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);

   if (!esgs_ring_bo && !gsvs_ring_bo)
      return;

   if (esgs_ring_bo)
      radv_cs_add_buffer(device->ws, cs, esgs_ring_bo);

   if (gsvs_ring_bo)
      radv_cs_add_buffer(device->ws, cs, gsvs_ring_bo);

   if (pdev->info.gfx_level >= GFX7) {
      radeon_set_uconfig_reg_seq(cs, R_030900_VGT_ESGS_RING_SIZE, 2);
      radeon_emit(cs, esgs_ring_size >> 8);
      radeon_emit(cs, gsvs_ring_size >> 8);
   } else {
      radeon_set_config_reg_seq(cs, R_0088C8_VGT_ESGS_RING_SIZE, 2);
      radeon_emit(cs, esgs_ring_size >> 8);
      radeon_emit(cs, gsvs_ring_size >> 8);
   }
}

static void
radv_emit_tess_factor_ring(struct radv_device *device, struct radeon_cmdbuf *cs, struct radeon_winsys_bo *tess_rings_bo)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   uint64_t tf_va;
   uint32_t tf_ring_size;
   if (!tess_rings_bo)
      return;

   tf_ring_size = pdev->hs.tess_factor_ring_size / 4;
   tf_va = radv_buffer_get_va(tess_rings_bo);

   radv_cs_add_buffer(device->ws, cs, tess_rings_bo);

   if (pdev->info.gfx_level >= GFX7) {
      if (pdev->info.gfx_level >= GFX11) {
         /* TF_RING_SIZE is per SE on GFX11. */
         tf_ring_size /= pdev->info.max_se;
      }

      radeon_set_uconfig_reg(cs, R_030938_VGT_TF_RING_SIZE, S_030938_SIZE(tf_ring_size));
      radeon_set_uconfig_reg(cs, R_030940_VGT_TF_MEMORY_BASE, tf_va >> 8);

      if (pdev->info.gfx_level >= GFX10) {
         radeon_set_uconfig_reg(cs, R_030984_VGT_TF_MEMORY_BASE_HI, S_030984_BASE_HI(tf_va >> 40));
      } else if (pdev->info.gfx_level == GFX9) {
         radeon_set_uconfig_reg(cs, R_030944_VGT_TF_MEMORY_BASE_HI, S_030944_BASE_HI(tf_va >> 40));
      }

      radeon_set_uconfig_reg(cs, R_03093C_VGT_HS_OFFCHIP_PARAM, pdev->hs.hs_offchip_param);
   } else {
      radeon_set_config_reg(cs, R_008988_VGT_TF_RING_SIZE, S_008988_SIZE(tf_ring_size));
      radeon_set_config_reg(cs, R_0089B8_VGT_TF_MEMORY_BASE, tf_va >> 8);
      radeon_set_config_reg(cs, R_0089B0_VGT_HS_OFFCHIP_PARAM, pdev->hs.hs_offchip_param);
   }
}

static VkResult
radv_initialise_task_control_buffer(struct radv_device *device, struct radeon_winsys_bo *task_rings_bo)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   uint32_t *ptr = (uint32_t *)radv_buffer_map(device->ws, task_rings_bo);
   if (!ptr)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;

   const uint32_t num_entries = pdev->task_info.num_entries;
   const uint64_t task_va = radv_buffer_get_va(task_rings_bo);
   const uint64_t task_draw_ring_va = task_va + pdev->task_info.draw_ring_offset;
   assert((task_draw_ring_va & 0xFFFFFF00) == (task_draw_ring_va & 0xFFFFFFFF));

   /* 64-bit write_ptr */
   ptr[0] = num_entries;
   ptr[1] = 0;
   /* 64-bit read_ptr */
   ptr[2] = num_entries;
   ptr[3] = 0;
   /* 64-bit dealloc_ptr */
   ptr[4] = num_entries;
   ptr[5] = 0;
   /* num_entries */
   ptr[6] = num_entries;
   /* 64-bit draw ring address */
   ptr[7] = task_draw_ring_va;
   ptr[8] = task_draw_ring_va >> 32;

   device->ws->buffer_unmap(device->ws, task_rings_bo, false);
   return VK_SUCCESS;
}

static void
radv_emit_task_rings(struct radv_device *device, struct radeon_cmdbuf *cs, struct radeon_winsys_bo *task_rings_bo,
                     bool compute)
{
   if (!task_rings_bo)
      return;

   const uint64_t task_ctrlbuf_va = radv_buffer_get_va(task_rings_bo);
   assert(util_is_aligned(task_ctrlbuf_va, 256));
   radv_cs_add_buffer(device->ws, cs, task_rings_bo);

   /* Tell the GPU where the task control buffer is. */
   radeon_emit(cs, PKT3(PKT3_DISPATCH_TASK_STATE_INIT, 1, 0) | PKT3_SHADER_TYPE_S(!!compute));
   /* bits [31:8]: control buffer address lo, bits[7:0]: reserved (set to zero) */
   radeon_emit(cs, task_ctrlbuf_va & 0xFFFFFF00);
   /* bits [31:0]: control buffer address hi */
   radeon_emit(cs, task_ctrlbuf_va >> 32);
}

static void
radv_emit_graphics_scratch(struct radv_device *device, struct radeon_cmdbuf *cs, uint32_t size_per_wave, uint32_t waves,
                           struct radeon_winsys_bo *scratch_bo)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radeon_info *gpu_info = &pdev->info;

   if (!scratch_bo)
      return;

   radv_cs_add_buffer(device->ws, cs, scratch_bo);

   if (gpu_info->gfx_level >= GFX11) {
      uint64_t va = radv_buffer_get_va(scratch_bo);

      /* WAVES is per SE for SPI_TMPRING_SIZE. */
      waves /= gpu_info->max_se;

      radeon_set_context_reg_seq(cs, R_0286E8_SPI_TMPRING_SIZE, 3);
      radeon_emit(cs, S_0286E8_WAVES(waves) | S_0286E8_WAVESIZE(DIV_ROUND_UP(size_per_wave, 256)));
      radeon_emit(cs, va >> 8);  /* SPI_GFX_SCRATCH_BASE_LO */
      radeon_emit(cs, va >> 40); /* SPI_GFX_SCRATCH_BASE_HI */
   } else {
      radeon_set_context_reg(cs, R_0286E8_SPI_TMPRING_SIZE,
                             S_0286E8_WAVES(waves) | S_0286E8_WAVESIZE(DIV_ROUND_UP(size_per_wave, 1024)));
   }
}

static void
radv_emit_compute_scratch(struct radv_device *device, struct radeon_cmdbuf *cs, uint32_t size_per_wave, uint32_t waves,
                          struct radeon_winsys_bo *compute_scratch_bo)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radeon_info *gpu_info = &pdev->info;
   uint64_t scratch_va;
   uint32_t rsrc1;

   if (!compute_scratch_bo)
      return;

   scratch_va = radv_buffer_get_va(compute_scratch_bo);
   rsrc1 = S_008F04_BASE_ADDRESS_HI(scratch_va >> 32);

   if (gpu_info->gfx_level >= GFX11)
      rsrc1 |= S_008F04_SWIZZLE_ENABLE_GFX11(1);
   else
      rsrc1 |= S_008F04_SWIZZLE_ENABLE_GFX6(1);

   radv_cs_add_buffer(device->ws, cs, compute_scratch_bo);

   if (gpu_info->gfx_level >= GFX11) {
      radeon_set_sh_reg_seq(cs, R_00B840_COMPUTE_DISPATCH_SCRATCH_BASE_LO, 2);
      radeon_emit(cs, scratch_va >> 8);
      radeon_emit(cs, scratch_va >> 40);

      waves /= gpu_info->max_se;
   }

   radeon_set_sh_reg_seq(cs, R_00B900_COMPUTE_USER_DATA_0, 2);
   radeon_emit(cs, scratch_va);
   radeon_emit(cs, rsrc1);

   radeon_set_sh_reg(cs, R_00B860_COMPUTE_TMPRING_SIZE,
                     S_00B860_WAVES(waves) |
                        S_00B860_WAVESIZE(DIV_ROUND_UP(size_per_wave, gpu_info->gfx_level >= GFX11 ? 256 : 1024)));
}

static void
radv_emit_compute_shader_pointers(struct radv_device *device, struct radeon_cmdbuf *cs,
                                  struct radeon_winsys_bo *descriptor_bo)
{
   if (!descriptor_bo)
      return;

   uint64_t va = radv_buffer_get_va(descriptor_bo);
   radv_cs_add_buffer(device->ws, cs, descriptor_bo);

   /* Compute shader user data 0-1 have the scratch pointer (unlike GFX shaders),
    * so emit the descriptor pointer to user data 2-3 instead (task_ring_offsets arg).
    */
   radv_emit_shader_pointer(device, cs, R_00B908_COMPUTE_USER_DATA_2, va, true);
}

static void
radv_emit_graphics_shader_pointers(struct radv_device *device, struct radeon_cmdbuf *cs,
                                   struct radeon_winsys_bo *descriptor_bo)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   uint64_t va;

   if (!descriptor_bo)
      return;

   va = radv_buffer_get_va(descriptor_bo);

   radv_cs_add_buffer(device->ws, cs, descriptor_bo);

   if (pdev->info.gfx_level >= GFX11) {
      uint32_t regs[] = {R_00B030_SPI_SHADER_USER_DATA_PS_0, R_00B420_SPI_SHADER_PGM_LO_HS,
                         R_00B220_SPI_SHADER_PGM_LO_GS};

      for (int i = 0; i < ARRAY_SIZE(regs); ++i) {
         radv_emit_shader_pointer(device, cs, regs[i], va, true);
      }
   } else if (pdev->info.gfx_level >= GFX10) {
      uint32_t regs[] = {R_00B030_SPI_SHADER_USER_DATA_PS_0, R_00B130_SPI_SHADER_USER_DATA_VS_0,
                         R_00B208_SPI_SHADER_USER_DATA_ADDR_LO_GS, R_00B408_SPI_SHADER_USER_DATA_ADDR_LO_HS};

      for (int i = 0; i < ARRAY_SIZE(regs); ++i) {
         radv_emit_shader_pointer(device, cs, regs[i], va, true);
      }
   } else if (pdev->info.gfx_level == GFX9) {
      uint32_t regs[] = {R_00B030_SPI_SHADER_USER_DATA_PS_0, R_00B130_SPI_SHADER_USER_DATA_VS_0,
                         R_00B208_SPI_SHADER_USER_DATA_ADDR_LO_GS, R_00B408_SPI_SHADER_USER_DATA_ADDR_LO_HS};

      for (int i = 0; i < ARRAY_SIZE(regs); ++i) {
         radv_emit_shader_pointer(device, cs, regs[i], va, true);
      }
   } else {
      uint32_t regs[] = {R_00B030_SPI_SHADER_USER_DATA_PS_0, R_00B130_SPI_SHADER_USER_DATA_VS_0,
                         R_00B230_SPI_SHADER_USER_DATA_GS_0, R_00B330_SPI_SHADER_USER_DATA_ES_0,
                         R_00B430_SPI_SHADER_USER_DATA_HS_0, R_00B530_SPI_SHADER_USER_DATA_LS_0};

      for (int i = 0; i < ARRAY_SIZE(regs); ++i) {
         radv_emit_shader_pointer(device, cs, regs[i], va, true);
      }
   }
}

static void
radv_emit_attribute_ring(struct radv_device *device, struct radeon_cmdbuf *cs, struct radeon_winsys_bo *attr_ring_bo,
                         uint32_t attr_ring_size)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   uint64_t va;

   if (!attr_ring_bo)
      return;

   assert(pdev->info.gfx_level >= GFX11);

   va = radv_buffer_get_va(attr_ring_bo);
   assert((va >> 32) == pdev->info.address32_hi);

   radv_cs_add_buffer(device->ws, cs, attr_ring_bo);

   /* We must wait for idle using an EOP event before changing the attribute ring registers. Use the
    * bottom-of-pipe EOP event, but increment the PWS counter instead of writing memory.
    */
   radeon_emit(cs, PKT3(PKT3_RELEASE_MEM, 6, 0));
   radeon_emit(cs, S_490_EVENT_TYPE(V_028A90_BOTTOM_OF_PIPE_TS) | S_490_EVENT_INDEX(5) | S_490_PWS_ENABLE(1));
   radeon_emit(cs, 0); /* DST_SEL, INT_SEL, DATA_SEL */
   radeon_emit(cs, 0); /* ADDRESS_LO */
   radeon_emit(cs, 0); /* ADDRESS_HI */
   radeon_emit(cs, 0); /* DATA_LO */
   radeon_emit(cs, 0); /* DATA_HI */
   radeon_emit(cs, 0); /* INT_CTXID */

   /* Wait for the PWS counter. */
   radeon_emit(cs, PKT3(PKT3_ACQUIRE_MEM, 6, 0));
   radeon_emit(cs, S_580_PWS_STAGE_SEL(V_580_CP_ME) | S_580_PWS_COUNTER_SEL(V_580_TS_SELECT) | S_580_PWS_ENA2(1) |
                      S_580_PWS_COUNT(0));
   radeon_emit(cs, 0xffffffff); /* GCR_SIZE */
   radeon_emit(cs, 0x01ffffff); /* GCR_SIZE_HI */
   radeon_emit(cs, 0);          /* GCR_BASE_LO */
   radeon_emit(cs, 0);          /* GCR_BASE_HI */
   radeon_emit(cs, S_585_PWS_ENA(1));
   radeon_emit(cs, 0); /* GCR_CNTL */

   /* The PS will read inputs from this address. */
   radeon_set_uconfig_reg(cs, R_031118_SPI_ATTRIBUTE_RING_BASE, va >> 16);
   radeon_set_uconfig_reg(cs, R_03111C_SPI_ATTRIBUTE_RING_SIZE,
                          S_03111C_MEM_SIZE(((attr_ring_size / pdev->info.max_se) >> 16) - 1) |
                             S_03111C_BIG_PAGE(pdev->info.discardable_allows_big_page) | S_03111C_L1_POLICY(1));
}

static void
radv_emit_compute(struct radv_device *device, struct radeon_cmdbuf *cs)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radeon_info *gpu_info = &pdev->info;

   radeon_set_sh_reg_seq(cs, R_00B810_COMPUTE_START_X, 3);
   radeon_emit(cs, 0);
   radeon_emit(cs, 0);
   radeon_emit(cs, 0);

   radeon_set_sh_reg(cs, R_00B834_COMPUTE_PGM_HI, S_00B834_DATA(pdev->info.address32_hi >> 8));

   radeon_set_sh_reg_seq(cs, R_00B858_COMPUTE_STATIC_THREAD_MGMT_SE0, 2);
   /* R_00B858_COMPUTE_STATIC_THREAD_MGMT_SE0 / SE1,
    * renamed COMPUTE_DESTINATION_EN_SEn on gfx10. */
   for (unsigned i = 0; i < 2; ++i) {
      unsigned cu_mask = i < gpu_info->max_se ? gpu_info->spi_cu_en : 0x0;
      radeon_emit(cs, S_00B8AC_SA0_CU_EN(cu_mask) | S_00B8AC_SA1_CU_EN(cu_mask));
   }

   if (pdev->info.gfx_level >= GFX7) {
      /* Also set R_00B858_COMPUTE_STATIC_THREAD_MGMT_SE2 / SE3 */
      radeon_set_sh_reg_seq(cs, R_00B864_COMPUTE_STATIC_THREAD_MGMT_SE2, 2);
      for (unsigned i = 2; i < 4; ++i) {
         unsigned cu_mask = i < gpu_info->max_se ? gpu_info->spi_cu_en : 0x0;
         radeon_emit(cs, S_00B8AC_SA0_CU_EN(cu_mask) | S_00B8AC_SA1_CU_EN(cu_mask));
      }

      if (device->border_color_data.bo) {
         uint64_t bc_va = radv_buffer_get_va(device->border_color_data.bo);

         radeon_set_uconfig_reg_seq(cs, R_030E00_TA_CS_BC_BASE_ADDR, 2);
         radeon_emit(cs, bc_va >> 8);
         radeon_emit(cs, S_030E04_ADDRESS(bc_va >> 40));
      }
   }

   if (pdev->info.gfx_level >= GFX9 && pdev->info.gfx_level < GFX11) {
      radeon_set_uconfig_reg(cs, R_0301EC_CP_COHER_START_DELAY, pdev->info.gfx_level >= GFX10 ? 0x20 : 0);
   }

   if (pdev->info.gfx_level >= GFX10) {
      radeon_set_sh_reg_seq(cs, R_00B890_COMPUTE_USER_ACCUM_0, 4);
      radeon_emit(cs, 0); /* R_00B890_COMPUTE_USER_ACCUM_0 */
      radeon_emit(cs, 0); /* R_00B894_COMPUTE_USER_ACCUM_1 */
      radeon_emit(cs, 0); /* R_00B898_COMPUTE_USER_ACCUM_2 */
      radeon_emit(cs, 0); /* R_00B89C_COMPUTE_USER_ACCUM_3 */

      radeon_set_sh_reg(cs, R_00B9F4_COMPUTE_DISPATCH_TUNNEL, 0);
   }

   if (pdev->info.gfx_level == GFX6) {
      if (device->border_color_data.bo) {
         uint64_t bc_va = radv_buffer_get_va(device->border_color_data.bo);
         radeon_set_config_reg(cs, R_00950C_TA_CS_BC_BASE_ADDR, bc_va >> 8);
      }
   }

   if (device->tma_bo) {
      uint64_t tba_va, tma_va;

      assert(pdev->info.gfx_level == GFX8);

      tba_va = radv_shader_get_va(device->trap_handler_shader);
      tma_va = radv_buffer_get_va(device->tma_bo);

      radeon_set_sh_reg_seq(cs, R_00B838_COMPUTE_TBA_LO, 4);
      radeon_emit(cs, tba_va >> 8);
      radeon_emit(cs, tba_va >> 40);
      radeon_emit(cs, tma_va >> 8);
      radeon_emit(cs, tma_va >> 40);
   }

   if (pdev->info.gfx_level >= GFX11) {
      radeon_set_sh_reg_seq(cs, R_00B8AC_COMPUTE_STATIC_THREAD_MGMT_SE4, 4);
      /* SE4-SE7 */
      for (unsigned i = 4; i < 8; ++i) {
         unsigned cu_mask = i < gpu_info->num_se ? gpu_info->spi_cu_en : 0x0;
         radeon_emit(cs, S_00B8AC_SA0_CU_EN(cu_mask) | S_00B8AC_SA1_CU_EN(cu_mask));
      }

      radeon_set_sh_reg(cs, R_00B8BC_COMPUTE_DISPATCH_INTERLEAVE, 64);
   }
}

static void
radv_write_harvested_raster_configs(struct radv_physical_device *pdev, struct radeon_cmdbuf *cs, unsigned raster_config,
                                    unsigned raster_config_1)
{
   unsigned num_se = MAX2(pdev->info.max_se, 1);
   unsigned raster_config_se[4];
   unsigned se;

   ac_get_harvested_configs(&pdev->info, raster_config, &raster_config_1, raster_config_se);

   for (se = 0; se < num_se; se++) {
      /* GRBM_GFX_INDEX has a different offset on GFX6 and GFX7+ */
      if (pdev->info.gfx_level < GFX7)
         radeon_set_config_reg(
            cs, R_00802C_GRBM_GFX_INDEX,
            S_00802C_SE_INDEX(se) | S_00802C_SH_BROADCAST_WRITES(1) | S_00802C_INSTANCE_BROADCAST_WRITES(1));
      else
         radeon_set_uconfig_reg(
            cs, R_030800_GRBM_GFX_INDEX,
            S_030800_SE_INDEX(se) | S_030800_SH_BROADCAST_WRITES(1) | S_030800_INSTANCE_BROADCAST_WRITES(1));
      radeon_set_context_reg(cs, R_028350_PA_SC_RASTER_CONFIG, raster_config_se[se]);
   }

   /* GRBM_GFX_INDEX has a different offset on GFX6 and GFX7+ */
   if (pdev->info.gfx_level < GFX7)
      radeon_set_config_reg(
         cs, R_00802C_GRBM_GFX_INDEX,
         S_00802C_SE_BROADCAST_WRITES(1) | S_00802C_SH_BROADCAST_WRITES(1) | S_00802C_INSTANCE_BROADCAST_WRITES(1));
   else
      radeon_set_uconfig_reg(
         cs, R_030800_GRBM_GFX_INDEX,
         S_030800_SE_BROADCAST_WRITES(1) | S_030800_SH_BROADCAST_WRITES(1) | S_030800_INSTANCE_BROADCAST_WRITES(1));

   if (pdev->info.gfx_level >= GFX7)
      radeon_set_context_reg(cs, R_028354_PA_SC_RASTER_CONFIG_1, raster_config_1);
}

static void
radv_set_raster_config(struct radv_physical_device *pdev, struct radeon_cmdbuf *cs)
{
   unsigned num_rb = MIN2(pdev->info.max_render_backends, 16);
   uint64_t rb_mask = pdev->info.enabled_rb_mask;
   unsigned raster_config, raster_config_1;

   ac_get_raster_config(&pdev->info, &raster_config, &raster_config_1, NULL);

   /* Always use the default config when all backends are enabled
    * (or when we failed to determine the enabled backends).
    */
   if (!rb_mask || util_bitcount64(rb_mask) >= num_rb) {
      radeon_set_context_reg(cs, R_028350_PA_SC_RASTER_CONFIG, raster_config);
      if (pdev->info.gfx_level >= GFX7)
         radeon_set_context_reg(cs, R_028354_PA_SC_RASTER_CONFIG_1, raster_config_1);
   } else {
      radv_write_harvested_raster_configs(pdev, cs, raster_config, raster_config_1);
   }
}

/* 12.4 fixed-point */
static unsigned
radv_pack_float_12p4(float x)
{
   return x <= 0 ? 0 : x >= 4096 ? 0xffff : x * 16;
}

void
radv_emit_graphics(struct radv_device *device, struct radeon_cmdbuf *cs)
{
   struct radv_physical_device *pdev = radv_device_physical(device);

   bool has_clear_state = pdev->info.has_clear_state;
   int i;

   if (!device->uses_shadow_regs) {
      radeon_emit(cs, PKT3(PKT3_CONTEXT_CONTROL, 1, 0));
      radeon_emit(cs, CC0_UPDATE_LOAD_ENABLES(1));
      radeon_emit(cs, CC1_UPDATE_SHADOW_ENABLES(1));

      if (has_clear_state) {
         radeon_emit(cs, PKT3(PKT3_CLEAR_STATE, 0, 0));
         radeon_emit(cs, 0);
      }
   }

   if (pdev->info.gfx_level <= GFX8)
      radv_set_raster_config(pdev, cs);

   /* Emulated in shader code on GFX9+. */
   if (pdev->info.gfx_level >= GFX9)
      radeon_set_context_reg(cs, R_028AAC_VGT_ESGS_RING_ITEMSIZE, 1);

   radeon_set_context_reg(cs, R_028A18_VGT_HOS_MAX_TESS_LEVEL, fui(64));
   if (!has_clear_state)
      radeon_set_context_reg(cs, R_028A1C_VGT_HOS_MIN_TESS_LEVEL, fui(0));

   /* FIXME calculate these values somehow ??? */
   if (pdev->info.gfx_level <= GFX8) {
      radeon_set_context_reg(cs, R_028A54_VGT_GS_PER_ES, SI_GS_PER_ES);
      radeon_set_context_reg(cs, R_028A58_VGT_ES_PER_GS, 0x40);
   }

   if (!has_clear_state) {
      if (pdev->info.gfx_level < GFX11) {
         radeon_set_context_reg(cs, R_028A5C_VGT_GS_PER_VS, 0x2);
         radeon_set_context_reg(cs, R_028B98_VGT_STRMOUT_BUFFER_CONFIG, 0x0);
      }
      radeon_set_context_reg(cs, R_028A8C_VGT_PRIMITIVEID_RESET, 0x0);
   }

   if (pdev->info.gfx_level <= GFX9)
      radeon_set_context_reg(cs, R_028AA0_VGT_INSTANCE_STEP_RATE_0, 1);
   if (!has_clear_state && pdev->info.gfx_level < GFX11)
      radeon_set_context_reg(cs, R_028AB8_VGT_VTX_CNT_EN, 0x0);
   if (pdev->info.gfx_level < GFX7)
      radeon_set_config_reg(cs, R_008A14_PA_CL_ENHANCE, S_008A14_NUM_CLIP_SEQ(3) | S_008A14_CLIP_VTX_REORDER_ENA(1));

   if (!has_clear_state)
      radeon_set_context_reg(cs, R_02882C_PA_SU_PRIM_FILTER_CNTL, 0);

   /* CLEAR_STATE doesn't clear these correctly on certain generations.
    * I don't know why. Deduced by trial and error.
    */
   if (pdev->info.gfx_level <= GFX7 || !has_clear_state) {
      radeon_set_context_reg(cs, R_028B28_VGT_STRMOUT_DRAW_OPAQUE_OFFSET, 0);
      radeon_set_context_reg(cs, R_028204_PA_SC_WINDOW_SCISSOR_TL, S_028204_WINDOW_OFFSET_DISABLE(1));
      radeon_set_context_reg(cs, R_028240_PA_SC_GENERIC_SCISSOR_TL, S_028240_WINDOW_OFFSET_DISABLE(1));
      radeon_set_context_reg(cs, R_028244_PA_SC_GENERIC_SCISSOR_BR,
                             S_028244_BR_X(MAX_FRAMEBUFFER_WIDTH) | S_028244_BR_Y(MAX_FRAMEBUFFER_HEIGHT));
      radeon_set_context_reg(cs, R_028030_PA_SC_SCREEN_SCISSOR_TL, 0);
   }

   if (!has_clear_state) {
      for (i = 0; i < 16; i++) {
         radeon_set_context_reg(cs, R_0282D0_PA_SC_VPORT_ZMIN_0 + i * 8, 0);
         radeon_set_context_reg(cs, R_0282D4_PA_SC_VPORT_ZMAX_0 + i * 8, fui(1.0));
      }
   }

   if (!has_clear_state) {
      radeon_set_context_reg(cs, R_02820C_PA_SC_CLIPRECT_RULE, 0xFFFF);
      radeon_set_context_reg(cs, R_028230_PA_SC_EDGERULE, 0xAAAAAAAA);
      /* PA_SU_HARDWARE_SCREEN_OFFSET must be 0 due to hw bug on GFX6 */
      radeon_set_context_reg(cs, R_028234_PA_SU_HARDWARE_SCREEN_OFFSET, 0);
      radeon_set_context_reg(cs, R_028820_PA_CL_NANINF_CNTL, 0);
      radeon_set_context_reg(cs, R_028AC0_DB_SRESULTS_COMPARE_STATE0, 0x0);
      radeon_set_context_reg(cs, R_028AC4_DB_SRESULTS_COMPARE_STATE1, 0x0);
      radeon_set_context_reg(cs, R_028AC8_DB_PRELOAD_CONTROL, 0x0);
   }

   radeon_set_context_reg(
      cs, R_02800C_DB_RENDER_OVERRIDE,
      S_02800C_FORCE_HIS_ENABLE0(V_02800C_FORCE_DISABLE) | S_02800C_FORCE_HIS_ENABLE1(V_02800C_FORCE_DISABLE));

   if (pdev->info.gfx_level >= GFX10) {
      radeon_set_context_reg(cs, R_028A98_VGT_DRAW_PAYLOAD_CNTL, 0);
      radeon_set_uconfig_reg(cs, R_030964_GE_MAX_VTX_INDX, ~0);
      radeon_set_uconfig_reg(cs, R_030924_GE_MIN_VTX_INDX, 0);
      radeon_set_uconfig_reg(cs, R_030928_GE_INDX_OFFSET, 0);
      radeon_set_uconfig_reg(cs, R_03097C_GE_STEREO_CNTL, 0);
      radeon_set_uconfig_reg(cs, R_030988_GE_USER_VGPR_EN, 0);

      if (pdev->info.gfx_level < GFX11) {
         radeon_set_context_reg(cs, R_028038_DB_DFSM_CONTROL, S_028038_PUNCHOUT_MODE(V_028038_FORCE_OFF));
      }
   } else if (pdev->info.gfx_level == GFX9) {
      radeon_set_uconfig_reg(cs, R_030920_VGT_MAX_VTX_INDX, ~0);
      radeon_set_uconfig_reg(cs, R_030924_VGT_MIN_VTX_INDX, 0);
      radeon_set_uconfig_reg(cs, R_030928_VGT_INDX_OFFSET, 0);

      radeon_set_context_reg(cs, R_028060_DB_DFSM_CONTROL, S_028060_PUNCHOUT_MODE(V_028060_FORCE_OFF));
   } else {
      /* These registers, when written, also overwrite the
       * CLEAR_STATE context, so we can't rely on CLEAR_STATE setting
       * them.  It would be an issue if there was another UMD
       * changing them.
       */
      radeon_set_context_reg(cs, R_028400_VGT_MAX_VTX_INDX, ~0);
      radeon_set_context_reg(cs, R_028404_VGT_MIN_VTX_INDX, 0);
      radeon_set_context_reg(cs, R_028408_VGT_INDX_OFFSET, 0);
   }

   if (pdev->info.gfx_level >= GFX10) {
      radeon_set_sh_reg(cs, R_00B524_SPI_SHADER_PGM_HI_LS, S_00B524_MEM_BASE(pdev->info.address32_hi >> 8));
      radeon_set_sh_reg(cs, R_00B324_SPI_SHADER_PGM_HI_ES, S_00B324_MEM_BASE(pdev->info.address32_hi >> 8));
   } else if (pdev->info.gfx_level == GFX9) {
      radeon_set_sh_reg(cs, R_00B414_SPI_SHADER_PGM_HI_LS, S_00B414_MEM_BASE(pdev->info.address32_hi >> 8));
      radeon_set_sh_reg(cs, R_00B214_SPI_SHADER_PGM_HI_ES, S_00B214_MEM_BASE(pdev->info.address32_hi >> 8));
   } else {
      radeon_set_sh_reg(cs, R_00B524_SPI_SHADER_PGM_HI_LS, S_00B524_MEM_BASE(pdev->info.address32_hi >> 8));
      radeon_set_sh_reg(cs, R_00B324_SPI_SHADER_PGM_HI_ES, S_00B324_MEM_BASE(pdev->info.address32_hi >> 8));
   }

   if (pdev->info.gfx_level < GFX11)
      radeon_set_sh_reg(cs, R_00B124_SPI_SHADER_PGM_HI_VS, S_00B124_MEM_BASE(pdev->info.address32_hi >> 8));

   unsigned cu_mask_ps = 0xffffffff;

   /* It's wasteful to enable all CUs for PS if shader arrays have a
    * different number of CUs. The reason is that the hardware sends the
    * same number of PS waves to each shader array, so the slowest shader
    * array limits the performance.  Disable the extra CUs for PS in
    * other shader arrays to save power and thus increase clocks for busy
    * CUs. In the future, we might disable or enable this tweak only for
    * certain apps.
    */
   if (pdev->info.gfx_level >= GFX10_3)
      cu_mask_ps = u_bit_consecutive(0, pdev->info.min_good_cu_per_sa);

   if (pdev->info.gfx_level >= GFX7) {
      if (pdev->info.gfx_level >= GFX10 && pdev->info.gfx_level < GFX11) {
         /* Logical CUs 16 - 31 */
         radeon_set_sh_reg_idx(pdev, cs, R_00B104_SPI_SHADER_PGM_RSRC4_VS, 3,
                               ac_apply_cu_en(S_00B104_CU_EN(0xffff), C_00B104_CU_EN, 16, &pdev->info));
      }

      if (pdev->info.gfx_level >= GFX10) {
         radeon_set_sh_reg_idx(pdev, cs, R_00B404_SPI_SHADER_PGM_RSRC4_HS, 3,
                               ac_apply_cu_en(S_00B404_CU_EN(0xffff), C_00B404_CU_EN, 16, &pdev->info));
         radeon_set_sh_reg_idx(pdev, cs, R_00B004_SPI_SHADER_PGM_RSRC4_PS, 3,
                               ac_apply_cu_en(S_00B004_CU_EN(cu_mask_ps >> 16), C_00B004_CU_EN, 16, &pdev->info));
      }

      if (pdev->info.gfx_level >= GFX9) {
         radeon_set_sh_reg_idx(
            pdev, cs, R_00B41C_SPI_SHADER_PGM_RSRC3_HS, 3,
            ac_apply_cu_en(S_00B41C_CU_EN(0xffff) | S_00B41C_WAVE_LIMIT(0x3F), C_00B41C_CU_EN, 0, &pdev->info));
      } else {
         radeon_set_sh_reg(
            cs, R_00B51C_SPI_SHADER_PGM_RSRC3_LS,
            ac_apply_cu_en(S_00B51C_CU_EN(0xffff) | S_00B51C_WAVE_LIMIT(0x3F), C_00B51C_CU_EN, 0, &pdev->info));
         radeon_set_sh_reg(cs, R_00B41C_SPI_SHADER_PGM_RSRC3_HS, S_00B41C_WAVE_LIMIT(0x3F));
         radeon_set_sh_reg(
            cs, R_00B31C_SPI_SHADER_PGM_RSRC3_ES,
            ac_apply_cu_en(S_00B31C_CU_EN(0xffff) | S_00B31C_WAVE_LIMIT(0x3F), C_00B31C_CU_EN, 0, &pdev->info));
         /* If this is 0, Bonaire can hang even if GS isn't being used.
          * Other chips are unaffected. These are suboptimal values,
          * but we don't use on-chip GS.
          */
         radeon_set_context_reg(cs, R_028A44_VGT_GS_ONCHIP_CNTL,
                                S_028A44_ES_VERTS_PER_SUBGRP(64) | S_028A44_GS_PRIMS_PER_SUBGRP(4));
      }

      radeon_set_sh_reg_idx(pdev, cs, R_00B01C_SPI_SHADER_PGM_RSRC3_PS, 3,
                            ac_apply_cu_en(S_00B01C_CU_EN(cu_mask_ps) | S_00B01C_WAVE_LIMIT(0x3F) |
                                              S_00B01C_LDS_GROUP_SIZE(pdev->info.gfx_level >= GFX11),
                                           C_00B01C_CU_EN, 0, &pdev->info));
   }

   if (pdev->info.gfx_level >= GFX10) {
      /* Break up a pixel wave if it contains deallocs for more than
       * half the parameter cache.
       *
       * To avoid a deadlock where pixel waves aren't launched
       * because they're waiting for more pixels while the frontend
       * is stuck waiting for PC space, the maximum allowed value is
       * the size of the PC minus the largest possible allocation for
       * a single primitive shader subgroup.
       */
      uint32_t max_deallocs_in_wave = pdev->info.gfx_level >= GFX11 ? 16 : 512;
      radeon_set_context_reg(cs, R_028C50_PA_SC_NGG_MODE_CNTL, S_028C50_MAX_DEALLOCS_IN_WAVE(max_deallocs_in_wave));

      if (pdev->info.gfx_level < GFX11)
         radeon_set_context_reg(cs, R_028C58_VGT_VERTEX_REUSE_BLOCK_CNTL, 14);

      /* Vulkan doesn't support user edge flags and it also doesn't
       * need to prevent drawing lines on internal edges of
       * decomposed primitives (such as quads) with polygon mode = lines.
       */
      unsigned vertex_reuse_depth = pdev->info.gfx_level >= GFX10_3 ? 30 : 0;
      radeon_set_context_reg(cs, R_028838_PA_CL_NGG_CNTL,
                             S_028838_INDEX_BUF_EDGE_FLAG_ENA(0) | S_028838_VERTEX_REUSE_DEPTH(vertex_reuse_depth));

      /* Enable CMASK/FMASK/HTILE/DCC caching in L2 for small chips. */
      unsigned meta_write_policy, meta_read_policy;
      unsigned no_alloc = pdev->info.gfx_level >= GFX11 ? V_02807C_CACHE_NOA_GFX11 : V_02807C_CACHE_NOA_GFX10;

      /* TODO: investigate whether LRU improves performance on other chips too */
      if (pdev->info.max_render_backends <= 4) {
         meta_write_policy = V_02807C_CACHE_LRU_WR; /* cache writes */
         meta_read_policy = V_02807C_CACHE_LRU_RD;  /* cache reads */
      } else {
         meta_write_policy = V_02807C_CACHE_STREAM; /* write combine */
         meta_read_policy = no_alloc;               /* don't cache reads */
      }

      radeon_set_context_reg(cs, R_02807C_DB_RMI_L2_CACHE_CONTROL,
                             S_02807C_Z_WR_POLICY(V_02807C_CACHE_STREAM) | S_02807C_S_WR_POLICY(V_02807C_CACHE_STREAM) |
                                S_02807C_HTILE_WR_POLICY(meta_write_policy) |
                                S_02807C_ZPCPSD_WR_POLICY(V_02807C_CACHE_STREAM) | S_02807C_Z_RD_POLICY(no_alloc) |
                                S_02807C_S_RD_POLICY(no_alloc) | S_02807C_HTILE_RD_POLICY(meta_read_policy));

      uint32_t gl2_cc;
      if (pdev->info.gfx_level >= GFX11) {
         gl2_cc = S_028410_DCC_WR_POLICY_GFX11(meta_write_policy) |
                  S_028410_COLOR_WR_POLICY_GFX11(V_028410_CACHE_STREAM) |
                  S_028410_COLOR_RD_POLICY(V_028410_CACHE_NOA_GFX11);
      } else {
         gl2_cc = S_028410_CMASK_WR_POLICY(meta_write_policy) | S_028410_FMASK_WR_POLICY(V_028410_CACHE_STREAM) |
                  S_028410_DCC_WR_POLICY_GFX10(meta_write_policy) |
                  S_028410_COLOR_WR_POLICY_GFX10(V_028410_CACHE_STREAM) | S_028410_CMASK_RD_POLICY(meta_read_policy) |
                  S_028410_FMASK_RD_POLICY(V_028410_CACHE_NOA_GFX10) |
                  S_028410_COLOR_RD_POLICY(V_028410_CACHE_NOA_GFX10);
      }

      radeon_set_context_reg(cs, R_028410_CB_RMI_GL2_CACHE_CONTROL, gl2_cc | S_028410_DCC_RD_POLICY(meta_read_policy));
      radeon_set_context_reg(cs, R_028428_CB_COVERAGE_OUT_CONTROL, 0);

      radeon_set_sh_reg_seq(cs, R_00B0C8_SPI_SHADER_USER_ACCUM_PS_0, 4);
      radeon_emit(cs, 0); /* R_00B0C8_SPI_SHADER_USER_ACCUM_PS_0 */
      radeon_emit(cs, 0); /* R_00B0CC_SPI_SHADER_USER_ACCUM_PS_1 */
      radeon_emit(cs, 0); /* R_00B0D0_SPI_SHADER_USER_ACCUM_PS_2 */
      radeon_emit(cs, 0); /* R_00B0D4_SPI_SHADER_USER_ACCUM_PS_3 */

      if (pdev->info.gfx_level < GFX11) {
         radeon_set_sh_reg_seq(cs, R_00B1C8_SPI_SHADER_USER_ACCUM_VS_0, 4);
         radeon_emit(cs, 0); /* R_00B1C8_SPI_SHADER_USER_ACCUM_VS_0 */
         radeon_emit(cs, 0); /* R_00B1CC_SPI_SHADER_USER_ACCUM_VS_1 */
         radeon_emit(cs, 0); /* R_00B1D0_SPI_SHADER_USER_ACCUM_VS_2 */
         radeon_emit(cs, 0); /* R_00B1D4_SPI_SHADER_USER_ACCUM_VS_3 */
      }

      radeon_set_sh_reg_seq(cs, R_00B2C8_SPI_SHADER_USER_ACCUM_ESGS_0, 4);
      radeon_emit(cs, 0); /* R_00B2C8_SPI_SHADER_USER_ACCUM_ESGS_0 */
      radeon_emit(cs, 0); /* R_00B2CC_SPI_SHADER_USER_ACCUM_ESGS_1 */
      radeon_emit(cs, 0); /* R_00B2D0_SPI_SHADER_USER_ACCUM_ESGS_2 */
      radeon_emit(cs, 0); /* R_00B2D4_SPI_SHADER_USER_ACCUM_ESGS_3 */
      radeon_set_sh_reg_seq(cs, R_00B4C8_SPI_SHADER_USER_ACCUM_LSHS_0, 4);
      radeon_emit(cs, 0); /* R_00B4C8_SPI_SHADER_USER_ACCUM_LSHS_0 */
      radeon_emit(cs, 0); /* R_00B4CC_SPI_SHADER_USER_ACCUM_LSHS_1 */
      radeon_emit(cs, 0); /* R_00B4D0_SPI_SHADER_USER_ACCUM_LSHS_2 */
      radeon_emit(cs, 0); /* R_00B4D4_SPI_SHADER_USER_ACCUM_LSHS_3 */

      radeon_set_sh_reg(cs, R_00B0C0_SPI_SHADER_REQ_CTRL_PS,
                        S_00B0C0_SOFT_GROUPING_EN(1) | S_00B0C0_NUMBER_OF_REQUESTS_PER_CU(4 - 1));

      if (pdev->info.gfx_level < GFX11)
         radeon_set_sh_reg(cs, R_00B1C0_SPI_SHADER_REQ_CTRL_VS, 0);

      if (pdev->info.gfx_level >= GFX10_3) {
         radeon_set_context_reg(cs, R_028750_SX_PS_DOWNCONVERT_CONTROL, 0xff);
         /* This allows sample shading. */
         radeon_set_context_reg(cs, R_028848_PA_CL_VRS_CNTL,
                                S_028848_SAMPLE_ITER_COMBINER_MODE(V_028848_SC_VRS_COMB_MODE_OVERRIDE));
      }
   }

   if (pdev->info.gfx_level >= GFX11) {
      /* ACCUM fields changed their meaning. */
      radeon_set_context_reg(cs, R_028B50_VGT_TESS_DISTRIBUTION,
                             S_028B50_ACCUM_ISOLINE(128) | S_028B50_ACCUM_TRI(128) | S_028B50_ACCUM_QUAD(128) |
                                S_028B50_DONUT_SPLIT_GFX9(24) | S_028B50_TRAP_SPLIT(6));
   } else if (pdev->info.gfx_level >= GFX9) {
      radeon_set_context_reg(cs, R_028B50_VGT_TESS_DISTRIBUTION,
                             S_028B50_ACCUM_ISOLINE(40) | S_028B50_ACCUM_TRI(30) | S_028B50_ACCUM_QUAD(24) |
                                S_028B50_DONUT_SPLIT_GFX9(24) | S_028B50_TRAP_SPLIT(6));
   } else if (pdev->info.gfx_level >= GFX8) {
      uint32_t vgt_tess_distribution;

      vgt_tess_distribution =
         S_028B50_ACCUM_ISOLINE(32) | S_028B50_ACCUM_TRI(11) | S_028B50_ACCUM_QUAD(11) | S_028B50_DONUT_SPLIT_GFX81(16);

      if (pdev->info.family == CHIP_FIJI || pdev->info.family >= CHIP_POLARIS10)
         vgt_tess_distribution |= S_028B50_TRAP_SPLIT(3);

      radeon_set_context_reg(cs, R_028B50_VGT_TESS_DISTRIBUTION, vgt_tess_distribution);
   } else if (!has_clear_state) {
      radeon_set_context_reg(cs, R_028C58_VGT_VERTEX_REUSE_BLOCK_CNTL, 14);
      radeon_set_context_reg(cs, R_028C5C_VGT_OUT_DEALLOC_CNTL, 16);
   }

   if (device->border_color_data.bo) {
      uint64_t border_color_va = radv_buffer_get_va(device->border_color_data.bo);

      radeon_set_context_reg(cs, R_028080_TA_BC_BASE_ADDR, border_color_va >> 8);
      if (pdev->info.gfx_level >= GFX7) {
         radeon_set_context_reg(cs, R_028084_TA_BC_BASE_ADDR_HI, S_028084_ADDRESS(border_color_va >> 40));
      }
   }

   if (pdev->info.gfx_level >= GFX8) {
      /* GFX8+ only compares the bits according to the index type by default,
       * so we can always leave the programmed value at the maximum.
       */
      radeon_set_context_reg(cs, R_02840C_VGT_MULTI_PRIM_IB_RESET_INDX, 0xffffffff);
   }

   if (pdev->info.gfx_level >= GFX9) {
      unsigned max_alloc_count = pdev->info.pbb_max_alloc_count;

      /* GFX11+ shouldn't subtract 1 from pbb_max_alloc_count.  */
      if (pdev->info.gfx_level < GFX11)
         max_alloc_count -= 1;

      radeon_set_context_reg(cs, R_028C48_PA_SC_BINNER_CNTL_1,
                             S_028C48_MAX_ALLOC_COUNT(max_alloc_count) | S_028C48_MAX_PRIM_PER_BATCH(1023));
      radeon_set_context_reg(cs, R_028C4C_PA_SC_CONSERVATIVE_RASTERIZATION_CNTL, S_028C4C_NULL_SQUAD_AA_MASK_ENABLE(1));
      radeon_set_uconfig_reg(cs, R_030968_VGT_INSTANCE_BASE_ID, 0);
   }

   unsigned tmp = (unsigned)(1.0 * 8.0);
   radeon_set_context_reg(cs, R_028A00_PA_SU_POINT_SIZE, S_028A00_HEIGHT(tmp) | S_028A00_WIDTH(tmp));
   radeon_set_context_reg(
      cs, R_028A04_PA_SU_POINT_MINMAX,
      S_028A04_MIN_SIZE(radv_pack_float_12p4(0)) | S_028A04_MAX_SIZE(radv_pack_float_12p4(8191.875 / 2)));

   if (!has_clear_state) {
      radeon_set_context_reg(cs, R_028004_DB_COUNT_CONTROL, S_028004_ZPASS_INCREMENT_DISABLE(1));
   }

   /* Enable the Polaris small primitive filter control.
    * XXX: There is possibly an issue when MSAA is off (see RadeonSI
    * has_msaa_sample_loc_bug). But this doesn't seem to regress anything,
    * and AMDVLK doesn't have a workaround as well.
    */
   if (pdev->info.family >= CHIP_POLARIS10) {
      unsigned small_prim_filter_cntl = S_028830_SMALL_PRIM_FILTER_ENABLE(1) |
                                        /* Workaround for a hw line bug. */
                                        S_028830_LINE_FILTER_DISABLE(pdev->info.family <= CHIP_POLARIS12);

      radeon_set_context_reg(cs, R_028830_PA_SU_SMALL_PRIM_FILTER_CNTL, small_prim_filter_cntl);
   }

   radeon_set_context_reg(cs, R_0286D4_SPI_INTERP_CONTROL_0,
                          S_0286D4_FLAT_SHADE_ENA(1) | S_0286D4_PNT_SPRITE_ENA(1) |
                             S_0286D4_PNT_SPRITE_OVRD_X(V_0286D4_SPI_PNT_SPRITE_SEL_S) |
                             S_0286D4_PNT_SPRITE_OVRD_Y(V_0286D4_SPI_PNT_SPRITE_SEL_T) |
                             S_0286D4_PNT_SPRITE_OVRD_Z(V_0286D4_SPI_PNT_SPRITE_SEL_0) |
                             S_0286D4_PNT_SPRITE_OVRD_W(V_0286D4_SPI_PNT_SPRITE_SEL_1) |
                             S_0286D4_PNT_SPRITE_TOP_1(0)); /* vulkan is top to bottom - 1.0 at bottom */

   radeon_set_context_reg(cs, R_028BE4_PA_SU_VTX_CNTL,
                          S_028BE4_PIX_CENTER(1) | S_028BE4_ROUND_MODE(V_028BE4_X_ROUND_TO_EVEN) |
                             S_028BE4_QUANT_MODE(V_028BE4_X_16_8_FIXED_POINT_1_256TH));

   radeon_set_context_reg(cs, R_028818_PA_CL_VTE_CNTL,
                          S_028818_VTX_W0_FMT(1) | S_028818_VPORT_X_SCALE_ENA(1) | S_028818_VPORT_X_OFFSET_ENA(1) |
                             S_028818_VPORT_Y_SCALE_ENA(1) | S_028818_VPORT_Y_OFFSET_ENA(1) |
                             S_028818_VPORT_Z_SCALE_ENA(1) | S_028818_VPORT_Z_OFFSET_ENA(1));

   if (device->tma_bo) {
      uint64_t tba_va, tma_va;

      assert(pdev->info.gfx_level == GFX8);

      tba_va = radv_shader_get_va(device->trap_handler_shader);
      tma_va = radv_buffer_get_va(device->tma_bo);

      uint32_t regs[] = {R_00B000_SPI_SHADER_TBA_LO_PS, R_00B100_SPI_SHADER_TBA_LO_VS, R_00B200_SPI_SHADER_TBA_LO_GS,
                         R_00B300_SPI_SHADER_TBA_LO_ES, R_00B400_SPI_SHADER_TBA_LO_HS, R_00B500_SPI_SHADER_TBA_LO_LS};

      for (i = 0; i < ARRAY_SIZE(regs); ++i) {
         radeon_set_sh_reg_seq(cs, regs[i], 4);
         radeon_emit(cs, tba_va >> 8);
         radeon_emit(cs, tba_va >> 40);
         radeon_emit(cs, tma_va >> 8);
         radeon_emit(cs, tma_va >> 40);
      }
   }

   if (pdev->info.gfx_level >= GFX11) {
      radeon_set_context_reg(cs, R_028C54_PA_SC_BINNER_CNTL_2,
                             S_028C54_ENABLE_PING_PONG_BIN_ORDER(pdev->info.gfx_level >= GFX11_5));

      uint64_t rb_mask = BITFIELD64_MASK(pdev->info.max_render_backends);

      radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 2, 0));
      radeon_emit(cs, EVENT_TYPE(V_028A90_PIXEL_PIPE_STAT_CONTROL) | EVENT_INDEX(1));
      radeon_emit(cs, PIXEL_PIPE_STATE_CNTL_COUNTER_ID(0) | PIXEL_PIPE_STATE_CNTL_STRIDE(2) |
                         PIXEL_PIPE_STATE_CNTL_INSTANCE_EN_LO(rb_mask));
      radeon_emit(cs, PIXEL_PIPE_STATE_CNTL_INSTANCE_EN_HI(rb_mask));

      radeon_set_uconfig_reg(cs, R_031110_SPI_GS_THROTTLE_CNTL1, 0x12355123);
      radeon_set_uconfig_reg(cs, R_031114_SPI_GS_THROTTLE_CNTL2, 0x1544D);
   }

   /* The exclusion bits can be set to improve rasterization efficiency if no sample lies on the
    * pixel boundary (-8 sample offset). It's currently always TRUE because the driver doesn't
    * support 16 samples.
    */
   bool exclusion = pdev->info.gfx_level >= GFX7;
   radeon_set_context_reg(cs, R_02882C_PA_SU_PRIM_FILTER_CNTL,
                          S_02882C_XMAX_RIGHT_EXCLUSION(exclusion) | S_02882C_YMAX_BOTTOM_EXCLUSION(exclusion));

   radeon_set_context_reg(cs, R_028828_PA_SU_LINE_STIPPLE_SCALE, 0x3f800000);
   if (pdev->info.gfx_level >= GFX7) {
      radeon_set_uconfig_reg(cs, R_030A00_PA_SU_LINE_STIPPLE_VALUE, 0);
      radeon_set_uconfig_reg(cs, R_030A04_PA_SC_LINE_STIPPLE_STATE, 0);
   } else {
      radeon_set_config_reg(cs, R_008A60_PA_SU_LINE_STIPPLE_VALUE, 0);
      radeon_set_config_reg(cs, R_008B10_PA_SC_LINE_STIPPLE_STATE, 0);
   }

   if (pdev->info.gfx_level >= GFX11) {
      /* Disable primitive restart for all non-indexed draws. */
      radeon_set_uconfig_reg(cs, R_03092C_GE_MULTI_PRIM_IB_RESET_EN, S_03092C_DISABLE_FOR_AUTO_INDEX(1));
   }

   radv_emit_compute(device, cs);
}

static void
radv_init_graphics_state(struct radeon_cmdbuf *cs, struct radv_device *device)
{
   if (device->gfx_init) {
      struct radeon_winsys *ws = device->ws;

      ws->cs_execute_ib(cs, device->gfx_init, 0, device->gfx_init_size_dw & 0xffff, false);

      radv_cs_add_buffer(device->ws, cs, device->gfx_init);
   } else {
      radv_emit_graphics(device, cs);
   }
}

static void
radv_init_compute_state(struct radeon_cmdbuf *cs, struct radv_device *device)
{
   radv_emit_compute(device, cs);
}

static VkResult
radv_update_preamble_cs(struct radv_queue_state *queue, struct radv_device *device,
                        const struct radv_queue_ring_info *needs)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_winsys *ws = device->ws;
   struct radeon_winsys_bo *scratch_bo = queue->scratch_bo;
   struct radeon_winsys_bo *descriptor_bo = queue->descriptor_bo;
   struct radeon_winsys_bo *compute_scratch_bo = queue->compute_scratch_bo;
   struct radeon_winsys_bo *esgs_ring_bo = queue->esgs_ring_bo;
   struct radeon_winsys_bo *gsvs_ring_bo = queue->gsvs_ring_bo;
   struct radeon_winsys_bo *tess_rings_bo = queue->tess_rings_bo;
   struct radeon_winsys_bo *task_rings_bo = queue->task_rings_bo;
   struct radeon_winsys_bo *mesh_scratch_ring_bo = queue->mesh_scratch_ring_bo;
   struct radeon_winsys_bo *attr_ring_bo = queue->attr_ring_bo;
   struct radeon_winsys_bo *gds_bo = queue->gds_bo;
   struct radeon_winsys_bo *gds_oa_bo = queue->gds_oa_bo;
   struct radeon_cmdbuf *dest_cs[3] = {0};
   const uint32_t ring_bo_flags = RADEON_FLAG_NO_CPU_ACCESS | RADEON_FLAG_NO_INTERPROCESS_SHARING;
   VkResult result = VK_SUCCESS;

   const bool add_sample_positions = !queue->ring_info.sample_positions && needs->sample_positions;
   const uint32_t scratch_size = needs->scratch_size_per_wave * needs->scratch_waves;
   const uint32_t queue_scratch_size = queue->ring_info.scratch_size_per_wave * queue->ring_info.scratch_waves;

   if (scratch_size > queue_scratch_size) {
      result = radv_bo_create(device, NULL, scratch_size, 4096, RADEON_DOMAIN_VRAM, ring_bo_flags,
                              RADV_BO_PRIORITY_SCRATCH, 0, true, &scratch_bo);
      if (result != VK_SUCCESS)
         goto fail;
      radv_rmv_log_command_buffer_bo_create(device, scratch_bo, 0, 0, scratch_size);
   }

   const uint32_t compute_scratch_size = needs->compute_scratch_size_per_wave * needs->compute_scratch_waves;
   const uint32_t compute_queue_scratch_size =
      queue->ring_info.compute_scratch_size_per_wave * queue->ring_info.compute_scratch_waves;
   if (compute_scratch_size > compute_queue_scratch_size) {
      result = radv_bo_create(device, NULL, compute_scratch_size, 4096, RADEON_DOMAIN_VRAM, ring_bo_flags,
                              RADV_BO_PRIORITY_SCRATCH, 0, true, &compute_scratch_bo);
      if (result != VK_SUCCESS)
         goto fail;
      radv_rmv_log_command_buffer_bo_create(device, compute_scratch_bo, 0, 0, compute_scratch_size);
   }

   if (needs->esgs_ring_size > queue->ring_info.esgs_ring_size) {
      result = radv_bo_create(device, NULL, needs->esgs_ring_size, 4096, RADEON_DOMAIN_VRAM, ring_bo_flags,
                              RADV_BO_PRIORITY_SCRATCH, 0, true, &esgs_ring_bo);
      if (result != VK_SUCCESS)
         goto fail;
      radv_rmv_log_command_buffer_bo_create(device, esgs_ring_bo, 0, 0, needs->esgs_ring_size);
   }

   if (needs->gsvs_ring_size > queue->ring_info.gsvs_ring_size) {
      result = radv_bo_create(device, NULL, needs->gsvs_ring_size, 4096, RADEON_DOMAIN_VRAM, ring_bo_flags,
                              RADV_BO_PRIORITY_SCRATCH, 0, true, &gsvs_ring_bo);
      if (result != VK_SUCCESS)
         goto fail;
      radv_rmv_log_command_buffer_bo_create(device, gsvs_ring_bo, 0, 0, needs->gsvs_ring_size);
   }

   if (!queue->ring_info.tess_rings && needs->tess_rings) {
      uint64_t tess_rings_size = pdev->hs.tess_offchip_ring_offset + pdev->hs.tess_offchip_ring_size;
      result = radv_bo_create(device, NULL, tess_rings_size, 256, RADEON_DOMAIN_VRAM, ring_bo_flags,
                              RADV_BO_PRIORITY_SCRATCH, 0, true, &tess_rings_bo);
      if (result != VK_SUCCESS)
         goto fail;
      radv_rmv_log_command_buffer_bo_create(device, tess_rings_bo, 0, 0, tess_rings_size);
   }

   if (!queue->ring_info.task_rings && needs->task_rings) {
      assert(pdev->info.gfx_level >= GFX10_3);

      /* We write the control buffer from the CPU, so need to grant CPU access to the BO.
       * The draw ring needs to be zero-initialized otherwise the ready bits will be incorrect.
       */
      uint32_t task_rings_bo_flags =
         RADEON_FLAG_CPU_ACCESS | RADEON_FLAG_NO_INTERPROCESS_SHARING | RADEON_FLAG_ZERO_VRAM;

      result = radv_bo_create(device, NULL, pdev->task_info.bo_size_bytes, 256, RADEON_DOMAIN_VRAM, task_rings_bo_flags,
                              RADV_BO_PRIORITY_SCRATCH, 0, true, &task_rings_bo);
      if (result != VK_SUCCESS)
         goto fail;
      radv_rmv_log_command_buffer_bo_create(device, task_rings_bo, 0, 0, pdev->task_info.bo_size_bytes);

      result = radv_initialise_task_control_buffer(device, task_rings_bo);
      if (result != VK_SUCCESS)
         goto fail;
   }

   if (!queue->ring_info.mesh_scratch_ring && needs->mesh_scratch_ring) {
      assert(pdev->info.gfx_level >= GFX10_3);
      result =
         radv_bo_create(device, NULL, RADV_MESH_SCRATCH_NUM_ENTRIES * RADV_MESH_SCRATCH_ENTRY_BYTES, 256,
                        RADEON_DOMAIN_VRAM, ring_bo_flags, RADV_BO_PRIORITY_SCRATCH, 0, true, &mesh_scratch_ring_bo);

      if (result != VK_SUCCESS)
         goto fail;
      radv_rmv_log_command_buffer_bo_create(device, mesh_scratch_ring_bo, 0, 0,
                                            RADV_MESH_SCRATCH_NUM_ENTRIES * RADV_MESH_SCRATCH_ENTRY_BYTES);
   }

   if (needs->attr_ring_size > queue->ring_info.attr_ring_size) {
      assert(pdev->info.gfx_level >= GFX11);
      result = radv_bo_create(device, NULL, needs->attr_ring_size, 2 * 1024 * 1024 /* 2MiB */, RADEON_DOMAIN_VRAM,
                              RADEON_FLAG_32BIT | RADEON_FLAG_DISCARDABLE | ring_bo_flags, RADV_BO_PRIORITY_SCRATCH, 0,
                              true, &attr_ring_bo);
      if (result != VK_SUCCESS)
         goto fail;
      radv_rmv_log_command_buffer_bo_create(device, attr_ring_bo, 0, 0, needs->attr_ring_size);
   }

   if (!queue->ring_info.gds && needs->gds) {
      assert(pdev->info.gfx_level >= GFX10);

      /* 4 streamout GDS counters.
       * We need 256B (64 dw) of GDS, otherwise streamout hangs.
       */
      result = radv_bo_create(device, NULL, 256, 4, RADEON_DOMAIN_GDS, ring_bo_flags, RADV_BO_PRIORITY_SCRATCH, 0, true,
                              &gds_bo);
      if (result != VK_SUCCESS)
         goto fail;

      /* Add the GDS BO to our global BO list to prevent the kernel to emit a GDS switch and reset
       * the state when a compute queue is used.
       */
      result = device->ws->buffer_make_resident(ws, gds_bo, true);
      if (result != VK_SUCCESS)
         goto fail;
   }

   if (!queue->ring_info.gds_oa && needs->gds_oa) {
      assert(pdev->info.gfx_level >= GFX10);

      result = radv_bo_create(device, NULL, 1, 1, RADEON_DOMAIN_OA, ring_bo_flags, RADV_BO_PRIORITY_SCRATCH, 0, true,
                              &gds_oa_bo);
      if (result != VK_SUCCESS)
         goto fail;

      /* Add the GDS OA BO to our global BO list to prevent the kernel to emit a GDS switch and
       * reset the state when a compute queue is used.
       */
      result = device->ws->buffer_make_resident(ws, gds_oa_bo, true);
      if (result != VK_SUCCESS)
         goto fail;
   }

   /* Re-initialize the descriptor BO when any ring BOs changed.
    *
    * Additionally, make sure to create the descriptor BO for the compute queue
    * when it uses the task shader rings. The task rings BO is shared between the
    * GFX and compute queues and already initialized here.
    */
   if ((queue->qf == RADV_QUEUE_COMPUTE && !descriptor_bo && task_rings_bo) || scratch_bo != queue->scratch_bo ||
       esgs_ring_bo != queue->esgs_ring_bo || gsvs_ring_bo != queue->gsvs_ring_bo ||
       tess_rings_bo != queue->tess_rings_bo || task_rings_bo != queue->task_rings_bo ||
       mesh_scratch_ring_bo != queue->mesh_scratch_ring_bo || attr_ring_bo != queue->attr_ring_bo ||
       add_sample_positions) {
      const uint32_t size = 304;

      result = radv_bo_create(device, NULL, size, 4096, RADEON_DOMAIN_VRAM,
                              RADEON_FLAG_CPU_ACCESS | RADEON_FLAG_NO_INTERPROCESS_SHARING | RADEON_FLAG_READ_ONLY,
                              RADV_BO_PRIORITY_DESCRIPTOR, 0, true, &descriptor_bo);
      if (result != VK_SUCCESS)
         goto fail;
   }

   if (descriptor_bo != queue->descriptor_bo) {
      uint32_t *map = (uint32_t *)radv_buffer_map(ws, descriptor_bo);
      if (!map) {
         result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
         goto fail;
      }

      radv_fill_shader_rings(device, map, scratch_bo, needs->esgs_ring_size, esgs_ring_bo, needs->gsvs_ring_size,
                             gsvs_ring_bo, tess_rings_bo, task_rings_bo, mesh_scratch_ring_bo, needs->attr_ring_size,
                             attr_ring_bo);

      ws->buffer_unmap(ws, descriptor_bo, false);
   }

   for (int i = 0; i < 3; ++i) {
      enum rgp_flush_bits sqtt_flush_bits = 0;
      struct radeon_cmdbuf *cs = NULL;
      cs = ws->cs_create(ws, radv_queue_family_to_ring(pdev, queue->qf), false);
      if (!cs) {
         result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
         goto fail;
      }

      radeon_check_space(ws, cs, 512);
      dest_cs[i] = cs;

      if (scratch_bo)
         radv_cs_add_buffer(ws, cs, scratch_bo);

      /* Emit initial configuration. */
      switch (queue->qf) {
      case RADV_QUEUE_GENERAL:
         if (queue->uses_shadow_regs)
            radv_emit_shadow_regs_preamble(cs, device, queue);
         radv_init_graphics_state(cs, device);

         if (esgs_ring_bo || gsvs_ring_bo || tess_rings_bo || task_rings_bo) {
            radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
            radeon_emit(cs, EVENT_TYPE(V_028A90_VS_PARTIAL_FLUSH) | EVENT_INDEX(4));

            radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
            radeon_emit(cs, EVENT_TYPE(V_028A90_VGT_FLUSH) | EVENT_INDEX(0));
         }

         radv_emit_gs_ring_sizes(device, cs, esgs_ring_bo, needs->esgs_ring_size, gsvs_ring_bo, needs->gsvs_ring_size);
         radv_emit_tess_factor_ring(device, cs, tess_rings_bo);
         radv_emit_task_rings(device, cs, task_rings_bo, false);
         radv_emit_attribute_ring(device, cs, attr_ring_bo, needs->attr_ring_size);
         radv_emit_graphics_shader_pointers(device, cs, descriptor_bo);
         radv_emit_compute_scratch(device, cs, needs->compute_scratch_size_per_wave, needs->compute_scratch_waves,
                                   compute_scratch_bo);
         radv_emit_graphics_scratch(device, cs, needs->scratch_size_per_wave, needs->scratch_waves, scratch_bo);
         break;
      case RADV_QUEUE_COMPUTE:
         radv_init_compute_state(cs, device);

         if (task_rings_bo) {
            radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
            radeon_emit(cs, EVENT_TYPE(V_028A90_CS_PARTIAL_FLUSH) | EVENT_INDEX(4));
         }

         radv_emit_task_rings(device, cs, task_rings_bo, true);
         radv_emit_compute_shader_pointers(device, cs, descriptor_bo);
         radv_emit_compute_scratch(device, cs, needs->compute_scratch_size_per_wave, needs->compute_scratch_waves,
                                   compute_scratch_bo);
         break;
      default:
         break;
      }

      if (i < 2) {
         /* The two initial preambles have a cache flush at the beginning. */
         const enum amd_gfx_level gfx_level = pdev->info.gfx_level;
         enum radv_cmd_flush_bits flush_bits = RADV_CMD_FLAG_INV_ICACHE | RADV_CMD_FLAG_INV_SCACHE |
                                               RADV_CMD_FLAG_INV_VCACHE | RADV_CMD_FLAG_INV_L2 |
                                               RADV_CMD_FLAG_START_PIPELINE_STATS;

         if (i == 0) {
            /* The full flush preamble should also wait for previous shader work to finish. */
            flush_bits |= RADV_CMD_FLAG_CS_PARTIAL_FLUSH;
            if (queue->qf == RADV_QUEUE_GENERAL)
               flush_bits |= RADV_CMD_FLAG_PS_PARTIAL_FLUSH;
         }

         radv_cs_emit_cache_flush(ws, cs, gfx_level, NULL, 0, queue->qf, flush_bits, &sqtt_flush_bits, 0);
      }

      result = ws->cs_finalize(cs);
      if (result != VK_SUCCESS)
         goto fail;
   }

   if (queue->initial_full_flush_preamble_cs)
      ws->cs_destroy(queue->initial_full_flush_preamble_cs);

   if (queue->initial_preamble_cs)
      ws->cs_destroy(queue->initial_preamble_cs);

   if (queue->continue_preamble_cs)
      ws->cs_destroy(queue->continue_preamble_cs);

   queue->initial_full_flush_preamble_cs = dest_cs[0];
   queue->initial_preamble_cs = dest_cs[1];
   queue->continue_preamble_cs = dest_cs[2];

   if (scratch_bo != queue->scratch_bo) {
      if (queue->scratch_bo) {
         radv_rmv_log_command_buffer_bo_destroy(device, queue->scratch_bo);
         radv_bo_destroy(device, NULL, queue->scratch_bo);
      }
      queue->scratch_bo = scratch_bo;
   }

   if (compute_scratch_bo != queue->compute_scratch_bo) {
      if (queue->compute_scratch_bo) {
         radv_rmv_log_command_buffer_bo_destroy(device, queue->compute_scratch_bo);
         radv_bo_destroy(device, NULL, queue->compute_scratch_bo);
      }
      queue->compute_scratch_bo = compute_scratch_bo;
   }

   if (esgs_ring_bo != queue->esgs_ring_bo) {
      if (queue->esgs_ring_bo) {
         radv_rmv_log_command_buffer_bo_destroy(device, queue->esgs_ring_bo);
         radv_bo_destroy(device, NULL, queue->esgs_ring_bo);
      }
      queue->esgs_ring_bo = esgs_ring_bo;
   }

   if (gsvs_ring_bo != queue->gsvs_ring_bo) {
      if (queue->gsvs_ring_bo) {
         radv_rmv_log_command_buffer_bo_destroy(device, queue->gsvs_ring_bo);
         radv_bo_destroy(device, NULL, queue->gsvs_ring_bo);
      }
      queue->gsvs_ring_bo = gsvs_ring_bo;
   }

   if (descriptor_bo != queue->descriptor_bo) {
      if (queue->descriptor_bo)
         radv_bo_destroy(device, NULL, queue->descriptor_bo);
      queue->descriptor_bo = descriptor_bo;
   }

   queue->tess_rings_bo = tess_rings_bo;
   queue->task_rings_bo = task_rings_bo;
   queue->mesh_scratch_ring_bo = mesh_scratch_ring_bo;
   queue->attr_ring_bo = attr_ring_bo;
   queue->gds_bo = gds_bo;
   queue->gds_oa_bo = gds_oa_bo;
   queue->ring_info = *needs;
   return VK_SUCCESS;
fail:
   for (int i = 0; i < ARRAY_SIZE(dest_cs); ++i)
      if (dest_cs[i])
         ws->cs_destroy(dest_cs[i]);
   if (descriptor_bo && descriptor_bo != queue->descriptor_bo)
      radv_bo_destroy(device, NULL, descriptor_bo);
   if (scratch_bo && scratch_bo != queue->scratch_bo)
      radv_bo_destroy(device, NULL, scratch_bo);
   if (compute_scratch_bo && compute_scratch_bo != queue->compute_scratch_bo)
      radv_bo_destroy(device, NULL, compute_scratch_bo);
   if (esgs_ring_bo && esgs_ring_bo != queue->esgs_ring_bo)
      radv_bo_destroy(device, NULL, esgs_ring_bo);
   if (gsvs_ring_bo && gsvs_ring_bo != queue->gsvs_ring_bo)
      radv_bo_destroy(device, NULL, gsvs_ring_bo);
   if (tess_rings_bo && tess_rings_bo != queue->tess_rings_bo)
      radv_bo_destroy(device, NULL, tess_rings_bo);
   if (task_rings_bo && task_rings_bo != queue->task_rings_bo)
      radv_bo_destroy(device, NULL, task_rings_bo);
   if (attr_ring_bo && attr_ring_bo != queue->attr_ring_bo)
      radv_bo_destroy(device, NULL, attr_ring_bo);
   if (gds_bo && gds_bo != queue->gds_bo) {
      ws->buffer_make_resident(ws, queue->gds_bo, false);
      radv_bo_destroy(device, NULL, gds_bo);
   }
   if (gds_oa_bo && gds_oa_bo != queue->gds_oa_bo) {
      ws->buffer_make_resident(ws, queue->gds_oa_bo, false);
      radv_bo_destroy(device, NULL, gds_oa_bo);
   }

   return vk_error(queue, result);
}

static VkResult
radv_update_preambles(struct radv_queue_state *queue, struct radv_device *device,
                      struct vk_command_buffer *const *cmd_buffers, uint32_t cmd_buffer_count, bool *use_perf_counters,
                      bool *has_follower)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   bool has_indirect_pipeline_binds = false;

   if (queue->qf != RADV_QUEUE_GENERAL && queue->qf != RADV_QUEUE_COMPUTE) {
      for (uint32_t j = 0; j < cmd_buffer_count; j++) {
         struct radv_cmd_buffer *cmd_buffer = container_of(cmd_buffers[j], struct radv_cmd_buffer, vk);

         *has_follower |= !!cmd_buffer->gang.cs;
      }

      return VK_SUCCESS;
   }

   /* Figure out the needs of the current submission.
    * Start by copying the queue's current info.
    * This is done because we only allow two possible behaviours for these buffers:
    * - Grow when the newly needed amount is larger than what we had
    * - Allocate the max size and reuse it, but don't free it until the queue is destroyed
    */
   struct radv_queue_ring_info needs = queue->ring_info;
   *use_perf_counters = false;
   *has_follower = false;

   for (uint32_t j = 0; j < cmd_buffer_count; j++) {
      struct radv_cmd_buffer *cmd_buffer = container_of(cmd_buffers[j], struct radv_cmd_buffer, vk);

      needs.scratch_size_per_wave = MAX2(needs.scratch_size_per_wave, cmd_buffer->scratch_size_per_wave_needed);
      needs.scratch_waves = MAX2(needs.scratch_waves, cmd_buffer->scratch_waves_wanted);
      needs.compute_scratch_size_per_wave =
         MAX2(needs.compute_scratch_size_per_wave, cmd_buffer->compute_scratch_size_per_wave_needed);
      needs.compute_scratch_waves = MAX2(needs.compute_scratch_waves, cmd_buffer->compute_scratch_waves_wanted);
      needs.esgs_ring_size = MAX2(needs.esgs_ring_size, cmd_buffer->esgs_ring_size_needed);
      needs.gsvs_ring_size = MAX2(needs.gsvs_ring_size, cmd_buffer->gsvs_ring_size_needed);
      needs.tess_rings |= cmd_buffer->tess_rings_needed;
      needs.task_rings |= cmd_buffer->task_rings_needed;
      needs.mesh_scratch_ring |= cmd_buffer->mesh_scratch_ring_needed;
      needs.gds |= cmd_buffer->gds_needed;
      needs.gds_oa |= cmd_buffer->gds_oa_needed;
      needs.sample_positions |= cmd_buffer->sample_positions_needed;
      *use_perf_counters |= cmd_buffer->state.uses_perf_counters;
      *has_follower |= !!cmd_buffer->gang.cs;

      has_indirect_pipeline_binds |= cmd_buffer->has_indirect_pipeline_binds;
   }

   if (has_indirect_pipeline_binds) {
      /* Use the maximum possible scratch size for indirect compute pipelines with DGC. */
      simple_mtx_lock(&device->compute_scratch_mtx);
      needs.compute_scratch_size_per_wave = MAX2(needs.compute_scratch_waves, device->compute_scratch_size_per_wave);
      needs.compute_scratch_waves = MAX2(needs.compute_scratch_waves, device->compute_scratch_waves);
      simple_mtx_unlock(&device->compute_scratch_mtx);
   }

   /* Sanitize scratch size information. */
   needs.scratch_waves =
      needs.scratch_size_per_wave ? MIN2(needs.scratch_waves, UINT32_MAX / needs.scratch_size_per_wave) : 0;
   needs.compute_scratch_waves =
      needs.compute_scratch_size_per_wave
         ? MIN2(needs.compute_scratch_waves, UINT32_MAX / needs.compute_scratch_size_per_wave)
         : 0;

   if (pdev->info.gfx_level >= GFX11 && queue->qf == RADV_QUEUE_GENERAL) {
      needs.attr_ring_size = pdev->info.attribute_ring_size_per_se * pdev->info.max_se;
   }

   /* Return early if we already match these needs.
    * Note that it's not possible for any of the needed values to be less
    * than what the queue already had, because we only ever increase the allocated size.
    */
   if (queue->initial_full_flush_preamble_cs && queue->ring_info.scratch_size_per_wave == needs.scratch_size_per_wave &&
       queue->ring_info.scratch_waves == needs.scratch_waves &&
       queue->ring_info.compute_scratch_size_per_wave == needs.compute_scratch_size_per_wave &&
       queue->ring_info.compute_scratch_waves == needs.compute_scratch_waves &&
       queue->ring_info.esgs_ring_size == needs.esgs_ring_size &&
       queue->ring_info.gsvs_ring_size == needs.gsvs_ring_size && queue->ring_info.tess_rings == needs.tess_rings &&
       queue->ring_info.task_rings == needs.task_rings &&
       queue->ring_info.mesh_scratch_ring == needs.mesh_scratch_ring &&
       queue->ring_info.attr_ring_size == needs.attr_ring_size && queue->ring_info.gds == needs.gds &&
       queue->ring_info.gds_oa == needs.gds_oa && queue->ring_info.sample_positions == needs.sample_positions)
      return VK_SUCCESS;

   return radv_update_preamble_cs(queue, device, &needs);
}

static VkResult
radv_create_gang_wait_preambles_postambles(struct radv_queue *queue)
{
   struct radv_device *device = radv_queue_device(queue);
   const struct radv_physical_device *pdev = radv_device_physical(device);

   if (queue->gang_sem_bo)
      return VK_SUCCESS;

   VkResult r = VK_SUCCESS;
   struct radeon_winsys *ws = device->ws;
   const enum amd_ip_type leader_ip = radv_queue_family_to_ring(pdev, queue->state.qf);
   struct radeon_winsys_bo *gang_sem_bo = NULL;

   /* Gang semaphores BO.
    * DWORD 0: used in preambles, gang leader writes, gang members wait.
    * DWORD 1: used in postambles, gang leader waits, gang members write.
    */
   r = radv_bo_create(device, NULL, 8, 4, RADEON_DOMAIN_VRAM,
                      RADEON_FLAG_NO_INTERPROCESS_SHARING | RADEON_FLAG_ZERO_VRAM, RADV_BO_PRIORITY_SCRATCH, 0, true,
                      &gang_sem_bo);
   if (r != VK_SUCCESS)
      return r;

   struct radeon_cmdbuf *leader_pre_cs = ws->cs_create(ws, leader_ip, false);
   struct radeon_cmdbuf *leader_post_cs = ws->cs_create(ws, leader_ip, false);
   struct radeon_cmdbuf *ace_pre_cs = ws->cs_create(ws, AMD_IP_COMPUTE, false);
   struct radeon_cmdbuf *ace_post_cs = ws->cs_create(ws, AMD_IP_COMPUTE, false);

   if (!leader_pre_cs || !leader_post_cs || !ace_pre_cs || !ace_post_cs) {
      r = VK_ERROR_OUT_OF_DEVICE_MEMORY;
      goto fail;
   }

   radeon_check_space(ws, leader_pre_cs, 256);
   radeon_check_space(ws, leader_post_cs, 256);
   radeon_check_space(ws, ace_pre_cs, 256);
   radeon_check_space(ws, ace_post_cs, 256);

   radv_cs_add_buffer(ws, leader_pre_cs, gang_sem_bo);
   radv_cs_add_buffer(ws, leader_post_cs, gang_sem_bo);
   radv_cs_add_buffer(ws, ace_pre_cs, gang_sem_bo);
   radv_cs_add_buffer(ws, ace_post_cs, gang_sem_bo);

   const uint64_t ace_wait_va = radv_buffer_get_va(gang_sem_bo);
   const uint64_t leader_wait_va = ace_wait_va + 4;
   const uint32_t zero = 0;
   const uint32_t one = 1;

   /* Preambles for gang submission.
    * Make gang members wait until the gang leader starts.
    * Userspace is required to emit this wait to make sure it behaves correctly
    * in a multi-process environment, because task shader dispatches are not
    * meant to be executed on multiple compute engines at the same time.
    */
   radv_cp_wait_mem(ace_pre_cs, RADV_QUEUE_COMPUTE, WAIT_REG_MEM_GREATER_OR_EQUAL, ace_wait_va, 1, 0xffffffff);
   radv_cs_write_data(device, ace_pre_cs, RADV_QUEUE_COMPUTE, V_370_ME, ace_wait_va, 1, &zero, false);
   radv_cs_write_data(device, leader_pre_cs, queue->state.qf, V_370_ME, ace_wait_va, 1, &one, false);

   /* Create postambles for gang submission.
    * This ensures that the gang leader waits for the whole gang,
    * which is necessary because the kernel signals the userspace fence
    * as soon as the gang leader is done, which may lead to bugs because the
    * same command buffers could be submitted again while still being executed.
    */
   radv_cp_wait_mem(leader_post_cs, queue->state.qf, WAIT_REG_MEM_GREATER_OR_EQUAL, leader_wait_va, 1, 0xffffffff);
   radv_cs_write_data(device, leader_post_cs, queue->state.qf, V_370_ME, leader_wait_va, 1, &zero, false);
   radv_cs_emit_write_event_eop(ace_post_cs, pdev->info.gfx_level, RADV_QUEUE_COMPUTE, V_028A90_BOTTOM_OF_PIPE_TS, 0,
                                EOP_DST_SEL_MEM, EOP_DATA_SEL_VALUE_32BIT, leader_wait_va, 1, 0);

   r = ws->cs_finalize(leader_pre_cs);
   if (r != VK_SUCCESS)
      goto fail;
   r = ws->cs_finalize(leader_post_cs);
   if (r != VK_SUCCESS)
      goto fail;
   r = ws->cs_finalize(ace_pre_cs);
   if (r != VK_SUCCESS)
      goto fail;
   r = ws->cs_finalize(ace_post_cs);
   if (r != VK_SUCCESS)
      goto fail;

   queue->gang_sem_bo = gang_sem_bo;
   queue->state.gang_wait_preamble_cs = leader_pre_cs;
   queue->state.gang_wait_postamble_cs = leader_post_cs;
   queue->follower_state->gang_wait_preamble_cs = ace_pre_cs;
   queue->follower_state->gang_wait_postamble_cs = ace_post_cs;

   return VK_SUCCESS;

fail:
   if (leader_pre_cs)
      ws->cs_destroy(leader_pre_cs);
   if (leader_post_cs)
      ws->cs_destroy(leader_post_cs);
   if (ace_pre_cs)
      ws->cs_destroy(ace_pre_cs);
   if (ace_post_cs)
      ws->cs_destroy(ace_post_cs);
   if (gang_sem_bo)
      radv_bo_destroy(device, &queue->vk.base, gang_sem_bo);

   return r;
}

static bool
radv_queue_init_follower_state(struct radv_queue *queue)
{
   if (queue->follower_state)
      return true;

   queue->follower_state = calloc(1, sizeof(struct radv_queue_state));
   if (!queue->follower_state)
      return false;

   queue->follower_state->qf = RADV_QUEUE_COMPUTE;
   return true;
}

static VkResult
radv_update_gang_preambles(struct radv_queue *queue)
{
   struct radv_device *device = radv_queue_device(queue);

   if (!radv_queue_init_follower_state(queue))
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   VkResult r = VK_SUCCESS;

   /* Copy task rings state.
    * Task shaders that are submitted on the ACE queue need to share
    * their ring buffers with the mesh shaders on the GFX queue.
    */
   queue->follower_state->ring_info.task_rings = queue->state.ring_info.task_rings;
   queue->follower_state->task_rings_bo = queue->state.task_rings_bo;

   /* Copy some needed states from the parent queue state.
    * These can only increase so it's okay to copy them as-is without checking.
    * Note, task shaders use the scratch size from their graphics pipeline.
    */
   struct radv_queue_ring_info needs = queue->follower_state->ring_info;
   needs.compute_scratch_size_per_wave = queue->state.ring_info.scratch_size_per_wave;
   needs.compute_scratch_waves = queue->state.ring_info.scratch_waves;
   needs.task_rings = queue->state.ring_info.task_rings;

   r = radv_update_preamble_cs(queue->follower_state, device, &needs);
   if (r != VK_SUCCESS)
      return r;

   r = radv_create_gang_wait_preambles_postambles(queue);
   if (r != VK_SUCCESS)
      return r;

   return VK_SUCCESS;
}

static struct radeon_cmdbuf *
radv_create_perf_counter_lock_cs(struct radv_device *device, unsigned pass, bool unlock)
{
   struct radeon_cmdbuf **cs_ref = &device->perf_counter_lock_cs[pass * 2 + (unlock ? 1 : 0)];
   struct radeon_cmdbuf *cs;

   if (*cs_ref)
      return *cs_ref;

   cs = device->ws->cs_create(device->ws, AMD_IP_GFX, false);
   if (!cs)
      return NULL;

   ASSERTED unsigned cdw = radeon_check_space(device->ws, cs, 21);

   radv_cs_add_buffer(device->ws, cs, device->perf_counter_bo);

   if (!unlock) {
      uint64_t mutex_va = radv_buffer_get_va(device->perf_counter_bo) + PERF_CTR_BO_LOCK_OFFSET;
      radeon_emit(cs, PKT3(PKT3_ATOMIC_MEM, 7, 0));
      radeon_emit(cs, ATOMIC_OP(TC_OP_ATOMIC_CMPSWAP_32) | ATOMIC_COMMAND(ATOMIC_COMMAND_LOOP));
      radeon_emit(cs, mutex_va);       /* addr lo */
      radeon_emit(cs, mutex_va >> 32); /* addr hi */
      radeon_emit(cs, 1);              /* data lo */
      radeon_emit(cs, 0);              /* data hi */
      radeon_emit(cs, 0);              /* compare data lo */
      radeon_emit(cs, 0);              /* compare data hi */
      radeon_emit(cs, 10);             /* loop interval */
   }

   uint64_t va = radv_buffer_get_va(device->perf_counter_bo) + PERF_CTR_BO_PASS_OFFSET;
   uint64_t unset_va = va + (unlock ? 8 * pass : 0);
   uint64_t set_va = va + (unlock ? 0 : 8 * pass);

   radeon_emit(cs, PKT3(PKT3_COPY_DATA, 4, 0));
   radeon_emit(cs, COPY_DATA_SRC_SEL(COPY_DATA_IMM) | COPY_DATA_DST_SEL(COPY_DATA_DST_MEM) | COPY_DATA_COUNT_SEL |
                      COPY_DATA_WR_CONFIRM);
   radeon_emit(cs, 0); /* immediate */
   radeon_emit(cs, 0);
   radeon_emit(cs, unset_va);
   radeon_emit(cs, unset_va >> 32);

   radeon_emit(cs, PKT3(PKT3_COPY_DATA, 4, 0));
   radeon_emit(cs, COPY_DATA_SRC_SEL(COPY_DATA_IMM) | COPY_DATA_DST_SEL(COPY_DATA_DST_MEM) | COPY_DATA_COUNT_SEL |
                      COPY_DATA_WR_CONFIRM);
   radeon_emit(cs, 1); /* immediate */
   radeon_emit(cs, 0);
   radeon_emit(cs, set_va);
   radeon_emit(cs, set_va >> 32);

   if (unlock) {
      uint64_t mutex_va = radv_buffer_get_va(device->perf_counter_bo) + PERF_CTR_BO_LOCK_OFFSET;

      radeon_emit(cs, PKT3(PKT3_COPY_DATA, 4, 0));
      radeon_emit(cs, COPY_DATA_SRC_SEL(COPY_DATA_IMM) | COPY_DATA_DST_SEL(COPY_DATA_DST_MEM) | COPY_DATA_COUNT_SEL |
                         COPY_DATA_WR_CONFIRM);
      radeon_emit(cs, 0); /* immediate */
      radeon_emit(cs, 0);
      radeon_emit(cs, mutex_va);
      radeon_emit(cs, mutex_va >> 32);
   }

   assert(cs->cdw <= cdw);

   VkResult result = device->ws->cs_finalize(cs);
   if (result != VK_SUCCESS) {
      device->ws->cs_destroy(cs);
      return NULL;
   }

   /* All the casts are to avoid MSVC errors around pointer truncation in a non-taken
    * alternative.
    */
   if (p_atomic_cmpxchg((uintptr_t *)cs_ref, 0, (uintptr_t)cs) != 0) {
      device->ws->cs_destroy(cs);
   }

   return *cs_ref;
}

static void
radv_get_shader_upload_sync_wait(struct radv_device *device, uint64_t shader_upload_seq,
                                 struct vk_sync_wait *out_sync_wait)
{
   struct vk_semaphore *semaphore = vk_semaphore_from_handle(device->shader_upload_sem);
   struct vk_sync *sync = vk_semaphore_get_active_sync(semaphore);
   *out_sync_wait = (struct vk_sync_wait){
      .sync = sync,
      .wait_value = shader_upload_seq,
      .stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
   };
}

static VkResult
radv_queue_submit_normal(struct radv_queue *queue, struct vk_queue_submit *submission)
{
   struct radv_device *device = radv_queue_device(queue);
   struct radeon_winsys_ctx *ctx = queue->hw_ctx;
   bool use_ace = false;
   bool use_perf_counters = false;
   VkResult result;
   uint64_t shader_upload_seq = 0;
   uint32_t wait_count = submission->wait_count;
   struct vk_sync_wait *waits = submission->waits;

   result = radv_update_preambles(&queue->state, device, submission->command_buffers, submission->command_buffer_count,
                                  &use_perf_counters, &use_ace);
   if (result != VK_SUCCESS)
      return result;

   if (use_ace) {
      result = radv_update_gang_preambles(queue);
      if (result != VK_SUCCESS)
         return result;
   }

   const unsigned cmd_buffer_count = submission->command_buffer_count;
   const unsigned max_cs_submission = radv_device_fault_detection_enabled(device) ? 1 : cmd_buffer_count;
   const unsigned cs_array_size = (use_ace ? 2 : 1) * MIN2(max_cs_submission, cmd_buffer_count);

   struct radeon_cmdbuf **cs_array = malloc(sizeof(struct radeon_cmdbuf *) * cs_array_size);
   if (!cs_array)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   if (radv_device_fault_detection_enabled(device))
      simple_mtx_lock(&device->trace_mtx);

   for (uint32_t j = 0; j < submission->command_buffer_count; j++) {
      struct radv_cmd_buffer *cmd_buffer = (struct radv_cmd_buffer *)submission->command_buffers[j];
      shader_upload_seq = MAX2(shader_upload_seq, cmd_buffer->shader_upload_seq);
   }

   if (shader_upload_seq > queue->last_shader_upload_seq) {
      /* Patch the wait array to add waiting for referenced shaders to upload. */
      struct vk_sync_wait *new_waits = malloc(sizeof(struct vk_sync_wait) * (wait_count + 1));
      if (!new_waits) {
         result = VK_ERROR_OUT_OF_HOST_MEMORY;
         goto fail;
      }

      memcpy(new_waits, submission->waits, sizeof(struct vk_sync_wait) * submission->wait_count);
      radv_get_shader_upload_sync_wait(device, shader_upload_seq, &new_waits[submission->wait_count]);

      waits = new_waits;
      wait_count += 1;
   }

   /* For fences on the same queue/vm amdgpu doesn't wait till all processing is finished
    * before starting the next cmdbuffer, so we need to do it here.
    */
   const bool need_wait = wait_count > 0;
   unsigned num_initial_preambles = 0;
   unsigned num_continue_preambles = 0;
   unsigned num_postambles = 0;
   struct radeon_cmdbuf *initial_preambles[5] = {0};
   struct radeon_cmdbuf *continue_preambles[5] = {0};
   struct radeon_cmdbuf *postambles[3] = {0};

   if (queue->state.qf == RADV_QUEUE_GENERAL || queue->state.qf == RADV_QUEUE_COMPUTE) {
      initial_preambles[num_initial_preambles++] =
         need_wait ? queue->state.initial_full_flush_preamble_cs : queue->state.initial_preamble_cs;

      continue_preambles[num_continue_preambles++] = queue->state.continue_preamble_cs;

      if (use_perf_counters) {
         /* RADV only supports perf counters on the GFX queue currently. */
         assert(queue->state.qf == RADV_QUEUE_GENERAL);

         /* Create the lock/unlock CS. */
         struct radeon_cmdbuf *perf_ctr_lock_cs =
            radv_create_perf_counter_lock_cs(device, submission->perf_pass_index, false);
         struct radeon_cmdbuf *perf_ctr_unlock_cs =
            radv_create_perf_counter_lock_cs(device, submission->perf_pass_index, true);

         if (!perf_ctr_lock_cs || !perf_ctr_unlock_cs) {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto fail;
         }

         initial_preambles[num_initial_preambles++] = perf_ctr_lock_cs;
         continue_preambles[num_continue_preambles++] = perf_ctr_lock_cs;
         postambles[num_postambles++] = perf_ctr_unlock_cs;
      }
   }

   const unsigned num_1q_initial_preambles = num_initial_preambles;
   const unsigned num_1q_continue_preambles = num_continue_preambles;
   const unsigned num_1q_postambles = num_postambles;

   if (use_ace) {
      initial_preambles[num_initial_preambles++] = queue->state.gang_wait_preamble_cs;
      initial_preambles[num_initial_preambles++] = queue->follower_state->gang_wait_preamble_cs;
      initial_preambles[num_initial_preambles++] =
         need_wait ? queue->follower_state->initial_full_flush_preamble_cs : queue->follower_state->initial_preamble_cs;

      continue_preambles[num_continue_preambles++] = queue->state.gang_wait_preamble_cs;
      continue_preambles[num_continue_preambles++] = queue->follower_state->gang_wait_preamble_cs;
      continue_preambles[num_continue_preambles++] = queue->follower_state->continue_preamble_cs;

      postambles[num_postambles++] = queue->follower_state->gang_wait_postamble_cs;
      postambles[num_postambles++] = queue->state.gang_wait_postamble_cs;
   }

   struct radv_winsys_submit_info submit = {
      .ip_type = radv_queue_ring(queue),
      .queue_index = queue->vk.index_in_family,
      .cs_array = cs_array,
      .cs_count = 0,
      .initial_preamble_count = num_1q_initial_preambles,
      .continue_preamble_count = num_1q_continue_preambles,
      .postamble_count = num_1q_postambles,
      .initial_preamble_cs = initial_preambles,
      .continue_preamble_cs = continue_preambles,
      .postamble_cs = postambles,
      .uses_shadow_regs = queue->state.uses_shadow_regs,
   };

   for (uint32_t j = 0, advance; j < cmd_buffer_count; j += advance) {
      advance = MIN2(max_cs_submission, cmd_buffer_count - j);
      const bool last_submit = j + advance == cmd_buffer_count;
      bool submit_ace = false;
      unsigned num_submitted_cs = 0;

      if (radv_device_fault_detection_enabled(device))
         *device->trace_id_ptr = 0;

      struct radeon_cmdbuf *chainable = NULL;
      struct radeon_cmdbuf *chainable_ace = NULL;

      /* Add CS from submitted command buffers. */
      for (unsigned c = 0; c < advance; ++c) {
         struct radv_cmd_buffer *cmd_buffer = (struct radv_cmd_buffer *)submission->command_buffers[j + c];
         assert(cmd_buffer->vk.level == VK_COMMAND_BUFFER_LEVEL_PRIMARY);
         const bool can_chain_next = !(cmd_buffer->usage_flags & VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

         /* Follower needs to be before the gang leader because the last CS must match the queue's IP type. */
         if (cmd_buffer->gang.cs) {
            device->ws->cs_unchain(cmd_buffer->gang.cs);
            if (!chainable_ace || !device->ws->cs_chain(chainable_ace, cmd_buffer->gang.cs, false)) {
               cs_array[num_submitted_cs++] = cmd_buffer->gang.cs;

               /* Prevent chaining the gang leader when the follower couldn't be chained.
                * Otherwise, they would be in the wrong order.
                */
               chainable = NULL;
            }

            chainable_ace = can_chain_next ? cmd_buffer->gang.cs : NULL;
            submit_ace = true;
         }

         device->ws->cs_unchain(cmd_buffer->cs);
         if (!chainable || !device->ws->cs_chain(chainable, cmd_buffer->cs, queue->state.uses_shadow_regs)) {
            /* don't submit empty command buffers to the kernel. */
            if ((radv_queue_ring(queue) != AMD_IP_VCN_ENC && radv_queue_ring(queue) != AMD_IP_UVD) ||
                cmd_buffer->cs->cdw != 0)
               cs_array[num_submitted_cs++] = cmd_buffer->cs;
         }

         chainable = can_chain_next ? cmd_buffer->cs : NULL;
      }

      submit.cs_count = num_submitted_cs;
      submit.initial_preamble_count = submit_ace ? num_initial_preambles : num_1q_initial_preambles;
      submit.continue_preamble_count = submit_ace ? num_continue_preambles : num_1q_continue_preambles;
      submit.postamble_count = submit_ace ? num_postambles : num_1q_postambles;

      result = device->ws->cs_submit(ctx, &submit, j == 0 ? wait_count : 0, waits,
                                     last_submit ? submission->signal_count : 0, submission->signals);

      if (result != VK_SUCCESS)
         goto fail;

      if (radv_device_fault_detection_enabled(device)) {
         radv_check_gpu_hangs(queue, &submit);
      }

      if (device->tma_bo) {
         radv_check_trap_handler(queue);
      }

      initial_preambles[0] = queue->state.initial_preamble_cs;
      initial_preambles[1] = !use_ace ? NULL : queue->follower_state->initial_preamble_cs;
   }

   queue->last_shader_upload_seq = MAX2(queue->last_shader_upload_seq, shader_upload_seq);

   radv_dump_printf_data(device, stdout);

fail:
   free(cs_array);
   if (waits != submission->waits)
      free(waits);
   if (radv_device_fault_detection_enabled(device))
      simple_mtx_unlock(&device->trace_mtx);

   return result;
}

static void
radv_report_gpuvm_fault(struct radv_device *device)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radv_winsys_gpuvm_fault_info fault_info = {0};

   if (!radv_vm_fault_occurred(device, &fault_info))
      return;

   fprintf(stderr, "radv: GPUVM fault detected at address 0x%08" PRIx64 ".\n", fault_info.addr);
   ac_print_gpuvm_fault_status(stderr, pdev->info.gfx_level, fault_info.status);
}

static VkResult
radv_queue_sparse_submit(struct vk_queue *vqueue, struct vk_queue_submit *submission)
{
   struct radv_queue *queue = (struct radv_queue *)vqueue;
   struct radv_device *device = radv_queue_device(queue);
   VkResult result;

   result = radv_queue_submit_bind_sparse_memory(device, submission);
   if (result != VK_SUCCESS)
      goto fail;

   /* We do a CPU wait here, in part to avoid more winsys mechanisms. In the likely kernel explicit
    * sync mechanism, we'd need to do a CPU wait anyway. Haven't seen this be a perf issue yet, but
    * we have to make sure the queue always has its submission thread enabled. */
   result = vk_sync_wait_many(&device->vk, submission->wait_count, submission->waits, 0, UINT64_MAX);
   if (result != VK_SUCCESS)
      goto fail;

   /* Ignore all the commandbuffers. They're necessarily empty anyway. */

   for (unsigned i = 0; i < submission->signal_count; ++i) {
      result = vk_sync_signal(&device->vk, submission->signals[i].sync, submission->signals[i].signal_value);
      if (result != VK_SUCCESS)
         goto fail;
   }

fail:
   if (result != VK_SUCCESS) {
      /* When something bad happened during the submission, such as
       * an out of memory issue, it might be hard to recover from
       * this inconsistent state. To avoid this sort of problem, we
       * assume that we are in a really bad situation and return
       * VK_ERROR_DEVICE_LOST to ensure the clients do not attempt
       * to submit the same job again to this device.
       */
      radv_report_gpuvm_fault(device);
      result = vk_device_set_lost(&device->vk, "vkQueueSubmit() failed");
   }
   return result;
}

static VkResult
radv_queue_submit(struct vk_queue *vqueue, struct vk_queue_submit *submission)
{
   struct radv_queue *queue = (struct radv_queue *)vqueue;
   struct radv_device *device = radv_queue_device(queue);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   VkResult result;

   if (!radv_sparse_queue_enabled(pdev)) {
      result = radv_queue_submit_bind_sparse_memory(device, submission);
      if (result != VK_SUCCESS)
         goto fail;
   } else {
      assert(!submission->buffer_bind_count && !submission->image_bind_count && !submission->image_opaque_bind_count);
   }

   if (!submission->command_buffer_count && !submission->wait_count && !submission->signal_count)
      return VK_SUCCESS;

   if (!submission->command_buffer_count) {
      result = radv_queue_submit_empty(queue, submission);
   } else {
      result = radv_queue_submit_normal(queue, submission);
   }

fail:
   if (result != VK_SUCCESS) {
      /* When something bad happened during the submission, such as
       * an out of memory issue, it might be hard to recover from
       * this inconsistent state. To avoid this sort of problem, we
       * assume that we are in a really bad situation and return
       * VK_ERROR_DEVICE_LOST to ensure the clients do not attempt
       * to submit the same job again to this device.
       */
      radv_report_gpuvm_fault(device);
      result = vk_device_set_lost(&device->vk, "vkQueueSubmit() failed");
   }
   return result;
}

bool
radv_queue_internal_submit(struct radv_queue *queue, struct radeon_cmdbuf *cs)
{
   struct radv_device *device = radv_queue_device(queue);
   struct radeon_winsys_ctx *ctx = queue->hw_ctx;
   struct radv_winsys_submit_info submit = {
      .ip_type = radv_queue_ring(queue),
      .queue_index = queue->vk.index_in_family,
      .cs_array = &cs,
      .cs_count = 1,
   };

   VkResult result = device->ws->cs_submit(ctx, &submit, 0, NULL, 0, NULL);
   if (result != VK_SUCCESS)
      return false;

   return true;
}

int
radv_queue_init(struct radv_device *device, struct radv_queue *queue, int idx,
                const VkDeviceQueueCreateInfo *create_info,
                const VkDeviceQueueGlobalPriorityCreateInfoKHR *global_priority)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);

   queue->priority = radv_get_queue_global_priority(global_priority);
   queue->hw_ctx = device->hw_ctx[queue->priority];
   queue->state.qf = vk_queue_to_radv(pdev, create_info->queueFamilyIndex);
   queue->gang_sem_bo = NULL;

   VkResult result = vk_queue_init(&queue->vk, &device->vk, create_info, idx);
   if (result != VK_SUCCESS)
      return result;

   queue->state.uses_shadow_regs = device->uses_shadow_regs && queue->state.qf == RADV_QUEUE_GENERAL;
   if (queue->state.uses_shadow_regs) {
      result = radv_create_shadow_regs_preamble(device, &queue->state);
      if (result != VK_SUCCESS)
         goto fail;
      result = radv_init_shadowed_regs_buffer_state(device, queue);
      if (result != VK_SUCCESS)
         goto fail;
   }

   if (queue->state.qf == RADV_QUEUE_SPARSE) {
      queue->vk.driver_submit = radv_queue_sparse_submit;
      vk_queue_enable_submit_thread(&queue->vk);
   } else {
      queue->vk.driver_submit = radv_queue_submit;
   }
   return VK_SUCCESS;
fail:
   vk_queue_finish(&queue->vk);
   return result;
}

static void
radv_queue_state_finish(struct radv_queue_state *queue, struct radv_device *device)
{
   radv_destroy_shadow_regs_preamble(device, queue, device->ws);
   if (queue->initial_full_flush_preamble_cs)
      device->ws->cs_destroy(queue->initial_full_flush_preamble_cs);
   if (queue->initial_preamble_cs)
      device->ws->cs_destroy(queue->initial_preamble_cs);
   if (queue->continue_preamble_cs)
      device->ws->cs_destroy(queue->continue_preamble_cs);
   if (queue->gang_wait_preamble_cs)
      device->ws->cs_destroy(queue->gang_wait_preamble_cs);
   if (queue->gang_wait_postamble_cs)
      device->ws->cs_destroy(queue->gang_wait_postamble_cs);
   if (queue->descriptor_bo)
      radv_bo_destroy(device, NULL, queue->descriptor_bo);
   if (queue->scratch_bo) {
      radv_rmv_log_command_buffer_bo_destroy(device, queue->scratch_bo);
      radv_bo_destroy(device, NULL, queue->scratch_bo);
   }
   if (queue->esgs_ring_bo) {
      radv_rmv_log_command_buffer_bo_destroy(device, queue->esgs_ring_bo);
      radv_bo_destroy(device, NULL, queue->esgs_ring_bo);
   }
   if (queue->gsvs_ring_bo) {
      radv_rmv_log_command_buffer_bo_destroy(device, queue->gsvs_ring_bo);
      radv_bo_destroy(device, NULL, queue->gsvs_ring_bo);
   }
   if (queue->tess_rings_bo) {
      radv_rmv_log_command_buffer_bo_destroy(device, queue->tess_rings_bo);
      radv_bo_destroy(device, NULL, queue->tess_rings_bo);
   }
   if (queue->task_rings_bo) {
      radv_rmv_log_command_buffer_bo_destroy(device, queue->task_rings_bo);
      radv_bo_destroy(device, NULL, queue->task_rings_bo);
   }
   if (queue->mesh_scratch_ring_bo) {
      radv_rmv_log_command_buffer_bo_destroy(device, queue->mesh_scratch_ring_bo);
      radv_bo_destroy(device, NULL, queue->mesh_scratch_ring_bo);
   }
   if (queue->attr_ring_bo) {
      radv_rmv_log_command_buffer_bo_destroy(device, queue->attr_ring_bo);
      radv_bo_destroy(device, NULL, queue->attr_ring_bo);
   }
   if (queue->gds_bo) {
      device->ws->buffer_make_resident(device->ws, queue->gds_bo, false);
      radv_bo_destroy(device, NULL, queue->gds_bo);
   }
   if (queue->gds_oa_bo) {
      device->ws->buffer_make_resident(device->ws, queue->gds_oa_bo, false);
      radv_bo_destroy(device, NULL, queue->gds_oa_bo);
   }
   if (queue->compute_scratch_bo) {
      radv_rmv_log_command_buffer_bo_destroy(device, queue->compute_scratch_bo);
      radv_bo_destroy(device, NULL, queue->compute_scratch_bo);
   }
}

void
radv_queue_finish(struct radv_queue *queue)
{
   struct radv_device *device = radv_queue_device(queue);

   if (queue->follower_state) {
      /* Prevent double free */
      queue->follower_state->task_rings_bo = NULL;

      /* Clean up the internal ACE queue state. */
      radv_queue_state_finish(queue->follower_state, device);
      free(queue->follower_state);
   }

   if (queue->gang_sem_bo)
      radv_bo_destroy(device, &queue->vk.base, queue->gang_sem_bo);

   radv_queue_state_finish(&queue->state, device);
   vk_queue_finish(&queue->vk);
}

enum amd_ip_type
radv_queue_ring(const struct radv_queue *queue)
{
   struct radv_device *device = radv_queue_device(queue);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   return radv_queue_family_to_ring(pdev, queue->state.qf);
}

enum amd_ip_type
radv_queue_family_to_ring(const struct radv_physical_device *pdev, enum radv_queue_family f)
{
   switch (f) {
   case RADV_QUEUE_GENERAL:
      return AMD_IP_GFX;
   case RADV_QUEUE_COMPUTE:
      return AMD_IP_COMPUTE;
   case RADV_QUEUE_TRANSFER:
      return AMD_IP_SDMA;
   case RADV_QUEUE_VIDEO_DEC:
      return pdev->vid_decode_ip;
   case RADV_QUEUE_VIDEO_ENC:
      return AMD_IP_VCN_ENC;
   default:
      unreachable("Unknown queue family");
   }
}
