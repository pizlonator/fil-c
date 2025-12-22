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
 * SECTION:element-cvsmooth
 *
 * Smooths the image using thes cvSmooth OpenCV function.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 videotestsrc ! cvsmooth ! videoconvert ! autovideosink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gst/opencv/gstopencvutils.h"
#include "gstcvsmooth.h"
#include <opencv2/imgproc.hpp>


GST_DEBUG_CATEGORY_STATIC (gst_cv_smooth_debug);
#define GST_CAT_DEFAULT gst_cv_smooth_debug

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
  PROP_SMOOTH_TYPE,
  PROP_KERNELWIDTH,
  PROP_KERNELHEIGHT,
  PROP_COLORSIGMA,
  PROP_SPATIALSIGMA,
  PROP_POSITION_X,
  PROP_POSITION_Y,
  PROP_WIDTH,
  PROP_HEIGHT
};

/* blur-no-scale only handle: gray 8bits -> gray 16bits
 * FIXME there is no way in base transform to override pad's getcaps
 * to be property-sensitive, instead of using the template caps as
 * the base caps, this might lead us to negotiating rgb in this
 * smooth type.
 *
 * Keep it deactivated for now.
 */

enum GstCvSmoothMethod
{
  GST_SMOOTH_BLUR = 1,
  GST_SMOOTH_GAUSSIAN = 2,
  GST_SMOOTH_MEDIAN = 3,
  GST_SMOOTH_BILATERAL = 4
};


#define GST_TYPE_CV_SMOOTH_TYPE (gst_cv_smooth_type_get_type ())
static GType
gst_cv_smooth_type_get_type (void)
{
  static GType cv_smooth_type_type = 0;

  static const GEnumValue smooth_types[] = {
    {GST_SMOOTH_BLUR, "CV Blur", "blur"},
    {GST_SMOOTH_GAUSSIAN, "CV Gaussian", "gaussian"},
    {GST_SMOOTH_MEDIAN, "CV Median", "median"},
    {GST_SMOOTH_BILATERAL, "CV Bilateral", "bilateral"},
    {0, NULL, NULL},
  };

  if (!cv_smooth_type_type) {
    cv_smooth_type_type =
        g_enum_register_static ("GstCvSmoothTypeType", smooth_types);
  }
  return cv_smooth_type_type;
}

#define DEFAULT_CV_SMOOTH_TYPE GST_SMOOTH_GAUSSIAN
#define DEFAULT_KERNELWIDTH 3
#define DEFAULT_KERNELHEIGHT 3
#define DEFAULT_COLORSIGMA 0.0
#define DEFAULT_SPATIALSIGMA 0.0
#define DEFAULT_POSITION_X 0
#define DEFAULT_POSITION_Y 0
#define DEFAULT_WIDTH G_MAXINT
#define DEFAULT_HEIGHT G_MAXINT

G_DEFINE_TYPE_WITH_CODE (GstCvSmooth, gst_cv_smooth,
    GST_TYPE_OPENCV_VIDEO_FILTER, GST_DEBUG_CATEGORY_INIT (gst_cv_smooth_debug,
        "cvsmooth", 0, "cvsmooth");
    );
GST_ELEMENT_REGISTER_DEFINE (cvsmooth, "cvsmooth", GST_RANK_NONE,
    GST_TYPE_CV_SMOOTH);

static void gst_cv_smooth_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_cv_smooth_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_cv_smooth_transform_ip (GstOpencvVideoFilter *
    filter, GstBuffer * buf, Mat img);

