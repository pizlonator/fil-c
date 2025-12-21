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
 * SECTION:gstgldisplay_x11
 * @short_description: X11 Display connection
 * @title: GstGLDisplayX11
 * @see_also: #GstGLDisplay
 *
 * #GstGLDisplayX11 represents a connection to an X11 `Display` handle created
 * internally (gst_gl_display_x11_new()) or wrapped by the application
 * (gst_gl_display_x11_new_with_display())
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gl/x11/gstgldisplay_x11.h>
#include <gst/gl/x11/gstglwindow_x11.h>
#include "xcb_event_source.h"

GST_DEBUG_CATEGORY_STATIC (gst_gl_display_debug);
#define GST_CAT_DEFAULT gst_gl_display_debug

G_DEFINE_TYPE (GstGLDisplayX11, gst_gl_display_x11, GST_TYPE_GL_DISPLAY);

static void gst_gl_display_x11_finalize (GObject * object);
static guintptr gst_gl_display_x11_get_handle (GstGLDisplay * display);

G_GNUC_INTERNAL
    gboolean gst_gl_display_x11_handle_event (GstGLDisplayX11 * display_x11);

extern gboolean gst_gl_window_x11_handle_event (GstGLWindowX11 * window_x11,
    xcb_generic_event_t * event);

static void
gst_gl_display_x11_class_init (GstGLDisplayX11Class * klass)
{
  GST_GL_DISPLAY_CLASS (klass)->get_handle =
      GST_DEBUG_FUNCPTR (gst_gl_display_x11_get_handle);

  G_OBJECT_CLASS (klass)->finalize = gst_gl_display_x11_finalize;
}

static void
gst_gl_display_x11_init (GstGLDisplayX11 * display_x11)
{
  GstGLDisplay *display = (GstGLDisplay *) display_x11;

  display->type = GST_GL_DISPLAY_TYPE_X11;
  display_x11->foreign_display = FALSE;
}

static void
gst_gl_display_x11_finalize (GObject * object)
{
  GstGLDisplayX11 *display_x11 = GST_GL_DISPLAY_X11 (object);

  g_free (display_x11->name);

  if (!display_x11->foreign_display && display_x11->display) {
    XSync (display_x11->display, FALSE);
    XCloseDisplay (display_x11->display);
  }

  G_OBJECT_CLASS (gst_gl_display_x11_parent_class)->finalize (object);
}

/**
 * gst_gl_display_x11_new:
 * @name: (allow-none): a display name
 *
 * Create a new #GstGLDisplayX11 from the x11 display name.  See `XOpenDisplay`()
 * for details on what is a valid name.
 *
 * Returns: (transfer full) (nullable): a new #GstGLDisplayX11 or %NULL
 */
GstGLDisplayX11 *
gst_gl_display_x11_new (const gchar * name)
{
  GstGLDisplayX11 *ret;

  GST_DEBUG_CATEGORY_GET (gst_gl_display_debug, "gldisplay");

  ret = g_object_new (GST_TYPE_GL_DISPLAY_X11, NULL);
  gst_object_ref_sink (ret);
  ret->name = g_strdup (name);
  ret->display = XOpenDisplay (ret->name);

  if (!ret->display) {
    GST_INFO ("Failed to open X11 display connection with name, \'%s\'", name);
    gst_object_unref (ret);
    return NULL;
  }

  ret->xcb_connection = XGetXCBConnection (ret->display);
  if (!ret->xcb_connection) {
    GST_ERROR ("Failed to retrieve XCB connection from X11 Display");
    gst_object_unref (ret);
    return NULL;
  }

  XSetEventQueueOwner (ret->display, XCBOwnsEventQueue);

  GST_GL_DISPLAY (ret)->event_source = xcb_event_source_new (ret);
  g_source_attach (GST_GL_DISPLAY (ret)->event_source,
      GST_GL_DISPLAY (ret)->main_context);

  return ret;
}

