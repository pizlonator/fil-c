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

#define GLIB_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <linux/input.h>

#include <gst/vulkan/wayland/gstvkdisplay_wayland.h>
#include "gstvkwindow_wayland.h"
#include "gstvkdisplay_wayland_private.h"

#include "wayland_event_source.h"

#define GST_CAT_DEFAULT gst_vulkan_window_wayland_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static void
_init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkanwindowwayland", 0,
        "Vulkan Wayland Window");
    g_once_init_leave (&_init, 1);
  }
}

#define gst_vulkan_window_wayland_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanWindowWayland, gst_vulkan_window_wayland,
    GST_TYPE_VULKAN_WINDOW, _init_debug ());

static void gst_vulkan_window_wayland_close (GstVulkanWindow * window);
static gboolean gst_vulkan_window_wayland_open (GstVulkanWindow * window,
    GError ** error);
static VkSurfaceKHR gst_vulkan_window_wayland_get_surface (GstVulkanWindow
    * window, GError ** error);
static gboolean
gst_vulkan_window_wayland_get_presentation_support (GstVulkanWindow *
    window, GstVulkanDevice * device, guint32 queue_family_idx);

static void
handle_xdg_toplevel_close (void *data, struct xdg_toplevel *xdg_toplevel)
{
  GstVulkanWindow *window = data;

  GST_DEBUG ("XDG toplevel got a \"close\" event.");

  gst_vulkan_window_close (window);
}

static void
handle_xdg_toplevel_configure (void *data, struct xdg_toplevel *xdg_toplevel,
    int32_t width, int32_t height, struct wl_array *states)
{
#if 0
  GstVulkanWindowWayland *window_wl = data;
  const uint32_t *state;
#endif
  GST_DEBUG ("configure event on XDG toplevel %p, %ix%i", xdg_toplevel,
      width, height);
#if 0
  // TODO: deal with resizes
  if (width > 0 && height > 0)
    window_resize (window_egl, width, height);
#endif
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
  handle_xdg_toplevel_configure,
  handle_xdg_toplevel_close,
};

