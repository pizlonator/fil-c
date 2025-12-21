/* GStreamer encoding bin
 * Copyright (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
 *           (C) 2009 Nokia Corporation
 * Copyright (C) 2020 Thibault Saunier <tsaunier@igalia.com>
 *           (C) 2020 Igalia S.L
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
 * SECTION:element-encodebin
 * @title: encodebin
 *
 * EncodeBin provides a bin for encoding/muxing various streams according to
 * a specified #GstEncodingProfile.
 *
 * Based on the profile that was set (via the #GstEncodeBaseBin:profile property),
 * EncodeBin will internally select and configure the required elements
 * (encoders, muxers, but also audio and video converters) so that you can
 * provide it raw or pre-encoded streams of data in input and have your
 * encoded/muxed/converted stream in output.
 *
 * ## Features
 *
 * * Automatic encoder and muxer selection based on elements available on the
 * system.
 *
 * * Conversion of raw audio/video streams (scaling, framerate conversion,
 * colorspace conversion, samplerate conversion) to conform to the profile
 * output format.
 *
 * * Variable number of streams. If the presence property for a stream encoding
 * profile is 0, you can request any number of sink pads for it via the
 * standard request pad gstreamer API or the #GstEncodeBaseBin::request-pad action
 * signal.
 *
 * * Avoid reencoding (passthrough). If the input stream is already encoded and is
 * compatible with what the #GstEncodingProfile expects, then the stream won't
 * be re-encoded but just passed through downstream to the muxer or the output.
 *
 * * Mix pre-encoded and raw streams as input. In addition to the passthrough
 * feature above, you can feed both raw audio/video *AND* already-encoded data
 * to a pad. #GstEncodeBaseBin will take care of passing through the compatible
 * segments and re-encoding the segments of media that need encoding.
 *
 * * Standard behaviour is to use a #GstEncodingContainerProfile to have both
 * encoding and muxing performed. But you can also provide a single stream
 * profile (like #GstEncodingAudioProfile) to only have the encoding done and
 * handle the encoded output yourself.
 *
 * * Audio imperfection corrections. Incoming audio streams can have non perfect
 * timestamps (jitter), like the streams coming from ASF files. #GstEncodeBaseBin
 * will automatically fix those imperfections for you. See
 * #GstEncodeBaseBin:audio-jitter-tolerance for more details.
 *
 * * Variable or Constant video framerate. If your #GstEncodingVideoProfile has
 * the variableframerate property deactivated (default), then the incoming
 * raw video stream will be retimestampped in order to produce a constant
 * framerate.
 *
 * * Cross-boundary re-encoding. When feeding compatible pre-encoded streams that
 * fall on segment boundaries, and for supported formats (right now only H263),
 * the GOP will be decoded/reencoded when needed to produce an encoded output
 * that fits exactly within the request GstSegment.
 *
 * * Missing plugin support. If a #GstElement is missing to encode/mux to the
 * request profile formats, a missing-plugin #GstMessage will be posted on the
 * #GstBus, allowing systems that support the missing-plugin system to offer the
 * user a way to install the missing element.
 *
 */

#include "gstencodingelements.h"
#include "gstencodebin.h"

struct _GstEncodeBin
{
  GstEncodeBaseBin parent;
};

G_DEFINE_TYPE (GstEncodeBin, gst_encode_bin, GST_TYPE_ENCODE_BASE_BIN);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (encodebin, "encodebin", GST_RANK_NONE,
    gst_encode_bin_get_type (), encoding_element_init (plugin));

static GstStaticPadTemplate muxer_src_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void
gst_encode_bin_class_init (GstEncodeBinClass * klass)
{
  GstElementClass *gstelement_klass = (GstElementClass *) klass;

  gst_element_class_add_static_pad_template (gstelement_klass,
      &muxer_src_template);

  gst_element_class_set_static_metadata (gstelement_klass,
      "Encoder Bin",
      "Generic/Bin/Encoder",
      "Convenience encoding/muxing element",
      "Edward Hervey <edward.hervey@collabora.co.uk>");
}

static void
gst_encode_bin_init (GstEncodeBin * encode_bin)
{
  GstEncodeBaseBin *encode_base_bin = (GstEncodeBaseBin *) (encode_bin);
  GstPadTemplate *tmpl;

  tmpl = gst_static_pad_template_get (&muxer_src_template);

  encode_base_bin->srcpad =
      gst_ghost_pad_new_no_target_from_template ("src", tmpl);
  gst_object_unref (tmpl);
  gst_pad_set_active (encode_base_bin->srcpad, TRUE);
  gst_element_add_pad (GST_ELEMENT_CAST (encode_base_bin),
      encode_base_bin->srcpad);
}
