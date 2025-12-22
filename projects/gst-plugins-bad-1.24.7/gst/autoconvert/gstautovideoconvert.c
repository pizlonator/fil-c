/* GStreamer
 * Copyright 2010 ST-Ericsson SA
 *  @author: Benjamin Gaignard <benjamin.gaignard@stericsson.com>
 * Copyright 2023 Igalia S.L.
 *  @author: Thibault Saunier <tsaunier@igalia.com>
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
/*
 * test autovideoconvert:
 * if rgb2bayer is present
 * gst-launch-1.0 videotestsrc num-buffers=2 ! "video/x-raw,width=100,height=100,framerate=10/1" ! autovideoconvert ! "video/x-bayer,width=100,height=100,format=bggr,framerate=10/1" ! fakesink -v
 * if bayer2rgb is present
 * gst-launch-1.0 videotestsrc num-buffers=2 ! "video/x-bayer,width=100,height=100,format=bggr,framerate=10/1" ! autovideoconvert ! "video/x-raw,width=100,height=100,framerate=10/1" ! fakesink -v
 * test with videoconvert
 * gst-launch-1.0 videotestsrc num-buffers=2 ! "video/x-raw,format=RGBx,width=100,height=100,framerate=10/1" ! autovideoconvert ! "video/x-raw,format=RGB16,width=100,height=100,framerate=10/1" ! fakesink -v
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstautovideoconvert.h"
#include "gstautovideo.h"

GST_DEBUG_CATEGORY (autovideoconvert_debug);
#define GST_CAT_DEFAULT (autovideoconvert_debug)

struct _GstAutoVideoConvert
{
  GstBaseAutoConvert parent;
};

G_DEFINE_TYPE (GstAutoVideoConvert, gst_auto_video_convert,
    GST_TYPE_BASE_AUTO_CONVERT);

GST_ELEMENT_REGISTER_DEFINE (autovideoconvert, "autovideoconvert",
    GST_RANK_NONE, gst_auto_video_convert_get_type ());

static void
gst_auto_video_convert_class_init (GstAutoVideoConvertClass * klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (autovideoconvert_debug, "autovideoconvert", 0,
      "Auto color space converter");

  gst_element_class_set_static_metadata (gstelement_class,
      "Select color space converter and scalers based on caps",
      "Bin/Colorspace/Scale/Video/Converter",
      "Selects the right color space converter based on the caps",
      "Thibault Saunier <tsaunier@igalia.com>");
}

static void
gst_auto_video_convert_init (GstAutoVideoConvert * autovideoconvert)
{
  /* *INDENT-OFF* */
  static const GstAutoVideoFilterGenerator gen[] = {
    {
      .first_elements = { "bayer2rgb", NULL},
      .colorspace_converters = { "videoconvertscale", NULL },
      .last_elements = { NULL } ,
      .filters = {  NULL},
      .rank = GST_RANK_SECONDARY,
    },
    {
      .first_elements = { "capsfilter caps=\"video/x-raw\"", NULL, },
      .colorspace_converters = { "videoconvertscale", NULL },
      .last_elements = { "rgb2bayer", NULL },
      .filters = {  NULL },
      .rank = GST_RANK_SECONDARY,
    },
    {
      .first_elements = { "capsfilter caps=\"video/x-raw\"", NULL, },
      .colorspace_converters = { "videoconvertscale", NULL },
      .last_elements = { NULL, },
      .filters = { NULL },
      .rank = GST_RANK_SECONDARY,
    },
    {
      .first_elements = { NULL, },
      .colorspace_converters = { "glcolorconvert", "glcolorscale", "glcolorconvert", NULL },
      .last_elements = { NULL, },
      .filters = { NULL },
      .rank = GST_RANK_PRIMARY,
    },
    {
      .first_elements = { "glupload", },
      .colorspace_converters = { "glcolorconvert", "glcolorscale", "glcolorconvert", NULL },
      .last_elements = { NULL, },
      .filters = { NULL },
      .rank = GST_RANK_PRIMARY,
    },
    {
      .first_elements = { "capsfilter caps=\"video/x-raw\"", "videoconvertscale", "glupload", NULL },
      .colorspace_converters = { NULL },
      .last_elements = { NULL },
      .filters = { NULL },
      .rank = GST_RANK_MARGINAL + 1,
    },
    {
      .first_elements = { "glcolorconvert", "gldownload", NULL },
      .colorspace_converters = { NULL },
      .last_elements = { NULL },
      .filters = { NULL },
      .rank = GST_RANK_MARGINAL + 2,
    },
    { /* Worst case we upload/download as required */
      .first_elements = { "glupload", "gldownload", NULL },
      .colorspace_converters = { "glcolorconvert",  "glcolorscale", "glcolorconvert", NULL },
      .last_elements = { "glupload", "gldownload", NULL },
      .filters = { NULL },
      .rank = GST_RANK_MARGINAL + 1,
    },
    { /* Pure cuda is preferred */
      .first_elements = { NULL },
      .colorspace_converters = { "cudaconvertscale", NULL },
      .last_elements = { NULL },
      .filters = { NULL },
      .rank = GST_RANK_PRIMARY,
    },
    { /* FIXME: Generically make it so we go through cudaconvert for formats not supported by `glcolorconvert` */
      .first_elements = { "capsfilter caps=video/x-raw(ANY),format={I420_10LE,I422_10LE,I422_12LE}", "cudaupload", NULL },
      .colorspace_converters = { "cudaconvert", NULL },
      .last_elements = { "cudadownload", "capsfilter caps=video/x-raw(memory:GLMemory)", NULL },
      .filters = { NULL },
      .rank = GST_RANK_SECONDARY + 2,
    },
    { /* CUDA -> GL */
      .first_elements = { "capsfilter caps=video/x-raw(memory:CUDAMemory)", "cudadownload", NULL },
      .colorspace_converters = { "glcolorconvert",  "glcolorscale", "glcolorconvert", NULL },
      .last_elements = { "glupload", "gldownload", NULL },
      .filters = { NULL },
      .rank = GST_RANK_SECONDARY,
    },
    { /* GL memory to cuda */
      .first_elements = { NULL },
      .colorspace_converters = { "glcolorconvert",  "glcolorscale", "glcolorconvert", NULL },
      .last_elements = { "cudaupload", "capsfilter caps=video/x-raw(memory:CUDAMemory)", NULL },
      .filters = { NULL },
      .rank = GST_RANK_MARGINAL,
    },
    { /* System memory to cuda */
      .first_elements = { "capsfilter caps=\"video/x-raw\"", NULL },
      .colorspace_converters = { "videoconvertscale", NULL },
      .last_elements = { "cudaupload", "capsfilter caps=video/x-raw(memory:CUDAMemory)", NULL },
      .filters = { NULL },
      .rank = GST_RANK_MARGINAL,
    },
    {
      .first_elements = { NULL, },
      .colorspace_converters = { "d3d11convert", NULL },
      .last_elements = { NULL, },
      .filters = { NULL },
      .rank = GST_RANK_PRIMARY,
    },
    {
      .first_elements = { "d3d11download", "d3d11upload", NULL},
      .colorspace_converters = { "glcolorconvert", "glcolorscale", "glcolorconvert", NULL },
      .last_elements = { "d3d11download", "d3d11upload", NULL },
      .filters = { NULL },
      .rank = GST_RANK_MARGINAL,
    },
    { /* Worst case we upload/download as required */
      .first_elements = { NULL},
      .colorspace_converters = { NULL },
      .last_elements = { NULL },
      .filters = { NULL },
      .rank = 0,
    },
  };
  /* *INDENT-ON* */


  gst_auto_video_register_well_known_bins (GST_BASE_AUTO_CONVERT
      (autovideoconvert), gen);
}
