/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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
 *
 * NOTE: some of implementations are copied/modified from Chromium code
 *
 * Copyright 2015 The Chromium Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * SECTION:gsth264decoder
 * @title: GstH264Decoder
 * @short_description: Base class to implement stateless H.264 decoders
 * @sources:
 * - gsth264picture.h
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/base/base.h>
#include "gsth264decoder.h"
#include "gsth264picture-private.h"

GST_DEBUG_CATEGORY (gst_h264_decoder_debug);
#define GST_CAT_DEFAULT gst_h264_decoder_debug

typedef enum
{
  GST_H264_DECODER_FORMAT_NONE,
  GST_H264_DECODER_FORMAT_AVC,
  GST_H264_DECODER_FORMAT_BYTE
} GstH264DecoderFormat;

typedef enum
{
  GST_H264_DECODER_ALIGN_NONE,
  GST_H264_DECODER_ALIGN_NAL,
  GST_H264_DECODER_ALIGN_AU
} GstH264DecoderAlign;

struct _GstH264DecoderPrivate
{
  GstH264DecoderCompliance compliance;

  guint8 profile_idc;
  gint width, height;

  guint nal_length_size;

  /* state */
  GstH264DecoderFormat in_format;
  GstH264DecoderAlign align;
  GstH264NalParser *parser;
  GstH264Dpb *dpb;
  /* Cache last field which can not enter the DPB, should be a non ref */
  GstH264Picture *last_field;

  /* used for low-latency vs. high throughput mode decision */
  gboolean is_live;

  /* sps/pps of the current slice */
  const GstH264SPS *active_sps;
  const GstH264PPS *active_pps;

  /* Picture currently being processed/decoded */
  GstH264Picture *current_picture;
  GstVideoCodecFrame *current_frame;

  /* Slice (slice header + nalu) currently being processed/decodec */
  GstH264Slice current_slice;

  gint max_frame_num;
  gint max_pic_num;
  gint max_long_term_frame_idx;

  gint prev_frame_num;
  gint prev_ref_frame_num;
  gint prev_frame_num_offset;
  gboolean prev_has_memmgmnt5;

  /* Values related to previously decoded reference picture */
  gboolean prev_ref_has_memmgmnt5;
  gint prev_ref_top_field_order_cnt;
  gint prev_ref_pic_order_cnt_msb;
  gint prev_ref_pic_order_cnt_lsb;

  GstH264PictureField prev_ref_field;

  gboolean process_ref_pic_lists;
  guint preferred_output_delay;

  /* Reference picture lists, constructed for each frame */
  GArray *ref_pic_list_p0;
  GArray *ref_pic_list_b0;
  GArray *ref_pic_list_b1;

  /* Temporary picture list, for reference picture lists in fields,
   * corresponding to 8.2.4.2.2 refFrameList0ShortTerm, refFrameList0LongTerm
   * and 8.2.4.2.5 refFrameList1ShortTerm and refFrameListLongTerm */
  GArray *ref_frame_list_0_short_term;
  GArray *ref_frame_list_1_short_term;
  GArray *ref_frame_list_long_term;

  /* Reference picture lists, constructed for each slice */
  GArray *ref_pic_list0;
  GArray *ref_pic_list1;

  /* Split packetized data into actual nal chunks (for malformed stream) */
  GArray *split_nalu;

  /* For delayed output */
  GstQueueArray *output_queue;

  gboolean input_state_changed;

  /* Return value from output_picture() */
  GstFlowReturn last_flow;

  /* Latency report params */
  guint32 max_reorder_count;
  guint32 last_reorder_frame_number;
  gint fps_n;
  gint fps_d;
};

typedef struct
{
  /* Holds ref */
  GstVideoCodecFrame *frame;
  GstH264Picture *picture;
  /* Without ref */
  GstH264Decoder *self;
} GstH264DecoderOutputFrame;

#define UPDATE_FLOW_RETURN(ret,new_ret) G_STMT_START { \
  if (*(ret) == GST_FLOW_OK) \
    *(ret) = new_ret; \
} G_STMT_END

#define parent_class gst_h264_decoder_parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstH264Decoder, gst_h264_decoder,
    GST_TYPE_VIDEO_DECODER,
    G_ADD_PRIVATE (GstH264Decoder);
    GST_DEBUG_CATEGORY_INIT (gst_h264_decoder_debug, "h264decoder", 0,
        "H.264 Video Decoder"));

static void gst_h264_decoder_finalize (GObject * object);

static gboolean gst_h264_decoder_start (GstVideoDecoder * decoder);
static gboolean gst_h264_decoder_stop (GstVideoDecoder * decoder);
static gboolean gst_h264_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_h264_decoder_negotiate (GstVideoDecoder * decoder);
static GstFlowReturn gst_h264_decoder_finish (GstVideoDecoder * decoder);
static gboolean gst_h264_decoder_flush (GstVideoDecoder * decoder);
static GstFlowReturn gst_h264_decoder_drain (GstVideoDecoder * decoder);
static GstFlowReturn gst_h264_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);

/* codec specific functions */
static GstFlowReturn gst_h264_decoder_process_sps (GstH264Decoder * self,
    GstH264SPS * sps);
static GstFlowReturn gst_h264_decoder_decode_slice (GstH264Decoder * self);
static GstFlowReturn gst_h264_decoder_decode_nal (GstH264Decoder * self,
    GstH264NalUnit * nalu);
static gboolean gst_h264_decoder_fill_picture_from_slice (GstH264Decoder * self,
    const GstH264Slice * slice, GstH264Picture * picture);
static gboolean gst_h264_decoder_calculate_poc (GstH264Decoder * self,
    GstH264Picture * picture);
static gboolean gst_h264_decoder_init_gap_picture (GstH264Decoder * self,
    GstH264Picture * picture, gint frame_num);
static GstFlowReturn gst_h264_decoder_drain_internal (GstH264Decoder * self);
static void gst_h264_decoder_finish_current_picture (GstH264Decoder * self,
    GstFlowReturn * ret);
static void gst_h264_decoder_finish_picture (GstH264Decoder * self,
    GstH264Picture * picture, GstFlowReturn * ret);
static void gst_h264_decoder_prepare_ref_pic_lists (GstH264Decoder * self,
    GstH264Picture * current_picture);
static void gst_h264_decoder_clear_ref_pic_lists (GstH264Decoder * self);
static gboolean gst_h264_decoder_modify_ref_pic_lists (GstH264Decoder * self);
static gboolean
gst_h264_decoder_sliding_window_picture_marking (GstH264Decoder * self,
    GstH264Picture * picture);
static void gst_h264_decoder_do_output_picture (GstH264Decoder * self,
    GstH264Picture * picture, GstFlowReturn * ret);
static GstH264Picture *gst_h264_decoder_new_field_picture (GstH264Decoder *
    self, GstH264Picture * picture);
static void
gst_h264_decoder_clear_output_frame (GstH264DecoderOutputFrame * output_frame);

enum
{
  PROP_0,
  PROP_COMPLIANCE,
};

/**
 * gst_h264_decoder_compliance_get_type:
 *
 * Get the compliance type of the h264 decoder.
 *
 * Since: 1.20
 */
GType
gst_h264_decoder_compliance_get_type (void)
{
  static gsize h264_decoder_compliance_type = 0;
  static const GEnumValue compliances[] = {
    {GST_H264_DECODER_COMPLIANCE_AUTO, "GST_H264_DECODER_COMPLIANCE_AUTO",
        "auto"},
    {GST_H264_DECODER_COMPLIANCE_STRICT, "GST_H264_DECODER_COMPLIANCE_STRICT",
        "strict"},
    {GST_H264_DECODER_COMPLIANCE_NORMAL, "GST_H264_DECODER_COMPLIANCE_NORMAL",
        "normal"},
    {GST_H264_DECODER_COMPLIANCE_FLEXIBLE,
        "GST_H264_DECODER_COMPLIANCE_FLEXIBLE", "flexible"},
    {0, NULL, NULL},
  };


  if (g_once_init_enter (&h264_decoder_compliance_type)) {
    GType _type;

    _type = g_enum_register_static ("GstH264DecoderCompliance", compliances);
    g_once_init_leave (&h264_decoder_compliance_type, _type);
  }

  return (GType) h264_decoder_compliance_type;
}

static void
gst_h264_decoder_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstH264Decoder *self = GST_H264_DECODER (object);
  GstH264DecoderPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_COMPLIANCE:
      GST_OBJECT_LOCK (self);
      g_value_set_enum (value, priv->compliance);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_h264_decoder_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstH264Decoder *self = GST_H264_DECODER (object);
  GstH264DecoderPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_COMPLIANCE:
      GST_OBJECT_LOCK (self);
      priv->compliance = g_value_get_enum (value);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_h264_decoder_class_init (GstH264DecoderClass * klass)
{
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = GST_DEBUG_FUNCPTR (gst_h264_decoder_finalize);
  object_class->get_property = gst_h264_decoder_get_property;
  object_class->set_property = gst_h264_decoder_set_property;

  decoder_class->start = GST_DEBUG_FUNCPTR (gst_h264_decoder_start);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_h264_decoder_stop);
  decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_h264_decoder_set_format);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_h264_decoder_negotiate);
  decoder_class->finish = GST_DEBUG_FUNCPTR (gst_h264_decoder_finish);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_h264_decoder_flush);
  decoder_class->drain = GST_DEBUG_FUNCPTR (gst_h264_decoder_drain);
  decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_h264_decoder_handle_frame);

  /**
   * GstH264Decoder:compliance:
   *
   * The compliance controls the behavior of the decoder to handle some
   * subtle cases and contexts, such as the low-latency DPB bumping or
   * mapping the baseline profile as the constrained-baseline profile,
   * etc.
   *
   * Since: 1.20
   */
  g_object_class_install_property (object_class, PROP_COMPLIANCE,
      g_param_spec_enum ("compliance", "Decoder Compliance",
          "The decoder's behavior in compliance with the h264 spec.",
          GST_TYPE_H264_DECODER_COMPLIANCE, GST_H264_DECODER_COMPLIANCE_AUTO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));
}

static void
gst_h264_decoder_init (GstH264Decoder * self)
{
  GstH264DecoderPrivate *priv;

  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (self), TRUE);
  gst_video_decoder_set_needs_format (GST_VIDEO_DECODER (self), TRUE);

  self->priv = priv = gst_h264_decoder_get_instance_private (self);

  priv->ref_pic_list_p0 = g_array_sized_new (FALSE, TRUE,
      sizeof (GstH264Picture *), 32);
  g_array_set_clear_func (priv->ref_pic_list_p0,
      (GDestroyNotify) gst_clear_h264_picture);

  priv->ref_pic_list_b0 = g_array_sized_new (FALSE, TRUE,
      sizeof (GstH264Picture *), 32);
  g_array_set_clear_func (priv->ref_pic_list_b0,
      (GDestroyNotify) gst_clear_h264_picture);

  priv->ref_pic_list_b1 = g_array_sized_new (FALSE, TRUE,
      sizeof (GstH264Picture *), 32);
  g_array_set_clear_func (priv->ref_pic_list_b1,
      (GDestroyNotify) gst_clear_h264_picture);

  priv->ref_frame_list_0_short_term = g_array_sized_new (FALSE, TRUE,
      sizeof (GstH264Picture *), 32);
  g_array_set_clear_func (priv->ref_frame_list_0_short_term,
      (GDestroyNotify) gst_clear_h264_picture);

  priv->ref_frame_list_1_short_term = g_array_sized_new (FALSE, TRUE,
      sizeof (GstH264Picture *), 32);
  g_array_set_clear_func (priv->ref_frame_list_1_short_term,
      (GDestroyNotify) gst_clear_h264_picture);

  priv->ref_frame_list_long_term = g_array_sized_new (FALSE, TRUE,
      sizeof (GstH264Picture *), 32);
  g_array_set_clear_func (priv->ref_frame_list_long_term,
      (GDestroyNotify) gst_clear_h264_picture);

  priv->ref_pic_list0 = g_array_sized_new (FALSE, TRUE,
      sizeof (GstH264Picture *), 32);
  priv->ref_pic_list1 = g_array_sized_new (FALSE, TRUE,
      sizeof (GstH264Picture *), 32);

  priv->split_nalu = g_array_new (FALSE, FALSE, sizeof (GstH264NalUnit));

  priv->output_queue =
      gst_queue_array_new_for_struct (sizeof (GstH264DecoderOutputFrame), 1);
  gst_queue_array_set_clear_func (priv->output_queue,
      (GDestroyNotify) gst_h264_decoder_clear_output_frame);
}

