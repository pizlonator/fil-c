/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstdxvavp8decoder.h"
#include <string.h>
#include <vector>

#include "gstdxvatypedef.h"

GST_DEBUG_CATEGORY_STATIC (gst_dxva_vp8_decoder_debug);
#define GST_CAT_DEFAULT gst_dxva_vp8_decoder_debug

/* *INDENT-OFF* */
struct _GstDxvaVp8DecoderPrivate
{
  DXVA_PicParams_VP8 pic_params;
  DXVA_Slice_VPx_Short slice;

  std::vector<guint8> bitstream_buffer;
  GPtrArray *ref_pics = nullptr;

  gint width = 0;
  gint height = 0;
};
/* *INDENT-ON* */

static void gst_dxva_vp8_decoder_finalize (GObject * object);

static gboolean gst_dxva_vp8_decoder_start (GstVideoDecoder * decoder);

static GstFlowReturn gst_dxva_vp8_decoder_new_sequence (GstVp8Decoder * decoder,
    const GstVp8FrameHdr * frame_hdr, gint max_dpb_size);
static GstFlowReturn gst_dxva_vp8_decoder_new_picture (GstVp8Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp8Picture * picture);
static GstFlowReturn
gst_dxva_vp8_decoder_decode_picture (GstVp8Decoder * decoder,
    GstVp8Picture * picture, GstVp8Parser * parser);
static GstFlowReturn gst_dxva_vp8_decoder_end_picture (GstVp8Decoder * decoder,
    GstVp8Picture * picture);
static GstFlowReturn gst_dxva_vp8_decoder_output_picture (GstVp8Decoder *
    decoder, GstVideoCodecFrame * frame, GstVp8Picture * picture);

#define gst_dxva_vp8_decoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstDxvaVp8Decoder,
    gst_dxva_vp8_decoder, GST_TYPE_VP8_DECODER,
    GST_DEBUG_CATEGORY_INIT (gst_dxva_vp8_decoder_debug, "dxvavp8decoder",
        0, "dxvavp8decoder"));

static void
gst_dxva_vp8_decoder_class_init (GstDxvaVp8DecoderClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstVp8DecoderClass *vp8decoder_class = GST_VP8_DECODER_CLASS (klass);

  object_class->finalize = gst_dxva_vp8_decoder_finalize;

  decoder_class->start = GST_DEBUG_FUNCPTR (gst_dxva_vp8_decoder_start);

  vp8decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_dxva_vp8_decoder_new_sequence);
  vp8decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_vp8_decoder_new_picture);
  vp8decoder_class->decode_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_vp8_decoder_decode_picture);
  vp8decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_vp8_decoder_end_picture);
  vp8decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_vp8_decoder_output_picture);
}

static void
gst_dxva_vp8_decoder_init (GstDxvaVp8Decoder * self)
{
  self->priv = new GstDxvaVp8DecoderPrivate ();
  self->priv->ref_pics = g_ptr_array_new ();
}

