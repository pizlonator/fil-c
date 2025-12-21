/*
 * GStreamer
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

/**
 * SECTION:gstglwindow
 * @short_description: window/surface abstraction
 * @title: GstGLWindow
 * @see_also: #GstGLContext, #GstGLDisplay
 *
 * GstGLWindow represents a window that elements can render into.  A window can
 * either be a user visible window (onscreen) or hidden (offscreen).
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gmodule.h>
#include <stdio.h>

#include "gl.h"
#include "gstglwindow.h"
#include "gstglwindow_private.h"

/* FIXME make this work with windowless contexts */

#if GST_GL_HAVE_WINDOW_X11
#include "x11/gstglwindow_x11.h"
#endif
#if GST_GL_HAVE_WINDOW_WIN32
#include "win32/gstglwindow_win32.h"
#endif
#if GST_GL_HAVE_WINDOW_COCOA
#include "cocoa/gstglwindow_cocoa.h"
#endif
#if GST_GL_HAVE_WINDOW_WAYLAND
#include "wayland/gstglwindow_wayland_egl.h"
#endif
#if GST_GL_HAVE_WINDOW_ANDROID
#include "android/gstglwindow_android_egl.h"
#endif
#if GST_GL_HAVE_WINDOW_EAGL
#include "eagl/gstglwindow_eagl.h"
#endif
#if GST_GL_HAVE_WINDOW_VIV_FB
#include "viv-fb/gstglwindow_viv_fb_egl.h"
#endif
#if GST_GL_HAVE_WINDOW_GBM
#include "gbm/gstglwindow_gbm_egl.h"
#endif
#if GST_GL_HAVE_WINDOW_DISPMANX
#include "dispmanx/gstglwindow_dispmanx_egl.h"
#endif
#if GST_GL_HAVE_WINDOW_WINRT
#include "winrt/gstglwindow_winrt_egl.h"
#endif

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))
#define USING_OPENGL3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 1))
#define USING_GLES(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES, 1, 0))
#define USING_GLES2(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0))
#define USING_GLES3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0))

#define GST_CAT_DEFAULT gst_gl_window_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

static void gst_gl_window_default_draw (GstGLWindow * window);
static void gst_gl_window_default_run (GstGLWindow * window);
static void gst_gl_window_default_quit (GstGLWindow * window);
static void gst_gl_window_default_send_message (GstGLWindow * window,
    GstGLWindowCB callback, gpointer data);
static void gst_gl_window_default_send_message_async (GstGLWindow * window,
    GstGLWindowCB callback, gpointer data, GDestroyNotify destroy);
static gboolean gst_gl_window_default_has_output_surface (GstGLWindow * window);

struct _GstGLWindowPrivate
{
  GMainLoop *loop;

  guint surface_width;
  guint surface_height;

  GMutex sync_message_lock;
  GCond sync_message_cond;
};

#define gst_gl_window_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GstGLWindow, gst_gl_window,
    GST_TYPE_OBJECT);

static void gst_gl_window_finalize (GObject * object);

typedef struct _GstGLDummyWindow
{
  GstGLWindow parent;

  guintptr handle;
} GstGLDummyWindow;

typedef struct _GstGLDummyWindowCass
{
  GstGLWindowClass parent;
} GstGLDummyWindowClass;

static GstGLDummyWindow *gst_gl_dummy_window_new (void);

enum
{
  SIGNAL_0,
  EVENT_MOUSE_SIGNAL,
  EVENT_KEY_SIGNAL,
  EVENT_SCROLL_SIGNAL,
  WINDOW_HANDLE_CHANGED_SIGNAL,
  LAST_SIGNAL
};

static guint gst_gl_window_signals[LAST_SIGNAL] = { 0 };

/**
 * gst_gl_window_error_quark:
 *
 * Returns: the quark used for #GstGLWindow in #GError's
 */
GQuark
gst_gl_window_error_quark (void)
{
  return g_quark_from_static_string ("gst-gl-window-error-quark");
}

static gboolean
gst_gl_window_default_open (GstGLWindow * window, GError ** error)
{
  g_main_context_push_thread_default (window->main_context);

  return TRUE;
}

static void
gst_gl_window_default_close (GstGLWindow * window)
{
  g_main_context_pop_thread_default (window->main_context);
}

static void
_init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (gst_gl_window_debug, "glwindow", 0,
        "glwindow element");
    g_once_init_leave (&_init, 1);
  }
}

