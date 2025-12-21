/*
 * GStreamer
 * Copyright (C) 2007 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
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
 * SECTION:gstgldisplay
 * @short_description: window system display connection abstraction
 * @title: GstGLDisplay
 * @see_also: #GstContext, #GstGLContext, #GstGLWindow
 *
 * #GstGLDisplay represents a connection to the underlying windowing system.
 * Elements are required to make use of #GstContext to share and propagate
 * a #GstGLDisplay.
 *
 * There are a number of environment variables that influence the choice of
 * platform and window system specific functionality.
 * - GST_GL_WINDOW influences the window system to use.  Common values are
 *   'x11', 'wayland', 'surfaceless', 'win32' or 'cocoa'.
 * - GST_GL_PLATFORM influences the OpenGL platform to use.  Common values are
 *   'egl', 'glx', 'wgl' or 'cgl'.
 * - GST_GL_API influences the OpenGL API requested by the OpenGL platform.
 *   Common values are 'opengl', 'opengl3' and 'gles2'.
 *
 * > Certain window systems require a special function to be called to
 * > initialize threading support.  As this GStreamer GL library does not preclude
 * > concurrent access to the windowing system, it is strongly advised that
 * > applications ensure that threading support has been initialized before any
 * > other toolkit/library functionality is accessed.  Failure to do so could
 * > result in sudden application abortion during execution.  The most notably
 * > example of such a function is X11's XInitThreads\().
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gl.h"
#include "gstgldisplay.h"

#if GST_GL_HAVE_WINDOW_COCOA
#include <gst/gl/cocoa/gstgldisplay_cocoa.h>
#endif
#if GST_GL_HAVE_WINDOW_X11
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif
#if GST_GL_HAVE_WINDOW_WAYLAND
#include <gst/gl/wayland/gstgldisplay_wayland.h>
#endif
#if GST_GL_HAVE_PLATFORM_EGL
#include <gst/gl/egl/gstgldisplay_egl.h>
#include <gst/gl/egl/gstgldisplay_egl_device.h>
#include <gst/gl/egl/gsteglimage.h>
#include <gst/gl/egl/gstglmemoryegl.h>
#endif
#if GST_GL_HAVE_WINDOW_VIV_FB
#include <gst/gl/viv-fb/gstgldisplay_viv_fb.h>
#endif
#if GST_GL_HAVE_WINDOW_GBM
#include <gst/gl/gbm/gstgldisplay_gbm.h>
#endif

GST_DEBUG_CATEGORY_STATIC (gst_context);
GST_DEBUG_CATEGORY_STATIC (gst_gl_display_debug);
#define GST_CAT_DEFAULT gst_gl_display_debug

enum
{
  SIGNAL_0,
  CREATE_CONTEXT,
  LAST_SIGNAL
};

static guint gst_gl_display_signals[LAST_SIGNAL] = { 0 };


static void gst_gl_display_dispose (GObject * object);
static void gst_gl_display_finalize (GObject * object);
static guintptr gst_gl_display_default_get_handle (GstGLDisplay * display);
static GstGLWindow *gst_gl_display_default_create_window (GstGLDisplay *
    display);

struct _GstGLDisplayPrivate
{
  GstGLAPI gl_api;

  GList *contexts;

  GThread *event_thread;

  GMutex thread_lock;
  GCond thread_cond;

  GMutex window_lock;
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_display_debug, "gldisplay", 0, "opengl display"); \
  GST_DEBUG_CATEGORY_GET (gst_context, "GST_CONTEXT");

G_DEFINE_TYPE_WITH_CODE (GstGLDisplay, gst_gl_display, GST_TYPE_OBJECT,
    G_ADD_PRIVATE (GstGLDisplay)
    DEBUG_INIT);

static gboolean
_unlock_main_thread (GstGLDisplay * display)
{
  g_mutex_unlock (&display->priv->thread_lock);

  return G_SOURCE_REMOVE;
}

static gpointer
_event_thread_main (GstGLDisplay * display)
{
  g_mutex_lock (&display->priv->thread_lock);

  display->main_context = g_main_context_new ();
  display->main_loop = g_main_loop_new (display->main_context, FALSE);

  g_main_context_invoke (display->main_context,
      (GSourceFunc) _unlock_main_thread, display);

  g_cond_broadcast (&display->priv->thread_cond);

  g_main_loop_run (display->main_loop);

  g_mutex_lock (&display->priv->thread_lock);
  g_main_loop_unref (display->main_loop);
  g_main_context_unref (display->main_context);

  display->main_loop = NULL;
  display->main_context = NULL;

  g_cond_broadcast (&display->priv->thread_cond);
  g_mutex_unlock (&display->priv->thread_lock);

  return NULL;
}

static void
gst_gl_display_class_init (GstGLDisplayClass * klass)
{
  /**
   * GstGLDisplay::create-context:
   * @object: the #GstGLDisplay
   * @context: (transfer none): other context to share resources with.
   *
   * Overrides the @GstGLContext creation mechanism.
   * It can be called in any thread and it is emitted with
   * display's object lock held.
   *
   * Returns: (transfer full) (nullable): the new context.
   */
  gst_gl_display_signals[CREATE_CONTEXT] =
      g_signal_new ("create-context", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, GST_TYPE_GL_CONTEXT, 1,
      GST_TYPE_GL_CONTEXT);

  klass->get_handle = gst_gl_display_default_get_handle;
  klass->create_window = gst_gl_display_default_create_window;

  G_OBJECT_CLASS (klass)->finalize = gst_gl_display_finalize;
  G_OBJECT_CLASS (klass)->dispose = gst_gl_display_dispose;
}

