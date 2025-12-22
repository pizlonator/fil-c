/* GStreamer
 * Copyright (C) 2004 Wim Taymans <wim@fluendo.com>
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2008 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) <2011-2012> Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
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
 * SECTION:element-opusparse
 * @title: opusparse
 * @see_also: opusenc, opusdec
 *
 * This element parses OPUS packets.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v filesrc location=opusdata ! opusparse ! opusdec ! audioconvert ! audioresample ! alsasink
 * ]| Decode and plays an unmuxed Opus file.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <opus.h>
#include "gstopusheader.h"
#include "gstopusparse.h"

#include <gst/audio/audio.h>
#include <gst/pbutils/pbutils.h>
#include <gst/tag/tag.h>

GST_DEBUG_CATEGORY_STATIC (opusparse_debug);
#define GST_CAT_DEFAULT opusparse_debug

#define MAX_PAYLOAD_BYTES 1500

static GstStaticPadTemplate opus_parse_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-opus, framed = (boolean) true")
    );

static GstStaticPadTemplate opus_parse_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-opus")
    );



static gboolean gst_opus_parse_start (GstBaseParse * parse);
static gboolean gst_opus_parse_stop (GstBaseParse * parse);
static GstFlowReturn gst_opus_parse_handle_frame (GstBaseParse * base,
    GstBaseParseFrame * frame, gint * skip);
static GstFlowReturn gst_opus_parse_parse_frame (GstBaseParse * base,
    GstBaseParseFrame * frame);
static gboolean opusparse_element_init (GstPlugin * plugin);

G_DEFINE_TYPE (GstOpusParse, gst_opus_parse, GST_TYPE_BASE_PARSE);
GST_ELEMENT_REGISTER_DEFINE_CUSTOM (opusparse, opusparse_element_init);

static void
gst_opus_parse_class_init (GstOpusParseClass * klass)
{
  GstBaseParseClass *bpclass;
  GstElementClass *element_class;

  bpclass = (GstBaseParseClass *) klass;
  element_class = (GstElementClass *) klass;

  bpclass->start = GST_DEBUG_FUNCPTR (gst_opus_parse_start);
  bpclass->stop = GST_DEBUG_FUNCPTR (gst_opus_parse_stop);
  bpclass->handle_frame = GST_DEBUG_FUNCPTR (gst_opus_parse_handle_frame);

  gst_element_class_add_static_pad_template (element_class,
      &opus_parse_src_factory);
  gst_element_class_add_static_pad_template (element_class,
      &opus_parse_sink_factory);
  gst_element_class_set_static_metadata (element_class, "Opus audio parser",
      "Codec/Parser/Audio", "parses opus audio streams",
      "Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>");

  GST_DEBUG_CATEGORY_INIT (opusparse_debug, "opusparse", 0,
      "opus parsing element");
}

static void
gst_opus_parse_init (GstOpusParse * parse)
{
  parse->header_sent = FALSE;
  parse->got_headers = FALSE;
  parse->pre_skip = 0;
}

static gboolean
gst_opus_parse_start (GstBaseParse * base)
{
  GstOpusParse *parse = GST_OPUS_PARSE (base);

  parse->header_sent = FALSE;
  parse->got_headers = FALSE;
  parse->pre_skip = 0;
  parse->next_ts = 0;

  return TRUE;
}

static gboolean
gst_opus_parse_stop (GstBaseParse * base)
{
  GstOpusParse *parse = GST_OPUS_PARSE (base);

  parse->header_sent = FALSE;
  parse->got_headers = FALSE;
  parse->pre_skip = 0;

  return TRUE;
}

static GstFlowReturn
gst_opus_parse_handle_frame (GstBaseParse * base,
    GstBaseParseFrame * frame, gint * skip)
{
  GstOpusParse *parse;
  guint8 *data;
  gsize size;
  guint32 packet_size;
  int ret = FALSE;
  const unsigned char *frames[48];
  unsigned char toc;
  short frame_sizes[48];
  int payload_offset;
  int packet_offset = 0;
  gboolean is_header, is_idheader, is_commentheader;
  GstMapInfo map;

  parse = GST_OPUS_PARSE (base);

  *skip = -1;

  gst_buffer_map (frame->buffer, &map, GST_MAP_READ);
  data = map.data;
  size = map.size;
  GST_DEBUG_OBJECT (parse,
      "Checking for frame, %" G_GSIZE_FORMAT " bytes in buffer", size);

  /* check for headers */
  is_idheader = gst_opus_header_is_id_header (frame->buffer);
  is_commentheader = gst_opus_header_is_comment_header (frame->buffer);
  is_header = is_idheader || is_commentheader;

  if (!is_header) {
    int nframes;

    /* Next, check if there's an Opus packet there */
    nframes =
        opus_packet_parse (data, size, &toc, frames, frame_sizes,
        &payload_offset);

    if (nframes < 0) {
      /* Then, check for the test vector framing */
      GST_DEBUG_OBJECT (parse,
          "No Opus packet found, trying test vector framing");
      if (size < 4) {
        GST_DEBUG_OBJECT (parse, "Too small");
        goto beach;
      }
      packet_size = GST_READ_UINT32_BE (data);
      GST_DEBUG_OBJECT (parse, "Packet size: %u bytes", packet_size);
      if (packet_size > MAX_PAYLOAD_BYTES) {
        GST_DEBUG_OBJECT (parse, "Too large");
        goto beach;
      }
      if (packet_size > size - 4) {
        GST_DEBUG_OBJECT (parse, "Truncated");
        goto beach;
      }
      nframes =
          opus_packet_parse (data + 8, packet_size, &toc, frames, frame_sizes,
          &payload_offset);
      if (nframes < 0) {
        GST_DEBUG_OBJECT (parse, "No test vector framing either");
        goto beach;
      }

      packet_offset = 8;

      /* for ad hoc framing, heed the framing, so we eat any padding */
      payload_offset = packet_size;
    } else {
      /* Add up all the frame sizes found */
      int f;
      for (f = 0; f < nframes; ++f)
        payload_offset += frame_sizes[f];
    }
  }

  if (is_header) {
    *skip = 0;
  } else {
    *skip = packet_offset;
    size = payload_offset;
  }

  GST_DEBUG_OBJECT (parse,
      "Got Opus packet at offset %d, %" G_GSIZE_FORMAT " bytes", *skip, size);
  ret = TRUE;

