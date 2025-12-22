/* Gstreamer H.265 bitstream parser
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 *  Contact: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
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
 * SECTION:gsth265parser
 * @title: GstH265Parser
 * @short_description: Convenience library for h265 video bitstream parsing.
 *
 * It offers you bitstream parsing in HEVC mode and non-HEVC mode. To identify
 * Nals in a bitstream and parse its headers, you should call:
 *
 *   * gst_h265_parser_identify_nalu() to identify the following nalu in
 *        non-HEVC bitstreams
 *
 *   * gst_h265_parser_identify_nalu_hevc() to identify the nalu in
 *        HEVC bitstreams
 *
 * Then, depending on the #GstH265NalUnitType of the newly parsed #GstH265NalUnit,
 * you should call the differents functions to parse the structure:
 *
 *   * From #GST_H265_NAL_SLICE_TRAIL_N to #GST_H265_NAL_SLICE_CRA_NUT: gst_h265_parser_parse_slice_hdr()
 *
 *   * `GST_H265_NAL_*_SEI`: gst_h265_parser_parse_sei()
 *
 *   * #GST_H265_NAL_VPS: gst_h265_parser_parse_vps()
 *
 *   * #GST_H265_NAL_SPS: gst_h265_parser_parse_sps()
 *
 *   * #GST_H265_NAL_PPS: #gst_h265_parser_parse_pps()
 *
 *   * Any other: gst_h265_parser_parse_nal()
 *
 * Note: You should always call gst_h265_parser_parse_nal() if you don't
 * actually need #GstH265NalUnitType to be parsed for your personal use, in
 * order to guarantee that the #GstH265Parser is always up to date.
 *
 * For more details about the structures, look at the ITU-T H.265
 * specifications, you can download them from:
 *
 *   * ITU-T H.265: http://www.itu.int/rec/T-REC-H.265
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "nalutils.h"
#include "gsth265parser.h"

#include <gst/base/gstbytereader.h>
#include <gst/base/gstbitreader.h>
#include <string.h>
#include <math.h>

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT gst_h265_debug_category_get()
static GstDebugCategory *
gst_h265_debug_category_get (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    GstDebugCategory *cat = NULL;

    GST_DEBUG_CATEGORY_INIT (cat, "codecparsers_h265", 0, "h265 parse library");

    g_once_init_leave (&cat_gonce, (gsize) cat);
  }

  return (GstDebugCategory *) cat_gonce;
}
#endif /* GST_DISABLE_GST_DEBUG */

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

static const guint8 zigzag_4x4[16] = {
  0, 1, 4, 8,
  5, 2, 3, 6,
  9, 12, 13, 10,
  7, 11, 14, 15,
};

static const guint8 zigzag_8x8[64] = {
  0, 1, 8, 16, 9, 2, 3, 10,
  17, 24, 32, 25, 18, 11, 4, 5,
  12, 19, 26, 33, 40, 48, 41, 34,
  27, 20, 13, 6, 7, 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36,
  29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46,
  53, 60, 61, 54, 47, 55, 62, 63
};

static const guint8 uprightdiagonal_4x4[16] = {
  0, 4, 1, 8,
  5, 2, 12, 9,
  6, 3, 13, 10,
  7, 14, 11, 15
};

static const guint8 uprightdiagonal_8x8[64] = {
  0, 8, 1, 16, 9, 2, 24, 17,
  10, 3, 32, 25, 18, 11, 4, 40,
  33, 26, 19, 12, 5, 48, 41, 34,
  27, 20, 13, 6, 56, 49, 42, 35,
  28, 21, 14, 7, 57, 50, 43, 36,
  29, 22, 15, 58, 51, 44, 37, 30,
  23, 59, 52, 45, 38, 31, 60, 53,
  46, 39, 61, 54, 47, 62, 55, 63
};

typedef struct
{
  guint par_n, par_d;
} PAR;

/* Table E-1 - Meaning of sample aspect ratio indicator (1..16) */
static const PAR aspect_ratios[17] = {
  {0, 0},
  {1, 1},
  {12, 11},
  {10, 11},
  {16, 11},
  {40, 33},
  {24, 11},
  {20, 11},
  {32, 11},
  {80, 33},
  {18, 11},
  {15, 11},
  {64, 33},
  {160, 99},
  {4, 3},
  {3, 2},
  {2, 1}
};

/*****  Utils ****/
#define EXTENDED_SAR 255

static GstH265VPS *
gst_h265_parser_get_vps (GstH265Parser * parser, guint8 vps_id)
{
  GstH265VPS *vps;

  vps = &parser->vps[vps_id];

  if (vps->valid)
    return vps;

  return NULL;
}

static GstH265SPS *
gst_h265_parser_get_sps (GstH265Parser * parser, guint8 sps_id)
{
  GstH265SPS *sps;

  sps = &parser->sps[sps_id];

  if (sps->valid)
    return sps;

  return NULL;
}

static GstH265PPS *
gst_h265_parser_get_pps (GstH265Parser * parser, guint8 pps_id)
{
  GstH265PPS *pps;

  pps = &parser->pps[pps_id];

  if (pps->valid)
    return pps;

  return NULL;
}

static gboolean
gst_h265_parse_nalu_header (GstH265NalUnit * nalu)
{
  guint8 *data = nalu->data + nalu->offset;
  GstBitReader br;

  if (nalu->size < 2)
    return FALSE;

  gst_bit_reader_init (&br, data, nalu->size - nalu->offset);

  /* skip the forbidden_zero_bit */
  gst_bit_reader_skip_unchecked (&br, 1);

  nalu->type = gst_bit_reader_get_bits_uint8_unchecked (&br, 6);
  nalu->layer_id = gst_bit_reader_get_bits_uint8_unchecked (&br, 6);
  nalu->temporal_id_plus1 = gst_bit_reader_get_bits_uint8_unchecked (&br, 3);
  nalu->header_bytes = 2;

  return TRUE;
}

struct h265_profile_string
{
  GstH265Profile profile;
  const gchar *name;
};

static const struct h265_profile_string h265_profiles[] = {
  /* keep in sync with definition in the header */
  {GST_H265_PROFILE_MAIN, "main"},
  {GST_H265_PROFILE_MAIN_10, "main-10"},
  {GST_H265_PROFILE_MAIN_STILL_PICTURE, "main-still-picture"},
  {GST_H265_PROFILE_MONOCHROME, "monochrome"},
  {GST_H265_PROFILE_MONOCHROME_12, "monochrome-12"},
  {GST_H265_PROFILE_MONOCHROME_16, "monochrome-16"},
  {GST_H265_PROFILE_MAIN_12, "main-12"},
  {GST_H265_PROFILE_MAIN_422_10, "main-422-10"},
  {GST_H265_PROFILE_MAIN_422_12, "main-422-12"},
  {GST_H265_PROFILE_MAIN_444, "main-444"},
  {GST_H265_PROFILE_MAIN_444_10, "main-444-10"},
  {GST_H265_PROFILE_MAIN_444_12, "main-444-12"},
  {GST_H265_PROFILE_MAIN_INTRA, "main-intra"},
  {GST_H265_PROFILE_MAIN_10_INTRA, "main-10-intra"},
  {GST_H265_PROFILE_MAIN_12_INTRA, "main-12-intra"},
  {GST_H265_PROFILE_MAIN_422_10_INTRA, "main-422-10-intra"},
  {GST_H265_PROFILE_MAIN_422_12_INTRA, "main-422-12-intra"},
  {GST_H265_PROFILE_MAIN_444_INTRA, "main-444-intra"},
  {GST_H265_PROFILE_MAIN_444_10_INTRA, "main-444-10-intra"},
  {GST_H265_PROFILE_MAIN_444_12_INTRA, "main-444-12-intra"},
  {GST_H265_PROFILE_MAIN_444_16_INTRA, "main-444-16-intra"},
  {GST_H265_PROFILE_MAIN_444_STILL_PICTURE, "main-444-still-picture"},
  {GST_H265_PROFILE_MAIN_444_16_STILL_PICTURE, "main-444-16-still-picture"},
  {GST_H265_PROFILE_MONOCHROME_10, "monochrome-10"},
  {GST_H265_PROFILE_HIGH_THROUGHPUT_444, "high-throughput-444"},
  {GST_H265_PROFILE_HIGH_THROUGHPUT_444_10, "high-throughput-444-10"},
  {GST_H265_PROFILE_HIGH_THROUGHPUT_444_14, "high-throughput-444-14"},
  {GST_H265_PROFILE_HIGH_THROUGHPUT_444_16_INTRA,
      "high-throughput-444-16-intra"},
  {GST_H265_PROFILE_SCREEN_EXTENDED_MAIN, "screen-extended-main"},
  {GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_10, "screen-extended-main-10"},
  {GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444, "screen-extended-main-444"},
  {GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444_10, "screen-extended-main-444-10"},
  {GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444,
      "screen-extended-high-throughput-444"},
  {GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_10,
      "screen-extended-high-throughput-444-10"},
  {GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_14,
      "screen-extended-high-throughput-444-14"},
  {GST_H265_PROFILE_MULTIVIEW_MAIN, "multiview-main"},
  {GST_H265_PROFILE_SCALABLE_MAIN, "scalable-main"},
  {GST_H265_PROFILE_SCALABLE_MAIN_10, "scalable-main-10"},
  {GST_H265_PROFILE_SCALABLE_MONOCHROME, "scalable-monochrome"},
  {GST_H265_PROFILE_SCALABLE_MONOCHROME_12, "scalable-monochrome-12"},
  {GST_H265_PROFILE_SCALABLE_MONOCHROME_16, "scalable-monochrome-16"},
  {GST_H265_PROFILE_SCALABLE_MAIN_444, "scalable-main-444"},
  {GST_H265_PROFILE_3D_MAIN, "3d-main"},
};

/****** Parsing functions *****/

static gboolean
gst_h265_parse_profile_tier_level (GstH265ProfileTierLevel * ptl,
    NalReader * nr, guint8 maxNumSubLayersMinus1)
{
  guint i, j;
  GST_DEBUG ("parsing \"ProfileTierLevel parameters\"");

  READ_UINT8 (nr, ptl->profile_space, 2);
  READ_UINT8 (nr, ptl->tier_flag, 1);
  READ_UINT8 (nr, ptl->profile_idc, 5);

  for (j = 0; j < 32; j++)
    READ_UINT8 (nr, ptl->profile_compatibility_flag[j], 1);

  READ_UINT8 (nr, ptl->progressive_source_flag, 1);
  READ_UINT8 (nr, ptl->interlaced_source_flag, 1);
  READ_UINT8 (nr, ptl->non_packed_constraint_flag, 1);
  READ_UINT8 (nr, ptl->frame_only_constraint_flag, 1);

  READ_UINT8 (nr, ptl->max_12bit_constraint_flag, 1);
  READ_UINT8 (nr, ptl->max_10bit_constraint_flag, 1);
  READ_UINT8 (nr, ptl->max_8bit_constraint_flag, 1);
  READ_UINT8 (nr, ptl->max_422chroma_constraint_flag, 1);
  READ_UINT8 (nr, ptl->max_420chroma_constraint_flag, 1);
  READ_UINT8 (nr, ptl->max_monochrome_constraint_flag, 1);
  READ_UINT8 (nr, ptl->intra_constraint_flag, 1);
  READ_UINT8 (nr, ptl->one_picture_only_constraint_flag, 1);
  READ_UINT8 (nr, ptl->lower_bit_rate_constraint_flag, 1);
  READ_UINT8 (nr, ptl->max_14bit_constraint_flag, 1);

  /* skip the reserved zero bits */
  if (!nal_reader_skip (nr, 34))
    goto error;

  READ_UINT8 (nr, ptl->level_idc, 8);
  for (j = 0; j < maxNumSubLayersMinus1; j++) {
    READ_UINT8 (nr, ptl->sub_layer_profile_present_flag[j], 1);
    READ_UINT8 (nr, ptl->sub_layer_level_present_flag[j], 1);
  }

  if (maxNumSubLayersMinus1 > 0) {
    for (i = maxNumSubLayersMinus1; i < 8; i++)
      if (!nal_reader_skip (nr, 2))
        goto error;
  }

  for (i = 0; i < maxNumSubLayersMinus1; i++) {
    if (ptl->sub_layer_profile_present_flag[i]) {
      READ_UINT8 (nr, ptl->sub_layer_profile_space[i], 2);
      READ_UINT8 (nr, ptl->sub_layer_tier_flag[i], 1);
      READ_UINT8 (nr, ptl->sub_layer_profile_idc[i], 5);

      for (j = 0; j < 32; j++)
        READ_UINT8 (nr, ptl->sub_layer_profile_compatibility_flag[i][j], 1);

      READ_UINT8 (nr, ptl->sub_layer_progressive_source_flag[i], 1);
      READ_UINT8 (nr, ptl->sub_layer_interlaced_source_flag[i], 1);
      READ_UINT8 (nr, ptl->sub_layer_non_packed_constraint_flag[i], 1);
      READ_UINT8 (nr, ptl->sub_layer_frame_only_constraint_flag[i], 1);

      if (!nal_reader_skip (nr, 44))
        goto error;
    }

    if (ptl->sub_layer_level_present_flag[i])
      READ_UINT8 (nr, ptl->sub_layer_level_idc[i], 8);
  }

  return TRUE;

error:
  GST_WARNING ("error parsing \"ProfileTierLevel Parameters\"");
  return FALSE;
}

static gboolean
gst_h265_parse_sub_layer_hrd_parameters (GstH265SubLayerHRDParams * sub_hrd,
    NalReader * nr, guint8 CpbCnt, guint8 sub_pic_hrd_params_present_flag)
{
  guint i;

  GST_DEBUG ("parsing \"SubLayer HRD Parameters\"");

  for (i = 0; i <= CpbCnt; i++) {
    READ_UE_MAX (nr, sub_hrd->bit_rate_value_minus1[i], G_MAXUINT32 - 1);
    READ_UE_MAX (nr, sub_hrd->cpb_size_value_minus1[i], G_MAXUINT32 - 1);

    if (sub_pic_hrd_params_present_flag) {
      READ_UE_MAX (nr, sub_hrd->cpb_size_du_value_minus1[i], G_MAXUINT32 - 1);
      READ_UE_MAX (nr, sub_hrd->bit_rate_du_value_minus1[i], G_MAXUINT32 - 1);
    }

    READ_UINT8 (nr, sub_hrd->cbr_flag[i], 1);
  }

  return TRUE;

error:
  GST_WARNING ("error parsing \"SubLayerHRD Parameters \"");
  return FALSE;
}

static gboolean
gst_h265_parse_hrd_parameters (GstH265HRDParams * hrd, NalReader * nr,
    guint8 commonInfPresentFlag, guint8 maxNumSubLayersMinus1)
{
  guint i;

  GST_DEBUG ("parsing \"HRD Parameters\"");

  /* set default values for fields that might not be present in the bitstream
     and have valid defaults */
  hrd->initial_cpb_removal_delay_length_minus1 = 23;
  hrd->au_cpb_removal_delay_length_minus1 = 23;
  hrd->dpb_output_delay_length_minus1 = 23;

  if (commonInfPresentFlag) {
    READ_UINT8 (nr, hrd->nal_hrd_parameters_present_flag, 1);
    READ_UINT8 (nr, hrd->vcl_hrd_parameters_present_flag, 1);

    if (hrd->nal_hrd_parameters_present_flag
        || hrd->vcl_hrd_parameters_present_flag) {

      READ_UINT8 (nr, hrd->sub_pic_hrd_params_present_flag, 1);

      if (hrd->sub_pic_hrd_params_present_flag) {
        READ_UINT8 (nr, hrd->tick_divisor_minus2, 8);
        READ_UINT8 (nr, hrd->du_cpb_removal_delay_increment_length_minus1, 5);
        READ_UINT8 (nr, hrd->sub_pic_cpb_params_in_pic_timing_sei_flag, 1);
        READ_UINT8 (nr, hrd->dpb_output_delay_du_length_minus1, 5);
      }

      READ_UINT8 (nr, hrd->bit_rate_scale, 4);
      READ_UINT8 (nr, hrd->cpb_size_scale, 4);

      if (hrd->sub_pic_hrd_params_present_flag)
        READ_UINT8 (nr, hrd->cpb_size_du_scale, 4);

      READ_UINT8 (nr, hrd->initial_cpb_removal_delay_length_minus1, 5);
      READ_UINT8 (nr, hrd->au_cpb_removal_delay_length_minus1, 5);
      READ_UINT8 (nr, hrd->dpb_output_delay_length_minus1, 5);
    }
  }

  for (i = 0; i <= maxNumSubLayersMinus1; i++) {
    READ_UINT8 (nr, hrd->fixed_pic_rate_general_flag[i], 1);

    if (!hrd->fixed_pic_rate_general_flag[i]) {
      READ_UINT8 (nr, hrd->fixed_pic_rate_within_cvs_flag[i], 1);
    } else
      hrd->fixed_pic_rate_within_cvs_flag[i] = 1;

    if (hrd->fixed_pic_rate_within_cvs_flag[i]) {
      READ_UE_MAX (nr, hrd->elemental_duration_in_tc_minus1[i], 2047);
    } else
      READ_UINT8 (nr, hrd->low_delay_hrd_flag[i], 1);

    if (!hrd->low_delay_hrd_flag[i])
      READ_UE_MAX (nr, hrd->cpb_cnt_minus1[i], 31);

    if (hrd->nal_hrd_parameters_present_flag)
      if (!gst_h265_parse_sub_layer_hrd_parameters (&hrd->sublayer_hrd_params
              [i], nr, hrd->cpb_cnt_minus1[i],
              hrd->sub_pic_hrd_params_present_flag))
        goto error;

    if (hrd->vcl_hrd_parameters_present_flag)
      if (!gst_h265_parse_sub_layer_hrd_parameters (&hrd->sublayer_hrd_params
              [i], nr, hrd->cpb_cnt_minus1[i],
              hrd->sub_pic_hrd_params_present_flag))
        goto error;
  }

  return TRUE;

error:
  GST_WARNING ("error parsing \"HRD Parameters\"");
  return FALSE;
}

static gboolean
gst_h265_parse_vui_parameters (GstH265SPS * sps, NalReader * nr)
{
  GstH265VUIParams *vui = &sps->vui_params;

  GST_DEBUG ("parsing \"VUI Parameters\"");

  /* set default values for fields that might not be present in the bitstream
     and have valid defaults */
  vui->video_format = 5;
  vui->colour_primaries = 2;
  vui->transfer_characteristics = 2;
  vui->matrix_coefficients = 2;
  vui->motion_vectors_over_pic_boundaries_flag = 1;
  vui->max_bytes_per_pic_denom = 2;
  vui->max_bits_per_min_cu_denom = 1;
  vui->log2_max_mv_length_horizontal = 15;
  vui->log2_max_mv_length_vertical = 15;

  if (sps && sps->profile_tier_level.progressive_source_flag
      && sps->profile_tier_level.interlaced_source_flag)
    vui->frame_field_info_present_flag = 1;

  READ_UINT8 (nr, vui->aspect_ratio_info_present_flag, 1);
  if (vui->aspect_ratio_info_present_flag) {
    READ_UINT8 (nr, vui->aspect_ratio_idc, 8);
    if (vui->aspect_ratio_idc == EXTENDED_SAR) {
      READ_UINT16 (nr, vui->sar_width, 16);
      READ_UINT16 (nr, vui->sar_height, 16);
      vui->par_n = vui->sar_width;
      vui->par_d = vui->sar_height;
    } else if (vui->aspect_ratio_idc <= 16) {
      vui->par_n = aspect_ratios[vui->aspect_ratio_idc].par_n;
      vui->par_d = aspect_ratios[vui->aspect_ratio_idc].par_d;
    }
  }

  READ_UINT8 (nr, vui->overscan_info_present_flag, 1);
  if (vui->overscan_info_present_flag)
    READ_UINT8 (nr, vui->overscan_appropriate_flag, 1);

  READ_UINT8 (nr, vui->video_signal_type_present_flag, 1);
  if (vui->video_signal_type_present_flag) {

    READ_UINT8 (nr, vui->video_format, 3);
    READ_UINT8 (nr, vui->video_full_range_flag, 1);
    READ_UINT8 (nr, vui->colour_description_present_flag, 1);
    if (vui->colour_description_present_flag) {
      READ_UINT8 (nr, vui->colour_primaries, 8);
      READ_UINT8 (nr, vui->transfer_characteristics, 8);
      READ_UINT8 (nr, vui->matrix_coefficients, 8);
    }
  }

  READ_UINT8 (nr, vui->chroma_loc_info_present_flag, 1);
  if (vui->chroma_loc_info_present_flag) {
    READ_UE_MAX (nr, vui->chroma_sample_loc_type_top_field, 5);
    READ_UE_MAX (nr, vui->chroma_sample_loc_type_bottom_field, 5);
  }

  READ_UINT8 (nr, vui->neutral_chroma_indication_flag, 1);
  READ_UINT8 (nr, vui->field_seq_flag, 1);
  READ_UINT8 (nr, vui->frame_field_info_present_flag, 1);

  READ_UINT8 (nr, vui->default_display_window_flag, 1);
  if (vui->default_display_window_flag) {
    READ_UE (nr, vui->def_disp_win_left_offset);
    READ_UE (nr, vui->def_disp_win_right_offset);
    READ_UE (nr, vui->def_disp_win_top_offset);
    READ_UE (nr, vui->def_disp_win_bottom_offset);
  }

  READ_UINT8 (nr, vui->timing_info_present_flag, 1);
  if (vui->timing_info_present_flag) {
    READ_UINT32 (nr, vui->num_units_in_tick, 32);
    if (vui->num_units_in_tick == 0)
      GST_WARNING ("num_units_in_tick = 0 detected in stream "
          "(incompliant to H.265 E.2.1).");

    READ_UINT32 (nr, vui->time_scale, 32);
    if (vui->time_scale == 0)
      GST_WARNING ("time_scale = 0 detected in stream "
          "(incompliant to H.265 E.2.1).");

    READ_UINT8 (nr, vui->poc_proportional_to_timing_flag, 1);
    if (vui->poc_proportional_to_timing_flag)
      READ_UE_MAX (nr, vui->num_ticks_poc_diff_one_minus1, G_MAXUINT32 - 1);

    READ_UINT8 (nr, vui->hrd_parameters_present_flag, 1);
    if (vui->hrd_parameters_present_flag)
      if (!gst_h265_parse_hrd_parameters (&vui->hrd_params, nr, 1,
              sps->max_sub_layers_minus1))
        goto error;
  }

  READ_UINT8 (nr, vui->bitstream_restriction_flag, 1);
  if (vui->bitstream_restriction_flag) {
    READ_UINT8 (nr, vui->tiles_fixed_structure_flag, 1);
    READ_UINT8 (nr, vui->motion_vectors_over_pic_boundaries_flag, 1);
    READ_UINT8 (nr, vui->restricted_ref_pic_lists_flag, 1);
    READ_UE_MAX (nr, vui->min_spatial_segmentation_idc, 4096);
    READ_UE_MAX (nr, vui->max_bytes_per_pic_denom, 16);
    READ_UE_MAX (nr, vui->max_bits_per_min_cu_denom, 16);
    READ_UE_MAX (nr, vui->log2_max_mv_length_horizontal, 16);
    READ_UE_MAX (nr, vui->log2_max_mv_length_vertical, 15);
  }

  vui->parsed = TRUE;
  return TRUE;

error:
  GST_WARNING ("error parsing \"VUI Parameters\"");
  vui->parsed = FALSE;
  return FALSE;
}

static gboolean
get_scaling_list_params (GstH265ScalingList * dest_scaling_list,
    guint8 sizeId, guint8 matrixId, guint8 ** sl, guint8 * size,
    gint16 ** scaling_list_dc_coef_minus8)
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
            dest_scaling_list->scaling_list_dc_coef_minus8_16x16;
      break;
    case GST_H265_QUANT_MATIX_32X32:
      *sl = dest_scaling_list->scaling_lists_32x32[matrixId];
      if (size)
        *size = 64;
      if (scaling_list_dc_coef_minus8)
        *scaling_list_dc_coef_minus8 =
            dest_scaling_list->scaling_list_dc_coef_minus8_32x32;
      break;
    default:
      return FALSE;
  }
  return TRUE;
}

static gboolean
get_default_scaling_lists (guint8 ** sl, guint8 sizeId, guint8 matrixId)
{
  switch (sizeId) {
    case GST_H265_QUANT_MATIX_4X4:
      memcpy (*sl, default_scaling_list0, 16);
      break;

    case GST_H265_QUANT_MATIX_8X8:
    case GST_H265_QUANT_MATIX_16X16:
      if (matrixId <= 2)
        memcpy (*sl, default_scaling_list1, 64);
      else
        memcpy (*sl, default_scaling_list2, 64);
      break;

    case GST_H265_QUANT_MATIX_32X32:
      if (matrixId == 0)
        memcpy (*sl, default_scaling_list1, 64);
      else
        memcpy (*sl, default_scaling_list2, 64);
      break;

    default:
      return FALSE;
      break;
  }
  return TRUE;
}

