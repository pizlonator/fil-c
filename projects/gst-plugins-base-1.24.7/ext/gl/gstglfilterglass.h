/* 
 * GStreamer
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
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

#ifndef _GST_GL_FILTERGLASS_H_
#define _GST_GL_FILTERGLASS_H_

#include <gst/gl/gstglfilter.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_FILTER_GLASS            (gst_gl_filter_glass_get_type())
#define GST_GL_FILTER_GLASS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_FILTER_GLASS,GstGLFilterGlass))
#define GST_IS_GL_FILTER_GLASS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_FILTER_GLASS))
#define GST_GL_FILTER_GLASS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_FILTER_GLASS,GstGLFilterGlassClass))
#define GST_IS_GL_FILTER_GLASS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_FILTER_GLASS))
#define GST_GL_FILTER_GLASS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_FILTER_GLASS,GstGLFilterGlassClass))

typedef struct _GstGLFilterGlass GstGLFilterGlass;
typedef struct _GstGLFilterGlassClass GstGLFilterGlassClass;

struct _GstGLFilterGlass
{
  GstGLFilter filter;
  GstGLShader *passthrough_shader;
  GstGLShader *shader;
  gint64 timestamp;
  GstGLMemory *in_tex;
  GstGLMemory *out_tex;
  gint64 start_time;
};

struct _GstGLFilterGlassClass
{
  GstGLFilterClass filter_class;
};

GType gst_gl_filter_glass_get_type (void);

G_END_DECLS

#endif /* _GST_GLFILTERGLASS_H_ */
