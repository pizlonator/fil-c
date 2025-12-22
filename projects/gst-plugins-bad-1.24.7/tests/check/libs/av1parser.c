/* Gstreamer
 * Copyright (C) 2018 Georg Ottinger
 *    Author: Georg Ottinger <g.ottinger@gmx.at>
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

#include <gst/check/gstcheck.h>
#include <gst/codecparsers/gstav1parser.h>

static const guint8 aom_testdata_av1_1_b8_01_size_16x16[] = {
  0x12, 0x00, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x01, 0x9f, 0xfb, 0xff, 0xf3,
  0x00, 0x80, 0x32, 0xa6, 0x01, 0x10, 0x00, 0x87, 0x80, 0x00, 0x03, 0x00,
  0x00, 0x00, 0x40, 0x00, 0x9e, 0x86, 0x5b, 0xb2, 0x22, 0xb5, 0x58, 0x4d,
  0x68, 0xe6, 0x37, 0x54, 0x42, 0x7b, 0x84, 0xce, 0xdf, 0x9f, 0xec, 0xab,
  0x07, 0x4d, 0xf6, 0xe1, 0x5e, 0x9e, 0x27, 0xbf, 0x93, 0x2f, 0x47, 0x0d,
  0x7b, 0x7c, 0x45, 0x8d, 0xcf, 0x26, 0xf7, 0x6c, 0x06, 0xd7, 0x8c, 0x2e,
  0xf5, 0x2c, 0xb0, 0x8a, 0x31, 0xac, 0x69, 0xf5, 0xcd, 0xd8, 0x71, 0x5d,
  0xaf, 0xf8, 0x96, 0x43, 0x8c, 0x9c, 0x23, 0x6f, 0xab, 0xd0, 0x35, 0x43,
  0xdf, 0x81, 0x12, 0xe3, 0x7d, 0xec, 0x22, 0xb0, 0x30, 0x54, 0x32, 0x9f,
  0x90, 0xc0, 0x5d, 0x64, 0x9b, 0x0f, 0x75, 0x31, 0x84, 0x3a, 0x57, 0xd7,
  0x5f, 0x03, 0x6e, 0x7f, 0x43, 0x17, 0x6d, 0x08, 0xc3, 0x81, 0x8a, 0xae,
  0x73, 0x1c, 0xa8, 0xa7, 0xe4, 0x9c, 0xa9, 0x5b, 0x3f, 0xd1, 0xeb, 0x75,
  0x3a, 0x7f, 0x22, 0x77, 0x38, 0x64, 0x1c, 0x77, 0xdb, 0xcd, 0xef, 0xb7,
  0x08, 0x45, 0x8e, 0x7f, 0xea, 0xa3, 0xd0, 0x81, 0xc9, 0xc1, 0xbc, 0x93,
  0x9b, 0x41, 0xb1, 0xa1, 0x42, 0x17, 0x98, 0x3f, 0x1e, 0x95, 0xdf, 0x68,
  0x7c, 0xb7, 0x98, 0x12, 0x00, 0x32, 0x4b, 0x30, 0x03, 0xc3, 0x00, 0xa7,
  0x2e, 0x46, 0x8a, 0x00, 0x00, 0x03, 0x00, 0x00, 0x50, 0xc0, 0x20, 0x00,
  0xf0, 0xb1, 0x2f, 0x43, 0xf3, 0xbb, 0xe6, 0x5c, 0xbe, 0xe6, 0x53, 0xbc,
  0xaa, 0x61, 0x7c, 0x7e, 0x0a, 0x04, 0x1b, 0xa2, 0x87, 0x81, 0xe8, 0xa6,
  0x85, 0xfe, 0xc2, 0x71, 0xb9, 0xf8, 0xc0, 0x78, 0x9f, 0x52, 0x4f, 0xa7,
  0x8f, 0x55, 0x96, 0x79, 0x90, 0xaa, 0x2b, 0x6d, 0x0a, 0xa7, 0x05, 0x2a,
  0xf8, 0xfc, 0xc9, 0x7d, 0x9d, 0x4a, 0x61, 0x16, 0xb1, 0x65
};

/* testdata taken from aom testdata deecoded and reencoded with annexb */
static const guint8 aom_testdata_av1_1_b8_01_size_16x16_reencoded_annexb[] = {
  0x8b, 0x02, 0x89, 0x02, 0x01, 0x10, 0x0b, 0x08, 0x00, 0x00, 0x00, 0x01,
  0x9f, 0xfb, 0xff, 0xf3, 0x00, 0x80, 0xf9, 0x01, 0x30, 0x10, 0x01, 0x80,
  0x00, 0xef, 0x38, 0x58, 0x9e, 0x27, 0x8c, 0x26, 0xc4, 0x61, 0x19, 0x41,
  0xff, 0x4f, 0x8c, 0xc9, 0x24, 0x93, 0x38, 0x20, 0x61, 0x7a, 0xc9, 0x5c,
  0xb8, 0xa7, 0xf2, 0x90, 0x41, 0x9e, 0xac, 0x22, 0x39, 0x4c, 0xd5, 0xf9,
  0x9e, 0xa9, 0xb1, 0x84, 0x43, 0x76, 0xd1, 0x7f, 0x96, 0x7d, 0xff, 0x66,
  0x7e, 0x39, 0x61, 0xe4, 0xce, 0x20, 0x39, 0xf6, 0xb5, 0xc7, 0xe2, 0x32,
  0xc0, 0x5e, 0xa4, 0x0a, 0x9e, 0x6b, 0xc4, 0x1d, 0x50, 0x04, 0xc9, 0x93,
  0x9c, 0x4c, 0xbb, 0x26, 0xd7, 0xe4, 0x1b, 0xcb, 0xa7, 0x20, 0x08, 0xd4,
  0xeb, 0x7e, 0x50, 0x83, 0x48, 0x71, 0x50, 0x01, 0xd1, 0x6c, 0xe7, 0xc1,
  0x00, 0x21, 0x5e, 0x96, 0xc6, 0x2a, 0x25, 0x81, 0xa7, 0x7e, 0x59, 0x70,
  0x34, 0x12, 0x84, 0xc0, 0xb8, 0xdc, 0xcf, 0xa1, 0xaf, 0xb2, 0x62, 0x64,
  0x2e, 0x7b, 0x03, 0x31, 0x9d, 0x43, 0xba, 0xd2, 0xb5, 0x4c, 0xab, 0xf0,
  0x20, 0x45, 0xdf, 0xf9, 0xcb, 0xdb, 0xe3, 0xe0, 0x73, 0xef, 0x4d, 0x1d,
  0xd7, 0xeb, 0xd9, 0x1f, 0xba, 0x33, 0xd8, 0x98, 0xe7, 0xe4, 0x72, 0x2f,
  0x19, 0x7c, 0x0d, 0xc8, 0x6c, 0x30, 0xa5, 0xbb, 0xb5, 0xb5, 0x8c, 0x69,
  0x52, 0xd4, 0xe5, 0x95, 0x15, 0xd7, 0xe6, 0x74, 0x8b, 0xe4, 0x8f, 0x38,
  0x52, 0xbc, 0x52, 0xcc, 0x97, 0x4e, 0x77, 0xf8, 0xab, 0xcc, 0x40, 0x3a,
  0x0c, 0x73, 0x56, 0x86, 0x66, 0x5b, 0xc2, 0xa9, 0x90, 0xea, 0xc7, 0xf4,
  0x1e, 0xd3, 0x35, 0x79, 0xd6, 0x7e, 0xc9, 0xd0, 0x83, 0x44, 0x8f, 0x5f,
  0xef, 0x3e, 0x0c, 0x38, 0xfe, 0xff, 0x17, 0x28, 0xff, 0x98, 0xf8, 0x6b,
  0xf2, 0x31, 0xc6, 0x58, 0x9a, 0x4c, 0xc2, 0x6c, 0x4e, 0xa7, 0xf2, 0xeb,
  0x9f, 0xfb, 0xd7, 0xdc, 0x30, 0xfb, 0x01, 0xf9, 0x01, 0x01, 0x10, 0xf5,
  0x01, 0x30, 0x30, 0x03, 0xc3, 0x00, 0xa7, 0x2e, 0x47, 0x80, 0x01, 0x00,
  0xc1, 0xc9, 0x8b, 0x3d, 0xd7, 0x44, 0x93, 0x49, 0xf8, 0xad, 0x73, 0x89,
  0x29, 0x50, 0x60, 0x35, 0x87, 0x2d, 0xbe, 0xde, 0x00, 0x4e, 0xa2, 0x75,
  0x62, 0xd7, 0xda, 0x28, 0xc4, 0xec, 0x65, 0xed, 0xcd, 0xbd, 0xa3, 0xd1,
  0x71, 0x8d, 0x49, 0x4e, 0xa1, 0xcd, 0xf1, 0xd0, 0x20, 0xb6, 0xd2, 0xda,
  0xe3, 0xc5, 0xab, 0xd6, 0xff, 0xb0, 0xd0, 0xff, 0x1f, 0x86, 0x79, 0x2e,
  0x69, 0x89, 0xce, 0x07, 0x72, 0x4f, 0xe8, 0xff, 0x22, 0xca, 0x08, 0x32,
  0x29, 0xdb, 0xb5, 0xfb, 0x75, 0x52, 0x6e, 0xf3, 0x32, 0x3c, 0x55, 0x9f,
  0x97, 0x9e, 0x1e, 0x1a, 0x51, 0x1d, 0xf4, 0x15, 0x16, 0xa0, 0xea, 0xec,
  0x64, 0xd3, 0xff, 0xd9, 0x7a, 0xb7, 0x91, 0x10, 0x4b, 0xfd, 0x7a, 0x49,
  0x62, 0xae, 0x46, 0xa8, 0x4b, 0x53, 0x15, 0xba, 0x27, 0x6d, 0x5b, 0x72,
  0x5f, 0x7e, 0x63, 0xc6, 0x70, 0x79, 0x84, 0xe4, 0x2e, 0x3e, 0xfd, 0xdf,
  0xeb, 0xf1, 0x2a, 0xe5, 0xc7, 0x68, 0x8e, 0x65, 0xfe, 0x0d, 0x1e, 0xea,
  0xce, 0x0f, 0x83, 0x47, 0xfc, 0x11, 0x18, 0x0f, 0x2d, 0x29, 0x8e, 0xff,
  0xbc, 0x5e, 0x7b, 0x45, 0x2e, 0x51, 0xd1, 0xa8, 0xdb, 0xd7, 0xbe, 0x1a,
  0xf2, 0x59, 0xa3, 0x0b, 0x96, 0x5a, 0xc1, 0x81, 0x0e, 0xc9, 0xe9, 0x3d,
  0x1c, 0x75, 0x41, 0xbe, 0x46, 0xba, 0xb1, 0x55, 0x95, 0xe1, 0x1a, 0x89,
  0xce, 0x4f, 0xf4, 0x78, 0x9b, 0x71, 0x49, 0xe8, 0xf7, 0x58, 0x5b, 0xca,
  0xde, 0xc3, 0x8f, 0x41, 0x80, 0xdd, 0xcc, 0xf8, 0xb6, 0x50, 0x24, 0x0d,
  0x53, 0xa1, 0xcf, 0x5a, 0xc8, 0xc4, 0x81, 0x83, 0x2c, 0x2f, 0xfc, 0x37,
  0x82, 0x67, 0xb6, 0x8a, 0xdc, 0xe0
};