static gboolean
gst_h265_parser_parse_scaling_lists (NalReader * nr,
    GstH265ScalingList * dest_scaling_list, gboolean use_default)
{
  guint8 sizeId;
  guint8 matrixId;
  guint8 scaling_list_pred_mode_flag = 0;
  guint8 scaling_list_pred_matrix_id_delta = 0;
  guint8 size, i;

  GST_DEBUG ("parsing scaling lists");

  for (sizeId = 0; sizeId < 4; sizeId++) {
    for (matrixId = 0; matrixId < ((sizeId == 3) ? 2 : 6); matrixId++) {
      gint16 *scaling_list_dc_coef_minus8 = NULL;
      guint8 *sl;

      if (!get_scaling_list_params (dest_scaling_list, sizeId, matrixId, &sl,
              &size, &scaling_list_dc_coef_minus8))
        goto error;

      /* use_default_scaling_matrices forcefully which means,
       * sps_scaling_list_enabled_flag=TRUE,
       * sps_scaling_list_data_present_flag=FALSE,
       * pps_scaling_list_data_present_falg=FALSE */
      if (use_default) {
        if (!get_default_scaling_lists (&sl, sizeId, matrixId))
          goto error;

        /* Inferring the value of scaling_list_dc_coef_minus8 */
        if (sizeId > 1)
          scaling_list_dc_coef_minus8[matrixId] = 8;

      } else {
        READ_UINT8 (nr, scaling_list_pred_mode_flag, 1);

        if (!scaling_list_pred_mode_flag) {
          guint8 refMatrixId;

          READ_UE_MAX (nr, scaling_list_pred_matrix_id_delta, matrixId);

          if (!scaling_list_pred_matrix_id_delta) {
            if (!get_default_scaling_lists (&sl, sizeId, matrixId))
              goto error;

            /* Inferring the value of scaling_list_dc_coef_minus8 */
            if (sizeId > 1)
              scaling_list_dc_coef_minus8[matrixId] = 8;

          } else {
            guint8 *temp_sl;

            refMatrixId = matrixId - scaling_list_pred_matrix_id_delta; /* 7-30 */

            if (!get_scaling_list_params (dest_scaling_list, sizeId,
                    refMatrixId, &temp_sl, NULL, NULL))
              goto error;

            for (i = 0; i < size; i++)
              sl[i] = temp_sl[i];       /* 7-31 */


            /* Inferring the value of scaling_list_dc_coef_minus8 */
            if (sizeId > 1)
              scaling_list_dc_coef_minus8[matrixId] =
                  scaling_list_dc_coef_minus8[refMatrixId];
          }
        } else {
          guint8 nextCoef = 8;
          gint8 scaling_list_delta_coef;

          if (sizeId > 1) {
            READ_SE_ALLOWED (nr, scaling_list_dc_coef_minus8[matrixId], -7,
                247);
            nextCoef = scaling_list_dc_coef_minus8[matrixId] + 8;
          }

          for (i = 0; i < size; i++) {
            READ_SE_ALLOWED (nr, scaling_list_delta_coef, -128, 127);
            nextCoef = (nextCoef + scaling_list_delta_coef) & 0xff;
            sl[i] = nextCoef;
          }
        }
      }
    }
  }

  return TRUE;

error:
  GST_WARNING ("error parsing scaling lists");
  return FALSE;
}

static gboolean
gst_h265_parser_parse_short_term_ref_pic_sets (GstH265ShortTermRefPicSet *
    stRPS, NalReader * nr, guint8 stRpsIdx, GstH265SPS * sps)
{
  guint8 num_short_term_ref_pic_sets;
  guint8 RefRpsIdx = 0;
  gint16 deltaRps = 0;
  guint8 use_delta_flag[16] = { 0 };
  guint8 used_by_curr_pic_flag[16] = { 0 };
  guint32 delta_poc_s0_minus1[16] = { 0 };
  guint32 delta_poc_s1_minus1[16] = { 0 };
  gint j, i = 0;
  gint dPoc;

  GST_DEBUG ("parsing \"ShortTermRefPicSetParameters\"");

  /* set default values for fields that might not be present in the bitstream
     and have valid defaults */
  for (j = 0; j < 16; j++)
    use_delta_flag[j] = 1;

  num_short_term_ref_pic_sets = sps->num_short_term_ref_pic_sets;

  if (stRpsIdx != 0)
    READ_UINT8 (nr, stRPS->inter_ref_pic_set_prediction_flag, 1);

  if (stRPS->inter_ref_pic_set_prediction_flag) {
    GstH265ShortTermRefPicSet *RefRPS;

    if (stRpsIdx == num_short_term_ref_pic_sets)
      READ_UE_MAX (nr, stRPS->delta_idx_minus1, stRpsIdx - 1);

    READ_UINT8 (nr, stRPS->delta_rps_sign, 1);
    READ_UE_MAX (nr, stRPS->abs_delta_rps_minus1, 32767);

    RefRpsIdx = stRpsIdx - stRPS->delta_idx_minus1 - 1; /* 7-45 */
    deltaRps = (1 - 2 * stRPS->delta_rps_sign) * (stRPS->abs_delta_rps_minus1 + 1);     /* 7-46 */

    RefRPS = &sps->short_term_ref_pic_set[RefRpsIdx];
    stRPS->NumDeltaPocsOfRefRpsIdx = RefRPS->NumDeltaPocs;

    for (j = 0; j <= RefRPS->NumDeltaPocs; j++) {
      READ_UINT8 (nr, used_by_curr_pic_flag[j], 1);
      if (!used_by_curr_pic_flag[j])
        READ_UINT8 (nr, use_delta_flag[j], 1);
    }

    /* 7-47: calculate NumNegativePics, DeltaPocS0 and UsedByCurrPicS0 */
    i = 0;
    for (j = (RefRPS->NumPositivePics - 1); j >= 0; j--) {
      dPoc = RefRPS->DeltaPocS1[j] + deltaRps;
      if (dPoc < 0 && use_delta_flag[RefRPS->NumNegativePics + j]) {
        stRPS->DeltaPocS0[i] = dPoc;
        stRPS->UsedByCurrPicS0[i++] =
            used_by_curr_pic_flag[RefRPS->NumNegativePics + j];
      }
    }
    if (deltaRps < 0 && use_delta_flag[RefRPS->NumDeltaPocs]) {
      stRPS->DeltaPocS0[i] = deltaRps;
      stRPS->UsedByCurrPicS0[i++] = used_by_curr_pic_flag[RefRPS->NumDeltaPocs];
    }
    for (j = 0; j < RefRPS->NumNegativePics; j++) {
      dPoc = RefRPS->DeltaPocS0[j] + deltaRps;
      if (dPoc < 0 && use_delta_flag[j]) {
        stRPS->DeltaPocS0[i] = dPoc;
        stRPS->UsedByCurrPicS0[i++] = used_by_curr_pic_flag[j];
      }
    }
    stRPS->NumNegativePics = i;

    /* 7-48: calculate NumPositivePics, DeltaPocS1 and UsedByCurrPicS1 */
    i = 0;
    for (j = (RefRPS->NumNegativePics - 1); j >= 0; j--) {
      dPoc = RefRPS->DeltaPocS0[j] + deltaRps;
      if (dPoc > 0 && use_delta_flag[j]) {
        stRPS->DeltaPocS1[i] = dPoc;
        stRPS->UsedByCurrPicS1[i++] = used_by_curr_pic_flag[j];
      }
    }
    if (deltaRps > 0 && use_delta_flag[RefRPS->NumDeltaPocs]) {
      stRPS->DeltaPocS1[i] = deltaRps;
      stRPS->UsedByCurrPicS1[i++] = used_by_curr_pic_flag[RefRPS->NumDeltaPocs];
    }
    for (j = 0; j < RefRPS->NumPositivePics; j++) {
      dPoc = RefRPS->DeltaPocS1[j] + deltaRps;
      if (dPoc > 0 && use_delta_flag[RefRPS->NumNegativePics + j]) {
        stRPS->DeltaPocS1[i] = dPoc;
        stRPS->UsedByCurrPicS1[i++] =
            used_by_curr_pic_flag[RefRPS->NumNegativePics + j];
      }
    }
    stRPS->NumPositivePics = i;

  } else {
    /* 7-49 */
    READ_UE_MAX (nr, stRPS->NumNegativePics,
        sps->max_dec_pic_buffering_minus1[sps->max_sub_layers_minus1]);

    /* 7-50 */
    READ_UE_MAX (nr, stRPS->NumPositivePics,
        (sps->max_dec_pic_buffering_minus1[sps->max_sub_layers_minus1] -
            stRPS->NumNegativePics));

    for (i = 0; i < stRPS->NumNegativePics; i++) {
      READ_UE_MAX (nr, delta_poc_s0_minus1[i], 32767);
      /* 7-51 */
      READ_UINT8 (nr, stRPS->UsedByCurrPicS0[i], 1);

      if (i == 0) {
        /* 7-53 */
        stRPS->DeltaPocS0[i] = -(delta_poc_s0_minus1[i] + 1);
      } else {
        /* 7-55 */
        stRPS->DeltaPocS0[i] =
            stRPS->DeltaPocS0[i - 1] - (delta_poc_s0_minus1[i] + 1);
      }
    }

    for (j = 0; j < stRPS->NumPositivePics; j++) {
      READ_UE_MAX (nr, delta_poc_s1_minus1[j], 32767);

      /* 7-52 */
      READ_UINT8 (nr, stRPS->UsedByCurrPicS1[j], 1);

      if (j == 0) {
        /* 7-54 */
        stRPS->DeltaPocS1[j] = delta_poc_s1_minus1[j] + 1;
      } else {
        /* 7-56 */
        stRPS->DeltaPocS1[j] =
            stRPS->DeltaPocS1[j - 1] + (delta_poc_s1_minus1[j] + 1);
      }
    }

  }

  /* 7-57 */
  stRPS->NumDeltaPocs = stRPS->NumPositivePics + stRPS->NumNegativePics;

  return TRUE;

error:
  GST_WARNING ("error parsing \"ShortTermRefPicSet Parameters\"");
  return FALSE;
}

static gboolean
gst_h265_slice_parse_ref_pic_list_modification (GstH265SliceHdr * slice,
    NalReader * nr, gint NumPocTotalCurr)
{
  guint i;
  GstH265RefPicListModification *rpl_mod = &slice->ref_pic_list_modification;
  const guint n = gst_util_ceil_log2 (NumPocTotalCurr);

  READ_UINT8 (nr, rpl_mod->ref_pic_list_modification_flag_l0, 1);

  if (rpl_mod->ref_pic_list_modification_flag_l0) {
    for (i = 0; i <= slice->num_ref_idx_l0_active_minus1; i++) {
      READ_UINT32 (nr, rpl_mod->list_entry_l0[i], n);
      CHECK_ALLOWED_MAX (rpl_mod->list_entry_l0[i], (NumPocTotalCurr - 1));
    }
  }
  if (GST_H265_IS_B_SLICE (slice)) {
    READ_UINT8 (nr, rpl_mod->ref_pic_list_modification_flag_l1, 1);
    if (rpl_mod->ref_pic_list_modification_flag_l1)
      for (i = 0; i <= slice->num_ref_idx_l1_active_minus1; i++) {
        READ_UINT32 (nr, rpl_mod->list_entry_l1[i], n);
        CHECK_ALLOWED_MAX (rpl_mod->list_entry_l1[i], (NumPocTotalCurr - 1));
      }
  }

  return TRUE;

error:
  GST_WARNING ("error parsing \"Reference picture list modifications\"");
  return FALSE;
}

static gboolean
gst_h265_slice_parse_pred_weight_table (GstH265SliceHdr * slice, NalReader * nr)
{
  GstH265PredWeightTable *p;
  gint i, j;
  GstH265PPS *pps = slice->pps;
  GstH265SPS *sps = pps->sps;

  GST_DEBUG ("parsing \"Prediction weight table\"");

  p = &slice->pred_weight_table;

  READ_UE_MAX (nr, p->luma_log2_weight_denom, 7);

  if (sps->chroma_format_idc != 0) {
    READ_SE_ALLOWED (nr, p->delta_chroma_log2_weight_denom,
        (0 - p->luma_log2_weight_denom), (7 - p->luma_log2_weight_denom));
  }

  for (i = 0; i <= slice->num_ref_idx_l0_active_minus1; i++)
    READ_UINT8 (nr, p->luma_weight_l0_flag[i], 1);

  if (sps->chroma_format_idc != 0)
    for (i = 0; i <= slice->num_ref_idx_l0_active_minus1; i++)
      READ_UINT8 (nr, p->chroma_weight_l0_flag[i], 1);

  for (i = 0; i <= slice->num_ref_idx_l0_active_minus1; i++) {
    if (p->luma_weight_l0_flag[i]) {
      READ_SE_ALLOWED (nr, p->delta_luma_weight_l0[i], -128, 127);
      READ_SE_ALLOWED (nr, p->luma_offset_l0[i], -128, 127);
    }
    if (p->chroma_weight_l0_flag[i])
      for (j = 0; j < 2; j++) {
        READ_SE_ALLOWED (nr, p->delta_chroma_weight_l0[i][j], -128, 127);
        READ_SE_ALLOWED (nr, p->delta_chroma_offset_l0[i][j], -512, 511);
      }
  }

  if (GST_H265_IS_B_SLICE (slice)) {
    for (i = 0; i <= slice->num_ref_idx_l1_active_minus1; i++)
      READ_UINT8 (nr, p->luma_weight_l1_flag[i], 1);
    if (sps->chroma_format_idc != 0)
      for (i = 0; i <= slice->num_ref_idx_l1_active_minus1; i++)
        READ_UINT8 (nr, p->chroma_weight_l1_flag[i], 1);

    for (i = 0; i <= slice->num_ref_idx_l1_active_minus1; i++) {
      if (p->luma_weight_l1_flag[i]) {
        READ_SE_ALLOWED (nr, p->delta_luma_weight_l1[i], -128, 127);
        READ_SE_ALLOWED (nr, p->luma_offset_l1[i], -128, 127);
      }
      if (p->chroma_weight_l1_flag[i])
        for (j = 0; j < 2; j++) {
          READ_SE_ALLOWED (nr, p->delta_chroma_weight_l1[i][j], -128, 127);
          READ_SE_ALLOWED (nr, p->delta_chroma_offset_l1[i][j], -512, 511);
        }
    }
  }

  return TRUE;

error:
  GST_WARNING ("error parsing \"Prediction weight table\"");
  return FALSE;
}

static GstH265ParserResult
gst_h265_parser_parse_buffering_period (GstH265Parser * parser,
    GstH265BufferingPeriod * per, NalReader * nr)
{
  GstH265SPS *sps;
  guint8 sps_id;
  guint i;
  guint n;

  GST_DEBUG ("parsing \"Buffering period\"");

  READ_UE_MAX (nr, sps_id, GST_H265_MAX_SPS_COUNT - 1);
  sps = gst_h265_parser_get_sps (parser, sps_id);
  if (!sps) {
    GST_WARNING ("couldn't find associated sequence parameter set with id: %d",
        sps_id);
    return GST_H265_PARSER_BROKEN_LINK;
  }
  per->sps = sps;

  if (sps->vui_parameters_present_flag) {
    GstH265VUIParams *vui = &sps->vui_params;
    GstH265HRDParams *hrd = &vui->hrd_params;

    if (!hrd->sub_pic_hrd_params_present_flag)
      READ_UINT8 (nr, per->irap_cpb_params_present_flag, 1);

    if (per->irap_cpb_params_present_flag) {
      READ_UINT8 (nr, per->cpb_delay_offset,
          (hrd->au_cpb_removal_delay_length_minus1 + 1));
      READ_UINT8 (nr, per->dpb_delay_offset,
          (hrd->dpb_output_delay_length_minus1 + 1));
    }

    n = hrd->initial_cpb_removal_delay_length_minus1 + 1;

    READ_UINT8 (nr, per->concatenation_flag, 1);
    READ_UINT8 (nr, per->au_cpb_removal_delay_delta_minus1,
        (hrd->au_cpb_removal_delay_length_minus1 + 1));

    if (hrd->nal_hrd_parameters_present_flag) {
      for (i = 0; i <= hrd->cpb_cnt_minus1[i]; i++) {
        READ_UINT8 (nr, per->nal_initial_cpb_removal_delay[i], n);
        READ_UINT8 (nr, per->nal_initial_cpb_removal_offset[i], n);
        if (hrd->sub_pic_hrd_params_present_flag
            || per->irap_cpb_params_present_flag) {
          READ_UINT8 (nr, per->nal_initial_alt_cpb_removal_delay[i], n);
          READ_UINT8 (nr, per->nal_initial_alt_cpb_removal_offset[i], n);
        }
      }
    }

    if (hrd->vcl_hrd_parameters_present_flag) {
      for (i = 0; i <= hrd->cpb_cnt_minus1[i]; i++) {
        READ_UINT8 (nr, per->vcl_initial_cpb_removal_delay[i], n);
        READ_UINT8 (nr, per->vcl_initial_cpb_removal_offset[i], n);
        if (hrd->sub_pic_hrd_params_present_flag
            || per->irap_cpb_params_present_flag) {
          READ_UINT8 (nr, per->vcl_initial_alt_cpb_removal_delay[i], n);
          READ_UINT8 (nr, per->vcl_initial_alt_cpb_removal_offset[i], n);
        }
      }
    }

  }
  return GST_H265_PARSER_OK;

error:
  GST_WARNING ("error parsing \"Buffering period\"");
  return GST_H265_PARSER_ERROR;
}

static GstH265ParserResult
gst_h265_parser_parse_pic_timing (GstH265Parser * parser,
    GstH265PicTiming * tim, NalReader * nr)
{
  GstH265ProfileTierLevel *profile_tier_level;
  guint i;

  GST_DEBUG ("parsing \"Picture timing\"");
  if (!parser->last_sps || !parser->last_sps->valid) {
    GST_WARNING ("didn't get the associated sequence parameter set for the "
        "current access unit");
    goto error;
  }

  profile_tier_level = &parser->last_sps->profile_tier_level;

  /* set default values */
  if (!profile_tier_level->progressive_source_flag
      && profile_tier_level->interlaced_source_flag)
    tim->source_scan_type = 0;
  else if (profile_tier_level->progressive_source_flag
      && !profile_tier_level->interlaced_source_flag)
    tim->source_scan_type = 1;
  else
    tim->source_scan_type = 2;

  if (parser->last_sps->vui_parameters_present_flag) {
    GstH265VUIParams *vui = &parser->last_sps->vui_params;

    if (vui->frame_field_info_present_flag) {
      READ_UINT8 (nr, tim->pic_struct, 4);
      READ_UINT8 (nr, tim->source_scan_type, 2);
      READ_UINT8 (nr, tim->duplicate_flag, 1);
    } else {
      /* set default values */
      tim->pic_struct = 0;
    }

    if (vui->hrd_parameters_present_flag) {
      GstH265HRDParams *hrd = &vui->hrd_params;

      READ_UINT8 (nr, tim->au_cpb_removal_delay_minus1,
          (hrd->au_cpb_removal_delay_length_minus1 + 1));
      READ_UINT8 (nr, tim->pic_dpb_output_delay,
          (hrd->dpb_output_delay_length_minus1 + 1));

      if (hrd->sub_pic_hrd_params_present_flag)
        READ_UINT8 (nr, tim->pic_dpb_output_du_delay,
            (hrd->dpb_output_delay_du_length_minus1 + 1));

      if (hrd->sub_pic_hrd_params_present_flag
          && hrd->sub_pic_cpb_params_in_pic_timing_sei_flag) {
        READ_UE (nr, tim->num_decoding_units_minus1);

        READ_UINT8 (nr, tim->du_common_cpb_removal_delay_flag, 1);
        if (tim->du_common_cpb_removal_delay_flag)
          READ_UINT8 (nr, tim->du_common_cpb_removal_delay_increment_minus1,
              (hrd->du_cpb_removal_delay_increment_length_minus1 + 1));

        tim->num_nalus_in_du_minus1 =
            g_new0 (guint32, (tim->num_decoding_units_minus1 + 1));
        tim->du_cpb_removal_delay_increment_minus1 =
            g_new0 (guint8, (tim->num_decoding_units_minus1 + 1));

        for (i = 0; i <= tim->num_decoding_units_minus1; i++) {
          READ_UE (nr, tim->num_nalus_in_du_minus1[i]);

          if (!tim->du_common_cpb_removal_delay_flag
              && (i < tim->num_decoding_units_minus1))
            READ_UINT8 (nr, tim->du_cpb_removal_delay_increment_minus1[i],
                (hrd->du_cpb_removal_delay_increment_length_minus1 + 1));
        }
      }
    }
  }
  return GST_H265_PARSER_OK;

error:
  GST_WARNING ("error parsing \"Picture timing\"");
  return GST_H265_PARSER_ERROR;
}

static GstH265ParserResult
gst_h265_parser_parse_recovery_point (GstH265Parser * parser,
    GstH265RecoveryPoint * rp, NalReader * nr)
{
  GstH265SPS *const sps = parser->last_sps;
  gint32 max_pic_order_cnt_lsb;

  GST_DEBUG ("parsing \"Recovery point\"");
  if (!sps || !sps->valid) {
    GST_WARNING ("didn't get the associated sequence parameter set for the "
        "current access unit");
    goto error;
  }

  max_pic_order_cnt_lsb = pow (2, (sps->log2_max_pic_order_cnt_lsb_minus4 + 4));
  READ_SE_ALLOWED (nr, rp->recovery_poc_cnt, -max_pic_order_cnt_lsb / 2,
      max_pic_order_cnt_lsb - 1);
  READ_UINT8 (nr, rp->exact_match_flag, 1);
  READ_UINT8 (nr, rp->broken_link_flag, 1);

  return GST_H265_PARSER_OK;

error:
  GST_WARNING ("error parsing \"Recovery point\"");
  return GST_H265_PARSER_ERROR;
}


static GstH265ParserResult
gst_h265_parser_parse_registered_user_data (GstH265Parser * parser,
    GstH265RegisteredUserData * rud, NalReader * nr, guint payload_size)
{
  guint8 *data = NULL;
  guint i;

  rud->data = NULL;
  rud->size = 0;

  if (payload_size < 2) {
    GST_WARNING ("Too small payload size %d", payload_size);
    return GST_H265_PARSER_BROKEN_DATA;
  }

  READ_UINT8 (nr, rud->country_code, 8);
  --payload_size;

  if (rud->country_code == 0xFF) {
    READ_UINT8 (nr, rud->country_code_extension, 8);
    --payload_size;
  } else {
    rud->country_code_extension = 0;
  }

  if (payload_size < 1) {
    GST_WARNING ("No more remaining payload data to store");
    return GST_H265_PARSER_BROKEN_DATA;
  }

  data = g_malloc (payload_size);
  for (i = 0; i < payload_size; ++i) {
    READ_UINT8 (nr, data[i], 8);
  }

  GST_MEMDUMP ("SEI user data", data, payload_size);

  rud->data = data;
  rud->size = payload_size;
  return GST_H265_PARSER_OK;

error:
  {
    GST_WARNING ("error parsing \"Registered User Data\"");
    g_free (data);
    return GST_H265_PARSER_ERROR;
  }
}

static GstH265ParserResult
gst_h265_parser_parse_user_data_unregistered (GstH265Parser * parser,
    GstH265UserDataUnregistered * urud, NalReader * nr, guint payload_size)
{
  guint8 *data = NULL;
  gint i;

  if (payload_size < 16) {
    GST_WARNING ("Too small payload size %d", payload_size);
    return GST_H265_PARSER_BROKEN_DATA;
  }

  for (int i = 0; i < 16; i++) {
    READ_UINT8 (nr, urud->uuid[i], 8);
  }
  payload_size -= 16;

  urud->size = payload_size;

  data = g_malloc0 (payload_size);
  for (i = 0; i < payload_size; ++i) {
    READ_UINT8 (nr, data[i], 8);
  }

  if (payload_size < 1) {
    GST_WARNING ("No more remaining payload data to store");
    g_clear_pointer (&data, g_free);
    return GST_H265_PARSER_BROKEN_DATA;
  }

  urud->data = data;
  GST_MEMDUMP ("SEI user data unregistered", data, payload_size);
  return GST_H265_PARSER_OK;

error:
  {
    GST_WARNING ("error parsing \"User Data Unregistered\"");
    g_clear_pointer (&data, g_free);
    return GST_H265_PARSER_ERROR;
  }
}