static void
gst_h264_decoder_finalize (GObject * object)
{
  GstH264Decoder *self = GST_H264_DECODER (object);
  GstH264DecoderPrivate *priv = self->priv;

  g_array_unref (priv->ref_pic_list_p0);
  g_array_unref (priv->ref_pic_list_b0);
  g_array_unref (priv->ref_pic_list_b1);
  g_array_unref (priv->ref_frame_list_0_short_term);
  g_array_unref (priv->ref_frame_list_1_short_term);
  g_array_unref (priv->ref_frame_list_long_term);
  g_array_unref (priv->ref_pic_list0);
  g_array_unref (priv->ref_pic_list1);
  g_array_unref (priv->split_nalu);
  gst_queue_array_free (priv->output_queue);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_h264_decoder_reset_latency_infos (GstH264Decoder * self)
{
  GstH264DecoderPrivate *priv = self->priv;

  priv->max_reorder_count = 0;
  priv->last_reorder_frame_number = 0;
  priv->fps_n = 25;
  priv->fps_d = 1;
}

static void
gst_h264_decoder_reset (GstH264Decoder * self)
{
  GstH264DecoderPrivate *priv = self->priv;

  g_clear_pointer (&self->input_state, gst_video_codec_state_unref);
  g_clear_pointer (&priv->parser, gst_h264_nal_parser_free);
  g_clear_pointer (&priv->dpb, gst_h264_dpb_free);
  gst_clear_h264_picture (&priv->last_field);

  priv->profile_idc = 0;
  priv->width = 0;
  priv->height = 0;
  priv->nal_length_size = 4;
  priv->last_flow = GST_FLOW_OK;

  gst_h264_decoder_reset_latency_infos (self);
}

static gboolean
gst_h264_decoder_start (GstVideoDecoder * decoder)
{
  GstH264Decoder *self = GST_H264_DECODER (decoder);
  GstH264DecoderPrivate *priv = self->priv;

  gst_h264_decoder_reset (self);

  priv->parser = gst_h264_nal_parser_new ();
  priv->dpb = gst_h264_dpb_new ();

  return TRUE;
}

static gboolean
gst_h264_decoder_stop (GstVideoDecoder * decoder)
{
  GstH264Decoder *self = GST_H264_DECODER (decoder);

  gst_h264_decoder_reset (self);

  return TRUE;
}

static void
gst_h264_decoder_clear_output_frame (GstH264DecoderOutputFrame * output_frame)
{
  if (!output_frame)
    return;

  if (output_frame->frame) {
    gst_video_decoder_release_frame (GST_VIDEO_DECODER (output_frame->self),
        output_frame->frame);
    output_frame->frame = NULL;
  }

  gst_clear_h264_picture (&output_frame->picture);
}

static void
gst_h264_decoder_clear_dpb (GstH264Decoder * self, gboolean flush)
{
  GstVideoDecoder *decoder = GST_VIDEO_DECODER (self);
  GstH264DecoderPrivate *priv = self->priv;
  GstH264Picture *picture;

  /* If we are not flushing now, videodecoder baseclass will hold
   * GstVideoCodecFrame. Release frames manually */
  if (!flush) {
    while ((picture = gst_h264_dpb_bump (priv->dpb, TRUE)) != NULL) {
      GstVideoCodecFrame *frame = gst_video_decoder_get_frame (decoder,
          GST_CODEC_PICTURE_FRAME_NUMBER (picture));

      if (frame)
        gst_video_decoder_release_frame (decoder, frame);
      gst_h264_picture_unref (picture);
    }
  }

  gst_queue_array_clear (priv->output_queue);
  gst_h264_decoder_clear_ref_pic_lists (self);
  gst_clear_h264_picture (&priv->last_field);
  gst_h264_dpb_clear (priv->dpb);
}

static gboolean
gst_h264_decoder_flush (GstVideoDecoder * decoder)
{
  GstH264Decoder *self = GST_H264_DECODER (decoder);

  gst_h264_decoder_clear_dpb (self, TRUE);

  return TRUE;
}

static GstFlowReturn
gst_h264_decoder_drain (GstVideoDecoder * decoder)
{
  GstH264Decoder *self = GST_H264_DECODER (decoder);

  /* dpb will be cleared by this method */
  return gst_h264_decoder_drain_internal (self);
}

static GstFlowReturn
gst_h264_decoder_finish (GstVideoDecoder * decoder)
{
  return gst_h264_decoder_drain (decoder);
}

static GstFlowReturn
gst_h264_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstH264Decoder *self = GST_H264_DECODER (decoder);
  GstH264DecoderPrivate *priv = self->priv;
  GstBuffer *in_buf = frame->input_buffer;
  GstH264NalUnit nalu;
  GstH264ParserResult pres = GST_H264_PARSER_OK;
  GstMapInfo map;
  GstFlowReturn decode_ret = GST_FLOW_OK;

  GST_LOG_OBJECT (self,
      "handle frame, PTS: %" GST_TIME_FORMAT ", DTS: %"
      GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_PTS (in_buf)),
      GST_TIME_ARGS (GST_BUFFER_DTS (in_buf)));

  priv->current_frame = frame;
  priv->last_flow = GST_FLOW_OK;

  gst_buffer_map (in_buf, &map, GST_MAP_READ);
  if (priv->in_format == GST_H264_DECODER_FORMAT_AVC) {
    guint offset = 0;
    gsize consumed = 0;
    guint i;

    do {
      pres = gst_h264_parser_identify_and_split_nalu_avc (priv->parser,
          map.data, offset, map.size, priv->nal_length_size, priv->split_nalu,
          &consumed);
      if (pres != GST_H264_PARSER_OK)
        break;

      for (i = 0; i < priv->split_nalu->len; i++) {
        GstH264NalUnit *nl =
            &g_array_index (priv->split_nalu, GstH264NalUnit, i);
        decode_ret = gst_h264_decoder_decode_nal (self, nl);
        if (decode_ret != GST_FLOW_OK)
          break;
      }

      offset += consumed;
    } while (pres == GST_H264_PARSER_OK && decode_ret == GST_FLOW_OK);
  } else {
    pres = gst_h264_parser_identify_nalu (priv->parser,
        map.data, 0, map.size, &nalu);

    if (pres == GST_H264_PARSER_NO_NAL_END)
      pres = GST_H264_PARSER_OK;

    while (pres == GST_H264_PARSER_OK && decode_ret == GST_FLOW_OK) {
      decode_ret = gst_h264_decoder_decode_nal (self, &nalu);

      pres = gst_h264_parser_identify_nalu (priv->parser,
          map.data, nalu.offset + nalu.size, map.size, &nalu);

      if (pres == GST_H264_PARSER_NO_NAL_END)
        pres = GST_H264_PARSER_OK;
    }
  }

  gst_buffer_unmap (in_buf, &map);

  if (decode_ret != GST_FLOW_OK) {
    if (decode_ret == GST_FLOW_ERROR) {
      GST_VIDEO_DECODER_ERROR (self, 1, STREAM, DECODE,
          ("Failed to decode data"), (NULL), decode_ret);
    }

    gst_video_decoder_release_frame (decoder, frame);
    gst_clear_h264_picture (&priv->current_picture);
    priv->current_frame = NULL;

    return decode_ret;
  }

  gst_h264_decoder_finish_current_picture (self, &decode_ret);
  gst_video_codec_frame_unref (frame);
  priv->current_frame = NULL;

  if (priv->last_flow != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (self,
        "Last flow %s", gst_flow_get_name (priv->last_flow));
    return priv->last_flow;
  }

  if (decode_ret == GST_FLOW_ERROR) {
    GST_VIDEO_DECODER_ERROR (self, 1, STREAM, DECODE,
        ("Failed to decode data"), (NULL), decode_ret);
  }

  return decode_ret;
}

static GstFlowReturn
gst_h264_decoder_parse_sps (GstH264Decoder * self, GstH264NalUnit * nalu)
{
  GstH264DecoderPrivate *priv = self->priv;
  GstH264SPS sps;
  GstH264ParserResult pres;
  GstFlowReturn ret;

  pres = gst_h264_parse_sps (nalu, &sps);
  if (pres != GST_H264_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to parse SPS, result %d", pres);
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (self, "SPS parsed");

  ret = gst_h264_decoder_process_sps (self, &sps);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "Failed to process SPS");
  } else if (gst_h264_parser_update_sps (priv->parser,
          &sps) != GST_H264_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to update SPS");
    ret = GST_FLOW_ERROR;
  }

  gst_h264_sps_clear (&sps);

  return ret;
}

static GstFlowReturn
gst_h264_decoder_parse_pps (GstH264Decoder * self, GstH264NalUnit * nalu)
{
  GstH264DecoderPrivate *priv = self->priv;
  GstH264PPS pps;
  GstH264ParserResult pres;
  GstFlowReturn ret = GST_FLOW_OK;

  pres = gst_h264_parse_pps (priv->parser, nalu, &pps);
  if (pres != GST_H264_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to parse PPS, result %d", pres);
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (self, "PPS parsed");

  if (pps.num_slice_groups_minus1 > 0) {
    GST_FIXME_OBJECT (self, "FMO is not supported");
    ret = GST_FLOW_ERROR;
  } else if (gst_h264_parser_update_pps (priv->parser, &pps)
      != GST_H264_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to update PPS");
    ret = GST_FLOW_ERROR;
  }

  gst_h264_pps_clear (&pps);

  return ret;
}

static GstFlowReturn
gst_h264_decoder_parse_codec_data (GstH264Decoder * self, const guint8 * data,
    gsize size)
{
  GstH264DecoderPrivate *priv = self->priv;
  GstH264DecoderConfigRecord *config = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  GstH264NalUnit *nalu;
  guint i;

  if (gst_h264_parser_parse_decoder_config_record (priv->parser, data, size,
          &config) != GST_H264_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to parse codec-data");
    return GST_FLOW_ERROR;
  }

  priv->nal_length_size = config->length_size_minus_one + 1;
  for (i = 0; i < config->sps->len; i++) {
    nalu = &g_array_index (config->sps, GstH264NalUnit, i);

    /* TODO: handle subset sps for SVC/MVC. That would need to be stored in
     * separate array instead of putting SPS/subset-SPS into a single array */
    if (nalu->type != GST_H264_NAL_SPS)
      continue;

    ret = gst_h264_decoder_parse_sps (self, nalu);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "Failed to parse SPS");
      goto out;
    }
  }

  for (i = 0; i < config->pps->len; i++) {
    nalu = &g_array_index (config->pps, GstH264NalUnit, i);
    if (nalu->type != GST_H264_NAL_PPS)
      continue;

    ret = gst_h264_decoder_parse_pps (self, nalu);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "Failed to parse PPS");
      goto out;
    }
  }

out:
  gst_h264_decoder_config_record_free (config);
  return ret;
}

static gboolean
gst_h264_decoder_preprocess_slice (GstH264Decoder * self, GstH264Slice * slice)
{
  GstH264DecoderPrivate *priv = self->priv;

  if (!priv->current_picture) {
    if (slice->header.first_mb_in_slice != 0) {
      GST_ERROR_OBJECT (self, "Invalid stream, first_mb_in_slice %d",
          slice->header.first_mb_in_slice);
      return FALSE;
    }
  }

  return TRUE;
}

static void
gst_h264_decoder_update_pic_nums (GstH264Decoder * self,
    GstH264Picture * current_picture, gint frame_num)
{
  GstH264DecoderPrivate *priv = self->priv;
  GArray *dpb = gst_h264_dpb_get_pictures_all (priv->dpb);
  gint i;

  for (i = 0; i < dpb->len; i++) {
    GstH264Picture *picture = g_array_index (dpb, GstH264Picture *, i);

    if (!GST_H264_PICTURE_IS_REF (picture))
      continue;

    if (GST_H264_PICTURE_IS_LONG_TERM_REF (picture)) {
      if (GST_H264_PICTURE_IS_FRAME (current_picture))
        picture->long_term_pic_num = picture->long_term_frame_idx;
      else if (current_picture->field == picture->field)
        picture->long_term_pic_num = 2 * picture->long_term_frame_idx + 1;
      else
        picture->long_term_pic_num = 2 * picture->long_term_frame_idx;
    } else {
      if (picture->frame_num > frame_num)
        picture->frame_num_wrap = picture->frame_num - priv->max_frame_num;
      else
        picture->frame_num_wrap = picture->frame_num;

      if (GST_H264_PICTURE_IS_FRAME (current_picture))
        picture->pic_num = picture->frame_num_wrap;
      else if (picture->field == current_picture->field)
        picture->pic_num = 2 * picture->frame_num_wrap + 1;
      else
        picture->pic_num = 2 * picture->frame_num_wrap;
    }
  }

  g_array_unref (dpb);
}

static GstH264Picture *
gst_h264_decoder_split_frame (GstH264Decoder * self, GstH264Picture * picture)
{
  GstH264Picture *other_field;

  g_assert (GST_H264_PICTURE_IS_FRAME (picture));

  other_field = gst_h264_decoder_new_field_picture (self, picture);
  if (!other_field) {
    GST_WARNING_OBJECT (self,
        "Couldn't split frame into complementary field pair");
    return NULL;
  }

  GST_LOG_OBJECT (self, "Split picture %p, poc %d, frame num %d",
      picture, picture->pic_order_cnt, picture->frame_num);

  /* FIXME: enhance TFF decision by using picture timing SEI */
  if (picture->top_field_order_cnt < picture->bottom_field_order_cnt) {
    picture->field = GST_H264_PICTURE_FIELD_TOP_FIELD;
    picture->pic_order_cnt = picture->top_field_order_cnt;

    other_field->field = GST_H264_PICTURE_FIELD_BOTTOM_FIELD;
    other_field->pic_order_cnt = picture->bottom_field_order_cnt;
  } else {
    picture->field = GST_H264_PICTURE_FIELD_BOTTOM_FIELD;
    picture->pic_order_cnt = picture->bottom_field_order_cnt;

    other_field->field = GST_H264_PICTURE_FIELD_TOP_FIELD;
    other_field->pic_order_cnt = picture->top_field_order_cnt;
  }

  other_field->top_field_order_cnt = picture->top_field_order_cnt;
  other_field->bottom_field_order_cnt = picture->bottom_field_order_cnt;
  other_field->frame_num = picture->frame_num;
  other_field->ref = picture->ref;
  other_field->nonexisting = picture->nonexisting;
  GST_CODEC_PICTURE_COPY_FRAME_NUMBER (other_field, picture);
  other_field->field_pic_flag = picture->field_pic_flag;

  return other_field;
}

static void
output_picture_directly (GstH264Decoder * self, GstH264Picture * picture,
    GstFlowReturn * ret)
{
  GstH264DecoderPrivate *priv = self->priv;
  GstH264Picture *out_pic = NULL;
  GstFlowReturn flow_ret = GST_FLOW_OK;

  g_assert (ret != NULL);

  if (GST_H264_PICTURE_IS_FRAME (picture)) {
    g_assert (priv->last_field == NULL);
    out_pic = g_steal_pointer (&picture);
    goto output;
  }

  if (priv->last_field == NULL) {
    if (picture->second_field) {
      GST_WARNING ("Set the last output %p poc:%d, without first field",
          picture, picture->pic_order_cnt);

      flow_ret = GST_FLOW_ERROR;
      goto output;
    }

    /* Just cache the first field. */
    priv->last_field = g_steal_pointer (&picture);
  } else {
    if (!picture->second_field || !picture->other_field
        || picture->other_field != priv->last_field) {
      GST_WARNING ("The last field %p poc:%d is not the pair of the "
          "current field %p poc:%d",
          priv->last_field, priv->last_field->pic_order_cnt,
          picture, picture->pic_order_cnt);

      gst_clear_h264_picture (&priv->last_field);
      flow_ret = GST_FLOW_ERROR;
      goto output;
    }

    GST_TRACE ("Pair the last field %p poc:%d and the current"
        " field %p poc:%d",
        priv->last_field, priv->last_field->pic_order_cnt,
        picture, picture->pic_order_cnt);

    out_pic = priv->last_field;
    priv->last_field = NULL;
    /* Link each field. */
    out_pic->other_field = picture;
  }

output:
  if (out_pic) {
    gst_h264_dpb_set_last_output (priv->dpb, out_pic);
    gst_h264_decoder_do_output_picture (self, out_pic, &flow_ret);
  }

  gst_clear_h264_picture (&picture);

  UPDATE_FLOW_RETURN (ret, flow_ret);
}

static void
add_picture_to_dpb (GstH264Decoder * self, GstH264Picture * picture)
{
  GstH264DecoderPrivate *priv = self->priv;

  if (!gst_h264_dpb_get_interlaced (priv->dpb)) {
    g_assert (priv->last_field == NULL);
    gst_h264_dpb_add (priv->dpb, picture);
    return;
  }

  /* The first field of the last picture may not be able to enter the
     DPB if it is a non ref, but if the second field enters the DPB, we
     need to add both of them. */
  if (priv->last_field && picture->other_field == priv->last_field) {
    gst_h264_dpb_add (priv->dpb, priv->last_field);
    priv->last_field = NULL;
  }

  gst_h264_dpb_add (priv->dpb, picture);
}

static void
_bump_dpb (GstH264Decoder * self, GstH264DpbBumpMode bump_level,
    GstH264Picture * current_picture, GstFlowReturn * ret)
{
  GstH264DecoderPrivate *priv = self->priv;

  g_assert (ret != NULL);

  while (gst_h264_dpb_needs_bump (priv->dpb, current_picture, bump_level)) {
    GstH264Picture *to_output;

    to_output = gst_h264_dpb_bump (priv->dpb, FALSE);

    if (!to_output) {
      GST_WARNING_OBJECT (self, "Bumping is needed but no picture to output");
      break;
    }

    gst_h264_decoder_do_output_picture (self, to_output, ret);
  }
}

