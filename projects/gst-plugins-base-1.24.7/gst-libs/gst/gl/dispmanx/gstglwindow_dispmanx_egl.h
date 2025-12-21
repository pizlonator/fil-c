/*
 * GStreamer
 * Copyright (C) 2013 Julien Isorce <julien.isorce@collabora.co.uk>
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

#ifndef __GST_GL_WINDOW_DISPMANX_EGL_H__
#define __GST_GL_WINDOW_DISPMANX_EGL_H__

#include <gst/video/gstvideosink.h>
#include <gst/gl/gl.h>
#include <gst/gl/egl/gstegl.h>

#if defined(__GNUC__)
#ifndef __VCCOREVER__
#define __VCCOREVER__ 0x04000000
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
#pragma GCC optimize ("gnu89-inline")
#endif

#include <bcm_host.h>

#if defined(__GNUC__)
#pragma GCC reset_options
#pragma GCC diagnostic pop
#endif

G_BEGIN_DECLS

#define GST_TYPE_GL_WINDOW_DISPMANX_EGL         (gst_gl_window_dispmanx_egl_get_type())
#define GST_GL_WINDOW_DISPMANX_EGL(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_GL_WINDOW_DISPMANX_EGL, GstGLWindowDispmanxEGL))
#define GST_GL_WINDOW_DISPMANX_EGL_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_GL_WINDOW_DISPMANX_EGL, GstGLWindowDispmanxEGLClass))
#define GST_IS_GL_WINDOW_DISPMANX_EGL(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_GL_WINDOW_DISPMANX_EGL))
#define GST_IS_GL_WINDOW_DISPMANX_EGL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_GL_WINDOW_DISPMANX_EGL))
#define GST_GL_WINDOW_DISPMANX_EGL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_GL_WINDOW_DISPMANX_EGL, GstGLWindowDispmanxEGL_Class))

typedef struct _GstGLWindowDispmanxEGL        GstGLWindowDispmanxEGL;
typedef struct _GstGLWindowDispmanxEGLClass   GstGLWindowDispmanxEGLClass;

struct _GstGLWindowDispmanxEGL {
  /*< private >*/
  GstGLWindow parent;

  EGLDisplay egldisplay;

  DISPMANX_DISPLAY_HANDLE_T display;
  uint32_t dp_height;
  uint32_t dp_width;
  EGL_DISPMANX_WINDOW_T native;
  EGL_DISPMANX_WINDOW_T foreign;

  gint preferred_width;
  gint preferred_height;
  GstVideoRectangle render_rect;

  gboolean visible;

  gpointer _reserved[GST_PADDING];
};

struct _GstGLWindowDispmanxEGLClass {
  /*< private >*/
  GstGLWindowClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING];
};

GType gst_gl_window_dispmanx_egl_get_type     (void);

GstGLWindowDispmanxEGL * gst_gl_window_dispmanx_egl_new  (GstGLDisplay * display);
gboolean gst_gl_window_dispmanx_egl_create_window (GstGLWindowDispmanxEGL * window_egl);

G_END_DECLS

#endif /* __GST_GL_WINDOW_X11_H__ */