static void
gst_gl_window_init (GstGLWindow * window)
{
  GstGLWindowPrivate *priv = gst_gl_window_get_instance_private (window);
  window->priv = priv;

  g_mutex_init (&window->lock);
  window->is_drawing = FALSE;

  g_weak_ref_init (&window->context_ref, NULL);

  g_mutex_init (&window->priv->sync_message_lock);
  g_cond_init (&window->priv->sync_message_cond);

  window->main_context = g_main_context_new ();
  priv->loop = g_main_loop_new (window->main_context, FALSE);
}

static void
gst_gl_window_class_init (GstGLWindowClass * klass)
{
  klass->open = GST_DEBUG_FUNCPTR (gst_gl_window_default_open);
  klass->close = GST_DEBUG_FUNCPTR (gst_gl_window_default_close);
  klass->run = GST_DEBUG_FUNCPTR (gst_gl_window_default_run);
  klass->quit = GST_DEBUG_FUNCPTR (gst_gl_window_default_quit);
  klass->draw = GST_DEBUG_FUNCPTR (gst_gl_window_default_draw);
  klass->send_message = GST_DEBUG_FUNCPTR (gst_gl_window_default_send_message);
  klass->send_message_async =
      GST_DEBUG_FUNCPTR (gst_gl_window_default_send_message_async);
  klass->has_output_surface =
      GST_DEBUG_FUNCPTR (gst_gl_window_default_has_output_surface);

  G_OBJECT_CLASS (klass)->finalize = gst_gl_window_finalize;

  /**
   * GstGLWindow::mouse-event:
   * @object: the #GstGLWindow
   * @id: the name of the event
   * @button: the id of the button
   * @x: the x coordinate of the mouse event
   * @y: the y coordinate of the mouse event
   *
   * Will be emitted when a mouse event is received by the GstGLwindow.
   *
   * Since: 1.6
   */
  gst_gl_window_signals[EVENT_MOUSE_SIGNAL] =
      g_signal_new ("mouse-event", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 4, G_TYPE_STRING,
      G_TYPE_INT, G_TYPE_DOUBLE, G_TYPE_DOUBLE);

  /**
   * GstGLWindow::key-event:
   * @object: the #GstGLWindow
   * @id: the name of the event
   * @key: the id of the key pressed
   *
   * Will be emitted when a key event is received by the GstGLwindow.
   *
   * Since: 1.6
   */
  gst_gl_window_signals[EVENT_KEY_SIGNAL] =
      g_signal_new ("key-event", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_STRING,
      G_TYPE_STRING);

  /**
   * GstGLWindow::scroll-event:
   * @object: the #GstGLWindow
   * @x: the x coordinate of the mouse event
   * @y: the y coordinate of the mouse event
   * @delta_x: the x offset of the scroll event
   * @delta_y: the y offset of the scroll event
   *
   * Will be emitted when a mouse scroll event is received by the GstGLwindow.
   *
   * Since: 1.18
   */
  gst_gl_window_signals[EVENT_SCROLL_SIGNAL] =
      g_signal_new ("scroll-event", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_generic,
      G_TYPE_NONE, 4, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE,
      G_TYPE_DOUBLE);

  /**
   * GstGLWindow::window-handle-changed:
   * @object: the #GstGLWindow
   *
   * Will be emitted when the window handle has been set into the native
   * implementation, but before the context is re-activated. By using this
   * signal, elements can refresh associated resource without relying on
   * direct handle comparision.
   *
   * Since: 1.20
   */
  gst_gl_window_signals[WINDOW_HANDLE_CHANGED_SIGNAL] =
      g_signal_new ("window-handle_changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_generic,
      G_TYPE_NONE, 0);

  _init_debug ();
}

/**
 * gst_gl_window_new:
 * @display: a #GstGLDisplay
 *
 * Returns: (transfer full): a new #GstGLWindow using @display's connection
 *
 * Since: 1.4
 */
