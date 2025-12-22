/*
 * GStreamer
 * Copyright (C) 2020 Vivek R <123vivekr@gmail.com>
 * Copyright (C) 2021 Cesar Fabian Orccon Chipana <cfoch.fabian@gmail.com>
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
 * SECTION:element-cvtracker
 *
 * Performs object tracking on videos and stores it in video buffer metadata.
 *
 * ## Example launch line
 *
 * ```
 * gst-launch-1.0 v4l2src ! videoconvert ! cvtracker box-x=50 box-y=50 box-wdith=50 box-height=50 ! videoconvert ! xvimagesink
 * ```
 *
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstcvtracker.h"

GST_DEBUG_CATEGORY_STATIC (gst_cvtracker_debug);
#define GST_CAT_DEFAULT gst_cvtracker_debug

#define DEFAULT_PROP_INITIAL_X 50
#define DEFAULT_PROP_INITIAL_Y 50
#define DEFAULT_PROP_INITIAL_WIDTH 50
#define DEFAULT_PROP_INITIAL_HEIGHT 50

enum
{
  PROP_0,
  PROP_INITIAL_X,
  PROP_INITIAL_Y,
  PROP_INITIAL_WIDTH,
  PROP_INITIAL_HEIGHT,
  PROP_ALGORITHM,
  PROP_DRAW,
};

#define GST_OPENCV_TRACKER_ALGORITHM (tracker_algorithm_get_type ())

/**
 * GstOpenCVTrackerAlgorithm:
 *
 * Since: 1.20
 */
static GType
tracker_algorithm_get_type (void)
{
  static GType algorithm = 0;
  static const GEnumValue algorithms[] = {
    {GST_OPENCV_TRACKER_ALGORITHM_BOOSTING, "the Boosting tracker", "Boosting"},
    {GST_OPENCV_TRACKER_ALGORITHM_CSRT, "the CSRT tracker", "CSRT"},
    {GST_OPENCV_TRACKER_ALGORITHM_KCF,
          "the KCF (Kernelized Correlation Filter) tracker",
        "KCF"},
    {GST_OPENCV_TRACKER_ALGORITHM_MEDIANFLOW, "the Median Flow tracker",
        "MedianFlow"},
    {GST_OPENCV_TRACKER_ALGORITHM_MIL, "the MIL tracker", "MIL"},
    {GST_OPENCV_TRACKER_ALGORITHM_MOSSE,
        "the MOSSE (Minimum Output Sum of Squared Error) tracker", "MOSSE"},
    {GST_OPENCV_TRACKER_ALGORITHM_TLD,
          "the TLD (Tracking, learning and detection) tracker",
        "TLD"},
    {0, NULL, NULL},
  };

  if (!algorithm) {
    algorithm =
        g_enum_register_static ("GstOpenCVTrackerAlgorithm", algorithms);
  }
  return algorithm;
}

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

G_DEFINE_TYPE_WITH_CODE (GstCVTracker, gst_cvtracker,
    GST_TYPE_OPENCV_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_cvtracker_debug, "cvtracker", 0,
        "Performs object tracking on videos and stores it in video buffer "
        "metadata"));
GST_ELEMENT_REGISTER_DEFINE (cvtracker, "cvtracker", GST_RANK_NONE,
    GST_TYPE_OPENCV_TRACKER);

static void gst_cvtracker_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_cvtracker_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_cvtracker_transform_ip (GstOpencvVideoFilter
    * filter, GstBuffer * buf, cv::Mat img);

static void
gst_cvtracker_finalize (GObject * obj)
{
  GstCVTracker *filter = GST_OPENCV_TRACKER (obj);

  filter->tracker.release ();
  filter->roi.release ();

  G_OBJECT_CLASS (gst_cvtracker_parent_class)->finalize (obj);
}

