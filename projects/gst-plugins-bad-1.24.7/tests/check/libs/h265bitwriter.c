/* GStreamer
 *  Copyright (C) 2022 Intel Corporation
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

#include <gst/check/gstcheck.h>
#include <gst/codecparsers/gsth265bitwriter.h>

#define DEFAULT_SCALING_LIST0  \
  { 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 }
#define SCALING_LIST0_4x4_0  \
  { 7, 8, 12, 25, 16, 22, 17, 17, 16, 18, 26, 26, 26, 26, 26, 26 }
#define SCALING_LIST0_4x4_1  \
  { 9, 8, 12, 25, 16, 22, 27, 17, 16, 38, 36, 26, 36, 26, 36, 26 }

#define DEFAULT_SCALING_LIST1  \
  { 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 16,    \
    17, 16, 17, 18, 17, 18, 18, 17, 18, 21, 19, 20,    \
    21, 20, 19, 21, 24, 22, 22, 24, 24, 22, 22, 24,    \
    25, 25, 27, 30, 27, 25, 25, 29, 31, 35, 35, 31,    \
    29, 36, 41, 44, 41, 36, 47, 54, 54, 47, 65, 70,    \
    65, 88, 88, 115 }
#define DEFAULT_SCALING_LIST2  \
  { 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17,    \
    17, 17, 17, 18, 18, 18, 18, 18, 18, 20, 20, 20,    \
    20, 20, 20, 20, 24, 24, 24, 24, 24, 24, 24, 24,    \
    25, 25, 25, 25, 25, 25, 25, 28, 28, 28, 28, 28,    \
    28, 33, 33, 33, 33, 33, 41, 41, 41, 41, 54, 54,    \
    54, 71, 71, 91 }
#define SCALING_LIST_16x16_0  \
  { 18, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17,    \
    17, 17, 17, 18, 18, 18, 18, 18, 18, 20, 20, 20,    \
    21, 20, 20, 20, 24, 24, 27, 24, 24, 24, 24, 25,    \
    25, 25, 25, 25, 25, 25, 23, 28, 28, 28, 28, 28,    \
    28, 33, 33, 33, 33, 33, 21, 21, 51, 51, 54, 54,    \
    54, 88, 71, 81 }
#define SCALING_LIST_16x16_1  \
  { 10, 10, 16, 16, 16, 16, 16, 16, 16, 16, 17, 12,    \
    17, 16, 17, 18, 17, 18, 18, 17, 18, 21, 19, 20,    \
    21, 22, 39, 21, 24, 22, 22, 14, 14, 18, 22, 32,    \
    25, 25, 27, 30, 27, 21, 25, 29, 31, 35, 35, 31,    \
    29, 36, 41, 64, 41, 56, 43, 54, 54, 47, 65, 70,    \
    65, 88, 105, 115 }

/* *INDENT-OFF* */
static const GstH265VPS vps = {
  .id = 1,
  .base_layer_internal_flag = 1,
  .base_layer_available_flag = 1,
  .max_layers_minus1 = 0,
  .max_sub_layers_minus1 = 0,
  .temporal_id_nesting_flag = 1,

  .profile_tier_level = {
    .profile_space = 0,
    .tier_flag = 0,
    .profile_idc = 4,
    .profile_compatibility_flag = { 0, 0, 0, 0, 1, },

    .progressive_source_flag = 1,
    .interlaced_source_flag = 0,
    .non_packed_constraint_flag = 1,
    .frame_only_constraint_flag = 1,

    .max_12bit_constraint_flag = 1,
    .max_10bit_constraint_flag = 1,
    .max_8bit_constraint_flag = 0,
    .max_422chroma_constraint_flag = 1,
    .max_420chroma_constraint_flag = 0,
    .max_monochrome_constraint_flag = 0,
    .intra_constraint_flag = 0,
    .one_picture_only_constraint_flag = 0,
    .lower_bit_rate_constraint_flag = 1,

    .level_idc = 123,

    .sub_layer_profile_present_flag = { 0, },
    .sub_layer_level_present_flag = { 0, },
  },

  .sub_layer_ordering_info_present_flag = 1,
  .max_dec_pic_buffering_minus1 = { 5, },
  .max_num_reorder_pics = { 2, },
  .max_latency_increase_plus1 = { 0, },

  .max_layer_id = 0,
  .num_layer_sets_minus1 = 0,

  .timing_info_present_flag = 1,
  .num_units_in_tick = 1001,
  .time_scale = 60000,
  .poc_proportional_to_timing_flag = 1,
  .num_ticks_poc_diff_one_minus1 = 0,

  .num_hrd_parameters = 1,
  .hrd_layer_set_idx = 0,
  .cprms_present_flag = 1,
  .hrd_params = {
    .nal_hrd_parameters_present_flag = 0,
    .vcl_hrd_parameters_present_flag = 1,
    .sub_pic_hrd_params_present_flag = 0,

    .bit_rate_scale = 0,
    .cpb_size_scale = 0,
    .cpb_size_du_scale = 0,

    .initial_cpb_removal_delay_length_minus1 = 23,
    .au_cpb_removal_delay_length_minus1 = 21,
    .dpb_output_delay_length_minus1 = 23,

    .fixed_pic_rate_general_flag = { 1, },
    .fixed_pic_rate_within_cvs_flag = { 1, },

    .elemental_duration_in_tc_minus1 = { 0, },
    .low_delay_hrd_flag = { 0, },
    .cpb_cnt_minus1 = { 0, },

    .sublayer_hrd_params = {
      { .bit_rate_value_minus1 = { 0, },
        .cpb_size_value_minus1 = { 0, },
        .cpb_size_du_value_minus1 = { 0, },
        .bit_rate_du_value_minus1 = { 0, },
        .cbr_flag = { 1, },
      },
    }
  },

  .vps_extension = 0,
};

