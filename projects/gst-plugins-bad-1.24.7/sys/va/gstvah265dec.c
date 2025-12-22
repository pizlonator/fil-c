/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
 * Copyright (C) 2020 Collabora
 *     Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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
 * License along with this library; if not, write to the0
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-vah265dec
 * @title: vah265dec
 * @short_description: A VA-API based H265 video decoder
 *
 * vah265dec decodes H265 bitstreams to VA surfaces using the
 * installed and chosen [VA-API](https://01.org/linuxmedia/vaapi)
 * driver.
 *
 * The decoding surfaces can be mapped onto main memory as video
 * frames.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=big_buck_bunny.mov ! parsebin ! vah265dec ! autovideosink
 * ```
 *
 * Since: 1.20
 *
 */

/* ToDo:
 *
 * + interlaced streams
 * + mutiview and stereo profiles
 * + SCC extension buffer
 * + Add 10bit support
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvah265dec.h"

#include "gstvabasedec.h"

GST_DEBUG_CATEGORY_STATIC (gst_va_h265dec_debug);
#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT gst_va_h265dec_debug
#else
#define GST_CAT_DEFAULT NULL
#endif

#define GST_VA_H265_DEC(obj)           ((GstVaH265Dec *) obj)
#define GST_VA_H265_DEC_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_FROM_INSTANCE (obj), GstVaH265DecClass))
#define GST_VA_H265_DEC_CLASS(klass)   ((GstVaH265DecClass *) klass)

struct slice
{
  guint8 *data;
  guint size;

  VASliceParameterBufferHEVCExtension param;
};

typedef struct _GstVaH265Dec GstVaH265Dec;
typedef struct _GstVaH265DecClass GstVaH265DecClass;

struct _GstVaH265DecClass
{
  GstVaBaseDecClass parent_class;
};

struct _GstVaH265Dec
{
  GstVaBaseDec parent;

  gint dpb_size;

  VAPictureParameterBufferHEVCExtension pic_param;

  gint32 WpOffsetHalfRangeC;

  struct slice prev_slice;
};

static GstElementClass *parent_class = NULL;

/* *INDENT-OFF* */
static const gchar *src_caps_str =
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VA,
        "{ NV12, P010_10LE }") " ;"
    GST_VIDEO_CAPS_MAKE ("{ NV12, P010_10LE }");
/* *INDENT-ON* */

static const gchar *sink_caps_str = "video/x-h265";

static gboolean
_is_range_extension_profile (VAProfile profile)
{
  if (profile == VAProfileHEVCMain422_10
      || profile == VAProfileHEVCMain444
      || profile == VAProfileHEVCMain444_10
      || profile == VAProfileHEVCMain12
      || profile == VAProfileHEVCMain444_12
      || profile == VAProfileHEVCMain422_12)
    return TRUE;
  return FALSE;
}

static gboolean
_is_screen_content_ext_profile (VAProfile profile)
{
  if (profile == VAProfileHEVCSccMain || profile == VAProfileHEVCSccMain10
      || profile == VAProfileHEVCSccMain444
      || profile == VAProfileHEVCSccMain444_10)
    return TRUE;

  return FALSE;
}

static inline void
_set_last_slice_flag (GstVaH265Dec * self)
{
  self->prev_slice.param.base.LongSliceFlags.fields.LastSliceOfPic = 1;
}

static void
_replace_previous_slice (GstVaH265Dec * self, guint8 * data, guint size)
{
  struct slice *slice = &self->prev_slice;
  gboolean do_reset = (slice->size < size);

  if (!data || do_reset) {
    g_clear_pointer (&slice->data, g_free);
    slice->size = 0;
  }

  if (!data)
    return;

  if (do_reset) {
    GST_LOG_OBJECT (self, "allocating slice data %u", size);
    slice->data = g_malloc (size);
  }

  memcpy (slice->data, data, size);
  slice->size = size;
}

static gboolean
_submit_previous_slice (GstVaBaseDec * base, GstVaDecodePicture * va_pic)
{
  GstVaH265Dec *self = GST_VA_H265_DEC (base);
  struct slice *slice;
  gboolean ret;
  gsize param_size;

  slice = &self->prev_slice;
  if (!slice->data && slice->size == 0)
    return TRUE;
  if (!slice->data || slice->size == 0)
    return FALSE;

  param_size = _is_range_extension_profile (self->parent.profile)
      || _is_screen_content_ext_profile (self->parent.profile) ?
      sizeof (slice->param) : sizeof (slice->param.base);
  ret = gst_va_decoder_add_slice_buffer (base->decoder, va_pic, &slice->param,
      param_size, slice->data, slice->size);

  return ret;
}

static GstFlowReturn
gst_va_h265_dec_end_picture (GstH265Decoder * decoder, GstH265Picture * picture)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaH265Dec *self = GST_VA_H265_DEC (decoder);
  GstVaDecodePicture *va_pic;
  gboolean ret;

  GST_LOG_OBJECT (base, "end picture %p, (poc %d)",
      picture, picture->pic_order_cnt);

  va_pic = gst_h265_picture_get_user_data (picture);

  _set_last_slice_flag (self);
  ret = _submit_previous_slice (base, va_pic);

  /* TODO(victor): optimization: this could be done at decoder's
   * stop() vmethod */
  _replace_previous_slice (self, NULL, 0);

  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to submit the previous slice");
    return GST_FLOW_ERROR;
  }

  ret = gst_va_decoder_decode (base->decoder, va_pic);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed at end picture %p, (poc %d)",
        picture, picture->pic_order_cnt);
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_h265_dec_output_picture (GstH265Decoder * decoder,
    GstVideoCodecFrame * frame, GstH265Picture * picture)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaH265Dec *self = GST_VA_H265_DEC (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstVaDecodePicture *va_pic;
  gboolean ret;

  va_pic = gst_h265_picture_get_user_data (picture);
  g_assert (va_pic->gstbuffer);

  GST_LOG_OBJECT (self,
      "Outputting picture %p (poc %d)", picture, picture->pic_order_cnt);

  gst_buffer_replace (&frame->output_buffer, va_pic->gstbuffer);

  ret = gst_va_base_dec_process_output (base, frame,
      GST_CODEC_PICTURE (picture)->discont_state, picture->buffer_flags);
  gst_h265_picture_unref (picture);

  if (ret)
    return gst_video_decoder_finish_frame (vdec, frame);
  return GST_FLOW_ERROR;
}

static void
_init_vaapi_pic (VAPictureHEVC * va_picture)
{
  va_picture->picture_id = VA_INVALID_ID;
  va_picture->flags = VA_PICTURE_HEVC_INVALID;
  va_picture->pic_order_cnt = 0;
}

static gint
_find_frame_rps_type (GstH265Decoder * decoder, GstH265Picture * ref_pic)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (decoder->RefPicSetStCurrBefore); i++) {
    if (ref_pic == decoder->RefPicSetStCurrBefore[i])
      return VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE;
  }

  for (i = 0; i < G_N_ELEMENTS (decoder->RefPicSetStCurrAfter); i++) {
    if (ref_pic == decoder->RefPicSetStCurrAfter[i])
      return VA_PICTURE_HEVC_RPS_ST_CURR_AFTER;
  }

  for (i = 0; i < G_N_ELEMENTS (decoder->RefPicSetLtCurr); i++) {
    if (ref_pic == decoder->RefPicSetLtCurr[i])
      return VA_PICTURE_HEVC_RPS_LT_CURR;
  }

  return 0;
}


static void
_fill_vaapi_pic (GstH265Decoder * decoder, VAPictureHEVC * va_picture,
    GstH265Picture * picture)
{
  GstVaDecodePicture *va_pic;

  va_pic = gst_h265_picture_get_user_data (picture);

  if (!va_pic) {
    _init_vaapi_pic (va_picture);
    return;
  }

  va_picture->picture_id = gst_va_decode_picture_get_surface (va_pic);
  va_picture->pic_order_cnt = picture->pic_order_cnt;
  va_picture->flags = 0;

  if (picture->ref && picture->long_term)
    va_picture->flags |= VA_PICTURE_HEVC_LONG_TERM_REFERENCE;

  va_picture->flags |= _find_frame_rps_type (decoder, picture);
}

static guint8
_get_reference_index (GstH265Decoder * decoder, GstH265Picture * picture)
{
  GstVaH265Dec *self = GST_VA_H265_DEC (decoder);
  guint8 i;

  if (!picture)
    return 0xFF;

  for (i = 0; i < 15; i++) {
    VAPictureHEVC *ref_va_pic = &self->pic_param.base.ReferenceFrames[i];

    if (ref_va_pic->picture_id == VA_INVALID_ID)
      break;

    if (ref_va_pic->pic_order_cnt == picture->pic_order_cnt)
      return i;
  }

  return 0xFF;
}

/* fill the VA API reference picture lists from the GstCodec reference
 * picture list */
static void
_fill_ref_pic_list (GstH265Decoder * decoder, GstH265Picture * cur_pic,
    guint8 va_reflist[15], GArray * reflist)
{
  guint i;

  for (i = 0; i < reflist->len && i < 15; i++) {
    GstH265Picture *picture = g_array_index (reflist, GstH265Picture *, i);
    va_reflist[i] = _get_reference_index (decoder, picture);
  }

  for (; i < 15; i++)
    va_reflist[i] = 0xFF;
}

static void
_fill_pred_weight_table (GstVaH265Dec * self, GstH265SliceHdr * header,
    VASliceParameterBufferHEVCExtension * slice_param)
{
  gint chroma_weight, chroma_log2_weight_denom;
  gint i, j;
  GstH265PPS *pps = header->pps;
  gboolean is_rext = _is_range_extension_profile (self->parent.profile);

  if (GST_H265_IS_I_SLICE (header) ||
      (!pps->weighted_pred_flag && GST_H265_IS_P_SLICE (header)) ||
      (!pps->weighted_bipred_flag && GST_H265_IS_B_SLICE (header)))
    return;

  slice_param->base.luma_log2_weight_denom =
      header->pred_weight_table.luma_log2_weight_denom;

  if (pps->sps->chroma_array_type != 0)
    slice_param->base.delta_chroma_log2_weight_denom =
        header->pred_weight_table.delta_chroma_log2_weight_denom;

  for (i = 0; i <= header->num_ref_idx_l0_active_minus1; i++) {
    if (!header->pred_weight_table.luma_weight_l0_flag[i])
      continue;

    slice_param->base.delta_luma_weight_l0[i] =
        header->pred_weight_table.delta_luma_weight_l0[i];
    slice_param->base.luma_offset_l0[i] =
        header->pred_weight_table.luma_offset_l0[i];

    if (is_rext) {
      slice_param->rext.luma_offset_l0[i] =
          header->pred_weight_table.luma_offset_l0[i];
    }
  }

  chroma_log2_weight_denom = slice_param->base.luma_log2_weight_denom +
      slice_param->base.delta_chroma_log2_weight_denom;

  for (i = 0; i <= header->num_ref_idx_l0_active_minus1; i++) {
    if (!header->pred_weight_table.chroma_weight_l0_flag[i])
      continue;

    for (j = 0; j < 2; j++) {
      gint16 delta_chroma_offset_l0 =
          header->pred_weight_table.delta_chroma_offset_l0[i][j];
      gint chroma_offset;

      slice_param->base.delta_chroma_weight_l0[i][j] =
          header->pred_weight_table.delta_chroma_weight_l0[i][j];

      /* Find  ChromaWeightL0 */
      chroma_weight = (1 << chroma_log2_weight_denom) +
          header->pred_weight_table.delta_chroma_weight_l0[i][j];
      chroma_offset = self->WpOffsetHalfRangeC + delta_chroma_offset_l0
          - ((self->WpOffsetHalfRangeC * chroma_weight)
          >> chroma_log2_weight_denom);

      /* 7-56 */
      slice_param->base.ChromaOffsetL0[i][j] = CLAMP (chroma_offset,
          -self->WpOffsetHalfRangeC, self->WpOffsetHalfRangeC - 1);

      if (is_rext) {
        slice_param->rext.ChromaOffsetL0[i][j] =
            slice_param->base.ChromaOffsetL0[i][j];
      }
    }
  }

  /* Skip l1 if this is not a B-Frame. */
  if (!GST_H265_IS_B_SLICE (header))
    return;

  for (i = 0; i <= header->num_ref_idx_l1_active_minus1; i++) {
    if (!header->pred_weight_table.luma_weight_l1_flag[i])
      continue;

    slice_param->base.delta_luma_weight_l1[i] =
        header->pred_weight_table.delta_luma_weight_l1[i];
    slice_param->base.luma_offset_l1[i] =
        header->pred_weight_table.luma_offset_l1[i];

    if (is_rext) {
      slice_param->rext.luma_offset_l1[i] =
          header->pred_weight_table.luma_offset_l1[i];
    }
  }

  for (i = 0; i <= header->num_ref_idx_l1_active_minus1; i++) {
    if (!header->pred_weight_table.chroma_weight_l1_flag[i])
      continue;

    for (j = 0; j < 2; j++) {
      gint16 delta_chroma_offset_l1 =
          header->pred_weight_table.delta_chroma_offset_l1[i][j];
      gint chroma_offset;

      slice_param->base.delta_chroma_weight_l1[i][j] =
          header->pred_weight_table.delta_chroma_weight_l1[i][j];

      /* Find  ChromaWeightL1 */
      chroma_weight = (1 << chroma_log2_weight_denom) +
          header->pred_weight_table.delta_chroma_weight_l1[i][j];

      chroma_offset = self->WpOffsetHalfRangeC + delta_chroma_offset_l1
          - ((self->WpOffsetHalfRangeC * chroma_weight)
          >> chroma_log2_weight_denom);

      /* 7-56 */
      slice_param->base.ChromaOffsetL1[i][j] = CLAMP (chroma_offset,
          -self->WpOffsetHalfRangeC, self->WpOffsetHalfRangeC - 1);

      if (is_rext) {
        slice_param->rext.ChromaOffsetL1[i][j] =
            slice_param->base.ChromaOffsetL1[i][j];
      }
    }
  }
}

static inline guint
_get_slice_data_byte_offset (GstH265SliceHdr * slice_hdr,
    guint nal_header_bytes)
{
  guint epb_count;

  epb_count = slice_hdr->n_emulation_prevention_bytes;
  return nal_header_bytes + (slice_hdr->header_size + 7) / 8 - epb_count;
}

static GstFlowReturn
gst_va_h265_dec_decode_slice (GstH265Decoder * decoder,
    GstH265Picture * picture, GstH265Slice * slice, GArray * ref_pic_list0,
    GArray * ref_pic_list1)
{
  GstH265SliceHdr *header = &slice->header;
  GstH265NalUnit *nalu = &slice->nalu;
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaH265Dec *self = GST_VA_H265_DEC (decoder);
  GstVaDecodePicture *va_pic;
  VASliceParameterBufferHEVCExtension *slice_param;

  va_pic = gst_h265_picture_get_user_data (picture);
  if (!_submit_previous_slice (base, va_pic)) {
    _replace_previous_slice (self, NULL, 0);
    GST_ERROR_OBJECT (base, "Failed to submit previous slice buffers");
    return GST_FLOW_ERROR;
  }

  slice_param = &self->prev_slice.param;

  /* *INDENT-OFF* */
  slice_param->base = (VASliceParameterBufferHEVC) {
    .slice_data_size = nalu->size,
    .slice_data_offset = 0,
    .slice_data_flag = VA_SLICE_DATA_FLAG_ALL,
    .slice_data_byte_offset = _get_slice_data_byte_offset (header, nalu->header_bytes),
    .slice_segment_address = header->segment_address,
    .collocated_ref_idx = header->temporal_mvp_enabled_flag ? header->collocated_ref_idx : 0xFF,
    .num_ref_idx_l0_active_minus1 = header->num_ref_idx_l0_active_minus1,
    .num_ref_idx_l1_active_minus1 = header->num_ref_idx_l1_active_minus1,
    .slice_qp_delta = header->qp_delta,
    .slice_cb_qp_offset = header->cb_qp_offset,
    .slice_cr_qp_offset = header->cr_qp_offset,
    .slice_beta_offset_div2 = header->beta_offset_div2,
    .slice_tc_offset_div2 = header->tc_offset_div2,
    .five_minus_max_num_merge_cand = header->five_minus_max_num_merge_cand,
    .num_entry_point_offsets = header->num_entry_point_offsets,
    .entry_offset_to_subset_array = 0, /* does not exist in spec */
    .slice_data_num_emu_prevn_bytes = header->n_emulation_prevention_bytes,
    .LongSliceFlags.fields = {
      .LastSliceOfPic = 0, /* the last one will be set on end_picture() */
      .dependent_slice_segment_flag = header->dependent_slice_segment_flag,
      .slice_type = header->type,
      .color_plane_id = header->colour_plane_id,
      .slice_sao_luma_flag = header->sao_luma_flag,
      .slice_sao_chroma_flag = header->sao_chroma_flag,
      .mvd_l1_zero_flag = header->mvd_l1_zero_flag,
      .cabac_init_flag = header->cabac_init_flag,
      .slice_temporal_mvp_enabled_flag = header->temporal_mvp_enabled_flag,
      .slice_deblocking_filter_disabled_flag =
          header->deblocking_filter_disabled_flag,
      .collocated_from_l0_flag = header->collocated_from_l0_flag,
      .slice_loop_filter_across_slices_enabled_flag =
          header->loop_filter_across_slices_enabled_flag,
    },
  };
  /* *INDENT-ON* */

  if (_is_range_extension_profile (base->profile)
      || _is_screen_content_ext_profile (base->profile)) {
    /* *INDENT-OFF* */
    slice_param->rext = (VASliceParameterBufferHEVCRext) {
      .slice_ext_flags.bits = {
        .cu_chroma_qp_offset_enabled_flag = header->cu_chroma_qp_offset_enabled_flag,
        .use_integer_mv_flag = header->use_integer_mv_flag,
      },
      .slice_act_y_qp_offset = header->slice_act_y_qp_offset,
      .slice_act_cb_qp_offset = header->slice_act_cb_qp_offset,
      .slice_act_cr_qp_offset = header->slice_act_cr_qp_offset,
    };
    /* *INDENT-ON* */
  }

  _fill_ref_pic_list (decoder, picture, slice_param->base.RefPicList[0],
      ref_pic_list0);
  _fill_ref_pic_list (decoder, picture, slice_param->base.RefPicList[1],
      ref_pic_list1);

  _fill_pred_weight_table (GST_VA_H265_DEC (decoder), header, slice_param);

  _replace_previous_slice (self, slice->nalu.data + slice->nalu.offset,
      slice->nalu.size);

  return GST_FLOW_OK;
}

static void
_fill_picture_range_ext_parameter (GstVaH265Dec * decoder,
    GstH265SPS * sps, GstH265PPS * pps)
{
  VAPictureParameterBufferHEVCRext *pic_param = &decoder->pic_param.rext;

  GstH265SPSExtensionParams *sps_ext = &sps->sps_extension_params;
  GstH265PPSExtensionParams *pps_ext = &pps->pps_extension_params;

  /* *INDENT-OFF* */
  *pic_param = (VAPictureParameterBufferHEVCRext) {
    .range_extension_pic_fields.bits = {
      .transform_skip_rotation_enabled_flag = sps_ext->transform_skip_rotation_enabled_flag,
      .transform_skip_context_enabled_flag = sps_ext->transform_skip_context_enabled_flag,
      .implicit_rdpcm_enabled_flag = sps_ext->implicit_rdpcm_enabled_flag,
      .explicit_rdpcm_enabled_flag = sps_ext->explicit_rdpcm_enabled_flag,
      .extended_precision_processing_flag = sps_ext->extended_precision_processing_flag,
      .intra_smoothing_disabled_flag = sps_ext->intra_smoothing_disabled_flag,
      .high_precision_offsets_enabled_flag = sps_ext->high_precision_offsets_enabled_flag,
      .persistent_rice_adaptation_enabled_flag = sps_ext->persistent_rice_adaptation_enabled_flag,
      .cabac_bypass_alignment_enabled_flag = sps_ext->cabac_bypass_alignment_enabled_flag,
      .cross_component_prediction_enabled_flag = pps_ext->cross_component_prediction_enabled_flag,
      .chroma_qp_offset_list_enabled_flag = pps_ext->chroma_qp_offset_list_enabled_flag,
    },
    .diff_cu_chroma_qp_offset_depth = pps_ext->diff_cu_chroma_qp_offset_depth,
    .chroma_qp_offset_list_len_minus1 = pps_ext->chroma_qp_offset_list_len_minus1,
    .log2_sao_offset_scale_luma = pps_ext->log2_sao_offset_scale_luma,
    .log2_sao_offset_scale_chroma = pps_ext->log2_sao_offset_scale_chroma,
    .log2_max_transform_skip_block_size_minus2 = pps_ext->log2_max_transform_skip_block_size_minus2,
  };
  /* *INDENT-ON* */

  memcpy (pic_param->cb_qp_offset_list, pps_ext->cb_qp_offset_list,
      sizeof (pic_param->cb_qp_offset_list));
  memcpy (pic_param->cr_qp_offset_list, pps_ext->cr_qp_offset_list,
      sizeof (pic_param->cr_qp_offset_list));
}

static void
_fill_screen_content_ext_parameter (GstVaH265Dec * decoder,
    GstH265SPS * sps, GstH265PPS * pps)
{
  VAPictureParameterBufferHEVCScc *pic_param = &decoder->pic_param.scc;
  const GstH265PPSSccExtensionParams *pps_scc = &pps->pps_scc_extension_params;
  const GstH265SPSSccExtensionParams *sps_scc = &sps->sps_scc_extension_params;
  guint32 num_comps;
  guint i, n;

  /* *INDENT-OFF* */
  *pic_param = (VAPictureParameterBufferHEVCScc) {
    .screen_content_pic_fields.bits = {
      .pps_curr_pic_ref_enabled_flag = pps_scc->pps_curr_pic_ref_enabled_flag,
      .palette_mode_enabled_flag = sps_scc->palette_mode_enabled_flag,
      .motion_vector_resolution_control_idc = sps_scc->motion_vector_resolution_control_idc,
      .intra_boundary_filtering_disabled_flag = sps_scc->intra_boundary_filtering_disabled_flag,
      .residual_adaptive_colour_transform_enabled_flag = pps_scc->residual_adaptive_colour_transform_enabled_flag,
      .pps_slice_act_qp_offsets_present_flag = pps_scc->pps_slice_act_qp_offsets_present_flag,
    },
    .palette_max_size = sps_scc->palette_max_size,
    .delta_palette_max_predictor_size = sps_scc->delta_palette_max_predictor_size,
    .pps_act_y_qp_offset_plus5 = pps_scc->pps_act_y_qp_offset_plus5,
    .pps_act_cb_qp_offset_plus5 = pps_scc->pps_act_cb_qp_offset_plus5,
    .pps_act_cr_qp_offset_plus3 = pps_scc->pps_act_cr_qp_offset_plus3,
  };
  /* *INDENT-ON* */

  /* firstly use the pps, then sps */
  num_comps = sps->chroma_format_idc ? 3 : 1;

  if (pps_scc->pps_palette_predictor_initializers_present_flag) {
    pic_param->predictor_palette_size =
        pps_scc->pps_num_palette_predictor_initializer;
    for (n = 0; n < num_comps; n++)
      for (i = 0; i < pps_scc->pps_num_palette_predictor_initializer; i++)
        pic_param->predictor_palette_entries[n][i] =
            (uint16_t) pps_scc->pps_palette_predictor_initializer[n][i];
  } else if (sps_scc->sps_palette_predictor_initializers_present_flag) {
    pic_param->predictor_palette_size =
        sps_scc->sps_num_palette_predictor_initializer_minus1 + 1;
    for (n = 0; n < num_comps; n++)
      for (i = 0;
          i < sps_scc->sps_num_palette_predictor_initializer_minus1 + 1; i++)
        pic_param->predictor_palette_entries[n][i] =
            (uint16_t) sps_scc->sps_palette_predictor_initializer[n][i];
  }
}

static GstFlowReturn
gst_va_h265_dec_start_picture (GstH265Decoder * decoder,
    GstH265Picture * picture, GstH265Slice * slice, GstH265Dpb * dpb)
{
  GstH265PPS *pps;
  GstH265SPS *sps;
  GstVaH265Dec *self = GST_VA_H265_DEC (decoder);
  GstVaBaseDec *base = &self->parent;
  GstVaDecodePicture *va_pic;
  GstH265ScalingList *scaling_list = NULL;
  VAIQMatrixBufferHEVC iq_matrix = { 0, };
  VAPictureParameterBufferHEVCExtension *pic_param = &self->pic_param;
  gsize pic_param_size;
  guint i;

  va_pic = gst_h265_picture_get_user_data (picture);

  pps = slice->header.pps;
  sps = pps->sps;

  /* *INDENT-OFF* */
  pic_param->base = (VAPictureParameterBufferHEVC) {
    .pic_width_in_luma_samples = sps->pic_width_in_luma_samples,
    .pic_height_in_luma_samples = sps->pic_height_in_luma_samples,
    .sps_max_dec_pic_buffering_minus1 = sps->max_dec_pic_buffering_minus1[sps->max_sub_layers_minus1],
    .bit_depth_luma_minus8 = sps->bit_depth_luma_minus8,
    .bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8,
    .pcm_sample_bit_depth_luma_minus1 = sps->pcm_sample_bit_depth_luma_minus1,
    .pcm_sample_bit_depth_chroma_minus1 = sps->pcm_sample_bit_depth_chroma_minus1,
    .log2_min_luma_coding_block_size_minus3 = sps->log2_min_luma_coding_block_size_minus3,
    .log2_diff_max_min_luma_coding_block_size = sps->log2_diff_max_min_luma_coding_block_size,
    .log2_min_transform_block_size_minus2 = sps->log2_min_transform_block_size_minus2,
    .log2_diff_max_min_transform_block_size = sps->log2_diff_max_min_transform_block_size,
    .log2_min_pcm_luma_coding_block_size_minus3 = sps->log2_min_pcm_luma_coding_block_size_minus3,
    .log2_diff_max_min_pcm_luma_coding_block_size = sps->log2_diff_max_min_pcm_luma_coding_block_size,
    .max_transform_hierarchy_depth_intra = sps->max_transform_hierarchy_depth_intra,
    .max_transform_hierarchy_depth_inter = sps->max_transform_hierarchy_depth_inter,
    .init_qp_minus26 = pps->init_qp_minus26,
    .diff_cu_qp_delta_depth = pps->diff_cu_qp_delta_depth,
    .pps_cb_qp_offset = pps->cb_qp_offset,
    .pps_cr_qp_offset = pps->cr_qp_offset,
    .log2_parallel_merge_level_minus2 = pps->log2_parallel_merge_level_minus2,
    .num_tile_columns_minus1 = pps->num_tile_columns_minus1,
    .num_tile_rows_minus1 = pps->num_tile_rows_minus1,
    .log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4,
    .num_short_term_ref_pic_sets = sps->num_short_term_ref_pic_sets,
    .num_long_term_ref_pic_sps = sps->num_long_term_ref_pics_sps,
    .num_ref_idx_l0_default_active_minus1 = pps->num_ref_idx_l0_default_active_minus1,
    .num_ref_idx_l1_default_active_minus1 = pps->num_ref_idx_l1_default_active_minus1,
    .pps_beta_offset_div2 = pps->beta_offset_div2,
    .pps_tc_offset_div2 = pps->tc_offset_div2,
    .num_extra_slice_header_bits = pps->num_extra_slice_header_bits,
    .st_rps_bits = slice->header.short_term_ref_pic_set_size, /* FIXME missing emulation bits removal */
    .pic_fields.bits = {
      .chroma_format_idc = sps->chroma_format_idc,
      .separate_colour_plane_flag = sps->separate_colour_plane_flag,
      .pcm_enabled_flag = sps->pcm_enabled_flag,
      .scaling_list_enabled_flag = sps->scaling_list_enabled_flag,
      .transform_skip_enabled_flag = pps->transform_skip_enabled_flag,
      .amp_enabled_flag = sps->amp_enabled_flag,
      .strong_intra_smoothing_enabled_flag = sps->strong_intra_smoothing_enabled_flag,
      .sign_data_hiding_enabled_flag = pps->sign_data_hiding_enabled_flag,
      .constrained_intra_pred_flag = pps->constrained_intra_pred_flag,
      .cu_qp_delta_enabled_flag = pps->cu_qp_delta_enabled_flag,
      .weighted_pred_flag = pps->weighted_pred_flag,
      .weighted_bipred_flag = pps->weighted_bipred_flag,
      .transquant_bypass_enabled_flag = pps->transquant_bypass_enabled_flag,
      .tiles_enabled_flag = pps->tiles_enabled_flag,
      .entropy_coding_sync_enabled_flag = pps->entropy_coding_sync_enabled_flag,
      .pps_loop_filter_across_slices_enabled_flag = pps->loop_filter_across_slices_enabled_flag,
      .loop_filter_across_tiles_enabled_flag = pps->loop_filter_across_tiles_enabled_flag,
      .pcm_loop_filter_disabled_flag = sps->pcm_loop_filter_disabled_flag,
      /* Not set by FFMPEG either */
      .NoPicReorderingFlag = 0,
      .NoBiPredFlag = 0,
    },
    .slice_parsing_fields.bits = {
      .lists_modification_present_flag = pps->lists_modification_present_flag,
      .long_term_ref_pics_present_flag = sps->long_term_ref_pics_present_flag,
      .sps_temporal_mvp_enabled_flag = sps->temporal_mvp_enabled_flag,
      .cabac_init_present_flag = pps->cabac_init_present_flag,
      .output_flag_present_flag = pps->output_flag_present_flag,
      .dependent_slice_segments_enabled_flag = pps->dependent_slice_segments_enabled_flag,
      .pps_slice_chroma_qp_offsets_present_flag = pps->slice_chroma_qp_offsets_present_flag,
      .sample_adaptive_offset_enabled_flag = sps->sample_adaptive_offset_enabled_flag,
      .deblocking_filter_override_enabled_flag = pps->deblocking_filter_override_enabled_flag,
      .pps_disable_deblocking_filter_flag = pps->deblocking_filter_disabled_flag,
      .slice_segment_header_extension_present_flag = pps->slice_segment_header_extension_present_flag,
      .RapPicFlag = picture->RapPicFlag,
      .IdrPicFlag = GST_H265_IS_NAL_TYPE_IDR (slice->nalu.type),
      .IntraPicFlag = GST_H265_IS_NAL_TYPE_IRAP (slice->nalu.type),
    },
  };
  /* *INDENT-ON* */

  if (_is_range_extension_profile (self->parent.profile)
      || _is_screen_content_ext_profile (self->parent.profile)) {
    _fill_picture_range_ext_parameter (self, sps, pps);
    if (_is_screen_content_ext_profile (self->parent.profile))
      _fill_screen_content_ext_parameter (self, sps, pps);
  }

  for (i = 0; i <= pps->num_tile_columns_minus1; i++)
    pic_param->base.column_width_minus1[i] = pps->column_width_minus1[i];

  for (i = 0; i <= pps->num_tile_rows_minus1; i++)
    pic_param->base.row_height_minus1[i] = pps->row_height_minus1[i];

  _fill_vaapi_pic (decoder, &pic_param->base.CurrPic, picture);

  /* reference frames */
  {
    GArray *ref_list = gst_h265_dpb_get_pictures_all (dpb);
    guint j;

    i = 0;
    for (j = 0; j < 15 && j < ref_list->len; j++) {
      GstH265Picture *pic = g_array_index (ref_list, GstH265Picture *, j);

      if (pic->ref) {
        _fill_vaapi_pic (decoder, &pic_param->base.ReferenceFrames[i], pic);
        i++;
      }
    }
    g_array_unref (ref_list);

    /* 7.4.3.3.3, the current decoded picture is marked as "used for
       long-term reference". Current picture is not in the DPB now. */
    if (pps->pps_scc_extension_params.pps_curr_pic_ref_enabled_flag && i < 15) {
      pic_param->base.ReferenceFrames[i].picture_id =
          gst_va_decode_picture_get_surface (gst_h265_picture_get_user_data
          (picture));
      pic_param->base.ReferenceFrames[i].pic_order_cnt = picture->pic_order_cnt;
      pic_param->base.ReferenceFrames[i].flags |=
          VA_PICTURE_HEVC_LONG_TERM_REFERENCE;
      pic_param->base.ReferenceFrames[i].flags |=
          _find_frame_rps_type (decoder, picture);
      i++;
    }

    for (; i < 15; i++)
      _init_vaapi_pic (&pic_param->base.ReferenceFrames[i]);
  }

  pic_param_size = _is_range_extension_profile (self->parent.profile)
      || _is_screen_content_ext_profile (self->parent.profile) ?
      sizeof (*pic_param) : sizeof (pic_param->base);
  if (!gst_va_decoder_add_param_buffer (base->decoder, va_pic,
          VAPictureParameterBufferType, pic_param, pic_param_size))
    return GST_FLOW_ERROR;

  if (pps->scaling_list_data_present_flag ||
      (sps->scaling_list_enabled_flag
          && !sps->scaling_list_data_present_flag)) {
    scaling_list = &pps->scaling_list;
    GST_DEBUG_OBJECT (decoder, "Passing scaling list from PPS");
  } else if (sps->scaling_list_enabled_flag &&
      sps->scaling_list_data_present_flag) {
    scaling_list = &sps->scaling_list;
    GST_DEBUG_OBJECT (decoder, "Passing scaling list from SPS");
  }

  if (scaling_list) {
    for (i = 0; i < G_N_ELEMENTS (iq_matrix.ScalingList4x4); i++)
      gst_h265_quant_matrix_4x4_get_raster_from_uprightdiagonal
          (iq_matrix.ScalingList4x4[i], scaling_list->scaling_lists_4x4[i]);

    for (i = 0; i < G_N_ELEMENTS (iq_matrix.ScalingList8x8); i++)
      gst_h265_quant_matrix_8x8_get_raster_from_uprightdiagonal
          (iq_matrix.ScalingList8x8[i], scaling_list->scaling_lists_8x8[i]);

    for (i = 0; i < G_N_ELEMENTS (iq_matrix.ScalingList16x16); i++)
      gst_h265_quant_matrix_16x16_get_raster_from_uprightdiagonal
          (iq_matrix.ScalingList16x16[i], scaling_list->scaling_lists_16x16[i]);

    for (i = 0; i < G_N_ELEMENTS (iq_matrix.ScalingList32x32); i++)
      gst_h265_quant_matrix_32x32_get_raster_from_uprightdiagonal
          (iq_matrix.ScalingList32x32[i], scaling_list->scaling_lists_32x32[i]);

    for (i = 0; i < 6; i++)
      iq_matrix.ScalingListDC16x16[i] =
          scaling_list->scaling_list_dc_coef_minus8_16x16[i] + 8;

    for (i = 0; i < 2; i++)
      iq_matrix.ScalingListDC32x32[i] =
          scaling_list->scaling_list_dc_coef_minus8_32x32[i] + 8;

    if (!gst_va_decoder_add_param_buffer (base->decoder, va_pic,
            VAIQMatrixBufferType, &iq_matrix, sizeof (iq_matrix))) {
      return GST_FLOW_ERROR;
    }
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_h265_dec_new_picture (GstH265Decoder * decoder,
    GstVideoCodecFrame * frame, GstH265Picture * picture)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaH265Dec *self = GST_VA_H265_DEC (decoder);
  GstVaDecodePicture *pic;
  GstBuffer *output_buffer;
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstFlowReturn ret = GST_FLOW_ERROR;

  if (base->need_negotiation) {
    if (!gst_video_decoder_negotiate (vdec)) {
      GST_ERROR_OBJECT (self, "Failed to negotiate with downstream");
      return GST_FLOW_NOT_NEGOTIATED;
    }
  }

  output_buffer = gst_video_decoder_allocate_output_buffer (vdec);
  if (!output_buffer)
    goto error;

  pic = gst_va_decode_picture_new (base->decoder, output_buffer);
  gst_buffer_unref (output_buffer);

  gst_h265_picture_set_user_data (picture, pic,
      (GDestroyNotify) gst_va_decode_picture_free);

  GST_LOG_OBJECT (self, "New va decode picture %p - %#x", pic,
      gst_va_decode_picture_get_surface (pic));

  return GST_FLOW_OK;

error:
  {
    GST_WARNING_OBJECT (self,
        "Failed to allocated output buffer, return %s",
        gst_flow_get_name (ret));
    return ret;
  }
}

static guint
_get_rtformat (GstVaH265Dec * self, guint8 bit_depth_luma,
    guint8 bit_depth_chroma, guint8 chroma_format_idc)
{
  guint8 bit_num = MAX (bit_depth_luma, bit_depth_chroma);

  switch (bit_num) {
    case 11:
    case 12:
      if (chroma_format_idc == 3)
        return VA_RT_FORMAT_YUV444_12;
      if (chroma_format_idc == 2)
        return VA_RT_FORMAT_YUV422_12;
      else
        return VA_RT_FORMAT_YUV420_12;
      break;
    case 9:
    case 10:
      if (chroma_format_idc == 3)
        return VA_RT_FORMAT_YUV444_10;
      if (chroma_format_idc == 2)
        return VA_RT_FORMAT_YUV422_10;
      else
        return VA_RT_FORMAT_YUV420_10;
      break;
    case 8:
      if (chroma_format_idc == 3)
        return VA_RT_FORMAT_YUV444;
      if (chroma_format_idc == 2)
        return VA_RT_FORMAT_YUV422;
      else
        return VA_RT_FORMAT_YUV420;
      break;
    default:
      GST_ERROR_OBJECT (self, "Unsupported chroma format: %d "
          "(with depth luma: %d, with depth chroma: %d)",
          chroma_format_idc, bit_depth_luma, bit_depth_chroma);
      return 0;
  }
}

/* *INDENT-OFF* */
static const struct
{
  GstH265Profile profile;
  VAProfile va_profile;
} profile_map[] = {
#define P(idc, va) { G_PASTE (GST_H265_PROFILE_, idc), G_PASTE (VAProfileHEVC, va) }
  P (MAIN, Main),
  P (MAIN_10, Main10),
  P (MAIN_STILL_PICTURE, Main),
  /*P (MONOCHROME, ),
  P (MONOCHROME_12, ),
  P (MONOCHROME_16, ),*/
  P (MAIN_12, Main12),
  P (MAIN_422_10, Main422_10),
  P (MAIN_422_12, Main422_12),
  P (MAIN_444, Main444),
  P (MAIN_444_10, Main444_10),
  P (MAIN_444_12, Main444_12),
  P (MAIN_INTRA, Main),
  P (MAIN_10_INTRA, Main10),
  /*P (MAIN_12_INTRA, ),
  P (MAIN_422_10_INTRA, ),
  P (MAIN_422_12_INTRA, ),
  P (MAIN_444_INTRA, ),
  P (MAIN_444_10_INTRA, ),
  P (MAIN_444_12_INTRA, ),
  P (MAIN_444_16_INTRA, ),
  P (MAIN_444_STILL_PICTURE, ),
  P (MAIN_444_16_STILL_PICTURE, ),
  P (MONOCHROME_10, ),
  P (HIGH_THROUGHPUT_444, ),
  P (HIGH_THROUGHPUT_444_10, ),
  P (HIGH_THROUGHPUT_444_14, ),
  P (HIGH_THROUGHPUT_444_16_INTRA, ),*/
  P (SCREEN_EXTENDED_MAIN, SccMain),
  P (SCREEN_EXTENDED_MAIN_10, SccMain10),
  P (SCREEN_EXTENDED_MAIN_444, SccMain444),
  P (SCREEN_EXTENDED_MAIN_444_10, SccMain444_10),
  /*P (SCREEN_EXTENDED_HIGH_THROUGHPUT_444, ),
  P (SCREEN_EXTENDED_HIGH_THROUGHPUT_444_10, ),
  P (SCREEN_EXTENDED_HIGH_THROUGHPUT_444_14, ),
  P (MULTIVIEW_MAIN, ),
  P (SCALABLE_MAIN, ),
  P (SCALABLE_MAIN_10, ),
  P (SCALABLE_MONOCHROME, ),
  P (SCALABLE_MONOCHROME_12, ),
  P (SCALABLE_MONOCHROME_16, ),
  P (SCALABLE_MAIN_444, ),
  P (3D_MAIN, ),*/
#undef P
};
/* *INDENT-ON* */

static VAProfile
_get_profile (GstVaH265Dec * self, const GstH265SPS * sps, gint max_dpb_size)
{
  GstH265Decoder *h265_decoder = GST_H265_DECODER (self);
  GstVaBaseDec *base = GST_VA_BASE_DEC (self);
  GstH265Profile profile = gst_h265_get_profile_from_sps ((GstH265SPS *) sps);
  VAProfile profiles[4];
  gint i = 0, j;

  /* 1. The profile directly specified by the SPS should always be the
     first choice. It is the exact one.
     2. The profile in the input caps may contain the compatible profile
     chosen by the upstream element. Upstream element such as the parse
     may already decide the best compatible profile for us. We also need
     to consider it as a choice. */

  for (j = 0; j < G_N_ELEMENTS (profile_map); j++) {
    if (profile_map[j].profile == profile) {
      profiles[i++] = profile_map[j].va_profile;
      break;
    }
  }

  if (h265_decoder->input_state->caps
      && gst_caps_is_fixed (h265_decoder->input_state->caps)) {
    GstH265Profile compatible_profile = GST_H265_PROFILE_INVALID;
    GstStructure *structure;
    const gchar *profile_str;

    structure = gst_caps_get_structure (h265_decoder->input_state->caps, 0);

    profile_str = gst_structure_get_string (structure, "profile");
    if (profile_str)
      compatible_profile = gst_h265_profile_from_string (profile_str);

    if (compatible_profile != profile) {
      GST_INFO_OBJECT (self, "The upstream set the compatible profile %s, "
          "also consider it as a candidate.", profile_str);

      for (j = 0; j < G_N_ELEMENTS (profile_map); j++) {
        if (profile_map[j].profile == compatible_profile) {
          profiles[i++] = profile_map[j].va_profile;
          break;
        }
      }
    }
  }

  for (j = 0; j < i && j < G_N_ELEMENTS (profiles); j++) {
    if (gst_va_decoder_has_profile (base->decoder, profiles[j]))
      return profiles[j];
  }

  GST_ERROR_OBJECT (self, "Unsupported profile: %d", profile);

  return VAProfileNone;
}

static GstFlowReturn
gst_va_h265_dec_new_sequence (GstH265Decoder * decoder, const GstH265SPS * sps,
    gint max_dpb_size)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaH265Dec *self = GST_VA_H265_DEC (decoder);
  GstVideoInfo *info = &base->output_info;
  VAProfile profile;
  gint display_width;
  gint display_height;
  gint padding_left, padding_right, padding_top, padding_bottom;
  guint rt_format;
  gboolean negotiation_needed = FALSE;

  if (self->dpb_size < max_dpb_size)
    self->dpb_size = max_dpb_size;

  if (sps->conformance_window_flag) {
    display_width = sps->crop_rect_width;
    display_height = sps->crop_rect_height;
    padding_left = sps->crop_rect_x;
    padding_right = sps->width - sps->crop_rect_x - display_width;
    padding_top = sps->crop_rect_y;
    padding_bottom = sps->height - sps->crop_rect_y - display_height;
  } else {
    display_width = sps->width;
    display_height = sps->height;
    padding_left = padding_right = padding_top = padding_bottom = 0;
  }

  profile = _get_profile (self, sps, max_dpb_size);
  if (profile == VAProfileNone)
    return GST_FLOW_NOT_NEGOTIATED;

  rt_format = _get_rtformat (self, sps->bit_depth_luma_minus8 + 8,
      sps->bit_depth_chroma_minus8 + 8, sps->chroma_format_idc);
  if (rt_format == 0)
    return GST_FLOW_NOT_NEGOTIATED;

  if (!gst_va_decoder_config_is_equal (base->decoder, profile,
          rt_format, sps->width, sps->height)) {
    base->profile = profile;
    base->rt_format = rt_format;
    base->width = sps->width;
    base->height = sps->height;

    negotiation_needed = TRUE;
    GST_INFO_OBJECT (self, "Format changed to %s [%x] (%dx%d)",
        gst_va_profile_name (profile), rt_format, base->width, base->height);
  }

  if (GST_VIDEO_INFO_WIDTH (info) != display_width ||
      GST_VIDEO_INFO_HEIGHT (info) != display_height) {
    GST_VIDEO_INFO_WIDTH (info) = display_width;
    GST_VIDEO_INFO_HEIGHT (info) = display_height;

    negotiation_needed = TRUE;
    GST_INFO_OBJECT (self, "Resolution changed to %dx%d",
        GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info));
  }

  base->need_valign = GST_VIDEO_INFO_WIDTH (info) < base->width ||
      GST_VIDEO_INFO_HEIGHT (info) < base->height;
  if (base->need_valign) {
    /* *INDENT-OFF* */
    if (base->valign.padding_left != padding_left ||
        base->valign.padding_right != padding_right ||
        base->valign.padding_top != padding_top ||
        base->valign.padding_bottom != padding_bottom) {
      negotiation_needed = TRUE;
      GST_INFO_OBJECT (self, "crop rect changed to (%d,%d)-->(%d,%d)",
          padding_left, padding_top, padding_right, padding_bottom);
    }
    base->valign = (GstVideoAlignment) {
      .padding_left = padding_left,
      .padding_right = padding_right,
      .padding_top = padding_top,
      .padding_bottom = padding_bottom,
    };
    /* *INDENT-ON* */
  }

  base->min_buffers = self->dpb_size + 4;       /* dpb size + scratch surfaces */
  base->need_negotiation = negotiation_needed;
  g_clear_pointer (&base->input_state, gst_video_codec_state_unref);
  base->input_state = gst_video_codec_state_ref (decoder->input_state);

  {
    /* FIXME: We don't have parser API for sps_range_extension, so
     * assuming high_precision_offsets_enabled_flag as zero */
    guint high_precision_offsets_enabled_flag = 0, bitdepthC = 0;

    /* Calculate WpOffsetHalfRangeC: (7-34) */
    bitdepthC = sps->bit_depth_chroma_minus8 + 8;
    self->WpOffsetHalfRangeC =
        1 << (high_precision_offsets_enabled_flag ? (bitdepthC - 1) : 7);
  }

  return GST_FLOW_OK;
}

static GstCaps *
_complete_sink_caps (GstCaps * sinkcaps)
{
  GstCaps *caps = gst_caps_copy (sinkcaps);
  GValue val = G_VALUE_INIT;
  const gchar *streamformat[] = { "hvc1", "hev1", "byte-stream" };
  gint i;

  g_value_init (&val, G_TYPE_STRING);
  g_value_set_string (&val, "au");
  gst_caps_set_value (caps, "alignment", &val);
  g_value_unset (&val);

  gst_value_list_init (&val, G_N_ELEMENTS (streamformat));
  for (i = 0; i < G_N_ELEMENTS (streamformat); i++) {
    GValue v = G_VALUE_INIT;

    g_value_init (&v, G_TYPE_STRING);
    g_value_set_string (&v, streamformat[i]);
    gst_value_list_append_value (&val, &v);
    g_value_unset (&v);
  }
  gst_caps_set_value (caps, "stream-format", &val);
  g_value_unset (&val);

  return caps;
}

static GstCaps *
gst_va_h265_dec_getcaps (GstVideoDecoder * decoder, GstCaps * filter)
{
  GstCaps *sinkcaps, *caps = NULL, *tmp;
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);

  if (base->decoder)
    caps = gst_va_decoder_get_sinkpad_caps (base->decoder);

  if (caps) {
    sinkcaps = _complete_sink_caps (caps);
    gst_caps_unref (caps);
    if (filter) {
      tmp = gst_caps_intersect_full (filter, sinkcaps,
          GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (sinkcaps);
      caps = tmp;
    } else {
      caps = sinkcaps;
    }
    GST_LOG_OBJECT (base, "Returning caps %" GST_PTR_FORMAT, caps);
  } else if (!caps) {
    caps = gst_video_decoder_proxy_getcaps (decoder, NULL, filter);
  }

  return caps;
}

static void
gst_va_h265_dec_dispose (GObject * object)
{
  g_free (GST_VA_H265_DEC (object)->prev_slice.data);

  gst_va_base_dec_close (GST_VIDEO_DECODER (object));

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_va_h265_dec_class_init (gpointer g_class, gpointer class_data)
{
  GstCaps *src_doc_caps, *sink_doc_caps;
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstH265DecoderClass *h265decoder_class = GST_H265_DECODER_CLASS (g_class);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (g_class);
  struct CData *cdata = class_data;
  gchar *long_name;

  if (cdata->description) {
    long_name = g_strdup_printf ("VA-API H.265 Decoder in %s",
        cdata->description);
  } else {
    long_name = g_strdup ("VA-API H.265 Decoder");
  }

  gst_element_class_set_metadata (element_class, long_name,
      "Codec/Decoder/Video/Hardware",
      "VA-API based H.265 video decoder",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com>");

  sink_doc_caps = gst_caps_from_string (sink_caps_str);
  src_doc_caps = gst_caps_from_string (src_caps_str);

  parent_class = g_type_class_peek_parent (g_class);

  /**
   * GstVaH265Dec:device-path:
   *
   * It shows the DRM device path used for the VA operation, if any.
   *
   * Since: 1.22
   */
  gst_va_base_dec_class_init (GST_VA_BASE_DEC_CLASS (g_class), HEVC,
      cdata->render_device_path, cdata->sink_caps, cdata->src_caps,
      src_doc_caps, sink_doc_caps);

  gobject_class->dispose = gst_va_h265_dec_dispose;

  decoder_class->getcaps = GST_DEBUG_FUNCPTR (gst_va_h265_dec_getcaps);

  h265decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_va_h265_dec_new_sequence);
  h265decoder_class->decode_slice =
      GST_DEBUG_FUNCPTR (gst_va_h265_dec_decode_slice);

  h265decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_va_h265_dec_new_picture);
  h265decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_va_h265_dec_output_picture);
  h265decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_va_h265_dec_start_picture);
  h265decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_va_h265_dec_end_picture);

  g_free (long_name);
  g_free (cdata->description);
  g_free (cdata->render_device_path);
  gst_caps_unref (cdata->src_caps);
  gst_caps_unref (cdata->sink_caps);
  g_free (cdata);
}

static void
gst_va_h265_dec_init (GTypeInstance * instance, gpointer g_class)
{
  gst_va_base_dec_init (GST_VA_BASE_DEC (instance), GST_CAT_DEFAULT);
  gst_h265_decoder_set_process_ref_pic_lists (GST_H265_DECODER (instance),
      TRUE);
}

static gpointer
_register_debug_category (gpointer data)
{
  GST_DEBUG_CATEGORY_INIT (gst_va_h265dec_debug, "vah265dec", 0,
      "VA H265 decoder");

  return NULL;
}

gboolean
gst_va_h265_dec_register (GstPlugin * plugin, GstVaDevice * device,
    GstCaps * sink_caps, GstCaps * src_caps, guint rank)
{
  static GOnce debug_once = G_ONCE_INIT;
  GType type;
  GTypeInfo type_info = {
    .class_size = sizeof (GstVaH265DecClass),
    .class_init = gst_va_h265_dec_class_init,
    .instance_size = sizeof (GstVaH265Dec),
    .instance_init = gst_va_h265_dec_init,
  };
  struct CData *cdata;
  gboolean ret;
  gchar *type_name, *feature_name;

  g_return_val_if_fail (GST_IS_PLUGIN (plugin), FALSE);
  g_return_val_if_fail (GST_IS_VA_DEVICE (device), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (sink_caps), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (src_caps), FALSE);

  cdata = g_new (struct CData, 1);
  cdata->description = NULL;
  cdata->render_device_path = g_strdup (device->render_device_path);
  cdata->sink_caps = _complete_sink_caps (sink_caps);
  cdata->src_caps = gst_caps_ref (src_caps);

  /* class data will be leaked if the element never gets instantiated */
  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  type_info.class_data = cdata;

  gst_va_create_feature_name (device, "GstVaH265Dec", "GstVa%sH265Dec",
      &type_name, "vah265dec", "va%sh265dec", &feature_name,
      &cdata->description, &rank);

  g_once (&debug_once, _register_debug_category, NULL);

  type = g_type_register_static (GST_TYPE_H265_DECODER,
      type_name, &type_info, 0);

  ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