static GstFlowReturn
gst_h264_decoder_handle_frame_num_gap (GstH264Decoder * self, gint frame_num)
{
  GstH264DecoderPrivate *priv = self->priv;
  const GstH264SPS *sps = priv->active_sps;
  gint unused_short_term_frame_num;

  if (!sps) {
    GST_ERROR_OBJECT (self, "No active sps");
    return GST_FLOW_ERROR;
  }

  if (priv->prev_ref_frame_num == frame_num) {
    GST_TRACE_OBJECT (self,
        "frame_num == PrevRefFrameNum (%d), not a gap", frame_num);
    return GST_FLOW_OK;
  }

  if (((priv->prev_ref_frame_num + 1) % priv->max_frame_num) == frame_num) {
    GST_TRACE_OBJECT (self,
        "frame_num ==  (PrevRefFrameNum + 1) %% MaxFrameNum (%d), not a gap",
        frame_num);
    return GST_FLOW_OK;
  }

  if (gst_h264_dpb_get_size (priv->dpb) == 0) {
    GST_TRACE_OBJECT (self, "DPB is empty, not a gap");
    return GST_FLOW_OK;
  }

  if (!sps->gaps_in_frame_num_value_allowed_flag) {
    /* This is likely the case where some frames were dropped.
     * then we need to keep decoding without error out */
    GST_WARNING_OBJECT (self, "Invalid frame num %d, maybe frame drop",
        frame_num);

    return GST_FLOW_OK;
  }

  GST_DEBUG_OBJECT (self, "Handling frame num gap %d -> %d (MaxFrameNum: %d)",
      priv->prev_ref_frame_num, frame_num, priv->max_frame_num);

  /* 7.4.3/7-23 */
  unused_short_term_frame_num =
      (priv->prev_ref_frame_num + 1) % priv->max_frame_num;
  while (unused_short_term_frame_num != frame_num) {
    GstH264Picture *picture = gst_h264_picture_new ();
    GstFlowReturn ret = GST_FLOW_OK;

    if (!gst_h264_decoder_init_gap_picture (self, picture,
            unused_short_term_frame_num))
      return GST_FLOW_ERROR;

    gst_h264_decoder_update_pic_nums (self, picture,
        unused_short_term_frame_num);

    /* C.2.1 */
    if (!gst_h264_decoder_sliding_window_picture_marking (self, picture)) {
      GST_ERROR_OBJECT (self,
          "Couldn't perform sliding window picture marking");
      return GST_FLOW_ERROR;
    }

    gst_h264_dpb_delete_unused (priv->dpb);

    _bump_dpb (self, GST_H264_DPB_BUMP_NORMAL_LATENCY, picture, &ret);
    if (ret != GST_FLOW_OK)
      return ret;

    /* the picture is short term ref, add to DPB. */
    if (gst_h264_dpb_get_interlaced (priv->dpb)) {
      GstH264Picture *other_field =
          gst_h264_decoder_split_frame (self, picture);

      add_picture_to_dpb (self, picture);
      add_picture_to_dpb (self, other_field);
    } else {
      add_picture_to_dpb (self, picture);
    }

    unused_short_term_frame_num++;
    unused_short_term_frame_num %= priv->max_frame_num;
  }

  return GST_FLOW_OK;
}

static gboolean
gst_h264_decoder_init_current_picture (GstH264Decoder * self)
{
  GstH264DecoderPrivate *priv = self->priv;

  if (!gst_h264_decoder_fill_picture_from_slice (self, &priv->current_slice,
          priv->current_picture)) {
    return FALSE;
  }

  if (!gst_h264_decoder_calculate_poc (self, priv->current_picture))
    return FALSE;

  /* If the slice header indicates we will have to perform reference marking
   * process after this picture is decoded, store required data for that
   * purpose */
  if (priv->current_slice.header.
      dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag) {
    priv->current_picture->dec_ref_pic_marking =
        priv->current_slice.header.dec_ref_pic_marking;
  }

  return TRUE;
}

static GstFlowReturn
gst_h264_decoder_start_current_picture (GstH264Decoder * self)
{
  GstH264DecoderClass *klass;
  GstH264DecoderPrivate *priv = self->priv;
  const GstH264SPS *sps;
  gint frame_num;
  GstFlowReturn ret = GST_FLOW_OK;
  GstH264Picture *current_picture;

  g_assert (priv->current_picture != NULL);
  g_assert (priv->active_sps != NULL);
  g_assert (priv->active_pps != NULL);

  current_picture = priv->current_picture;

  /* If subclass didn't update output state at this point,
   * marking this picture as a discont and stores current input state */
  if (priv->input_state_changed) {
    gst_h264_picture_set_discont_state (current_picture, self->input_state);
    priv->input_state_changed = FALSE;
  }

  sps = priv->active_sps;

  priv->max_frame_num = sps->max_frame_num;
  frame_num = priv->current_slice.header.frame_num;
  if (priv->current_slice.nalu.idr_pic_flag)
    priv->prev_ref_frame_num = 0;

  ret = gst_h264_decoder_handle_frame_num_gap (self, frame_num);
  if (ret != GST_FLOW_OK)
    return ret;

  if (!gst_h264_decoder_init_current_picture (self))
    return GST_FLOW_ERROR;

  /* If the new picture is an IDR, flush DPB */
  if (current_picture->idr) {
    if (!current_picture->dec_ref_pic_marking.no_output_of_prior_pics_flag) {
      ret = gst_h264_decoder_drain_internal (self);
      if (ret != GST_FLOW_OK)
        return ret;
    } else {
      /* C.4.4 Removal of pictures from the DPB before possible insertion
       * of the current picture
       *
       * If decoded picture is IDR and no_output_of_prior_pics_flag is equal to 1
       * or is inferred to be equal to 1, all frame buffers in the DPB
       * are emptied without output of the pictures they contain,
       * and DPB fullness is set to 0.
       */
      gst_h264_decoder_clear_dpb (self, FALSE);
    }
  }

  gst_h264_decoder_update_pic_nums (self, current_picture, frame_num);

  if (priv->process_ref_pic_lists)
    gst_h264_decoder_prepare_ref_pic_lists (self, current_picture);

  klass = GST_H264_DECODER_GET_CLASS (self);
  if (klass->start_picture) {
    ret = klass->start_picture (self, priv->current_picture,
        &priv->current_slice, priv->dpb);

    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "subclass does not want to start picture");
      return ret;
    }
  }

  return GST_FLOW_OK;
}

static GstH264Picture *
gst_h264_decoder_new_field_picture (GstH264Decoder * self,
    GstH264Picture * picture)
{
  GstH264DecoderClass *klass = GST_H264_DECODER_GET_CLASS (self);
  GstH264Picture *new_picture;

  if (!klass->new_field_picture) {
    GST_WARNING_OBJECT (self, "Subclass does not support interlaced stream");
    return NULL;
  }

  new_picture = gst_h264_picture_new ();
  /* don't confuse subclass by non-existing picture */
  if (!picture->nonexisting) {
    GstFlowReturn ret;

    ret = klass->new_field_picture (self, picture, new_picture);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "Subclass couldn't handle new field picture");
      gst_h264_picture_unref (new_picture);

      return NULL;
    }
  }

  new_picture->other_field = picture;
  new_picture->second_field = TRUE;

  return new_picture;
}

static gboolean
gst_h264_decoder_find_first_field_picture (GstH264Decoder * self,
    GstH264Slice * slice, GstH264Picture ** first_field)
{
  GstH264DecoderPrivate *priv = self->priv;
  const GstH264SliceHdr *slice_hdr = &slice->header;
  GstH264Picture *prev_field;
  gboolean in_dpb;

  *first_field = NULL;
  prev_field = NULL;
  in_dpb = FALSE;
  if (gst_h264_dpb_get_interlaced (priv->dpb)) {
    if (priv->last_field) {
      prev_field = priv->last_field;
      in_dpb = FALSE;
    } else if (gst_h264_dpb_get_size (priv->dpb) > 0) {
      GstH264Picture *prev_picture;
      GArray *pictures;

      pictures = gst_h264_dpb_get_pictures_all (priv->dpb);
      prev_picture =
          g_array_index (pictures, GstH264Picture *, pictures->len - 1);
      g_array_unref (pictures); /* prev_picture should be held */

      /* Previous picture was a field picture. */
      if (!GST_H264_PICTURE_IS_FRAME (prev_picture)
          && !prev_picture->other_field) {
        prev_field = prev_picture;
        in_dpb = TRUE;
      }
    }
  } else {
    g_assert (priv->last_field == NULL);
  }

  /* This is not a field picture */
  if (!slice_hdr->field_pic_flag) {
    if (!prev_field)
      return TRUE;

    GST_WARNING_OBJECT (self, "Previous picture %p (poc %d) is not complete",
        prev_field, prev_field->pic_order_cnt);
    goto error;
  }

  /* OK, this is the first field. */
  if (!prev_field)
    return TRUE;

  if (prev_field->frame_num != slice_hdr->frame_num) {
    GST_WARNING_OBJECT (self, "Previous picture %p (poc %d) is not complete",
        prev_field, prev_field->pic_order_cnt);
    goto error;
  } else {
    GstH264PictureField current_field = slice_hdr->bottom_field_flag ?
        GST_H264_PICTURE_FIELD_BOTTOM_FIELD : GST_H264_PICTURE_FIELD_TOP_FIELD;

    if (current_field == prev_field->field) {
      GST_WARNING_OBJECT (self,
          "Currnet picture and previous picture have identical field %d",
          current_field);
      goto error;
    }
  }

  *first_field = gst_h264_picture_ref (prev_field);
  return TRUE;

error:
  if (!in_dpb) {
    gst_clear_h264_picture (&priv->last_field);
  } else {
    /* FIXME: implement fill gap field picture if it is already in DPB */
  }

  return FALSE;
}

static GstFlowReturn
gst_h264_decoder_parse_slice (GstH264Decoder * self, GstH264NalUnit * nalu)
{
  GstH264DecoderPrivate *priv = self->priv;
  GstH264ParserResult pres = GST_H264_PARSER_OK;
  GstFlowReturn ret = GST_FLOW_OK;

  memset (&priv->current_slice, 0, sizeof (GstH264Slice));

  pres = gst_h264_parser_parse_slice_hdr (priv->parser, nalu,
      &priv->current_slice.header, TRUE, TRUE);

  if (pres != GST_H264_PARSER_OK) {
    GST_ERROR_OBJECT (self, "Failed to parse slice header, ret %d", pres);
    memset (&priv->current_slice, 0, sizeof (GstH264Slice));

    return GST_FLOW_ERROR;
  }

  priv->current_slice.nalu = *nalu;

  if (!gst_h264_decoder_preprocess_slice (self, &priv->current_slice))
    return GST_FLOW_ERROR;

  priv->active_pps = priv->current_slice.header.pps;
  priv->active_sps = priv->active_pps->sequence;

  /* Check whether field picture boundary within given codec frame.
   * This might happen in case that upstream sent buffer per frame unit,
   * not picture unit (i.e., AU unit).
   * If AU boundary is detected, then finish first field picture we decoded
   * in this chain, we should finish the current picture and
   * start new field picture decoding */
  if (gst_h264_dpb_get_interlaced (priv->dpb) && priv->current_picture &&
      !GST_H264_PICTURE_IS_FRAME (priv->current_picture) &&
      !priv->current_picture->second_field) {
    GstH264PictureField prev_field = priv->current_picture->field;
    GstH264PictureField cur_field = GST_H264_PICTURE_FIELD_FRAME;
    if (priv->current_slice.header.field_pic_flag)
      cur_field = priv->current_slice.header.bottom_field_flag ?
          GST_H264_PICTURE_FIELD_BOTTOM_FIELD :
          GST_H264_PICTURE_FIELD_TOP_FIELD;

    if (cur_field != prev_field) {
      GST_LOG_OBJECT (self,
          "Found new field picture, finishing the first field picture");
      gst_h264_decoder_finish_current_picture (self, &ret);
    }
  }

  if (!priv->current_picture) {
    GstH264DecoderClass *klass = GST_H264_DECODER_GET_CLASS (self);
    GstH264Picture *picture = NULL;
    GstH264Picture *first_field = NULL;
    GstFlowReturn ret = GST_FLOW_OK;

    g_assert (priv->current_frame);

    if (!gst_h264_decoder_find_first_field_picture (self,
            &priv->current_slice, &first_field)) {
      GST_ERROR_OBJECT (self, "Couldn't find or determine first picture");
      return GST_FLOW_ERROR;
    }

    if (first_field) {
      picture = gst_h264_decoder_new_field_picture (self, first_field);
      gst_h264_picture_unref (first_field);

      if (!picture) {
        GST_ERROR_OBJECT (self, "Couldn't duplicate the first field picture");
        return GST_FLOW_ERROR;
      }
    } else {
      picture = gst_h264_picture_new ();

      if (klass->new_picture)
        ret = klass->new_picture (self, priv->current_frame, picture);

      if (ret != GST_FLOW_OK) {
        GST_WARNING_OBJECT (self, "subclass does not want accept new picture");
        priv->current_picture = NULL;
        gst_h264_picture_unref (picture);
        return ret;
      }

      priv->last_reorder_frame_number++;
      picture->reorder_frame_number = priv->last_reorder_frame_number;
    }

    /* This allows accessing the frame from the picture. */
    GST_CODEC_PICTURE_FRAME_NUMBER (picture) =
        priv->current_frame->system_frame_number;
    priv->current_picture = picture;

    ret = gst_h264_decoder_start_current_picture (self);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "start picture failed");
      return ret;
    }
  }

  return gst_h264_decoder_decode_slice (self);
}

