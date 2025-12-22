/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
 * Copyright (C) 2011 Stefan Sauer <ensonic@users.sf.net>
 * Copyright (C) 2014 Robert Jobbagy <jobbagy.robert@gmail.com>
 * Copyright (C) 2018 Nicola Murino <nicola.murino@gmail.com>
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
 * SECTION:element-facedetect
 *
 * Performs face detection on videos and images.
 * If you have high cpu load you need to use videoscale with capsfilter and reduce the video resolution.
 *
 * The image is scaled down multiple times using the GstFaceDetect::scale-factor
 * until the size is &lt;= GstFaceDetect::min-size-width or
 * GstFaceDetect::min-size-height.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 autovideosrc ! decodebin ! colorspace ! facedetect ! videoconvert ! xvimagesink
 * ]| Detect and show faces
 * |[
 * gst-launch-1.0 autovideosrc ! video/x-raw,width=320,height=240 ! videoconvert ! facedetect min-size-width=60 min-size-height=60 ! colorspace ! xvimagesink
 * ]| Detect large faces on a smaller image
 *
 */

/* FIXME: development version of OpenCV has CV_HAAR_FIND_BIGGEST_OBJECT which
 * we might want to use if available
 * see https://code.ros.org/svn/opencv/trunk/opencv/modules/objdetect/src/haar.cpp
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <vector>

using namespace std;

#include "gstfacedetect.h"
#include <opencv2/imgproc.hpp>

GST_DEBUG_CATEGORY_STATIC (gst_face_detect_debug);
#define GST_CAT_DEFAULT gst_face_detect_debug

#define HAAR_CASCADES_DIR OPENCV_PREFIX G_DIR_SEPARATOR_S "share" \
    G_DIR_SEPARATOR_S OPENCV_PATH_NAME G_DIR_SEPARATOR_S "haarcascades" \
    G_DIR_SEPARATOR_S
#define DEFAULT_FACE_PROFILE HAAR_CASCADES_DIR "haarcascade_frontalface_default.xml"
#define DEFAULT_NOSE_PROFILE HAAR_CASCADES_DIR "haarcascade_mcs_nose.xml"
#define DEFAULT_MOUTH_PROFILE HAAR_CASCADES_DIR "haarcascade_mcs_mouth.xml"
#define DEFAULT_EYES_PROFILE HAAR_CASCADES_DIR "haarcascade_mcs_eyepair_small.xml"
#define DEFAULT_SCALE_FACTOR 1.25
#if (CV_MAJOR_VERSION >= 4)
#define DEFAULT_FLAGS CASCADE_DO_CANNY_PRUNING
#else
#define DEFAULT_FLAGS CV_HAAR_DO_CANNY_PRUNING
#endif
#define DEFAULT_MIN_NEIGHBORS 3
#define DEFAULT_MIN_SIZE_WIDTH 30
#define DEFAULT_MIN_SIZE_HEIGHT 30
#define DEFAULT_MIN_STDDEV 0

using namespace cv;
/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_DISPLAY,
  PROP_FACE_PROFILE,
  PROP_NOSE_PROFILE,
  PROP_MOUTH_PROFILE,
  PROP_EYES_PROFILE,
  PROP_SCALE_FACTOR,
  PROP_MIN_NEIGHBORS,
  PROP_FLAGS,
  PROP_MIN_SIZE_WIDTH,
  PROP_MIN_SIZE_HEIGHT,
  PROP_UPDATES,
  PROP_MIN_STDDEV
};


/*
 * GstOpencvFaceDetectFlags:
 *
 * Flags parameter to OpenCV's cvHaarDetectObjects function.
 */
typedef enum
{
  GST_OPENCV_FACE_DETECT_HAAR_DO_CANNY_PRUNING = (1 << 0)
} GstOpencvFaceDetectFlags;

#define GST_TYPE_OPENCV_FACE_DETECT_FLAGS (gst_opencv_face_detect_flags_get_type())

