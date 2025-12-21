/*
 * GStreamer
 * Copyright (C) 2003 Julien Moutte <julien@moutte.net>
 * Copyright (C) 2005,2006,2007 David A. Schleef <ds@schleef.org>
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

#ifndef _GLIMAGESINK_H_
#define _GLIMAGESINK_H_

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>

#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN (gst_debug_glimage_sink);

#define GST_TYPE_GLIMAGE_SINK \
    (gst_glimage_sink_get_type())
#define GST_GLIMAGE_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GLIMAGE_SINK,GstGLImageSink))
#define GST_GLIMAGE_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GLIMAGE_SINK,GstGLImageSinkClass))
#define GST_IS_GLIMAGE_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GLIMAGE_SINK))
#define GST_IS_GLIMAGE_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GLIMAGE_SINK))

typedef struct _GstGLImageSink GstGLImageSink;
typedef struct _GstGLImageSinkClass GstGLImageSinkClass;

struct _GstGLImageSink
{
    GstVideoSink video_sink;

    guintptr window_id;
    guintptr new_window_id;
    gulong mouse_sig_id;
    gulong key_sig_id;
    gulong mouse_scroll_sig_id;

    /* GstVideoOverlay::set_render_rectangle() cache */
    gint x;
    gint y;
    gint width;
    gint height;

    /* Input info before 3d stereo output conversion, if any */
    GstVideoInfo in_info;
    GstCaps *in_caps;

    /* format/caps we actually hand off to the app */
    GstVideoInfo out_info;
    GstCaps *out_caps;
    GstGLTextureTarget texture_target;

    GstGLDisplay *display;
    GstGLContext *context;
    GstGLContext *other_context;
    gboolean handle_events;
    gboolean ignore_alpha;

    GstGLViewConvert *convert_views;

    /* Original input RGBA buffer, ready for display,
     * or possible reconversion through the views filter */
    GstBuffer *input_buffer;
    /* Secondary view buffer - when operating in frame-by-frame mode */
    GstBuffer *input_buffer2;

    guint      next_tex;
    GstBuffer *next_buffer;
    GstBuffer *next_buffer2; /* frame-by-frame 2nd view */
    GstBuffer *next_sync;
    GstGLSyncMeta *next_sync_meta;

    gint to_quit;
    gboolean keep_aspect_ratio;
    gint par_n, par_d;

    /* avoid replacing the stored_buffer while drawing */
    GMutex drawing_lock;
    GstBuffer *stored_buffer[2];
    GstBuffer *stored_sync;
    GstGLSyncMeta *stored_sync_meta;
    GLuint redisplay_texture;

    /* protected with drawing_lock */
    gboolean window_resized;
    guint window_width;
    guint window_height;

    GstVideoRectangle display_rect;

    GstGLShader *redisplay_shader;
    GLuint vao;
    GLuint vbo_indices;
    GLuint vertex_buffer;
    GLint  attr_position;
    GLint  attr_texture;

    GstVideoMultiviewMode mview_output_mode;
    GstVideoMultiviewFlags mview_output_flags;
    gboolean output_mode_changed;
    GstGLStereoDownmix mview_downmix_mode;

    GstGLOverlayCompositor *overlay_compositor;

    /* current video flip method */
    GstVideoOrientationMethod current_rotate_method;
    GstVideoOrientationMethod rotate_method;
    const gfloat *transform_matrix;
};

struct _GstGLImageSinkClass
{
    GstVideoSinkClass video_sink_class;
};

GType gst_glimage_sink_get_type(void);
GType gst_gl_image_sink_bin_get_type(void);

G_END_DECLS

#endif

