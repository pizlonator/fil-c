/* GStreamer
 * Copyright (C) 2020 Intel Corporation
 *     Author: He Junyan <junyan.he@intel.com>
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
 * SECTION:gstmpeg2decoder
 * @title: GstMpeg2Decoder
 * @short_description: Base class to implement stateless MPEG2 decoders
 * @sources:
 * - gstmpeg2picture.h
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/base/base.h>
#include "gstmpeg2decoder.h"

GST_DEBUG_CATEGORY (gst_mpeg2_decoder_debug);
#define GST_CAT_DEFAULT gst_mpeg2_decoder_debug

/* ------------------------------------------------------------------------- */
/* --- PTS Generator                                                     --- */
/* ------------------------------------------------------------------------- */
typedef struct _PTSGenerator PTSGenerator;
struct _PTSGenerator
{
  /* The current GOP PTS */
  GstClockTime gop_pts;
  /* Max picture PTS */
  GstClockTime max_pts;
  /* Absolute GOP TSN */
  guint gop_tsn;
  /* Max picture TSN, relative to last GOP TSN */
  guint max_tsn;
  /* How many times TSN overflowed since GOP */
  guint ovl_tsn;
  /* Last picture TSN */
  guint lst_tsn;
  guint fps_n;
  guint fps_d;
};

static void
_pts_init (PTSGenerator * tsg)
{
  tsg->gop_pts = GST_CLOCK_TIME_NONE;
  tsg->max_pts = GST_CLOCK_TIME_NONE;
  tsg->gop_tsn = 0;
  tsg->max_tsn = 0;
  tsg->ovl_tsn = 0;
  tsg->lst_tsn = 0;
  tsg->fps_n = 0;
  tsg->fps_d = 0;
}

static inline GstClockTime
_pts_get_duration (PTSGenerator * tsg, guint num_frames)
{
  return gst_util_uint64_scale (num_frames, GST_SECOND * tsg->fps_d,
      tsg->fps_n);
}

static inline guint
_pts_get_poc (PTSGenerator * tsg)
{
  return tsg->gop_tsn + tsg->ovl_tsn * 1024 + tsg->lst_tsn;
}

static void
_pts_set_framerate (PTSGenerator * tsg, guint fps_n, guint fps_d)
{
  tsg->fps_n = fps_n;
  tsg->fps_d = fps_d;
}

static void
_pts_sync (PTSGenerator * tsg, GstClockTime gop_pts)
{
  guint gop_tsn;

  if (!GST_CLOCK_TIME_IS_VALID (gop_pts) ||
      (GST_CLOCK_TIME_IS_VALID (tsg->max_pts) && tsg->max_pts >= gop_pts)) {
    /* Invalid GOP PTS, interpolate from the last known picture PTS */
    if (GST_CLOCK_TIME_IS_VALID (tsg->max_pts)) {
      gop_pts = tsg->max_pts + _pts_get_duration (tsg, 1);
      gop_tsn = tsg->gop_tsn + tsg->ovl_tsn * 1024 + tsg->max_tsn + 1;
    } else {
      gop_pts = 0;
      gop_tsn = 0;
    }
  } else {
    /* Interpolate GOP TSN from this valid PTS */
    if (GST_CLOCK_TIME_IS_VALID (tsg->gop_pts))
      gop_tsn = tsg->gop_tsn + gst_util_uint64_scale (gop_pts - tsg->gop_pts +
          _pts_get_duration (tsg, 1) - 1, tsg->fps_n, GST_SECOND * tsg->fps_d);
    else
      gop_tsn = 0;
  }

  tsg->gop_pts = gop_pts;
  tsg->gop_tsn = gop_tsn;
  tsg->max_tsn = 0;
  tsg->ovl_tsn = 0;
  tsg->lst_tsn = 0;
}

static GstClockTime
_pts_eval (PTSGenerator * tsg, GstClockTime pic_pts, guint pic_tsn)
{
  GstClockTime pts;

  if (!GST_CLOCK_TIME_IS_VALID (tsg->gop_pts))
    tsg->gop_pts = _pts_get_duration (tsg, pic_tsn);

  pts = pic_pts;
  if (!GST_CLOCK_TIME_IS_VALID (pts))
    pts = tsg->gop_pts + _pts_get_duration (tsg, tsg->ovl_tsn * 1024 + pic_tsn);
  else if (pts == tsg->gop_pts) {
    /* The picture following the GOP header shall be an I-frame.
       So we can compensate for the GOP start time from here */
    tsg->gop_pts -= _pts_get_duration (tsg, pic_tsn);
  }

  if (!GST_CLOCK_TIME_IS_VALID (tsg->max_pts) || tsg->max_pts < pts)
    tsg->max_pts = pts;

  if (tsg->max_tsn < pic_tsn)
    tsg->max_tsn = pic_tsn;
  else if (tsg->max_tsn == 1023 && pic_tsn < tsg->lst_tsn) {    /* TSN wrapped */
    tsg->max_tsn = pic_tsn;
    tsg->ovl_tsn++;
  }
  tsg->lst_tsn = pic_tsn;

  return pts;
}

static inline gboolean
_seq_hdr_is_valid (GstMpegVideoSequenceHdr * hdr)
{
  return hdr->width > 0 && hdr->height > 0;
}

#define SEQ_HDR_INIT (GstMpegVideoSequenceHdr) { 0, }

static inline gboolean
_seq_ext_is_valid (GstMpegVideoSequenceExt * ext)
{
  return ext->profile >= GST_MPEG_VIDEO_PROFILE_422
      && ext->profile <= GST_MPEG_VIDEO_PROFILE_SIMPLE;
}

#define SEQ_EXT_INIT (GstMpegVideoSequenceExt) { 0xff, 0, }

static inline gboolean
_seq_display_ext_is_valid (GstMpegVideoSequenceDisplayExt * ext)
{
  return ext->video_format != 0xff;
}

#define SEQ_DISPLAY_EXT_INIT (GstMpegVideoSequenceDisplayExt) { 0xff, 0, }

static inline gboolean
_seq_scalable_ext_is_valid (GstMpegVideoSequenceScalableExt * ext)
{
  return ext->scalable_mode != 0xff;
}

