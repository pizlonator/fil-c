/* GStreamer
 * Copyright (C) <2006> Wim Taymans <wim@fluendo.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-videoanalyse
 * @title: videoanalyse
 *
 * This plugin analyses every video frame and if the #GstVideoAnalyse:message
 * property is %TRUE, posts an element message with video statistics called
 * `GstVideoAnalyse`.
 *
 * The message's structure contains these fields:
 *
 * * #GstClockTime `timestamp`: the timestamp of the buffer that triggered the message.
 *
 * * #GstClockTime `stream-time`: the stream time of the buffer.
 *
 * * #GstClockTime `running-time`: the running_time of the buffer.
 *
 * * #GstClockTime`duration`:the duration of the buffer.
 *
 * * #gdouble`luma-average`: the average brightness of the frame. Range: 0.0-1.0
 *
 * * #gdouble`luma-variance`: the brightness variance of the frame.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -m videotestsrc ! videoanalyse ! videoconvert ! ximagesink
 * ]| This pipeline emits messages to the console for each frame that has been analysed.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include "gstvideoanalyse.h"

GST_DEBUG_CATEGORY_STATIC (gst_video_analyse_debug_category);
#define GST_CAT_DEFAULT gst_video_analyse_debug_category

/* prototypes */


static void gst_video_analyse_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_video_analyse_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_video_analyse_finalize (GObject * object);

static GstFlowReturn gst_video_analyse_transform_frame_ip (GstVideoFilter *
    filter, GstVideoFrame * frame);

enum
{
  PROP_0,
  PROP_MESSAGE
};

#define DEFAULT_MESSAGE TRUE

#define VIDEO_CAPS \
    GST_VIDEO_CAPS_MAKE("{ I420, YV12, Y444, Y42B, Y41B }")


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVideoAnalyse, gst_video_analyse,
    GST_TYPE_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_video_analyse_debug_category, "videoanalyse",
        0, "debug category for videoanalyse element"));
GST_ELEMENT_REGISTER_DEFINE (videoanalyse, "videoanalyse",
    GST_RANK_NONE, GST_TYPE_VIDEO_ANALYSE);

static void
gst_video_analyse_class_init (GstVideoAnalyseClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Video analyser", "Filter/Analyzer/Video",
      "Analyse video signal", "Wim Taymans <wim@fluendo.com>");

  gobject_class->set_property = gst_video_analyse_set_property;
  gobject_class->get_property = gst_video_analyse_get_property;
  gobject_class->finalize = gst_video_analyse_finalize;
  video_filter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_video_analyse_transform_frame_ip);

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_MESSAGE,
      g_param_spec_boolean ("message", "Message",
          "Post statics messages",
          DEFAULT_MESSAGE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  //trans_class->passthrough_on_same_caps = TRUE;
}

static void
gst_video_analyse_init (GstVideoAnalyse * videoanalyse)
{
}

void
gst_video_analyse_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoAnalyse *videoanalyse = GST_VIDEO_ANALYSE (object);

  GST_DEBUG_OBJECT (videoanalyse, "set_property");

  switch (property_id) {
    case PROP_MESSAGE:
      videoanalyse->message = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_video_analyse_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVideoAnalyse *videoanalyse = GST_VIDEO_ANALYSE (object);

  GST_DEBUG_OBJECT (videoanalyse, "get_property");

  switch (property_id) {
    case PROP_MESSAGE:
      g_value_set_boolean (value, videoanalyse->message);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_video_analyse_finalize (GObject * object)
{
  GstVideoAnalyse *videoanalyse = GST_VIDEO_ANALYSE (object);

  GST_DEBUG_OBJECT (videoanalyse, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_video_analyse_parent_class)->finalize (object);
}

static void
gst_video_analyse_post_message (GstVideoAnalyse * videoanalyse,
    GstVideoFrame * frame)
{
  GstBaseTransform *trans;
  GstMessage *m;
  guint64 duration, timestamp, running_time, stream_time;

  trans = GST_BASE_TRANSFORM_CAST (videoanalyse);

  /* get timestamps */
  timestamp = GST_BUFFER_TIMESTAMP (frame->buffer);
  duration = GST_BUFFER_DURATION (frame->buffer);
  running_time = gst_segment_to_running_time (&trans->segment, GST_FORMAT_TIME,
      timestamp);
  stream_time = gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME,
      timestamp);

  m = gst_message_new_element (GST_OBJECT_CAST (videoanalyse),
      gst_structure_new ("GstVideoAnalyse",
          "timestamp", G_TYPE_UINT64, timestamp,
          "stream-time", G_TYPE_UINT64, stream_time,
          "running-time", G_TYPE_UINT64, running_time,
          "duration", G_TYPE_UINT64, duration,
          "luma-average", G_TYPE_DOUBLE, videoanalyse->luma_average,
          "luma-variance", G_TYPE_DOUBLE, videoanalyse->luma_variance, NULL));

  gst_element_post_message (GST_ELEMENT_CAST (videoanalyse), m);
}

static void
gst_video_analyse_planar (GstVideoAnalyse * videoanalyse, GstVideoFrame * frame)
{
  guint64 sum;
  gint avg, diff;
  gint i, j;
  guint8 *d;
  gint width = frame->info.width;
  gint height = frame->info.height;
  gint stride;

  d = frame->data[0];
  stride = frame->info.stride[0];
  sum = 0;
  /* do brightness as average of pixel brightness in 0.0 to 1.0 */
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      sum += d[j];
    }
    d += stride;
  }
  avg = sum / (width * height);
  videoanalyse->luma_average = sum / (255.0 * width * height);

  d = frame->data[0];
  stride = frame->info.stride[0];
  sum = 0;
  /* do variance */
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      diff = (avg - d[j]);
      sum += diff * diff;
    }
    d += stride;
  }
  videoanalyse->luma_variance = sum / (255.0 * 255.0 * width * height);
}

static GstFlowReturn
gst_video_analyse_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame)
{
  GstVideoAnalyse *videoanalyse = GST_VIDEO_ANALYSE (filter);

  GST_DEBUG_OBJECT (videoanalyse, "transform_frame_ip");

  gst_video_analyse_planar (videoanalyse, frame);

  if (videoanalyse->message)
    gst_video_analyse_post_message (videoanalyse, frame);

  return GST_FLOW_OK;
}
