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

#ifndef _GST_GL_FILTERCUBE_H_
#define _GST_GL_FILTERCUBE_H_

#include <gst/gl/gstglfilter.h>
#include <gst/gl/gstglfuncs.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_FILTER_CUBE            (gst_gl_filter_cube_get_type())
#define GST_GL_FILTER_CUBE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_FILTER_CUBE,GstGLFilterCube))
#define GST_IS_GL_FILTER_CUBE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_FILTER_CUBE))
#define GST_GL_FILTER_CUBE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_FILTER_CUBE,GstGLFilterCubeClass))
#define GST_IS_GL_FILTER_CUBE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_FILTER_CUBE))
#define GST_GL_FILTER_CUBE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_FILTER_CUBE,GstGLFilterCubeClass))

typedef struct _GstGLFilterCube GstGLFilterCube;
typedef struct _GstGLFilterCubeClass GstGLFilterCubeClass;

struct _GstGLFilterCube
{
    GstGLFilter filter;

    GstGLShader *shader;
    GstGLMemory *in_tex;

    /* background color */
    gfloat red;
    gfloat green;
    gfloat blue;

    /* perspective */
    gdouble fovy;
    gdouble aspect;
    gdouble znear;
    gdouble zfar;

    GLuint             vao;
    GLuint             vbo_indices;
    GLuint             vertex_buffer;
    GLint              attr_position;
    GLint              attr_texture;

    GLfloat            xrot, yrot, zrot;
};

struct _GstGLFilterCubeClass
{
    GstGLFilterClass filter_class;
};

GType gst_gl_filter_cube_get_type (void);

G_END_DECLS

#endif /* _GST_GLFILTERCUBE_H_ */
