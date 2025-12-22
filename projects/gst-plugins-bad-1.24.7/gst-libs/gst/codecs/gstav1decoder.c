/* GStreamer
 * Copyright (C) 2020 He Junyan <junyan.he@intel.com>
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
 * SECTION:gstav1decoder
 * @title: Gstav1Decoder
 * @short_description: Base class to implement stateless AV1 decoders
 * @sources:
 * - gstav1picture.h
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/base/base.h>
#include "gstav1decoder.h"

GST_DEBUG_CATEGORY (gst_av1_decoder_debug);
#define GST_CAT_DEFAULT gst_av1_decoder_debug

struct _GstAV1DecoderPrivate
{
  gint max_width;
  gint max_height;
  GstAV1Profile profile;
  GstAV1Parser *parser;
  GstAV1Dpb *dpb;
  GstAV1Picture *current_picture;
  GstVideoCodecFrame *current_frame;

  guint preferred_output_delay;
  GstQueueArray *output_queue;
  gboolean is_live;

  gboolean input_state_changed;
};

typedef struct
{
  /* Holds ref */
  GstVideoCodecFrame *frame;
  GstAV1Picture *picture;
  /* Without ref */
  GstAV1Decoder *self;
} GstAV1DecoderOutputFrame;

#define parent_class gst_av1_decoder_parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstAV1Decoder, gst_av1_decoder,
    GST_TYPE_VIDEO_DECODER,
    G_ADD_PRIVATE (GstAV1Decoder);
    GST_DEBUG_CATEGORY_INIT (gst_av1_decoder_debug, "av1decoder", 0,
        "AV1 Video Decoder"));

static gint
_floor_log2 (guint32 x)
{
  gint s = 0;

  while (x != 0) {
    x = x >> 1;
    s++;
  }
  return s - 1;
}

static void gst_av1_decoder_finalize (GObject * object);
static gboolean gst_av1_decoder_start (GstVideoDecoder * decoder);
static gboolean gst_av1_decoder_stop (GstVideoDecoder * decoder);
static gboolean gst_av1_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_av1_decoder_negotiate (GstVideoDecoder * decoder);
static GstFlowReturn gst_av1_decoder_finish (GstVideoDecoder * decoder);
static gboolean gst_av1_decoder_flush (GstVideoDecoder * decoder);
static GstFlowReturn gst_av1_decoder_drain (GstVideoDecoder * decoder);
static GstFlowReturn gst_av1_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static void
gst_av1_decoder_clear_output_frame (GstAV1DecoderOutputFrame * output_frame);

static void
gst_av1_decoder_class_init (GstAV1DecoderClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  object_class->finalize = gst_av1_decoder_finalize;

  decoder_class->start = GST_DEBUG_FUNCPTR (gst_av1_decoder_start);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_av1_decoder_stop);
  decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_av1_decoder_set_format);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_av1_decoder_negotiate);
  decoder_class->finish = GST_DEBUG_FUNCPTR (gst_av1_decoder_finish);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_av1_decoder_flush);
  decoder_class->drain = GST_DEBUG_FUNCPTR (gst_av1_decoder_drain);
  decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_av1_decoder_handle_frame);
}

static void
gst_av1_decoder_init (GstAV1Decoder * self)
{
  GstAV1DecoderPrivate *priv;

  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (self), TRUE);
  gst_video_decoder_set_needs_format (GST_VIDEO_DECODER (self), TRUE);

  self->priv = priv = gst_av1_decoder_get_instance_private (self);

  priv->output_queue =
      gst_queue_array_new_for_struct (sizeof (GstAV1DecoderOutputFrame), 1);
  gst_queue_array_set_clear_func (priv->output_queue,
      (GDestroyNotify) gst_av1_decoder_clear_output_frame);
}

