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

#include "gstdxvavp9decoder.h"
#include <string.h>
#include <vector>

#include "gstdxvatypedef.h"

GST_DEBUG_CATEGORY_STATIC (gst_dxva_vp9_decoder_debug);
#define GST_CAT_DEFAULT gst_dxva_vp9_decoder_debug

/* *INDENT-OFF* */
struct _GstDxvaVp9DecoderPrivate
{
  DXVA_PicParams_VP9 pic_params;
  DXVA_Slice_VPx_Short slice;

  std::vector<guint8> bitstream_buffer;
  GPtrArray *ref_pics = nullptr;

  /* To calculate use_prev_in_find_mv_refs */
  guint last_frame_width = 0;
  guint last_frame_height = 0;
  gboolean last_show_frame = FALSE;
};
/* *INDENT-ON* */

static void gst_dxva_vp9_decoder_finalize (GObject * object);

static gboolean gst_dxva_vp9_decoder_start (GstVideoDecoder * decoder);

static GstFlowReturn gst_dxva_vp9_decoder_new_sequence (GstVp9Decoder * decoder,
    const GstVp9FrameHeader * frame_hdr, gint max_dpb_size);
static GstFlowReturn gst_dxva_vp9_decoder_new_picture (GstVp9Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp9Picture * picture);
static GstVp9Picture *gst_dxva_vp9_decoder_duplicate_picture (GstVp9Decoder *
    decoder, GstVideoCodecFrame * frame, GstVp9Picture * picture);
static GstFlowReturn
gst_dxva_vp9_decoder_decode_picture (GstVp9Decoder * decoder,
    GstVp9Picture * picture, GstVp9Dpb * dpb);
static GstFlowReturn gst_dxva_vp9_decoder_end_picture (GstVp9Decoder * decoder,
    GstVp9Picture * picture);
static GstFlowReturn gst_dxva_vp9_decoder_output_picture (GstVp9Decoder *
    decoder, GstVideoCodecFrame * frame, GstVp9Picture * picture);

#define gst_dxva_vp9_decoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstDxvaVp9Decoder,
    gst_dxva_vp9_decoder, GST_TYPE_VP9_DECODER,
    GST_DEBUG_CATEGORY_INIT (gst_dxva_vp9_decoder_debug, "dxvavp9decoder",
        0, "dxvavp9decoder"));

static void
gst_dxva_vp9_decoder_class_init (GstDxvaVp9DecoderClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstVp9DecoderClass *vp9decoder_class = GST_VP9_DECODER_CLASS (klass);

  object_class->finalize = gst_dxva_vp9_decoder_finalize;

  decoder_class->start = GST_DEBUG_FUNCPTR (gst_dxva_vp9_decoder_start);

  vp9decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_dxva_vp9_decoder_new_sequence);
  vp9decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_vp9_decoder_new_picture);
  vp9decoder_class->duplicate_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_vp9_decoder_duplicate_picture);
  vp9decoder_class->decode_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_vp9_decoder_decode_picture);
  vp9decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_vp9_decoder_end_picture);
  vp9decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_vp9_decoder_output_picture);
}

static void
gst_dxva_vp9_decoder_init (GstDxvaVp9Decoder * self)
{
  self->priv = new GstDxvaVp9DecoderPrivate ();
  self->priv->ref_pics = g_ptr_array_new ();
}

