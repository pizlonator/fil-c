/* GStreamer
 *  Copyright (C) 2021 Intel Corporation
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
#include "config.h"
#endif

#include "gsth265bitwriter.h"
#include <gst/codecparsers/nalutils.h>
#include <gst/base/gstbitwriter.h>
#include <math.h>

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

#define EXTENDED_SAR 255

/**** Default scaling_lists according to Table 7-5 and 7-6 *****/
/* Table 7-5 */
static const guint8 default_scaling_list0[16] = {
  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
  16, 16, 16, 16
};

/*  Combined the values in Table  7-6 to make the calculation easier
 *  Default scaling list of 8x8 and 16x16 matrices for matrixId = 0, 1 and 2
 *  Default scaling list of 32x32 matrix for matrixId = 0
 */
static const guint8 default_scaling_list1[64] = {
  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 16,
  17, 16, 17, 18, 17, 18, 18, 17, 18, 21, 19, 20,
  21, 20, 19, 21, 24, 22, 22, 24, 24, 22, 22, 24,
  25, 25, 27, 30, 27, 25, 25, 29, 31, 35, 35, 31,
  29, 36, 41, 44, 41, 36, 47, 54, 54, 47, 65, 70,
  65, 88, 88, 115
};

/*  Combined the values in Table 7-6 to make the calculation easier
 *  Default scaling list of 8x8 and 16x16 matrices for matrixId = 3, 4 and 5
 *  Default scaling list of 32x32 matrix for matrixId = 1
 */
static const guint8 default_scaling_list2[64] = {
  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17,
  17, 17, 17, 18, 18, 18, 18, 18, 18, 20, 20, 20,
  20, 20, 20, 20, 24, 24, 24, 24, 24, 24, 24, 24,
  25, 25, 25, 25, 25, 25, 25, 28, 28, 28, 28, 28,
  28, 33, 33, 33, 33, 33, 41, 41, 41, 41, 54, 54,
  54, 71, 71, 91
};

static gboolean
_h265_bit_writer_profile_tier_level (const GstH265ProfileTierLevel * ptl,
    guint8 maxNumSubLayersMinus1, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  guint i, j;

  GST_DEBUG ("writing profile_tier_level");

  WRITE_BITS (bw, ptl->profile_space, 2);
  WRITE_BITS (bw, ptl->tier_flag, 1);
  WRITE_BITS (bw, ptl->profile_idc, 5);

  for (j = 0; j < 32; j++)
    WRITE_BITS (bw, ptl->profile_compatibility_flag[j], 1);

  WRITE_BITS (bw, ptl->progressive_source_flag, 1);
  WRITE_BITS (bw, ptl->interlaced_source_flag, 1);
  WRITE_BITS (bw, ptl->non_packed_constraint_flag, 1);
  WRITE_BITS (bw, ptl->frame_only_constraint_flag, 1);

  if (ptl->profile_idc == 4 || ptl->profile_compatibility_flag[4] ||
      ptl->profile_idc == 5 || ptl->profile_compatibility_flag[5] ||
      ptl->profile_idc == 6 || ptl->profile_compatibility_flag[6] ||
      ptl->profile_idc == 7 || ptl->profile_compatibility_flag[7] ||
      ptl->profile_idc == 8 || ptl->profile_compatibility_flag[8] ||
      ptl->profile_idc == 9 || ptl->profile_compatibility_flag[9] ||
      ptl->profile_idc == 10 || ptl->profile_compatibility_flag[10] ||
      ptl->profile_idc == 11 || ptl->profile_compatibility_flag[11]) {
    WRITE_BITS (bw, ptl->max_12bit_constraint_flag, 1);
    WRITE_BITS (bw, ptl->max_10bit_constraint_flag, 1);
    WRITE_BITS (bw, ptl->max_8bit_constraint_flag, 1);
    WRITE_BITS (bw, ptl->max_422chroma_constraint_flag, 1);
    WRITE_BITS (bw, ptl->max_420chroma_constraint_flag, 1);
    WRITE_BITS (bw, ptl->max_monochrome_constraint_flag, 1);
    WRITE_BITS (bw, ptl->intra_constraint_flag, 1);
    WRITE_BITS (bw, ptl->one_picture_only_constraint_flag, 1);
    WRITE_BITS (bw, ptl->lower_bit_rate_constraint_flag, 1);

    if (ptl->profile_idc == 5 || ptl->profile_compatibility_flag[5] ||
        ptl->profile_idc == 9 || ptl->profile_compatibility_flag[9] ||
        ptl->profile_idc == 10 || ptl->profile_compatibility_flag[10] ||
        ptl->profile_idc == 11 || ptl->profile_compatibility_flag[11]) {
      WRITE_BITS (bw, ptl->max_14bit_constraint_flag, 1);
      /* general_reserved_zero_33bits */
      WRITE_BITS (bw, 0, 32);
      WRITE_BITS (bw, 0, 1);
    } else {
      /* general_reserved_zero_34bits */
      WRITE_BITS (bw, 0, 32);
      WRITE_BITS (bw, 0, 2);
    }
  } else if (ptl->profile_idc == 2 || ptl->profile_compatibility_flag[2]) {
    /* general_reserved_zero_7bits */
    WRITE_BITS (bw, 0, 7);
    WRITE_BITS (bw, ptl->one_picture_only_constraint_flag, 1);
    /* general_reserved_zero_35bits */
    WRITE_BITS (bw, 0, 32);
    WRITE_BITS (bw, 0, 3);
  } else {
    /* general_reserved_zero_43bits */
    WRITE_BITS (bw, 0, 32);
    WRITE_BITS (bw, 0, 11);
  }

  /* general_inbld_flag, just set to 0 */
  WRITE_BITS (bw, 0, 1);

  WRITE_BITS (bw, ptl->level_idc, 8);

  for (j = 0; j < maxNumSubLayersMinus1; j++) {
    if (ptl->sub_layer_profile_present_flag[j]) {
      GST_WARNING ("sub layer profile does not supported now");
      goto error;
    }
    WRITE_BITS (bw, ptl->sub_layer_profile_present_flag[j], 1);

    if (ptl->sub_layer_level_present_flag[j]) {
      GST_WARNING ("sub layer level does not supported now");
      goto error;
    }
    WRITE_BITS (bw, ptl->sub_layer_level_present_flag[j], 1);
  }

  if (maxNumSubLayersMinus1 > 0) {
    for (i = maxNumSubLayersMinus1; i < 8; i++)
      /* reserved_zero_2bits */
      WRITE_BITS (bw, 0, 2);
  }

  /* TODO: Add sub layers profiles. */

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write profile_tier_level");

  *space = have_space;
  return FALSE;
}