GstGLWindow *
gst_gl_window_new (GstGLDisplay * display)
{
  GstGLWindow *window = NULL;
  const gchar *user_choice;

  g_return_val_if_fail (display != NULL, NULL);

  _init_debug ();

  user_choice = g_getenv ("GST_GL_WINDOW");
  GST_INFO ("creating a window, user choice:%s", user_choice);

#if GST_GL_HAVE_WINDOW_COCOA
  if (!window && (!user_choice || g_strstr_len (user_choice, 5, "cocoa")))
    window = GST_GL_WINDOW (gst_gl_window_cocoa_new (display));
#endif
#if GST_GL_HAVE_WINDOW_X11
  if (!window && (!user_choice || g_strstr_len (user_choice, 3, "x11")))
    window = GST_GL_WINDOW (gst_gl_window_x11_new (display));
#endif
#if GST_GL_HAVE_WINDOW_WIN32
  if (!window && (!user_choice || g_strstr_len (user_choice, 5, "win32")))
    window = GST_GL_WINDOW (gst_gl_window_win32_new (display));
#endif
#if GST_GL_HAVE_WINDOW_WAYLAND
  if (!window && (!user_choice || g_strstr_len (user_choice, 7, "wayland")))
    window = GST_GL_WINDOW (gst_gl_window_wayland_egl_new (display));
#endif
#if GST_GL_HAVE_WINDOW_DISPMANX
  if (!window && (!user_choice || g_strstr_len (user_choice, 8, "dispmanx")))
    window = GST_GL_WINDOW (gst_gl_window_dispmanx_egl_new (display));
#endif
#if GST_GL_HAVE_WINDOW_ANDROID
  if (!window && (!user_choice || g_strstr_len (user_choice, 7, "android")))
    window = GST_GL_WINDOW (gst_gl_window_android_egl_new (display));
#endif
#if GST_GL_HAVE_WINDOW_EAGL
  if (!window && (!user_choice || g_strstr_len (user_choice, 4, "eagl")))
    window = GST_GL_WINDOW (gst_gl_window_eagl_new (display));
#endif
#if GST_GL_HAVE_WINDOW_VIV_FB
  if (!window && (!user_choice || g_strstr_len (user_choice, 6, "viv-fb")))
    window = GST_GL_WINDOW (gst_gl_window_viv_fb_egl_new (display));
#endif
#if GST_GL_HAVE_WINDOW_GBM
  if (!window && (!user_choice || g_strstr_len (user_choice, 3, "gbm")))
    window = GST_GL_WINDOW (gst_gl_window_gbm_egl_new (display));
#endif
#if GST_GL_HAVE_WINDOW_WINRT
  if (!window && (!user_choice || g_strstr_len (user_choice, 5, "winrt")))
    window = GST_GL_WINDOW (gst_gl_window_winrt_egl_new (display));
#endif
  if (!window && (!user_choice
          || g_strstr_len (user_choice, 11, "surfaceless")))
    window = GST_GL_WINDOW (gst_gl_dummy_window_new ());

  if (!window) {
    /* subclass returned a NULL window */
    GST_WARNING ("Could not create window. user specified %s, creating dummy"
        " window", user_choice ? user_choice : "(null)");

    window = GST_GL_WINDOW (gst_gl_dummy_window_new ());
  }

  window->display = gst_object_ref (display);

  return window;
}

static void
gst_gl_window_finalize (GObject * object)
{
  GstGLWindow *window = GST_GL_WINDOW (object);
  GstGLWindowPrivate *priv = window->priv;

  gst_gl_display_remove_window (window->display, window);

  if (priv->loop)
    g_main_loop_unref (priv->loop);

  if (window->main_context)
    g_main_context_unref (window->main_context);
  window->main_context = NULL;

  g_weak_ref_clear (&window->context_ref);

  g_mutex_clear (&window->lock);
  g_mutex_clear (&window->priv->sync_message_lock);
  g_cond_clear (&window->priv->sync_message_cond);
  gst_object_unref (window->display);

  G_OBJECT_CLASS (gst_gl_window_parent_class)->finalize (object);
}

typedef struct _GstSetWindowHandleCb
{
  GstGLWindow *window;
  guintptr handle;
} GstSetWindowHandleCb;

static void
_set_window_handle_cb (GstSetWindowHandleCb * data)
{
  GstGLContext *context = gst_gl_window_get_context (data->window);
  GstGLWindowClass *window_class = GST_GL_WINDOW_GET_CLASS (data->window);
  GThread *thread = NULL;

  /* deactivate if necessary */
  if (context) {
    thread = gst_gl_context_get_thread (context);
    if (thread) {
      /* This is only thread safe iff the context thread == g_thread_self() */
      g_assert (thread == g_thread_self ());
      gst_gl_context_activate (context, FALSE);
    }
  }

  window_class->set_window_handle (data->window, data->handle);

  /* Let's assume users don't call this without a new handle, this is the
   * safest since we don't know the nature of the handle and direct
   * comparision might not be safe */
  g_signal_emit (data->window,
      gst_gl_window_signals[WINDOW_HANDLE_CHANGED_SIGNAL], 0, NULL);

  /* reactivate */
  if (context && thread)
    gst_gl_context_activate (context, TRUE);

  if (context)
    gst_object_unref (context);
  if (thread)
    g_thread_unref (thread);
}

