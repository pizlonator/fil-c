/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
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

#ifndef _GST_GL_MOSAIC_H_
#define _GST_GL_MOSAIC_H_

#include <gst/gl/gstglmixer.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_MOSAIC            (gst_gl_mosaic_get_type())
#define GST_GL_MOSAIC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_MOSAIC,GstGLMosaic))
#define GST_IS_GL_MOSAIC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_MOSAIC))
#define GST_GL_MOSAIC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_MOSAIC,GstGLMosaicClass))
#define GST_IS_GL_MOSAIC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_MOSAIC))
#define GST_GL_MOSAIC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_MOSAIC,GstGLMosaicClass))

typedef struct _GstGLMosaic GstGLMosaic;
typedef struct _GstGLMosaicClass GstGLMosaicClass;

struct _GstGLMosaic
{
    GstGLMixer mixer;

    GstGLShader *shader;
    guint vao;
    guint vertex_buffer;
    guint vbo_indices;
    gint attr_position_loc;
    gint attr_texture_loc;
    GstGLMemory *out_tex;
    GLfloat xrot, yrot, zrot;
};

struct _GstGLMosaicClass
{
    GstGLMixerClass mixer_class;
};

GType gst_gl_mosaic_get_type (void);

G_END_DECLS

#endif /* _GST_GLFILTERCUBE_H_ */
