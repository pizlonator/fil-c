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
 * SECTION:element-h264timestamper
 * @title: h264timestamper
 * @short_description: A timestamp correction element for H.264 streams
 *
 * `h264timestamper` updates the DTS (Decoding Time Stamp) of each frame
 * based on H.264 SPS codec setup data, specifically the frame reordering
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
 * buffers, but in case where the input H.264 data comes from Matroska files or
 * RTP/RTSP streams DTS timestamps may be absent and this element may need to
 * be used to clean up the DTS timestamps before handing it to the mp4 muxer.
 *
 * This is particularly the case where the H.264 stream contains B-frames
 * (i.e. frame reordering is required), as streams without correct DTS information
 * will confuse the muxer element and will result in unexpected (or bogus)
 * duration/framerate/timestamp values in the muxed container stream.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=video.mkv ! matroskademux ! h264parse ! h264timestamper ! mp4mux ! filesink location=output.mp4
 * ```
 *
 * Since: 1.22
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/base/base.h>
#include <gst/codecparsers/gsth264parser.h>
#include "gsth264timestamper.h"

GST_DEBUG_CATEGORY_STATIC (gst_h264_timestamper_debug);
#define GST_CAT_DEFAULT gst_h264_timestamper_debug

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, alignment=(string) au"));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, alignment=(string) au"));

struct _GstH264Timestamper
{
  GstCodecTimestamper parent;

  GstH264NalParser *parser;
  gboolean packetized;
  guint nal_length_size;
};

static gboolean gst_h264_timestamper_start (GstCodecTimestamper * timestamper);
static gboolean gst_h264_timestamper_stop (GstCodecTimestamper * timestamper);
static gboolean gst_h264_timestamper_set_caps (GstCodecTimestamper *
    timestamper, GstCaps * caps);
static GstFlowReturn gst_h264_timestamper_handle_buffer (GstCodecTimestamper *
    timestamper, GstBuffer * buffer);
static void gst_h264_timestamper_process_nal (GstH264Timestamper * self,
    GstH264NalUnit * nalu);

G_DEFINE_TYPE (GstH264Timestamper,
    gst_h264_timestamper, GST_TYPE_CODEC_TIMESTAMPER);

GST_ELEMENT_REGISTER_DEFINE (h264timestamper, "h264timestamper",
    GST_RANK_MARGINAL, GST_TYPE_H264_TIMESTAMPER);

static void
gst_h264_timestamper_class_init (GstH264TimestamperClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstCodecTimestamperClass *timestamper_class =
      GST_CODEC_TIMESTAMPER_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &sinktemplate);
  gst_element_class_add_static_pad_template (element_class, &srctemplate);

  gst_element_class_set_static_metadata (element_class, "H.264 timestamper",
      "Codec/Video/Timestamper", "Timestamp H.264 streams",
      "Seungha Yang <seungha@centricular.com>");

  timestamper_class->start = GST_DEBUG_FUNCPTR (gst_h264_timestamper_start);
  timestamper_class->stop = GST_DEBUG_FUNCPTR (gst_h264_timestamper_stop);
  timestamper_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_h264_timestamper_set_caps);
  timestamper_class->handle_buffer =
      GST_DEBUG_FUNCPTR (gst_h264_timestamper_handle_buffer);

  GST_DEBUG_CATEGORY_INIT (gst_h264_timestamper_debug, "h264timestamper", 0,
      "h264timestamper");
}

static void
gst_h264_timestamper_init (GstH264Timestamper * self)
{
}

static gboolean
gst_h264_timestamper_set_caps (GstCodecTimestamper * timestamper,
    GstCaps * caps)
{
  GstH264Timestamper *self = GST_H264_TIMESTAMPER (timestamper);
  GstStructure *s = gst_caps_get_structure (caps, 0);
  const gchar *str;
  gboolean found_format = FALSE;
  const GValue *codec_data_val;

  self->packetized = FALSE;
  self->nal_length_size = 4;
  str = gst_structure_get_string (s, "stream-format");
  if (g_strcmp0 (str, "avc") == 0 || g_strcmp0 (str, "avc3") == 0) {
    self->packetized = TRUE;
    found_format = TRUE;
  } else if (g_strcmp0 (str, "byte-stream") == 0) {
    found_format = TRUE;
  }

  codec_data_val = gst_structure_get_value (s, "codec_data");
  if (codec_data_val && GST_VALUE_HOLDS_BUFFER (codec_data_val)) {
    GstBuffer *codec_data = gst_value_get_buffer (codec_data_val);
    GstMapInfo map;
    GstH264NalUnit *nalu;
    GstH264ParserResult ret;
    GstH264DecoderConfigRecord *config = NULL;
    guint i;

    if (!gst_buffer_map (codec_data, &map, GST_MAP_READ)) {
      GST_ERROR_OBJECT (self, "Unable to map codec-data buffer");
      return FALSE;
    }

    ret = gst_h264_parser_parse_decoder_config_record (self->parser,
        map.data, map.size, &config);
    if (ret != GST_H264_PARSER_OK) {
      GST_WARNING_OBJECT (self, "Failed to parse codec-data");
      goto unmap;
    }

    self->nal_length_size = config->length_size_minus_one + 1;
    for (i = 0; i < config->sps->len; i++) {
      nalu = &g_array_index (config->sps, GstH264NalUnit, i);
      gst_h264_timestamper_process_nal (self, nalu);
    }

    for (i = 0; i < config->pps->len; i++) {
      nalu = &g_array_index (config->pps, GstH264NalUnit, i);
      gst_h264_timestamper_process_nal (self, nalu);
    }

    /* codec_data would mean packetized format */
    if (!found_format)
      self->packetized = TRUE;

  unmap:
    gst_buffer_unmap (codec_data, &map);
    g_clear_pointer (&config, gst_h264_decoder_config_record_free);
  }

  return TRUE;
}

typedef struct
{
  GstH264Level level;

  guint32 max_mbps;
  guint32 max_fs;
  guint32 max_dpb_mbs;
  guint32 max_main_br;
} LevelLimits;

static const LevelLimits level_limits_map[] = {
  {GST_H264_LEVEL_L1, 1485, 99, 396, 64},
  {GST_H264_LEVEL_L1B, 1485, 99, 396, 128},
  {GST_H264_LEVEL_L1_1, 3000, 396, 900, 192},
  {GST_H264_LEVEL_L1_2, 6000, 396, 2376, 384},
  {GST_H264_LEVEL_L1_3, 11800, 396, 2376, 768},
  {GST_H264_LEVEL_L2, 11880, 396, 2376, 2000},
  {GST_H264_LEVEL_L2_1, 19800, 792, 4752, 4000},
  {GST_H264_LEVEL_L2_2, 20250, 1620, 8100, 4000},
  {GST_H264_LEVEL_L3, 40500, 1620, 8100, 10000},
  {GST_H264_LEVEL_L3_1, 108000, 3600, 18000, 14000},
  {GST_H264_LEVEL_L3_2, 216000, 5120, 20480, 20000},
  {GST_H264_LEVEL_L4, 245760, 8192, 32768, 20000},
  {GST_H264_LEVEL_L4_1, 245760, 8192, 32768, 50000},
  {GST_H264_LEVEL_L4_2, 522240, 8704, 34816, 50000},
  {GST_H264_LEVEL_L5, 589824, 22080, 110400, 135000},
  {GST_H264_LEVEL_L5_1, 983040, 36864, 184320, 240000},
  {GST_H264_LEVEL_L5_2, 2073600, 36864, 184320, 240000},
  {GST_H264_LEVEL_L6, 4177920, 139264, 696320, 240000},
  {GST_H264_LEVEL_L6_1, 8355840, 139264, 696320, 480000},
  {GST_H264_LEVEL_L6_2, 16711680, 139264, 696320, 800000}
};

static guint
h264_level_to_max_dpb_mbs (GstH264Level level)
{
  gint i;
  for (i = 0; i < G_N_ELEMENTS (level_limits_map); i++) {
    if (level == level_limits_map[i].level)
      return level_limits_map[i].max_dpb_mbs;
  }

  return 0;
}

static void
gst_h264_timestamper_process_sps (GstH264Timestamper * self, GstH264SPS * sps)
{
  guint8 level;
  guint max_dpb_mbs;
  guint width_mb, height_mb;
  guint max_dpb_frames = 0;
  guint max_reorder_frames = 0;

  /* Spec A.3.1 and A.3.2
   * For Baseline, Constrained Baseline and Main profile, the indicated level is
   * Level 1b if level_idc is equal to 11 and constraint_set3_flag is equal to 1
   */
  level = sps->level_idc;
  if (level == 11 && (sps->profile_idc == 66 || sps->profile_idc == 77) &&
      sps->constraint_set3_flag) {
    /* Level 1b */
    level = 9;
  }

  max_dpb_mbs = h264_level_to_max_dpb_mbs ((GstH264Level) level);
  if (sps->vui_parameters_present_flag
      && sps->vui_parameters.bitstream_restriction_flag) {
    max_dpb_frames = MAX (1, sps->vui_parameters.max_dec_frame_buffering);
  } else if (max_dpb_mbs != 0) {
    width_mb = sps->width / 16;
    height_mb = sps->height / 16;

    max_dpb_frames = MIN (max_dpb_mbs / (width_mb * height_mb), 16);
  } else {
    GST_WARNING_OBJECT (self, "Unable to get MAX DPB MBs");
    max_dpb_frames = 16;
  }

  GST_DEBUG_OBJECT (self, "Max DPB size %d", max_dpb_frames);

  max_reorder_frames = max_dpb_frames;
  if (sps->vui_parameters_present_flag
      && sps->vui_parameters.bitstream_restriction_flag) {
    max_reorder_frames = sps->vui_parameters.num_reorder_frames;
    if (max_reorder_frames > max_dpb_frames) {
      GST_WARNING_OBJECT (self, "num_reorder_frames %d > dpb size %d",
          max_reorder_frames, max_dpb_frames);
      max_reorder_frames = max_dpb_frames;
    }
  } else {
    if (sps->profile_idc == 66 || sps->profile_idc == 83) {
      /* baseline, constrained baseline and scalable-baseline profiles
         only contain I/P frames. */
      max_reorder_frames = 0;
    } else if (sps->constraint_set3_flag) {
      /* constraint_set3_flag may mean the -intra only profile. */
      switch (sps->profile_idc) {
        case 44:
        case 86:
        case 100:
        case 110:
        case 122:
        case 244:
          max_reorder_frames = 0;
          break;
        default:
          break;
      }
    }
  }

  GST_DEBUG_OBJECT (self, "Max num reorder frames %d", max_reorder_frames);

  gst_codec_timestamper_set_window_size (GST_CODEC_TIMESTAMPER_CAST (self),
      max_reorder_frames);
}

static void
gst_h264_timestamper_process_nal (GstH264Timestamper * self,
    GstH264NalUnit * nalu)
{
  GstH264ParserResult ret;

  switch (nalu->type) {
    case GST_H264_NAL_SPS:{
      GstH264SPS sps;
      ret = gst_h264_parser_parse_sps (self->parser, nalu, &sps);
      if (ret != GST_H264_PARSER_OK) {
        GST_WARNING_OBJECT (self, "Failed to parse SPS");
        break;
      }

      gst_h264_timestamper_process_sps (self, &sps);
      gst_h264_sps_clear (&sps);
      break;
    }
      /* TODO: parse PPS/SLICE and correct PTS based on POC if needed */
    default:
      break;
  }
}

static GstFlowReturn
gst_h264_timestamper_handle_buffer (GstCodecTimestamper * timestamper,
    GstBuffer * buffer)
{
  GstH264Timestamper *self = GST_H264_TIMESTAMPER (timestamper);
  GstMapInfo map;

  /* Ignore any error while parsing NAL */
  if (gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GstH264ParserResult ret;
    GstH264NalUnit nalu;

    if (self->packetized) {
      ret = gst_h264_parser_identify_nalu_avc (self->parser,
          map.data, 0, map.size, self->nal_length_size, &nalu);

      while (ret == GST_H264_PARSER_OK) {
        gst_h264_timestamper_process_nal (self, &nalu);

        ret = gst_h264_parser_identify_nalu_avc (self->parser,
            map.data, nalu.offset + nalu.size, map.size, self->nal_length_size,
            &nalu);
      }
    } else {
      ret = gst_h264_parser_identify_nalu (self->parser,
          map.data, 0, map.size, &nalu);

      if (ret == GST_H264_PARSER_NO_NAL_END)
        ret = GST_H264_PARSER_OK;

      while (ret == GST_H264_PARSER_OK) {
        gst_h264_timestamper_process_nal (self, &nalu);

        ret = gst_h264_parser_identify_nalu (self->parser,
            map.data, nalu.offset + nalu.size, map.size, &nalu);

        if (ret == GST_H264_PARSER_NO_NAL_END)
          ret = GST_H264_PARSER_OK;
      }
    }
    gst_buffer_unmap (buffer, &map);
  }

  return GST_FLOW_OK;
}

static void
gst_h264_timestamper_reset (GstH264Timestamper * self)
{
  g_clear_pointer (&self->parser, gst_h264_nal_parser_free);
}

static gboolean
gst_h264_timestamper_start (GstCodecTimestamper * timestamper)
{
  GstH264Timestamper *self = GST_H264_TIMESTAMPER (timestamper);

  gst_h264_timestamper_reset (self);

  self->parser = gst_h264_nal_parser_new ();

  return TRUE;
}

static gboolean
gst_h264_timestamper_stop (GstCodecTimestamper * timestamper)
{
  GstH264Timestamper *self = GST_H264_TIMESTAMPER (timestamper);

  gst_h264_timestamper_reset (self);

  return TRUE;
}
