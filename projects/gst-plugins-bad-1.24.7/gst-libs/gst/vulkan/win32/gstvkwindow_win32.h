/*
 * GStreamer
 * Copyright (C) 2012 Matthew Waters <ystreet00@gmail.com>
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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

#ifndef __GST_VULKAN_WINDOW_WIN32_H__
#define __GST_VULKAN_WINDOW_WIN32_H__

#undef UNICODE
#include <windows.h>
#define UNICODE

#include <gst/vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_WINDOW_WIN32         (gst_vulkan_window_win32_get_type())
#define GST_VULKAN_WINDOW_WIN32(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VULKAN_WINDOW_WIN32, GstVulkanWindowWin32))
#define GST_VULKAN_WINDOW_WIN32_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_VULKAN_WINDOW_WIN32, GstVulkanWindowWin32Class))
#define GST_IS_VULKAN_WINDOW_WIN32(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VULKAN_WINDOW_WIN32))
#define GST_IS_VULKAN_WINDOW_WIN32_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VULKAN_WINDOW_WIN32))
#define GST_VULKAN_WINDOW_WIN32_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VULKAN_WINDOW_WIN32, GstVulkanWindowWin32Class))

typedef struct _GstVulkanWindowWin32        GstVulkanWindowWin32;
typedef struct _GstVulkanWindowWin32Private GstVulkanWindowWin32Private;
typedef struct _GstVulkanWindowWin32Class   GstVulkanWindowWin32Class;

struct _GstVulkanWindowWin32 {
  /*< private >*/
  GstVulkanWindow parent;

  PFN_vkCreateWin32SurfaceKHR CreateWin32Surface;
  PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR GetPhysicalDeviceWin32PresentationSupport;

  HWND internal_win_id;
  HWND parent_win_id;
  HDC device;
  gboolean is_closed;
  gboolean visible;

  GSource *msg_source;

  /*< private >*/
  gpointer _reserved[GST_PADDING];
};

struct _GstVulkanWindowWin32Class {
  /*< private >*/
  GstVulkanWindowClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING];
};

GType gst_vulkan_window_win32_get_type     (void);

GstVulkanWindowWin32 * gst_vulkan_window_win32_new  (GstVulkanDisplay * display);

G_END_DECLS

#endif /* __GST_VULKAN_WINDOW_WIN32_H__ */