static void
_free_swh_cb (GstSetWindowHandleCb * data)
{
  gst_object_unref (data->window);
  g_free (data);
}

/**
 * gst_gl_window_set_window_handle:
 * @window: a #GstGLWindow
 * @handle: handle to the window
 *
 * Sets the window that this @window should render into.  Some implementations
 * require this to be called with a valid handle before drawing can commence.
 *
 * Since: 1.4
 */
void
gst_gl_window_set_window_handle (GstGLWindow * window, guintptr handle)
{
  GstGLWindowClass *window_class;
  GstSetWindowHandleCb *data;

  g_return_if_fail (GST_IS_GL_WINDOW (window));
  g_return_if_fail (handle != 0);
  window_class = GST_GL_WINDOW_GET_CLASS (window);
  g_return_if_fail (window_class->set_window_handle != NULL);

  data = g_new (GstSetWindowHandleCb, 1);
  data->window = gst_object_ref (window);
  data->handle = handle;

  /* FIXME: Move to a message which deactivates, calls implementation, activates */
  gst_gl_window_send_message_async (window,
      (GstGLWindowCB) _set_window_handle_cb, data,
      (GDestroyNotify) _free_swh_cb);

  /* window_class->set_window_handle (window, handle); */
}

static void
draw_cb (gpointer data)
{
  GstGLWindow *window = GST_GL_WINDOW (data);
  GstGLContext *context = gst_gl_window_get_context (window);

  if (window->queue_resize) {
    guint width, height;

    gst_gl_window_get_surface_dimensions (window, &width, &height);
    gst_gl_window_resize (window, width, height);
  }

  if (window->draw)
    window->draw (window->draw_data);

  gst_gl_context_swap_buffers (context);

  gst_object_unref (context);
}

static void
gst_gl_window_default_draw (GstGLWindow * window)
{
  gst_gl_window_send_message (window, (GstGLWindowCB) draw_cb, window);
}

/**
 * gst_gl_window_draw:
 * @window: a #GstGLWindow
 *
 * Redraw the window contents.  Implementations should invoke the draw callback.
 *
 * Since: 1.4
 */
void
gst_gl_window_draw (GstGLWindow * window)
{
  GstGLWindowClass *window_class;

  g_return_if_fail (GST_IS_GL_WINDOW (window));
  window_class = GST_GL_WINDOW_GET_CLASS (window);
  g_return_if_fail (window_class->draw != NULL);

  /* avoid to overload the drawer */
  if (window->is_drawing) {
    return;
  }

  window_class->draw (window);
}

/**
 * gst_gl_window_set_preferred_size:
 * @window: a #GstGLWindow
 * @width: new preferred width
 * @height: new preferred height
 *
 * Set the preferred width and height of the window.  Implementations are free
 * to ignore this information.
 *
 * Since: 1.6
 */
void
gst_gl_window_set_preferred_size (GstGLWindow * window, gint width, gint height)
{
  GstGLWindowClass *window_class;

  g_return_if_fail (GST_IS_GL_WINDOW (window));
  window_class = GST_GL_WINDOW_GET_CLASS (window);

  if (window_class->set_preferred_size)
    window_class->set_preferred_size (window, width, height);
}

/**
 * gst_gl_window_show:
 * @window: a #GstGLWindow
 *
 * Present the window to the screen.
 *
 * Since: 1.6
 */
void
gst_gl_window_show (GstGLWindow * window)
{
  GstGLWindowClass *window_class;

  g_return_if_fail (GST_IS_GL_WINDOW (window));
  window_class = GST_GL_WINDOW_GET_CLASS (window);

  if (window_class->show)
    window_class->show (window);
}

static void
gst_gl_window_default_run (GstGLWindow * window)
{
  GstGLWindowPrivate *priv = window->priv;

  g_main_loop_run (priv->loop);
}

/**
 * gst_gl_window_run:
 * @window: a #GstGLWindow
 *
 * Start the execution of the runloop.
 *
 * Since: 1.4
 */