static void
gst_cvtracker_class_init (GstCVTrackerClass * klass)
{
  GObjectClass *gobject_class;
  GstOpencvVideoFilterClass *gstopencvbasefilter_class;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gobject_class = (GObjectClass *) klass;
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_cvtracker_finalize);
  gstopencvbasefilter_class = (GstOpencvVideoFilterClass *) klass;

  gstopencvbasefilter_class->cv_trans_ip_func = gst_cvtracker_transform_ip;

  gobject_class->set_property = gst_cvtracker_set_property;
  gobject_class->get_property = gst_cvtracker_get_property;

   /*
    * Tracker API in versions older than OpenCV 4.5.1 worked with a ROI based
    * on Rect<double>. However newer versions use Rect<int>. Running the same
    * tracker type on different versions may lead to round up errors.
    * To avoid inconsistencies from the GStreamer side depending on the OpenCV
    * version, use integer properties independently on the OpenCV.
    **/
  g_object_class_install_property (gobject_class, PROP_INITIAL_X,
      g_param_spec_uint ("object-initial-x", "Initial X coordinate",
          "Track object box's initial X coordinate", 0, G_MAXUINT,
          DEFAULT_PROP_INITIAL_X,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_INITIAL_Y,
      g_param_spec_uint ("object-initial-y", "Initial Y coordinate",
          "Track object box's initial Y coordinate", 0, G_MAXUINT,
          DEFAULT_PROP_INITIAL_Y,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_INITIAL_WIDTH,
      g_param_spec_uint ("object-initial-width", "Object Initial Width",
          "Track object box's initial width", 0, G_MAXUINT,
          DEFAULT_PROP_INITIAL_WIDTH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_INITIAL_HEIGHT,
      g_param_spec_uint ("object-initial-height", "Object Initial Height",
          "Track object box's initial height", 0, G_MAXUINT,
          DEFAULT_PROP_INITIAL_HEIGHT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_ALGORITHM,
      g_param_spec_enum ("algorithm", "Algorithm",
          "Algorithm for tracking objects", GST_OPENCV_TRACKER_ALGORITHM,
          GST_OPENCV_TRACKER_ALGORITHM_MEDIANFLOW,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_DRAW,
      g_param_spec_boolean ("draw-rect", "Display",
          "Draw rectangle around tracked object",
          TRUE, (GParamFlags) G_PARAM_READWRITE));

  gst_element_class_set_static_metadata (element_class,
      "cvtracker",
      "Filter/Effect/Video",
      "Performs object tracking on videos and stores it in video buffer metadata.",
      "Vivek R <123vivekr@gmail.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  gst_type_mark_as_plugin_api (GST_OPENCV_TRACKER_ALGORITHM,
      (GstPluginAPIFlags) 0);
}

static void
gst_cvtracker_init (GstCVTracker * filter)
{
  filter->x = DEFAULT_PROP_INITIAL_X;
  filter->y = DEFAULT_PROP_INITIAL_Y;
  filter->width = DEFAULT_PROP_INITIAL_WIDTH;
  filter->height = DEFAULT_PROP_INITIAL_HEIGHT;
#if GST_OPENCV_CHECK_VERSION(4, 5, 1)
  filter->tracker = cv::legacy::upgradeTrackingAPI(
      cv::legacy::TrackerMedianFlow::create());
#else
  filter->tracker = cv::TrackerMedianFlow::create();
#endif
  filter->draw = TRUE;
  filter->post_debug_info = TRUE;

  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      TRUE);
  filter->algorithm = GST_OPENCV_TRACKER_ALGORITHM_MEDIANFLOW;
}

static void
gst_cvtracker_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCVTracker *filter = GST_OPENCV_TRACKER (object);

  switch (prop_id) {
    case PROP_INITIAL_X:
      filter->x = g_value_get_uint (value);
      break;
    case PROP_INITIAL_Y:
      filter->y = g_value_get_uint (value);
      break;
    case PROP_INITIAL_WIDTH:
      filter->width = g_value_get_uint (value);
      break;
    case PROP_INITIAL_HEIGHT:
      filter->height = g_value_get_uint (value);
      break;
    case PROP_ALGORITHM:
      filter->algorithm = g_value_get_enum (value);
      break;
    case PROP_DRAW:
      filter->draw = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
create_cvtracker (GstCVTracker * filter)
{
  switch (filter->algorithm) {
    case GST_OPENCV_TRACKER_ALGORITHM_BOOSTING:
#if GST_OPENCV_CHECK_VERSION(4, 5, 1)
      filter->tracker = cv::legacy::upgradeTrackingAPI(
          cv::legacy::TrackerBoosting::create());
#else
      filter->tracker = cv::TrackerBoosting::create();
#endif
      break;
    case GST_OPENCV_TRACKER_ALGORITHM_CSRT:
      filter->tracker = cv::TrackerCSRT::create ();
      break;
    case GST_OPENCV_TRACKER_ALGORITHM_KCF:
      filter->tracker = cv::TrackerKCF::create ();
      break;
    case GST_OPENCV_TRACKER_ALGORITHM_MEDIANFLOW:
#if GST_OPENCV_CHECK_VERSION(4, 5, 1)
      filter->tracker = cv::legacy::upgradeTrackingAPI(
          cv::legacy::TrackerMedianFlow::create());
#else
      filter->tracker = cv::TrackerMedianFlow::create();
#endif
      break;
    case GST_OPENCV_TRACKER_ALGORITHM_MIL:
      filter->tracker = cv::TrackerMIL::create ();
      break;
    case GST_OPENCV_TRACKER_ALGORITHM_MOSSE:
#if GST_OPENCV_CHECK_VERSION(4, 5, 1)
      filter->tracker = cv::legacy::upgradeTrackingAPI(
          cv::legacy::TrackerMOSSE::create());
#else
      filter->tracker = cv::TrackerMOSSE::create ();
#endif
      break;
    case GST_OPENCV_TRACKER_ALGORITHM_TLD:
#if GST_OPENCV_CHECK_VERSION(4, 5, 1)
      filter->tracker = cv::legacy::upgradeTrackingAPI(
          cv::legacy::TrackerTLD::create());
#else
      filter->tracker = cv::TrackerTLD::create();
#endif
      break;
  }
}

static void
gst_cvtracker_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCVTracker *filter = GST_OPENCV_TRACKER (object);

  switch (prop_id) {
    case PROP_INITIAL_X:
      g_value_set_uint (value, filter->x);
      break;
    case PROP_INITIAL_Y:
      g_value_set_uint (value, filter->y);
      break;
    case PROP_INITIAL_WIDTH:
      g_value_set_uint (value, filter->width);
      break;
    case PROP_INITIAL_HEIGHT:
      g_value_set_uint (value, filter->height);
      break;
    case PROP_ALGORITHM:
      g_value_set_enum (value, filter->algorithm);
      break;
    case PROP_DRAW:
      g_value_set_boolean (value, filter->draw);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_cvtracker_transform_ip (GstOpencvVideoFilter * base,
    GstBuffer * buf, cv::Mat img)
{
  GstCVTracker *filter = GST_OPENCV_TRACKER (base);
  GstStructure *s;
  GstMessage *msg;

  if (filter->roi.empty ()) {
#if GST_OPENCV_CHECK_VERSION(4, 5, 1)
    filter->roi = new (cv::Rect);
#else
    filter->roi = new (cv::Rect2d);
#endif
    filter->roi->x = filter->x;
    filter->roi->y = filter->y;
    filter->roi->width = filter->width;
    filter->roi->height = filter->height;
    create_cvtracker (filter);
    filter->tracker->init (img, *filter->roi);
  } else if (filter->tracker->update (img, *filter->roi)) {
#if !GST_OPENCV_CHECK_VERSION(4, 5, 1)
    /* Round values to avoid inconsistencies depending on the OpenCV version. */
    filter->roi->x = cvRound (filter->roi->x);
    filter->roi->y = cvRound (filter->roi->y);
    filter->roi->width = cvRound (filter->roi->width);
    filter->roi->height = cvRound (filter->roi->height);
#endif
    s = gst_structure_new ("object",
        "x", G_TYPE_UINT, (guint) filter->roi->x,
        "y", G_TYPE_UINT, (guint) filter->roi->y,
        "width", G_TYPE_UINT, (guint) filter->roi->width,
        "height", G_TYPE_UINT, (guint) filter->roi->height, NULL);
    msg = gst_message_new_element (GST_OBJECT (filter), s);
    gst_buffer_add_video_region_of_interest_meta (buf, "object",
        filter->roi->x, filter->roi->y, filter->roi->width,
        filter->roi->height);
    gst_element_post_message (GST_ELEMENT (filter), msg);
    if (filter->draw)
      cv::rectangle (img, *filter->roi, cv::Scalar (255, 0, 0), 2, 1);
    if (!(filter->post_debug_info))
      filter->post_debug_info = TRUE;
  } else if (filter->post_debug_info) {
    GST_DEBUG_OBJECT (filter, "tracker lost");
    filter->post_debug_info = FALSE;
  }

  return GST_FLOW_OK;
}
