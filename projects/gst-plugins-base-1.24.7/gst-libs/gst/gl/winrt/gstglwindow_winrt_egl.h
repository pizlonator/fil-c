/*
 * GStreamer
 * Copyright (C) 2019 Nirbheek Chauhan <nirbheek@centricular.com>
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

#ifndef __GST_GL_WINDOW_WINRT_EGL_H__
#define __GST_GL_WINDOW_WINRT_EGL_H__

#include <gst/gl/gl.h>
#include <gst/gl/egl/gstegl.h>

G_BEGIN_DECLS

GType gst_gl_window_winrt_egl_get_type (void);

#define GST_TYPE_GL_WINDOW_WINRT_EGL         (gst_gl_window_winrt_egl_get_type())
#define GST_GL_WINDOW_WINRT_EGL(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_GL_WINDOW_WINRT_EGL, GstGLWindowWinRTEGL))
#define GST_GL_WINDOW_WINRT_EGL_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_GL_WINDOW_WINRT_EGL, GstGLWindowWinRTEGLClass))
#define GST_IS_GL_WINDOW_WINRT_EGL(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_GL_WINDOW_WINRT_EGL))
#define GST_IS_GL_WINDOW_WINRT_EGL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_GL_WINDOW_WINRT_EGL))
#define GST_GL_WINDOW_WINRT_EGL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_GL_WINDOW_WINRT_EGL, GstGLWindowWinRTEGL_Class))

typedef struct _GstGLWindowWinRTEGL        GstGLWindowWinRTEGL;
typedef struct _GstGLWindowWinRTEGLClass   GstGLWindowWinRTEGLClass;
typedef struct _GstGLWindowWinRTEGLPrivate GstGLWindowWinRTEGLPrivate;

struct _GstGLWindowWinRTEGL {
  /*< private >*/
  GstGLWindow parent;

  /* This is actually an IInspectable type, which must be one of:
   * ICoreWindow, ISwapChainPanel, IPropertySet */
  EGLNativeWindowType window;

  GstGLWindowWinRTEGLPrivate *priv;

  gpointer _reserved[GST_PADDING];
};

struct _GstGLWindowWinRTEGLClass {
  /*< private >*/
  GstGLWindowClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING_LARGE];
};

GstGLWindowWinRTEGL * gst_gl_window_winrt_egl_new (GstGLDisplay * display);

G_END_DECLS

#endif /* __GST_GL_WINDOW_WINRT_EGL_H__ */