static GstFlowReturn
gst_h264_decoder_decode_nal (GstH264Decoder * self, GstH264NalUnit * nalu)
{
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (self, "Parsed nal type: %d, offset %d, size %d",
      nalu->type, nalu->offset, nalu->size);

  switch (nalu->type) {
    case GST_H264_NAL_SPS:
      ret = gst_h264_decoder_parse_sps (self, nalu);
      break;
    case GST_H264_NAL_PPS:
      ret = gst_h264_decoder_parse_pps (self, nalu);
      break;
    case GST_H264_NAL_SLICE:
    case GST_H264_NAL_SLICE_DPA:
    case GST_H264_NAL_SLICE_DPB:
    case GST_H264_NAL_SLICE_DPC:
    case GST_H264_NAL_SLICE_IDR:
    case GST_H264_NAL_SLICE_EXT:
      ret = gst_h264_decoder_parse_slice (self, nalu);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_h264_decoder_format_from_caps (GstH264Decoder * self, GstCaps * caps,
    GstH264DecoderFormat * format, GstH264DecoderAlign * align)
{
  if (format)
    *format = GST_H264_DECODER_FORMAT_NONE;

  if (align)
    *align = GST_H264_DECODER_ALIGN_NONE;

  if (!gst_caps_is_fixed (caps)) {
    GST_WARNING_OBJECT (self, "Caps wasn't fixed");
    return;
  }

  GST_DEBUG_OBJECT (self, "parsing caps: %" GST_PTR_FORMAT, caps);

  if (caps && gst_caps_get_size (caps) > 0) {
    GstStructure *s = gst_caps_get_structure (caps, 0);
    const gchar *str = NULL;

    if (format) {
      if ((str = gst_structure_get_string (s, "stream-format"))) {
        if (strcmp (str, "avc") == 0 || strcmp (str, "avc3") == 0)
          *format = GST_H264_DECODER_FORMAT_AVC;
        else if (strcmp (str, "byte-stream") == 0)
          *format = GST_H264_DECODER_FORMAT_BYTE;
      }
    }

    if (align) {
      if ((str = gst_structure_get_string (s, "alignment"))) {
        if (strcmp (str, "au") == 0)
          *align = GST_H264_DECODER_ALIGN_AU;
        else if (strcmp (str, "nal") == 0)
          *align = GST_H264_DECODER_ALIGN_NAL;
      }
    }
  }
}

static gboolean
gst_h264_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstH264Decoder *self = GST_H264_DECODER (decoder);
  GstH264DecoderPrivate *priv = self->priv;
  GstQuery *query;

  GST_DEBUG_OBJECT (decoder, "Set format");

  priv->input_state_changed = TRUE;

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);

  self->input_state = gst_video_codec_state_ref (state);

  /* in case live streaming, we will run on low-latency mode */
  priv->is_live = FALSE;
  query = gst_query_new_latency ();
  if (gst_pad_peer_query (GST_VIDEO_DECODER_SINK_PAD (self), query))
    gst_query_parse_latency (query, &priv->is_live, NULL, NULL);
  gst_query_unref (query);

  if (priv->is_live)
    GST_DEBUG_OBJECT (self, "Live source, will run on low-latency mode");

  if (state->caps) {
    GstH264DecoderFormat format;
    GstH264DecoderAlign align;

    gst_h264_decoder_format_from_caps (self, state->caps, &format, &align);

    if (format == GST_H264_DECODER_FORMAT_NONE) {
      /* codec_data implies avc */
      if (state->codec_data) {
        GST_WARNING_OBJECT (self,
            "video/x-h264 caps with codec_data but no stream-format=avc");
        format = GST_H264_DECODER_FORMAT_AVC;
      } else {
        /* otherwise assume bytestream input */
        GST_WARNING_OBJECT (self,
            "video/x-h264 caps without codec_data or stream-format");
        format = GST_H264_DECODER_FORMAT_BYTE;
      }
    }

    if (format == GST_H264_DECODER_FORMAT_AVC) {
      /* AVC requires codec_data, AVC3 might have one and/or SPS/PPS inline */
      if (!state->codec_data) {
        /* Try it with size 4 anyway */
        priv->nal_length_size = 4;
        GST_WARNING_OBJECT (self,
            "avc format without codec data, assuming nal length size is 4");
      }

      /* AVC implies alignment=au */
      if (align == GST_H264_DECODER_ALIGN_NONE)
        align = GST_H264_DECODER_ALIGN_AU;
    }

    if (format == GST_H264_DECODER_FORMAT_BYTE && state->codec_data)
      GST_WARNING_OBJECT (self, "bytestream with codec data");

    priv->in_format = format;
    priv->align = align;
  }

  if (state->codec_data) {
    GstMapInfo map;

    gst_buffer_map (state->codec_data, &map, GST_MAP_READ);
    if (gst_h264_decoder_parse_codec_data (self, map.data, map.size) !=
        GST_FLOW_OK) {
      /* keep going without error.
       * Probably inband SPS/PPS might be valid data */
      GST_WARNING_OBJECT (self, "Failed to handle codec data");
    }
    gst_buffer_unmap (state->codec_data, &map);
  }

  return TRUE;
}

