/*
 * Copyright © 2022 Collabora Ltd. and Red Hat Inc.
 * SPDX-License-Identifier: MIT
 */
#include "nvk_buffer.h"
#include "nvk_cmd_buffer.h"
#include "nvk_descriptor_set.h"
#include "nvk_device.h"
#include "nvk_entrypoints.h"
#include "nvk_mme.h"
#include "nvk_physical_device.h"
#include "nvk_shader.h"

#include "nouveau_context.h"

#include "cla0b5.h"
#include "cla1c0.h"
#include "clc0c0.h"
#include "clc5c0.h"
#include "nvk_cl90c0.h"
#include "nvk_cl9097.h"
#include "nvk_cla0c0.h"
#include "nvk_clb0c0.h"
#include "nvk_clb1c0.h"
#include "nvk_clc3c0.h"
#include "nvk_clc597.h"
#include "nvk_clc6c0.h"

VkResult
nvk_push_dispatch_state_init(struct nvk_device *dev, struct nv_push *p)
{
   struct nvk_physical_device *pdev = nvk_device_physical(dev);

   P_MTHD(p, NV90C0, SET_OBJECT);
   P_NV90C0_SET_OBJECT(p, {
      .class_id = pdev->info.cls_compute,
      .engine_id = 0,
   });

   if (pdev->info.cls_compute == MAXWELL_COMPUTE_A)
      P_IMMD(p, NVB0C0, SET_SELECT_MAXWELL_TEXTURE_HEADERS, V_TRUE);

   if (pdev->info.cls_eng3d < VOLTA_COMPUTE_A) {
      uint64_t shader_base_addr =
         nvk_heap_contiguous_base_address(&dev->shader_heap);

      P_MTHD(p, NVA0C0, SET_PROGRAM_REGION_A);
      P_NVA0C0_SET_PROGRAM_REGION_A(p, shader_base_addr >> 32);
      P_NVA0C0_SET_PROGRAM_REGION_B(p, shader_base_addr);
   }

   return VK_SUCCESS;
}

static inline uint16_t
nvk_cmd_buffer_compute_cls(struct nvk_cmd_buffer *cmd)
{
   struct nvk_device *dev = nvk_cmd_buffer_device(cmd);
   struct nvk_physical_device *pdev = nvk_device_physical(dev);
   return pdev->info.cls_compute;
}

void
nvk_cmd_buffer_begin_compute(struct nvk_cmd_buffer *cmd,
                             const VkCommandBufferBeginInfo *pBeginInfo)
{
   if (cmd->vk.level == VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
      struct nv_push *p = nvk_cmd_buffer_push(cmd, 6);
      if (nvk_cmd_buffer_compute_cls(cmd) >= MAXWELL_COMPUTE_B) {
         P_IMMD(p, NVB1C0, INVALIDATE_SKED_CACHES, 0);
      }
      P_IMMD(p, NVA0C0, INVALIDATE_SAMPLER_CACHE_NO_WFI, {
         .lines = LINES_ALL,
      });
      P_IMMD(p, NVA0C0, INVALIDATE_TEXTURE_HEADER_CACHE_NO_WFI, {
         .lines = LINES_ALL,
      });
   }
}

void
nvk_cmd_invalidate_compute_state(struct nvk_cmd_buffer *cmd)
{
   memset(&cmd->state.cs, 0, sizeof(cmd->state.cs));
}

void
nvk_cmd_bind_compute_shader(struct nvk_cmd_buffer *cmd,
                            struct nvk_shader *shader)
{
   cmd->state.cs.shader = shader;
}

static uint32_t
nvk_compute_local_size(struct nvk_cmd_buffer *cmd)
{
   const struct nvk_shader *shader = cmd->state.cs.shader;

   return shader->info.cs.local_size[0] *
          shader->info.cs.local_size[1] *
          shader->info.cs.local_size[2];
}

