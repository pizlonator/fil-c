/*
 * GStreamer
 * Copyright (C) 2013 Matthew Waters <ystreet00@gmail.com>
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

/**
 * SECTION:gstgldisplay_wayland
 * @short_description: Wayland display connection
 * @title: GstGLDisplayWayland
 * @see_also: #GstGLDisplay
 *
 * #GstGLDisplayWayland represents a connection to a Wayland `wl_display` handle
 * created internally (gst_gl_display_wayland_new()) or wrapped by the application
 * (gst_gl_display_wayland_new_with_display())
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgldisplay_wayland.h"

GST_DEBUG_CATEGORY_STATIC (gst_gl_display_debug);
#define GST_CAT_DEFAULT gst_gl_display_debug

/* We can't define these in the public struct, or we'd break ABI */
typedef struct _GstGLDisplayWaylandPrivate
{
  gint dummy;
} GstGLDisplayWaylandPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GstGLDisplayWayland, gst_gl_display_wayland,
    GST_TYPE_GL_DISPLAY);

static void gst_gl_display_wayland_finalize (GObject * object);
static guintptr gst_gl_display_wayland_get_handle (GstGLDisplay * display);

static void
gst_gl_display_wayland_class_init (GstGLDisplayWaylandClass * klass)
{
  GST_GL_DISPLAY_CLASS (klass)->get_handle =
      GST_DEBUG_FUNCPTR (gst_gl_display_wayland_get_handle);

  G_OBJECT_CLASS (klass)->finalize = gst_gl_display_wayland_finalize;
}

static void
gst_gl_display_wayland_init (GstGLDisplayWayland * display_wayland)
{
  GstGLDisplay *display = (GstGLDisplay *) display_wayland;

  display->type = GST_GL_DISPLAY_TYPE_WAYLAND;
  display_wayland->foreign_display = FALSE;
}

static void
gst_gl_display_wayland_finalize (GObject * object)
{
  GstGLDisplayWayland *display_wayland = GST_GL_DISPLAY_WAYLAND (object);

  /* Cause eglTerminate() to occur before wl_display_disconnect()
   * https://bugzilla.gnome.org/show_bug.cgi?id=787293 */
  g_object_set_data (object, "gst.gl.display.egl", NULL);

  if (!display_wayland->foreign_display && display_wayland->display) {
    wl_display_flush (display_wayland->display);
    wl_display_disconnect (display_wayland->display);
  }

  G_OBJECT_CLASS (gst_gl_display_wayland_parent_class)->finalize (object);
}

/**
 * gst_gl_display_wayland_new:
 * @name: (allow-none): a display name
 *
 * Create a new #GstGLDisplayWayland from the wayland display name.  See `wl_display_connect`()
 * for details on what is a valid name.
 *
 * Returns: (transfer full) (nullable): a new #GstGLDisplayWayland or %NULL
 */
GstGLDisplayWayland *
gst_gl_display_wayland_new (const gchar * name)
{
  GstGLDisplayWayland *ret;

  GST_DEBUG_CATEGORY_GET (gst_gl_display_debug, "gldisplay");

  ret = g_object_new (GST_TYPE_GL_DISPLAY_WAYLAND, NULL);
  gst_object_ref_sink (ret);
  ret->display = wl_display_connect (name);

  if (!ret->display) {
    if (name != NULL) {
      GST_ERROR ("Failed to open Wayland display connection with name \'%s\'",
          name);
    } else {
      GST_INFO ("Failed to open Wayland display connection.");
    }
    gst_object_unref (ret);
    return NULL;
  }

  return ret;
}

/**
 * gst_gl_display_wayland_new_with_display:
 * @display: an existing, wayland display
 *
 * Creates a new display connection from a wl_display Display.
 *
 * Returns: (transfer full): a new #GstGLDisplayWayland
 */
GstGLDisplayWayland *
gst_gl_display_wayland_new_with_display (struct wl_display *display)
{
  GstGLDisplayWayland *ret;

  g_return_val_if_fail (display != NULL, NULL);

  GST_DEBUG_CATEGORY_GET (gst_gl_display_debug, "gldisplay");

  ret = g_object_new (GST_TYPE_GL_DISPLAY_WAYLAND, NULL);
  gst_object_ref_sink (ret);

  ret->display = display;
  ret->foreign_display = TRUE;

  return ret;
}

static guintptr
gst_gl_display_wayland_get_handle (GstGLDisplay * display)
{
  return (guintptr) GST_GL_DISPLAY_WAYLAND (display)->display;
}