static gboolean
gst_h264_decoder_negotiate (GstVideoDecoder * decoder)
{
  GstH264Decoder *self = GST_H264_DECODER (decoder);

  /* output state must be updated by subclass using new input state already */
  self->priv->input_state_changed = FALSE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static gboolean
gst_h264_decoder_fill_picture_from_slice (GstH264Decoder * self,
    const GstH264Slice * slice, GstH264Picture * picture)
{
  GstH264DecoderClass *klass = GST_H264_DECODER_GET_CLASS (self);
  const GstH264SliceHdr *slice_hdr = &slice->header;
  const GstH264PPS *pps;
  const GstH264SPS *sps;

  pps = slice_hdr->pps;
  if (!pps) {
    GST_ERROR_OBJECT (self, "No pps in slice header");
    return FALSE;
  }

  sps = pps->sequence;
  if (!sps) {
    GST_ERROR_OBJECT (self, "No sps in pps");
    return FALSE;
  }

  picture->idr = slice->nalu.idr_pic_flag;
  picture->dec_ref_pic_marking = slice_hdr->dec_ref_pic_marking;
  picture->field_pic_flag = slice_hdr->field_pic_flag;

  if (picture->idr)
    picture->idr_pic_id = slice_hdr->idr_pic_id;

  if (slice_hdr->field_pic_flag)
    picture->field =
        slice_hdr->bottom_field_flag ?
        GST_H264_PICTURE_FIELD_BOTTOM_FIELD : GST_H264_PICTURE_FIELD_TOP_FIELD;
  else
    picture->field = GST_H264_PICTURE_FIELD_FRAME;

  if (!GST_H264_PICTURE_IS_FRAME (picture) && !klass->new_field_picture) {
    GST_FIXME_OBJECT (self, "Subclass doesn't support interlace stream");
    return FALSE;
  }

  picture->nal_ref_idc = slice->nalu.ref_idc;
  if (slice->nalu.ref_idc != 0)
    gst_h264_picture_set_reference (picture,
        GST_H264_PICTURE_REF_SHORT_TERM, FALSE);

  picture->frame_num = slice_hdr->frame_num;

  /* 7.4.3 */
  if (!slice_hdr->field_pic_flag)
    picture->pic_num = slice_hdr->frame_num;
  else
    picture->pic_num = 2 * slice_hdr->frame_num + 1;

  picture->pic_order_cnt_type = sps->pic_order_cnt_type;
  switch (picture->pic_order_cnt_type) {
    case 0:
      picture->pic_order_cnt_lsb = slice_hdr->pic_order_cnt_lsb;
      picture->delta_pic_order_cnt_bottom =
          slice_hdr->delta_pic_order_cnt_bottom;
      break;
    case 1:
      picture->delta_pic_order_cnt0 = slice_hdr->delta_pic_order_cnt[0];
      picture->delta_pic_order_cnt1 = slice_hdr->delta_pic_order_cnt[1];
      break;
    case 2:
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  return TRUE;
}

static gboolean
gst_h264_decoder_calculate_poc (GstH264Decoder * self, GstH264Picture * picture)
{
  GstH264DecoderPrivate *priv = self->priv;
  const GstH264SPS *sps = priv->active_sps;

  if (!sps) {
    GST_ERROR_OBJECT (self, "No active SPS");
    return FALSE;
  }

  switch (picture->pic_order_cnt_type) {
    case 0:{
      /* See spec 8.2.1.1 */
      gint prev_pic_order_cnt_msb, prev_pic_order_cnt_lsb;
      gint max_pic_order_cnt_lsb;

      if (picture->idr) {
        prev_pic_order_cnt_msb = prev_pic_order_cnt_lsb = 0;
      } else {
        if (priv->prev_ref_has_memmgmnt5) {
          if (priv->prev_ref_field != GST_H264_PICTURE_FIELD_BOTTOM_FIELD) {
            prev_pic_order_cnt_msb = 0;
            prev_pic_order_cnt_lsb = priv->prev_ref_top_field_order_cnt;
          } else {
            prev_pic_order_cnt_msb = 0;
            prev_pic_order_cnt_lsb = 0;
          }
        } else {
          prev_pic_order_cnt_msb = priv->prev_ref_pic_order_cnt_msb;
          prev_pic_order_cnt_lsb = priv->prev_ref_pic_order_cnt_lsb;
        }
      }

      max_pic_order_cnt_lsb = 1 << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);

      if ((picture->pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
          (prev_pic_order_cnt_lsb - picture->pic_order_cnt_lsb >=
              max_pic_order_cnt_lsb / 2)) {
        picture->pic_order_cnt_msb =
            prev_pic_order_cnt_msb + max_pic_order_cnt_lsb;
      } else if ((picture->pic_order_cnt_lsb > prev_pic_order_cnt_lsb)
          && (picture->pic_order_cnt_lsb - prev_pic_order_cnt_lsb >
              max_pic_order_cnt_lsb / 2)) {
        picture->pic_order_cnt_msb =
            prev_pic_order_cnt_msb - max_pic_order_cnt_lsb;
      } else {
        picture->pic_order_cnt_msb = prev_pic_order_cnt_msb;
      }

      if (picture->field != GST_H264_PICTURE_FIELD_BOTTOM_FIELD) {
        picture->top_field_order_cnt =
            picture->pic_order_cnt_msb + picture->pic_order_cnt_lsb;
      }

      switch (picture->field) {
        case GST_H264_PICTURE_FIELD_FRAME:
          picture->top_field_order_cnt = picture->pic_order_cnt_msb +
              picture->pic_order_cnt_lsb;
          picture->bottom_field_order_cnt = picture->top_field_order_cnt +
              picture->delta_pic_order_cnt_bottom;
          break;
        case GST_H264_PICTURE_FIELD_TOP_FIELD:
          picture->top_field_order_cnt = picture->pic_order_cnt_msb +
              picture->pic_order_cnt_lsb;
          break;
        case GST_H264_PICTURE_FIELD_BOTTOM_FIELD:
          picture->bottom_field_order_cnt = picture->pic_order_cnt_msb +
              picture->pic_order_cnt_lsb;
          break;
      }
      break;
    }

    case 1:{
      gint abs_frame_num = 0;
      gint expected_pic_order_cnt = 0;
      gint i;

      /* See spec 8.2.1.2 */
      if (priv->prev_has_memmgmnt5)
        priv->prev_frame_num_offset = 0;

      if (picture->idr)
        picture->frame_num_offset = 0;
      else if (priv->prev_frame_num > picture->frame_num)
        picture->frame_num_offset =
            priv->prev_frame_num_offset + priv->max_frame_num;
      else
        picture->frame_num_offset = priv->prev_frame_num_offset;

      if (sps->num_ref_frames_in_pic_order_cnt_cycle != 0)
        abs_frame_num = picture->frame_num_offset + picture->frame_num;
      else
        abs_frame_num = 0;

      if (picture->nal_ref_idc == 0 && abs_frame_num > 0)
        --abs_frame_num;

      if (abs_frame_num > 0) {
        gint pic_order_cnt_cycle_cnt, frame_num_in_pic_order_cnt_cycle;
        gint expected_delta_per_pic_order_cnt_cycle = 0;

        if (sps->num_ref_frames_in_pic_order_cnt_cycle == 0) {
          GST_WARNING_OBJECT (self,
              "Invalid num_ref_frames_in_pic_order_cnt_cycle in stream");
          return FALSE;
        }

        pic_order_cnt_cycle_cnt =
            (abs_frame_num - 1) / sps->num_ref_frames_in_pic_order_cnt_cycle;
        frame_num_in_pic_order_cnt_cycle =
            (abs_frame_num - 1) % sps->num_ref_frames_in_pic_order_cnt_cycle;

        for (i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++) {
          expected_delta_per_pic_order_cnt_cycle +=
              sps->offset_for_ref_frame[i];
        }

        expected_pic_order_cnt = pic_order_cnt_cycle_cnt *
            expected_delta_per_pic_order_cnt_cycle;
        /* frame_num_in_pic_order_cnt_cycle is verified < 255 in parser */
        for (i = 0; i <= frame_num_in_pic_order_cnt_cycle; ++i)
          expected_pic_order_cnt += sps->offset_for_ref_frame[i];
      }

      if (!picture->nal_ref_idc)
        expected_pic_order_cnt += sps->offset_for_non_ref_pic;

      if (GST_H264_PICTURE_IS_FRAME (picture)) {
        picture->top_field_order_cnt =
            expected_pic_order_cnt + picture->delta_pic_order_cnt0;
        picture->bottom_field_order_cnt = picture->top_field_order_cnt +
            sps->offset_for_top_to_bottom_field + picture->delta_pic_order_cnt1;
      } else if (picture->field != GST_H264_PICTURE_FIELD_BOTTOM_FIELD) {
        picture->top_field_order_cnt =
            expected_pic_order_cnt + picture->delta_pic_order_cnt0;
      } else {
        picture->bottom_field_order_cnt = expected_pic_order_cnt +
            sps->offset_for_top_to_bottom_field + picture->delta_pic_order_cnt0;
      }
      break;
    }

    case 2:{
      gint temp_pic_order_cnt;

      /* See spec 8.2.1.3 */
      if (priv->prev_has_memmgmnt5)
        priv->prev_frame_num_offset = 0;

      if (picture->idr)
        picture->frame_num_offset = 0;
      else if (priv->prev_frame_num > picture->frame_num)
        picture->frame_num_offset =
            priv->prev_frame_num_offset + priv->max_frame_num;
      else
        picture->frame_num_offset = priv->prev_frame_num_offset;

      if (picture->idr) {
        temp_pic_order_cnt = 0;
      } else if (!picture->nal_ref_idc) {
        temp_pic_order_cnt =
            2 * (picture->frame_num_offset + picture->frame_num) - 1;
      } else {
        temp_pic_order_cnt =
            2 * (picture->frame_num_offset + picture->frame_num);
      }

      if (GST_H264_PICTURE_IS_FRAME (picture)) {
        picture->top_field_order_cnt = temp_pic_order_cnt;
        picture->bottom_field_order_cnt = temp_pic_order_cnt;
      } else if (picture->field == GST_H264_PICTURE_FIELD_BOTTOM_FIELD) {
        picture->bottom_field_order_cnt = temp_pic_order_cnt;
      } else {
        picture->top_field_order_cnt = temp_pic_order_cnt;
      }
      break;
    }

    default:
      GST_WARNING_OBJECT (self,
          "Invalid pic_order_cnt_type: %d", sps->pic_order_cnt_type);
      return FALSE;
  }

  switch (picture->field) {
    case GST_H264_PICTURE_FIELD_FRAME:
      picture->pic_order_cnt =
          MIN (picture->top_field_order_cnt, picture->bottom_field_order_cnt);
      break;
    case GST_H264_PICTURE_FIELD_TOP_FIELD:
      picture->pic_order_cnt = picture->top_field_order_cnt;
      break;
    case GST_H264_PICTURE_FIELD_BOTTOM_FIELD:
      picture->pic_order_cnt = picture->bottom_field_order_cnt;
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  return TRUE;
}

static void
gst_h264_decoder_drain_output_queue (GstH264Decoder * self, guint num,
    GstFlowReturn * ret)
{
  GstH264DecoderPrivate *priv = self->priv;
  GstH264DecoderClass *klass = GST_H264_DECODER_GET_CLASS (self);

  g_assert (klass->output_picture);
  g_assert (ret != NULL);

  while (gst_queue_array_get_length (priv->output_queue) > num) {
    GstH264DecoderOutputFrame *output_frame = (GstH264DecoderOutputFrame *)
        gst_queue_array_pop_head_struct (priv->output_queue);
    GstFlowReturn flow_ret = klass->output_picture (self, output_frame->frame,
        output_frame->picture);

    UPDATE_FLOW_RETURN (ret, flow_ret);
  }
}

static void
gst_h264_decoder_do_output_picture (GstH264Decoder * self,
    GstH264Picture * picture, GstFlowReturn * ret)
{
  GstH264DecoderPrivate *priv = self->priv;
  GstVideoCodecFrame *frame = NULL;
  GstH264DecoderOutputFrame output_frame;
#ifndef GST_DISABLE_GST_DEBUG
  guint32 last_output_poc;
#endif

  g_assert (ret != NULL);

  GST_LOG_OBJECT (self, "Outputting picture %p (frame_num %d, poc %d)",
      picture, picture->frame_num, picture->pic_order_cnt);

#ifndef GST_DISABLE_GST_DEBUG
  last_output_poc = gst_h264_dpb_get_last_output_poc (priv->dpb);
  if (picture->pic_order_cnt < last_output_poc) {
    GST_WARNING_OBJECT (self,
        "Outputting out of order %d -> %d, likely a broken stream",
        last_output_poc, picture->pic_order_cnt);
  }
#endif

  if (priv->last_reorder_frame_number > picture->reorder_frame_number) {
    guint64 diff = priv->last_reorder_frame_number -
        picture->reorder_frame_number;
    guint64 total_delay = diff + priv->preferred_output_delay;
    if (diff > priv->max_reorder_count && total_delay < G_MAXUINT32) {
      GstClockTime latency;

      priv->max_reorder_count = (guint32) diff;
      latency = gst_util_uint64_scale_int (GST_SECOND * total_delay,
          priv->fps_d, priv->fps_n);

      if (latency != G_MAXUINT64) {
        GST_DEBUG_OBJECT (self, "Updating latency to %" GST_TIME_FORMAT
            ", reorder count: %" G_GUINT64_FORMAT ", output-delay: %u",
            GST_TIME_ARGS (latency), diff, priv->preferred_output_delay);

        gst_video_decoder_set_latency (GST_VIDEO_DECODER (self),
            latency, latency);
      }
    }
  }

  frame = gst_video_decoder_get_frame (GST_VIDEO_DECODER (self),
      GST_CODEC_PICTURE_FRAME_NUMBER (picture));

  if (!frame) {
    /* The case where the end_picture() got failed and corresponding
     * GstVideoCodecFrame was dropped already */
    if (picture->nonexisting) {
      GST_DEBUG_OBJECT (self, "Dropping non-existing picture %p", picture);
    } else {
      GST_ERROR_OBJECT (self,
          "No available codec frame with frame number %d",
          GST_CODEC_PICTURE_FRAME_NUMBER (picture));
      UPDATE_FLOW_RETURN (ret, GST_FLOW_ERROR);
    }

    gst_h264_picture_unref (picture);

    return;
  }

  output_frame.frame = frame;
  output_frame.picture = picture;
  output_frame.self = self;
  gst_queue_array_push_tail_struct (priv->output_queue, &output_frame);

  gst_h264_decoder_drain_output_queue (self, priv->preferred_output_delay,
      &priv->last_flow);
}

static void
gst_h264_decoder_finish_current_picture (GstH264Decoder * self,
    GstFlowReturn * ret)
{
  GstH264DecoderPrivate *priv = self->priv;
  GstH264DecoderClass *klass;
  GstFlowReturn flow_ret = GST_FLOW_OK;

  if (!priv->current_picture) {
    if (priv->current_frame) {
      GST_DEBUG_OBJECT (self,
          "AU buffer without slice data, releasing current frame %u",
          priv->current_frame->system_frame_number);
      gst_video_decoder_release_frame (GST_VIDEO_DECODER_CAST (self),
          gst_video_codec_frame_ref (priv->current_frame));
    }
    return;
  }

  klass = GST_H264_DECODER_GET_CLASS (self);

  if (klass->end_picture) {
    flow_ret = klass->end_picture (self, priv->current_picture);
    if (flow_ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self,
          "end picture failed, marking picture %p non-existing "
          "(frame_num %d, poc %d)", priv->current_picture,
          priv->current_picture->frame_num,
          priv->current_picture->pic_order_cnt);
      priv->current_picture->nonexisting = TRUE;

      /* this fake nonexisting picture will not trigger ouput_picture() */
      gst_video_decoder_release_frame (GST_VIDEO_DECODER (self),
          gst_video_codec_frame_ref (priv->current_frame));
    }
  }

  /* We no longer need the per frame reference lists */
  gst_h264_decoder_clear_ref_pic_lists (self);

  /* finish picture takes ownership of the picture */
  gst_h264_decoder_finish_picture (self, priv->current_picture, &flow_ret);
  priv->current_picture = NULL;

  UPDATE_FLOW_RETURN (ret, flow_ret);
}

static gint
poc_asc_compare (const GstH264Picture ** a, const GstH264Picture ** b)
{
  return (*a)->pic_order_cnt - (*b)->pic_order_cnt;
}

static gint
poc_desc_compare (const GstH264Picture ** a, const GstH264Picture ** b)
{
  return (*b)->pic_order_cnt - (*a)->pic_order_cnt;
}

static GstFlowReturn
gst_h264_decoder_drain_internal (GstH264Decoder * self)
{
  GstH264DecoderPrivate *priv = self->priv;
  GstH264Picture *picture;
  GstFlowReturn ret = GST_FLOW_OK;

  while ((picture = gst_h264_dpb_bump (priv->dpb, TRUE)) != NULL) {
    gst_h264_decoder_do_output_picture (self, picture, &ret);
  }

  gst_h264_decoder_drain_output_queue (self, 0, &ret);

  gst_clear_h264_picture (&priv->last_field);
  gst_h264_dpb_clear (priv->dpb);

  return ret;
}

static gboolean
gst_h264_decoder_handle_memory_management_opt (GstH264Decoder * self,
    GstH264Picture * picture)
{
  GstH264DecoderPrivate *priv = self->priv;
  gint i;

  for (i = 0; i < G_N_ELEMENTS (picture->dec_ref_pic_marking.ref_pic_marking);
      i++) {
    GstH264RefPicMarking *ref_pic_marking =
        &picture->dec_ref_pic_marking.ref_pic_marking[i];
    guint8 type = ref_pic_marking->memory_management_control_operation;

    GST_TRACE_OBJECT (self, "memory management operation %d, type %d", i, type);

    /* Normal end of operations' specification */
    if (type == 0)
      return TRUE;

    switch (type) {
      case 4:
        priv->max_long_term_frame_idx =
            ref_pic_marking->max_long_term_frame_idx_plus1 - 1;
        break;
      case 5:
        priv->max_long_term_frame_idx = -1;
        break;
      default:
        break;
    }

    if (!gst_h264_dpb_perform_memory_management_control_operation (priv->dpb,
            ref_pic_marking, picture)) {
      GST_WARNING_OBJECT (self, "memory management operation type %d failed",
          type);
      /* Most likely our implementation fault, but let's just perform
       * next MMCO if any */
    }
  }

  return TRUE;
}

static gboolean
gst_h264_decoder_sliding_window_picture_marking (GstH264Decoder * self,
    GstH264Picture * picture)
{
  GstH264DecoderPrivate *priv = self->priv;
  const GstH264SPS *sps = priv->active_sps;
  gint num_ref_pics;
  gint max_num_ref_frames;

  /* Skip this for the second field */
  if (picture->second_field)
    return TRUE;

  if (!sps) {
    GST_ERROR_OBJECT (self, "No active sps");
    return FALSE;
  }

  /* 8.2.5.3. Ensure the DPB doesn't overflow by discarding the oldest picture */
  num_ref_pics = gst_h264_dpb_num_ref_frames (priv->dpb);
  max_num_ref_frames = MAX (1, sps->num_ref_frames);

  if (num_ref_pics < max_num_ref_frames)
    return TRUE;

  /* In theory, num_ref_pics shouldn't be larger than max_num_ref_frames
   * but it could happen if our implementation is wrong somehow or so.
   * Just try to remove reference pictures as many as possible in order to
   * avoid DPB overflow.
   */
  while (num_ref_pics >= max_num_ref_frames) {
    /* Max number of reference pics reached, need to remove one of the short
     * term ones. Find smallest frame_num_wrap short reference picture and mark
     * it as unused */
    GstH264Picture *to_unmark =
        gst_h264_dpb_get_lowest_frame_num_short_ref (priv->dpb);

    if (num_ref_pics > max_num_ref_frames) {
      GST_WARNING_OBJECT (self,
          "num_ref_pics %d is larger than allowed maximum %d",
          num_ref_pics, max_num_ref_frames);
    }

    if (!to_unmark) {
      GST_WARNING_OBJECT (self, "Could not find a short ref picture to unmark");
      return FALSE;
    }

    GST_TRACE_OBJECT (self,
        "Unmark reference flag of picture %p (frame_num %d, poc %d)",
        to_unmark, to_unmark->frame_num, to_unmark->pic_order_cnt);

    gst_h264_picture_set_reference (to_unmark, GST_H264_PICTURE_REF_NONE, TRUE);
    gst_h264_picture_unref (to_unmark);

    num_ref_pics--;
  }

  return TRUE;
}

/* This method ensures that DPB does not overflow, either by removing
 * reference pictures as specified in the stream, or using a sliding window
 * procedure to remove the oldest one.
 * It also performs marking and unmarking pictures as reference.
 * See spac 8.2.5.1 */
static gboolean
gst_h264_decoder_reference_picture_marking (GstH264Decoder * self,
    GstH264Picture * picture)
{
  GstH264DecoderPrivate *priv = self->priv;

  /* If the current picture is an IDR, all reference pictures are unmarked */
  if (picture->idr) {
    gst_h264_dpb_mark_all_non_ref (priv->dpb);

    if (picture->dec_ref_pic_marking.long_term_reference_flag) {
      gst_h264_picture_set_reference (picture,
          GST_H264_PICTURE_REF_LONG_TERM, FALSE);
      picture->long_term_frame_idx = 0;
      priv->max_long_term_frame_idx = 0;
    } else {
      gst_h264_picture_set_reference (picture,
          GST_H264_PICTURE_REF_SHORT_TERM, FALSE);
      priv->max_long_term_frame_idx = -1;
    }

    return TRUE;
  }

  /* Not an IDR. If the stream contains instructions on how to discard pictures
   * from DPB and how to mark/unmark existing reference pictures, do so.
   * Otherwise, fall back to default sliding window process */
  if (picture->dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag) {
    if (picture->nonexisting) {
      GST_WARNING_OBJECT (self,
          "Invalid memory management operation for non-existing picture "
          "%p (frame_num %d, poc %d", picture, picture->frame_num,
          picture->pic_order_cnt);
    }

    return gst_h264_decoder_handle_memory_management_opt (self, picture);
  }

  return gst_h264_decoder_sliding_window_picture_marking (self, picture);
}

static GstH264DpbBumpMode
get_bump_level (GstH264Decoder * self)
{
  GstH264DecoderPrivate *priv = self->priv;

  /* User set the mode explicitly. */
  switch (priv->compliance) {
    case GST_H264_DECODER_COMPLIANCE_STRICT:
      return GST_H264_DPB_BUMP_NORMAL_LATENCY;
    case GST_H264_DECODER_COMPLIANCE_NORMAL:
      return GST_H264_DPB_BUMP_LOW_LATENCY;
    case GST_H264_DECODER_COMPLIANCE_FLEXIBLE:
      return GST_H264_DPB_BUMP_VERY_LOW_LATENCY;
    default:
      break;
  }

  /* GST_H264_DECODER_COMPLIANCE_AUTO case. */

  if (priv->is_live) {
    /* The baseline and constrained-baseline profiles do not have B frames
       and do not use the picture reorder, safe to use the higher bump level. */
    if (priv->profile_idc == GST_H264_PROFILE_BASELINE)
      return GST_H264_DPB_BUMP_VERY_LOW_LATENCY;

    return GST_H264_DPB_BUMP_LOW_LATENCY;
  }

  return GST_H264_DPB_BUMP_NORMAL_LATENCY;
}

static void
gst_h264_decoder_finish_picture (GstH264Decoder * self,
    GstH264Picture * picture, GstFlowReturn * ret)
{
  GstVideoDecoder *decoder = GST_VIDEO_DECODER (self);
  GstH264DecoderPrivate *priv = self->priv;
  GstH264DpbBumpMode bump_level = get_bump_level (self);

  /* Finish processing the picture.
   * Start by storing previous picture data for later use */
  if (picture->ref) {
    gst_h264_decoder_reference_picture_marking (self, picture);
    priv->prev_ref_has_memmgmnt5 = picture->mem_mgmt_5;
    priv->prev_ref_top_field_order_cnt = picture->top_field_order_cnt;
    priv->prev_ref_pic_order_cnt_msb = picture->pic_order_cnt_msb;
    priv->prev_ref_pic_order_cnt_lsb = picture->pic_order_cnt_lsb;
    priv->prev_ref_field = picture->field;
    priv->prev_ref_frame_num = picture->frame_num;
  }

  priv->prev_frame_num = picture->frame_num;
  priv->prev_has_memmgmnt5 = picture->mem_mgmt_5;
  priv->prev_frame_num_offset = picture->frame_num_offset;

  /* Remove unused (for reference or later output) pictures from DPB, marking
   * them as such */
  gst_h264_dpb_delete_unused (priv->dpb);

  /* If field pictures belong to different codec frame,
   * drop codec frame of the second field because we are consuming
   * only the first codec frame via GstH264Decoder::output_picture() method */
  if (picture->second_field && picture->other_field &&
      GST_CODEC_PICTURE_FRAME_NUMBER (picture) !=
      GST_CODEC_PICTURE_FRAME_NUMBER (picture->other_field)) {
    GstVideoCodecFrame *second_field = gst_video_decoder_get_frame (decoder,
        GST_CODEC_PICTURE_FRAME_NUMBER (picture));
    GstVideoCodecFrame *first_field = gst_video_decoder_get_frame (decoder,
        GST_CODEC_PICTURE_FRAME_NUMBER (picture->other_field));

    /* Update frame duration. since we output a frame for two input field
     * pictures, output buffer duration should be first-field-duration +
     * second-field-duration */
    if (first_field) {
      if (GST_CLOCK_TIME_IS_VALID (first_field->pts) &&
          GST_CLOCK_TIME_IS_VALID (second_field->pts)) {
        GstClockTime first_field_end_time = first_field->pts;
        GstClockTime frame_end_time = second_field->pts;
        GstClockTime frame_duration;

        if (GST_CLOCK_TIME_IS_VALID (first_field->duration))
          first_field_end_time += first_field->duration;

        if (GST_CLOCK_TIME_IS_VALID (second_field->duration))
          frame_end_time += second_field->duration;

        frame_end_time = MAX (first_field_end_time, frame_end_time);
        if (frame_end_time >= first_field->pts) {
          frame_duration = frame_end_time - first_field->pts;

          GST_LOG_OBJECT (self, "Updating frame duration %"
              GST_TIME_FORMAT " -> %" GST_TIME_FORMAT,
              GST_TIME_ARGS (first_field->duration),
              GST_TIME_ARGS (frame_duration));

          first_field->duration = frame_duration;
        }
      }
      gst_video_codec_frame_unref (first_field);
    } else {
      GST_ERROR_OBJECT (self, "Couldn't get first field codec frame %u",
          GST_CODEC_PICTURE_FRAME_NUMBER (picture->other_field));
    }

    gst_video_decoder_release_frame (decoder, second_field);
  }

  /* C.4.4 */
  if (picture->mem_mgmt_5) {
    GstFlowReturn drain_ret;

    GST_TRACE_OBJECT (self, "Memory management type 5, drain the DPB");

    drain_ret = gst_h264_decoder_drain_internal (self);
    UPDATE_FLOW_RETURN (ret, drain_ret);
  }

  _bump_dpb (self, bump_level, picture, ret);

  /* Add a ref to avoid the case of directly outputed and destroyed. */
  gst_h264_picture_ref (picture);

  /* C.4.5.1, C.4.5.2
     - If the current decoded picture is the second field of a complementary
     reference field pair, add to DPB.
     C.4.5.1
     For A reference decoded picture, the "bumping" process is invoked
     repeatedly until there is an empty frame buffer, then add to DPB:
     C.4.5.2
     For a non-reference decoded picture, if there is empty frame buffer
     after bumping the smaller POC, add to DPB.
     Otherwise, output directly. */
  if ((picture->second_field && picture->other_field
          && picture->other_field->ref)
      || picture->ref || gst_h264_dpb_has_empty_frame_buffer (priv->dpb)) {
    /* Split frame into top/bottom field pictures for reference picture marking
     * process. Even if current picture has field_pic_flag equal to zero,
     * if next picture is a field picture, complementary field pair of reference
     * frame should have individual pic_num and long_term_pic_num.
     */
    if (gst_h264_dpb_get_interlaced (priv->dpb) &&
        GST_H264_PICTURE_IS_FRAME (picture)) {
      GstH264Picture *other_field =
          gst_h264_decoder_split_frame (self, picture);

      add_picture_to_dpb (self, picture);
      if (!other_field) {
        GST_WARNING_OBJECT (self,
            "Couldn't split frame into complementary field pair");
        /* Keep decoding anyway... */
      } else {
        add_picture_to_dpb (self, other_field);
      }
    } else {
      add_picture_to_dpb (self, picture);
    }
  } else {
    output_picture_directly (self, picture, ret);
  }

  GST_LOG_OBJECT (self,
      "Finishing picture %p (frame_num %d, poc %d), entries in DPB %d",
      picture, picture->frame_num, picture->pic_order_cnt,
      gst_h264_dpb_get_size (priv->dpb));

  gst_h264_picture_unref (picture);

  /* For low-latency output, we try to bump here to avoid waiting
   * for another decoding circle. */
  if (bump_level != GST_H264_DPB_BUMP_NORMAL_LATENCY)
    _bump_dpb (self, bump_level, NULL, ret);
}

static gint
gst_h264_decoder_get_max_num_reorder_frames (GstH264Decoder * self,
    GstH264SPS * sps, gint max_dpb_size)
{
  GstH264DecoderPrivate *priv = self->priv;

  if (sps->vui_parameters_present_flag
      && sps->vui_parameters.bitstream_restriction_flag) {
    if (sps->vui_parameters.num_reorder_frames > max_dpb_size) {
      GST_WARNING
          ("max_num_reorder_frames present, but larger than MaxDpbFrames (%d > %d)",
          sps->vui_parameters.num_reorder_frames, max_dpb_size);
      return max_dpb_size;
    }

    return sps->vui_parameters.num_reorder_frames;
  } else if (sps->constraint_set3_flag) {
    /* If max_num_reorder_frames is not present, if profile id is equal to
     * 44, 86, 100, 110, 122, or 244 and constraint_set3_flag is equal to 1,
     * max_num_reorder_frames shall be inferred to be equal to 0 */
    switch (sps->profile_idc) {
      case 44:
      case 86:
      case 100:
      case 110:
      case 122:
      case 244:
        return 0;
      default:
        break;
    }
  }

  /* Relaxed conditions (undefined by spec) */
  if (priv->compliance != GST_H264_DECODER_COMPLIANCE_STRICT &&
      (sps->profile_idc == 66 || sps->profile_idc == 83)) {
    /* baseline, constrained baseline and scalable-baseline profiles
     * only contain I/P frames. */
    return 0;
  }

  return max_dpb_size;
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

static gint
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
gst_h264_decoder_set_latency (GstH264Decoder * self, const GstH264SPS * sps,
    gint max_dpb_size)
{
  GstH264DecoderPrivate *priv = self->priv;
  GstCaps *caps;
  GstClockTime min, max;
  GstStructure *structure;
  gint fps_d = 1, fps_n = 0;
  GstH264DpbBumpMode bump_level;
  guint32 frames_delay, max_frames_delay;

  caps = gst_pad_get_current_caps (GST_VIDEO_DECODER_SRC_PAD (self));
  if (!caps && self->input_state)
    caps = gst_caps_ref (self->input_state->caps);

  if (caps) {
    structure = gst_caps_get_structure (caps, 0);
    if (gst_structure_get_fraction (structure, "framerate", &fps_n, &fps_d)) {
      if (fps_n == 0) {
        /* variable framerate: see if we have a max-framerate */
        gst_structure_get_fraction (structure, "max-framerate", &fps_n, &fps_d);
      }
    }
    gst_caps_unref (caps);
  }

  /* if no fps or variable, then 25/1 */
  if (fps_n == 0) {
    fps_n = 25;
    fps_d = 1;
  }

  frames_delay = max_dpb_size;

  bump_level = get_bump_level (self);
  if (bump_level != GST_H264_DPB_BUMP_NORMAL_LATENCY) {
    GST_DEBUG_OBJECT (self, "Actual latency will be updated later");
    frames_delay = 0;
  }

  priv->max_reorder_count = frames_delay;
  priv->fps_n = fps_n;
  priv->fps_d = fps_d;

  /* Consider output delay wanted by subclass */
  frames_delay += priv->preferred_output_delay;

  max_frames_delay = max_dpb_size + priv->preferred_output_delay;

  min = gst_util_uint64_scale_int (frames_delay * GST_SECOND, fps_d, fps_n);
  max = gst_util_uint64_scale_int (max_frames_delay * GST_SECOND, fps_d, fps_n);

  GST_DEBUG_OBJECT (self,
      "latency min %" GST_TIME_FORMAT ", max %" GST_TIME_FORMAT
      ", frames-delay %d", GST_TIME_ARGS (min), GST_TIME_ARGS (max),
      frames_delay);

  gst_video_decoder_set_latency (GST_VIDEO_DECODER (self), min, max);
}

static GstFlowReturn
gst_h264_decoder_process_sps (GstH264Decoder * self, GstH264SPS * sps)
{
  GstH264DecoderClass *klass = GST_H264_DECODER_GET_CLASS (self);
  GstH264DecoderPrivate *priv = self->priv;
  guint8 level;
  gint max_dpb_mbs;
  gint width_mb, height_mb;
  gint max_dpb_frames;
  gint max_dpb_size;
  gint prev_max_dpb_size;
  gint max_reorder_frames;
  gint prev_max_reorder_frames;
  gboolean prev_interlaced;
  gboolean interlaced;
  GstFlowReturn ret = GST_FLOW_OK;

  if (sps->frame_mbs_only_flag == 0) {
    if (!klass->new_field_picture) {
      GST_FIXME_OBJECT (self,
          "frame_mbs_only_flag != 1 not supported by subclass");
      return GST_FLOW_NOT_NEGOTIATED;
    }

    if (sps->mb_adaptive_frame_field_flag) {
      GST_LOG_OBJECT (self,
          "mb_adaptive_frame_field_flag == 1, MBAFF sequence");
    } else {
      GST_LOG_OBJECT (self, "mb_adaptive_frame_field_flag == 0, PAFF sequence");
    }
  }

  interlaced = !sps->frame_mbs_only_flag;

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
  if (!max_dpb_mbs)
    return GST_FLOW_ERROR;

  width_mb = sps->width / 16;
  height_mb = sps->height / 16;

  max_dpb_frames = MIN (max_dpb_mbs / (width_mb * height_mb),
      GST_H264_DPB_MAX_SIZE);

  if (sps->vui_parameters_present_flag
      && sps->vui_parameters.bitstream_restriction_flag)
    max_dpb_frames = MAX (1, sps->vui_parameters.max_dec_frame_buffering);

  /* Case 1) There might be some non-conforming streams that require more DPB
   * size than that of specified one by SPS
   * Case 2) If bitstream_restriction_flag is not present,
   * max_dec_frame_buffering should be inferred
   * to be equal to MaxDpbFrames, then MaxDpbFrames can exceed num_ref_frames
   * See https://chromium-review.googlesource.com/c/chromium/src/+/760276/
   */
  max_dpb_size = MAX (max_dpb_frames, sps->num_ref_frames);
  if (max_dpb_size > GST_H264_DPB_MAX_SIZE) {
    GST_WARNING_OBJECT (self, "Too large calculated DPB size %d", max_dpb_size);
    max_dpb_size = GST_H264_DPB_MAX_SIZE;
  }

  /* Safety, so that subclass don't need bound checking */
  g_return_val_if_fail (max_dpb_size <= GST_H264_DPB_MAX_SIZE, GST_FLOW_ERROR);

  prev_max_dpb_size = gst_h264_dpb_get_max_num_frames (priv->dpb);
  prev_interlaced = gst_h264_dpb_get_interlaced (priv->dpb);

  prev_max_reorder_frames = gst_h264_dpb_get_max_num_reorder_frames (priv->dpb);
  max_reorder_frames =
      gst_h264_decoder_get_max_num_reorder_frames (self, sps, max_dpb_size);

  if (priv->width != sps->width || priv->height != sps->height ||
      prev_max_dpb_size != max_dpb_size || prev_interlaced != interlaced ||
      prev_max_reorder_frames != max_reorder_frames) {
    GstH264DecoderClass *klass = GST_H264_DECODER_GET_CLASS (self);

    GST_DEBUG_OBJECT (self,
        "SPS updated, resolution: %dx%d -> %dx%d, dpb size: %d -> %d, "
        "interlaced %d -> %d, max_reorder_frames: %d -> %d",
        priv->width, priv->height, sps->width, sps->height,
        prev_max_dpb_size, max_dpb_size, prev_interlaced, interlaced,
        prev_max_reorder_frames, max_reorder_frames);

    ret = gst_h264_decoder_drain (GST_VIDEO_DECODER (self));
    if (ret != GST_FLOW_OK)
      return ret;

    gst_h264_decoder_reset_latency_infos (self);

    g_assert (klass->new_sequence);

    if (klass->get_preferred_output_delay) {
      priv->preferred_output_delay =
          klass->get_preferred_output_delay (self, priv->is_live);
    } else {
      priv->preferred_output_delay = 0;
    }

    ret = klass->new_sequence (self,
        sps, max_dpb_size + priv->preferred_output_delay);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "subclass does not want accept new sequence");
      return ret;
    }

    priv->profile_idc = sps->profile_idc;
    priv->width = sps->width;
    priv->height = sps->height;

    gst_h264_dpb_set_max_num_frames (priv->dpb, max_dpb_size);
    gst_h264_dpb_set_interlaced (priv->dpb, interlaced);
    gst_h264_dpb_set_max_num_reorder_frames (priv->dpb, max_reorder_frames);
    gst_h264_decoder_set_latency (self, sps, max_dpb_size);
  }

  return GST_FLOW_OK;
}

static gboolean
gst_h264_decoder_init_gap_picture (GstH264Decoder * self,
    GstH264Picture * picture, gint frame_num)
{
  picture->nonexisting = TRUE;
  picture->nal_ref_idc = 1;
  picture->frame_num = picture->pic_num = frame_num;
  picture->dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag = FALSE;
  picture->ref = GST_H264_PICTURE_REF_SHORT_TERM;
  picture->ref_pic = TRUE;
  picture->dec_ref_pic_marking.long_term_reference_flag = FALSE;
  picture->field = GST_H264_PICTURE_FIELD_FRAME;

  return gst_h264_decoder_calculate_poc (self, picture);
}

static GstFlowReturn
gst_h264_decoder_decode_slice (GstH264Decoder * self)
{
  GstH264DecoderClass *klass = GST_H264_DECODER_GET_CLASS (self);
  GstH264DecoderPrivate *priv = self->priv;
  GstH264Slice *slice = &priv->current_slice;
  GstH264Picture *picture = priv->current_picture;
  GArray *ref_pic_list0 = NULL;
  GArray *ref_pic_list1 = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!picture) {
    GST_ERROR_OBJECT (self, "No current picture");
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (self, "Decode picture %p (frame_num %d, poc %d)",
      picture, picture->frame_num, picture->pic_order_cnt);

  priv->max_pic_num = slice->header.max_pic_num;

  if (priv->process_ref_pic_lists) {
    if (!gst_h264_decoder_modify_ref_pic_lists (self)) {
      ret = GST_FLOW_ERROR;
      goto beach;
    }

    ref_pic_list0 = priv->ref_pic_list0;
    ref_pic_list1 = priv->ref_pic_list1;
  }

  g_assert (klass->decode_slice);

  ret = klass->decode_slice (self, picture, slice, ref_pic_list0,
      ref_pic_list1);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self,
        "Subclass didn't want to decode picture %p (frame_num %d, poc %d)",
        picture, picture->frame_num, picture->pic_order_cnt);
  }

