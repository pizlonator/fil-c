/*
 * GStreamer
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_GL_CONTEXT_EAGL_H__
#define __GST_GL_CONTEXT_EAGL_H__

#include <gst/gst.h>
#include <gst/gl/gl.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_CONTEXT_EAGL         (gst_gl_context_eagl_get_type())
#define GST_GL_CONTEXT_EAGL(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_GL_CONTEXT_EAGL, GstGLContextEagl))
#define GST_GL_CONTEXT_EAGL_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_GL_CONTEXT_EAGL, GstGLContextEaglClass))
#define GST_IS_GL_CONTEXT_EAGL(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_GL_CONTEXT_EAGL))
#define GST_IS_GL_CONTEXT_EAGL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_GL_CONTEXT_EAGL))
#define GST_GL_CONTEXT_EAGL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_GL_CONTEXT_EAGL, GstGLContextEaglClass))

typedef struct _GstGLContextEagl        GstGLContextEagl;
typedef struct _GstGLContextEaglPrivate GstGLContextEaglPrivate;
typedef struct _GstGLContextEaglClass   GstGLContextEaglClass;

struct _GstGLContextEagl {
  /*< private >*/
  GstGLContext parent;

  /*< private >*/
  GstGLContextEaglPrivate *priv;
  
  gpointer _reserved[GST_PADDING];
};

struct _GstGLContextEaglClass {
  /*< private >*/
  GstGLContextClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING_LARGE];
};

GType gst_gl_context_eagl_get_type (void);

GstGLContextEagl * gst_gl_context_eagl_new (GstGLDisplay * display);

void gst_gl_context_eagl_update_layer (GstGLContext * context, gpointer layer);
void gst_gl_context_eagl_resize (GstGLContextEagl * eagl_context);
void gst_gl_context_eagl_prepare_draw (GstGLContextEagl * context);
void gst_gl_context_eagl_finish_draw (GstGLContextEagl * context);
guintptr gst_gl_context_eagl_get_current_context (void);

G_END_DECLS

#endif /* __GST_GL_CONTEXT_EAGL_H__ */