#define SEQ_SCALABLE_EXT_INIT (GstMpegVideoSequenceScalableExt) { 0xff, 0, }

static inline gboolean
_quant_matrix_ext_is_valid (GstMpegVideoQuantMatrixExt * ext)
{
  return ext->load_intra_quantiser_matrix != 0xff;
}

#define QUANT_MATRIX_EXT_INIT (GstMpegVideoQuantMatrixExt) { 0xff, { 0, } }

static inline gboolean
_pic_hdr_is_valid (GstMpegVideoPictureHdr * hdr)
{
  return hdr->tsn != 0xffff;
}

#define PIC_HDR_INIT (GstMpegVideoPictureHdr) { 0xffff, 0, }

static inline gboolean
_pic_hdr_ext_is_valid (GstMpegVideoPictureExt * ext)
{
  return ext->f_code[0][0] != 0xff;
}

#define PIC_HDR_EXT_INIT                                        \
    (GstMpegVideoPictureExt) { { { 0xff, 0, }, { 0, } }, 0, }

typedef enum
{
  GST_MPEG2_DECODER_STATE_GOT_SEQ_HDR = 1 << 0,
  GST_MPEG2_DECODER_STATE_GOT_SEQ_EXT = 1 << 1,
  GST_MPEG2_DECODER_STATE_GOT_PIC_HDR = 1 << 2,
  GST_MPEG2_DECODER_STATE_GOT_PIC_EXT = 1 << 3,
  GST_MPEG2_DECODER_STATE_GOT_SLICE = 1 << 4,

  GST_MPEG2_DECODER_STATE_VALID_SEQ_HEADERS =
      (GST_MPEG2_DECODER_STATE_GOT_SEQ_HDR |
      GST_MPEG2_DECODER_STATE_GOT_SEQ_EXT),
  GST_MPEG2_DECODER_STATE_VALID_PIC_HEADERS =
      (GST_MPEG2_DECODER_STATE_GOT_PIC_HDR |
      GST_MPEG2_DECODER_STATE_GOT_PIC_EXT),
  GST_MPEG2_DECODER_STATE_VALID_PICTURE =
      (GST_MPEG2_DECODER_STATE_VALID_SEQ_HEADERS |
      GST_MPEG2_DECODER_STATE_VALID_PIC_HEADERS |
      GST_MPEG2_DECODER_STATE_GOT_SLICE)
} GstMpeg2DecoderState;

struct _GstMpeg2DecoderPrivate
{
  gint width;
  gint height;
  gint display_width;
  gint display_height;
  GstMpegVideoProfile profile;
  gboolean progressive;

  GstMpegVideoSequenceHdr seq_hdr;
  GstMpegVideoSequenceExt seq_ext;
  GstMpegVideoSequenceDisplayExt seq_display_ext;
  GstMpegVideoSequenceScalableExt seq_scalable_ext;

  /* some sequence info changed after last new_sequence () */
  gboolean seq_changed;
  /* whether we need to drain before new_sequence () */
  gboolean need_to_drain;
  GstMpegVideoGop gop;
  GstMpegVideoQuantMatrixExt quant_matrix;
  GstMpegVideoPictureHdr pic_hdr;
  GstMpegVideoPictureExt pic_ext;

  GstMpeg2Dpb *dpb;
  GstMpeg2DecoderState state;
  PTSGenerator tsg;
  GstClockTime current_pts;

  GstMpeg2Picture *current_picture;
  GstVideoCodecFrame *current_frame;
  GstMpeg2Picture *first_field;

  guint preferred_output_delay;
  /* for delayed output */
  GstQueueArray *output_queue;
  /* used for low-latency vs. high throughput mode decision */
  gboolean is_live;

  gboolean input_state_changed;

  GstFlowReturn last_flow;
};

#define UPDATE_FLOW_RETURN(ret,new_ret) G_STMT_START { \
  if (*(ret) == GST_FLOW_OK) \
    *(ret) = new_ret; \
} G_STMT_END

typedef struct
{
  GstVideoCodecFrame *frame;
  GstMpeg2Picture *picture;
  GstMpeg2Decoder *self;
} GstMpeg2DecoderOutputFrame;


#define parent_class gst_mpeg2_decoder_parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstMpeg2Decoder, gst_mpeg2_decoder,
    GST_TYPE_VIDEO_DECODER,
    G_ADD_PRIVATE (GstMpeg2Decoder);
    GST_DEBUG_CATEGORY_INIT (gst_mpeg2_decoder_debug, "mpeg2decoder", 0,
        "MPEG2 Video Decoder"));

static gboolean gst_mpeg2_decoder_start (GstVideoDecoder * decoder);
static gboolean gst_mpeg2_decoder_stop (GstVideoDecoder * decoder);
static gboolean gst_mpeg2_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_mpeg2_decoder_negotiate (GstVideoDecoder * decoder);
static GstFlowReturn gst_mpeg2_decoder_finish (GstVideoDecoder * decoder);
static gboolean gst_mpeg2_decoder_flush (GstVideoDecoder * decoder);
static GstFlowReturn gst_mpeg2_decoder_drain (GstVideoDecoder * decoder);
static GstFlowReturn gst_mpeg2_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static void gst_mpeg2_decoder_do_output_picture (GstMpeg2Decoder * self,
    GstMpeg2Picture * picture, GstFlowReturn * ret);
static void gst_mpeg2_decoder_clear_output_frame (GstMpeg2DecoderOutputFrame *
    output_frame);
static void gst_mpeg2_decoder_drain_output_queue (GstMpeg2Decoder *
    self, guint num, GstFlowReturn * ret);


static void
gst_mpeg2_decoder_class_init (GstMpeg2DecoderClass * klass)
{
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  decoder_class->start = GST_DEBUG_FUNCPTR (gst_mpeg2_decoder_start);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_mpeg2_decoder_stop);
  decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_mpeg2_decoder_set_format);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_mpeg2_decoder_negotiate);
  decoder_class->finish = GST_DEBUG_FUNCPTR (gst_mpeg2_decoder_finish);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_mpeg2_decoder_flush);
  decoder_class->drain = GST_DEBUG_FUNCPTR (gst_mpeg2_decoder_drain);
  decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_mpeg2_decoder_handle_frame);
}

