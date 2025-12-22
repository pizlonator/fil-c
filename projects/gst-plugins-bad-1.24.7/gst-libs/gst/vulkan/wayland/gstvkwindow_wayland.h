/*
 * GStreamer
 * Copyright (C) 2016 Matthew Waters <matthew@centricular.com>
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

#ifndef __GST_VK_WINDOW_WAYLAND_H__
#define __GST_VK_WINDOW_WAYLAND_H__

#include <wayland-client.h>
#include <wayland-cursor.h>

#include <gst/vulkan/vulkan.h>

#include <vulkan/vulkan_wayland.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_WINDOW_WAYLAND         (gst_vulkan_window_wayland_get_type())
GType gst_vulkan_window_wayland_get_type     (void);

#define GST_VULKAN_WINDOW_WAYLAND(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VULKAN_WINDOW_WAYLAND, GstVulkanWindowWayland))
#define GST_VULKAN_WINDOW_WAYLAND_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_VULKAN_WINDOW_WAYLAND, GstVulkanWindowWaylandClass))
#define GST_IS_VULKAN_WINDOW_WAYLAND(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VULKAN_WINDOW_WAYLAND))
#define GST_IS_VULKAN_WINDOW_WAYLAND_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VULKAN_WINDOW_WAYLAND))
#define GST_VULKAN_WINDOW_WAYLAND_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VULKAN_WINDOW_WAYLAND, GstVulkanWindowWaylandClass))

typedef struct _GstVulkanWindowWayland        GstVulkanWindowWayland;
typedef struct _GstVulkanWindowWaylandClass   GstVulkanWindowWaylandClass;

struct _GstVulkanWindowWayland {
  /*< private >*/
  GstVulkanWindow parent;

  PFN_vkCreateWaylandSurfaceKHR CreateWaylandSurface;
  PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR GetPhysicalDeviceWaylandPresentationSupport;

  struct wl_event_queue     *queue;
  struct wl_surface         *surface;
  struct wl_shell_surface   *shell_surface;
  struct wl_callback        *callback;

  struct xdg_surface        *xdg_surface;
  struct xdg_toplevel       *xdg_toplevel;

  int window_width, window_height;

  GSource *wl_source;

  gpointer _reserved[GST_PADDING];
};

struct _GstVulkanWindowWaylandClass {
  /*< private >*/
  GstVulkanWindowClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING];
};

GstVulkanWindowWayland * gst_vulkan_window_wayland_new  (GstVulkanDisplay * display);

G_END_DECLS

#endif /* __GST_GL_WINDOW_X11_H__ */
