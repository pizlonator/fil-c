/* GStreamer
 *  Copyright (C) 2020 Intel Corporation
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
 * License along with this library; if not, write to the0
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gsth264bitwriter.h"
#include <gst/codecparsers/nalutils.h>
#include <gst/base/gstbitwriter.h>

/********************************  Utils ********************************/
#define SIGNED(val)    (2 * ABS(val) - ((val) > 0))

/* Write an unsigned integer Exp-Golomb-coded syntax element. i.e. ue(v) */
static gboolean
_bs_write_ue (GstBitWriter * bs, guint32 value)
{
  guint32 size_in_bits = 0;
  guint32 tmp_value = ++value;

  while (tmp_value) {
    ++size_in_bits;
    tmp_value >>= 1;
  }
  if (size_in_bits > 1
      && !gst_bit_writer_put_bits_uint32 (bs, 0, size_in_bits - 1))
    return FALSE;
  if (!gst_bit_writer_put_bits_uint32 (bs, value, size_in_bits))
    return FALSE;
  return TRUE;
}

#define WRITE_BITS_UNCHECK(bw, val, nbits)                                    \
  (nbits <= 8 ? gst_bit_writer_put_bits_uint8 (bw, val, nbits) :              \
   (nbits <= 16 ? gst_bit_writer_put_bits_uint16 (bw, val, nbits) :           \
    (nbits <= 32 ? gst_bit_writer_put_bits_uint32 (bw, val, nbits) :          \
     FALSE)))

#define WRITE_BITS(bw, val, nbits)                                            \
  if (!WRITE_BITS_UNCHECK (bw, val, nbits)) {                                 \
    g_warning ("Unsupported bit size: %u", nbits);                            \
    have_space = FALSE;                                                       \
    goto error;                                                               \
  }

#define WRITE_UE_UNCHECK(bw, val)  _bs_write_ue (bw, val)

#ifdef WRITE_UE
#undef WRITE_UE
#endif
#define WRITE_UE(bw, val)                                                     \
  if (!(have_space = WRITE_UE_UNCHECK (bw, val)))                             \
    goto error;                                                               \

#define WRITE_UE_MAX(bw, val, max)                                            \
  if ((guint32) val > (max) || !(have_space = WRITE_UE_UNCHECK (bw, val)))    \
    goto error;

#define WRITE_SE(bw, val) WRITE_UE (bw, SIGNED (val))

#define WRITE_SE_RANGE(bw, val, min, max)                                     \
  if (val > max || val < min ||                                               \
      !(have_space = WRITE_UE_UNCHECK (bw, SIGNED (val))))                    \
    goto error;

#define WRITE_BYTES_UNCHECK(bw, ptr, nbytes)                                  \
  gst_bit_writer_put_bytes(bw, ptr, nbytes)

#ifdef WRITE_BYTES
#undef WRITE_BYTES
#endif
#define WRITE_BYTES(bw, ptr, nbytes)                                          \
  if (!(have_space = WRITE_BYTES_UNCHECK (bw, ptr, nbytes)))                  \
    goto error;

/*****************************  End of Utils ****************************/

/**** Default scaling_lists according to Table 7-2 *****/
static const guint8 default_4x4_intra[16] = {
  6, 13, 13, 20, 20, 20, 28, 28, 28, 28, 32, 32,
  32, 37, 37, 42
};

static const guint8 default_4x4_inter[16] = {
  10, 14, 14, 20, 20, 20, 24, 24, 24, 24, 27, 27,
  27, 30, 30, 34
};

static const guint8 default_8x8_intra[64] = {
  6, 10, 10, 13, 11, 13, 16, 16, 16, 16, 18, 18,
  18, 18, 18, 23, 23, 23, 23, 23, 23, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27,
  27, 27, 27, 27, 27, 29, 29, 29, 29, 29, 29, 29, 31, 31, 31, 31, 31, 31, 33,
  33, 33, 33, 33, 36, 36, 36, 36, 38, 38, 38, 40, 40, 42
};

static const guint8 default_8x8_inter[64] = {
  9, 13, 13, 15, 13, 15, 17, 17, 17, 17, 19, 19,
  19, 19, 19, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 24, 24, 24,
  24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27, 27, 27, 28,
  28, 28, 28, 28, 30, 30, 30, 30, 32, 32, 32, 33, 33, 35
};

static gboolean
_h264_bit_writer_scaling_list (GstBitWriter * bw, gboolean * space,
    const guint8 scaling_lists_4x4[6][16],
    const guint8 scaling_lists_8x8[6][64], const guint8 fallback_4x4_inter[16],
    const guint8 fallback_4x4_intra[16], const guint8 fallback_8x8_inter[64],
    const guint8 fallback_8x8_intra[64], guint8 n_lists)
{
  gboolean have_space = TRUE;
  guint i, j;

  const guint8 *default_lists[12] = {
    fallback_4x4_intra, fallback_4x4_intra, fallback_4x4_intra,
    fallback_4x4_inter, fallback_4x4_inter, fallback_4x4_inter,
    fallback_8x8_intra, fallback_8x8_inter,
    fallback_8x8_intra, fallback_8x8_inter,
    fallback_8x8_intra, fallback_8x8_inter
  };

  GST_DEBUG ("writing scaling lists");

  for (i = 0; i < 12; i++) {
    if (i < n_lists) {
      guint8 scaling_list_present_flag = FALSE;
      const guint8 *scaling_list;
      guint size;

      if (i < 6) {
        scaling_list = scaling_lists_4x4[i];
        size = 16;
      } else {
        scaling_list = scaling_lists_8x8[i - 6];
        size = 64;
      }

      if (memcmp (scaling_list, default_lists[i], size))
        scaling_list_present_flag = TRUE;

      WRITE_BITS (bw, scaling_list_present_flag, 1);
      if (scaling_list_present_flag) {
        guint8 last_scale, next_scale;
        gint8 delta_scale;

        for (j = 0; j < size; j++) {
          last_scale = next_scale = 8;

          for (j = 0; j < size; j++) {
            if (next_scale != 0) {
              delta_scale = (gint8) (scaling_list[j] - last_scale);

              WRITE_SE (bw, delta_scale);

              next_scale = scaling_list[j];
            }
            last_scale = (next_scale == 0) ? last_scale : next_scale;
          }
        }
      }
    }
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write scaling lists");

  *space = have_space;
  return FALSE;
}

static gboolean
_h264_bit_writer_hrd_parameters (const GstH264HRDParams * hrd,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  guint sched_sel_idx;

  GST_DEBUG ("writing \"HRD Parameters\"");

  WRITE_UE_MAX (bw, hrd->cpb_cnt_minus1, 31);
  WRITE_BITS (bw, hrd->bit_rate_scale, 4);
  WRITE_BITS (bw, hrd->cpb_size_scale, 4);

  for (sched_sel_idx = 0; sched_sel_idx <= hrd->cpb_cnt_minus1; sched_sel_idx++) {
    WRITE_UE (bw, hrd->bit_rate_value_minus1[sched_sel_idx]);
    WRITE_UE (bw, hrd->cpb_size_value_minus1[sched_sel_idx]);
    WRITE_BITS (bw, hrd->cbr_flag[sched_sel_idx], 1);
  }

  WRITE_BITS (bw, hrd->initial_cpb_removal_delay_length_minus1, 5);
  WRITE_BITS (bw, hrd->cpb_removal_delay_length_minus1, 5);
  WRITE_BITS (bw, hrd->dpb_output_delay_length_minus1, 5);
  WRITE_BITS (bw, hrd->time_offset_length, 5);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write \"HRD Parameters\"");

  *space = have_space;
  return FALSE;
}

