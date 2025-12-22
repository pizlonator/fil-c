/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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
 * SECTION:element-h265timestamper
 * @title: h265timestamper
 * @short_description: A timestamp correction element for H.265 streams
 *
 * `h265timestamper` updates the DTS (Decoding Time Stamp) of each frame
 * based on H.265 SPS codec setup data, specifically the frame reordering
 * information written in the SPS indicating the maximum number of B-frames
 * allowed.
 *
 * In order to determine the DTS of each frame, this element may need to hold
 * back a few frames in case the codec data indicates that frame reordering is
 * allowed for the given stream. That means this element may introduce additional
 * latency for the DTS decision.
 *
 * This element can be useful if downstream elements require correct DTS
 * information but upstream elements either do not provide it at all or the
 * upstream DTS information is unreliable.
 *
 * For example, mp4 muxers typically require both DTS and PTS on the input
 * buffers, but in case where the input H.265 data comes from Matroska files or
 * RTP/RTSP streams DTS timestamps may be absent and this element may need to
 * be used to clean up the DTS timestamps before handing it to the mp4 muxer.
 *
 * This is particularly the case where the H.265 stream contains B-frames
 * (i.e. frame reordering is required), as streams without correct DTS information
 * will confuse the muxer element and will result in unexpected (or bogus)
 * duration/framerate/timestamp values in the muxed container stream.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=video.mkv ! matroskademux ! h265parse ! h265timestamper ! mp4mux ! filesink location=output.mp4
 * ```
 *
 * Since: 1.22
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/base/base.h>
#include <gst/codecparsers/gsth265parser.h>
#include "gsth265timestamper.h"

GST_DEBUG_CATEGORY_STATIC (gst_h265_timestamper_debug);
#define GST_CAT_DEFAULT gst_h265_timestamper_debug

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h265, alignment=(string) au"));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h265, alignment=(string) au"));

struct _GstH265Timestamper
{
  GstCodecTimestamper parent;

  GstH265Parser *parser;
  gboolean packetized;
  guint nal_length_size;
};

static gboolean gst_h265_timestamper_start (GstCodecTimestamper * timestamper);
static gboolean gst_h265_timestamper_stop (GstCodecTimestamper * timestamper);
static gboolean gst_h265_timestamper_set_caps (GstCodecTimestamper *
    timestamper, GstCaps * caps);
static GstFlowReturn gst_h265_timestamper_handle_buffer (GstCodecTimestamper *
    timestamper, GstBuffer * buffer);
static void gst_h265_timestamper_process_nal (GstH265Timestamper * self,
    GstH265NalUnit * nalu);

G_DEFINE_TYPE (GstH265Timestamper,
    gst_h265_timestamper, GST_TYPE_CODEC_TIMESTAMPER);

GST_ELEMENT_REGISTER_DEFINE (h265timestamper, "h265timestamper",
    GST_RANK_MARGINAL, GST_TYPE_H265_TIMESTAMPER);

static void
gst_h265_timestamper_class_init (GstH265TimestamperClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstCodecTimestamperClass *timestamper_class =
      GST_CODEC_TIMESTAMPER_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &sinktemplate);
  gst_element_class_add_static_pad_template (element_class, &srctemplate);

  gst_element_class_set_static_metadata (element_class, "H.265 timestamper",
      "Codec/Video/Timestamper", "Timestamp H.265 streams",
      "Seungha Yang <seungha@centricular.com>");

  timestamper_class->start = GST_DEBUG_FUNCPTR (gst_h265_timestamper_start);
  timestamper_class->stop = GST_DEBUG_FUNCPTR (gst_h265_timestamper_stop);
  timestamper_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_h265_timestamper_set_caps);
  timestamper_class->handle_buffer =
      GST_DEBUG_FUNCPTR (gst_h265_timestamper_handle_buffer);

  GST_DEBUG_CATEGORY_INIT (gst_h265_timestamper_debug, "h265timestamper", 0,
      "h265timestamper");
}

static void
gst_h265_timestamper_init (GstH265Timestamper * self)
{
}

static gboolean
gst_h265_timestamper_set_caps (GstCodecTimestamper * timestamper,
    GstCaps * caps)
{
  GstH265Timestamper *self = GST_H265_TIMESTAMPER (timestamper);
  GstStructure *s = gst_caps_get_structure (caps, 0);
  const gchar *str;
  gboolean found_format = FALSE;
  const GValue *codec_data_val;
  gboolean ret = TRUE;

  self->packetized = FALSE;
  self->nal_length_size = 4;
  str = gst_structure_get_string (s, "stream-format");
  if (g_strcmp0 (str, "hvc1") == 0 || g_strcmp0 (str, "hev1") == 0) {
    self->packetized = TRUE;
    found_format = TRUE;
  } else if (g_strcmp0 (str, "byte-stream") == 0) {
    found_format = TRUE;
  }

  codec_data_val = gst_structure_get_value (s, "codec_data");
  if (codec_data_val && GST_VALUE_HOLDS_BUFFER (codec_data_val)) {
    GstBuffer *codec_data = gst_value_get_buffer (codec_data_val);
    GstH265Parser *parser = self->parser;
    GstMapInfo map;
    GstH265ParserResult pres;
    GstH265DecoderConfigRecord *config = NULL;
    guint i, j;

    if (!gst_buffer_map (codec_data, &map, GST_MAP_READ)) {
      GST_ERROR_OBJECT (self, "Unable to map codec-data buffer");
      return FALSE;
    }

    pres = gst_h265_parser_parse_decoder_config_record (parser,
        map.data, map.size, &config);
    if (pres != GST_H265_PARSER_OK) {
      GST_WARNING_OBJECT (self, "Failed to parse hvcC data");
      ret = FALSE;
      goto unmap;
    }

    self->nal_length_size = config->length_size_minus_one + 1;
    GST_DEBUG_OBJECT (self, "nal length size %u", self->nal_length_size);

    for (i = 0; i < config->nalu_array->len; i++) {
      GstH265DecoderConfigRecordNalUnitArray *array =
          &g_array_index (config->nalu_array,
          GstH265DecoderConfigRecordNalUnitArray, i);

      for (j = 0; j < array->nalu->len; j++) {
        GstH265NalUnit *nalu = &g_array_index (array->nalu, GstH265NalUnit, j);
        gst_h265_timestamper_process_nal (self, nalu);
      }
    }

    /* codec_data would mean packetized format */
    if (!found_format)
      self->packetized = TRUE;

  unmap:
    gst_buffer_unmap (codec_data, &map);
    gst_h265_decoder_config_record_free (config);
  }

  return ret;
}

static void
gst_h265_timestamper_process_sps (GstH265Timestamper * self, GstH265SPS * sps)
{
  guint max_reorder_frames =
      sps->max_num_reorder_pics[sps->max_sub_layers_minus1];

  GST_DEBUG_OBJECT (self, "Max num reorder frames %d", max_reorder_frames);

  gst_codec_timestamper_set_window_size (GST_CODEC_TIMESTAMPER_CAST (self),
      max_reorder_frames);
}

static void
gst_h265_timestamper_process_nal (GstH265Timestamper * self,
    GstH265NalUnit * nalu)
{
  GstH265ParserResult ret;

  switch (nalu->type) {
    case GST_H265_NAL_VPS:{
      GstH265VPS vps;
      ret = gst_h265_parser_parse_vps (self->parser, nalu, &vps);
      if (ret != GST_H265_PARSER_OK)
        GST_WARNING_OBJECT (self, "Failed to parse SPS");
      break;
    }
    case GST_H265_NAL_SPS:{
      GstH265SPS sps;
      ret = gst_h265_parser_parse_sps (self->parser, nalu, &sps, FALSE);
      if (ret != GST_H265_PARSER_OK) {
        GST_WARNING_OBJECT (self, "Failed to parse SPS");
        break;
      }

      gst_h265_timestamper_process_sps (self, &sps);
      break;
    }
      /* TODO: parse PPS/SLICE and correct PTS based on POC if needed */
    default:
      break;
  }
}

static GstFlowReturn
gst_h265_timestamper_handle_buffer (GstCodecTimestamper * timestamper,
    GstBuffer * buffer)
{
  GstH265Timestamper *self = GST_H265_TIMESTAMPER (timestamper);
  GstMapInfo map;

  /* Ignore any error while parsing NAL */
  if (gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GstH265ParserResult ret;
    GstH265NalUnit nalu;

    if (self->packetized) {
      ret = gst_h265_parser_identify_nalu_hevc (self->parser,
          map.data, 0, map.size, self->nal_length_size, &nalu);

      while (ret == GST_H265_PARSER_OK) {
        gst_h265_timestamper_process_nal (self, &nalu);

        ret = gst_h265_parser_identify_nalu_hevc (self->parser,
            map.data, nalu.offset + nalu.size, map.size, self->nal_length_size,
            &nalu);
      }
    } else {
      ret = gst_h265_parser_identify_nalu (self->parser,
          map.data, 0, map.size, &nalu);

      if (ret == GST_H265_PARSER_NO_NAL_END)
        ret = GST_H265_PARSER_OK;

      while (ret == GST_H265_PARSER_OK) {
        gst_h265_timestamper_process_nal (self, &nalu);

        ret = gst_h265_parser_identify_nalu (self->parser,
            map.data, nalu.offset + nalu.size, map.size, &nalu);

        if (ret == GST_H265_PARSER_NO_NAL_END)
          ret = GST_H265_PARSER_OK;
      }
    }
    gst_buffer_unmap (buffer, &map);
  }

  return GST_FLOW_OK;
}

static void
gst_h265_timestamper_reset (GstH265Timestamper * self)
{
  g_clear_pointer (&self->parser, gst_h265_parser_free);
}

static gboolean
gst_h265_timestamper_start (GstCodecTimestamper * timestamper)
{
  GstH265Timestamper *self = GST_H265_TIMESTAMPER (timestamper);

  gst_h265_timestamper_reset (self);

  self->parser = gst_h265_parser_new ();

  return TRUE;
}

static gboolean
gst_h265_timestamper_stop (GstCodecTimestamper * timestamper)
{
  GstH265Timestamper *self = GST_H265_TIMESTAMPER (timestamper);

  gst_h265_timestamper_reset (self);

  return TRUE;
}