static void
gst_gl_display_init (GstGLDisplay * display)
{
  display->priv = gst_gl_display_get_instance_private (display);

  display->type = GST_GL_DISPLAY_TYPE_ANY;
  display->priv->gl_api = GST_GL_API_ANY;

  g_mutex_init (&display->priv->thread_lock);
  g_cond_init (&display->priv->thread_cond);

  g_mutex_init (&display->priv->window_lock);

  display->priv->event_thread = g_thread_new ("gldisplay-event",
      (GThreadFunc) _event_thread_main, display);

  g_mutex_lock (&display->priv->thread_lock);
  while (!display->main_loop)
    g_cond_wait (&display->priv->thread_cond, &display->priv->thread_lock);
  g_mutex_unlock (&display->priv->thread_lock);

  GST_TRACE ("init %p", display);

  gst_gl_buffer_init_once ();
  gst_gl_memory_pbo_init_once ();
  gst_gl_renderbuffer_init_once ();

#if GST_GL_HAVE_PLATFORM_EGL
  gst_gl_memory_egl_init_once ();
#endif
}

static void
gst_gl_display_dispose (GObject * object)
{
  GstGLDisplay *display = GST_GL_DISPLAY (object);

  if (display->main_loop)
    g_main_loop_quit (display->main_loop);

  if (display->priv->event_thread) {
    /* can't use g_thread_join() as we could lose the last ref from a user
     * function */
    g_mutex_lock (&display->priv->thread_lock);
    while (display->main_loop)
      g_cond_wait (&display->priv->thread_cond, &display->priv->thread_lock);
    g_mutex_unlock (&display->priv->thread_lock);
    g_thread_unref (display->priv->event_thread);
  }
  display->priv->event_thread = NULL;

  if (display->event_source) {
    g_source_destroy (display->event_source);
    g_source_unref (display->event_source);
  }
  display->event_source = NULL;

  G_OBJECT_CLASS (gst_gl_display_parent_class)->dispose (object);
}

static void
gst_gl_display_finalize (GObject * object)
{
  GstGLDisplay *display = GST_GL_DISPLAY (object);
  GList *l;

  GST_TRACE_OBJECT (object, "finalizing");

  for (l = display->priv->contexts; l; l = l->next) {
    g_weak_ref_clear ((GWeakRef *) l->data);
    g_free (l->data);
  }

  g_list_free (display->windows);
  g_list_free (display->priv->contexts);

  g_cond_clear (&display->priv->thread_cond);
  g_mutex_clear (&display->priv->thread_lock);
  g_mutex_clear (&display->priv->window_lock);

  G_OBJECT_CLASS (gst_gl_display_parent_class)->finalize (object);
}

static void
init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (gst_gl_display_debug, "gldisplay", 0,
        "gldisplay element");
    g_once_init_leave (&_init, 1);
  }
}

