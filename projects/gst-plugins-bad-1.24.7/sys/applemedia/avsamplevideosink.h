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

#ifndef _AV_SINK_H_
#define _AV_SINK_H_

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>

#include <QuartzCore/QuartzCore.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <AVFoundation/AVFoundation.h>

G_BEGIN_DECLS

#define GST_TYPE_AV_SAMPLE_VIDEO_SINK \
    (gst_av_sample_video_sink_get_type())
#define GST_AV_SAMPLE_VIDEO_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AV_SAMPLE_VIDEO_SINK,GstAVSampleVideoSink))
#define GST_AV_SAMPLE_VIDEO_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AV_SAMPLE_VIDEO_SINK,GstAVSampleVideoSinkClass))
#define GST_IS_AV_SAMPLE_VIDEO_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AV_SAMPLE_VIDEO_SINK))
#define GST_IS_AV_SAMPLE_VIDEO_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AV_SAMPLE_VIDEO_SINK))
#define GST_AV_SAMPLE_VIDEO_SINK_LAYER(obj) \
    ((__bridge AVSampleBufferDisplayLayer *)(obj->layer))

typedef struct _GstAVSampleVideoSink GstAVSampleVideoSink;
typedef struct _GstAVSampleVideoSinkClass GstAVSampleVideoSinkClass;

struct _GstAVSampleVideoSink
{
    GstVideoSink video_sink;

    /* NOTE: ARC no longer allows Objective-C pointers in structs. */
    /* Instead, use gpointer with explicit __bridge_* calls */
    gpointer layer;

    GstVideoInfo info;

    gboolean keep_aspect_ratio;

    GstBufferPool *pool;

    gboolean layer_requesting_data;

    GMutex render_lock;
    GstBuffer *buffer;
    GstFlowReturn render_flow_return;
};

struct _GstAVSampleVideoSinkClass
{
    GstVideoSinkClass video_sink_class;
};

GType gst_av_sample_video_sink_get_type(void);

G_END_DECLS

#endif