static GstH265ParserResult
gst_h265_parser_parse_time_code (GstH265Parser * parser,
    GstH265TimeCode * tc, NalReader * nr)
{
  guint i;

  GST_DEBUG ("parsing \"Time code\"");

  READ_UINT8 (nr, tc->num_clock_ts, 2);

  for (i = 0; i < tc->num_clock_ts; i++) {
    READ_UINT8 (nr, tc->clock_timestamp_flag[i], 1);
    if (tc->clock_timestamp_flag[i]) {
      READ_UINT8 (nr, tc->units_field_based_flag[i], 1);
      READ_UINT8 (nr, tc->counting_type[i], 5);
      READ_UINT8 (nr, tc->full_timestamp_flag[i], 1);
      READ_UINT8 (nr, tc->discontinuity_flag[i], 1);
      READ_UINT8 (nr, tc->cnt_dropped_flag[i], 1);
      READ_UINT16 (nr, tc->n_frames[i], 9);

      if (tc->full_timestamp_flag[i]) {
        tc->seconds_flag[i] = TRUE;
        READ_UINT8 (nr, tc->seconds_value[i], 6);

        tc->minutes_flag[i] = TRUE;
        READ_UINT8 (nr, tc->minutes_value[i], 6);

        tc->hours_flag[i] = TRUE;
        READ_UINT8 (nr, tc->hours_value[i], 5);
      } else {
        READ_UINT8 (nr, tc->seconds_flag[i], 1);
        if (tc->seconds_flag[i]) {
          READ_UINT8 (nr, tc->seconds_value[i], 6);
          READ_UINT8 (nr, tc->minutes_flag[i], 1);
          if (tc->minutes_flag[i]) {
            READ_UINT8 (nr, tc->minutes_value[i], 6);
            READ_UINT8 (nr, tc->hours_flag[i], 1);
            if (tc->hours_flag[i]) {
              READ_UINT8 (nr, tc->hours_value[i], 5);
            }
          }
        }
      }
    }

    READ_UINT8 (nr, tc->time_offset_length[i], 5);

    if (tc->time_offset_length[i] > 0)
      READ_UINT32 (nr, tc->time_offset_value[i], tc->time_offset_length[i]);
  }

  return GST_H265_PARSER_OK;

error:
  GST_WARNING ("error parsing \"Time code\"");
  return GST_H265_PARSER_ERROR;
}

static GstH265ParserResult
gst_h265_parser_parse_mastering_display_colour_volume (GstH265Parser * parser,
    GstH265MasteringDisplayColourVolume * mdcv, NalReader * nr)
{
  guint i;

  GST_DEBUG ("parsing \"Mastering display colour volume\"");

  for (i = 0; i < 3; i++) {
    READ_UINT16 (nr, mdcv->display_primaries_x[i], 16);
    READ_UINT16 (nr, mdcv->display_primaries_y[i], 16);
  }

  READ_UINT16 (nr, mdcv->white_point_x, 16);
  READ_UINT16 (nr, mdcv->white_point_y, 16);
  READ_UINT32 (nr, mdcv->max_display_mastering_luminance, 32);
  READ_UINT32 (nr, mdcv->min_display_mastering_luminance, 32);

  return GST_H265_PARSER_OK;

error:
  GST_WARNING ("error parsing \"Mastering display colour volume\"");
  return GST_H265_PARSER_ERROR;
}

static GstH265ParserResult
gst_h265_parser_parse_content_light_level_info (GstH265Parser * parser,
    GstH265ContentLightLevel * cll, NalReader * nr)
{
  GST_DEBUG ("parsing \"Content light level\"");

  READ_UINT16 (nr, cll->max_content_light_level, 16);
  READ_UINT16 (nr, cll->max_pic_average_light_level, 16);

  return GST_H265_PARSER_OK;

error:
  GST_WARNING ("error parsing \"Content light level\"");
  return GST_H265_PARSER_ERROR;
}

/******** API *************/

/**
 * gst_h265_parser_new:
 *
 * Creates a new #GstH265Parser. It should be freed with
 * gst_h265_parser_free after use.
 *
 * Returns: a new #GstH265Parser
 */
GstH265Parser *
gst_h265_parser_new (void)
{
  GstH265Parser *parser;

  parser = g_new0 (GstH265Parser, 1);

  return parser;
}

/**
 * gst_h265_parser_free:
 * @parser: the #GstH265Parser to free
 *
 * Frees @parser and sets it to %NULL
 */
void
gst_h265_parser_free (GstH265Parser * parser)
{
  g_free (parser);
  parser = NULL;
}

/**
 * gst_h265_parser_identify_nalu_unchecked:
 * @parser: a #GstH265Parser
 * @data: The data to parse
 * @offset: the offset from which to parse @data
 * @size: the size of @data
 * @nalu: The #GstH265NalUnit where to store parsed nal headers
 *
 * Parses @data and fills @nalu from the next nalu data from @data.
 *
 * This differs from @gst_h265_parser_identify_nalu in that it doesn't
 * check whether the packet is complete or not.
 *
 * Note: Only use this function if you already know the provided @data
 * is a complete NALU, else use @gst_h265_parser_identify_nalu.
 *
 * Returns: a #GstH265ParserResult
 */
GstH265ParserResult
gst_h265_parser_identify_nalu_unchecked (GstH265Parser * parser,
    const guint8 * data, guint offset, gsize size, GstH265NalUnit * nalu)
{
  gint off1;

  memset (nalu, 0, sizeof (*nalu));

  if (size < offset + 4) {
    GST_DEBUG ("Can't parse, buffer has too small size %" G_GSIZE_FORMAT
        ", offset %u", size, offset);
    return GST_H265_PARSER_ERROR;
  }

  off1 = scan_for_start_codes (data + offset, size - offset);

  if (off1 < 0) {
    GST_DEBUG ("No start code prefix in this buffer");
    return GST_H265_PARSER_NO_NAL;
  }

  nalu->sc_offset = offset + off1;

  /* The scanner ensures one byte passed the start code but to
   * identify an HEVC NAL, we need 2. */
  if (size - nalu->sc_offset - 3 < 2) {
    GST_DEBUG ("Not enough bytes after start code to identify");
    return GST_H265_PARSER_NO_NAL;
  }

  /* sc might have 2 or 3 0-bytes */
  if (nalu->sc_offset > 0 && data[nalu->sc_offset - 1] == 00)
    nalu->sc_offset--;

  nalu->offset = offset + off1 + 3;
  nalu->data = (guint8 *) data;
  nalu->size = size - nalu->offset;

  if (!gst_h265_parse_nalu_header (nalu)) {
    GST_WARNING ("error parsing \"NAL unit header\"");
    nalu->size = 0;
    return GST_H265_PARSER_BROKEN_DATA;
  }

  nalu->valid = TRUE;

  if (nalu->type == GST_H265_NAL_EOS || nalu->type == GST_H265_NAL_EOB) {
    GST_DEBUG ("end-of-seq or end-of-stream nal found");
    nalu->size = 2;
    return GST_H265_PARSER_OK;
  }

  return GST_H265_PARSER_OK;
}

/**
 * gst_h265_parser_identify_nalu:
 * @parser: a #GstH265Parser
 * @data: The data to parse
 * @offset: the offset from which to parse @data
 * @size: the size of @data
 * @nalu: The #GstH265NalUnit where to store parsed nal headers
 *
 * Parses @data and fills @nalu from the next nalu data from @data
 *
 * Returns: a #GstH265ParserResult
 */
GstH265ParserResult
gst_h265_parser_identify_nalu (GstH265Parser * parser,
    const guint8 * data, guint offset, gsize size, GstH265NalUnit * nalu)
{
  GstH265ParserResult res;
  gint off2;

  res =
      gst_h265_parser_identify_nalu_unchecked (parser, data, offset, size,
      nalu);

  if (res != GST_H265_PARSER_OK)
    goto beach;

  /* The two NALs are exactly 2 bytes size and are placed at the end of an AU,
   * there is no need to wait for the following */
  if (nalu->type == GST_H265_NAL_EOS || nalu->type == GST_H265_NAL_EOB)
    goto beach;

  off2 = scan_for_start_codes (data + nalu->offset, size - nalu->offset);
  if (off2 < 0) {
    GST_DEBUG ("Nal start %d, No end found", nalu->offset);

    return GST_H265_PARSER_NO_NAL_END;
  }

  /* Callers assumes that enough data will available to identify the next NAL,
   * but scan_for_start_codes() only ensure 1 extra byte is available. Ensure
   * we have the required two header bytes (3 bytes start code and 2 byte
   * header). */
  if (size - (nalu->offset + off2) < 5) {
    GST_DEBUG ("Not enough bytes identify the next NAL.");
    return GST_H265_PARSER_NO_NAL_END;
  }

  /* Mini performance improvement:
   * We could have a way to store how many 0s were skipped to avoid
   * parsing them again on the next NAL */
  while (off2 > 0 && data[nalu->offset + off2 - 1] == 00)
    off2--;

  nalu->size = off2;
  if (nalu->size < 3)
    return GST_H265_PARSER_BROKEN_DATA;

  GST_DEBUG ("Complete nal found. Off: %d, Size: %d", nalu->offset, nalu->size);

beach:
  return res;
}

/**
 * gst_h265_parser_identify_nalu_hevc:
 * @parser: a #GstH265Parser
 * @data: The data to parse, must be the beging of the Nal unit
 * @offset: the offset from which to parse @data
 * @size: the size of @data
 * @nal_length_size: the size in bytes of the HEVC nal length prefix.
 * @nalu: The #GstH265NalUnit where to store parsed nal headers
 *
 * Parses @data and sets @nalu.
 *
 * Returns: a #GstH265ParserResult
 */
GstH265ParserResult
gst_h265_parser_identify_nalu_hevc (GstH265Parser * parser,
    const guint8 * data, guint offset, gsize size, guint8 nal_length_size,
    GstH265NalUnit * nalu)
{
  GstBitReader br;

  memset (nalu, 0, sizeof (*nalu));

  /* Would overflow guint below otherwise: the callers needs to ensure that
   * this never happens */
  if (offset > G_MAXUINT32 - nal_length_size) {
    GST_WARNING ("offset + nal_length_size overflow");
    nalu->size = 0;
    return GST_H265_PARSER_BROKEN_DATA;
  }

  if (size < offset + nal_length_size) {
    GST_DEBUG ("Can't parse, buffer has too small size %" G_GSIZE_FORMAT
        ", offset %u", size, offset);
    return GST_H265_PARSER_ERROR;
  }

  size = size - offset;
  gst_bit_reader_init (&br, data + offset, size);

  nalu->size = gst_bit_reader_get_bits_uint32_unchecked (&br,
      nal_length_size * 8);
  nalu->sc_offset = offset;
  nalu->offset = offset + nal_length_size;

  if (nalu->size > G_MAXUINT32 - nal_length_size) {
    GST_WARNING ("NALU size + nal_length_size overflow");
    nalu->size = 0;
    return GST_H265_PARSER_BROKEN_DATA;
  }

  if (size < (gsize) nalu->size + nal_length_size) {
    nalu->size = 0;

    return GST_H265_PARSER_NO_NAL_END;
  }

  nalu->data = (guint8 *) data;

  if (!gst_h265_parse_nalu_header (nalu)) {
    GST_WARNING ("error parsing \"NAL unit header\"");
    nalu->size = 0;
    return GST_H265_PARSER_BROKEN_DATA;
  }

  if (nalu->size < 2)
    return GST_H265_PARSER_BROKEN_DATA;

  nalu->valid = TRUE;

  return GST_H265_PARSER_OK;
}

/**
 * gst_h265_parser_identify_and_split_nalu_hevc:
 * @parser: a #GstH265Parser
 * @data: The data to parse, must be the beging of the Nal unit
 * @offset: the offset from which to parse @data
 * @size: the size of @data
 * @nal_length_size: the size in bytes of the HEVC nal length prefix.
 * @nalus: a caller allocated GArray of #GstH265NalUnit where to store parsed nal headers
 * @consumed: the size of consumed bytes
 *
 * Parses @data for packetized (e.g., hvc1/hev1) bitstream and
 * sets @nalus. In addition to nal identifying process,
 * this method scans start-code prefix to split malformed packet into
 * actual nal chunks.
 *
 * Returns: a #GstH265ParserResult
 *
 * Since: 1.22
 */
GstH265ParserResult
gst_h265_parser_identify_and_split_nalu_hevc (GstH265Parser * parser,
    const guint8 * data, guint offset, gsize size, guint8 nal_length_size,
    GArray * nalus, gsize * consumed)
{
  GstBitReader br;
  guint nalu_size;
  guint remaining;
  guint off;
  guint sc_size;

  g_return_val_if_fail (data != NULL, GST_H265_PARSER_ERROR);
  g_return_val_if_fail (nalus != NULL, GST_H265_PARSER_ERROR);
  g_return_val_if_fail (nal_length_size > 0 && nal_length_size < 5,
      GST_H265_PARSER_ERROR);

  g_array_set_size (nalus, 0);

  if (consumed)
    *consumed = 0;

  /* Would overflow guint below otherwise: the callers needs to ensure that
   * this never happens */
  if (offset > G_MAXUINT32 - nal_length_size) {
    GST_WARNING ("offset + nal_length_size overflow");
    return GST_H265_PARSER_BROKEN_DATA;
  }

  if (size < offset + nal_length_size) {
    GST_DEBUG ("Can't parse, buffer has too small size %" G_GSIZE_FORMAT
        ", offset %u", size, offset);
    return GST_H265_PARSER_ERROR;
  }

  /* Read nal unit size and unwrap the size field */
  gst_bit_reader_init (&br, data + offset, size - offset);
  nalu_size = gst_bit_reader_get_bits_uint32_unchecked (&br,
      nal_length_size * 8);

  if (nalu_size < 2) {
    GST_WARNING ("too small nal size %d", nalu_size);
    return GST_H265_PARSER_BROKEN_DATA;
  }

  if (size < (gsize) nalu_size + nal_length_size) {
    GST_WARNING ("larger nalu size %d than data size %" G_GSIZE_FORMAT,
        nalu_size + nal_length_size, size);
    return GST_H265_PARSER_BROKEN_DATA;
  }

  if (consumed)
    *consumed = nalu_size + nal_length_size;

  off = offset + nal_length_size;
  remaining = nalu_size;
  sc_size = nal_length_size;

  /* Drop trailing start-code since it will not be scanned */
  if (remaining >= 3) {
    if (data[off + remaining - 1] == 0x01 && data[off + remaining - 2] == 0x00
        && data[off + remaining - 3] == 0x00) {
      remaining -= 3;

      /* 4 bytes start-code */
      if (remaining > 0 && data[off + remaining - 1] == 0x00)
        remaining--;
    }
  }

  /* Looping to split malformed nal units. nal-length field was dropped above
   * so expected bitstream structure are:
   *
   * <complete nalu>
   * | nalu |
   * sc scan result will be -1 and handled in CONDITION-A
   *
   * <nalu with startcode prefix>
   * | SC | nalu |
   * Hit CONDITION-C first then terminated in CONDITION-A
   *
   * <first nal has no startcode but others have>
   * | nalu | SC | nalu | ...
   * CONDITION-B handles those cases
   */
  do {
    GstH265NalUnit nalu;
    gint sc_offset = -1;
    guint skip_size = 0;

    memset (&nalu, 0, sizeof (GstH265NalUnit));

    /* startcode 3 bytes + minimum nal size 2 */
    if (remaining >= 5)
      sc_offset = scan_for_start_codes (data + off, remaining);

    if (sc_offset < 0) {
      if (remaining >= 2) {
        /* CONDITION-A */
        /* Last chunk */
        nalu.size = remaining;
        nalu.sc_offset = off - sc_size;
        nalu.offset = off;
        nalu.data = (guint8 *) data;
        nalu.valid = TRUE;

        gst_h265_parse_nalu_header (&nalu);
        g_array_append_val (nalus, nalu);
      }
      break;
    } else if ((sc_offset == 2 && data[off + sc_offset - 1] != 0)
        || sc_offset > 2) {
      /* CONDITION-B */
      /* Found trailing startcode prefix */

      nalu.size = sc_offset;
      if (data[off + sc_offset - 1] == 0) {
        /* 4 bytes start code */
        nalu.size--;
      }

      nalu.sc_offset = off - sc_size;
      nalu.offset = off;
      nalu.data = (guint8 *) data;
      nalu.valid = TRUE;

      gst_h265_parse_nalu_header (&nalu);
      g_array_append_val (nalus, nalu);
    } else {
      /* CONDITION-C */
      /* startcode located at beginning of this chunk without actual nal data.
       * skip this start code */
    }

    skip_size = sc_offset + 3;
    if (skip_size >= remaining)
      break;

    /* no more nal-length bytes but 3bytes startcode */
    sc_size = 3;
    if (sc_offset > 0 && data[off + sc_offset - 1] == 0)
      sc_size++;

    remaining -= skip_size;
    off += skip_size;
  } while (remaining >= 2);

  if (nalus->len > 0)
    return GST_H265_PARSER_OK;

  GST_WARNING ("No nal found");

  return GST_H265_PARSER_BROKEN_DATA;
}

/**
 * gst_h265_parser_parse_nal:
 * @parser: a #GstH265Parser
 * @nalu: The #GstH265NalUnit to parse
 *
 * This function should be called in the case one doesn't need to
 * parse a specific structure. It is necessary to do so to make
 * sure @parser is up to date.
 *
 * Returns: a #GstH265ParserResult
 */
GstH265ParserResult
gst_h265_parser_parse_nal (GstH265Parser * parser, GstH265NalUnit * nalu)
{
  GstH265VPS vps;
  GstH265SPS sps;
  GstH265PPS pps;

  switch (nalu->type) {
    case GST_H265_NAL_VPS:
      return gst_h265_parser_parse_vps (parser, nalu, &vps);
      break;
    case GST_H265_NAL_SPS:
      return gst_h265_parser_parse_sps (parser, nalu, &sps, FALSE);
      break;
    case GST_H265_NAL_PPS:
      return gst_h265_parser_parse_pps (parser, nalu, &pps);
  }

  return GST_H265_PARSER_OK;
}

/**
 * gst_h265_parser_parse_vps:
 * @parser: a #GstH265Parser
 * @nalu: The #GST_H265_NAL_VPS #GstH265NalUnit to parse
 * @vps: The #GstH265VPS to fill.
 *
 * Parses @data, and fills the @vps structure.
 *
 * Returns: a #GstH265ParserResult
 */
GstH265ParserResult
gst_h265_parser_parse_vps (GstH265Parser * parser, GstH265NalUnit * nalu,
    GstH265VPS * vps)
{
  GstH265ParserResult res = gst_h265_parse_vps (nalu, vps);

  if (res == GST_H265_PARSER_OK) {
    GST_DEBUG ("adding video parameter set with id: %d to array", vps->id);

    parser->vps[vps->id] = *vps;
    parser->last_vps = &parser->vps[vps->id];
  }

  return res;
}

/**
 * gst_h265_parse_vps:
 * @nalu: The #GST_H265_NAL_VPS #GstH265NalUnit to parse
 * @sps: The #GstH265VPS to fill.
 *
 * Parses @data, and fills the @vps structure.
 *
 * Returns: a #GstH265ParserResult
 */
GstH265ParserResult
gst_h265_parse_vps (GstH265NalUnit * nalu, GstH265VPS * vps)
{
  NalReader nr;
  guint i, j;

  GST_DEBUG ("parsing VPS");

  nal_reader_init (&nr, nalu->data + nalu->offset + nalu->header_bytes,
      nalu->size - nalu->header_bytes);

  memset (vps, 0, sizeof (*vps));

  vps->cprms_present_flag = 1;

  READ_UINT8 (&nr, vps->id, 4);

  READ_UINT8 (&nr, vps->base_layer_internal_flag, 1);
  READ_UINT8 (&nr, vps->base_layer_available_flag, 1);

  READ_UINT8 (&nr, vps->max_layers_minus1, 6);
  READ_UINT8 (&nr, vps->max_sub_layers_minus1, 3);
  CHECK_ALLOWED (vps->max_sub_layers_minus1, 0, 6);
  READ_UINT8 (&nr, vps->temporal_id_nesting_flag, 1);

  /* skip reserved_0xffff_16bits */
  if (!nal_reader_skip (&nr, 16))
    goto error;

  if (!gst_h265_parse_profile_tier_level (&vps->profile_tier_level, &nr,
          vps->max_sub_layers_minus1))
    goto error;

  READ_UINT8 (&nr, vps->sub_layer_ordering_info_present_flag, 1);

  for (i =
      (vps->sub_layer_ordering_info_present_flag ? 0 :
          vps->max_sub_layers_minus1); i <= vps->max_sub_layers_minus1; i++) {
    READ_UE_MAX (&nr, vps->max_dec_pic_buffering_minus1[i], G_MAXUINT32 - 1);
    READ_UE_MAX (&nr, vps->max_num_reorder_pics[i],
        vps->max_dec_pic_buffering_minus1[i]);
    READ_UE_MAX (&nr, vps->max_latency_increase_plus1[i], G_MAXUINT32 - 1);
  }
  /* setting default values if vps->sub_layer_ordering_info_present_flag is zero */
  if (!vps->sub_layer_ordering_info_present_flag && vps->max_sub_layers_minus1) {
    for (i = 0; i <= (vps->max_sub_layers_minus1 - 1); i++) {
      vps->max_dec_pic_buffering_minus1[i] =
          vps->max_dec_pic_buffering_minus1[vps->max_sub_layers_minus1];
      vps->max_num_reorder_pics[i] =
          vps->max_num_reorder_pics[vps->max_sub_layers_minus1];
      vps->max_latency_increase_plus1[i] =
          vps->max_latency_increase_plus1[vps->max_sub_layers_minus1];
    }
  }

  READ_UINT8 (&nr, vps->max_layer_id, 6);
  /* shall allow 63 */
  CHECK_ALLOWED_MAX (vps->max_layer_id, 63);

  READ_UE_MAX (&nr, vps->num_layer_sets_minus1, 1023);
  /* allowed range is 0 to 1023 */
  CHECK_ALLOWED_MAX (vps->num_layer_sets_minus1, 1023);

  for (i = 1; i <= vps->num_layer_sets_minus1; i++) {
    for (j = 0; j <= vps->max_layer_id; j++) {
      /* layer_id_included_flag[i][j] */
      /* FIXME: need to parse this when we can support parsing multi-layer info. */
      if (!nal_reader_skip (&nr, 1))
        goto error;
    }
  }

  READ_UINT8 (&nr, vps->timing_info_present_flag, 1);

  if (vps->timing_info_present_flag) {
    READ_UINT32 (&nr, vps->num_units_in_tick, 32);
    READ_UINT32 (&nr, vps->time_scale, 32);
    READ_UINT8 (&nr, vps->poc_proportional_to_timing_flag, 1);

    if (vps->poc_proportional_to_timing_flag)
      READ_UE_MAX (&nr, vps->num_ticks_poc_diff_one_minus1, G_MAXUINT32 - 1);

    READ_UE_MAX (&nr, vps->num_hrd_parameters, 1024);
    /* allowed range is
     * 0 to vps_num_layer_sets_minus1 + 1 */
    CHECK_ALLOWED_MAX (vps->num_hrd_parameters, vps->num_layer_sets_minus1 + 1);

    if (vps->num_hrd_parameters) {
      READ_UE_MAX (&nr, vps->hrd_layer_set_idx, 1023);
      /* allowed range is
       * ( vps_base_layer_internal_flag ? 0 : 1 ) to vps_num_layer_sets_minus1
       */
      CHECK_ALLOWED_MAX (vps->hrd_layer_set_idx, vps->num_layer_sets_minus1);

      if (!gst_h265_parse_hrd_parameters (&vps->hrd_params, &nr,
              vps->cprms_present_flag, vps->max_sub_layers_minus1))
        goto error;
    }

    /* FIXME: VPS can have multiple hrd parameters, and therefore hrd_params
     * should be an array (like Garray). But it also requires new _clear()
     * method for free the array in GstH265VPS whenever gst_h265_parse_vps()
     * is called. Need to work for multi-layer related parsing supporting
     *
     * FIXME: Following code is just work around to find correct
     * vps_extension position */

    /* skip the first parsed one above */
    for (i = 1; i < vps->num_hrd_parameters; i++) {
      guint16 hrd_layer_set_idx;
      guint8 cprms_present_flag;
      GstH265HRDParams hrd_params;

      READ_UE_MAX (&nr, hrd_layer_set_idx, 1023);
      CHECK_ALLOWED_MAX (hrd_layer_set_idx, vps->num_layer_sets_minus1);

      /* need parsing if (i > 1) */
      READ_UINT8 (&nr, cprms_present_flag, 1);

      if (!gst_h265_parse_hrd_parameters (&hrd_params, &nr,
              cprms_present_flag, vps->max_sub_layers_minus1))
        goto error;
    }
  }
  READ_UINT8 (&nr, vps->vps_extension, 1);
  vps->valid = TRUE;

  return GST_H265_PARSER_OK;

error:
  GST_WARNING ("error parsing \"Video parameter set\"");
  vps->valid = FALSE;
  return GST_H265_PARSER_ERROR;
}

