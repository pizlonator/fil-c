/*
 * GStreamer
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2012 Matthew Waters <ystreet00@gmail.com>
 * Copyright (C) 2013 Sebastian Dröge <slomo@circular-chaos.org>
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

/* TODO: - Window resize handling
 *       - Event handling input event handling
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include <gst/gl/egl/gstglcontext_egl.h>
#include "gstglwindow_android_egl.h"
#include "../gstglwindow_private.h"

#define GST_CAT_DEFAULT gst_gl_window_debug

#define gst_gl_window_android_egl_parent_class parent_class
G_DEFINE_TYPE (GstGLWindowAndroidEGL, gst_gl_window_android_egl,
    GST_TYPE_GL_WINDOW);

static guintptr gst_gl_window_android_egl_get_display (GstGLWindow * window);
static guintptr gst_gl_window_android_egl_get_window_handle (GstGLWindow *
    window);
static void gst_gl_window_android_egl_set_window_handle (GstGLWindow * window,
    guintptr handle);
static void gst_gl_window_android_egl_draw (GstGLWindow * window);

static void
gst_gl_window_android_egl_class_init (GstGLWindowAndroidEGLClass * klass)
{
  GstGLWindowClass *window_class = (GstGLWindowClass *) klass;

  window_class->get_display =
      GST_DEBUG_FUNCPTR (gst_gl_window_android_egl_get_display);
  window_class->get_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_android_egl_get_window_handle);
  window_class->set_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_android_egl_set_window_handle);
  window_class->draw = GST_DEBUG_FUNCPTR (gst_gl_window_android_egl_draw);
}

static void
gst_gl_window_android_egl_init (GstGLWindowAndroidEGL * window)
{
}

/* Must be called in the gl thread */
GstGLWindowAndroidEGL *
gst_gl_window_android_egl_new (GstGLDisplay * display)
{
  GstGLWindowAndroidEGL *window;

  if ((gst_gl_display_get_handle_type (display) & GST_GL_DISPLAY_TYPE_EGL) == 0)
    /* we require an egl display to create android windows */
    return NULL;

  GST_DEBUG ("creating Android EGL window");

  window = g_object_new (GST_TYPE_GL_WINDOW_ANDROID_EGL, NULL);
  gst_object_ref_sink (window);

  return window;
}

static void
gst_gl_window_android_egl_set_window_handle (GstGLWindow * window,
    guintptr handle)
{
  GstGLWindowAndroidEGL *window_egl = GST_GL_WINDOW_ANDROID_EGL (window);

  window_egl->native_window = (EGLNativeWindowType) handle;
}

static guintptr
gst_gl_window_android_egl_get_window_handle (GstGLWindow * window)
{
  GstGLWindowAndroidEGL *window_egl = GST_GL_WINDOW_ANDROID_EGL (window);

  return (guintptr) window_egl->native_window;
}

static void
draw_cb (gpointer data)
{
  GstGLWindowAndroidEGL *window_egl = data;
  GstGLWindow *window = GST_GL_WINDOW (window_egl);
  GstGLContext *context = gst_gl_window_get_context (window);
  GstGLContextEGL *context_egl = GST_GL_CONTEXT_EGL (context);

  if (context_egl->egl_surface) {
    gint width, height;
    guint window_width, window_height;

    gst_gl_window_get_surface_dimensions (window, &window_width,
        &window_height);
    if (eglQuerySurface (context_egl->egl_display, context_egl->egl_surface,
            EGL_WIDTH, &width)
        && eglQuerySurface (context_egl->egl_display, context_egl->egl_surface,
            EGL_HEIGHT, &height)
        && (window->queue_resize || width != window_egl->window_width
            || height != window_egl->window_height)) {
      gst_gl_window_resize (window, width, height);
    }
  }

  if (window->draw)
    window->draw (window->draw_data);

  gst_gl_context_swap_buffers (context);

  gst_object_unref (context);
}

static void
gst_gl_window_android_egl_draw (GstGLWindow * window)
{
  gst_gl_window_send_message (window, (GstGLWindowCB) draw_cb, window);
}

static guintptr
gst_gl_window_android_egl_get_display (GstGLWindow * window)
{
  return 0;
}
