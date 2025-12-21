/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * This file:
 * Copyright (C) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2010 David Schleef <ds@schleef.org>
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
 * SECTION:element-videoconvert
 * @title: videoconvert
 *
 * Convert video frames between a great variety of video formats.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! video/x-raw,format=YUY2 ! videoconvert ! autovideosink
 * ]|
 *  This will output a test video (generated in YUY2 format) in a video
 * window. If the video sink selected does not support YUY2 videoconvert will
 * automatically convert the video to a format understood by the video sink.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvideoconvert.h"

G_DEFINE_TYPE (GstVideoConvert, gst_video_convert,
    GST_TYPE_VIDEO_CONVERT_SCALE);
GST_ELEMENT_REGISTER_DEFINE (videoconvert, "videoconvert",
    GST_RANK_MARGINAL, gst_video_convert_get_type ());

static void
gst_video_convert_class_init (GstVideoConvertClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoConvertScaleClass *convertscale_class =
      GST_VIDEO_CONVERT_SCALE_CLASS (klass);

  gst_element_class_set_static_metadata (element_class,
      "Video colorspace converter",
      "Filter/Converter/Video/Colorspace",
      "Converts video from one colorspace to another",
      "Wim Taymans <wim.taymans@gmail.com>");

  convertscale_class->converts = TRUE;
  convertscale_class->scales = FALSE;
}

static void
gst_video_convert_init (GstVideoConvert * self)
{

}
