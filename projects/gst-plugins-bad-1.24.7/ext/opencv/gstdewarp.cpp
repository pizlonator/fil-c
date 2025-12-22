/*
 * GStreamer
 * Copyright (C) 2016 - 2018 Prassel S.r.l
 *  Author: Nicola Murino <nicola.murino@gmail.com>
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
 * SECTION:element-dewarp
 *
 * Dewarp fisheye images
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 videotestsrc ! videoconvert ! circle radius=0.1 height=80  ! dewarp outer-radius=0.35 inner-radius=0.1 ! videoconvert ! xvimagesink
 * ]|
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstdewarp.h"
#include <math.h>

GST_DEBUG_CATEGORY_STATIC (gst_dewarp_debug);
#define GST_CAT_DEFAULT gst_dewarp_debug

enum
{
  PROP_0,
  PROP_X_CENTER,
  PROP_Y_CENTER,
  PROP_INNER_RADIUS,
  PROP_OUTER_RADIUS,
  PROP_REMAP_X_CORRECTION,
  PROP_REMAP_Y_CORRECTION,
  PROP_DISPLAY_MODE,
  PROP_INTERPOLATION_MODE
};

#define DEFAULT_CENTER 			0.5
#define DEFAULT_RADIUS 			0.0
#define DEFAULT_REMAP_CORRECTION	1.0

#define GST_TYPE_DEWARP_DISPLAY_MODE (dewarp_display_mode_get_type ())

static GType
dewarp_display_mode_get_type (void)
{
  static GType dewarp_display_mode_type = 0;
  static const GEnumValue dewarp_display_mode[] = {
    {GST_DEWARP_DISPLAY_PANORAMA, "Single panorama image", "single-panorama"},
    {GST_DEWARP_DISPLAY_DOUBLE_PANORAMA, "Dewarped image is split in two "
          "images displayed one below the other", "double-panorama"},
    {GST_DEWARP_DISPLAY_QUAD_VIEW, "Dewarped image is split in four images "
          "dysplayed as a quad view",
        "quad-view"},
    {0, NULL, NULL},
  };

  if (!dewarp_display_mode_type) {
    dewarp_display_mode_type =
        g_enum_register_static ("GstDewarpDisplayMode", dewarp_display_mode);
  }
  return dewarp_display_mode_type;
}

#define GST_TYPE_DEWARP_INTERPOLATION_MODE (dewarp_interpolation_mode_get_type ())

static GType
dewarp_interpolation_mode_get_type (void)
{
  static GType dewarp_interpolation_mode_type = 0;
  static const GEnumValue dewarp_interpolation_mode[] = {
    {GST_DEWARP_INTER_NEAREST, "A nearest-neighbor interpolation", "nearest"},
    {GST_DEWARP_INTER_LINEAR, "A bilinear interpolation", "bilinear"},
    {GST_DEWARP_INTER_CUBIC,
        "A bicubic interpolation over 4x4 pixel neighborhood", "bicubic"},
    {GST_DEWARP_INTER_LANCZOS4,
        "A Lanczos interpolation over 8x8 pixel neighborhood", "Lanczos"},
    {0, NULL, NULL},
  };

  if (!dewarp_interpolation_mode_type) {
    dewarp_interpolation_mode_type =
        g_enum_register_static ("GstDewarpInterpolationMode",
        dewarp_interpolation_mode);
  }
  return dewarp_interpolation_mode_type;
}

G_DEFINE_TYPE_WITH_CODE (GstDewarp, gst_dewarp, GST_TYPE_OPENCV_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_dewarp_debug, "dewarp", 0,
        "Dewarp fisheye images");
    );
GST_ELEMENT_REGISTER_DEFINE (dewarp, "dewarp", GST_RANK_NONE, GST_TYPE_DEWARP);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGBA")));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGBA")));

static void gst_dewarp_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dewarp_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_dewarp_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps);

static GstFlowReturn gst_dewarp_transform_frame (GstOpencvVideoFilter * btrans,
    GstBuffer * buffer, cv::Mat img, GstBuffer * outbuf, cv::Mat outimg);

static gboolean gst_dewarp_set_caps (GstOpencvVideoFilter * filter,
    gint in_width, gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type);

static void
gst_dewarp_finalize (GObject * obj)
{
  GstDewarp *filter = GST_DEWARP (obj);

  filter->map_x.release ();
  filter->map_y.release ();

  G_OBJECT_CLASS (gst_dewarp_parent_class)->finalize (obj);
}

static void
gst_dewarp_class_init (GstDewarpClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *basesrc_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstOpencvVideoFilterClass *cvfilter_class =
      (GstOpencvVideoFilterClass *) klass;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_dewarp_finalize);
  gobject_class->set_property = gst_dewarp_set_property;
  gobject_class->get_property = gst_dewarp_get_property;

  basesrc_class->transform_caps = GST_DEBUG_FUNCPTR (gst_dewarp_transform_caps);
  basesrc_class->transform_ip_on_passthrough = FALSE;
  basesrc_class->passthrough_on_same_caps = TRUE;

  cvfilter_class->cv_trans_func =
      GST_DEBUG_FUNCPTR (gst_dewarp_transform_frame);
  cvfilter_class->cv_set_caps = GST_DEBUG_FUNCPTR (gst_dewarp_set_caps);

  g_object_class_install_property (gobject_class, PROP_X_CENTER,
      g_param_spec_double ("x-center", "x center",
          "X axis center of the fisheye image",
          0.0, 1.0, DEFAULT_CENTER,
          (GParamFlags) (GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_Y_CENTER,
      g_param_spec_double ("y-center", "y center",
          "Y axis center of the fisheye image",
          0.0, 1.0, DEFAULT_CENTER,
          (GParamFlags) (GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_INNER_RADIUS,
      g_param_spec_double ("inner-radius", "inner radius",
          "Inner radius of the fisheye image donut. If outer radius <= inner "
          "radius the element will work in passthrough mode",
          0.0, 1.0, DEFAULT_RADIUS,
          (GParamFlags) (GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_OUTER_RADIUS,
      g_param_spec_double ("outer-radius", "outer radius",
          "Outer radius of the fisheye image donut. If outer radius <= inner "
          "radius the element will work in passthrough mode",
          0.0, 1.0, DEFAULT_RADIUS,
          (GParamFlags) (GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_REMAP_X_CORRECTION,
      g_param_spec_double ("x-remap-correction", "x remap correction",
          "Correction factor for remapping on x axis. A correction is needed if "
          "the fisheye image is not inside a circle",
          0.1, 10.0, DEFAULT_REMAP_CORRECTION,
          (GParamFlags) (GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_REMAP_Y_CORRECTION,
      g_param_spec_double ("y-remap-correction", "y remap correction",
          "Correction factor for remapping on y axis. A correction is needed if "
          "the fisheye image is not inside a circle",
          0.1, 10.0, DEFAULT_REMAP_CORRECTION,
          (GParamFlags) (GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_INTERPOLATION_MODE,
      g_param_spec_enum ("interpolation-method", "Interpolation method",
          "Interpolation method to use",
          GST_TYPE_DEWARP_INTERPOLATION_MODE, GST_DEWARP_INTER_LINEAR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_DISPLAY_MODE,
      g_param_spec_enum ("display-mode", "Display mode",
          "How to display the dewarped image",
          GST_TYPE_DEWARP_DISPLAY_MODE, GST_DEWARP_DISPLAY_PANORAMA,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "Dewarp fisheye images",
      "Filter/Effect/Video",
      "Dewarp fisheye images", "Nicola Murino <nicola.murino@gmail.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  gst_type_mark_as_plugin_api (GST_TYPE_DEWARP_DISPLAY_MODE, (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_DEWARP_INTERPOLATION_MODE, (GstPluginAPIFlags) 0);
}

static void
gst_dewarp_init (GstDewarp * filter)
{
  filter->x_center = DEFAULT_CENTER;
  filter->y_center = DEFAULT_CENTER;
  filter->inner_radius = DEFAULT_RADIUS;
  filter->outer_radius = DEFAULT_RADIUS;
  filter->remap_correction_x = DEFAULT_REMAP_CORRECTION;
  filter->remap_correction_y = DEFAULT_REMAP_CORRECTION;
  filter->display_mode = GST_DEWARP_DISPLAY_PANORAMA;
  filter->interpolation_mode = GST_DEWARP_INTER_LINEAR;
  filter->pad_sink_width = 0;
  filter->pad_sink_height = 0;
  filter->in_width = 0;
  filter->in_height = 0;
  filter->out_width = 0;
  filter->out_height = 0;
  filter->need_map_update = TRUE;

  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      FALSE);
}

static void
gst_dewarp_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  gdouble v;
  gboolean need_reconfigure;
  int disp_mode;
  GstDewarp *filter = GST_DEWARP (object);

  need_reconfigure = FALSE;

  GST_OBJECT_LOCK (filter);

  switch (prop_id) {
    case PROP_X_CENTER:
      v = g_value_get_double (value);
      if (v != filter->x_center) {
        filter->x_center = v;
        filter->need_map_update = TRUE;
        need_reconfigure = TRUE;
        GST_LOG_OBJECT (filter, "x center set to %f", filter->x_center);
      }
      break;
    case PROP_Y_CENTER:
      v = g_value_get_double (value);
      if (v != filter->y_center) {
        filter->y_center = v;
        filter->need_map_update = TRUE;
        need_reconfigure = TRUE;
        GST_LOG_OBJECT (filter, "y center set to %f", filter->y_center);
      }
      break;
    case PROP_INNER_RADIUS:
      v = g_value_get_double (value);
      if (v != filter->inner_radius) {
        filter->inner_radius = v;
        filter->need_map_update = TRUE;
        need_reconfigure = TRUE;
        GST_LOG_OBJECT (filter, "inner radius set to %f",
            filter->inner_radius);
      }
      break;
    case PROP_OUTER_RADIUS:
      v = g_value_get_double (value);
      if (v != filter->outer_radius) {
        filter->outer_radius = v;
        filter->need_map_update = TRUE;
        need_reconfigure = TRUE;
        GST_LOG_OBJECT (filter, "outer radius set to %f",
            filter->outer_radius);
      }
      break;
    case PROP_REMAP_X_CORRECTION:
      v = g_value_get_double (value);
      if (v != filter->remap_correction_x) {
        filter->remap_correction_x = v;
        filter->need_map_update = TRUE;
        need_reconfigure = TRUE;
        GST_LOG_OBJECT (filter, "x remap correction set to %f",
            filter->remap_correction_x);
      }
      break;
    case PROP_REMAP_Y_CORRECTION:
      v = g_value_get_double (value);
      if (v != filter->remap_correction_y) {
        filter->remap_correction_y = v;
        filter->need_map_update = TRUE;
        need_reconfigure = TRUE;
        GST_LOG_OBJECT (filter, "y remap correction set to %f",
            filter->remap_correction_y);
      }
      break;
    case PROP_INTERPOLATION_MODE:
      filter->interpolation_mode = g_value_get_enum (value);
      GST_LOG_OBJECT (filter, "interpolation mode set to %" G_GINT32_FORMAT,
          filter->interpolation_mode);
      break;
    case PROP_DISPLAY_MODE:
      disp_mode = g_value_get_enum (value);
      if (disp_mode != filter->display_mode) {
        filter->display_mode = disp_mode;
        need_reconfigure = TRUE;
        GST_LOG_OBJECT (filter, "display mode set to %" G_GINT32_FORMAT,
            filter->display_mode);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  if (filter->need_map_update)
    GST_LOG_OBJECT (filter, "need map update after property change");

  GST_OBJECT_UNLOCK (filter);

  if (need_reconfigure) {
    GST_DEBUG_OBJECT (filter, "Reconfigure src after property change");
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (filter));
  } else {
    GST_DEBUG_OBJECT (filter,
        "No property value changed, reconfigure src is not" " needed");
  }
}

static void
gst_dewarp_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDewarp *filter = GST_DEWARP (object);

  GST_OBJECT_LOCK (filter);

  switch (prop_id) {
    case PROP_X_CENTER:
      g_value_set_double (value, filter->x_center);
      break;
    case PROP_Y_CENTER:
      g_value_set_double (value, filter->y_center);
      break;
    case PROP_INNER_RADIUS:
      g_value_set_double (value, filter->inner_radius);
      break;
    case PROP_OUTER_RADIUS:
      g_value_set_double (value, filter->outer_radius);
      break;
    case PROP_REMAP_X_CORRECTION:
      g_value_set_double (value, filter->remap_correction_x);
      break;
    case PROP_REMAP_Y_CORRECTION:
      g_value_set_double (value, filter->remap_correction_y);
      break;
    case PROP_INTERPOLATION_MODE:
      g_value_set_enum (value, filter->interpolation_mode);
      break;
    case PROP_DISPLAY_MODE:
      g_value_set_enum (value, filter->display_mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (filter);
}

static void
gst_dewarp_update_map (GstDewarp * filter)
{
  gdouble r1, r2, cx, cy;
  gint x, y;
  gint out_width, out_height;

  if (filter->display_mode == GST_DEWARP_DISPLAY_PANORAMA) {
    out_width = filter->out_width;
    out_height = filter->out_height;
  } else {
    out_width = filter->out_width * 2;
    out_height = filter->out_height / 2;
  }

  GST_DEBUG_OBJECT (filter,
      "start update map out_width: %" G_GINT32_FORMAT " out height: %"
      G_GINT32_FORMAT, out_width, out_height);

  r1 = filter->in_width * filter->inner_radius;
  r2 = filter->in_width * filter->outer_radius;
  cx = filter->x_center * filter->in_width;
  cy = filter->y_center * filter->in_height;
  cv::Size destSize (out_width, out_height);
  filter->map_x.create (destSize, CV_32FC1);
  filter->map_y.create (destSize, CV_32FC1);

  for (y = 0; y < out_height; y++) {
    for (x = 0; x < out_width; x++) {
      float r = ((float) (y) / (float) (out_height)) * (r2 - r1) + r1;
      float theta = ((float) (x) / (float) (out_width)) * 2.0 * G_PI;
      float xs = cx + r * sin (theta) * filter->remap_correction_x;
      float ys = cy + r * cos (theta) * filter->remap_correction_y;
      filter->map_x.at < float >(y, x) = xs;
      filter->map_y.at < float >(y, x) = ys;
    }
  }

  filter->need_map_update = FALSE;

  GST_DEBUG_OBJECT (filter, "update map done");
}

static void
gst_dewarp_calculate_dimensions (GstDewarp * filter, GstPadDirection direction,
    gint in_width, gint in_height, gint * out_width, gint * out_height)
{
  if (filter->outer_radius <= filter->inner_radius) {
    GST_LOG_OBJECT (filter,
        "No dimensions conversion required, in width: %" G_GINT32_FORMAT
        " in height: %" G_GINT32_FORMAT, in_width, in_height);
    *out_width = in_width;
    *out_height = in_height;
  } else {
    gdouble r1, r2;

    GST_LOG_OBJECT (filter,
        "Calculate dimensions, in_width: %" G_GINT32_FORMAT
        " in_height: %" G_GINT32_FORMAT " pad sink width: %" G_GINT32_FORMAT
        " pad sink height: %" G_GINT32_FORMAT
        " inner radius: %f, outer radius: %f, direction: %d", in_width,
        in_height, filter->pad_sink_width, filter->pad_sink_height,
        filter->inner_radius, filter->outer_radius, direction);

    r1 = in_width * filter->inner_radius;
    r2 = in_width * filter->outer_radius;

    if (direction == GST_PAD_SINK) {
      /* roundup is required to have integer results when we divide width, height
       * in display mode different from GST_DEWARP_PANORAMA.
       * Additionally some elements such as xvimagesink have problems with arbitrary
       * dimensions, a roundup solves this issue too
       */
      *out_width = GST_ROUND_UP_8 ((gint) ((2.0 * G_PI) * ((r2 + r1) / 2.0)));
      *out_height = GST_ROUND_UP_8 ((gint) (r2 - r1));

      if (filter->display_mode != GST_DEWARP_DISPLAY_PANORAMA) {
        *out_width = *out_width / 2;
        *out_height = *out_height * 2;
      }

      /* if outer_radius and inner radius are very close then width and height
         could be 0, we assume passthrough in this case
       */
      if (G_UNLIKELY (*out_width == 0) || G_UNLIKELY (*out_height == 0)) {
        GST_WARNING_OBJECT (filter,
            "Invalid calculated dimensions, width: %" G_GINT32_FORMAT
            " height: %" G_GINT32_FORMAT, *out_width, *out_height);
        *out_width = in_width;
        *out_height = in_height;
      }
      filter->pad_sink_width = in_width;
      filter->pad_sink_height = in_height;
    } else {
      if (filter->pad_sink_width > 0) {
        *out_width = filter->pad_sink_width;
      } else {
        *out_width = in_width;
      }
      if (filter->pad_sink_height > 0) {
        *out_height = filter->pad_sink_height;
      } else {
        *out_height = in_height;
      }
    }
  }

  GST_LOG_OBJECT (filter,
      "Calculated dimensions: width %" G_GINT32_FORMAT " => %" G_GINT32_FORMAT
      ", height %" G_GINT32_FORMAT " => %" G_GINT32_FORMAT " direction: %d",
      in_width, *out_width, in_height, *out_height, direction);
}

