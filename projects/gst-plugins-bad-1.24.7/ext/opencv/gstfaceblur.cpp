/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
 * Copyright (C) 2011 Robert Jobbagy <jobbagy.robert@gmail.com>
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
 * SECTION:element-faceblur
 *
 * Blurs faces in images and videos.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 autovideosrc ! videoconvert ! faceblur ! videoconvert ! autovideosink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <vector>

#include "gstfaceblur.h"
#include <opencv2/imgproc.hpp>

GST_DEBUG_CATEGORY_STATIC (gst_face_blur_debug);
#define GST_CAT_DEFAULT gst_face_blur_debug

#define DEFAULT_PROFILE OPENCV_PREFIX G_DIR_SEPARATOR_S "share" \
    G_DIR_SEPARATOR_S OPENCV_PATH_NAME G_DIR_SEPARATOR_S "haarcascades" \
    G_DIR_SEPARATOR_S "haarcascade_frontalface_default.xml"
#define DEFAULT_SCALE_FACTOR 1.25
#if (CV_MAJOR_VERSION >= 4)
#define DEFAULT_FLAGS CASCADE_DO_CANNY_PRUNING
#else
#define DEFAULT_FLAGS CV_HAAR_DO_CANNY_PRUNING
#endif
#define DEFAULT_MIN_NEIGHBORS 3
#define DEFAULT_MIN_SIZE_WIDTH 30
#define DEFAULT_MIN_SIZE_HEIGHT 30

using namespace cv;
using namespace std;
enum
{
  PROP_0,
  PROP_PROFILE,
  PROP_SCALE_FACTOR,
  PROP_MIN_NEIGHBORS,
  PROP_FLAGS,
  PROP_MIN_SIZE_WIDTH,
  PROP_MIN_SIZE_HEIGHT
};

/**
 * GstOpencvFaceDetectFlags:
 * @GST_CAMERABIN_FLAG_SOURCE_RESIZE: enable video crop and scale
 *   after capture
 *
 * Flags parameter to OpenCV's cvHaarDetectObjects function.
 */
typedef enum
{
  GST_OPENCV_FACE_BLUR_HAAR_DO_CANNY_PRUNING = (1 << 0)
} GstOpencvFaceBlurFlags;

#define GST_TYPE_OPENCV_FACE_BLUR_FLAGS (gst_opencv_face_blur_flags_get_type())

static void
register_gst_opencv_face_blur_flags (GType * id)
{
  static const GFlagsValue values[] = {
    {(guint) GST_OPENCV_FACE_BLUR_HAAR_DO_CANNY_PRUNING,
        "Do Canny edge detection to discard some regions", "do-canny-pruning"},
    {0, NULL, NULL}
  };
  *id = g_flags_register_static ("GstOpencvFaceBlurFlags", values);
}

static GType
gst_opencv_face_blur_flags_get_type (void)
{
  static GType id;
  static GOnce once = G_ONCE_INIT;

  g_once (&once, (GThreadFunc) register_gst_opencv_face_blur_flags, &id);
  return id;
}

/* the capabilities of the inputs and outputs.
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

G_DEFINE_TYPE_WITH_CODE (GstFaceBlur, gst_face_blur,
    GST_TYPE_OPENCV_VIDEO_FILTER, GST_DEBUG_CATEGORY_INIT (gst_face_blur_debug,
        "faceblur", 0, "Blurs faces in images and videos"););
GST_ELEMENT_REGISTER_DEFINE (faceblur, "faceblur", GST_RANK_NONE,
    GST_TYPE_FACE_BLUR);

static void gst_face_blur_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_face_blur_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);


static gboolean gst_face_blur_set_caps (GstOpencvVideoFilter * transform,
    gint in_width, gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type);
static GstFlowReturn gst_face_blur_transform_ip (GstOpencvVideoFilter *
    transform, GstBuffer * buffer, Mat img);

static CascadeClassifier *gst_face_blur_load_profile (GstFaceBlur *
    filter, gchar * profile);

/* Clean up */
static void
gst_face_blur_finalize (GObject * obj)
{
  GstFaceBlur *filter = GST_FACE_BLUR (obj);

  filter->cvGray.release ();

  if (filter->cvCascade)
    delete filter->cvCascade;

  g_free (filter->profile);

  G_OBJECT_CLASS (gst_face_blur_parent_class)->finalize (obj);
}


