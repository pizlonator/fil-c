/*
 * GStreamer
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2012 Matthew Waters <ystreet00@gmail.com>
 * Copyright (C) 2015 Freescale Semiconductor <b55597@freescale.com>
 * Copyright (C) 2017 Sebastian Dröge <sebastian@centricular.com>
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

#include "../gstgl_fwd.h"
#include <gst/gl/gstglcontext.h>
#include <gst/gl/gstglfuncs.h>

#include "gstglwindow_viv_fb_egl.h"
#include "../gstglwindow_private.h"

#define GST_CAT_DEFAULT gst_gl_window_debug

#define gst_gl_window_viv_fb_egl_parent_class parent_class
G_DEFINE_TYPE (GstGLWindowVivFBEGL, gst_gl_window_viv_fb_egl,
    GST_TYPE_GL_WINDOW);

static guintptr gst_gl_window_viv_fb_egl_get_window_handle (GstGLWindow *
    window);
static guintptr gst_gl_window_viv_fb_egl_get_display (GstGLWindow * window);
static void gst_gl_window_viv_fb_egl_set_window_handle (GstGLWindow * window,
    guintptr handle);
static void gst_gl_window_viv_fb_egl_close (GstGLWindow * window);
static gboolean gst_gl_window_viv_fb_egl_open (GstGLWindow * window,
    GError ** error);
static void gst_gl_window_viv_fb_egl_draw (GstGLWindow * window);
static gboolean
gst_gl_window_viv_fb_egl_set_render_rectangle (GstGLWindow * window,
    gint x, gint y, gint width, gint height);
static gboolean gst_gl_window_viv_fb_egl_controls_viewport (GstGLWindow *
    window);

static void
gst_gl_window_viv_fb_egl_class_init (GstGLWindowVivFBEGLClass * klass)
{
  GstGLWindowClass *window_class = (GstGLWindowClass *) klass;

  window_class->get_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_viv_fb_egl_get_window_handle);
  window_class->get_display =
      GST_DEBUG_FUNCPTR (gst_gl_window_viv_fb_egl_get_display);
  window_class->set_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_viv_fb_egl_set_window_handle);
  window_class->close = GST_DEBUG_FUNCPTR (gst_gl_window_viv_fb_egl_close);
  window_class->open = GST_DEBUG_FUNCPTR (gst_gl_window_viv_fb_egl_open);
  window_class->draw = GST_DEBUG_FUNCPTR (gst_gl_window_viv_fb_egl_draw);
  window_class->set_render_rectangle =
      GST_DEBUG_FUNCPTR (gst_gl_window_viv_fb_egl_set_render_rectangle);
  window_class->controls_viewport =
      GST_DEBUG_FUNCPTR (gst_gl_window_viv_fb_egl_controls_viewport);
}

static void
gst_gl_window_viv_fb_egl_init (GstGLWindowVivFBEGL * window)
{
}

/* Must be called in the gl thread */
GstGLWindowVivFBEGL *
gst_gl_window_viv_fb_egl_new (GstGLDisplay * display)
{
  GstGLWindowVivFBEGL *window;

  if ((gst_gl_display_get_handle_type (display) & GST_GL_DISPLAY_TYPE_VIV_FB) ==
      0)
    /* we require a Vivante FB display to create windows */
    return NULL;

  window = g_object_new (GST_TYPE_GL_WINDOW_VIV_FB_EGL, NULL);
  gst_object_ref_sink (window);

  return window;
}

static void
gst_gl_window_viv_fb_egl_close (GstGLWindow * window)
{
  GstGLWindowVivFBEGL *window_egl = GST_GL_WINDOW_VIV_FB_EGL (window);

  if (window_egl->win_id && !window_egl->external_window) {
    fbDestroyWindow (window_egl->win_id);
    window_egl->win_id = 0;
  }

  GST_GL_WINDOW_CLASS (parent_class)->close (window);
}

static guintptr
gst_gl_window_viv_fb_egl_get_display (GstGLWindow * window)
{
  return gst_gl_display_get_handle (window->display);
}

static gboolean
gst_gl_window_viv_fb_egl_open (GstGLWindow * window, GError ** error)
{
  GstGLWindowVivFBEGL *window_egl = GST_GL_WINDOW_VIV_FB_EGL (window);
  EGLNativeDisplayType display;

  display = (EGLNativeDisplayType) gst_gl_window_get_display (window);

  window_egl->win_id = fbCreateWindow (display, -1, -1, 0, 0);
  window_egl->external_window = FALSE;
  if (!window_egl->win_id) {
    g_set_error (error, GST_GL_WINDOW_ERROR,
        GST_GL_WINDOW_ERROR_RESOURCE_UNAVAILABLE, "Can't create window");
    return FALSE;
  }

  return GST_GL_WINDOW_CLASS (parent_class)->open (window, error);
}

