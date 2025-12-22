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

#include "gstdxvah264decoder.h"
#include <string.h>
#include <vector>

#include "gstdxvatypedef.h"

GST_DEBUG_CATEGORY_STATIC (gst_dxva_h264_decoder_debug);
#define GST_CAT_DEFAULT gst_dxva_h264_decoder_debug

/* *INDENT-OFF* */
struct _GstDxvaH264DecoderPrivate
{
  DXVA_PicParams_H264 pic_params;
  DXVA_Qmatrix_H264 iq_matrix;

  std::vector<DXVA_Slice_H264_Short> slice_list;
  std::vector<guint8> bitstream_buffer;
  GPtrArray *ref_pics = nullptr;

  gint crop_x = 0;
  gint crop_y = 0;
  gint width = 0;
  gint height = 0;
  gint coded_width = 0;
  gint coded_height = 0;
  gint bitdepth = 0;
  guint8 chroma_format_idc = 0;
  GstVideoFormat out_format = GST_VIDEO_FORMAT_UNKNOWN;
  gboolean interlaced = FALSE;
  gint max_dpb_size = 0;

  gboolean configured = FALSE;
};
/* *INDENT-ON* */

static void gst_dxva_h264_decoder_finalize (GObject * object);

static gboolean gst_dxva_h264_decoder_start (GstVideoDecoder * decoder);

static GstFlowReturn
gst_dxva_h264_decoder_new_sequence (GstH264Decoder * decoder,
    const GstH264SPS * sps, gint max_dpb_size);
static GstFlowReturn
gst_dxva_h264_decoder_new_picture (GstH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstH264Picture * picture);
static GstFlowReturn
gst_dxva_h264_decoder_new_field_picture (GstH264Decoder * decoder,
    GstH264Picture * first_field, GstH264Picture * second_field);
static GstFlowReturn
gst_dxva_h264_decoder_start_picture (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GstH264Dpb * dpb);
static GstFlowReturn
gst_dxva_h264_decoder_decode_slice (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GArray * ref_pic_list0,
    GArray * ref_pic_list1);
static GstFlowReturn
gst_dxva_h264_decoder_end_picture (GstH264Decoder * decoder,
    GstH264Picture * picture);
static GstFlowReturn
gst_dxva_h264_decoder_output_picture (GstH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstH264Picture * picture);

#define gst_dxva_h264_decoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstDxvaH264Decoder,
    gst_dxva_h264_decoder, GST_TYPE_H264_DECODER,
    GST_DEBUG_CATEGORY_INIT (gst_dxva_h264_decoder_debug, "dxvah264decoder",
        0, "dxvah264decoder"));

static void
gst_dxva_h264_decoder_class_init (GstDxvaH264DecoderClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstH264DecoderClass *h264decoder_class = GST_H264_DECODER_CLASS (klass);

  object_class->finalize = gst_dxva_h264_decoder_finalize;

  decoder_class->start = GST_DEBUG_FUNCPTR (gst_dxva_h264_decoder_start);

  h264decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_dxva_h264_decoder_new_sequence);
  h264decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_h264_decoder_new_picture);
  h264decoder_class->new_field_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_h264_decoder_new_field_picture);
  h264decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_h264_decoder_start_picture);
  h264decoder_class->decode_slice =
      GST_DEBUG_FUNCPTR (gst_dxva_h264_decoder_decode_slice);
  h264decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_h264_decoder_end_picture);
  h264decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_h264_decoder_output_picture);
}

static void
gst_dxva_h264_decoder_init (GstDxvaH264Decoder * self)
{
  self->priv = new GstDxvaH264DecoderPrivate ();
  self->priv->ref_pics = g_ptr_array_new ();
}

