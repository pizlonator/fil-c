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

#include "gstdxvah265decoder.h"
#include <string.h>
#include <vector>

#include "gstdxvatypedef.h"

GST_DEBUG_CATEGORY_STATIC (gst_dxva_h265_decoder_debug);
#define GST_CAT_DEFAULT gst_dxva_h265_decoder_debug

/* *INDENT-OFF* */
struct _GstDxvaH265DecoderPrivate
{
  DXVA_PicParams_HEVC pic_params;
  DXVA_Qmatrix_HEVC iq_matrix;

  std::vector<DXVA_Slice_HEVC_Short> slice_list;
  std::vector<guint8> bitstream_buffer;
  GPtrArray *ref_pics = nullptr;

  gboolean submit_iq_data;

  gint crop_x = 0;
  gint crop_y = 0;
  gint width = 0;
  gint height = 0;
  gint coded_width = 0;
  gint coded_height = 0;
  gint bitdepth = 0;
  guint8 chroma_format_idc = 0;
  GstVideoFormat out_format = GST_VIDEO_FORMAT_UNKNOWN;
  GstVideoInterlaceMode interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
  gint max_dpb_size = 0;

  gboolean configured;
};
/* *INDENT-ON* */

static void gst_dxva_h265_decoder_finalize (GObject * object);

static gboolean gst_dxva_h265_decoder_start (GstVideoDecoder * decoder);

static GstFlowReturn
gst_dxva_h265_decoder_new_sequence (GstH265Decoder * decoder,
    const GstH265SPS * sps, gint max_dpb_size);
static GstFlowReturn
gst_dxva_h265_decoder_new_picture (GstH265Decoder * decoder,
    GstVideoCodecFrame * frame, GstH265Picture * picture);
static GstFlowReturn
gst_dxva_h265_decoder_start_picture (GstH265Decoder * decoder,
    GstH265Picture * picture, GstH265Slice * slice, GstH265Dpb * dpb);
static GstFlowReturn
gst_dxva_h265_decoder_decode_slice (GstH265Decoder * decoder,
    GstH265Picture * picture, GstH265Slice * slice,
    GArray * ref_pic_list0, GArray * ref_pic_list1);
static GstFlowReturn
gst_dxva_h265_decoder_end_picture (GstH265Decoder * decoder,
    GstH265Picture * picture);
static GstFlowReturn
gst_dxva_h265_decoder_output_picture (GstH265Decoder *
    decoder, GstVideoCodecFrame * frame, GstH265Picture * picture);

#define gst_dxva_h265_decoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstDxvaH265Decoder,
    gst_dxva_h265_decoder, GST_TYPE_H265_DECODER,
    GST_DEBUG_CATEGORY_INIT (gst_dxva_h265_decoder_debug, "dxvah265decoder",
        0, "dxvah265decoder"));

static void
gst_dxva_h265_decoder_class_init (GstDxvaH265DecoderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstH265DecoderClass *h265decoder_class = GST_H265_DECODER_CLASS (klass);

  gobject_class->finalize = gst_dxva_h265_decoder_finalize;

  decoder_class->start = GST_DEBUG_FUNCPTR (gst_dxva_h265_decoder_start);

  h265decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_dxva_h265_decoder_new_sequence);
  h265decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_h265_decoder_new_picture);
  h265decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_h265_decoder_start_picture);
  h265decoder_class->decode_slice =
      GST_DEBUG_FUNCPTR (gst_dxva_h265_decoder_decode_slice);
  h265decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_h265_decoder_end_picture);
  h265decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_h265_decoder_output_picture);
}

static void
gst_dxva_h265_decoder_init (GstDxvaH265Decoder * self)
{
  self->priv = new GstDxvaH265DecoderPrivate ();
  self->priv->ref_pics = g_ptr_array_new ();
}

