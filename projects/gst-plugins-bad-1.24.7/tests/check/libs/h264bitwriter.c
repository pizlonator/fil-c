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

#include <gst/check/gstcheck.h>
#include <gst/codecparsers/gsth264bitwriter.h>

/* *INDENT-OFF* */
static const GstH264SPS sps = {
  .id = 0,
  .profile_idc = 100,
  .constraint_set0_flag = 0,
  .constraint_set1_flag = 0,
  .constraint_set2_flag = 0,
  .constraint_set3_flag = 0,
  .constraint_set4_flag = 0,
  .constraint_set5_flag = 0,
  .level_idc = 31,

  .chroma_format_idc = 1,
  .bit_depth_luma_minus8 = 0,
  .bit_depth_chroma_minus8 = 0,

  .scaling_matrix_present_flag = 1,
  .scaling_lists_4x4[0] = { 17, 32, 31, 30, 23, 15, 33, 39, 39, 35, 35, 14, 28, 32, 27, 27 },

  .log2_max_frame_num_minus4 = 2,
  .pic_order_cnt_type = 0,
  .log2_max_pic_order_cnt_lsb_minus4 = 3,

  .num_ref_frames = 8,
  .gaps_in_frame_num_value_allowed_flag = 0,
  .pic_width_in_mbs_minus1 = 49,
  .pic_height_in_map_units_minus1 = 37,
  .frame_mbs_only_flag = 1,
  .mb_adaptive_frame_field_flag = 0,
  .direct_8x8_inference_flag = 1,
  .frame_cropping_flag = 1,
  .frame_crop_left_offset = 8,
  .frame_crop_right_offset = 8,
  .frame_crop_top_offset = 16,
  .frame_crop_bottom_offset = 8,

  .vui_parameters_present_flag = 1,
  .vui_parameters = {
    .aspect_ratio_info_present_flag = 1,
    .aspect_ratio_idc = 255,
    .sar_width = 1,
    .sar_height = 1,
    .overscan_info_present_flag = 0,
    .overscan_appropriate_flag = 0,
    .chroma_loc_info_present_flag = 0,
    .timing_info_present_flag = 1,
    .num_units_in_tick = 1,
    .time_scale = 60,
    .fixed_frame_rate_flag = 1,

    .nal_hrd_parameters_present_flag = 1,
    .nal_hrd_parameters = {
      .cpb_cnt_minus1 = 0,
      .bit_rate_scale = 4,
      .cpb_size_scale = 2,
      .bit_rate_value_minus1[0] = 1999,
      .cpb_size_value_minus1[0] = 63999,
      .cbr_flag[0] = 1,
      .initial_cpb_removal_delay_length_minus1 = 23,
      .cpb_removal_delay_length_minus1 = 23,
      .dpb_output_delay_length_minus1 = 23,
      .time_offset_length = 24,
    },

    .vcl_hrd_parameters_present_flag = 0,
    .low_delay_hrd_flag = 0,
    .pic_struct_present_flag = 1,
    .bitstream_restriction_flag = 1,
    .motion_vectors_over_pic_boundaries_flag = 1,
    .max_bytes_per_pic_denom = 2,
    .max_bits_per_mb_denom = 1,
    .log2_max_mv_length_horizontal = 13,
    .log2_max_mv_length_vertical = 11,
    .num_reorder_frames = 3,
    .max_dec_frame_buffering = 8,
  },
};

static const GstH264PPS pps = {
  .id = 2,
  .entropy_coding_mode_flag = 1,
  .pic_order_present_flag = 0,
  .num_slice_groups_minus1 = 0,

  .num_ref_idx_l0_active_minus1 = 4,
  .num_ref_idx_l1_active_minus1 = 2,

  .weighted_pred_flag = 0,
  .weighted_bipred_idc = 0,
  .pic_init_qp_minus26 = 2,
  .pic_init_qs_minus26 = 0,
  .chroma_qp_index_offset = 1,
  .deblocking_filter_control_present_flag = 1,
  .constrained_intra_pred_flag = 0,
  .redundant_pic_cnt_present_flag = 0,
  .transform_8x8_mode_flag = 0,

  .pic_scaling_matrix_present_flag = 0,
  .second_chroma_qp_index_offset = 0,

  .sequence = (GstH264SPS *) &sps,
};

