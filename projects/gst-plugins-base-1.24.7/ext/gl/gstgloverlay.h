/*
 * GStreamer
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
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

#ifndef _GST_GL_OVERLAY_H_
#define _GST_GL_OVERLAY_H_

#include <gst/gl/gstglfilter.h>
#include <gst/gl/gstglfuncs.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_OVERLAY            (gst_gl_overlay_get_type())
#define GST_GL_OVERLAY(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GL_OVERLAY,GstGLOverlay))
#define GST_IS_GL_OVERLAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GL_OVERLAY))
#define GST_GL_OVERLAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) , GST_TYPE_GL_OVERLAY,GstGLOverlayClass))
#define GST_IS_GL_OVERLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) , GST_TYPE_GL_OVERLAY))
#define GST_GL_OVERLAY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) , GST_TYPE_GL_OVERLAY,GstGLOverlayClass))

typedef struct _GstGLOverlay GstGLOverlay;
typedef struct _GstGLOverlayClass GstGLOverlayClass;

struct _GstGLOverlay
{
  GstGLFilter  filter;

  /* properties */
  gchar        *location;
  gint          offset_x;
  gint          offset_y;

  gdouble       relative_x;
  gdouble       relative_y;

  gint          overlay_width;
  gint          overlay_height;

  gdouble       alpha;

  GstGLShader  *shader;
  GstGLMemory  *image_memory;

  gboolean      location_has_changed;
  gint          window_width, window_height;
  gint          image_width, image_height;

  gboolean      geometry_change;

  GLuint        vao;
  GLuint        overlay_vao;
  GLuint        vbo;
  GLuint        overlay_vbo;
  GLuint        vbo_indices;
  GLuint        attr_position;
  GLuint        attr_texture;
};

struct _GstGLOverlayClass
{
  GstGLFilterClass filter_class;
};

GType gst_gl_overlay_get_type (void);

G_END_DECLS

#endif /* _GST_GL_OVERLAY_H_ */
