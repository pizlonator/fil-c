/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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
 * SECTION:element-vah264dec
 * @title: vah264dec
 * @short_description: A VA-API based H264 video decoder
 *
 * vah264dec decodes H264 bitstreams to VA surfaces using the
 * installed and chosen [VA-API](https://01.org/linuxmedia/vaapi)
 * driver.
 *
 * The decoding surfaces can be mapped onto main memory as video
 * frames.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=big_buck_bunny.mov ! parsebin ! vah264dec ! autovideosink
 * ```
 *
 * Since: 1.18
 *
 */

/* ToDo:
 *
 * + mutiview and stereo profiles
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvah264dec.h"

#include "gstvabasedec.h"

GST_DEBUG_CATEGORY_STATIC (gst_va_h264dec_debug);
#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT gst_va_h264dec_debug
#else
#define GST_CAT_DEFAULT NULL
#endif

#define GST_VA_H264_DEC(obj)           ((GstVaH264Dec *) obj)
#define GST_VA_H264_DEC_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_FROM_INSTANCE (obj), GstVaH264DecClass))
#define GST_VA_H264_DEC_CLASS(klass)   ((GstVaH264DecClass *) klass)

typedef struct _GstVaH264Dec GstVaH264Dec;
typedef struct _GstVaH264DecClass GstVaH264DecClass;

struct _GstVaH264DecClass
{
  GstVaBaseDecClass parent_class;
};

struct _GstVaH264Dec
{
  GstVaBaseDec parent;

  gint dpb_size;

  /* Used to fill VAPictureParameterBufferH264.ReferenceFrames */
  GArray *ref_list;

  gboolean interlaced;
};

static GstElementClass *parent_class = NULL;

/* *INDENT-OFF* */
static const gchar *src_caps_str =
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VA,
        "{ NV12, P010_10LE }") " ;"
    GST_VIDEO_CAPS_MAKE ("{ NV12, P010_10LE }");
/* *INDENT-ON* */

static const gchar *sink_caps_str = "video/x-h264";

static GstFlowReturn
gst_va_h264_dec_end_picture (GstH264Decoder * decoder, GstH264Picture * picture)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaDecodePicture *va_pic;

  GST_LOG_OBJECT (base, "end picture %p, (poc %d)",
      picture, picture->pic_order_cnt);

  va_pic = gst_h264_picture_get_user_data (picture);

  if (!gst_va_decoder_decode (base->decoder, va_pic))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_h264_dec_output_picture (GstH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstH264Picture * picture)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaH264Dec *self = GST_VA_H264_DEC (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  gboolean ret;

  GST_LOG_OBJECT (self,
      "Outputting picture %p (poc %d)", picture, picture->pic_order_cnt);

  ret = gst_va_base_dec_process_output (base, frame,
      GST_CODEC_PICTURE (picture)->discont_state, picture->buffer_flags);
  gst_h264_picture_unref (picture);

  if (ret)
    return gst_video_decoder_finish_frame (vdec, frame);
  return GST_FLOW_ERROR;
}

static void
_init_vaapi_pic (VAPictureH264 * va_picture)
{
  va_picture->picture_id = VA_INVALID_ID;
  va_picture->frame_idx = 0;
  va_picture->flags = VA_PICTURE_H264_INVALID;
  va_picture->TopFieldOrderCnt = 0;
  va_picture->BottomFieldOrderCnt = 0;
}

static void
_fill_vaapi_pic (VAPictureH264 * va_picture, GstH264Picture * picture,
    gboolean merge_other_field)
{
  GstVaDecodePicture *va_pic;

  va_pic = gst_h264_picture_get_user_data (picture);

  if (!va_pic) {
    _init_vaapi_pic (va_picture);
    return;
  }

  va_picture->picture_id = gst_va_decode_picture_get_surface (va_pic);
  va_picture->flags = 0;

  if (GST_H264_PICTURE_IS_LONG_TERM_REF (picture)) {
    va_picture->flags |= VA_PICTURE_H264_LONG_TERM_REFERENCE;
    va_picture->frame_idx = picture->long_term_frame_idx;
  } else {
    if (GST_H264_PICTURE_IS_SHORT_TERM_REF (picture))
      va_picture->flags |= VA_PICTURE_H264_SHORT_TERM_REFERENCE;
    va_picture->frame_idx = picture->frame_num;
  }

  switch (picture->field) {
    case GST_H264_PICTURE_FIELD_FRAME:
      va_picture->TopFieldOrderCnt = picture->top_field_order_cnt;
      va_picture->BottomFieldOrderCnt = picture->bottom_field_order_cnt;
      break;
    case GST_H264_PICTURE_FIELD_TOP_FIELD:
      if (merge_other_field && picture->other_field) {
        va_picture->BottomFieldOrderCnt =
            picture->other_field->bottom_field_order_cnt;
      } else {
        va_picture->flags |= VA_PICTURE_H264_TOP_FIELD;
        va_picture->BottomFieldOrderCnt = 0;
      }
      va_picture->TopFieldOrderCnt = picture->top_field_order_cnt;
      break;
    case GST_H264_PICTURE_FIELD_BOTTOM_FIELD:
      if (merge_other_field && picture->other_field) {
        va_picture->TopFieldOrderCnt =
            picture->other_field->top_field_order_cnt;
      } else {
        va_picture->flags |= VA_PICTURE_H264_BOTTOM_FIELD;
        va_picture->TopFieldOrderCnt = 0;
      }
      va_picture->BottomFieldOrderCnt = picture->bottom_field_order_cnt;
      break;
    default:
      va_picture->TopFieldOrderCnt = 0;
      va_picture->BottomFieldOrderCnt = 0;
      break;
  }
}

/* fill the VA API reference picture lists from the GstCodec reference
 * picture list */
static void
_fill_ref_pic_list (VAPictureH264 va_reflist[32], GArray * reflist,
    GstH264Picture * current_picture)
{
  guint i;

  for (i = 0; i < reflist->len; i++) {
    GstH264Picture *picture = g_array_index (reflist, GstH264Picture *, i);

    if (picture) {
      _fill_vaapi_pic (&va_reflist[i], picture,
          GST_H264_PICTURE_IS_FRAME (current_picture));
    } else {
      /* list might include null picture if reference picture was missing */
      _init_vaapi_pic (&va_reflist[i]);
    }
  }

  for (; i < 32; i++)
    _init_vaapi_pic (&va_reflist[i]);
}

static void
_fill_pred_weight_table (GstH264SliceHdr * header,
    VASliceParameterBufferH264 * slice_param)
{
  GstH264PPS *pps;
  GstH264SPS *sps;
  guint num_weight_tables = 0;
  gint i, j;

  pps = header->pps;
  sps = pps->sequence;

  if (pps->weighted_pred_flag
      && (GST_H264_IS_P_SLICE (header) || GST_H264_IS_SP_SLICE (header)))
    num_weight_tables = 1;
  else if (pps->weighted_bipred_idc == 1 && GST_H264_IS_B_SLICE (header))
    num_weight_tables = 2;

  if (num_weight_tables == 0)
    return;

  slice_param->luma_log2_weight_denom =
      header->pred_weight_table.luma_log2_weight_denom;
  slice_param->chroma_log2_weight_denom =
      header->pred_weight_table.chroma_log2_weight_denom;

  /* VA API also wants the inferred (default) values, not only what is
   * available in the bitstream (7.4.3.2). */

  slice_param->luma_weight_l0_flag = 1;
  for (i = 0; i <= slice_param->num_ref_idx_l0_active_minus1; i++) {
    slice_param->luma_weight_l0[i] =
        header->pred_weight_table.luma_weight_l0[i];
    slice_param->luma_offset_l0[i] =
        header->pred_weight_table.luma_offset_l0[i];
  }

  slice_param->chroma_weight_l0_flag = sps->chroma_array_type != 0;
  if (slice_param->chroma_weight_l0_flag) {
    for (i = 0; i <= slice_param->num_ref_idx_l0_active_minus1; i++) {
      for (j = 0; j < 2; j++) {
        slice_param->chroma_weight_l0[i][j] =
            header->pred_weight_table.chroma_weight_l0[i][j];
        slice_param->chroma_offset_l0[i][j] =
            header->pred_weight_table.chroma_offset_l0[i][j];
      }
    }
  }

  if (num_weight_tables == 1)
    return;

  slice_param->luma_weight_l1_flag = 1;
  for (i = 0; i <= slice_param->num_ref_idx_l1_active_minus1; i++) {
    slice_param->luma_weight_l1[i] =
        header->pred_weight_table.luma_weight_l1[i];
    slice_param->luma_offset_l1[i] =
        header->pred_weight_table.luma_offset_l1[i];
  }

  slice_param->chroma_weight_l1_flag = sps->chroma_array_type != 0;
  if (slice_param->chroma_weight_l1_flag) {
    for (i = 0; i <= slice_param->num_ref_idx_l1_active_minus1; i++) {
      for (j = 0; j < 2; j++) {
        slice_param->chroma_weight_l1[i][j] =
            header->pred_weight_table.chroma_weight_l1[i][j];
        slice_param->chroma_offset_l1[i][j] =
            header->pred_weight_table.chroma_offset_l1[i][j];
      }
    }
  }
}

static inline guint
_get_slice_data_bit_offset (GstH264SliceHdr * header, guint nal_header_bytes)
{
  guint epb_count;

  epb_count = header->n_emulation_prevention_bytes;
  return 8 * nal_header_bytes + header->header_size - epb_count * 8;
}

static GstFlowReturn
gst_va_h264_dec_decode_slice (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GArray * ref_pic_list0,
    GArray * ref_pic_list1)
{
  GstH264SliceHdr *header = &slice->header;
  GstH264NalUnit *nalu = &slice->nalu;
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaDecodePicture *va_pic;
  VASliceParameterBufferH264 slice_param;

  /* *INDENT-OFF* */
  slice_param = (VASliceParameterBufferH264) {
    .slice_data_size = nalu->size,
    .slice_data_offset = 0,
    .slice_data_flag = VA_SLICE_DATA_FLAG_ALL,
    .slice_data_bit_offset =
        _get_slice_data_bit_offset (header, nalu->header_bytes),
    .first_mb_in_slice = header->first_mb_in_slice,
    .slice_type = header->type % 5,
    .direct_spatial_mv_pred_flag = header->direct_spatial_mv_pred_flag,
    .cabac_init_idc = header->cabac_init_idc,
    .slice_qp_delta = header->slice_qp_delta,
    .disable_deblocking_filter_idc = header->disable_deblocking_filter_idc,
    .slice_alpha_c0_offset_div2 = header->slice_alpha_c0_offset_div2,
    .slice_beta_offset_div2 = header->slice_beta_offset_div2,
    .num_ref_idx_l0_active_minus1 = header->num_ref_idx_l0_active_minus1,
    .num_ref_idx_l1_active_minus1 = header->num_ref_idx_l1_active_minus1,
  };
  /* *INDENT-ON* */

  _fill_ref_pic_list (slice_param.RefPicList0, ref_pic_list0, picture);
  _fill_ref_pic_list (slice_param.RefPicList1, ref_pic_list1, picture);

  _fill_pred_weight_table (header, &slice_param);

  va_pic = gst_h264_picture_get_user_data (picture);

  if (!gst_va_decoder_add_slice_buffer (base->decoder, va_pic, &slice_param,
          sizeof (slice_param), slice->nalu.data + slice->nalu.offset,
          slice->nalu.size)) {
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_h264_dec_start_picture (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GstH264Dpb * dpb)
{
  GstVaH264Dec *self = GST_VA_H264_DEC (decoder);
  GstH264PPS *pps;
  GstH264SPS *sps;
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaDecodePicture *va_pic;
  VAIQMatrixBufferH264 iq_matrix = { 0, };
  VAPictureParameterBufferH264 pic_param;
  guint i;
  GArray *ref_list = self->ref_list;

  va_pic = gst_h264_picture_get_user_data (picture);

  pps = slice->header.pps;
  sps = pps->sequence;

  /* *INDENT-OFF* */
  pic_param = (VAPictureParameterBufferH264) {
    /* .CurrPic */
    /* .ReferenceFrames */
    .picture_width_in_mbs_minus1 = sps->pic_width_in_mbs_minus1,
    .picture_height_in_mbs_minus1 =
        ((sps->pic_height_in_map_units_minus1 + 1) <<
            !sps->frame_mbs_only_flag) -1,
    .bit_depth_luma_minus8 = sps->bit_depth_luma_minus8,
    .bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8,
    .num_ref_frames = sps->num_ref_frames,
    .seq_fields.bits = {
      .chroma_format_idc = sps->chroma_format_idc,
      .residual_colour_transform_flag = sps->separate_colour_plane_flag,
      .gaps_in_frame_num_value_allowed_flag =
          sps->gaps_in_frame_num_value_allowed_flag,
      .frame_mbs_only_flag = sps->frame_mbs_only_flag,
      .mb_adaptive_frame_field_flag = sps->mb_adaptive_frame_field_flag,
      .direct_8x8_inference_flag = sps->direct_8x8_inference_flag,
      .MinLumaBiPredSize8x8 = sps->level_idc >= 31, /* A.3.3.2 */
      .log2_max_frame_num_minus4 = sps->log2_max_frame_num_minus4,
      .pic_order_cnt_type = sps->pic_order_cnt_type,
      .log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4,
      .delta_pic_order_always_zero_flag = sps->delta_pic_order_always_zero_flag,
    },
    .pic_init_qp_minus26 = pps->pic_init_qp_minus26,
    .pic_init_qs_minus26 = pps->pic_init_qs_minus26,
    .chroma_qp_index_offset = pps->chroma_qp_index_offset,
    .second_chroma_qp_index_offset = pps->second_chroma_qp_index_offset,
    .pic_fields.bits = {
      .entropy_coding_mode_flag = pps->entropy_coding_mode_flag,
      .weighted_pred_flag = pps->weighted_pred_flag,
      .weighted_bipred_idc = pps->weighted_bipred_idc,
      .transform_8x8_mode_flag = pps->transform_8x8_mode_flag,
      .field_pic_flag = slice->header.field_pic_flag,
      .constrained_intra_pred_flag = pps->constrained_intra_pred_flag,
      .pic_order_present_flag = pps->pic_order_present_flag,
      .deblocking_filter_control_present_flag =
          pps->deblocking_filter_control_present_flag,
      .redundant_pic_cnt_present_flag = pps->redundant_pic_cnt_present_flag,
      .reference_pic_flag = picture->nal_ref_idc != 0,
    },
    .frame_num = slice->header.frame_num,
  };
  /* *INDENT-ON* */

  _fill_vaapi_pic (&pic_param.CurrPic, picture, FALSE);

  /* reference frames */
  {
    guint ref_frame_idx = 0;
    g_array_set_size (ref_list, 0);

    gst_h264_dpb_get_pictures_short_term_ref (dpb, FALSE, FALSE, ref_list);
    for (i = 0; ref_frame_idx < 16 && i < ref_list->len; i++) {
      GstH264Picture *pic = g_array_index (ref_list, GstH264Picture *, i);
      _fill_vaapi_pic (&pic_param.ReferenceFrames[ref_frame_idx++], pic, TRUE);
    }
    g_array_set_size (ref_list, 0);

    gst_h264_dpb_get_pictures_long_term_ref (dpb, FALSE, ref_list);
    for (i = 0; ref_frame_idx < 16 && i < ref_list->len; i++) {
      GstH264Picture *pic = g_array_index (ref_list, GstH264Picture *, i);
      _fill_vaapi_pic (&pic_param.ReferenceFrames[ref_frame_idx++], pic, TRUE);
    }
    g_array_set_size (ref_list, 0);

    for (; ref_frame_idx < 16; ref_frame_idx++)
      _init_vaapi_pic (&pic_param.ReferenceFrames[ref_frame_idx]);
  }

  if (!gst_va_decoder_add_param_buffer (base->decoder, va_pic,
          VAPictureParameterBufferType, &pic_param, sizeof (pic_param)))
    return GST_FLOW_ERROR;

  /* there are always 6 4x4 scaling lists */
  for (i = 0; i < 6; i++) {
    gst_h264_quant_matrix_4x4_get_raster_from_zigzag (iq_matrix.ScalingList4x4
        [i], pps->scaling_lists_4x4[i]);
  }

  /* We need the first 2 entries (Y intra and Y inter for YCbCr 4:2:2 and
   * less, and the full 6 entries for 4:4:4, see Table 7-2 of the spec for
   * more details.
   * But VA API only define the first 2 entries, so we may lose scaling
   * list info for 4:4:4 stream. */
  if (pps->sequence->chroma_format_idc == 3)
    GST_WARNING_OBJECT (self, "We do not have scaling list entries "
        "for U/V planes in 4:4:4 stream. It may have artifact if "
        "those scaling lists are not default value.");

  for (i = 0; i < 2; i++) {
    gst_h264_quant_matrix_8x8_get_raster_from_zigzag (iq_matrix.ScalingList8x8
        [i], pps->scaling_lists_8x8[i]);
  }

  if (!gst_va_decoder_add_param_buffer (base->decoder, va_pic,
          VAIQMatrixBufferType, &iq_matrix, sizeof (iq_matrix)))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_h264_dec_new_picture (GstH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstH264Picture * picture)
{
  GstVaH264Dec *self = GST_VA_H264_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaDecodePicture *pic;
  GstFlowReturn ret;

  ret = gst_va_base_dec_prepare_output_frame (base, frame);
  if (ret != GST_FLOW_OK)
    goto error;

  pic = gst_va_decode_picture_new (base->decoder, frame->output_buffer);

  gst_h264_picture_set_user_data (picture, pic,
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

static GstFlowReturn
gst_va_h264_dec_new_field_picture (GstH264Decoder * decoder,
    GstH264Picture * first_field, GstH264Picture * second_field)
{
  GstVaDecodePicture *first_pic, *second_pic;
  GstVaH264Dec *self = GST_VA_H264_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);

  first_pic = gst_h264_picture_get_user_data (first_field);
  if (!first_pic)
    return GST_FLOW_ERROR;

  second_pic = gst_va_decode_picture_new (base->decoder, first_pic->gstbuffer);
  gst_h264_picture_set_user_data (second_field, second_pic,
      (GDestroyNotify) gst_va_decode_picture_free);

  GST_LOG_OBJECT (self, "New va decode picture %p - %#x", second_pic,
      gst_va_decode_picture_get_surface (second_pic));

  return GST_FLOW_OK;
}

static inline guint
_get_num_views (const GstH264SPS * sps)
{
  return 1 + (sps->extension_type == GST_H264_NAL_EXTENSION_MVC ?
      sps->extension.mvc.num_views_minus1 : 0);
}

static guint
_get_rtformat (GstVaH264Dec * self, guint8 bit_depth_luma,
    guint8 chroma_format_idc)
{
  switch (bit_depth_luma) {
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
          "(with depth luma: %d)", chroma_format_idc, bit_depth_luma);
      return 0;
  }
}

/* *INDENT-OFF* */
static const struct
{
  GstH264Profile profile_idc;
  VAProfile va_profile;
} profile_map[] = {
#define P(idc, va) { G_PASTE (GST_H264_PROFILE_, idc), G_PASTE (VAProfileH264, va) }
  /* P (BASELINE, ), */
  P (MAIN, Main),
  /* P (EXTENDED, ), */
  P (HIGH, High),
  /* P (HIGH10, ), */
  /* P (HIGH_422, ), */
  /* P (HIGH_444, ), */
  P (MULTIVIEW_HIGH, MultiviewHigh),
  P (STEREO_HIGH, StereoHigh),
  /* P (SCALABLE_BASELINE, ), */
  /* P (SCALABLE_HIGH, ), */
#undef P
};
/* *INDENT-ON* */

static VAProfile
_get_profile (GstVaH264Dec * self, const GstH264SPS * sps, gint max_dpb_size)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (self);
  VAProfile profiles[4];
  gint i = 0, j;

  for (j = 0; j < G_N_ELEMENTS (profile_map); j++) {
    if (profile_map[j].profile_idc == sps->profile_idc) {
      profiles[i++] = profile_map[j].va_profile;
      break;
    }
  }

  switch (sps->profile_idc) {
    case GST_H264_PROFILE_BASELINE:
    {
      GstH264DecoderCompliance compliance = GST_H264_DECODER_COMPLIANCE_STRICT;

      g_object_get (G_OBJECT (self), "compliance", &compliance, NULL);

      /* A.2 compliant or not strict */
      if (sps->constraint_set0_flag || sps->constraint_set1_flag
          || sps->constraint_set2_flag
          || compliance != GST_H264_DECODER_COMPLIANCE_STRICT) {
        profiles[i++] = VAProfileH264ConstrainedBaseline;
        profiles[i++] = VAProfileH264Main;
      }

      break;
    }
    case GST_H264_PROFILE_EXTENDED:
      if (sps->constraint_set1_flag) {  /* A.2.2 (main profile) */
        profiles[i++] = VAProfileH264Main;
      }
      break;
    case GST_H264_PROFILE_MULTIVIEW_HIGH:
      if (_get_num_views (sps) == 2) {
        profiles[i++] = VAProfileH264StereoHigh;
      }
      if (max_dpb_size <= 16 /* && i965 driver */ ) {
        profiles[i++] = VAProfileH264MultiviewHigh;
      }
    default:
      break;
  }

  for (j = 0; j < i && j < G_N_ELEMENTS (profiles); j++) {
    if (gst_va_decoder_has_profile (base->decoder, profiles[j]))
      return profiles[j];
  }

  GST_ERROR_OBJECT (self, "Unsupported profile: %d", sps->profile_idc);

  return VAProfileNone;
}

static GstFlowReturn
gst_va_h264_dec_new_sequence (GstH264Decoder * decoder, const GstH264SPS * sps,
    gint max_dpb_size)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaH264Dec *self = GST_VA_H264_DEC (decoder);
  GstVideoInfo *info = &base->output_info;
  VAProfile profile;
  gint display_width;
  gint display_height;
  gint padding_left, padding_right, padding_top, padding_bottom;
  guint rt_format;
  gboolean negotiation_needed = FALSE;
  gboolean interlaced;

  if (self->dpb_size < max_dpb_size)
    self->dpb_size = max_dpb_size;

  if (sps->frame_cropping_flag) {
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
      sps->chroma_format_idc);
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

  interlaced = !sps->frame_mbs_only_flag;
  if (self->interlaced != interlaced) {
    self->interlaced = interlaced;
    GST_VIDEO_INFO_INTERLACE_MODE (info) = interlaced ?
        GST_VIDEO_INTERLACE_MODE_MIXED : GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
    negotiation_needed = TRUE;
    GST_INFO_OBJECT (self, "Interlaced mode changed to %d", interlaced);
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

  return GST_FLOW_OK;
}

static inline void
_append_str (GValue * list, const gchar * str)
{
  GValue v = G_VALUE_INIT;

  g_value_init (&v, G_TYPE_STRING);
  g_value_set_string (&v, str);
  gst_value_list_append_value (list, &v);
  g_value_unset (&v);
}

static GstCaps *
_complete_sink_caps (GstCaps * sinkcaps)
{
  GstCaps *caps = gst_caps_copy (sinkcaps);
  GValue val = G_VALUE_INIT;
  const GValue *profiles;
  GstStructure *st;
  const gchar *streamformat[] = { "avc", "avc3", "byte-stream" };
  const gchar *high_synthetic[] = { "progressive-high", "constrained-high" };
  guint i, j, siz;
  gboolean baseline = FALSE;

  g_value_init (&val, G_TYPE_STRING);
  g_value_set_string (&val, "au");
  gst_caps_set_value (caps, "alignment", &val);
  g_value_unset (&val);

  gst_value_list_init (&val, G_N_ELEMENTS (streamformat));
  for (i = 0; i < G_N_ELEMENTS (streamformat); i++)
    _append_str (&val, streamformat[i]);
  gst_caps_set_value (caps, "stream-format", &val);
  g_value_unset (&val);

  /* add synthetic profiles */
  st = gst_caps_get_structure (caps, 0);
  profiles = gst_structure_get_value (st, "profile");
  siz = gst_value_list_get_size (profiles);
  gst_value_list_init (&val, siz);
  for (i = 0; i < siz; i++) {
    const gchar *profile =
        g_value_get_string (gst_value_list_get_value (profiles, i));

    _append_str (&val, profile);

    if (g_strcmp0 (profile, "high") == 0) {
      for (j = 0; j < G_N_ELEMENTS (high_synthetic); j++)
        _append_str (&val, high_synthetic[j]);
    }
    if (!baseline && ((g_strcmp0 (profile, "main") == 0)
            || g_strcmp0 (profile, "constrained-baseline") == 0)) {
      _append_str (&val, "baseline");
      baseline = TRUE;
    }
  }
  gst_caps_set_value (caps, "profile", &val);
  g_value_unset (&val);

  return caps;
}

static GstCaps *
gst_va_h264_dec_getcaps (GstVideoDecoder * decoder, GstCaps * filter)
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
  } else {
    caps = gst_video_decoder_proxy_getcaps (decoder, NULL, filter);
  }

  return caps;
}

static void
gst_va_h264_dec_dispose (GObject * object)
{
  GstVaH264Dec *self = GST_VA_H264_DEC (object);

  gst_va_base_dec_close (GST_VIDEO_DECODER (object));
  g_clear_pointer (&self->ref_list, g_array_unref);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_va_h264_dec_class_init (gpointer g_class, gpointer class_data)
{
  GstCaps *src_doc_caps, *sink_doc_caps;
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstH264DecoderClass *h264decoder_class = GST_H264_DECODER_CLASS (g_class);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (g_class);
  struct CData *cdata = class_data;
  gchar *long_name;

  if (cdata->description) {
    long_name = g_strdup_printf ("VA-API H.264 Decoder in %s",
        cdata->description);
  } else {
    long_name = g_strdup ("VA-API H.264 Decoder");
  }

  gst_element_class_set_metadata (element_class, long_name,
      "Codec/Decoder/Video/Hardware",
      "VA-API based H.264 video decoder",
      "Víctor Jáquez <vjaquez@igalia.com>");

  sink_doc_caps = gst_caps_from_string (sink_caps_str);
  src_doc_caps = gst_caps_from_string (src_caps_str);

  parent_class = g_type_class_peek_parent (g_class);

  /**
   * GstVaH264Dec:device-path:
   *
   * It shows the DRM device path used for the VA operation, if any.
   *
   * Since: 1.22
   */
  gst_va_base_dec_class_init (GST_VA_BASE_DEC_CLASS (g_class), H264,
      cdata->render_device_path, cdata->sink_caps, cdata->src_caps,
      src_doc_caps, sink_doc_caps);

  gobject_class->dispose = gst_va_h264_dec_dispose;

  decoder_class->getcaps = GST_DEBUG_FUNCPTR (gst_va_h264_dec_getcaps);

  h264decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_va_h264_dec_new_sequence);
  h264decoder_class->decode_slice =
      GST_DEBUG_FUNCPTR (gst_va_h264_dec_decode_slice);

  h264decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_va_h264_dec_new_picture);
  h264decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_va_h264_dec_output_picture);
  h264decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_va_h264_dec_start_picture);
  h264decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_va_h264_dec_end_picture);
  h264decoder_class->new_field_picture =
      GST_DEBUG_FUNCPTR (gst_va_h264_dec_new_field_picture);

  g_free (long_name);
  g_free (cdata->description);
  g_free (cdata->render_device_path);
  gst_caps_unref (cdata->src_caps);
  gst_caps_unref (cdata->sink_caps);
  g_free (cdata);
}

static void
gst_va_h264_dec_init (GTypeInstance * instance, gpointer g_class)
{
  GstVaH264Dec *self = GST_VA_H264_DEC (instance);

  gst_va_base_dec_init (GST_VA_BASE_DEC (instance), GST_CAT_DEFAULT);
  gst_h264_decoder_set_process_ref_pic_lists (GST_H264_DECODER (instance),
      TRUE);

  self->ref_list = g_array_sized_new (FALSE, TRUE,
      sizeof (GstH264Picture *), 16);
  g_array_set_clear_func (self->ref_list,
      (GDestroyNotify) gst_clear_h264_picture);
}

static gpointer
_register_debug_category (gpointer data)
{
  GST_DEBUG_CATEGORY_INIT (gst_va_h264dec_debug, "vah264dec", 0,
      "VA h264 decoder");

  return NULL;
}

gboolean
gst_va_h264_dec_register (GstPlugin * plugin, GstVaDevice * device,
    GstCaps * sink_caps, GstCaps * src_caps, guint rank)
{
  static GOnce debug_once = G_ONCE_INIT;
  GType type;
  GTypeInfo type_info = {
    .class_size = sizeof (GstVaH264DecClass),
    .class_init = gst_va_h264_dec_class_init,
    .instance_size = sizeof (GstVaH264Dec),
    .instance_init = gst_va_h264_dec_init,
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

  gst_va_create_feature_name (device, "GstVaH264Dec", "GstVa%sH264Dec",
      &type_name, "vah264dec", "va%sh264dec", &feature_name,
      &cdata->description, &rank);

  g_once (&debug_once, _register_debug_category, NULL);

  type = g_type_register_static (GST_TYPE_H264_DECODER,
      type_name, &type_info, 0);

  ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
