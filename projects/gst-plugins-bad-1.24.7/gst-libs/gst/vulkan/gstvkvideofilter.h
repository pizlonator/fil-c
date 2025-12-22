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

#ifndef _VK_FULL_SCREEN_RENDER_H_
#define _VK_FULL_SCREEN_RENDER_H_

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/vulkan/gstvkqueue.h>

G_BEGIN_DECLS

GST_VULKAN_API
GType gst_vulkan_video_filter_get_type(void);
#define GST_TYPE_VULKAN_VIDEO_FILTER              (gst_vulkan_video_filter_get_type())
#define GST_VULKAN_VIDEO_FILTER(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_VIDEO_FILTER,GstVulkanVideoFilter))
#define GST_VULKAN_VIDEO_FILTER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VULKAN_VIDEO_FILTER,GstVulkanVideoFilterClass))
#define GST_VULKAN_VIDEO_FILTER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_VULKAN_VIDEO_FILTER,GstVulkanVideoFilterClass))
#define GST_IS_VULKAN_VIDEO_FILTER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_VIDEO_FILTER))
#define GST_IS_VULKAN_VIDEO_FILTER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VULKAN_VIDEO_FILTER))

typedef struct _GstVulkanVideoFilter GstVulkanVideoFilter;
typedef struct _GstVulkanVideoFilterClass GstVulkanVideoFilterClass;

/**
 * GstVulkanVideoFilter:
 * @parent: the parent #GstBaseTransform
 * @instance: the configured #GstVulkanInstance
 * @device: the configured #GstVulkanDevice
 * @queue: the configured #GstVulkanQueue
 * @in_caps: the configured input #GstCaps
 * @in_info: the configured input #GstVideoInfo
 * @out_caps: the configured output #GstCaps
 * @out_info: the configured output #GstVideoInfo
 *
 * Since: 1.18
 */
struct _GstVulkanVideoFilter
{
  GstBaseTransform      parent;

  GstVulkanInstance    *instance;
  GstVulkanDevice      *device;
  GstVulkanQueue       *queue;

  GstCaps              *in_caps;
  GstVideoInfo          in_info;
  GstCaps              *out_caps;
  GstVideoInfo          out_info;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

/**
 * GstVulkanVideoFilterClass:
 * @parent_class: the parent #GstBaseTransformClass
 *
 * Since: 1.18
 */
struct _GstVulkanVideoFilterClass
{
  GstBaseTransformClass parent_class;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVulkanVideoFilter, gst_object_unref);

G_END_DECLS

#endif