static void
gst_mpeg2_decoder_init (GstMpeg2Decoder * self)
{
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (self), TRUE);
  gst_video_decoder_set_needs_format (GST_VIDEO_DECODER (self), TRUE);

  self->priv = gst_mpeg2_decoder_get_instance_private (self);

  self->priv->seq_hdr = SEQ_HDR_INIT;
  self->priv->seq_ext = SEQ_EXT_INIT;
  self->priv->seq_display_ext = SEQ_DISPLAY_EXT_INIT;
  self->priv->seq_scalable_ext = SEQ_SCALABLE_EXT_INIT;
  self->priv->quant_matrix = QUANT_MATRIX_EXT_INIT;
  self->priv->pic_hdr = PIC_HDR_INIT;
  self->priv->pic_ext = PIC_HDR_EXT_INIT;
}

static gboolean
gst_mpeg2_decoder_start (GstVideoDecoder * decoder)
{
  GstMpeg2Decoder *self = GST_MPEG2_DECODER (decoder);
  GstMpeg2DecoderPrivate *priv = self->priv;

  _pts_init (&priv->tsg);
  priv->dpb = gst_mpeg2_dpb_new ();
  priv->profile = -1;
  priv->progressive = TRUE;
  priv->last_flow = GST_FLOW_OK;

  priv->output_queue =
      gst_queue_array_new_for_struct (sizeof (GstMpeg2DecoderOutputFrame), 1);
  gst_queue_array_set_clear_func (priv->output_queue,
      (GDestroyNotify) gst_mpeg2_decoder_clear_output_frame);

  return TRUE;
}

static gboolean
gst_mpeg2_decoder_stop (GstVideoDecoder * decoder)
{
  GstMpeg2Decoder *self = GST_MPEG2_DECODER (decoder);
  GstMpeg2DecoderPrivate *priv = self->priv;

  g_clear_pointer (&self->input_state, gst_video_codec_state_unref);
  g_clear_pointer (&priv->dpb, gst_mpeg2_dpb_free);
  gst_queue_array_free (priv->output_queue);

  return TRUE;
}

static gboolean
gst_mpeg2_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstMpeg2Decoder *self = GST_MPEG2_DECODER (decoder);
  GstMpeg2DecoderPrivate *priv = self->priv;
  GstQuery *query;

  GST_DEBUG_OBJECT (decoder, "Set format");

  priv->input_state_changed = TRUE;

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);

  self->input_state = gst_video_codec_state_ref (state);

  priv->width = GST_VIDEO_INFO_WIDTH (&state->info);
  priv->height = GST_VIDEO_INFO_HEIGHT (&state->info);

  query = gst_query_new_latency ();
  if (gst_pad_peer_query (GST_VIDEO_DECODER_SINK_PAD (self), query))
    gst_query_parse_latency (query, &priv->is_live, NULL, NULL);
  gst_query_unref (query);

  return TRUE;
}

static gboolean
gst_mpeg2_decoder_negotiate (GstVideoDecoder * decoder)
{
  GstMpeg2Decoder *self = GST_MPEG2_DECODER (decoder);

  /* output state must be updated by subclass using new input state already */
  self->priv->input_state_changed = FALSE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static GstFlowReturn
gst_mpeg2_decoder_drain (GstVideoDecoder * decoder)
{
  GstMpeg2Decoder *self = GST_MPEG2_DECODER (decoder);
  GstMpeg2DecoderPrivate *priv = self->priv;
  GstMpeg2Picture *picture;
  GstFlowReturn ret = GST_FLOW_OK;

  while ((picture = gst_mpeg2_dpb_bump (priv->dpb)) != NULL) {
    gst_mpeg2_decoder_do_output_picture (self, picture, &ret);
  }

  gst_mpeg2_decoder_drain_output_queue (self, 0, &ret);
  gst_queue_array_clear (priv->output_queue);
  gst_mpeg2_dpb_clear (priv->dpb);

  return ret;
}

static GstFlowReturn
gst_mpeg2_decoder_finish (GstVideoDecoder * decoder)
{
  return gst_mpeg2_decoder_drain (decoder);
}

static gboolean
gst_mpeg2_decoder_flush (GstVideoDecoder * decoder)
{
  GstMpeg2Decoder *self = GST_MPEG2_DECODER (decoder);
  GstMpeg2DecoderPrivate *priv = self->priv;

  gst_mpeg2_dpb_clear (priv->dpb);
  gst_queue_array_clear (priv->output_queue);
  priv->state &= GST_MPEG2_DECODER_STATE_VALID_SEQ_HEADERS;
  priv->pic_hdr = PIC_HDR_INIT;
  priv->pic_ext = PIC_HDR_EXT_INIT;

  return TRUE;
}

static inline gboolean
_is_valid_state (GstMpeg2Decoder * decoder, GstMpeg2DecoderState state)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;

  return (priv->state & state) == state;
}

static void
gst_mpeg2_decoder_set_latency (GstMpeg2Decoder * decoder)
{
  GstCaps *caps;
  GstClockTime min, max;
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstStructure *structure;
  gint fps_d = 1, fps_n = 0;

  if (priv->tsg.fps_d > 0 && priv->tsg.fps_n > 0) {
    fps_n = priv->tsg.fps_n;
    fps_d = priv->tsg.fps_d;
  } else {
    caps = gst_pad_get_current_caps (GST_VIDEO_DECODER_SINK_PAD (decoder));
    if (caps) {
      structure = gst_caps_get_structure (caps, 0);
      if (gst_structure_get_fraction (structure, "framerate", &fps_n, &fps_d)) {
        if (fps_n == 0) {
          /* variable framerate: see if we have a max-framerate */
          gst_structure_get_fraction (structure, "max-framerate", &fps_n,
              &fps_d);
        }
      }
      gst_caps_unref (caps);
    }
  }

  /* if no fps or variable, then 25/1 */
  if (fps_n == 0) {
    fps_n = 25;
    fps_d = 1;
  }

  max = gst_util_uint64_scale (2 * GST_SECOND, fps_d, fps_n);
  min = gst_util_uint64_scale (1 * GST_SECOND, fps_d, fps_n);

  GST_LOG_OBJECT (decoder,
      "latency min %" G_GUINT64_FORMAT " max %" G_GUINT64_FORMAT, min, max);

  gst_video_decoder_set_latency (GST_VIDEO_DECODER (decoder), min, max);
}

