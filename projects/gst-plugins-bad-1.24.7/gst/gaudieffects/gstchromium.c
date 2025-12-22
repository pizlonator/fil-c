/*
 * GStreamer
 * Copyright (C) <2010> Luis de Bethencourt <luis@debethencourt.com>
 *
 * Chromium - burning chrome video effect.
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
 * SECTION:element-chromium
 * @title: chromium
 *
 * Chromium breaks the colors of a video stream in realtime.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! chromium ! videoconvert ! autovideosink
 * ]| This pipeline shows the effect of chromium on a test stream
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <math.h>
#include <gst/gst.h>

#include "gstchromium.h"

GST_DEBUG_CATEGORY_STATIC (gst_chromium_debug);
#define GST_CAT_DEFAULT gst_chromium_debug

#define gst_chromium_parent_class parent_class
G_DEFINE_TYPE (GstChromium, gst_chromium, GST_TYPE_VIDEO_FILTER);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (chromium, "chromium", GST_RANK_NONE,
    GST_TYPE_CHROMIUM, GST_DEBUG_CATEGORY_INIT (gst_chromium_debug, "chromium",
        0, "Template chromium"));

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
  PROP_EDGE_A,
  PROP_EDGE_B,
};

/* Initializations */

#define DEFAULT_EDGE_A 200
#define DEFAULT_EDGE_B 1

const float pi = 3.141582f;

gint cosTablePi = 512;
gint cosTableTwoPi = (2 * 512);
gint cosTableOne = 512;
gint cosTableMask = 1023;

gint cosTable[2 * 512];

void setup_cos_table (void);
static gint cos_from_table (int angle);
static inline int abs_int (int val);
static void transform (guint32 * src, guint32 * dest, gint video_area,
    gint edge_a, gint edge_b);

/* The capabilities of the inputs and outputs. */

static GstStaticPadTemplate gst_chromium_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static GstStaticPadTemplate gst_chromium_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static void gst_chromium_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_chromium_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_chromium_finalize (GObject * object);

static GstFlowReturn gst_chromium_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame);

/* GObject vmethod implementations */

/* Initialize the chromium's class. */
static void
gst_chromium_class_init (GstChromiumClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstVideoFilterClass *vfilter_class = (GstVideoFilterClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class, "Chromium",
      "Filter/Effect/Video",
      "Chromium breaks the colors of the video signal.",
      "Luis de Bethencourt <luis@debethencourt.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_chromium_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_chromium_src_template);

  gobject_class->set_property = gst_chromium_set_property;
  gobject_class->get_property = gst_chromium_get_property;
  gobject_class->finalize = gst_chromium_finalize;

  g_object_class_install_property (gobject_class, PROP_EDGE_A,
      g_param_spec_uint ("edge-a", "Edge A",
          "First edge parameter", 0, 256, DEFAULT_EDGE_A,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

  g_object_class_install_property (gobject_class, PROP_EDGE_B,
      g_param_spec_uint ("edge-b", "Edge B",
          "Second edge parameter", 0, 256, DEFAULT_EDGE_B,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

  vfilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_chromium_transform_frame);
}

/* Initialize the element,
 * instantiate pads and add them to element,
 * set pad callback functions, and
 * initialize instance structure.
 */
static void
gst_chromium_init (GstChromium * filter)
{
  filter->edge_a = DEFAULT_EDGE_A;
  filter->edge_b = DEFAULT_EDGE_B;

  setup_cos_table ();
}

static void
gst_chromium_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstChromium *filter = GST_CHROMIUM (object);

  switch (prop_id) {
    case PROP_EDGE_A:
      filter->edge_a = g_value_get_uint (value);
      break;
    case PROP_EDGE_B:
      filter->edge_b = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_chromium_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstChromium *filter = GST_CHROMIUM (object);

  GST_OBJECT_LOCK (filter);
  switch (prop_id) {
    case PROP_EDGE_A:
      g_value_set_uint (value, filter->edge_a);
      break;
    case PROP_EDGE_B:
      g_value_set_uint (value, filter->edge_b);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (filter);
}

static void
gst_chromium_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* GstElement vmethod implementations */

/* Actual processing. */
static GstFlowReturn
gst_chromium_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
  GstChromium *filter = GST_CHROMIUM (vfilter);
  gint video_size, edge_a, edge_b;
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
  edge_a = filter->edge_a;
  edge_b = filter->edge_b;
  GST_OBJECT_UNLOCK (filter);

  video_size = GST_VIDEO_FRAME_WIDTH (in_frame) *
      GST_VIDEO_FRAME_HEIGHT (in_frame);
  transform (src, dest, video_size, edge_a, edge_b);

  return GST_FLOW_OK;
}

/*** Now the image processing work.... ***/
/* Set up the cosine table. */
void
setup_cos_table (void)
{
  int angle;

  for (angle = 0; angle < cosTableTwoPi; ++angle) {
    float angleInRadians = ((float) (angle) / cosTablePi) * pi;
    cosTable[angle] = (int) (cos (angleInRadians) * cosTableOne);
  }
}

/* Keep the values absolute. */
static inline int
abs_int (int val)
{
  if (val > 0) {
    return val;
  } else {
    return -val;
  }
}

/* Cosine from Table. */
static gint
cos_from_table (int angle)
{
  angle &= cosTableMask;
  return cosTable[angle];
}

/* Transform processes each frame. */
static void
transform (guint32 * src, guint32 * dest, gint video_area,
    gint edge_a, gint edge_b)
{
  guint32 in;
  gint x, red, green, blue;

  for (x = 0; x < video_area; x++) {
    in = *src++;

    red = (in >> 16) & 0xff;
    green = (in >> 8) & 0xff;
    blue = (in) & 0xff;

    red = abs_int (cos_from_table ((red + edge_a) + ((red * edge_b) / 2)));
    green = abs_int (cos_from_table (
            (green + edge_a) + ((green * edge_b) / 2)));
    blue = abs_int (cos_from_table ((blue + edge_a) + ((blue * edge_b) / 2)));

    red = CLAMP (red, 0, 255);
    green = CLAMP (green, 0, 255);
    blue = CLAMP (blue, 0, 255);

    *dest++ = (red << 16) | (green << 8) | blue;
  }
}