beach:
  g_array_set_size (priv->ref_pic_list0, 0);
  g_array_set_size (priv->ref_pic_list1, 0);

  return ret;
}

static gint
pic_num_desc_compare (const GstH264Picture ** a, const GstH264Picture ** b)
{
  return (*b)->pic_num - (*a)->pic_num;
}

static gint
long_term_pic_num_asc_compare (const GstH264Picture ** a,
    const GstH264Picture ** b)
{
  return (*a)->long_term_pic_num - (*b)->long_term_pic_num;
}

static void
construct_ref_pic_lists_p (GstH264Decoder * self,
    GstH264Picture * current_picture)
{
  GstH264DecoderPrivate *priv = self->priv;
  gint pos;

  /* RefPicList0 (8.2.4.2.1) [[1] [2]], where:
   * [1] shortterm ref pics sorted by descending pic_num,
   * [2] longterm ref pics by ascending long_term_pic_num.
   */
  g_array_set_size (priv->ref_pic_list_p0, 0);

  gst_h264_dpb_get_pictures_short_term_ref (priv->dpb,
      TRUE, FALSE, priv->ref_pic_list_p0);
  g_array_sort (priv->ref_pic_list_p0, (GCompareFunc) pic_num_desc_compare);

  pos = priv->ref_pic_list_p0->len;
  gst_h264_dpb_get_pictures_long_term_ref (priv->dpb,
      FALSE, priv->ref_pic_list_p0);
  g_qsort_with_data (&g_array_index (priv->ref_pic_list_p0, gpointer, pos),
      priv->ref_pic_list_p0->len - pos, sizeof (gpointer),
      (GCompareDataFunc) long_term_pic_num_asc_compare, NULL);

#ifndef GST_DISABLE_GST_DEBUG
  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
    GString *str = g_string_new (NULL);
    for (pos = 0; pos < priv->ref_pic_list_p0->len; pos++) {
      GstH264Picture *ref =
          g_array_index (priv->ref_pic_list_p0, GstH264Picture *, pos);
      if (!GST_H264_PICTURE_IS_LONG_TERM_REF (ref))
        g_string_append_printf (str, "|%i", ref->pic_num);
      else
        g_string_append_printf (str, "|%is", ref->pic_num);
    }
    GST_DEBUG_OBJECT (self, "ref_pic_list_p0: %s|", str->str);
    g_string_free (str, TRUE);
  }
