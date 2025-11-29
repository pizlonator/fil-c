/*
 * Copyright (c) 2012-2015 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Wladimir J. van der Laan <laanwj@gmail.com>
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#ifndef H_ETNAVIV_SCREEN
#define H_ETNAVIV_SCREEN

#include "etna_core_info.h"
#include "etnaviv_internal.h"
#include "etnaviv_perfmon.h"

#include "util/u_thread.h"
#include "pipe/p_screen.h"
#include "renderonly/renderonly.h"
#include "util/set.h"
#include "util/slab.h"
#include "util/u_dynarray.h"
#include "util/u_helpers.h"
#include "util/u_queue.h"
#include "compiler/nir/nir.h"

struct etna_bo;

struct etna_screen {
   struct pipe_screen base;

   struct etna_device *dev;
   struct etna_gpu *gpu;
   struct etna_pipe *pipe;
   struct etna_perfmon *perfmon;
   struct renderonly *ro;

   struct util_dynarray supported_pm_queries;
   struct slab_parent_pool transfer_pool;

   struct etna_core_info *info;

   struct etna_specs specs;

   uint32_t drm_version;

   struct etna_compiler *compiler;
   struct util_queue shader_compiler_queue;

   /* dummy render target for GPUs that can't fully disable the color pipe */
   struct etna_reloc dummy_rt_reloc;

   /* dummy texture descriptor */
   struct etna_reloc dummy_desc_reloc;
};

static inline bool
VIV_FEATURE(const struct etna_screen *screen, enum etna_feature feature)
{
   return etna_core_has_feature(screen->info, feature);
}

static inline struct etna_screen *
etna_screen(struct pipe_screen *pscreen)
{
   return (struct etna_screen *)pscreen;
}

struct etna_bo *
etna_screen_bo_from_handle(struct pipe_screen *pscreen,
                           struct winsys_handle *whandle);

struct pipe_screen *
etna_screen_create(struct etna_device *dev, struct etna_gpu *gpu,
                   struct renderonly *ro);

static inline size_t
etna_screen_get_tile_size(struct etna_screen *screen, uint8_t ts_mode,
                          bool is_msaa)
{
   if (!VIV_FEATURE(screen, ETNA_FEATURE_CACHE128B256BPERLINE)) {
      if (VIV_FEATURE(screen, ETNA_FEATURE_SMALL_MSAA) && is_msaa)
         return 256;
      return 64;
   }

   if (ts_mode == TS_MODE_256B)
      return 256;
   else
      return 128;
}

#endif