static void
gst_dxva_h265_decoder_finalize (GObject * object)
{
  GstDxvaH265Decoder *self = GST_DXVA_H265_DECODER (object);
  GstDxvaH265DecoderPrivate *priv = self->priv;

  g_ptr_array_unref (priv->ref_pics);
  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dxva_h265_decoder_reset (GstDxvaH265Decoder * self)
{
  GstDxvaH265DecoderPrivate *priv = self->priv;

  priv->crop_x = 0;
  priv->crop_y = 0;
  priv->width = 0;
  priv->height = 0;
  priv->coded_width = 0;
  priv->coded_height = 0;
  priv->bitdepth = 0;
  priv->chroma_format_idc = 0;
  priv->out_format = GST_VIDEO_FORMAT_UNKNOWN;
  priv->interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
  priv->max_dpb_size = 0;
  priv->configured = FALSE;
}

static gboolean
gst_dxva_h265_decoder_start (GstVideoDecoder * decoder)
{
  GstDxvaH265Decoder *self = GST_DXVA_H265_DECODER (decoder);

  gst_dxva_h265_decoder_reset (self);

  return GST_VIDEO_DECODER_CLASS (parent_class)->start (decoder);
}

static GstFlowReturn
gst_dxva_h265_decoder_new_sequence (GstH265Decoder * decoder,
    const GstH265SPS * sps, gint max_dpb_size)
{
  GstDxvaH265Decoder *self = GST_DXVA_H265_DECODER (decoder);
  GstDxvaH265DecoderPrivate *priv = self->priv;
  GstDxvaH265DecoderClass *klass = GST_DXVA_H265_DECODER_GET_CLASS (self);
  gint crop_width, crop_height;
  gboolean modified = FALSE;
  GstVideoInterlaceMode interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
  GstVideoInfo info;
  GstFlowReturn ret;

  GST_LOG_OBJECT (self, "new sequence");

  if (sps->conformance_window_flag) {
    crop_width = sps->crop_rect_width;
    crop_height = sps->crop_rect_height;
  } else {
    crop_width = sps->width;
    crop_height = sps->height;
  }

  if (priv->width != crop_width || priv->height != crop_height ||
      priv->coded_width != sps->width || priv->coded_height != sps->height ||
      priv->crop_x != sps->crop_rect_x || priv->crop_y != sps->crop_rect_y) {
    GST_INFO_OBJECT (self, "resolution changed %dx%d (%dx%d) -> %dx%d (%dx%d)",
        priv->width, priv->height, priv->coded_width, priv->coded_height,
        crop_width, crop_height, sps->width, sps->height);
    priv->crop_x = sps->crop_rect_x;
    priv->crop_y = sps->crop_rect_y;
    priv->width = crop_width;
    priv->height = crop_height;
    priv->coded_width = sps->width;
    priv->coded_height = sps->height;
    modified = TRUE;
  }

  if (priv->bitdepth != sps->bit_depth_luma_minus8 + 8) {
    gint bitdepth = sps->bit_depth_luma_minus8 + 8;
    GST_INFO_OBJECT (self,
        "bitdepth change, %d -> %d", priv->bitdepth, bitdepth);
    priv->bitdepth = bitdepth;
    modified = TRUE;
  }

  if (sps->vui_parameters_present_flag && sps->vui_params.field_seq_flag) {
    interlace_mode = GST_VIDEO_INTERLACE_MODE_ALTERNATE;
  } else {
    /* 7.4.4 Profile, tier and level sementics */
    if (sps->profile_tier_level.progressive_source_flag &&
        !sps->profile_tier_level.interlaced_source_flag) {
      interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
    } else {
      interlace_mode = GST_VIDEO_INTERLACE_MODE_MIXED;
    }
  }

  if (priv->interlace_mode != interlace_mode) {
    GST_INFO_OBJECT (self, "Interlace mode change %d -> %d",
        priv->interlace_mode, interlace_mode);
    priv->interlace_mode = interlace_mode;
    modified = TRUE;
  }

  if (priv->chroma_format_idc != sps->chroma_format_idc) {
    GST_INFO_OBJECT (self, "chroma format changed");
    priv->chroma_format_idc = sps->chroma_format_idc;
    modified = TRUE;
  }

  if (priv->max_dpb_size < max_dpb_size) {
    GST_INFO_OBJECT (self, "Requires larger DPB size (%d -> %d)",
        priv->max_dpb_size, max_dpb_size);
    modified = TRUE;
  }

  if (!modified && priv->configured)
    return GST_FLOW_OK;

  priv->out_format = GST_VIDEO_FORMAT_UNKNOWN;

  if (priv->bitdepth == 8) {
    if (priv->chroma_format_idc == 1) {
      priv->out_format = GST_VIDEO_FORMAT_NV12;
    } else {
      GST_FIXME_OBJECT (self, "Could not support 8bits non-4:2:0 format");
    }
  } else if (priv->bitdepth == 10) {
    if (priv->chroma_format_idc == 1) {
      priv->out_format = GST_VIDEO_FORMAT_P010_10LE;
    } else {
      GST_FIXME_OBJECT (self, "Could not support 10bits non-4:2:0 format");
    }
  }

  if (priv->out_format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ERROR_OBJECT (self, "Could not support bitdepth/chroma format");
    priv->configured = FALSE;
    return GST_FLOW_NOT_NEGOTIATED;
  }

  gst_video_info_set_interlaced_format (&info, priv->out_format,
      priv->interlace_mode, priv->width, priv->height);

  priv->max_dpb_size = max_dpb_size;

  g_assert (klass->configure);
  ret = klass->configure (self, decoder->input_state, &info, priv->crop_x,
      priv->crop_y, priv->coded_width, priv->coded_height, max_dpb_size);

  if (ret == GST_FLOW_OK) {
    priv->configured = TRUE;
    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
      GST_WARNING_OBJECT (self, "Couldn't negotiate with new sequence");
      ret = GST_FLOW_NOT_NEGOTIATED;
    }
  } else {
    priv->configured = FALSE;
  }

  return ret;
}

