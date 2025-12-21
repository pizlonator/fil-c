/*
 * GStreamer
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2012 Matthew Waters <ystreet00@gmail.com>
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

#include <gst/gst.h>
#include <locale.h>

#include "gstglwindow_x11.h"
#include "gstgldisplay_x11.h"

#include "../gstglwindow_private.h"

/* for XkbKeycodeToKeysym */
#include <X11/XKBlib.h>

#define GST_CAT_DEFAULT gst_gl_window_debug

G_GNUC_INTERNAL
    gboolean gst_gl_window_x11_handle_event (GstGLWindowX11 * window_x11,
    xcb_generic_event_t * event);

/* X error trap */
static int TrappedErrorCode = 0;
static int (*old_error_handler) (Display *, XErrorEvent *);

enum
{
  ARG_0,
  ARG_DISPLAY
};

struct _GstGLWindowX11Private
{
  gboolean activate;
  gboolean activate_result;

  gint preferred_width;
  gint preferred_height;

  gboolean handle_events;

  Colormap internal_colormap;
  GstVideoRectangle render_rect;
};

#define gst_gl_window_x11_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstGLWindowX11, gst_gl_window_x11,
    GST_TYPE_GL_WINDOW);

static guintptr gst_gl_window_x11_get_display (GstGLWindow * window);
guintptr gst_gl_window_x11_get_gl_context (GstGLWindow * window);
gboolean gst_gl_window_x11_activate (GstGLWindow * window, gboolean activate);
static void gst_gl_window_x11_set_window_handle (GstGLWindow * window,
    guintptr handle);
static gboolean gst_gl_window_x11_set_render_rectangle (GstGLWindow * window,
    int x, int y, int width, int height);
static guintptr gst_gl_window_x11_get_window_handle (GstGLWindow * window);
static void gst_gl_window_x11_set_preferred_size (GstGLWindow * window,
    gint width, gint height);
static void gst_gl_window_x11_show (GstGLWindow * window);
static void gst_gl_window_x11_draw (GstGLWindow * window);
gboolean gst_gl_window_x11_create_context (GstGLWindow * window,
    GstGLAPI gl_api, guintptr external_gl_context, GError ** error);
static gboolean gst_gl_window_x11_open (GstGLWindow * window, GError ** error);
static void gst_gl_window_x11_close (GstGLWindow * window);
static void gst_gl_window_x11_handle_events (GstGLWindow * window,
    gboolean handle_events);

static void
gst_gl_window_x11_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_gl_window_x11_class_init (GstGLWindowX11Class * klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);
  GstGLWindowClass *window_class = (GstGLWindowClass *) klass;

  obj_class->finalize = gst_gl_window_x11_finalize;

  window_class->get_display = GST_DEBUG_FUNCPTR (gst_gl_window_x11_get_display);
  window_class->set_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_x11_set_window_handle);
  window_class->set_render_rectangle =
      GST_DEBUG_FUNCPTR (gst_gl_window_x11_set_render_rectangle);
  window_class->get_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_x11_get_window_handle);
  window_class->draw = GST_DEBUG_FUNCPTR (gst_gl_window_x11_draw);
  window_class->open = GST_DEBUG_FUNCPTR (gst_gl_window_x11_open);
  window_class->close = GST_DEBUG_FUNCPTR (gst_gl_window_x11_close);
  window_class->handle_events =
      GST_DEBUG_FUNCPTR (gst_gl_window_x11_handle_events);
  window_class->set_preferred_size =
      GST_DEBUG_FUNCPTR (gst_gl_window_x11_set_preferred_size);
  window_class->show = GST_DEBUG_FUNCPTR (gst_gl_window_x11_show);
}

static void
gst_gl_window_x11_init (GstGLWindowX11 * window)
{
  window->priv = gst_gl_window_x11_get_instance_private (window);
}

/* Must be called in the gl thread */
GstGLWindowX11 *
gst_gl_window_x11_new (GstGLDisplay * display)
{
  GstGLWindowX11 *window;

  if ((gst_gl_display_get_handle_type (display) & GST_GL_DISPLAY_TYPE_X11)
      == GST_GL_DISPLAY_TYPE_NONE) {
    GST_INFO ("Wrong display type %u for this window type %u", display->type,
        GST_GL_DISPLAY_TYPE_X11);
    return NULL;
  }

  window = g_object_new (GST_TYPE_GL_WINDOW_X11, NULL);
  gst_object_ref_sink (window);

  return window;
}

