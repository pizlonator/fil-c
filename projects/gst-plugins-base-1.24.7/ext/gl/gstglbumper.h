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

#ifndef _GST_GL_BUMPER_H_
#define _GST_GL_BUMPER_H_

#include <gst/gl/gstglfilter.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_BUMPER            (gst_gl_bumper_get_type())
#define GST_GL_BUMPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_BUMPER,GstGLBumper))
#define GST_IS_GL_BUMPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_BUMPER))
#define GST_GL_BUMPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_BUMPER,GstGLBumperClass))
#define GST_IS_GL_BUMPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_BUMPER))
#define GST_GL_BUMPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_BUMPER,GstGLBumperClass))

typedef struct _GstGLBumper GstGLBumper;
typedef struct _GstGLBumperClass GstGLBumperClass;

struct _GstGLBumper
{
  GstGLFilter filter;
  GstGLShader *shader;
  GLuint bumpmap;
  gint bumpmap_width;
  gint bumpmap_height;
  gchar *location;
  GLfloat xrot, yrot, zrot;
};

struct _GstGLBumperClass
{
  GstGLFilterClass filter_class;
};

GType gst_gl_bumper_get_type (void);

G_END_DECLS

#endif /* _GST_GLBUMPER_H_ */