inline void
structure_and_message (const vector < Rect > &rectangles, const gchar * name,
    guint rx, guint ry, GstFaceDetect * filter, GstStructure * s)
{
  Rect sr = rectangles[0];
  gchar *nx = g_strconcat (name, "->x", NULL);
  gchar *ny = g_strconcat (name, "->y", NULL);
  gchar *nw = g_strconcat (name, "->width", NULL);
  gchar *nh = g_strconcat (name, "->height", NULL);

  GST_LOG_OBJECT (filter,
      "%s/%" G_GSIZE_FORMAT ": x,y = %4u,%4u: w.h = %4u,%4u",
      name, rectangles.size (), rx + sr.x, ry + sr.y, sr.width, sr.height);
  gst_structure_set (s, nx, G_TYPE_UINT, rx + sr.x, ny, G_TYPE_UINT, ry + sr.y,
      nw, G_TYPE_UINT, sr.width, nh, G_TYPE_UINT, sr.height, NULL);

  g_free (nx);
  g_free (ny);
  g_free (nw);
  g_free (nh);
}

static void
register_gst_opencv_face_detect_flags (GType * id)
{
  static const GFlagsValue values[] = {
    {(guint) GST_OPENCV_FACE_DETECT_HAAR_DO_CANNY_PRUNING,
        "Do Canny edge detection to discard some regions", "do-canny-pruning"},
    {0, NULL, NULL}
  };
  *id = g_flags_register_static ("GstOpencvFaceDetectFlags", values);
}

static GType
gst_opencv_face_detect_flags_get_type (void)
{
  static GType id;
  static GOnce once = G_ONCE_INIT;

  g_once (&once, (GThreadFunc) register_gst_opencv_face_detect_flags, &id);
  return id;
}

#define GST_TYPE_FACE_DETECT_UPDATES (facedetect_update_get_type ())

static GType
facedetect_update_get_type (void)
{
  static GType facedetect_update_type = 0;
  static const GEnumValue facedetect_update[] = {
    {GST_FACEDETECT_UPDATES_EVERY_FRAME, "Send update messages on every frame",
        "every_frame"},
    {GST_FACEDETECT_UPDATES_ON_CHANGE,
          "Send messages when a new face is detected or one is not anymore detected",
        "on_change"},
    {GST_FACEDETECT_UPDATES_ON_FACE,
          "Send messages whenever a face is detected",
        "on_face"},
    {GST_FACEDETECT_UPDATES_NONE, "Send no messages update", "none"},
    {0, NULL, NULL},
  };

  if (!facedetect_update_type) {
    facedetect_update_type =
        g_enum_register_static ("GstFaceDetectUpdates", facedetect_update);
  }
  return facedetect_update_type;
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

G_DEFINE_TYPE_WITH_CODE (GstFaceDetect, gst_face_detect,
    GST_TYPE_OPENCV_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_face_detect_debug, "facedetect", 0,
        "Performs face detection on videos and images, providing detected positions via bus messages"););
GST_ELEMENT_REGISTER_DEFINE (facedetect, "facedetect", GST_RANK_NONE,
    GST_TYPE_FACE_DETECT);

static void gst_face_detect_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_face_detect_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_face_detect_set_caps (GstOpencvVideoFilter * transform,
    gint in_width, gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type);
static GstFlowReturn gst_face_detect_transform_ip (GstOpencvVideoFilter * base,
    GstBuffer * buf, Mat img);

static CascadeClassifier *gst_face_detect_load_profile (GstFaceDetect *
    filter, gchar * profile);

/* Clean up */
static void
gst_face_detect_finalize (GObject * obj)
{
  GstFaceDetect *filter = GST_FACE_DETECT (obj);

  filter->cvGray.release ();

  g_free (filter->face_profile);
  g_free (filter->nose_profile);
  g_free (filter->mouth_profile);
  g_free (filter->eyes_profile);

  if (filter->cvFaceDetect)
    delete (filter->cvFaceDetect);
  if (filter->cvNoseDetect)
    delete (filter->cvNoseDetect);
  if (filter->cvMouthDetect)
    delete (filter->cvMouthDetect);
  if (filter->cvEyesDetect)
    delete (filter->cvEyesDetect);

  G_OBJECT_CLASS (gst_face_detect_parent_class)->finalize (obj);
}

