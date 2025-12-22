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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/vulkan/wayland/gstvkdisplay_wayland.h>
#include "gstvkdisplay_wayland_private.h"

#include "wayland_event_source.h"

GST_DEBUG_CATEGORY_STATIC (gst_vulkan_display_wayland_debug);
#define GST_CAT_DEFAULT gst_vulkan_display_wayland_debug

G_DEFINE_TYPE_WITH_CODE (GstVulkanDisplayWayland, gst_vulkan_display_wayland,
    GST_TYPE_VULKAN_DISPLAY, G_ADD_PRIVATE (GstVulkanDisplayWayland);
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT,
        "vulkandisplaywayland", 0, "Vulkan Wayland Display");
    );

static void gst_vulkan_display_wayland_finalize (GObject * object);
static gpointer gst_vulkan_display_wayland_get_handle (GstVulkanDisplay *
    display);

static void
handle_xdg_wm_base_ping (void *user_data, struct xdg_wm_base *xdg_wm_base,
    uint32_t serial)
{
  xdg_wm_base_pong (xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
  handle_xdg_wm_base_ping
};

static void
registry_handle_global (void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version)
{
  GstVulkanDisplayWayland *display = data;
  GstVulkanDisplayWaylandPrivate *priv =
      gst_vulkan_display_wayland_get_instance_private (display);

  GST_TRACE_OBJECT (display, "registry_handle_global with registry %p, "
      "interface %s, version %u", registry, interface, version);

  if (g_strcmp0 (interface, "wl_compositor") == 0) {
    display->compositor =
        wl_registry_bind (registry, name, &wl_compositor_interface, 1);
  } else if (g_strcmp0 (interface, "wl_subcompositor") == 0) {
    display->subcompositor =
        wl_registry_bind (registry, name, &wl_subcompositor_interface, 1);
  } else if (g_strcmp0 (interface, "wl_shell") == 0) {
    display->shell = wl_registry_bind (registry, name, &wl_shell_interface, 1);
  } else if (g_strcmp0 (interface, "xdg_wm_base") == 0) {
    priv->xdg_wm_base =
        wl_registry_bind (registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener (priv->xdg_wm_base, &xdg_wm_base_listener,
        display);
  }
}

static void
registry_handle_global_remove (void *data, struct wl_registry *registry,
    uint32_t name)
{
  GstVulkanDisplayWayland *window_wayland = data;

  /* TODO: deal with any registry objects that may be removed */
  GST_TRACE_OBJECT (window_wayland, "wl_registry %p global_remove %"
      G_GUINT32_FORMAT, registry, name);
}

static const struct wl_registry_listener registry_listener = {
  registry_handle_global,
  registry_handle_global_remove,
};

static void
_connect_listeners (GstVulkanDisplayWayland * display)
{
  display->registry = wl_display_get_registry (display->display);
  wl_registry_add_listener (display->registry, &registry_listener, display);

  wl_display_roundtrip (display->display);
}

GstVulkanDisplayWaylandPrivate *
gst_vulkan_display_wayland_get_private (GstVulkanDisplayWayland *
    display_wayland)
{
  return gst_vulkan_display_wayland_get_instance_private (display_wayland);
}

static void
gst_vulkan_display_wayland_class_init (GstVulkanDisplayWaylandClass * klass)
{
  GST_VULKAN_DISPLAY_CLASS (klass)->get_handle =
      GST_DEBUG_FUNCPTR (gst_vulkan_display_wayland_get_handle);

  G_OBJECT_CLASS (klass)->finalize = gst_vulkan_display_wayland_finalize;
}

static void
gst_vulkan_display_wayland_init (GstVulkanDisplayWayland * display_wayland)
{
  GstVulkanDisplay *display = (GstVulkanDisplay *) display_wayland;

  display->type = GST_VULKAN_DISPLAY_TYPE_WAYLAND;
  display_wayland->foreign_display = FALSE;
}

static void
gst_vulkan_display_wayland_finalize (GObject * object)
{
  GstVulkanDisplayWayland *display_wayland =
      GST_VULKAN_DISPLAY_WAYLAND (object);

  if (!display_wayland->foreign_display && display_wayland->display) {
    wl_display_flush (display_wayland->display);
    wl_display_disconnect (display_wayland->display);
  }

  G_OBJECT_CLASS (gst_vulkan_display_wayland_parent_class)->finalize (object);
}

/**
 * gst_vulkan_display_wayland_new:
 * @name: (nullable): a display name
 *
 * Create a new #GstVulkanDisplayWayland from the wayland display name.  See `wl_display_connect`()
 * for details on what is a valid name.
 *
 * Returns: (transfer full) (nullable): a new #GstVulkanDisplayWayland or %NULL
 *
 * Since: 1.18
 */
GstVulkanDisplayWayland *
gst_vulkan_display_wayland_new (const gchar * name)
{
  GstVulkanDisplayWayland *ret;
  struct wl_display *display;

  display = wl_display_connect (name);

  if (!display) {
    GST_ERROR ("Failed to open Wayland display connection with name, \'%s\'",
        name);
    return NULL;
  }

  ret = g_object_new (GST_TYPE_VULKAN_DISPLAY_WAYLAND, NULL);
  gst_object_ref_sink (ret);
  ret->display = display;

  /* connecting the listeners after attaching the event source will race with
   * the source and the source may eat an event that we're waiting for and
   * deadlock */
  _connect_listeners (ret);

  GST_VULKAN_DISPLAY (ret)->event_source =
      wayland_event_source_new (ret->display, NULL);
  g_source_attach (GST_VULKAN_DISPLAY (ret)->event_source,
      GST_VULKAN_DISPLAY (ret)->main_context);

  return ret;
}

/**
 * gst_vulkan_display_wayland_new_with_display:
 * @display: an existing, wayland display
 *
 * Creates a new display connection from a wl_display Display.
 *
 * Returns: (transfer full): a new #GstVulkanDisplayWayland
 *
 * Since: 1.18
 */
GstVulkanDisplayWayland *
gst_vulkan_display_wayland_new_with_display (struct wl_display *display)
{
  GstVulkanDisplayWayland *ret;

  g_return_val_if_fail (display != NULL, NULL);

  ret = g_object_new (GST_TYPE_VULKAN_DISPLAY_WAYLAND, NULL);
  gst_object_ref_sink (ret);

  ret->display = display;
  ret->foreign_display = TRUE;

  _connect_listeners (ret);

  return ret;
}

static gpointer
gst_vulkan_display_wayland_get_handle (GstVulkanDisplay * display)
{
  return GST_VULKAN_DISPLAY_WAYLAND (display)->display;
}

static gboolean
_roundtrip_async (gpointer data)
{
  GstVulkanDisplayWayland *display = data;

  wl_display_roundtrip (display->display);

  return G_SOURCE_REMOVE;
}

void
gst_vulkan_display_wayland_roundtrip_async (GstVulkanDisplayWayland * display)
{
  g_return_if_fail (GST_IS_VULKAN_DISPLAY_WAYLAND (display));

  g_main_context_invoke (GST_VULKAN_DISPLAY (display)->main_context,
      (GSourceFunc) _roundtrip_async, display);
}