static GstCaps *
gst_dewarp_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps)
{
  GstDewarp *dewarp = GST_DEWARP (trans);

  GstCaps *ret;
  gint width, height;
  guint i;

  ret = gst_caps_copy (caps);

  GST_OBJECT_LOCK (dewarp);

  for (i = 0; i < gst_caps_get_size (ret); i++) {
    GstStructure *structure = gst_caps_get_structure (ret, i);

    if (gst_structure_get_int (structure, "width", &width) &&
        gst_structure_get_int (structure, "height", &height)) {
      gint out_width, out_height;
      gst_dewarp_calculate_dimensions (dewarp, direction, width, height,
          &out_width, &out_height);
      gst_structure_set (structure, "width", G_TYPE_INT, out_width, "height",
          G_TYPE_INT, out_height, NULL);
    }
  }

  GST_OBJECT_UNLOCK (dewarp);

  if (filter_caps) {
    GstCaps *intersection;

    GST_DEBUG_OBJECT (dewarp, "Using filter caps %" GST_PTR_FORMAT,
        filter_caps);

    intersection =
        gst_caps_intersect_full (filter_caps, ret, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (ret);
    ret = intersection;

    GST_DEBUG_OBJECT (dewarp, "Intersection %" GST_PTR_FORMAT, ret);
  }

  return ret;
}

static gboolean
gst_dewarp_set_caps (GstOpencvVideoFilter * filter,
    gint in_width, gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type)
{
  GstDewarp *dewarp = GST_DEWARP (filter);

  GST_DEBUG_OBJECT (dewarp,
      "Set new caps, in width: %" G_GINT32_FORMAT " in height: %"
      G_GINT32_FORMAT " out width: %" G_GINT32_FORMAT " out height: %"
      G_GINT32_FORMAT, in_width, in_height, out_width, out_height);

  GST_OBJECT_LOCK (dewarp);

  dewarp->in_width = in_width;
  dewarp->in_height = in_height;
  dewarp->out_width = out_width;
  dewarp->out_height = out_height;
  gst_dewarp_update_map (dewarp);

  GST_OBJECT_UNLOCK (dewarp);

  return TRUE;
}

static GstFlowReturn
gst_dewarp_transform_frame (GstOpencvVideoFilter * btrans, GstBuffer * buffer,
    cv::Mat img, GstBuffer * outbuf, cv::Mat outimg)
{
  GstDewarp *filter = GST_DEWARP (btrans);
  GstFlowReturn ret;

  GST_OBJECT_LOCK (filter);

  if (img.size ().width == filter->in_width
      && img.size ().height == filter->in_height
      && outimg.size ().width == filter->out_width
      && outimg.size ().height == filter->out_height) {
    cv::Mat fisheye_image, dewarped_image;
    int inter_mode;

    if (filter->need_map_update) {
      GST_LOG_OBJECT (filter, "map update is needed");
      gst_dewarp_update_map (filter);
    }

    switch (filter->interpolation_mode) {
      case GST_DEWARP_INTER_NEAREST:
        inter_mode = cv::INTER_NEAREST;
        break;
      case GST_DEWARP_INTER_LINEAR:
        inter_mode = cv::INTER_LINEAR;
        break;
      case GST_DEWARP_INTER_CUBIC:
        inter_mode = cv::INTER_CUBIC;
        break;
      case GST_DEWARP_INTER_LANCZOS4:
        inter_mode = cv::INTER_LANCZOS4;
        break;
      default:
        inter_mode = cv::INTER_LINEAR;
        break;
    }

    fisheye_image = img;
    dewarped_image = outimg;

    if (filter->display_mode == GST_DEWARP_DISPLAY_PANORAMA) {
      cv::remap (fisheye_image, dewarped_image, filter->map_x, filter->map_y,
          inter_mode);
    } else if (filter->display_mode == GST_DEWARP_DISPLAY_DOUBLE_PANORAMA) {
      cv::Mat view1, view2, panorama_image, concatenated;
      gint panorama_width, panorama_height;
      panorama_width = filter->out_width * 2;
      panorama_height = filter->out_height / 2;
      cv::Size panoramaSize (panorama_width, panorama_height);
      panorama_image.create (panoramaSize, fisheye_image.type ());
      cv::remap (fisheye_image, panorama_image, filter->map_x, filter->map_y,
          inter_mode);
      view1 =
          panorama_image (cv::Rect (0, 0, filter->out_width, panorama_height));
      view2 =
          panorama_image (cv::Rect (filter->out_width, 0, filter->out_width,
              panorama_height));
      cv::vconcat (view1, view2, concatenated);
      concatenated.copyTo (dewarped_image);
    } else if (filter->display_mode == GST_DEWARP_DISPLAY_QUAD_VIEW) {
      cv::Mat view1, view2, view3, view4, concat1, concat2, panorama_image,
          concatenated;
      gint panorama_width, panorama_height;
      gint view_width, view_height;
      panorama_width = filter->out_width * 2;
      panorama_height = filter->out_height / 2;
      view_width = filter->out_width / 2;
      view_height = filter->out_height / 2;
      cv::Size panoramaSize (panorama_width, panorama_height);
      panorama_image.create (panoramaSize, fisheye_image.type ());
      cv::remap (fisheye_image, panorama_image, filter->map_x, filter->map_y,
          inter_mode);
      view1 = panorama_image (cv::Rect (0, 0, view_width, view_height));
      view2 =
          panorama_image (cv::Rect (view_width, 0, view_width, view_height));
      view3 =
          panorama_image (cv::Rect ((view_width * 2), 0, view_width,
              view_height));
      view4 =
          panorama_image (cv::Rect ((view_width * 3), 0, view_width,
              view_height));
      cv::vconcat (view1, view2, concat1);
      cv::vconcat (view3, view4, concat2);
      cv::hconcat (concat1, concat2, concatenated);
      concatenated.copyTo (dewarped_image);
    }

    ret = GST_FLOW_OK;
  } else {
    GST_WARNING_OBJECT (filter, "Frame dropped, dimensions do not match");

    ret = GST_BASE_TRANSFORM_FLOW_DROPPED;
  }

  GST_OBJECT_UNLOCK (filter);

  return ret;
}