beach:
  gst_buffer_unmap (frame->buffer, &map);

  /* convert old style result to new one */
  if (!ret) {
    if (*skip < 0)
      *skip = 1;
    return GST_FLOW_OK;
  }

  /* always skip first if needed */
  if (*skip > 0)
    return GST_FLOW_OK;

  /* normalize again */
  if (*skip < 0)
    *skip = 0;

  /* not enough */
  if (size > map.size)
    return GST_FLOW_OK;

  /* FIXME some day ... should not mess with buffer itself */
  if (!parse->got_headers) {
    gst_buffer_replace (&frame->buffer,
        gst_buffer_copy_region (frame->buffer, GST_BUFFER_COPY_ALL, 0, size));
    gst_buffer_unref (frame->buffer);
  }

  ret = gst_opus_parse_parse_frame (base, frame);

  if (ret == GST_BASE_PARSE_FLOW_DROPPED) {
    frame->flags |= GST_BASE_PARSE_FRAME_FLAG_DROP;
    ret = GST_FLOW_OK;
  }
  if (ret == GST_FLOW_OK)
    ret = gst_base_parse_finish_frame (base, frame, size);

  return ret;
}

/* Adapted copy of the one in gstoggstream.c... */
static guint64
packet_duration_opus (const guint8 * data, size_t len)
{
  static const guint64 durations[32] = {
    10000, 20000, 40000, 60000, /* Silk NB */
    10000, 20000, 40000, 60000, /* Silk MB */
    10000, 20000, 40000, 60000, /* Silk WB */
    10000, 20000,               /* Hybrid SWB */
    10000, 20000,               /* Hybrid FB */
    2500, 5000, 10000, 20000,   /* CELT NB */
    2500, 5000, 10000, 20000,   /* CELT NB */
    2500, 5000, 10000, 20000,   /* CELT NB */
    2500, 5000, 10000, 20000,   /* CELT NB */
  };

  gint64 duration;
  gint64 frame_duration;
  gint nframes = 0;
  guint8 toc;

  if (len < 1)
    return 0;

  toc = data[0];

  frame_duration = durations[toc >> 3] * 1000;
  switch (toc & 3) {
    case 0:
      nframes = 1;
      break;
    case 1:
      nframes = 2;
      break;
    case 2:
      nframes = 2;
      break;
    case 3:
      if (len < 2) {
        GST_WARNING ("Code 3 Opus packet has less than 2 bytes");
        return 0;
      }
      nframes = data[1] & 63;
      break;
  }

  duration = nframes * frame_duration;
  if (duration > 120 * GST_MSECOND) {
    GST_WARNING ("Opus packet duration > 120 ms, invalid");
    return 0;
  }
  GST_LOG ("Opus packet: frame size %.1f ms, %d frames, duration %.1f ms",
      frame_duration / 1000000.f, nframes, duration / 1000000.f);
  return duration;
}

