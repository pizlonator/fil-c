/*
 * GStreamer
 * Copyright (C) 2009 David A. Schleef <ds@schleef.org>
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

#include <gst/gl/gl.h>
#include "gstgtk.h"

#if GST_GL_HAVE_WINDOW_WIN32 && defined(GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#endif
#if GST_GL_HAVE_WINDOW_X11 && defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#endif
#if GST_GL_HAVE_WINDOW_WAYLAND && defined(GDK_WINDOWING_WAYLAND)
#include <gdk/gdkwayland.h>
#endif
#if GST_GL_HAVE_WINDOW_COCOA && defined(GDK_WINDOWING_QUARTZ)
#include <gdk/gdkquartz.h>
#if GTK_CHECK_VERSION(3, 24, 10)
#include <AppKit/AppKit.h>
NSView *gdk_quartz_window_get_nsview (GdkWindow * window);
#endif
#endif

gboolean
gst_gtk_handle_need_context (GstBus * bus, GstMessage * msg, gpointer data)
{
  gboolean ret = FALSE;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_NEED_CONTEXT:
    {
      const gchar *context_type;

      gst_message_parse_context_type (msg, &context_type);
      g_print ("got need context %s\n", context_type);

      if (g_strcmp0 (context_type, "GstWaylandDisplayHandleContextType") == 0) {
#if GST_GL_HAVE_WINDOW_WAYLAND && defined(GDK_WINDOWING_WAYLAND)
        GstContext *context = NULL;
        GdkDisplay *gdk_display = gdk_display_get_default ();
        if (GDK_IS_WAYLAND_DISPLAY (gdk_display)) {
          struct wl_display *wayland_display =
              gdk_wayland_display_get_wl_display (gdk_display);
          if (wayland_display) {
            GstStructure *s;

            context =
                gst_context_new ("GstWaylandDisplayHandleContextType", TRUE);

            s = gst_context_writable_structure (context);
            gst_structure_set (s, "display", G_TYPE_POINTER, wayland_display,
                NULL);

            gst_element_set_context (GST_ELEMENT (msg->src), context);

            ret = TRUE;
          }
        }
#else
        GST_ERROR
            ("Asked for wayland display context, but compiled without wayland support");
#endif
      }
    }
    default:
      break;
  }

  return ret;
}

void
gst_video_overlay_set_gtk_window (GstVideoOverlay * videooverlay,
    GtkWidget * widget)
{
  GdkWindow *window;
  GdkDisplay *display;
  const gchar *user_choice = g_getenv ("GST_GL_WINDOW");

  window = gtk_widget_get_window (widget);
  display = gdk_window_get_display (window);

#if GST_GL_HAVE_WINDOW_WIN32 && defined(GDK_WINDOWING_WIN32)
  if (GDK_IS_WIN32_DISPLAY (display) && (!user_choice
          || g_strcmp0 (user_choice, "win32") == 0)) {
    gst_video_overlay_set_window_handle (videooverlay,
        (guintptr) GDK_WINDOW_HWND (window));
  } else
#endif
#if GST_GL_HAVE_WINDOW_COCOA && defined(GDK_WINDOWING_QUARTZ)
  if (GDK_IS_QUARTZ_DISPLAY (display) && (!user_choice
          || g_strcmp0 (user_choice, "cocoa") == 0)) {
    gst_video_overlay_set_window_handle (videooverlay, (guintptr)
        gdk_quartz_window_get_nsview (window));
  } else
#endif
#if GST_GL_HAVE_WINDOW_X11 && defined(GDK_WINDOWING_X11)
  if (GDK_IS_X11_DISPLAY (display) && (!user_choice
          || g_strcmp0 (user_choice, "x11") == 0)) {
    gst_video_overlay_set_window_handle (videooverlay, GDK_WINDOW_XID (window));
  } else
#endif
#if GST_GL_HAVE_WINDOW_WAYLAND && defined(GDK_WINDOWING_WAYLAND)
  if (GDK_IS_WAYLAND_DISPLAY (display) && (!user_choice
          || g_strcmp0 (user_choice, "wayland") == 0)) {
    gst_video_overlay_set_window_handle (videooverlay,
        (guintptr) gdk_wayland_window_get_wl_surface (window));
  } else
#endif
    g_error ("Unsupported Gtk+ backend");
}
