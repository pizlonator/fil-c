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
 * SECTION:element-skindetect
 *
 * Human skin detection on videos and images
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 videotestsrc ! decodebin ! videoconvert ! skindetect ! videoconvert ! xvimagesink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstskindetect.h"
#include <opencv2/imgproc.hpp>

GST_DEBUG_CATEGORY_STATIC (gst_skin_detect_debug);
#define GST_CAT_DEFAULT gst_skin_detect_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_POSTPROCESS,
  PROP_METHOD,
  PROP_MASK
};
typedef enum
{
  HSV,
  RGB
} GstSkindetectMethod;

#define GST_TYPE_SKIN_DETECT_METHOD (gst_skin_detect_method_get_type ())
static GType
gst_skin_detect_method_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      {HSV, "Classic HSV thresholding", "hsv"},
      {RGB, "Normalised-RGB colorspace thresholding", "rgb"},
      {0, NULL, NULL},
    };
    etype = g_enum_register_static ("GstSkindetectMethod", values);
  }
  return etype;
}

G_DEFINE_TYPE_WITH_CODE (GstSkinDetect, gst_skin_detect,
    GST_TYPE_OPENCV_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_skin_detect_debug, "skindetect", 0,
        "Performs skin detection on videos and images");
    );
GST_ELEMENT_REGISTER_DEFINE (skindetect, "skindetect", GST_RANK_NONE,
    GST_TYPE_SKIN_DETECT);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB")));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB")));


static void gst_skin_detect_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_skin_detect_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_skin_detect_transform (GstOpencvVideoFilter * filter,
    GstBuffer * buf, cv::Mat img, GstBuffer * outbuf, cv::Mat outimg);

static void gst_skin_detect_finalize (GObject * object);
static gboolean
gst_skin_detect_set_caps (GstOpencvVideoFilter * transform,
    gint in_width, gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type);