static GstGLDisplayType
gst_gl_display_type_from_environment (void)
{
  const char *env = g_getenv ("GST_GL_WINDOW");

  if (!env)
    return GST_GL_DISPLAY_TYPE_ANY;

  if (env) {
    if (g_strstr_len (env, 3, "x11")) {
      return GST_GL_DISPLAY_TYPE_X11;
    } else if (g_strstr_len (env, 7, "wayland")) {
      return GST_GL_DISPLAY_TYPE_WAYLAND;
    } else if (g_strstr_len (env, 5, "cocoa")) {
      return GST_GL_DISPLAY_TYPE_COCOA;
    } else if (g_strstr_len (env, 5, "win32")) {
      return GST_GL_DISPLAY_TYPE_WIN32;
    } else if (g_strstr_len (env, 8, "dispmanx")) {
      return GST_GL_DISPLAY_TYPE_DISPMANX;
    } else if (g_strstr_len (env, 10, "egl-device")) {
      return GST_GL_DISPLAY_TYPE_EGL_DEVICE;
    } else if (g_strstr_len (env, 3, "egl")) {
      return GST_GL_DISPLAY_TYPE_EGL;
    } else if (g_strstr_len (env, 6, "viv-fb")) {
      return GST_GL_DISPLAY_TYPE_VIV_FB;
    } else if (g_strstr_len (env, 3, "gbm")) {
      return GST_GL_DISPLAY_TYPE_GBM;
    } else if (g_strstr_len (env, 4, "eagl")) {
      return GST_GL_DISPLAY_TYPE_EAGL;
    } else if (g_strstr_len (env, 7, "android")) {
      return GST_GL_DISPLAY_TYPE_EGL;
    } else if (g_strstr_len (env, 5, "winrt")) {
      return GST_GL_DISPLAY_TYPE_EGL;
    } else if (g_strstr_len (env, 11, "surfaceless")) {
      return GST_GL_DISPLAY_TYPE_EGL_SURFACELESS;
    } else {
      return GST_GL_DISPLAY_TYPE_NONE;
    }
  }

  return GST_GL_DISPLAY_TYPE_ANY;
}

static GstGLDisplay *
create_dummy_display (void)
{
  GstGLDisplay *display = g_object_new (GST_TYPE_GL_DISPLAY, NULL);
  return gst_object_ref_sink (display);
}

/**
 * gst_gl_display_new_with_type:
 * @type: #GstGLDisplayType
 *
 * Will always return a #GstGLDisplay of a single type.  This differs from
 * gst_gl_display_new() and the seemingly equivalent call
 * gst_gl_display_new_with_type (GST_GL_DISPLAY_TYPE_ANY) in that the latter
 * may return NULL.
 *
 * Returns: (transfer full) (nullable): a new #GstGLDisplay or %NULL if @type is
 *          not supported
 *
 * Since: 1.20
 */