static const GstH264SliceHdr slice_hdr = {
  .first_mb_in_slice = 0,
  .type = 1,
  .frame_num = 10,

  .field_pic_flag = 0,
  .bottom_field_flag = 0,
  .idr_pic_id = 0,

  .pic_order_cnt_lsb = 4,
  .delta_pic_order_cnt_bottom = 0,

  .direct_spatial_mv_pred_flag = 1,

  .num_ref_idx_active_override_flag = 1,
  .num_ref_idx_l0_active_minus1 = 2,
  .num_ref_idx_l1_active_minus1 = 2,

  .ref_pic_list_modification_flag_l0 = 0,
  .ref_pic_list_modification_flag_l1 = 0,

  .dec_ref_pic_marking.no_output_of_prior_pics_flag = 0,
  .dec_ref_pic_marking.long_term_reference_flag = 0,
  .dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag = 0,

  .cabac_init_idc = 1,
  .slice_qp_delta = 8,

  .disable_deblocking_filter_idc = 0,
  .slice_alpha_c0_offset_div2 = 2,
  .slice_beta_offset_div2 = 2,

  .pps = (GstH264PPS *) &pps,
};
/* *INDENT-ON* */

GST_START_TEST (test_h264_bitwriter_sps_pps_slice_hdr)
{
  GstH264ParserResult res;
  gboolean ret;
  GstH264NalUnit nalu;
  GstH264NalParser *const parser = gst_h264_nal_parser_new ();
  GstH264SPS sps_parsed;
  GstH264PPS pps_parsed;
  GstH264SliceHdr slice_parsed;
  guint8 header_data[128] = { 0, };
  guint8 header_nal[128] = { 0, };
  guint size, trail_bits;
  guint nal_size;
  guint i;

  size = sizeof (header_data);
  ret = gst_h264_bit_writer_sps (&sps, TRUE, header_data, &size);
  fail_if (ret != GST_H264_BIT_WRITER_OK);

  nal_size = sizeof (header_nal);
  ret = gst_h264_bit_writer_convert_to_nal (4, FALSE, TRUE, FALSE,
      header_data, size * 8, header_nal, &nal_size);
  fail_if (ret != GST_H264_BIT_WRITER_OK);
  fail_if (nal_size < size);

  /* Parse it again */
  res = gst_h264_parser_identify_nalu (parser, header_nal, 0,
      sizeof (header_nal), &nalu);
  assert_equals_int (res, GST_H264_PARSER_NO_NAL_END);

  res = gst_h264_parser_parse_sps (parser, &nalu, &sps_parsed);
  assert_equals_int (res, GST_H264_PARSER_OK);

  /* We can not do simply memcmp, the parser may set some default
     value for the fields which are not used for writing. */
#define CHECK_FIELD(FIELD)  fail_if(sps_parsed.FIELD != sps.FIELD)
  CHECK_FIELD (id);
  CHECK_FIELD (profile_idc);
  CHECK_FIELD (constraint_set0_flag);
  CHECK_FIELD (constraint_set1_flag);
  CHECK_FIELD (constraint_set2_flag);
  CHECK_FIELD (constraint_set3_flag);
  CHECK_FIELD (constraint_set4_flag);
  CHECK_FIELD (constraint_set5_flag);
  CHECK_FIELD (level_idc);

  CHECK_FIELD (chroma_format_idc);
  CHECK_FIELD (bit_depth_luma_minus8);
  CHECK_FIELD (bit_depth_chroma_minus8);

  CHECK_FIELD (scaling_matrix_present_flag);
  for (i = 0; i < 16; i++)
    CHECK_FIELD (scaling_lists_4x4[0][i]);

  CHECK_FIELD (log2_max_frame_num_minus4);
  CHECK_FIELD (pic_order_cnt_type);
  CHECK_FIELD (log2_max_pic_order_cnt_lsb_minus4);

  CHECK_FIELD (num_ref_frames);
  CHECK_FIELD (gaps_in_frame_num_value_allowed_flag);
  CHECK_FIELD (pic_width_in_mbs_minus1);
  CHECK_FIELD (pic_height_in_map_units_minus1);
  CHECK_FIELD (frame_mbs_only_flag);
  CHECK_FIELD (mb_adaptive_frame_field_flag);
  CHECK_FIELD (direct_8x8_inference_flag);
  CHECK_FIELD (frame_cropping_flag);
  CHECK_FIELD (frame_crop_left_offset);
  CHECK_FIELD (frame_crop_right_offset);
  CHECK_FIELD (frame_crop_top_offset);
  CHECK_FIELD (frame_crop_bottom_offset);

  CHECK_FIELD (vui_parameters_present_flag);
  CHECK_FIELD (vui_parameters.aspect_ratio_info_present_flag);
  CHECK_FIELD (vui_parameters.aspect_ratio_idc);
  CHECK_FIELD (vui_parameters.sar_width);
  CHECK_FIELD (vui_parameters.sar_height);
  CHECK_FIELD (vui_parameters.overscan_info_present_flag);
  CHECK_FIELD (vui_parameters.overscan_appropriate_flag);
  CHECK_FIELD (vui_parameters.chroma_loc_info_present_flag);
  CHECK_FIELD (vui_parameters.timing_info_present_flag);
  CHECK_FIELD (vui_parameters.num_units_in_tick);
  CHECK_FIELD (vui_parameters.time_scale);
  CHECK_FIELD (vui_parameters.fixed_frame_rate_flag);

  CHECK_FIELD (vui_parameters.nal_hrd_parameters_present_flag);
  CHECK_FIELD (vui_parameters.nal_hrd_parameters.cpb_cnt_minus1);
  CHECK_FIELD (vui_parameters.nal_hrd_parameters.bit_rate_scale);
  CHECK_FIELD (vui_parameters.nal_hrd_parameters.cpb_size_scale);
  CHECK_FIELD (vui_parameters.nal_hrd_parameters.bit_rate_value_minus1[0]);
  CHECK_FIELD (vui_parameters.nal_hrd_parameters.cpb_size_value_minus1[0]);
  CHECK_FIELD (vui_parameters.nal_hrd_parameters.cbr_flag[0]);
  CHECK_FIELD (vui_parameters.
      nal_hrd_parameters.initial_cpb_removal_delay_length_minus1);
  CHECK_FIELD (vui_parameters.
      nal_hrd_parameters.cpb_removal_delay_length_minus1);
  CHECK_FIELD (vui_parameters.
      nal_hrd_parameters.dpb_output_delay_length_minus1);
  CHECK_FIELD (vui_parameters.nal_hrd_parameters.time_offset_length);

  CHECK_FIELD (vui_parameters.vcl_hrd_parameters_present_flag);
  CHECK_FIELD (vui_parameters.low_delay_hrd_flag);
  CHECK_FIELD (vui_parameters.pic_struct_present_flag);
  CHECK_FIELD (vui_parameters.bitstream_restriction_flag);
  CHECK_FIELD (vui_parameters.motion_vectors_over_pic_boundaries_flag);
  CHECK_FIELD (vui_parameters.max_bytes_per_pic_denom);
  CHECK_FIELD (vui_parameters.max_bits_per_mb_denom);
  CHECK_FIELD (vui_parameters.log2_max_mv_length_horizontal);
  CHECK_FIELD (vui_parameters.log2_max_mv_length_vertical);
  CHECK_FIELD (vui_parameters.num_reorder_frames);
  CHECK_FIELD (vui_parameters.max_dec_frame_buffering);
#undef CHECK_FIELD

  memset (header_data, 0, sizeof (header_data));
  memset (header_nal, 0, sizeof (header_nal));

  size = sizeof (header_data);
  ret = gst_h264_bit_writer_pps (&pps, TRUE, header_data, &size);
  fail_if (ret != GST_H264_BIT_WRITER_OK);

  nal_size = sizeof (header_nal);
  ret = gst_h264_bit_writer_convert_to_nal (4, FALSE, TRUE, FALSE,
      header_data, size * 8, header_nal, &nal_size);
  fail_if (ret != GST_H264_BIT_WRITER_OK);
  fail_if (nal_size < size);

  /* Parse it again */
  res = gst_h264_parser_identify_nalu (parser, header_nal, 0,
      sizeof (header_nal), &nalu);
  assert_equals_int (res, GST_H264_PARSER_NO_NAL_END);

  res = gst_h264_parser_parse_pps (parser, &nalu, &pps_parsed);
  assert_equals_int (res, GST_H264_PARSER_OK);

#define CHECK_FIELD(FIELD)  fail_if(pps_parsed.FIELD != pps.FIELD)
  CHECK_FIELD (id);
  CHECK_FIELD (entropy_coding_mode_flag);
  CHECK_FIELD (pic_order_present_flag);
  CHECK_FIELD (num_slice_groups_minus1);

  CHECK_FIELD (num_ref_idx_l0_active_minus1);
  CHECK_FIELD (num_ref_idx_l1_active_minus1);

  CHECK_FIELD (weighted_pred_flag);
  CHECK_FIELD (weighted_bipred_idc);
  CHECK_FIELD (pic_init_qp_minus26);
  CHECK_FIELD (pic_init_qs_minus26);
  CHECK_FIELD (chroma_qp_index_offset);
  CHECK_FIELD (deblocking_filter_control_present_flag);
  CHECK_FIELD (constrained_intra_pred_flag);
  CHECK_FIELD (redundant_pic_cnt_present_flag);
  CHECK_FIELD (transform_8x8_mode_flag);

  CHECK_FIELD (pic_scaling_matrix_present_flag);
  CHECK_FIELD (second_chroma_qp_index_offset);
#undef CHECK_FIELD

  memset (header_data, 0, sizeof (header_data));
  memset (header_nal, 0, sizeof (header_nal));

  size = sizeof (header_data);
  trail_bits = 0;
  ret = gst_h264_bit_writer_slice_hdr (&slice_hdr, TRUE, GST_H264_NAL_SLICE,
      FALSE, header_data, &size, &trail_bits);
  fail_if (ret != GST_H264_BIT_WRITER_OK);

  nal_size = sizeof (header_nal);
  ret = gst_h264_bit_writer_convert_to_nal (4, FALSE, TRUE, TRUE,
      header_data, size * 8 + trail_bits, header_nal, &nal_size);
  fail_if (ret != GST_H264_BIT_WRITER_OK);
  fail_if (nal_size < size);

  /* Parse it again */
  res = gst_h264_parser_identify_nalu (parser, header_nal, 0,
      sizeof (header_nal), &nalu);
  assert_equals_int (res, GST_H264_PARSER_NO_NAL_END);

  res = gst_h264_parser_parse_slice_hdr (parser, &nalu, &slice_parsed,
      TRUE, TRUE);
  assert_equals_int (res, GST_H264_PARSER_OK);

#define CHECK_FIELD(FIELD)  fail_if(slice_parsed.FIELD != slice_hdr.FIELD)
  CHECK_FIELD (first_mb_in_slice);
  CHECK_FIELD (type);
  CHECK_FIELD (frame_num);
  CHECK_FIELD (field_pic_flag);
  CHECK_FIELD (bottom_field_flag);
  CHECK_FIELD (idr_pic_id);
  CHECK_FIELD (pic_order_cnt_lsb);
  CHECK_FIELD (delta_pic_order_cnt_bottom);
  CHECK_FIELD (direct_spatial_mv_pred_flag);
  CHECK_FIELD (num_ref_idx_active_override_flag);
  CHECK_FIELD (num_ref_idx_l0_active_minus1);
  CHECK_FIELD (num_ref_idx_l1_active_minus1);
  CHECK_FIELD (ref_pic_list_modification_flag_l0);
  CHECK_FIELD (ref_pic_list_modification_flag_l1);
  CHECK_FIELD (dec_ref_pic_marking.no_output_of_prior_pics_flag);
  CHECK_FIELD (dec_ref_pic_marking.long_term_reference_flag);
  CHECK_FIELD (dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag);
  CHECK_FIELD (cabac_init_idc);
  CHECK_FIELD (slice_qp_delta);
  CHECK_FIELD (disable_deblocking_filter_idc);
  CHECK_FIELD (slice_alpha_c0_offset_div2);
  CHECK_FIELD (slice_beta_offset_div2);
#undef CHECK_FIELD

  gst_h264_nal_parser_free (parser);
}

