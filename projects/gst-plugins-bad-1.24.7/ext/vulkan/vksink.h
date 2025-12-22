/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#ifndef _VK_SINK_H_
#define _VK_SINK_H_

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>
#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_SINK            (gst_vulkan_sink_get_type())
#define GST_VULKAN_SINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_SINK,GstVulkanSink))
#define GST_VULKAN_SINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VULKAN_SINK,GstVulkanSinkClass))
#define GST_IS_VULKAN_SINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_SINK))
#define GST_IS_VULKAN_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VULKAN_SINK))

typedef struct _GstVulkanSink GstVulkanSink;
typedef struct _GstVulkanSinkClass GstVulkanSinkClass;

struct _GstVulkanSink
{
  GstVideoSink video_sink;

  GstVulkanInstance *instance;
  GstVulkanDevice *device;

  GstVulkanDisplay *display;
  GstVulkanWindow *window;

  GstVulkanSwapper *swapper;

  /* properties */
  gboolean force_aspect_ratio;
  gint par_n;
  gint par_d;

  /* stream configuration */
  GstVideoInfo v_info;

  /* the currently set window handle */
  guintptr set_window_handle;

  gulong key_sig_id;
  gulong mouse_sig_id;
};

struct _GstVulkanSinkClass
{
    GstVideoSinkClass video_sink_class;
};

GType gst_vulkan_sink_get_type(void);

GST_ELEMENT_REGISTER_DECLARE (vulkansink);

G_END_DECLS

#endif