GstGLDisplay *
gst_gl_display_new_with_type (GstGLDisplayType type)
{
  GstGLDisplay *display = NULL;
  GstGLDisplayType custom_new_types = 0;

  init_debug ();

#if GST_GL_HAVE_WINDOW_COCOA
  if (!display && (type & GST_GL_DISPLAY_TYPE_COCOA)) {
    display = GST_GL_DISPLAY (gst_gl_display_cocoa_new ());
    if (!display)
      return NULL;
  }
#endif
  custom_new_types |= GST_GL_DISPLAY_TYPE_COCOA;
#if GST_GL_HAVE_WINDOW_WAYLAND
  if (!display && (type & GST_GL_DISPLAY_TYPE_WAYLAND))
    display = GST_GL_DISPLAY (gst_gl_display_wayland_new (NULL));
#endif
  custom_new_types |= GST_GL_DISPLAY_TYPE_WAYLAND;
#if GST_GL_HAVE_WINDOW_X11
  if (!display && (type & GST_GL_DISPLAY_TYPE_X11))
    display = GST_GL_DISPLAY (gst_gl_display_x11_new (NULL));
#endif
  custom_new_types |= GST_GL_DISPLAY_TYPE_X11;
#if GST_GL_HAVE_WINDOW_VIV_FB
  if (!display && (type & GST_GL_DISPLAY_TYPE_VIV_FB)) {
    const gchar *disp_idx_str = NULL;
    gint disp_idx = 0;
    disp_idx_str = g_getenv ("GST_GL_VIV_FB");
    if (disp_idx_str) {
      gint64 v = g_ascii_strtoll (disp_idx_str, NULL, 10);
      if (v >= G_MININT && v <= G_MAXINT)
        disp_idx = v;
    }
    display = GST_GL_DISPLAY (gst_gl_display_viv_fb_new (disp_idx));
  }
#endif
  custom_new_types |= GST_GL_DISPLAY_TYPE_VIV_FB;
#if GST_GL_HAVE_WINDOW_GBM
  if (!display && (type & GST_GL_DISPLAY_TYPE_GBM)) {
    display = GST_GL_DISPLAY (gst_gl_display_gbm_new ());
  }
#endif
  custom_new_types |= GST_GL_DISPLAY_TYPE_GBM;
#if GST_GL_HAVE_PLATFORM_EGL
  if (!display && (type == GST_GL_DISPLAY_TYPE_EGL_DEVICE)) {
    display = GST_GL_DISPLAY (gst_gl_display_egl_device_new (0));
  }

  if (!display && (type & GST_GL_DISPLAY_TYPE_EGL)) {
    display = GST_GL_DISPLAY (gst_gl_display_egl_new ());
  }

  if (!display && (type & GST_GL_DISPLAY_TYPE_EGL_SURFACELESS)) {
    display = GST_GL_DISPLAY (gst_gl_display_egl_new_surfaceless ());
  }
#endif
  custom_new_types |= GST_GL_DISPLAY_TYPE_EGL_DEVICE;
  custom_new_types |= GST_GL_DISPLAY_TYPE_EGL;
  custom_new_types |= GST_GL_DISPLAY_TYPE_EGL_SURFACELESS;
  custom_new_types |= GST_GL_DISPLAY_TYPE_DISPMANX;
  custom_new_types |= GST_GL_DISPLAY_TYPE_WINRT;
  custom_new_types |= GST_GL_DISPLAY_TYPE_ANDROID;
#if GST_GL_HAVE_WINDOW_WIN32 || GST_GL_HAVE_WINDOW_EAGL
  if (!display) {
    GstGLDisplayType create_type = 0;
#if GST_GL_HAVE_WINDOW_WIN32
    if (type & GST_GL_DISPLAY_TYPE_WIN32)
      create_type = GST_GL_DISPLAY_TYPE_WIN32;
#endif
#if GST_GL_HAVE_WINDOW_EAGL
    if (type & GST_GL_DISPLAY_TYPE_EAGL)
      create_type = GST_GL_DISPLAY_TYPE_EAGL;
#endif
    if (create_type) {
      GST_INFO ("Creating display with type %u(0x%x)",
          create_type, create_type);
      display = create_dummy_display ();
      display->type = create_type;
    }
  }
#endif
  custom_new_types |= GST_GL_DISPLAY_TYPE_WIN32;
  custom_new_types |= GST_GL_DISPLAY_TYPE_EAGL;

  if (!display && type != GST_GL_DISPLAY_TYPE_ANY
      && type != GST_GL_DISPLAY_TYPE_NONE) {
    /* remove all the display types that we know about */
    type &= ~custom_new_types;
    if (type && (type & (type - 1)) == 0) {
      /* only create a dummy display if we only have a single type */
      GST_INFO_OBJECT (display, "Creating dummy display with type %u(0x%x)",
          type, type);
      display = create_dummy_display ();
      display->type = type;
    }
  }

  return display;
}

/**
 * gst_gl_display_new:
 *
 * Returns: (transfer full): a new #GstGLDisplay
 *
 * Since: 1.4
 */
GstGLDisplay *
gst_gl_display_new (void)
{
  GstGLDisplayType env_choice = gst_gl_display_type_from_environment ();
  GstGLDisplay *display = gst_gl_display_new_with_type (env_choice);

  if (!display) {
    display = g_object_new (GST_TYPE_GL_DISPLAY, NULL);
    GST_INFO_OBJECT (display, "Creating dummy display");
    gst_object_ref_sink (display);
  }

  return display;
}

/**
 * gst_gl_display_get_handle:
 * @display: a #GstGLDisplay
 *
 * Returns: the native handle for the display
 *
 * Since: 1.4
 */
guintptr
gst_gl_display_get_handle (GstGLDisplay * display)
{
  GstGLDisplayClass *klass;

  g_return_val_if_fail (GST_IS_GL_DISPLAY (display), 0);
  klass = GST_GL_DISPLAY_GET_CLASS (display);
  g_return_val_if_fail (klass->get_handle != NULL, 0);

  return klass->get_handle (display);
}

