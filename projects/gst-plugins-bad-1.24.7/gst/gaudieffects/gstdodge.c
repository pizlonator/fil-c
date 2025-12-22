/*
 * GStreamer
 * Copyright (C) <2010-2015> Luis de Bethencourt <luis@debethencourt.com>
 *
 * Dodge - saturation video effect.
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
 * SECTION:element-dodge
 * @title: dodge
 *
 * Dodge saturates the colors of a video stream in realtime.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! dodge ! videoconvert ! autovideosink
 * ]| This pipeline shows the effect of dodge on a test stream
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gstdodge.h"

GST_DEBUG_CATEGORY_STATIC (gst_dodge_debug);
#define GST_CAT_DEFAULT gst_dodge_debug

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define CAPS_STR GST_VIDEO_CAPS_MAKE ("{  BGRx, RGBx }")
#else
#define CAPS_STR GST_VIDEO_CAPS_MAKE ("{  xBGR, xRGB }")
#endif

#define gst_dodge_parent_class parent_class
G_DEFINE_TYPE (GstDodge, gst_dodge, GST_TYPE_VIDEO_FILTER);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (dodge, "dodge", GST_RANK_NONE,
    GST_TYPE_DODGE, GST_DEBUG_CATEGORY_INIT (gst_dodge_debug, "dodge", 0,
        "Template dodge"));

/* Filter signals and args. */
enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
};

/* Initializations */

static void transform (guint32 * src, guint32 * dest, gint video_area);

/* The capabilities of the inputs and outputs. */

static GstStaticPadTemplate gst_dodge_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static GstStaticPadTemplate gst_dodge_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static void gst_dodge_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dodge_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_dodge_finalize (GObject * object);

static GstFlowReturn gst_dodge_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame);

/* GObject vmethod implementations */

/* Initialize the dodge's class. */
static void
gst_dodge_class_init (GstDodgeClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstVideoFilterClass *vfilter_class = (GstVideoFilterClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "Dodge",
      "Filter/Effect/Video",
      "Dodge saturates the colors in the video signal.",
      "Luis de Bethencourt <luis@debethencourt.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_dodge_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_dodge_src_template);

  gobject_class->set_property = gst_dodge_set_property;
  gobject_class->get_property = gst_dodge_get_property;
  gobject_class->finalize = gst_dodge_finalize;

  vfilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_dodge_transform_frame);
}

/* Initialize the element,
 * instantiate pads and add them to element,
 * set pad callback functions, and
 * initialize instance structure.
 */
static void
gst_dodge_init (GstDodge * filter)
{
}

static void
gst_dodge_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dodge_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dodge_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* GstElement vmethod implementations */

/* Actual processing. */
static GstFlowReturn
gst_dodge_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
  GstDodge *filter = GST_DODGE (vfilter);
  guint32 *src, *dest;
  gint video_size;

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

  video_size = GST_VIDEO_FRAME_WIDTH (in_frame) *
      GST_VIDEO_FRAME_HEIGHT (in_frame);

  transform (src, dest, video_size);

  return GST_FLOW_OK;
}

/*** Now the image processing work.... ***/

/* Transform processes each frame. */
static void
transform (guint32 * src, guint32 * dest, gint video_area)
{
  guint32 in;
  gint x, red, green, blue;

  for (x = 0; x < video_area; x++) {
    in = *src++;

    red = (in >> 16) & 0xff;
    green = (in >> 8) & 0xff;
    blue = (in) & 0xff;

    red = (256 * red) / (256 - red);
    green = (256 * green) / (256 - green);
    blue = (256 * blue) / (256 - blue);

    red = CLAMP (red, 0, 255);
    green = CLAMP (green, 0, 255);
    blue = CLAMP (blue, 0, 255);

    *dest++ = (red << 16) | (green << 8) | blue;
  }
}