static gboolean
gst_gl_window_x11_open (GstGLWindow * window, GError ** error)
{
  GstGLWindowX11 *window_x11 = GST_GL_WINDOW_X11 (window);
  GstGLDisplayX11 *display_x11 = (GstGLDisplayX11 *) window->display;

  window_x11->device = display_x11->display;
//  window_x11->device = XOpenDisplay (display_x11->name);
  if (window_x11->device == NULL) {
    g_set_error (error, GST_GL_WINDOW_ERROR,
        GST_GL_WINDOW_ERROR_RESOURCE_UNAVAILABLE,
        "Failed to connect to X display server");
    goto failure;
  }

  GST_LOG ("gl device id: %ld", (gulong) window_x11->device);

  window_x11->screen = DefaultScreenOfDisplay (window_x11->device);
  window_x11->screen_num = DefaultScreen (window_x11->device);
  window_x11->visual =
      DefaultVisual (window_x11->device, window_x11->screen_num);
  window_x11->root = DefaultRootWindow (window_x11->device);
  window_x11->white = XWhitePixel (window_x11->device, window_x11->screen_num);
  window_x11->black = XBlackPixel (window_x11->device, window_x11->screen_num);
  window_x11->depth = DefaultDepthOfScreen (window_x11->screen);

  GST_LOG ("gl root id: %lud", (gulong) window_x11->root);

  window_x11->device_width =
      DisplayWidth (window_x11->device, window_x11->screen_num);
  window_x11->device_height =
      DisplayHeight (window_x11->device, window_x11->screen_num);

  window_x11->allow_extra_expose_events = TRUE;

  return GST_GL_WINDOW_CLASS (parent_class)->open (window, error);

failure:
  return FALSE;
}

gboolean
gst_gl_window_x11_create_window (GstGLWindowX11 * window_x11)
{
  XSetWindowAttributes win_attr;
  XTextProperty text_property;
  XWMHints wm_hints;
  unsigned long mask;
  const gchar *title = "OpenGL renderer";
  Atom wm_atoms[1];
  gint x = 0, y = 0, width = 1, height = 1;

  if (window_x11->visual_info->visual != window_x11->visual)
    GST_LOG ("selected visual is different from the default");

  GST_LOG ("visual XID:%d, screen:%d, visualid:%d, depth:%d, class:%d, "
      "red_mask:%ld, green_mask:%ld, blue_mask:%ld bpp:%d",
      (gint) XVisualIDFromVisual (window_x11->visual_info->visual),
      window_x11->visual_info->screen, (gint) window_x11->visual_info->visualid,
      window_x11->visual_info->depth, window_x11->visual_info->class,
      window_x11->visual_info->red_mask, window_x11->visual_info->green_mask,
      window_x11->visual_info->blue_mask,
      window_x11->visual_info->bits_per_rgb);

  win_attr.event_mask =
      StructureNotifyMask | ExposureMask | VisibilityChangeMask;
  win_attr.do_not_propagate_mask = NoEventMask;

  win_attr.background_pixmap = None;
  win_attr.background_pixel = 0;
  win_attr.border_pixel = 0;

  window_x11->priv->internal_colormap = win_attr.colormap =
      XCreateColormap (window_x11->device, window_x11->root,
      window_x11->visual_info->visual, AllocNone);

  mask = CWBackPixmap | CWBorderPixel | CWColormap | CWEventMask;

  window_x11->internal_win_id =
      XCreateWindow (window_x11->device,
      window_x11->parent_win ? window_x11->parent_win : window_x11->root,
      x, y, width, height, 0,
      window_x11->visual_info->depth, InputOutput,
      window_x11->visual_info->visual, mask, &win_attr);

  gst_gl_window_x11_handle_events (GST_GL_WINDOW (window_x11),
      window_x11->priv->handle_events);

  XSync (window_x11->device, FALSE);

  XSetWindowBackgroundPixmap (window_x11->device,
      window_x11->internal_win_id, None);

  GST_LOG ("gl window id: %lud", (gulong) window_x11->internal_win_id);
  GST_LOG ("gl window props: x:%d y:%d", x, y);

  wm_atoms[0] = XInternAtom (window_x11->device, "WM_DELETE_WINDOW", True);
  if (wm_atoms[0] == None)
    GST_DEBUG ("Cannot create WM_DELETE_WINDOW");

  XSetWMProtocols (window_x11->device, window_x11->internal_win_id,
      wm_atoms, 1);

  wm_hints.flags = StateHint;
  wm_hints.initial_state = NormalState;
  wm_hints.input = False;

  XStringListToTextProperty ((char **) &title, 1, &text_property);

  XSetWMProperties (window_x11->device, window_x11->internal_win_id,
      &text_property, &text_property, 0, 0, NULL, &wm_hints, NULL);

  XFree (text_property.value);

  return TRUE;
}