static GstFlowReturn
gst_dxva_h265_decoder_new_picture (GstH265Decoder * decoder,
    GstVideoCodecFrame * frame, GstH265Picture * picture)
{
  GstDxvaH265Decoder *self = GST_DXVA_H265_DECODER (decoder);
  GstDxvaH265DecoderClass *klass = GST_DXVA_H265_DECODER_GET_CLASS (self);

  g_assert (klass->new_picture);

  return klass->new_picture (self, GST_CODEC_PICTURE (picture));
}

static void
gst_dxva_h265_decoder_picture_params_from_sps (GstDxvaH265Decoder * self,
    const GstH265SPS * sps, DXVA_PicParams_HEVC * params)
{
#define COPY_FIELD(f) \
  (params)->f = (sps)->f
#define COPY_FIELD_WITH_PREFIX(f) \
  (params)->G_PASTE(sps_,f) = (sps)->f

  params->PicWidthInMinCbsY =
      sps->width >> (sps->log2_min_luma_coding_block_size_minus3 + 3);
  params->PicHeightInMinCbsY =
      sps->height >> (sps->log2_min_luma_coding_block_size_minus3 + 3);
  params->sps_max_dec_pic_buffering_minus1 =
      sps->max_dec_pic_buffering_minus1[sps->max_sub_layers_minus1];

  COPY_FIELD (chroma_format_idc);
  COPY_FIELD (separate_colour_plane_flag);
  COPY_FIELD (bit_depth_luma_minus8);
  COPY_FIELD (bit_depth_chroma_minus8);
  COPY_FIELD (log2_max_pic_order_cnt_lsb_minus4);
  COPY_FIELD (log2_min_luma_coding_block_size_minus3);
  COPY_FIELD (log2_diff_max_min_luma_coding_block_size);
  COPY_FIELD (log2_min_transform_block_size_minus2);
  COPY_FIELD (log2_diff_max_min_transform_block_size);
  COPY_FIELD (max_transform_hierarchy_depth_inter);
  COPY_FIELD (max_transform_hierarchy_depth_intra);
  COPY_FIELD (num_short_term_ref_pic_sets);
  COPY_FIELD (num_long_term_ref_pics_sps);
  COPY_FIELD (scaling_list_enabled_flag);
  COPY_FIELD (amp_enabled_flag);
  COPY_FIELD (sample_adaptive_offset_enabled_flag);
  COPY_FIELD (pcm_enabled_flag);

  if (sps->pcm_enabled_flag) {
    COPY_FIELD (pcm_sample_bit_depth_luma_minus1);
    COPY_FIELD (pcm_sample_bit_depth_chroma_minus1);
    COPY_FIELD (log2_min_pcm_luma_coding_block_size_minus3);
    COPY_FIELD (log2_diff_max_min_pcm_luma_coding_block_size);
  }

  COPY_FIELD (pcm_loop_filter_disabled_flag);
  COPY_FIELD (long_term_ref_pics_present_flag);
  COPY_FIELD_WITH_PREFIX (temporal_mvp_enabled_flag);
  COPY_FIELD (strong_intra_smoothing_enabled_flag);

#undef COPY_FIELD
#undef COPY_FIELD_WITH_PREFIX
}

