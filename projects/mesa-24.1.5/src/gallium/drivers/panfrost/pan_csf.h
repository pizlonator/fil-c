/*
 * Copyright (C) 2023 Collabora Ltd.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef __PAN_CSF_H__
#define __PAN_CSF_H__

#include "compiler/shader_enums.h"

#include "pan_bo.h"
#include "pan_mempool.h"

struct cs_builder;

struct panfrost_csf_batch {
   /* CS related fields. */
   struct {
      /* CS builder. */
      struct cs_builder *builder;

      /* CS state, written through the CS, and checked when PAN_MESA_DEBUG=sync.
       */
      struct panfrost_ptr state;
   } cs;

   /* Pool used to allocate CS chunks. */
   struct panfrost_pool cs_chunk_pool;
};

struct panfrost_csf_context {
   uint32_t group_handle;

   struct {
      uint32_t handle;
      struct panfrost_bo *desc_bo;
   } heap;

   /* Temporary geometry buffer. Used as a FIFO by the tiler. */
   struct panfrost_bo *tmp_geom_bo;
};

#if defined(PAN_ARCH) && PAN_ARCH >= 10

#include "genxml/gen_macros.h"

struct panfrost_batch;
struct panfrost_context;
struct pan_fb_info;
struct pipe_draw_info;
struct pipe_grid_info;
struct pipe_draw_start_count_bias;

void GENX(csf_init_context)(struct panfrost_context *ctx);
void GENX(csf_cleanup_context)(struct panfrost_context *ctx);

void GENX(csf_init_batch)(struct panfrost_batch *batch);
void GENX(csf_cleanup_batch)(struct panfrost_batch *batch);
int GENX(csf_submit_batch)(struct panfrost_batch *batch);

void GENX(csf_preload_fb)(struct panfrost_batch *batch, struct pan_fb_info *fb);
void GENX(csf_emit_fragment_job)(struct panfrost_batch *batch,
                                 const struct pan_fb_info *pfb);
void GENX(csf_emit_batch_end)(struct panfrost_batch *batch);
void GENX(csf_launch_xfb)(struct panfrost_batch *batch,
                          const struct pipe_draw_info *info, unsigned count);
void GENX(csf_launch_grid)(struct panfrost_batch *batch,
                           const struct pipe_grid_info *info);
void GENX(csf_launch_draw)(struct panfrost_batch *batch,
                           const struct pipe_draw_info *info,
                           unsigned drawid_offset,
                           const struct pipe_draw_start_count_bias *draw,
                           unsigned vertex_count);

#endif /* PAN_ARCH >= 10 */

#endif /* __PAN_CSF_H__ */