/**
 * gst_h265_parser_parse_sps:
 * @parser: a #GstH265Parser
 * @nalu: The #GST_H265_NAL_SPS #GstH265NalUnit to parse
 * @sps: The #GstH265SPS to fill.
 * @parse_vui_params: Whether to parse the vui_params or not
 *
 * Parses @data, and fills the @sps structure.
 *
 * Returns: a #GstH265ParserResult
 */
GstH265ParserResult
gst_h265_parser_parse_sps (GstH265Parser * parser, GstH265NalUnit * nalu,
    GstH265SPS * sps, gboolean parse_vui_params)
{
  GstH265ParserResult res =
      gst_h265_parse_sps (parser, nalu, sps, parse_vui_params);

  if (res == GST_H265_PARSER_OK) {
    GST_DEBUG ("adding sequence parameter set with id: %d to array", sps->id);

    parser->sps[sps->id] = *sps;
    parser->last_sps = &parser->sps[sps->id];
  }

  return res;
}

/**
 * gst_h265_parse_sps:
 * parser: The #GstH265Parser
 * @nalu: The #GST_H265_NAL_SPS #GstH265NalUnit to parse
 * @sps: The #GstH265SPS to fill.
 * @parse_vui_params: Whether to parse the vui_params or not
 *
 * Parses @data, and fills the @sps structure.
 *
 * Returns: a #GstH265ParserResult
 */
GstH265ParserResult
gst_h265_parse_sps (GstH265Parser * parser, GstH265NalUnit * nalu,
    GstH265SPS * sps, gboolean parse_vui_params)
{
  NalReader nr;
  guint i;
  guint subwc[] = { 1, 2, 2, 1, 1 };
  guint subhc[] = { 1, 2, 1, 1, 1 };

  GST_DEBUG ("parsing SPS");

  nal_reader_init (&nr, nalu->data + nalu->offset + nalu->header_bytes,
      nalu->size - nalu->header_bytes);

  memset (sps, 0, sizeof (*sps));

  READ_UINT8 (&nr, sps->vps_id, 4);

  READ_UINT8 (&nr, sps->max_sub_layers_minus1, 3);
  CHECK_ALLOWED (sps->max_sub_layers_minus1, 0, 6);
  READ_UINT8 (&nr, sps->temporal_id_nesting_flag, 1);

  if (!gst_h265_parse_profile_tier_level (&sps->profile_tier_level, &nr,
          sps->max_sub_layers_minus1))
    goto error;

  READ_UE_MAX (&nr, sps->id, GST_H265_MAX_SPS_COUNT - 1);

  READ_UE_MAX (&nr, sps->chroma_format_idc, 3);
  if (sps->chroma_format_idc == 3)
    READ_UINT8 (&nr, sps->separate_colour_plane_flag, 1);

  READ_UE_ALLOWED (&nr, sps->pic_width_in_luma_samples, 1, 16888);
  READ_UE_ALLOWED (&nr, sps->pic_height_in_luma_samples, 1, 16888);

  READ_UINT8 (&nr, sps->conformance_window_flag, 1);
  if (sps->conformance_window_flag) {
    READ_UE (&nr, sps->conf_win_left_offset);
    READ_UE (&nr, sps->conf_win_right_offset);
    READ_UE (&nr, sps->conf_win_top_offset);
    READ_UE (&nr, sps->conf_win_bottom_offset);
  }

  READ_UE_MAX (&nr, sps->bit_depth_luma_minus8, 6);
  READ_UE_MAX (&nr, sps->bit_depth_chroma_minus8, 6);
  READ_UE_MAX (&nr, sps->log2_max_pic_order_cnt_lsb_minus4, 12);

  READ_UINT8 (&nr, sps->sub_layer_ordering_info_present_flag, 1);
  for (i =
      (sps->sub_layer_ordering_info_present_flag ? 0 :
          sps->max_sub_layers_minus1); i <= sps->max_sub_layers_minus1; i++) {
    READ_UE_MAX (&nr, sps->max_dec_pic_buffering_minus1[i], 16);
    READ_UE_MAX (&nr, sps->max_num_reorder_pics[i],
        sps->max_dec_pic_buffering_minus1[i]);
    READ_UE_MAX (&nr, sps->max_latency_increase_plus1[i], G_MAXUINT32 - 1);
  }
  /* setting default values if sps->sub_layer_ordering_info_present_flag is zero */
  if (!sps->sub_layer_ordering_info_present_flag && sps->max_sub_layers_minus1) {
    for (i = 0; i <= (sps->max_sub_layers_minus1 - 1); i++) {
      sps->max_dec_pic_buffering_minus1[i] =
          sps->max_dec_pic_buffering_minus1[sps->max_sub_layers_minus1];
      sps->max_num_reorder_pics[i] =
          sps->max_num_reorder_pics[sps->max_sub_layers_minus1];
      sps->max_latency_increase_plus1[i] =
          sps->max_latency_increase_plus1[sps->max_sub_layers_minus1];
    }
  }

  /* The limits are calculted based on the profile_tier_level constraint
   * in Annex-A: CtbLog2SizeY = 4 to 6 */
  READ_UE_MAX (&nr, sps->log2_min_luma_coding_block_size_minus3, 3);
  READ_UE_MAX (&nr, sps->log2_diff_max_min_luma_coding_block_size, 6);
  READ_UE_MAX (&nr, sps->log2_min_transform_block_size_minus2, 3);
  READ_UE_MAX (&nr, sps->log2_diff_max_min_transform_block_size, 3);
  READ_UE_MAX (&nr, sps->max_transform_hierarchy_depth_inter, 4);
  READ_UE_MAX (&nr, sps->max_transform_hierarchy_depth_intra, 4);

  READ_UINT8 (&nr, sps->scaling_list_enabled_flag, 1);
  if (sps->scaling_list_enabled_flag) {
    READ_UINT8 (&nr, sps->scaling_list_data_present_flag, 1);

    if (sps->scaling_list_data_present_flag)
      if (!gst_h265_parser_parse_scaling_lists (&nr, &sps->scaling_list, FALSE))
        goto error;
  }

  READ_UINT8 (&nr, sps->amp_enabled_flag, 1);
  READ_UINT8 (&nr, sps->sample_adaptive_offset_enabled_flag, 1);
  READ_UINT8 (&nr, sps->pcm_enabled_flag, 1);

  if (sps->pcm_enabled_flag) {
    READ_UINT8 (&nr, sps->pcm_sample_bit_depth_luma_minus1, 4);
    READ_UINT8 (&nr, sps->pcm_sample_bit_depth_chroma_minus1, 4);
    READ_UE_MAX (&nr, sps->log2_min_pcm_luma_coding_block_size_minus3, 2);
    READ_UE_MAX (&nr, sps->log2_diff_max_min_pcm_luma_coding_block_size, 2);
    READ_UINT8 (&nr, sps->pcm_loop_filter_disabled_flag, 1);
  }

  READ_UE_MAX (&nr, sps->num_short_term_ref_pic_sets, 64);
  for (i = 0; i < sps->num_short_term_ref_pic_sets; i++)
    if (!gst_h265_parser_parse_short_term_ref_pic_sets
        (&sps->short_term_ref_pic_set[i], &nr, i, sps))
      goto error;

  READ_UINT8 (&nr, sps->long_term_ref_pics_present_flag, 1);
  if (sps->long_term_ref_pics_present_flag) {
    READ_UE_MAX (&nr, sps->num_long_term_ref_pics_sps, 32);
    for (i = 0; i < sps->num_long_term_ref_pics_sps; i++) {
      READ_UINT16 (&nr, sps->lt_ref_pic_poc_lsb_sps[i],
          sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
      READ_UINT8 (&nr, sps->used_by_curr_pic_lt_sps_flag[i], 1);
    }
  }

  READ_UINT8 (&nr, sps->temporal_mvp_enabled_flag, 1);
  READ_UINT8 (&nr, sps->strong_intra_smoothing_enabled_flag, 1);
  READ_UINT8 (&nr, sps->vui_parameters_present_flag, 1);

  if (sps->vui_parameters_present_flag && parse_vui_params)
    if (!gst_h265_parse_vui_parameters (sps, &nr))
      goto error;

  READ_UINT8 (&nr, sps->sps_extension_flag, 1);

  if (sps->sps_extension_flag) {
    READ_UINT8 (&nr, sps->sps_range_extension_flag, 1);
    READ_UINT8 (&nr, sps->sps_multilayer_extension_flag, 1);
    READ_UINT8 (&nr, sps->sps_3d_extension_flag, 1);
    READ_UINT8 (&nr, sps->sps_scc_extension_flag, 1);
    READ_UINT8 (&nr, sps->sps_extension_4bits, 4);
  }

  if (sps->sps_range_extension_flag) {
    READ_UINT8 (&nr,
        sps->sps_extension_params.transform_skip_rotation_enabled_flag, 1);
    READ_UINT8 (&nr,
        sps->sps_extension_params.transform_skip_context_enabled_flag, 1);
    READ_UINT8 (&nr, sps->sps_extension_params.implicit_rdpcm_enabled_flag, 1);
    READ_UINT8 (&nr, sps->sps_extension_params.explicit_rdpcm_enabled_flag, 1);
    READ_UINT8 (&nr,
        sps->sps_extension_params.extended_precision_processing_flag, 1);
    READ_UINT8 (&nr, sps->sps_extension_params.intra_smoothing_disabled_flag,
        1);
    READ_UINT8 (&nr,
        sps->sps_extension_params.high_precision_offsets_enabled_flag, 1);
    READ_UINT8 (&nr,
        sps->sps_extension_params.persistent_rice_adaptation_enabled_flag, 1);
    READ_UINT8 (&nr,
        sps->sps_extension_params.cabac_bypass_alignment_enabled_flag, 1);
  }

  if (sps->sps_multilayer_extension_flag) {
    GST_WARNING ("do not support multilayer extension, skip all"
        " remaining bits");
    goto done;
  }
  if (sps->sps_3d_extension_flag) {
    GST_WARNING ("do not support 3d extension, skip all remaining bits");
    goto done;
  }

  if (sps->sps_scc_extension_flag) {
    READ_UINT8 (&nr,
        sps->sps_scc_extension_params.sps_curr_pic_ref_enabled_flag, 1);
    READ_UINT8 (&nr, sps->sps_scc_extension_params.palette_mode_enabled_flag,
        1);
    if (sps->sps_scc_extension_params.palette_mode_enabled_flag) {
      READ_UE_MAX (&nr, sps->sps_scc_extension_params.palette_max_size, 64);
      READ_UE_MAX (&nr,
          sps->sps_scc_extension_params.delta_palette_max_predictor_size,
          128 - sps->sps_scc_extension_params.palette_max_size);

      READ_UINT8 (&nr,
          sps->
          sps_scc_extension_params.sps_palette_predictor_initializers_present_flag,
          1);
      if (sps->
          sps_scc_extension_params.sps_palette_predictor_initializers_present_flag)
      {
        guint comp;
        READ_UE_MAX (&nr,
            sps->
            sps_scc_extension_params.sps_num_palette_predictor_initializer_minus1,
            sps->sps_scc_extension_params.palette_max_size +
            sps->sps_scc_extension_params.delta_palette_max_predictor_size - 1);

        for (comp = 0; comp < (sps->chroma_format_idc == 0 ? 1 : 3); comp++) {
          guint num_bits;
          guint num =
              sps->
              sps_scc_extension_params.sps_num_palette_predictor_initializer_minus1
              + 1;

          num_bits = (comp == 0 ? sps->bit_depth_luma_minus8 + 8 :
              sps->bit_depth_chroma_minus8 + 8);
          for (i = 0; i < num; i++)
            READ_UINT32 (&nr,
                sps->sps_scc_extension_params.sps_palette_predictor_initializer
                [comp]
                [i], num_bits);
        }
      }
    }

    READ_UINT8 (&nr,
        sps->sps_scc_extension_params.motion_vector_resolution_control_idc, 2);
    READ_UINT8 (&nr,
        sps->sps_scc_extension_params.intra_boundary_filtering_disabled_flag,
        1);
  }

done:
  /* calculate ChromaArrayType */
  if (!sps->separate_colour_plane_flag)
    sps->chroma_array_type = sps->chroma_format_idc;

  /* Calculate  width and height */
  sps->width = sps->pic_width_in_luma_samples;
  sps->height = sps->pic_height_in_luma_samples;
  if (sps->width < 0 || sps->height < 0) {
    GST_WARNING ("invalid width/height in SPS");
    goto error;
  }

  if (sps->conformance_window_flag) {
    const guint crop_unit_x = subwc[sps->chroma_format_idc];
    const guint crop_unit_y = subhc[sps->chroma_format_idc];

    sps->crop_rect_width = sps->width -
        (sps->conf_win_left_offset + sps->conf_win_right_offset) * crop_unit_x;
    sps->crop_rect_height = sps->height -
        (sps->conf_win_top_offset + sps->conf_win_bottom_offset) * crop_unit_y;
    sps->crop_rect_x = sps->conf_win_left_offset * crop_unit_x;
    sps->crop_rect_y = sps->conf_win_top_offset * crop_unit_y;

    GST_LOG ("crop_rectangle x=%u y=%u width=%u, height=%u", sps->crop_rect_x,
        sps->crop_rect_y, sps->crop_rect_width, sps->crop_rect_height);
  }

  sps->fps_num = 0;
  sps->fps_den = 1;

  sps->valid = TRUE;

  return GST_H265_PARSER_OK;

error:
  GST_WARNING ("error parsing \"Sequence parameter set\"");
  sps->valid = FALSE;
  return GST_H265_PARSER_ERROR;
}

/**
 * gst_h265_parse_pps:
 * @parser: a #GstH265Parser
 * @nalu: The #GST_H265_NAL_PPS #GstH265NalUnit to parse
 * @pps: The #GstH265PPS to fill.
 *
 * Parses @data, and fills the @pps structure.
 *
 * Returns: a #GstH265ParserResult
 */
GstH265ParserResult
gst_h265_parse_pps (GstH265Parser * parser, GstH265NalUnit * nalu,
    GstH265PPS * pps)
{
  guint32 MaxBitDepthY, MaxBitDepthC;
  NalReader nr;
  guint8 i;

  GST_DEBUG ("parsing PPS");

  nal_reader_init (&nr, nalu->data + nalu->offset + nalu->header_bytes,
      nalu->size - nalu->header_bytes);

  memset (pps, 0, sizeof (*pps));

  READ_UE_MAX (&nr, pps->id, GST_H265_MAX_PPS_COUNT - 1);
  READ_UE_MAX (&nr, pps->sps_id, GST_H265_MAX_SPS_COUNT - 1);

  /* set default values for fields that might not be present in the bitstream
     and have valid defaults */
  pps->uniform_spacing_flag = 1;
  pps->loop_filter_across_tiles_enabled_flag = 1;

  READ_UINT8 (&nr, pps->dependent_slice_segments_enabled_flag, 1);
  READ_UINT8 (&nr, pps->output_flag_present_flag, 1);
  READ_UINT8 (&nr, pps->num_extra_slice_header_bits, 3);
  READ_UINT8 (&nr, pps->sign_data_hiding_enabled_flag, 1);
  READ_UINT8 (&nr, pps->cabac_init_present_flag, 1);

  READ_UE_MAX (&nr, pps->num_ref_idx_l0_default_active_minus1, 14);
  READ_UE_MAX (&nr, pps->num_ref_idx_l1_default_active_minus1, 14);

  /* The value of init_qp_minus26 shall be in the range of
   * −( 26 + QpBdOffsetY ) to +25, inclusive.
   * QpBdOffsetY = 6 * bit_depth_luma_minus8 (7-5)
   * and bit_depth_luma_minus8 shall be in the range of 0 to 8, inclusive.
   * so the minimum possible value of init_qp_minus26 is -(26 + 6*8) */
  READ_SE_ALLOWED (&nr, pps->init_qp_minus26, -(26 + 6 * 8), 25);

  READ_UINT8 (&nr, pps->constrained_intra_pred_flag, 1);
  READ_UINT8 (&nr, pps->transform_skip_enabled_flag, 1);

  READ_UINT8 (&nr, pps->cu_qp_delta_enabled_flag, 1);
  if (pps->cu_qp_delta_enabled_flag) {
    READ_UE_MAX (&nr, pps->diff_cu_qp_delta_depth, 6);
  }

  READ_SE_ALLOWED (&nr, pps->cb_qp_offset, -12, 12);
  READ_SE_ALLOWED (&nr, pps->cr_qp_offset, -12, 12);

  READ_UINT8 (&nr, pps->slice_chroma_qp_offsets_present_flag, 1);
  READ_UINT8 (&nr, pps->weighted_pred_flag, 1);
  READ_UINT8 (&nr, pps->weighted_bipred_flag, 1);
  READ_UINT8 (&nr, pps->transquant_bypass_enabled_flag, 1);
  READ_UINT8 (&nr, pps->tiles_enabled_flag, 1);
  READ_UINT8 (&nr, pps->entropy_coding_sync_enabled_flag, 1);

  if (pps->tiles_enabled_flag) {
    GstH265SPS *sps;
    guint32 CtbSizeY, MinCbLog2SizeY, CtbLog2SizeY;

    sps = gst_h265_parser_get_sps (parser, pps->sps_id);
    if (!sps) {
      GST_WARNING
          ("couldn't find associated sequence parameter set with id: %d",
          pps->sps_id);
      return GST_H265_PARSER_BROKEN_LINK;
    }

    MinCbLog2SizeY = sps->log2_min_luma_coding_block_size_minus3 + 3;
    CtbLog2SizeY =
        MinCbLog2SizeY + sps->log2_diff_max_min_luma_coding_block_size;
    CtbSizeY = 1 << CtbLog2SizeY;
    pps->PicHeightInCtbsY =
        ceil ((gdouble) sps->pic_height_in_luma_samples / (gdouble) CtbSizeY);
    pps->PicWidthInCtbsY =
        ceil ((gdouble) sps->pic_width_in_luma_samples / (gdouble) CtbSizeY);

    READ_UE_ALLOWED (&nr,
        pps->num_tile_columns_minus1, 0, pps->PicWidthInCtbsY - 1);
    READ_UE_ALLOWED (&nr,
        pps->num_tile_rows_minus1, 0, pps->PicHeightInCtbsY - 1);

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

    READ_UINT8 (&nr, pps->uniform_spacing_flag, 1);
    /* 6.5.1, 6-4, 6-5, 7.4.3.3.1 */
    if (pps->uniform_spacing_flag) {
      guint8 num_col = pps->num_tile_columns_minus1 + 1;
      guint8 num_row = pps->num_tile_rows_minus1 + 1;
      for (i = 0; i < num_col; i++) {
        pps->column_width_minus1[i] =
            ((i + 1) * pps->PicWidthInCtbsY / num_col
            - i * pps->PicWidthInCtbsY / num_col) - 1;
      }
      for (i = 0; i < num_row; i++) {
        pps->row_height_minus1[i] =
            ((i + 1) * pps->PicHeightInCtbsY / num_row
            - i * pps->PicHeightInCtbsY / num_row) - 1;
      }
    } else {
      pps->column_width_minus1[pps->num_tile_columns_minus1] =
          pps->PicWidthInCtbsY - 1;
      for (i = 0; i < pps->num_tile_columns_minus1; i++) {
        READ_UE (&nr, pps->column_width_minus1[i]);
        pps->column_width_minus1[pps->num_tile_columns_minus1] -=
            (pps->column_width_minus1[i] + 1);
      }

      pps->row_height_minus1[pps->num_tile_rows_minus1] =
          pps->PicHeightInCtbsY - 1;
      for (i = 0; i < pps->num_tile_rows_minus1; i++) {
        READ_UE (&nr, pps->row_height_minus1[i]);
        pps->row_height_minus1[pps->num_tile_rows_minus1] -=
            (pps->row_height_minus1[i] + 1);
      }
    }
    READ_UINT8 (&nr, pps->loop_filter_across_tiles_enabled_flag, 1);
  }

  READ_UINT8 (&nr, pps->loop_filter_across_slices_enabled_flag, 1);

  READ_UINT8 (&nr, pps->deblocking_filter_control_present_flag, 1);
  if (pps->deblocking_filter_control_present_flag) {
    READ_UINT8 (&nr, pps->deblocking_filter_override_enabled_flag, 1);

    READ_UINT8 (&nr, pps->deblocking_filter_disabled_flag, 1);
    if (!pps->deblocking_filter_disabled_flag) {
      READ_SE_ALLOWED (&nr, pps->beta_offset_div2, -6, 6);
      READ_SE_ALLOWED (&nr, pps->tc_offset_div2, -6, +6);
    }
  }

  READ_UINT8 (&nr, pps->scaling_list_data_present_flag, 1);
  if (pps->scaling_list_data_present_flag)
    if (!gst_h265_parser_parse_scaling_lists (&nr, &pps->scaling_list, FALSE))
      goto error;

  READ_UINT8 (&nr, pps->lists_modification_present_flag, 1);
  READ_UE_MAX (&nr, pps->log2_parallel_merge_level_minus2, 4);
  READ_UINT8 (&nr, pps->slice_segment_header_extension_present_flag, 1);
  READ_UINT8 (&nr, pps->pps_extension_flag, 1);

  if (pps->pps_extension_flag) {
    READ_UINT8 (&nr, pps->pps_range_extension_flag, 1);
    READ_UINT8 (&nr, pps->pps_multilayer_extension_flag, 1);
    READ_UINT8 (&nr, pps->pps_3d_extension_flag, 1);
    READ_UINT8 (&nr, pps->pps_scc_extension_flag, 1);
    READ_UINT8 (&nr, pps->pps_extension_4bits, 4);
  }

  if (pps->pps_range_extension_flag) {
    GstH265SPS *sps;

    sps = gst_h265_parser_get_sps (parser, pps->sps_id);
    if (!sps) {
      GST_WARNING
          ("couldn't find associated sequence parameter set with id: %d",
          pps->sps_id);
      return GST_H265_PARSER_BROKEN_LINK;
    }

    if (pps->transform_skip_enabled_flag)
      READ_UE (&nr,
          pps->pps_extension_params.log2_max_transform_skip_block_size_minus2);
    READ_UINT8 (&nr,
        pps->pps_extension_params.cross_component_prediction_enabled_flag, 1);
    READ_UINT8 (&nr,
        pps->pps_extension_params.chroma_qp_offset_list_enabled_flag, 1);
    if (pps->pps_extension_params.chroma_qp_offset_list_enabled_flag) {
      READ_UE_MAX (&nr,
          pps->pps_extension_params.diff_cu_chroma_qp_offset_depth,
          sps->log2_diff_max_min_luma_coding_block_size);
      READ_UE_MAX (&nr,
          pps->pps_extension_params.chroma_qp_offset_list_len_minus1, 5);
      for (i = 0;
          i <= pps->pps_extension_params.chroma_qp_offset_list_len_minus1;
          i++) {
        READ_SE_ALLOWED (&nr, pps->pps_extension_params.cb_qp_offset_list[i],
            -12, 12);
        READ_SE_ALLOWED (&nr, pps->pps_extension_params.cr_qp_offset_list[i],
            -12, 12);
      }
    }
    MaxBitDepthY =
        sps->bit_depth_luma_minus8 > 2 ? sps->bit_depth_luma_minus8 - 2 : 0;
    MaxBitDepthC =
        sps->bit_depth_chroma_minus8 > 2 ? sps->bit_depth_chroma_minus8 - 2 : 0;
    READ_UE_ALLOWED (&nr, pps->pps_extension_params.log2_sao_offset_scale_luma,
        0, MaxBitDepthY);
    READ_UE_ALLOWED (&nr,
        pps->pps_extension_params.log2_sao_offset_scale_chroma, 0,
        MaxBitDepthC);
  }

  if (pps->pps_multilayer_extension_flag) {
    GST_WARNING ("do not support multilayer extension, skip all"
        " remaining bits");
    goto done;
  }
  if (pps->pps_3d_extension_flag) {
    GST_WARNING ("do not support 3d extension, skip all remaining bits");
    goto done;
  }

  if (pps->pps_scc_extension_flag) {
    GstH265SPS *sps;

    sps = gst_h265_parser_get_sps (parser, pps->sps_id);
    if (!sps) {
      GST_WARNING
          ("couldn't find associated sequence parameter set with id: %d",
          pps->sps_id);
      return GST_H265_PARSER_BROKEN_LINK;
    }

    READ_UINT8 (&nr,
        pps->pps_scc_extension_params.pps_curr_pic_ref_enabled_flag, 1);
    READ_UINT8 (&nr,
        pps->
        pps_scc_extension_params.residual_adaptive_colour_transform_enabled_flag,
        1);
    if (pps->
        pps_scc_extension_params.residual_adaptive_colour_transform_enabled_flag)
    {
      READ_UINT8 (&nr,
          pps->pps_scc_extension_params.pps_slice_act_qp_offsets_present_flag,
          1);
      READ_SE_ALLOWED (&nr,
          pps->pps_scc_extension_params.pps_act_y_qp_offset_plus5, -7, 17);
      READ_SE_ALLOWED (&nr,
          pps->pps_scc_extension_params.pps_act_cb_qp_offset_plus5, -7, 17);
      READ_SE_ALLOWED (&nr,
          pps->pps_scc_extension_params.pps_act_cr_qp_offset_plus3, -9, 15);
    }

    READ_UINT8 (&nr,
        pps->
        pps_scc_extension_params.pps_palette_predictor_initializers_present_flag,
        1);
    if (pps->
        pps_scc_extension_params.pps_palette_predictor_initializers_present_flag)
    {
      READ_UE_MAX (&nr,
          pps->pps_scc_extension_params.pps_num_palette_predictor_initializer,
          sps->sps_scc_extension_params.palette_max_size +
          sps->sps_scc_extension_params.delta_palette_max_predictor_size);
      if (pps->pps_scc_extension_params.pps_num_palette_predictor_initializer >
          0) {
        guint comp;

        READ_UINT8 (&nr, pps->pps_scc_extension_params.monochrome_palette_flag,
            1);
        /* It is a requirement of bitstream conformance that the value of
           luma_bit_depth_entry_minus8 shall be equal to the value of
           bit_depth_luma_minus8 */
        READ_UE_ALLOWED (&nr,
            pps->pps_scc_extension_params.luma_bit_depth_entry_minus8,
            sps->bit_depth_luma_minus8, sps->bit_depth_luma_minus8);
        if (!pps->pps_scc_extension_params.monochrome_palette_flag) {
          /* It is a requirement of bitstream conformance that the value
             of chroma_bit_depth_entry_minus8 shall be equal to the value
             of bit_depth_chroma_minus8. */
          READ_UE_ALLOWED (&nr,
              pps->pps_scc_extension_params.chroma_bit_depth_entry_minus8,
              sps->bit_depth_chroma_minus8, sps->bit_depth_chroma_minus8);
        }

        for (comp = 0; comp <
            (pps->pps_scc_extension_params.monochrome_palette_flag ? 1 : 3);
            comp++) {
          guint num_bits;
          guint num =
              pps->
              pps_scc_extension_params.pps_num_palette_predictor_initializer;

          num_bits = (comp == 0 ?
              pps->pps_scc_extension_params.luma_bit_depth_entry_minus8 + 8 :
              pps->pps_scc_extension_params.chroma_bit_depth_entry_minus8 + 8);
          for (i = 0; i < num; i++)
            READ_UINT32 (&nr,
                pps->pps_scc_extension_params.pps_palette_predictor_initializer
                [comp][i], num_bits);
        }
      }
    }
  }

done:
  pps->valid = TRUE;
  return GST_H265_PARSER_OK;

error:
  GST_WARNING ("error parsing \"Picture parameter set\"");
  pps->valid = FALSE;
  return GST_H265_PARSER_ERROR;
}

/**
 * gst_h265_parser_parse_pps:
 * @parser: a #GstH265Parser
 * @nalu: The #GST_H265_NAL_PPS #GstH265NalUnit to parse
 * @pps: The #GstH265PPS to fill.
 *
 * Parses @data, and fills the @pps structure.
 *
 * Returns: a #GstH265ParserResult
 */
GstH265ParserResult
gst_h265_parser_parse_pps (GstH265Parser * parser,
    GstH265NalUnit * nalu, GstH265PPS * pps)
{
  GstH265ParserResult res = gst_h265_parse_pps (parser, nalu, pps);
  if (res == GST_H265_PARSER_OK) {
    GST_DEBUG ("adding picture parameter set with id: %d to array", pps->id);

    parser->pps[pps->id] = *pps;
    parser->last_pps = &parser->pps[pps->id];
  }

  return res;
}

static GstH265ParserResult
gst_h265_parser_fill_sps (GstH265Parser * parser, GstH265SPS * sps)
{
  GstH265VPS *vps;
  GstH265VUIParams *vui = &sps->vui_params;
  GstH265ParserResult ret = GST_H265_PARSER_OK;

  vps = gst_h265_parser_get_vps (parser, sps->vps_id);
  if (!vps) {
    GST_DEBUG ("couldn't find associated video parameter set with id: %d",
        sps->vps_id);
    return GST_H265_PARSER_BROKEN_LINK;
  }
  sps->vps = vps;

  if (vui && vui->timing_info_present_flag) {
    /* derive framerate for progressive stream if the pic_struct
     * syntax element is not present in picture timing SEI messages */
    /* Fixme: handle other cases also */
    if (vui->parsed && vui->timing_info_present_flag
        && !vui->field_seq_flag && !vui->frame_field_info_present_flag) {
      sps->fps_num = vui->time_scale;
      sps->fps_den = vui->num_units_in_tick;
      GST_LOG ("framerate %d/%d in VUI", sps->fps_num, sps->fps_den);
    }
  } else if (vps && vps->timing_info_present_flag) {
    sps->fps_num = vps->time_scale;
    sps->fps_den = vps->num_units_in_tick;
    GST_LOG ("framerate %d/%d in VPS", sps->fps_num, sps->fps_den);
  } else {
    GST_LOG ("No VUI, unknown framerate");
  }

  return ret;
}

static GstH265ParserResult
gst_h265_parser_fill_pps (GstH265Parser * parser, GstH265PPS * pps)
{
  GstH265SPS *sps;
  gint qp_bd_offset;
  guint32 CtbSizeY, MinCbLog2SizeY, CtbLog2SizeY;
  GstH265ParserResult ret = GST_H265_PARSER_OK;

  sps = gst_h265_parser_get_sps (parser, pps->sps_id);
  if (!sps) {
    GST_WARNING ("couldn't find associated sequence parameter set with id: %d",
        pps->sps_id);
    return GST_H265_PARSER_BROKEN_LINK;
  }

  ret = gst_h265_parser_fill_sps (parser, sps);
  if (ret != GST_H265_PARSER_OK) {
    GST_WARNING ("couldn't fill sps id: %d", pps->sps_id);
    return ret;
  }

  pps->sps = sps;
  qp_bd_offset = 6 * sps->bit_depth_luma_minus8;

  MinCbLog2SizeY = sps->log2_min_luma_coding_block_size_minus3 + 3;
  CtbLog2SizeY = MinCbLog2SizeY + sps->log2_diff_max_min_luma_coding_block_size;
  CtbSizeY = 1 << CtbLog2SizeY;
  pps->PicHeightInCtbsY =
      ceil ((gdouble) sps->pic_height_in_luma_samples / (gdouble) CtbSizeY);
  pps->PicWidthInCtbsY =
      ceil ((gdouble) sps->pic_width_in_luma_samples / (gdouble) CtbSizeY);

  if (pps->init_qp_minus26 < -(26 + qp_bd_offset))
    return GST_H265_PARSER_BROKEN_LINK;

  if (sps->scaling_list_enabled_flag && !sps->scaling_list_data_present_flag
      && !pps->scaling_list_data_present_flag)
    if (!gst_h265_parser_parse_scaling_lists (NULL, &pps->scaling_list, TRUE))
      return GST_H265_PARSER_BROKEN_LINK;

  if (pps->cu_qp_delta_enabled_flag)
    if (pps->diff_cu_qp_delta_depth >
        sps->log2_diff_max_min_luma_coding_block_size)
      return GST_H265_PARSER_BROKEN_LINK;

  return ret;
}

/**
 * gst_h265_parser_parse_slice_hdr:
 * @parser: a #GstH265Parser
 * @nalu: The `GST_H265_NAL_SLICE` #GstH265NalUnit to parse
 * @slice: The #GstH265SliceHdr to fill.
 *
 * Parses @data, and fills the @slice structure.
 * The resulting @slice_hdr structure shall be deallocated with
 * gst_h265_slice_hdr_free() when it is no longer needed
 *
 * Returns: a #GstH265ParserResult
 */
GstH265ParserResult
gst_h265_parser_parse_slice_hdr (GstH265Parser * parser,
    GstH265NalUnit * nalu, GstH265SliceHdr * slice)
{
  NalReader nr;
  gint pps_id;
  GstH265PPS *pps;
  GstH265SPS *sps;
  guint i;
  GstH265ShortTermRefPicSet *stRPS = NULL;
  guint32 UsedByCurrPicLt[16];
  guint32 PicSizeInCtbsY;
  gint NumPocTotalCurr = 0;
  GstH265ParserResult err;

  memset (slice, 0, sizeof (*slice));

  if (!nalu->size) {
    GST_DEBUG ("Invalid Nal Unit");
    return GST_H265_PARSER_ERROR;
  }

  nal_reader_init (&nr, nalu->data + nalu->offset + nalu->header_bytes,
      nalu->size - nalu->header_bytes);

  GST_DEBUG ("parsing \"Slice header\", slice type");

  READ_UINT8 (&nr, slice->first_slice_segment_in_pic_flag, 1);

  if (GST_H265_IS_NAL_TYPE_IRAP (nalu->type))
    READ_UINT8 (&nr, slice->no_output_of_prior_pics_flag, 1);

  READ_UE_MAX (&nr, pps_id, GST_H265_MAX_PPS_COUNT - 1);
  pps = gst_h265_parser_get_pps (parser, pps_id);
  if (!pps) {
    GST_WARNING
        ("couldn't find associated picture parameter set with id: %d", pps_id);
    return GST_H265_PARSER_BROKEN_LINK;
  }

  err = gst_h265_parser_fill_pps (parser, pps);
  if (err != GST_H265_PARSER_OK) {
    GST_WARNING ("couldn't fill pps id: %d", pps_id);
    return err;
  }

  slice->pps = pps;
  sps = pps->sps;
  if (!sps) {
    GST_WARNING
        ("couldn't find associated sequence parameter set with id: %d",
        pps->id);
    return GST_H265_PARSER_BROKEN_LINK;
  }

  PicSizeInCtbsY = pps->PicWidthInCtbsY * pps->PicHeightInCtbsY;
  /* set default values for fields that might not be present in the bitstream
   * and have valid defaults */
  slice->pic_output_flag = 1;
  slice->collocated_from_l0_flag = 1;
  slice->deblocking_filter_disabled_flag = pps->deblocking_filter_disabled_flag;
  slice->beta_offset_div2 = pps->beta_offset_div2;
  slice->tc_offset_div2 = pps->tc_offset_div2;
  slice->loop_filter_across_slices_enabled_flag =
      pps->loop_filter_across_slices_enabled_flag;

  if (!slice->first_slice_segment_in_pic_flag) {
    const guint n = gst_util_ceil_log2 (PicSizeInCtbsY);

    if (pps->dependent_slice_segments_enabled_flag)
      READ_UINT8 (&nr, slice->dependent_slice_segment_flag, 1);
    /* sice_segment_address parsing */
    READ_UINT32 (&nr, slice->segment_address, n);
  }

  if (!slice->dependent_slice_segment_flag) {
    for (i = 0; i < pps->num_extra_slice_header_bits; i++) {
      if (!nal_reader_skip (&nr, 1))
        goto error;
    }
    READ_UE_MAX (&nr, slice->type, 63);

    if (pps->output_flag_present_flag)
      READ_UINT8 (&nr, slice->pic_output_flag, 1);
    if (sps->separate_colour_plane_flag == 1)
      READ_UINT8 (&nr, slice->colour_plane_id, 2);

    if (!GST_H265_IS_NAL_TYPE_IDR (nalu->type)) {
      READ_UINT16 (&nr, slice->pic_order_cnt_lsb,
          (sps->log2_max_pic_order_cnt_lsb_minus4 + 4));

      READ_UINT8 (&nr, slice->short_term_ref_pic_set_sps_flag, 1);
      if (!slice->short_term_ref_pic_set_sps_flag) {
        guint pos = nal_reader_get_pos (&nr);
        guint epb_pos = nal_reader_get_epb_count (&nr);

        if (!gst_h265_parser_parse_short_term_ref_pic_sets
            (&slice->short_term_ref_pic_sets, &nr,
                sps->num_short_term_ref_pic_sets, sps))
          goto error;

        slice->short_term_ref_pic_set_size =
            (nal_reader_get_pos (&nr) - pos) -
            (8 * (nal_reader_get_epb_count (&nr) - epb_pos));
      } else if (sps->num_short_term_ref_pic_sets > 1) {
        const guint n = gst_util_ceil_log2 (sps->num_short_term_ref_pic_sets);
        READ_UINT8 (&nr, slice->short_term_ref_pic_set_idx, n);
        CHECK_ALLOWED_MAX (slice->short_term_ref_pic_set_idx,
            sps->num_short_term_ref_pic_sets - 1);
      }

      if (sps->long_term_ref_pics_present_flag) {
        guint32 limit;
        guint pos = nal_reader_get_pos (&nr);
        guint epb_pos = nal_reader_get_epb_count (&nr);

        if (sps->num_long_term_ref_pics_sps > 0)
          READ_UE_MAX (&nr, slice->num_long_term_sps,
              sps->num_long_term_ref_pics_sps);

        READ_UE_MAX (&nr, slice->num_long_term_pics, 16);
        limit = slice->num_long_term_sps + slice->num_long_term_pics;
        for (i = 0; i < limit; i++) {
          if (i < slice->num_long_term_sps) {
            if (sps->num_long_term_ref_pics_sps > 1) {
              const guint n =
                  gst_util_ceil_log2 (sps->num_long_term_ref_pics_sps);
              READ_UINT8 (&nr, slice->lt_idx_sps[i], n);
            }
          } else {
            READ_UINT32 (&nr, slice->poc_lsb_lt[i],
                (sps->log2_max_pic_order_cnt_lsb_minus4 + 4));
            READ_UINT8 (&nr, slice->used_by_curr_pic_lt_flag[i], 1);
          }

          /* calculate UsedByCurrPicLt */
          if (i < slice->num_long_term_sps)
            UsedByCurrPicLt[i] =
                sps->used_by_curr_pic_lt_sps_flag[slice->lt_idx_sps[i]];
          else
            UsedByCurrPicLt[i] = slice->used_by_curr_pic_lt_flag[i];
          READ_UINT8 (&nr, slice->delta_poc_msb_present_flag[i], 1);
          if (slice->delta_poc_msb_present_flag[i])
            READ_UE (&nr, slice->delta_poc_msb_cycle_lt[i]);
        }

        slice->long_term_ref_pic_set_size =
            (nal_reader_get_pos (&nr) - pos) -
            (8 * (nal_reader_get_epb_count (&nr) - epb_pos));
      }
      if (sps->temporal_mvp_enabled_flag)
        READ_UINT8 (&nr, slice->temporal_mvp_enabled_flag, 1);
    }

    if (sps->sample_adaptive_offset_enabled_flag) {
      READ_UINT8 (&nr, slice->sao_luma_flag, 1);
      if (sps->chroma_array_type)
        READ_UINT8 (&nr, slice->sao_chroma_flag, 1);
    }

    if (GST_H265_IS_B_SLICE (slice) || GST_H265_IS_P_SLICE (slice)) {
      READ_UINT8 (&nr, slice->num_ref_idx_active_override_flag, 1);

      if (slice->num_ref_idx_active_override_flag) {
        READ_UE_MAX (&nr, slice->num_ref_idx_l0_active_minus1, 14);
        if (GST_H265_IS_B_SLICE (slice))
          READ_UE_MAX (&nr, slice->num_ref_idx_l1_active_minus1, 14);
      } else {
        /*set default values */
        slice->num_ref_idx_l0_active_minus1 =
            pps->num_ref_idx_l0_default_active_minus1;
        slice->num_ref_idx_l1_active_minus1 =
            pps->num_ref_idx_l1_default_active_minus1;
      }

      /* calculate NumPocTotalCurr */
      if (slice->short_term_ref_pic_set_sps_flag)
        stRPS = &sps->short_term_ref_pic_set[slice->short_term_ref_pic_set_idx];
      else
        stRPS = &slice->short_term_ref_pic_sets;

      for (i = 0; i < stRPS->NumNegativePics; i++)
        if (stRPS->UsedByCurrPicS0[i])
          NumPocTotalCurr++;
      for (i = 0; i < stRPS->NumPositivePics; i++)
        if (stRPS->UsedByCurrPicS1[i])
          NumPocTotalCurr++;
      for (i = 0;
          i < (slice->num_long_term_sps + slice->num_long_term_pics); i++)
        if (UsedByCurrPicLt[i])
          NumPocTotalCurr++;
      slice->NumPocTotalCurr = NumPocTotalCurr;

      if (pps->lists_modification_present_flag) {
        if (NumPocTotalCurr > 1)
          if (!gst_h265_slice_parse_ref_pic_list_modification (slice, &nr,
                  NumPocTotalCurr))
            goto error;
      }

      if (GST_H265_IS_B_SLICE (slice))
        READ_UINT8 (&nr, slice->mvd_l1_zero_flag, 1);
      if (pps->cabac_init_present_flag)
        READ_UINT8 (&nr, slice->cabac_init_flag, 1);
      if (slice->temporal_mvp_enabled_flag) {
        if (GST_H265_IS_B_SLICE (slice))
          READ_UINT8 (&nr, slice->collocated_from_l0_flag, 1);

        if ((slice->collocated_from_l0_flag
                && slice->num_ref_idx_l0_active_minus1 > 0)
            || (!slice->collocated_from_l0_flag
                && slice->num_ref_idx_l1_active_minus1 > 0)) {

          /*fixme: add optimization */
          if ((GST_H265_IS_P_SLICE (slice))
              || ((GST_H265_IS_B_SLICE (slice))
                  && (slice->collocated_from_l0_flag))) {
            READ_UE_MAX (&nr, slice->collocated_ref_idx,
                slice->num_ref_idx_l0_active_minus1);
          } else if ((GST_H265_IS_B_SLICE (slice))
              && (!slice->collocated_from_l0_flag)) {
            READ_UE_MAX (&nr, slice->collocated_ref_idx,
                slice->num_ref_idx_l1_active_minus1);
          }
        }
      }
      if ((pps->weighted_pred_flag && GST_H265_IS_P_SLICE (slice)) ||
          (pps->weighted_bipred_flag && GST_H265_IS_B_SLICE (slice)))
        if (!gst_h265_slice_parse_pred_weight_table (slice, &nr))
          goto error;
      READ_UE_MAX (&nr, slice->five_minus_max_num_merge_cand, 4);

      if (sps->sps_scc_extension_params.motion_vector_resolution_control_idc
          == 2)
        READ_UINT8 (&nr, slice->use_integer_mv_flag, 1);
    }

    READ_SE_ALLOWED (&nr, slice->qp_delta, -87, 77);
    if (pps->slice_chroma_qp_offsets_present_flag) {
      READ_SE_ALLOWED (&nr, slice->cb_qp_offset, -12, 12);
      READ_SE_ALLOWED (&nr, slice->cr_qp_offset, -12, 12);
    }

    if (pps->pps_scc_extension_params.pps_slice_act_qp_offsets_present_flag) {
      READ_SE_ALLOWED (&nr, slice->slice_act_y_qp_offset, -12, 12);
      READ_SE_ALLOWED (&nr, slice->slice_act_cb_qp_offset, -12, 12);
      READ_SE_ALLOWED (&nr, slice->slice_act_cr_qp_offset, -12, 12);
    }

    if (pps->pps_extension_params.chroma_qp_offset_list_enabled_flag)
      READ_UINT8 (&nr, slice->cu_chroma_qp_offset_enabled_flag, 1);

    if (pps->deblocking_filter_override_enabled_flag)
      READ_UINT8 (&nr, slice->deblocking_filter_override_flag, 1);
    if (slice->deblocking_filter_override_flag) {
      READ_UINT8 (&nr, slice->deblocking_filter_disabled_flag, 1);
      if (!slice->deblocking_filter_disabled_flag) {
        READ_SE_ALLOWED (&nr, slice->beta_offset_div2, -6, 6);
        READ_SE_ALLOWED (&nr, slice->tc_offset_div2, -6, 6);
      }
    }

    if (pps->loop_filter_across_slices_enabled_flag &&
        (slice->sao_luma_flag || slice->sao_chroma_flag ||
            !slice->deblocking_filter_disabled_flag))
      READ_UINT8 (&nr, slice->loop_filter_across_slices_enabled_flag, 1);
  }

  if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag) {
    guint32 offset_max;

    if (!pps->tiles_enabled_flag && pps->entropy_coding_sync_enabled_flag)
      offset_max = pps->PicHeightInCtbsY - 1;
    else if (pps->tiles_enabled_flag && !pps->entropy_coding_sync_enabled_flag)
      offset_max =
          (pps->num_tile_columns_minus1 + 1) * (pps->num_tile_rows_minus1 + 1) -
          1;
    else
      offset_max =
          (pps->num_tile_columns_minus1 + 1) * pps->PicHeightInCtbsY - 1;

    READ_UE_MAX (&nr, slice->num_entry_point_offsets, offset_max);
    if (slice->num_entry_point_offsets > 0) {
      READ_UE_MAX (&nr, slice->offset_len_minus1, 31);
      slice->entry_point_offset_minus1 =
          g_new0 (guint32, slice->num_entry_point_offsets);
      for (i = 0; i < slice->num_entry_point_offsets; i++)
        READ_UINT32 (&nr, slice->entry_point_offset_minus1[i],
            (slice->offset_len_minus1 + 1));
    }
  }

  if (pps->slice_segment_header_extension_present_flag) {
    guint16 slice_segment_header_extension_length;
    READ_UE_MAX (&nr, slice_segment_header_extension_length, 256);
    for (i = 0; i < slice_segment_header_extension_length; i++)
      if (!nal_reader_skip (&nr, 8))
        goto error;
  }

  /* Skip the byte alignment bits */
  if (!nal_reader_skip (&nr, 1))
    goto error;
  while (!nal_reader_is_byte_aligned (&nr)) {
    if (!nal_reader_skip (&nr, 1))
      goto error;
  }

  slice->header_size = nal_reader_get_pos (&nr);
  slice->n_emulation_prevention_bytes = nal_reader_get_epb_count (&nr);

  return GST_H265_PARSER_OK;

error:
  GST_WARNING ("error parsing \"Slice header\"");

  gst_h265_slice_hdr_free (slice);

  return GST_H265_PARSER_ERROR;
}