static GstFlowReturn
gst_mpeg2_decoder_handle_sequence (GstMpeg2Decoder * decoder,
    GstMpegVideoPacket * packet)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpegVideoSequenceHdr seq_hdr = { 0, };

  if (!gst_mpeg_video_packet_parse_sequence_header (packet, &seq_hdr)) {
    GST_ERROR_OBJECT (decoder, "failed to parse sequence header");
    return GST_FLOW_ERROR;
  }

  /* 6.1.1.6 Sequence header
     The quantisation matrices may be redefined each time that a sequence
     header occurs in the bitstream */
  priv->quant_matrix = QUANT_MATRIX_EXT_INIT;

  if (_seq_hdr_is_valid (&priv->seq_hdr) &&
      memcmp (&priv->seq_hdr, &seq_hdr, sizeof (seq_hdr)) == 0)
    return GST_FLOW_OK;

  priv->seq_ext = SEQ_EXT_INIT;
  priv->seq_display_ext = SEQ_DISPLAY_EXT_INIT;
  priv->seq_scalable_ext = SEQ_SCALABLE_EXT_INIT;
  priv->pic_ext = PIC_HDR_EXT_INIT;

  priv->seq_hdr = seq_hdr;
  priv->seq_changed = TRUE;

  if (priv->width != seq_hdr.width || priv->height != seq_hdr.height) {
    priv->need_to_drain = TRUE;
    priv->width = seq_hdr.width;
    priv->height = seq_hdr.height;
  }
  priv->display_width = priv->width;
  priv->display_height = priv->height;

  _pts_set_framerate (&priv->tsg, seq_hdr.fps_n, seq_hdr.fps_d);

  gst_mpeg2_decoder_set_latency (decoder);

  priv->state = GST_MPEG2_DECODER_STATE_GOT_SEQ_HDR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mpeg2_decoder_handle_sequence_ext (GstMpeg2Decoder * decoder,
    GstMpegVideoPacket * packet)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpegVideoSequenceExt seq_ext = { 0, };
  guint width, height;

  if (!_is_valid_state (decoder, GST_MPEG2_DECODER_STATE_GOT_SEQ_HDR)) {
    GST_ERROR_OBJECT (decoder, "no sequence before parsing sequence-extension");
    return GST_FLOW_ERROR;
  }

  if (!gst_mpeg_video_packet_parse_sequence_extension (packet, &seq_ext)) {
    GST_ERROR_OBJECT (decoder, "failed to parse sequence-extension");
    return GST_FLOW_ERROR;
  }

  if (_seq_ext_is_valid (&priv->seq_ext) &&
      memcmp (&priv->seq_ext, &seq_ext, sizeof (seq_ext)) == 0)
    return GST_FLOW_OK;

  priv->seq_ext = seq_ext;
  priv->seq_changed = TRUE;

  if (seq_ext.fps_n_ext && seq_ext.fps_d_ext) {
    guint fps_n = priv->tsg.fps_n;
    guint fps_d = priv->tsg.fps_d;
    fps_n *= seq_ext.fps_n_ext + 1;
    fps_d *= seq_ext.fps_d_ext + 1;
    _pts_set_framerate (&priv->tsg, fps_n, fps_d);
    gst_mpeg2_decoder_set_latency (decoder);
  }

  width = (priv->width & 0x0fff) | ((guint32) seq_ext.horiz_size_ext << 12);
  height = (priv->height & 0x0fff) | ((guint32) seq_ext.vert_size_ext << 12);

  if (priv->width != width || priv->height != height ||
      priv->profile != seq_ext.profile ||
      priv->progressive != seq_ext.progressive) {
    priv->need_to_drain = TRUE;
    priv->width = width;
    priv->height = height;
    priv->profile = seq_ext.profile;
    priv->progressive = seq_ext.progressive;

    GST_DEBUG_OBJECT (decoder, "video resolution %ux%u, profile %d,"
        " progressive %d", priv->width, priv->height, priv->profile,
        priv->progressive);
  }

  priv->state |= GST_MPEG2_DECODER_STATE_GOT_SEQ_EXT;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mpeg2_decoder_handle_sequence_display_ext (GstMpeg2Decoder * decoder,
    GstMpegVideoPacket * packet)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpegVideoSequenceDisplayExt seq_display_ext = { 0, };

  if (!_is_valid_state (decoder, GST_MPEG2_DECODER_STATE_GOT_SEQ_HDR)) {
    GST_ERROR_OBJECT (decoder,
        "no sequence before parsing sequence-display-extension");
    return GST_FLOW_ERROR;
  }

  if (!gst_mpeg_video_packet_parse_sequence_display_extension (packet,
          &seq_display_ext)) {
    GST_ERROR_OBJECT (decoder, "failed to parse sequence-display-extension");
    return GST_FLOW_ERROR;
  }

  if (_seq_display_ext_is_valid (&priv->seq_display_ext) &&
      memcmp (&priv->seq_display_ext, &seq_display_ext,
          sizeof (seq_display_ext)) == 0)
    return GST_FLOW_OK;

  priv->seq_display_ext = seq_display_ext;
  priv->seq_changed = TRUE;

  priv->display_width = seq_display_ext.display_horizontal_size;
  priv->display_height = seq_display_ext.display_vertical_size;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mpeg2_decoder_handle_sequence_scalable_ext (GstMpeg2Decoder * decoder,
    GstMpegVideoPacket * packet)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpegVideoSequenceScalableExt seq_scalable_ext = { 0, };

  if (!_is_valid_state (decoder, GST_MPEG2_DECODER_STATE_GOT_SEQ_HDR)) {
    GST_ERROR_OBJECT (decoder,
        "no sequence before parsing sequence-scalable-extension");
    return GST_FLOW_ERROR;
  }

  if (!gst_mpeg_video_packet_parse_sequence_scalable_extension (packet,
          &seq_scalable_ext)) {
    GST_ERROR_OBJECT (decoder, "failed to parse sequence-scalable-extension");
    return GST_FLOW_ERROR;
  }

  if (_seq_scalable_ext_is_valid (&priv->seq_scalable_ext) &&
      memcmp (&priv->seq_scalable_ext, &seq_scalable_ext,
          sizeof (seq_scalable_ext)) == 0)
    return GST_FLOW_OK;

  priv->seq_scalable_ext = seq_scalable_ext;
  priv->seq_changed = TRUE;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mpeg2_decoder_handle_quant_matrix_ext (GstMpeg2Decoder * decoder,
    GstMpegVideoPacket * packet)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpegVideoQuantMatrixExt matrix_ext = { 0, };

  if (!gst_mpeg_video_packet_parse_quant_matrix_extension (packet, &matrix_ext)) {
    GST_ERROR_OBJECT (decoder, "failed to parse sequence-scalable-extension");
    return GST_FLOW_ERROR;
  }

  priv->quant_matrix = matrix_ext;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mpeg2_decoder_handle_picture_ext (GstMpeg2Decoder * decoder,
    GstMpegVideoPacket * packet)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpegVideoPictureExt pic_ext = { {{0,},}, };

  if (!_is_valid_state (decoder,
          GST_MPEG2_DECODER_STATE_VALID_SEQ_HEADERS |
          GST_MPEG2_DECODER_STATE_GOT_PIC_HDR)) {
    GST_ERROR_OBJECT (decoder,
        "no sequence before parsing sequence-scalable-extension");
    return GST_FLOW_ERROR;
  }

  if (!gst_mpeg_video_packet_parse_picture_extension (packet, &pic_ext)) {
    GST_ERROR_OBJECT (decoder, "failed to parse picture-extension");
    return GST_FLOW_ERROR;
  }

  if (priv->progressive && !pic_ext.progressive_frame) {
    GST_WARNING_OBJECT (decoder,
        "invalid interlaced frame in progressive sequence, fixing");
    pic_ext.progressive_frame = 1;
  }

  if (pic_ext.picture_structure == 0 ||
      (pic_ext.progressive_frame &&
          pic_ext.picture_structure !=
          GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME)) {
    GST_WARNING_OBJECT (decoder,
        "invalid picture_structure %d, replacing with \"frame\"",
        pic_ext.picture_structure);
    pic_ext.picture_structure = GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME;
  }

  priv->pic_ext = pic_ext;

  priv->state |= GST_MPEG2_DECODER_STATE_GOT_PIC_EXT;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mpeg2_decoder_handle_gop (GstMpeg2Decoder * decoder,
    GstMpegVideoPacket * packet)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpegVideoGop gop = { 0, };

  if (!gst_mpeg_video_packet_parse_gop (packet, &gop)) {
    GST_ERROR_OBJECT (decoder, "failed to parse GOP");
    return GST_FLOW_ERROR;
  }

  GST_DEBUG_OBJECT (decoder,
      "GOP %02u:%02u:%02u:%02u (closed_gop %d, broken_link %d)", gop.hour,
      gop.minute, gop.second, gop.frame, gop.closed_gop, gop.broken_link);

  priv->gop = gop;

  _pts_sync (&priv->tsg, priv->current_frame->pts);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mpeg2_decoder_handle_picture (GstMpeg2Decoder * decoder,
    GstMpegVideoPacket * packet)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpegVideoPictureHdr pic_hdr = { 0, };
  GstMpeg2DecoderClass *klass = GST_MPEG2_DECODER_GET_CLASS (decoder);

  g_assert (klass->new_sequence);

  if (!_is_valid_state (decoder, GST_MPEG2_DECODER_STATE_VALID_SEQ_HEADERS)) {
    GST_ERROR_OBJECT (decoder, "no sequence before parsing picture header");
    return GST_FLOW_ERROR;
  }

  /* If need_to_drain, we must have sequence changed. */
  g_assert (priv->need_to_drain ? priv->seq_changed : TRUE);

  /* 6.1.1.6: Conversely if no sequence_xxx_extension() occurs between
     the first sequence_header() and the first picture_header() then
     sequence_xxx_extension() shall not occur in the bitstream. */
  if (priv->seq_changed) {
    GstFlowReturn ret;

    /* There are a lot of info in the mpeg2's sequence(also including ext
       display_ext and scalable_ext). We need to notify the subclass about
       its change, but not all the changes should trigger a drain(), which
       may change the output picture order. */
    if (priv->need_to_drain) {
      ret = gst_mpeg2_decoder_drain (GST_VIDEO_DECODER (decoder));
      if (ret != GST_FLOW_OK)
        return ret;

      priv->need_to_drain = FALSE;
    }

    if (klass->get_preferred_output_delay) {
      priv->preferred_output_delay =
          klass->get_preferred_output_delay (decoder, priv->is_live);
    } else {
      priv->preferred_output_delay = 0;
    }

    priv->seq_changed = FALSE;

    ret = klass->new_sequence (decoder, &priv->seq_hdr,
        _seq_ext_is_valid (&priv->seq_ext) ? &priv->seq_ext : NULL,
        _seq_display_ext_is_valid (&priv->seq_display_ext) ?
        &priv->seq_display_ext : NULL,
        _seq_scalable_ext_is_valid (&priv->seq_scalable_ext) ?
        &priv->seq_scalable_ext : NULL,
        /* previous/next 2 pictures + current picture */
        3 + priv->preferred_output_delay);

    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (decoder, "new sequence error");
      return ret;
    }
  }

  priv->state &= (GST_MPEG2_DECODER_STATE_GOT_SEQ_HDR |
      GST_MPEG2_DECODER_STATE_GOT_SEQ_EXT);

  if (!gst_mpeg_video_packet_parse_picture_header (packet, &pic_hdr)) {
    GST_ERROR_OBJECT (decoder, "failed to parse picture header");
    return GST_FLOW_ERROR;
  }

  priv->pic_hdr = pic_hdr;

  priv->state |= GST_MPEG2_DECODER_STATE_GOT_PIC_HDR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mpeg2_decoder_start_current_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Slice * slice)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpeg2DecoderClass *klass = GST_MPEG2_DECODER_GET_CLASS (decoder);
  GstMpeg2Picture *prev_picture, *next_picture;
  GstFlowReturn ret;

  /* If subclass didn't update output state at this point,
   * marking this picture as a discont and stores current input state */
  if (priv->input_state_changed) {
    gst_mpeg2_picture_set_discont_state (priv->current_picture,
        decoder->input_state);
    priv->input_state_changed = FALSE;
  }

  if (!klass->start_picture)
    return GST_FLOW_OK;

  gst_mpeg2_dpb_get_neighbours (priv->dpb, priv->current_picture,
      &prev_picture, &next_picture);

  if (priv->current_picture->type == GST_MPEG_VIDEO_PICTURE_TYPE_B
      && !prev_picture && !priv->gop.closed_gop) {
    GST_VIDEO_CODEC_FRAME_FLAG_SET (priv->current_frame,
        GST_VIDEO_CODEC_FRAME_FLAG_DECODE_ONLY);
  }

  ret = klass->start_picture (decoder, priv->current_picture, slice,
      prev_picture, next_picture);

  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (decoder, "subclass does not want to start picture");
    return ret;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mpeg2_decoder_ensure_current_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Slice * slice)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpeg2DecoderClass *klass = GST_MPEG2_DECODER_GET_CLASS (decoder);
  GstMpeg2Picture *picture = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  if (priv->current_picture) {
    g_assert (_is_valid_state (decoder, GST_MPEG2_DECODER_STATE_GOT_SLICE));
    return GST_FLOW_OK;
  }

  if (priv->progressive ||
      priv->pic_ext.picture_structure ==
      GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME) {
    g_assert (!_is_valid_state (decoder, GST_MPEG2_DECODER_STATE_GOT_SLICE));

    if (priv->first_field) {
      GST_WARNING_OBJECT (decoder, "An unmatched first field");
      gst_clear_mpeg2_picture (&priv->first_field);
    }

    picture = gst_mpeg2_picture_new ();
    if (klass->new_picture)
      ret = klass->new_picture (decoder, priv->current_frame, picture);

    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (decoder, "subclass does not want accept new picture");
      gst_mpeg2_picture_unref (picture);
      return ret;
    }

    picture->structure = GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME;
  } else {
    if (!priv->first_field) {
      picture = gst_mpeg2_picture_new ();
      if (klass->new_picture)
        ret = klass->new_picture (decoder, priv->current_frame, picture);

      if (ret != GST_FLOW_OK) {
        GST_WARNING_OBJECT (decoder,
            "subclass does not want accept new picture");
        gst_mpeg2_picture_unref (picture);
        return ret;
      }
    } else {
      picture = gst_mpeg2_picture_new ();

      if (klass->new_field_picture)
        ret = klass->new_field_picture (decoder, priv->first_field, picture);

      if (ret != GST_FLOW_OK) {
        GST_WARNING_OBJECT (decoder,
            "Subclass couldn't handle new field picture");
        gst_mpeg2_picture_unref (picture);
        return ret;
      }

      picture->first_field = gst_mpeg2_picture_ref (priv->first_field);

      /* At this moment, this picture should be interlaced */
      picture->buffer_flags |= GST_VIDEO_BUFFER_FLAG_INTERLACED;
      if (priv->pic_ext.top_field_first)
        picture->buffer_flags |= GST_VIDEO_BUFFER_FLAG_TFF;
    }

    picture->structure = priv->pic_ext.picture_structure;
  }

  picture->needed_for_output = TRUE;
  /* This allows accessing the frame from the picture. */
  GST_CODEC_PICTURE_FRAME_NUMBER (picture) =
      priv->current_frame->system_frame_number;
  picture->type = priv->pic_hdr.pic_type;
  picture->tsn = priv->pic_hdr.tsn;
  priv->current_pts =
      _pts_eval (&priv->tsg, priv->current_frame->pts, picture->tsn);
  picture->pic_order_cnt = _pts_get_poc (&priv->tsg);

  priv->current_picture = picture;
  GST_LOG_OBJECT (decoder,
      "Create new picture %p(%s), system number: %d, poc: %d,"
      " type: 0x%d, first field %p",
      picture,
      (picture->structure == GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME) ?
      "frame" : "field",
      GST_CODEC_PICTURE_FRAME_NUMBER (picture),
      picture->pic_order_cnt, picture->type, picture->first_field);

  return gst_mpeg2_decoder_start_current_picture (decoder, slice);
}