static guintptr
gst_gl_display_default_get_handle (GstGLDisplay * display)
{
  return 0;
}

/**
 * gst_gl_display_filter_gl_api:
 * @display: a #GstGLDisplay
 * @gl_api: a #GstGLAPI to filter with
 *
 * limit the use of OpenGL to the requested @gl_api.  This is intended to allow
 * application and elements to request a specific set of OpenGL API's based on
 * what they support.  See gst_gl_context_get_gl_api() for the retrieving the
 * API supported by a #GstGLContext.
 */
void
gst_gl_display_filter_gl_api (GstGLDisplay * display, GstGLAPI gl_api)
{
  gchar *gl_api_s;

  g_return_if_fail (GST_IS_GL_DISPLAY (display));

  gl_api_s = gst_gl_api_to_string (gl_api);
  GST_TRACE_OBJECT (display, "filtering with api %s", gl_api_s);
  g_free (gl_api_s);

  GST_OBJECT_LOCK (display);
  display->priv->gl_api &= gl_api;
  GST_OBJECT_UNLOCK (display);
}

GstGLAPI
gst_gl_display_get_gl_api_unlocked (GstGLDisplay * display)
{
  g_return_val_if_fail (GST_IS_GL_DISPLAY (display), GST_GL_API_NONE);

  return display->priv->gl_api;
}

/**
 * gst_gl_display_get_gl_api:
 * @display: a #GstGLDisplay
 *
 * see gst_gl_display_filter_gl_api() for what the returned value represents
 *
 * Returns: the #GstGLAPI configured for @display
 */
GstGLAPI
gst_gl_display_get_gl_api (GstGLDisplay * display)
{
  GstGLAPI ret;

  g_return_val_if_fail (GST_IS_GL_DISPLAY (display), GST_GL_API_NONE);

  GST_OBJECT_LOCK (display);
  ret = display->priv->gl_api;
  GST_OBJECT_UNLOCK (display);

  return ret;
}

/**
 * gst_gl_display_get_handle_type:
 * @display: a #GstGLDisplay
 *
 * Returns: the #GstGLDisplayType of @display
 *
 * Since: 1.4
 */
GstGLDisplayType
gst_gl_display_get_handle_type (GstGLDisplay * display)
{
  g_return_val_if_fail (GST_IS_GL_DISPLAY (display), GST_GL_DISPLAY_TYPE_NONE);

  return display->type;
}

/**
 * gst_context_set_gl_display:
 * @context: a #GstContext
 * @display: (transfer none) (nullable): resulting #GstGLDisplay
 *
 * Sets @display on @context
 *
 * Since: 1.4
 */
void
gst_context_set_gl_display (GstContext * context, GstGLDisplay * display)
{
  GstStructure *s;

  g_return_if_fail (context != NULL);

  if (display)
    GST_CAT_LOG (gst_context,
        "setting GstGLDisplay(%" GST_PTR_FORMAT ") on context(%" GST_PTR_FORMAT
        ")", display, context);

  s = gst_context_writable_structure (context);
  gst_structure_set (s, GST_GL_DISPLAY_CONTEXT_TYPE, GST_TYPE_GL_DISPLAY,
      display, NULL);
}

/**
 * gst_context_get_gl_display:
 * @context: a #GstContext
 * @display: (out) (optional) (nullable) (transfer full): resulting #GstGLDisplay
 *
 * Returns: Whether @display was in @context
 *
 * Since: 1.4
 */
gboolean
gst_context_get_gl_display (GstContext * context, GstGLDisplay ** display)
{
  const GstStructure *s;
  gboolean ret;

  g_return_val_if_fail (display != NULL, FALSE);
  g_return_val_if_fail (context != NULL, FALSE);

  s = gst_context_get_structure (context);
  ret = gst_structure_get (s, GST_GL_DISPLAY_CONTEXT_TYPE,
      GST_TYPE_GL_DISPLAY, display, NULL);

  GST_CAT_LOG (gst_context, "got GstGLDisplay(%p) from context(%p)", *display,
      context);

  return ret;
}

