/*
 * GStreamer
 * Copyright (C) 2013 Miguel Casas-Sanchez <miguelecasassanchez@gmail.com>
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
 * SECTION:element-retinex
 *
 * Basic and multiscale retinex for colour image enhancement, see article:
 *
 * Rahman, Zia-ur, Daniel J. Jobson, and Glenn A. Woodell. "Multi-scale retinex for
 * color image enhancement." Image Processing, 1996. Proceedings., International
 * Conference on. Vol. 3. IEEE, 1996.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 videotestsrc ! videoconvert ! retinex ! videoconvert ! xvimagesink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstretinex.h"
#include <opencv2/imgproc.hpp>

GST_DEBUG_CATEGORY_STATIC (gst_retinex_debug);
#define GST_CAT_DEFAULT gst_retinex_debug

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
  PROP_METHOD,
  PROP_SCALES,
  PROP_SIGMA,
  PROP_GAIN,
  PROP_OFFSET,
};
typedef enum
{
  METHOD_BASIC,
  METHOD_MULTISCALE
} GstRetinexMethod;

#define DEFAULT_METHOD METHOD_BASIC
#define DEFAULT_SCALES 3
#define DEFAULT_SIGMA 14.0
#define DEFAULT_GAIN 128
#define DEFAULT_OFFSET 128

#define GST_TYPE_RETINEX_METHOD (gst_retinex_method_get_type ())
static GType
gst_retinex_method_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      {METHOD_BASIC, "Basic retinex restoration", "basic"},
      {METHOD_MULTISCALE, "Mutiscale retinex restoration", "multiscale"},
      {0, NULL, NULL},
    };
    etype = g_enum_register_static ("GstRetinexMethod", values);
  }
  return etype;
}

G_DEFINE_TYPE_WITH_CODE (GstRetinex, gst_retinex, GST_TYPE_OPENCV_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_retinex_debug, "retinex", 0,
        "Multiscale retinex for colour image enhancement");
    );
GST_ELEMENT_REGISTER_DEFINE (retinex, "retinex", GST_RANK_NONE,
    GST_TYPE_RETINEX);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB")));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB")));


static void gst_retinex_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_retinex_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_retinex_transform_ip (GstOpencvVideoFilter * filter,
    GstBuffer * buff, Mat img);
static gboolean gst_retinex_set_caps (GstOpencvVideoFilter * btrans,
    gint in_width, gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type);

static void gst_retinex_finalize (GObject * object);

/* initialize the retinex's class */
static void
gst_retinex_class_init (GstRetinexClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstOpencvVideoFilterClass *cvbasefilter_class =
      (GstOpencvVideoFilterClass *) klass;

  gobject_class->finalize = gst_retinex_finalize;
  gobject_class->set_property = gst_retinex_set_property;
  gobject_class->get_property = gst_retinex_get_property;

  cvbasefilter_class->cv_trans_ip_func = gst_retinex_transform_ip;
  cvbasefilter_class->cv_set_caps = gst_retinex_set_caps;

  g_object_class_install_property (gobject_class, PROP_METHOD,
      g_param_spec_enum ("method",
          "Retinex method to use",
          "Retinex method to use",
          GST_TYPE_RETINEX_METHOD, DEFAULT_METHOD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SCALES,
      g_param_spec_int ("scales", "scales",
          "Amount of gaussian filters (scales) used in multiscale retinex", 1,
          4, DEFAULT_SCALES,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstRetinex:sigma:
   *
   * Sigma
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_SIGMA,
      g_param_spec_double ("sigma", "Sigma",
			   "Sigma", 0.0, G_MAXDOUBLE, DEFAULT_SIGMA,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstRetinex:gain:
   *
   * Gain
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_GAIN,
      g_param_spec_int ("gain", "gain",
			"Gain", 0, G_MAXINT, DEFAULT_GAIN,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstRetinex:offset:
   *
   * Offset
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_OFFSET,
      g_param_spec_int ("offset", "Offset",
			"Offset", 0, G_MAXINT, DEFAULT_OFFSET,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "Retinex image colour enhancement", "Filter/Effect/Video",
      "Multiscale retinex for colour image enhancement",
      "Miguel Casas-Sanchez <miguelecasassanchez@gmail.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  gst_type_mark_as_plugin_api (GST_TYPE_RETINEX_METHOD, (GstPluginAPIFlags) 0);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_retinex_init (GstRetinex * filter)
{
  filter->method = DEFAULT_METHOD;
  filter->scales = DEFAULT_SCALES;
  filter->current_scales = 0;
  filter->gain = DEFAULT_GAIN;
  filter->offset = DEFAULT_OFFSET;
  filter->sigma = DEFAULT_SIGMA;
  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      TRUE);
}

static void
gst_retinex_finalize (GObject * object)
{
  GstRetinex *filter;
  filter = GST_RETINEX (object);

  filter->cvA.release ();
  filter->cvB.release ();
  filter->cvC.release ();
  filter->cvD.release ();
  g_free (filter->weights);
  filter->weights = NULL;
  g_free (filter->sigmas);
  filter->sigmas = NULL;

  G_OBJECT_CLASS (gst_retinex_parent_class)->finalize (object);
}

static void
gst_retinex_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRetinex *retinex = GST_RETINEX (object);

  switch (prop_id) {
    case PROP_METHOD:
      retinex->method = g_value_get_enum (value);
      break;
    case PROP_SCALES:
      retinex->scales = g_value_get_int (value);
      break;
  case PROP_SIGMA:
    retinex->sigma = g_value_get_double (value);
    break;
    case PROP_GAIN:
      retinex->gain = g_value_get_int (value);
      break;
    case PROP_OFFSET:
      retinex->offset = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_retinex_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRetinex *filter = GST_RETINEX (object);

  switch (prop_id) {
    case PROP_METHOD:
      g_value_set_enum (value, filter->method);
      break;
    case PROP_SCALES:
      g_value_set_int (value, filter->scales);
      break;
    case PROP_SIGMA:
      g_value_set_double (value, filter->sigma);
      break;
    case PROP_GAIN:
      g_value_set_int (value, filter->gain);
      break;
    case PROP_OFFSET:
      g_value_set_int (value, filter->offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_retinex_set_caps (GstOpencvVideoFilter * filter, gint in_width,
    gint in_height, int in_cv_type, gint out_width,
    gint out_height, int out_cv_type)
{
  GstRetinex *retinex = GST_RETINEX (filter);
  Size size;

  size = Size (in_width, in_height);

  retinex->cvA.create (size, CV_32FC3);
  retinex->cvB.create (size, CV_32FC3);
  retinex->cvC.create (size, CV_32FC3);
  retinex->cvD.create (size, CV_32FC3);

  return TRUE;
}

static GstFlowReturn
gst_retinex_transform_ip (GstOpencvVideoFilter * filter, GstBuffer * buf,
    Mat img)
{
  GstRetinex *retinex = GST_RETINEX (filter);
  int filter_size;

  /* Basic retinex restoration.  The image and a filtered image are converted
     to the log domain and subtracted.
     O = Log(I) - Log(H(I))
     where O is the output, H is a gaussian 2d filter and I is the input image. */
  if (METHOD_BASIC == retinex->method) {
    /*  Compute log image */
    img.convertTo (retinex->cvA, retinex->cvA.type ());
    log (retinex->cvA, retinex->cvB);

    /*  Compute log of blurred image */
    filter_size = (int) floor (retinex->sigma * 6) / 2;
    filter_size = filter_size * 2 + 1;

    img.convertTo (retinex->cvD, retinex->cvD.type ());
    GaussianBlur (retinex->cvD, retinex->cvD, Size (filter_size, filter_size),
        0.0, 0.0);
    log (retinex->cvD, retinex->cvC);

    /*  Compute difference */
    subtract (retinex->cvB, retinex->cvC, retinex->cvA);

    /*  Restore */
    retinex->cvA.convertTo (img, img.type (), (float) retinex->gain, (float) retinex->offset);
  }
  /* Multiscale retinex restoration.  The image and a set of filtered images are
     converted to the log domain and subtracted from the original with some set
     of weights. Typically called with three equally weighted scales of fine,
     medium and wide standard deviations.
     O = Log(I) - sum_i [ wi * Log(H(I)) ]
     where O is the output, H is a gaussian 2d filter and I is the input image
     sum_i means summatory on var i with i in [0..scales) and wi are the weights */
  else if (METHOD_MULTISCALE == retinex->method) {
    int i;

    /* allocate or reallocate the weights and sigmas according to scales */
    if (retinex->current_scales != retinex->scales || !retinex->sigmas) {
      retinex->weights =
          (double *) g_realloc (retinex->weights,
          sizeof (double) * retinex->scales);
      retinex->sigmas =
          (double *) g_realloc (retinex->sigmas,
          sizeof (double) * retinex->scales);
      for (i = 0; i < retinex->scales; i++) {
        retinex->weights[i] = 1.0 / (double) retinex->scales;
        retinex->sigmas[i] = 10.0 + 4.0 * (double) retinex->scales;
      }
      retinex->current_scales = retinex->scales;
    }

    /*  Compute log image */
    img.convertTo (retinex->cvA, retinex->cvA.type ());
    log (retinex->cvA, retinex->cvB);

    /*  Filter at each scale */
    for (i = 0; i < retinex->scales; i++) {
      filter_size = (int) floor (retinex->sigmas[i] * 6) / 2;
      filter_size = filter_size * 2 + 1;

      img.convertTo (retinex->cvD, retinex->cvD.type ());
      GaussianBlur (retinex->cvD, retinex->cvD, Size (filter_size, filter_size),
          0.0, 0.0);
      log (retinex->cvD, retinex->cvC);

      /*  Compute weighted difference */
      retinex->cvC.convertTo (retinex->cvC, -1, retinex->weights[i], 0.0);
      subtract (retinex->cvB, retinex->cvC, retinex->cvB);
    }

    /*  Restore */
    retinex->cvB.convertTo (img, img.type (), (float) retinex->gain, (float) retinex->offset);
  }

  return GST_FLOW_OK;
}