static void
gst_gl_window_x11_close (GstGLWindow * window)
{
  GstGLWindowX11 *window_x11 = GST_GL_WINDOW_X11 (window);

  if (window_x11->device) {
    if (window_x11->internal_win_id) {
      XUnmapWindow (window_x11->device, window_x11->internal_win_id);

      XDestroyWindow (window_x11->device, window_x11->internal_win_id);

      XFreeColormap (window_x11->device, window_x11->priv->internal_colormap);

      /* Ensure everything is sent immediatly */
      XSync (window_x11->device, FALSE);
    }
    XFree (window_x11->visual_info);

    GST_DEBUG ("display receiver closed");
  }

  window_x11->running = FALSE;

  GST_GL_WINDOW_CLASS (parent_class)->close (window);
}

/* called by the gl thread */
static void
gst_gl_window_x11_set_window_handle (GstGLWindow * window, guintptr id)
{
  GstGLWindowX11 *window_x11;
  gint x, y, width, height;

  window_x11 = GST_GL_WINDOW_X11 (window);

  window_x11->parent_win = (Window) id;

  if (window_x11->priv->render_rect.w > 0 &&
      window_x11->priv->render_rect.h > 0) {
    x = window_x11->priv->render_rect.x;
    y = window_x11->priv->render_rect.y;
    width = window_x11->priv->render_rect.w;
    height = window_x11->priv->render_rect.h;
  } else {
    x = y = 0;
    if (window_x11->parent_win) {
      XWindowAttributes attr;

      XGetWindowAttributes (window_x11->device, window_x11->parent_win, &attr);
      width = attr.width;
      height = attr.height;
    } else {
      width = window_x11->priv->preferred_width;
      height = window_x11->priv->preferred_height;
    }
  }

  XResizeWindow (window_x11->device, window_x11->internal_win_id,
      width, height);

  XReparentWindow (window_x11->device, window_x11->internal_win_id,
      window_x11->parent_win, x, y);

  XSync (window_x11->device, FALSE);
}

struct SetRenderRectangle
{
  GstGLWindowX11 *window_x11;
  GstVideoRectangle rect;
};

static void
_free_set_render_rectangle (struct SetRenderRectangle *render)
{
  if (render) {
    if (render->window_x11)
      gst_object_unref (render->window_x11);
    g_free (render);
  }
}

static void
_set_render_rectangle (gpointer data)
{
  struct SetRenderRectangle *render = data;

  GST_LOG_OBJECT (render->window_x11, "setting render rectangle %i,%i+%ix%i",
      render->rect.x, render->rect.y, render->rect.w, render->rect.h);

  if (render->window_x11->internal_win_id)
    XMoveResizeWindow (render->window_x11->device,
        render->window_x11->internal_win_id, render->rect.x, render->rect.y,
        render->rect.w, render->rect.h);

  if (render->window_x11->device)
    XSync (render->window_x11->device, FALSE);

  render->window_x11->priv->render_rect = render->rect;
}

static gboolean
gst_gl_window_x11_set_render_rectangle (GstGLWindow * window,
    int x, int y, int width, int height)
{
  GstGLWindowX11 *window_x11 = GST_GL_WINDOW_X11 (window);
  struct SetRenderRectangle *render;

  render = g_new0 (struct SetRenderRectangle, 1);
  render->window_x11 = gst_object_ref (window_x11);
  render->rect.x = x;
  render->rect.y = y;
  render->rect.w = width;
  render->rect.h = height;

  gst_gl_window_send_message_async (window,
      (GstGLWindowCB) _set_render_rectangle, render,
      (GDestroyNotify) _free_set_render_rectangle);

  return TRUE;
}