void
gst_gl_window_run (GstGLWindow * window)
{
  GstGLWindowClass *window_class;

  g_return_if_fail (GST_IS_GL_WINDOW (window));
  window_class = GST_GL_WINDOW_GET_CLASS (window);
  g_return_if_fail (window_class->run != NULL);

  window_class->run (window);
}

static void
window_default_quit_func (GstGLWindow * window)
{
  gst_gl_display_remove_window (window->display, window);
  g_main_loop_quit (window->priv->loop);
}

static void
gst_gl_window_default_quit (GstGLWindow * window)
{
  gst_gl_window_send_message_async (window,
      (GstGLWindowCB) window_default_quit_func, gst_object_ref (window),
      gst_object_unref);
}

/**
 * gst_gl_window_quit:
 * @window: a #GstGLWindow
 *
 * Quit the runloop's execution.
 *
 * Since: 1.4
 */
void
gst_gl_window_quit (GstGLWindow * window)
{
  GstGLWindowClass *window_class;

  g_return_if_fail (GST_IS_GL_WINDOW (window));
  window_class = GST_GL_WINDOW_GET_CLASS (window);
  g_return_if_fail (window_class->quit != NULL);

  GST_GL_WINDOW_LOCK (window);

  window_class->quit (window);

  GST_INFO ("quit sent to gl window loop");

  GST_GL_WINDOW_UNLOCK (window);
}

typedef struct _GstGLSyncMessage
{
  GstGLWindow *window;
  gboolean fired;

  GstGLWindowCB callback;
  gpointer data;
} GstGLSyncMessage;

static void
_run_message_sync (GstGLSyncMessage * message)
{

  if (message->callback)
    message->callback (message->data);

  g_mutex_lock (&message->window->priv->sync_message_lock);
  message->fired = TRUE;
  g_cond_broadcast (&message->window->priv->sync_message_cond);
  g_mutex_unlock (&message->window->priv->sync_message_lock);
}

void
gst_gl_window_default_send_message (GstGLWindow * window,
    GstGLWindowCB callback, gpointer data)
{
  GstGLSyncMessage message;

  message.window = window;
  message.callback = callback;
  message.data = data;
  message.fired = FALSE;

  gst_gl_window_send_message_async (window, (GstGLWindowCB) _run_message_sync,
      &message, NULL);

  g_mutex_lock (&window->priv->sync_message_lock);

  /* block until opengl calls have been executed in the gl thread */
  while (!message.fired)
    g_cond_wait (&window->priv->sync_message_cond,
        &window->priv->sync_message_lock);
  g_mutex_unlock (&window->priv->sync_message_lock);
}

/**
 * gst_gl_window_send_message:
 * @window: a #GstGLWindow
 * @callback: (scope async): function to invoke
 * @data: (closure): data to invoke @callback with
 *
 * Invoke @callback with data on the window thread.  @callback is guaranteed to
 * have executed when this function returns.
 *
 * Since: 1.4
 */
void
gst_gl_window_send_message (GstGLWindow * window, GstGLWindowCB callback,
    gpointer data)
{
  GstGLWindowClass *window_class;

  g_return_if_fail (GST_IS_GL_WINDOW (window));
  g_return_if_fail (callback != NULL);
  window_class = GST_GL_WINDOW_GET_CLASS (window);
  g_return_if_fail (window_class->send_message != NULL);

  window_class->send_message (window, callback, data);
}

typedef struct _GstGLAsyncMessage
{
  GstGLWindowCB callback;
  gpointer data;
  GDestroyNotify destroy;
} GstGLAsyncMessage;

static gboolean
_run_message_async (GstGLAsyncMessage * message)
{
  if (message->callback)
    message->callback (message->data);

  if (message->destroy)
    message->destroy (message->data);

  g_free (message);

  return FALSE;
}

static void
gst_gl_window_default_send_message_async (GstGLWindow * window,
    GstGLWindowCB callback, gpointer data, GDestroyNotify destroy)
{
  GstGLAsyncMessage *message = g_new (GstGLAsyncMessage, 1);

  message->callback = callback;
  message->data = data;
  message->destroy = destroy;

  g_main_context_invoke (window->main_context, (GSourceFunc) _run_message_async,
      message);
}

static gboolean
gst_gl_window_default_has_output_surface (GstGLWindow * window)
{
  return TRUE;
}

/**
 * gst_gl_window_has_output_surface:
 * @window: a #GstGLWindow
 *
 * Query whether @window has output surface or not
 *
 * Returns: %TRUE if @window has useable output surface
 *
 * Since: 1.18
 */
