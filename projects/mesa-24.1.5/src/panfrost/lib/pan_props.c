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

#include "util/macros.h"

#include "kmod/pan_kmod.h"
#include "panfrost/util/pan_ir.h"
#include "pan_props.h"

#include <genxml/gen_macros.h>

/* Fixed "minimum revisions" */
#define NO_ANISO  (~0)
#define HAS_ANISO (0)

#define MODEL(gpu_id_, gpu_variant_, shortname, counters_,                     \
              min_rev_anisotropic_, tib_size_, quirks_)                        \
   {                                                                           \
      .gpu_id = gpu_id_, .gpu_variant = gpu_variant_,                          \
      .name = "Mali-" shortname " (Panfrost)",                                 \
      .performance_counters = counters_,                                       \
      .min_rev_anisotropic = min_rev_anisotropic_,                             \
      .tilebuffer_size = tib_size_, .quirks = quirks_,                         \
   }

/* Table of supported Mali GPUs */
/* clang-format off */
const struct panfrost_model panfrost_model_list[] = {
        MODEL(0x600, 0, "T600",    "T60x", NO_ANISO,          8192, {}),
        MODEL(0x620, 0, "T620",    "T62x", NO_ANISO,          8192, {}),
        MODEL(0x720, 0, "T720",    "T72x", NO_ANISO,          8192, { .no_hierarchical_tiling = true }),
        MODEL(0x750, 0, "T760",    "T76x", NO_ANISO,          8192, {}),
        MODEL(0x820, 0, "T820",    "T82x", NO_ANISO,          8192, { .no_hierarchical_tiling = true }),
        MODEL(0x830, 0, "T830",    "T83x", NO_ANISO,          8192, { .no_hierarchical_tiling = true }),
        MODEL(0x860, 0, "T860",    "T86x", NO_ANISO,          8192, {}),
        MODEL(0x880, 0, "T880",    "T88x", NO_ANISO,          8192, {}),

        MODEL(0x6000, 0, "G71",    "TMIx", NO_ANISO,          8192, {}),
        MODEL(0x6221, 0, "G72",    "THEx", 0x0030 /* r0p3 */, 16384, {}),
        MODEL(0x7090, 0, "G51",    "TSIx", 0x1010 /* r1p1 */, 16384, {}),
        MODEL(0x7093, 0, "G31",    "TDVx", HAS_ANISO,         16384, {}),
        MODEL(0x7211, 0, "G76",    "TNOx", HAS_ANISO,         16384, {}),
        MODEL(0x7212, 0, "G52",    "TGOx", HAS_ANISO,         16384, {}),
        MODEL(0x7402, 0, "G52 r1", "TGOx", HAS_ANISO,         16384, {}),
        MODEL(0x9091, 0, "G57",    "TNAx", HAS_ANISO,         16384, {}),
        MODEL(0x9093, 0, "G57",    "TNAx", HAS_ANISO,         16384, {}),

        MODEL(0xa867, 0, "G610",   "TVIx", HAS_ANISO,         32768, {}),
        MODEL(0xac74, 0, "G310",   "TVAx", HAS_ANISO,         16384, {}),
        MODEL(0xac74, 1, "G310",   "TVAx", HAS_ANISO,         16384, {}),
        MODEL(0xac74, 2, "G310",   "TVAx", HAS_ANISO,         16384, {}),
        MODEL(0xac74, 3, "G310",   "TVAx", HAS_ANISO,         32768, {}),
        MODEL(0xac74, 4, "G310",   "TVAx", HAS_ANISO,         32768, {}),
};
/* clang-format on */

#undef NO_ANISO
#undef HAS_ANISO
#undef MODEL

/*
 * Look up a supported model by its GPU ID, or return NULL if the model is not
 * supported at this time.
 */
const struct panfrost_model *
panfrost_get_model(uint32_t gpu_id, uint32_t gpu_variant)
{
   for (unsigned i = 0; i < ARRAY_SIZE(panfrost_model_list); ++i) {
      if (panfrost_model_list[i].gpu_id == gpu_id &&
          panfrost_model_list[i].gpu_variant == gpu_variant)
         return &panfrost_model_list[i];
   }

   return NULL;
}