static gboolean
nal_reader_has_more_data_in_payload (NalReader * nr,
    guint32 payload_start_pos_bit, guint32 payloadSize)
{
  if (nal_reader_is_byte_aligned (nr) &&
      (nal_reader_get_pos (nr) >= (payload_start_pos_bit + 8 * payloadSize)))
    return FALSE;

  return TRUE;
}

static GstH265ParserResult
gst_h265_parser_parse_sei_message (GstH265Parser * parser,
    guint8 nal_type, NalReader * nr, GstH265SEIMessage * sei)
{
  guint32 payloadSize;
  guint8 payload_type_byte, payload_size_byte;
  guint remaining, payload_size;
  guint32 payload_start_pos_bit;
  GstH265ParserResult res = GST_H265_PARSER_OK;

  GST_DEBUG ("parsing \"Sei message\"");

  memset (sei, 0, sizeof (*sei));

  do {
    READ_UINT8 (nr, payload_type_byte, 8);
    sei->payloadType += payload_type_byte;
  } while (payload_type_byte == 0xff);
  payloadSize = 0;
  do {
    READ_UINT8 (nr, payload_size_byte, 8);
    payloadSize += payload_size_byte;
  }
  while (payload_size_byte == 0xff);

  remaining = nal_reader_get_remaining (nr);
  payload_size = payloadSize * 8 < remaining ? payloadSize * 8 : remaining;

  payload_start_pos_bit = nal_reader_get_pos (nr);
  GST_DEBUG
      ("SEI message received: payloadType  %u, payloadSize = %u bits",
      sei->payloadType, payload_size);

  if (nal_type == GST_H265_NAL_PREFIX_SEI) {
    switch (sei->payloadType) {
      case GST_H265_SEI_BUF_PERIOD:
        /* size not set; might depend on emulation_prevention_three_byte */
        res = gst_h265_parser_parse_buffering_period (parser,
            &sei->payload.buffering_period, nr);
        break;
      case GST_H265_SEI_PIC_TIMING:
        /* size not set; might depend on emulation_prevention_three_byte */
        res = gst_h265_parser_parse_pic_timing (parser,
            &sei->payload.pic_timing, nr);
        break;
      case GST_H265_SEI_REGISTERED_USER_DATA:
        res = gst_h265_parser_parse_registered_user_data (parser,
            &sei->payload.registered_user_data, nr, payload_size >> 3);
        break;
      case GST_H265_SEI_USER_DATA_UNREGISTERED:
        res = gst_h265_parser_parse_user_data_unregistered (parser,
            &sei->payload.user_data_unregistered, nr, payload_size >> 3);
        break;
      case GST_H265_SEI_RECOVERY_POINT:
        res = gst_h265_parser_parse_recovery_point (parser,
            &sei->payload.recovery_point, nr);
        break;
      case GST_H265_SEI_TIME_CODE:
        res = gst_h265_parser_parse_time_code (parser,
            &sei->payload.time_code, nr);
        break;
      case GST_H265_SEI_MASTERING_DISPLAY_COLOUR_VOLUME:
        res = gst_h265_parser_parse_mastering_display_colour_volume (parser,
            &sei->payload.mastering_display_colour_volume, nr);
        break;
      case GST_H265_SEI_CONTENT_LIGHT_LEVEL:
        res = gst_h265_parser_parse_content_light_level_info (parser,
            &sei->payload.content_light_level, nr);
        break;
      default:
        /* Just consume payloadSize bytes, which does not account for
           emulation prevention bytes */
        if (!nal_reader_skip_long (nr, payload_size))
          goto error;
        res = GST_H265_PARSER_OK;
        break;
    }
  } else if (nal_type == GST_H265_NAL_SUFFIX_SEI) {
    switch (sei->payloadType) {
      default:
        /* Just consume payloadSize bytes, which does not account for
           emulation prevention bytes */
        if (!nal_reader_skip_long (nr, payload_size))
          goto error;
        res = GST_H265_PARSER_OK;
        break;
    }
  }

  /* Not parsing the reserved_payload_extension, but it shouldn't be
   * an issue because of 1: There shall not be any reserved_payload_extension
   * present in bitstreams conforming to the specification.2. Even though
   * it is present, the size will be less than total PayloadSize since the
   * size of reserved_payload_extension is supposed to be
   * 8 * payloadSize - nEarlierBits - nPayloadZeroBits -1 which means the
   * the current implementation will still skip all unnecessary bits correctly.
   * In theory, we can have a more optimized implementation by skipping the
   * data left in PayLoadSize without out individually checking for each bits,
   * since the totoal size will be always less than payloadSize*/
  while (nal_reader_has_more_data_in_payload (nr, payload_start_pos_bit,
          payloadSize)) {
    /* Skip the byte alignment bits */
    if (!nal_reader_skip (nr, 1))
      goto error;
    while (!nal_reader_is_byte_aligned (nr)) {
      if (!nal_reader_skip (nr, 1))
        goto error;
    }
  }

  return res;

error:
  GST_WARNING ("error parsing \"Sei message\"");
  return GST_H265_PARSER_ERROR;
}