static uint64_t
nvk_flush_compute_state(struct nvk_cmd_buffer *cmd,
                        uint64_t *root_desc_addr_out)
{
   struct nvk_device *dev = nvk_cmd_buffer_device(cmd);
   struct nvk_physical_device *pdev = nvk_device_physical(dev);
   const uint32_t min_cbuf_alignment = nvk_min_cbuf_alignment(&pdev->info);
   const struct nvk_shader *shader = cmd->state.cs.shader;
   struct nvk_descriptor_state *desc = &cmd->state.cs.descriptors;
   VkResult result;

   nvk_cmd_buffer_flush_push_descriptors(cmd, desc);

   /* pre Pascal the constant buffer sizes need to be 0x100 aligned. As we
    * simply allocated a buffer and upload data to it, make sure its size is
    * 0x100 aligned.
    */
   STATIC_ASSERT((sizeof(desc->root) & 0xff) == 0);
   assert(sizeof(desc->root) % min_cbuf_alignment == 0);

   void *root_desc_map;
   uint64_t root_desc_addr;
   result = nvk_cmd_buffer_upload_alloc(cmd, sizeof(desc->root),
                                        min_cbuf_alignment,
                                        &root_desc_addr, &root_desc_map);
   if (unlikely(result != VK_SUCCESS)) {
      vk_command_buffer_set_error(&cmd->vk, result);
      return 0;
   }

   desc->root.root_desc_addr = root_desc_addr;
   memcpy(root_desc_map, &desc->root, sizeof(desc->root));

   struct nak_qmd_info qmd_info = {
      .addr = shader->hdr_addr,
      .smem_size = shader->info.cs.smem_size,
      .smem_max = NVK_MAX_SHARED_SIZE,
      .global_size = {
         desc->root.cs.group_count[0],
         desc->root.cs.group_count[1],
         desc->root.cs.group_count[2],
      },
   };

   assert(shader->cbuf_map.cbuf_count <= ARRAY_SIZE(qmd_info.cbufs));
   for (uint32_t c = 0; c < shader->cbuf_map.cbuf_count; c++) {
      const struct nvk_cbuf *cbuf = &shader->cbuf_map.cbufs[c];

      struct nvk_buffer_address ba;
      if (cbuf->type == NVK_CBUF_TYPE_ROOT_DESC) {
         ba = (struct nvk_buffer_address) {
            .base_addr = root_desc_addr,
            .size = sizeof(desc->root),
         };
      } else {
         ASSERTED bool direct_descriptor =
            nvk_cmd_buffer_get_cbuf_descriptor(cmd, desc, shader, cbuf, &ba);
         assert(direct_descriptor);
      }

      if (ba.size > 0) {
         assert(ba.base_addr % min_cbuf_alignment == 0);
         ba.size = align(ba.size, min_cbuf_alignment);
         ba.size = MIN2(ba.size, NVK_MAX_CBUF_SIZE);

         qmd_info.cbufs[qmd_info.num_cbufs++] = (struct nak_qmd_cbuf) {
            .index = c,
            .addr = ba.base_addr,
            .size = ba.size,
         };
      }
   }

   uint32_t qmd[64];
   nak_fill_qmd(&pdev->info, &shader->info, &qmd_info, qmd, sizeof(qmd));

   uint64_t qmd_addr;
   result = nvk_cmd_buffer_upload_data(cmd, qmd, sizeof(qmd), 0x100, &qmd_addr);
   if (unlikely(result != VK_SUCCESS)) {
      vk_command_buffer_set_error(&cmd->vk, result);
      return 0;
   }

   if (root_desc_addr_out != NULL)
      *root_desc_addr_out = root_desc_addr;

   return qmd_addr;
}

static void
nvk_build_mme_add_cs_invocations(struct mme_builder *b,
                                 struct mme_value64 count)
{
   struct mme_value accum_hi = nvk_mme_load_scratch(b, CS_INVOCATIONS_HI);
   struct mme_value accum_lo = nvk_mme_load_scratch(b, CS_INVOCATIONS_LO);
   struct mme_value64 accum = mme_value64(accum_lo, accum_hi);

   accum = mme_add64(b, accum, count);

   STATIC_ASSERT(NVK_MME_SCRATCH_CS_INVOCATIONS_HI + 1 ==
                 NVK_MME_SCRATCH_CS_INVOCATIONS_LO);

   mme_mthd(b, NVC597_SET_MME_SHADOW_SCRATCH(NVK_MME_SCRATCH_CS_INVOCATIONS_HI));
   mme_emit(b, accum.hi);
   mme_emit(b, accum.lo);
}

void
nvk_mme_add_cs_invocations(struct mme_builder *b)
{
   struct mme_value64 count = mme_load_addr64(b);

   nvk_build_mme_add_cs_invocations(b, count);
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdDispatchBase(VkCommandBuffer commandBuffer,
                    uint32_t baseGroupX,
                    uint32_t baseGroupY,
                    uint32_t baseGroupZ,
                    uint32_t groupCountX,
                    uint32_t groupCountY,
                    uint32_t groupCountZ)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   struct nvk_descriptor_state *desc = &cmd->state.cs.descriptors;

   desc->root.cs.base_group[0] = baseGroupX;
   desc->root.cs.base_group[1] = baseGroupY;
   desc->root.cs.base_group[2] = baseGroupZ;
   desc->root.cs.group_count[0] = groupCountX;
   desc->root.cs.group_count[1] = groupCountY;
   desc->root.cs.group_count[2] = groupCountZ;

   uint64_t qmd_addr = nvk_flush_compute_state(cmd, NULL);
   if (unlikely(qmd_addr == 0))
      return;

   const uint32_t local_size = nvk_compute_local_size(cmd);
   const uint64_t cs_invocations =
      (uint64_t)local_size * (uint64_t)groupCountX *
      (uint64_t)groupCountY * (uint64_t)groupCountZ;

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 7);

   P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_ADD_CS_INVOCATIONS));
   P_INLINE_DATA(p, cs_invocations >> 32);
   P_INLINE_DATA(p, cs_invocations);

   P_MTHD(p, NVA0C0, SEND_PCAS_A);
   P_NVA0C0_SEND_PCAS_A(p, qmd_addr >> 8);

   if (nvk_cmd_buffer_compute_cls(cmd) <= TURING_COMPUTE_A) {
      P_IMMD(p, NVA0C0, SEND_SIGNALING_PCAS_B, {
            .invalidate = INVALIDATE_TRUE,
            .schedule = SCHEDULE_TRUE
      });
   } else {
      P_IMMD(p, NVC6C0, SEND_SIGNALING_PCAS2_B,
             PCAS_ACTION_INVALIDATE_COPY_SCHEDULE);
   }
}

