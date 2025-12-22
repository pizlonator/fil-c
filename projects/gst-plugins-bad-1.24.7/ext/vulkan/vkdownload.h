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

#ifndef _VK_DOWNLOAD_H_
#define _VK_DOWNLOAD_H_

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_DOWNLOAD            (gst_vulkan_download_get_type())
#define GST_VULKAN_DOWNLOAD(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_DOWNLOAD,GstVulkanDownload))
#define GST_VULKAN_DOWNLOAD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VULKAN_DOWNLOAD,GstVulkanDownloadClass))
#define GST_IS_VULKAN_DOWNLOAD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_DOWNLOAD))
#define GST_IS_VULKAN_DOWNLOAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VULKAN_DOWNLOAD))

typedef struct _GstVulkanDownload GstVulkanDownload;
typedef struct _GstVulkanDownloadClass GstVulkanDownloadClass;

struct DownloadMethod
{
  const gchar       *name;

  GstStaticCaps     *in_template;
  GstStaticCaps     *out_template;

  gpointer          (*new_impl)                 (GstVulkanDownload * download);
  GstCaps *         (*transform_caps)           (gpointer impl,
                                                 GstPadDirection direction,
                                                 GstCaps * caps);
  gboolean          (*set_caps)                 (gpointer impl,
                                                 GstCaps * in_caps,
                                                 GstCaps * out_caps);
  void              (*propose_allocation)       (gpointer impl,
                                                 GstQuery * decide_query,
                                                 GstQuery * query);
  GstFlowReturn     (*perform)                  (gpointer impl,
                                                 GstBuffer * buffer,
                                                 GstBuffer ** outbuf);
  void              (*free)                     (gpointer impl);
};

struct _GstVulkanDownload
{
  GstBaseTransform      parent;

  GstVulkanInstance     *instance;
  GstVulkanDevice       *device;
  GstVulkanQueue        *queue;

  GstCaps               *in_caps;
  GstCaps               *out_caps;

  /* all impl pointers */
  gpointer              *download_impls;
  guint                 current_impl;
};

struct _GstVulkanDownloadClass
{
  GstBaseTransformClass video_sink_class;
};

GType gst_vulkan_download_get_type(void);

GST_ELEMENT_REGISTER_DECLARE (vulkandownload);

G_END_DECLS

#endif
