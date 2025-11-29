/* Copyright © 2023 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "libintel_shaders.h"

static void end_generated_draws(global void *dst_ptr,
                                uint32_t item_idx,
                                uint32_t draw_id, uint32_t draw_count,
                                uint32_t ring_count, uint32_t max_draw_count,
                                uint32_t flags,
                                uint64_t gen_addr, uint64_t end_addr)
{
   uint32_t _3dprim_size_B = ((flags >> 16) & 0xff) * 4;
   bool indirect_count = (flags & ANV_GENERATED_FLAG_COUNT) != 0;
   bool ring_mode = (flags & ANV_GENERATED_FLAG_RING_MODE) != 0;
   /* We can have an indirect draw count = 0. */
   uint32_t last_draw_id = draw_count == 0 ? 0 : (min(draw_count, max_draw_count) - 1);
   global void *jump_dst = draw_count == 0 ? dst_ptr : (dst_ptr + _3dprim_size_B);

   if (ring_mode) {
      if (draw_id == last_draw_id) {
         /* Exit the ring buffer to the next user commands */
         genX(write_MI_BATCH_BUFFER_START)(jump_dst, end_addr);
      } else if (item_idx == (ring_count - 1)) {
         /* Jump back to the generation shader to generate mode draws */
         genX(write_MI_BATCH_BUFFER_START)(jump_dst, gen_addr);
      }
   } else {
      if (draw_id == last_draw_id && draw_count < max_draw_count) {
         /* Skip forward to the end of the generated draws */
         genX(write_MI_BATCH_BUFFER_START)(jump_dst, end_addr);
      }
   }
}

void
genX(libanv_write_draw)(global void *dst_base,
                        global void *indirect_base,
                        global void *draw_id_base,
                        uint32_t indirect_stride,
                        global uint32_t *_draw_count,
                        uint32_t draw_base,
                        uint32_t instance_multiplier,
                        uint32_t max_draw_count,
                        uint32_t flags,
                        uint32_t ring_count,
                        uint64_t gen_addr,
                        uint64_t end_addr,
                        uint32_t item_idx)
{
   uint32_t _3dprim_size_B = ((flags >> 16) & 0xff) * 4;
   uint32_t draw_id = draw_base + item_idx;
   uint32_t draw_count = *_draw_count;
   global void *dst_ptr = dst_base + item_idx * _3dprim_size_B;
   global void *indirect_ptr = indirect_base + draw_id * indirect_stride;
   global void *draw_id_ptr = draw_id_base + item_idx * 4;

   if (draw_id < min(draw_count, max_draw_count)) {
      bool is_indexed = (flags & ANV_GENERATED_FLAG_INDEXED) != 0;
      bool is_predicated = (flags & ANV_GENERATED_FLAG_PREDICATED) != 0;
      bool uses_tbimr = (flags & ANV_GENERATED_FLAG_TBIMR) != 0;
      bool uses_base = (flags & ANV_GENERATED_FLAG_BASE) != 0;
      bool uses_drawid = (flags & ANV_GENERATED_FLAG_DRAWID) != 0;
      uint32_t mocs = (flags >> 8) & 0xff;

      genX(write_draw)(dst_ptr, indirect_ptr, draw_id_ptr,
                       draw_id, instance_multiplier,
                       is_indexed, is_predicated,
                       uses_tbimr, uses_base, uses_drawid,
                       mocs);
   }

   end_generated_draws(dst_ptr, item_idx, draw_id, draw_count,
                       ring_count, max_draw_count, flags,
                       gen_addr, end_addr);
}