static guintptr
gst_gl_window_x11_get_window_handle (GstGLWindow * window)
{
  GstGLWindowX11 *window_x11;

  window_x11 = GST_GL_WINDOW_X11 (window);

  return window_x11->internal_win_id;
}

static void
gst_gl_window_x11_set_preferred_size (GstGLWindow * window, gint width,
    gint height)
{
  GstGLWindowX11 *window_x11 = GST_GL_WINDOW_X11 (window);

  window_x11->priv->preferred_width = width;
  window_x11->priv->preferred_height = height;
}

static void
_show_window (GstGLWindow * window)
{
  GstGLWindowX11 *window_x11 = GST_GL_WINDOW_X11 (window);
  guint width = window_x11->priv->preferred_width;
  guint height = window_x11->priv->preferred_height;

  if (!window_x11->visible) {
    if (!window_x11->parent_win) {
      XResizeWindow (window_x11->device, window_x11->internal_win_id,
          width, height);

      gst_gl_window_resize (window, width, height);
    }

    XMapWindow (window_x11->device, window_x11->internal_win_id);
    XSync (window_x11->device, FALSE);
    window_x11->visible = TRUE;
  }
}

static void
gst_gl_window_x11_show (GstGLWindow * window)
{
  gst_gl_window_send_message (window, (GstGLWindowCB) _show_window, window);
}

static void
_context_draw (GstGLContext * context, GstGLWindow * window)
{
  window->draw (window->draw_data);
  gst_gl_context_swap_buffers (context);

  gst_object_unref (context);
}

static void
draw_cb (gpointer data)
{
  GstGLWindowX11 *window_x11 = data;
  GstGLWindow *window = GST_GL_WINDOW (window_x11);
  guint width, height;
  XWindowAttributes attr;

  if (window_x11->internal_win_id) {
    gboolean need_resize = FALSE;

    XGetWindowAttributes (window_x11->device, window_x11->internal_win_id,
        &attr);
    GST_TRACE_OBJECT (window, "window size %ux%u", attr.width, attr.height);

    if (window_x11->parent_win &&
        (window_x11->priv->render_rect.w <= 0 ||
            window_x11->priv->render_rect.h <= 0)) {
      XWindowAttributes attr_parent;
      XGetWindowAttributes (window_x11->device, window_x11->parent_win,
          &attr_parent);
      GST_TRACE_OBJECT (window, "parent window size %ux%u", attr_parent.width,
          attr_parent.height);

      if (attr.width != attr_parent.width || attr.height != attr_parent.height) {
        XMoveResizeWindow (window_x11->device, window_x11->internal_win_id,
            0, 0, attr_parent.width, attr_parent.height);
        XSync (window_x11->device, FALSE);

        attr.width = attr_parent.width;
        attr.height = attr_parent.height;

        GST_LOG ("parent resize:  %d, %d",
            attr_parent.width, attr_parent.height);
        need_resize = TRUE;
      }
    }

    gst_gl_window_get_surface_dimensions (window, &width, &height);
    if (attr.width != width || attr.height != height)
      need_resize = TRUE;

    if (need_resize)
      gst_gl_window_queue_resize (window);

    if (window_x11->allow_extra_expose_events) {
      if (window->queue_resize)
        gst_gl_window_resize (window, width, height);

      if (window->draw) {
        GstGLContext *context = gst_gl_window_get_context (window);

        _context_draw (context, window);
      }
    }
  }
}

/* Not called by the gl thread */
static void
gst_gl_window_x11_draw (GstGLWindow * window)
{
  gst_gl_window_send_message (window, (GstGLWindowCB) draw_cb, window);
}

static void
gst_gl_window_x11_handle_events (GstGLWindow * window, gboolean handle_events)
{
  GstGLWindowX11 *window_x11;

  g_return_if_fail (window != NULL);

  window_x11 = GST_GL_WINDOW_X11 (window);

  window_x11->priv->handle_events = handle_events;

  if (window_x11->internal_win_id) {
    if (handle_events) {
      XSelectInput (window_x11->device, window_x11->internal_win_id,
          StructureNotifyMask | ExposureMask | VisibilityChangeMask |
          PointerMotionMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
          ButtonReleaseMask);
    } else {
      XSelectInput (window_x11->device, window_x11->internal_win_id,
          StructureNotifyMask | ExposureMask | VisibilityChangeMask);
    }
  }
}

