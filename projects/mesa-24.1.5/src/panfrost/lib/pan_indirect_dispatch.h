/*
 * Copyright (C) 2021 Collabora, Ltd.
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
 */

#ifndef __PAN_INDIRECT_DISPATCH_SHADERS_H__
#define __PAN_INDIRECT_DISPATCH_SHADERS_H__

#include "genxml/gen_macros.h"
#include "pan_jc.h"

#include "panfrost/util/pan_ir.h"

struct pan_jc;
struct pan_pool;

struct pan_indirect_dispatch_meta {
   struct panfrost_ubo_push push;

   unsigned gpu_id;

   /* Renderer state descriptor. */
   mali_ptr rsd;

   /* Thread storage descriptor. */
   mali_ptr tsd;

   /* Shader binary pool. */
   struct pan_pool *bin_pool;

   /* Shader desc pool for any descriptor that can be re-used across
    * indirect dispatch calls. Job descriptors are allocated from the pool
    * passed to pan_indirect_dispatch_emit().
    */
   struct pan_pool *desc_pool;
};

struct pan_indirect_dispatch_info {
   mali_ptr job;
   mali_ptr indirect_dim;
   mali_ptr num_wg_sysval[3];
} PACKED;

static inline void
pan_indirect_dispatch_meta_init(struct pan_indirect_dispatch_meta *meta,
                                unsigned gpu_id, struct pan_pool *bin_pool,
                                struct pan_pool *desc_pool)
{
   memset(meta, 0, sizeof(*meta));
   meta->gpu_id = gpu_id;
   meta->bin_pool = bin_pool;
   meta->desc_pool = desc_pool;
}

#ifdef PAN_ARCH
unsigned GENX(pan_indirect_dispatch_emit)(
   struct pan_indirect_dispatch_meta *meta,
   struct pan_pool *pool, struct pan_jc *jc,
   const struct pan_indirect_dispatch_info *dispatch_info);
#endif

#endif
