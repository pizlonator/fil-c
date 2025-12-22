/*
 * GStreamer
 * Copyright (C) <2010-2015> Luis de Bethencourt <luis@debethencourt.com>
 *
 * Dilate - dilated eye video effect.
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
 * SECTION:element-dilate
 * @title: dilate
 *
 * Dilate adjusts the colors of a video stream in realtime.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! dilate ! videoconvert ! autovideosink
 * ]| This pipeline shows the effect of dilate on a test stream
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gstdilate.h"

GST_DEBUG_CATEGORY_STATIC (gst_dilate_debug);
#define GST_CAT_DEFAULT gst_dilate_debug

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define CAPS_STR GST_VIDEO_CAPS_MAKE ("{  BGRx, RGBx }")
#else
#define CAPS_STR GST_VIDEO_CAPS_MAKE ("{  xBGR, xRGB }")
#endif

#define gst_dilate_parent_class parent_class
G_DEFINE_TYPE (GstDilate, gst_dilate, GST_TYPE_VIDEO_FILTER);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (dilate, "dilate", GST_RANK_NONE,
    GST_TYPE_DILATE, GST_DEBUG_CATEGORY_INIT (gst_dilate_debug, "dilate", 0,
        "Template dilate"));

/* Filter signals and args. */
enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_ERODE,
};

/* Initializations */

#define DEFAULT_ERODE FALSE

static void transform (guint32 * src, guint32 * dest, gint video_area,
    gint width, gint height, gboolean erode);
static inline guint32 get_luminance (guint32 in);

/* The capabilities of the inputs and outputs. */

static GstStaticPadTemplate gst_dilate_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static GstStaticPadTemplate gst_dilate_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static void gst_dilate_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dilate_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_dilate_finalize (GObject * object);

static GstFlowReturn gst_dilate_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame);

/* GObject vmethod implementations */

/* Initialize the dilate's class. */
static void
gst_dilate_class_init (GstDilateClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstVideoFilterClass *vfilter_class = (GstVideoFilterClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "Dilate",
      "Filter/Effect/Video",
      "Dilate copies the brightest pixel around.",
      "Luis de Bethencourt <luis@debethencourt.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_dilate_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_dilate_src_template);

  gobject_class->set_property = gst_dilate_set_property;
  gobject_class->get_property = gst_dilate_get_property;
  gobject_class->finalize = gst_dilate_finalize;

  g_object_class_install_property (gobject_class, PROP_ERODE,
      g_param_spec_boolean ("erode", "Erode", "Erode parameter", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

  vfilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_dilate_transform_frame);
}

/* Initialize the element,
 * instantiate pads and add them to element,
 * set pad callback functions, and
 * initialize instance structure.
 */
static void
gst_dilate_init (GstDilate * filter)
{
  filter->erode = DEFAULT_ERODE;
}

static void
gst_dilate_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDilate *filter = GST_DILATE (object);

  switch (prop_id) {
    case PROP_ERODE:
      filter->erode = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dilate_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDilate *filter = GST_DILATE (object);

  GST_OBJECT_LOCK (filter);
  switch (prop_id) {
    case PROP_ERODE:
      g_value_set_boolean (value, filter->erode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (filter);
}

static void
gst_dilate_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* GstElement vmethod implementations */

/* Actual processing. */
static GstFlowReturn
gst_dilate_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
  GstDilate *filter = GST_DILATE (vfilter);
  gint video_size, width, height;
  gboolean erode;
  guint32 *src, *dest;
  GstClockTime timestamp;
  gint64 stream_time;

  src = GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0);
  dest = GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0);

  width = GST_VIDEO_FRAME_WIDTH (in_frame);
  height = GST_VIDEO_FRAME_HEIGHT (in_frame);

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
  erode = filter->erode;
  GST_OBJECT_UNLOCK (filter);

  video_size = width * height;
  transform (src, dest, video_size, width, height, erode);

  return GST_FLOW_OK;
}

/*** Now the image processing work.... ***/

/* Return luminance of the color */
static inline guint32
get_luminance (guint32 in)
{
  guint32 red, green, blue, luminance;

  red = (in >> 16) & 0xff;
  green = (in >> 8) & 0xff;
  blue = (in) & 0xff;

  luminance = ((90 * red) + (115 * green) + (51 * blue));

  return luminance;
}

/* Transform processes each frame. */
static void
transform (guint32 * src, guint32 * dest, gint video_area, gint width,
    gint height, gboolean erode)
{
  guint32 out_luminance, down_luminance, right_luminance;
  guint32 up_luminance, left_luminance;

  guint32 *src_end = src + video_area;
  guint32 *up;
  guint32 *left;
  guint32 *down;
  guint32 *right;

  while (src != src_end) {
    guint32 *src_line_start = src;
    guint32 *src_line_end = src + width;

    while (src != src_line_end) {
      up = src - width;
      if (up < src) {
        up = src;
      }

      left = src - 1;
      if (left < src_line_start) {
        left = src;
      }

      down = src + width;
      if (down >= src_end) {
        down = src;
      }

      right = src + 1;
      if (right >= src_line_end) {
        right = src;
      }

      *dest = *src;
      out_luminance = get_luminance (*src);

      down_luminance = get_luminance (*down);
      if ((erode && down_luminance < out_luminance) ||
          (!erode && down_luminance > out_luminance)) {
        *dest = *down;
        out_luminance = down_luminance;
      }

      right_luminance = get_luminance (*right);
      if ((erode && right_luminance < out_luminance) ||
          (!erode && right_luminance > out_luminance)) {
        *dest = *right;
        out_luminance = right_luminance;
      }

      up_luminance = get_luminance (*up);
      if ((erode && up_luminance < out_luminance) ||
          (!erode && up_luminance > out_luminance)) {
        *dest = *up;
        out_luminance = up_luminance;
      }

      left_luminance = get_luminance (*left);
      if ((erode && left_luminance < out_luminance) ||
          (!erode && left_luminance > out_luminance)) {
        *dest = *left;
      }

      src += 1;
      dest += 1;
    }
  }
}