/**
 * gst_gl_display_create_context:
 * @display: a #GstGLDisplay
 * @other_context: (transfer none) (nullable): other #GstGLContext to share resources with.
 * @p_context: (transfer full) (out): resulting #GstGLContext
 * @error: resulting #GError
 *
 * It requires the display's object lock to be held.
 *
 * Returns: whether a new context could be created.
 *
 * Since: 1.6
 */
gboolean
gst_gl_display_create_context (GstGLDisplay * display,
    GstGLContext * other_context, GstGLContext ** p_context, GError ** error)
{
  GstGLContext *context = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (display != NULL, FALSE);
  g_return_val_if_fail (p_context != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_signal_emit (display, gst_gl_display_signals[CREATE_CONTEXT], 0,
      other_context, &context);

  if (context) {
    *p_context = context;
    return TRUE;
  }

  context = gst_gl_context_new (display);
  if (!context) {
    g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_FAILED,
        "Failed to create GL context");
    return FALSE;
  }

  GST_DEBUG_OBJECT (display,
      "creating context %" GST_PTR_FORMAT " from other context %"
      GST_PTR_FORMAT, context, other_context);

  ret = gst_gl_context_create (context, other_context, error);

  if (ret)
    *p_context = context;
  else
    gst_object_unref (context);

  return ret;
}

/**
 * gst_gl_display_create_window:
 * @display: a #GstGLDisplay
 *
 * Returns: (transfer full) (nullable): a new #GstGLWindow for @display or %NULL.
 */
/* XXX: previous versions had documentation requiring the OBJECT lock to be
 * held when this fuction is called so that needs to always work. */
GstGLWindow *
gst_gl_display_create_window (GstGLDisplay * display)
{
  GstGLDisplayClass *klass;
  GstGLWindow *window;

  g_return_val_if_fail (GST_IS_GL_DISPLAY (display), NULL);
  klass = GST_GL_DISPLAY_GET_CLASS (display);
  g_return_val_if_fail (klass->create_window != NULL, NULL);

  g_mutex_lock (&display->priv->window_lock);
  window = klass->create_window (display);

  if (window) {
    display->windows = g_list_prepend (display->windows, window);
  }
  g_mutex_unlock (&display->priv->window_lock);
  GST_DEBUG_OBJECT (display, "Adding window %" GST_PTR_FORMAT
      " (%p) to internal list", window, window);

  return window;
}

static GstGLWindow *
gst_gl_display_default_create_window (GstGLDisplay * display)
{
  return gst_gl_window_new (display);
}

/**
 * gst_gl_display_remove_window:
 * @display: a #GstGLDisplay
 * @window: a #GstGLWindow to remove
 *
 * Returns: if @window could be removed from @display
 *
 * Since: 1.12
 */
gboolean
gst_gl_display_remove_window (GstGLDisplay * display, GstGLWindow * window)
{
  gboolean ret = FALSE;
  GList *l;

  g_mutex_lock (&display->priv->window_lock);
  l = g_list_find (display->windows, window);
  if (l) {
    display->windows = g_list_delete_link (display->windows, l);
    ret = TRUE;
  }
  GST_DEBUG_OBJECT (display, "Removing window %" GST_PTR_FORMAT
      " (%p) from internal list", window, window);
  g_mutex_unlock (&display->priv->window_lock);

  return ret;
}

#ifndef GST_REMOVE_DEPRECATED
/**
 * gst_gl_display_find_window:
 * @display: a #GstGLDisplay
 * @data: (closure): some data to pass to @compare_func
 * @compare_func: (scope call): a comparison function to run
 *
 * Execute @compare_func over the list of windows stored by @display.  The
 * first argument to @compare_func is the #GstGLWindow being checked and the
 * second argument is @data.
 *
 * Returns: (transfer none) (nullable): The first #GstGLWindow that causes a match
 *          from @compare_func
 *
 * Since: 1.12
 * Deprecated: 1.18: Use gst_gl_display_retrieve_window() instead.
 */
GstGLWindow *
gst_gl_display_find_window (GstGLDisplay * display, gpointer data,
    GCompareFunc compare_func)
{
  GstGLWindow *ret;

  ret = gst_gl_display_retrieve_window (display, data, compare_func);
  if (ret)
    gst_object_unref (ret);

  return ret;
}
#endif