static const GstH265SPS sps = {
  .id = 2,
  .max_sub_layers_minus1 = 0,
  .temporal_id_nesting_flag = 1,

  .profile_tier_level = {
    .profile_space = 0,
    .tier_flag = 0,
    .profile_idc = 4,
    .profile_compatibility_flag = { 0, 0, 0, 0, 1, },

    .progressive_source_flag = 1,
    .interlaced_source_flag = 0,
    .non_packed_constraint_flag = 1,
    .frame_only_constraint_flag = 1,

    .max_12bit_constraint_flag = 1,
    .max_10bit_constraint_flag = 1,
    .max_8bit_constraint_flag = 0,
    .max_422chroma_constraint_flag = 1,
    .max_420chroma_constraint_flag = 0,
    .max_monochrome_constraint_flag = 0,
    .intra_constraint_flag = 0,
    .one_picture_only_constraint_flag = 0,
    .lower_bit_rate_constraint_flag = 1,

    .level_idc = 123,

    .sub_layer_profile_present_flag = { 0, },
    .sub_layer_level_present_flag = { 0, },
  },

  .chroma_format_idc = 2,
  .pic_width_in_luma_samples = 192,
  .pic_height_in_luma_samples = 256,
  .conformance_window_flag = 1,
  .conf_win_left_offset = 0,
  .conf_win_right_offset = 8,
  .conf_win_top_offset = 0,
  .conf_win_bottom_offset = 56,
  .bit_depth_luma_minus8 = 0,
  .bit_depth_chroma_minus8 = 0,
  .log2_max_pic_order_cnt_lsb_minus4 = 4,

  .sub_layer_ordering_info_present_flag = 1,
  .max_dec_pic_buffering_minus1 = { 5, },
  .max_num_reorder_pics = { 2, },
  .max_latency_increase_plus1 = { 0, },

  .log2_min_luma_coding_block_size_minus3 = 2,
  .log2_diff_max_min_luma_coding_block_size = 1,
  .log2_min_transform_block_size_minus2 = 0,
  .log2_diff_max_min_transform_block_size = 3,
  .max_transform_hierarchy_depth_inter = 3,
  .max_transform_hierarchy_depth_intra = 3,

  .scaling_list_enabled_flag = 1,
  .scaling_list_data_present_flag = 1,

  /* Set it manually. */
  .scaling_list = {
    .scaling_list_dc_coef_minus8_16x16 = { 8, 15, 9, 12, 18, 8 },
    .scaling_list_dc_coef_minus8_32x32 = { 8, 6 },

    .scaling_lists_4x4 = {
      SCALING_LIST0_4x4_0,
      DEFAULT_SCALING_LIST0,
      SCALING_LIST0_4x4_1,
      DEFAULT_SCALING_LIST0,
      SCALING_LIST0_4x4_0,
      SCALING_LIST0_4x4_1
    },
    .scaling_lists_8x8 = {
      DEFAULT_SCALING_LIST1,
      SCALING_LIST_16x16_0,
      SCALING_LIST_16x16_0,
      DEFAULT_SCALING_LIST2,
      SCALING_LIST_16x16_1,
      SCALING_LIST_16x16_0
    },
    .scaling_lists_16x16 = {
      DEFAULT_SCALING_LIST1,
      SCALING_LIST_16x16_0,
      DEFAULT_SCALING_LIST1,
      DEFAULT_SCALING_LIST2,
      SCALING_LIST_16x16_1,
      DEFAULT_SCALING_LIST2
    },
    .scaling_lists_32x32 = {
      DEFAULT_SCALING_LIST1,
      DEFAULT_SCALING_LIST2
    }
  },

  .amp_enabled_flag = 1,
  .sample_adaptive_offset_enabled_flag = 1,
  .pcm_enabled_flag = 1,
  .pcm_sample_bit_depth_luma_minus1 = 7,
  .pcm_sample_bit_depth_chroma_minus1 = 7,
  .log2_min_pcm_luma_coding_block_size_minus3 = 2,
  .log2_diff_max_min_pcm_luma_coding_block_size = 0,
  .pcm_loop_filter_disabled_flag = 0,

  .num_short_term_ref_pic_sets = 3,
  .short_term_ref_pic_set = {
    {
      .inter_ref_pic_set_prediction_flag = 0,
      .NumDeltaPocs = 1,

      .NumNegativePics = 0,
      .DeltaPocS0 = { 0, },
      .UsedByCurrPicS0 = { 0, },

      .NumPositivePics = 1,
      .DeltaPocS1 = { 3, },
      .UsedByCurrPicS1 = { 1, },
    },
    {
      .inter_ref_pic_set_prediction_flag = 0,
      .NumDeltaPocs = 3,

      .NumNegativePics = 2,
      .DeltaPocS0 = { -1, -3, },
      .UsedByCurrPicS0 = { 1, 1, },

      .NumPositivePics = 1,
      .DeltaPocS1 = { 2, },
      .UsedByCurrPicS1 = { 1, },
    },
    {
      .inter_ref_pic_set_prediction_flag = 0,
      .NumDeltaPocs = 5,

      .NumNegativePics = 3,
      .DeltaPocS0 = { -1, -2, -4, },
      .UsedByCurrPicS0 = { 1, 0, 1, },

      .NumPositivePics = 2,
      .DeltaPocS1 = { 2, 7, },
      .UsedByCurrPicS1 = { 0, 1, },
    },
  },

  .long_term_ref_pics_present_flag = 0,

  .temporal_mvp_enabled_flag = 1,
  .strong_intra_smoothing_enabled_flag = 0,

  .vui_parameters_present_flag = 1,
  .vui_params = {
    .aspect_ratio_info_present_flag = 0,

    .overscan_info_present_flag = 1,
    .overscan_appropriate_flag = 0,
    .video_signal_type_present_flag = 1,
    .video_format = 5,
    .video_full_range_flag = 0,
    .colour_description_present_flag = 0,

    .chroma_loc_info_present_flag = 1,
    .chroma_sample_loc_type_top_field = 0,
    .chroma_sample_loc_type_bottom_field = 0,

    .neutral_chroma_indication_flag = 0,
    .field_seq_flag = 0,
    .frame_field_info_present_flag = 0,
    .default_display_window_flag = 1,
    .def_disp_win_left_offset = 0,
    .def_disp_win_right_offset = 0,
    .def_disp_win_top_offset = 0,
    .def_disp_win_bottom_offset = 56,

    .timing_info_present_flag = 1,
    .num_units_in_tick = 1,
    .time_scale = 60,

    .poc_proportional_to_timing_flag = 0,

    .hrd_parameters_present_flag = 1,
    .hrd_params = {
      .nal_hrd_parameters_present_flag = 0,
      .vcl_hrd_parameters_present_flag = 1,
      .sub_pic_hrd_params_present_flag = 0,

      .bit_rate_scale = 0,
      .cpb_size_scale = 0,

      .initial_cpb_removal_delay_length_minus1 = 31,
      .au_cpb_removal_delay_length_minus1 = 23,
      .dpb_output_delay_length_minus1 = 23,

      .fixed_pic_rate_general_flag = { 1, },
      .fixed_pic_rate_within_cvs_flag = { 1, },
      .elemental_duration_in_tc_minus1 = { 0, },
      .cpb_cnt_minus1 = { 0, },

      .sublayer_hrd_params = {
        {
          .bit_rate_value_minus1 = { 108353, },
          .cpb_size_value_minus1 = { 1602517, },
          .cbr_flag = { 1, },
        },
      }
    },

    .bitstream_restriction_flag = 1,
    .tiles_fixed_structure_flag = 0,
    .motion_vectors_over_pic_boundaries_flag = 1,
    .restricted_ref_pic_lists_flag = 0,
    .min_spatial_segmentation_idc = 0,
    .max_bytes_per_pic_denom = 0,
    .max_bits_per_min_cu_denom = 0,
    .log2_max_mv_length_horizontal = 15,
    .log2_max_mv_length_vertical = 15,
  },

  .sps_extension_flag = 1,
  .sps_range_extension_flag = 1,
  .sps_multilayer_extension_flag = 0,
  .sps_3d_extension_flag = 0,
  .sps_scc_extension_flag = 0,

  .sps_extension_params = {
    .transform_skip_rotation_enabled_flag = 0,
    .transform_skip_context_enabled_flag = 0,
    .implicit_rdpcm_enabled_flag = 0,
    .explicit_rdpcm_enabled_flag = 0,
    .extended_precision_processing_flag = 0,
    .intra_smoothing_disabled_flag = 0,
    .high_precision_offsets_enabled_flag = 0,
    .persistent_rice_adaptation_enabled_flag = 0,
    .cabac_bypass_alignment_enabled_flag = 0,
  },

  .vps = (GstH265VPS *) &vps,
};

