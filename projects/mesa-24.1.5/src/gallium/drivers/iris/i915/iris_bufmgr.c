/*
 * Copyright © 2023 Intel Corporation
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
 */
#include "i915/iris_bufmgr.h"

#include "common/intel_gem.h"
#include "intel/dev/intel_debug.h"
#include "iris/iris_bufmgr.h"

#include "drm-uapi/i915_drm.h"

#define FILE_DEBUG_FLAG DEBUG_BUFMGR

bool iris_i915_bo_busy_gem(struct iris_bo *bo)
{
   assert(iris_bo_is_real(bo));

   struct iris_bufmgr *bufmgr = bo->bufmgr;
   struct drm_i915_gem_busy busy = { .handle = bo->gem_handle };

   if (intel_ioctl(iris_bufmgr_get_fd(bufmgr), DRM_IOCTL_I915_GEM_BUSY, &busy))
      return false;

   return busy.busy;
}

int iris_i915_bo_wait_gem(struct iris_bo *bo, int64_t timeout_ns)
{
   assert(iris_bo_is_real(bo));

   struct iris_bufmgr *bufmgr = bo->bufmgr;
   struct drm_i915_gem_wait wait = {
      .bo_handle = bo->gem_handle,
      .timeout_ns = timeout_ns,
   };

   if (intel_ioctl(iris_bufmgr_get_fd(bufmgr), DRM_IOCTL_I915_GEM_WAIT, &wait))
      return -errno;

   return 0;
}

bool iris_i915_init_global_vm(struct iris_bufmgr *bufmgr, uint32_t *vm_id)
{
   uint64_t value;
   bool ret = intel_gem_get_context_param(iris_bufmgr_get_fd(bufmgr), 0,
                                          I915_CONTEXT_PARAM_VM, &value);
   if (ret)
      *vm_id = value;
   return ret;
}

int iris_i915_bo_get_tiling(struct iris_bo *bo, uint32_t *tiling)
{
   struct iris_bufmgr *bufmgr = bo->bufmgr;
   struct drm_i915_gem_get_tiling ti = { .handle = bo->gem_handle };
   int ret = intel_ioctl(iris_bufmgr_get_fd(bufmgr), DRM_IOCTL_I915_GEM_GET_TILING, &ti);

   if (ret) {
      DBG("gem_get_tiling failed for BO %u: %s\n",
          bo->gem_handle, strerror(errno));
   }

   *tiling = ti.tiling_mode;

   return ret;
}

int iris_i915_bo_set_tiling(struct iris_bo *bo, const struct isl_surf *surf)
{
   struct iris_bufmgr *bufmgr = bo->bufmgr;
   uint32_t tiling_mode = isl_tiling_to_i915_tiling(surf->tiling);
   int ret;

   /* GEM_SET_TILING is slightly broken and overwrites the input on the
    * error path, so we have to open code intel_ioctl().
    */
   struct drm_i915_gem_set_tiling set_tiling = {
      .handle = bo->gem_handle,
      .tiling_mode = tiling_mode,
      .stride = surf->row_pitch_B,
   };

   ret = intel_ioctl(iris_bufmgr_get_fd(bufmgr), DRM_IOCTL_I915_GEM_SET_TILING, &set_tiling);
   if (ret) {
      DBG("gem_set_tiling failed for BO %u: %s\n",
          bo->gem_handle, strerror(errno));
   }

   return ret;
}

uint64_t
iris_i915_tiling_to_modifier(uint32_t tiling)
{
   static const uint64_t map[] = {
      [I915_TILING_NONE]   = DRM_FORMAT_MOD_LINEAR,
      [I915_TILING_X]      = I915_FORMAT_MOD_X_TILED,
      [I915_TILING_Y]      = I915_FORMAT_MOD_Y_TILED,
   };

   assert(tiling < ARRAY_SIZE(map));

   return map[tiling];
}
