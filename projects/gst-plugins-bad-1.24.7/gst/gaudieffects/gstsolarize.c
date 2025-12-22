/*
 * GStreamer
 * Copyright (C) <2010-2015> Luis de Bethencourt <luis@debethencourt.com>
 *
 * Solarize - curve adjustment video effect.
 * Based on Pete Warden's FreeFrame plugin with the same name.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

/**
 * SECTION:element-solarize
 * @title: solarize
 *
 * Solarize does a smart inverse in a video stream in realtime.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! solarize ! videoconvert ! autovideosink
 * ]| This pipeline shows the effect of solarize on a test stream
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gstsolarize.h"

GST_DEBUG_CATEGORY_STATIC (gst_solarize_debug);
#define GST_CAT_DEFAULT gst_solarize_debug

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define CAPS_STR GST_VIDEO_CAPS_MAKE ("{  BGRx, RGBx }")
#else
#define CAPS_STR GST_VIDEO_CAPS_MAKE ("{  xBGR, xRGB }")
#endif

/* Filter signals and args. */
enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0 = 0,
  PROP_THRESHOLD,
  PROP_START,
  PROP_END,
};

/* Initializations */

#define DEFAULT_THRESHOLD 127
#define DEFAULT_START 50
#define DEFAULT_END 185

static void transform (guint32 * src, guint32 * dest, gint video_area,
    gint threshold, gint start, gint end);

/* The capabilities of the inputs and outputs. */

static GstStaticPadTemplate gst_solarize_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static GstStaticPadTemplate gst_solarize_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

#define gst_solarize_parent_class parent_class
G_DEFINE_TYPE (GstSolarize, gst_solarize, GST_TYPE_VIDEO_FILTER);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (solarize, "solarize", GST_RANK_NONE,
    GST_TYPE_SOLARIZE, GST_DEBUG_CATEGORY_INIT (gst_solarize_debug, "solarize",
        0, "Template solarize"));

static void gst_solarize_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_solarize_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_solarize_finalize (GObject * object);

static GstFlowReturn gst_solarize_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame);

/* GObject vmethod implementations */

/* Initialize the solarize's class. */
static void
gst_solarize_class_init (GstSolarizeClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstVideoFilterClass *vfilter_class = (GstVideoFilterClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "Solarize",
      "Filter/Effect/Video",
      "Solarize tunable inverse in the video signal.",
      "Luis de Bethencourt <luis@debethencourt.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_solarize_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_solarize_src_template);

  gobject_class->set_property = gst_solarize_set_property;
  gobject_class->get_property = gst_solarize_get_property;
  gobject_class->finalize = gst_solarize_finalize;

  g_object_class_install_property (gobject_class, PROP_THRESHOLD,
      g_param_spec_uint ("threshold", "Threshold",
          "Threshold parameter", 0, 256, DEFAULT_THRESHOLD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

  g_object_class_install_property (gobject_class, PROP_START,
      g_param_spec_uint ("start", "Start",
          "Start parameter", 0, 256, DEFAULT_START,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

  g_object_class_install_property (gobject_class, PROP_END,
      g_param_spec_uint ("end", "End",
          "End parameter", 0, 256, DEFAULT_END,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

  vfilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_solarize_transform_frame);
}

/* Initialize the element,
 * instantiate pads and add them to element,
 * set pad callback functions, and
 * initialize instance structure.
 */
static void
gst_solarize_init (GstSolarize * filter)
{
  filter->threshold = DEFAULT_THRESHOLD;
  filter->start = DEFAULT_START;
  filter->end = DEFAULT_END;
}

static void
gst_solarize_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSolarize *filter = GST_SOLARIZE (object);

  switch (prop_id) {
    case PROP_THRESHOLD:
      filter->threshold = g_value_get_uint (value);
      break;
    case PROP_START:
      filter->start = g_value_get_uint (value);
      break;
    case PROP_END:
      filter->end = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_solarize_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSolarize *filter = GST_SOLARIZE (object);

  GST_OBJECT_LOCK (filter);
  switch (prop_id) {
    case PROP_THRESHOLD:
      g_value_set_uint (value, filter->threshold);
      break;
    case PROP_START:
      g_value_set_uint (value, filter->start);
      break;
    case PROP_END:
      g_value_set_uint (value, filter->end);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (filter);
}

static void
gst_solarize_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* GstElement vmethod implementations */

/* Actual processing. */
static GstFlowReturn
gst_solarize_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
  GstSolarize *filter = GST_SOLARIZE (vfilter);
  gint video_size, threshold, start, end;
  guint32 *src, *dest;
  GstClockTime timestamp;
  gint64 stream_time;

  src = GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0);
  dest = GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0);

  /* GstController: update the properties */
  timestamp = GST_BUFFER_TIMESTAMP (in_frame->buffer);
  stream_time =
      gst_segment_to_stream_time (&GST_BASE_TRANSFORM (filter)->segment,
      GST_FORMAT_TIME, timestamp);

  GST_DEBUG_OBJECT (filter, "sync to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (filter), stream_time);

  GST_OBJECT_LOCK (filter);
  threshold = filter->threshold;
  start = filter->start;
  end = filter->end;
  GST_OBJECT_UNLOCK (filter);

  video_size = GST_VIDEO_FRAME_WIDTH (in_frame) *
      GST_VIDEO_FRAME_HEIGHT (in_frame);

  transform (src, dest, video_size, threshold, start, end);

  return GST_FLOW_OK;
}

/*** Now the image processing work.... ***/

/* Transform processes each frame. */
static void
transform (guint32 * src, guint32 * dest, gint video_area,
    gint threshold, gint start, gint end)
{
  guint32 in;
  guint32 color[3];
  gint period = 1, up_length = 1, down_length = 1;
  gint x, c;
  gint param;
  static const guint ceiling = 255;

  if (end != start)
    period = end - start;

  if (threshold != start)
    up_length = threshold - start;

  if (threshold != end)
    down_length = end - threshold;

  /* Loop through pixels. */
  for (x = 0; x < video_area; x++) {
    in = *src++;

    color[0] = (in >> 16) & 0xff;
    color[1] = (in >> 8) & 0xff;
    color[2] = (in) & 0xff;

    /* Loop through colors. */
    for (c = 0; c < 3; c++) {
      param = color[c];
      param += 256;
      param -= start;
      param %= period;

      if (param < up_length) {
        color[c] = param * ceiling;
        color[c] /= up_length;
      } else {
        color[c] = down_length - (param - up_length);
        color[c] *= ceiling;
        color[c] /= down_length;
      }
    }

    /* Clamp colors */
    for (c = 0; c < 3; c++) {
      if (G_UNLIKELY (color[c] > 255))
        color[c] = 255;
    }

    *dest++ = (color[0] << 16) | (color[1] << 8) | color[2];
  }
}