/**
 * gst_gl_display_retrieve_window:
 * @display: a #GstGLDisplay
 * @data: (closure): some data to pass to @compare_func
 * @compare_func: (scope call): a comparison function to run
 *
 * Execute @compare_func over the list of windows stored by @display.  The
 * first argument to @compare_func is the #GstGLWindow being checked and the
 * second argument is @data.
 *
 * Returns: (transfer full) (nullable): The first #GstGLWindow that causes a match
 *          from @compare_func
 *
 * Since: 1.18
 */
GstGLWindow *
gst_gl_display_retrieve_window (GstGLDisplay * display, gpointer data,
    GCompareFunc compare_func)
{
  GstGLWindow *ret = NULL;
  GList *l;

  g_mutex_lock (&display->priv->window_lock);
  l = g_list_find_custom (display->windows, data, compare_func);
  if (l)
    ret = gst_object_ref (l->data);

  GST_DEBUG_OBJECT (display, "Found window %" GST_PTR_FORMAT
      " (%p) in internal list", ret, ret);
  g_mutex_unlock (&display->priv->window_lock);

  return ret;
}

static GstGLContext *
_get_gl_context_for_thread_unlocked (GstGLDisplay * display, GThread * thread)
{
  GstGLContext *context = NULL;
  GList *prev = NULL, *l = display->priv->contexts;

  while (l) {
    GWeakRef *ref = l->data;
    GThread *context_thread;

    context = g_weak_ref_get (ref);
    if (!context) {
      /* remove dead contexts */
      g_weak_ref_clear (l->data);
      g_free (l->data);
      display->priv->contexts = g_list_delete_link (display->priv->contexts, l);
      l = prev ? prev->next : display->priv->contexts;
      continue;
    }

    if (thread == NULL) {
      GST_DEBUG_OBJECT (display, "Returning GL context %" GST_PTR_FORMAT " for "
          "NULL thread", context);
      return context;
    }

    context_thread = gst_gl_context_get_thread (context);
    if (thread != context_thread) {
      g_thread_unref (context_thread);
      gst_object_unref (context);
      prev = l;
      l = l->next;
      continue;
    }

    if (context_thread)
      g_thread_unref (context_thread);

    GST_DEBUG_OBJECT (display, "Returning GL context %" GST_PTR_FORMAT " for "
        "thread %p", context, thread);
    return context;
  }

  GST_DEBUG_OBJECT (display, "No GL context for thread %p", thread);
  return NULL;
}

/**
 * gst_gl_display_get_gl_context_for_thread:
 * @display: a #GstGLDisplay
 * @thread: a #GThread
 *
 * Returns: (transfer full) (nullable): the #GstGLContext current on @thread or %NULL
 *
 * Must be called with the object lock held.
 *
 * Since: 1.6
 */
GstGLContext *
gst_gl_display_get_gl_context_for_thread (GstGLDisplay * display,
    GThread * thread)
{
  GstGLContext *context;

  g_return_val_if_fail (GST_IS_GL_DISPLAY (display), NULL);

  context = _get_gl_context_for_thread_unlocked (display, thread);
  GST_DEBUG_OBJECT (display, "returning context %" GST_PTR_FORMAT " for thread "
      "%p", context, thread);

  return context;
}

static gboolean
_check_collision (GstGLContext * context, GstGLContext * collision)
{
  GThread *thread, *collision_thread;
  gboolean ret = FALSE;

  if (!context || !collision)
    return FALSE;

  thread = gst_gl_context_get_thread (context);
  collision_thread = gst_gl_context_get_thread (collision);

  if (!thread || !collision_thread) {
    ret = FALSE;
    goto out;
  }

  if (thread == collision_thread) {
    ret = TRUE;
    goto out;
  }

out:
  if (thread)
    g_thread_unref (thread);
  if (collision_thread)
    g_thread_unref (collision_thread);

  return ret;
}

/**
 * gst_gl_display_add_context:
 * @display: a #GstGLDisplay
 * @context: (transfer none): a #GstGLContext
 *
 * Returns: whether @context was successfully added. %FALSE may be returned
 * if there already exists another context for @context's active thread.
 *
 * Must be called with the object lock held.
 *
 * Since: 1.6
 */
