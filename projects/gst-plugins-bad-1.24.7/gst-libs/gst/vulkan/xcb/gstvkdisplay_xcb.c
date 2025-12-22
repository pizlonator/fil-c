/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#include <gst/vulkan/gstvkapi.h>

#include <gst/vulkan/xcb/gstvkdisplay_xcb.h>
#include "xcb_event_source.h"

#define GST_CAT_DEFAULT gst_vulkan_display_debug
GST_DEBUG_CATEGORY_STATIC (gst_vulkan_display_debug);

G_DEFINE_TYPE (GstVulkanDisplayXCB, gst_vulkan_display_xcb,
    GST_TYPE_VULKAN_DISPLAY);

static void gst_vulkan_display_xcb_finalize (GObject * object);
static gpointer gst_vulkan_display_xcb_get_handle (GstVulkanDisplay * display);

static void
gst_vulkan_display_xcb_class_init (GstVulkanDisplayXCBClass * klass)
{
  GST_VULKAN_DISPLAY_CLASS (klass)->get_handle =
      GST_DEBUG_FUNCPTR (gst_vulkan_display_xcb_get_handle);

  G_OBJECT_CLASS (klass)->finalize = gst_vulkan_display_xcb_finalize;
}

static void
gst_vulkan_display_xcb_init (GstVulkanDisplayXCB * display_xcb)
{
  GstVulkanDisplay *display = (GstVulkanDisplay *) display_xcb;

  display->type = GST_VULKAN_DISPLAY_TYPE_XCB;
  display_xcb->foreign_display = FALSE;
}

static void
gst_vulkan_display_xcb_finalize (GObject * object)
{
  GstVulkanDisplayXCB *display_xcb = GST_VULKAN_DISPLAY_XCB (object);

  G_OBJECT_CLASS (gst_vulkan_display_xcb_parent_class)->finalize (object);

  if (!display_xcb->foreign_display && display_xcb->connection)
    xcb_disconnect (display_xcb->connection);
  display_xcb->connection = NULL;
}

static xcb_screen_t *
_get_screen_from_connection (xcb_connection_t * connection, int screen_no)
{
  const xcb_setup_t *setup;
  xcb_screen_iterator_t iter;

  setup = xcb_get_setup (connection);
  iter = xcb_setup_roots_iterator (setup);
  while (screen_no-- > 0)
    xcb_screen_next (&iter);

  return iter.data;
}

/**
 * gst_vulkan_display_xcb_new:
 * @name: (nullable): a display name
 *
 * Create a new #GstVulkanDisplayXCB from the xcb display name.  See XOpenDisplay\()
 * for details on what is a valid name.
 *
 * Returns: (transfer full) (nullable): a new #GstVulkanDisplayXCB or %NULL
 *
 * Since: 1.18
 */
GstVulkanDisplayXCB *
gst_vulkan_display_xcb_new (const gchar * name)
{
  xcb_connection_t *connection;
  GstVulkanDisplayXCB *ret;
  int screen_no = 0;

  GST_DEBUG_CATEGORY_GET (gst_vulkan_display_debug, "vulkandisplay");

  connection = xcb_connect (NULL, &screen_no);
  if (connection == NULL || xcb_connection_has_error (connection)) {
    GST_ERROR ("Failed to open XCB display connection with name, \'%s\'", name);
    return NULL;
  }

  ret = gst_vulkan_display_xcb_new_with_connection (connection, screen_no);
  GST_VULKAN_DISPLAY (ret)->event_source = xcb_event_source_new (ret);
  g_source_attach (GST_VULKAN_DISPLAY (ret)->event_source,
      GST_VULKAN_DISPLAY (ret)->main_context);
  ret->foreign_display = FALSE;

  return ret;
}

/**
 * gst_vulkan_display_xcb_new_with_connection: (skip)
 * @connection: an existing, xcb display
 * @screen_no: XCB screen number
 *
 * Creates a new display connection from a XCB Display.
 *
 * Returns: (transfer full): a new #GstVulkanDisplayXCB
 *
 * Since: 1.18
 */
GstVulkanDisplayXCB *
gst_vulkan_display_xcb_new_with_connection (xcb_connection_t * connection,
    int screen_no)
{
  GstVulkanDisplayXCB *ret;

  g_return_val_if_fail (connection != NULL, NULL);

  GST_DEBUG_CATEGORY_GET (gst_vulkan_display_debug, "vulkandisplay");

  ret = g_object_new (GST_TYPE_VULKAN_DISPLAY_XCB, NULL);
  gst_object_ref_sink (ret);

  ret->connection = connection;
  ret->screen = _get_screen_from_connection (connection, screen_no);
  ret->root_window = ret->screen->root;
  ret->foreign_display = TRUE;

  return ret;
}

static gpointer
gst_vulkan_display_xcb_get_handle (GstVulkanDisplay * display)
{
  return (gpointer) GST_VULKAN_DISPLAY_XCB_CONNECTION (display);
}