static gboolean
_h265_bit_writer_sub_layer_hrd_parameters (const GstH265SubLayerHRDParams *
    sub_hrd, guint8 CpbCnt, guint8 sub_pic_hrd_params_present_flag,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  guint i;

  GST_DEBUG ("writing \"subLayer HRD Parameters\"");

  for (i = 0; i <= CpbCnt; i++) {
    WRITE_UE_MAX (bw, sub_hrd->bit_rate_value_minus1[i], G_MAXUINT32 - 1);
    WRITE_UE_MAX (bw, sub_hrd->cpb_size_value_minus1[i], G_MAXUINT32 - 1);

    if (sub_pic_hrd_params_present_flag) {
      WRITE_UE_MAX (bw, sub_hrd->cpb_size_du_value_minus1[i], G_MAXUINT32 - 1);
      WRITE_UE_MAX (bw, sub_hrd->bit_rate_du_value_minus1[i], G_MAXUINT32 - 1);
    }

    WRITE_BITS (bw, sub_hrd->cbr_flag[i], 1);
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write \"subLayer HRD Parameters \"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h265_bit_writer_hrd_parameters (const GstH265HRDParams * hrd,
    guint8 commonInfPresentFlag, guint8 maxNumSubLayersMinus1,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  guint i;

  GST_DEBUG ("writing \"HRD Parameters\"");

  if (commonInfPresentFlag) {
    WRITE_BITS (bw, hrd->nal_hrd_parameters_present_flag, 1);
    WRITE_BITS (bw, hrd->vcl_hrd_parameters_present_flag, 1);

    if (hrd->nal_hrd_parameters_present_flag
        || hrd->vcl_hrd_parameters_present_flag) {
      WRITE_BITS (bw, hrd->sub_pic_hrd_params_present_flag, 1);

      if (hrd->sub_pic_hrd_params_present_flag) {
        WRITE_BITS (bw, hrd->tick_divisor_minus2, 8);
        WRITE_BITS (bw, hrd->du_cpb_removal_delay_increment_length_minus1, 5);
        WRITE_BITS (bw, hrd->sub_pic_cpb_params_in_pic_timing_sei_flag, 1);
        WRITE_BITS (bw, hrd->dpb_output_delay_du_length_minus1, 5);
      }

      WRITE_BITS (bw, hrd->bit_rate_scale, 4);
      WRITE_BITS (bw, hrd->cpb_size_scale, 4);

      if (hrd->sub_pic_hrd_params_present_flag)
        WRITE_BITS (bw, hrd->cpb_size_du_scale, 4);

      WRITE_BITS (bw, hrd->initial_cpb_removal_delay_length_minus1, 5);
      WRITE_BITS (bw, hrd->au_cpb_removal_delay_length_minus1, 5);
      WRITE_BITS (bw, hrd->dpb_output_delay_length_minus1, 5);
    }
  }

  for (i = 0; i <= maxNumSubLayersMinus1; i++) {
    WRITE_BITS (bw, hrd->fixed_pic_rate_general_flag[i], 1);

    if (!hrd->fixed_pic_rate_general_flag[i]) {
      WRITE_BITS (bw, hrd->fixed_pic_rate_within_cvs_flag[i], 1);
    } else {
      if (hrd->fixed_pic_rate_within_cvs_flag[i] == 0)
        goto error;
    }

    if (hrd->fixed_pic_rate_within_cvs_flag[i]) {
      WRITE_UE_MAX (bw, hrd->elemental_duration_in_tc_minus1[i], 2047);
    } else {
      WRITE_BITS (bw, hrd->low_delay_hrd_flag[i], 1);
    }

    if (!hrd->low_delay_hrd_flag[i])
      WRITE_UE_MAX (bw, hrd->cpb_cnt_minus1[i], 31);

    if (hrd->nal_hrd_parameters_present_flag)
      if (!_h265_bit_writer_sub_layer_hrd_parameters
          (&hrd->sublayer_hrd_params[i], hrd->cpb_cnt_minus1[i],
              hrd->sub_pic_hrd_params_present_flag, bw, &have_space))
        goto error;

    /* TODO: need to separate nal and vcl from hrd_parameters. */
    if (hrd->vcl_hrd_parameters_present_flag)
      if (!_h265_bit_writer_sub_layer_hrd_parameters
          (&hrd->sublayer_hrd_params[i], hrd->cpb_cnt_minus1[i],
              hrd->sub_pic_hrd_params_present_flag, bw, &have_space))
        goto error;
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write \"HRD Parameters\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h265_bit_writer_vps (const GstH265VPS * vps, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;
  guint i, j;

  GST_DEBUG ("writing VPS");

  WRITE_BITS (bw, vps->id, 4);

  WRITE_BITS (bw, vps->base_layer_internal_flag, 1);
  WRITE_BITS (bw, vps->base_layer_available_flag, 1);

  WRITE_BITS (bw, vps->max_layers_minus1, 6);
  WRITE_BITS (bw, vps->max_sub_layers_minus1, 3);
  WRITE_BITS (bw, vps->temporal_id_nesting_flag, 1);

  /* reserved_0xffff_16bits */
  WRITE_BITS (bw, 0xffff, 16);

  if (!_h265_bit_writer_profile_tier_level (&vps->profile_tier_level,
          vps->max_sub_layers_minus1, bw, &have_space))
    goto error;

  WRITE_BITS (bw, vps->sub_layer_ordering_info_present_flag, 1);

  for (i = (vps->sub_layer_ordering_info_present_flag ? 0 :
          vps->max_sub_layers_minus1); i <= vps->max_sub_layers_minus1; i++) {
    WRITE_UE (bw, vps->max_dec_pic_buffering_minus1[i]);
    WRITE_UE_MAX (bw, vps->max_num_reorder_pics[i],
        vps->max_dec_pic_buffering_minus1[i]);
    WRITE_UE_MAX (bw, vps->max_latency_increase_plus1[i], G_MAXUINT32 - 1);
  }

  /* max_layer_id should be <63, but only support 1 layer now. */
  if (vps->max_layer_id > 1) {
    GST_WARNING ("multi layers are not supported now");
    goto error;
  }

  WRITE_BITS (bw, vps->max_layer_id, 6);

  if (vps->num_layer_sets_minus1 >= 1) {
    GST_WARNING ("layer set is not supported now");
    goto error;
  }
  WRITE_UE_MAX (bw, vps->num_layer_sets_minus1, 1023);

  /* TODO: support multi-layer. */
  for (i = 1; i <= vps->num_layer_sets_minus1; i++) {
    for (j = 0; j <= vps->max_layer_id; j++) {
      /* layer_id_included_flag[i][j] */
      WRITE_BITS (bw, 0, 1);
    }
  }

  WRITE_BITS (bw, vps->timing_info_present_flag, 1);
  if (vps->timing_info_present_flag) {
    WRITE_BITS (bw, vps->num_units_in_tick, 32);
    WRITE_BITS (bw, vps->time_scale, 32);
    WRITE_BITS (bw, vps->poc_proportional_to_timing_flag, 1);

    if (vps->poc_proportional_to_timing_flag)
      WRITE_UE_MAX (bw, vps->num_ticks_poc_diff_one_minus1, G_MAXUINT32 - 1);

    /* TODO: VPS can have multiple hrd parameters, and therefore hrd_params
     * should be an array (like Garray). Just support 1 hdr parameter now.
     */
    if (vps->num_hrd_parameters > 1) {
      GST_WARNING ("HRD parameters > 1 is not supported now");
      goto error;
    }
    WRITE_UE_MAX (bw, vps->num_hrd_parameters, vps->num_layer_sets_minus1 + 1);

    if (vps->num_hrd_parameters) {
      WRITE_UE_MAX (bw, vps->hrd_layer_set_idx, vps->num_layer_sets_minus1);

      if (!_h265_bit_writer_hrd_parameters (&vps->hrd_params,
              vps->cprms_present_flag, vps->max_sub_layers_minus1,
              bw, &have_space))
        goto error;
    }

  }

  if (vps->vps_extension) {
    GST_WARNING ("vps extension is not supported now");
    goto error;
  }
  WRITE_BITS (bw, 0, 1);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write VPS");

  *space = have_space;
  return FALSE;
}

/**
 * gst_h265_bit_writer_vps:
 * @vps: the vps of #GstH265VPS to write
 * @start_code: whether adding the nal start code
 * @data: (out): the bit stream generated by the sps
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according h265 bit stream by providing the vps.
 *
 * Returns: a #GstH265BitWriterResult
 *
 * Since: 1.22
 **/
GstH265BitWriterResult
gst_h265_bit_writer_vps (const GstH265VPS * vps, gboolean start_code,
    guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;

  g_return_val_if_fail (vps != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_H265_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  if (start_code)
    WRITE_BITS (&bw, 0x00000001, 32);

  /* NAL unit header */
  /* forbidden_zero_bit */
  WRITE_BITS (&bw, 0, 1);
  /* nal_unit_type */
  WRITE_BITS (&bw, GST_H265_NAL_VPS, 6);
  /* nuh_layer_id, only support 0 now */
  WRITE_BITS (&bw, 0, 6);
  /* nuh_temporal_id_plus1, only support 1 now */
  WRITE_BITS (&bw, 1, 3);

  if (!_h265_bit_writer_vps (vps, &bw, &have_space))
    goto error;

  /* Add trailings. */
  WRITE_BITS (&bw, 1, 1);
  if (!gst_bit_writer_align_bytes (&bw, 0)) {
    have_space = FALSE;
    goto error;
  }

  *size = gst_bit_writer_get_size (&bw) / 8;
  gst_bit_writer_reset (&bw);

  return GST_H265_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;

  if (!have_space)
    return GST_H265_BIT_WRITER_NO_MORE_SPACE;
  return GST_H265_BIT_WRITER_INVALID_DATA;
}

static gboolean
_get_scaling_list_params (const GstH265ScalingList * dest_scaling_list,
    guint8 sizeId, guint8 matrixId, const guint8 ** sl, guint8 * size,
    gint16 * scaling_list_dc_coef_minus8)
{
  switch (sizeId) {
    case GST_H265_QUANT_MATIX_4X4:
      *sl = dest_scaling_list->scaling_lists_4x4[matrixId];
      if (size)
        *size = 16;
      break;
    case GST_H265_QUANT_MATIX_8X8:
      *sl = dest_scaling_list->scaling_lists_8x8[matrixId];
      if (size)
        *size = 64;
      break;
    case GST_H265_QUANT_MATIX_16X16:
      *sl = dest_scaling_list->scaling_lists_16x16[matrixId];
      if (size)
        *size = 64;
      if (scaling_list_dc_coef_minus8)
        *scaling_list_dc_coef_minus8 =
            dest_scaling_list->scaling_list_dc_coef_minus8_16x16[matrixId];
      break;
    case GST_H265_QUANT_MATIX_32X32:
      *sl = dest_scaling_list->scaling_lists_32x32[matrixId];
      if (size)
        *size = 64;
      if (scaling_list_dc_coef_minus8)
        *scaling_list_dc_coef_minus8 =
            dest_scaling_list->scaling_list_dc_coef_minus8_32x32[matrixId];
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  return TRUE;
}

static const guint8 *
_get_default_scaling_lists (GstH265QuantMatrixSize sizeId, guint8 matrixId)
{
  const guint8 *sl;

  switch (sizeId) {
    case GST_H265_QUANT_MATIX_4X4:
      sl = default_scaling_list0;
      break;

    case GST_H265_QUANT_MATIX_8X8:
    case GST_H265_QUANT_MATIX_16X16:
      if (matrixId <= 2) {
        sl = default_scaling_list1;
      } else {
        sl = default_scaling_list2;
      }
      break;

    case GST_H265_QUANT_MATIX_32X32:
      if (matrixId == 0) {
        sl = default_scaling_list1;
      } else {
        sl = default_scaling_list2;
      }
      break;

    default:
      g_assert_not_reached ();
      return NULL;
  }

  return sl;
}

static gboolean
_compare_scaling_list_matrix (GstH265QuantMatrixSize sizeId,
    const guint8 * sl0, const guint8 * sl1,
    gint16 dc_coef_minus8_0, gint16 dc_coef_minus8_1)
{
  guint size = sizeId == GST_H265_QUANT_MATIX_4X4 ? 16 : 64;

  if (memcmp (sl0, sl1, size * sizeof (guint8)))
    return FALSE;

  if (sizeId <= GST_H265_QUANT_MATIX_8X8)
    return TRUE;

  return dc_coef_minus8_0 == dc_coef_minus8_1;
}

static gboolean
_h265_bit_writer_scaling_lists (const GstH265ScalingList * src_scaling_list,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  GstH265QuantMatrixSize sizeId;
  guint8 matrixId;
  guint8 scaling_list_pred_mode_flag = 0;
  guint8 size, i, j;

  GST_DEBUG ("writing scaling lists");

  for (sizeId = 0; sizeId <= GST_H265_QUANT_MATIX_32X32; sizeId++) {
    for (matrixId = 0;
        matrixId < ((sizeId == GST_H265_QUANT_MATIX_32X32) ? 2 : 6);
        matrixId++) {
      gint16 scaling_list_dc_coef_minus8 = 8;
      const guint8 *sl;
      const guint8 *default_sl;
      guint8 nextCoef;
      gint8 coef_val;
      guint8 scaling_list_pred_matrix_id_delta;

      if (!_get_scaling_list_params (src_scaling_list, sizeId, matrixId,
              &sl, &size, &scaling_list_dc_coef_minus8))
        goto error;

      /* Check whether it is the default matrix. */
      default_sl = _get_default_scaling_lists (sizeId, matrixId);
      if (_compare_scaling_list_matrix (sizeId, sl, default_sl,
              scaling_list_dc_coef_minus8, 8)) {
        scaling_list_pred_mode_flag = 0;
        WRITE_BITS (bw, scaling_list_pred_mode_flag, 1);
        scaling_list_pred_matrix_id_delta = 0;
        WRITE_UE_MAX (bw, scaling_list_pred_matrix_id_delta, matrixId);
        continue;
      }

      /* If some previous matrix is the same, just ref it. */
      scaling_list_pred_matrix_id_delta = 0;
      for (j = 0; j < matrixId; j++) {
        gboolean ret;
        guint8 size2;
        const guint8 *sl2;
        gint16 scaling_list_dc_coef_minus8_2 = 8;

        ret = _get_scaling_list_params (src_scaling_list, sizeId, j,
            &sl2, &size2, &scaling_list_dc_coef_minus8_2);
        g_assert (ret);
        g_assert (size == size2);

        if (_compare_scaling_list_matrix (sizeId, sl, sl2,
                scaling_list_dc_coef_minus8, scaling_list_dc_coef_minus8_2)) {
          scaling_list_pred_matrix_id_delta = matrixId - j;
          break;
        }
      }

      if (scaling_list_pred_matrix_id_delta > 0) {
        scaling_list_pred_mode_flag = 0;
        WRITE_BITS (bw, scaling_list_pred_mode_flag, 1);
        WRITE_UE_MAX (bw, scaling_list_pred_matrix_id_delta, matrixId);
        continue;
      }

      /* Just explicitly signal all matrix coef. */
      scaling_list_pred_mode_flag = 1;
      WRITE_BITS (bw, scaling_list_pred_mode_flag, 1);

      nextCoef = 8;

      if (sizeId > 1) {
        WRITE_SE_RANGE (bw, scaling_list_dc_coef_minus8, -7, 247);
        nextCoef = scaling_list_dc_coef_minus8 + 8;
      }

      for (i = 0; i < size; i++) {
        coef_val = sl[i] - nextCoef;
        nextCoef = sl[i];

        if (coef_val > 127) {
          coef_val = coef_val - 256;
        }
        if (coef_val < -128) {
          coef_val = coef_val + 256;
        }

        WRITE_SE_RANGE (bw, coef_val, -128, 127);
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
_h265_bit_writer_short_term_ref_pic_set (const GstH265ShortTermRefPicSet *
    stRPS, guint8 stRpsIdx, const GstH265SPS * sps,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  gint32 prev;
  gint i = 0;

  GST_DEBUG ("writing \"ShortTermRefPicSetParameter\"");

  if (stRPS->inter_ref_pic_set_prediction_flag) {
    /* TODO */
    GST_WARNING ("inter_ref_pic_set_prediction_flag mode not supported");
    goto error;
  }

  if (stRpsIdx != 0)
    WRITE_BITS (bw, stRPS->inter_ref_pic_set_prediction_flag, 1);

  if (stRPS->NumNegativePics + stRPS->NumPositivePics != stRPS->NumDeltaPocs)
    goto error;

  /* 7-49 */
  WRITE_UE_MAX (bw, stRPS->NumNegativePics,
      sps->max_dec_pic_buffering_minus1[sps->max_sub_layers_minus1]);
  /* 7-50 */
  WRITE_UE_MAX (bw, stRPS->NumPositivePics,
      (sps->max_dec_pic_buffering_minus1[sps->max_sub_layers_minus1] -
          stRPS->NumNegativePics));

  prev = 0;
  for (i = 0; i < stRPS->NumNegativePics; i++) {
    WRITE_UE_MAX (bw, prev - stRPS->DeltaPocS0[i] - 1, 32767);
    prev = stRPS->DeltaPocS0[i];
    /* 7-51 */
    WRITE_BITS (bw, stRPS->UsedByCurrPicS0[i], 1);
  }

  prev = 0;
  for (i = 0; i < stRPS->NumPositivePics; i++) {
    WRITE_UE_MAX (bw, stRPS->DeltaPocS1[i] - prev - 1, 32767);
    prev = stRPS->DeltaPocS1[i];
    /* 7-52 */
    WRITE_BITS (bw, stRPS->UsedByCurrPicS1[i], 1);
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write \"ShortTermRefPicSet Parameters\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h265_bit_writer_vui_parameters (const GstH265SPS * sps,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  const GstH265VUIParams *vui = &sps->vui_params;

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

  WRITE_BITS (bw, vui->neutral_chroma_indication_flag, 1);
  WRITE_BITS (bw, vui->field_seq_flag, 1);
  WRITE_BITS (bw, vui->frame_field_info_present_flag, 1);

  WRITE_BITS (bw, vui->default_display_window_flag, 1);
  if (vui->default_display_window_flag) {
    WRITE_UE (bw, vui->def_disp_win_left_offset);
    WRITE_UE (bw, vui->def_disp_win_right_offset);
    WRITE_UE (bw, vui->def_disp_win_top_offset);
    WRITE_UE (bw, vui->def_disp_win_bottom_offset);
  }

  WRITE_BITS (bw, vui->timing_info_present_flag, 1);
  if (vui->timing_info_present_flag) {
    if (vui->num_units_in_tick == 0)
      GST_WARNING ("num_units_in_tick = 0 (incompliant to H.265 E.2.1).");
    WRITE_BITS (bw, vui->num_units_in_tick, 32);

    if (vui->time_scale == 0)
      GST_WARNING ("time_scale = 0 (incompliant to H.265 E.2.1).");
    WRITE_BITS (bw, vui->time_scale, 32);

    WRITE_BITS (bw, vui->poc_proportional_to_timing_flag, 1);
    if (vui->poc_proportional_to_timing_flag)
      WRITE_UE_MAX (bw, vui->num_ticks_poc_diff_one_minus1, G_MAXUINT32 - 1);

    WRITE_BITS (bw, vui->hrd_parameters_present_flag, 1);
    if (vui->hrd_parameters_present_flag)
      if (!_h265_bit_writer_hrd_parameters (&vui->hrd_params, 1,
              sps->max_sub_layers_minus1, bw, &have_space))
        goto error;
  }

  WRITE_BITS (bw, vui->bitstream_restriction_flag, 1);
  if (vui->bitstream_restriction_flag) {
    WRITE_BITS (bw, vui->tiles_fixed_structure_flag, 1);
    WRITE_BITS (bw, vui->motion_vectors_over_pic_boundaries_flag, 1);
    WRITE_BITS (bw, vui->restricted_ref_pic_lists_flag, 1);
    WRITE_UE_MAX (bw, vui->min_spatial_segmentation_idc, 4096);
    WRITE_UE_MAX (bw, vui->max_bytes_per_pic_denom, 16);
    WRITE_UE_MAX (bw, vui->max_bits_per_min_cu_denom, 16);
    WRITE_UE_MAX (bw, vui->log2_max_mv_length_horizontal, 16);
    WRITE_UE_MAX (bw, vui->log2_max_mv_length_vertical, 15);
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write \"VUI Parameters\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h265_bit_writer_sps (const GstH265SPS * sps,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  guint i;

  GST_DEBUG ("writing SPS");

  WRITE_BITS (bw, sps->vps->id, 4);

  WRITE_BITS (bw, sps->max_sub_layers_minus1, 3);
  WRITE_BITS (bw, sps->temporal_id_nesting_flag, 1);

  if (!_h265_bit_writer_profile_tier_level (&sps->profile_tier_level,
          sps->max_sub_layers_minus1, bw, &have_space))
    goto error;

  WRITE_UE_MAX (bw, sps->id, GST_H265_MAX_SPS_COUNT - 1);

  WRITE_UE_MAX (bw, sps->chroma_format_idc, 3);
  if (sps->chroma_format_idc == 3)
    WRITE_BITS (bw, sps->separate_colour_plane_flag, 1);

  if (sps->pic_width_in_luma_samples < 1)
    goto error;
  WRITE_UE_MAX (bw, sps->pic_width_in_luma_samples, 16888);

  if (sps->pic_height_in_luma_samples < 1)
    goto error;
  WRITE_UE_MAX (bw, sps->pic_height_in_luma_samples, 16888);

  WRITE_BITS (bw, sps->conformance_window_flag, 1);
  if (sps->conformance_window_flag) {
    WRITE_UE (bw, sps->conf_win_left_offset);
    WRITE_UE (bw, sps->conf_win_right_offset);
    WRITE_UE (bw, sps->conf_win_top_offset);
    WRITE_UE (bw, sps->conf_win_bottom_offset);
  }

  WRITE_UE_MAX (bw, sps->bit_depth_luma_minus8, 6);
  WRITE_UE_MAX (bw, sps->bit_depth_chroma_minus8, 6);
  WRITE_UE_MAX (bw, sps->log2_max_pic_order_cnt_lsb_minus4, 12);

  WRITE_BITS (bw, sps->sub_layer_ordering_info_present_flag, 1);
  for (i = (sps->sub_layer_ordering_info_present_flag ? 0 :
          sps->max_sub_layers_minus1); i <= sps->max_sub_layers_minus1; i++) {
    WRITE_UE_MAX (bw, sps->max_dec_pic_buffering_minus1[i], 16);
    WRITE_UE_MAX (bw, sps->max_num_reorder_pics[i],
        sps->max_dec_pic_buffering_minus1[i]);
    WRITE_UE (bw, sps->max_latency_increase_plus1[i]);
  }

  /* The limits are calculted based on the profile_tier_level constraint
   * in Annex-A: CtbLog2SizeY = 4 to 6 */
  WRITE_UE_MAX (bw, sps->log2_min_luma_coding_block_size_minus3, 3);
  WRITE_UE_MAX (bw, sps->log2_diff_max_min_luma_coding_block_size, 6);
  WRITE_UE_MAX (bw, sps->log2_min_transform_block_size_minus2, 3);
  WRITE_UE_MAX (bw, sps->log2_diff_max_min_transform_block_size, 3);
  WRITE_UE_MAX (bw, sps->max_transform_hierarchy_depth_inter, 4);
  WRITE_UE_MAX (bw, sps->max_transform_hierarchy_depth_intra, 4);

  WRITE_BITS (bw, sps->scaling_list_enabled_flag, 1);
  if (sps->scaling_list_enabled_flag) {
    WRITE_BITS (bw, sps->scaling_list_data_present_flag, 1);

    if (sps->scaling_list_data_present_flag)
      if (!_h265_bit_writer_scaling_lists (&sps->scaling_list, bw, &have_space))
        goto error;
  }

  WRITE_BITS (bw, sps->amp_enabled_flag, 1);
  WRITE_BITS (bw, sps->sample_adaptive_offset_enabled_flag, 1);
  WRITE_BITS (bw, sps->pcm_enabled_flag, 1);

  if (sps->pcm_enabled_flag) {
    WRITE_BITS (bw, sps->pcm_sample_bit_depth_luma_minus1, 4);
    WRITE_BITS (bw, sps->pcm_sample_bit_depth_chroma_minus1, 4);
    WRITE_UE_MAX (bw, sps->log2_min_pcm_luma_coding_block_size_minus3, 2);
    WRITE_UE_MAX (bw, sps->log2_diff_max_min_pcm_luma_coding_block_size, 2);
    WRITE_BITS (bw, sps->pcm_loop_filter_disabled_flag, 1);
  }

  WRITE_UE_MAX (bw, sps->num_short_term_ref_pic_sets, 64);
  for (i = 0; i < sps->num_short_term_ref_pic_sets; i++) {
    if (!_h265_bit_writer_short_term_ref_pic_set
        (&sps->short_term_ref_pic_set[i], i, sps, bw, &have_space))
      goto error;
  }

  WRITE_BITS (bw, sps->long_term_ref_pics_present_flag, 1);
  if (sps->long_term_ref_pics_present_flag) {
    WRITE_UE_MAX (bw, sps->num_long_term_ref_pics_sps, 32);
    for (i = 0; i < sps->num_long_term_ref_pics_sps; i++) {
      WRITE_BITS (bw, sps->lt_ref_pic_poc_lsb_sps[i],
          sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
      WRITE_BITS (bw, sps->used_by_curr_pic_lt_sps_flag[i], 1);
    }
  }

  WRITE_BITS (bw, sps->temporal_mvp_enabled_flag, 1);
  WRITE_BITS (bw, sps->strong_intra_smoothing_enabled_flag, 1);
  WRITE_BITS (bw, sps->vui_parameters_present_flag, 1);

  if (sps->vui_parameters_present_flag) {
    if (!_h265_bit_writer_vui_parameters (sps, bw, &have_space))
      goto error;
  }

  WRITE_BITS (bw, sps->sps_extension_flag, 1);

  if (sps->sps_extension_flag) {
    WRITE_BITS (bw, sps->sps_range_extension_flag, 1);
    WRITE_BITS (bw, sps->sps_multilayer_extension_flag, 1);
    WRITE_BITS (bw, sps->sps_3d_extension_flag, 1);
    WRITE_BITS (bw, sps->sps_scc_extension_flag, 1);
    WRITE_BITS (bw, sps->sps_extension_4bits, 4);
  }

  if (sps->sps_range_extension_flag) {
    WRITE_BITS (bw,
        sps->sps_extension_params.transform_skip_rotation_enabled_flag, 1);
    WRITE_BITS (bw,
        sps->sps_extension_params.transform_skip_context_enabled_flag, 1);
    WRITE_BITS (bw, sps->sps_extension_params.implicit_rdpcm_enabled_flag, 1);
    WRITE_BITS (bw, sps->sps_extension_params.explicit_rdpcm_enabled_flag, 1);
    WRITE_BITS (bw,
        sps->sps_extension_params.extended_precision_processing_flag, 1);
    WRITE_BITS (bw, sps->sps_extension_params.intra_smoothing_disabled_flag, 1);
    WRITE_BITS (bw,
        sps->sps_extension_params.high_precision_offsets_enabled_flag, 1);
    WRITE_BITS (bw,
        sps->sps_extension_params.persistent_rice_adaptation_enabled_flag, 1);
    WRITE_BITS (bw,
        sps->sps_extension_params.cabac_bypass_alignment_enabled_flag, 1);
  }

  if (sps->sps_multilayer_extension_flag) {
    GST_WARNING ("do not support multilayer extension");
    goto error;
  }
  if (sps->sps_3d_extension_flag) {
    GST_WARNING ("do not support 3d extension");
    goto error;
  }

  if (sps->sps_scc_extension_flag) {
    const GstH265SPSSccExtensionParams *scc_params =
        &sps->sps_scc_extension_params;

    WRITE_BITS (bw, scc_params->sps_curr_pic_ref_enabled_flag, 1);
    WRITE_BITS (bw, scc_params->palette_mode_enabled_flag, 1);
    if (scc_params->palette_mode_enabled_flag) {
      WRITE_UE_MAX (bw, scc_params->palette_max_size, 64);
      WRITE_UE_MAX (bw, scc_params->delta_palette_max_predictor_size,
          128 - scc_params->palette_max_size);

      WRITE_BITS (bw,
          scc_params->sps_palette_predictor_initializers_present_flag, 1);
      if (scc_params->sps_palette_predictor_initializers_present_flag) {
        guint comp;
        WRITE_UE_MAX (bw,
            scc_params->sps_num_palette_predictor_initializer_minus1,
            scc_params->palette_max_size +
            scc_params->delta_palette_max_predictor_size - 1);

        for (comp = 0; comp < (sps->chroma_format_idc == 0 ? 1 : 3); comp++) {
          guint num_bits;
          guint num =
              scc_params->sps_num_palette_predictor_initializer_minus1 + 1;

          num_bits = (comp == 0 ? sps->bit_depth_luma_minus8 + 8 :
              sps->bit_depth_chroma_minus8 + 8);
          for (i = 0; i < num; i++)
            WRITE_BITS (bw,
                scc_params->sps_palette_predictor_initializer[comp][i],
                num_bits);
        }
      }
    }

    WRITE_BITS (bw, scc_params->motion_vector_resolution_control_idc, 2);
    WRITE_BITS (bw, scc_params->intra_boundary_filtering_disabled_flag, 1);
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write SPS");

  *space = have_space;
  return FALSE;
}

/**
 * gst_h265_bit_writer_sps:
 * @sps: the sps of #GstH265SPS to write
 * @start_code: whether adding the nal start code
 * @data: (out): the bit stream generated by the sps
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according h265 bit stream by providing the sps.
 *
 * Returns: a #GstH265BitWriterResult
 *
 * Since: 1.22
 **/
GstH265BitWriterResult
gst_h265_bit_writer_sps (const GstH265SPS * sps, gboolean start_code,
    guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;

  g_return_val_if_fail (sps != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (sps->vps != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_H265_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  if (start_code)
    WRITE_BITS (&bw, 0x00000001, 32);

  /* NAL unit header */
  /* forbidden_zero_bit */
  WRITE_BITS (&bw, 0, 1);
  /* nal_unit_type */
  WRITE_BITS (&bw, GST_H265_NAL_SPS, 6);
  /* nuh_layer_id, only support 0 now */
  WRITE_BITS (&bw, 0, 6);
  /* nuh_temporal_id_plus1, only support 1 now */
  WRITE_BITS (&bw, 1, 3);

  if (!_h265_bit_writer_sps (sps, &bw, &have_space))
    goto error;

  /* Add trailings. */
  WRITE_BITS (&bw, 1, 1);
  if (!gst_bit_writer_align_bytes (&bw, 0)) {
    have_space = FALSE;
    goto error;
  }

  *size = gst_bit_writer_get_size (&bw) / 8;
  gst_bit_writer_reset (&bw);

  return GST_H265_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;

  if (!have_space)
    return GST_H265_BIT_WRITER_NO_MORE_SPACE;
  return GST_H265_BIT_WRITER_INVALID_DATA;
}

static gboolean
_h265_bit_writer_pps (const GstH265PPS * pps, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing PPS");

  WRITE_UE_MAX (bw, pps->id, GST_H265_MAX_PPS_COUNT - 1);
  WRITE_UE_MAX (bw, pps->sps->id, GST_H265_MAX_SPS_COUNT - 1);

  WRITE_BITS (bw, pps->dependent_slice_segments_enabled_flag, 1);
  WRITE_BITS (bw, pps->output_flag_present_flag, 1);
  WRITE_BITS (bw, pps->num_extra_slice_header_bits, 3);
  WRITE_BITS (bw, pps->sign_data_hiding_enabled_flag, 1);
  WRITE_BITS (bw, pps->cabac_init_present_flag, 1);

  WRITE_UE_MAX (bw, pps->num_ref_idx_l0_default_active_minus1, 14);
  WRITE_UE_MAX (bw, pps->num_ref_idx_l1_default_active_minus1, 14);
  WRITE_SE_RANGE (bw, pps->init_qp_minus26,
      -(26 + 6 * pps->sps->bit_depth_luma_minus8), 25);

  WRITE_BITS (bw, pps->constrained_intra_pred_flag, 1);
  WRITE_BITS (bw, pps->transform_skip_enabled_flag, 1);

  WRITE_BITS (bw, pps->cu_qp_delta_enabled_flag, 1);
  if (pps->cu_qp_delta_enabled_flag)
    WRITE_UE_MAX (bw, pps->diff_cu_qp_delta_depth,
        pps->sps->log2_diff_max_min_luma_coding_block_size);

  WRITE_SE_RANGE (bw, pps->cb_qp_offset, -12, 12);
  WRITE_SE_RANGE (bw, pps->cr_qp_offset, -12, 12);

  WRITE_BITS (bw, pps->slice_chroma_qp_offsets_present_flag, 1);
  WRITE_BITS (bw, pps->weighted_pred_flag, 1);
  WRITE_BITS (bw, pps->weighted_bipred_flag, 1);
  WRITE_BITS (bw, pps->transquant_bypass_enabled_flag, 1);
  WRITE_BITS (bw, pps->tiles_enabled_flag, 1);
  WRITE_BITS (bw, pps->entropy_coding_sync_enabled_flag, 1);

  if (pps->tiles_enabled_flag) {
    if (pps->num_tile_columns_minus1 + 1 >
        G_N_ELEMENTS (pps->column_width_minus1)) {
      GST_WARNING ("Invalid \"num_tile_columns_minus1\" %d",
          pps->num_tile_columns_minus1);
      goto error;
    }

    if (pps->num_tile_rows_minus1 + 1 > G_N_ELEMENTS (pps->row_height_minus1)) {
      GST_WARNING ("Invalid \"num_tile_rows_minus1\" %d",
          pps->num_tile_rows_minus1);
      goto error;
    }

    WRITE_UE_MAX (bw, pps->num_tile_columns_minus1, pps->PicWidthInCtbsY - 1);
    WRITE_UE_MAX (bw, pps->num_tile_rows_minus1, pps->PicHeightInCtbsY - 1);

    WRITE_BITS (bw, pps->uniform_spacing_flag, 1);

    /* 6.5.1, 6-4, 6-5, 7.4.3.3.1 */
    if (!pps->uniform_spacing_flag) {
      guint i;

      for (i = 0; i < pps->num_tile_columns_minus1; i++)
        WRITE_UE (bw, pps->column_width_minus1[i]);

      for (i = 0; i < pps->num_tile_rows_minus1; i++)
        WRITE_UE (bw, pps->row_height_minus1[i]);
    }
    WRITE_BITS (bw, pps->loop_filter_across_tiles_enabled_flag, 1);
  }

  WRITE_BITS (bw, pps->loop_filter_across_slices_enabled_flag, 1);

  WRITE_BITS (bw, pps->deblocking_filter_control_present_flag, 1);
  if (pps->deblocking_filter_control_present_flag) {
    WRITE_BITS (bw, pps->deblocking_filter_override_enabled_flag, 1);

    WRITE_BITS (bw, pps->deblocking_filter_disabled_flag, 1);
    if (!pps->deblocking_filter_disabled_flag) {
      WRITE_SE_RANGE (bw, pps->beta_offset_div2, -6, 6);
      WRITE_SE_RANGE (bw, pps->tc_offset_div2, -6, +6);
    }
  }

  WRITE_BITS (bw, pps->scaling_list_data_present_flag, 1);
  if (pps->scaling_list_data_present_flag)
    if (!_h265_bit_writer_scaling_lists (&pps->scaling_list, bw, &have_space))
      goto error;

  WRITE_BITS (bw, pps->lists_modification_present_flag, 1);
  WRITE_UE_MAX (bw, pps->log2_parallel_merge_level_minus2, 4);

  /* TODO: slice_segment_header */
  if (pps->slice_segment_header_extension_present_flag) {
    GST_WARNING
        ("slice_segment_header_extension_present_flag is not supported");
    goto error;
  }
  WRITE_BITS (bw, pps->slice_segment_header_extension_present_flag, 1);

  WRITE_BITS (bw, pps->pps_extension_flag, 1);

  if (pps->pps_extension_flag) {
    WRITE_BITS (bw, pps->pps_range_extension_flag, 1);
    WRITE_BITS (bw, pps->pps_multilayer_extension_flag, 1);
    WRITE_BITS (bw, pps->pps_3d_extension_flag, 1);
    WRITE_BITS (bw, pps->pps_scc_extension_flag, 1);
    WRITE_BITS (bw, pps->pps_extension_4bits, 4);
  }

  if (pps->pps_range_extension_flag) {
    guint i;
    guint32 MaxBitDepthY, MaxBitDepthC;

    if (pps->transform_skip_enabled_flag)
      WRITE_UE (bw,
          pps->pps_extension_params.log2_max_transform_skip_block_size_minus2);

    WRITE_BITS (bw,
        pps->pps_extension_params.cross_component_prediction_enabled_flag, 1);
    WRITE_BITS (bw,
        pps->pps_extension_params.chroma_qp_offset_list_enabled_flag, 1);

    if (pps->pps_extension_params.chroma_qp_offset_list_enabled_flag) {
      WRITE_UE_MAX (bw,
          pps->pps_extension_params.diff_cu_chroma_qp_offset_depth,
          pps->sps->log2_diff_max_min_luma_coding_block_size);

      WRITE_UE_MAX (bw,
          pps->pps_extension_params.chroma_qp_offset_list_len_minus1, 5);
      for (i = 0;
          i <= pps->pps_extension_params.chroma_qp_offset_list_len_minus1;
          i++) {
        WRITE_SE_RANGE (bw, pps->pps_extension_params.cb_qp_offset_list[i],
            -12, 12);
        WRITE_SE_RANGE (bw, pps->pps_extension_params.cr_qp_offset_list[i],
            -12, 12);
      }
    }

    MaxBitDepthY = pps->sps->bit_depth_luma_minus8 > 2 ?
        pps->sps->bit_depth_luma_minus8 - 2 : 0;
    MaxBitDepthC = pps->sps->bit_depth_chroma_minus8 > 2 ?
        pps->sps->bit_depth_chroma_minus8 - 2 : 0;
    WRITE_UE_MAX (bw, pps->pps_extension_params.log2_sao_offset_scale_luma,
        MaxBitDepthY);
    WRITE_UE_MAX (bw, pps->pps_extension_params.log2_sao_offset_scale_chroma,
        MaxBitDepthC);
  }

  if (pps->pps_multilayer_extension_flag) {
    GST_WARNING ("do not support multilayer extension");
    goto error;
  }

  if (pps->pps_3d_extension_flag) {
    GST_WARNING ("do not support 3d extension");
    goto error;
  }

  if (pps->pps_scc_extension_flag) {
    const GstH265PPSSccExtensionParams *pps_scc =
        &pps->pps_scc_extension_params;

    WRITE_BITS (bw, pps_scc->pps_curr_pic_ref_enabled_flag, 1);
    WRITE_BITS (bw,
        pps_scc->residual_adaptive_colour_transform_enabled_flag, 1);
    if (pps_scc->residual_adaptive_colour_transform_enabled_flag) {
      WRITE_BITS (bw, pps_scc->pps_slice_act_qp_offsets_present_flag, 1);
      WRITE_SE_RANGE (bw, (gint8) pps_scc->pps_act_y_qp_offset_plus5, -7, 17);
      WRITE_SE_RANGE (bw, (gint8) pps_scc->pps_act_cb_qp_offset_plus5, -7, 17);
      WRITE_SE_RANGE (bw, (gint8) pps_scc->pps_act_cr_qp_offset_plus3, -9, 15);
    }

    WRITE_BITS (bw,
        pps_scc->pps_palette_predictor_initializers_present_flag, 1);
    if (pps_scc->pps_palette_predictor_initializers_present_flag) {
      guint i;

      WRITE_UE_MAX (bw,
          pps_scc->pps_num_palette_predictor_initializer,
          pps->sps->sps_scc_extension_params.palette_max_size +
          pps->sps->sps_scc_extension_params.delta_palette_max_predictor_size);
      if (pps_scc->pps_num_palette_predictor_initializer > 0) {
        guint comp;

        WRITE_BITS (bw, pps_scc->monochrome_palette_flag, 1);
        /* It is a requirement of bitstream conformance that the value of
           luma_bit_depth_entry_minus8 shall be equal to the value of
           bit_depth_luma_minus8 */
        WRITE_UE_MAX (bw, pps_scc->luma_bit_depth_entry_minus8,
            pps->sps->bit_depth_luma_minus8);
        if (!pps_scc->monochrome_palette_flag) {
          /* It is a requirement of bitstream conformance that the value
             of chroma_bit_depth_entry_minus8 shall be equal to the value
             of bit_depth_chroma_minus8. */
          WRITE_UE_MAX (bw, pps_scc->chroma_bit_depth_entry_minus8,
              pps->sps->bit_depth_chroma_minus8);
        }

        for (comp = 0; comp < (pps_scc->monochrome_palette_flag ? 1 : 3);
            comp++) {
          guint num_bits;
          guint num = pps_scc->pps_num_palette_predictor_initializer;

          num_bits = (comp == 0 ?
              pps_scc->luma_bit_depth_entry_minus8 + 8 :
              pps_scc->chroma_bit_depth_entry_minus8 + 8);
          for (i = 0; i < num; i++)
            WRITE_BITS (bw,
                pps_scc->pps_palette_predictor_initializer[comp][i], num_bits);
        }
      }
    }
  }

  return TRUE;

error:
  GST_WARNING ("failed to write PPS");
  return FALSE;
}

/**
 * gst_h265_bit_writer_pps:
 * @pps: the pps of #GstH265PPS to write
 * @start_code: whether adding the nal start code
 * @data: (out): the bit stream generated by the pps
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according h265 bit stream by providing the pps.
 *
 * Returns: a #GstH265BitWriterResult
 *
 * Since: 1.22
 **/
GstH265BitWriterResult
gst_h265_bit_writer_pps (const GstH265PPS * pps, gboolean start_code,
    guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;

  g_return_val_if_fail (pps != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (pps->sps != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_H265_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  if (start_code)
    WRITE_BITS (&bw, 0x00000001, 32);

  /* NAL unit header */
  /* forbidden_zero_bit */
  WRITE_BITS (&bw, 0, 1);
  /* nal_unit_type */
  WRITE_BITS (&bw, GST_H265_NAL_PPS, 6);
  /* nuh_layer_id, only support 0 now */
  WRITE_BITS (&bw, 0, 6);
  /* nuh_temporal_id_plus1, only support 1 now */
  WRITE_BITS (&bw, 1, 3);

  if (!_h265_bit_writer_pps (pps, &bw, &have_space))
    goto error;

  /* Add trailings. */
  WRITE_BITS (&bw, 1, 1);
  if (!gst_bit_writer_align_bytes (&bw, 0)) {
    have_space = FALSE;
    goto error;
  }

  *size = gst_bit_writer_get_size (&bw) / 8;
  gst_bit_writer_reset (&bw);

  return GST_H265_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;

  if (!have_space)
    return GST_H265_BIT_WRITER_NO_MORE_SPACE;
  return GST_H265_BIT_WRITER_INVALID_DATA;
}

static gboolean
_h265_slice_bit_writer_ref_pic_list_modification (const GstH265SliceHdr *
    slice, gint NumPocTotalCurr, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  guint i;
  const GstH265RefPicListModification *rpl_mod =
      &slice->ref_pic_list_modification;
  const guint n = gst_util_ceil_log2 (NumPocTotalCurr);

  WRITE_BITS (bw, rpl_mod->ref_pic_list_modification_flag_l0, 1);

  if (rpl_mod->ref_pic_list_modification_flag_l0) {
    for (i = 0; i <= slice->num_ref_idx_l0_active_minus1; i++) {
      WRITE_BITS (bw, rpl_mod->list_entry_l0[i], n);
    }
  }

  if (GST_H265_IS_B_SLICE (slice)) {
    WRITE_BITS (bw, rpl_mod->ref_pic_list_modification_flag_l1, 1);

    if (rpl_mod->ref_pic_list_modification_flag_l1)
      for (i = 0; i <= slice->num_ref_idx_l1_active_minus1; i++) {
        WRITE_BITS (bw, rpl_mod->list_entry_l1[i], n);
      }
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write \"Reference picture list modifications\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h265_slice_bit_writer_pred_weight_table (const GstH265SliceHdr * slice,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  const GstH265PredWeightTable *p;
  const GstH265PPS *pps = slice->pps;
  const GstH265SPS *sps = pps->sps;
  gint i, j;

  GST_DEBUG ("writing \"Prediction weight table\"");

  p = &slice->pred_weight_table;

  WRITE_UE_MAX (bw, p->luma_log2_weight_denom, 7);

  if (sps->chroma_format_idc != 0) {
    WRITE_SE_RANGE (bw, p->delta_chroma_log2_weight_denom,
        (0 - p->luma_log2_weight_denom), (7 - p->luma_log2_weight_denom));
  }

  for (i = 0; i <= slice->num_ref_idx_l0_active_minus1; i++)
    WRITE_BITS (bw, p->luma_weight_l0_flag[i], 1);

  if (sps->chroma_format_idc != 0)
    for (i = 0; i <= slice->num_ref_idx_l0_active_minus1; i++)
      WRITE_BITS (bw, p->chroma_weight_l0_flag[i], 1);

  for (i = 0; i <= slice->num_ref_idx_l0_active_minus1; i++) {
    if (p->luma_weight_l0_flag[i]) {
      WRITE_SE_RANGE (bw, p->delta_luma_weight_l0[i], -128, 127);
      WRITE_SE_RANGE (bw, p->luma_offset_l0[i], -128, 127);
    }
    if (p->chroma_weight_l0_flag[i])
      for (j = 0; j < 2; j++) {
        WRITE_SE_RANGE (bw, p->delta_chroma_weight_l0[i][j], -128, 127);
        WRITE_SE_RANGE (bw, p->delta_chroma_offset_l0[i][j], -512, 511);
      }
  }

  if (GST_H265_IS_B_SLICE (slice)) {
    for (i = 0; i <= slice->num_ref_idx_l1_active_minus1; i++)
      WRITE_BITS (bw, p->luma_weight_l1_flag[i], 1);

    if (sps->chroma_format_idc != 0)
      for (i = 0; i <= slice->num_ref_idx_l1_active_minus1; i++)
        WRITE_BITS (bw, p->chroma_weight_l1_flag[i], 1);

    for (i = 0; i <= slice->num_ref_idx_l1_active_minus1; i++) {
      if (p->luma_weight_l1_flag[i]) {
        WRITE_SE_RANGE (bw, p->delta_luma_weight_l1[i], -128, 127);
        WRITE_SE_RANGE (bw, p->luma_offset_l1[i], -128, 127);
      }
      if (p->chroma_weight_l1_flag[i])
        for (j = 0; j < 2; j++) {
          WRITE_SE_RANGE (bw, p->delta_chroma_weight_l1[i][j], -128, 127);
          WRITE_SE_RANGE (bw, p->delta_chroma_offset_l1[i][j], -512, 511);
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
_h265_bit_writer_slice_header (const GstH265SliceHdr * slice,
    guint32 nal_type, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  guint i;
  const GstH265SPS *sps = slice->pps->sps;
  const GstH265PPSSccExtensionParams *pps_scc_extension_params =
      &slice->pps->pps_scc_extension_params;
  const GstH265PPSExtensionParams *pps_extension_params =
      &slice->pps->pps_extension_params;

  GST_DEBUG ("writing slice header");

  WRITE_BITS (bw, slice->first_slice_segment_in_pic_flag, 1);

  if (GST_H265_IS_NAL_TYPE_IRAP (nal_type))
    WRITE_BITS (bw, slice->no_output_of_prior_pics_flag, 1);

  WRITE_UE_MAX (bw, slice->pps->id, GST_H265_MAX_PPS_COUNT - 1);

  if (!slice->first_slice_segment_in_pic_flag) {
    guint32 PicSizeInCtbsY;
    guint32 PicWidthInCtbsY;
    guint32 PicHeightInCtbsY;
    guint32 CtbSizeY, MinCbLog2SizeY, CtbLog2SizeY;
    guint n;

    /* We can not directly use slice->pps->PicWidthInCtbsY/PicHeightInCtbsY,
       they are calculated value when parsing but may not have value here. */
    MinCbLog2SizeY = sps->log2_min_luma_coding_block_size_minus3 + 3;
    CtbLog2SizeY =
        MinCbLog2SizeY + sps->log2_diff_max_min_luma_coding_block_size;
    CtbSizeY = 1 << CtbLog2SizeY;
    PicHeightInCtbsY =
        ceil ((gdouble) sps->pic_height_in_luma_samples / (gdouble) CtbSizeY);
    PicWidthInCtbsY =
        ceil ((gdouble) sps->pic_width_in_luma_samples / (gdouble) CtbSizeY);
    PicSizeInCtbsY = PicWidthInCtbsY * PicHeightInCtbsY;

    n = gst_util_ceil_log2 (PicSizeInCtbsY);

    if (slice->pps->dependent_slice_segments_enabled_flag)
      WRITE_BITS (bw, slice->dependent_slice_segment_flag, 1);
    /* sice_segment_address parsing */
    WRITE_BITS (bw, slice->segment_address, n);
  }

  if (!slice->dependent_slice_segment_flag) {
    for (i = 0; i < slice->pps->num_extra_slice_header_bits; i++) {
      /* slice_reserved_flag */
      WRITE_BITS (bw, 0, 1);
    }

    WRITE_UE_MAX (bw, slice->type, 63);

    if (slice->pps->output_flag_present_flag)
      WRITE_BITS (bw, slice->pic_output_flag, 1);

    if (sps->separate_colour_plane_flag == 1)
      WRITE_BITS (bw, slice->colour_plane_id, 2);

    if (!GST_H265_IS_NAL_TYPE_IDR (nal_type)) {
      WRITE_BITS (bw, slice->pic_order_cnt_lsb,
          (sps->log2_max_pic_order_cnt_lsb_minus4 + 4));

      WRITE_BITS (bw, slice->short_term_ref_pic_set_sps_flag, 1);
      if (!slice->short_term_ref_pic_set_sps_flag) {
        if (!_h265_bit_writer_short_term_ref_pic_set
            (&slice->short_term_ref_pic_sets, sps->num_short_term_ref_pic_sets,
                slice->pps->sps, bw, &have_space))
          goto error;
      } else if (sps->num_short_term_ref_pic_sets > 1) {
        const guint n = gst_util_ceil_log2 (sps->num_short_term_ref_pic_sets);

        if (slice->short_term_ref_pic_set_idx >
            sps->num_short_term_ref_pic_sets - 1)
          goto error;

        WRITE_BITS (bw, slice->short_term_ref_pic_set_idx, n);
      }

      if (sps->long_term_ref_pics_present_flag) {
        guint32 limit;

        if (sps->num_long_term_ref_pics_sps > 0)
          WRITE_UE_MAX (bw, slice->num_long_term_sps,
              sps->num_long_term_ref_pics_sps);

        WRITE_UE_MAX (bw, slice->num_long_term_pics, 16);
        limit = slice->num_long_term_sps + slice->num_long_term_pics;
        for (i = 0; i < limit; i++) {
          if (i < slice->num_long_term_sps) {
            if (sps->num_long_term_ref_pics_sps > 1) {
              const guint n =
                  gst_util_ceil_log2 (sps->num_long_term_ref_pics_sps);
              WRITE_BITS (bw, slice->lt_idx_sps[i], n);
            }
          } else {
            WRITE_BITS (bw, slice->poc_lsb_lt[i],
                (sps->log2_max_pic_order_cnt_lsb_minus4 + 4));
            WRITE_BITS (bw, slice->used_by_curr_pic_lt_flag[i], 1);
          }

          WRITE_BITS (bw, slice->delta_poc_msb_present_flag[i], 1);
          if (slice->delta_poc_msb_present_flag[i])
            WRITE_UE (bw, slice->delta_poc_msb_cycle_lt[i]);
        }
      }

      if (sps->temporal_mvp_enabled_flag)
        WRITE_BITS (bw, slice->temporal_mvp_enabled_flag, 1);
    }

    if (sps->sample_adaptive_offset_enabled_flag) {
      gboolean ChromaArrayType =
          sps->separate_colour_plane_flag == 0 ? sps->chroma_format_idc : 0;

      WRITE_BITS (bw, slice->sao_luma_flag, 1);
      if (ChromaArrayType)
        WRITE_BITS (bw, slice->sao_chroma_flag, 1);
    }

    if (GST_H265_IS_B_SLICE (slice) || GST_H265_IS_P_SLICE (slice)) {
      WRITE_BITS (bw, slice->num_ref_idx_active_override_flag, 1);

      if (slice->num_ref_idx_active_override_flag) {
        WRITE_UE_MAX (bw, slice->num_ref_idx_l0_active_minus1, 14);
        if (GST_H265_IS_B_SLICE (slice))
          WRITE_UE_MAX (bw, slice->num_ref_idx_l1_active_minus1, 14);
      }

      if (slice->pps->lists_modification_present_flag
          && slice->NumPocTotalCurr > 1) {
        if (!_h265_slice_bit_writer_ref_pic_list_modification (slice,
                slice->NumPocTotalCurr, bw, &have_space))
          goto error;
      }

      if (GST_H265_IS_B_SLICE (slice))
        WRITE_BITS (bw, slice->mvd_l1_zero_flag, 1);

      if (slice->pps->cabac_init_present_flag)
        WRITE_BITS (bw, slice->cabac_init_flag, 1);

      if (slice->temporal_mvp_enabled_flag) {
        if (GST_H265_IS_B_SLICE (slice))
          WRITE_BITS (bw, slice->collocated_from_l0_flag, 1);

        if ((slice->collocated_from_l0_flag
                && slice->num_ref_idx_l0_active_minus1 > 0)
            || (!slice->collocated_from_l0_flag
                && slice->num_ref_idx_l1_active_minus1 > 0)) {
          if ((GST_H265_IS_P_SLICE (slice))
              || ((GST_H265_IS_B_SLICE (slice))
                  && (slice->collocated_from_l0_flag))) {
            WRITE_UE_MAX (bw, slice->collocated_ref_idx,
                slice->num_ref_idx_l0_active_minus1);
          } else if ((GST_H265_IS_B_SLICE (slice))
              && (!slice->collocated_from_l0_flag)) {
            WRITE_UE_MAX (bw, slice->collocated_ref_idx,
                slice->num_ref_idx_l1_active_minus1);
          }
        }
      }

      if ((slice->pps->weighted_pred_flag && GST_H265_IS_P_SLICE (slice)) ||
          (slice->pps->weighted_bipred_flag && GST_H265_IS_B_SLICE (slice)))
        if (!_h265_slice_bit_writer_pred_weight_table (slice, bw, &have_space))
          goto error;

      WRITE_UE_MAX (bw, slice->five_minus_max_num_merge_cand, 4);

      if (sps->sps_scc_extension_params.motion_vector_resolution_control_idc
          == 2)
        WRITE_BITS (bw, slice->use_integer_mv_flag, 1);
    }

    WRITE_SE_RANGE (bw, slice->qp_delta, -87, 77);
    if (slice->pps->slice_chroma_qp_offsets_present_flag) {
      WRITE_SE_RANGE (bw, slice->cb_qp_offset, -12, 12);
      WRITE_SE_RANGE (bw, slice->cr_qp_offset, -12, 12);
    }

    if (pps_scc_extension_params->pps_slice_act_qp_offsets_present_flag) {
      WRITE_SE_RANGE (bw, slice->slice_act_y_qp_offset, -12, 12);
      WRITE_SE_RANGE (bw, slice->slice_act_cb_qp_offset, -12, 12);
      WRITE_SE_RANGE (bw, slice->slice_act_cr_qp_offset, -12, 12);
    }

    if (pps_extension_params->chroma_qp_offset_list_enabled_flag)
      WRITE_BITS (bw, slice->cu_chroma_qp_offset_enabled_flag, 1);

    if (slice->pps->deblocking_filter_override_enabled_flag)
      WRITE_BITS (bw, slice->deblocking_filter_override_flag, 1);

    if (slice->deblocking_filter_override_flag) {
      WRITE_BITS (bw, slice->deblocking_filter_disabled_flag, 1);

      if (!slice->deblocking_filter_disabled_flag) {
        WRITE_SE_RANGE (bw, slice->beta_offset_div2, -6, 6);
        WRITE_SE_RANGE (bw, slice->tc_offset_div2, -6, 6);
      }
    }

    if (slice->pps->loop_filter_across_slices_enabled_flag &&
        (slice->sao_luma_flag || slice->sao_chroma_flag ||
            !slice->deblocking_filter_disabled_flag))
      WRITE_BITS (bw, slice->loop_filter_across_slices_enabled_flag, 1);
  }

  if (slice->pps->tiles_enabled_flag
      || slice->pps->entropy_coding_sync_enabled_flag) {
    guint32 offset_max;

    if (!slice->pps->tiles_enabled_flag
        && slice->pps->entropy_coding_sync_enabled_flag) {
      offset_max = slice->pps->PicHeightInCtbsY - 1;
    } else if (slice->pps->tiles_enabled_flag
        && !slice->pps->entropy_coding_sync_enabled_flag) {
      offset_max =
          (slice->pps->num_tile_columns_minus1 +
          1) * (slice->pps->num_tile_rows_minus1 + 1) - 1;
    } else {
      offset_max =
          (slice->pps->num_tile_columns_minus1 +
          1) * slice->pps->PicHeightInCtbsY - 1;
    }

    WRITE_UE_MAX (bw, slice->num_entry_point_offsets, offset_max);
    if (slice->num_entry_point_offsets > 0) {
      WRITE_UE_MAX (bw, slice->offset_len_minus1, 31);
      for (i = 0; i < slice->num_entry_point_offsets; i++)
        WRITE_BITS (bw, slice->entry_point_offset_minus1[i],
            (slice->offset_len_minus1 + 1));
    }
  }

  /* TODO */
  if (slice->pps->slice_segment_header_extension_present_flag) {
    GST_WARNING
        ("slice_segment_header_extension_present_flag is not supported");
    goto error;
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("error to write slice header");

  *space = have_space;
  return FALSE;
}

/**
 * gst_h265_bit_writer_slice_hdr:
 * @slice: the slice header of #GstH265SliceHdr to write
 * @start_code: whether adding the nal start code
 * @nal_type: the slice's nal type of #GstH265NalUnitType
 * @data: (out): the bit stream generated by the slice header
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according h265 bit stream by providing the slice header.
 *
 * Returns: a #GstH265BitWriterResult
 *
 * Since: 1.22
 **/
GstH265BitWriterResult
gst_h265_bit_writer_slice_hdr (const GstH265SliceHdr * slice,
    gboolean start_code, guint32 nal_type, guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;

  g_return_val_if_fail (slice != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (slice->pps != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (slice->pps->sps != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (nal_type <= GST_H265_NAL_SLICE_CRA_NUT,
      GST_H265_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  if (start_code)
    WRITE_BITS (&bw, 0x00000001, 32);

  /* NAL unit header */
  /* forbidden_zero_bit */
  WRITE_BITS (&bw, 0, 1);
  /* nal_unit_type */
  WRITE_BITS (&bw, nal_type, 6);
  /* nuh_layer_id, only support 0 now */
  WRITE_BITS (&bw, 0, 6);
  /* nuh_temporal_id_plus1, only support 1 now */
  WRITE_BITS (&bw, 1, 3);

  if (!_h265_bit_writer_slice_header (slice, nal_type, &bw, &have_space))
    goto error;

  /* Add trailings. */
  WRITE_BITS (&bw, 1, 1);
  if (!gst_bit_writer_align_bytes (&bw, 0)) {
    have_space = FALSE;
    goto error;
  }

  *size = gst_bit_writer_get_size (&bw) / 8;
  gst_bit_writer_reset (&bw);
  return GST_H265_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;

  if (!have_space)
    return GST_H265_BIT_WRITER_NO_MORE_SPACE;
  return GST_H265_BIT_WRITER_INVALID_DATA;
}


static gboolean
_h265_bit_writer_sei_registered_user_data (const GstH265RegisteredUserData *
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

error:
  GST_DEBUG ("Failed to write \"Registered user data\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h265_bit_writer_sei_time_code (const GstH265TimeCode * tc,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  gint i;

  GST_DEBUG ("Wrtiting \"Time code\"");

  WRITE_BITS (bw, tc->num_clock_ts, 2);

  for (i = 0; i < tc->num_clock_ts; i++) {
    WRITE_BITS (bw, tc->clock_timestamp_flag[i], 1);
    if (tc->clock_timestamp_flag[i]) {
      WRITE_BITS (bw, tc->units_field_based_flag[i], 1);
      WRITE_BITS (bw, tc->counting_type[i], 5);
      WRITE_BITS (bw, tc->full_timestamp_flag[i], 1);
      WRITE_BITS (bw, tc->discontinuity_flag[i], 1);
      WRITE_BITS (bw, tc->cnt_dropped_flag[i], 1);
      WRITE_BITS (bw, tc->n_frames[i], 9);

      if (tc->full_timestamp_flag[i]) {
        WRITE_BITS (bw, tc->seconds_value[i], 6);
        WRITE_BITS (bw, tc->minutes_value[i], 6);
        WRITE_BITS (bw, tc->hours_value[i], 5);
      } else {
        WRITE_BITS (bw, tc->seconds_flag[i], 1);
        if (tc->seconds_flag[i]) {
          WRITE_BITS (bw, tc->seconds_value[i], 6);
          WRITE_BITS (bw, tc->minutes_flag[i], 1);
          if (tc->minutes_flag[i]) {
            WRITE_BITS (bw, tc->minutes_value[i], 6);
            WRITE_BITS (bw, tc->hours_flag[i], 1);
            if (tc->hours_flag[i]) {
              WRITE_BITS (bw, tc->hours_value[i], 5);
            }
          }
        }
      }
    }

    WRITE_BITS (bw, tc->time_offset_length[i], 5);

    if (tc->time_offset_length[i] > 0)
      WRITE_BITS (bw, tc->time_offset_value[i], tc->time_offset_length[i]);
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("Failed to write \"Time code\"");

  *space = have_space;
  return FALSE;
}

static gboolean
_h265_bit_writer_sei_mastering_display_colour_volume (const
    GstH265MasteringDisplayColourVolume * mdcv, GstBitWriter * bw,
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
_h265_bit_writer_sei_content_light_level_info (const
    GstH265ContentLightLevel * cll, GstBitWriter * bw, gboolean * space)
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
_h265_bit_writer_sei_message (const GstH265SEIMessage * msg,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing SEI message");

  switch (msg->payloadType) {
    case GST_H265_SEI_REGISTERED_USER_DATA:
      if (!_h265_bit_writer_sei_registered_user_data
          (&msg->payload.registered_user_data, bw, &have_space))
        goto error;
      break;
    case GST_H265_SEI_TIME_CODE:
      if (!_h265_bit_writer_sei_time_code
          (&msg->payload.time_code, bw, &have_space))
        goto error;
      break;
    case GST_H265_SEI_MASTERING_DISPLAY_COLOUR_VOLUME:
      if (!_h265_bit_writer_sei_mastering_display_colour_volume
          (&msg->payload.mastering_display_colour_volume, bw, &have_space))
        goto error;
      break;
    case GST_H265_SEI_CONTENT_LIGHT_LEVEL:
      if (!_h265_bit_writer_sei_content_light_level_info
          (&msg->payload.content_light_level, bw, &have_space))
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
 * gst_h265_bit_writer_sei:
 * @sei_messages: An array of #GstH265SEIMessage to write
 * @start_code: whether adding the nal start code
 * @data: (out): the bit stream generated by the sei messages
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according h265 bit stream by providing sei messages.
 *
 * Returns: a #GstH265BitWriterResult
 *
 * Since: 1.22
 **/
GstH265BitWriterResult
gst_h265_bit_writer_sei (GArray * sei_messages,
    GstH265NalUnitType nal_type, gboolean start_code, guint8 * data,
    guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;
  GstH265SEIMessage *sei;
  gboolean have_written_data = FALSE;
  guint i;

  g_return_val_if_fail (sei_messages != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (nal_type == GST_H265_NAL_PREFIX_SEI
      || nal_type == GST_H265_NAL_SUFFIX_SEI, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_H265_BIT_WRITER_ERROR);

  if (nal_type == GST_H265_NAL_PREFIX_SEI) {
    GST_WARNING ("prefix sei is not supported");
    return GST_H265_BIT_WRITER_ERROR;
  }

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  if (start_code)
    WRITE_BITS (&bw, 0x00000001, 32);

  /* NAL unit header */
  /* forbidden_zero_bit */
  WRITE_BITS (&bw, 0, 1);
  /* nal_unit_type */
  WRITE_BITS (&bw, nal_type, 6);
  /* nuh_layer_id, only support 0 now */
  WRITE_BITS (&bw, 0, 6);
  /* nuh_temporal_id_plus1, only support 1 now */
  WRITE_BITS (&bw, 1, 3);

  for (i = 0; i < sei_messages->len; i++) {
    guint32 payload_size_data;
    guint32 payload_type_data;

    gst_bit_writer_init (&bw);

    sei = &g_array_index (sei_messages, GstH265SEIMessage, i);
    if (!_h265_bit_writer_sei_message (sei, &bw, &have_space))
      goto error;

    if (gst_bit_writer_get_size (&bw) == 0) {
      GST_FIXME ("Unsupported SEI type %d", sei->payloadType);
      continue;
    }

    have_written_data = TRUE;

    g_assert (gst_bit_writer_get_size (&bw) % 8 == 0);
    payload_size_data = (gst_bit_writer_get_size (&bw) + 7) / 8;
    payload_type_data = sei->payloadType;

    /* write payload type bytes */
    while (payload_type_data >= 0xff) {
      WRITE_BITS (&bw, 0xff, 8);
      payload_type_data -= -0xff;
    }
    WRITE_BITS (&bw, payload_type_data, 8);

    /* write payload size bytes */
    while (payload_size_data >= 0xff) {
      WRITE_BITS (&bw, 0xff, 8);
      payload_size_data -= -0xff;
    }
    WRITE_BITS (&bw, payload_size_data, 8);

    if (gst_bit_writer_get_size (&bw) / 8)
      WRITE_BYTES (&bw, gst_bit_writer_get_data (&bw),
          gst_bit_writer_get_size (&bw) / 8);

    gst_bit_writer_reset (&bw);
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

  *size = gst_bit_writer_get_size (&bw) / 8;
  gst_bit_writer_reset (&bw);
  return GST_H265_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;

  if (!have_space)
    return GST_H265_BIT_WRITER_NO_MORE_SPACE;
  return GST_H265_BIT_WRITER_INVALID_DATA;
}

/**
 * gst_h265_bit_writer_aud:
 * @pic_type: indicate the possible slice types list just
 *   as the H265 spec Table 7-2 defines
 * @start_code: whether adding the nal start code
 * @data: (out): the bit stream generated by the aud
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according h265 bit stream of an aud.
 *
 * Returns: a #GstH265BitWriterResult
 *
 * Since: 1.22
 **/
GstH265BitWriterResult
gst_h265_bit_writer_aud (guint8 pic_type, gboolean start_code,
    guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;

  g_return_val_if_fail (pic_type <= 2, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_H265_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  if (start_code)
    WRITE_BITS (&bw, 0x00000001, 32);

  /* NAL unit header */
  /* forbidden_zero_bit */
  WRITE_BITS (&bw, 0, 1);
  /* nal_unit_type */
  WRITE_BITS (&bw, GST_H265_NAL_AUD, 6);
  /* nuh_layer_id, only support 0 now */
  WRITE_BITS (&bw, 0, 6);
  /* nuh_temporal_id_plus1, only support 1 now */
  WRITE_BITS (&bw, 1, 3);

  WRITE_BITS (&bw, pic_type, 3);

  /* Add trailings. */
  WRITE_BITS (&bw, 1, 1);
  if (!gst_bit_writer_align_bytes (&bw, 0)) {
    goto error;
  }

  *size = gst_bit_writer_get_size (&bw) / 8;
  gst_bit_writer_reset (&bw);

  return GST_H265_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;

  if (!have_space)
    return GST_H265_BIT_WRITER_NO_MORE_SPACE;
  return GST_H265_BIT_WRITER_INVALID_DATA;
}

/**
 * gst_h265_bit_writer_convert_to_nal:
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
 * Returns: a #GstH265BitWriterResult
 *
 * Since: 1.22
 **/
GstH265BitWriterResult
gst_h265_bit_writer_convert_to_nal (guint nal_prefix_size,
    gboolean packetized, gboolean has_startcode, gboolean add_trailings,
    const guint8 * raw_data, gsize raw_size, guint8 * nal_data,
    guint * nal_size)
{
  NalWriter nw;
  guint8 *data;
  guint32 size = 0;
  gboolean need_more_space = FALSE;

  g_return_val_if_fail (
      (packetized && nal_prefix_size > 1 && nal_prefix_size < 5) ||
      (!packetized && (nal_prefix_size == 3 || nal_prefix_size == 4)),
      GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (raw_data != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (raw_size > 0, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (nal_data != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (nal_size != NULL, GST_H265_BIT_WRITER_ERROR);
  g_return_val_if_fail (*nal_size > 0, GST_H265_BIT_WRITER_ERROR);

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
      g_return_val_if_reached (GST_H265_BIT_WRITER_ERROR);
    }
  }

  /* If no RBSP trailing needed, it must align to byte. We assume
     that the rbsp trailing bits are already added. */
  if (!add_trailings)
    g_return_val_if_fail (raw_size % 8 == 0, GST_H265_BIT_WRITER_ERROR);

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
  return GST_H265_BIT_WRITER_OK;

error:
  nal_writer_reset (&nw);
  *nal_size = 0;

  GST_WARNING ("Failed to convert nal data");

  if (need_more_space)
    return GST_H265_BIT_WRITER_NO_MORE_SPACE;
  return GST_H265_BIT_WRITER_INVALID_DATA;
}
