/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#ifndef __CA_OPENGL_LAYER_SINK_H__
#define __CA_OPENGL_LAYER_SINK_H__

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>

#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>
#include <gst/gl/cocoa/gstglcaopengllayer.h>

G_BEGIN_DECLS

#define GST_TYPE_CA_OPENGL_LAYER_SINK \
    (gst_ca_opengl_layer_sink_get_type())
#define GST_CA_OPENGL_LAYER_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CA_OPENGL_LAYER_SINK,GstCAOpenGLLayerSink))
#define GST_CA_OPENGL_LAYER_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CA_OPENGL_LAYER_SINK,GstCAOpenGLLayerSinkClass))
#define GST_IS_CA_OPENGL_LAYER_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CA_OPENGL_LAYER_SINK))
#define GST_IS_CA_OPENGL_LAYER_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CA_OPENGL_LAYER_SINK))

typedef struct _GstCAOpenGLLayerSink GstCAOpenGLLayerSink;
typedef struct _GstCAOpenGLLayerSinkClass GstCAOpenGLLayerSinkClass;

struct _GstCAOpenGLLayerSink
{
    GstVideoSink video_sink;

    /* caps */
    GstVideoInfo info;
    GstCaps *gl_caps;

    /* gl state */
    GstGLDisplay *display;
    GstGLContext *other_context;
    GstGLContext *context;

    guint      next_tex;
    GstBuffer *next_buffer;
    GstBuffer *next_sync;

    gpointer layer;

    gboolean keep_aspect_ratio;

    /* avoid replacing the stored_buffer while drawing */
    GMutex drawing_lock;
    GstBuffer *stored_buffer;
    GstBuffer *stored_sync;
    GLuint redisplay_texture;

    gboolean caps_change;
    guint window_width;
    guint window_height;

    /* gl state */
    GstGLShader *redisplay_shader;
    GLuint vao;
    GLuint vertex_buffer;
    GLuint vbo_indices;
    GLint  attr_position;
    GLint  attr_texture;
};

struct _GstCAOpenGLLayerSinkClass
{
    GstVideoSinkClass video_sink_class;
};

GType gst_ca_opengl_layer_sink_get_type(void);
GType gst_ca_opengl_layer_sink_bin_get_type (void);

G_END_DECLS

#endif /* __CA_OPENGL_LAYER_SINK__ */