#endif
}

static gint
frame_num_wrap_desc_compare (const GstH264Picture ** a,
    const GstH264Picture ** b)
{
  return (*b)->frame_num_wrap - (*a)->frame_num_wrap;
}

static gint
long_term_frame_idx_asc_compare (const GstH264Picture ** a,
    const GstH264Picture ** b)
{
  return (*a)->long_term_frame_idx - (*b)->long_term_frame_idx;
}

/* init_picture_refs_fields_1 in gstvaapidecoder_h264.c */
static void
init_picture_refs_fields_1 (GstH264Decoder * self, GstH264PictureField field,
    GArray * ref_frame_list, GArray * ref_pic_list_x)
{
  guint i = 0, j = 0;

  do {
    for (; i < ref_frame_list->len; i++) {
      GstH264Picture *pic = g_array_index (ref_frame_list, GstH264Picture *, i);
      if (pic->field == field) {
        pic = gst_h264_picture_ref (pic);
        g_array_append_val (ref_pic_list_x, pic);
        i++;
        break;
      }
    }

    for (; j < ref_frame_list->len; j++) {
      GstH264Picture *pic = g_array_index (ref_frame_list, GstH264Picture *, j);
      if (pic->field != field) {
        pic = gst_h264_picture_ref (pic);
        g_array_append_val (ref_pic_list_x, pic);
        j++;
        break;
      }
    }
  } while (i < ref_frame_list->len || j < ref_frame_list->len);
}

static void
construct_ref_field_pic_lists_p (GstH264Decoder * self,
    GstH264Picture * current_picture)
{
  GstH264DecoderPrivate *priv = self->priv;
  gint pos;

  g_array_set_size (priv->ref_pic_list_p0, 0);
  g_array_set_size (priv->ref_frame_list_0_short_term, 0);
  g_array_set_size (priv->ref_frame_list_long_term, 0);

  /* 8.2.4.2.2, 8.2.4.2.5 refFrameList0ShortTerm:
   * short-term ref pictures sorted by descending frame_num_wrap.
   */
  gst_h264_dpb_get_pictures_short_term_ref (priv->dpb,
      TRUE, TRUE, priv->ref_frame_list_0_short_term);
  g_array_sort (priv->ref_frame_list_0_short_term,
      (GCompareFunc) frame_num_wrap_desc_compare);

#ifndef GST_DISABLE_GST_DEBUG
  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_TRACE
      && priv->ref_frame_list_0_short_term->len) {
    GString *str = g_string_new (NULL);
    for (pos = 0; pos < priv->ref_frame_list_0_short_term->len; pos++) {
      GstH264Picture *ref = g_array_index (priv->ref_frame_list_0_short_term,
          GstH264Picture *, pos);
      g_string_append_printf (str, "|%i(%d)", ref->frame_num_wrap, ref->field);
    }
    GST_TRACE_OBJECT (self, "ref_frame_list_0_short_term (%d): %s|",
        current_picture->field, str->str);
    g_string_free (str, TRUE);
  }
#endif

  /* 8.2.4.2.2 refFrameList0LongTerm,:
   * long-term ref pictures sorted by ascending long_term_frame_idx.
   */
  gst_h264_dpb_get_pictures_long_term_ref (priv->dpb,
      TRUE, priv->ref_frame_list_long_term);
  g_array_sort (priv->ref_frame_list_long_term,
      (GCompareFunc) long_term_frame_idx_asc_compare);

#ifndef GST_DISABLE_GST_DEBUG
  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_TRACE
      && priv->ref_frame_list_long_term->len) {
    GString *str = g_string_new (NULL);
    for (pos = 0; pos < priv->ref_frame_list_long_term->len; pos++) {
      GstH264Picture *ref = g_array_index (priv->ref_frame_list_0_short_term,
          GstH264Picture *, pos);
      g_string_append_printf (str, "|%i(%d)", ref->long_term_frame_idx,
          ref->field);
    }
    GST_TRACE_OBJECT (self, "ref_frame_list_0_long_term (%d): %s|",
        current_picture->field, str->str);
    g_string_free (str, TRUE);
  }
#endif

  /* 8.2.4.2.5 */
  init_picture_refs_fields_1 (self, current_picture->field,
      priv->ref_frame_list_0_short_term, priv->ref_pic_list_p0);
  init_picture_refs_fields_1 (self, current_picture->field,
      priv->ref_frame_list_long_term, priv->ref_pic_list_p0);

#ifndef GST_DISABLE_GST_DEBUG
  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG
      && priv->ref_pic_list_p0->len) {
    GString *str = g_string_new (NULL);
    for (pos = 0; pos < priv->ref_pic_list_p0->len; pos++) {
      GstH264Picture *ref =
          g_array_index (priv->ref_pic_list_p0, GstH264Picture *, pos);
      if (!GST_H264_PICTURE_IS_LONG_TERM_REF (ref))
        g_string_append_printf (str, "|%i(%d)s", ref->frame_num_wrap,
            ref->field);
      else
        g_string_append_printf (str, "|%i(%d)l", ref->long_term_frame_idx,
            ref->field);
    }
    GST_DEBUG_OBJECT (self, "ref_pic_list_p0 (%d): %s|", current_picture->field,
        str->str);
    g_string_free (str, TRUE);
  }
#endif

  /* Clear temporary lists, now pictures are owned by ref_pic_list_p0 */
  g_array_set_size (priv->ref_frame_list_0_short_term, 0);
  g_array_set_size (priv->ref_frame_list_long_term, 0);
}

static gboolean
lists_are_equal (GArray * l1, GArray * l2)
{
  gint i;

  if (l1->len != l2->len)
    return FALSE;

  for (i = 0; i < l1->len; i++)
    if (g_array_index (l1, gpointer, i) != g_array_index (l2, gpointer, i))
      return FALSE;

  return TRUE;
}

static gint
split_ref_pic_list_b (GstH264Decoder * self, GArray * ref_pic_list_b,
    GCompareFunc compare_func)
{
  gint pos;

  for (pos = 0; pos < ref_pic_list_b->len; pos++) {
    GstH264Picture *pic = g_array_index (ref_pic_list_b, GstH264Picture *, pos);
    if (compare_func (&pic, &self->priv->current_picture) > 0)
      break;
  }

  return pos;
}

static void
print_ref_pic_list_b (GstH264Decoder * self, GArray * ref_list_b,
    const gchar * name)
{
#ifndef GST_DISABLE_GST_DEBUG
  GString *str;
  gint i;

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) < GST_LEVEL_DEBUG)
    return;

  str = g_string_new (NULL);

  for (i = 0; i < ref_list_b->len; i++) {
    GstH264Picture *ref = g_array_index (ref_list_b, GstH264Picture *, i);

    if (!GST_H264_PICTURE_IS_LONG_TERM_REF (ref))
      g_string_append_printf (str, "|%i", ref->pic_order_cnt);
    else
      g_string_append_printf (str, "|%il", ref->long_term_pic_num);
  }

  GST_DEBUG_OBJECT (self, "%s: %s| curr %i", name, str->str,
      self->priv->current_picture->pic_order_cnt);
  g_string_free (str, TRUE);
#endif
}

static void
construct_ref_pic_lists_b (GstH264Decoder * self,
    GstH264Picture * current_picture)
{
  GstH264DecoderPrivate *priv = self->priv;
  gint pos;

  /* RefPicList0 (8.2.4.2.3) [[1] [2] [3]], where:
   * [1] shortterm ref pics with POC < current_picture's POC sorted by descending POC,
   * [2] shortterm ref pics with POC > current_picture's POC by ascending POC,
   * [3] longterm ref pics by ascending long_term_pic_num.
   */
  g_array_set_size (priv->ref_pic_list_b0, 0);
  g_array_set_size (priv->ref_pic_list_b1, 0);

  /* 8.2.4.2.3
   * When pic_order_cnt_type is equal to 0, reference pictures that are marked
   * as "non-existing" as specified in clause 8.2.5.2 are not included in either
   * RefPicList0 or RefPicList1
   */
  gst_h264_dpb_get_pictures_short_term_ref (priv->dpb,
      current_picture->pic_order_cnt_type != 0, FALSE, priv->ref_pic_list_b0);

  /* First sort ascending, this will put [1] in right place and finish
   * [2]. */
  print_ref_pic_list_b (self, priv->ref_pic_list_b0, "ref_pic_list_b0");
  g_array_sort (priv->ref_pic_list_b0, (GCompareFunc) poc_asc_compare);
  print_ref_pic_list_b (self, priv->ref_pic_list_b0, "ref_pic_list_b0");

  /* Find first with POC > current_picture's POC to get first element
   * in [2]... */
  pos = split_ref_pic_list_b (self, priv->ref_pic_list_b0,
      (GCompareFunc) poc_asc_compare);

  GST_DEBUG_OBJECT (self, "split point %i", pos);

  /* and sort [1] descending, thus finishing sequence [1] [2]. */
  g_qsort_with_data (priv->ref_pic_list_b0->data, pos, sizeof (gpointer),
      (GCompareDataFunc) poc_desc_compare, NULL);

  /* Now add [3] and sort by ascending long_term_pic_num. */
  pos = priv->ref_pic_list_b0->len;
  gst_h264_dpb_get_pictures_long_term_ref (priv->dpb,
      FALSE, priv->ref_pic_list_b0);
  g_qsort_with_data (&g_array_index (priv->ref_pic_list_b0, gpointer, pos),
      priv->ref_pic_list_b0->len - pos, sizeof (gpointer),
      (GCompareDataFunc) long_term_pic_num_asc_compare, NULL);

  /* RefPicList1 (8.2.4.2.4) [[1] [2] [3]], where:
   * [1] shortterm ref pics with POC > curr_pic's POC sorted by ascending POC,
   * [2] shortterm ref pics with POC < curr_pic's POC by descending POC,
   * [3] longterm ref pics by ascending long_term_pic_num.
   */
  gst_h264_dpb_get_pictures_short_term_ref (priv->dpb,
      current_picture->pic_order_cnt_type != 0, FALSE, priv->ref_pic_list_b1);

  /* First sort by descending POC. */
  g_array_sort (priv->ref_pic_list_b1, (GCompareFunc) poc_desc_compare);

  /* Split at first with POC < current_picture's POC to get first element
   * in [2]... */
  pos = split_ref_pic_list_b (self, priv->ref_pic_list_b1,
      (GCompareFunc) poc_desc_compare);

  /* and sort [1] ascending. */
  g_qsort_with_data (priv->ref_pic_list_b1->data, pos, sizeof (gpointer),
      (GCompareDataFunc) poc_asc_compare, NULL);

  /* Now add [3] and sort by ascending long_term_pic_num */
  pos = priv->ref_pic_list_b1->len;
  gst_h264_dpb_get_pictures_long_term_ref (priv->dpb,
      FALSE, priv->ref_pic_list_b1);
  g_qsort_with_data (&g_array_index (priv->ref_pic_list_b1, gpointer, pos),
      priv->ref_pic_list_b1->len - pos, sizeof (gpointer),
      (GCompareDataFunc) long_term_pic_num_asc_compare, NULL);

  /* If lists identical, swap first two entries in RefPicList1 (spec
   * 8.2.4.2.3) */
  if (priv->ref_pic_list_b1->len > 1
      && lists_are_equal (priv->ref_pic_list_b0, priv->ref_pic_list_b1)) {
    /* swap */
    GstH264Picture **list = (GstH264Picture **) priv->ref_pic_list_b1->data;
    GstH264Picture *pic = list[0];
    list[0] = list[1];
    list[1] = pic;
  }

  print_ref_pic_list_b (self, priv->ref_pic_list_b0, "ref_pic_list_b0");
  print_ref_pic_list_b (self, priv->ref_pic_list_b1, "ref_pic_list_b1");
}