/**
 * gst_h265_slice_hdr_copy:
 * @dst_slice: The destination #GstH265SliceHdr to copy into
 * @src_slice: The source #GstH265SliceHdr to copy from
 *
 * Copies @src_slice into @dst_slice
 *
 * Returns: %TRUE if everything went fine, %FALSE otherwise
 */
gboolean
gst_h265_slice_hdr_copy (GstH265SliceHdr * dst_slice,
    const GstH265SliceHdr * src_slice)
{
  guint i;

  g_return_val_if_fail (dst_slice != NULL, FALSE);
  g_return_val_if_fail (src_slice != NULL, FALSE);

  gst_h265_slice_hdr_free (dst_slice);

  *dst_slice = *src_slice;

  if (dst_slice->num_entry_point_offsets > 0) {
    dst_slice->entry_point_offset_minus1 =
        g_new0 (guint32, dst_slice->num_entry_point_offsets);
    for (i = 0; i < dst_slice->num_entry_point_offsets; i++)
      dst_slice->entry_point_offset_minus1[i] =
          src_slice->entry_point_offset_minus1[i];
  }

  return TRUE;
}

/**
 * gst_h265_slice_hdr_free:
 * slice_hdr: The #GstH265SliceHdr to free
 *
 * Frees @slice_hdr fields.
 */
void
gst_h265_slice_hdr_free (GstH265SliceHdr * slice_hdr)
{
  g_return_if_fail (slice_hdr != NULL);

  if (slice_hdr->num_entry_point_offsets > 0)
    g_free (slice_hdr->entry_point_offset_minus1);
  slice_hdr->entry_point_offset_minus1 = 0;
}

/**
 * gst_h265_sei_copy:
 * @dst_sei: The destination #GstH265SEIMessage to copy into
 * @src_sei: The source #GstH265SEIMessage to copy from
 *
 * Copies @src_sei into @dst_sei
 *
 * Returns: %TRUE if everything went fine, %FALSE otherwise
 */
gboolean
gst_h265_sei_copy (GstH265SEIMessage * dst_sei,
    const GstH265SEIMessage * src_sei)
{
  guint i;

  g_return_val_if_fail (dst_sei != NULL, FALSE);
  g_return_val_if_fail (src_sei != NULL, FALSE);

  gst_h265_sei_free (dst_sei);

  *dst_sei = *src_sei;

  if (dst_sei->payloadType == GST_H265_SEI_PIC_TIMING) {
    GstH265PicTiming *dst_pic_timing = &dst_sei->payload.pic_timing;
    const GstH265PicTiming *src_pic_timing = &src_sei->payload.pic_timing;

    if (dst_pic_timing->num_decoding_units_minus1 > 0) {
      dst_pic_timing->num_nalus_in_du_minus1 =
          g_new0 (guint32, (dst_pic_timing->num_decoding_units_minus1 + 1));
      dst_pic_timing->du_cpb_removal_delay_increment_minus1 =
          g_new0 (guint8, (dst_pic_timing->num_decoding_units_minus1 + 1));

      for (i = 0; i <= dst_pic_timing->num_decoding_units_minus1; i++) {
        dst_pic_timing->num_nalus_in_du_minus1[i] =
            src_pic_timing->num_nalus_in_du_minus1[i];
        dst_pic_timing->du_cpb_removal_delay_increment_minus1[i] =
            src_pic_timing->du_cpb_removal_delay_increment_minus1[i];
      }
    }
  } else if (dst_sei->payloadType == GST_H265_SEI_REGISTERED_USER_DATA) {
    GstH265RegisteredUserData *dst_rud = &dst_sei->payload.registered_user_data;
    const GstH265RegisteredUserData *src_rud =
        &src_sei->payload.registered_user_data;

    if (src_rud->size) {
      dst_rud->data = g_malloc (src_rud->size);
      memcpy ((guint8 *) dst_rud->data, src_rud->data, src_rud->size);
    }
  } else if (dst_sei->payloadType == GST_H265_SEI_USER_DATA_UNREGISTERED) {
    GstH265UserDataUnregistered *dst_udu =
        &dst_sei->payload.user_data_unregistered;
    const GstH265UserDataUnregistered *src_udu =
        &src_sei->payload.user_data_unregistered;

    if (src_udu->size) {
      dst_udu->data = g_malloc (src_udu->size);
      memcpy ((guint8 *) dst_udu->data, src_udu->data, src_udu->size);
    }
  }

  return TRUE;
}

/**
 * gst_h265_sei_free:
 * sei: The #GstH265SEIMessage to free
 *
 * Frees @sei fields.
 */
void
gst_h265_sei_free (GstH265SEIMessage * sei)
{
  g_return_if_fail (sei != NULL);

  if (sei->payloadType == GST_H265_SEI_PIC_TIMING) {
    GstH265PicTiming *pic_timing = &sei->payload.pic_timing;
    if (pic_timing->num_decoding_units_minus1 > 0) {
      g_free (pic_timing->num_nalus_in_du_minus1);
      g_free (pic_timing->du_cpb_removal_delay_increment_minus1);
    }
    pic_timing->num_nalus_in_du_minus1 = 0;
    pic_timing->du_cpb_removal_delay_increment_minus1 = 0;
  } else if (sei->payloadType == GST_H265_SEI_REGISTERED_USER_DATA) {
    GstH265RegisteredUserData *rud = &sei->payload.registered_user_data;
    g_free ((guint8 *) rud->data);
    rud->data = NULL;
  } else if (sei->payloadType == GST_H265_SEI_USER_DATA_UNREGISTERED) {
    GstH265UserDataUnregistered *udu = &sei->payload.user_data_unregistered;
    g_free ((guint8 *) udu->data);
    udu->data = NULL;
  }
}

/**
 * gst_h265_parser_parse_sei:
 * @nalparser: a #GstH265Parser
 * @nalu: The `GST_H265_NAL_*_SEI` #GstH265NalUnit to parse
 * @messages: The GArray of #GstH265SEIMessage to fill. The caller must free it when done.
 *
 * Parses @data, create and fills the @messages array.
 *
 * Returns: a #GstH265ParserResult
 */
GstH265ParserResult
gst_h265_parser_parse_sei (GstH265Parser * nalparser, GstH265NalUnit * nalu,
    GArray ** messages)
{
  NalReader nr;
  GstH265SEIMessage sei;
  GstH265ParserResult res;

  GST_DEBUG ("parsing SEI nal");
  nal_reader_init (&nr, nalu->data + nalu->offset + nalu->header_bytes,
      nalu->size - nalu->header_bytes);
  *messages = g_array_new (FALSE, FALSE, sizeof (GstH265SEIMessage));
  g_array_set_clear_func (*messages, (GDestroyNotify) gst_h265_sei_free);

  do {
    res = gst_h265_parser_parse_sei_message (nalparser, nalu->type, &nr, &sei);
    if (res == GST_H265_PARSER_OK)
      g_array_append_val (*messages, sei);
    else
      break;
  } while (nal_reader_has_more_data (&nr));

  return res;
}

/**
 * gst_h265_parser_update_vps:
 * @parser: a #GstH265Parser
 * @vps: (transfer none): a #GstH265VPS.
 *
 * Replace internal Video Parameter Set struct corresponding to id of @vps
 * with @vps. @nalparser will mark @vps as last parsed vps.
 *
 * Returns: a #GstH265ParserResult
 *
 * Since: 1.18
 */
GstH265ParserResult
gst_h265_parser_update_vps (GstH265Parser * parser, GstH265VPS * vps)
{
  g_return_val_if_fail (parser != NULL, GST_H265_PARSER_ERROR);
  g_return_val_if_fail (vps != NULL, GST_H265_PARSER_ERROR);
  g_return_val_if_fail (vps->id < GST_H265_MAX_VPS_COUNT,
      GST_H265_PARSER_ERROR);

  if (!vps->valid) {
    GST_WARNING ("Cannot update with invalid VPS");
    return GST_H265_PARSER_ERROR;
  }

  GST_DEBUG ("Updating video parameter set with id: %d", vps->id);

  parser->vps[vps->id] = *vps;
  parser->last_vps = &parser->vps[vps->id];

  return GST_H265_PARSER_OK;
}

/**
 * gst_h265_parser_update_sps:
 * @parser: a #GstH265Parser
 * @sps: (transfer none): a #GstH265SPS.
 *
 * Replace internal Sequence Parameter Set struct corresponding to id of @sps
 * with @sps. @nalparser will mark @sps as last parsed sps.
 *
 * Returns: a #GstH265ParserResult
 *
 * Since: 1.18
 */
GstH265ParserResult
gst_h265_parser_update_sps (GstH265Parser * parser, GstH265SPS * sps)
{
  g_return_val_if_fail (parser != NULL, GST_H265_PARSER_ERROR);
  g_return_val_if_fail (sps != NULL, GST_H265_PARSER_ERROR);
  g_return_val_if_fail (sps->id < GST_H265_MAX_SPS_COUNT,
      GST_H265_PARSER_ERROR);

  if (!sps->valid) {
    GST_WARNING ("Cannot update with invalid SPS");
    return GST_H265_PARSER_ERROR;
  }

  if (sps->vps) {
    GstH265VPS *vps = gst_h265_parser_get_vps (parser, sps->vps->id);
    if (!vps || vps != sps->vps) {
      GST_WARNING ("Linked VPS is not identical to internal VPS");
      return GST_H265_PARSER_BROKEN_LINK;
    }
  }

  GST_DEBUG ("Updating sequence parameter set with id: %d", sps->id);

  parser->sps[sps->id] = *sps;
  parser->last_sps = &parser->sps[sps->id];

  return GST_H265_PARSER_OK;
}

/**
 * gst_h265_parser_update_pps:
 * @parser: a #GstH265Parser
 * @pps: (transfer none): a #GstH265PPS.
 *
 * Replace internal Sequence Parameter Set struct corresponding to id of @pps
 * with @pps. @nalparser will mark @pps as last parsed sps.
 *
 * Returns: a #GstH265ParserResult
 *
 * Since: 1.18
 */
GstH265ParserResult
gst_h265_parser_update_pps (GstH265Parser * parser, GstH265PPS * pps)
{
  GstH265SPS *sps;

  g_return_val_if_fail (parser != NULL, GST_H265_PARSER_ERROR);
  g_return_val_if_fail (pps != NULL, GST_H265_PARSER_ERROR);
  g_return_val_if_fail (pps->id < GST_H265_MAX_PPS_COUNT,
      GST_H265_PARSER_ERROR);

  if (!pps->valid) {
    GST_WARNING ("Cannot update with invalid PPS");
    return GST_H265_PARSER_ERROR;
  }

  if (!pps->sps) {
    GST_WARNING ("No linked SPS struct");
    return GST_H265_PARSER_BROKEN_LINK;
  }

  sps = gst_h265_parser_get_sps (parser, pps->sps->id);
  if (!sps || sps != pps->sps) {
    GST_WARNING ("Linked SPS is not identical to internal SPS");
    return GST_H265_PARSER_BROKEN_LINK;
  }

  GST_DEBUG ("Updating picture parameter set with id: %d", pps->id);

  parser->pps[pps->id] = *pps;
  parser->last_pps = &parser->pps[pps->id];

  return GST_H265_PARSER_OK;
}

/**
 * gst_h265_quant_matrix_4x4_get_zigzag_from_raster:
 * @out_quant: (out): The resulting quantization matrix
 * @quant: The source quantization matrix
 *
 * Converts quantization matrix @quant from raster scan order to
 * zigzag scan order and store the resulting factors into @out_quant.
 *
 * Note: it is an error to pass the same table in both @quant and
 * @out_quant arguments.
 *
 * Since: 1.6
 */
void
gst_h265_quant_matrix_4x4_get_zigzag_from_raster (guint8 out_quant[16],
    const guint8 quant[16])
{
  guint i;

  g_return_if_fail (out_quant != quant);

  for (i = 0; i < 16; i++)
    out_quant[i] = quant[zigzag_4x4[i]];
}

/**
 * gst_h265_quant_matrix_4x4_get_raster_from_zigzag:
 * @out_quant: (out): The resulting quantization matrix
 * @quant: The source quantization matrix
 *
 * Converts quantization matrix @quant from zigzag scan order to
 * raster scan order and store the resulting factors into @out_quant.
 *
 * Note: it is an error to pass the same table in both @quant and
 * @out_quant arguments.
 *
 * Since: 1.6
 */
void
gst_h265_quant_matrix_4x4_get_raster_from_zigzag (guint8 out_quant[16],
    const guint8 quant[16])
{
  guint i;

  g_return_if_fail (out_quant != quant);

  for (i = 0; i < 16; i++)
    out_quant[zigzag_4x4[i]] = quant[i];
}

/**
 * gst_h265_quant_matrix_8x8_get_zigzag_from_raster:
 * @out_quant: (out): The resulting quantization matrix
 * @quant: The source quantization matrix
 *
 * Converts quantization matrix @quant from raster scan order to
 * zigzag scan order and store the resulting factors into @out_quant.
 *
 * Note: it is an error to pass the same table in both @quant and
 * @out_quant arguments.
 *
 * Since: 1.6
 */
void
gst_h265_quant_matrix_8x8_get_zigzag_from_raster (guint8 out_quant[64],
    const guint8 quant[64])
{
  guint i;

  g_return_if_fail (out_quant != quant);

  for (i = 0; i < 64; i++)
    out_quant[i] = quant[zigzag_8x8[i]];
}

/**
 * gst_h265_quant_matrix_8x8_get_raster_from_zigzag:
 * @out_quant: (out): The resulting quantization matrix
 * @quant: The source quantization matrix
 *
 * Converts quantization matrix @quant from zigzag scan order to
 * raster scan order and store the resulting factors into @out_quant.
 *
 * Note: it is an error to pass the same table in both @quant and
 * @out_quant arguments.
 *
 * Since: 1.6
 */
void
gst_h265_quant_matrix_8x8_get_raster_from_zigzag (guint8 out_quant[64],
    const guint8 quant[64])
{
  guint i;

  g_return_if_fail (out_quant != quant);

  for (i = 0; i < 64; i++)
    out_quant[zigzag_8x8[i]] = quant[i];
}

/**
 * gst_h265_quant_matrix_4x4_get_uprightdiagonal_from_raster:
 * @out_quant: (out): The resulting quantization matrix
 * @quant: The source quantization matrix
 *
 * Converts quantization matrix @quant from raster scan order to
 * uprightdiagonal scan order and store the resulting factors
 * into @out_quant.
 *
 * Note: it is an error to pass the same table in both @quant and
 * @out_quant arguments.
 *
 * Since: 1.6
 */
void
gst_h265_quant_matrix_4x4_get_uprightdiagonal_from_raster (guint8 out_quant[16],
    const guint8 quant[16])
{
  guint i;

  g_return_if_fail (out_quant != quant);

  for (i = 0; i < 16; i++)
    out_quant[i] = quant[uprightdiagonal_4x4[i]];
}

/**
 * gst_h265_quant_matrix_4x4_get_raster_from_uprightdiagonal:
 * @out_quant: (out): The resulting quantization matrix
 * @quant: The source quantization matrix
 *
 * Converts quantization matrix @quant from uprightdiagonal scan order to
 * raster scan order and store the resulting factors into @out_quant.
 *
 * Note: it is an error to pass the same table in both @quant and
 * @out_quant arguments.
 *
 * Since: 1.6
 */
void
gst_h265_quant_matrix_4x4_get_raster_from_uprightdiagonal (guint8 out_quant[16],
    const guint8 quant[16])
{
  guint i;

  g_return_if_fail (out_quant != quant);

  for (i = 0; i < 16; i++)
    out_quant[uprightdiagonal_4x4[i]] = quant[i];
}

/**
 * gst_h265_quant_matrix_8x8_get_uprightdiagonal_from_raster:
 * @out_quant: (out): The resulting quantization matrix
 * @quant: The source quantization matrix
 *
 * Converts quantization matrix @quant from raster scan order to
 * uprightdiagonal scan order and store the resulting factors
 * into @out_quant.
 *
 * Note: it is an error to pass the same table in both @quant and
 * @out_quant arguments.
 *
 * Since: 1.6
 */
void
gst_h265_quant_matrix_8x8_get_uprightdiagonal_from_raster (guint8 out_quant[64],
    const guint8 quant[64])
{
  guint i;

  g_return_if_fail (out_quant != quant);

  for (i = 0; i < 64; i++)
    out_quant[i] = quant[uprightdiagonal_8x8[i]];
}

/**
 * gst_h265_quant_matrix_8x8_get_raster_from_uprightdiagonal:
 * @out_quant: (out): The resulting quantization matrix
 * @quant: The source quantization matrix
 *
 * Converts quantization matrix @quant from uprightdiagonal scan order to
 * raster scan order and store the resulting factors into @out_quant.
 *
 * Note: it is an error to pass the same table in both @quant and
 * @out_quant arguments.
 *
 * Since: 1.6
 */
void
gst_h265_quant_matrix_8x8_get_raster_from_uprightdiagonal (guint8 out_quant[64],
    const guint8 quant[64])
{
  guint i;

  g_return_if_fail (out_quant != quant);

  for (i = 0; i < 64; i++)
    out_quant[uprightdiagonal_8x8[i]] = quant[i];
}

typedef struct
{
  GstH265Profile profile;

  guint8 max_14bit_constraint_flag;
  guint8 max_12bit_constraint_flag;
  guint8 max_10bit_constraint_flag;
  guint8 max_8bit_constraint_flag;
  guint8 max_422chroma_constraint_flag;
  guint8 max_420chroma_constraint_flag;
  guint8 max_monochrome_constraint_flag;
  guint8 intra_constraint_flag;
  guint8 one_picture_only_constraint_flag;
  gboolean lower_bit_rate_constraint_flag_set;

  /* Tie breaker if more than one profiles are matching */
  guint priority;
} H265ExtensionProfile;

typedef struct
{
  H265ExtensionProfile *profile;
  guint extra_constraints;
} H265ExtensionProfileMatch;

static gint
sort_fre_profile_matches (H265ExtensionProfileMatch * a,
    H265ExtensionProfileMatch * b)
{
  gint d;

  d = a->extra_constraints - b->extra_constraints;
  if (d)
    return d;

  return b->profile->priority - a->profile->priority;
}

static GstH265Profile
get_extension_profile (H265ExtensionProfile * profiles, guint num,
    const GstH265ProfileTierLevel * ptl)
{
  GstH265Profile result = GST_H265_PROFILE_INVALID;
  guint i;
  GList *matches = NULL;

  for (i = 0; i < num; i++) {
    H265ExtensionProfile p = profiles[i];
    guint extra_constraints = 0;
    H265ExtensionProfileMatch *m;

    /* Filter out all the profiles having constraints not satisfied by @ptl.
     * Then pick the one having the least extra constraints. This allow us
     * to match the closest profile if bitstream contains not standard
     * constraints. */
    if (p.max_14bit_constraint_flag != ptl->max_14bit_constraint_flag) {
      if (p.max_14bit_constraint_flag)
        continue;
      extra_constraints++;
    }

    if (p.max_12bit_constraint_flag != ptl->max_12bit_constraint_flag) {
      if (p.max_12bit_constraint_flag)
        continue;
      extra_constraints++;
    }

    if (p.max_10bit_constraint_flag != ptl->max_10bit_constraint_flag) {
      if (p.max_10bit_constraint_flag)
        continue;
      extra_constraints++;
    }

    if (p.max_8bit_constraint_flag != ptl->max_8bit_constraint_flag) {
      if (p.max_8bit_constraint_flag)
        continue;
      extra_constraints++;
    }

    if (p.max_422chroma_constraint_flag != ptl->max_422chroma_constraint_flag) {
      if (p.max_422chroma_constraint_flag)
        continue;
      extra_constraints++;
    }

    if (p.max_420chroma_constraint_flag != ptl->max_420chroma_constraint_flag) {
      if (p.max_420chroma_constraint_flag)
        continue;
      extra_constraints++;
    }

    if (p.max_monochrome_constraint_flag != ptl->max_monochrome_constraint_flag) {
      if (p.max_monochrome_constraint_flag)
        continue;
      extra_constraints++;
    }

    if (p.intra_constraint_flag != ptl->intra_constraint_flag) {
      if (p.intra_constraint_flag)
        continue;
      extra_constraints++;
    }

    if (p.one_picture_only_constraint_flag !=
        ptl->one_picture_only_constraint_flag) {
      if (p.one_picture_only_constraint_flag)
        continue;
      extra_constraints++;
    }

    if (p.lower_bit_rate_constraint_flag_set
        && !ptl->lower_bit_rate_constraint_flag)
      continue;

    if (extra_constraints == 0) {
      result = p.profile;
      break;
    }

    m = g_new0 (H265ExtensionProfileMatch, 1);
    m->profile = &profiles[i];
    m->extra_constraints = extra_constraints;
    matches = g_list_prepend (matches, m);
  }

  if (result == GST_H265_PROFILE_INVALID && matches) {
    H265ExtensionProfileMatch *m;

    matches = g_list_sort (matches, (GCompareFunc) sort_fre_profile_matches);
    m = matches->data;
    result = m->profile->profile;
    GST_INFO ("Fail to find the profile matches all extensions bits,"
        " select the closest %s with %d bit diff",
        gst_h265_profile_to_string (result), m->extra_constraints);
  }

  if (matches)
    g_list_free_full (matches, g_free);

  return result;
}