static void
handle_xdg_surface_configure (void *data, struct xdg_surface *xdg_surface,
    uint32_t serial)
{
  xdg_surface_ack_configure (xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
  handle_xdg_surface_configure,
};

static void
create_xdg_surface (GstVulkanWindowWayland * window_wl)
{
  GstVulkanDisplayWayland *display =
      GST_VULKAN_DISPLAY_WAYLAND (GST_VULKAN_WINDOW (window_wl)->display);
  GstVulkanDisplayWaylandPrivate *display_priv =
      gst_vulkan_display_wayland_get_private (display);
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;

  GST_DEBUG ("Creating surfaces with XDG-shell");

  /* First create the XDG surface */
  xdg_surface = xdg_wm_base_get_xdg_surface (display_priv->xdg_wm_base,
      window_wl->surface);
  xdg_surface_add_listener (xdg_surface, &xdg_surface_listener, window_wl);

  /* Then the XDG top-level */
  xdg_toplevel = xdg_surface_get_toplevel (xdg_surface);
  if (g_get_prgname ()) {
    xdg_toplevel_set_app_id (xdg_toplevel, g_get_prgname ());
  } else {
    xdg_toplevel_set_app_id (xdg_toplevel, "org.gstreamer.wayland");
  }
  xdg_toplevel_set_title (xdg_toplevel, "Vulkan Renderer");
  xdg_toplevel_add_listener (xdg_toplevel, &xdg_toplevel_listener, window_wl);

  /* Commit the xdg_surface state */
  wl_surface_commit (window_wl->surface);

  /* And save them into the fields */
  window_wl->xdg_surface = xdg_surface;
  window_wl->xdg_toplevel = xdg_toplevel;
}

static void
handle_ping (void *data, struct wl_shell_surface *shell_surface,
    uint32_t serial)
{
  GstVulkanWindowWayland *window_wl = data;

  GST_TRACE_OBJECT (window_wl, "ping received serial %u", serial);

  wl_shell_surface_pong (shell_surface, serial);
}

/*
static void window_resize (GstVulkanWindowWayland * window_wl, guint width,
    guint height);*/

static void
handle_configure (void *data, struct wl_shell_surface *shell_surface,
    uint32_t edges, int32_t width, int32_t height)
{
  GstVulkanWindowWayland *window_wl = data;

  GST_DEBUG_OBJECT (window_wl, "configure event on surface %p, %ix%i",
      shell_surface, width, height);

  /*window_resize (window_wl, width, height); */
}

static void
handle_popup_done (void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
  handle_ping,
  handle_configure,
  handle_popup_done
};

static void
create_wl_shell_surface (GstVulkanWindowWayland * window_wl)
{
  GstVulkanDisplayWayland *display =
      GST_VULKAN_DISPLAY_WAYLAND (GST_VULKAN_WINDOW (window_wl)->display);

  GST_DEBUG ("Creating surfaces with wl_shell");

  window_wl->shell_surface =
      wl_shell_get_shell_surface (display->shell, window_wl->surface);
  if (window_wl->queue)
    wl_proxy_set_queue ((struct wl_proxy *) window_wl->shell_surface,
        window_wl->queue);

  wl_shell_surface_add_listener (window_wl->shell_surface,
      &shell_surface_listener, window_wl);

  wl_shell_surface_set_title (window_wl->shell_surface, "Vulkan Renderer");
  wl_shell_surface_set_toplevel (window_wl->shell_surface);
  GST_DEBUG_OBJECT (window_wl, "Successfully created shell surface %p",
      window_wl->shell_surface);
}

static void
destroy_surfaces (GstVulkanWindowWayland * window_wl)
{
  GST_DEBUG_OBJECT (window_wl, "destroying created surfaces");

  g_clear_pointer (&window_wl->xdg_toplevel, xdg_toplevel_destroy);
  g_clear_pointer (&window_wl->xdg_surface, xdg_surface_destroy);
  g_clear_pointer (&window_wl->shell_surface, wl_shell_surface_destroy);
  g_clear_pointer (&window_wl->surface, wl_surface_destroy);
}

static void
create_surfaces (GstVulkanWindowWayland * window_wl)
{
  GstVulkanDisplayWayland *display =
      GST_VULKAN_DISPLAY_WAYLAND (GST_VULKAN_WINDOW (window_wl)->display);
  GstVulkanDisplayWaylandPrivate *display_priv =
      gst_vulkan_display_wayland_get_private (display);
  gint width, height;

  if (!window_wl->surface) {
    window_wl->surface = wl_compositor_create_surface (display->compositor);
    if (window_wl->queue)
      wl_proxy_set_queue ((struct wl_proxy *) window_wl->surface,
          window_wl->queue);
  }

  if (display_priv->xdg_wm_base) {
    create_xdg_surface (window_wl);
  } else {
    create_wl_shell_surface (window_wl);
  }

  if (window_wl->window_width > 0)
    width = window_wl->window_width;
  else
    width = 320;
  window_wl->window_width = width;

  if (window_wl->window_height > 0)
    height = window_wl->window_height;
  else
    height = 240;
  window_wl->window_height = height;

  gst_vulkan_window_resize (GST_VULKAN_WINDOW (window_wl),
      window_wl->window_width, window_wl->window_height);
}

static void
gst_vulkan_window_wayland_class_init (GstVulkanWindowWaylandClass * klass)
{
  GstVulkanWindowClass *window_class = (GstVulkanWindowClass *) klass;

  window_class->close = GST_DEBUG_FUNCPTR (gst_vulkan_window_wayland_close);
  window_class->open = GST_DEBUG_FUNCPTR (gst_vulkan_window_wayland_open);
  window_class->get_surface =
      GST_DEBUG_FUNCPTR (gst_vulkan_window_wayland_get_surface);
  window_class->get_presentation_support =
      GST_DEBUG_FUNCPTR (gst_vulkan_window_wayland_get_presentation_support);
}

static void
gst_vulkan_window_wayland_init (GstVulkanWindowWayland * window)
{
}

GstVulkanWindowWayland *
gst_vulkan_window_wayland_new (GstVulkanDisplay * display)
{
  GstVulkanWindowWayland *window;

  if ((gst_vulkan_display_get_handle_type (display) &
          GST_VULKAN_DISPLAY_TYPE_WAYLAND)
      == 0)
    /* we require a wayland display to create wayland surfaces */
    return NULL;

  _init_debug ();

  GST_DEBUG ("creating Wayland window");

  window = g_object_new (GST_TYPE_VULKAN_WINDOW_WAYLAND, NULL);
  gst_object_ref_sink (window);

  return window;
}

static void
gst_vulkan_window_wayland_close (GstVulkanWindow * window)
{
  GstVulkanWindowWayland *window_wl;

  window_wl = GST_VULKAN_WINDOW_WAYLAND (window);

  destroy_surfaces (window_wl);

  g_source_destroy (window_wl->wl_source);
  g_source_unref (window_wl->wl_source);
  window_wl->wl_source = NULL;

  GST_VULKAN_WINDOW_CLASS (parent_class)->close (window);
}

static gboolean
gst_vulkan_window_wayland_open (GstVulkanWindow * window, GError ** error)
{
  GstVulkanDisplayWayland *display;
  GstVulkanWindowWayland *window_wl = GST_VULKAN_WINDOW_WAYLAND (window);

  if (!GST_IS_VULKAN_DISPLAY_WAYLAND (window->display)) {
    g_set_error (error, GST_VULKAN_WINDOW_ERROR,
        GST_VULKAN_WINDOW_ERROR_RESOURCE_UNAVAILABLE,
        "Failed to retrieve Wayland display (wrong type?)");
    return FALSE;
  }
  display = GST_VULKAN_DISPLAY_WAYLAND (window->display);

  if (!display->display) {
    g_set_error (error, GST_VULKAN_WINDOW_ERROR,
        GST_VULKAN_WINDOW_ERROR_RESOURCE_UNAVAILABLE,
        "Failed to retrieve Wayland display");
    return FALSE;
  }

  window_wl->queue = NULL;

  if (!GST_VULKAN_WINDOW_CLASS (parent_class)->open (window, error))
    return FALSE;

  create_surfaces (window_wl);

  gst_vulkan_display_wayland_roundtrip_async (display);

  return TRUE;
}

static VkSurfaceKHR
gst_vulkan_window_wayland_get_surface (GstVulkanWindow * window,
    GError ** error)
{
  GstVulkanWindowWayland *window_wl = GST_VULKAN_WINDOW_WAYLAND (window);
  VkWaylandSurfaceCreateInfoKHR info = { 0, };
  VkSurfaceKHR ret;
  VkResult err;

  info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
  info.pNext = NULL;
  info.flags = 0;
  info.display = GST_VULKAN_DISPLAY_WAYLAND_DISPLAY (window->display);
  info.surface = window_wl->surface;

  if (!window_wl->CreateWaylandSurface)
    window_wl->CreateWaylandSurface =
        gst_vulkan_instance_get_proc_address (window->display->instance,
        "vkCreateWaylandSurfaceKHR");
  if (!window_wl->CreateWaylandSurface) {
    g_set_error_literal (error, GST_VULKAN_ERROR, VK_ERROR_FEATURE_NOT_PRESENT,
        "Could not retrieve \"vkCreateWaylandSurfaceKHR\" function pointer");
    return VK_NULL_HANDLE;
  }

  err =
      window_wl->CreateWaylandSurface (window->display->instance->instance,
      &info, NULL, &ret);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateWaylandSurfaceKHR") < 0)
    return VK_NULL_HANDLE;

  return ret;
}

static gboolean
gst_vulkan_window_wayland_get_presentation_support (GstVulkanWindow * window,
    GstVulkanDevice * device, guint32 queue_family_idx)
{
  GstVulkanWindowWayland *window_wl = GST_VULKAN_WINDOW_WAYLAND (window);
  VkPhysicalDevice gpu;

  if (!window_wl->GetPhysicalDeviceWaylandPresentationSupport)
    window_wl->GetPhysicalDeviceWaylandPresentationSupport =
        gst_vulkan_instance_get_proc_address (window->display->instance,
        "vkGetPhysicalDeviceWaylandPresentationSupportKHR");
  if (!window_wl->GetPhysicalDeviceWaylandPresentationSupport) {
    GST_WARNING_OBJECT (window, "Could not retrieve "
        "\"vkGetPhysicalDeviceWaylandPresentationSupportKHR\" "
        "function pointer");
    return FALSE;
  }

  gpu = gst_vulkan_device_get_physical_device (device);
  if (window_wl->GetPhysicalDeviceWaylandPresentationSupport (gpu,
          queue_family_idx,
          GST_VULKAN_DISPLAY_WAYLAND_DISPLAY (window->display)))
    return TRUE;
  return FALSE;
}