gboolean
gst_gl_window_has_output_surface (GstGLWindow * window)
{
  GstGLWindowClass *window_class;

  g_return_val_if_fail (GST_IS_GL_WINDOW (window), FALSE);

  window_class = GST_GL_WINDOW_GET_CLASS (window);

  g_assert (window_class->has_output_surface);

  return window_class->has_output_surface (window);
}

/**
 * gst_gl_window_send_message_async:
 * @window: a #GstGLWindow
 * @callback: (scope async): function to invoke
 * @data: (closure): data to invoke @callback with
 * @destroy: called when @data is not needed anymore
 *
 * Invoke @callback with @data on the window thread.  The callback may not
 * have been executed when this function returns.
 *
 * Since: 1.4
 */
void
gst_gl_window_send_message_async (GstGLWindow * window, GstGLWindowCB callback,
    gpointer data, GDestroyNotify destroy)
{
  GstGLWindowClass *window_class;

  g_return_if_fail (GST_IS_GL_WINDOW (window));
  g_return_if_fail (callback != NULL);
  window_class = GST_GL_WINDOW_GET_CLASS (window);
  g_return_if_fail (window_class->send_message_async != NULL);

  window_class->send_message_async (window, callback, data, destroy);
}

/**
 * gst_gl_window_set_draw_callback:
 * @window: a #GstGLWindow
 * @callback: (scope notified): function to invoke
 * @data: (closure): data to invoke @callback with
 * @destroy_notify: called when @data is not needed any more
 *
 * Sets the draw callback called every time gst_gl_window_draw() is called
 *
 * Since: 1.4
 */
void
gst_gl_window_set_draw_callback (GstGLWindow * window, GstGLWindowCB callback,
    gpointer data, GDestroyNotify destroy_notify)
{
  g_return_if_fail (GST_IS_GL_WINDOW (window));

  GST_GL_WINDOW_LOCK (window);

  if (window->draw_notify)
    window->draw_notify (window->draw_data);

  window->draw = callback;
  window->draw_data = data;
  window->draw_notify = destroy_notify;

  GST_GL_WINDOW_UNLOCK (window);
}

/**
 * gst_gl_window_set_resize_callback:
 * @window: a #GstGLWindow
 * @callback: (scope notified): function to invoke
 * @data: (closure): data to invoke @callback with
 * @destroy_notify: called when @data is not needed any more
 *
 * Sets the resize callback called every time a resize of the window occurs.
 *
 * Since: 1.4
 */
void
gst_gl_window_set_resize_callback (GstGLWindow * window,
    GstGLWindowResizeCB callback, gpointer data, GDestroyNotify destroy_notify)
{
  g_return_if_fail (GST_IS_GL_WINDOW (window));

  GST_GL_WINDOW_LOCK (window);

  if (window->resize_notify)
    window->resize_notify (window->resize_data);

  window->resize = callback;
  window->resize_data = data;
  window->resize_notify = destroy_notify;

  GST_GL_WINDOW_UNLOCK (window);
}

/**
 * gst_gl_window_set_close_callback:
 * @window: a #GstGLWindow
 * @callback: (scope notified): function to invoke
 * @data: (closure): data to invoke @callback with
 * @destroy_notify: called when @data is not needed any more
 *
 * Sets the callback called when the window is about to close.
 *
 * Since: 1.4
 */
void
gst_gl_window_set_close_callback (GstGLWindow * window, GstGLWindowCB callback,
    gpointer data, GDestroyNotify destroy_notify)
{
  g_return_if_fail (GST_IS_GL_WINDOW (window));

  GST_GL_WINDOW_LOCK (window);

  if (window->close_notify)
    window->close_notify (window->close_data);

  window->close = callback;
  window->close_data = data;
  window->close_notify = destroy_notify;

  GST_GL_WINDOW_UNLOCK (window);
}

/**
 * gst_gl_window_get_display:
 * @window: a #GstGLWindow
 *
 * Returns: the windowing system display handle for this @window
 *
 * Since: 1.4
 */
guintptr
gst_gl_window_get_display (GstGLWindow * window)
{
  GstGLWindowClass *window_class;

  g_return_val_if_fail (GST_IS_GL_WINDOW (window), 0);
  window_class = GST_GL_WINDOW_GET_CLASS (window);
  g_return_val_if_fail (window_class->get_display != NULL, 0);

  return window_class->get_display (window);
}