void
gst_gl_window_viv_fb_egl_create_window (GstGLWindowVivFBEGL * window_egl)
{
  fbGetWindowGeometry (window_egl->win_id, NULL, NULL,
      &window_egl->window_width, &window_egl->window_height);
  window_egl->render_rectangle.x = 0;
  window_egl->render_rectangle.y = 0;
  window_egl->render_rectangle.w = window_egl->window_width;
  window_egl->render_rectangle.h = window_egl->window_height;
  gst_gl_window_resize (GST_GL_WINDOW (window_egl), window_egl->window_width,
      window_egl->window_height);

  GST_DEBUG
      ("create viv-fb window, resolution is (%dx%d), window %p.",
      window_egl->window_width, window_egl->window_height,
      (gpointer) window_egl->win_id);
}

static guintptr
gst_gl_window_viv_fb_egl_get_window_handle (GstGLWindow * window)
{
  return (guintptr) GST_GL_WINDOW_VIV_FB_EGL (window)->win_id;
}

static void
gst_gl_window_viv_fb_egl_set_window_handle (GstGLWindow * window,
    guintptr handle)
{
  GstGLWindowVivFBEGL *window_egl = GST_GL_WINDOW_VIV_FB_EGL (window);
  gint width, height;

  if (window_egl->win_id)
    fbDestroyWindow (window_egl->win_id);
  window_egl->win_id = (EGLNativeWindowType) handle;
  window_egl->external_window = handle != 0;

  fbGetWindowGeometry (window_egl->win_id, NULL, NULL, &width, &height);
  gst_gl_window_resize (window, width, height);
}

static void
draw_cb (gpointer data)
{
  GstGLWindowVivFBEGL *window_egl = data;
  GstGLWindow *window = GST_GL_WINDOW (window_egl);
  GstGLContext *context = gst_gl_window_get_context (window);
  const GstGLFuncs *gl;
  gint viewport_dim[4];

  gl = context->gl_vtable;

  if (window->queue_resize) {
    guint width, height;

    gst_gl_window_get_surface_dimensions (window, &width, &height);
    gst_gl_window_resize (window, width, height);

    gl->GetIntegerv (GL_VIEWPORT, viewport_dim);
    window_egl->viewport.x = viewport_dim[0] + window_egl->render_rectangle.x;
    window_egl->viewport.y = viewport_dim[1] + window_egl->render_rectangle.y;
    window_egl->viewport.w = viewport_dim[2];
    window_egl->viewport.h = viewport_dim[2];
  }

  gl->Viewport (window_egl->viewport.x, window_egl->viewport.y,
      window_egl->viewport.w, window_egl->viewport.h);

  if (window->draw)
    window->draw (window->draw_data);

  gst_gl_context_swap_buffers (context);

  gst_object_unref (context);
}

static void
gst_gl_window_viv_fb_egl_draw (GstGLWindow * window)
{
  gst_gl_window_send_message (window, (GstGLWindowCB) draw_cb, window);
}

typedef struct
{
  GstGLWindowVivFBEGL *window_egl;
  GstVideoRectangle rect;
} SetRenderRectangleData;

static void
_free_set_render_rectangle (SetRenderRectangleData * render)
{
  if (render) {
    if (render->window_egl)
      gst_object_unref (render->window_egl);
    g_free (render);
  }
}

static void
_calculate_viewport_coordinates (GstGLWindowVivFBEGL * window_egl,
    GstVideoRectangle * req, GstVideoRectangle * result)
{
  result->x = req->x;
  result->y = window_egl->window_height - (req->y + req->h);
  result->w = req->w;
  result->h = req->h;
}

static void
_set_render_rectangle (gpointer data)
{
  SetRenderRectangleData *render = data;
  GstGLWindowVivFBEGL *window_egl = render->window_egl;
  GstGLWindow *window = GST_GL_WINDOW (window_egl);

  GST_LOG_OBJECT (render->window_egl, "setting render rectangle %i,%i+%ix%i",
      render->rect.x, render->rect.y, render->rect.w, render->rect.h);

  _calculate_viewport_coordinates (window_egl, &render->rect,
      &window_egl->render_rectangle);

  gst_gl_window_resize (window, render->rect.w, render->rect.h);

  window->queue_resize = TRUE;
}

static gboolean
gst_gl_window_viv_fb_egl_set_render_rectangle (GstGLWindow * window,
    gint x, gint y, gint width, gint height)
{
  GstGLWindowVivFBEGL *window_egl = GST_GL_WINDOW_VIV_FB_EGL (window);
  SetRenderRectangleData *render;

  render = g_new0 (SetRenderRectangleData, 1);
  render->window_egl = gst_object_ref (window_egl);
  render->rect.x = x;
  render->rect.y = y;
  render->rect.w = width;
  render->rect.h = height;

  gst_gl_window_send_message_async (window,
      (GstGLWindowCB) _set_render_rectangle, render,
      (GDestroyNotify) _free_set_render_rectangle);

  return TRUE;
}

static gboolean
gst_gl_window_viv_fb_egl_controls_viewport (GstGLWindow * window)
{
  return TRUE;
}