gboolean
gst_gl_display_add_context (GstGLDisplay * display, GstGLContext * context)
{
  GstGLContext *collision = NULL;
  GstGLDisplay *context_display;
  gboolean ret = TRUE;
  GThread *thread;
  GWeakRef *ref;

  g_return_val_if_fail (GST_IS_GL_DISPLAY (display), FALSE);
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);

  context_display = gst_gl_context_get_display (context);
  g_assert (context_display == display);
  gst_object_unref (context_display);

  thread = gst_gl_context_get_thread (context);
  if (thread) {
    collision = _get_gl_context_for_thread_unlocked (display, thread);
    g_thread_unref (thread);

    /* adding the same context is a no-op */
    if (context == collision) {
      GST_LOG_OBJECT (display, "Attempting to add the same GL context %"
          GST_PTR_FORMAT ". Ignoring", context);
      ret = TRUE;
      goto out;
    }

    if (_check_collision (context, collision)) {
      GST_DEBUG_OBJECT (display, "Collision detected adding GL context "
          "%" GST_PTR_FORMAT, context);
      ret = FALSE;
      goto out;
    }
  }

  ref = g_new0 (GWeakRef, 1);
  g_weak_ref_init (ref, context);

  GST_DEBUG_OBJECT (display, "Adding GL context %" GST_PTR_FORMAT, context);
  display->priv->contexts = g_list_prepend (display->priv->contexts, ref);

out:
  if (collision)
    gst_object_unref (collision);

  GST_DEBUG_OBJECT (display, "%ssuccessfully inserted context %" GST_PTR_FORMAT,
      ret ? "" : "un", context);

  return ret;
}

/**
 * gst_gl_display_remove_context:
 * @display: a #GstGLDisplay
 * @context: (transfer none): the #GstGLContext to remove
 *
 * Must be called with the object lock held.
 *
 * Since: 1.18
 */
void
gst_gl_display_remove_context (GstGLDisplay * display, GstGLContext * needle)
{
  GstGLContext *context;
  GList *prev = NULL, *l;

  g_return_if_fail (GST_IS_GL_DISPLAY (display));
  g_return_if_fail (GST_IS_GL_CONTEXT (needle));

  l = display->priv->contexts;

  while (l) {
    GWeakRef *ref = l->data;

    context = g_weak_ref_get (ref);
    if (!context || context == needle) {
      /* remove dead contexts */
      g_weak_ref_clear (l->data);
      g_free (l->data);
      display->priv->contexts = g_list_delete_link (display->priv->contexts, l);
      if (context) {
        GST_INFO_OBJECT (display, "removed context %" GST_PTR_FORMAT
            " from internal list", context);
        gst_object_unref (context);
        return;
      }
      l = prev ? prev->next : display->priv->contexts;
      continue;
    }
    prev = l;
    l = l->next;
  }

  GST_WARNING_OBJECT (display, "%" GST_PTR_FORMAT " was not found in this "
      "display", needle);
}


/**
 * gst_gl_display_ensure_context:
 * @display: a #GstGLDisplay
 * @other_context: (transfer none)(nullable): other #GstGLContext to share resources with.
 * @context: (inout)(transfer full)(nullable): the resulting #GstGLContext
 * @error: (out)(transfer full)(nullable): possible error
 *
 * Ensures that the display has a valid GL context for the current thread. If
 * @context already contains a valid context, this does nothing.
 *
 * Returns: wether @context contains a valid context.
 *
 * Since: 1.24
 */
gboolean
gst_gl_display_ensure_context (GstGLDisplay * display,
    GstGLContext * other_context, GstGLContext ** context, GError ** error)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (GST_IS_GL_DISPLAY (display), FALSE);
  g_return_val_if_fail (other_context == NULL
      || GST_IS_GL_CONTEXT (other_context), FALSE);
  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (*context == NULL
      || GST_IS_GL_CONTEXT (*context), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (*context && (*context)->display == display) {
    return TRUE;
  }

  GST_OBJECT_LOCK (display);
  do {
    if (*context) {
      gst_object_unref (*context);
      *context = NULL;
    }
    /* just get a GL context.  we don't care */
    *context = gst_gl_display_get_gl_context_for_thread (display, NULL);
    if (!*context) {
      if (!gst_gl_display_create_context (display, other_context, &*context,
              error)) {
        goto out;
      }
    }
  } while (!gst_gl_display_add_context (display, *context));
  ret = TRUE;
out:
  GST_OBJECT_UNLOCK (display);
  return ret;
}
