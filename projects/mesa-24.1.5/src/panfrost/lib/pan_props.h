/*
 * Copyright (C) 2019 Collabora, Ltd.
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
 * Authors:
 *   Alyssa Rosenzweig <alyssa.rosenzweig@collabora.com>
 */

#ifndef PAN_PROPS_H
#define PAN_PROPS_H

#include <stdbool.h>
#include <stdint.h>

struct pan_kmod_dev;
struct pan_kmod_dev_props;

/** Implementation-defined tiler features */
struct panfrost_tiler_features {
   /** Number of bytes per tiler bin */
   unsigned bin_size;

   /** Maximum number of levels that may be simultaneously enabled.
    * Invariant: bitcount(hierarchy_mask) <= max_levels */
   unsigned max_levels;
};

struct panfrost_model {
   /* GPU ID */
   uint32_t gpu_id;

   /* GPU variant. */
   uint32_t gpu_variant;

   /* Marketing name for the GPU, used as the GL_RENDERER */
   const char *name;

   /* Set of associated performance counters */
   const char *performance_counters;

   /* Minimum GPU revision required for anisotropic filtering. ~0 and 0
    * means "no revisions support anisotropy" and "all revisions support
    * anistropy" respectively -- so checking for anisotropy is simply
    * comparing the reivsion.
    */
   uint32_t min_rev_anisotropic;

   /* Default tilebuffer size in bytes for the model. */
   unsigned tilebuffer_size;

   struct {
      /* The GPU lacks the capability for hierarchical tiling, without
       * an "Advanced Tiling Unit", instead requiring a single bin
       * size for the entire framebuffer be selected by the driver
       */
      bool no_hierarchical_tiling;
   } quirks;
};

const struct panfrost_model *panfrost_get_model(uint32_t gpu_id,
                                                uint32_t gpu_variant);

unsigned panfrost_query_l2_slices(const struct pan_kmod_dev_props *props);

struct panfrost_tiler_features
panfrost_query_tiler_features(const struct pan_kmod_dev_props *props);

unsigned
panfrost_query_thread_tls_alloc(const struct pan_kmod_dev_props *props);

uint32_t
panfrost_query_compressed_formats(const struct pan_kmod_dev_props *props);

unsigned panfrost_query_core_count(const struct pan_kmod_dev_props *props,
                                   unsigned *core_id_range);

bool panfrost_query_afbc(const struct pan_kmod_dev_props *props);

unsigned panfrost_query_optimal_tib_size(const struct panfrost_model *model);

uint64_t panfrost_clamp_to_usable_va_range(const struct pan_kmod_dev *dev,
                                           uint64_t va);

unsigned
panfrost_compute_max_thread_count(const struct pan_kmod_dev_props *props,
                                  unsigned work_reg_count);

#endif
