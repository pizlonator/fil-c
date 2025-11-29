/*
 * Copyright © 2015 Intel Corporation
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

#include <stdlib.h>

#include "util/macros.h"

#include "intel_engine.h"
#include "i915/intel_engine.h"
#include "xe/intel_engine.h"

struct intel_query_engine_info *
intel_engine_get_info(int fd, enum intel_kmd_type type)
{
   switch (type) {
   case INTEL_KMD_TYPE_I915:
      return i915_engine_get_info(fd);
   case INTEL_KMD_TYPE_XE:
      return xe_engine_get_info(fd);
   default:
      unreachable("Missing");
      return NULL;
   }
}

int
intel_engines_count(const struct intel_query_engine_info *info,
                    enum intel_engine_class engine_class)
{
   int count = 0;

   for (int i = 0; i < info->num_engines; i++) {
      if (info->engines[i].engine_class == engine_class)
         count++;
   }

   return count;
}

const char *
intel_engines_class_to_string(enum intel_engine_class engine_class)
{
   switch (engine_class) {
   case INTEL_ENGINE_CLASS_RENDER:
      return "render";
   case INTEL_ENGINE_CLASS_COPY:
      return "copy";
   case INTEL_ENGINE_CLASS_VIDEO:
      return "video";
   case INTEL_ENGINE_CLASS_VIDEO_ENHANCE:
      return "video-enh";
   case INTEL_ENGINE_CLASS_COMPUTE:
      return "compute";
   default:
      return "unknown";
   }
}

static bool
is_guc_semaphore_functional(int fd, const struct intel_device_info *info)
{
   switch (info->kmd_type) {
   case INTEL_KMD_TYPE_I915:
      return i915_engines_is_guc_semaphore_functional(fd, info);
   case INTEL_KMD_TYPE_XE:
      return xe_engines_is_guc_semaphore_functional(fd, info);
   default:
      unreachable("Missing");
      return false;
   }
}

int
intel_engines_supported_count(int fd, const struct intel_device_info *info,
                              const struct intel_query_engine_info *engine_info,
                              enum intel_engine_class engine_class)
{
   bool supported;

   /* check if user set the force enabled engine with run-time parameter */
   switch (engine_class) {
   case INTEL_ENGINE_CLASS_COMPUTE:
      supported = debug_get_bool_option("INTEL_ENGINE_CLASS_COMPUTE", false);
      supported |= is_guc_semaphore_functional(fd, info);
      break;
   case INTEL_ENGINE_CLASS_COPY:
      supported = debug_get_bool_option("INTEL_ENGINE_CLASS_COPY", true);
      break;
   default:
      /* There is no restrictions or parameters for other engines */
      supported = true;
   }

   return supported ? intel_engines_count(engine_info, engine_class) : 0;
}
