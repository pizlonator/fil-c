 /*
  * Copyright © 2013 Intel Corporation
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
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  * IN THE SOFTWARE.
  *
  */

#ifndef INTEL_DEVICE_INFO_H
#define INTEL_DEVICE_INFO_H

#include <stdbool.h>
#include <stdint.h>

#include "util/bitset.h"
#include "util/macros.h"
#include "compiler/shader_enums.h"
#include "intel_kmd.h"

#include "intel/dev/intel_wa.h"

#include "intel/dev/intel_device_info_gen.h"

#ifdef __cplusplus
extern "C" {
#endif

#define intel_platform_in_range(platform, platform_range) \
   (((platform) >= INTEL_PLATFORM_ ## platform_range ## _START) && \
    ((platform) <= INTEL_PLATFORM_ ## platform_range ## _END))

#define intel_device_info_is_atsm(devinfo) \
   intel_platform_in_range((devinfo)->platform, ATSM)

#define intel_device_info_is_dg2(devinfo) \
   (intel_platform_in_range((devinfo)->platform, DG2) || \
    intel_platform_in_range((devinfo)->platform, ATSM))

#define intel_device_info_is_mtl(devinfo) \
   intel_platform_in_range((devinfo)->platform, MTL)

#define intel_device_info_is_adln(devinfo) \
   (devinfo->is_adl_n == true)

#define intel_device_info_is_arl(devinfo) \
   intel_platform_in_range((devinfo)->platform, ARL)

#define intel_device_info_is_mtl_or_arl(devinfo) \
   (intel_device_info_is_mtl(devinfo) || intel_device_info_is_arl(devinfo))

#define PAT_ENTRY(index_, mmap_, coh_)                      \
{                                                           \
   .index = index_,                                         \
   .mmap = INTEL_DEVICE_INFO_MMAP_MODE_##mmap_,             \
   .coherency = INTEL_DEVICE_INFO_COHERENCY_MODE_##coh_     \
}

#ifdef GFX_VER

#define intel_device_info_is_9lp(devinfo) \
   (GFX_VER == 9 && ((devinfo)->platform == INTEL_PLATFORM_BXT || \
                     (devinfo)->platform == INTEL_PLATFORM_GLK))

#else

#define intel_device_info_is_9lp(devinfo) \
   ((devinfo)->platform == INTEL_PLATFORM_BXT || \
    (devinfo)->platform == INTEL_PLATFORM_GLK)

#endif

#define GFX_IP_VER(major, minor) ((major << 16) | minor)

static inline bool
intel_device_info_slice_available(const struct intel_device_info *devinfo,
                                  int slice)
{
   assert(slice < INTEL_DEVICE_MAX_SLICES);
   return (devinfo->slice_masks & (1U << slice)) != 0;
}

static inline bool
intel_device_info_subslice_available(const struct intel_device_info *devinfo,
                                     int slice, int subslice)
{
   return (devinfo->subslice_masks[slice * devinfo->subslice_slice_stride +
                                   subslice / 8] & (1U << (subslice % 8))) != 0;
}

static inline bool
intel_device_info_eu_available(const struct intel_device_info *devinfo,
                               int slice, int subslice, int eu)
{
   unsigned subslice_offset = slice * devinfo->eu_slice_stride +
      subslice * devinfo->eu_subslice_stride;

   return (devinfo->eu_masks[subslice_offset + eu / 8] & (1U << eu % 8)) != 0;
}

static inline uint32_t
intel_device_info_subslice_total(const struct intel_device_info *devinfo)
{
   uint32_t total = 0;

   for (size_t i = 0; i < ARRAY_SIZE(devinfo->subslice_masks); i++) {
      total += __builtin_popcount(devinfo->subslice_masks[i]);
   }

   return total;
}

static inline uint32_t
intel_device_info_eu_total(const struct intel_device_info *devinfo)
{
   uint32_t total = 0;

   for (size_t i = 0; i < ARRAY_SIZE(devinfo->eu_masks); i++)
      total += __builtin_popcount(devinfo->eu_masks[i]);

   return total;
}

/**
 * Computes the bound of dualsubslice ID that can be used on this device.
 *
 * You should use this number if you're going to make calculation based on the
 * slice/dualsubslice ID provided by the SR0.0 EU register. The maximum
 * dualsubslice ID can be superior to the total number of dualsubslices on the
 * device, depending on fusing.
 *
 * On a 16 dualsubslice GPU, the maximum dualsubslice ID is 15. This function
 * would return the exclusive bound : 16.
 */
static inline unsigned
intel_device_info_dual_subslice_id_bound(const struct intel_device_info *devinfo)
{
   /* Start from the last slice/subslice so we find the answer faster. */
   for (int s = devinfo->max_slices - 1; s >= 0; s--) {
      for (int ss = devinfo->max_subslices_per_slice - 1; ss >= 0; ss--) {
         if (intel_device_info_subslice_available(devinfo, s, ss))
            return s * devinfo->max_subslices_per_slice + ss + 1;
      }
   }
   unreachable("Invalid topology");
   return 0;
}

int intel_device_name_to_pci_device_id(const char *name);

static inline uint64_t
intel_device_info_timebase_scale(const struct intel_device_info *devinfo,
                                 uint64_t gpu_timestamp)
{
   /* Try to avoid going over the 64bits when doing the scaling */
   uint64_t upper_ts = gpu_timestamp >> 32;
   uint64_t lower_ts = gpu_timestamp & 0xffffffff;
   uint64_t upper_scaled_ts = upper_ts * 1000000000ull / devinfo->timestamp_frequency;
   uint64_t lower_scaled_ts = lower_ts * 1000000000ull / devinfo->timestamp_frequency;
   return (upper_scaled_ts << 32) + lower_scaled_ts;
}

static inline bool
intel_vram_all_mappable(const struct intel_device_info *devinfo)
{
   return devinfo->mem.vram.unmappable.size == 0;
}

bool intel_get_device_info_from_fd(int fh, struct intel_device_info *devinfo, int min_ver, int max_ver);
bool intel_get_device_info_from_pci_id(int pci_id,
                                       struct intel_device_info *devinfo);

/* Only updates intel_device_info::regions::...::free fields. The
 * class/instance/size should remain the same over time.
 */
bool intel_device_info_update_memory_info(struct intel_device_info *devinfo,
                                          int fd);

void intel_device_info_topology_reset_masks(struct intel_device_info *devinfo);
void intel_device_info_topology_update_counts(struct intel_device_info *devinfo);
void intel_device_info_update_pixel_pipes(struct intel_device_info *devinfo, uint8_t *subslice_masks);
void intel_device_info_update_l3_banks(struct intel_device_info *devinfo);
void intel_device_info_update_cs_workgroup_threads(struct intel_device_info *devinfo);
bool intel_device_info_compute_system_memory(struct intel_device_info *devinfo, bool update);
void intel_device_info_update_after_hwconfig(struct intel_device_info *devinfo);

#ifdef GFX_VERx10
#define intel_needs_workaround(devinfo, id)         \
   (INTEL_WA_ ## id ## _GFX_VER &&                              \
    BITSET_TEST(devinfo->workarounds, INTEL_WA_##id))
#else
#define intel_needs_workaround(devinfo, id) \
   BITSET_TEST(devinfo->workarounds, INTEL_WA_##id)
#endif

enum intel_wa_steppings intel_device_info_wa_stepping(struct intel_device_info *devinfo);

#ifdef __cplusplus
}
#endif

#endif /* INTEL_DEVICE_INFO_H */
