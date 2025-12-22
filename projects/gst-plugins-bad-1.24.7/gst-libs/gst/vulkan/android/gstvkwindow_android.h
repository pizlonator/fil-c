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

#ifndef __GST_VULKAN_WINDOW_ANDROID_H__
#define __GST_VULKAN_WINDOW_ANDROID_H__

#include <gst/vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_WINDOW_ANDROID         (gst_vulkan_window_android_get_type())
#define GST_VULKAN_WINDOW_ANDROID(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VULKAN_WINDOW_ANDROID, GstVulkanWindowAndroid))
#define GST_VULKAN_WINDOW_ANDROID_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_VULKAN_WINDOW_ANDROID, GstVulkanWindowAndroidClass))
#define GST_IS_VULKAN_WINDOW_ANDROID(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VULKAN_WINDOW_ANDROID))
#define GST_IS_VULKAN_WINDOW_ANDROID_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VULKAN_WINDOW_ANDROID))
#define GST_VULKAN_WINDOW_ANDROID_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VULKAN_WINDOW_ANDROID, GstVulkanWindowAndroidClass))

typedef struct _GstVulkanWindowAndroid        GstVulkanWindowAndroid;
typedef struct _GstVulkanWindowAndroidPrivate GstVulkanWindowAndroidPrivate;
typedef struct _GstVulkanWindowAndroidClass   GstVulkanWindowAndroidClass;

/**
 * GstVulkanWindowAndroid:
 *
 * Opaque #GstVulkanWindowAndroid object
 */
struct _GstVulkanWindowAndroid
{
  /*< private >*/
  GstVulkanWindow parent;

  gint          visible :1;

  PFN_vkCreateAndroidSurfaceKHR CreateAndroidSurface;

  /*< private >*/
  gpointer _reserved[GST_PADDING];
};

/**
 * GstVulkanWindowAndroidClass:
 *
 * Opaque #GstVulkanWindowAndroidClass object
 */
struct _GstVulkanWindowAndroidClass {
  /*< private >*/
  GstVulkanWindowClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING_LARGE];
};

GType gst_vulkan_window_android_get_type     (void);

GstVulkanWindowAndroid * gst_vulkan_window_android_new (GstVulkanDisplay * display);

gboolean gst_vulkan_window_android_create_window (GstVulkanWindowAndroid * window_android);

G_END_DECLS

#endif /* __GST_VULKAN_WINDOW_ANDROID_H__ */
