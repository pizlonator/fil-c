/* GStreamer
 * Copyright (C) <2021> Collabora Ltd.
 *   Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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
 * SECTION:element-vp8alphadecodebin
 * @title: Wrapper to decode VP8 alpha using vp8dec
 *
 * Use two `vp8dec` instance in order to decode VP8 alpha channel.
 *
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvp8alphadecodebin.h"

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp8, codec-alpha = (boolean) true")
    );

struct _GstVp8AlphaDecodeBin
{
  GstAlphaDecodeBin parent;
};

#define gst_vp8_alpha_decode_bin_parent_class parent_class
G_DEFINE_TYPE (GstVp8AlphaDecodeBin, gst_vp8_alpha_decode_bin,
    GST_TYPE_ALPHA_DECODE_BIN);

GST_ELEMENT_REGISTER_DEFINE (vp8_alpha_decode_bin, "vp8alphadecodebin",
    GST_RANK_PRIMARY + GST_ALPHA_DECODE_BIN_RANK_OFFSET,
    GST_TYPE_VP8_ALPHA_DECODE_BIN);

static void
gst_vp8_alpha_decode_bin_class_init (GstVp8AlphaDecodeBinClass * klass)
{
  GstAlphaDecodeBinClass *adbin_class = (GstAlphaDecodeBinClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;

  adbin_class->decoder_name = "vp8dec";
  gst_element_class_add_static_pad_template (element_class, &sink_template);

  gst_element_class_set_static_metadata (element_class,
      "VP8 Alpha Decoder", "Codec/Decoder/Video",
      "Wrapper bin to decode VP8 with alpha stream.",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com>");
}

static void
gst_vp8_alpha_decode_bin_init (GstVp8AlphaDecodeBin * self)
{
}
