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

#include <xf86drm.h>

#include "drm-uapi/panfrost_drm.h"
#include "util/hash_table.h"
#include "util/macros.h"
#include "util/u_math.h"
#include "util/u_thread.h"
#include "pan_bo.h"
#include "pan_device.h"
#include "pan_encoder.h"
#include "pan_samples.h"
#include "pan_texture.h"
#include "pan_util.h"
#include "wrap.h"

/* DRM_PANFROST_PARAM_TEXTURE_FEATURES0 will return a bitmask of supported
 * compressed formats, so we offer a helper to test if a format is supported */

bool
panfrost_supports_compressed_format(struct panfrost_device *dev, unsigned fmt)
{
   if (MALI_EXTRACT_TYPE(fmt) != MALI_FORMAT_COMPRESSED)
      return true;

   unsigned idx = fmt & ~MALI_FORMAT_COMPRESSED;
   assert(idx < 32);

   return panfrost_query_compressed_formats(&dev->kmod.props) & (1 << idx);
}

/* Always reserve the lower 32MB. */
#define PANFROST_VA_RESERVE_BOTTOM 0x2000000ull

void
panfrost_open_device(void *memctx, int fd, struct panfrost_device *dev)
{
   dev->memctx = memctx;

   dev->kmod.dev = pan_kmod_dev_create(fd, PAN_KMOD_DEV_FLAG_OWNS_FD, NULL);
   if (!dev->kmod.dev) {
      close(fd);
      return;
   }

   pan_kmod_dev_query_props(dev->kmod.dev, &dev->kmod.props);

   dev->arch = pan_arch(dev->kmod.props.gpu_prod_id);
   dev->model = panfrost_get_model(dev->kmod.props.gpu_prod_id,
                                   dev->kmod.props.gpu_variant);

   /* If we don't recognize the model, bail early */
   if (!dev->model)
      goto err_free_kmod_dev;

   /* 32bit address space, with the lower 32MB reserved. We clamp
    * things so it matches kmod VA range limitations.
    */
   uint64_t user_va_start = panfrost_clamp_to_usable_va_range(
      dev->kmod.dev, PANFROST_VA_RESERVE_BOTTOM);
   uint64_t user_va_end =
      panfrost_clamp_to_usable_va_range(dev->kmod.dev, 1ull << 32);

   dev->kmod.vm = pan_kmod_vm_create(
      dev->kmod.dev, PAN_KMOD_VM_FLAG_AUTO_VA | PAN_KMOD_VM_FLAG_TRACK_ACTIVITY,
      user_va_start, user_va_end - user_va_start);
   if (!dev->kmod.vm)
      goto err_free_kmod_dev;

   dev->core_count =
      panfrost_query_core_count(&dev->kmod.props, &dev->core_id_range);
   dev->thread_tls_alloc = panfrost_query_thread_tls_alloc(&dev->kmod.props);
   dev->optimal_tib_size = panfrost_query_optimal_tib_size(dev->model);
   dev->compressed_formats =
      panfrost_query_compressed_formats(&dev->kmod.props);
   dev->tiler_features = panfrost_query_tiler_features(&dev->kmod.props);
   dev->has_afbc = panfrost_query_afbc(&dev->kmod.props);
   dev->formats = panfrost_format_table(dev->arch);
   dev->blendable_formats = panfrost_blendable_format_table(dev->arch);

   util_sparse_array_init(&dev->bo_map, sizeof(struct panfrost_bo), 512);

   pthread_mutex_init(&dev->bo_cache.lock, NULL);
   list_inithead(&dev->bo_cache.lru);

   for (unsigned i = 0; i < ARRAY_SIZE(dev->bo_cache.buckets); ++i)
      list_inithead(&dev->bo_cache.buckets[i]);

   /* Initialize pandecode before we start allocating */
   if (dev->debug & (PAN_DBG_TRACE | PAN_DBG_SYNC))
      dev->decode_ctx = pandecode_create_context(!(dev->debug & PAN_DBG_TRACE));

   /* Tiler heap is internally required by the tiler, which can only be
    * active for a single job chain at once, so a single heap can be
    * shared across batches/contextes.
    *
    * Heap management is completely different on CSF HW, don't allocate the
    * heap BO in that case.
    */

   if (dev->arch < 10) {
      dev->tiler_heap = panfrost_bo_create(
         dev, 128 * 1024 * 1024, PAN_BO_INVISIBLE | PAN_BO_GROWABLE, "Tiler heap");
   }

   pthread_mutex_init(&dev->submit_lock, NULL);

   /* Done once on init */
   dev->sample_positions = panfrost_bo_create(
      dev, panfrost_sample_positions_buffer_size(), 0, "Sample positions");
   panfrost_upload_sample_positions(dev->sample_positions->ptr.cpu);
   return;

err_free_kmod_dev:
   pan_kmod_dev_destroy(dev->kmod.dev);
   dev->kmod.dev = NULL;
}

void
panfrost_close_device(struct panfrost_device *dev)
{
   /* If we don't recognize the model, the rest of the device won't exist,
    * we will have early-exited the device open.
    */
   if (dev->model) {
      pthread_mutex_destroy(&dev->submit_lock);
      panfrost_bo_unreference(dev->tiler_heap);
      panfrost_bo_unreference(dev->sample_positions);
      panfrost_bo_cache_evict_all(dev);
      pthread_mutex_destroy(&dev->bo_cache.lock);
      util_sparse_array_finish(&dev->bo_map);
   }

   if (dev->kmod.vm)
      pan_kmod_vm_destroy(dev->kmod.vm);

   if (dev->kmod.dev)
      pan_kmod_dev_destroy(dev->kmod.dev);
}
