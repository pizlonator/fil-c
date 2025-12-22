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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/vulkan/vulkan.h>

#include "gstvkdisplay_android.h"

#define GST_CAT_DEFAULT gst_vulkan_display_debug
GST_DEBUG_CATEGORY_STATIC (gst_vulkan_display_debug);

G_DEFINE_TYPE (GstVulkanDisplayAndroid, gst_vulkan_display_android,
    GST_TYPE_VULKAN_DISPLAY);

static void gst_vulkan_display_android_finalize (GObject * object);
static gpointer gst_vulkan_display_android_get_handle (GstVulkanDisplay *
    display);

static void
gst_vulkan_display_android_class_init (GstVulkanDisplayAndroidClass * klass)
{
  GST_VULKAN_DISPLAY_CLASS (klass)->get_handle =
      GST_DEBUG_FUNCPTR (gst_vulkan_display_android_get_handle);

  G_OBJECT_CLASS (klass)->finalize = gst_vulkan_display_android_finalize;
}

static void
gst_vulkan_display_android_init (GstVulkanDisplayAndroid * display_android)
{
  GstVulkanDisplay *display = (GstVulkanDisplay *) display_android;

  display->type = GST_VULKAN_DISPLAY_TYPE_ANDROID;
}

static void
gst_vulkan_display_android_finalize (GObject * object)
{
  G_OBJECT_CLASS (gst_vulkan_display_android_parent_class)->finalize (object);
}

/**
 * gst_vulkan_display_android_new:
 *
 * Create a new #GstVulkanDisplayAndroid.
 *
 * Returns: (transfer full): a new #GstVulkanDisplayAndroid
 */
GstVulkanDisplayAndroid *
gst_vulkan_display_android_new (void)
{
  GstVulkanDisplayAndroid *ret;

  GST_DEBUG_CATEGORY_GET (gst_vulkan_display_debug, "vulkandisplay");

  ret = g_object_new (GST_TYPE_VULKAN_DISPLAY_ANDROID, NULL);
  gst_object_ref_sink (ret);

  return ret;
}

static gpointer
gst_vulkan_display_android_get_handle (GstVulkanDisplay * display)
{
  return NULL;
}