static void
gst_dxva_vp8_decoder_finalize (GObject * object)
{
  GstDxvaVp8Decoder *self = GST_DXVA_VP8_DECODER (object);
  GstDxvaVp8DecoderPrivate *priv = self->priv;

  g_ptr_array_unref (priv->ref_pics);
  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dxva_vp8_decoder_reset (GstDxvaVp8Decoder * self)
{
  GstDxvaVp8DecoderPrivate *priv = self->priv;

  priv->width = 0;
  priv->height = 0;
}

static gboolean
gst_dxva_vp8_decoder_start (GstVideoDecoder * decoder)
{
  GstDxvaVp8Decoder *self = GST_DXVA_VP8_DECODER (decoder);

  gst_dxva_vp8_decoder_reset (self);

  return GST_VIDEO_DECODER_CLASS (parent_class)->start (decoder);
}

static GstFlowReturn
gst_dxva_vp8_decoder_new_sequence (GstVp8Decoder * decoder,
    const GstVp8FrameHdr * frame_hdr, gint max_dpb_size)
{
  GstDxvaVp8Decoder *self = GST_DXVA_VP8_DECODER (decoder);
  GstDxvaVp8DecoderPrivate *priv = self->priv;
  GstDxvaVp8DecoderClass *klass = GST_DXVA_VP8_DECODER_GET_CLASS (self);
  GstVideoInfo info;
  GstFlowReturn ret;

  GST_LOG_OBJECT (self, "new sequence");

  priv->width = frame_hdr->width;
  priv->height = frame_hdr->height;

  gst_video_info_set_format (&info,
      GST_VIDEO_FORMAT_NV12, frame_hdr->width, frame_hdr->height);

  g_assert (klass->configure);

  ret = klass->configure (self, decoder->input_state, &info, 0, 0,
      frame_hdr->width, frame_hdr->height, max_dpb_size);

  if (ret == GST_FLOW_OK &&
      !gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
    GST_WARNING_OBJECT (self, "Couldn't negotiate with new sequence");
    ret = GST_FLOW_NOT_NEGOTIATED;
  }

  return ret;
}

static GstFlowReturn
gst_dxva_vp8_decoder_new_picture (GstVp8Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp8Picture * picture)
{
  GstDxvaVp8Decoder *self = GST_DXVA_VP8_DECODER (decoder);
  GstDxvaVp8DecoderClass *klass = GST_DXVA_VP8_DECODER_GET_CLASS (self);

  g_assert (klass->new_picture);

  return klass->new_picture (self, GST_CODEC_PICTURE (picture));
}

static void
gst_dxva_vp8_decoder_copy_frame_params (GstDxvaVp8Decoder * self,
    GstVp8Picture * picture, GstVp8Parser * parser, DXVA_PicParams_VP8 * params)
{
  const GstVp8FrameHdr *frame_hdr = &picture->frame_hdr;
  gint i;

  /* 0: keyframe, 1: inter */
  params->frame_type = !frame_hdr->key_frame;
  params->version = frame_hdr->version;
  params->show_frame = frame_hdr->show_frame;
  params->clamp_type = frame_hdr->clamping_type;

  params->filter_type = frame_hdr->filter_type;
  params->filter_level = frame_hdr->loop_filter_level;
  params->sharpness_level = frame_hdr->sharpness_level;
  params->mode_ref_lf_delta_enabled =
      parser->mb_lf_adjust.loop_filter_adj_enable;
  params->mode_ref_lf_delta_update =
      parser->mb_lf_adjust.mode_ref_lf_delta_update;
  for (i = 0; i < 4; i++) {
    params->ref_lf_deltas[i] = parser->mb_lf_adjust.ref_frame_delta[i];
    params->mode_lf_deltas[i] = parser->mb_lf_adjust.mb_mode_delta[i];
  }
  params->log2_nbr_of_dct_partitions = frame_hdr->log2_nbr_of_dct_partitions;
  params->base_qindex = frame_hdr->quant_indices.y_ac_qi;
  params->y1dc_delta_q = frame_hdr->quant_indices.y_dc_delta;
  params->y2dc_delta_q = frame_hdr->quant_indices.y2_dc_delta;
  params->y2ac_delta_q = frame_hdr->quant_indices.y2_ac_delta;
  params->uvdc_delta_q = frame_hdr->quant_indices.uv_dc_delta;
  params->uvac_delta_q = frame_hdr->quant_indices.uv_ac_delta;

  params->ref_frame_sign_bias_golden = frame_hdr->sign_bias_golden;
  params->ref_frame_sign_bias_altref = frame_hdr->sign_bias_alternate;

  params->refresh_entropy_probs = frame_hdr->refresh_entropy_probs;

  memcpy (params->vp8_coef_update_probs, frame_hdr->token_probs.prob,
      sizeof (frame_hdr->token_probs.prob));

  params->mb_no_coeff_skip = frame_hdr->mb_no_skip_coeff;
  params->prob_skip_false = frame_hdr->prob_skip_false;
  params->prob_intra = frame_hdr->prob_intra;
  params->prob_last = frame_hdr->prob_last;
  params->prob_golden = frame_hdr->prob_gf;

  memcpy (params->intra_16x16_prob, frame_hdr->mode_probs.y_prob,
      sizeof (frame_hdr->mode_probs.y_prob));
  memcpy (params->intra_chroma_prob, frame_hdr->mode_probs.uv_prob,
      sizeof (frame_hdr->mode_probs.uv_prob));
  memcpy (params->vp8_mv_update_probs, frame_hdr->mv_probs.prob,
      sizeof (frame_hdr->mv_probs.prob));
}

static gboolean
gst_dxva_vp8_decoder_copy_reference_frames (GstDxvaVp8Decoder * self,
    DXVA_PicParams_VP8 * params)
{
  GstVp8Decoder *decoder = GST_VP8_DECODER (self);
  GstDxvaVp8DecoderPrivate *priv = self->priv;
  GstDxvaVp8DecoderClass *klass = GST_DXVA_VP8_DECODER_GET_CLASS (self);
  guint8 id;

  params->alt_fb_idx.bPicEntry = 0xff;
  if (decoder->alt_ref_picture) {
    id = klass->get_picture_id (self,
        GST_CODEC_PICTURE (decoder->alt_ref_picture));
    if (id != 0xff) {
      params->alt_fb_idx.Index7Bits = id;
      g_ptr_array_add (priv->ref_pics, decoder->alt_ref_picture);
    }
  }

  params->gld_fb_idx.bPicEntry = 0xff;
  if (decoder->golden_ref_picture) {
    id = klass->get_picture_id (self,
        GST_CODEC_PICTURE (decoder->golden_ref_picture));

    if (id != 0xff) {
      params->gld_fb_idx.Index7Bits = id;
      g_ptr_array_add (priv->ref_pics, decoder->golden_ref_picture);
    }
  }

  params->lst_fb_idx.bPicEntry = 0xff;
  if (decoder->last_picture) {
    id = klass->get_picture_id (self,
        GST_CODEC_PICTURE (decoder->last_picture));

    if (id != 0xff) {
      params->lst_fb_idx.Index7Bits = id;
      g_ptr_array_add (priv->ref_pics, decoder->last_picture);
    }
  }

  return TRUE;
}

static void
gst_dxva_vp8_decoder_copy_segmentation_params (GstDxvaVp8Decoder * self,
    GstVp8Parser * parser, DXVA_PicParams_VP8 * params)
{
  const GstVp8Segmentation *seg = &parser->segmentation;
  gint i;

  params->stVP8Segments.segmentation_enabled = seg->segmentation_enabled;
  params->stVP8Segments.update_mb_segmentation_map =
      seg->update_mb_segmentation_map;
  params->stVP8Segments.update_mb_segmentation_data =
      seg->update_segment_feature_data;
  params->stVP8Segments.mb_segement_abs_delta = seg->segment_feature_mode;

  for (i = 0; i < 4; i++) {
    params->stVP8Segments.segment_feature_data[0][i] =
        seg->quantizer_update_value[i];
  }

  for (i = 0; i < 4; i++) {
    params->stVP8Segments.segment_feature_data[1][i] = seg->lf_update_value[i];
  }

  for (i = 0; i < 3; i++) {
    params->stVP8Segments.mb_segment_tree_probs[i] = seg->segment_prob[i];
  }
}

static GstFlowReturn
gst_dxva_vp8_decoder_decode_picture (GstVp8Decoder * decoder,
    GstVp8Picture * picture, GstVp8Parser * parser)
{
  GstDxvaVp8Decoder *self = GST_DXVA_VP8_DECODER (decoder);
  GstDxvaVp8DecoderPrivate *priv = self->priv;
  GstDxvaVp8DecoderClass *klass = GST_DXVA_VP8_DECODER_GET_CLASS (self);
  DXVA_PicParams_VP8 *pic_params = &priv->pic_params;
  DXVA_Slice_VPx_Short *slice = &priv->slice;
  const GstVp8FrameHdr *frame_hdr = &picture->frame_hdr;
  GstCodecPicture *codec_picture = GST_CODEC_PICTURE (picture);
  GstFlowReturn ret;
  guint8 picture_id;

  g_assert (klass->start_picture);
  g_assert (klass->get_picture_id);

  ret = klass->start_picture (self, codec_picture, &picture_id);
  if (ret != GST_FLOW_OK)
    return ret;

  priv->bitstream_buffer.resize (0);
  g_ptr_array_set_size (priv->ref_pics, 0);

  memset (pic_params, 0, sizeof (DXVA_PicParams_VP8));

  pic_params->first_part_size = frame_hdr->first_part_size;
  pic_params->width = priv->width;
  pic_params->height = priv->height;
  pic_params->CurrPic.Index7Bits = picture_id;
  pic_params->StatusReportFeedbackNumber = 1;

  if (!gst_dxva_vp8_decoder_copy_reference_frames (self, pic_params))
    return GST_FLOW_ERROR;

  gst_dxva_vp8_decoder_copy_frame_params (self, picture, parser, pic_params);
  gst_dxva_vp8_decoder_copy_segmentation_params (self, parser, pic_params);

  priv->bitstream_buffer.resize (picture->size);
  memcpy (&priv->bitstream_buffer[0], picture->data, picture->size);

  slice->BSNALunitDataLocation = 0;
  slice->SliceBytesInBuffer = priv->bitstream_buffer.size ();
  slice->wBadSliceChopping = 0;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_dxva_vp8_decoder_end_picture (GstVp8Decoder * decoder,
    GstVp8Picture * picture)
{
  GstDxvaVp8Decoder *self = GST_DXVA_VP8_DECODER (decoder);
  GstDxvaVp8DecoderPrivate *priv = self->priv;
  GstDxvaVp8DecoderClass *klass = GST_DXVA_VP8_DECODER_GET_CLASS (self);
  size_t bitstream_buffer_size;
  size_t bitstream_pos;
  GstDxvaDecodingArgs args;

  if (priv->bitstream_buffer.empty ()) {
    GST_ERROR_OBJECT (self, "No bitstream buffer to submit");
    return GST_FLOW_ERROR;
  }

  memset (&args, 0, sizeof (GstDxvaDecodingArgs));

  bitstream_pos = priv->bitstream_buffer.size ();
  bitstream_buffer_size = GST_ROUND_UP_128 (bitstream_pos);

  if (bitstream_buffer_size > bitstream_pos) {
    size_t padding = bitstream_buffer_size - bitstream_pos;

    /* As per DXVA spec, total amount of bitstream buffer size should be
     * 128 bytes aligned. If actual data is not multiple of 128 bytes,
     * the last slice data needs to be zero-padded */
    priv->bitstream_buffer.resize (bitstream_buffer_size, 0);

    priv->slice.SliceBytesInBuffer += padding;
  }

  args.picture_params = &priv->pic_params;
  args.picture_params_size = sizeof (DXVA_PicParams_VP8);
  args.slice_control = &priv->slice;
  args.slice_control_size = sizeof (DXVA_Slice_VPx_Short);
  args.bitstream = &priv->bitstream_buffer[0];
  args.bitstream_size = priv->bitstream_buffer.size ();

  g_assert (klass->end_picture);

  return klass->end_picture (self, GST_CODEC_PICTURE (picture),
      priv->ref_pics, &args);
}

static GstFlowReturn
gst_dxva_vp8_decoder_output_picture (GstVp8Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp8Picture * picture)
{
  GstDxvaVp8Decoder *self = GST_DXVA_VP8_DECODER (decoder);
  GstDxvaVp8DecoderPrivate *priv = self->priv;
  GstDxvaVp8DecoderClass *klass = GST_DXVA_VP8_DECODER_GET_CLASS (self);

  g_assert (klass->output_picture);

  GST_LOG_OBJECT (self, "Outputting picture %p", picture);

  return klass->output_picture (self, frame, GST_CODEC_PICTURE (picture),
      (GstVideoBufferFlags) 0, priv->width, priv->height);
}