/* initialize the facedetect's class */
static void
gst_face_detect_class_init (GstFaceDetectClass * klass)
{
  GObjectClass *gobject_class;
  GstOpencvVideoFilterClass *gstopencvbasefilter_class;

  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  gobject_class = (GObjectClass *) klass;
  gstopencvbasefilter_class = (GstOpencvVideoFilterClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_face_detect_finalize);
  gobject_class->set_property = gst_face_detect_set_property;
  gobject_class->get_property = gst_face_detect_get_property;

  gstopencvbasefilter_class->cv_trans_ip_func = gst_face_detect_transform_ip;
  gstopencvbasefilter_class->cv_set_caps = gst_face_detect_set_caps;

  g_object_class_install_property (gobject_class, PROP_DISPLAY,
      g_param_spec_boolean ("display", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          TRUE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FACE_PROFILE,
      g_param_spec_string ("profile", "Face profile",
          "Location of Haar cascade file to use for face detection",
          DEFAULT_FACE_PROFILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_NOSE_PROFILE,
      g_param_spec_string ("nose-profile", "Nose profile",
          "Location of Haar cascade file to use for nose detection",
          DEFAULT_NOSE_PROFILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MOUTH_PROFILE,
      g_param_spec_string ("mouth-profile", "Mouth profile",
          "Location of Haar cascade file to use for mouth detection",
          DEFAULT_MOUTH_PROFILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_EYES_PROFILE,
      g_param_spec_string ("eyes-profile", "Eyes profile",
          "Location of Haar cascade file to use for eye-pair detection",
          DEFAULT_EYES_PROFILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FLAGS,
      g_param_spec_flags ("flags", "Flags", "Flags to cvHaarDetectObjects",
          GST_TYPE_OPENCV_FACE_DETECT_FLAGS, DEFAULT_FLAGS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_SCALE_FACTOR,
      g_param_spec_double ("scale-factor", "Scale factor",
          "Factor by which the frame is scaled after each object scan",
          1.1, 10.0, DEFAULT_SCALE_FACTOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MIN_NEIGHBORS,
      g_param_spec_int ("min-neighbors", "Minimum neighbors",
          "Minimum number (minus 1) of neighbor rectangles that makes up "
          "an object", 0, G_MAXINT, DEFAULT_MIN_NEIGHBORS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MIN_SIZE_WIDTH,
      g_param_spec_int ("min-size-width", "Minimum face width",
          "Minimum area width to be recognized as a face", 0, G_MAXINT,
          DEFAULT_MIN_SIZE_WIDTH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MIN_SIZE_HEIGHT,
      g_param_spec_int ("min-size-height", "Minimum face height",
          "Minimum area height to be recognized as a face", 0, G_MAXINT,
          DEFAULT_MIN_SIZE_HEIGHT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_UPDATES,
      g_param_spec_enum ("updates", "Updates",
          "When send update bus messages, if at all",
          GST_TYPE_FACE_DETECT_UPDATES, GST_FACEDETECT_UPDATES_EVERY_FRAME,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MIN_STDDEV,
      g_param_spec_int ("min-stddev", "Minimum image standard deviation",
          "Minimum image average standard deviation: on images with standard "
          "deviation lesser than this value facedetection will not be "
          "performed. Setting this property help to save cpu and reduce "
          "false positives not performing face detection on images with "
          "little changes", 0, 255, DEFAULT_MIN_STDDEV,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "facedetect",
      "Filter/Effect/Video",
      "Performs face detection on videos and images, providing detected positions via bus messages",
      "Michael Sheldon <mike@mikeasoft.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  gst_type_mark_as_plugin_api (GST_TYPE_OPENCV_FACE_DETECT_FLAGS, (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_FACE_DETECT_UPDATES, (GstPluginAPIFlags) 0);
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_face_detect_init (GstFaceDetect * filter)
{
  filter->face_profile = g_strdup (DEFAULT_FACE_PROFILE);
  filter->nose_profile = g_strdup (DEFAULT_NOSE_PROFILE);
  filter->mouth_profile = g_strdup (DEFAULT_MOUTH_PROFILE);
  filter->eyes_profile = g_strdup (DEFAULT_EYES_PROFILE);
  filter->display = TRUE;
  filter->face_detected = FALSE;
  filter->scale_factor = DEFAULT_SCALE_FACTOR;
  filter->min_neighbors = DEFAULT_MIN_NEIGHBORS;
  filter->flags = DEFAULT_FLAGS;
  filter->min_size_width = DEFAULT_MIN_SIZE_WIDTH;
  filter->min_size_height = DEFAULT_MIN_SIZE_HEIGHT;
  filter->min_stddev = DEFAULT_MIN_STDDEV;
  filter->cvFaceDetect =
      gst_face_detect_load_profile (filter, filter->face_profile);
  filter->cvNoseDetect =
      gst_face_detect_load_profile (filter, filter->nose_profile);
  filter->cvMouthDetect =
      gst_face_detect_load_profile (filter, filter->mouth_profile);
  filter->cvEyesDetect =
      gst_face_detect_load_profile (filter, filter->eyes_profile);

  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      TRUE);
  filter->updates = GST_FACEDETECT_UPDATES_EVERY_FRAME;
}

static void
gst_face_detect_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFaceDetect *filter = GST_FACE_DETECT (object);

  switch (prop_id) {
    case PROP_FACE_PROFILE:
      g_free (filter->face_profile);
      if (filter->cvFaceDetect)
        delete (filter->cvFaceDetect);
      filter->face_profile = g_value_dup_string (value);
      filter->cvFaceDetect =
          gst_face_detect_load_profile (filter, filter->face_profile);
      break;
    case PROP_NOSE_PROFILE:
      g_free (filter->nose_profile);
      if (filter->cvNoseDetect)
        delete (filter->cvNoseDetect);
      filter->nose_profile = g_value_dup_string (value);
      filter->cvNoseDetect =
          gst_face_detect_load_profile (filter, filter->nose_profile);
      break;
    case PROP_MOUTH_PROFILE:
      g_free (filter->mouth_profile);
      if (filter->cvMouthDetect)
        delete (filter->cvMouthDetect);
      filter->mouth_profile = g_value_dup_string (value);
      filter->cvMouthDetect =
          gst_face_detect_load_profile (filter, filter->mouth_profile);
      break;
    case PROP_EYES_PROFILE:
      g_free (filter->eyes_profile);
      if (filter->cvEyesDetect)
        delete (filter->cvEyesDetect);
      filter->eyes_profile = g_value_dup_string (value);
      filter->cvEyesDetect =
          gst_face_detect_load_profile (filter, filter->eyes_profile);
      break;
    case PROP_DISPLAY:
      filter->display = g_value_get_boolean (value);
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
    case PROP_MIN_STDDEV:
      filter->min_stddev = g_value_get_int (value);
      break;
    case PROP_FLAGS:
      filter->flags = g_value_get_flags (value);
      break;
    case PROP_UPDATES:
      filter->updates = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_face_detect_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstFaceDetect *filter = GST_FACE_DETECT (object);

  switch (prop_id) {
    case PROP_FACE_PROFILE:
      g_value_set_string (value, filter->face_profile);
      break;
    case PROP_NOSE_PROFILE:
      g_value_set_string (value, filter->nose_profile);
      break;
    case PROP_MOUTH_PROFILE:
      g_value_set_string (value, filter->mouth_profile);
      break;
    case PROP_EYES_PROFILE:
      g_value_set_string (value, filter->eyes_profile);
      break;
    case PROP_DISPLAY:
      g_value_set_boolean (value, filter->display);
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
    case PROP_MIN_STDDEV:
      g_value_set_int (value, filter->min_stddev);
      break;
    case PROP_FLAGS:
      g_value_set_flags (value, filter->flags);
      break;
    case PROP_UPDATES:
      g_value_set_enum (value, filter->updates);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_face_detect_set_caps (GstOpencvVideoFilter * transform, gint in_width,
    gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type)
{
  GstFaceDetect *filter;

  filter = GST_FACE_DETECT (transform);

  filter->cvGray.create (Size (in_width, in_height), CV_8UC1);

  return TRUE;
}

static GstMessage *
gst_face_detect_message_new (GstFaceDetect * filter, GstBuffer * buf)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM_CAST (filter);
  GstStructure *s;
  GstClockTime running_time, stream_time;

  running_time = gst_segment_to_running_time (&trans->segment, GST_FORMAT_TIME,
      GST_BUFFER_TIMESTAMP (buf));
  stream_time = gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME,
      GST_BUFFER_TIMESTAMP (buf));

  s = gst_structure_new ("facedetect",
      "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP (buf),
      "stream-time", G_TYPE_UINT64, stream_time,
      "running-time", G_TYPE_UINT64, running_time,
      "duration", G_TYPE_UINT64, GST_BUFFER_DURATION (buf), NULL);

  return gst_message_new_element (GST_OBJECT (filter), s);
}

static void
gst_face_detect_run_detector (GstFaceDetect * filter,
    CascadeClassifier * detector, gint min_size_width,
    gint min_size_height, Rect r, vector < Rect > &faces)
{
  double img_stddev = 0;
  if (filter->min_stddev > 0) {
    Scalar mean, stddev;
    meanStdDev (filter->cvGray, mean, stddev);
    img_stddev = stddev.val[0];
  }
  if (img_stddev >= filter->min_stddev) {
    Mat roi (filter->cvGray, r);
    detector->detectMultiScale (roi, faces, filter->scale_factor,
        filter->min_neighbors, filter->flags, Size (min_size_width,
            min_size_height), Size (0, 0));
  } else {
    GST_LOG_OBJECT (filter,
        "Calculated stddev %f lesser than min_stddev %d, detection not performed",
        img_stddev, filter->min_stddev);
  }
}

/*
 * Performs the face detection
 */
static GstFlowReturn
gst_face_detect_transform_ip (GstOpencvVideoFilter * base, GstBuffer * buf,
    Mat img)
{
  GstFaceDetect *filter = GST_FACE_DETECT (base);

  if (filter->cvFaceDetect) {
    GstMessage *msg = NULL;
    GstStructure *s;
    GValue facelist = { 0 };
    GValue facedata = { 0 };
    vector < Rect > faces;
    vector < Rect > mouth;
    vector < Rect > nose;
    vector < Rect > eyes;
    gboolean post_msg = FALSE;

    cvtColor (img, filter->cvGray, COLOR_RGB2GRAY);

    gst_face_detect_run_detector (filter, filter->cvFaceDetect,
        filter->min_size_width, filter->min_size_height,
        Rect (0, 0,
            filter->cvGray.size ().width, filter->cvGray.size ().height),
        faces);

    switch (filter->updates) {
      case GST_FACEDETECT_UPDATES_EVERY_FRAME:
        post_msg = TRUE;
        break;
      case GST_FACEDETECT_UPDATES_ON_CHANGE:
        if (!faces.empty ()) {
          if (!filter->face_detected)
            post_msg = TRUE;
        } else {
          if (filter->face_detected) {
            post_msg = TRUE;
          }
        }
        break;
      case GST_FACEDETECT_UPDATES_ON_FACE:
        if (!faces.empty ()) {
          post_msg = TRUE;
        } else {
          post_msg = FALSE;
        }
        break;
      case GST_FACEDETECT_UPDATES_NONE:
        post_msg = FALSE;
        break;
      default:
        post_msg = TRUE;
        break;
    }

    filter->face_detected = !faces.empty ()? TRUE : FALSE;

    if (post_msg) {
      msg = gst_face_detect_message_new (filter, buf);
      g_value_init (&facelist, GST_TYPE_LIST);
    }

    for (unsigned int i = 0; i < faces.size (); ++i) {
      Rect r = faces[i];
      guint mw = filter->min_size_width / 8;
      guint mh = filter->min_size_height / 8;
      guint rnx = 0, rny = 0, rnw, rnh;
      guint rmx = 0, rmy = 0, rmw, rmh;
      guint rex = 0, rey = 0, rew, reh;
      guint rhh = r.height / 2;
      gboolean have_nose, have_mouth, have_eyes;

      /* detect face features */

      if (filter->cvNoseDetect) {
        rnx = r.x + r.width / 4;
        rny = r.y + r.height / 4;
        rnw = r.width / 2;
        rnh = rhh;
        gst_face_detect_run_detector (filter, filter->cvNoseDetect, mw, mh,
            Rect (rnx, rny, rnw, rnh), nose);
        have_nose = !nose.empty ();
      } else {
        have_nose = FALSE;
      }

      if (filter->cvMouthDetect) {
        rmx = r.x;
        rmy = r.y + r.height / 2;
        rmw = r.width;
        rmh = rhh;
        gst_face_detect_run_detector (filter, filter->cvMouthDetect, mw,
            mh, Rect (rmx, rmy, rmw, rmh), mouth);
        have_mouth = !mouth.empty ();
      } else {
        have_mouth = FALSE;
      }

      if (filter->cvEyesDetect) {
        rex = r.x;
        rey = r.y;
        rew = r.width;
        reh = rhh;
        gst_face_detect_run_detector (filter, filter->cvEyesDetect, mw, mh,
            Rect (rex, rey, rew, reh), eyes);
        have_eyes = !eyes.empty ();
      } else {
        have_eyes = FALSE;
      }

      GST_LOG_OBJECT (filter,
          "%2d/%2" G_GSIZE_FORMAT
          ": x,y = %4u,%4u: w.h = %4u,%4u : features(e,n,m) = %d,%d,%d", i,
          faces.size (), r.x, r.y, r.width, r.height, have_eyes, have_nose,
          have_mouth);
      if (post_msg) {
        s = gst_structure_new ("face",
            "x", G_TYPE_UINT, r.x,
            "y", G_TYPE_UINT, r.y,
            "width", G_TYPE_UINT, r.width,
            "height", G_TYPE_UINT, r.height, NULL);
        if (have_nose)
          structure_and_message (nose, "nose", rnx, rny, filter, s);
        if (have_mouth)
          structure_and_message (mouth, "mouth", rmx, rmy, filter, s);
        if (have_eyes)
          structure_and_message (eyes, "eyes", rex, rey, filter, s);

        g_value_init (&facedata, GST_TYPE_STRUCTURE);
        g_value_take_boxed (&facedata, s);
        gst_value_list_append_value (&facelist, &facedata);
        g_value_unset (&facedata);
        s = NULL;
      }

      if (filter->display) {
        Point center;
        Size axes;
        gdouble w, h;
        gint cb = 255 - ((i & 3) << 7);
        gint cg = 255 - ((i & 12) << 5);
        gint cr = 255 - ((i & 48) << 3);

        w = r.width / 2;
        h = r.height / 2;
        center.x = cvRound ((r.x + w));
        center.y = cvRound ((r.y + h));
        axes.width = w;
        axes.height = h * 1.25; /* tweak for face form */
        ellipse (img, center, axes, 0, 0, 360, Scalar (cr, cg, cb), 3, 8, 0);

        if (have_nose) {
          Rect sr = nose[0];

          w = sr.width / 2;
          h = sr.height / 2;
          center.x = cvRound ((rnx + sr.x + w));
          center.y = cvRound ((rny + sr.y + h));
          axes.width = w;
          axes.height = h * 1.25;       /* tweak for nose form */
          ellipse (img, center, axes, 0, 0, 360, Scalar (cr, cg, cb), 1, 8, 0);
        }
        if (have_mouth) {
          Rect sr = mouth[0];

          w = sr.width / 2;
          h = sr.height / 2;
          center.x = cvRound ((rmx + sr.x + w));
          center.y = cvRound ((rmy + sr.y + h));
          axes.width = w * 1.5; /* tweak for mouth form */
          axes.height = h;
          ellipse (img, center, axes, 0, 0, 360, Scalar (cr, cg, cb), 1, 8, 0);
        }
        if (have_eyes) {
          Rect sr = eyes[0];

          w = sr.width / 2;
          h = sr.height / 2;
          center.x = cvRound ((rex + sr.x + w));
          center.y = cvRound ((rey + sr.y + h));
          axes.width = w * 1.5; /* tweak for eyes form */
          axes.height = h;
          ellipse (img, center, axes, 0, 0, 360, Scalar (cr, cg, cb), 1, 8, 0);
        }
      }
      gst_buffer_add_video_region_of_interest_meta (buf, "face",
          (guint) r.x, (guint) r.y, (guint) r.width, (guint) r.height);
    }

    if (post_msg) {
      gst_structure_set_value ((GstStructure *) gst_message_get_structure (msg),
          "faces", &facelist);
      g_value_unset (&facelist);
      gst_element_post_message (GST_ELEMENT (filter), msg);
    }
  }

  return GST_FLOW_OK;
}


static CascadeClassifier *
gst_face_detect_load_profile (GstFaceDetect * filter, gchar * profile)
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
