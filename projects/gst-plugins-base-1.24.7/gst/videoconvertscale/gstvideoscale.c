/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2005-2012 David Schleef <ds@schleef.org>
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
 * SECTION:element-videoscale
 * @title: videoscale
 * @see_also: videorate, videoconvert
 *
 * This element resizes video frames. By default the element will try to
 * negotiate to the same size on the source and sinkpad so that no scaling
 * is needed. It is therefore safe to insert this element in a pipeline to
 * get more robust behaviour without any cost if no scaling is needed.
 *
 * This element supports a wide range of color spaces including various YUV and
 * RGB formats and is therefore generally able to operate anywhere in a
 * pipeline.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v filesrc location=videotestsrc.ogg ! oggdemux ! theoradec ! videoconvert ! videoscale ! autovideosink
 * ]|
 *  Decode an Ogg/Theora and display the video. If the video sink chosen
 * cannot perform scaling, the video scaling will be performed by videoscale
 * when you resize the video window.
 * To create the test Ogg/Theora file refer to the documentation of theoraenc.
 * |[
 * gst-launch-1.0 -v filesrc location=videotestsrc.ogg ! oggdemux ! theoradec ! videoconvert ! videoscale ! video/x-raw,width=100 ! autovideosink
 * ]|
 *  Decode an Ogg/Theora and display the video with a width of 100.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define DEFAULT_PROP_GAMMA_DECODE FALSE

#include "gstvideoscale.h"

G_DEFINE_TYPE (GstVideoScale, gst_video_scale, GST_TYPE_VIDEO_CONVERT_SCALE);
GST_ELEMENT_REGISTER_DEFINE (videoscale, "videoscale",
    GST_RANK_MARGINAL, gst_video_scale_get_type ());

enum
{
  PROP_0,
  PROP_GAMMA_DECODE
};

static void
gst_video_scale_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    case PROP_GAMMA_DECODE:
    {
      gint mode;

      g_object_get (object, "gamma-mode", &mode, NULL);
      g_value_set_boolean (value, mode == GST_VIDEO_GAMMA_MODE_REMAP);

      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_video_scale_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    case PROP_GAMMA_DECODE:
    {
      if (g_value_get_boolean (value))
        g_object_set (object, "gamma-mode", GST_VIDEO_GAMMA_MODE_REMAP, NULL);
      else
        g_object_set (object, "gamma-mode", GST_VIDEO_GAMMA_MODE_NONE, NULL);

      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_video_scale_class_init (GstVideoScaleClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoConvertScaleClass *convertscale_class =
      GST_VIDEO_CONVERT_SCALE_CLASS (klass);

  gobject_class->set_property = gst_video_scale_set_property;
  gobject_class->get_property = gst_video_scale_get_property;

  g_object_class_install_property (gobject_class, PROP_GAMMA_DECODE,
      g_param_spec_boolean ("gamma-decode", "Gamma Decode",
          "Decode gamma before scaling", DEFAULT_PROP_GAMMA_DECODE,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element_class,
      "Video scaler", "Filter/Video/Scaler",
      "Resizes video", "Wim Taymans <wim.taymans@gmail.com>");

  convertscale_class->converts = FALSE;
  convertscale_class->scales = TRUE;
}

static void
gst_video_scale_init (GstVideoScale * self)
{

}
