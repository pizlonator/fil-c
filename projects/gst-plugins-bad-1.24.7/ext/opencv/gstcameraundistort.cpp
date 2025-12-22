/*
 * GStreamer
 * Copyright (C) <2017> Philippe Renon <philippe_renon@yahoo.fr>
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
 * SECTION:element-cameraundistort
 *
 * This element performs camera distortion correction.
 *
 * Camera correction settings are obtained by running through
 * the camera calibration process with the cameracalibrate element.
 *
 * It is possible to do live correction and calibration by chaining
 * a cameraundistort and a cameracalibrate element. The cameracalibrate
 * will send an event with the correction parameters to the the cameraundistort.
 *
 * Based on this tutorial: https://docs.opencv.org/2.4/doc/tutorials/calib3d/camera_calibration/camera_calibration.html
 *
 * ## Example pipelines
 *
 * |[
 * gst-launch-1.0 -v v4l2src ! videoconvert ! cameraundistort settings="???" ! autovideosink
 * ]| will correct camera distortion based on provided settings.
 * |[
 * gst-launch-1.0 -v v4l2src ! videoconvert ! cameraundistort ! cameracalibrate ! autovideosink
 * ]| will correct camera distortion once camera calibration is done.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <vector>

#include "camerautils.hpp"
#include "cameraevent.hpp"

#include "gstcameraundistort.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>

#include <gst/opencv/gstopencvutils.h>

GST_DEBUG_CATEGORY_STATIC (gst_camera_undistort_debug);
#define GST_CAT_DEFAULT gst_camera_undistort_debug

#define DEFAULT_SHOW_UNDISTORTED TRUE
#define DEFAULT_ALPHA 0.0
#define DEFAULT_CROP FALSE

enum
{
  PROP_0,
  PROP_SHOW_UNDISTORTED,
  PROP_ALPHA,
  PROP_CROP,
  PROP_SETTINGS
};

G_DEFINE_TYPE_WITH_CODE (GstCameraUndistort, gst_camera_undistort,
    GST_TYPE_OPENCV_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_camera_undistort_debug, "cameraundistort", 0,
        "Performs camera undistortion");
    );
GST_ELEMENT_REGISTER_DEFINE (cameraundistort, "cameraundistort", GST_RANK_NONE,
    GST_TYPE_CAMERA_UNDISTORT);

static void gst_camera_undistort_dispose (GObject * object);
static void gst_camera_undistort_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_camera_undistort_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_camera_undistort_set_info (GstOpencvVideoFilter * cvfilter,
    gint in_width, gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type);
static GstFlowReturn gst_camera_undistort_transform_frame (GstOpencvVideoFilter
    * cvfilter, GstBuffer * frame, cv::Mat img, GstBuffer * outframe,
    cv::Mat outimg);

static gboolean gst_camera_undistort_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static gboolean gst_camera_undistort_src_event (GstBaseTransform * trans,
    GstEvent * event);

static void camera_undistort_run (GstCameraUndistort * undist, cv::Mat img,
    cv::Mat outimg);
static gboolean camera_undistort_init_undistort_rectify_map (GstCameraUndistort
    * undist);

/* initialize the cameraundistort's class */
static void
gst_camera_undistort_class_init (GstCameraUndistortClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstOpencvVideoFilterClass *opencvfilter_class =
      GST_OPENCV_VIDEO_FILTER_CLASS (klass);

  GstCaps *caps;
  GstPadTemplate *templ;

  gobject_class->dispose = gst_camera_undistort_dispose;
  gobject_class->set_property = gst_camera_undistort_set_property;
  gobject_class->get_property = gst_camera_undistort_get_property;

  trans_class->sink_event = GST_DEBUG_FUNCPTR (gst_camera_undistort_sink_event);
  trans_class->src_event = GST_DEBUG_FUNCPTR (gst_camera_undistort_src_event);

  opencvfilter_class->cv_set_caps = gst_camera_undistort_set_info;
  opencvfilter_class->cv_trans_func = gst_camera_undistort_transform_frame;

  g_object_class_install_property (gobject_class, PROP_SHOW_UNDISTORTED,
      g_param_spec_boolean ("undistort", "Apply camera corrections",
          "Apply camera corrections",
          DEFAULT_SHOW_UNDISTORTED,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_ALPHA,
      g_param_spec_float ("alpha", "Pixels",
          "Show all pixels (1), only valid ones (0) or something in between",
          0.0, 1.0, DEFAULT_ALPHA,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SETTINGS,
      g_param_spec_string ("settings", "Settings",
          "Camera correction parameters (opaque string of serialized OpenCV objects)",
          NULL, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "cameraundistort",
      "Filter/Effect/Video",
      "Performs camera undistort", "Philippe Renon <philippe_renon@yahoo.fr>");

  /* add sink and source pad templates */
  caps = gst_opencv_caps_from_cv_image_type (CV_16UC1);
  gst_caps_append (caps, gst_opencv_caps_from_cv_image_type (CV_8UC4));
  gst_caps_append (caps, gst_opencv_caps_from_cv_image_type (CV_8UC3));
  gst_caps_append (caps, gst_opencv_caps_from_cv_image_type (CV_8UC1));
  templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      gst_caps_ref (caps));
  gst_element_class_add_pad_template (element_class, templ);
  templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
  gst_element_class_add_pad_template (element_class, templ);
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_camera_undistort_init (GstCameraUndistort * undist)
{
  undist->showUndistorted = DEFAULT_SHOW_UNDISTORTED;
  undist->alpha = DEFAULT_ALPHA;
  undist->crop = DEFAULT_CROP;

  undist->doUndistort = FALSE;
  undist->settingsChanged = FALSE;

  undist->cameraMatrix = 0;
  undist->distCoeffs = 0;
  undist->map1 = 0;
  undist->map2 = 0;

  undist->settings = NULL;
}

static void
gst_camera_undistort_dispose (GObject * object)
{
  GstCameraUndistort *undist = GST_CAMERA_UNDISTORT (object);

  g_free (undist->settings);
  undist->settings = NULL;

  G_OBJECT_CLASS (gst_camera_undistort_parent_class)->dispose (object);
}


static void
gst_camera_undistort_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCameraUndistort *undist = GST_CAMERA_UNDISTORT (object);
  const char *str;

  switch (prop_id) {
    case PROP_SHOW_UNDISTORTED:
      undist->showUndistorted = g_value_get_boolean (value);
      undist->settingsChanged = TRUE;
      break;
    case PROP_ALPHA:
      undist->alpha = g_value_get_float (value);
      undist->settingsChanged = TRUE;
      break;
    case PROP_CROP:
      undist->crop = g_value_get_boolean (value);
      break;
    case PROP_SETTINGS:
      if (undist->settings) {
        g_free (undist->settings);
        undist->settings = NULL;
      }
      str = g_value_get_string (value);
      if (str)
        undist->settings = g_strdup (str);
      undist->settingsChanged = TRUE;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_camera_undistort_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCameraUndistort *undist = GST_CAMERA_UNDISTORT (object);

  switch (prop_id) {
    case PROP_SHOW_UNDISTORTED:
      g_value_set_boolean (value, undist->showUndistorted);
      break;
    case PROP_ALPHA:
      g_value_set_float (value, undist->alpha);
      break;
    case PROP_CROP:
      g_value_set_boolean (value, undist->crop);
      break;
    case PROP_SETTINGS:
      g_value_set_string (value, undist->settings);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

gboolean
gst_camera_undistort_set_info (GstOpencvVideoFilter * cvfilter,
    gint in_width, gint in_height, G_GNUC_UNUSED int in_cv_type,
    G_GNUC_UNUSED gint out_width, G_GNUC_UNUSED gint out_height,
    G_GNUC_UNUSED int out_cv_type)
{
  GstCameraUndistort *undist = GST_CAMERA_UNDISTORT (cvfilter);

  undist->imageSize = cv::Size (in_width, in_height);

  return TRUE;
}

/*
 * Performs the camera undistort
 */
static GstFlowReturn
gst_camera_undistort_transform_frame (GstOpencvVideoFilter * cvfilter,
    G_GNUC_UNUSED GstBuffer * frame, cv::Mat img,
    G_GNUC_UNUSED GstBuffer * outframe, cv::Mat outimg)
{
  GstCameraUndistort *undist = GST_CAMERA_UNDISTORT (cvfilter);

  camera_undistort_run (undist, img, outimg);

  return GST_FLOW_OK;
}

static void
camera_undistort_run (GstCameraUndistort * undist, cv::Mat img, cv::Mat outimg)
{
  /* TODO is settingsChanged handling thread safe ? */
  if (undist->settingsChanged) {
    /* settings have changed, need to recompute undistort */
    undist->settingsChanged = FALSE;
    undist->doUndistort = FALSE;
    if (undist->showUndistorted && undist->settings) {
      if (camera_deserialize_undistort_settings (undist->settings,
              undist->cameraMatrix, undist->distCoeffs)) {
        undist->doUndistort =
            camera_undistort_init_undistort_rectify_map (undist);
      }
    }
  }

  if (undist->showUndistorted && undist->doUndistort) {
    /* do the undistort */
    cv::remap (img, outimg, undist->map1, undist->map2, cv::INTER_LINEAR);

    if (undist->crop) {
      /* TODO do the cropping */
      const cv::Scalar CROP_COLOR (0, 255, 0);
      cv::rectangle (outimg, undist->validPixROI, CROP_COLOR);
    }
  } else {
    /* FIXME should use pass through to avoid this copy when not undistorting */
    img.copyTo (outimg);
  }
}

/* compute undistort */
static gboolean
camera_undistort_init_undistort_rectify_map (GstCameraUndistort * undist)
{
  cv::Size newImageSize;
  cv::Rect validPixROI;
  cv::Mat newCameraMatrix =
      cv::getOptimalNewCameraMatrix (undist->cameraMatrix, undist->distCoeffs,
      undist->imageSize, undist->alpha, newImageSize, &validPixROI);
  undist->validPixROI = validPixROI;

  cv::initUndistortRectifyMap (undist->cameraMatrix, undist->distCoeffs,
      cv::Mat (), newCameraMatrix, undist->imageSize, CV_16SC2, undist->map1,
      undist->map2);

  return TRUE;
}

static gboolean
camera_undistort_calibration_event (GstCameraUndistort * undist,
    GstEvent * event)
{
  g_free (undist->settings);

  if (!gst_camera_event_parse_calibrated (event, &(undist->settings))) {
    return FALSE;
  }

  undist->settingsChanged = TRUE;

  return TRUE;
}

static gboolean
gst_camera_undistort_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstCameraUndistort *undist = GST_CAMERA_UNDISTORT (trans);

  const GstStructure *structure = gst_event_get_structure (event);

  if (GST_EVENT_TYPE (event) == GST_EVENT_CUSTOM_BOTH && structure) {
    if (strcmp (gst_structure_get_name (structure),
            GST_CAMERA_EVENT_CALIBRATED_NAME) == 0) {
      return camera_undistort_calibration_event (undist, event);
    }
  }

  return
      GST_BASE_TRANSFORM_CLASS (gst_camera_undistort_parent_class)->sink_event
      (trans, event);
}

static gboolean
gst_camera_undistort_src_event (GstBaseTransform * trans, GstEvent * event)
{
  GstCameraUndistort *undist = GST_CAMERA_UNDISTORT (trans);

  const GstStructure *structure = gst_event_get_structure (event);

  if (GST_EVENT_TYPE (event) == GST_EVENT_CUSTOM_BOTH && structure) {
    if (strcmp (gst_structure_get_name (structure),
            GST_CAMERA_EVENT_CALIBRATED_NAME) == 0) {
      return camera_undistort_calibration_event (undist, event);
    }
  }

  return
      GST_BASE_TRANSFORM_CLASS (gst_camera_undistort_parent_class)->src_event
      (trans, event);
}