/* initialize the skindetect's class */
static void
gst_skin_detect_class_init (GstSkinDetectClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstOpencvVideoFilterClass *gstopencvbasefilter_class;

  gobject_class = (GObjectClass *) klass;
  gstopencvbasefilter_class = (GstOpencvVideoFilterClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_skin_detect_finalize);
  gobject_class->set_property = gst_skin_detect_set_property;
  gobject_class->get_property = gst_skin_detect_get_property;

  gstopencvbasefilter_class->cv_trans_func = gst_skin_detect_transform;

  g_object_class_install_property (gobject_class, PROP_POSTPROCESS,
      g_param_spec_boolean ("postprocess", "Postprocess",
          "Apply opening-closing to skin detection to extract large, significant blobs ",
          TRUE, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_METHOD,
      g_param_spec_enum ("method",
          "Method to use",
          "Method to use",
          GST_TYPE_SKIN_DETECT_METHOD, HSV,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "skindetect",
      "Filter/Effect/Video",
      "Performs non-parametric skin detection on input",
      "Miguel Casas-Sanchez <miguelecasassanchez@gmail.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  gstopencvbasefilter_class->cv_set_caps = gst_skin_detect_set_caps;

  gst_type_mark_as_plugin_api (GST_TYPE_SKIN_DETECT_METHOD, (GstPluginAPIFlags) 0);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_skin_detect_init (GstSkinDetect * filter)
{
  filter->postprocess = TRUE;
  filter->method = HSV;

  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      FALSE);
}


static void
gst_skin_detect_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSkinDetect *filter = GST_SKIN_DETECT (object);

  switch (prop_id) {
    case PROP_POSTPROCESS:
      filter->postprocess = g_value_get_boolean (value);
      break;
    case PROP_METHOD:
      filter->method = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_skin_detect_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSkinDetect *filter = GST_SKIN_DETECT (object);

  switch (prop_id) {
    case PROP_POSTPROCESS:
      g_value_set_boolean (value, filter->postprocess);
      break;
    case PROP_METHOD:
      g_value_set_enum (value, filter->method);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */
/* this function handles the link with other elements */
static gboolean
gst_skin_detect_set_caps (GstOpencvVideoFilter * transform,
    gint in_width, gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type)
{
  GstSkinDetect *filter = GST_SKIN_DETECT (transform);
  cv::Size size = cv::Size (in_width, in_height);

  filter->cvRGB.create (size, CV_8UC3);
  filter->cvChA.create (size, CV_8UC1);
  filter->width = in_width;
  filter->height = in_height;

  filter->cvHSV.create (size, CV_8UC3);
  filter->cvH.create (size, CV_8UC1);   /*  Hue component. */
  filter->cvH2.create (size, CV_8UC1);  /*  Hue component, 2nd threshold */
  filter->cvS.create (size, CV_8UC1);   /*  Saturation component. */
  filter->cvV.create (size, CV_8UC1);   /*  Brightness component. */
  filter->cvSkinPixels1.create (size, CV_8UC1); /*  Greyscale output image */

  filter->cvR.create (size, CV_8UC1);   /*  R component. */
  filter->cvG.create (size, CV_8UC1);   /*  G component. */
  filter->cvB.create (size, CV_8UC1);   /*  B component. */
  filter->cvAll.create (size, CV_32FC1);        /*  (R+G+B) component. */
  filter->cvR2.create (size, CV_32FC1); /*  R component, 32bits */
  filter->cvRp.create (size, CV_32FC1); /*  R' and >0.4 */
  filter->cvGp.create (size, CV_32FC1); /*  G' and > 0.28 */
  filter->cvRp2.create (size, CV_32FC1);        /*  R' <0.6 */
  filter->cvGp2.create (size, CV_32FC1);        /*  G' <0.4 */
  filter->cvSkinPixels2.create (size, CV_32FC1);        /*  Greyscale output image. */
  filter->cvdraft.create (size, CV_8UC1);       /*  Greyscale output image. */

  return TRUE;
}

/* Clean up */
static void
gst_skin_detect_finalize (GObject * object)
{
  GstSkinDetect *filter = GST_SKIN_DETECT (object);

  filter->cvRGB.release ();
  filter->cvChA.release ();
  filter->cvHSV.release ();
  filter->cvH.release ();
  filter->cvH2.release ();
  filter->cvS.release ();
  filter->cvV.release ();
  filter->cvSkinPixels1.release ();
  filter->cvR.release ();
  filter->cvG.release ();
  filter->cvB.release ();
  filter->cvAll.release ();
  filter->cvR2.release ();
  filter->cvRp.release ();
  filter->cvGp.release ();
  filter->cvRp2.release ();
  filter->cvGp2.release ();
  filter->cvdraft.release ();
  filter->cvSkinPixels2.release ();

  G_OBJECT_CLASS (gst_skin_detect_parent_class)->finalize (object);
}

static GstFlowReturn
gst_skin_detect_transform (GstOpencvVideoFilter * base, GstBuffer * buf,
    cv::Mat img, GstBuffer * outbuf, cv::Mat outimg)
{
  GstSkinDetect *filter = GST_SKIN_DETECT (base);

  std::vector < cv::Mat > channels (3);
  filter->cvRGB = cv::Mat (img);

  /* SKIN COLOUR BLOB DETECTION */
  if (HSV == filter->method) {
    cv::cvtColor (filter->cvRGB, filter->cvHSV, cv::COLOR_RGB2HSV);
    cv::split (filter->cvHSV, channels);
    filter->cvH = channels.at (0);
    filter->cvS = channels.at (1);
    filter->cvV = channels.at (2);

    /*  Detect which pixels in each of the H, S and V channels are probably skin pixels.
       Assume that skin has a Hue between 0 to 18 (out of 180), and Saturation above 50, and Brightness above 80. */
    cv::threshold (filter->cvH, filter->cvH2, 10, UCHAR_MAX, cv::THRESH_BINARY);        /* (hue > 10) */
    cv::threshold (filter->cvH, filter->cvH, 20, UCHAR_MAX, cv::THRESH_BINARY_INV);     /* (hue < 20) */
    cv::threshold (filter->cvS, filter->cvS, 48, UCHAR_MAX, cv::THRESH_BINARY); /* (sat > 48) */
    cv::threshold (filter->cvV, filter->cvV, 80, UCHAR_MAX, cv::THRESH_BINARY); /* (val > 80) */

    /*  erode the HUE to get rid of noise. */
    cv::erode (filter->cvH, filter->cvH, cv::Mat (), cv::Point (-1, -1), 1);

    /*  Combine all 3 thresholded color components, so that an output pixel will only
       be white (255) if the H, S and V pixels were also white.
       imageSkin = (hue > 10) ^ (hue < 20) ^ (sat > 48) ^ (val > 80), where   ^ mean pixels-wise AND */
    cv::bitwise_and (filter->cvH, filter->cvS, filter->cvSkinPixels1);
    cv::bitwise_and (filter->cvSkinPixels1, filter->cvH2,
        filter->cvSkinPixels1);
    cv::bitwise_and (filter->cvSkinPixels1, filter->cvV, filter->cvSkinPixels1);

    cv::cvtColor (filter->cvSkinPixels1, filter->cvRGB, cv::COLOR_GRAY2RGB);
  } else if (RGB == filter->method) {
    cv::split (filter->cvRGB, channels);
    filter->cvR = channels.at (0);
    filter->cvG = channels.at (1);
    filter->cvB = channels.at (2);
    cv::add (filter->cvR, filter->cvG, filter->cvAll);
    cv::add (filter->cvB, filter->cvAll, filter->cvAll);        /*  All = R + G + B */
    cv::divide (filter->cvR, filter->cvAll, filter->cvRp, 1.0, filter->cvRp.type ());   /*  R' = R / ( R + G + B) */
    cv::divide (filter->cvG, filter->cvAll, filter->cvGp, 1.0, filter->cvGp.type ());   /*  G' = G / ( R + G + B) */

    filter->cvR.convertTo (filter->cvR2, filter->cvR2.type (), 1.0, 0.0);
    filter->cvGp.copyTo (filter->cvGp2);
    filter->cvRp.copyTo (filter->cvRp2);

    cv::threshold (filter->cvR2, filter->cvR2, 60, UCHAR_MAX, cv::THRESH_BINARY);       /* (R > 60) */
    cv::threshold (filter->cvRp, filter->cvRp, 0.42, UCHAR_MAX, cv::THRESH_BINARY);     /* (R'> 0.4) */
    cv::threshold (filter->cvRp2, filter->cvRp2, 0.6, UCHAR_MAX, cv::THRESH_BINARY_INV);        /* (R'< 0.6) */
    cv::threshold (filter->cvGp, filter->cvGp, 0.28, UCHAR_MAX, cv::THRESH_BINARY);     /* (G'> 0.28) */
    cv::threshold (filter->cvGp2, filter->cvGp2, 0.4, UCHAR_MAX, cv::THRESH_BINARY_INV);        /* (G'< 0.4) */

    /*  Combine all 3 thresholded color components, so that an output pixel will only
       be white (255) if the H, S and V pixels were also white. */

    cv::bitwise_and (filter->cvR2, filter->cvRp, filter->cvSkinPixels2);
    cv::bitwise_and (filter->cvRp, filter->cvSkinPixels2,
        filter->cvSkinPixels2);
    cv::bitwise_and (filter->cvRp2, filter->cvSkinPixels2,
        filter->cvSkinPixels2);
    cv::bitwise_and (filter->cvGp, filter->cvSkinPixels2,
        filter->cvSkinPixels2);
    cv::bitwise_and (filter->cvGp2, filter->cvSkinPixels2,
        filter->cvSkinPixels2);

    filter->cvSkinPixels2.convertTo (filter->cvdraft, filter->cvdraft.type (),
        1.0, 0.0);
    cv::cvtColor (filter->cvdraft, filter->cvRGB, cv::COLOR_GRAY2RGB);
  }

  /* After this we have a RGB Black and white image with the skin, in
     filter->cvRGB. We can postprocess by applying 1 erode-dilate and 1
     dilate-erode, or alternatively 1 opening-closing all together, with
     the goal of removing small (spurious) skin spots and creating large
     connected areas */
  if (filter->postprocess) {
    cv::split (filter->cvRGB, channels);
    filter->cvChA = channels.at (0);

    cv::Mat element =
        cv::getStructuringElement (cv::MORPH_RECT, cv::Size (3, 3),
        cv::Point (1, 1));
    cv::erode (filter->cvChA, filter->cvChA, element, cv::Point (1, 1), 1);
    cv::dilate (filter->cvChA, filter->cvChA, element, cv::Point (1, 1), 2);
    cv::erode (filter->cvChA, filter->cvChA, element, cv::Point (1, 1), 1);

    cv::cvtColor (filter->cvChA, filter->cvRGB, cv::COLOR_GRAY2RGB);
  }

  filter->cvRGB.copyTo (outimg);

  return GST_FLOW_OK;
}