static void
gst_av1_decoder_finalize (GObject * object)
{
  GstAV1Decoder *self = GST_AV1_DECODER (object);
  GstAV1DecoderPrivate *priv = self->priv;

  gst_queue_array_free (priv->output_queue);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_av1_decoder_reset (GstAV1Decoder * self)
{
  GstAV1DecoderPrivate *priv = self->priv;

  self->highest_spatial_layer = 0;

  priv->max_width = 0;
  priv->max_height = 0;
  gst_clear_av1_picture (&priv->current_picture);
  priv->current_frame = NULL;
  priv->profile = GST_AV1_PROFILE_UNDEFINED;

  if (priv->dpb)
    gst_av1_dpb_clear (priv->dpb);
  if (priv->parser)
    gst_av1_parser_reset (priv->parser, FALSE);

  gst_queue_array_clear (priv->output_queue);
}

static gboolean
gst_av1_decoder_start (GstVideoDecoder * decoder)
{
  GstAV1Decoder *self = GST_AV1_DECODER (decoder);
  GstAV1DecoderPrivate *priv = self->priv;

  priv->parser = gst_av1_parser_new ();
  priv->dpb = gst_av1_dpb_new ();

  gst_av1_decoder_reset (self);

  return TRUE;
}

static gboolean
gst_av1_decoder_stop (GstVideoDecoder * decoder)
{
  GstAV1Decoder *self = GST_AV1_DECODER (decoder);
  GstAV1DecoderPrivate *priv = self->priv;

  gst_av1_decoder_reset (self);

  g_clear_pointer (&self->input_state, gst_video_codec_state_unref);
  g_clear_pointer (&priv->parser, gst_av1_parser_free);
  g_clear_pointer (&priv->dpb, gst_av1_dpb_free);

  return TRUE;
}

static void
gst_av1_decoder_clear_output_frame (GstAV1DecoderOutputFrame * output_frame)
{
  if (!output_frame)
    return;

  if (output_frame->frame) {
    gst_video_decoder_release_frame (GST_VIDEO_DECODER (output_frame->self),
        output_frame->frame);
    output_frame->frame = NULL;
  }

  gst_clear_av1_picture (&output_frame->picture);
}

static gboolean
gst_av1_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstAV1Decoder *self = GST_AV1_DECODER (decoder);
  GstAV1DecoderPrivate *priv = self->priv;
  GstQuery *query;

  GST_DEBUG_OBJECT (decoder, "Set format");

  priv->input_state_changed = TRUE;

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);

  self->input_state = gst_video_codec_state_ref (state);

  priv->max_width = GST_VIDEO_INFO_WIDTH (&state->info);
  priv->max_height = GST_VIDEO_INFO_HEIGHT (&state->info);

  priv->is_live = FALSE;
  query = gst_query_new_latency ();
  if (gst_pad_peer_query (GST_VIDEO_DECODER_SINK_PAD (self), query))
    gst_query_parse_latency (query, &priv->is_live, NULL, NULL);
  gst_query_unref (query);

  return TRUE;
}

