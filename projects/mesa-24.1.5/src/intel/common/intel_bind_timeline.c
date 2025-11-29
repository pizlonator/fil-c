/*
 * Copyright 2023 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "intel_bind_timeline.h"

#include "drm-uapi/drm.h"
#include "intel_gem.h"

bool intel_bind_timeline_init(struct intel_bind_timeline *bind_timeline, int fd)
{
   struct drm_syncobj_create syncobj_create = { .flags = DRM_SYNCOBJ_CREATE_SIGNALED };

   if (intel_ioctl(fd, DRM_IOCTL_SYNCOBJ_CREATE, &syncobj_create))
      return false;

   simple_mtx_init(&bind_timeline->mutex, mtx_plain);
   bind_timeline->syncobj = syncobj_create.handle;
   bind_timeline->point = 0;

   return true;
}

void intel_bind_timeline_finish(struct intel_bind_timeline *bind_timeline, int fd)
{
   if (bind_timeline->syncobj == 0)
      return;

   uint64_t point = intel_bind_timeline_get_last_point(bind_timeline);
   struct drm_syncobj_timeline_wait syncobj_wait = {
      .timeout_nsec = INT64_MAX,
      .handles = (uintptr_t)&bind_timeline->syncobj,
      .count_handles = 1,
      .points = (uintptr_t)&point,
   };
   struct drm_syncobj_destroy syncobj_destroy = {
      .handle = bind_timeline->syncobj,
   };

   /* Makes sure last unbind was signaled otherwise it can trigger job
    * timeouts in KMD
    */
   intel_ioctl(fd, DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT, &syncobj_wait);
   intel_ioctl(fd, DRM_IOCTL_SYNCOBJ_DESTROY, &syncobj_destroy);

   simple_mtx_destroy(&bind_timeline->mutex);
}

uint32_t intel_bind_timeline_get_syncobj(struct intel_bind_timeline *bind_timeline)
{
   return bind_timeline->syncobj;
}

uint64_t intel_bind_timeline_bind_begin(struct intel_bind_timeline *bind_timeline)
{
   simple_mtx_lock(&bind_timeline->mutex);
   return ++bind_timeline->point;
}

void intel_bind_timeline_bind_end(struct intel_bind_timeline *bind_timeline)
{
   simple_mtx_unlock(&bind_timeline->mutex);
}

/*
 * Returns the timeline point that should be waited on before execute any
 * batch buffers.
 */
uint64_t intel_bind_timeline_get_last_point(struct intel_bind_timeline *bind_timeline)
{
   uint64_t ret;

   simple_mtx_lock(&bind_timeline->mutex);
   ret = bind_timeline->point;
   simple_mtx_unlock(&bind_timeline->mutex);

   return ret;
}