static void
construct_ref_field_pic_lists_b (GstH264Decoder * self,
    GstH264Picture * current_picture)
{
  GstH264DecoderPrivate *priv = self->priv;
  gint pos;

  /* refFrameList0ShortTerm (8.2.4.2.4) [[1] [2]], where:
   * [1] shortterm ref pics with POC < current_picture's POC sorted by descending POC,
   * [2] shortterm ref pics with POC > current_picture's POC by ascending POC,
   */
  g_array_set_size (priv->ref_pic_list_b0, 0);
  g_array_set_size (priv->ref_pic_list_b1, 0);
  g_array_set_size (priv->ref_frame_list_0_short_term, 0);
  g_array_set_size (priv->ref_frame_list_1_short_term, 0);
  g_array_set_size (priv->ref_frame_list_long_term, 0);

  /* 8.2.4.2.4
   * When pic_order_cnt_type is equal to 0, reference pictures that are marked
   * as "non-existing" as specified in clause 8.2.5.2 are not included in either
   * RefPicList0 or RefPicList1
   */
  gst_h264_dpb_get_pictures_short_term_ref (priv->dpb,
      current_picture->pic_order_cnt_type != 0, TRUE,
      priv->ref_frame_list_0_short_term);

  /* First sort ascending, this will put [1] in right place and finish
   * [2]. */
  print_ref_pic_list_b (self, priv->ref_frame_list_0_short_term,
      "ref_frame_list_0_short_term");
  g_array_sort (priv->ref_frame_list_0_short_term,
      (GCompareFunc) poc_asc_compare);
  print_ref_pic_list_b (self, priv->ref_frame_list_0_short_term,
      "ref_frame_list_0_short_term");

  /* Find first with POC > current_picture's POC to get first element
   * in [2]... */
  pos = split_ref_pic_list_b (self, priv->ref_frame_list_0_short_term,
      (GCompareFunc) poc_asc_compare);

  GST_DEBUG_OBJECT (self, "split point %i", pos);

  /* and sort [1] descending, thus finishing sequence [1] [2]. */
  g_qsort_with_data (priv->ref_frame_list_0_short_term->data, pos,
      sizeof (gpointer), (GCompareDataFunc) poc_desc_compare, NULL);

  /* refFrameList1ShortTerm (8.2.4.2.4) [[1] [2]], where:
   * [1] shortterm ref pics with POC > curr_pic's POC sorted by ascending POC,
   * [2] shortterm ref pics with POC < curr_pic's POC by descending POC,
   */
  gst_h264_dpb_get_pictures_short_term_ref (priv->dpb,
      current_picture->pic_order_cnt_type != 0, TRUE,
      priv->ref_frame_list_1_short_term);

  /* First sort by descending POC. */
  g_array_sort (priv->ref_frame_list_1_short_term,
      (GCompareFunc) poc_desc_compare);

  /* Split at first with POC < current_picture's POC to get first element
   * in [2]... */
  pos = split_ref_pic_list_b (self, priv->ref_frame_list_1_short_term,
      (GCompareFunc) poc_desc_compare);

  /* and sort [1] ascending. */
  g_qsort_with_data (priv->ref_frame_list_1_short_term->data, pos,
      sizeof (gpointer), (GCompareDataFunc) poc_asc_compare, NULL);

  /* 8.2.4.2.2 refFrameList0LongTerm,:
   * long-term ref pictures sorted by ascending long_term_frame_idx.
   */
  gst_h264_dpb_get_pictures_long_term_ref (priv->dpb,
      TRUE, priv->ref_frame_list_long_term);
  g_array_sort (priv->ref_frame_list_long_term,
      (GCompareFunc) long_term_frame_idx_asc_compare);

  /* 8.2.4.2.5 RefPicList0 */
  init_picture_refs_fields_1 (self, current_picture->field,
      priv->ref_frame_list_0_short_term, priv->ref_pic_list_b0);
  init_picture_refs_fields_1 (self, current_picture->field,
      priv->ref_frame_list_long_term, priv->ref_pic_list_b0);

  /* 8.2.4.2.5 RefPicList1 */
  init_picture_refs_fields_1 (self, current_picture->field,
      priv->ref_frame_list_1_short_term, priv->ref_pic_list_b1);
  init_picture_refs_fields_1 (self, current_picture->field,
      priv->ref_frame_list_long_term, priv->ref_pic_list_b1);

  /* If lists identical, swap first two entries in RefPicList1 (spec
   * 8.2.4.2.5) */
  if (priv->ref_pic_list_b1->len > 1
      && lists_are_equal (priv->ref_pic_list_b0, priv->ref_pic_list_b1)) {
    /* swap */
    GstH264Picture **list = (GstH264Picture **) priv->ref_pic_list_b1->data;
    GstH264Picture *pic = list[0];
    list[0] = list[1];
    list[1] = pic;
  }

  print_ref_pic_list_b (self, priv->ref_pic_list_b0, "ref_pic_list_b0");
  print_ref_pic_list_b (self, priv->ref_pic_list_b1, "ref_pic_list_b1");

  /* Clear temporary lists, now pictures are owned by ref_pic_list_b0
   * and ref_pic_list_b1 */
  g_array_set_size (priv->ref_frame_list_0_short_term, 0);
  g_array_set_size (priv->ref_frame_list_1_short_term, 0);
  g_array_set_size (priv->ref_frame_list_long_term, 0);
}

static void
gst_h264_decoder_prepare_ref_pic_lists (GstH264Decoder * self,
    GstH264Picture * current_picture)
{
  GstH264DecoderPrivate *priv = self->priv;
  gboolean construct_list = FALSE;
  gint i;
  GArray *dpb_array = gst_h264_dpb_get_pictures_all (priv->dpb);

  /* 8.2.4.2.1 ~ 8.2.4.2.4
   * When this process is invoked, there shall be at least one reference entry
   * that is currently marked as "used for reference"
   * (i.e., as "used for short-term reference" or "used for long-term reference")
   * and is not marked as "non-existing"
   */
  for (i = 0; i < dpb_array->len; i++) {
    GstH264Picture *picture = g_array_index (dpb_array, GstH264Picture *, i);
    if (GST_H264_PICTURE_IS_REF (picture) && !picture->nonexisting) {
      construct_list = TRUE;
      break;
    }
  }
  g_array_unref (dpb_array);

  if (!construct_list) {
    gst_h264_decoder_clear_ref_pic_lists (self);
    return;
  }

  if (GST_H264_PICTURE_IS_FRAME (current_picture)) {
    construct_ref_pic_lists_p (self, current_picture);
    construct_ref_pic_lists_b (self, current_picture);
  } else {
    construct_ref_field_pic_lists_p (self, current_picture);
    construct_ref_field_pic_lists_b (self, current_picture);
  }
}

static void
gst_h264_decoder_clear_ref_pic_lists (GstH264Decoder * self)
{
  GstH264DecoderPrivate *priv = self->priv;

  g_array_set_size (priv->ref_pic_list_p0, 0);
  g_array_set_size (priv->ref_pic_list_b0, 0);
  g_array_set_size (priv->ref_pic_list_b1, 0);
}

static gint
long_term_pic_num_f (GstH264Decoder * self, const GstH264Picture * picture)
{
  if (GST_H264_PICTURE_IS_LONG_TERM_REF (picture))
    return picture->long_term_pic_num;
  return 2 * (self->priv->max_long_term_frame_idx + 1);
}

static gint
pic_num_f (GstH264Decoder * self, const GstH264Picture * picture)
{
  if (!GST_H264_PICTURE_IS_LONG_TERM_REF (picture))
    return picture->pic_num;
  return self->priv->max_pic_num;
}

/* shift elements on the |array| starting from |from| to |to|,
 * inclusive, one position to the right and insert pic at |from| */
static void
shift_right_and_insert (GArray * array, gint from, gint to,
    GstH264Picture * picture)
{
  g_return_if_fail (from <= to);
  g_return_if_fail (array && picture);

  g_array_set_size (array, to + 2);
  g_array_insert_val (array, from, picture);
}

/* This can process either ref_pic_list0 or ref_pic_list1, depending
 * on the list argument. Set up pointers to proper list to be
 * processed here. */
static gboolean
modify_ref_pic_list (GstH264Decoder * self, int list)
{
  GstH264DecoderPrivate *priv = self->priv;
  GstH264Picture *picture = priv->current_picture;
  GArray *ref_pic_listx;
  const GstH264SliceHdr *slice_hdr = &priv->current_slice.header;
  const GstH264RefPicListModification *list_mod;
  gboolean ref_pic_list_modification_flag_lX;
  gint num_ref_idx_lX_active_minus1;
  guint num_ref_pic_list_modifications;
  gint i;
  gint pic_num_lx_pred = picture->pic_num;
  gint ref_idx_lx = 0, src, dst;
  gint pic_num_lx_no_wrap;
  gint pic_num_lx;
  gboolean done = FALSE;
  GstH264Picture *pic;

  if (list == 0) {
    ref_pic_listx = priv->ref_pic_list0;
    ref_pic_list_modification_flag_lX =
        slice_hdr->ref_pic_list_modification_flag_l0;
    num_ref_pic_list_modifications = slice_hdr->n_ref_pic_list_modification_l0;
    num_ref_idx_lX_active_minus1 = slice_hdr->num_ref_idx_l0_active_minus1;
    list_mod = slice_hdr->ref_pic_list_modification_l0;
  } else {
    ref_pic_listx = priv->ref_pic_list1;
    ref_pic_list_modification_flag_lX =
        slice_hdr->ref_pic_list_modification_flag_l1;
    num_ref_pic_list_modifications = slice_hdr->n_ref_pic_list_modification_l1;
    num_ref_idx_lX_active_minus1 = slice_hdr->num_ref_idx_l1_active_minus1;
    list_mod = slice_hdr->ref_pic_list_modification_l1;
  }

  /* Resize the list to the size requested in the slice header.
   *
   * Note that per 8.2.4.2 it's possible for
   * num_ref_idx_lX_active_minus1 to indicate there should be more ref
   * pics on list than we constructed.  Those superfluous ones should
   * be treated as non-reference and will be initialized to null,
   * which must be handled by clients */
  g_assert (num_ref_idx_lX_active_minus1 >= 0);
  if (ref_pic_listx->len > num_ref_idx_lX_active_minus1 + 1)
    g_array_set_size (ref_pic_listx, num_ref_idx_lX_active_minus1 + 1);

  if (!ref_pic_list_modification_flag_lX)
    return TRUE;

  /* Spec 8.2.4.3:
   * Reorder pictures on the list in a way specified in the stream. */
  for (i = 0; i < num_ref_pic_list_modifications && !done; i++) {
    switch (list_mod->modification_of_pic_nums_idc) {
        /* 8.2.4.3.1 - Modify short reference picture position. */
      case 0:
      case 1:
        /* 8-34 */
        if (list_mod->modification_of_pic_nums_idc == 0) {
          /* Substract given value from predicted PicNum. */
          pic_num_lx_no_wrap = pic_num_lx_pred -
              (list_mod->value.abs_diff_pic_num_minus1 + 1);
          /* Wrap around max_pic_num if it becomes < 0 as result of
           * subtraction */
          if (pic_num_lx_no_wrap < 0)
            pic_num_lx_no_wrap += priv->max_pic_num;
        } else {                /* 8-35 */
          /* Add given value to predicted PicNum. */
          pic_num_lx_no_wrap = pic_num_lx_pred +
              (list_mod->value.abs_diff_pic_num_minus1 + 1);
          /* Wrap around max_pic_num if it becomes >= max_pic_num as
           * result of the addition */
          if (pic_num_lx_no_wrap >= priv->max_pic_num)
            pic_num_lx_no_wrap -= priv->max_pic_num;
        }

        /* For use in next iteration */
        pic_num_lx_pred = pic_num_lx_no_wrap;

        /* 8-36 */
        if (pic_num_lx_no_wrap > picture->pic_num)
          pic_num_lx = pic_num_lx_no_wrap - priv->max_pic_num;
        else
          pic_num_lx = pic_num_lx_no_wrap;

        /* 8-37 */
        g_assert (num_ref_idx_lX_active_minus1 + 1 < 32);
        pic = gst_h264_dpb_get_short_ref_by_pic_num (priv->dpb, pic_num_lx);
        if (!pic) {
          GST_WARNING_OBJECT (self, "Malformed stream, no pic num %d",
              pic_num_lx);
          break;
        }
        shift_right_and_insert (ref_pic_listx, ref_idx_lx,
            num_ref_idx_lX_active_minus1, pic);
        ref_idx_lx++;

        for (src = ref_idx_lx, dst = ref_idx_lx;
            src <= num_ref_idx_lX_active_minus1 + 1; src++) {
          GstH264Picture *src_pic =
              g_array_index (ref_pic_listx, GstH264Picture *, src);
          gint src_pic_num_lx = src_pic ? pic_num_f (self, src_pic) : -1;
          if (src_pic_num_lx != pic_num_lx)
            g_array_index (ref_pic_listx, GstH264Picture *, dst++) = src_pic;
        }

        break;

        /* 8.2.4.3.2 - Long-term reference pictures */
      case 2:
        /* (8-28) */
        g_assert (num_ref_idx_lX_active_minus1 + 1 < 32);
        pic = gst_h264_dpb_get_long_ref_by_long_term_pic_num (priv->dpb,
            list_mod->value.long_term_pic_num);
        if (!pic) {
          GST_WARNING_OBJECT (self, "Malformed stream, no pic num %d",
              list_mod->value.long_term_pic_num);
          break;
        }
        shift_right_and_insert (ref_pic_listx, ref_idx_lx,
            num_ref_idx_lX_active_minus1, pic);
        ref_idx_lx++;

        for (src = ref_idx_lx, dst = ref_idx_lx;
            src <= num_ref_idx_lX_active_minus1 + 1; src++) {
          GstH264Picture *src_pic =
              g_array_index (ref_pic_listx, GstH264Picture *, src);
          if (long_term_pic_num_f (self, src_pic) !=
              list_mod->value.long_term_pic_num)
            g_array_index (ref_pic_listx, GstH264Picture *, dst++) = src_pic;
        }

        break;

        /* End of modification list */
      case 3:
        done = TRUE;
        break;

      default:
        /* may be recoverable */
        GST_WARNING ("Invalid modification_of_pic_nums_idc = %d",
            list_mod->modification_of_pic_nums_idc);
        break;
    }

    list_mod++;
  }

  /* Per NOTE 2 in 8.2.4.3.2, the ref_pic_listx in the above loop is
   * temporarily made one element longer than the required final list.
   * Resize the list back to its required size. */
  if (ref_pic_listx->len > num_ref_idx_lX_active_minus1 + 1)
    g_array_set_size (ref_pic_listx, num_ref_idx_lX_active_minus1 + 1);

  return TRUE;
}

static void
copy_pic_list_into (GArray * dest, GArray * src)
{
  gint i;
  g_array_set_size (dest, 0);

  for (i = 0; i < src->len; i++)
    g_array_append_val (dest, g_array_index (src, gpointer, i));
}

static gboolean
gst_h264_decoder_modify_ref_pic_lists (GstH264Decoder * self)
{
  GstH264DecoderPrivate *priv = self->priv;
  GstH264SliceHdr *slice_hdr = &priv->current_slice.header;

  g_array_set_size (priv->ref_pic_list0, 0);
  g_array_set_size (priv->ref_pic_list1, 0);

  if (GST_H264_IS_P_SLICE (slice_hdr) || GST_H264_IS_SP_SLICE (slice_hdr)) {
    /* 8.2.4 fill reference picture list RefPicList0 for P or SP slice */
    copy_pic_list_into (priv->ref_pic_list0, priv->ref_pic_list_p0);
    return modify_ref_pic_list (self, 0);
  } else if (GST_H264_IS_B_SLICE (slice_hdr)) {
    /* 8.2.4 fill reference picture list RefPicList0 and RefPicList1 for B slice */
    copy_pic_list_into (priv->ref_pic_list0, priv->ref_pic_list_b0);
    copy_pic_list_into (priv->ref_pic_list1, priv->ref_pic_list_b1);
    return modify_ref_pic_list (self, 0)
        && modify_ref_pic_list (self, 1);
  }

  return TRUE;
}

/**
 * gst_h264_decoder_set_process_ref_pic_lists:
 * @decoder: a #GstH264Decoder
 * @process: whether subclass is requiring reference picture modification process
 *
 * Called to en/disable reference picture modification process.
 *
 * Since: 1.18
 */
void
gst_h264_decoder_set_process_ref_pic_lists (GstH264Decoder * decoder,
    gboolean process)
{
  decoder->priv->process_ref_pic_lists = process;
}

/**
 * gst_h264_decoder_get_picture:
 * @decoder: a #GstH264Decoder
 * @system_frame_number: a target system frame number of #GstH264Picture
 *
 * Retrive DPB and return a #GstH264Picture corresponding to
 * the @system_frame_number
 *
 * Returns: (transfer full) (nullable): a #GstH264Picture if successful, or %NULL otherwise
 *
 * Since: 1.18
 */
GstH264Picture *
gst_h264_decoder_get_picture (GstH264Decoder * decoder,
    guint32 system_frame_number)
{
  return gst_h264_dpb_get_picture (decoder->priv->dpb, system_frame_number);
}