static void
mme_store_global(struct mme_builder *b,
                 struct mme_value64 addr,
                 uint64_t offset,
                 struct mme_value v)
{
   if (offset > 0)
      addr = mme_add64(b, addr, mme_imm64(offset));

   mme_mthd(b, NV9097_SET_REPORT_SEMAPHORE_A);
   mme_emit_addr64(b, addr);
   mme_emit(b, v);
   mme_emit(b, mme_imm(0x10000000));

   if (offset > 0) {
      mme_free_reg(b, addr.lo);
      mme_free_reg(b, addr.hi);
   }
}

static void
mme_store_global_vec3(struct mme_builder *b,
                      struct mme_value64 addr,
                      uint32_t offset,
                      struct mme_value x,
                      struct mme_value y,
                      struct mme_value z)
{
   mme_store_global(b, addr, offset + 0, x);
   mme_store_global(b, addr, offset + 4, y);
   mme_store_global(b, addr, offset + 8, z);
}

void
nvk_mme_dispatch_indirect(struct mme_builder *b)
{
   if (b->devinfo->cls_eng3d < TURING_A)
      return;

   struct mme_value local_size = mme_load(b);
   struct mme_value64 dispatch_addr = mme_load_addr64(b);
   struct mme_value64 root_desc_addr = mme_load_addr64(b);
   struct mme_value64 qmd_addr = mme_load_addr64(b);

   mme_tu104_read_fifoed(b, dispatch_addr, mme_imm(3));

   uint32_t qmd_size_offset = nak_qmd_dispatch_size_offset(b->devinfo);
   uint32_t root_desc_size_offset =
      offsetof(struct nvk_root_descriptor_table, cs.group_count);

   struct mme_value group_count_x = mme_load(b);
   struct mme_value group_count_y = mme_load(b);
   struct mme_value group_count_z = mme_load(b);

   struct mme_value64 cs1 = mme_umul_32x32_64(b, local_size, group_count_x);
   struct mme_value64 cs2 = mme_umul_32x32_64(b, group_count_y, group_count_z);
   nvk_build_mme_add_cs_invocations(b, mme_mul64(b, cs1, cs2));

   mme_store_global_vec3(b, qmd_addr, qmd_size_offset,
                         group_count_x, group_count_y, group_count_z);
   mme_store_global_vec3(b, root_desc_addr, root_desc_size_offset,
                         group_count_x, group_count_y, group_count_z);
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdDispatchIndirect(VkCommandBuffer commandBuffer,
                        VkBuffer _buffer,
                        VkDeviceSize offset)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   VK_FROM_HANDLE(nvk_buffer, buffer, _buffer);
   struct nvk_descriptor_state *desc = &cmd->state.cs.descriptors;

   /* TODO: Indirect dispatch pre-Turing */
   assert(nvk_cmd_buffer_compute_cls(cmd) >= TURING_COMPUTE_A);

   desc->root.cs.base_group[0] = 0;
   desc->root.cs.base_group[1] = 0;
   desc->root.cs.base_group[2] = 0;

   uint64_t dispatch_addr = nvk_buffer_address(buffer, offset);

   uint64_t root_desc_addr;
   uint64_t qmd_addr = nvk_flush_compute_state(cmd, &root_desc_addr);
   if (unlikely(qmd_addr == 0))
      return;

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 14);

   P_IMMD(p, NVC597, SET_MME_DATA_FIFO_CONFIG, FIFO_SIZE_SIZE_4KB);
   P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_DISPATCH_INDIRECT));
   P_INLINE_DATA(p, nvk_compute_local_size(cmd));
   P_INLINE_DATA(p, dispatch_addr >> 32);
   P_INLINE_DATA(p, dispatch_addr);
   P_INLINE_DATA(p, root_desc_addr >> 32);
   P_INLINE_DATA(p, root_desc_addr);
   P_INLINE_DATA(p, qmd_addr >> 32);
   P_INLINE_DATA(p, qmd_addr);

   P_MTHD(p, NVA0C0, SEND_PCAS_A);
   P_NVA0C0_SEND_PCAS_A(p, qmd_addr >> 8);
   if (nvk_cmd_buffer_compute_cls(cmd) <= TURING_COMPUTE_A) {
      P_IMMD(p, NVA0C0, SEND_SIGNALING_PCAS_B, {
            .invalidate = INVALIDATE_TRUE,
            .schedule = SCHEDULE_TRUE
      });
   } else {
      P_IMMD(p, NVC6C0, SEND_SIGNALING_PCAS2_B,
             PCAS_ACTION_INVALIDATE_COPY_SCHEDULE);
   }
}