GST_END_TEST;

static const guint8 nalu_sps[] = {
  0x00, 0x00, 0x00, 0x01, 0x27, 0x64, 0x00, 0x32, 0xac, 0x2c, 0xa2,
  0x40, 0x78, 0x02, 0x27, 0xe5, 0xc0, 0x50, 0x80, 0x80, 0x80, 0xa0,
  0x00, 0x00, 0x03, 0x00, 0x20, 0x00, 0x00, 0x07, 0x9d, 0x08, 0x00,
  0x7a, 0x10, 0x00, 0x07, 0xa1, 0x23, 0x7b, 0xdf, 0x07, 0x68, 0x70,
  0xc2, 0x89, 0x80,
};

GST_START_TEST (test_h264_bitwriter_sei)
{
  GstH264ParserResult res;
  GstH264NalUnit nalu;
  GstH264SEIMessage sei_msg = { 0, };
  gboolean ret;
  GstH264SPS sps_parsed;
  GstH264HRDParams *hrd_param = &sps_parsed.vui_parameters.nal_hrd_parameters;
  GstH264NalParser *const parser = gst_h264_nal_parser_new ();
  GstH264PicTiming *pic_timing = &sei_msg.payload.pic_timing;
  GstH264BufferingPeriod *buf_per = &sei_msg.payload.buffering_period;
  GstH264PicTiming *pic_timing_parsed;
  GstH264BufferingPeriod *buf_per_parsed;
  GArray *msg_array;
  GArray *sei_parsed = NULL;
  GstH264SEIMessage *sei_msg_parsed;
  guint size;
  guint size_nal;
  guint8 sei_data[128] = { 0, };
  guint8 sei_nal[128] = { 0, };

  res = gst_h264_parser_identify_nalu (parser, nalu_sps, 0, sizeof (nalu_sps),
      &nalu);
  assert_equals_int (res, GST_H264_PARSER_NO_NAL_END);
  assert_equals_int (nalu.type, GST_H264_NAL_SPS);
  assert_equals_int (nalu.size, 43);
  res = gst_h264_parser_parse_sps (parser, &nalu, &sps_parsed);
  assert_equals_int (res, GST_H264_PARSER_OK);

  msg_array = g_array_new (FALSE, FALSE, sizeof (GstH264SEIMessage));

  sei_msg.payloadType = GST_H264_SEI_PIC_TIMING;
  pic_timing->CpbDpbDelaysPresentFlag =
      sps_parsed.vui_parameters.nal_hrd_parameters_present_flag;
  pic_timing->cpb_removal_delay_length_minus1 =
      hrd_param->cpb_removal_delay_length_minus1;
  pic_timing->dpb_output_delay_length_minus1 =
      hrd_param->dpb_output_delay_length_minus1;
  pic_timing->cpb_removal_delay = 1020;
  pic_timing->dpb_output_delay = 80;
  pic_timing->pic_struct_present_flag = 1;
  pic_timing->pic_struct = 2;

  pic_timing->clock_timestamp_flag[0] = 1;
  pic_timing->clock_timestamp_flag[1] = 0;
  pic_timing->clock_timestamp_flag[2] = 0;

  pic_timing->clock_timestamp[0].ct_type = GST_H264_CT_TYPE_INTERLACED;
  pic_timing->clock_timestamp[0].nuit_field_based_flag = 1;
  pic_timing->clock_timestamp[0].counting_type = 0;
  pic_timing->clock_timestamp[0].discontinuity_flag = 0;
  pic_timing->clock_timestamp[0].cnt_dropped_flag = 0;
  pic_timing->clock_timestamp[0].n_frames = 1;
  pic_timing->clock_timestamp[0].seconds_flag = 1;
  pic_timing->clock_timestamp[0].seconds_value = 32;
  pic_timing->clock_timestamp[0].minutes_flag = 1;
  pic_timing->clock_timestamp[0].minutes_value = 52;
  pic_timing->clock_timestamp[0].hours_flag = 1;
  pic_timing->clock_timestamp[0].hours_value = 8;
  pic_timing->clock_timestamp[0].full_timestamp_flag = 1;
  pic_timing->clock_timestamp[0].time_offset = 80;

  pic_timing->time_offset_length = 24;

  g_array_append_val (msg_array, sei_msg);

  memset (&sei_msg, 0, sizeof (sei_msg));

  sei_msg.payloadType = GST_H264_SEI_BUF_PERIOD;

  buf_per->sps = &sps_parsed;
  buf_per->nal_initial_cpb_removal_delay[0] = 90021;
  buf_per->nal_initial_cpb_removal_delay_offset[0] = 90021;

  g_array_append_val (msg_array, sei_msg);

  size = sizeof (sei_data);
  ret = gst_h264_bit_writer_sei (msg_array, TRUE, sei_data, &size);
  fail_if (ret != GST_H264_BIT_WRITER_OK);

  size_nal = sizeof (sei_nal);
  ret = gst_h264_bit_writer_convert_to_nal (4, FALSE, TRUE, FALSE,
      sei_data, size * 8, sei_nal, &size_nal);
  fail_if (ret != GST_H264_BIT_WRITER_OK);

  /* Parse it again. */
  res = gst_h264_parser_identify_nalu (parser, sei_nal, 0,
      sizeof (sei_nal), &nalu);
  assert_equals_int (res, GST_H264_PARSER_NO_NAL_END);

  res = gst_h264_parser_parse_sei (parser, &nalu, &sei_parsed);
  assert_equals_int (res, GST_H264_PARSER_OK);
  assert_equals_int (sei_parsed->len, 2);

  sei_msg_parsed = &g_array_index (sei_parsed, GstH264SEIMessage, 0);
  assert_equals_int (sei_msg_parsed->payloadType, GST_H264_SEI_PIC_TIMING);
  pic_timing_parsed = &sei_msg_parsed->payload.pic_timing;
  pic_timing =
      &((g_array_index (msg_array, GstH264SEIMessage, 0)).payload.pic_timing);

#define CHECK_FIELD(FIELD)  fail_if(pic_timing_parsed->FIELD != pic_timing->FIELD)
  CHECK_FIELD (CpbDpbDelaysPresentFlag);
  CHECK_FIELD (cpb_removal_delay_length_minus1);
  CHECK_FIELD (dpb_output_delay_length_minus1);
  CHECK_FIELD (cpb_removal_delay);
  CHECK_FIELD (dpb_output_delay);
  CHECK_FIELD (pic_struct_present_flag);
  CHECK_FIELD (pic_struct);
  CHECK_FIELD (clock_timestamp_flag[0]);
  CHECK_FIELD (clock_timestamp_flag[1]);
  CHECK_FIELD (clock_timestamp_flag[2]);
  CHECK_FIELD (clock_timestamp[0].ct_type);
  CHECK_FIELD (clock_timestamp[0].nuit_field_based_flag);
  CHECK_FIELD (clock_timestamp[0].counting_type);
  CHECK_FIELD (clock_timestamp[0].discontinuity_flag);
  CHECK_FIELD (clock_timestamp[0].cnt_dropped_flag);
  CHECK_FIELD (clock_timestamp[0].n_frames);
  CHECK_FIELD (clock_timestamp[0].seconds_flag);
  CHECK_FIELD (clock_timestamp[0].seconds_value);
  CHECK_FIELD (clock_timestamp[0].minutes_flag);
  CHECK_FIELD (clock_timestamp[0].minutes_value);
  CHECK_FIELD (clock_timestamp[0].hours_flag);
  CHECK_FIELD (clock_timestamp[0].hours_value);
  CHECK_FIELD (clock_timestamp[0].full_timestamp_flag);
  CHECK_FIELD (clock_timestamp[0].time_offset);
#undef CHECK_FIELD

  sei_msg_parsed = &g_array_index (sei_parsed, GstH264SEIMessage, 1);
  assert_equals_int (sei_msg_parsed->payloadType, GST_H264_SEI_BUF_PERIOD);
  buf_per_parsed = &sei_msg_parsed->payload.buffering_period;
  buf_per = &((g_array_index (msg_array, GstH264SEIMessage,
              1)).payload.buffering_period);

  fail_if (buf_per_parsed->nal_initial_cpb_removal_delay[0] !=
      buf_per->nal_initial_cpb_removal_delay[0]);
  fail_if (buf_per_parsed->nal_initial_cpb_removal_delay_offset[0] !=
      buf_per->nal_initial_cpb_removal_delay_offset[0]);

  g_array_unref (sei_parsed);
  g_array_unref (msg_array);
  gst_h264_nal_parser_free (parser);
}

GST_END_TEST;

static Suite *
h264bitwriter_suite (void)
{
  Suite *s = suite_create ("H264 bitwriter library");

  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_h264_bitwriter_sps_pps_slice_hdr);
  tcase_add_test (tc_chain, test_h264_bitwriter_sei);

  return s;
}

GST_CHECK_MAIN (h264bitwriter);