static GstH265Profile
get_format_range_extension_profile (const GstH265ProfileTierLevel * ptl)
{
  /* Profile idc: GST_H265_PROFILE_IDC_FORMAT_RANGE_EXTENSION
     See Table A.2 for the definition of those formats */
  static H265ExtensionProfile profiles[] = {
    {GST_H265_PROFILE_MONOCHROME,
        0, 1, 1, 1, 1, 1, 1, 0, 0, TRUE, 0},
    {GST_H265_PROFILE_MONOCHROME_10,
        0, 1, 1, 0, 1, 1, 1, 0, 0, TRUE, 1},
    {GST_H265_PROFILE_MONOCHROME_12,
        0, 1, 0, 0, 1, 1, 1, 0, 0, TRUE, 2},
    {GST_H265_PROFILE_MONOCHROME_16,
        0, 0, 0, 0, 1, 1, 1, 0, 0, TRUE, 3},
    {GST_H265_PROFILE_MAIN_12,
        0, 1, 0, 0, 1, 1, 0, 0, 0, TRUE, 4},
    {GST_H265_PROFILE_MAIN_422_10,
        0, 1, 1, 0, 1, 0, 0, 0, 0, TRUE, 5},
    {GST_H265_PROFILE_MAIN_422_12,
        0, 1, 0, 0, 1, 0, 0, 0, 0, TRUE, 6},
    {GST_H265_PROFILE_MAIN_444,
        0, 1, 1, 1, 0, 0, 0, 0, 0, TRUE, 7},
    {GST_H265_PROFILE_MAIN_444_10,
        0, 1, 1, 0, 0, 0, 0, 0, 0, TRUE, 8},
    {GST_H265_PROFILE_MAIN_444_12,
        0, 1, 0, 0, 0, 0, 0, 0, 0, TRUE, 9},
    {GST_H265_PROFILE_MAIN_INTRA,
        0, 1, 1, 1, 1, 1, 0, 1, 0, FALSE, 10},
    {GST_H265_PROFILE_MAIN_10_INTRA,
        0, 1, 1, 0, 1, 1, 0, 1, 0, FALSE, 11},
    {GST_H265_PROFILE_MAIN_12_INTRA,
        0, 1, 0, 0, 1, 1, 0, 1, 0, FALSE, 12},
    {GST_H265_PROFILE_MAIN_422_10_INTRA,
        0, 1, 1, 0, 1, 0, 0, 1, 0, FALSE, 13},
    {GST_H265_PROFILE_MAIN_422_12_INTRA,
        0, 1, 0, 0, 1, 0, 0, 1, 0, FALSE, 14},
    {GST_H265_PROFILE_MAIN_444_INTRA,
        0, 1, 1, 1, 0, 0, 0, 1, 0, FALSE, 15},
    {GST_H265_PROFILE_MAIN_444_10_INTRA,
        0, 1, 1, 0, 0, 0, 0, 1, 0, FALSE, 16},
    {GST_H265_PROFILE_MAIN_444_12_INTRA,
        0, 1, 0, 0, 0, 0, 0, 1, 0, FALSE, 17},
    {GST_H265_PROFILE_MAIN_444_16_INTRA,
        0, 0, 0, 0, 0, 0, 0, 1, 0, FALSE, 18},
    {GST_H265_PROFILE_MAIN_444_STILL_PICTURE,
        0, 1, 1, 1, 0, 0, 0, 1, 1, FALSE, 19},
    {GST_H265_PROFILE_MAIN_444_16_STILL_PICTURE,
        0, 0, 0, 0, 0, 0, 0, 1, 1, FALSE, 20},
  };

  return get_extension_profile (profiles, G_N_ELEMENTS (profiles), ptl);
}

static GstH265Profile
get_3d_profile (const GstH265ProfileTierLevel * ptl)
{
  /* profile idc: GST_H265_PROFILE_IDC_3D_MAIN */
  static H265ExtensionProfile profiles[] = {
    {GST_H265_PROFILE_3D_MAIN,
        0, 1, 1, 1, 1, 1, 0, 0, 0, TRUE, 0},
  };

  return get_extension_profile (profiles, G_N_ELEMENTS (profiles), ptl);
}

static GstH265Profile
get_multiview_profile (const GstH265ProfileTierLevel * ptl)
{
  static H265ExtensionProfile profiles[] = {
    {GST_H265_PROFILE_MULTIVIEW_MAIN,
        0, 1, 1, 1, 1, 1, 0, 0, 0, TRUE, 0},
  };

  return get_extension_profile (profiles, G_N_ELEMENTS (profiles), ptl);
}

static GstH265Profile
get_scalable_profile (const GstH265ProfileTierLevel * ptl)
{
  static H265ExtensionProfile profiles[] = {
    {GST_H265_PROFILE_SCALABLE_MAIN,
        0, 1, 1, 1, 1, 1, 0, 0, 0, TRUE, 0},
    {GST_H265_PROFILE_SCALABLE_MAIN_10,
        0, 1, 1, 0, 1, 1, 0, 0, 0, TRUE, 1},
  };

  return get_extension_profile (profiles, G_N_ELEMENTS (profiles), ptl);
}

static GstH265Profile
get_high_throughput_profile (const GstH265ProfileTierLevel * ptl)
{
  static H265ExtensionProfile profiles[] = {
    {GST_H265_PROFILE_HIGH_THROUGHPUT_444,
        1, 1, 1, 1, 0, 0, 0, 0, 0, TRUE, 0},
    {GST_H265_PROFILE_HIGH_THROUGHPUT_444_10,
        1, 1, 1, 0, 0, 0, 0, 0, 0, TRUE, 1},
    {GST_H265_PROFILE_HIGH_THROUGHPUT_444_14,
        1, 0, 0, 0, 0, 0, 0, 0, 0, TRUE, 2},
    {GST_H265_PROFILE_HIGH_THROUGHPUT_444_16_INTRA,
        0, 0, 0, 0, 0, 0, 0, 1, 0, FALSE, 3},
  };

  return get_extension_profile (profiles, G_N_ELEMENTS (profiles), ptl);
}

static GstH265Profile
get_screen_content_coding_extensions_profile (const GstH265ProfileTierLevel *
    ptl)
{
  static H265ExtensionProfile profiles[] = {
    {GST_H265_PROFILE_SCREEN_EXTENDED_MAIN,
        1, 1, 1, 1, 1, 1, 0, 0, 0, TRUE, 0},
    {GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_10,
        1, 1, 1, 0, 1, 1, 0, 0, 0, TRUE, 1},
    {GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444,
        1, 1, 1, 1, 0, 0, 0, 0, 0, TRUE, 2},
    {GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444_10,
        1, 1, 1, 0, 0, 0, 0, 0, 0, TRUE, 3},
  };

  return get_extension_profile (profiles, G_N_ELEMENTS (profiles), ptl);
}

static GstH265Profile
get_scalable_format_range_extensions_profile (const GstH265ProfileTierLevel *
    ptl)
{
  static H265ExtensionProfile profiles[] = {
    {GST_H265_PROFILE_SCALABLE_MONOCHROME,
        1, 1, 1, 1, 1, 1, 1, 0, 0, TRUE, 0},
    {GST_H265_PROFILE_SCALABLE_MONOCHROME_12,
        1, 1, 0, 0, 1, 1, 1, 0, 0, TRUE, 1},
    {GST_H265_PROFILE_SCALABLE_MONOCHROME_16,
        0, 0, 0, 0, 1, 1, 1, 0, 0, TRUE, 2},
    {GST_H265_PROFILE_SCALABLE_MAIN_444,
        1, 1, 1, 1, 0, 0, 0, 0, 0, TRUE, 3},
  };

  return get_extension_profile (profiles, G_N_ELEMENTS (profiles), ptl);
}

static GstH265Profile
    get_screen_content_coding_extensions_high_throughput_profile
    (const GstH265ProfileTierLevel * ptl)
{
  static H265ExtensionProfile profiles[] = {
    {GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444,
        1, 1, 1, 1, 0, 0, 0, 0, 0, TRUE, 0},
    {GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_10,
        1, 1, 1, 0, 0, 0, 0, 0, 0, TRUE, 1},
    {GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_14,
        1, 0, 0, 0, 0, 0, 0, 0, 0, TRUE, 2},
  };

  return get_extension_profile (profiles, G_N_ELEMENTS (profiles), ptl);
}

static inline void
append_profile (GstH265Profile profiles[GST_H265_PROFILE_MAX], guint * idx,
    GstH265Profile profile)
{
  if (profile == GST_H265_PROFILE_INVALID)
    return;
  profiles[*idx] = profile;
  (*idx)++;
}

/* *INDENT-OFF* */
struct h265_profiles_map
{
  GstH265ProfileIDC profile_idc;
  GstH265Profile (*get_profile) (const GstH265ProfileTierLevel *);
  GstH265Profile profile;
};
/* *INDENT-ON* */

static const struct h265_profiles_map profiles_map[] = {
  /* keep profile check in asc order */
  {GST_H265_PROFILE_IDC_MAIN, NULL, GST_H265_PROFILE_MAIN},
  {GST_H265_PROFILE_IDC_MAIN_10, NULL, GST_H265_PROFILE_MAIN_10},
  {GST_H265_PROFILE_IDC_MAIN_STILL_PICTURE, NULL,
      GST_H265_PROFILE_MAIN_STILL_PICTURE},
  {GST_H265_PROFILE_IDC_FORMAT_RANGE_EXTENSION,
      get_format_range_extension_profile, GST_H265_PROFILE_INVALID},
  {GST_H265_PROFILE_IDC_HIGH_THROUGHPUT, get_high_throughput_profile,
      GST_H265_PROFILE_INVALID},
  {GST_H265_PROFILE_IDC_MULTIVIEW_MAIN, get_multiview_profile,
      GST_H265_PROFILE_INVALID},
  {GST_H265_PROFILE_IDC_SCALABLE_MAIN, get_scalable_profile,
      GST_H265_PROFILE_INVALID},
  {GST_H265_PROFILE_IDC_3D_MAIN, get_3d_profile, GST_H265_PROFILE_INVALID},
  {GST_H265_PROFILE_IDC_SCREEN_CONTENT_CODING,
        get_screen_content_coding_extensions_profile,
      GST_H265_PROFILE_INVALID},
  {GST_H265_PROFILE_IDC_SCALABLE_FORMAT_RANGE_EXTENSION,
        get_scalable_format_range_extensions_profile,
      GST_H265_PROFILE_INVALID},
  {GST_H265_PROFILE_IDC_HIGH_THROUGHPUT_SCREEN_CONTENT_CODING_EXTENSION,
        get_screen_content_coding_extensions_high_throughput_profile,
      GST_H265_PROFILE_INVALID},
};

static void
gst_h265_profile_tier_level_get_profiles (const GstH265ProfileTierLevel * ptl,
    GstH265Profile profiles[GST_H265_PROFILE_MAX], guint * len)
{
  guint i = 0, j;

  /* First add profile idc */
  for (j = 0; j < G_N_ELEMENTS (profiles_map); j++) {
    if (ptl->profile_idc == profiles_map[j].profile_idc) {
      if (profiles_map[j].get_profile)
        append_profile (profiles, &i, profiles_map[j].get_profile (ptl));
      else
        profiles[i++] = profiles_map[j].profile;
      break;
    }
  }

  /* Later add compatibility flags */
  for (j = 0; j < G_N_ELEMENTS (profiles_map); j++) {
    if (i > 0 && ptl->profile_idc == profiles_map[j].profile_idc)
      continue;
    if (ptl->profile_compatibility_flag[profiles_map[j].profile_idc]) {
      if (profiles_map[j].get_profile)
        append_profile (profiles, &i, profiles_map[j].get_profile (ptl));
      else
        profiles[i++] = profiles_map[j].profile;
    }
  }

  *len = i;
}

/**
 * gst_h265_profile_tier_level_get_profile:
 * @ptl: a #GstH265ProfileTierLevel
 *
 * Return the H265 profile defined in @ptl.
 *
 * Returns: a #GstH265Profile
 * Since: 1.14
 */
GstH265Profile
gst_h265_profile_tier_level_get_profile (const GstH265ProfileTierLevel * ptl)
{
  guint len = 0;
  GstH265Profile profiles[GST_H265_PROFILE_MAX] = { GST_H265_PROFILE_INVALID, };

  gst_h265_profile_tier_level_get_profiles (ptl, profiles, &len);

  if (len > 0)
    return profiles[0];

  return GST_H265_PROFILE_INVALID;
}

/**
 * gst_h265_profile_to_string:
 * @profile: a #GstH265Profile
 *
 * Returns the descriptive name for the #GstH265Profile.
 *
 * Returns: (nullable): the name for @profile or %NULL on error
 *
 * Since: 1.18
 */
const gchar *
gst_h265_profile_to_string (GstH265Profile profile)
{
  guint i;

  if (profile == GST_H265_PROFILE_INVALID || profile == GST_H265_PROFILE_MAX)
    return NULL;

  for (i = 0; i < G_N_ELEMENTS (h265_profiles); i++) {
    if (profile == h265_profiles[i].profile)
      return h265_profiles[i].name;
  }

  return NULL;
}

/**
 * gst_h265_profile_from_string:
 * @string: the descriptive name for #GstH265Profile
 *
 * Returns a #GstH265Profile for the @string.
 *
 * Returns: the #GstH265Profile of @string or %GST_H265_PROFILE_INVALID on error
 *
 * Since: 1.18
 */
GstH265Profile
gst_h265_profile_from_string (const gchar * string)
{
  guint i;

  if (string == NULL)
    return GST_H265_PROFILE_INVALID;

  for (i = 0; i < G_N_ELEMENTS (h265_profiles); i++) {
    if (g_strcmp0 (string, h265_profiles[i].name) == 0) {
      return h265_profiles[i].profile;
    }
  }

  return GST_H265_PROFILE_INVALID;
}

/**
 * gst_h265_slice_type_to_string:
 * @slice_type: a #GstH265SliceType
 *
 * Returns the descriptive name for the #GstH265SliceType.
 *
 * Returns: (nullable): the name for @slice_type or %NULL on error
 *
 * Since: 1.24
 */
const gchar *
gst_h265_slice_type_to_string (GstH265SliceType slice_type)
{
  switch (slice_type) {
    case GST_H265_P_SLICE:
      return "P";
    case GST_H265_B_SLICE:
      return "B";
    case GST_H265_I_SLICE:
      return "I";
    default:
      GST_ERROR ("unknown %d slice type", slice_type);
  }

  return NULL;
}