gboolean
gst_gl_window_x11_handle_event (GstGLWindowX11 * window_x11,
    xcb_generic_event_t * event)
{
  GstGLWindow *window = GST_GL_WINDOW (window_x11);
  GstGLDisplayX11 *display_x11 = GST_GL_DISPLAY_X11 (window->display);
  xcb_connection_t *connection = display_x11->xcb_connection;
  uint8_t event_code = event->response_type & 0x7f;

  switch (event_code) {
    case XCB_CLIENT_MESSAGE:{
      xcb_client_message_event_t *client_event;
      xcb_intern_atom_cookie_t cookie;
      xcb_intern_atom_reply_t *reply;

      client_event = (xcb_client_message_event_t *) event;
      cookie = xcb_intern_atom (connection, 0, 16, "WM_DELETE_WINDOW");
      reply = xcb_intern_atom_reply (connection, cookie, 0);

      if (client_event->data.data32[0] == reply->atom) {
        GST_INFO_OBJECT (window_x11, "Close requested");

        if (window->close)
          window->close (window->close_data);

        gst_gl_display_remove_window (GST_GL_DISPLAY (display_x11),
            GST_GL_WINDOW (window_x11));
      }

      g_free (reply);
      break;
    }
    case XCB_CONFIGURE_NOTIFY:{
      xcb_configure_notify_event_t *configure_event;

      configure_event = (xcb_configure_notify_event_t *) event;

      gst_gl_window_resize (window, configure_event->width,
          configure_event->height);

      gst_gl_window_draw (window);
      break;
    }
    case XCB_EXPOSE:{
      xcb_expose_event_t *expose_event = (xcb_expose_event_t *) event;
      /* non-zero means that other Expose follows
       * so just wait for the last one
       * in theory we should not receive non-zero because
       * we have no sub areas here but just in case */
      if (expose_event->count != 0)
        break;

      gst_gl_window_draw (window);
      break;
    }
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:{
      xcb_key_press_event_t *kp = (xcb_key_press_event_t *) event;
      const gchar *event_type_str;
      gchar *key_str;
      KeySym keysym;

      keysym = XkbKeycodeToKeysym (window_x11->device, kp->detail, 0, 0);
      key_str = XKeysymToString (keysym);

      if (event_code == XCB_KEY_PRESS)
        event_type_str = "key-press";
      else
        event_type_str = "key-release";

      gst_gl_window_send_key_event (window, event_type_str, key_str);
      break;
    }
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:{
      xcb_button_press_event_t *bp = (xcb_button_press_event_t *) event;
      const gchar *event_type_str;

      if (event_code == XCB_BUTTON_PRESS)
        event_type_str = "mouse-button-press";
      else
        event_type_str = "mouse-button-release";

      gst_gl_window_send_mouse_event (window, event_type_str, bp->detail,
          (double) bp->event_x, (double) bp->event_y);
      break;
    }
    case XCB_MOTION_NOTIFY:{
      xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *) event;

      gst_gl_window_send_mouse_event (window, "mouse-move", 0,
          (double) motion->event_x, (double) motion->event_y);
      break;
    }
    default:
      GST_TRACE ("unhandled XCB event: %u", event_code);
      break;
  }

  return TRUE;
}

static int
error_handler (Display * xdpy, XErrorEvent * error)
{
  TrappedErrorCode = error->error_code;
  return 0;
}

/**
 * gst_gl_window_x11_trap_x_errors:
 *
 * Traps every X error until gst_gl_window_x11_untrap_x_errors() is called.
 */
void
gst_gl_window_x11_trap_x_errors (void)
{
  TrappedErrorCode = 0;
  old_error_handler = XSetErrorHandler (error_handler);
}

/**
 * gst_gl_window_x11_untrap_x_errors:
 *
 * Removes the X error trap and returns the current status.
 *
 * Return value: the trapped error code, or 0 for success
 */
gint
gst_gl_window_x11_untrap_x_errors (void)
{
  XSetErrorHandler (old_error_handler);

  return TrappedErrorCode;
}

static guintptr
gst_gl_window_x11_get_display (GstGLWindow * window)
{
  GstGLWindowX11 *window_x11 = GST_GL_WINDOW_X11 (window);

  return (guintptr) window_x11->device;
}