/* initialize the faceblur's class */
static void
gst_face_blur_class_init (GstFaceBlurClass * klass)
{
  GObjectClass *gobject_class;
  GstOpencvVideoFilterClass *gstopencvbasefilter_class;

  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  gobject_class = (GObjectClass *) klass;
  gstopencvbasefilter_class = (GstOpencvVideoFilterClass *) klass;

  gstopencvbasefilter_class->cv_trans_ip_func = gst_face_blur_transform_ip;
  gstopencvbasefilter_class->cv_set_caps = gst_face_blur_set_caps;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_face_blur_finalize);
  gobject_class->set_property = gst_face_blur_set_property;
  gobject_class->get_property = gst_face_blur_get_property;

  g_object_class_install_property (gobject_class, PROP_PROFILE,
      g_param_spec_string ("profile", "Profile",
          "Location of Haar cascade file to use for face blurion",
          DEFAULT_PROFILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_FLAGS,
      g_param_spec_flags ("flags", "Flags", "Flags to cvHaarDetectObjects",
          GST_TYPE_OPENCV_FACE_BLUR_FLAGS, DEFAULT_FLAGS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_SCALE_FACTOR,
      g_param_spec_double ("scale-factor", "Scale factor",
          "Factor by which the windows is scaled after each scan", 1.1, 10.0,
          DEFAULT_SCALE_FACTOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MIN_NEIGHBORS,
      g_param_spec_int ("min-neighbors", "Minimum neighbors",
          "Minimum number (minus 1) of neighbor rectangles that makes up "
          "an object", 0, G_MAXINT, DEFAULT_MIN_NEIGHBORS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MIN_SIZE_WIDTH,
      g_param_spec_int ("min-size-width", "Minimum size width",
          "Minimum window width size", 0, G_MAXINT, DEFAULT_MIN_SIZE_WIDTH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MIN_SIZE_HEIGHT,
      g_param_spec_int ("min-size-height", "Minimum size height",
          "Minimum window height size", 0, G_MAXINT, DEFAULT_MIN_SIZE_HEIGHT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "faceblur",
      "Filter/Effect/Video",
      "Blurs faces in images and videos",
      "Michael Sheldon <mike@mikeasoft.com>,Robert Jobbagy <jobbagy.robert@gmail.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  gst_type_mark_as_plugin_api (GST_TYPE_OPENCV_FACE_BLUR_FLAGS, (GstPluginAPIFlags) 0);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_face_blur_init (GstFaceBlur * filter)
{
  filter->profile = g_strdup (DEFAULT_PROFILE);
  filter->cvCascade = gst_face_blur_load_profile (filter, filter->profile);
  filter->sent_profile_load_failed_msg = FALSE;
  filter->scale_factor = DEFAULT_SCALE_FACTOR;
  filter->min_neighbors = DEFAULT_MIN_NEIGHBORS;
  filter->flags = DEFAULT_FLAGS;
  filter->min_size_width = DEFAULT_MIN_SIZE_WIDTH;
  filter->min_size_height = DEFAULT_MIN_SIZE_HEIGHT;

  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      TRUE);
}

static void
gst_face_blur_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFaceBlur *filter = GST_FACE_BLUR (object);

  switch (prop_id) {
    case PROP_PROFILE:
      g_free (filter->profile);
      if (filter->cvCascade)
        delete filter->cvCascade;
      filter->profile = g_value_dup_string (value);
      filter->cvCascade = gst_face_blur_load_profile (filter, filter->profile);
      filter->sent_profile_load_failed_msg = FALSE;
      break;
    case PROP_SCALE_FACTOR:
      filter->scale_factor = g_value_get_double (value);
      break;
    case PROP_MIN_NEIGHBORS:
      filter->min_neighbors = g_value_get_int (value);
      break;
    case PROP_MIN_SIZE_WIDTH:
      filter->min_size_width = g_value_get_int (value);
      break;
    case PROP_MIN_SIZE_HEIGHT:
      filter->min_size_height = g_value_get_int (value);
      break;
    case PROP_FLAGS:
      filter->flags = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_face_blur_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstFaceBlur *filter = GST_FACE_BLUR (object);

  switch (prop_id) {
    case PROP_PROFILE:
      g_value_set_string (value, filter->profile);
      break;
    case PROP_SCALE_FACTOR:
      g_value_set_double (value, filter->scale_factor);
      break;
    case PROP_MIN_NEIGHBORS:
      g_value_set_int (value, filter->min_neighbors);
      break;
    case PROP_MIN_SIZE_WIDTH:
      g_value_set_int (value, filter->min_size_width);
      break;
    case PROP_MIN_SIZE_HEIGHT:
      g_value_set_int (value, filter->min_size_height);
      break;
    case PROP_FLAGS:
      g_value_set_flags (value, filter->flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_face_blur_set_caps (GstOpencvVideoFilter * transform,
    gint in_width, gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type)
{
  GstFaceBlur *filter = GST_FACE_BLUR (transform);

  filter->cvGray.create (Size (in_width, in_height), CV_8UC1);

  return TRUE;
}

static GstFlowReturn
gst_face_blur_transform_ip (GstOpencvVideoFilter * transform,
    GstBuffer * buffer, Mat img)
{
  GstFaceBlur *filter = GST_FACE_BLUR (transform);
  vector < Rect > faces;
  unsigned int i;

  if (!filter->cvCascade) {
    if (filter->profile != NULL
        && filter->sent_profile_load_failed_msg == FALSE) {
      GST_ELEMENT_WARNING (filter, RESOURCE, NOT_FOUND,
          ("Profile %s is missing.", filter->profile),
          ("missing faceblur profile file %s", filter->profile));
      filter->sent_profile_load_failed_msg = TRUE;
    }
    return GST_FLOW_OK;
  }

  cvtColor (img, filter->cvGray, COLOR_RGB2GRAY);

  filter->cvCascade->detectMultiScale (filter->cvGray, faces,
      filter->scale_factor, filter->min_neighbors, filter->flags,
      Size (filter->min_size_width, filter->min_size_height), Size (0, 0));

  if (!faces.empty ()) {

    for (i = 0; i < faces.size (); ++i) {
      Rect *r = &faces[i];
      Mat roi (img, Rect (r->x, r->y, r->width, r->height));
      blur (roi, roi, Size (11, 11));
      GaussianBlur (roi, roi, Size (11, 11), 0, 0);
    }
  }

  return GST_FLOW_OK;
}

static CascadeClassifier *
gst_face_blur_load_profile (GstFaceBlur * filter, gchar * profile)
{
  CascadeClassifier *cascade;

  cascade = new CascadeClassifier (profile);
  if (cascade->empty ()) {
    GST_ERROR_OBJECT (filter, "Invalid profile file: %s", profile);
    delete cascade;
    return NULL;
  }
  return cascade;
}