static const GstH265PPS pps = {
  .id = 1,
  .dependent_slice_segments_enabled_flag = 1,
  .output_flag_present_flag = 0,
  .num_extra_slice_header_bits = 0,
  .sign_data_hiding_enabled_flag = 1,
  .cabac_init_present_flag = 0,
  .num_ref_idx_l0_default_active_minus1 = 9,
  .num_ref_idx_l1_default_active_minus1 = 4,
  .init_qp_minus26 = -13,
  .constrained_intra_pred_flag = 0,
  .transform_skip_enabled_flag = 1,
  .cu_qp_delta_enabled_flag = 1,
  .diff_cu_qp_delta_depth = 0,
  .cb_qp_offset = 4,
  .cr_qp_offset = 5,
  .slice_chroma_qp_offsets_present_flag = 0,
  .weighted_pred_flag = 1,
  .weighted_bipred_flag = 0,
  .transquant_bypass_enabled_flag = 0,
  .tiles_enabled_flag = 1,
  .entropy_coding_sync_enabled_flag = 0,
  .num_tile_columns_minus1 = 1,
  .num_tile_rows_minus1 = 2,
  .uniform_spacing_flag = 0,
  .column_width_minus1 = { 2, },
  .row_height_minus1 = { 1, 0 },
  .loop_filter_across_tiles_enabled_flag = 1,
  .loop_filter_across_slices_enabled_flag = 1,
  .deblocking_filter_control_present_flag = 1,
  .deblocking_filter_override_enabled_flag = 1,
  .deblocking_filter_disabled_flag = 0,
  .beta_offset_div2 = 3,
  .tc_offset_div2 = 6,

  .scaling_list_data_present_flag = 1,
  .scaling_list = {
    .scaling_list_dc_coef_minus8_16x16 = { 8, 8, 7, 15, 12, 8 },
    .scaling_list_dc_coef_minus8_32x32 = { 8, 6 },

    .scaling_lists_4x4 = {
      DEFAULT_SCALING_LIST0,
      SCALING_LIST0_4x4_0,
      SCALING_LIST0_4x4_1,
      SCALING_LIST0_4x4_1,
      SCALING_LIST0_4x4_0,
      DEFAULT_SCALING_LIST0,
    },
    .scaling_lists_8x8 = {
      DEFAULT_SCALING_LIST1,
      SCALING_LIST_16x16_0,
      SCALING_LIST_16x16_0,
      SCALING_LIST_16x16_1,
      DEFAULT_SCALING_LIST2,
      SCALING_LIST_16x16_0
    },
    .scaling_lists_16x16 = {
      SCALING_LIST_16x16_1,
      DEFAULT_SCALING_LIST1,
      SCALING_LIST_16x16_0,
      SCALING_LIST_16x16_1,
      DEFAULT_SCALING_LIST2,
      SCALING_LIST_16x16_1,
    },
    .scaling_lists_32x32 = {
      DEFAULT_SCALING_LIST1,
      SCALING_LIST_16x16_0,
    }
  },

  .lists_modification_present_flag = 0,
  .log2_parallel_merge_level_minus2 = 3,
  .slice_segment_header_extension_present_flag = 0,
  .pps_extension_flag = 1,
  .pps_range_extension_flag = 1,
  .pps_multilayer_extension_flag = 0,
  .pps_3d_extension_flag = 0,
  .pps_scc_extension_flag = 0,

  .pps_extension_params = {
    .log2_max_transform_skip_block_size_minus2 = 0,
    .cross_component_prediction_enabled_flag = 0,
    .chroma_qp_offset_list_enabled_flag = 1,
    .diff_cu_chroma_qp_offset_depth = 1,
    .chroma_qp_offset_list_len_minus1 = 5,
    .cb_qp_offset_list = { -2, -7, -1, 3, 4, 1 },
    .cr_qp_offset_list = { -2, 6, 5, 6, 8, 9 },
    .log2_sao_offset_scale_luma = 0,
    .log2_sao_offset_scale_chroma = 0,
  },

  .sps = (GstH265SPS *) &sps,
};

