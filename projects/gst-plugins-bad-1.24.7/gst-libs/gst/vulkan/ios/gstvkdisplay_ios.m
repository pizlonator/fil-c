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

#include <UIKit/UIKit.h>

#include <gst/vulkan/vulkan.h>

#include "gstvkdisplay_ios.h"

#define GST_CAT_DEFAULT gst_vulkan_display_debug
GST_DEBUG_CATEGORY_STATIC (gst_vulkan_display_debug);

G_DEFINE_TYPE (GstVulkanDisplayIos, gst_vulkan_display_ios,
    GST_TYPE_VULKAN_DISPLAY);

static void gst_vulkan_display_ios_finalize (GObject * object);
static gpointer gst_vulkan_display_ios_get_handle (GstVulkanDisplay * display);

static void
gst_vulkan_display_ios_class_init (GstVulkanDisplayIosClass * klass)
{
  GST_VULKAN_DISPLAY_CLASS (klass)->get_handle =
      GST_DEBUG_FUNCPTR (gst_vulkan_display_ios_get_handle);

  G_OBJECT_CLASS (klass)->finalize = gst_vulkan_display_ios_finalize;
}

static void
gst_vulkan_display_ios_init (GstVulkanDisplayIos * display_ios)
{
  GstVulkanDisplay *display = (GstVulkanDisplay *) display_ios;

  display->type = GST_VULKAN_DISPLAY_TYPE_IOS;
}

static void
gst_vulkan_display_ios_finalize (GObject * object)
{
  G_OBJECT_CLASS (gst_vulkan_display_ios_parent_class)->finalize (object);
}

/**
 * gst_vulkan_display_ios_new:
 *
 * Create a new #GstVulkanDisplayIos.
 *
 * Returns: (transfer full): a new #GstVulkanDisplayIos
 */
GstVulkanDisplayIos *
gst_vulkan_display_ios_new (void)
{
  GstVulkanDisplayIos *ret;

  GST_DEBUG_CATEGORY_GET (gst_vulkan_display_debug, "vulkandisplay");

  ret = g_object_new (GST_TYPE_VULKAN_DISPLAY_IOS, NULL);
  gst_object_ref_sink (ret);

  return ret;
}

static gpointer
gst_vulkan_display_ios_get_handle (GstVulkanDisplay * display)
{
  return (gpointer) (__bridge gpointer) [UIApplication sharedApplication];
}