static gboolean
gst_av1_decoder_negotiate (GstVideoDecoder * decoder)
{
  GstAV1Decoder *self = GST_AV1_DECODER (decoder);

  /* output state must be updated by subclass using new input state already */
  self->priv->input_state_changed = FALSE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static void
gst_av1_decoder_drain_output_queue (GstAV1Decoder * self,
    guint num, GstFlowReturn * ret)
{
  GstAV1DecoderClass *klass = GST_AV1_DECODER_GET_CLASS (self);
  GstAV1DecoderPrivate *priv = self->priv;

  g_assert (klass->output_picture);

  while (gst_queue_array_get_length (priv->output_queue) > num) {
    GstAV1DecoderOutputFrame *output_frame = (GstAV1DecoderOutputFrame *)
        gst_queue_array_pop_head_struct (priv->output_queue);
    GstFlowReturn flow_ret = klass->output_picture (self,
        output_frame->frame, output_frame->picture);

    if (*ret == GST_FLOW_OK)
      *ret = flow_ret;
  }
}

static GstFlowReturn
gst_av1_decoder_finish (GstVideoDecoder * decoder)
{
  GstAV1Decoder *self = GST_AV1_DECODER (decoder);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT (decoder, "finish");

  gst_av1_decoder_drain_output_queue (self, 0, &ret);
  gst_av1_decoder_reset (self);

  return ret;
}

static gboolean
gst_av1_decoder_flush (GstVideoDecoder * decoder)
{
  GST_DEBUG_OBJECT (decoder, "flush");

  gst_av1_decoder_reset (GST_AV1_DECODER (decoder));

  return TRUE;
}

static GstFlowReturn
gst_av1_decoder_drain (GstVideoDecoder * decoder)
{
  GstAV1Decoder *self = GST_AV1_DECODER (decoder);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT (decoder, "drain");

  gst_av1_decoder_drain_output_queue (self, 0, &ret);
  gst_av1_decoder_reset (self);

  return ret;
}

static const gchar *
get_obu_name (GstAV1OBUType type)
{
  switch (type) {
    case GST_AV1_OBU_SEQUENCE_HEADER:
      return "sequence header";
    case GST_AV1_OBU_TEMPORAL_DELIMITER:
      return "temporal delimiter";
    case GST_AV1_OBU_FRAME_HEADER:
      return "frame header";
    case GST_AV1_OBU_TILE_GROUP:
      return "tile group";
    case GST_AV1_OBU_METADATA:
      return "metadata";
    case GST_AV1_OBU_FRAME:
      return "frame";
    case GST_AV1_OBU_REDUNDANT_FRAME_HEADER:
      return "redundant frame header";
    case GST_AV1_OBU_TILE_LIST:
      return "tile list";
    case GST_AV1_OBU_PADDING:
      return "padding";
    default:
      return "unknown";
  }

  return NULL;
}

static const gchar *
gst_av1_decoder_profile_to_string (GstAV1Profile profile)
{
  switch (profile) {
    case GST_AV1_PROFILE_0:
      return "0";
    case GST_AV1_PROFILE_1:
      return "1";
    case GST_AV1_PROFILE_2:
      return "2";
    default:
      break;
  }

  return NULL;
}

static GstFlowReturn
gst_av1_decoder_process_sequence (GstAV1Decoder * self, GstAV1OBU * obu)
{
  GstAV1ParserResult res;
  GstAV1DecoderPrivate *priv = self->priv;
  GstAV1SequenceHeaderOBU seq_header;
  GstAV1SequenceHeaderOBU old_seq_header = { 0, };
  GstAV1DecoderClass *klass = GST_AV1_DECODER_GET_CLASS (self);
  GstFlowReturn ret = GST_FLOW_OK;

  if (priv->parser->seq_header)
    old_seq_header = *priv->parser->seq_header;

  res = gst_av1_parser_parse_sequence_header_obu (priv->parser,
      obu, &seq_header);
  if (res != GST_AV1_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Parsing sequence failed.");
    return GST_FLOW_ERROR;
  }

  if (!memcmp (&old_seq_header, &seq_header, sizeof (GstAV1SequenceHeaderOBU))) {
    GST_DEBUG_OBJECT (self, "Get same sequence header.");
    return GST_FLOW_OK;
  }

  g_assert (klass->new_sequence);

  GST_DEBUG_OBJECT (self,
      "Sequence updated, profile %s -> %s, max resolution: %dx%d -> %dx%d",
      gst_av1_decoder_profile_to_string (priv->profile),
      gst_av1_decoder_profile_to_string (seq_header.seq_profile),
      priv->max_width, priv->max_height, seq_header.max_frame_width_minus_1 + 1,
      seq_header.max_frame_height_minus_1 + 1);

  gst_av1_decoder_drain_output_queue (self, 0, &ret);
  gst_av1_dpb_clear (priv->dpb);

  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "Draining for new sequence returned %s",
        gst_flow_get_name (ret));
    return ret;
  }

  if (klass->get_preferred_output_delay) {
    priv->preferred_output_delay =
        klass->get_preferred_output_delay (self, priv->is_live);
  } else {
    priv->preferred_output_delay = 0;
  }

  if (priv->parser->state.operating_point_idc) {
    self->highest_spatial_layer =
        _floor_log2 (priv->parser->state.operating_point_idc >> 8);
    GST_INFO_OBJECT (self, "set highest spatial layer to %d",
        self->highest_spatial_layer);
  } else {
    self->highest_spatial_layer = 0;
  }

  ret = klass->new_sequence (self, &seq_header,
      /* +1 for the current frame */
      GST_AV1_TOTAL_REFS_PER_FRAME + 1 + priv->preferred_output_delay);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (self, "subclass does not want accept new sequence");
    return ret;
  }

  priv->profile = seq_header.seq_profile;
  priv->max_width = seq_header.max_frame_width_minus_1 + 1;
  priv->max_height = seq_header.max_frame_height_minus_1 + 1;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_av1_decoder_decode_tile_group (GstAV1Decoder * self,
    GstAV1TileGroupOBU * tile_group, GstAV1OBU * obu)
{
  GstAV1DecoderPrivate *priv = self->priv;
  GstAV1DecoderClass *klass = GST_AV1_DECODER_GET_CLASS (self);
  GstAV1Picture *picture = priv->current_picture;
  GstAV1Tile tile;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!picture) {
    GST_ERROR_OBJECT (self, "No picture has created for current frame");
    return GST_FLOW_ERROR;
  }

  if (picture->frame_hdr.show_existing_frame) {
    GST_ERROR_OBJECT (self, "Current picture is showing the existing frame.");
    return GST_FLOW_ERROR;
  }

  tile.obu = *obu;
  tile.tile_group = *tile_group;

  g_assert (klass->decode_tile);
  ret = klass->decode_tile (self, picture, &tile);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "Decode tile error");
    return ret;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_av1_decoder_decode_frame_header (GstAV1Decoder * self,
    GstAV1OBU * obu, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1DecoderPrivate *priv = self->priv;
  GstAV1DecoderClass *klass = GST_AV1_DECODER_GET_CLASS (self);
  GstAV1Picture *picture = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  g_assert (priv->current_frame);

  if (priv->current_picture != NULL) {
    GST_ERROR_OBJECT (self, "Already have picture for current frame");
    return GST_FLOW_ERROR;
  }

  if (frame_header->show_existing_frame) {
    GstAV1Picture *ref_picture;

    ref_picture = priv->dpb->pic_list[frame_header->frame_to_show_map_idx];
    if (!ref_picture) {
      GST_WARNING_OBJECT (self, "Failed to find the frame index %d to show.",
          frame_header->frame_to_show_map_idx);
      return GST_FLOW_ERROR;
    }

    /* The duplicated picture, if a key frame, will be placed in the DPB and
     * for this reason is not optional. */
    g_assert (klass->duplicate_picture);
    picture = klass->duplicate_picture (self, priv->current_frame, ref_picture);
    if (!picture) {
      GST_ERROR_OBJECT (self, "subclass didn't provide duplicated picture");
      return GST_FLOW_ERROR;
    }

    GST_CODEC_PICTURE_COPY_FRAME_NUMBER (picture, ref_picture);
    picture->frame_hdr = *frame_header;
    priv->current_picture = picture;
  } else {
    picture = gst_av1_picture_new ();
    picture->frame_hdr = *frame_header;
    picture->display_frame_id = frame_header->display_frame_id;
    picture->show_frame = frame_header->show_frame;
    picture->showable_frame = frame_header->showable_frame;
    picture->apply_grain = frame_header->film_grain_params.apply_grain;
    GST_CODEC_PICTURE_FRAME_NUMBER (picture) =
        priv->current_frame->system_frame_number;
    picture->temporal_id = obu->header.obu_temporal_id;
    picture->spatial_id = obu->header.obu_spatial_id;

    g_assert (picture->spatial_id <= self->highest_spatial_layer);
    g_assert (self->highest_spatial_layer < GST_AV1_MAX_NUM_SPATIAL_LAYERS);

    if (!frame_header->show_frame && !frame_header->showable_frame)
      GST_VIDEO_CODEC_FRAME_FLAG_SET (priv->current_frame,
          GST_VIDEO_CODEC_FRAME_FLAG_DECODE_ONLY);

    if (klass->new_picture) {
      ret = klass->new_picture (self, priv->current_frame, picture);
      if (ret != GST_FLOW_OK) {
        GST_WARNING_OBJECT (self, "new picture error");
        return ret;
      }
    }
    priv->current_picture = picture;

    if (klass->start_picture) {
      ret = klass->start_picture (self, picture, priv->dpb);
      if (ret != GST_FLOW_OK) {
        GST_WARNING_OBJECT (self, "start picture error");
        return ret;
      }
    }
  }

  g_assert (priv->current_picture != NULL);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_av1_decoder_process_frame_header (GstAV1Decoder * self, GstAV1OBU * obu)
{
  GstAV1ParserResult res;
  GstAV1DecoderPrivate *priv = self->priv;
  GstAV1FrameHeaderOBU frame_header;

  res = gst_av1_parser_parse_frame_header_obu (priv->parser, obu,
      &frame_header);
  if (res != GST_AV1_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Parsing frame header failed.");
    return GST_FLOW_ERROR;
  }

  return gst_av1_decoder_decode_frame_header (self, obu, &frame_header);
}

static GstFlowReturn
gst_av1_decoder_process_tile_group (GstAV1Decoder * self, GstAV1OBU * obu)
{
  GstAV1ParserResult res;
  GstAV1DecoderPrivate *priv = self->priv;
  GstAV1TileGroupOBU tile_group;

  res = gst_av1_parser_parse_tile_group_obu (priv->parser, obu, &tile_group);
  if (res != GST_AV1_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Parsing tile group failed.");
    return GST_FLOW_ERROR;
  }

  return gst_av1_decoder_decode_tile_group (self, &tile_group, obu);
}

static GstFlowReturn
gst_av1_decoder_process_frame (GstAV1Decoder * self, GstAV1OBU * obu)
{
  GstAV1ParserResult res;
  GstAV1DecoderPrivate *priv = self->priv;
  GstAV1FrameOBU frame;
  GstFlowReturn ret = GST_FLOW_OK;

  res = gst_av1_parser_parse_frame_obu (priv->parser, obu, &frame);
  if (res != GST_AV1_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Parsing frame failed.");
    return GST_FLOW_ERROR;
  }

  ret = gst_av1_decoder_decode_frame_header (self, obu, &frame.frame_header);
  if (ret != GST_FLOW_OK)
    return ret;

  return gst_av1_decoder_decode_tile_group (self, &frame.tile_group, obu);
}

static GstFlowReturn
gst_av1_decoder_temporal_delimiter (GstAV1Decoder * self, GstAV1OBU * obu)
{
  GstAV1DecoderPrivate *priv = self->priv;

  if (gst_av1_parser_parse_temporal_delimiter_obu (priv->parser, obu) ==
      GST_AV1_PARSER_OK) {
    return GST_FLOW_OK;
  }

  return GST_FLOW_ERROR;
}

static GstFlowReturn
gst_av1_decoder_decode_one_obu (GstAV1Decoder * self, GstAV1OBU * obu)
{
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (self, "Decode obu %s", get_obu_name (obu->obu_type));
  switch (obu->obu_type) {
    case GST_AV1_OBU_SEQUENCE_HEADER:
      ret = gst_av1_decoder_process_sequence (self, obu);
      break;
    case GST_AV1_OBU_FRAME_HEADER:
      ret = gst_av1_decoder_process_frame_header (self, obu);
      break;
    case GST_AV1_OBU_FRAME:
      ret = gst_av1_decoder_process_frame (self, obu);
      break;
    case GST_AV1_OBU_TILE_GROUP:
      ret = gst_av1_decoder_process_tile_group (self, obu);
      break;
    case GST_AV1_OBU_TEMPORAL_DELIMITER:
      ret = gst_av1_decoder_temporal_delimiter (self, obu);
      break;
      /* TODO: may need to handled. */
    case GST_AV1_OBU_METADATA:
    case GST_AV1_OBU_REDUNDANT_FRAME_HEADER:
    case GST_AV1_OBU_TILE_LIST:
    case GST_AV1_OBU_PADDING:
      break;
    default:
      GST_WARNING_OBJECT (self, "an unrecognized obu type %d", obu->obu_type);
      break;
  }

  if (ret != GST_FLOW_OK)
    GST_WARNING_OBJECT (self, "Failed to handle %s OBU",
        get_obu_name (obu->obu_type));

  return ret;
}

static void
gst_av1_decoder_update_state (GstAV1Decoder * self)
{
  GstAV1DecoderPrivate *priv = self->priv;
  GstAV1Picture *picture = priv->current_picture;
  GstAV1ParserResult res;
  GstAV1FrameHeaderOBU *fh;

  g_assert (picture);
  fh = &picture->frame_hdr;

  /* This is a show_existing_frame case, only update key frame */
  if (fh->show_existing_frame && fh->frame_type != GST_AV1_KEY_FRAME)
    return;

  res = gst_av1_parser_reference_frame_update (priv->parser, fh);
  if (res != GST_AV1_PARSER_OK) {
    GST_ERROR_OBJECT (self, "failed to update the reference.");
    return;
  }

  gst_av1_dpb_add (priv->dpb, gst_av1_picture_ref (picture));
}

static GstFlowReturn
gst_av1_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstAV1Decoder *self = GST_AV1_DECODER (decoder);
  GstAV1DecoderPrivate *priv = self->priv;
  GstAV1DecoderClass *klass = GST_AV1_DECODER_GET_CLASS (self);
  GstBuffer *in_buf = frame->input_buffer;
  GstMapInfo map;
  GstFlowReturn ret = GST_FLOW_OK;
  GstFlowReturn output_ret = GST_FLOW_OK;
  guint32 total_consumed, consumed;
  GstAV1OBU obu;
  GstAV1ParserResult res;

  GST_LOG_OBJECT (self, "handle frame id %d, buf %" GST_PTR_FORMAT,
      frame->system_frame_number, in_buf);

  priv->current_frame = frame;
  g_assert (!priv->current_picture);

  if (!gst_buffer_map (in_buf, &map, GST_MAP_READ)) {
    priv->current_frame = NULL;
    GST_ERROR_OBJECT (self, "can not map input buffer");

    return GST_FLOW_ERROR;
  }

  total_consumed = 0;
  while (total_consumed < map.size) {
    res = gst_av1_parser_identify_one_obu (priv->parser,
        map.data + total_consumed, map.size, &obu, &consumed);
    if (res == GST_AV1_PARSER_DROP) {
      total_consumed += consumed;
      continue;
    }

    if (res != GST_AV1_PARSER_OK) {
      ret = GST_FLOW_ERROR;
      goto out;
    }

    ret = gst_av1_decoder_decode_one_obu (self, &obu);
    if (ret != GST_FLOW_OK) {
      goto out;
    }

    total_consumed += consumed;
  }

  if (!priv->current_picture) {
    GST_ERROR_OBJECT (self, "No valid picture after exhaust input frame");
    ret = GST_FLOW_ERROR;
    goto out;
  }

  if (priv->current_picture->spatial_id > self->highest_spatial_layer) {
    ret = GST_FLOW_ERROR;
    GST_VIDEO_DECODER_ERROR (self, 1, STREAM, DECODE,
        ("current picture spatial_id %d should not be higher than "
            "highest spatial layer %d", priv->current_picture->spatial_id,
            self->highest_spatial_layer), (NULL), ret);
    goto out;
  }

  if (!priv->current_picture->frame_hdr.show_existing_frame) {
    if (klass->end_picture) {
      ret = klass->end_picture (self, priv->current_picture);
      if (ret != GST_FLOW_OK) {
        GST_WARNING_OBJECT (self, "end picture error");
        goto out;
      }
    }
  }

  gst_av1_decoder_update_state (self);

out:
  gst_buffer_unmap (in_buf, &map);

  if (ret == GST_FLOW_OK) {
    if (priv->current_picture->frame_hdr.show_frame ||
        priv->current_picture->frame_hdr.show_existing_frame) {
      /* Only output one frame with the highest spatial id from each TU
       * when there are multiple spatial layers.
       */
      if (obu.header.obu_spatial_id < self->highest_spatial_layer) {
        gst_av1_picture_unref (priv->current_picture);
        gst_video_decoder_release_frame (decoder, frame);
      } else {
        GstAV1DecoderOutputFrame output_frame;

        /* If subclass didn't update output state at this point,
         * marking this picture as a discont and stores current input state */
        if (priv->input_state_changed) {
          gst_av1_picture_set_discont_state (priv->current_picture,
              self->input_state);
          priv->input_state_changed = FALSE;
        }

        output_frame.frame = frame;
        output_frame.picture = priv->current_picture;
        output_frame.self = self;

        gst_queue_array_push_tail_struct (priv->output_queue, &output_frame);
      }
    } else {
      GST_LOG_OBJECT (self, "Decode only picture %p", priv->current_picture);
      GST_VIDEO_CODEC_FRAME_SET_DECODE_ONLY (frame);
      gst_av1_picture_unref (priv->current_picture);
      ret = gst_video_decoder_finish_frame (GST_VIDEO_DECODER (self), frame);
    }
  } else {
    if (priv->current_picture)
      gst_av1_picture_unref (priv->current_picture);

    gst_video_decoder_release_frame (decoder, frame);
  }

  gst_av1_decoder_drain_output_queue (self,
      priv->preferred_output_delay, &output_ret);

  priv->current_picture = NULL;
  priv->current_frame = NULL;

  if (output_ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (self,
        "Output returned %s", gst_flow_get_name (output_ret));
    return output_ret;
  }

  if (ret == GST_FLOW_ERROR) {
    GST_VIDEO_DECODER_ERROR (decoder, 1, STREAM, DECODE,
        ("Failed to handle the frame %d", frame->system_frame_number),
        NULL, ret);
  }

  return ret;
}