static const GstH265SliceHdr slice_hdr = {
  .first_slice_segment_in_pic_flag = 1,
  .type = 0,
  .pic_order_cnt_lsb = 53,
  .short_term_ref_pic_set_sps_flag = 1,
  .short_term_ref_pic_set_idx = 2,
  .temporal_mvp_enabled_flag = 1,
  .sao_luma_flag = 1,
  .sao_chroma_flag = 1,
  .num_ref_idx_active_override_flag = 1,
  .num_ref_idx_l0_active_minus1 = 3,
  .num_ref_idx_l1_active_minus1 = 2,
  .mvd_l1_zero_flag = 0,
  .collocated_ref_idx = 1,
  .five_minus_max_num_merge_cand = 1,
  .qp_delta = 24,
  .cu_chroma_qp_offset_enabled_flag = 0,
  .deblocking_filter_override_flag = 1,
  .deblocking_filter_disabled_flag = 0,
  .beta_offset_div2 = 5,
  .tc_offset_div2 = -1,
  .num_entry_point_offsets = 0,

  .pps = (GstH265PPS *) &pps,
};
/* *INDENT-ON* */

GST_START_TEST (test_h265_bitwriter_vps_sps_pps_slice_hdr)
{
  GstH265ParserResult res;
  gboolean ret;
  GstH265NalUnit nalu;
  GstH265Parser *const parser = gst_h265_parser_new ();
  GstH265VPS vps_parsed;
  GstH265SPS sps_parsed;
  GstH265PPS pps_parsed;
  GstH265SliceHdr slice_parsed;
  guint8 header_data[2048] = { 0, };
  guint8 header_nal[2048] = { 0, };
  guint size;
  guint32 nal_size;
  guint i, j;

  size = sizeof (header_data);
  ret = gst_h265_bit_writer_vps (&vps, TRUE, header_data, &size);
  fail_if (ret != GST_H265_BIT_WRITER_OK);

  nal_size = sizeof (header_nal);
  ret = gst_h265_bit_writer_convert_to_nal (4, FALSE, TRUE, FALSE,
      header_data, size * 8, header_nal, &nal_size);
  fail_if (ret != GST_H265_BIT_WRITER_OK);
  fail_if (nal_size < size);

  /* Parse it again */
  res = gst_h265_parser_identify_nalu (parser, header_nal, 0,
      sizeof (header_nal), &nalu);
  assert_equals_int (res, GST_H265_PARSER_NO_NAL_END);

  res = gst_h265_parser_parse_vps (parser, &nalu, &vps_parsed);
  assert_equals_int (res, GST_H265_PARSER_OK);

  /* We can not do simply memcmp, the parser may set some default
     value for the fields which are not used for writing. */
#define CHECK_FIELD(FIELD)  fail_if(vps_parsed.FIELD != vps.FIELD)
  CHECK_FIELD (id);
  CHECK_FIELD (base_layer_internal_flag);
  CHECK_FIELD (base_layer_available_flag);
  CHECK_FIELD (max_layers_minus1);
  CHECK_FIELD (max_sub_layers_minus1);
  CHECK_FIELD (temporal_id_nesting_flag);
  CHECK_FIELD (profile_tier_level.profile_space);
  CHECK_FIELD (profile_tier_level.tier_flag);
  CHECK_FIELD (profile_tier_level.profile_idc);
  CHECK_FIELD (profile_tier_level.profile_compatibility_flag[0]);
  CHECK_FIELD (profile_tier_level.profile_compatibility_flag[1]);
  CHECK_FIELD (profile_tier_level.profile_compatibility_flag[2]);
  CHECK_FIELD (profile_tier_level.profile_compatibility_flag[3]);
  CHECK_FIELD (profile_tier_level.profile_compatibility_flag[4]);
  CHECK_FIELD (profile_tier_level.profile_compatibility_flag[5]);
  CHECK_FIELD (profile_tier_level.progressive_source_flag);
  CHECK_FIELD (profile_tier_level.interlaced_source_flag);
  CHECK_FIELD (profile_tier_level.non_packed_constraint_flag);
  CHECK_FIELD (profile_tier_level.frame_only_constraint_flag);
  CHECK_FIELD (profile_tier_level.max_12bit_constraint_flag);
  CHECK_FIELD (profile_tier_level.max_10bit_constraint_flag);
  CHECK_FIELD (profile_tier_level.max_8bit_constraint_flag);
  CHECK_FIELD (profile_tier_level.max_422chroma_constraint_flag);
  CHECK_FIELD (profile_tier_level.max_420chroma_constraint_flag);
  CHECK_FIELD (profile_tier_level.max_monochrome_constraint_flag);
  CHECK_FIELD (profile_tier_level.intra_constraint_flag);
  CHECK_FIELD (profile_tier_level.one_picture_only_constraint_flag);
  CHECK_FIELD (profile_tier_level.lower_bit_rate_constraint_flag);
  CHECK_FIELD (profile_tier_level.level_idc);
  CHECK_FIELD (sub_layer_ordering_info_present_flag);
  CHECK_FIELD (max_dec_pic_buffering_minus1[0]);
  CHECK_FIELD (max_num_reorder_pics[0]);
  CHECK_FIELD (max_latency_increase_plus1[0]);
  CHECK_FIELD (max_layer_id);
  CHECK_FIELD (num_layer_sets_minus1);
  CHECK_FIELD (timing_info_present_flag);
  CHECK_FIELD (num_units_in_tick);
  CHECK_FIELD (time_scale);
  CHECK_FIELD (poc_proportional_to_timing_flag);
  CHECK_FIELD (num_ticks_poc_diff_one_minus1);
  CHECK_FIELD (num_hrd_parameters);
  CHECK_FIELD (hrd_layer_set_idx);
  CHECK_FIELD (cprms_present_flag);
  CHECK_FIELD (hrd_params.nal_hrd_parameters_present_flag);
  CHECK_FIELD (hrd_params.vcl_hrd_parameters_present_flag);
  CHECK_FIELD (hrd_params.sub_pic_hrd_params_present_flag);
  CHECK_FIELD (hrd_params.bit_rate_scale);
  CHECK_FIELD (hrd_params.cpb_size_scale);
  CHECK_FIELD (hrd_params.cpb_size_du_scale);
  CHECK_FIELD (hrd_params.initial_cpb_removal_delay_length_minus1);
  CHECK_FIELD (hrd_params.au_cpb_removal_delay_length_minus1);
  CHECK_FIELD (hrd_params.dpb_output_delay_length_minus1);
  CHECK_FIELD (hrd_params.fixed_pic_rate_general_flag[0]);
  CHECK_FIELD (hrd_params.fixed_pic_rate_within_cvs_flag[0]);
  CHECK_FIELD (hrd_params.elemental_duration_in_tc_minus1[0]);
  CHECK_FIELD (hrd_params.low_delay_hrd_flag[0]);
  CHECK_FIELD (hrd_params.cpb_cnt_minus1[0]);
  CHECK_FIELD (hrd_params.sublayer_hrd_params[0].bit_rate_value_minus1[0]);
  CHECK_FIELD (hrd_params.sublayer_hrd_params[0].cpb_size_value_minus1[0]);
  CHECK_FIELD (hrd_params.sublayer_hrd_params[0].cpb_size_du_value_minus1[0]);
  CHECK_FIELD (hrd_params.sublayer_hrd_params[0].bit_rate_du_value_minus1[0]);
  CHECK_FIELD (hrd_params.sublayer_hrd_params[0].cbr_flag[0]);
  CHECK_FIELD (vps_extension);
#undef CHECK_FIELD

  memset (header_data, 0, sizeof (header_data));
  memset (header_nal, 0, sizeof (header_nal));

  size = sizeof (header_data);
  ret = gst_h265_bit_writer_sps (&sps, TRUE, header_data, &size);
  fail_if (ret != GST_H265_BIT_WRITER_OK);

  nal_size = sizeof (header_nal);
  ret = gst_h265_bit_writer_convert_to_nal (4, FALSE, TRUE, FALSE,
      header_data, size * 8, header_nal, &nal_size);
  fail_if (ret != GST_H265_BIT_WRITER_OK);
  fail_if (nal_size < size);

  /* Parse it again */
  res = gst_h265_parser_identify_nalu (parser, header_nal, 0,
      sizeof (header_nal), &nalu);
  assert_equals_int (res, GST_H265_PARSER_NO_NAL_END);

  res = gst_h265_parser_parse_sps (parser, &nalu, &sps_parsed, TRUE);
  assert_equals_int (res, GST_H265_PARSER_OK);

#define CHECK_FIELD(FIELD)  fail_if(sps_parsed.FIELD != sps.FIELD)
  CHECK_FIELD (id);
  CHECK_FIELD (max_sub_layers_minus1);
  CHECK_FIELD (temporal_id_nesting_flag);
  CHECK_FIELD (profile_tier_level.profile_space);
  CHECK_FIELD (profile_tier_level.tier_flag);
  CHECK_FIELD (profile_tier_level.profile_idc);
  CHECK_FIELD (profile_tier_level.profile_compatibility_flag[0]);
  CHECK_FIELD (profile_tier_level.profile_compatibility_flag[1]);
  CHECK_FIELD (profile_tier_level.profile_compatibility_flag[2]);
  CHECK_FIELD (profile_tier_level.profile_compatibility_flag[3]);
  CHECK_FIELD (profile_tier_level.profile_compatibility_flag[4]);
  CHECK_FIELD (profile_tier_level.profile_compatibility_flag[5]);
  CHECK_FIELD (profile_tier_level.progressive_source_flag);
  CHECK_FIELD (profile_tier_level.interlaced_source_flag);
  CHECK_FIELD (profile_tier_level.non_packed_constraint_flag);
  CHECK_FIELD (profile_tier_level.frame_only_constraint_flag);
  CHECK_FIELD (profile_tier_level.max_12bit_constraint_flag);
  CHECK_FIELD (profile_tier_level.max_10bit_constraint_flag);
  CHECK_FIELD (profile_tier_level.max_8bit_constraint_flag);
  CHECK_FIELD (profile_tier_level.max_422chroma_constraint_flag);
  CHECK_FIELD (profile_tier_level.max_420chroma_constraint_flag);
  CHECK_FIELD (profile_tier_level.max_monochrome_constraint_flag);
  CHECK_FIELD (profile_tier_level.intra_constraint_flag);
  CHECK_FIELD (profile_tier_level.one_picture_only_constraint_flag);
  CHECK_FIELD (profile_tier_level.lower_bit_rate_constraint_flag);
  CHECK_FIELD (profile_tier_level.level_idc);
  CHECK_FIELD (chroma_format_idc);
  CHECK_FIELD (pic_width_in_luma_samples);
  CHECK_FIELD (pic_height_in_luma_samples);
  CHECK_FIELD (conformance_window_flag);
  CHECK_FIELD (conf_win_left_offset);
  CHECK_FIELD (conf_win_right_offset);
  CHECK_FIELD (conf_win_top_offset);
  CHECK_FIELD (conf_win_bottom_offset);
  CHECK_FIELD (bit_depth_luma_minus8);
  CHECK_FIELD (bit_depth_chroma_minus8);
  CHECK_FIELD (log2_max_pic_order_cnt_lsb_minus4);
  CHECK_FIELD (sub_layer_ordering_info_present_flag);
  CHECK_FIELD (max_dec_pic_buffering_minus1[0]);
  CHECK_FIELD (max_num_reorder_pics[0]);
  CHECK_FIELD (max_latency_increase_plus1[0]);
  CHECK_FIELD (log2_min_luma_coding_block_size_minus3);
  CHECK_FIELD (log2_diff_max_min_luma_coding_block_size);
  CHECK_FIELD (log2_min_transform_block_size_minus2);
  CHECK_FIELD (log2_diff_max_min_transform_block_size);
  CHECK_FIELD (max_transform_hierarchy_depth_inter);
  CHECK_FIELD (max_transform_hierarchy_depth_intra);
  CHECK_FIELD (scaling_list_enabled_flag);
  CHECK_FIELD (scaling_list_data_present_flag);

  for (i = 0; i < 6; i++)
    CHECK_FIELD (scaling_list.scaling_list_dc_coef_minus8_16x16[i]);
  for (i = 0; i < 2; i++)
    CHECK_FIELD (scaling_list.scaling_list_dc_coef_minus8_32x32[i]);
  for (i = 0; i < 6; i++) {
    for (j = 0; j < 16; j++)
      CHECK_FIELD (scaling_list.scaling_lists_4x4[i][j]);
  }
  for (i = 0; i < 6; i++) {
    for (j = 0; j < 64; j++)
      CHECK_FIELD (scaling_list.scaling_lists_8x8[i][j]);
  }
  for (i = 0; i < 6; i++) {
    for (j = 0; j < 64; j++)
      CHECK_FIELD (scaling_list.scaling_lists_16x16[i][j]);
  }
  for (i = 0; i < 2; i++) {
    for (j = 0; j < 64; j++)
      CHECK_FIELD (scaling_list.scaling_lists_32x32[i][j]);
  }

  CHECK_FIELD (amp_enabled_flag);
  CHECK_FIELD (sample_adaptive_offset_enabled_flag);
  CHECK_FIELD (pcm_enabled_flag);
  CHECK_FIELD (pcm_sample_bit_depth_luma_minus1);
  CHECK_FIELD (pcm_sample_bit_depth_chroma_minus1);
  CHECK_FIELD (log2_min_pcm_luma_coding_block_size_minus3);
  CHECK_FIELD (log2_diff_max_min_pcm_luma_coding_block_size);
  CHECK_FIELD (pcm_loop_filter_disabled_flag);
  CHECK_FIELD (num_short_term_ref_pic_sets);

  for (i = 0; i < 3; i++) {
    CHECK_FIELD (short_term_ref_pic_set[i].inter_ref_pic_set_prediction_flag);
    CHECK_FIELD (short_term_ref_pic_set[i].NumDeltaPocs);
    CHECK_FIELD (short_term_ref_pic_set[i].NumNegativePics);
    for (j = 0; j < sps_parsed.short_term_ref_pic_set[i].NumNegativePics; j++) {
      CHECK_FIELD (short_term_ref_pic_set[i].DeltaPocS0[j]);
      CHECK_FIELD (short_term_ref_pic_set[i].UsedByCurrPicS0[j]);
    }
    CHECK_FIELD (short_term_ref_pic_set[i].NumPositivePics);
    for (j = 0; j < sps_parsed.short_term_ref_pic_set[i].NumPositivePics; j++) {
      CHECK_FIELD (short_term_ref_pic_set[i].DeltaPocS1[j]);
      CHECK_FIELD (short_term_ref_pic_set[i].UsedByCurrPicS1[j]);
    }
  }

  CHECK_FIELD (long_term_ref_pics_present_flag);
  CHECK_FIELD (temporal_mvp_enabled_flag);
  CHECK_FIELD (strong_intra_smoothing_enabled_flag);
  CHECK_FIELD (vui_parameters_present_flag);
  CHECK_FIELD (vui_params.aspect_ratio_info_present_flag);
  CHECK_FIELD (vui_params.overscan_info_present_flag);
  CHECK_FIELD (vui_params.overscan_appropriate_flag);
  CHECK_FIELD (vui_params.video_signal_type_present_flag);
  CHECK_FIELD (vui_params.video_format);
  CHECK_FIELD (vui_params.video_full_range_flag);
  CHECK_FIELD (vui_params.colour_description_present_flag);
  CHECK_FIELD (vui_params.chroma_loc_info_present_flag);
  CHECK_FIELD (vui_params.chroma_sample_loc_type_top_field);
  CHECK_FIELD (vui_params.chroma_sample_loc_type_bottom_field);
  CHECK_FIELD (vui_params.neutral_chroma_indication_flag);
  CHECK_FIELD (vui_params.field_seq_flag);
  CHECK_FIELD (vui_params.frame_field_info_present_flag);
  CHECK_FIELD (vui_params.default_display_window_flag);
  CHECK_FIELD (vui_params.def_disp_win_left_offset);
  CHECK_FIELD (vui_params.def_disp_win_right_offset);
  CHECK_FIELD (vui_params.def_disp_win_top_offset);
  CHECK_FIELD (vui_params.def_disp_win_bottom_offset);
  CHECK_FIELD (vui_params.timing_info_present_flag);
  CHECK_FIELD (vui_params.num_units_in_tick);
  CHECK_FIELD (vui_params.time_scale);
  CHECK_FIELD (vui_params.poc_proportional_to_timing_flag);
  CHECK_FIELD (vui_params.hrd_parameters_present_flag);
  CHECK_FIELD (vui_params.hrd_params.nal_hrd_parameters_present_flag);
  CHECK_FIELD (vui_params.hrd_params.vcl_hrd_parameters_present_flag);
  CHECK_FIELD (vui_params.hrd_params.sub_pic_hrd_params_present_flag);
  CHECK_FIELD (vui_params.hrd_params.bit_rate_scale);
  CHECK_FIELD (vui_params.hrd_params.cpb_size_scale);
  CHECK_FIELD (vui_params.hrd_params.cpb_size_du_scale);
  CHECK_FIELD (vui_params.hrd_params.initial_cpb_removal_delay_length_minus1);
  CHECK_FIELD (vui_params.hrd_params.au_cpb_removal_delay_length_minus1);
  CHECK_FIELD (vui_params.hrd_params.dpb_output_delay_length_minus1);
  CHECK_FIELD (vui_params.hrd_params.fixed_pic_rate_general_flag[0]);
  CHECK_FIELD (vui_params.hrd_params.fixed_pic_rate_within_cvs_flag[0]);
  CHECK_FIELD (vui_params.hrd_params.elemental_duration_in_tc_minus1[0]);
  CHECK_FIELD (vui_params.hrd_params.low_delay_hrd_flag[0]);
  CHECK_FIELD (vui_params.hrd_params.cpb_cnt_minus1[0]);
  CHECK_FIELD
      (vui_params.hrd_params.sublayer_hrd_params[0].bit_rate_value_minus1[0]);
  CHECK_FIELD
      (vui_params.hrd_params.sublayer_hrd_params[0].cpb_size_value_minus1[0]);
  CHECK_FIELD (vui_params.hrd_params.sublayer_hrd_params[0].
      cpb_size_du_value_minus1[0]);
  CHECK_FIELD (vui_params.hrd_params.sublayer_hrd_params[0].
      bit_rate_du_value_minus1[0]);
  CHECK_FIELD (vui_params.hrd_params.sublayer_hrd_params[0].cbr_flag[0]);
  CHECK_FIELD (vui_params.bitstream_restriction_flag);
  CHECK_FIELD (vui_params.tiles_fixed_structure_flag);
  CHECK_FIELD (vui_params.motion_vectors_over_pic_boundaries_flag);
  CHECK_FIELD (vui_params.restricted_ref_pic_lists_flag);
  CHECK_FIELD (vui_params.min_spatial_segmentation_idc);
  CHECK_FIELD (vui_params.max_bytes_per_pic_denom);
  CHECK_FIELD (vui_params.max_bits_per_min_cu_denom);
  CHECK_FIELD (vui_params.log2_max_mv_length_horizontal);
  CHECK_FIELD (vui_params.log2_max_mv_length_vertical);
  CHECK_FIELD (sps_extension_flag);
  CHECK_FIELD (sps_range_extension_flag);
  CHECK_FIELD (sps_multilayer_extension_flag);
  CHECK_FIELD (sps_3d_extension_flag);
  CHECK_FIELD (sps_scc_extension_flag);
  CHECK_FIELD (sps_extension_params.transform_skip_rotation_enabled_flag);
  CHECK_FIELD (sps_extension_params.transform_skip_context_enabled_flag);
  CHECK_FIELD (sps_extension_params.implicit_rdpcm_enabled_flag);
  CHECK_FIELD (sps_extension_params.explicit_rdpcm_enabled_flag);
  CHECK_FIELD (sps_extension_params.extended_precision_processing_flag);
  CHECK_FIELD (sps_extension_params.intra_smoothing_disabled_flag);
  CHECK_FIELD (sps_extension_params.high_precision_offsets_enabled_flag);
  CHECK_FIELD (sps_extension_params.persistent_rice_adaptation_enabled_flag);
  CHECK_FIELD (sps_extension_params.cabac_bypass_alignment_enabled_flag);
#undef CHECK_FIELD

  memset (header_data, 0, sizeof (header_data));
  memset (header_nal, 0, sizeof (header_nal));

  size = sizeof (header_data);
  ret = gst_h265_bit_writer_pps (&pps, TRUE, header_data, &size);
  fail_if (ret != GST_H265_BIT_WRITER_OK);

  nal_size = sizeof (header_nal);
  ret = gst_h265_bit_writer_convert_to_nal (4, FALSE, TRUE, FALSE,
      header_data, size * 8, header_nal, &nal_size);
  fail_if (ret != GST_H265_BIT_WRITER_OK);
  fail_if (nal_size < size);

  /* Parse it again */
  res = gst_h265_parser_identify_nalu (parser, header_nal, 0,
      sizeof (header_nal), &nalu);
  assert_equals_int (res, GST_H265_PARSER_NO_NAL_END);

  res = gst_h265_parser_parse_pps (parser, &nalu, &pps_parsed);
  assert_equals_int (res, GST_H265_PARSER_OK);

#define CHECK_FIELD(FIELD)  fail_if(pps_parsed.FIELD != pps.FIELD)
  CHECK_FIELD (id);
  CHECK_FIELD (dependent_slice_segments_enabled_flag);
  CHECK_FIELD (output_flag_present_flag);
  CHECK_FIELD (num_extra_slice_header_bits);
  CHECK_FIELD (sign_data_hiding_enabled_flag);
  CHECK_FIELD (cabac_init_present_flag);
  CHECK_FIELD (num_ref_idx_l0_default_active_minus1);
  CHECK_FIELD (num_ref_idx_l1_default_active_minus1);
  CHECK_FIELD (init_qp_minus26);
  CHECK_FIELD (constrained_intra_pred_flag);
  CHECK_FIELD (transform_skip_enabled_flag);
  CHECK_FIELD (cu_qp_delta_enabled_flag);
  CHECK_FIELD (diff_cu_qp_delta_depth);
  CHECK_FIELD (cb_qp_offset);
  CHECK_FIELD (cr_qp_offset);
  CHECK_FIELD (slice_chroma_qp_offsets_present_flag);
  CHECK_FIELD (weighted_pred_flag);
  CHECK_FIELD (weighted_bipred_flag);
  CHECK_FIELD (transquant_bypass_enabled_flag);
  CHECK_FIELD (tiles_enabled_flag);
  CHECK_FIELD (entropy_coding_sync_enabled_flag);
  CHECK_FIELD (num_tile_columns_minus1);
  CHECK_FIELD (num_tile_rows_minus1);
  CHECK_FIELD (uniform_spacing_flag);
  CHECK_FIELD (column_width_minus1[0]);
  CHECK_FIELD (row_height_minus1[0]);
  CHECK_FIELD (row_height_minus1[1]);
  CHECK_FIELD (loop_filter_across_tiles_enabled_flag);
  CHECK_FIELD (loop_filter_across_slices_enabled_flag);
  CHECK_FIELD (deblocking_filter_control_present_flag);
  CHECK_FIELD (deblocking_filter_override_enabled_flag);
  CHECK_FIELD (deblocking_filter_disabled_flag);
  CHECK_FIELD (beta_offset_div2);
  CHECK_FIELD (tc_offset_div2);
  CHECK_FIELD (scaling_list_data_present_flag);

  for (i = 0; i < 6; i++)
    CHECK_FIELD (scaling_list.scaling_list_dc_coef_minus8_16x16[i]);
  for (i = 0; i < 2; i++)
    CHECK_FIELD (scaling_list.scaling_list_dc_coef_minus8_32x32[i]);
  for (i = 0; i < 6; i++) {
    for (j = 0; j < 16; j++)
      CHECK_FIELD (scaling_list.scaling_lists_4x4[i][j]);
  }
  for (i = 0; i < 6; i++) {
    for (j = 0; j < 64; j++)
      CHECK_FIELD (scaling_list.scaling_lists_8x8[i][j]);
  }
  for (i = 0; i < 6; i++) {
    for (j = 0; j < 64; j++)
      CHECK_FIELD (scaling_list.scaling_lists_16x16[i][j]);
  }
  for (i = 0; i < 2; i++) {
    for (j = 0; j < 64; j++)
      CHECK_FIELD (scaling_list.scaling_lists_32x32[i][j]);
  }

  CHECK_FIELD (lists_modification_present_flag);
  CHECK_FIELD (log2_parallel_merge_level_minus2);
  CHECK_FIELD (slice_segment_header_extension_present_flag);
  CHECK_FIELD (pps_extension_flag);
  CHECK_FIELD (pps_range_extension_flag);
  CHECK_FIELD (pps_multilayer_extension_flag);
  CHECK_FIELD (pps_3d_extension_flag);
  CHECK_FIELD (pps_scc_extension_flag);
  CHECK_FIELD (pps_extension_params.log2_max_transform_skip_block_size_minus2);
  CHECK_FIELD (pps_extension_params.cross_component_prediction_enabled_flag);
  CHECK_FIELD (pps_extension_params.chroma_qp_offset_list_enabled_flag);
  CHECK_FIELD (pps_extension_params.diff_cu_chroma_qp_offset_depth);
  CHECK_FIELD (pps_extension_params.chroma_qp_offset_list_len_minus1);
  for (i = 0; i < 6; i++)
    CHECK_FIELD (pps_extension_params.cb_qp_offset_list[i]);
  for (i = 0; i < 6; i++)
    CHECK_FIELD (pps_extension_params.cr_qp_offset_list[i]);
  CHECK_FIELD (pps_extension_params.log2_sao_offset_scale_luma);
  CHECK_FIELD (pps_extension_params.log2_sao_offset_scale_chroma);
#undef CHECK_FIELD

  memset (header_data, 0, sizeof (header_data));
  memset (header_nal, 0, sizeof (header_nal));

  size = sizeof (header_data);
  ret = gst_h265_bit_writer_slice_hdr (&slice_hdr, TRUE,
      GST_H265_NAL_SLICE_TRAIL_N, header_data, &size);
  fail_if (ret != GST_H265_BIT_WRITER_OK);

  nal_size = sizeof (header_nal);
  ret = gst_h265_bit_writer_convert_to_nal (4, FALSE, TRUE, FALSE,
      header_data, size * 8, header_nal, &nal_size);
  fail_if (ret != GST_H265_BIT_WRITER_OK);
  fail_if (nal_size < size);

  /* Parse it again */
  res = gst_h265_parser_identify_nalu (parser, header_nal, 0,
      sizeof (header_nal), &nalu);
  assert_equals_int (res, GST_H265_PARSER_NO_NAL_END);

  res = gst_h265_parser_parse_slice_hdr (parser, &nalu, &slice_parsed);
  assert_equals_int (res, GST_H265_PARSER_OK);

#define CHECK_FIELD(FIELD)  fail_if(slice_parsed.FIELD != slice_hdr.FIELD)
  CHECK_FIELD (first_slice_segment_in_pic_flag);
  CHECK_FIELD (type);
  CHECK_FIELD (pic_order_cnt_lsb);
  CHECK_FIELD (short_term_ref_pic_set_sps_flag);
  CHECK_FIELD (short_term_ref_pic_set_idx);
  CHECK_FIELD (temporal_mvp_enabled_flag);
  CHECK_FIELD (sao_luma_flag);
  CHECK_FIELD (sao_chroma_flag);
  CHECK_FIELD (num_ref_idx_active_override_flag);
  CHECK_FIELD (num_ref_idx_l0_active_minus1);
  CHECK_FIELD (num_ref_idx_l1_active_minus1);
  CHECK_FIELD (mvd_l1_zero_flag);
  CHECK_FIELD (collocated_ref_idx);
  CHECK_FIELD (five_minus_max_num_merge_cand);
  CHECK_FIELD (qp_delta);
  CHECK_FIELD (deblocking_filter_override_flag);
  CHECK_FIELD (cu_chroma_qp_offset_enabled_flag);
  CHECK_FIELD (deblocking_filter_override_flag);
  CHECK_FIELD (beta_offset_div2);
  CHECK_FIELD (tc_offset_div2);
  CHECK_FIELD (num_entry_point_offsets);
#undef CHECK_FIELD

  gst_h265_parser_free (parser);
}

GST_END_TEST;

static Suite *
h265bitwriter_suite (void)
{
  Suite *s = suite_create ("H265 bitwriter library");

  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_h265_bitwriter_vps_sps_pps_slice_hdr);

  return s;
}

GST_CHECK_MAIN (h265bitwriter);
