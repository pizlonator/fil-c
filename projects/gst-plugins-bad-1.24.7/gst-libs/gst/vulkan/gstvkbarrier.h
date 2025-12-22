/*
 * GStreamer
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_VULKAN_BARRIER_H__
#define __GST_VULKAN_BARRIER_H__

#include <gst/vulkan/vulkan_fwd.h>
#include <gst/vulkan/gstvkapi.h>

G_BEGIN_DECLS

/**
 * GstVulkanBarrierType:
 * @GST_VULKAN_BARRIER_NONE: no barrier type
 * @GST_VULKAN_BARRIER_MEMORY: memory barrier
 * @GST_VULKAN_BARRIER_BUFFER: buffer barrier
 * @GST_VULKAN_BARRIER_IMAGE: image barrier
 *
 * Since: 1.18
 */
typedef enum
{
  GST_VULKAN_BARRIER_NONE = 0,
  GST_VULKAN_BARRIER_TYPE_MEMORY = 1,
  GST_VULKAN_BARRIER_TYPE_BUFFER = 2,
  GST_VULKAN_BARRIER_TYPE_IMAGE = 3,
} GstVulkanBarrierType;

/**
 * GstVulkanBarrierFlags:
 * @GST_VULKAN_BARRIER_FLAGS_NONE: no flags
 *
 * Since: 1.18
 */
typedef enum
{
  GST_VULKAN_BARRIER_FLAG_NONE = 0,
} GstVulkanBarrierFlags;

/**
 * GstVulkanBarrierMemoryInfo:
 * @type: the #GstVulkanBarrierType of the barrier
 * @flags the #GstVulkanBarrierFlags of the barrier
 * @queue: the #GstVulkanQueue this barrier is to execute with
 * @pipeline_stages: the stages in the graphics pipeline to execute the barrier
 * @access_flags: access flags
 * @semaphore: timeline semaphore
 * @semaphore_value: current value of the timeline semaphore
 *
 * Since: 1.18
 */
struct _GstVulkanBarrierMemoryInfo
{
  GstVulkanBarrierType type;
  GstVulkanBarrierFlags flags;
  GstVulkanQueue * queue;
  guint64 pipeline_stages;
  guint64 access_flags;

  /**
   * GstVulkanBarrierMemoryInfo.semaphore:
   *
   * Timeline semaphore
   *
   * Since: 1.24
   */
  VkSemaphore semaphore;
  /**
   * GstVulkanBarrierMemoryInfo.semaphore_value:
   *
   * Current value of the timeline semaphore
   *
   * Since: 1.24
   */
  guint64 semaphore_value;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

G_END_DECLS

#endif /* __GST_VULKAN_BARRIER_H__ */
