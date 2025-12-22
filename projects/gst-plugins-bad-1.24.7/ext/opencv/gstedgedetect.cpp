/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
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
 * SECTION:element-edgedetect
 *
 * Performs canny edge detection on videos and images
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 videotestsrc ! videoconvert ! edgedetect ! videoconvert ! xvimagesink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstedgedetect.h"
#include <opencv2/imgproc.hpp>

GST_DEBUG_CATEGORY_STATIC (gst_edge_detect_debug);
#define GST_CAT_DEFAULT gst_edge_detect_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_THRESHOLD1,
  PROP_THRESHOLD2,
  PROP_APERTURE,
  PROP_MASK
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB"))
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB"))
    );

G_DEFINE_TYPE_WITH_CODE (GstEdgeDetect, gst_edge_detect,
    GST_TYPE_OPENCV_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_edge_detect_debug, "edgedetect", 0,
        "Performs canny edge detection on videos and images"););
GST_ELEMENT_REGISTER_DEFINE (edgedetect, "edgedetect", GST_RANK_NONE,
    GST_TYPE_EDGE_DETECT);

static void gst_edge_detect_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_edge_detect_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_edge_detect_transform (GstOpencvVideoFilter * filter,
    GstBuffer * buf, cv::Mat img, GstBuffer * outbuf, cv::Mat outimg);
static gboolean gst_edge_detect_set_caps (GstOpencvVideoFilter * transform,
    gint in_width, gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type);

/* Clean up */
static void
gst_edge_detect_finalize (GObject * obj)
{
  GstEdgeDetect *filter = GST_EDGE_DETECT (obj);

  filter->cvGray.release ();
  filter->cvEdge.release ();

  G_OBJECT_CLASS (gst_edge_detect_parent_class)->finalize (obj);
}

/* initialize the edgedetect's class */
static void
gst_edge_detect_class_init (GstEdgeDetectClass * klass)
{
  GObjectClass *gobject_class;
  GstOpencvVideoFilterClass *gstopencvbasefilter_class;

  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  gobject_class = (GObjectClass *) klass;
  gstopencvbasefilter_class = (GstOpencvVideoFilterClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_edge_detect_finalize);
  gobject_class->set_property = gst_edge_detect_set_property;
  gobject_class->get_property = gst_edge_detect_get_property;

  gstopencvbasefilter_class->cv_trans_func = gst_edge_detect_transform;
  gstopencvbasefilter_class->cv_set_caps = gst_edge_detect_set_caps;

  g_object_class_install_property (gobject_class, PROP_MASK,
      g_param_spec_boolean ("mask", "Mask",
          "Sets whether the detected edges should be used as a mask on the original input or not",
          TRUE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_THRESHOLD1,
      g_param_spec_int ("threshold1", "Threshold1",
          "Threshold value for canny edge detection", 0, 1000, 50,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_THRESHOLD2,
      g_param_spec_int ("threshold2", "Threshold2",
          "Second threshold value for canny edge detection", 0, 1000, 150,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_APERTURE,
      g_param_spec_int ("aperture", "Aperture",
          "Aperture size for Sobel operator (Must be either 3, 5 or 7", 3, 7, 3,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "edgedetect",
      "Filter/Effect/Video",
      "Performs canny edge detection on videos and images.",
      "Michael Sheldon <mike@mikeasoft.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_edge_detect_init (GstEdgeDetect * filter)
{
  filter->mask = TRUE;
  filter->threshold1 = 50;
  filter->threshold2 = 150;
  filter->aperture = 3;

  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      FALSE);
}

static void
gst_edge_detect_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstEdgeDetect *filter = GST_EDGE_DETECT (object);

  switch (prop_id) {
    case PROP_MASK:
      filter->mask = g_value_get_boolean (value);
      break;
    case PROP_THRESHOLD1:
      filter->threshold1 = g_value_get_int (value);
      break;
    case PROP_THRESHOLD2:
      filter->threshold2 = g_value_get_int (value);
      break;
    case PROP_APERTURE:
      filter->aperture = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_edge_detect_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstEdgeDetect *filter = GST_EDGE_DETECT (object);

  switch (prop_id) {
    case PROP_MASK:
      g_value_set_boolean (value, filter->mask);
      break;
    case PROP_THRESHOLD1:
      g_value_set_int (value, filter->threshold1);
      break;
    case PROP_THRESHOLD2:
      g_value_set_int (value, filter->threshold2);
      break;
    case PROP_APERTURE:
      g_value_set_int (value, filter->aperture);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_edge_detect_set_caps (GstOpencvVideoFilter * transform,
    gint in_width, gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type)
{
  GstEdgeDetect *filter = GST_EDGE_DETECT (transform);

  filter->cvGray.create (cv::Size (in_width, in_height), CV_8UC1);
  filter->cvEdge.create (cv::Size (in_width, in_height), CV_8UC1);

  return TRUE;
}

static GstFlowReturn
gst_edge_detect_transform (GstOpencvVideoFilter * base, GstBuffer * buf,
    cv::Mat img, GstBuffer * outbuf, cv::Mat outimg)
{
  GstEdgeDetect *filter = GST_EDGE_DETECT (base);

  cv::cvtColor (img, filter->cvGray, cv::COLOR_RGB2GRAY);
  cv::Canny (filter->cvGray, filter->cvEdge, filter->threshold1,
      filter->threshold2, filter->aperture);

  outimg.setTo (cv::Scalar::all (0));
  if (filter->mask) {
    img.copyTo (outimg, filter->cvEdge);
  } else {
    cv::cvtColor (filter->cvEdge, outimg, cv::COLOR_GRAY2RGB);
  }

  return GST_FLOW_OK;
}