/**
 * gst_gl_display_x11_new_with_display: (skip)
 * @display: an existing, x11 display
 *
 * Creates a new display connection from a X11 Display.
 *
 * Returns: (transfer full): a new #GstGLDisplayX11
 */
GstGLDisplayX11 *
gst_gl_display_x11_new_with_display (Display * display)
{
  GstGLDisplayX11 *ret;

  g_return_val_if_fail (display != NULL, NULL);

  GST_DEBUG_CATEGORY_GET (gst_gl_display_debug, "gldisplay");

  ret = g_object_new (GST_TYPE_GL_DISPLAY_X11, NULL);
  gst_object_ref_sink (ret);

  ret->name = g_strdup (DisplayString (display));
  ret->display = display;

  ret->xcb_connection = XGetXCBConnection (ret->display);
  if (!ret->xcb_connection) {
    GST_ERROR ("Failed to retrieve XCB connection from X11 Display");
    gst_object_unref (ret);
    return NULL;
  }

  ret->foreign_display = TRUE;

  return ret;
}

static guintptr
gst_gl_display_x11_get_handle (GstGLDisplay * display)
{
  return (guintptr) GST_GL_DISPLAY_X11 (display)->display;
}

static int
_compare_xcb_window (GstGLWindowX11 * window_x11, xcb_window_t * window_id)
{
  return window_x11->internal_win_id - *window_id;
}

static GstGLWindowX11 *
_find_window_from_xcb_window (GstGLDisplayX11 * display_x11,
    xcb_window_t window_id)
{
  GstGLDisplay *display = GST_GL_DISPLAY (display_x11);
  GstGLWindow *window = NULL;

  if (!window_id)
    return NULL;

  window = gst_gl_display_retrieve_window (display, &window_id,
      (GCompareFunc) _compare_xcb_window);

  return (GstGLWindowX11 *) window;
}

static GstGLWindowX11 *
_window_from_event (GstGLDisplayX11 * display_x11, xcb_generic_event_t * event)
{
  uint8_t event_code = event->response_type & 0x7f;

  switch (event_code) {
/* *INDENT-OFF* */
#define WIN_FROM_EVENT(case_val,event_type,window_field) \
    case case_val:{ \
      event_type * real_event = (event_type *) event; \
      return _find_window_from_xcb_window (display_x11, real_event->window_field); \
    }
    WIN_FROM_EVENT (XCB_CLIENT_MESSAGE, xcb_client_message_event_t, window)
    WIN_FROM_EVENT (XCB_CONFIGURE_NOTIFY, xcb_configure_notify_event_t, window)
    WIN_FROM_EVENT (XCB_EXPOSE, xcb_expose_event_t, window)
    WIN_FROM_EVENT (XCB_KEY_PRESS, xcb_key_press_event_t, event)
    WIN_FROM_EVENT (XCB_KEY_RELEASE, xcb_key_release_event_t, event)
    WIN_FROM_EVENT (XCB_BUTTON_PRESS, xcb_button_press_event_t, event)
    WIN_FROM_EVENT (XCB_BUTTON_RELEASE, xcb_button_release_event_t, event)
    WIN_FROM_EVENT (XCB_MOTION_NOTIFY, xcb_motion_notify_event_t, event)
#undef WIN_FROM_EVENT
/* *INDENT-ON* */
    default:
      return NULL;
  }
}

gboolean
gst_gl_display_x11_handle_event (GstGLDisplayX11 * display_x11)
{
  xcb_connection_t *connection = display_x11->xcb_connection;
  xcb_generic_event_t *event;
  gboolean ret = TRUE;

  while ((event = xcb_poll_for_event (connection))) {
    GstGLWindowX11 *window_x11 = _window_from_event (display_x11, event);

    GST_TRACE_OBJECT (display_x11, "got event %p to window %" GST_PTR_FORMAT,
        event, window_x11);

    if (window_x11) {
      ret = gst_gl_window_x11_handle_event (window_x11, event);
    } else {
      /* unknown window, ignore */
      ret = TRUE;
    }

    if (window_x11)
      gst_object_unref (window_x11);
    g_free (event);
  }

  return ret;
}