#define EXTENDED_SAR 255

static gboolean
_h264_bit_writer_vui_parameters (const GstH264SPS * sps, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;
  const GstH264VUIParams *vui = &sps->vui_parameters;

  GST_DEBUG ("writing \"VUI Parameters\"");

  WRITE_BITS (bw, vui->aspect_ratio_info_present_flag, 1);
  if (vui->aspect_ratio_info_present_flag) {
    WRITE_BITS (bw, vui->aspect_ratio_idc, 8);
    if (vui->aspect_ratio_idc == EXTENDED_SAR) {
      WRITE_BITS (bw, vui->sar_width, 16);
      WRITE_BITS (bw, vui->sar_height, 16);
    }
  }

  WRITE_BITS (bw, vui->overscan_info_present_flag, 1);
  if (vui->overscan_info_present_flag)
    WRITE_BITS (bw, vui->overscan_appropriate_flag, 1);

  WRITE_BITS (bw, vui->video_signal_type_present_flag, 1);
  if (vui->video_signal_type_present_flag) {
    WRITE_BITS (bw, vui->video_format, 3);
    WRITE_BITS (bw, vui->video_full_range_flag, 1);
    WRITE_BITS (bw, vui->colour_description_present_flag, 1);
    if (vui->colour_description_present_flag) {
      WRITE_BITS (bw, vui->colour_primaries, 8);
      WRITE_BITS (bw, vui->transfer_characteristics, 8);
      WRITE_BITS (bw, vui->matrix_coefficients, 8);
    }
  }

  WRITE_BITS (bw, vui->chroma_loc_info_present_flag, 1);
  if (vui->chroma_loc_info_present_flag) {
    WRITE_UE_MAX (bw, vui->chroma_sample_loc_type_top_field, 5);
    WRITE_UE_MAX (bw, vui->chroma_sample_loc_type_bottom_field, 5);
  }

  WRITE_BITS (bw, vui->timing_info_present_flag, 1);
  if (vui->timing_info_present_flag) {
    WRITE_BITS (bw, vui->num_units_in_tick, 32);
    if (vui->num_units_in_tick == 0)
      GST_WARNING ("num_units_in_tick = 0 write to stream "
          "(incompliant to H.264 E.2.1).");

    WRITE_BITS (bw, vui->time_scale, 32);
    if (vui->time_scale == 0)
      GST_WARNING ("time_scale = 0 write to stream "
          "(incompliant to H.264 E.2.1).");

    WRITE_BITS (bw, vui->fixed_frame_rate_flag, 1);
  }

  WRITE_BITS (bw, vui->nal_hrd_parameters_present_flag, 1);
  if (vui->nal_hrd_parameters_present_flag) {
    if (!_h264_bit_writer_hrd_parameters (&vui->nal_hrd_parameters, bw,
            &have_space))
      goto error;
  }

  WRITE_BITS (bw, vui->vcl_hrd_parameters_present_flag, 1);
  if (vui->vcl_hrd_parameters_present_flag) {
    if (!_h264_bit_writer_hrd_parameters (&vui->vcl_hrd_parameters, bw,
            &have_space))
      goto error;
  }

  if (vui->nal_hrd_parameters_present_flag ||
      vui->vcl_hrd_parameters_present_flag)
    WRITE_BITS (bw, vui->low_delay_hrd_flag, 1);

  WRITE_BITS (bw, vui->pic_struct_present_flag, 1);
  WRITE_BITS (bw, vui->bitstream_restriction_flag, 1);
  if (vui->bitstream_restriction_flag) {
    WRITE_BITS (bw, vui->motion_vectors_over_pic_boundaries_flag, 1);
    WRITE_UE (bw, vui->max_bytes_per_pic_denom);
    WRITE_UE_MAX (bw, vui->max_bits_per_mb_denom, 16);
    WRITE_UE_MAX (bw, vui->log2_max_mv_length_horizontal, 16);
    WRITE_UE_MAX (bw, vui->log2_max_mv_length_vertical, 16);
    WRITE_UE (bw, vui->num_reorder_frames);
    WRITE_UE (bw, vui->max_dec_frame_buffering);
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write \"VUI Parameters\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h264_bit_writer_sps (const GstH264SPS * sps, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing SPS");

  WRITE_BITS (bw, sps->profile_idc, 8);
  WRITE_BITS (bw, sps->constraint_set0_flag, 1);
  WRITE_BITS (bw, sps->constraint_set1_flag, 1);
  WRITE_BITS (bw, sps->constraint_set2_flag, 1);
  WRITE_BITS (bw, sps->constraint_set3_flag, 1);
  WRITE_BITS (bw, sps->constraint_set4_flag, 1);
  WRITE_BITS (bw, sps->constraint_set5_flag, 1);
  /* reserved_zero_2bits */
  WRITE_BITS (bw, 0, 2);

  WRITE_BITS (bw, sps->level_idc, 8);

  WRITE_UE_MAX (bw, sps->id, GST_H264_MAX_SPS_COUNT - 1);

  if (sps->profile_idc == 100 || sps->profile_idc == 110 ||
      sps->profile_idc == 122 || sps->profile_idc == 244 ||
      sps->profile_idc == 44 || sps->profile_idc == 83 ||
      sps->profile_idc == 86 || sps->profile_idc == 118 ||
      sps->profile_idc == 128 || sps->profile_idc == 138 ||
      sps->profile_idc == 139 || sps->profile_idc == 134 ||
      sps->profile_idc == 135) {
    WRITE_UE_MAX (bw, sps->chroma_format_idc, 3);
    if (sps->chroma_format_idc == 3)
      WRITE_BITS (bw, sps->separate_colour_plane_flag, 1);

    WRITE_UE_MAX (bw, sps->bit_depth_luma_minus8, 6);
    WRITE_UE_MAX (bw, sps->bit_depth_chroma_minus8, 6);
    WRITE_BITS (bw, sps->qpprime_y_zero_transform_bypass_flag, 1);

    WRITE_BITS (bw, sps->scaling_matrix_present_flag, 1);
    if (sps->scaling_matrix_present_flag) {
      guint8 n_lists;

      n_lists = (sps->chroma_format_idc != 3) ? 8 : 12;
      if (!_h264_bit_writer_scaling_list (bw, &have_space,
              sps->scaling_lists_4x4, sps->scaling_lists_8x8,
              default_4x4_inter, default_4x4_intra,
              default_8x8_inter, default_8x8_intra, n_lists))
        goto error;
    }
  }

  WRITE_UE_MAX (bw, sps->log2_max_frame_num_minus4, 12);

  WRITE_UE_MAX (bw, sps->pic_order_cnt_type, 2);
  if (sps->pic_order_cnt_type == 0) {
    WRITE_UE_MAX (bw, sps->log2_max_pic_order_cnt_lsb_minus4, 12);
  } else if (sps->pic_order_cnt_type == 1) {
    guint i;

    WRITE_BITS (bw, sps->delta_pic_order_always_zero_flag, 1);
    WRITE_SE (bw, sps->offset_for_non_ref_pic);
    WRITE_SE (bw, sps->offset_for_top_to_bottom_field);
    WRITE_UE_MAX (bw, sps->num_ref_frames_in_pic_order_cnt_cycle, 255);

    for (i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
      WRITE_SE (bw, sps->offset_for_ref_frame[i]);
  }

  WRITE_UE (bw, sps->num_ref_frames);
  WRITE_BITS (bw, sps->gaps_in_frame_num_value_allowed_flag, 1);
  WRITE_UE (bw, sps->pic_width_in_mbs_minus1);
  WRITE_UE (bw, sps->pic_height_in_map_units_minus1);
  WRITE_BITS (bw, sps->frame_mbs_only_flag, 1);

  if (!sps->frame_mbs_only_flag)
    WRITE_BITS (bw, sps->mb_adaptive_frame_field_flag, 1);

  WRITE_BITS (bw, sps->direct_8x8_inference_flag, 1);
  WRITE_BITS (bw, sps->frame_cropping_flag, 1);
  if (sps->frame_cropping_flag) {
    WRITE_UE (bw, sps->frame_crop_left_offset);
    WRITE_UE (bw, sps->frame_crop_right_offset);
    WRITE_UE (bw, sps->frame_crop_top_offset);
    WRITE_UE (bw, sps->frame_crop_bottom_offset);
  }

  WRITE_BITS (bw, sps->vui_parameters_present_flag, 1);
  if (sps->vui_parameters_present_flag)
    if (!_h264_bit_writer_vui_parameters (sps, bw, &have_space))
      goto error;

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write sps");

  *space = have_space;
  return FALSE;
}

/**
 * gst_h264_bit_writer_sps:
 * @sps: the sps of #GstH264SPS to write
 * @start_code: whether adding the nal start code
 * @data: (out): the bit stream generated by the sps
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according h264 bit stream by providing the sps.
 *
 * Returns: a #GstH264BitWriterResult
 *
 * Since: 1.22
 **/
GstH264BitWriterResult
gst_h264_bit_writer_sps (const GstH264SPS * sps, gboolean start_code,
    guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;

  g_return_val_if_fail (sps != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_H264_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  if (start_code)
    WRITE_BITS (&bw, 0x00000001, 32);

  /* nal header */
  /* forbidden_zero_bit */
  WRITE_BITS (&bw, 0, 1);
  /* nal_ref_idc */
  WRITE_BITS (&bw, 1, 2);
  /* nal_unit_type */
  WRITE_BITS (&bw, GST_H264_NAL_SPS, 5);

  if (!_h264_bit_writer_sps (sps, &bw, &have_space))
    goto error;

  /* Add trailings. */
  WRITE_BITS (&bw, 1, 1);
  if (!gst_bit_writer_align_bytes (&bw, 0)) {
    have_space = FALSE;
    goto error;
  }

  *size = (gst_bit_writer_get_size (&bw)) / 8;
  gst_bit_writer_reset (&bw);

  return GST_H264_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;

  return have_space ? GST_H264_BIT_WRITER_INVALID_DATA :
      GST_H264_BIT_WRITER_NO_MORE_SPACE;
}

static gboolean
_h264_bit_writer_pps (const GstH264PPS * pps, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;
  gint qp_bd_offset;

  GST_DEBUG ("writing PPS");

  qp_bd_offset = 6 * (pps->sequence->bit_depth_luma_minus8 +
      pps->sequence->separate_colour_plane_flag);

  WRITE_UE_MAX (bw, pps->id, GST_H264_MAX_PPS_COUNT - 1);
  WRITE_UE_MAX (bw, pps->sequence->id, GST_H264_MAX_SPS_COUNT - 1);

  WRITE_BITS (bw, pps->entropy_coding_mode_flag, 1);
  WRITE_BITS (bw, pps->pic_order_present_flag, 1);

  WRITE_UE_MAX (bw, pps->num_slice_groups_minus1, 7);
  if (pps->num_slice_groups_minus1 > 0) {
    WRITE_UE_MAX (bw, pps->slice_group_map_type, 6);

    if (pps->slice_group_map_type == 0) {
      gint i;

      for (i = 0; i <= pps->num_slice_groups_minus1; i++)
        WRITE_UE (bw, pps->run_length_minus1[i]);
    } else if (pps->slice_group_map_type == 2) {
      gint i;

      for (i = 0; i < pps->num_slice_groups_minus1; i++) {
        WRITE_UE (bw, pps->top_left[i]);
        WRITE_UE (bw, pps->bottom_right[i]);
      }
    } else if (pps->slice_group_map_type >= 3 && pps->slice_group_map_type <= 5) {
      WRITE_BITS (bw, pps->slice_group_change_direction_flag, 1);
      WRITE_UE (bw, pps->slice_group_change_rate_minus1);
    } else if (pps->slice_group_map_type == 6) {
      gint bits;
      gint i;

      WRITE_UE (bw, pps->pic_size_in_map_units_minus1);
      bits = g_bit_storage (pps->num_slice_groups_minus1);

      g_assert (pps->slice_group_id);
      for (i = 0; i <= pps->pic_size_in_map_units_minus1; i++)
        WRITE_BITS (bw, pps->slice_group_id[i], bits);
    }
  }

  WRITE_UE_MAX (bw, pps->num_ref_idx_l0_active_minus1, 31);
  WRITE_UE_MAX (bw, pps->num_ref_idx_l1_active_minus1, 31);
  WRITE_BITS (bw, pps->weighted_pred_flag, 1);
  WRITE_BITS (bw, pps->weighted_bipred_idc, 2);
  WRITE_SE_RANGE (bw, pps->pic_init_qp_minus26, -(26 + qp_bd_offset), 25);
  WRITE_SE_RANGE (bw, pps->pic_init_qs_minus26, -26, 25);
  WRITE_SE_RANGE (bw, pps->chroma_qp_index_offset, -12, 12);

  WRITE_BITS (bw, pps->deblocking_filter_control_present_flag, 1);
  WRITE_BITS (bw, pps->constrained_intra_pred_flag, 1);
  WRITE_BITS (bw, pps->redundant_pic_cnt_present_flag, 1);

  /* A.2.1 Baseline profile, A.2.2 Main profile and
     A.2.3 Extended profile:
     The syntax elements transform_8x8_mode_flag,
     pic_scaling_matrix_present_flag, second_chroma_qp_index_offset
     shall not be present in picture parameter sets. */
  if (pps->sequence->profile_idc == 66 ||
      pps->sequence->profile_idc == 77 || pps->sequence->profile_idc == 88)
    return TRUE;

  WRITE_BITS (bw, pps->transform_8x8_mode_flag, 1);

  WRITE_BITS (bw, pps->pic_scaling_matrix_present_flag, 1);

  if (pps->pic_scaling_matrix_present_flag) {
    guint8 n_lists;

    n_lists = 6 + ((pps->sequence->chroma_format_idc != 3) ? 2 : 6) *
        pps->transform_8x8_mode_flag;

    if (pps->sequence->scaling_matrix_present_flag) {
      if (!_h264_bit_writer_scaling_list (bw, &have_space,
              pps->scaling_lists_4x4, pps->scaling_lists_8x8,
              pps->sequence->scaling_lists_4x4[3],
              pps->sequence->scaling_lists_4x4[0],
              pps->sequence->scaling_lists_8x8[3],
              pps->sequence->scaling_lists_8x8[0], n_lists))
        goto error;
    } else {
      if (!_h264_bit_writer_scaling_list (bw, &have_space,
              pps->scaling_lists_4x4, pps->scaling_lists_8x8,
              default_4x4_inter, default_4x4_intra,
              default_8x8_inter, default_8x8_intra, n_lists))
        goto error;
    }
  }

  WRITE_SE_RANGE (bw, pps->second_chroma_qp_index_offset, -12, 12);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write pps");

  *space = have_space;
  return FALSE;
}

/**
 * gst_h264_bit_writer_pps:
 * @pps: the pps of #GstH264PPS to write
 * @start_code: whether adding the nal start code
 * @data: (out): the bit stream generated by the pps
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according h264 bit stream by providing the pps.
 *
 * Returns: a #GstH264BitWriterResult
 *
 * Since: 1.22
 **/
GstH264BitWriterResult
gst_h264_bit_writer_pps (const GstH264PPS * pps, gboolean start_code,
    guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;

  g_return_val_if_fail (pps != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (pps->sequence != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_H264_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  if (start_code)
    WRITE_BITS (&bw, 0x00000001, 32);

  /* nal header */
  /* forbidden_zero_bit */
  WRITE_BITS (&bw, 0, 1);
  /* nal_ref_idc */
  WRITE_BITS (&bw, 1, 2);
  /* nal_unit_type */
  WRITE_BITS (&bw, GST_H264_NAL_PPS, 5);

  if (!_h264_bit_writer_pps (pps, &bw, &have_space))
    goto error;

  /* Add trailings. */
  WRITE_BITS (&bw, 1, 1);
  if (!gst_bit_writer_align_bytes (&bw, 0)) {
    have_space = FALSE;
    goto error;
  }

  *size = (gst_bit_writer_get_size (&bw)) / 8;
  gst_bit_writer_reset (&bw);
  return GST_H264_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;

  return have_space ? GST_H264_BIT_WRITER_INVALID_DATA :
      GST_H264_BIT_WRITER_NO_MORE_SPACE;
}

static gboolean
_h264_slice_bit_writer_ref_pic_list_modification_1 (const GstH264SliceHdr *
    slice, guint list, gboolean is_mvc, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  const GstH264RefPicListModification *entries;
  guint8 ref_pic_list_modification_flag = 0;
  guint i;

  if (list == 0) {
    entries = slice->ref_pic_list_modification_l0;
    ref_pic_list_modification_flag = slice->ref_pic_list_modification_flag_l0;
  } else {
    entries = slice->ref_pic_list_modification_l1;
    ref_pic_list_modification_flag = slice->ref_pic_list_modification_flag_l1;
  }

  WRITE_BITS (bw, ref_pic_list_modification_flag, 1);
  if (ref_pic_list_modification_flag) {
    i = 0;
    do {
      g_assert (i < 32);

      WRITE_UE (bw, entries[i].modification_of_pic_nums_idc);
      if (entries[i].modification_of_pic_nums_idc == 0 ||
          entries[i].modification_of_pic_nums_idc == 1) {
        WRITE_UE_MAX (bw, entries[i].value.abs_diff_pic_num_minus1,
            slice->max_pic_num - 1);
      } else if (entries[i].modification_of_pic_nums_idc == 2) {
        WRITE_UE (bw, entries[i].value.long_term_pic_num);
      } else if (is_mvc && (entries[i].modification_of_pic_nums_idc == 4 ||
              entries[i].modification_of_pic_nums_idc == 5)) {
        WRITE_UE (bw, entries[i].value.abs_diff_view_idx_minus1);
      }
    } while (entries[i++].modification_of_pic_nums_idc != 3);
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write \"Reference picture list %u modification\"",
      list);

  *space = have_space;
  return FALSE;
}

static gboolean
_h264_slice_bit_writer_ref_pic_list_modification (const GstH264SliceHdr *
    slice, gboolean is_mvc, GstBitWriter * bw, gboolean * space)
{
  if (!GST_H264_IS_I_SLICE (slice) && !GST_H264_IS_SI_SLICE (slice)) {
    if (!_h264_slice_bit_writer_ref_pic_list_modification_1 (slice, 0,
            is_mvc, bw, space))
      return FALSE;
  }

  if (GST_H264_IS_B_SLICE (slice)) {
    if (!_h264_slice_bit_writer_ref_pic_list_modification_1 (slice, 1,
            is_mvc, bw, space))
      return FALSE;
  }

  *space = TRUE;
  return TRUE;
}

static gboolean
_h264_slice_bit_writer_pred_weight_table (const GstH264SliceHdr * slice,
    guint8 chroma_array_type, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  const GstH264PredWeightTable *p;
  gint i;
  gint16 default_luma_weight, default_chroma_weight;

  GST_DEBUG ("writing \"Prediction weight table\"");

  p = &slice->pred_weight_table;
  default_luma_weight = 1 << p->luma_log2_weight_denom;
  default_chroma_weight = 1 << p->chroma_log2_weight_denom;

  WRITE_UE_MAX (bw, p->luma_log2_weight_denom, 7);

  if (chroma_array_type != 0)
    WRITE_UE_MAX (bw, p->chroma_log2_weight_denom, 7);

  for (i = 0; i <= slice->num_ref_idx_l0_active_minus1; i++) {
    guint8 luma_weight_l0_flag = 0;

    if (p->luma_weight_l0[i] != default_luma_weight ||
        p->luma_offset_l0[i] != 0)
      luma_weight_l0_flag = 1;

    WRITE_BITS (bw, luma_weight_l0_flag, 1);
    if (luma_weight_l0_flag) {
      WRITE_SE_RANGE (bw, p->luma_weight_l0[i], -128, 127);
      WRITE_SE_RANGE (bw, p->luma_offset_l0[i], -128, 127);
    }
    if (chroma_array_type != 0) {
      guint8 chroma_weight_l0_flag = 0;
      gint j;

      for (j = 0; j < 2; j++) {
        if (p->chroma_weight_l0[i][j] != default_chroma_weight ||
            p->chroma_offset_l0[i][j] != 0)
          chroma_weight_l0_flag = 1;
      }

      WRITE_BITS (bw, chroma_weight_l0_flag, 1);
      if (chroma_weight_l0_flag) {
        for (j = 0; j < 2; j++) {
          WRITE_SE_RANGE (bw, p->chroma_weight_l0[i][j], -128, 127);
          WRITE_SE_RANGE (bw, p->chroma_offset_l0[i][j], -128, 127);
        }
      }
    }
  }

  if (GST_H264_IS_B_SLICE (slice)) {
    for (i = 0; i <= slice->num_ref_idx_l1_active_minus1; i++) {
      guint8 luma_weight_l1_flag = 0;

      if (p->luma_weight_l1[i] != default_luma_weight ||
          p->luma_offset_l1[i] != 0)
        luma_weight_l1_flag = 1;

      WRITE_BITS (bw, luma_weight_l1_flag, 1);
      if (luma_weight_l1_flag) {
        WRITE_SE_RANGE (bw, p->luma_weight_l1[i], -128, 127);
        WRITE_SE_RANGE (bw, p->luma_offset_l1[i], -128, 127);
      }
      if (chroma_array_type != 0) {
        guint8 chroma_weight_l1_flag = 0;
        gint j;

        for (j = 0; j < 2; j++) {
          if (p->chroma_weight_l1[i][j] != default_chroma_weight ||
              p->chroma_offset_l1[i][j] != 0)
            chroma_weight_l1_flag = 1;
        }

        WRITE_BITS (bw, chroma_weight_l1_flag, 1);
        if (chroma_weight_l1_flag) {
          for (j = 0; j < 2; j++) {
            WRITE_SE_RANGE (bw, p->chroma_weight_l1[i][j], -128, 127);
            WRITE_SE_RANGE (bw, p->chroma_offset_l1[i][j], -128, 127);
          }
        }
      }
    }
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write \"Prediction weight table\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h264_bit_writer_slice_dec_ref_pic_marking (const GstH264SliceHdr * slice,
    guint32 nal_type, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing \"Dec Ref Pic Marking\"");

  if (nal_type == GST_H264_NAL_SLICE_IDR) {
    WRITE_BITS (bw, slice->dec_ref_pic_marking.no_output_of_prior_pics_flag, 1);
    WRITE_BITS (bw, slice->dec_ref_pic_marking.long_term_reference_flag, 1);
  } else {
    WRITE_BITS (bw,
        slice->dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag, 1);

    if (slice->dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag) {
      const GstH264RefPicMarking *refpicmarking;
      guint i;

      for (i = 0; i < slice->dec_ref_pic_marking.n_ref_pic_marking; i++) {
        refpicmarking = &slice->dec_ref_pic_marking.ref_pic_marking[i];

        WRITE_UE_MAX (bw,
            refpicmarking->memory_management_control_operation, 6);

        if (refpicmarking->memory_management_control_operation == 0)
          break;

        if (refpicmarking->memory_management_control_operation == 1
            || refpicmarking->memory_management_control_operation == 3)
          WRITE_UE (bw, refpicmarking->difference_of_pic_nums_minus1);

        if (refpicmarking->memory_management_control_operation == 2)
          WRITE_UE (bw, refpicmarking->long_term_pic_num);

        if (refpicmarking->memory_management_control_operation == 3
            || refpicmarking->memory_management_control_operation == 6)
          WRITE_UE (bw, refpicmarking->long_term_frame_idx);

        if (refpicmarking->memory_management_control_operation == 4)
          WRITE_UE (bw, refpicmarking->max_long_term_frame_idx_plus1);
      }
    }
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write \"Dec Ref Pic Marking\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h264_bit_writer_slice_hdr (const GstH264SliceHdr * slice, guint32 nal_type,
    guint32 ext_type, gboolean is_ref, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing slice header");

  WRITE_UE (bw, slice->first_mb_in_slice);
  WRITE_UE (bw, slice->type);

  WRITE_UE_MAX (bw, slice->pps->id, GST_H264_MAX_PPS_COUNT - 1);

  if (slice->pps->sequence->separate_colour_plane_flag)
    WRITE_BITS (bw, slice->colour_plane_id, 2);

  WRITE_BITS (bw, slice->frame_num,
      slice->pps->sequence->log2_max_frame_num_minus4 + 4);

  if (!slice->pps->sequence->frame_mbs_only_flag) {
    WRITE_BITS (bw, slice->field_pic_flag, 1);
    if (slice->field_pic_flag)
      WRITE_BITS (bw, slice->bottom_field_flag, 1);
  }

  if (nal_type == GST_H264_NAL_SLICE_IDR)
    WRITE_UE_MAX (bw, slice->idr_pic_id, G_MAXUINT16);

  if (slice->pps->sequence->pic_order_cnt_type == 0) {
    WRITE_BITS (bw, slice->pic_order_cnt_lsb,
        slice->pps->sequence->log2_max_pic_order_cnt_lsb_minus4 + 4);

    if (slice->pps->pic_order_present_flag && !slice->field_pic_flag)
      WRITE_SE (bw, slice->delta_pic_order_cnt_bottom);
  }

  if (slice->pps->sequence->pic_order_cnt_type == 1
      && !slice->pps->sequence->delta_pic_order_always_zero_flag) {
    WRITE_SE (bw, slice->delta_pic_order_cnt[0]);
    if (slice->pps->pic_order_present_flag && !slice->field_pic_flag)
      WRITE_SE (bw, slice->delta_pic_order_cnt[1]);
  }

  if (slice->pps->redundant_pic_cnt_present_flag)
    WRITE_UE_MAX (bw, slice->redundant_pic_cnt, G_MAXINT8);

  if (GST_H264_IS_B_SLICE (slice))
    WRITE_BITS (bw, slice->direct_spatial_mv_pred_flag, 1);

  if (GST_H264_IS_P_SLICE (slice) || GST_H264_IS_SP_SLICE (slice) ||
      GST_H264_IS_B_SLICE (slice)) {
    WRITE_BITS (bw, slice->num_ref_idx_active_override_flag, 1);
    if (slice->num_ref_idx_active_override_flag) {
      WRITE_UE_MAX (bw, slice->num_ref_idx_l0_active_minus1, 31);

      if (GST_H264_IS_B_SLICE (slice))
        WRITE_UE_MAX (bw, slice->num_ref_idx_l1_active_minus1, 31);
    }
  }

  if (!_h264_slice_bit_writer_ref_pic_list_modification (slice,
          ext_type == GST_H264_NAL_EXTENSION_MVC, bw, &have_space))
    goto error;

  if ((slice->pps->weighted_pred_flag && (GST_H264_IS_P_SLICE (slice)
              || GST_H264_IS_SP_SLICE (slice)))
      || (slice->pps->weighted_bipred_idc == 1 && GST_H264_IS_B_SLICE (slice))) {
    if (!_h264_slice_bit_writer_pred_weight_table (slice,
            slice->pps->sequence->chroma_array_type, bw, &have_space))
      goto error;
  }

  if (is_ref) {
    if (!_h264_bit_writer_slice_dec_ref_pic_marking (slice, nal_type, bw,
            &have_space))
      goto error;
  }

  if (slice->pps->entropy_coding_mode_flag && !GST_H264_IS_I_SLICE (slice) &&
      !GST_H264_IS_SI_SLICE (slice))
    WRITE_UE_MAX (bw, slice->cabac_init_idc, 2);

  WRITE_SE_RANGE (bw, slice->slice_qp_delta, -87, 77);

  if (GST_H264_IS_SP_SLICE (slice) || GST_H264_IS_SI_SLICE (slice)) {
    if (GST_H264_IS_SP_SLICE (slice))
      WRITE_BITS (bw, slice->sp_for_switch_flag, 1);

    WRITE_SE_RANGE (bw, slice->slice_qs_delta, -51, 51);
  }

  if (slice->pps->deblocking_filter_control_present_flag) {
    WRITE_UE_MAX (bw, slice->disable_deblocking_filter_idc, 2);
    if (slice->disable_deblocking_filter_idc != 1) {
      WRITE_SE_RANGE (bw, slice->slice_alpha_c0_offset_div2, -6, 6);
      WRITE_SE_RANGE (bw, slice->slice_beta_offset_div2, -6, 6);
    }
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write slice header");

  *space = have_space;
  return FALSE;
}

/**
 * gst_h264_bit_writer_slice_hdr:
 * @slice: the slice header of #GstH264SliceHdr to write
 * @start_code: whether adding the nal start code
 * @nal_type: the slice's nal type of #GstH264NalUnitType
 * @is_ref: whether the slice is a reference
 * @data: (out): the bit stream generated by the slice header
 * @size: (inout): the size in bytes of the input and output
 * @trail_bits_num: (out): the trail bits number which is not byte aligned.
 *
 * Generating the according h264 bit stream by providing the slice header.
 *
 * Returns: a #GstH264BitWriterResult
 *
 * Since: 1.22
 **/
GstH264BitWriterResult
gst_h264_bit_writer_slice_hdr (const GstH264SliceHdr * slice,
    gboolean start_code, GstH264NalUnitType nal_type, gboolean is_ref,
    guint8 * data, guint * size, guint * trail_bits_num)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;

  g_return_val_if_fail (slice != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (slice->pps != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (slice->pps->sequence != NULL,
      GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (nal_type >= GST_H264_NAL_SLICE
      && nal_type <= GST_H264_NAL_SLICE_IDR, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (trail_bits_num != NULL, GST_H264_BIT_WRITER_ERROR);

  if (nal_type == GST_H264_NAL_SLICE_IDR)
    g_return_val_if_fail (is_ref, GST_H264_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  if (start_code)
    WRITE_BITS (&bw, 0x00000001, 32);

  /* nal header */
  /* forbidden_zero_bit */
  WRITE_BITS (&bw, 0, 1);
  /* nal_ref_idc, zero for non-reference picture */
  WRITE_BITS (&bw, is_ref, 2);
  /* nal_unit_type */
  WRITE_BITS (&bw, nal_type, 5);

  if (!_h264_bit_writer_slice_hdr (slice, nal_type,
          GST_H264_NAL_EXTENSION_NONE, is_ref, &bw, &have_space))
    goto error;

  /* We do not add trailing bits here, the slice data should follow it. */

  *size = gst_bit_writer_get_size (&bw) / 8;
  *trail_bits_num = gst_bit_writer_get_size (&bw) % 8;
  gst_bit_writer_reset (&bw);
  return GST_H264_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;

  return have_space ? GST_H264_BIT_WRITER_INVALID_DATA :
      GST_H264_BIT_WRITER_NO_MORE_SPACE;
}

static gboolean
_h264_bit_writer_sei_registered_user_data (const GstH264RegisteredUserData *
    rud, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("Writing \"Registered user data\"");

  WRITE_BITS (bw, rud->country_code, 8);
  if (rud->country_code == 0xff)
    WRITE_BITS (bw, rud->country_code_extension, 8);

  WRITE_BYTES (bw, rud->data, rud->size);

  *space = TRUE;
  return TRUE;

error:GST_WARNING ("Failed to write \"Registered user data\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h264_bit_writer_sei_frame_packing (const GstH264FramePacking *
    frame_packing, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("Writing \"Frame packing\"");

  WRITE_UE (bw, frame_packing->frame_packing_id);
  WRITE_BITS (bw, frame_packing->frame_packing_cancel_flag, 1);

  if (!frame_packing->frame_packing_cancel_flag) {
    WRITE_BITS (bw, frame_packing->frame_packing_type, 7);
    WRITE_BITS (bw, frame_packing->quincunx_sampling_flag, 1);
    WRITE_BITS (bw, frame_packing->content_interpretation_type, 6);
    WRITE_BITS (bw, frame_packing->spatial_flipping_flag, 1);
    WRITE_BITS (bw, frame_packing->frame0_flipped_flag, 1);
    WRITE_BITS (bw, frame_packing->field_views_flag, 1);
    WRITE_BITS (bw, frame_packing->current_frame_is_frame0_flag, 1);
    WRITE_BITS (bw, frame_packing->frame0_self_contained_flag, 1);
    WRITE_BITS (bw, frame_packing->frame1_self_contained_flag, 1);

    if (!frame_packing->quincunx_sampling_flag &&
        frame_packing->frame_packing_type !=
        GST_H264_FRAME_PACKING_TEMPORAL_INTERLEAVING) {
      WRITE_BITS (bw, frame_packing->frame0_grid_position_x, 4);
      WRITE_BITS (bw, frame_packing->frame0_grid_position_y, 4);
      WRITE_BITS (bw, frame_packing->frame1_grid_position_x, 4);
      WRITE_BITS (bw, frame_packing->frame1_grid_position_y, 4);
    }

    /* frame_packing_arrangement_reserved_byte */
    WRITE_BITS (bw, 0, 8);
    WRITE_UE (bw, frame_packing->frame_packing_repetition_period);
  }

  /* frame_packing_arrangement_extension_flag */
  WRITE_BITS (bw, 0, 1);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("Failed to write \"Frame packing\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h264_bit_writer_sei_mastering_display_colour_volume (const
    GstH264MasteringDisplayColourVolume * mdcv, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;
  gint i;

  GST_DEBUG ("Wrtiting \"Mastering display colour volume\"");

  for (i = 0; i < 3; i++) {
    WRITE_BITS (bw, mdcv->display_primaries_x[i], 16);
    WRITE_BITS (bw, mdcv->display_primaries_y[i], 16);
  }

  WRITE_BITS (bw, mdcv->white_point_x, 16);
  WRITE_BITS (bw, mdcv->white_point_y, 16);
  WRITE_BITS (bw, mdcv->max_display_mastering_luminance, 32);
  WRITE_BITS (bw, mdcv->min_display_mastering_luminance, 32);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("Failed to write \"Mastering display colour volume\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h264_bit_writer_sei_content_light_level_info (const
    GstH264ContentLightLevel * cll, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("Writing \"Content light level\"");

  WRITE_BITS (bw, cll->max_content_light_level, 16);
  WRITE_BITS (bw, cll->max_pic_average_light_level, 16);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("Failed to write \"Content light level\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h264_bit_writer_sei_pic_timing (const GstH264PicTiming * tim,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("Writing \"Picture timing\"");

  if (tim->CpbDpbDelaysPresentFlag) {
    WRITE_BITS (bw, tim->cpb_removal_delay,
        tim->cpb_removal_delay_length_minus1 + 1);
    WRITE_BITS (bw, tim->dpb_output_delay,
        tim->dpb_output_delay_length_minus1 + 1);
  }

  if (tim->pic_struct_present_flag) {
    const guint8 num_clock_ts_table[9] = {
      1, 1, 1, 2, 2, 3, 3, 2, 3
    };
    guint8 num_clock_num_ts;
    guint i;

    WRITE_BITS (bw, tim->pic_struct, 4);

    num_clock_num_ts = num_clock_ts_table[tim->pic_struct];
    for (i = 0; i < num_clock_num_ts; i++) {
      WRITE_BITS (bw, tim->clock_timestamp_flag[i], 1);
      if (tim->clock_timestamp_flag[i]) {
        const GstH264ClockTimestamp *timestamp = &tim->clock_timestamp[i];

        WRITE_BITS (bw, timestamp->ct_type, 2);
        WRITE_BITS (bw, timestamp->nuit_field_based_flag, 1);
        WRITE_BITS (bw, timestamp->counting_type, 5);
        WRITE_BITS (bw, timestamp->full_timestamp_flag, 1);
        WRITE_BITS (bw, timestamp->discontinuity_flag, 1);
        WRITE_BITS (bw, timestamp->cnt_dropped_flag, 1);
        WRITE_BITS (bw, timestamp->n_frames, 8);

        if (timestamp->full_timestamp_flag) {
          if (!timestamp->seconds_flag || !timestamp->minutes_flag
              || !timestamp->hours_flag)
            goto error;

          WRITE_BITS (bw, timestamp->seconds_value, 6);
          WRITE_BITS (bw, timestamp->minutes_value, 6);
          WRITE_BITS (bw, timestamp->hours_value, 5);
        } else {
          WRITE_BITS (bw, timestamp->seconds_flag, 1);
          if (timestamp->seconds_flag) {
            WRITE_BITS (bw, timestamp->seconds_value, 6);
            WRITE_BITS (bw, timestamp->minutes_flag, 1);
            if (timestamp->minutes_flag) {
              WRITE_BITS (bw, timestamp->minutes_value, 6);
              WRITE_BITS (bw, timestamp->hours_flag, 1);
              if (timestamp->hours_flag)
                WRITE_BITS (bw, timestamp->hours_value, 5);
            }
          }
        }

        if (tim->time_offset_length > 0) {
          WRITE_BITS (bw, timestamp->time_offset, tim->time_offset_length);
        }
      }
    }
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("Failed to write \"Picture timing\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h264_bit_writer_sei_buffering_period (const GstH264BufferingPeriod * per,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("Writing \"Buffering period\"");

  if (!per->sps)
    goto error;

  WRITE_UE_MAX (bw, per->sps->id, GST_H264_MAX_SPS_COUNT - 1);

  if (per->sps->vui_parameters_present_flag) {
    GstH264VUIParams *vui = &per->sps->vui_parameters;

    if (vui->nal_hrd_parameters_present_flag) {
      GstH264HRDParams *hrd = &vui->nal_hrd_parameters;
      const guint8 nbits = hrd->initial_cpb_removal_delay_length_minus1 + 1;
      guint8 sched_sel_idx;

      for (sched_sel_idx = 0; sched_sel_idx <= hrd->cpb_cnt_minus1;
          sched_sel_idx++) {
        WRITE_BITS (bw, per->nal_initial_cpb_removal_delay[sched_sel_idx],
            nbits);
        WRITE_BITS (bw,
            per->nal_initial_cpb_removal_delay_offset[sched_sel_idx], nbits);
      }
    }

    if (vui->vcl_hrd_parameters_present_flag) {
      GstH264HRDParams *hrd = &vui->vcl_hrd_parameters;
      const guint8 nbits = hrd->initial_cpb_removal_delay_length_minus1 + 1;
      guint8 sched_sel_idx;

      for (sched_sel_idx = 0; sched_sel_idx <= hrd->cpb_cnt_minus1;
          sched_sel_idx++) {
        WRITE_BITS (bw, per->vcl_initial_cpb_removal_delay[sched_sel_idx],
            nbits);
        WRITE_BITS (bw,
            per->vcl_initial_cpb_removal_delay_offset[sched_sel_idx], nbits);
      }
    }
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("Failed to write \"Buffering period\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h264_bit_writer_sei_message (const GstH264SEIMessage * msg,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing SEI message");

  switch (msg->payloadType) {
    case GST_H264_SEI_REGISTERED_USER_DATA:
      if (!_h264_bit_writer_sei_registered_user_data
          (&msg->payload.registered_user_data, bw, &have_space))
        goto error;
      break;
    case GST_H264_SEI_FRAME_PACKING:
      if (!_h264_bit_writer_sei_frame_packing
          (&msg->payload.frame_packing, bw, &have_space))
        goto error;
      break;
    case GST_H264_SEI_MASTERING_DISPLAY_COLOUR_VOLUME:
      if (!_h264_bit_writer_sei_mastering_display_colour_volume
          (&msg->payload.mastering_display_colour_volume, bw, &have_space))
        goto error;
      break;
    case GST_H264_SEI_CONTENT_LIGHT_LEVEL:
      if (!_h264_bit_writer_sei_content_light_level_info
          (&msg->payload.content_light_level, bw, &have_space))
        goto error;
      break;
    case GST_H264_SEI_PIC_TIMING:
      if (!_h264_bit_writer_sei_pic_timing (&msg->payload.pic_timing, bw,
              &have_space))
        goto error;
      break;
    case GST_H264_SEI_BUF_PERIOD:
      if (!_h264_bit_writer_sei_buffering_period
          (&msg->payload.buffering_period, bw, &have_space))
        goto error;
      break;
    default:
      break;
  }

  /* Add trailings. */
  WRITE_BITS (bw, 1, 1);
  gst_bit_writer_align_bytes_unchecked (bw, 0);

  *space = TRUE;

  return TRUE;

error:
  GST_WARNING ("error to write SEI message");

  *space = have_space;
  return FALSE;
}

/**
 * gst_h264_bit_writer_sei:
 * @sei_messages: An array of #GstH264SEIMessage to write
 * @start_code: whether adding the nal start code
 * @data: (out): the bit stream generated by the sei messages
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according h264 bit stream by providing sei messages.
 *
 * Returns: a #GstH264BitWriterResult
 *
 * Since: 1.22
 **/
GstH264BitWriterResult
gst_h264_bit_writer_sei (GArray * sei_messages, gboolean start_code,
    guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;
  GstBitWriter bw_msg;
  GstH264SEIMessage *sei;
  gboolean have_written_data = FALSE;
  guint i;

  g_return_val_if_fail (sei_messages != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_H264_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  if (start_code)
    WRITE_BITS (&bw, 0x00000001, 32);

  /* nal header */
  /* forbidden_zero_bit */
  WRITE_BITS (&bw, 0, 1);
  /* nal_ref_idc, zero for sei nalu */
  WRITE_BITS (&bw, 0, 2);
  /* nal_unit_type */
  WRITE_BITS (&bw, GST_H264_NAL_SEI, 5);

  for (i = 0; i < sei_messages->len; i++) {
    guint32 payload_size_data;
    guint32 payload_type_data;
    guint32 sz;

    gst_bit_writer_init (&bw_msg);

    sei = &g_array_index (sei_messages, GstH264SEIMessage, i);
    if (!_h264_bit_writer_sei_message (sei, &bw_msg, &have_space))
      goto error;

    if (gst_bit_writer_get_size (&bw_msg) == 0) {
      GST_FIXME ("Unsupported SEI type %d", sei->payloadType);
      continue;
    }

    have_written_data = TRUE;

    g_assert (gst_bit_writer_get_size (&bw_msg) % 8 == 0);
    payload_size_data = gst_bit_writer_get_size (&bw_msg) / 8;
    payload_type_data = sei->payloadType;

    /* write payload type bytes */
    while (payload_type_data >= 0xff) {
      WRITE_BITS (&bw, 0xff, 8);
      payload_type_data -= 0xff;
    }
    WRITE_BITS (&bw, payload_type_data, 8);

    /* write payload size bytes */
    sz = payload_size_data;
    while (sz >= 0xff) {
      WRITE_BITS (&bw, 0xff, 8);
      sz -= 0xff;
    }
    WRITE_BITS (&bw, sz, 8);

    if (payload_size_data > 0)
      WRITE_BYTES (&bw, gst_bit_writer_get_data (&bw_msg), payload_size_data);

    gst_bit_writer_reset (&bw_msg);
  }

  if (!have_written_data) {
    GST_WARNING ("No written sei data");
    goto error;
  }

  /* Add trailings. */
  WRITE_BITS (&bw, 1, 1);
  if (!gst_bit_writer_align_bytes (&bw, 0)) {
    have_space = FALSE;
    goto error;
  }

  *size = (gst_bit_writer_get_size (&bw)) / 8;
  gst_bit_writer_reset (&bw);
  return GST_H264_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;

  return have_space ? GST_H264_BIT_WRITER_INVALID_DATA :
      GST_H264_BIT_WRITER_NO_MORE_SPACE;
}

/**
 * gst_h264_bit_writer_aud:
 * @primary_pic_type: indicate the possible slice types list just
 *   as the H264 spec defines
 * @start_code: whether adding the nal start code
 * @data: (out): the bit stream generated by the aud
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according h264 bit stream of an aud.
 *
 * Returns: a #GstH264BitWriterResult
 *
 * Since: 1.22
 **/
GstH264BitWriterResult
gst_h264_bit_writer_aud (guint8 primary_pic_type, gboolean start_code,
    guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;

  g_return_val_if_fail (primary_pic_type <= 7, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_H264_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  if (start_code)
    WRITE_BITS (&bw, 0x00000001, 32);

  /* nal header */
  /* forbidden_zero_bit */
  WRITE_BITS (&bw, 0, 1);
  /* nal_ref_idc */
  WRITE_BITS (&bw, 0, 2);
  /* nal_unit_type */
  WRITE_BITS (&bw, GST_H264_NAL_AU_DELIMITER, 5);

  WRITE_BITS (&bw, primary_pic_type, 3);

  /* Add trailings. */
  WRITE_BITS (&bw, 1, 1);
  if (!gst_bit_writer_align_bytes (&bw, 0)) {
    goto error;
  }

  *size = (gst_bit_writer_get_size (&bw)) / 8;
  gst_bit_writer_reset (&bw);

  return GST_H264_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;

  return have_space ? GST_H264_BIT_WRITER_INVALID_DATA :
      GST_H264_BIT_WRITER_NO_MORE_SPACE;
}

/**
 * gst_h264_bit_writer_convert_to_nal:
 * @nal_prefix_size: the size in bytes for the prefix of a nal, may
 *   be 2, 3 or 4
 * @packetized: whether to write the bit stream in packetized format,
 *   which does not have the start code but has a @nal_prefix_size bytes'
 *   size prepending to the real nal data
 * @has_startcode: whether the input already has a start code
 * @add_trailings: whether to add rbsp trailing bits to make the output
 *   aligned to byte
 * @raw_data: the input bit stream
 * @raw_size: the size in bits of the input bit stream
 * @nal_data: (out): the output bit stream converted to a real nal
 * @nal_size: (inout): the size in bytes of the output
 *
 * Converting a bit stream into a real nal packet. If the bit stream already
 * has a start code, it will be replaced by the new one specified by the
 * @nal_prefix_size and @packetized. It is assured that the output aligns to
 * the byte and the all the emulations are inserted.
 *
 * Returns: a #GstH264BitWriterResult
 *
 * Since: 1.22
 **/
GstH264BitWriterResult
gst_h264_bit_writer_convert_to_nal (guint nal_prefix_size, gboolean packetized,
    gboolean has_startcode, gboolean add_trailings, const guint8 * raw_data,
    gsize raw_size, guint8 * nal_data, guint * nal_size)
{
  NalWriter nw;
  guint8 *data;
  guint32 size = 0;
  gboolean need_more_space = FALSE;

  g_return_val_if_fail (
      (packetized && nal_prefix_size > 1 && nal_prefix_size < 5) ||
      (!packetized && (nal_prefix_size == 3 || nal_prefix_size == 4)),
      GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (raw_data != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (raw_size > 0, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (nal_data != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (nal_size != NULL, GST_H264_BIT_WRITER_ERROR);
  g_return_val_if_fail (*nal_size > 0, GST_H264_BIT_WRITER_ERROR);

  if (has_startcode) {
    /* Skip the start code, the NalWriter will add it automatically. */
    if (raw_size >= 4 && raw_data[0] == 0
        && raw_data[1] == 0 && raw_data[2] == 0 && raw_data[3] == 0x01) {
      raw_data += 4;
      raw_size -= 4 * 8;
    } else if (raw_size >= 3 && raw_data[0] == 0 && raw_data[1] == 0
        && raw_data[2] == 0x01) {
      raw_data += 3;
      raw_size -= 3 * 8;
    } else {
      /* Fail to find the start code. */
      g_return_val_if_reached (GST_H264_BIT_WRITER_ERROR);
    }
  }

  /* If no RBSP trailing needed, it must align to byte. We assume
     that the rbsp trailing bits are already added. */
  if (!add_trailings)
    g_return_val_if_fail (raw_size % 8 == 0, GST_H264_BIT_WRITER_ERROR);

  nal_writer_init (&nw, nal_prefix_size, packetized);

  if (!nal_writer_put_bytes (&nw, raw_data, raw_size / 8))
    goto error;

  if (raw_size % 8) {
    guint8 data = *(raw_data + raw_size / 8);

    if (!nal_writer_put_bits_uint8 (&nw,
            data >> (8 - raw_size % 8), raw_size % 8))
      goto error;
  }

  if (add_trailings) {
    if (!nal_writer_do_rbsp_trailing_bits (&nw))
      goto error;
  }

  data = nal_writer_reset_and_get_data (&nw, &size);
  if (!data)
    goto error;

  if (size > *nal_size) {
    need_more_space = TRUE;
    g_free (data);
    goto error;
  }

  memcpy (nal_data, data, size);
  *nal_size = size;
  g_free (data);
  nal_writer_reset (&nw);
  return GST_H264_BIT_WRITER_OK;

error:
  nal_writer_reset (&nw);
  *nal_size = 0;

  GST_WARNING ("Failed to convert nal data");

  return need_more_space ? GST_H264_BIT_WRITER_INVALID_DATA :
      GST_H264_BIT_WRITER_NO_MORE_SPACE;
}