static gboolean
gst_h265_write_sei_registered_user_data (NalWriter * nw,
    GstH265RegisteredUserData * rud)
{
  WRITE_UINT8 (nw, rud->country_code, 8);
  if (rud->country_code == 0xff)
    WRITE_UINT8 (nw, rud->country_code_extension, 8);

  WRITE_BYTES (nw, rud->data, rud->size);

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_h265_write_sei_user_data_unregistered (NalWriter * nw,
    GstH265UserDataUnregistered * udu)
{
  WRITE_BYTES (nw, udu->uuid, 16);
  WRITE_BYTES (nw, udu->data, udu->size);

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_h265_write_sei_time_code (NalWriter * nw, GstH265TimeCode * tc)
{
  gint i;

  WRITE_UINT8 (nw, tc->num_clock_ts, 2);

  for (i = 0; i < tc->num_clock_ts; i++) {
    WRITE_UINT8 (nw, tc->clock_timestamp_flag[i], 1);
    if (tc->clock_timestamp_flag[i]) {
      WRITE_UINT8 (nw, tc->units_field_based_flag[i], 1);
      WRITE_UINT8 (nw, tc->counting_type[i], 5);
      WRITE_UINT8 (nw, tc->full_timestamp_flag[i], 1);
      WRITE_UINT8 (nw, tc->discontinuity_flag[i], 1);
      WRITE_UINT8 (nw, tc->cnt_dropped_flag[i], 1);
      WRITE_UINT16 (nw, tc->n_frames[i], 9);

      if (tc->full_timestamp_flag[i]) {
        WRITE_UINT8 (nw, tc->seconds_value[i], 6);
        WRITE_UINT8 (nw, tc->minutes_value[i], 6);
        WRITE_UINT8 (nw, tc->hours_value[i], 5);
      } else {
        WRITE_UINT8 (nw, tc->seconds_flag[i], 1);
        if (tc->seconds_flag[i]) {
          WRITE_UINT8 (nw, tc->seconds_value[i], 6);
          WRITE_UINT8 (nw, tc->minutes_flag[i], 1);
          if (tc->minutes_flag[i]) {
            WRITE_UINT8 (nw, tc->minutes_value[i], 6);
            WRITE_UINT8 (nw, tc->hours_flag[i], 1);
            if (tc->hours_flag[i]) {
              WRITE_UINT8 (nw, tc->hours_value[i], 5);
            }
          }
        }
      }
    }

    WRITE_UINT8 (nw, tc->time_offset_length[i], 5);

    if (tc->time_offset_length[i] > 0)
      WRITE_UINT8 (nw, tc->time_offset_value[i], tc->time_offset_length[i]);
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_h265_write_sei_mastering_display_colour_volume (NalWriter * nw,
    GstH265MasteringDisplayColourVolume * mdcv)
{
  gint i;

  for (i = 0; i < 3; i++) {
    WRITE_UINT16 (nw, mdcv->display_primaries_x[i], 16);
    WRITE_UINT16 (nw, mdcv->display_primaries_y[i], 16);
  }

  WRITE_UINT16 (nw, mdcv->white_point_x, 16);
  WRITE_UINT16 (nw, mdcv->white_point_y, 16);
  WRITE_UINT32 (nw, mdcv->max_display_mastering_luminance, 32);
  WRITE_UINT32 (nw, mdcv->min_display_mastering_luminance, 32);

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_h265_write_sei_content_light_level_info (NalWriter * nw,
    GstH265ContentLightLevel * cll)
{
  WRITE_UINT16 (nw, cll->max_content_light_level, 16);
  WRITE_UINT16 (nw, cll->max_pic_average_light_level, 16);

  return TRUE;

error:
  return FALSE;
}

static GstMemory *
gst_h265_create_sei_memory_internal (guint8 layer_id, guint8 temporal_id_plus1,
    guint nal_prefix_size, gboolean packetized, GArray * messages)
{
  NalWriter nw;
  gint i;
  gboolean have_written_data = FALSE;

  nal_writer_init (&nw, nal_prefix_size, packetized);

  if (messages->len == 0)
    goto error;

  GST_DEBUG ("Create SEI nal from array, len: %d", messages->len);

  /* nal header */
  /* forbidden_zero_bit */
  WRITE_UINT8 (&nw, 0, 1);
  /* nal_unit_type */
  WRITE_UINT8 (&nw, GST_H265_NAL_PREFIX_SEI, 6);
  /* nuh_layer_id */
  WRITE_UINT8 (&nw, layer_id, 6);
  /* nuh_temporal_id_plus1 */
  WRITE_UINT8 (&nw, temporal_id_plus1, 3);

  for (i = 0; i < messages->len; i++) {
    GstH265SEIMessage *msg = &g_array_index (messages, GstH265SEIMessage, i);
    guint32 payload_size_data = 0;
    guint32 payload_size_in_bits = 0;
    guint32 payload_type_data = msg->payloadType;
    gboolean need_align = FALSE;

    switch (payload_type_data) {
      case GST_H265_SEI_REGISTERED_USER_DATA:{
        GstH265RegisteredUserData *rud = &msg->payload.registered_user_data;

        /* itu_t_t35_country_code: 8 bits */
        payload_size_data = 1;
        if (rud->country_code == 0xff) {
          /* itu_t_t35_country_code_extension_byte */
          payload_size_data++;
        }

        payload_size_data += rud->size;
        break;
      }
      case GST_H265_SEI_USER_DATA_UNREGISTERED:{
        GstH265UserDataUnregistered *udu = &msg->payload.user_data_unregistered;

        payload_size_data = 16 + udu->size;
        break;
      }
      case GST_H265_SEI_TIME_CODE:{
        gint j;
        GstH265TimeCode *tc = &msg->payload.time_code;
        /* num_clock_ts: 2 bits */
        payload_size_in_bits = 2;
        for (j = 0; j < tc->num_clock_ts; j++) {
          /* clock_timestamp_flag: 1 bit */
          payload_size_in_bits += 1;

          if (tc->clock_timestamp_flag[j]) {
            /* units_field_based_flag: 1 bit
             * counting_type: 5 bits
             * full_timestamp_flag: 1 bit
             * discontinuity_flag: 1 bit
             * cnt_dropped_flag: 1 bit
             * n_frames: 9 bit
             */
            payload_size_in_bits += 18;

            if (tc->full_timestamp_flag[j]) {
              /* seconds_value: 6 bits
               * minutes_value: 6 bits
               * hours_value: 5 bits
               */
              payload_size_in_bits += 17;
            } else {
              /* seconds_flag: 1 bit */
              payload_size_in_bits += 1;

              if (tc->seconds_flag[j]) {
                /* seconds_value: 6 bits
                 * minutes_flag: 1 bit
                 */
                payload_size_in_bits += 7;

                if (tc->minutes_flag[j]) {
                  /* minutes_value: 6 bits
                   * hours_flag: 1 bit
                   */
                  payload_size_in_bits += 7;
                  if (tc->hours_flag[j]) {
                    /* hours_value: 5 bits */
                    payload_size_in_bits += 5;
                  }
                }
              }
            }

            /* time_offset_length: 5bits
             * time_offset_value: time_offset_length bits
             */
            payload_size_in_bits += (5 + tc->time_offset_length[j]);
          }
        }

        payload_size_data = payload_size_in_bits >> 3;

        if ((payload_size_in_bits & 0x7) != 0) {
          GST_INFO ("Bits for Time Code SEI is not byte aligned");
          payload_size_data++;
          need_align = TRUE;
        }
        break;
      }
      case GST_H265_SEI_MASTERING_DISPLAY_COLOUR_VOLUME:
        /* x, y 16 bits per RGB channel
         * x, y 16 bits white point
         * max, min luminance 32 bits
         *
         * (2 * 2 * 3) + (2 * 2) + (4 * 2) = 24 bytes
         */
        payload_size_data = 24;
        break;
      case GST_H265_SEI_CONTENT_LIGHT_LEVEL:
        /* maxCLL and maxFALL per 16 bits
         *
         * 2 * 2 = 4 bytes
         */
        payload_size_data = 4;
        break;
      default:
        break;
    }

    if (payload_size_data == 0) {
      GST_FIXME ("Unsupported SEI type %d", msg->payloadType);
      continue;
    }

    /* write payload type bytes */
    while (payload_type_data >= 0xff) {
      WRITE_UINT8 (&nw, 0xff, 8);
      payload_type_data -= 0xff;
    }
    WRITE_UINT8 (&nw, payload_type_data, 8);

    /* write payload size bytes */
    while (payload_size_data >= 0xff) {
      WRITE_UINT8 (&nw, 0xff, 8);
      payload_size_data -= 0xff;
    }
    WRITE_UINT8 (&nw, payload_size_data, 8);

    switch (msg->payloadType) {
      case GST_H265_SEI_REGISTERED_USER_DATA:
        GST_DEBUG ("Writing \"Registered user data\" done");
        if (!gst_h265_write_sei_registered_user_data (&nw,
                &msg->payload.registered_user_data)) {
          GST_WARNING ("Failed to write \"Registered user data\"");
          goto error;
        }
        have_written_data = TRUE;
        break;
      case GST_H265_SEI_USER_DATA_UNREGISTERED:
        GST_DEBUG ("Writing \"Unregistered user data\" done");
        if (!gst_h265_write_sei_user_data_unregistered (&nw,
                &msg->payload.user_data_unregistered)) {
          GST_WARNING ("Failed to write \"Unregistered user data\"");
          goto error;
        }
        have_written_data = TRUE;
        break;
      case GST_H265_SEI_TIME_CODE:
        GST_DEBUG ("Wrtiting \"Time code\"");
        if (!gst_h265_write_sei_time_code (&nw, &msg->payload.time_code)) {
          GST_WARNING ("Failed to write \"Time code\"");
          goto error;
        }
        have_written_data = TRUE;
        break;
      case GST_H265_SEI_MASTERING_DISPLAY_COLOUR_VOLUME:
        GST_DEBUG ("Wrtiting \"Mastering display colour volume\"");
        if (!gst_h265_write_sei_mastering_display_colour_volume (&nw,
                &msg->payload.mastering_display_colour_volume)) {
          GST_WARNING ("Failed to write \"Mastering display colour volume\"");
          goto error;
        }
        have_written_data = TRUE;
        break;
      case GST_H265_SEI_CONTENT_LIGHT_LEVEL:
        GST_DEBUG ("Writing \"Content light level\" done");
        if (!gst_h265_write_sei_content_light_level_info (&nw,
                &msg->payload.content_light_level)) {
          GST_WARNING ("Failed to write \"Content light level\"");
          goto error;
        }
        have_written_data = TRUE;
        break;
      default:
        break;
    }

    if (need_align && !nal_writer_do_rbsp_trailing_bits (&nw)) {
      GST_WARNING ("Cannot insert traling bits");
      goto error;
    }
  }

  if (!have_written_data) {
    GST_WARNING ("No written sei data");
    goto error;
  }

  if (!nal_writer_do_rbsp_trailing_bits (&nw)) {
    GST_WARNING ("Failed to insert rbsp trailing bits");
    goto error;
  }

  return nal_writer_reset_and_get_memory (&nw);

error:
  nal_writer_reset (&nw);

  return NULL;
}

/**
 * gst_h265_create_sei_memory:
 * @layer_id: a nal unit layer id
 * @temporal_id_plus1: a nal unit temporal identifier
 * @start_code_prefix_length: a length of start code prefix, must be 3 or 4
 * @messages: (transfer none): a GArray of #GstH265SEIMessage
 *
 * Creates raw byte-stream format (a.k.a Annex B type) SEI nal unit data
 * from @messages
 *
 * Returns: a #GstMemory containing a PREFIX SEI nal unit
 *
 * Since: 1.18
 */
GstMemory *
gst_h265_create_sei_memory (guint8 layer_id, guint8 temporal_id_plus1,
    guint8 start_code_prefix_length, GArray * messages)
{
  g_return_val_if_fail (start_code_prefix_length == 3
      || start_code_prefix_length == 4, NULL);
  g_return_val_if_fail (messages != NULL, NULL);
  g_return_val_if_fail (messages->len > 0, NULL);

  return gst_h265_create_sei_memory_internal (layer_id, temporal_id_plus1,
      start_code_prefix_length, FALSE, messages);
}

/**
 * gst_h265_create_sei_memory_hevc:
 * @layer_id: a nal unit layer id
 * @temporal_id_plus1: a nal unit temporal identifier
 * @nal_length_size: a size of nal length field, allowed range is [1, 4]
 * @messages: (transfer none): a GArray of #GstH265SEIMessage
 *
 * Creates raw packetized format SEI nal unit data from @messages
 *
 * Returns: a #GstMemory containing a PREFIX SEI nal unit
 *
 * Since: 1.18
 */
GstMemory *
gst_h265_create_sei_memory_hevc (guint8 layer_id, guint8 temporal_id_plus1,
    guint8 nal_length_size, GArray * messages)
{
  return gst_h265_create_sei_memory_internal (layer_id, temporal_id_plus1,
      nal_length_size, TRUE, messages);
}

static GstBuffer *
gst_h265_parser_insert_sei_internal (GstH265Parser * parser,
    guint8 nal_prefix_size, gboolean packetized, GstBuffer * au,
    GstMemory * sei)
{
  GstH265NalUnit nalu;
  GstH265NalUnit sei_nalu;
  GstMapInfo info;
  GstMapInfo sei_info;
  GstH265ParserResult pres;
  guint offset = 0;
  GstBuffer *new_buffer = NULL;
  GstMemory *new_mem = NULL;

  /* all SEI payload types supported by us need to have the identical
   * temporal id to that of slice. Parse SEI first and we will
   * update it if it's required */
  if (!gst_memory_map (sei, &sei_info, GST_MAP_READ)) {
    GST_ERROR ("Cannot map sei memory");
    return NULL;
  }

  if (packetized) {
    pres = gst_h265_parser_identify_nalu_hevc (parser,
        sei_info.data, 0, sei_info.size, nal_prefix_size, &sei_nalu);
  } else {
    pres = gst_h265_parser_identify_nalu (parser,
        sei_info.data, 0, sei_info.size, &sei_nalu);
  }
  gst_memory_unmap (sei, &sei_info);
  if (pres != GST_H265_PARSER_OK && pres != GST_H265_PARSER_NO_NAL_END) {
    GST_DEBUG ("Failed to identify sei nal unit, ret: %d", pres);
    return NULL;
  }

  if (!gst_buffer_map (au, &info, GST_MAP_READ)) {
    GST_ERROR ("Cannot map au buffer");
    return NULL;
  }

  /* Find the offset of the first slice */
  do {
    if (packetized) {
      pres = gst_h265_parser_identify_nalu_hevc (parser,
          info.data, offset, info.size, nal_prefix_size, &nalu);
    } else {
      pres = gst_h265_parser_identify_nalu (parser,
          info.data, offset, info.size, &nalu);
    }

    if (pres != GST_H265_PARSER_OK && pres != GST_H265_PARSER_NO_NAL_END) {
      GST_DEBUG ("Failed to identify nal unit, ret: %d", pres);
      gst_buffer_unmap (au, &info);

      return NULL;
    }

    if (nalu.type <= GST_H265_NAL_SLICE_RASL_R
        || (nalu.type >= GST_H265_NAL_SLICE_BLA_W_LP
            && nalu.type <= GST_H265_NAL_SLICE_CRA_NUT)) {
      GST_DEBUG ("Found slice nal type %d at offset %d", nalu.type,
          nalu.sc_offset);
      break;
    }

    offset = nalu.offset + nalu.size;
  } while (pres == GST_H265_PARSER_OK);
  gst_buffer_unmap (au, &info);

  /* found the best position now, create new buffer */
  new_buffer = gst_buffer_new ();

  /* copy all metadata */
  if (!gst_buffer_copy_into (new_buffer, au, GST_BUFFER_COPY_METADATA, 0, -1)) {
    GST_ERROR ("Failed to copy metadata into new buffer");
    gst_clear_buffer (&new_buffer);
    goto out;
  }

  /* copy non-slice nal */
  if (nalu.sc_offset > 0) {
    if (!gst_buffer_copy_into (new_buffer, au,
            GST_BUFFER_COPY_MEMORY, 0, nalu.sc_offset)) {
      GST_ERROR ("Failed to copy buffer");
      gst_clear_buffer (&new_buffer);
      goto out;
    }
  }

  /* check whether we need to update temporal id and layer id.
   * If it's not matched to slice nalu, update it.
   */
  if (sei_nalu.layer_id != nalu.layer_id || sei_nalu.temporal_id_plus1 !=
      nalu.temporal_id_plus1) {
    guint16 nalu_header;
    guint16 layer_id_temporal_id = 0;
    new_mem = gst_memory_copy (sei, 0, -1);

    if (!gst_memory_map (new_mem, &sei_info, GST_MAP_READWRITE)) {
      GST_ERROR ("Failed to map new sei memory");
      gst_memory_unref (new_mem);
      gst_clear_buffer (&new_buffer);
      goto out;
    }

    nalu_header = GST_READ_UINT16_BE (sei_info.data + sei_nalu.offset);

    /* clear bits 7 ~ 15
     * NOTE:
     * bit 0: forbidden_zero_bit
     * bits 1 ~ 6: nalu type */
    nalu_header &= 0xfe00;

    layer_id_temporal_id = ((nalu.layer_id << 3) & 0x1f8);
    layer_id_temporal_id |= (nalu.temporal_id_plus1 & 0x7);

    nalu_header |= layer_id_temporal_id;
    GST_WRITE_UINT16_BE (sei_info.data + sei_nalu.offset, nalu_header);
    gst_memory_unmap (new_mem, &sei_info);
  } else {
    new_mem = gst_memory_ref (sei);
  }

  /* insert sei */
  gst_buffer_append_memory (new_buffer, new_mem);

  /* copy the rest */
  if (!gst_buffer_copy_into (new_buffer, au,
          GST_BUFFER_COPY_MEMORY, nalu.sc_offset, -1)) {
    GST_ERROR ("Failed to copy buffer");
    gst_clear_buffer (&new_buffer);
    goto out;
  }

out:
  return new_buffer;
}

/**
 * gst_h265_parser_insert_sei:
 * @parser: a #GstH265Parser
 * @au: (transfer none): a #GstBuffer containing AU data
 * @sei: (transfer none): a #GstMemory containing a SEI nal
 *
 * Copy @au into new #GstBuffer and insert @sei into the #GstBuffer.
 * The validation for completeness of @au and @sei is caller's responsibility.
 * Both @au and @sei must be byte-stream formatted
 *
 * Returns: (transfer full) (nullable): a SEI inserted #GstBuffer or %NULL
 *   if cannot figure out proper position to insert a @sei
 *
 * Since: 1.18
 */
GstBuffer *
gst_h265_parser_insert_sei (GstH265Parser * parser, GstBuffer * au,
    GstMemory * sei)
{
  g_return_val_if_fail (parser != NULL, NULL);
  g_return_val_if_fail (GST_IS_BUFFER (au), NULL);
  g_return_val_if_fail (sei != NULL, NULL);

  /* the size of start code prefix (3 or 4) is not matter since it will be
   * scanned */
  return gst_h265_parser_insert_sei_internal (parser, 4, FALSE, au, sei);
}

/**
 * gst_h265_parser_insert_sei_hevc:
 * @parser: a #GstH265Parser
 * @nal_length_size: a size of nal length field, allowed range is [1, 4]
 * @au: (transfer none): a #GstBuffer containing AU data
 * @sei: (transfer none): a #GstMemory containing a SEI nal
 *
 * Copy @au into new #GstBuffer and insert @sei into the #GstBuffer.
 * The validation for completeness of @au and @sei is caller's responsibility.
 * Nal prefix type of both @au and @sei must be packetized, and
 * also the size of nal length field must be identical to @nal_length_size
 *
 * Returns: (transfer full) (nullable): a SEI inserted #GstBuffer or %NULL
 *   if cannot figure out proper position to insert a @sei
 *
 * Since: 1.18
 */
GstBuffer *
gst_h265_parser_insert_sei_hevc (GstH265Parser * parser, guint8 nal_length_size,
    GstBuffer * au, GstMemory * sei)
{
  g_return_val_if_fail (parser != NULL, NULL);
  g_return_val_if_fail (nal_length_size > 0 && nal_length_size < 5, NULL);
  g_return_val_if_fail (GST_IS_BUFFER (au), NULL);
  g_return_val_if_fail (sei != NULL, NULL);

  return gst_h265_parser_insert_sei_internal (parser, nal_length_size, TRUE,
      au, sei);
}

/**
 * gst_h265_get_profile_from_sps:
 * @sps: a #GstH265SPS
 *
 * Return the H265 profile from @sps.
 *
 * Returns: a #GstH265Profile
 * Since: 1.20
 */
GstH265Profile
gst_h265_get_profile_from_sps (GstH265SPS * sps)
{
  GstH265Profile profiles[GST_H265_PROFILE_MAX] = { GST_H265_PROFILE_INVALID, };
  GstH265ProfileTierLevel tmp_ptl;
  guint i, len = 0;
  guint chroma_format_idc, bit_depth_luma, bit_depth_chroma;

  g_return_val_if_fail (sps != NULL, GST_H265_PROFILE_INVALID);

  tmp_ptl = sps->profile_tier_level;
  chroma_format_idc = sps->chroma_format_idc;
  bit_depth_luma = sps->bit_depth_luma_minus8 + 8;
  bit_depth_chroma = sps->bit_depth_chroma_minus8 + 8;

  gst_h265_profile_tier_level_get_profiles (&sps->profile_tier_level, profiles,
      &len);

  for (i = 0; i < len && i < G_N_ELEMENTS (profiles); i++) {
    switch (profiles[i]) {
      case GST_H265_PROFILE_INVALID:
        break;
      case GST_H265_PROFILE_MAIN:
      case GST_H265_PROFILE_MAIN_STILL_PICTURE:
        /* A.3.2 or A.3.5 */
        if (chroma_format_idc == 1
            && bit_depth_luma == 8 && bit_depth_chroma == 8)
          return profiles[i];
        break;
      case GST_H265_PROFILE_MAIN_10:
        /* A.3.3 */
        if (chroma_format_idc == 1
            && bit_depth_luma >= 8 && bit_depth_luma <= 10
            && bit_depth_chroma >= 8 && bit_depth_chroma <= 10)
          return profiles[i];
        break;
      default:
        return profiles[i];
    }
  }

  /* Invalid profile: */
  /* Set the conformance indicators based on chroma_format_idc / bit_depth */
  switch (chroma_format_idc) {
    case 0:
      tmp_ptl.max_monochrome_constraint_flag = 1;
      tmp_ptl.max_420chroma_constraint_flag = 1;
      tmp_ptl.max_422chroma_constraint_flag = 1;
      break;

    case 1:
      tmp_ptl.max_monochrome_constraint_flag = 0;
      tmp_ptl.max_420chroma_constraint_flag = 1;
      tmp_ptl.max_422chroma_constraint_flag = 1;
      break;

    case 2:
      tmp_ptl.max_monochrome_constraint_flag = 0;
      tmp_ptl.max_420chroma_constraint_flag = 0;
      tmp_ptl.max_422chroma_constraint_flag = 1;
      break;

    case 3:
      tmp_ptl.max_monochrome_constraint_flag = 0;
      tmp_ptl.max_420chroma_constraint_flag = 0;
      tmp_ptl.max_422chroma_constraint_flag = 0;
      break;

    default:
      g_assert_not_reached ();
      break;
  }

  tmp_ptl.max_8bit_constraint_flag = 1;
  tmp_ptl.max_10bit_constraint_flag = 1;
  tmp_ptl.max_12bit_constraint_flag = 1;
  tmp_ptl.max_14bit_constraint_flag = 1;

  if (bit_depth_luma > 8 || bit_depth_chroma > 8)
    tmp_ptl.max_8bit_constraint_flag = 0;

  if (bit_depth_luma > 10 || bit_depth_chroma > 10)
    tmp_ptl.max_10bit_constraint_flag = 0;

  if (bit_depth_luma > 12 || bit_depth_chroma > 12)
    tmp_ptl.max_12bit_constraint_flag = 0;

  if (tmp_ptl.profile_idc == GST_H265_PROFILE_IDC_HIGH_THROUGHPUT
      || tmp_ptl.profile_idc == GST_H265_PROFILE_IDC_SCREEN_CONTENT_CODING
      || tmp_ptl.profile_idc ==
      GST_H265_PROFILE_IDC_SCALABLE_FORMAT_RANGE_EXTENSION
      || tmp_ptl.profile_idc ==
      GST_H265_PROFILE_IDC_HIGH_THROUGHPUT_SCREEN_CONTENT_CODING_EXTENSION
      || tmp_ptl.profile_compatibility_flag[5]
      || tmp_ptl.profile_compatibility_flag[9]
      || tmp_ptl.profile_compatibility_flag[10]
      || tmp_ptl.profile_compatibility_flag[11]) {
    if (bit_depth_luma > 14 || bit_depth_chroma > 14)
      tmp_ptl.max_14bit_constraint_flag = 0;
  } else {
    tmp_ptl.max_14bit_constraint_flag = 0;
  }

  /* first profile of the synthetic ptl */
  return gst_h265_profile_tier_level_get_profile (&tmp_ptl);
}

/* *INDENT-OFF* */
static void
gst_clear_h265_decoder_config_record_nalu_array (
    GstH265DecoderConfigRecordNalUnitArray * array)
{
  if (!array)
    return;

  if (array->nalu)
    g_array_unref (array->nalu);
}
/* *INDENT-ON* */

/**
 * gst_h265_decoder_config_record_free:
 * @config: (nullable): a #GstH265DecoderConfigRecord data
 *
 * Free @config data
 *
 * Since: 1.24
 */
void
gst_h265_decoder_config_record_free (GstH265DecoderConfigRecord * config)
{
  if (!config)
    return;

  if (config->nalu_array)
    g_array_unref (config->nalu_array);

  g_free (config);
}

static GstH265DecoderConfigRecord *
gst_h265_decoder_config_record_new (void)
{
  GstH265DecoderConfigRecord *config;

  config = g_new0 (GstH265DecoderConfigRecord, 1);
  config->nalu_array = g_array_new (FALSE,
      FALSE, sizeof (GstH265DecoderConfigRecordNalUnitArray));
  g_array_set_clear_func (config->nalu_array,
      (GDestroyNotify) gst_clear_h265_decoder_config_record_nalu_array);

  return config;
}

/**
 * gst_h265_parser_parse_decoder_config_record:
 * @parser: a #GstH265Parser
 * @data: the data to parse
 * @size: the size of @data
 * @config: (out): parsed #GstH265DecoderConfigRecord data
 *
 * Parses HEVCDecoderConfigurationRecord data and fill into @config.
 * The caller must free @config via gst_h265_decoder_config_record_free()
 *
 * This method does not parse VPS, SPS and PPS and therefore the caller needs to
 * parse each NAL unit via appropriate parsing method.
 *
 * Returns: a #GstH265ParserResult
 *
 * Since: 1.24
 */
GstH265ParserResult
gst_h265_parser_parse_decoder_config_record (GstH265Parser * parser,
    const guint8 * data, gsize size, GstH265DecoderConfigRecord ** config)
{
  GstH265DecoderConfigRecord *ret;
  GstBitReader br;
  GstH265ParserResult result = GST_H265_PARSER_OK;
  guint i;
  guint8 num_of_arrays;

  g_return_val_if_fail (parser != NULL, GST_H265_PARSER_ERROR);
  g_return_val_if_fail (data != NULL, GST_H265_PARSER_ERROR);
  g_return_val_if_fail (config != NULL, GST_H265_PARSER_ERROR);

#define READ_CONFIG_UINT8(val, nbits) G_STMT_START { \
  if (!gst_bit_reader_get_bits_uint8 (&br, (guint8 *) &val, nbits)) { \
    GST_WARNING ("Failed to read " G_STRINGIFY (val)); \
    result = GST_H265_PARSER_ERROR; \
    goto error; \
  } \
} G_STMT_END;

#define READ_CONFIG_UINT16(val, nbits) G_STMT_START { \
  if (!gst_bit_reader_get_bits_uint16 (&br, &val, nbits)) { \
    GST_WARNING ("Failed to read " G_STRINGIFY (val)); \
    result = GST_H265_PARSER_ERROR; \
    goto error; \
  } \
} G_STMT_END;

#define SKIP_CONFIG_BITS(nbits) G_STMT_START { \
  if (!gst_bit_reader_skip (&br, nbits)) { \
    GST_WARNING ("Failed to skip %d bits", nbits); \
    result = GST_H265_PARSER_ERROR; \
    goto error; \
  } \
} G_STMT_END;

  *config = NULL;

  if (size < 23) {
    GST_WARNING ("Too small size hvcC");
    return GST_H265_PARSER_ERROR;
  }

  gst_bit_reader_init (&br, data, size);

  ret = gst_h265_decoder_config_record_new ();

  READ_CONFIG_UINT8 (ret->configuration_version, 8);
  if (ret->configuration_version != 1) {
    GST_WARNING ("Wrong configurationVersion %d", ret->configuration_version);
    /* Must be 1 but allows 0 for backward compatibility.
     * See commit
     * https://gitlab.freedesktop.org/gstreamer/gstreamer/-/commit/63fee31a3f95021fa0bb2429c723f5356c9b3c4b */
    if (ret->configuration_version != 0) {
      result = GST_H265_PARSER_ERROR;
      goto error;
    }
  }

  READ_CONFIG_UINT8 (ret->general_profile_space, 2);
  READ_CONFIG_UINT8 (ret->general_tier_flag, 1);
  READ_CONFIG_UINT8 (ret->general_profile_idc, 5);

  for (i = 0; i < 32; i++)
    READ_CONFIG_UINT8 (ret->general_profile_compatibility_flags[i], 1);

  /* 7.3.3 Profile, tier and level syntax */
  READ_CONFIG_UINT8 (ret->general_progressive_source_flag, 1);
  READ_CONFIG_UINT8 (ret->general_interlaced_source_flag, 1);
  READ_CONFIG_UINT8 (ret->general_non_packed_constraint_flag, 1);
  READ_CONFIG_UINT8 (ret->general_frame_only_constraint_flag, 1);

  if (ret->general_profile_idc == 4 ||
      ret->general_profile_compatibility_flags[4] ||
      ret->general_profile_idc == 5 ||
      ret->general_profile_compatibility_flags[5] ||
      ret->general_profile_idc == 6 ||
      ret->general_profile_compatibility_flags[6] ||
      ret->general_profile_idc == 7 ||
      ret->general_profile_compatibility_flags[7] ||
      ret->general_profile_idc == 8 ||
      ret->general_profile_compatibility_flags[8] ||
      ret->general_profile_idc == 9 ||
      ret->general_profile_compatibility_flags[9] ||
      ret->general_profile_idc == 10 ||
      ret->general_profile_compatibility_flags[10] ||
      ret->general_profile_idc == 11 ||
      ret->general_profile_compatibility_flags[11]) {
    READ_CONFIG_UINT8 (ret->general_max_12bit_constraint_flag, 1);
    READ_CONFIG_UINT8 (ret->general_max_10bit_constraint_flag, 1);
    READ_CONFIG_UINT8 (ret->general_max_8bit_constraint_flag, 1);
    READ_CONFIG_UINT8 (ret->general_max_422chroma_constraint_flag, 1);
    READ_CONFIG_UINT8 (ret->general_max_420chroma_constraint_flag, 1);
    READ_CONFIG_UINT8 (ret->general_max_monochrome_constraint_flag, 1);
    READ_CONFIG_UINT8 (ret->general_intra_constraint_flag, 1);
    READ_CONFIG_UINT8 (ret->general_one_picture_only_constraint_flag, 1);
    READ_CONFIG_UINT8 (ret->general_lower_bit_rate_constraint_flag, 1);

    if (ret->general_profile_idc == 5 ||
        ret->general_profile_compatibility_flags[5] ||
        ret->general_profile_idc == 9 ||
        ret->general_profile_compatibility_flags[9] ||
        ret->general_profile_idc == 10 ||
        ret->general_profile_compatibility_flags[10] ||
        ret->general_profile_idc == 11 ||
        ret->general_profile_compatibility_flags[11]) {
      READ_CONFIG_UINT8 (ret->general_max_14bit_constraint_flag, 1);
      SKIP_CONFIG_BITS (33);
    } else {
      SKIP_CONFIG_BITS (34);
    }
  } else if (ret->general_profile_idc == 2 ||
      ret->general_profile_compatibility_flags[2]) {
    SKIP_CONFIG_BITS (7);
    READ_CONFIG_UINT8 (ret->general_one_picture_only_constraint_flag, 1);

    SKIP_CONFIG_BITS (35);
  } else {
    SKIP_CONFIG_BITS (43);
  }

  if (ret->general_profile_idc == 1 ||
      ret->general_profile_compatibility_flags[1] ||
      ret->general_profile_idc == 2 ||
      ret->general_profile_compatibility_flags[2] ||
      ret->general_profile_idc == 3 ||
      ret->general_profile_compatibility_flags[3] ||
      ret->general_profile_idc == 4 ||
      ret->general_profile_compatibility_flags[4] ||
      ret->general_profile_idc == 5 ||
      ret->general_profile_compatibility_flags[5] ||
      ret->general_profile_idc == 9 ||
      ret->general_profile_compatibility_flags[9] ||
      ret->general_profile_idc == 11 ||
      ret->general_profile_compatibility_flags[11]) {
    READ_CONFIG_UINT8 (ret->general_inbld_flag, 1);
  } else {
    SKIP_CONFIG_BITS (1);
  }

  g_assert (gst_bit_reader_get_pos (&br) == 12 * 8);

  READ_CONFIG_UINT8 (ret->general_level_idc, 8);

  SKIP_CONFIG_BITS (4);
  READ_CONFIG_UINT16 (ret->min_spatial_segmentation_idc, 12);

  SKIP_CONFIG_BITS (6);
  READ_CONFIG_UINT8 (ret->parallelism_type, 2);

  SKIP_CONFIG_BITS (6);
  READ_CONFIG_UINT8 (ret->chroma_format_idc, 2);

  SKIP_CONFIG_BITS (5);
  READ_CONFIG_UINT8 (ret->bit_depth_luma_minus8, 3);

  SKIP_CONFIG_BITS (5);
  READ_CONFIG_UINT8 (ret->bit_depth_chroma_minus8, 3);

  READ_CONFIG_UINT16 (ret->avg_frame_rate, 16);

  READ_CONFIG_UINT8 (ret->constant_frame_rate, 2);
  READ_CONFIG_UINT8 (ret->num_temporal_layers, 3);
  READ_CONFIG_UINT8 (ret->temporal_id_nested, 1);
  READ_CONFIG_UINT8 (ret->length_size_minus_one, 2);
  if (ret->length_size_minus_one == 2) {
    /* "length_size_minus_one + 1" should be 1, 2, or 4 */
    GST_WARNING ("Wrong nal-length-size");
  }

  READ_CONFIG_UINT8 (num_of_arrays, 8);

  g_assert (gst_bit_reader_get_pos (&br) == 23 * 8);
  for (i = 0; i < num_of_arrays; i++) {
    GstH265DecoderConfigRecordNalUnitArray array;
    guint8 nalu_type;
    GstH265NalUnit nalu;
    guint16 num_nalu, j;
    guint offset;

    READ_CONFIG_UINT8 (array.array_completeness, 1);
    SKIP_CONFIG_BITS (1);
    READ_CONFIG_UINT8 (nalu_type, 6);
    array.nal_unit_type = nalu_type;

    READ_CONFIG_UINT16 (num_nalu, 16);

    offset = gst_bit_reader_get_pos (&br) / 8;
    array.nalu = g_array_sized_new (FALSE, FALSE, sizeof (GstH265NalUnit),
        num_nalu);
    for (j = 0; j < num_nalu; j++) {
      result = gst_h265_parser_identify_nalu_hevc (parser, data, offset, size,
          2, &nalu);
      if (result != GST_H265_PARSER_OK) {
        g_array_unref (array.nalu);
        /* Ignores parsing error if this is the last nalu and not an essential
         * nalu for decoding */
        if (i + 1 == num_of_arrays && j + 1 == num_nalu &&
            nalu_type != GST_H265_NAL_VPS && nalu_type != GST_H265_NAL_SPS &&
            nalu_type != GST_H265_NAL_PPS) {
          GST_WARNING ("Couldn't parse the last nalu, type %d at array %d / %d",
              nalu_type, i, j);
          goto out;
        }
        goto error;
      }

      g_array_append_val (array.nalu, nalu);
      offset = nalu.offset + nalu.size;
    }

    g_array_append_val (ret->nalu_array, array);

    if (i != num_of_arrays - 1 && !gst_bit_reader_set_pos (&br, offset * 8)) {
      GST_WARNING ("Not enough byte for NAL reading");
      result = GST_H265_PARSER_ERROR;
      goto error;
    }
  }

out:
  *config = ret;
  return GST_H265_PARSER_OK;

error:
  {
    gst_h265_decoder_config_record_free (ret);
    return result;
  }

#undef READ_CONFIG_UINT8
#undef READ_CONFIG_UINT16
#undef SKIP_CONFIG_BITS
}
