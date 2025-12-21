/* GStreamer encoding bin
 * Copyright (C) 2016 Jan Schmidt <jan@centricular.com>
 *           (C) 2020 Thibault saunier <tsaunier@igalia.com>
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
#include "gstencodingelements.h"
#include "gstencodebasebin.h"
#include "gstencodebin2.h"


/**
 * SECTION:element-encodebin2
 *
 * Encodebin2 is an updated version of #encodebin which has a request srcpad
 * instead of having an always source pad. This makes the element more flexible
 * and allows supporting muxing sinks for example.
 *
 * Based on the profile that was set (via the #GstEncodeBaseBin:profile
 * property), EncodeBin will internally select and configure the required
 * elements (encoders, muxers, but also audio and video converters) so that you
 * can provide it raw or pre-encoded streams of data in input and have your
 * encoded/muxed/converted stream in output.
 *
 * Since: 1.20
 */

enum
{
  PROP_0,
};

static GstStaticPadTemplate muxer_src_template =
GST_STATIC_PAD_TEMPLATE ("src_%u", GST_PAD_SRC, GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

struct _GstEncodeBin2
{
  GstEncodeBaseBin parent;
};

G_DEFINE_TYPE (GstEncodeBin2, gst_encode_bin2, GST_TYPE_ENCODE_BASE_BIN);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (encodebin2, "encodebin2", GST_RANK_NONE,
    gst_encode_bin2_get_type (), encoding_element_init (plugin));

static void
gst_encode_bin2_class_init (GstEncodeBin2Class * klass)
{
  GstElementClass *gstelement_klass = (GstElementClass *) klass;

  gst_element_class_add_static_pad_template (gstelement_klass,
      &muxer_src_template);
}

static void
gst_encode_bin2_init (GstEncodeBin2 * encode_bin)
{
}