static GstFlowReturn
gst_mpeg2_decoder_finish_current_field (GstMpeg2Decoder * decoder)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpeg2DecoderClass *klass = GST_MPEG2_DECODER_GET_CLASS (decoder);
  GstFlowReturn ret;

  if (priv->current_picture == NULL)
    return GST_FLOW_OK;

  ret = klass->end_picture (decoder, priv->current_picture);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (decoder, "subclass end_picture failed");
    return ret;
  }

  if (priv->current_picture->structure !=
      GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME &&
      !priv->current_picture->first_field) {
    priv->first_field = priv->current_picture;
    priv->current_picture = NULL;
  } else {
    GST_WARNING_OBJECT (decoder, "The current picture %p is not %s, should not "
        "begin another picture. Just discard this.",
        priv->current_picture, priv->current_picture->structure ==
        GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME ?
        " a field" : "the first field");
    gst_clear_mpeg2_picture (&priv->current_picture);
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mpeg2_decoder_finish_current_picture (GstMpeg2Decoder * decoder)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpeg2DecoderClass *klass = GST_MPEG2_DECODER_GET_CLASS (decoder);
  GstFlowReturn ret;

  g_assert (priv->current_picture != NULL);

  ret = klass->end_picture (decoder, priv->current_picture);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (decoder, "subclass end_picture failed");
    return ret;
  }

  if (priv->current_picture->structure !=
      GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME &&
      !priv->current_picture->first_field) {
    priv->first_field = priv->current_picture;
    priv->current_picture = NULL;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mpeg2_decoder_handle_slice (GstMpeg2Decoder * decoder,
    GstMpegVideoPacket * packet)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpegVideoSliceHdr slice_hdr;
  GstMpeg2DecoderClass *klass = GST_MPEG2_DECODER_GET_CLASS (decoder);
  GstMpeg2Slice slice;
  GstFlowReturn ret;

  if (!_is_valid_state (decoder, GST_MPEG2_DECODER_STATE_VALID_PIC_HEADERS)) {
    GST_ERROR_OBJECT (decoder,
        "no sequence or picture header before parsing picture header");
    return GST_FLOW_ERROR;
  }

  if (!gst_mpeg_video_packet_parse_slice_header (packet, &slice_hdr,
          &priv->seq_hdr,
          _seq_scalable_ext_is_valid (&priv->seq_scalable_ext) ?
          &priv->seq_scalable_ext : NULL)) {
    GST_ERROR_OBJECT (decoder, "failed to parse slice header");
    return GST_FLOW_ERROR;
  }

  slice.header = slice_hdr;
  slice.packet = *packet;
  slice.quant_matrix = _quant_matrix_ext_is_valid (&priv->quant_matrix) ?
      &priv->quant_matrix : NULL;
  g_assert (_pic_hdr_is_valid (&priv->pic_hdr));
  slice.pic_hdr = &priv->pic_hdr;
  slice.pic_ext = _pic_hdr_ext_is_valid (&priv->pic_ext) ?
      &priv->pic_ext : NULL;
  slice.sc_offset = slice.packet.offset - 4;
  slice.size = slice.packet.size + 4;

  ret = gst_mpeg2_decoder_ensure_current_picture (decoder, &slice);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (decoder, "failed to start current picture");
    return ret;
  }

  g_assert (klass->decode_slice);
  ret = klass->decode_slice (decoder, priv->current_picture, &slice);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (decoder,
        "Subclass didn't want to decode picture %p (frame_num %d, poc %d)",
        priv->current_picture,
        GST_CODEC_PICTURE_FRAME_NUMBER (priv->current_picture),
        priv->current_picture->pic_order_cnt);
    return ret;
  }

  priv->state |= GST_MPEG2_DECODER_STATE_GOT_SLICE;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mpeg2_decoder_decode_packet (GstMpeg2Decoder * decoder,
    GstMpegVideoPacket * packet)
{
  GstMpegVideoPacketExtensionCode ext_type;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (decoder, "Parsing the packet 0x%x, size %d",
      packet->type, packet->size);
  switch (packet->type) {
    case GST_MPEG_VIDEO_PACKET_PICTURE:{
      ret = gst_mpeg2_decoder_finish_current_field (decoder);
      if (ret != GST_FLOW_OK)
        break;

      ret = gst_mpeg2_decoder_handle_picture (decoder, packet);
      break;
    }
    case GST_MPEG_VIDEO_PACKET_SEQUENCE:
      ret = gst_mpeg2_decoder_handle_sequence (decoder, packet);
      break;
    case GST_MPEG_VIDEO_PACKET_EXTENSION:
      ext_type = packet->data[packet->offset] >> 4;
      GST_LOG_OBJECT (decoder, "  Parsing the ext packet 0x%x", ext_type);
      switch (ext_type) {
        case GST_MPEG_VIDEO_PACKET_EXT_SEQUENCE:
          ret = gst_mpeg2_decoder_handle_sequence_ext (decoder, packet);
          break;
        case GST_MPEG_VIDEO_PACKET_EXT_SEQUENCE_DISPLAY:
          ret = gst_mpeg2_decoder_handle_sequence_display_ext (decoder, packet);
          break;
        case GST_MPEG_VIDEO_PACKET_EXT_SEQUENCE_SCALABLE:
          ret =
              gst_mpeg2_decoder_handle_sequence_scalable_ext (decoder, packet);
          break;
        case GST_MPEG_VIDEO_PACKET_EXT_QUANT_MATRIX:
          ret = gst_mpeg2_decoder_handle_quant_matrix_ext (decoder, packet);
          break;
        case GST_MPEG_VIDEO_PACKET_EXT_PICTURE:
          ret = gst_mpeg2_decoder_handle_picture_ext (decoder, packet);
          break;
        default:
          /* Ignore unknown start-code extensions */
          break;
      }
      break;
    case GST_MPEG_VIDEO_PACKET_SEQUENCE_END:
      break;
    case GST_MPEG_VIDEO_PACKET_GOP:
      ret = gst_mpeg2_decoder_handle_gop (decoder, packet);
      break;
    case GST_MPEG_VIDEO_PACKET_USER_DATA:
      break;
    default:
      if (packet->type >= GST_MPEG_VIDEO_PACKET_SLICE_MIN &&
          packet->type <= GST_MPEG_VIDEO_PACKET_SLICE_MAX) {
        ret = gst_mpeg2_decoder_handle_slice (decoder, packet);
        break;
      }
      GST_WARNING_OBJECT (decoder, "unsupported packet type 0x%02x, ignore",
          packet->type);
      break;
  }

  return ret;
}

