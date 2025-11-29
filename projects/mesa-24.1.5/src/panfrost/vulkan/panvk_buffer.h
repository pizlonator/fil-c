/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_BUFFER_H
#define PANVK_BUFFER_H

#include <stdint.h>

#include "vk_buffer.h"

struct panvk_priv_bo;

struct panvk_buffer {
   struct vk_buffer vk;

   uint64_t dev_addr;

   /* TODO: See if we can rework the synchronization logic so we don't need to
    * pass BOs around.
    */
   struct pan_kmod_bo *bo;

   /* FIXME: Only used for index buffers to do the min/max index retrieval on
    * the CPU. This is all broken anyway and the min/max search should be done
    * with a compute shader that also patches the job descriptor accordingly
    * (basically an indirect draw).
    *
    * Make sure this field goes away as soon as we fixed indirect draws.
    */
   void *host_ptr;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(panvk_buffer, vk.base, VkBuffer,
                               VK_OBJECT_TYPE_BUFFER)

static inline uint64_t
panvk_buffer_gpu_ptr(const struct panvk_buffer *buffer, uint64_t offset)
{
   if (buffer->bo == NULL)
      return 0;

   return buffer->dev_addr + offset;
}

static inline uint64_t
panvk_buffer_range(const struct panvk_buffer *buffer, uint64_t offset,
                   uint64_t range)
{
   if (buffer->bo == NULL)
      return 0;

   return vk_buffer_range(&buffer->vk, offset, range);
}

#endif
