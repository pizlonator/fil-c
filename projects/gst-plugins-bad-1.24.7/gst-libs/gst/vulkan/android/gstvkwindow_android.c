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

#include <gst/gst.h>

#include <gst/vulkan/vulkan.h>

#include "gstvkwindow_android.h"
#include "gstvkdisplay_android.h"

#define GET_PRIV(o) gst_vulkan_window_android_get_instance_private (o)

#define GST_CAT_DEFAULT gst_vulkan_window_android_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static void
_init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkanwindowandroid", 0,
        "Vulkan Android Window");
    g_once_init_leave (&_init, 1);
  }
}

enum
{
  PROP_0,
};

struct _GstVulkanWindowAndroidPrivate
{
  struct ANativeWindow *internal_window;
  gint window_width, window_height;

  gint preferred_width;
  gint preferred_height;
};

#define gst_vulkan_window_android_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanWindowAndroid, gst_vulkan_window_android,
    GST_TYPE_VULKAN_WINDOW,
    G_ADD_PRIVATE (GstVulkanWindowAndroid) _init_debug ());

static VkSurfaceKHR gst_vulkan_window_android_get_surface (GstVulkanWindow *
    window, GError ** error);
static gboolean
gst_vulkan_window_android_get_presentation_support (GstVulkanWindow * window,
    GstVulkanDevice * device, guint32 queue_family_idx);
static gboolean gst_vulkan_window_android_open (GstVulkanWindow * window,
    GError ** error);
static void gst_vulkan_window_android_close (GstVulkanWindow * window);
static void gst_vulkan_window_android_set_window_handle (GstVulkanWindow *
    window, guintptr window_handle);

static void
gst_vulkan_window_android_class_init (GstVulkanWindowAndroidClass * klass)
{
  GstVulkanWindowClass *window_class = (GstVulkanWindowClass *) klass;

  window_class->open = GST_DEBUG_FUNCPTR (gst_vulkan_window_android_open);
  window_class->close = GST_DEBUG_FUNCPTR (gst_vulkan_window_android_close);
  window_class->get_surface = gst_vulkan_window_android_get_surface;
  window_class->get_presentation_support =
      gst_vulkan_window_android_get_presentation_support;
  window_class->set_window_handle = gst_vulkan_window_android_set_window_handle;
}

static void
gst_vulkan_window_android_init (GstVulkanWindowAndroid * window)
{
  GstVulkanWindowAndroidPrivate *priv = GET_PRIV (window);

  priv->preferred_width = 320;
  priv->preferred_height = 240;
}

/* Must be called in the gl thread */
GstVulkanWindowAndroid *
gst_vulkan_window_android_new (GstVulkanDisplay * display)
{
  GstVulkanWindowAndroid *window;

  _init_debug ();

  if ((gst_vulkan_display_get_handle_type (display) &
          GST_VULKAN_DISPLAY_TYPE_ANDROID)
      == GST_VULKAN_DISPLAY_TYPE_NONE) {
    GST_INFO ("Wrong display type %u for this window type %u", display->type,
        GST_VULKAN_DISPLAY_TYPE_ANDROID);
    return NULL;
  }

  window = g_object_new (GST_TYPE_VULKAN_WINDOW_ANDROID, NULL);
  gst_object_ref_sink (window);

  return window;
}

gboolean
gst_vulkan_window_android_create_window (GstVulkanWindowAndroid *
    window_android)
{
  GstVulkanWindowAndroidPrivate *priv = GET_PRIV (window_android);

  if (!priv->internal_window) {
    GST_WARNING_OBJECT (window_android, "No ANativeWindow provided");
    return FALSE;
  }

  return TRUE;
}

static VkSurfaceKHR
gst_vulkan_window_android_get_surface (GstVulkanWindow * window,
    GError ** error)
{
  GstVulkanWindowAndroid *window_android = GST_VULKAN_WINDOW_ANDROID (window);
  GstVulkanWindowAndroidPrivate *priv = GET_PRIV (window_android);
  VkAndroidSurfaceCreateInfoKHR info = { 0, };
  VkSurfaceKHR ret;
  VkResult err;

  if (!priv->internal_window) {
    g_set_error_literal (error, GST_VULKAN_ERROR,
        VK_ERROR_INITIALIZATION_FAILED,
        "No layer to retrieve surface for. Has create_window() been called?");
    return 0;
  }

  info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  info.pNext = NULL;
  info.flags = 0;
  info.window = priv->internal_window;

  if (!window_android->CreateAndroidSurface)
    window_android->CreateAndroidSurface =
        gst_vulkan_instance_get_proc_address (window->display->instance,
        "vkCreateAndroidSurfaceKHR");
  if (!window_android->CreateAndroidSurface) {
    g_set_error_literal (error, GST_VULKAN_ERROR, VK_ERROR_FEATURE_NOT_PRESENT,
        "Could not retrieve \"vkCreateAndroidSurfaceKHR\" function pointer");
    return VK_NULL_HANDLE;
  }

  err =
      window_android->CreateAndroidSurface (window->display->instance->instance,
      &info, NULL, &ret);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateAndroidSurfaceKHR") < 0)
    return VK_NULL_HANDLE;

  return ret;
}

static gboolean
gst_vulkan_window_android_get_presentation_support (GstVulkanWindow * window,
    GstVulkanDevice * device, guint32 queue_family_idx)
{
  return TRUE;
}

static gboolean
gst_vulkan_window_android_open (GstVulkanWindow * window, GError ** error)
{
  GstVulkanWindowAndroid *window_android = GST_VULKAN_WINDOW_ANDROID (window);

  if (!GST_VULKAN_WINDOW_CLASS (parent_class)->open (window, error))
    return FALSE;

  return gst_vulkan_window_android_create_window (window_android);
}

static void
gst_vulkan_window_android_close (GstVulkanWindow * window)
{
  GST_VULKAN_WINDOW_CLASS (parent_class)->close (window);
}

static void
gst_vulkan_window_android_set_window_handle (GstVulkanWindow * window,
    guintptr window_handle)
{
  GstVulkanWindowAndroid *window_android = GST_VULKAN_WINDOW_ANDROID (window);
  GstVulkanWindowAndroidPrivate *priv = GET_PRIV (window_android);
  struct ANativeWindow *native_window = (struct ANativeWindow *) window_handle;

  g_return_if_fail (native_window != NULL);

  if (priv->internal_window && priv->internal_window != native_window) {
    GST_FIXME_OBJECT (window_android, "View changes are not implemented");
    return;
  }

  priv->internal_window = native_window;
}
