/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@entropywave.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-zebrastripe
 * @title: gstzebrastripe
 *
 * The zebrastripe element marks areas of images in a video stream
 * that are brighter than a threshold with a diagonal zebra stripe
 * pattern.  Typically, this is used to aid in adjusting the exposure
 * setting on the camera.  Setting the threshold to 95 or 100 will
 * show areas that are completely overexposed and clipping.  A
 * threshold setting of 70 is often used to properly adjust skin
 * tones.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! zebrastripe ! xvimagesink
 * ]|
 * Marks overexposed areas of the video with zebra stripes.
 *
 * The threshold property is expressed as percentage of full scale,
 * whereas common usage expresses thresholds in terms of IRE.  The
 * property setting can be calculated from IRE by using the formula
 * percent = (IRE * 1.075) - 7.5.  Note that 100 IRE corresponds to
 * 100 %, and 70 IRE corresponds to 68 %.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include "gstzebrastripe.h"
#include <math.h>

GST_DEBUG_CATEGORY_STATIC (gst_zebra_stripe_debug_category);
#define GST_CAT_DEFAULT gst_zebra_stripe_debug_category

/* prototypes */


static void gst_zebra_stripe_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_zebra_stripe_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static gboolean gst_zebra_stripe_start (GstBaseTransform * trans);
static gboolean gst_zebra_stripe_stop (GstBaseTransform * trans);

static GstFlowReturn gst_zebra_stripe_transform_frame_ip (GstVideoFilter *
    filter, GstVideoFrame * frame);

enum
{
  PROP_0,
  PROP_THRESHOLD
};

#define DEFAULT_THRESHOLD 90

/* pad templates */

#define VIDEO_CAPS GST_VIDEO_CAPS_MAKE( \
    "{ I420, Y444, Y42B, Y41B, YUY2, UYVY, AYUV, NV12, NV21, YV12 }")


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstZebraStripe, gst_zebra_stripe,
    GST_TYPE_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_zebra_stripe_debug_category, "zebrastripe", 0,
        "debug category for zebrastripe element"));
GST_ELEMENT_REGISTER_DEFINE (zebrastripe, "zebrastripe",
    GST_RANK_NONE, gst_zebra_stripe_get_type ());

static void
gst_zebra_stripe_class_init (GstZebraStripeClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Zebra stripe overlay",
      "Filter/Analysis",
      "Overlays zebra striping on overexposed areas of video",
      "David Schleef <ds@entropywave.com>");

  gobject_class->set_property = gst_zebra_stripe_set_property;
  gobject_class->get_property = gst_zebra_stripe_get_property;
  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_zebra_stripe_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_zebra_stripe_stop);
  video_filter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_zebra_stripe_transform_frame_ip);

  g_object_class_install_property (gobject_class, PROP_THRESHOLD,
      g_param_spec_int ("threshold", "Threshold",
          "Threshold above which the video is striped", 0, 100,
          DEFAULT_THRESHOLD,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

static void
gst_zebra_stripe_init (GstZebraStripe * zebrastripe)
{
}

void
gst_zebra_stripe_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstZebraStripe *zebrastripe = GST_ZEBRA_STRIPE (object);

  GST_DEBUG_OBJECT (zebrastripe, "set_property");

  switch (property_id) {
    case PROP_THRESHOLD:
      zebrastripe->threshold = g_value_get_int (value);
      zebrastripe->y_threshold =
          16 + floor (0.5 + 2.19 * zebrastripe->threshold);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_zebra_stripe_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstZebraStripe *zebrastripe = GST_ZEBRA_STRIPE (object);

  GST_DEBUG_OBJECT (zebrastripe, "get_property");

  switch (property_id) {
    case PROP_THRESHOLD:
      g_value_set_int (value, zebrastripe->threshold);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static gboolean
gst_zebra_stripe_start (GstBaseTransform * trans)
{
#ifndef GST_DISABLE_GST_DEBUG
  GstZebraStripe *zebrastripe = GST_ZEBRA_STRIPE (trans);

  GST_DEBUG_OBJECT (zebrastripe, "start");
#endif

  if (GST_BASE_TRANSFORM_CLASS (gst_zebra_stripe_parent_class)->start)
    return
        GST_BASE_TRANSFORM_CLASS (gst_zebra_stripe_parent_class)->start (trans);
  return TRUE;
}

static gboolean
gst_zebra_stripe_stop (GstBaseTransform * trans)
{
#ifndef GST_DISABLE_GST_DEBUG
  GstZebraStripe *zebrastripe = GST_ZEBRA_STRIPE (trans);

  GST_DEBUG_OBJECT (zebrastripe, "stop");
#endif

  if (GST_BASE_TRANSFORM_CLASS (gst_zebra_stripe_parent_class)->stop)
    return
        GST_BASE_TRANSFORM_CLASS (gst_zebra_stripe_parent_class)->stop (trans);
  return TRUE;
}

static GstFlowReturn
gst_zebra_stripe_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame)
{
  GstZebraStripe *zebrastripe = GST_ZEBRA_STRIPE (filter);
  int width = frame->info.width;
  int height = frame->info.height;
  int i, j;
  int threshold = zebrastripe->y_threshold;
  int t = zebrastripe->t;
  int offset = 0;
  int pixel_stride = 0, y_position = 0;

  GST_DEBUG_OBJECT (zebrastripe, "transform_frame_ip");
  zebrastripe->t++;
  pixel_stride = GST_VIDEO_FORMAT_INFO_PSTRIDE (frame->info.finfo, 0);

  switch (frame->info.finfo->format) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_Y41B:
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
    case GST_VIDEO_FORMAT_YV12:
      break;
    case GST_VIDEO_FORMAT_UYVY:
      offset = 1;
      break;
    case GST_VIDEO_FORMAT_AYUV:
      y_position = 1;
      break;
    default:
      g_assert_not_reached ();
  }

  for (j = 0; j < height; j++) {
    guint8 *data =
        (guint8 *) frame->data[0] + frame->info.stride[0] * j + offset;
    for (i = 0; i < width; i++) {
      if (data[pixel_stride * i + y_position] >= threshold) {
        if ((i + j + t) & 0x4)
          data[pixel_stride * i + y_position] = 16;
      }
    }
  }

  return GST_FLOW_OK;
}
