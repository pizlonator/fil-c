/*
 * GStreamer
 * Copyright (C) 2014 Lubosz Sarnecki <lubosz@gmail.com>
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

#ifndef _GST_GL_TRANSFORMATION_H_
#define _GST_GL_TRANSFORMATION_H_

#include <gst/gl/gstglfilter.h>
#include <gst/gl/gstglfuncs.h>
#include <graphene.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_TRANSFORMATION            (gst_gl_transformation_get_type())
#define GST_GL_TRANSFORMATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_TRANSFORMATION,GstGLTransformation))
#define GST_IS_GL_TRANSFORMATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_TRANSFORMATION))
#define GST_GL_TRANSFORMATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_TRANSFORMATION,GstGLTransformationClass))
#define GST_IS_GL_TRANSFORMATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_TRANSFORMATION))
#define GST_GL_TRANSFORMATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_TRANSFORMATION,GstGLTransformationClass))

typedef struct _GstGLTransformation GstGLTransformation;
typedef struct _GstGLTransformationClass GstGLTransformationClass;

struct _GstGLTransformation
{
    GstGLFilter filter;

    GstGLShader *shader;
    GLuint       vao;
    GLuint       vbo_indices;
    GLuint       vertex_buffer;
    GLint        attr_position;
    GLint        attr_texture;

    GstGLMemory *in_tex;
    GstGLMemory *out_tex;

    gfloat xrotation;
    gfloat yrotation;
    gfloat zrotation;

    gfloat xscale;
    gfloat yscale;

    gfloat xtranslation;
    gfloat ytranslation;
    gfloat ztranslation;

    gfloat xpivot;
    gfloat ypivot;
    gfloat zpivot;

    /* perspective */
    gfloat fov;
    gfloat aspect;
    gfloat znear;
    gfloat zfar;
    gboolean ortho;

    graphene_matrix_t *model_matrix;
    graphene_matrix_t *view_matrix;
    graphene_matrix_t *projection_matrix;
    graphene_matrix_t *inv_model_matrix;
    graphene_matrix_t *inv_view_matrix;
    graphene_matrix_t *inv_projection_matrix;
    graphene_matrix_t *mvp_matrix;

    graphene_vec3_t *camera_position;

    gboolean downstream_supports_affine_meta;
    gboolean caps_change;
};

struct _GstGLTransformationClass
{
    GstGLFilterClass filter_class;
};

GType gst_gl_transformation_get_type (void);

G_END_DECLS

#endif /* _GST_GL_TRANSFORMATION_H_ */
