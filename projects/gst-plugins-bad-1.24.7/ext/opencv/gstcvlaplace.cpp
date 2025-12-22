/*
 * GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
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
 * SECTION:element-cvlaplace
 *
 * Applies cvLaplace OpenCV function to the image.
 *
 * Example launch line
 *
 * |[
 * gst-launch-1.0 videotestsrc ! cvlaplace ! videoconvert ! autovideosink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstcvlaplace.h"
#include <opencv2/imgproc.hpp>


GST_DEBUG_CATEGORY_STATIC (gst_cv_laplace_debug);
#define GST_CAT_DEFAULT gst_cv_laplace_debug

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

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};
enum
{
  PROP_0,
  PROP_APERTURE_SIZE,
  PROP_SCALE,
  PROP_SHIFT,
  PROP_MASK
};

#define DEFAULT_APERTURE_SIZE 3
#define DEFAULT_SCALE_FACTOR 1.0
#define DEFAULT_SHIFT 0.0
#define DEFAULT_MASK TRUE

G_DEFINE_TYPE_WITH_CODE (GstCvLaplace, gst_cv_laplace,
    GST_TYPE_OPENCV_VIDEO_FILTER, GST_DEBUG_CATEGORY_INIT (gst_cv_laplace_debug,
        "cvlaplace", 0, "cvlaplace");
    );
GST_ELEMENT_REGISTER_DEFINE (cvlaplace, "cvlaplace", GST_RANK_NONE,
    GST_TYPE_CV_LAPLACE);
static void gst_cv_laplace_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_cv_laplace_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_cv_laplace_transform (GstOpencvVideoFilter * filter,
    GstBuffer * buf, cv::Mat img, GstBuffer * outbuf, cv::Mat outimg);

static gboolean gst_cv_laplace_cv_set_caps (GstOpencvVideoFilter * trans,
    gint in_width, gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type);

/* Clean up */
static void
gst_cv_laplace_finalize (GObject * obj)
{
  GstCvLaplace *filter = GST_CV_LAPLACE (obj);

  filter->intermediary_img.release ();
  filter->cvGray.release ();
  filter->Laplace.release ();

  G_OBJECT_CLASS (gst_cv_laplace_parent_class)->finalize (obj);
}

/* initialize the cvlaplace's class */
static void
gst_cv_laplace_class_init (GstCvLaplaceClass * klass)
{
  GObjectClass *gobject_class;
  GstOpencvVideoFilterClass *gstopencvbasefilter_class;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gobject_class = (GObjectClass *) klass;
  gstopencvbasefilter_class = (GstOpencvVideoFilterClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_cv_laplace_finalize);
  gobject_class->set_property = gst_cv_laplace_set_property;
  gobject_class->get_property = gst_cv_laplace_get_property;

  gstopencvbasefilter_class->cv_trans_func = gst_cv_laplace_transform;
  gstopencvbasefilter_class->cv_set_caps = gst_cv_laplace_cv_set_caps;

  g_object_class_install_property (gobject_class, PROP_APERTURE_SIZE,
      g_param_spec_int ("aperture-size", "aperture size",
          "Size of the extended Laplace Kernel (1, 3, 5 or 7)", 1, 7,
          DEFAULT_APERTURE_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_SCALE,
      g_param_spec_double ("scale", "scale factor", "Scale factor", 0.0,
          G_MAXDOUBLE, DEFAULT_SCALE_FACTOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_SHIFT,
      g_param_spec_double ("shift", "Shift",
          "Value added to the scaled source array elements", 0.0, G_MAXDOUBLE,
          DEFAULT_SHIFT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MASK,
      g_param_spec_boolean ("mask", "Mask",
          "Sets whether the detected edges should be used as a mask on the original input or not",
          DEFAULT_MASK,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  gst_element_class_set_static_metadata (element_class,
      "cvlaplace",
      "Transform/Effect/Video",
      "Applies cvLaplace OpenCV function to the image",
      "Thiago Santos<thiago.sousa.santos@collabora.co.uk>");
}

static void
gst_cv_laplace_init (GstCvLaplace * filter)
{
  filter->aperture_size = DEFAULT_APERTURE_SIZE;
  filter->scale = DEFAULT_SCALE_FACTOR;
  filter->shift = DEFAULT_SHIFT;
  filter->mask = DEFAULT_MASK;

  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      FALSE);
}

static gboolean
gst_cv_laplace_cv_set_caps (GstOpencvVideoFilter * trans, gint in_width,
    gint in_height, int in_cv_type, gint out_width,
    gint out_height, int out_cv_type)
{
  GstCvLaplace *filter = GST_CV_LAPLACE (trans);

  filter->intermediary_img.create (cv::Size (out_width, out_height), CV_16SC1);
  filter->cvGray.create (cv::Size (in_width, in_height), CV_8UC1);
  filter->Laplace.create (cv::Size (in_width, in_height), CV_8UC1);

  return TRUE;
}

static void
gst_cv_laplace_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCvLaplace *filter = GST_CV_LAPLACE (object);

  switch (prop_id) {
    case PROP_APERTURE_SIZE:{
      gint as = g_value_get_int (value);

      if (as % 2 != 1) {
        GST_WARNING_OBJECT (filter, "Invalid value %d for aperture size", as);
      } else
        filter->aperture_size = g_value_get_int (value);
    }
      break;
    case PROP_SCALE:
      filter->scale = g_value_get_double (value);
      break;
    case PROP_SHIFT:
      filter->shift = g_value_get_double (value);
      break;
    case PROP_MASK:
      filter->mask = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cv_laplace_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCvLaplace *filter = GST_CV_LAPLACE (object);

  switch (prop_id) {
    case PROP_APERTURE_SIZE:
      g_value_set_int (value, filter->aperture_size);
      break;
    case PROP_SCALE:
      g_value_set_double (value, filter->scale);
      break;
    case PROP_SHIFT:
      g_value_set_double (value, filter->shift);
      break;
    case PROP_MASK:
      g_value_set_boolean (value, filter->mask);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_cv_laplace_transform (GstOpencvVideoFilter * base, GstBuffer * buf,
    cv::Mat img, GstBuffer * outbuf, cv::Mat outimg)
{
  GstCvLaplace *filter = GST_CV_LAPLACE (base);

  cv::cvtColor (img, filter->cvGray, cv::COLOR_RGB2GRAY);
  cv::Laplacian (filter->cvGray, filter->intermediary_img,
      filter->intermediary_img.depth (), filter->aperture_size);
  filter->intermediary_img.convertTo (filter->Laplace, filter->Laplace.type (),
      filter->scale, filter->shift);

  outimg.setTo (cv::Scalar::all (0));
  if (filter->mask) {
    img.copyTo (outimg, filter->Laplace);
  } else {
    cv::cvtColor (filter->Laplace, outimg, cv::COLOR_GRAY2RGB);
  }

  return GST_FLOW_OK;
}