static void
gst_mpeg2_decoder_do_output_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * to_output, GstFlowReturn * ret)
{
  GstVideoCodecFrame *frame = NULL;
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpeg2DecoderOutputFrame output_frame;

  g_assert (ret != NULL);

  frame =
      gst_video_decoder_get_frame (GST_VIDEO_DECODER (decoder),
      GST_CODEC_PICTURE_FRAME_NUMBER (to_output));

  if (!frame) {
    GST_ERROR_OBJECT (decoder,
        "No available codec frame with frame number %d",
        GST_CODEC_PICTURE_FRAME_NUMBER (to_output));
    UPDATE_FLOW_RETURN (ret, GST_FLOW_ERROR);

    gst_mpeg2_picture_unref (to_output);

    return;
  }

  output_frame.frame = frame;
  output_frame.picture = to_output;
  output_frame.self = decoder;
  gst_queue_array_push_tail_struct (priv->output_queue, &output_frame);
  gst_mpeg2_decoder_drain_output_queue (decoder, priv->preferred_output_delay,
      &priv->last_flow);
}

static GstFlowReturn
gst_mpeg2_decoder_output_current_picture (GstMpeg2Decoder * decoder)
{
  GstMpeg2DecoderPrivate *priv = decoder->priv;
  GstMpeg2Picture *picture = priv->current_picture;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!picture && priv->first_field) {
    GST_WARNING_OBJECT (decoder, "Missing the second field");
    picture = priv->first_field;
  }

  g_assert (picture);

  /* Update the presentation time */
  priv->current_frame->pts = priv->current_pts;

  gst_mpeg2_dpb_add (priv->dpb, picture);

  GST_LOG_OBJECT (decoder,
      "Add picture %p (frame_num %d, poc %d, type 0x%x), into DPB", picture,
      GST_CODEC_PICTURE_FRAME_NUMBER (picture), picture->pic_order_cnt,
      picture->type);

  while (gst_mpeg2_dpb_need_bump (priv->dpb)) {
    GstMpeg2Picture *to_output;

    to_output = gst_mpeg2_dpb_bump (priv->dpb);
    g_assert (to_output);

    gst_mpeg2_decoder_do_output_picture (decoder, to_output, &ret);
    if (ret != GST_FLOW_OK)
      break;
  }

  return ret;
}