unsigned
panfrost_query_l2_slices(const struct pan_kmod_dev_props *props)
{
   /* L2_SLICES is MEM_FEATURES[11:8] minus(1) */
   return ((props->mem_features >> 8) & 0xF) + 1;
}

struct panfrost_tiler_features
panfrost_query_tiler_features(const struct pan_kmod_dev_props *props)
{
   /* Default value (2^9 bytes and 8 levels) to match old behaviour */
   uint32_t raw = props->tiler_features;

   /* Bin size is log2 in the first byte, max levels in the second byte */
   return (struct panfrost_tiler_features){
      .bin_size = (1 << (raw & BITFIELD_MASK(5))),
      .max_levels = (raw >> 8) & BITFIELD_MASK(4),
   };
}

unsigned
panfrost_query_core_count(const struct pan_kmod_dev_props *props,
                          unsigned *core_id_range)
{
   /* On older kernels, worst-case to 16 cores */

   unsigned mask = props->shader_present;

   /* Some cores might be absent. In some cases, we care
    * about the range of core IDs (that is, the greatest core ID + 1). If
    * the core mask is contiguous, this equals the core count.
    */
   *core_id_range = util_last_bit(mask);

   /* The actual core count skips overs the gaps */
   return util_bitcount(mask);
}

unsigned
panfrost_query_thread_tls_alloc(const struct pan_kmod_dev_props *props)
{
   return props->max_tls_instance_per_core ?: props->max_threads_per_core;
}

unsigned
panfrost_compute_max_thread_count(const struct pan_kmod_dev_props *props,
                                  unsigned work_reg_count)
{
   unsigned aligned_reg_count;

   /* 4, 8 or 16 registers per shader on Midgard
    * 32 or 64 registers per shader on Bifrost
    */
   if (pan_arch(props->gpu_prod_id) <= 5) {
      aligned_reg_count = util_next_power_of_two(MAX2(work_reg_count, 4));
      assert(aligned_reg_count <= 16);
   } else {
      aligned_reg_count = work_reg_count <= 32 ? 32 : 64;
   }

   return MIN3(props->max_threads_per_wg, props->max_threads_per_core,
               props->num_registers_per_core / aligned_reg_count);
}

uint32_t
panfrost_query_compressed_formats(const struct pan_kmod_dev_props *props)
{
   return props->texture_features[0];
}

/* Check for AFBC hardware support. AFBC is introduced in v5. Implementations
 * may omit it, signaled as a nonzero value in the AFBC_FEATURES property. */

bool
panfrost_query_afbc(const struct pan_kmod_dev_props *props)
{
   unsigned reg = props->afbc_features;

   return (pan_arch(props->gpu_prod_id) >= 5) && (reg == 0);
}

/*
 * To pipeline multiple tiles, a given tile may use at most half of the tile
 * buffer. This function returns the optimal size (assuming pipelining).
 *
 * For Mali-G510 and Mali-G310, we will need extra logic to query the tilebuffer
 * size for the particular variant. The CORE_FEATURES register might help.
 */
unsigned
panfrost_query_optimal_tib_size(const struct panfrost_model *model)
{
   /* Preconditions ensure the returned value is a multiple of 1 KiB, the
    * granularity of the colour buffer allocation field.
    */
   assert(model->tilebuffer_size >= 2048);
   assert(util_is_power_of_two_nonzero(model->tilebuffer_size));

   return model->tilebuffer_size / 2;
}

uint64_t
panfrost_clamp_to_usable_va_range(const struct pan_kmod_dev *dev, uint64_t va)
{
   struct pan_kmod_va_range user_va_range =
      pan_kmod_dev_query_user_va_range(dev);

   if (va < user_va_range.start)
      return user_va_range.start;
   else if (va > user_va_range.start + user_va_range.size)
      return user_va_range.start + user_va_range.size;

   return va;
}
