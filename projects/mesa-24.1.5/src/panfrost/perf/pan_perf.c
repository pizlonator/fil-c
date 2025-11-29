/*
 * Copyright © 2021 Collabora, Ltd.
 * Author: Antonio Caggiano <antonio.caggiano@collabora.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <assert.h>
#include <string.h>
#include <xf86drm.h>

#include "util/macros.h"
#include "util/ralloc.h"

#include "pan_perf.h"

#include <drm-uapi/panfrost_drm.h>
#include <lib/kmod/pan_kmod.h>
#include <lib/pan_props.h>
#include <pan_perf_metrics.h>

#define PAN_COUNTERS_PER_CATEGORY 64
#define PAN_SHADER_CORE_INDEX     3

uint32_t
panfrost_perf_counter_read(const struct panfrost_perf_counter *counter,
                           const struct panfrost_perf *perf)
{
   unsigned offset = perf->category_offset[counter->category_index];
   offset += counter->offset;
   assert(offset < perf->n_counter_values);

   uint32_t ret = perf->counter_values[offset];

   // If counter belongs to shader core, accumulate values for all other cores
   if (counter->category_index == PAN_SHADER_CORE_INDEX) {
      for (uint32_t core = 1; core < perf->core_id_range; ++core) {
         ret += perf->counter_values[offset + PAN_COUNTERS_PER_CATEGORY * core];
      }
   }

   return ret;
}

static const struct panfrost_perf_config *
panfrost_lookup_counters(const char *name)
{
   for (unsigned i = 0; i < ARRAY_SIZE(panfrost_perf_configs); ++i) {
      if (strcmp(panfrost_perf_configs[i]->name, name) == 0)
         return panfrost_perf_configs[i];
   }

   return NULL;
}

void
panfrost_perf_init(struct panfrost_perf *perf, int fd)
{
   ASSERTED drmVersionPtr version = drmGetVersion(fd);

   /* We only support panfrost at the moment. */
   assert(version && !strcmp(version->name, "panfrost"));

   drmFreeVersion(version);

   perf->dev = pan_kmod_dev_create(fd, 0, NULL);
   assert(perf->dev);

   struct pan_kmod_dev_props props = {};
   pan_kmod_dev_query_props(perf->dev, &props);

   const struct panfrost_model *model =
      panfrost_get_model(props.gpu_prod_id, props.gpu_variant);
   if (model == NULL)
      unreachable("Invalid GPU ID");

   perf->cfg = panfrost_lookup_counters(model->performance_counters);

   if (perf->cfg == NULL)
      unreachable("Performance counters missing!");

   // Generally counter blocks are laid out in the following order:
   // Job manager, tiler, one or more L2 caches, and one or more shader cores.
   unsigned l2_slices = panfrost_query_l2_slices(&props);
   panfrost_query_core_count(&props, &perf->core_id_range);

   uint32_t n_blocks = 2 + l2_slices + perf->core_id_range;
   perf->n_counter_values = PAN_COUNTERS_PER_CATEGORY * n_blocks;
   perf->counter_values = ralloc_array(perf, uint32_t, perf->n_counter_values);

   /* Setup the layout */
   perf->category_offset[0] = PAN_COUNTERS_PER_CATEGORY * 0;
   perf->category_offset[1] = PAN_COUNTERS_PER_CATEGORY * 1;
   perf->category_offset[2] = PAN_COUNTERS_PER_CATEGORY * 2;
   perf->category_offset[3] = PAN_COUNTERS_PER_CATEGORY * (2 + l2_slices);
}

static int
panfrost_perf_query(struct panfrost_perf *perf, uint32_t enable)
{
   struct drm_panfrost_perfcnt_enable perfcnt_enable = {enable, 0};
   return drmIoctl(perf->dev->fd, DRM_IOCTL_PANFROST_PERFCNT_ENABLE,
                   &perfcnt_enable);
}

int
panfrost_perf_enable(struct panfrost_perf *perf)
{
   return panfrost_perf_query(perf, 1 /* enable */);
}

int
panfrost_perf_disable(struct panfrost_perf *perf)
{
   return panfrost_perf_query(perf, 0 /* disable */);
}

int
panfrost_perf_dump(struct panfrost_perf *perf)
{
   // Dump performance counter values to the memory buffer pointed to by
   // counter_values
   struct drm_panfrost_perfcnt_dump perfcnt_dump = {
      (uint64_t)(uintptr_t)perf->counter_values};
   return drmIoctl(perf->dev->fd, DRM_IOCTL_PANFROST_PERFCNT_DUMP,
                   &perfcnt_dump);
}