/* initialize the cvsmooth's class */
static void
gst_cv_smooth_class_init (GstCvSmoothClass * klass)
{
  GObjectClass *gobject_class;
  GstOpencvVideoFilterClass *gstopencvbasefilter_class;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstCaps *caps;
  GstPadTemplate *templ;

  gobject_class = (GObjectClass *) klass;
  gstopencvbasefilter_class = (GstOpencvVideoFilterClass *) klass;

  gobject_class->set_property = gst_cv_smooth_set_property;
  gobject_class->get_property = gst_cv_smooth_get_property;

  gstopencvbasefilter_class->cv_trans_ip_func = gst_cv_smooth_transform_ip;

  g_object_class_install_property (gobject_class, PROP_SMOOTH_TYPE,
      g_param_spec_enum ("type",
          "type",
          "Smooth Type",
          GST_TYPE_CV_SMOOTH_TYPE,
          DEFAULT_CV_SMOOTH_TYPE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS))
      );
  g_object_class_install_property (gobject_class, PROP_KERNELWIDTH,
      g_param_spec_int ("kernel-width", "kernel width",
          "The gaussian kernel width (must be positive and odd)."
          "If type is median, this means the aperture linear size."
          "Check OpenCV docs: http://docs.opencv.org"
          "/2.4/modules/imgproc/doc/filtering.htm",
          1, G_MAXINT, DEFAULT_KERNELWIDTH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_KERNELHEIGHT,
      g_param_spec_int ("kernel-height", "kernel height",
          "The gaussian kernel height (must be positive and odd).",
          0, G_MAXINT, DEFAULT_KERNELHEIGHT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORSIGMA,
      g_param_spec_double ("color", "color (gaussian standard deviation or "
          "color sigma",
          "If type is gaussian, this means the standard deviation."
          "If type is bilateral, this means the color-sigma. If zero, "
          "Default values are used.",
          0, G_MAXDOUBLE, DEFAULT_COLORSIGMA,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_SPATIALSIGMA,
      g_param_spec_double ("spatial", "spatial (spatial sigma, bilateral only)",
          "Only used in bilateral type, means the spatial-sigma.",
          0, G_MAXDOUBLE, DEFAULT_SPATIALSIGMA,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_POSITION_X,
      g_param_spec_int ("position-x", "starting x position for blur",
          "Starting x position for blur (in pixels).",
          0, G_MAXINT, DEFAULT_POSITION_X,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_POSITION_Y,
      g_param_spec_int ("position-y", "starting y position for blur",
          "Starting y position for blur (in pixels).",
          0, G_MAXINT, DEFAULT_POSITION_Y,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_int ("width", "width of area to blur",
          "Width of the area to blur (in pixels).",
          0, G_MAXINT, DEFAULT_WIDTH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_int ("height", "height of area to blur",
          "Height of the area to blur (in pixels).",
          0, G_MAXINT, DEFAULT_HEIGHT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "cvsmooth",
      "Transform/Effect/Video",
      "Applies cvSmooth OpenCV function to the image",
      "Thiago Santos<thiago.sousa.santos@collabora.co.uk>");

  /* add sink and source pad templates */
  caps = gst_opencv_caps_from_cv_image_type (CV_8UC3);
  gst_caps_append (caps, gst_opencv_caps_from_cv_image_type (CV_8UC1));
  templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      gst_caps_ref (caps));
  gst_element_class_add_pad_template (element_class, templ);
  templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
  gst_element_class_add_pad_template (element_class, templ);
  gst_caps_unref (caps);

  gst_type_mark_as_plugin_api (GST_TYPE_CV_SMOOTH_TYPE, (GstPluginAPIFlags) 0);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_cv_smooth_init (GstCvSmooth * filter)
{
  filter->type = DEFAULT_CV_SMOOTH_TYPE;
  filter->kernelwidth = DEFAULT_KERNELWIDTH;
  filter->kernelheight = DEFAULT_KERNELHEIGHT;
  filter->colorsigma = DEFAULT_COLORSIGMA;
  filter->spatialsigma = DEFAULT_SPATIALSIGMA;
  filter->positionx = DEFAULT_POSITION_X;
  filter->positiony = DEFAULT_POSITION_Y;
  filter->width = DEFAULT_WIDTH;
  filter->height = DEFAULT_HEIGHT;

  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      TRUE);
}

static void
gst_cv_smooth_change_type (GstCvSmooth * filter, gint value)
{
  GST_DEBUG_OBJECT (filter, "Changing type from %d to %d", filter->type, value);
  if (filter->type == value)
    return;

  filter->type = value;
  switch (value) {
    case GST_SMOOTH_GAUSSIAN:
    case GST_SMOOTH_BLUR:
      gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST
          (filter), TRUE);
      break;
    default:
      gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST
          (filter), FALSE);
      break;
  }
}

static void
gst_cv_smooth_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCvSmooth *filter = GST_CV_SMOOTH (object);

  switch (prop_id) {
    case PROP_SMOOTH_TYPE:
      gst_cv_smooth_change_type (filter, g_value_get_enum (value));
      break;
    case PROP_KERNELWIDTH:{
      gint prop = g_value_get_int (value);

      if (prop % 2 == 1) {
        filter->kernelwidth = prop;
      } else {
        GST_WARNING_OBJECT (filter, "Ignoring value for kernel-width, not odd"
            "(%d)", prop);
      }
    }
      break;
    case PROP_KERNELHEIGHT:{
      gint prop = g_value_get_int (value);

      if (prop % 2 == 1) {
        filter->kernelheight = prop;
      } else {
        GST_WARNING_OBJECT (filter, "Ignoring value for kernel-height, not odd"
            " nor zero (%d)", prop);
      }
    }
      break;
    case PROP_COLORSIGMA:
      filter->colorsigma = g_value_get_double (value);
      break;
    case PROP_SPATIALSIGMA:
      filter->spatialsigma = g_value_get_double (value);
      break;
    case PROP_POSITION_X:
      filter->positionx = g_value_get_int (value);
      break;
    case PROP_POSITION_Y:
      filter->positiony = g_value_get_int (value);
      break;
    case PROP_WIDTH:
      filter->width = g_value_get_int (value);
      break;
    case PROP_HEIGHT:
      filter->height = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cv_smooth_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCvSmooth *filter = GST_CV_SMOOTH (object);

  switch (prop_id) {
    case PROP_SMOOTH_TYPE:
      g_value_set_enum (value, filter->type);
      break;
    case PROP_KERNELWIDTH:
      g_value_set_int (value, filter->kernelwidth);
      break;
    case PROP_KERNELHEIGHT:
      g_value_set_int (value, filter->kernelheight);
      break;
    case PROP_COLORSIGMA:
      g_value_set_double (value, filter->colorsigma);
      break;
    case PROP_SPATIALSIGMA:
      g_value_set_double (value, filter->spatialsigma);
      break;
    case PROP_POSITION_X:
      g_value_set_int (value, filter->positionx);
      break;
    case PROP_POSITION_Y:
      g_value_set_int (value, filter->positiony);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, filter->width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, filter->height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_cv_smooth_transform_ip (GstOpencvVideoFilter * base, GstBuffer * buf,
    Mat img)
{
  GstCvSmooth *filter = GST_CV_SMOOTH (base);

  if (filter->positionx != 0 || filter->positiony != 0 ||
      filter->width != G_MAXINT || filter->height != G_MAXINT) {
    Size mat_size = img.size ();

    /* if the effect would start outside the image, just skip it */
    if (filter->positionx >= mat_size.width
        || filter->positiony >= mat_size.height)
      return GST_FLOW_OK;
    /* explicitly account for empty area */
    if (filter->width <= 0 || filter->height <= 0)
      return GST_FLOW_OK;

    Rect mat_rect (filter->positionx,
        filter->positiony,
        MIN (filter->width, mat_size.width - filter->positionx),
        MIN (filter->height, mat_size.height - filter->positiony));

    img = img (mat_rect);
  }

  switch (filter->type) {
    case GST_SMOOTH_BLUR:
      blur (img, img, Size (filter->kernelwidth, filter->kernelheight),
          Point (-1, -1));
      break;
    case GST_SMOOTH_GAUSSIAN:
      GaussianBlur (img, img, Size (filter->kernelwidth, filter->kernelheight),
          filter->colorsigma, filter->colorsigma);
      break;
    case GST_SMOOTH_MEDIAN:
      medianBlur (img, img, filter->kernelwidth);
      break;
    case GST_SMOOTH_BILATERAL:
      bilateralFilter (img, img, -1, filter->colorsigma, 0.0);
      break;
    default:
      break;
  }

  return GST_FLOW_OK;
}
