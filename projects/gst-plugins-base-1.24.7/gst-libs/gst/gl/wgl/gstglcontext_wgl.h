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

#ifndef __GST_GL_CONTEXT_WGL_H__
#define __GST_GL_CONTEXT_WGL_H__

#include "../win32/gstglwindow_win32.h"

G_BEGIN_DECLS

#define GST_TYPE_GL_CONTEXT_WGL         (gst_gl_context_wgl_get_type())
#define GST_GL_CONTEXT_WGL(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_GL_CONTEXT_WGL, GstGLContextWGL))
#define GST_GL_CONTEXT_WGL_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_GL_CONTEXT_WGL, GstGLContextWGLClass))
#define GST_IS_GL_CONTEXT_WGL(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_GL_CONTEXT_WGL))
#define GST_IS_GL_CONTEXT_WGL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_GL_CONTEXT_WGL))
#define GST_GL_CONTEXT_WGL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_GL_CONTEXT_WGL, GstGLContextWGLClass))

typedef struct _GstGLContextWGL        GstGLContextWGL;
typedef struct _GstGLContextWGLClass   GstGLContextWGLClass;
typedef struct _GstGLContextWGLPrivate GstGLContextWGLPrivate;

struct _GstGLContextWGL {
  /*< private >*/
  GstGLContext parent;

  HGLRC wgl_context;
  HGLRC external_gl_context;

  GstGLContextWGLPrivate *priv;

  gpointer _reserved[GST_PADDING];
};

struct _GstGLContextWGLClass {
  /*< private >*/
  GstGLContextClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING];
};

GType gst_gl_context_wgl_get_type     (void);

GstGLContextWGL *   gst_gl_context_wgl_new                  (GstGLDisplay * display);
guintptr            gst_gl_context_wgl_get_current_context  (void);
gpointer            gst_gl_context_wgl_get_proc_address     (GstGLAPI gl_api, const gchar * name);

G_END_DECLS

#endif /* __GST_GL_CONTEXT_WGL_H__ */
