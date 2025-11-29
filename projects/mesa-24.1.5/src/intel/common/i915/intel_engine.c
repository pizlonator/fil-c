/*
 * Copyright © 2022 Intel Corporation
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

#include "i915/intel_engine.h"

#include <stdlib.h>

#include "i915/intel_gem.h"

static enum intel_engine_class
i915_engine_class_to_intel(enum drm_i915_gem_engine_class i915)
{
   switch (i915) {
   case I915_ENGINE_CLASS_RENDER:
      return INTEL_ENGINE_CLASS_RENDER;
   case I915_ENGINE_CLASS_COPY:
      return INTEL_ENGINE_CLASS_COPY;
   case I915_ENGINE_CLASS_VIDEO:
      return INTEL_ENGINE_CLASS_VIDEO;
   case I915_ENGINE_CLASS_VIDEO_ENHANCE:
      return INTEL_ENGINE_CLASS_VIDEO_ENHANCE;
   case I915_ENGINE_CLASS_COMPUTE:
      return INTEL_ENGINE_CLASS_COMPUTE;
   default:
      return INTEL_ENGINE_CLASS_INVALID;
   }
}

enum drm_i915_gem_engine_class
intel_engine_class_to_i915(enum intel_engine_class intel)
{
   switch (intel) {
   case INTEL_ENGINE_CLASS_RENDER:
      return I915_ENGINE_CLASS_RENDER;
   case INTEL_ENGINE_CLASS_COPY:
      return I915_ENGINE_CLASS_COPY;
   case INTEL_ENGINE_CLASS_VIDEO:
      return I915_ENGINE_CLASS_VIDEO;
   case INTEL_ENGINE_CLASS_VIDEO_ENHANCE:
      return I915_ENGINE_CLASS_VIDEO_ENHANCE;
   case INTEL_ENGINE_CLASS_COMPUTE:
      return I915_ENGINE_CLASS_COMPUTE;
   default:
      return I915_ENGINE_CLASS_INVALID;
   }
}

struct intel_query_engine_info *
i915_engine_get_info(int fd)
{
   struct drm_i915_query_engine_info *i915_engines_info;
   i915_engines_info = intel_i915_query_alloc(fd, DRM_I915_QUERY_ENGINE_INFO, NULL);
   if (!i915_engines_info)
      return NULL;

   struct intel_query_engine_info *intel_engines_info;
   intel_engines_info = calloc(1, sizeof(*intel_engines_info) +
                               sizeof(*intel_engines_info->engines) *
                               i915_engines_info->num_engines);
   if (!intel_engines_info) {
      free(i915_engines_info);
      return NULL;
   }

   for (int i = 0; i < i915_engines_info->num_engines; i++) {
      struct drm_i915_engine_info *i915_engine = &i915_engines_info->engines[i];
      struct intel_engine_class_instance *intel_engine = &intel_engines_info->engines[i];

      intel_engine->engine_class = i915_engine_class_to_intel(i915_engine->engine.engine_class);
      intel_engine->engine_instance = i915_engine->engine.engine_instance;
      intel_engine->gt_id = 0;
   }

   intel_engines_info->num_engines = i915_engines_info->num_engines;

   free(i915_engines_info);
   return intel_engines_info;
}

bool
i915_engines_is_guc_semaphore_functional(int fd, const struct intel_device_info *info)
{
   struct drm_i915_query_guc_submission_version *guc_submission_ver =
      intel_i915_query_alloc(fd, DRM_I915_QUERY_GUC_SUBMISSION_VERSION, NULL);
   uint32_t read_ver, min_ver;

   if (guc_submission_ver == NULL)
      return false;

   /* branch == 0 is mainline branch, any other branch value indicates that
    * other version numbers cannot be used to infer whether features or fixes
    * are present in the release.
    *
    * major, minor and patch are u8 for GuC, uAPI have it as u32 because of HuC.
    */
   if (guc_submission_ver->branch == 0) {
      read_ver = guc_submission_ver->major << 16;
      read_ver |= guc_submission_ver->minor << 8;
      read_ver |= guc_submission_ver->patch;
   } else {
      read_ver = 0;
   }

   free(guc_submission_ver);

   /* Requires at least GuC submission version 1.1.3 */
   min_ver = 1ULL << 16 | 1ULL << 8 | 3;

   return read_ver >= min_ver;
}