static void
gst_dxva_vp9_decoder_finalize (GObject * object)
{
  GstDxvaVp9Decoder *self = GST_DXVA_VP9_DECODER (object);
  GstDxvaVp9DecoderPrivate *priv = self->priv;

  g_ptr_array_unref (priv->ref_pics);
  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dxva_vp9_decoder_reset (GstDxvaVp9Decoder * self)
{
  GstDxvaVp9DecoderPrivate *priv = self->priv;

  priv->last_frame_width = 0;
  priv->last_frame_height = 0;
  priv->last_show_frame = FALSE;
}

static gboolean
gst_dxva_vp9_decoder_start (GstVideoDecoder * decoder)
{
  GstDxvaVp9Decoder *self = GST_DXVA_VP9_DECODER (decoder);

  gst_dxva_vp9_decoder_reset (self);

  return GST_VIDEO_DECODER_CLASS (parent_class)->start (decoder);
}

static GstFlowReturn
gst_dxva_vp9_decoder_new_sequence (GstVp9Decoder * decoder,
    const GstVp9FrameHeader * frame_hdr, gint max_dpb_size)
{
  GstDxvaVp9Decoder *self = GST_DXVA_VP9_DECODER (decoder);
  GstDxvaVp9DecoderPrivate *priv = self->priv;
  GstDxvaVp9DecoderClass *klass = GST_DXVA_VP9_DECODER_GET_CLASS (self);
  GstVideoInfo info;
  GstVideoFormat out_format = GST_VIDEO_FORMAT_UNKNOWN;
  GstFlowReturn ret;

  GST_LOG_OBJECT (self, "new sequence");

  if (frame_hdr->profile == GST_VP9_PROFILE_0)
    out_format = GST_VIDEO_FORMAT_NV12;
  else if (frame_hdr->profile == GST_VP9_PROFILE_2)
    out_format = GST_VIDEO_FORMAT_P010_10LE;

  if (out_format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ERROR_OBJECT (self, "Could not support profile %d", frame_hdr->profile);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  /* Will be updated per decode_picture */
  priv->last_frame_width = priv->last_frame_height = 0;
  priv->last_show_frame = FALSE;

  gst_video_info_set_format (&info,
      out_format, frame_hdr->width, frame_hdr->height);

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
gst_dxva_vp9_decoder_new_picture (GstVp9Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp9Picture * picture)
{
  GstDxvaVp9Decoder *self = GST_DXVA_VP9_DECODER (decoder);
  GstDxvaVp9DecoderClass *klass = GST_DXVA_VP9_DECODER_GET_CLASS (self);

  g_assert (klass->new_picture);

  return klass->new_picture (self, GST_CODEC_PICTURE (picture));
}

static GstVp9Picture *
gst_dxva_vp9_decoder_duplicate_picture (GstVp9Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp9Picture * picture)
{
  GstDxvaVp9Decoder *self = GST_DXVA_VP9_DECODER (decoder);
  GstDxvaVp9DecoderClass *klass = GST_DXVA_VP9_DECODER_GET_CLASS (self);
  GstVp9Picture *new_picture;

  g_assert (klass->duplicate_picture);

  new_picture = gst_vp9_picture_new ();
  new_picture->frame_hdr = picture->frame_hdr;

  if (klass->duplicate_picture (self, GST_CODEC_PICTURE (picture),
          GST_CODEC_PICTURE (new_picture)) != GST_FLOW_OK) {
    gst_vp9_picture_unref (new_picture);
    return nullptr;
  }

  return new_picture;
}

static void
gst_dxva_vp9_decoder_copy_frame_params (GstDxvaVp9Decoder * self,
    GstVp9Picture * picture, DXVA_PicParams_VP9 * params)
{
  const GstVp9FrameHeader *frame_hdr = &picture->frame_hdr;

  params->profile = frame_hdr->profile;
  params->frame_type = frame_hdr->frame_type;
  params->show_frame = frame_hdr->show_frame;
  params->error_resilient_mode = frame_hdr->error_resilient_mode;
  params->subsampling_x = frame_hdr->subsampling_x;
  params->subsampling_y = frame_hdr->subsampling_y;
  params->refresh_frame_context = frame_hdr->refresh_frame_context;
  params->frame_parallel_decoding_mode =
      frame_hdr->frame_parallel_decoding_mode;
  params->intra_only = frame_hdr->intra_only;
  params->frame_context_idx = frame_hdr->frame_context_idx;
  params->reset_frame_context = frame_hdr->reset_frame_context;
  if (frame_hdr->frame_type == GST_VP9_KEY_FRAME)
    params->allow_high_precision_mv = 0;
  else
    params->allow_high_precision_mv = frame_hdr->allow_high_precision_mv;

  params->width = frame_hdr->width;
  params->height = frame_hdr->height;
  params->BitDepthMinus8Luma = frame_hdr->bit_depth - 8;
  params->BitDepthMinus8Chroma = frame_hdr->bit_depth - 8;

  params->interp_filter = frame_hdr->interpolation_filter;
  params->log2_tile_cols = frame_hdr->tile_cols_log2;
  params->log2_tile_rows = frame_hdr->tile_rows_log2;
}

static void
gst_dxva_vp9_decoder_copy_reference_frames (GstDxvaVp9Decoder * self,
    GstVp9Picture * picture, GstVp9Dpb * dpb, DXVA_PicParams_VP9 * params)
{
  GstDxvaVp9DecoderPrivate *priv = self->priv;
  GstDxvaVp9DecoderClass *klass = GST_DXVA_VP9_DECODER_GET_CLASS (self);

  for (guint i = 0; i < GST_VP9_REF_FRAMES; i++) {
    params->ref_frame_map[i].bPicEntry = 0xff;
    params->ref_frame_coded_width[i] = 0;
    params->ref_frame_coded_height[i] = 0;

    if (dpb->pic_list[i]) {
      GstVp9Picture *other = dpb->pic_list[i];
      guint8 id = klass->get_picture_id (self, GST_CODEC_PICTURE (other));

      if (id != 0xff) {
        params->ref_frame_map[i].Index7Bits = id;
        params->ref_frame_coded_width[i] = other->frame_hdr.width;
        params->ref_frame_coded_height[i] = other->frame_hdr.height;
        g_ptr_array_add (priv->ref_pics, other);
      }
    }
  }
}

static void
gst_dxva_vp9_decoder_copy_frame_refs (GstDxvaVp9Decoder * self,
    GstVp9Picture * picture, DXVA_PicParams_VP9 * params)
{
  const GstVp9FrameHeader *frame_hdr = &picture->frame_hdr;
  gint i;

  for (i = 0; i < GST_VP9_REFS_PER_FRAME; i++)
    params->frame_refs[i] = params->ref_frame_map[frame_hdr->ref_frame_idx[i]];

  G_STATIC_ASSERT (G_N_ELEMENTS (params->ref_frame_sign_bias) ==
      G_N_ELEMENTS (frame_hdr->ref_frame_sign_bias));
  G_STATIC_ASSERT (sizeof (params->ref_frame_sign_bias) ==
      sizeof (frame_hdr->ref_frame_sign_bias));
  memcpy (params->ref_frame_sign_bias,
      frame_hdr->ref_frame_sign_bias, sizeof (frame_hdr->ref_frame_sign_bias));
}

static void
gst_dxva_vp9_decoder_copy_loop_filter_params (GstDxvaVp9Decoder * self,
    GstVp9Picture * picture, DXVA_PicParams_VP9 * params)
{
  GstDxvaVp9DecoderPrivate *priv = self->priv;
  const GstVp9FrameHeader *frame_hdr = &picture->frame_hdr;
  const GstVp9LoopFilterParams *lfp = &frame_hdr->loop_filter_params;

  params->filter_level = lfp->loop_filter_level;
  params->sharpness_level = lfp->loop_filter_sharpness;
  params->mode_ref_delta_enabled = lfp->loop_filter_delta_enabled;
  params->mode_ref_delta_update = lfp->loop_filter_delta_update;
  params->use_prev_in_find_mv_refs =
      priv->last_show_frame && !frame_hdr->error_resilient_mode;

  if (frame_hdr->frame_type != GST_VP9_KEY_FRAME && !frame_hdr->intra_only) {
    params->use_prev_in_find_mv_refs &=
        (frame_hdr->width == priv->last_frame_width &&
        frame_hdr->height == priv->last_frame_height);
  }

  G_STATIC_ASSERT (G_N_ELEMENTS (params->ref_deltas) ==
      G_N_ELEMENTS (lfp->loop_filter_ref_deltas));
  G_STATIC_ASSERT (sizeof (params->ref_deltas) ==
      sizeof (lfp->loop_filter_ref_deltas));
  memcpy (params->ref_deltas, lfp->loop_filter_ref_deltas,
      sizeof (lfp->loop_filter_ref_deltas));

  G_STATIC_ASSERT (G_N_ELEMENTS (params->mode_deltas) ==
      G_N_ELEMENTS (lfp->loop_filter_mode_deltas));
  G_STATIC_ASSERT (sizeof (params->mode_deltas) ==
      sizeof (lfp->loop_filter_mode_deltas));
  memcpy (params->mode_deltas, lfp->loop_filter_mode_deltas,
      sizeof (lfp->loop_filter_mode_deltas));
}

static void
gst_dxva_vp9_decoder_copy_quant_params (GstDxvaVp9Decoder * self,
    GstVp9Picture * picture, DXVA_PicParams_VP9 * params)
{
  const GstVp9FrameHeader *frame_hdr = &picture->frame_hdr;
  const GstVp9QuantizationParams *qp = &frame_hdr->quantization_params;

  params->base_qindex = qp->base_q_idx;
  params->y_dc_delta_q = qp->delta_q_y_dc;
  params->uv_dc_delta_q = qp->delta_q_uv_dc;
  params->uv_ac_delta_q = qp->delta_q_uv_ac;
}

static void
gst_dxva_vp9_decoder_copy_segmentation_params (GstDxvaVp9Decoder * self,
    GstVp9Picture * picture, DXVA_PicParams_VP9 * params)
{
  const GstVp9FrameHeader *frame_hdr = &picture->frame_hdr;
  const GstVp9SegmentationParams *sp = &frame_hdr->segmentation_params;

  params->stVP9Segments.enabled = sp->segmentation_enabled;
  params->stVP9Segments.update_map = sp->segmentation_update_map;
  params->stVP9Segments.temporal_update = sp->segmentation_temporal_update;
  params->stVP9Segments.abs_delta = sp->segmentation_abs_or_delta_update;

  G_STATIC_ASSERT (G_N_ELEMENTS (params->stVP9Segments.tree_probs) ==
      G_N_ELEMENTS (sp->segmentation_tree_probs));
  G_STATIC_ASSERT (sizeof (params->stVP9Segments.tree_probs) ==
      sizeof (sp->segmentation_tree_probs));
  memcpy (params->stVP9Segments.tree_probs, sp->segmentation_tree_probs,
      sizeof (sp->segmentation_tree_probs));

  G_STATIC_ASSERT (G_N_ELEMENTS (params->stVP9Segments.pred_probs) ==
      G_N_ELEMENTS (sp->segmentation_pred_prob));
  G_STATIC_ASSERT (sizeof (params->stVP9Segments.pred_probs) ==
      sizeof (sp->segmentation_pred_prob));

  if (sp->segmentation_temporal_update) {
    memcpy (params->stVP9Segments.pred_probs, sp->segmentation_pred_prob,
        sizeof (params->stVP9Segments.pred_probs));
  } else {
    memset (params->stVP9Segments.pred_probs, 255,
        sizeof (params->stVP9Segments.pred_probs));
  }

  for (guint i = 0; i < GST_VP9_MAX_SEGMENTS; i++) {
    params->stVP9Segments.feature_mask[i] =
        (sp->feature_enabled[i][GST_VP9_SEG_LVL_ALT_Q] << 0) |
        (sp->feature_enabled[i][GST_VP9_SEG_LVL_ALT_L] << 1) |
        (sp->feature_enabled[i][GST_VP9_SEG_LVL_REF_FRAME] << 2) |
        (sp->feature_enabled[i][GST_VP9_SEG_SEG_LVL_SKIP] << 3);

    for (guint j = 0; j < 3; j++)
      params->stVP9Segments.feature_data[i][j] = sp->feature_data[i][j];
    params->stVP9Segments.feature_data[i][3] = 0;
  }
}

static GstFlowReturn
gst_dxva_vp9_decoder_decode_picture (GstVp9Decoder * decoder,
    GstVp9Picture * picture, GstVp9Dpb * dpb)
{
  GstDxvaVp9Decoder *self = GST_DXVA_VP9_DECODER (decoder);
  GstDxvaVp9DecoderPrivate *priv = self->priv;
  GstDxvaVp9DecoderClass *klass = GST_DXVA_VP9_DECODER_GET_CLASS (self);
  DXVA_PicParams_VP9 *pic_params = &priv->pic_params;
  DXVA_Slice_VPx_Short *slice = &priv->slice;
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

  memset (pic_params, 0, sizeof (DXVA_PicParams_VP9));

  pic_params->CurrPic.Index7Bits = picture_id;
  pic_params->uncompressed_header_size_byte_aligned =
      picture->frame_hdr.frame_header_length_in_bytes;
  pic_params->first_partition_size = picture->frame_hdr.header_size_in_bytes;
  pic_params->StatusReportFeedbackNumber = 1;

  gst_dxva_vp9_decoder_copy_reference_frames (self, picture, dpb, pic_params);
  gst_dxva_vp9_decoder_copy_frame_params (self, picture, pic_params);
  gst_dxva_vp9_decoder_copy_frame_refs (self, picture, pic_params);
  gst_dxva_vp9_decoder_copy_loop_filter_params (self, picture, pic_params);
  gst_dxva_vp9_decoder_copy_quant_params (self, picture, pic_params);
  gst_dxva_vp9_decoder_copy_segmentation_params (self, picture, pic_params);

  priv->bitstream_buffer.resize (picture->size);
  memcpy (&priv->bitstream_buffer[0], picture->data, picture->size);

  slice->BSNALunitDataLocation = 0;
  slice->SliceBytesInBuffer = priv->bitstream_buffer.size ();
  slice->wBadSliceChopping = 0;

  priv->last_frame_width = pic_params->width;
  priv->last_frame_height = pic_params->height;
  priv->last_show_frame = pic_params->show_frame;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_dxva_vp9_decoder_end_picture (GstVp9Decoder * decoder,
    GstVp9Picture * picture)
{
  GstDxvaVp9Decoder *self = GST_DXVA_VP9_DECODER (decoder);
  GstDxvaVp9DecoderPrivate *priv = self->priv;
  GstDxvaVp9DecoderClass *klass = GST_DXVA_VP9_DECODER_GET_CLASS (self);
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
  args.picture_params_size = sizeof (DXVA_PicParams_VP9);
  args.slice_control = &priv->slice;
  args.slice_control_size = sizeof (DXVA_Slice_VPx_Short);
  args.bitstream = &priv->bitstream_buffer[0];
  args.bitstream_size = priv->bitstream_buffer.size ();

  g_assert (klass->end_picture);

  return klass->end_picture (self, GST_CODEC_PICTURE (picture),
      priv->ref_pics, &args);
}

static GstFlowReturn
gst_dxva_vp9_decoder_output_picture (GstVp9Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp9Picture * picture)
{
  GstDxvaVp9Decoder *self = GST_DXVA_VP9_DECODER (decoder);
  GstDxvaVp9DecoderClass *klass = GST_DXVA_VP9_DECODER_GET_CLASS (self);

  g_assert (klass->output_picture);

  GST_LOG_OBJECT (self, "Outputting picture %p", picture);

  return klass->output_picture (self, frame, GST_CODEC_PICTURE (picture),
      (GstVideoBufferFlags) 0, picture->frame_hdr.width,
      picture->frame_hdr.height);
}
