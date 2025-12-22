/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *               <2001> Wim Taymans <wim.taymans@chello.be>
 *               <2011> Stefan Sauer <ensonic@user.sf.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>
#include "gstsmooth.h"
#include <gst/video/video.h>

/* Smooth args */

enum
{
  PROP_0,
  PROP_ACTIVE,
  PROP_TOLERANCE,
  PROP_FILTER_SIZE,
  PROP_LUMA_ONLY
};

static GstStaticPadTemplate gst_smooth_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("I420")
    )
    );

static GstStaticPadTemplate gst_smooth_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("I420")
    )
    );

static gboolean gst_smooth_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info);
static GstFlowReturn gst_smooth_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame);

static void gst_smooth_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_smooth_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

G_DEFINE_TYPE (GstSmooth, gst_smooth, GST_TYPE_VIDEO_FILTER);
GST_ELEMENT_REGISTER_DEFINE (smooth, "smooth", GST_RANK_NONE, GST_TYPE_SMOOTH);

static void
gst_smooth_class_init (GstSmoothClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstVideoFilterClass *vfilter_class = (GstVideoFilterClass *) klass;

  gobject_class->set_property = gst_smooth_set_property;
  gobject_class->get_property = gst_smooth_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_ACTIVE,
      g_param_spec_boolean ("active", "active", "process video", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TOLERANCE,
      g_param_spec_int ("tolerance",
          "tolerance", "contrast tolerance for smoothing", G_MININT,
          G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FILTER_SIZE,
      g_param_spec_int ("filter-size", "filter-size", "size of media filter",
          G_MININT, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_LUMA_ONLY,
      g_param_spec_boolean ("luma-only", "luma-only", "only filter luma part",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_smooth_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_smooth_src_template);
  gst_element_class_set_static_metadata (gstelement_class, "Smooth effect",
      "Filter/Effect/Video", "Apply a smooth filter to an image",
      "Wim Taymans <wim.taymans@chello.be>");

  vfilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_smooth_transform_frame);
  vfilter_class->set_info = GST_DEBUG_FUNCPTR (gst_smooth_set_info);
}

static gboolean
gst_smooth_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstSmooth *smooth;

  smooth = GST_SMOOTH (filter);

  smooth->width = GST_VIDEO_INFO_WIDTH (in_info);
  smooth->height = GST_VIDEO_INFO_HEIGHT (in_info);

  return TRUE;
}

static void
gst_smooth_init (GstSmooth * smooth)
{
  smooth->active = TRUE;
  smooth->tolerance = 8;
  smooth->filtersize = 3;
  smooth->luma_only = TRUE;
}

static void
smooth_filter (guchar * dest, guchar * src, gint width, gint height,
    gint stride, gint dstride, gint tolerance, gint filtersize)
{
  gint refval, aktval, upperval, lowerval, numvalues, sum;
  gint x, y, fx, fy, fy1, fy2, fx1, fx2;
  guchar *srcp = src, *destp = dest;

  fy1 = 0;
  fy2 = MIN (filtersize + 1, height) * stride;

  for (y = 0; y < height; y++) {
    if (y > (filtersize + 1))
      fy1 += stride;
    if (y < height - (filtersize + 1))
      fy2 += stride;

    for (x = 0; x < width; x++) {
      refval = *src;
      upperval = refval + tolerance;
      lowerval = refval - tolerance;

      numvalues = 1;
      sum = refval;

      fx1 = MAX (x - filtersize, 0) + fy1;
      fx2 = MIN (x + filtersize + 1, width) + fy1;

      for (fy = fy1; fy < fy2; fy += stride) {
        for (fx = fx1; fx < fx2; fx++) {
          aktval = srcp[fx];
          if ((lowerval - aktval) * (upperval - aktval) < 0) {
            numvalues++;
            sum += aktval;
          }
        }                       /*for fx */
        fx1 += stride;
        fx2 += stride;
      }                         /*for fy */

      src++;
      *dest++ = sum / numvalues;
    }

    src = srcp + stride * y;
    dest = destp + dstride * y;
  }
}

static GstFlowReturn
gst_smooth_transform_frame (GstVideoFilter * vfilter, GstVideoFrame * in_frame,
    GstVideoFrame * out_frame)
{
  GstSmooth *smooth;

  smooth = GST_SMOOTH (vfilter);

  if (!smooth->active) {
    gst_video_frame_copy (out_frame, in_frame);
    return GST_FLOW_OK;
  }

  smooth_filter (GST_VIDEO_FRAME_COMP_DATA (out_frame, 0),
      GST_VIDEO_FRAME_COMP_DATA (in_frame, 0),
      GST_VIDEO_FRAME_COMP_WIDTH (in_frame, 0),
      GST_VIDEO_FRAME_COMP_HEIGHT (in_frame, 0),
      GST_VIDEO_FRAME_COMP_STRIDE (in_frame, 0),
      GST_VIDEO_FRAME_COMP_STRIDE (out_frame, 0),
      smooth->tolerance, smooth->filtersize);
  if (!smooth->luma_only) {
    smooth_filter (GST_VIDEO_FRAME_COMP_DATA (out_frame, 1),
        GST_VIDEO_FRAME_COMP_DATA (in_frame, 1),
        GST_VIDEO_FRAME_COMP_WIDTH (in_frame, 1),
        GST_VIDEO_FRAME_COMP_HEIGHT (in_frame, 1),
        GST_VIDEO_FRAME_COMP_STRIDE (in_frame, 1),
        GST_VIDEO_FRAME_COMP_STRIDE (out_frame, 1),
        smooth->tolerance, smooth->filtersize);
    smooth_filter (GST_VIDEO_FRAME_COMP_DATA (out_frame, 2),
        GST_VIDEO_FRAME_COMP_DATA (in_frame, 2),
        GST_VIDEO_FRAME_COMP_WIDTH (in_frame, 2),
        GST_VIDEO_FRAME_COMP_HEIGHT (in_frame, 2),
        GST_VIDEO_FRAME_COMP_STRIDE (in_frame, 2),
        GST_VIDEO_FRAME_COMP_STRIDE (out_frame, 2),
        smooth->tolerance, smooth->filtersize);
  } else {
    gst_video_frame_copy_plane (out_frame, in_frame, 1);
    gst_video_frame_copy_plane (out_frame, in_frame, 2);
  }

  return GST_FLOW_OK;
}

static void
gst_smooth_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstSmooth *smooth;

  g_return_if_fail (GST_IS_SMOOTH (object));
  smooth = GST_SMOOTH (object);

  switch (prop_id) {
    case PROP_ACTIVE:
      smooth->active = g_value_get_boolean (value);
      break;
    case PROP_TOLERANCE:
      smooth->tolerance = g_value_get_int (value);
      break;
    case PROP_FILTER_SIZE:
      smooth->filtersize = g_value_get_int (value);
      break;
    case PROP_LUMA_ONLY:
      smooth->luma_only = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_smooth_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstSmooth *smooth;

  g_return_if_fail (GST_IS_SMOOTH (object));
  smooth = GST_SMOOTH (object);

  switch (prop_id) {
    case PROP_ACTIVE:
      g_value_set_boolean (value, smooth->active);
      break;
    case PROP_TOLERANCE:
      g_value_set_int (value, smooth->tolerance);
      break;
    case PROP_FILTER_SIZE:
      g_value_set_int (value, smooth->filtersize);
      break;
    case PROP_LUMA_ONLY:
      g_value_set_boolean (value, smooth->luma_only);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (smooth, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    smooth,
    "Apply a smooth filter to an image",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