static void
gst_mpeg2_decoder_clear_output_frame (GstMpeg2DecoderOutputFrame * output_frame)
{
  if (!output_frame)
    return;

  if (output_frame->frame) {
    gst_video_decoder_release_frame (GST_VIDEO_DECODER (output_frame->self),
        output_frame->frame);
    output_frame->frame = NULL;
  }

  gst_clear_mpeg2_picture (&output_frame->picture);
}

static GstFlowReturn
gst_mpeg2_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstMpeg2Decoder *self = GST_MPEG2_DECODER (decoder);
  GstMpeg2DecoderPrivate *priv = self->priv;
  GstBuffer *in_buf = frame->input_buffer;
  GstMapInfo map_info;
  GstMpegVideoPacket packet;
  GstFlowReturn ret = GST_FLOW_OK;
  guint offset;
  gboolean last_one;

  GST_LOG_OBJECT (self, "handle frame, PTS: %" GST_TIME_FORMAT
      ", DTS: %" GST_TIME_FORMAT " system frame number is %d",
      GST_TIME_ARGS (GST_BUFFER_PTS (in_buf)),
      GST_TIME_ARGS (GST_BUFFER_DTS (in_buf)), frame->system_frame_number);

  priv->state &= ~GST_MPEG2_DECODER_STATE_GOT_SLICE;
  priv->last_flow = GST_FLOW_OK;

  priv->current_frame = frame;
  gst_buffer_map (in_buf, &map_info, GST_MAP_READ);

  offset = 0;
  last_one = FALSE;
  while (gst_mpeg_video_parse (&packet, map_info.data, map_info.size, offset)) {
    /* The packet is the last one */
    if (packet.size == -1) {
      if (packet.offset < map_info.size) {
        packet.size = map_info.size - packet.offset;
        last_one = TRUE;
      } else {
        GST_WARNING_OBJECT (decoder, "Get a packet with wrong size");
        break;
      }
    }

    ret = gst_mpeg2_decoder_decode_packet (self, &packet);
    if (ret != GST_FLOW_OK) {
      gst_buffer_unmap (in_buf, &map_info);
      GST_WARNING_OBJECT (decoder, "failed to handle the packet type 0x%x",
          packet.type);
      goto failed;
    }

    if (last_one)
      break;

    offset = packet.offset;
  }

  gst_buffer_unmap (in_buf, &map_info);

  if (!priv->current_picture) {
    GST_ERROR_OBJECT (decoder, "no valid picture created");
    goto failed;
  }

  ret = gst_mpeg2_decoder_finish_current_picture (self);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (decoder, "failed to decode the current picture");
    goto failed;
  }

  ret = gst_mpeg2_decoder_output_current_picture (self);
  gst_clear_mpeg2_picture (&priv->current_picture);
  gst_clear_mpeg2_picture (&priv->first_field);
  gst_video_codec_frame_unref (priv->current_frame);
  priv->current_frame = NULL;

  if (priv->last_flow != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (self,
        "Last flow %s", gst_flow_get_name (priv->last_flow));
    return priv->last_flow;
  }

  if (ret == GST_FLOW_ERROR) {
    GST_VIDEO_DECODER_ERROR (self, 1, STREAM, DECODE,
        ("Failed to decode data"), (NULL), ret);
  }

  return ret;