/**
 * gst_gl_window_get_window_handle:
 * @window: a #GstGLWindow
 *
 * Returns: the window handle we are currently rendering into
 *
 * Since: 1.4
 */
guintptr
gst_gl_window_get_window_handle (GstGLWindow * window)
{
  GstGLWindowClass *window_class;

  g_return_val_if_fail (GST_IS_GL_WINDOW (window), 0);
  window_class = GST_GL_WINDOW_GET_CLASS (window);
  g_return_val_if_fail (window_class->get_window_handle != NULL, 0);

  return window_class->get_window_handle (window);
}

/**
 * gst_gl_window_get_context:
 * @window: a #GstGLWindow
 *
 * Returns: (transfer full): the #GstGLContext associated with this @window
 *
 * Since: 1.4
 */
GstGLContext *
gst_gl_window_get_context (GstGLWindow * window)
{
  g_return_val_if_fail (GST_IS_GL_WINDOW (window), NULL);

  return (GstGLContext *) g_weak_ref_get (&window->context_ref);
}

/**
 * gst_gl_window_get_surface_dimensions:
 * @window: a #GstGLWindow
 * @width: (out): resulting surface width
 * @height: (out): resulting surface height
 *
 * Since: 1.6
 */
void
gst_gl_window_get_surface_dimensions (GstGLWindow * window, guint * width,
    guint * height)
{
  GST_GL_WINDOW_LOCK (window);
  if (width)
    *width = window->priv->surface_width;
  if (height)
    *height = window->priv->surface_height;
  GST_GL_WINDOW_UNLOCK (window);
}

void
gst_gl_window_send_key_event (GstGLWindow * window, const char *event_type,
    const char *key_str)
{
  g_signal_emit (window, gst_gl_window_signals[EVENT_KEY_SIGNAL], 0,
      event_type, key_str);
}

void
gst_gl_window_send_mouse_event (GstGLWindow * window, const char *event_type,
    int button, double posx, double posy)
{
  g_signal_emit (window, gst_gl_window_signals[EVENT_MOUSE_SIGNAL], 0,
      event_type, button, posx, posy);
}

/**
 * gst_gl_window_send_scroll_event:
 * @window: a #GstGLWindow
 * @posx: x position of the mouse cursor
 * @posy: y position of the mouse cursor
 * @delta_x: the x offset of the scroll event
 * @delta_y: the y offset of the scroll event
 *
 * Notify a @window about a scroll event. A scroll signal holding the event
 * coordinates will be emitted.
 *
 * Since: 1.18
 */
void
gst_gl_window_send_scroll_event (GstGLWindow * window,
    double posx, double posy, double delta_x, double delta_y)
{
  g_signal_emit (window, gst_gl_window_signals[EVENT_SCROLL_SIGNAL], 0,
      posx, posy, delta_x, delta_y);
}


/**
 * gst_gl_window_handle_events:
 * @window: a #GstGLWindow
 * @handle_events: a #gboolean indicating if events should be handled or not.
 *
 * Tell a @window that it should handle events from the window system. These
 * events are forwarded upstream as navigation events. In some window systems
 * events are not propagated in the window hierarchy if a client is listening
 * for them. This method allows you to disable events handling completely
 * from the @window.
 */
void
gst_gl_window_handle_events (GstGLWindow * window, gboolean handle_events)
{
  GstGLWindowClass *window_class;

  g_return_if_fail (GST_IS_GL_WINDOW (window));
  window_class = GST_GL_WINDOW_GET_CLASS (window);

  if (window_class->handle_events)
    window_class->handle_events (window, handle_events);
}

/**
 * gst_gl_window_set_render_rectangle:
 * @window: a #GstGLWindow
 * @x: x position
 * @y: y position
 * @width: width
 * @height: height
 *
 * Tell a @window that it should render into a specific region of the window
 * according to the #GstVideoOverlay interface.
 *
 * Returns: whether the specified region could be set
 */
gboolean
gst_gl_window_set_render_rectangle (GstGLWindow * window, gint x, gint y,
    gint width, gint height)
{
  GstGLWindowClass *window_class;
  gboolean ret = FALSE;

  g_return_val_if_fail (GST_IS_GL_WINDOW (window), FALSE);
  window_class = GST_GL_WINDOW_GET_CLASS (window);

  GST_GL_WINDOW_LOCK (window);
  /* When x/y is smaller then reset the render rectangle */
  if (x < 0 || y < 0) {
    x = y = 0;
    width = window->priv->surface_width;
    height = window->priv->surface_height;
  }

  if (x < 0 || y < 0 || width <= 0 || height <= 0) {
    GST_GL_WINDOW_UNLOCK (window);
    return FALSE;
  }

  if (window_class->set_render_rectangle)
    ret = window_class->set_render_rectangle (window, x, y, width, height);

  GST_GL_WINDOW_UNLOCK (window);

  return ret;
}