static void
gst_dxva_h265_decoder_picture_params_from_pps (GstDxvaH265Decoder * self,
    const GstH265PPS * pps, DXVA_PicParams_HEVC * params)
{
  guint i;

#define COPY_FIELD(f) \
  (params)->f = (pps)->f
#define COPY_FIELD_WITH_PREFIX(f) \
  (params)->G_PASTE(pps_,f) = (pps)->f

  COPY_FIELD (num_ref_idx_l0_default_active_minus1);
  COPY_FIELD (num_ref_idx_l1_default_active_minus1);
  COPY_FIELD (init_qp_minus26);
  COPY_FIELD (dependent_slice_segments_enabled_flag);
  COPY_FIELD (output_flag_present_flag);
  COPY_FIELD (num_extra_slice_header_bits);
  COPY_FIELD (sign_data_hiding_enabled_flag);
  COPY_FIELD (cabac_init_present_flag);
  COPY_FIELD (constrained_intra_pred_flag);
  COPY_FIELD (transform_skip_enabled_flag);
  COPY_FIELD (cu_qp_delta_enabled_flag);
  COPY_FIELD_WITH_PREFIX (slice_chroma_qp_offsets_present_flag);
  COPY_FIELD (weighted_pred_flag);
  COPY_FIELD (weighted_bipred_flag);
  COPY_FIELD (transquant_bypass_enabled_flag);
  COPY_FIELD (tiles_enabled_flag);
  COPY_FIELD (entropy_coding_sync_enabled_flag);
  COPY_FIELD (uniform_spacing_flag);

  if (pps->tiles_enabled_flag)
    COPY_FIELD (loop_filter_across_tiles_enabled_flag);

  COPY_FIELD_WITH_PREFIX (loop_filter_across_slices_enabled_flag);
  COPY_FIELD (deblocking_filter_override_enabled_flag);
  COPY_FIELD_WITH_PREFIX (deblocking_filter_disabled_flag);
  COPY_FIELD (lists_modification_present_flag);
  COPY_FIELD (slice_segment_header_extension_present_flag);
  COPY_FIELD_WITH_PREFIX (cb_qp_offset);
  COPY_FIELD_WITH_PREFIX (cr_qp_offset);

  if (pps->tiles_enabled_flag) {
    COPY_FIELD (num_tile_columns_minus1);
    COPY_FIELD (num_tile_rows_minus1);
    if (!pps->uniform_spacing_flag) {
      for (i = 0; i < pps->num_tile_columns_minus1 &&
          i < G_N_ELEMENTS (params->column_width_minus1); i++)
        COPY_FIELD (column_width_minus1[i]);

      for (i = 0; i < pps->num_tile_rows_minus1 &&
          i < G_N_ELEMENTS (params->row_height_minus1); i++)
        COPY_FIELD (row_height_minus1[i]);
    }
  }

  COPY_FIELD (diff_cu_qp_delta_depth);
  COPY_FIELD_WITH_PREFIX (beta_offset_div2);
  COPY_FIELD_WITH_PREFIX (tc_offset_div2);
  COPY_FIELD (log2_parallel_merge_level_minus2);

#undef COPY_FIELD
#undef COPY_FIELD_WITH_PREFIX
}

static void
gst_dxva_h265_decoder_picture_params_from_slice_header (GstDxvaH265Decoder *
    self, const GstH265SliceHdr * slice_header, DXVA_PicParams_HEVC * params)
{
  if (slice_header->short_term_ref_pic_set_sps_flag == 0) {
    params->ucNumDeltaPocsOfRefRpsIdx =
        slice_header->short_term_ref_pic_sets.NumDeltaPocsOfRefRpsIdx;
    params->wNumBitsForShortTermRPSInSlice =
        slice_header->short_term_ref_pic_set_size;
  }
}