/* hand crafted test case for metadata */
static const guint8 metadata_obu[] = {
  0x2a, 0x05, 0x01, 0x12, 0x34, 0x56, 0x78
};

/* hand crafted test case for tile list */
static const guint8 tile_list_obu[] = {
  0x42, 0x0a, 0x01, 0x01, 0x00, 0x01, 0x11, 0x22, 0x33, 0x00, 0x01, 0xa5
};

GST_START_TEST (test_av1_parse_aom_testdata_av1_1_b8_01_size_16x16)
{
  GstAV1Parser *parser;
  GstAV1SequenceHeaderOBU seq_header;
  GstAV1FrameOBU frame;
  GstAV1OBU obu;
  GstAV1ParserResult ret;
  guint32 consumed = 0;
  const guint8 *data_ptr = aom_testdata_av1_1_b8_01_size_16x16;
  guint data_sz = sizeof (aom_testdata_av1_1_b8_01_size_16x16);

  parser = gst_av1_parser_new ();
  gst_av1_parser_reset (parser, FALSE);
  ret = gst_av1_parser_identify_one_obu (parser, data_ptr, data_sz,
      &obu, &consumed);
  assert_equals_int (ret, GST_AV1_PARSER_OK);
  data_ptr += consumed;
  data_sz -= consumed;

  /* 1st OBU should be OBU_TEMPORAL_DELIMITER */
  assert_equals_int (obu.obu_type, GST_AV1_OBU_TEMPORAL_DELIMITER);
  assert_equals_int (obu.header.obu_extention_flag, 0);
  assert_equals_int (obu.header.obu_has_size_field, 1);
  assert_equals_int (obu.obu_size, 0);
  ret = gst_av1_parser_parse_temporal_delimiter_obu (parser, &obu);
  assert_equals_int (ret, GST_AV1_PARSER_OK);

  /* 2nd OBU should be OBU_SEQUENCE_HEADER */
  ret = gst_av1_parser_identify_one_obu (parser, data_ptr, data_sz,
      &obu, &consumed);
  assert_equals_int (ret, GST_AV1_PARSER_OK);
  data_ptr += consumed;
  data_sz -= consumed;

  assert_equals_int (obu.obu_type, GST_AV1_OBU_SEQUENCE_HEADER);
  assert_equals_int (obu.header.obu_extention_flag, 0);
  assert_equals_int (obu.header.obu_has_size_field, 1);
  assert_equals_int (obu.obu_size, 10);

  ret = gst_av1_parser_parse_sequence_header_obu (parser, &obu, &seq_header);
  assert_equals_int (ret, GST_AV1_PARSER_OK);

  assert_equals_int (seq_header.seq_profile, GST_AV1_PROFILE_0);
  assert_equals_int (seq_header.still_picture, 0);
  assert_equals_int (seq_header.reduced_still_picture_header, 0);
  assert_equals_int (seq_header.timing_info_present_flag, 0);
  assert_equals_int (seq_header.initial_display_delay_present_flag, 0);
  assert_equals_int (seq_header.operating_points_cnt_minus_1, 0);
  assert_equals_int (seq_header.operating_points[0].idc, 0);
  assert_equals_int (seq_header.operating_points[0].seq_level_idx, 0);
  assert_equals_int (seq_header.frame_width_bits_minus_1, 3);
  assert_equals_int (seq_header.frame_height_bits_minus_1, 3);
  assert_equals_int (seq_header.max_frame_width_minus_1, 15);
  assert_equals_int (seq_header.max_frame_height_minus_1, 15);
  assert_equals_int (seq_header.frame_id_numbers_present_flag, 0);
  assert_equals_int (seq_header.use_128x128_superblock, 1);
  assert_equals_int (seq_header.enable_filter_intra, 1);
  assert_equals_int (seq_header.enable_intra_edge_filter, 1);
  assert_equals_int (seq_header.enable_interintra_compound, 1);
  assert_equals_int (seq_header.enable_masked_compound, 1);
  assert_equals_int (seq_header.enable_warped_motion, 1);
  assert_equals_int (seq_header.enable_dual_filter, 1);
  assert_equals_int (seq_header.enable_order_hint, 1);
  assert_equals_int (seq_header.enable_jnt_comp, 1);
  assert_equals_int (seq_header.enable_ref_frame_mvs, 1);
  assert_equals_int (seq_header.seq_choose_screen_content_tools, 1);
  assert_equals_int (seq_header.seq_choose_integer_mv, 1);
  assert_equals_int (seq_header.order_hint_bits_minus_1, 6);
  assert_equals_int (seq_header.enable_superres, 0);
  assert_equals_int (seq_header.enable_cdef, 1);
  assert_equals_int (seq_header.enable_restoration, 1);
  assert_equals_int (seq_header.color_config.high_bitdepth, 0);
  assert_equals_int (seq_header.color_config.mono_chrome, 0);
  assert_equals_int (seq_header.color_config.color_description_present_flag, 0);
  assert_equals_int (seq_header.color_config.chroma_sample_position,
      GST_AV1_CSP_UNKNOWN);
  assert_equals_int (seq_header.color_config.separate_uv_delta_q, 0);
  assert_equals_int (seq_header.film_grain_params_present, 0);

  /* 3rd OBU should be GST_AV1_OBU_FRAME */
  ret = gst_av1_parser_identify_one_obu (parser, data_ptr, data_sz,
      &obu, &consumed);
  assert_equals_int (ret, GST_AV1_PARSER_OK);
  data_ptr += consumed;
  data_sz -= consumed;

  assert_equals_int (obu.obu_type, GST_AV1_OBU_FRAME);
  assert_equals_int (obu.header.obu_extention_flag, 0);
  assert_equals_int (obu.header.obu_has_size_field, 1);
  assert_equals_int (obu.obu_size, 166);

  ret = gst_av1_parser_parse_frame_obu (parser, &obu, &frame);
  assert_equals_int (ret, GST_AV1_PARSER_OK);

  assert_equals_int (frame.frame_header.show_existing_frame, 0);
  assert_equals_int (frame.frame_header.frame_type, GST_AV1_KEY_FRAME);
  assert_equals_int (frame.frame_header.show_frame, 1);
  assert_equals_int (frame.frame_header.disable_cdf_update, 0);
  assert_equals_int (frame.frame_header.allow_screen_content_tools, 0);
  assert_equals_int (frame.frame_header.frame_size_override_flag, 0);
  assert_equals_int (frame.frame_header.order_hint, 0);
  assert_equals_int (frame.frame_header.render_and_frame_size_different, 0);
  assert_equals_int (frame.frame_header.disable_frame_end_update_cdf, 0);
  assert_equals_int (frame.frame_header.tile_info.uniform_tile_spacing_flag, 1);
  assert_equals_int (frame.frame_header.quantization_params.base_q_idx, 15);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_y_dc, 0);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_u_dc, 0);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_u_ac, 0);
  assert_equals_int (frame.frame_header.quantization_params.using_qmatrix, 0);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_present, 0);
  assert_equals_int (frame.frame_header.loop_filter_params.loop_filter_level[0],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.loop_filter_level[1],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.
      loop_filter_sharpness, 0);
  assert_equals_int (frame.frame_header.loop_filter_params.
      loop_filter_delta_enabled, 1);
  assert_equals_int (frame.frame_header.loop_filter_params.
      loop_filter_delta_update, 1);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[0], 1);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[1], 0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[2], 0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[3], 0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[4], -1);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[5], 0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[6], -1);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[7], -1);
  assert_equals_int (frame.frame_header.loop_filter_params.
      loop_filter_mode_deltas[0], 0);
  assert_equals_int (frame.frame_header.loop_filter_params.
      loop_filter_mode_deltas[1], 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_damping, 3);
  assert_equals_int (frame.frame_header.cdef_params.cdef_bits, 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_y_pri_strength[0], 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_y_sec_strength[0], 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_uv_pri_strength[0], 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_uv_sec_strength[0], 1);
  assert_equals_int (frame.frame_header.loop_restoration_params.
      frame_restoration_type[0], GST_AV1_FRAME_RESTORE_NONE);
  assert_equals_int (frame.frame_header.loop_restoration_params.
      frame_restoration_type[1], GST_AV1_FRAME_RESTORE_NONE);
  assert_equals_int (frame.frame_header.loop_restoration_params.
      frame_restoration_type[2], GST_AV1_FRAME_RESTORE_NONE);
  assert_equals_int (frame.frame_header.tx_mode_select, 0);
  assert_equals_int (frame.frame_header.reduced_tx_set, 0);

  /* 4th OBU should be OBU_TEMPORAL_DELIMITER */
  ret = gst_av1_parser_identify_one_obu (parser, data_ptr, data_sz,
      &obu, &consumed);
  assert_equals_int (ret, GST_AV1_PARSER_OK);
  data_ptr += consumed;
  data_sz -= consumed;

  assert_equals_int (obu.obu_type, GST_AV1_OBU_TEMPORAL_DELIMITER);
  assert_equals_int (obu.header.obu_extention_flag, 0);
  assert_equals_int (obu.header.obu_has_size_field, 1);
  assert_equals_int (obu.obu_size, 0);

  ret = gst_av1_parser_parse_temporal_delimiter_obu (parser, &obu);
  assert_equals_int (ret, GST_AV1_PARSER_OK);

  /* 5th OBU should be GST_AV1_OBU_FRAME */
  ret = gst_av1_parser_identify_one_obu (parser, data_ptr, data_sz,
      &obu, &consumed);
  assert_equals_int (ret, GST_AV1_PARSER_OK);
  data_ptr += consumed;
  data_sz -= consumed;

  assert_equals_int (obu.obu_type, GST_AV1_OBU_FRAME);
  assert_equals_int (obu.header.obu_extention_flag, 0);
  assert_equals_int (obu.header.obu_has_size_field, 1);
  assert_equals_int (obu.obu_size, 75);

  ret = gst_av1_parser_parse_frame_obu (parser, &obu, &frame);
  assert_equals_int (ret, GST_AV1_PARSER_OK);

  assert_equals_int (frame.frame_header.show_existing_frame, 0);
  assert_equals_int (frame.frame_header.frame_type, GST_AV1_INTER_FRAME);
  assert_equals_int (frame.frame_header.show_frame, 1);
  assert_equals_int (frame.frame_header.error_resilient_mode, 0);
  assert_equals_int (frame.frame_header.disable_cdf_update, 0);
  assert_equals_int (frame.frame_header.allow_screen_content_tools, 0);
  assert_equals_int (frame.frame_header.frame_size_override_flag, 0);
  assert_equals_int (frame.frame_header.order_hint, 1);
  assert_equals_int (frame.frame_header.primary_ref_frame, 7);
  assert_equals_int (frame.frame_header.refresh_frame_flags, 12);
  assert_equals_int (frame.frame_header.frame_refs_short_signaling, 0);
  assert_equals_int (frame.frame_header.ref_frame_idx[0], 0);
  assert_equals_int (frame.frame_header.ref_frame_idx[1], 1);
  assert_equals_int (frame.frame_header.ref_frame_idx[2], 2);
  assert_equals_int (frame.frame_header.ref_frame_idx[3], 3);
  assert_equals_int (frame.frame_header.ref_frame_idx[4], 4);
  assert_equals_int (frame.frame_header.ref_frame_idx[5], 5);
  assert_equals_int (frame.frame_header.ref_frame_idx[6], 6);
  assert_equals_int (frame.frame_header.allow_high_precision_mv, 1);
  assert_equals_int (frame.frame_header.is_filter_switchable, 0);
  assert_equals_int (frame.frame_header.interpolation_filter,
      GST_AV1_INTERPOLATION_FILTER_EIGHTTAP);
  assert_equals_int (frame.frame_header.is_motion_mode_switchable, 1);
  assert_equals_int (frame.frame_header.use_ref_frame_mvs, 1);
  assert_equals_int (frame.frame_header.disable_frame_end_update_cdf, 0);
  assert_equals_int (frame.frame_header.quantization_params.base_q_idx, 20);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_y_dc, 0);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_u_dc, 0);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_u_ac, 0);
  assert_equals_int (frame.frame_header.quantization_params.using_qmatrix, 0);
  assert_equals_int (frame.frame_header.segmentation_params.
      segmentation_enabled, 0);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_present, 0);
  assert_equals_int (frame.frame_header.loop_filter_params.loop_filter_level[0],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.loop_filter_level[1],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.
      loop_filter_sharpness, 0);
  assert_equals_int (frame.frame_header.loop_filter_params.
      loop_filter_delta_enabled, 1);
  assert_equals_int (frame.frame_header.loop_filter_params.
      loop_filter_delta_update, 1);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[0], 1);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[1], 0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[2], 0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[3], 0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[4], -1);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[5], 0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[6], -1);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_ref_deltas[7], -1);
  assert_equals_int (frame.frame_header.loop_filter_params.
      loop_filter_mode_deltas[0], 0);
  assert_equals_int (frame.frame_header.loop_filter_params.
      loop_filter_mode_deltas[1], 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_damping, 3);
  assert_equals_int (frame.frame_header.cdef_params.cdef_bits, 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_y_pri_strength[0], 1);
  assert_equals_int (frame.frame_header.cdef_params.cdef_y_sec_strength[0], 1);
  assert_equals_int (frame.frame_header.cdef_params.cdef_uv_pri_strength[0], 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_uv_sec_strength[0], 4);
  assert_equals_int (frame.frame_header.loop_restoration_params.
      frame_restoration_type[0], GST_AV1_FRAME_RESTORE_NONE);
  assert_equals_int (frame.frame_header.loop_restoration_params.
      frame_restoration_type[1], GST_AV1_FRAME_RESTORE_NONE);
  assert_equals_int (frame.frame_header.loop_restoration_params.
      frame_restoration_type[2], GST_AV1_FRAME_RESTORE_NONE);
  assert_equals_int (frame.frame_header.tx_mode_select, 0);
  assert_equals_int (frame.frame_header.reference_select, 0);
  assert_equals_int (frame.frame_header.allow_warped_motion, 1);
  assert_equals_int (frame.frame_header.reduced_tx_set, 0);
  assert_equals_int (frame.frame_header.global_motion_params.is_global[1], 0);
  assert_equals_int (frame.frame_header.global_motion_params.is_global[2], 0);
  assert_equals_int (frame.frame_header.global_motion_params.is_global[3], 0);
  assert_equals_int (frame.frame_header.global_motion_params.is_global[4], 0);
  assert_equals_int (frame.frame_header.global_motion_params.is_global[5], 0);
  assert_equals_int (frame.frame_header.global_motion_params.is_global[6], 0);

  gst_av1_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST
    (test_av1_parse_aom_testdata_av1_1_b8_01_size_16x16_reencoded_annexb) {
  GstAV1Parser *parser;
  GstAV1OBU obu;
  GstAV1SequenceHeaderOBU seq_header;
  GstAV1FrameOBU frame;
  gsize size;
  guint32 consumed = 0;
  const guint8 *data;
  GstAV1ParserResult ret;

  parser = gst_av1_parser_new ();

  data = aom_testdata_av1_1_b8_01_size_16x16_reencoded_annexb;
  size = sizeof (aom_testdata_av1_1_b8_01_size_16x16_reencoded_annexb);

  gst_av1_parser_reset (parser, TRUE);

  /* 1st OBU should be OBU_TEMPORAL_DELIMITER */
  ret = gst_av1_parser_identify_one_obu (parser, data, size, &obu, &consumed);
  assert_equals_int (ret, GST_AV1_PARSER_OK);
  data += consumed;
  size -= consumed;

  assert_equals_int (obu.header.obu_type, GST_AV1_OBU_TEMPORAL_DELIMITER);
  assert_equals_int (obu.header.obu_extention_flag, 0);
  assert_equals_int (obu.header.obu_has_size_field, 0);
  assert_equals_int (obu.obu_size, 0);

  gst_av1_parser_parse_temporal_delimiter_obu (parser, &obu);

  /* 2nd OBU should be OBU_SEQUENCE_HEADER */
  ret = gst_av1_parser_identify_one_obu (parser, data, size, &obu, &consumed);
  assert_equals_int (ret, GST_AV1_PARSER_OK);
  data += consumed;
  size -= consumed;

  assert_equals_int (obu.header.obu_type, GST_AV1_OBU_SEQUENCE_HEADER);
  assert_equals_int (obu.header.obu_extention_flag, 0);
  assert_equals_int (obu.header.obu_has_size_field, 0);
  assert_equals_int (obu.obu_size, 10);

  gst_av1_parser_parse_sequence_header_obu (parser, &obu, &seq_header);
  assert_equals_int (seq_header.seq_profile, GST_AV1_PROFILE_0);
  assert_equals_int (seq_header.frame_width_bits_minus_1, 3);
  assert_equals_int (seq_header.frame_height_bits_minus_1, 3);
  assert_equals_int (seq_header.max_frame_width_minus_1, 15);
  assert_equals_int (seq_header.max_frame_height_minus_1, 15);

  /* 3rd OBU should be GST_AV1_OBU_FRAME */
  ret = gst_av1_parser_identify_one_obu (parser, data, size, &obu, &consumed);
  assert_equals_int (ret, GST_AV1_PARSER_OK);
  data += consumed;
  size -= consumed;

  assert_equals_int (obu.header.obu_type, GST_AV1_OBU_FRAME);
  assert_equals_int (obu.header.obu_extention_flag, 0);
  assert_equals_int (obu.header.obu_has_size_field, 0);
  assert_equals_int (obu.obu_size, 248);

  gst_av1_parser_parse_frame_obu (parser, &obu, &frame);
  assert_equals_int (frame.frame_header.show_existing_frame, 0);
  assert_equals_int (frame.frame_header.frame_type, GST_AV1_KEY_FRAME);
  assert_equals_int (frame.frame_header.show_frame, 1);
  assert_equals_int (frame.frame_header.quantization_params.base_q_idx, 0);

  assert_equals_int (frame.tile_group.num_tiles, 1);

  /* 4th OBU should be OBU_TEMPORAL_DELIMITER */
  ret = gst_av1_parser_identify_one_obu (parser, data, size, &obu, &consumed);
  assert_equals_int (ret, GST_AV1_PARSER_OK);
  data += consumed;
  size -= consumed;

  assert_equals_int (obu.header.obu_type, GST_AV1_OBU_TEMPORAL_DELIMITER);
  assert_equals_int (obu.header.obu_extention_flag, 0);
  assert_equals_int (obu.header.obu_has_size_field, 0);
  assert_equals_int (obu.obu_size, 0);

  gst_av1_parser_parse_temporal_delimiter_obu (parser, &obu);

  /* 5th OBU should be GST_AV1_OBU_FRAME */
  ret = gst_av1_parser_identify_one_obu (parser, data, size, &obu, &consumed);
  assert_equals_int (ret, GST_AV1_PARSER_OK);
  data += consumed;
  size -= consumed;

  assert_equals_int (obu.header.obu_type, GST_AV1_OBU_FRAME);
  assert_equals_int (obu.header.obu_extention_flag, 0);
  assert_equals_int (obu.header.obu_has_size_field, 0);
  assert_equals_int (obu.obu_size, 244);

  gst_av1_parser_parse_frame_obu (parser, &obu, &frame);
  assert_equals_int (frame.frame_header.show_existing_frame, 0);
  assert_equals_int (frame.frame_header.frame_type, GST_AV1_INTER_FRAME);
  assert_equals_int (frame.frame_header.show_frame, 1);
  assert_equals_int (frame.frame_header.quantization_params.base_q_idx, 0);

  assert_equals_int (frame.tile_group.num_tiles, 1);

  gst_av1_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_metadata_obu)
{
  GstAV1Parser *parser;
  GstAV1OBU obu;
  GstAV1MetadataOBU metadata;
  guint32 consumed = 0;
  gsize size;
  const guint8 *data;
  GstAV1ParserResult ret;

  parser = gst_av1_parser_new ();

  data = metadata_obu;
  size = sizeof (metadata_obu);

  gst_av1_parser_reset (parser, FALSE);

  ret = gst_av1_parser_identify_one_obu (parser, data, size, &obu, &consumed);
  assert_equals_int (ret, GST_AV1_PARSER_OK);
  data += consumed;
  size -= consumed;

  assert_equals_int (obu.header.obu_type, GST_AV1_OBU_METADATA);
  assert_equals_int (obu.header.obu_extention_flag, 0);
  assert_equals_int (obu.header.obu_has_size_field, 1);
  assert_equals_int (obu.obu_size, 5);

  gst_av1_parser_parse_metadata_obu (parser, &obu, &metadata);

  assert_equals_int (metadata.metadata_type, GST_AV1_METADATA_TYPE_HDR_CLL);
  assert_equals_int (metadata.hdr_cll.max_cll, 0x1234);
  assert_equals_int (metadata.hdr_cll.max_fall, 0x5678);

  gst_av1_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_tile_list_obu)
{
  GstAV1Parser *parser;
  GstAV1OBU obu;
  GstAV1TileListOBU tile_list;
  guint32 consumed = 0;
  gsize size;
  const guint8 *data;
  GstAV1ParserResult ret;

  parser = gst_av1_parser_new ();

  data = tile_list_obu;
  size = sizeof (tile_list_obu);

  gst_av1_parser_reset (parser, FALSE);

  ret = gst_av1_parser_identify_one_obu (parser, data, size, &obu, &consumed);
  assert_equals_int (ret, GST_AV1_PARSER_OK);
  data += consumed;
  size -= consumed;

  assert_equals_int (obu.header.obu_type, GST_AV1_OBU_TILE_LIST);
  assert_equals_int (obu.header.obu_extention_flag, 0);
  assert_equals_int (obu.header.obu_has_size_field, 1);
  assert_equals_int (obu.obu_size, 10);

  gst_av1_parser_parse_tile_list_obu (parser, &obu, &tile_list);

  assert_equals_int (tile_list.output_frame_width_in_tiles_minus_1, 1);
  assert_equals_int (tile_list.output_frame_height_in_tiles_minus_1, 1);
  assert_equals_int (tile_list.tile_count_minus_1, 1);

  assert_equals_int (tile_list.entry[0].anchor_frame_idx, 0x11);
  assert_equals_int (tile_list.entry[0].anchor_tile_row, 0x22);
  assert_equals_int (tile_list.entry[0].anchor_tile_col, 0x33);
  assert_equals_int (tile_list.entry[0].tile_data_size_minus_1, 0x01);
  assert_equals_int (tile_list.entry[0].coded_tile_data[0], 0xa5);

  gst_av1_parser_free (parser);
}

GST_END_TEST;

static Suite *
av1parsers_suite (void)
{
  Suite *s = suite_create ("AV1 Parser library");

  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_av1_parse_aom_testdata_av1_1_b8_01_size_16x16);
  tcase_add_test (tc_chain,
      test_av1_parse_aom_testdata_av1_1_b8_01_size_16x16_reencoded_annexb);
  tcase_add_test (tc_chain, test_metadata_obu);
  tcase_add_test (tc_chain, test_tile_list_obu);

  return s;
}

GST_CHECK_MAIN (av1parsers);
