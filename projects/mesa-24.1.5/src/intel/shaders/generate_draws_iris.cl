/* Copyright © 2023 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "libintel_shaders.h"

/* As defined in src/gallium/include/pipe/p_state.h : */
struct indirect_indexed_draw {
   uint32_t count;
   uint32_t instance_count;
   uint32_t start;
   int32_t index_bias;
   uint32_t start_instance;
};

struct indirect_draw {
   uint32_t count;
   uint32_t instance_count;
   uint32_t start;
   uint32_t start_instance;
};

static global void *write_vertex_inputs_iris(global uint32_t *dst_ptr,
                                             global void *indirect_ptr,
                                             global uint32_t *draw_id_ptr,
                                             uint32_t draw_id,
                                             bool is_indexed,
                                             bool uses_base,
                                             bool uses_drawid,
                                             uint32_t sgvs_id,
                                             uint32_t mocs)
{
   if (!uses_base && !uses_drawid)
      return dst_ptr;

   uint32_t vertex_buffer_count =
      (uses_base ? 1 : 0) + (uses_drawid ? 1 : 0);
   genX(write_3DSTATE_VERTEX_BUFFERS)(dst_ptr, vertex_buffer_count);
   dst_ptr += 1; /* GENX(3DSTATE_VERTEX_BUFFERS_length); */
   if (uses_base) {
      genX(write_VERTEX_BUFFER_STATE)(dst_ptr, mocs, sgvs_id++,
                                      (uint64_t)indirect_ptr, 8, 0);
      dst_ptr += GENX(VERTEX_BUFFER_STATE_length);
   }
   if (uses_drawid) {
      draw_id_ptr[0] = draw_id;
      draw_id_ptr[1] = is_indexed ? -1 : 0;
      genX(write_VERTEX_BUFFER_STATE)(dst_ptr, mocs, sgvs_id++,
                                      (uint64_t)draw_id_ptr, 8, 0);
      dst_ptr += GENX(VERTEX_BUFFER_STATE_length);
   }
   return dst_ptr;
}

static void write_draw_iris(global uint32_t *dst_ptr,
                            global void *indirect_ptr,
                            global uint32_t *draw_id_ptr,
                            uint32_t draw_id,
                            bool is_indexed,
                            bool is_predicated,
                            bool uses_tbimr,
                            bool uses_base,
                            bool uses_drawid,
                            uint32_t sgvs_id,
                            uint32_t mocs)
{
   if (is_indexed) {
      dst_ptr = write_vertex_inputs_iris(dst_ptr, indirect_ptr + 12,
                                         draw_id_ptr,
                                         draw_id, is_indexed,
                                         uses_base, uses_drawid,
                                         sgvs_id, mocs);


      struct indirect_indexed_draw data =
         *((global struct indirect_indexed_draw *)indirect_ptr);

      genX(write_3DPRIMITIVE)(dst_ptr,
                              is_predicated,
                              is_indexed,
                              uses_tbimr,
                              data.count,
                              data.start,
                              data.instance_count,
                              data.start_instance,
                              data.index_bias);
   } else {
      dst_ptr = write_vertex_inputs_iris(dst_ptr, indirect_ptr + 8,
                                         draw_id_ptr,
                                         draw_id, is_indexed,
                                         uses_base, uses_drawid,
                                         sgvs_id, mocs);

      struct indirect_draw data =
         *((global struct indirect_draw *)indirect_ptr);

      genX(write_3DPRIMITIVE)(dst_ptr,
                              is_predicated,
                              is_indexed,
                              uses_tbimr,
                              data.count,
                              data.start,
                              data.instance_count,
                              data.start_instance,
                              0 /* base_vertex_location */);
   }
}

static void end_generated_draws_iris(global void *dst_ptr,
                                     uint32_t item_idx,
                                     uint32_t draw_id, uint32_t draw_count,
                                     uint32_t ring_count,
                                     uint32_t flags,
                                     uint64_t gen_addr, uint64_t end_addr)
{
   uint32_t _3dprim_size = ((flags >> 16) & 0xff) * 4;
   /* We can have an indirect draw count = 0. */
   uint32_t last_draw_id = draw_count == 0 ? 0 : (draw_count - 1);
   global void *jump_dst = draw_count == 0 ? dst_ptr : (dst_ptr + _3dprim_size);

   if (draw_id == last_draw_id) {
      /* Exit the ring buffer to the next user commands */
      genX(write_MI_BATCH_BUFFER_START)(jump_dst, end_addr);
   } else if (item_idx == (ring_count - 1)) {
      /* Jump back to the generation shader to generate mode draws */
      genX(write_MI_BATCH_BUFFER_START)(jump_dst, gen_addr);
   }
}

void
genX(libiris_write_draw)(global void *dst_base,
                         global void *indirect_base,
                         global void *draw_id_base,
                         uint32_t indirect_stride,
                         global uint32_t *indirect_draw_count,
                         uint32_t draw_base,
                         uint32_t max_draw_count,
                         uint32_t flags,
                         uint32_t ring_count,
                         uint64_t gen_addr,
                         uint64_t end_addr,
                         uint32_t item_idx)
{
   uint32_t _3dprim_size = ((flags >> 16) & 0xff) * 4;
   uint32_t sgvs_id = flags >> 24;
   uint32_t draw_id = draw_base + item_idx;
   uint32_t draw_count = max_draw_count;
   global void *dst_ptr = dst_base +
#if GFX_VER >= 12
                          GENX(MI_ARB_CHECK_length) * 4 +
#endif
                          item_idx * _3dprim_size;
   global void *indirect_ptr = indirect_base + draw_id * indirect_stride;
   global void *draw_id_ptr = draw_id_base + item_idx * 8;

   /* Only read the indirect count if we have a valid pointer */
   if (indirect_draw_count != 0)
      draw_count = min(draw_count, *indirect_draw_count);

#if GFX_VER >= 12
   /* Reenable CS prefetching */
   if (item_idx == 0) {
      struct GENX(MI_ARB_CHECK) v = {
         GENX(MI_ARB_CHECK_header),
         /* This is a trick to get the CLC->SPIRV not to use a constant
          * variable for this. Otherwise we run into issues trying to store
          * that variable in constant memory which is inefficient for a single
          * dword and also not handled in our backend.
          */
         .PreParserDisableMask = item_idx == 0,
         .PreParserDisable = false,
      };
      GENX(MI_ARB_CHECK_pack)(dst_base, &v);
   }
#endif

   if (draw_id < draw_count) {
      bool is_indexed = (flags & ANV_GENERATED_FLAG_INDEXED) != 0;
      bool is_predicated = (flags & ANV_GENERATED_FLAG_PREDICATED) != 0;
      bool uses_tbimr = (flags & ANV_GENERATED_FLAG_TBIMR) != 0;
      bool uses_base = (flags & ANV_GENERATED_FLAG_BASE) != 0;
      bool uses_drawid = (flags & ANV_GENERATED_FLAG_DRAWID) != 0;
      uint32_t mocs = (flags >> 8) & 0xff;

      write_draw_iris(dst_ptr, indirect_ptr, draw_id_ptr,
                      draw_id, is_indexed, is_predicated,
                      uses_tbimr, uses_base, uses_drawid,
                      sgvs_id, mocs);
   }

   end_generated_draws_iris(dst_ptr, item_idx, draw_id, draw_count,
                            ring_count, flags, gen_addr, end_addr);
}