failed:
  {
    if (ret == GST_FLOW_ERROR) {
      GST_VIDEO_DECODER_ERROR (decoder, 1, STREAM, DECODE,
          ("failed to handle the frame %d", frame->system_frame_number), (NULL),
          ret);
    }

    gst_video_decoder_release_frame (decoder, frame);
    gst_clear_mpeg2_picture (&priv->current_picture);
    gst_clear_mpeg2_picture (&priv->first_field);
    priv->current_frame = NULL;

    return ret;
  }
}

static void
gst_mpeg2_decoder_drain_output_queue (GstMpeg2Decoder * self, guint num,
    GstFlowReturn * ret)
{
  GstMpeg2DecoderPrivate *priv = self->priv;
  GstMpeg2DecoderClass *klass = GST_MPEG2_DECODER_GET_CLASS (self);
  GstFlowReturn flow_ret;

  g_assert (klass->output_picture);

  while (gst_queue_array_get_length (priv->output_queue) > num) {
    GstMpeg2DecoderOutputFrame *output_frame = (GstMpeg2DecoderOutputFrame *)
        gst_queue_array_pop_head_struct (priv->output_queue);
    GST_LOG_OBJECT (self,
        "Output picture %p (frame_num %d, poc %d, pts: %" GST_TIME_FORMAT
        "), from DPB",
        output_frame->picture,
        GST_CODEC_PICTURE_FRAME_NUMBER (output_frame->picture),
        output_frame->picture->pic_order_cnt,
        GST_TIME_ARGS (output_frame->frame->pts));

    flow_ret =
        klass->output_picture (self, output_frame->frame,
        output_frame->picture);

    UPDATE_FLOW_RETURN (ret, flow_ret);
  }
}