/**
 * gst_gl_window_queue_resize:
 * @window: a #GstGLWindow
 *
 * Queue resizing of @window.
 */
void
gst_gl_window_queue_resize (GstGLWindow * window)
{
  GstGLWindowClass *window_class;

  g_return_if_fail (GST_IS_GL_WINDOW (window));
  window_class = GST_GL_WINDOW_GET_CLASS (window);

  window->queue_resize = TRUE;
  if (window_class->queue_resize)
    window_class->queue_resize (window);
}

struct resize_data
{
  GstGLWindow *window;
  guint width, height;
};

static void
_on_resize (gpointer data)
{
  struct resize_data *resize = data;

  GST_GL_WINDOW_LOCK (resize->window);

  if (resize->window->resize)
    resize->window->resize (resize->window->resize_data, resize->width,
        resize->height);

  resize->window->priv->surface_width = resize->width;
  resize->window->priv->surface_height = resize->height;

  GST_GL_WINDOW_UNLOCK (resize->window);

  resize->window->queue_resize = FALSE;
}

/**
 * gst_gl_window_resize:
 * @window: a #GstGLWindow
 * @width: new width
 * @height: new height
 *
 * Resize @window to the given @width and @height.
 */
void
gst_gl_window_resize (GstGLWindow * window, guint width, guint height)
{
  struct resize_data resize = { 0, };

  g_return_if_fail (GST_IS_GL_WINDOW (window));

  resize.window = window;
  resize.width = width;
  resize.height = height;

  gst_gl_window_send_message (window, (GstGLWindowCB) _on_resize, &resize);
}

/**
 * gst_gl_window_controls_viewport:
 * @window: a #GstGLWindow
 *
 * Checks if @window controls the GL viewport.
 *
 * Returns: %TRUE if @window controls the GL viewport, otherwise %FALSE
 *
 * Since: 1.16
 */
gboolean
gst_gl_window_controls_viewport (GstGLWindow * window)
{
  GstGLWindowClass *window_class;

  g_return_val_if_fail (GST_IS_GL_WINDOW (window), FALSE);
  window_class = GST_GL_WINDOW_GET_CLASS (window);

  if (!window_class->controls_viewport)
    return FALSE;

  return window_class->controls_viewport (window);
}

static GType gst_gl_dummy_window_get_type (void);

static gboolean
gst_gl_dummy_window_has_output_surface (GstGLWindow * window)
{
  return FALSE;
}

G_DEFINE_TYPE (GstGLDummyWindow, gst_gl_dummy_window, GST_TYPE_GL_WINDOW);

static void
gst_gl_dummy_window_set_window_handle (GstGLWindow * window, guintptr handle)
{
  GstGLDummyWindow *dummy = (GstGLDummyWindow *) window;

  dummy->handle = handle;
}

static guintptr
gst_gl_dummy_window_get_window_handle (GstGLWindow * window)
{
  GstGLDummyWindow *dummy = (GstGLDummyWindow *) window;

  return (guintptr) dummy->handle;
}

static guintptr
gst_gl_dummy_window_get_display (GstGLWindow * window)
{
  return 0;
}

static void
gst_gl_dummy_window_class_init (GstGLDummyWindowClass * klass)
{
  GstGLWindowClass *window_class = (GstGLWindowClass *) klass;

  window_class->get_display =
      GST_DEBUG_FUNCPTR (gst_gl_dummy_window_get_display);
  window_class->get_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_dummy_window_get_window_handle);
  window_class->set_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_dummy_window_set_window_handle);
  window_class->has_output_surface =
      GST_DEBUG_FUNCPTR (gst_gl_dummy_window_has_output_surface);
}

static void
gst_gl_dummy_window_init (GstGLDummyWindow * dummy)
{
  dummy->handle = 0;
}

static GstGLDummyWindow *
gst_gl_dummy_window_new (void)
{
  GstGLDummyWindow *window;

  window = g_object_new (gst_gl_dummy_window_get_type (), NULL);
  gst_object_ref_sink (window);

  return window;
}