static void
gst_dxva_h264_decoder_finalize (GObject * object)
{
  GstDxvaH264Decoder *self = GST_DXVA_H264_DECODER (object);
  GstDxvaH264DecoderPrivate *priv = self->priv;

  g_ptr_array_unref (priv->ref_pics);
  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dxva_h264_decoder_reset (GstDxvaH264Decoder * self)
{
  GstDxvaH264DecoderPrivate *priv = self->priv;

  priv->crop_x = 0;
  priv->crop_y = 0;
  priv->width = 0;
  priv->height = 0;
  priv->coded_width = 0;
  priv->coded_height = 0;
  priv->bitdepth = 0;
  priv->chroma_format_idc = 0;
  priv->out_format = GST_VIDEO_FORMAT_UNKNOWN;
  priv->interlaced = FALSE;
  priv->max_dpb_size = 0;
  priv->configured = FALSE;
}

static gboolean
gst_dxva_h264_decoder_start (GstVideoDecoder * decoder)
{
  GstDxvaH264Decoder *self = GST_DXVA_H264_DECODER (decoder);

  gst_dxva_h264_decoder_reset (self);

  return GST_VIDEO_DECODER_CLASS (parent_class)->start (decoder);
}

static GstFlowReturn
gst_dxva_h264_decoder_new_sequence (GstH264Decoder * decoder,
    const GstH264SPS * sps, gint max_dpb_size)
{
  GstDxvaH264Decoder *self = GST_DXVA_H264_DECODER (decoder);
  GstDxvaH264DecoderPrivate *priv = self->priv;
  GstDxvaH264DecoderClass *klass = GST_DXVA_H264_DECODER_GET_CLASS (self);
  gint crop_width, crop_height;
  gboolean interlaced;
  gboolean modified = FALSE;
  GstVideoInfo info;
  GstFlowReturn ret;

  GST_LOG_OBJECT (self, "new sequence");

  if (sps->frame_cropping_flag) {
    crop_width = sps->crop_rect_width;
    crop_height = sps->crop_rect_height;
  } else {
    crop_width = sps->width;
    crop_height = sps->height;
  }

  if (priv->width != crop_width || priv->height != crop_height ||
      priv->coded_width != sps->width || priv->coded_height != sps->height ||
      priv->crop_x != sps->crop_rect_x || priv->crop_y != sps->crop_rect_y) {
    GST_INFO_OBJECT (self,
        "resolution change, %dx%d (%dx%d) -> %dx%d (%dx%d)",
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

  if (priv->chroma_format_idc != sps->chroma_format_idc) {
    GST_INFO_OBJECT (self, "chroma format change, %d -> %d",
        priv->chroma_format_idc, sps->chroma_format_idc);
    priv->chroma_format_idc = sps->chroma_format_idc;
    modified = TRUE;
  }

  interlaced = !sps->frame_mbs_only_flag;
  if (priv->interlaced != interlaced) {
    GST_INFO_OBJECT (self, "interlaced sequence change, %d -> %d",
        priv->interlaced, interlaced);
    priv->interlaced = interlaced;
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
  }

  if (priv->out_format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ERROR_OBJECT (self, "Could not support bitdepth/chroma format");
    priv->configured = FALSE;
    return GST_FLOW_NOT_NEGOTIATED;
  }

  gst_video_info_set_interlaced_format (&info, priv->out_format,
      priv->interlaced ? GST_VIDEO_INTERLACE_MODE_MIXED :
      GST_VIDEO_INTERLACE_MODE_PROGRESSIVE, priv->width, priv->height);

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
gst_dxva_h264_decoder_new_picture (GstH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstH264Picture * picture)
{
  GstDxvaH264Decoder *self = GST_DXVA_H264_DECODER (decoder);
  GstDxvaH264DecoderClass *klass = GST_DXVA_H264_DECODER_GET_CLASS (self);

  g_assert (klass->new_picture);

  return klass->new_picture (self, GST_CODEC_PICTURE (picture));
}

static GstFlowReturn
gst_dxva_h264_decoder_new_field_picture (GstH264Decoder * decoder,
    GstH264Picture * first_field, GstH264Picture * second_field)
{
  GstDxvaH264Decoder *self = GST_DXVA_H264_DECODER (decoder);
  GstDxvaH264DecoderClass *klass = GST_DXVA_H264_DECODER_GET_CLASS (self);

  g_assert (klass->duplicate_picture);

  return klass->duplicate_picture (self, GST_CODEC_PICTURE (first_field),
      GST_CODEC_PICTURE (second_field));
}

static void
gst_dxva_h264_decoder_picture_params_from_sps (GstDxvaH264Decoder * self,
    const GstH264SPS * sps, gboolean field_pic, DXVA_PicParams_H264 * params)
{
#define COPY_FIELD(f) \
  (params)->f = (sps)->f

  params->wFrameWidthInMbsMinus1 = sps->pic_width_in_mbs_minus1;
  if (!sps->frame_mbs_only_flag) {
    params->wFrameHeightInMbsMinus1 =
        ((sps->pic_height_in_map_units_minus1 + 1) << 1) - 1;
  } else {
    params->wFrameHeightInMbsMinus1 = sps->pic_height_in_map_units_minus1;
  }
  params->residual_colour_transform_flag = sps->separate_colour_plane_flag;
  params->MbaffFrameFlag = (sps->mb_adaptive_frame_field_flag && !field_pic);
  params->field_pic_flag = field_pic;
  params->MinLumaBipredSize8x8Flag = sps->level_idc >= 31;

  COPY_FIELD (num_ref_frames);
  COPY_FIELD (chroma_format_idc);
  COPY_FIELD (frame_mbs_only_flag);
  COPY_FIELD (bit_depth_luma_minus8);
  COPY_FIELD (bit_depth_chroma_minus8);
  COPY_FIELD (log2_max_frame_num_minus4);
  COPY_FIELD (pic_order_cnt_type);
  COPY_FIELD (log2_max_pic_order_cnt_lsb_minus4);
  COPY_FIELD (delta_pic_order_always_zero_flag);
  COPY_FIELD (direct_8x8_inference_flag);

#undef COPY_FIELD
}

static void
gst_dxva_h264_decoder_picture_params_from_pps (GstDxvaH264Decoder * self,
    const GstH264PPS * pps, DXVA_PicParams_H264 * params)
{
#define COPY_FIELD(f) \
  (params)->f = (pps)->f

  COPY_FIELD (constrained_intra_pred_flag);
  COPY_FIELD (weighted_pred_flag);
  COPY_FIELD (weighted_bipred_idc);
  COPY_FIELD (transform_8x8_mode_flag);
  COPY_FIELD (pic_init_qs_minus26);
  COPY_FIELD (chroma_qp_index_offset);
  COPY_FIELD (second_chroma_qp_index_offset);
  COPY_FIELD (pic_init_qp_minus26);
  COPY_FIELD (num_ref_idx_l0_active_minus1);
  COPY_FIELD (num_ref_idx_l1_active_minus1);
  COPY_FIELD (entropy_coding_mode_flag);
  COPY_FIELD (pic_order_present_flag);
  COPY_FIELD (deblocking_filter_control_present_flag);
  COPY_FIELD (redundant_pic_cnt_present_flag);
  COPY_FIELD (num_slice_groups_minus1);
  COPY_FIELD (slice_group_map_type);

#undef COPY_FIELD
}

static void
gst_dxva_h264_decoder_picture_params_from_slice_header (GstDxvaH264Decoder *
    self, const GstH264SliceHdr * slice_header, DXVA_PicParams_H264 * params)
{
  params->sp_for_switch_flag = slice_header->sp_for_switch_flag;
  params->field_pic_flag = slice_header->field_pic_flag;
  params->CurrPic.AssociatedFlag = slice_header->bottom_field_flag;
  params->IntraPicFlag =
      GST_H264_IS_I_SLICE (slice_header) || GST_H264_IS_SI_SLICE (slice_header);
}

static gboolean
gst_dxva_h264_decoder_fill_picture_params (GstDxvaH264Decoder * self,
    const GstH264SliceHdr * slice_header, DXVA_PicParams_H264 * params)
{
  const GstH264SPS *sps;
  const GstH264PPS *pps;

  g_return_val_if_fail (slice_header->pps != nullptr, FALSE);
  g_return_val_if_fail (slice_header->pps->sequence != nullptr, FALSE);

  pps = slice_header->pps;
  sps = pps->sequence;

  params->MbsConsecutiveFlag = 1;
  params->Reserved16Bits = 3;
  params->ContinuationFlag = 1;
  params->Reserved8BitsA = 0;
  params->Reserved8BitsB = 0;
  params->StatusReportFeedbackNumber = 1;

  gst_dxva_h264_decoder_picture_params_from_sps (self,
      sps, slice_header->field_pic_flag, params);
  gst_dxva_h264_decoder_picture_params_from_pps (self, pps, params);
  gst_dxva_h264_decoder_picture_params_from_slice_header (self,
      slice_header, params);

  return TRUE;
}

static inline void
init_pic_params (DXVA_PicParams_H264 * params)
{
  memset (params, 0, sizeof (DXVA_PicParams_H264));
  for (guint i = 0; i < G_N_ELEMENTS (params->RefFrameList); i++)
    params->RefFrameList[i].bPicEntry = 0xff;
}

static GstFlowReturn
gst_dxva_h264_decoder_start_picture (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GstH264Dpb * dpb)
{
  GstDxvaH264Decoder *self = GST_DXVA_H264_DECODER (decoder);
  GstDxvaH264DecoderClass *klass = GST_DXVA_H264_DECODER_GET_CLASS (self);
  GstDxvaH264DecoderPrivate *priv = self->priv;
  DXVA_PicParams_H264 *pic_params = &priv->pic_params;
  DXVA_Qmatrix_H264 *iq_matrix = &priv->iq_matrix;
  GstCodecPicture *codec_picture = GST_CODEC_PICTURE (picture);
  GArray *dpb_array;
  GstH264PPS *pps;
  guint i, j;
  GstFlowReturn ret;
  guint8 picture_id;

  g_assert (klass->start_picture);
  g_assert (klass->get_picture_id);

  ret = klass->start_picture (self, codec_picture, &picture_id);
  if (ret != GST_FLOW_OK)
    return ret;

  pps = slice->header.pps;

  priv->slice_list.resize (0);
  priv->bitstream_buffer.resize (0);
  g_ptr_array_set_size (priv->ref_pics, 0);

  init_pic_params (pic_params);
  gst_dxva_h264_decoder_fill_picture_params (self, &slice->header, pic_params);

  pic_params->CurrPic.Index7Bits = picture_id;
  pic_params->RefPicFlag = GST_H264_PICTURE_IS_REF (picture);
  pic_params->frame_num = picture->frame_num;

  if (picture->field == GST_H264_PICTURE_FIELD_TOP_FIELD) {
    pic_params->CurrFieldOrderCnt[0] = picture->top_field_order_cnt;
    pic_params->CurrFieldOrderCnt[1] = 0;
  } else if (picture->field == GST_H264_PICTURE_FIELD_BOTTOM_FIELD) {
    pic_params->CurrFieldOrderCnt[0] = 0;
    pic_params->CurrFieldOrderCnt[1] = picture->bottom_field_order_cnt;
  } else {
    pic_params->CurrFieldOrderCnt[0] = picture->top_field_order_cnt;
    pic_params->CurrFieldOrderCnt[1] = picture->bottom_field_order_cnt;
  }

  dpb_array = gst_h264_dpb_get_pictures_all (dpb);
  for (i = 0, j = 0; i < dpb_array->len && j < 16; i++) {
    GstH264Picture *other = g_array_index (dpb_array, GstH264Picture *, i);

    if (!GST_H264_PICTURE_IS_REF (other))
      continue;

    /* Ignore nonexisting picture */
    if (other->nonexisting)
      continue;

    /* The second field picture will be handled differently */
    if (other->second_field)
      continue;

    pic_params->RefFrameList[j].Index7Bits =
        klass->get_picture_id (self, GST_CODEC_PICTURE (other));

    if (GST_H264_PICTURE_IS_LONG_TERM_REF (other)) {
      pic_params->RefFrameList[j].AssociatedFlag = 1;
      pic_params->FrameNumList[j] = other->long_term_frame_idx;
    } else {
      pic_params->RefFrameList[j].AssociatedFlag = 0;
      pic_params->FrameNumList[j] = other->frame_num;
    }

    switch (other->field) {
      case GST_H264_PICTURE_FIELD_TOP_FIELD:
        pic_params->FieldOrderCntList[j][0] = other->top_field_order_cnt;
        pic_params->UsedForReferenceFlags |= 0x1 << (2 * j);
        break;
      case GST_H264_PICTURE_FIELD_BOTTOM_FIELD:
        pic_params->FieldOrderCntList[j][1] = other->bottom_field_order_cnt;
        pic_params->UsedForReferenceFlags |= 0x1 << (2 * j + 1);
        break;
      default:
        pic_params->FieldOrderCntList[j][0] = other->top_field_order_cnt;
        pic_params->FieldOrderCntList[j][1] = other->bottom_field_order_cnt;
        pic_params->UsedForReferenceFlags |= 0x3 << (2 * j);
        break;
    }

    if (other->other_field) {
      GstH264Picture *other_field = other->other_field;

      switch (other_field->field) {
        case GST_H264_PICTURE_FIELD_TOP_FIELD:
          pic_params->FieldOrderCntList[j][0] =
              other_field->top_field_order_cnt;
          pic_params->UsedForReferenceFlags |= 0x1 << (2 * j);
          break;
        case GST_H264_PICTURE_FIELD_BOTTOM_FIELD:
          pic_params->FieldOrderCntList[j][1] =
              other_field->bottom_field_order_cnt;
          pic_params->UsedForReferenceFlags |= 0x1 << (2 * j + 1);
          break;
        default:
          break;
      }
    }

    g_ptr_array_add (priv->ref_pics, other);
    j++;
  }
  g_array_unref (dpb_array);

  G_STATIC_ASSERT (sizeof (iq_matrix->bScalingLists4x4) ==
      sizeof (pps->scaling_lists_4x4));
  memcpy (iq_matrix->bScalingLists4x4, pps->scaling_lists_4x4,
      sizeof (pps->scaling_lists_4x4));

  G_STATIC_ASSERT (sizeof (iq_matrix->bScalingLists8x8[0]) ==
      sizeof (pps->scaling_lists_8x8[0]));
  memcpy (iq_matrix->bScalingLists8x8[0], pps->scaling_lists_8x8[0],
      sizeof (pps->scaling_lists_8x8[0]));
  memcpy (iq_matrix->bScalingLists8x8[1], pps->scaling_lists_8x8[1],
      sizeof (pps->scaling_lists_8x8[1]));

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_dxva_h264_decoder_decode_slice (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GArray * ref_pic_list0,
    GArray * ref_pic_list1)
{
  GstDxvaH264Decoder *self = GST_DXVA_H264_DECODER (decoder);
  GstDxvaH264DecoderPrivate *priv = self->priv;
  DXVA_Slice_H264_Short dxva_slice;
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
gst_dxva_h264_decoder_end_picture (GstH264Decoder * decoder,
    GstH264Picture * picture)
{
  GstDxvaH264Decoder *self = GST_DXVA_H264_DECODER (decoder);
  GstDxvaH264DecoderPrivate *priv = self->priv;
  GstDxvaH264DecoderClass *klass = GST_DXVA_H264_DECODER_GET_CLASS (self);
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

    DXVA_Slice_H264_Short & slice = priv->slice_list.back ();
    slice.SliceBytesInBuffer += padding;
  }

  args.picture_params = &priv->pic_params;
  args.picture_params_size = sizeof (DXVA_PicParams_H264);
  args.slice_control = &priv->slice_list[0];
  args.slice_control_size =
      sizeof (DXVA_Slice_H264_Short) * priv->slice_list.size ();
  args.bitstream = &priv->bitstream_buffer[0];
  args.bitstream_size = priv->bitstream_buffer.size ();
  args.inverse_quantization_matrix = &priv->iq_matrix;
  args.inverse_quantization_matrix_size = sizeof (DXVA_Qmatrix_H264);

  g_assert (klass->end_picture);

  return klass->end_picture (self, GST_CODEC_PICTURE (picture),
      priv->ref_pics, &args);
}

static GstFlowReturn
gst_dxva_h264_decoder_output_picture (GstH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstH264Picture * picture)
{
  GstDxvaH264Decoder *self = GST_DXVA_H264_DECODER (decoder);
  GstDxvaH264DecoderClass *klass = GST_DXVA_H264_DECODER_GET_CLASS (self);
  GstDxvaH264DecoderPrivate *priv = self->priv;

  g_assert (klass->output_picture);

  GST_LOG_OBJECT (self,
      "Outputting picture %p (poc %d)", picture, picture->pic_order_cnt);

  return klass->output_picture (self, frame, GST_CODEC_PICTURE (picture),
      picture->buffer_flags, priv->width, priv->height);
}
