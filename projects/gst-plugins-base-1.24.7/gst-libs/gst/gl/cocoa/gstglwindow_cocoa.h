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

#ifndef __GST_GL_WINDOW_COCOA_H__
#define __GST_GL_WINDOW_COCOA_H__

#include <gst/gst.h>

#include <gst/gl/gl.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_WINDOW_COCOA         (gst_gl_window_cocoa_get_type())
#define GST_GL_WINDOW_COCOA(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_GL_WINDOW_COCOA, GstGLWindowCocoa))
#define GST_GL_WINDOW_COCOA_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_GL_WINDOW_COCOA, GstGLWindowCocoaClass))
#define GST_IS_GL_WINDOW_COCOA(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_GL_WINDOW_COCOA))
#define GST_IS_GL_WINDOW_COCOA_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_GL_WINDOW_COCOA))
#define GST_GL_WINDOW_COCOA_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_GL_WINDOW_COCOA, GstGLWindowCocoaClass))

typedef struct _GstGLWindowCocoa        GstGLWindowCocoa;
typedef struct _GstGLWindowCocoaPrivate GstGLWindowCocoaPrivate;
typedef struct _GstGLWindowCocoaClass   GstGLWindowCocoaClass;

struct _GstGLWindowCocoa {
  /*< private >*/
  GstGLWindow parent;

  /*< private >*/
  GstGLWindowCocoaPrivate *priv;
  
  gpointer _reserved[GST_PADDING];
};

struct _GstGLWindowCocoaClass {
  /*< private >*/
  GstGLWindowClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING_LARGE];
};

GType gst_gl_window_cocoa_get_type     (void);

GstGLWindowCocoa * gst_gl_window_cocoa_new (GstGLDisplay * display);

void gst_gl_window_cocoa_draw_thread (GstGLWindowCocoa *window_cocoa);

G_END_DECLS

#endif /* __GST_GL_WINDOW_COCOA_H__ */