static GstFlowReturn
gst_opus_parse_parse_frame (GstBaseParse * base, GstBaseParseFrame * frame)
{
  guint64 duration;
  GstOpusParse *parse;
  gboolean is_idheader, is_commentheader;
  GstMapInfo map;
  GstAudioClippingMeta *cmeta =
      gst_buffer_get_audio_clipping_meta (frame->buffer);

  parse = GST_OPUS_PARSE (base);

  g_assert (!cmeta || cmeta->format == GST_FORMAT_DEFAULT);

  is_idheader = gst_opus_header_is_id_header (frame->buffer);
  is_commentheader = gst_opus_header_is_comment_header (frame->buffer);

  if (!parse->got_headers || !parse->header_sent) {
    GstCaps *caps;

    /* Opus streams can decode to 1 or 2 channels, so use the header
       value if we have one, or 2 otherwise */
    if (is_idheader) {
      gst_buffer_replace (&parse->id_header, frame->buffer);
      GST_DEBUG_OBJECT (parse, "Found ID header, keeping");
      return GST_BASE_PARSE_FLOW_DROPPED;
    } else if (is_commentheader) {
      gst_buffer_replace (&parse->comment_header, frame->buffer);
      GST_DEBUG_OBJECT (parse, "Found comment header, keeping");
      return GST_BASE_PARSE_FLOW_DROPPED;
    }

    parse->got_headers = TRUE;

    if (cmeta && cmeta->start) {
      parse->pre_skip += cmeta->start;

      gst_buffer_map (frame->buffer, &map, GST_MAP_READ);
      duration = packet_duration_opus (map.data, map.size);
      gst_buffer_unmap (frame->buffer, &map);

      /* Queue frame for later once we know all initial padding */
      if (duration == cmeta->start) {
        frame->flags |= GST_BASE_PARSE_FRAME_FLAG_QUEUE;
      }
    }

    if (!(frame->flags & GST_BASE_PARSE_FRAME_FLAG_QUEUE)) {
      GstCaps *sink_caps;
      guint32 sample_rate = 48000;
      guint8 n_channels, n_streams, n_stereo_streams, channel_mapping_family;
      guint8 channel_mapping[256];
      GstBuffer *id_header;
      guint16 pre_skip = 0;
      gint16 gain = 0;

      if (parse->id_header) {
        gst_buffer_map (parse->id_header, &map, GST_MAP_READWRITE);
        pre_skip = GST_READ_UINT16_LE (map.data + 10);
        gain = GST_READ_UINT16_LE (map.data + 16);
        gst_buffer_unmap (parse->id_header, &map);
      }

      sink_caps = gst_pad_get_current_caps (GST_BASE_PARSE_SINK_PAD (parse));
      if (!sink_caps
          || !gst_codec_utils_opus_parse_caps (sink_caps, &sample_rate,
              &n_channels, &channel_mapping_family, &n_streams,
              &n_stereo_streams, channel_mapping)) {
        GST_INFO_OBJECT (parse,
            "No headers and no caps, blindly setting up canonical stereo");
        n_channels = 2;
        n_streams = 1;
        n_stereo_streams = 1;
        channel_mapping_family = 0;
        channel_mapping[0] = 0;
        channel_mapping[1] = 1;
      }
      if (sink_caps)
        gst_caps_unref (sink_caps);

      id_header =
          gst_codec_utils_opus_create_header (sample_rate, n_channels,
          channel_mapping_family, n_streams, n_stereo_streams,
          channel_mapping, pre_skip, gain);
      caps = gst_codec_utils_opus_create_caps_from_header (id_header, NULL);
      gst_buffer_unref (id_header);

      gst_buffer_replace (&parse->id_header, NULL);
      gst_buffer_replace (&parse->comment_header, NULL);

      gst_pad_set_caps (GST_BASE_PARSE_SRC_PAD (parse), caps);
      gst_caps_unref (caps);
      parse->header_sent = TRUE;
    }
  }

  GST_BUFFER_TIMESTAMP (frame->buffer) = parse->next_ts;

  gst_buffer_map (frame->buffer, &map, GST_MAP_READ);
  duration = packet_duration_opus (map.data, map.size);
  gst_buffer_unmap (frame->buffer, &map);
  parse->next_ts += duration;

  GST_BUFFER_DURATION (frame->buffer) = duration;
  GST_BUFFER_OFFSET_END (frame->buffer) =
      gst_util_uint64_scale (parse->next_ts, 48000, GST_SECOND);
  GST_BUFFER_OFFSET (frame->buffer) = parse->next_ts;

  return GST_FLOW_OK;
}

static gboolean
opusparse_element_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "opusparse", GST_RANK_NONE,
          GST_TYPE_OPUS_PARSE))
    return FALSE;

  gst_tag_register_musicbrainz_tags ();
  return TRUE;
}
