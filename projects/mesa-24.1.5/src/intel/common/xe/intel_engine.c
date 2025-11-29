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
#include "xe/intel_engine.h"

#include <stdlib.h>

#include "common/intel_gem.h"
#include "common/xe/intel_device_query.h"

#include "drm-uapi/xe_drm.h"

static enum intel_engine_class
xe_engine_class_to_intel(uint16_t xe)
{
   switch (xe) {
   case DRM_XE_ENGINE_CLASS_RENDER:
      return INTEL_ENGINE_CLASS_RENDER;
   case DRM_XE_ENGINE_CLASS_COPY:
      return INTEL_ENGINE_CLASS_COPY;
   case DRM_XE_ENGINE_CLASS_VIDEO_DECODE:
      return INTEL_ENGINE_CLASS_VIDEO;
   case DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE:
      return INTEL_ENGINE_CLASS_VIDEO_ENHANCE;
   case DRM_XE_ENGINE_CLASS_COMPUTE:
      return INTEL_ENGINE_CLASS_COMPUTE;
   default:
      return INTEL_ENGINE_CLASS_INVALID;
   }
}

uint16_t
intel_engine_class_to_xe(enum intel_engine_class intel)
{
   switch (intel) {
   case INTEL_ENGINE_CLASS_RENDER:
      return DRM_XE_ENGINE_CLASS_RENDER;
   case INTEL_ENGINE_CLASS_COPY:
      return DRM_XE_ENGINE_CLASS_COPY;
   case INTEL_ENGINE_CLASS_VIDEO:
      return DRM_XE_ENGINE_CLASS_VIDEO_DECODE;
   case INTEL_ENGINE_CLASS_VIDEO_ENHANCE:
      return DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE;
   case INTEL_ENGINE_CLASS_COMPUTE:
      return DRM_XE_ENGINE_CLASS_COMPUTE;
   default:
      return -1;
   }
}

struct intel_query_engine_info *
xe_engine_get_info(int fd)
{
   struct drm_xe_query_engines *xe_engines;

   xe_engines = xe_device_query_alloc_fetch(fd, DRM_XE_DEVICE_QUERY_ENGINES, NULL);
   if (!xe_engines)
      return NULL;

   struct intel_query_engine_info *intel_engines_info;
   intel_engines_info = calloc(1, sizeof(*intel_engines_info) +
                               sizeof(*intel_engines_info->engines) *
                               xe_engines->num_engines);
   if (!intel_engines_info) {
      goto error_free_xe_engines;
      return NULL;
   }

   for (uint32_t i = 0; i < xe_engines->num_engines; i++) {
      struct drm_xe_engine_class_instance *xe_engine = &xe_engines->engines[i].instance;
      struct intel_engine_class_instance *intel_engine = &intel_engines_info->engines[i];

      intel_engine->engine_class = xe_engine_class_to_intel(xe_engine->engine_class);
      intel_engine->engine_instance = xe_engine->engine_instance;
      intel_engine->gt_id = xe_engine->gt_id;
   }

   intel_engines_info->num_engines = xe_engines->num_engines;
   free(xe_engines);
   return intel_engines_info;

error_free_xe_engines:
   free(xe_engines);
   return NULL;
}

bool
xe_engines_is_guc_semaphore_functional(int fd, const struct intel_device_info *info)
{
   struct drm_xe_query_uc_fw_version uc_fw_version = {
      .uc_type = XE_QUERY_UC_TYPE_GUC_SUBMISSION,
   };
   struct drm_xe_device_query query = {
      .query = DRM_XE_DEVICE_QUERY_UC_FW_VERSION,
      .data = (uintptr_t)&uc_fw_version,
      .size = sizeof(uc_fw_version)
   };
   uint32_t read_ver, min_ver;

   if (intel_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query))
      return false;

   /* branch == 0 is mainline branch, any other branch value indicates that
    * other version numbers cannot be used to infer whether features or fixes
    * are present in the release.
    *
    * major, minor and patch are u8 for GuC, uAPI have it as u32 because of HuC.
    */
   if (uc_fw_version.branch_ver == 0) {
      read_ver = uc_fw_version.major_ver << 16;
      read_ver |= uc_fw_version.minor_ver << 8;
      read_ver |= uc_fw_version.patch_ver;
   } else {
      read_ver = 0;
   }

   /* Requires at least GuC submission version 1.1.3 */
   min_ver = 1ULL << 16 | 1ULL << 8 | 3;

   return read_ver >= min_ver;
}