static gboolean
gst_dxva_h265_decoder_fill_picture_params (GstDxvaH265Decoder * self,
    const GstH265SliceHdr * slice_header, DXVA_PicParams_HEVC * params)
{
  const GstH265SPS *sps;
  const GstH265PPS *pps;

  pps = slice_header->pps;
  sps = pps->sps;

  /* not related to hevc syntax */
  params->NoPicReorderingFlag = 0;
  params->NoBiPredFlag = 0;
  params->ReservedBits1 = 0;
  params->StatusReportFeedbackNumber = 1;

  gst_dxva_h265_decoder_picture_params_from_sps (self, sps, params);
  gst_dxva_h265_decoder_picture_params_from_pps (self, pps, params);
  gst_dxva_h265_decoder_picture_params_from_slice_header (self,
      slice_header, params);

  return TRUE;
}

static UCHAR
gst_dxva_h265_decoder_get_ref_index (const DXVA_PicParams_HEVC * pic_params,
    guint8 picture_id)
{
  if (picture_id == 0xff)
    return 0xff;

  for (UCHAR i = 0; i < G_N_ELEMENTS (pic_params->RefPicList); i++) {
    if (pic_params->RefPicList[i].Index7Bits == picture_id)
      return i;
  }

  return 0xff;
}

static inline void
init_pic_params (DXVA_PicParams_HEVC * params)
{
  memset (params, 0, sizeof (DXVA_PicParams_HEVC));
  for (guint i = 0; i < G_N_ELEMENTS (params->RefPicList); i++)
    params->RefPicList[i].bPicEntry = 0xff;

  for (guint i = 0; i < G_N_ELEMENTS (params->RefPicSetStCurrBefore); i++) {
    params->RefPicSetStCurrBefore[i] = 0xff;
    params->RefPicSetStCurrAfter[i] = 0xff;
    params->RefPicSetLtCurr[i] = 0xff;
  }
}

