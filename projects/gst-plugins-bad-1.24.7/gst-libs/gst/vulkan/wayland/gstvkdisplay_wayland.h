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

#ifndef __GST_VK_DISPLAY_WAYLAND_H__
#define __GST_VK_DISPLAY_WAYLAND_H__

#include <gst/gst.h>

#include <wayland-client.h>

#include <gst/vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_DISPLAY_WAYLAND             (gst_vulkan_display_wayland_get_type())
GST_VULKAN_API
GType gst_vulkan_display_wayland_get_type (void);

#define GST_VULKAN_DISPLAY_WAYLAND(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_DISPLAY_WAYLAND,GstVulkanDisplayWayland))
#define GST_VULKAN_DISPLAY_WAYLAND_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VULKAN_DISPLAY_WAYLAND,GstVulkanDisplayWaylandClass))
#define GST_IS_VULKAN_DISPLAY_WAYLAND(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_DISPLAY_WAYLAND))
#define GST_IS_VULKAN_DISPLAY_WAYLAND_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VULKAN_DISPLAY_WAYLAND))
/**
 * GST_VULKAN_DISPLAY_WAYLAND_CAST:
 *
 * Since: 1.18
 */
#define GST_VULKAN_DISPLAY_WAYLAND_CAST(obj)        ((GstVulkanDisplayWayland*)(obj))

typedef struct _GstVulkanDisplayWayland GstVulkanDisplayWayland;
typedef struct _GstVulkanDisplayWaylandClass GstVulkanDisplayWaylandClass;

/**
 * GstVulkanDisplayWayland:
 *
 * the contents of a #GstVulkanDisplayWayland are private and should only be accessed
 * through the provided API
 *
 * Since: 1.18
 */
struct _GstVulkanDisplayWayland
{
  GstVulkanDisplay            parent;

  struct wl_display       *display;
  struct wl_registry      *registry;
  struct wl_compositor    *compositor;
  struct wl_subcompositor *subcompositor;
  struct wl_shell         *shell;

  /* <private> */
  gboolean foreign_display;
};

/**
 * GstVulkanDisplayWaylandClass:s
 *
 * Since: 1.18
 */
struct _GstVulkanDisplayWaylandClass
{
  GstVulkanDisplayClass object_class;
};

/**
 * GST_VULKAN_DISPLAY_WAYLAND_DISPLAY
 *
 * Since: 1.18
 */
#define GST_VULKAN_DISPLAY_WAYLAND_DISPLAY(display_) (GST_VULKAN_DISPLAY_WAYLAND (display_)->display)

GST_VULKAN_API
GstVulkanDisplayWayland *gst_vulkan_display_wayland_new (const gchar * name);
GST_VULKAN_API
GstVulkanDisplayWayland *gst_vulkan_display_wayland_new_with_display (struct wl_display *display);

G_END_DECLS

#endif /* __GST_VULKAN_DISPLAY_WAYLAND_H__ */