static GstFlowReturn
gst_dxva_h265_decoder_start_picture (GstH265Decoder * decoder,
    GstH265Picture * picture, GstH265Slice * slice, GstH265Dpb * dpb)
{
  GstDxvaH265Decoder *self = GST_DXVA_H265_DECODER (decoder);
  GstDxvaH265DecoderPrivate *priv = self->priv;
  GstDxvaH265DecoderClass *klass = GST_DXVA_H265_DECODER_GET_CLASS (self);
  DXVA_PicParams_HEVC *pic_params = &priv->pic_params;
  DXVA_Qmatrix_HEVC *iq_matrix = &priv->iq_matrix;
  GstCodecPicture *codec_picture = GST_CODEC_PICTURE (picture);
  guint i, j;
  GArray *dpb_array;
  GstH265SPS *sps;
  GstH265PPS *pps;
  GstH265ScalingList *scaling_list = nullptr;
  GstFlowReturn ret;
  guint8 picture_id;

  g_assert (klass->start_picture);
  g_assert (klass->get_picture_id);

  ret = klass->start_picture (self, codec_picture, &picture_id);
  if (ret != GST_FLOW_OK)
    return ret;

  pps = slice->header.pps;
  g_assert (pps);

  sps = pps->sps;
  g_assert (sps);

  priv->slice_list.resize (0);
  priv->bitstream_buffer.resize (0);
  g_ptr_array_set_size (priv->ref_pics, 0);

  init_pic_params (pic_params);
  gst_dxva_h265_decoder_fill_picture_params (self, &slice->header, pic_params);

  pic_params->CurrPic.Index7Bits = picture_id;
  pic_params->IrapPicFlag = GST_H265_IS_NAL_TYPE_IRAP (slice->nalu.type);
  pic_params->IdrPicFlag = GST_H265_IS_NAL_TYPE_IDR (slice->nalu.type);
  pic_params->IntraPicFlag = GST_H265_IS_NAL_TYPE_IRAP (slice->nalu.type);
  pic_params->CurrPicOrderCntVal = picture->pic_order_cnt;

  dpb_array = gst_h265_dpb_get_pictures_all (dpb);
  for (i = 0, j = 0;
      i < dpb_array->len && j < G_N_ELEMENTS (pic_params->RefPicList); i++) {
    GstH265Picture *other = g_array_index (dpb_array, GstH265Picture *, i);
    guint8 id;

    if (!other->ref)
      continue;

    id = klass->get_picture_id (self, GST_CODEC_PICTURE (other));
    if (id != 0xff) {
      pic_params->RefPicList[j].Index7Bits = id;
      pic_params->RefPicList[j].AssociatedFlag = other->long_term;
      pic_params->PicOrderCntValList[j] = other->pic_order_cnt;
      g_ptr_array_add (priv->ref_pics, other);
    }

    j++;
  }
  g_array_unref (dpb_array);

  for (i = 0, j = 0; i < G_N_ELEMENTS (pic_params->RefPicSetStCurrBefore); i++) {
    GstH265Picture *other = nullptr;
    guint8 id = 0xff;

    while (!other && j < decoder->NumPocStCurrBefore)
      other = decoder->RefPicSetStCurrBefore[j++];

    if (other)
      id = klass->get_picture_id (self, GST_CODEC_PICTURE (other));

    pic_params->RefPicSetStCurrBefore[i] =
        gst_dxva_h265_decoder_get_ref_index (pic_params, id);
  }

  for (i = 0, j = 0; i < G_N_ELEMENTS (pic_params->RefPicSetStCurrAfter); i++) {
    GstH265Picture *other = nullptr;
    guint8 id = 0xff;

    while (!other && j < decoder->NumPocStCurrAfter)
      other = decoder->RefPicSetStCurrAfter[j++];

    if (other)
      id = klass->get_picture_id (self, GST_CODEC_PICTURE (other));

    pic_params->RefPicSetStCurrAfter[i] =
        gst_dxva_h265_decoder_get_ref_index (pic_params, id);
  }

  for (i = 0, j = 0; i < G_N_ELEMENTS (pic_params->RefPicSetLtCurr); i++) {
    GstH265Picture *other = nullptr;
    guint8 id = 0xff;

    while (!other && j < decoder->NumPocLtCurr)
      other = decoder->RefPicSetLtCurr[j++];

    if (other)
      id = klass->get_picture_id (self, GST_CODEC_PICTURE (other));

    pic_params->RefPicSetLtCurr[i] =
        gst_dxva_h265_decoder_get_ref_index (pic_params, id);
  }

  if (pps->scaling_list_data_present_flag ||
      (sps->scaling_list_enabled_flag
          && !sps->scaling_list_data_present_flag)) {
    scaling_list = &pps->scaling_list;
  } else if (sps->scaling_list_enabled_flag &&
      sps->scaling_list_data_present_flag) {
    scaling_list = &sps->scaling_list;
  }

  if (scaling_list) {
    G_STATIC_ASSERT (sizeof (iq_matrix->ucScalingLists0) ==
        sizeof (scaling_list->scaling_lists_4x4));
    G_STATIC_ASSERT (sizeof (iq_matrix->ucScalingLists1) ==
        sizeof (scaling_list->scaling_lists_8x8));
    G_STATIC_ASSERT (sizeof (iq_matrix->ucScalingLists2) ==
        sizeof (scaling_list->scaling_lists_16x16));
    G_STATIC_ASSERT (sizeof (iq_matrix->ucScalingLists3) ==
        sizeof (scaling_list->scaling_lists_32x32));

    memcpy (iq_matrix->ucScalingLists0, scaling_list->scaling_lists_4x4,
        sizeof (iq_matrix->ucScalingLists0));
    memcpy (iq_matrix->ucScalingLists1, scaling_list->scaling_lists_8x8,
        sizeof (iq_matrix->ucScalingLists1));
    memcpy (iq_matrix->ucScalingLists2, scaling_list->scaling_lists_16x16,
        sizeof (iq_matrix->ucScalingLists2));
    memcpy (iq_matrix->ucScalingLists3, scaling_list->scaling_lists_32x32,
        sizeof (iq_matrix->ucScalingLists3));

    for (i = 0; i < G_N_ELEMENTS (iq_matrix->ucScalingListDCCoefSizeID2); i++) {
      iq_matrix->ucScalingListDCCoefSizeID2[i] =
          scaling_list->scaling_list_dc_coef_minus8_16x16[i] + 8;
    }

    for (i = 0; i < G_N_ELEMENTS (iq_matrix->ucScalingListDCCoefSizeID3); i++) {
      iq_matrix->ucScalingListDCCoefSizeID3[i] =
          scaling_list->scaling_list_dc_coef_minus8_32x32[i] + 8;
    }

    priv->submit_iq_data = TRUE;
  } else {
    priv->submit_iq_data = FALSE;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_dxva_h265_decoder_decode_slice (GstH265Decoder * decoder,
    GstH265Picture * picture, GstH265Slice * slice,
    GArray * ref_pic_list0, GArray * ref_pic_list1)
{
  GstDxvaH265Decoder *self = GST_DXVA_H265_DECODER (decoder);
  GstDxvaH265DecoderPrivate *priv = self->priv;
  DXVA_Slice_HEVC_Short dxva_slice;
  static const guint8 start_code[] = { 0, 0, 1 };
  const size_t start_code_size = sizeof (start_code);

  dxva_slice.BSNALunitDataLocation = priv->bitstream_buffer.size ();
  /* Includes 3 bytes start code prefix */
  dxva_slice.SliceBytesInBuffer = slice->nalu.size + start_code_size;
  dxva_slice.wBadSliceChopping = 0;

  priv->slice_list.push_back (dxva_slice);

  size_t pos = priv->bitstream_buffer.size ();
  priv->bitstream_buffer.resize (pos + start_code_size + slice->nalu.size);

  /* Fill start code prefix */
  memcpy (&priv->bitstream_buffer[0] + pos, start_code, start_code_size);

  /* Copy bitstream */
  memcpy (&priv->bitstream_buffer[0] + pos + start_code_size,
      slice->nalu.data + slice->nalu.offset, slice->nalu.size);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_dxva_h265_decoder_end_picture (GstH265Decoder * decoder,
    GstH265Picture * picture)
{
  GstDxvaH265Decoder *self = GST_DXVA_H265_DECODER (decoder);
  GstDxvaH265DecoderPrivate *priv = self->priv;
  GstDxvaH265DecoderClass *klass = GST_DXVA_H265_DECODER_GET_CLASS (self);
  size_t bitstream_buffer_size;
  size_t bitstream_pos;
  GstDxvaDecodingArgs args;

  GST_LOG_OBJECT (self, "end picture %p, (poc %d)",
      picture, picture->pic_order_cnt);

  if (priv->bitstream_buffer.empty () || priv->slice_list.empty ()) {
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

    DXVA_Slice_HEVC_Short & slice = priv->slice_list.back ();
    slice.SliceBytesInBuffer += padding;
  }

  args.picture_params = &priv->pic_params;
  args.picture_params_size = sizeof (DXVA_PicParams_HEVC);
  args.slice_control = &priv->slice_list[0];
  args.slice_control_size =
      sizeof (DXVA_Slice_HEVC_Short) * priv->slice_list.size ();
  args.bitstream = &priv->bitstream_buffer[0];
  args.bitstream_size = priv->bitstream_buffer.size ();

  if (priv->submit_iq_data) {
    args.inverse_quantization_matrix = &priv->iq_matrix;
    args.inverse_quantization_matrix_size = sizeof (DXVA_Qmatrix_HEVC);
  }

  g_assert (klass->end_picture);

  return klass->end_picture (self, GST_CODEC_PICTURE (picture),
      priv->ref_pics, &args);
}

static GstFlowReturn
gst_dxva_h265_decoder_output_picture (GstH265Decoder * decoder,
    GstVideoCodecFrame * frame, GstH265Picture * picture)
{
  GstDxvaH265Decoder *self = GST_DXVA_H265_DECODER (decoder);
  GstDxvaH265DecoderPrivate *priv = self->priv;
  GstDxvaH265DecoderClass *klass = GST_DXVA_H265_DECODER_GET_CLASS (self);

  g_assert (klass->output_picture);

  GST_LOG_OBJECT (self, "Outputting picture %p, poc %d, picture_struct %d, "
      "buffer flags 0x%x", picture, picture->pic_order_cnt, picture->pic_struct,
      picture->buffer_flags);

  return klass->output_picture (self, frame, GST_CODEC_PICTURE (picture),
      picture->buffer_flags, priv->width, priv->height);
}
