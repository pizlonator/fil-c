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
#include <gst/codecparsers/gstav1bitwriter.h>

/* *INDENT-OFF* */
static const GstAV1SequenceHeaderOBU sequence = {
  .seq_profile = 0,
  .still_picture = 0,
  .num_planes = 3,
  .reduced_still_picture_header = 0,
  .timing_info_present_flag = 1,
  .timing_info = {
    .num_units_in_display_tick = 127,
    .time_scale = 10,
    .equal_picture_interval = 1,
    .num_ticks_per_picture_minus_1 = 1082,
  },
  .decoder_model_info_present_flag = 0,
  .initial_display_delay_present_flag = 0,
  .operating_points_cnt_minus_1 = 3,
  .operating_points = {
    {
      .seq_level_idx = 1,
      .seq_tier = 0,
      .idc = 771,
    },
    {
      .seq_level_idx = 1,
      .seq_tier = 0,
      .idc = 769,
    },
    {
      .seq_level_idx = 1,
      .seq_tier = 0,
      .idc = 259,
    },
    {
      .seq_level_idx = 1,
      .seq_tier = 0,
      .idc = 257,
    },
  },

  .frame_width_bits_minus_1 = 10,
  .frame_height_bits_minus_1 = 9,
  .max_frame_width_minus_1 = 1279,
  .max_frame_height_minus_1 = 719,

  .frame_id_numbers_present_flag = 0,
  .use_128x128_superblock = 1,
  .enable_filter_intra = 1,
  .enable_intra_edge_filter = 1,
  .enable_interintra_compound = 1,
  .enable_masked_compound = 1,
  .enable_warped_motion = 1,
  .enable_dual_filter = 1,
  .enable_order_hint = 1,
  .enable_jnt_comp = 1,
  .enable_ref_frame_mvs = 1,
  .seq_choose_screen_content_tools = 1,
  .seq_force_screen_content_tools = GST_AV1_SELECT_SCREEN_CONTENT_TOOLS,
  .seq_choose_integer_mv = 1,
  .order_hint_bits_minus_1 = 6,
  .enable_superres = 0,
  .enable_cdef = 1,
  .enable_restoration = 1,

  .color_config = {
    .high_bitdepth = 0,
    .mono_chrome = 0,
    .color_description_present_flag = 1,
    .color_primaries = 1,
    .transfer_characteristics = 1,
    .matrix_coefficients = 1,
    .color_range = 0,
    .subsampling_x = 1,
    .subsampling_y = 1,
    .chroma_sample_position = 0,
    .separate_uv_delta_q = 0,
  },

  .film_grain_params_present = 0,
};

static const GstAV1FrameHeaderOBU key_frame = {
  /* default setting */
  .frame_is_intra = 1,
  .last_frame_idx = -1,
  .gold_frame_idx = -1,
  .ref_frame_idx = { -1, -1, -1, -1, -1, -1, -1 },

  .show_existing_frame = 0,
  .frame_type = 0,
  .show_frame = 1,
  .disable_cdf_update = 0,
  .allow_screen_content_tools = 0,
  .frame_size_override_flag = 1,
  .order_hint = 0,
  .primary_ref_frame = 7,
  .frame_width = 640,
  .frame_height = 360,
  .use_superres = 0,
  .render_and_frame_size_different = 0,
  .disable_frame_end_update_cdf = 1,
  .tile_info = {
    .uniform_tile_spacing_flag = 1,
    .tile_cols_log2 = 2,
    .tile_rows_log2 = 0,
    .context_update_tile_id = 1,
    .tile_size_bytes_minus_1 = 3,
  },
  .quantization_params = {
    .base_q_idx = 19,
    .delta_q_y_dc = 2,
    .delta_q_u_dc = 3,
    .delta_q_u_ac = -5,
    .using_qmatrix = 0,

    .delta_q_present = 0,
  },
  .segmentation_params = {
    .segmentation_enabled = 0,
  },
  .loop_filter_params = {
    .loop_filter_level = { 0, 0, },
    .loop_filter_sharpness = 1,
    .loop_filter_delta_enabled = 1,
    .loop_filter_delta_update = 1,
    .loop_filter_ref_deltas = { 1, 2, 0, 0, -1, 0, -1, -1 },
    .loop_filter_mode_deltas = { 0, 0 },
  },
  .cdef_params = {
    .cdef_damping = 3,
    .cdef_bits = 3,
    .cdef_y_pri_strength = { 0, 0, 2, 0, 0, 0, 0, 0, },
    .cdef_y_sec_strength = { 0, 1, 0, 1, 1, 1, 3, 0, },
    .cdef_uv_pri_strength = { 0, 0, 2, 1, 0, 0, 0, 0, },
    .cdef_uv_sec_strength = { 1, 1, 0, 0, 0, 3, 0, 3, },
  },
  .loop_restoration_params = {
    .uses_lr = 1,
    .frame_restoration_type[0] = GST_AV1_FRAME_RESTORE_SWITCHABLE,
    .frame_restoration_type[1] = GST_AV1_FRAME_RESTORE_SGRPROJ,
    .frame_restoration_type[2] = GST_AV1_FRAME_RESTORE_SGRPROJ,
    .lr_unit_shift = 1,
    .lr_uv_shift = 0,
  },
  .tx_mode = GST_AV1_TX_MODE_SELECT,
  .reduced_tx_set = 0,
};

static const GstAV1FrameHeaderOBU inter_frame = {
  /* default setting */
  .frame_is_intra = 0,

  .show_existing_frame = 0,
  .frame_type = 1,
  .show_frame = 1,
  .error_resilient_mode = 0,
  .disable_cdf_update = 0,
  .allow_screen_content_tools = 0,
  .frame_size_override_flag = 0,
  .frame_width = 1280,
  .frame_height = 720,
  .order_hint = 1,
  .primary_ref_frame = 7,
  .refresh_frame_flags = 0x08,
  .frame_refs_short_signaling = 0,
  .ref_frame_idx = { 0, 1, 2, 3, 4, 5, 6 },
  .render_and_frame_size_different = 0,
  .allow_high_precision_mv = 1,
  .is_filter_switchable = 1,
  .is_motion_mode_switchable = 1,
  .use_ref_frame_mvs = 1,
  .disable_frame_end_update_cdf = 1,
  .tile_info = {
    .uniform_tile_spacing_flag = 1,
    .tile_cols_log2 = 2,
    .tile_rows_log2 = 0,
    .context_update_tile_id = 1,
    .tile_size_bytes_minus_1 = 3,
  },
  .quantization_params = {
    .base_q_idx = 61,
    .delta_q_y_dc = -2,
    .delta_q_u_dc = -1,
    .delta_q_u_ac = 2,
    .using_qmatrix = 0,

    .delta_q_present = 0,
  },
  .segmentation_params = {
    .segmentation_enabled = 0,
  },
  .loop_filter_params = {
    .loop_filter_level = { 0, 0, },
    .loop_filter_sharpness = 0,
    .loop_filter_delta_enabled = 0,
  },
  .cdef_params = {
    .cdef_damping = 3,
    .cdef_bits = 3,
    .cdef_y_pri_strength = { 0, 3, 0, 7, 1, 2, 0, 1, },
    .cdef_y_sec_strength = { 3, 1, 1, 1, 2, 0, 0, 1, },
    .cdef_uv_pri_strength = { 2, 7, 0, 0, 0, 7, 7, 3, },
    .cdef_uv_sec_strength = { 0, 0, 2, 3, 1, 0, 0, 0, },
  },
  .loop_restoration_params = {
    .uses_lr = 1,
    .frame_restoration_type[0] = GST_AV1_FRAME_RESTORE_WIENER,
    .frame_restoration_type[1] = GST_AV1_FRAME_RESTORE_SWITCHABLE,
    .frame_restoration_type[2] = GST_AV1_FRAME_RESTORE_WIENER,
    .lr_unit_shift = 1,
    .lr_uv_shift = 0,
  },
  .tx_mode = GST_AV1_TX_MODE_SELECT,
  .reference_select = 0,
  .allow_warped_motion = 1,
  .reduced_tx_set = 0,
  .global_motion_params = {
    .gm_type = {
      GST_AV1_WARP_MODEL_IDENTITY,
      GST_AV1_WARP_MODEL_IDENTITY,
      GST_AV1_WARP_MODEL_IDENTITY,
      GST_AV1_WARP_MODEL_IDENTITY,
      GST_AV1_WARP_MODEL_IDENTITY,
      GST_AV1_WARP_MODEL_IDENTITY,
      GST_AV1_WARP_MODEL_IDENTITY,
      GST_AV1_WARP_MODEL_IDENTITY
    },
  },
};

static const GstAV1FrameHeaderOBU show_existing_frame = {
  .show_existing_frame = 1,
  .frame_to_show_map_idx = 3,
};

static const GstAV1MetadataOBU hdr_mdcv = {
  .metadata_type = GST_AV1_METADATA_TYPE_HDR_MDCV,
  .hdr_mdcv = {
    .primary_chromaticity_x = { 6554, 19661, 32768 },
    .primary_chromaticity_y = { 13107, 26214, 39322 },
    .white_point_chromaticity_x = 45875,
    .white_point_chromaticity_y = 52429,
    .luminance_max = 512,
    .luminance_min = 16384,
  }
};

static const GstAV1MetadataOBU hdr_cll = {
  .metadata_type = GST_AV1_METADATA_TYPE_HDR_CLL,
  .hdr_cll = {
    .max_cll = 11122,
    .max_fall = 22211,
  }
};
/* *INDENT-ON* */

GST_START_TEST (test_av1_bitwriter_sequence_and_frame_hdr)
{
  GstAV1ParserResult res;
  gboolean ret;
  guint size;
  guint i;
  GstAV1Parser *const parser = gst_av1_parser_new ();
  GstAV1OBU obu;
  guint32 consumed;
  GstAV1SequenceHeaderOBU seq_header = { 0, };
  GstAV1FrameHeaderOBU frame_header = { 0, };
  guint8 sequence_obu[128] = { 0, };
  guint8 frame_header_obu[256] = { 0, };
  guint8 td_obu[16] = { 0, };

  size = sizeof (sequence_obu);
  ret = gst_av1_bit_writer_sequence_header_obu (&sequence, TRUE,
      sequence_obu, &size);
  fail_if (ret != GST_AV1_BIT_WRITER_OK);

  /* Parse it again */
  res = gst_av1_parser_identify_one_obu (parser,
      sequence_obu, size, &obu, &consumed);
  assert_equals_int (res, GST_AV1_PARSER_OK);
  assert_equals_int (obu.obu_type, GST_AV1_OBU_SEQUENCE_HEADER);

  res = gst_av1_parser_parse_sequence_header_obu (parser, &obu, &seq_header);
  assert_equals_int (res, GST_AV1_PARSER_OK);

  /* We can not do simply memcmp, the parser may set some default
     value for the fields which are not used for writing. */
#define CHECK_FIELD(FIELD)  fail_if(seq_header.FIELD != sequence.FIELD)
  CHECK_FIELD (seq_profile);
  CHECK_FIELD (still_picture);
  CHECK_FIELD (reduced_still_picture_header);
  CHECK_FIELD (frame_width_bits_minus_1);
  CHECK_FIELD (frame_height_bits_minus_1);
  CHECK_FIELD (max_frame_width_minus_1);
  CHECK_FIELD (max_frame_height_minus_1);
  CHECK_FIELD (frame_id_numbers_present_flag);
  CHECK_FIELD (delta_frame_id_length_minus_2);
  CHECK_FIELD (additional_frame_id_length_minus_1);
  CHECK_FIELD (use_128x128_superblock);
  CHECK_FIELD (enable_filter_intra);
  CHECK_FIELD (enable_intra_edge_filter);
  CHECK_FIELD (enable_interintra_compound);
  CHECK_FIELD (enable_masked_compound);
  CHECK_FIELD (enable_warped_motion);
  CHECK_FIELD (enable_order_hint);
  CHECK_FIELD (enable_dual_filter);
  CHECK_FIELD (enable_jnt_comp);
  CHECK_FIELD (enable_ref_frame_mvs);
  CHECK_FIELD (seq_choose_screen_content_tools);
  CHECK_FIELD (seq_force_screen_content_tools);
  CHECK_FIELD (seq_choose_integer_mv);
  CHECK_FIELD (order_hint_bits_minus_1);
  CHECK_FIELD (enable_superres);
  CHECK_FIELD (enable_cdef);
  CHECK_FIELD (enable_restoration);
  CHECK_FIELD (film_grain_params_present);

  CHECK_FIELD (operating_points_cnt_minus_1);
  for (i = 0; i < 4; i++) {
    CHECK_FIELD (operating_points[i].seq_level_idx);
    CHECK_FIELD (operating_points[i].seq_tier);
    CHECK_FIELD (operating_points[i].idc);
  }

  CHECK_FIELD (decoder_model_info_present_flag);
  CHECK_FIELD (initial_display_delay_present_flag);

  CHECK_FIELD (timing_info_present_flag);
  CHECK_FIELD (timing_info.num_units_in_display_tick);
  CHECK_FIELD (timing_info.time_scale);
  CHECK_FIELD (timing_info.equal_picture_interval);
  CHECK_FIELD (timing_info.num_ticks_per_picture_minus_1);
#undef CHECK_FIELD

  size = sizeof (frame_header_obu);
  ret = gst_av1_bit_writer_frame_header_obu (&key_frame, &sequence, 0, 0, TRUE,
      frame_header_obu, &size);
  fail_if (ret != GST_AV1_BIT_WRITER_OK);

  /* Parse it again */
  res = gst_av1_parser_identify_one_obu (parser,
      frame_header_obu, size, &obu, &consumed);
  assert_equals_int (res, GST_AV1_PARSER_OK);
  assert_equals_int (obu.obu_type, GST_AV1_OBU_FRAME_HEADER);

  res = gst_av1_parser_parse_frame_header_obu (parser, &obu, &frame_header);
  assert_equals_int (res, GST_AV1_PARSER_OK);

#define CHECK_FIELD(FIELD)  fail_if(frame_header.FIELD != key_frame.FIELD)
  CHECK_FIELD (show_existing_frame);
  CHECK_FIELD (frame_type);
  CHECK_FIELD (show_frame);
  CHECK_FIELD (disable_cdf_update);
  CHECK_FIELD (allow_screen_content_tools);
  CHECK_FIELD (frame_size_override_flag);
  CHECK_FIELD (order_hint);
  CHECK_FIELD (frame_width);
  CHECK_FIELD (frame_height);
  CHECK_FIELD (use_superres);
  CHECK_FIELD (render_and_frame_size_different);
  CHECK_FIELD (disable_frame_end_update_cdf);

  CHECK_FIELD (tile_info.uniform_tile_spacing_flag);
  CHECK_FIELD (tile_info.tile_cols_log2);
  CHECK_FIELD (tile_info.tile_rows_log2);
  CHECK_FIELD (tile_info.context_update_tile_id);
  CHECK_FIELD (tile_info.tile_size_bytes_minus_1);

  CHECK_FIELD (quantization_params.base_q_idx);
  CHECK_FIELD (quantization_params.delta_q_y_dc);
  CHECK_FIELD (quantization_params.delta_q_u_dc);
  CHECK_FIELD (quantization_params.delta_q_u_ac);
  CHECK_FIELD (quantization_params.using_qmatrix);
  CHECK_FIELD (quantization_params.delta_q_present);

  CHECK_FIELD (segmentation_params.segmentation_enabled);

  for (i = 0; i < 2; i++)
    CHECK_FIELD (loop_filter_params.loop_filter_level[i]);
  CHECK_FIELD (loop_filter_params.loop_filter_sharpness);
  CHECK_FIELD (loop_filter_params.loop_filter_delta_enabled);
  CHECK_FIELD (loop_filter_params.loop_filter_delta_update);
  for (i = 0; i < 8; i++)
    CHECK_FIELD (loop_filter_params.loop_filter_ref_deltas[i]);

  CHECK_FIELD (cdef_params.cdef_damping);
  CHECK_FIELD (cdef_params.cdef_bits);
  for (i = 0; i < 8; i++)
    CHECK_FIELD (cdef_params.cdef_y_pri_strength[i]);
  for (i = 0; i < 8; i++) {
    if (frame_header.cdef_params.cdef_y_sec_strength[i] == 4)
      frame_header.cdef_params.cdef_y_sec_strength[i]--;
    CHECK_FIELD (cdef_params.cdef_y_sec_strength[i]);
  }
  for (i = 0; i < 8; i++)
    CHECK_FIELD (cdef_params.cdef_uv_pri_strength[i]);
  for (i = 0; i < 8; i++) {
    if (frame_header.cdef_params.cdef_uv_sec_strength[i] == 4)
      frame_header.cdef_params.cdef_uv_sec_strength[i]--;
    CHECK_FIELD (cdef_params.cdef_uv_sec_strength[i]);
  }

  CHECK_FIELD (loop_restoration_params.uses_lr);
  for (i = 0; i < 3; i++)
    CHECK_FIELD (loop_restoration_params.frame_restoration_type[i]);
  CHECK_FIELD (loop_restoration_params.lr_unit_shift);
  CHECK_FIELD (loop_restoration_params.lr_uv_shift);

  CHECK_FIELD (tx_mode);
  CHECK_FIELD (reduced_tx_set);
#undef CHECK_FIELD

  /* Append a TD */
  size = sizeof (td_obu);
  ret = gst_av1_bit_writer_temporal_delimiter_obu (TRUE, td_obu, &size);
  fail_if (ret != GST_AV1_BIT_WRITER_OK);

  /* Parse it again */
  res = gst_av1_parser_identify_one_obu (parser, td_obu, size, &obu, &consumed);
  assert_equals_int (res, GST_AV1_PARSER_OK);
  assert_equals_int (obu.obu_type, GST_AV1_OBU_TEMPORAL_DELIMITER);

  res = gst_av1_parser_parse_temporal_delimiter_obu (parser, &obu);
  assert_equals_int (res, GST_AV1_PARSER_OK);

  ret = gst_av1_parser_reference_frame_update (parser, &frame_header);
  assert_equals_int (res, GST_AV1_PARSER_OK);

  /* Inter frame */
  size = sizeof (frame_header_obu);
  memset (frame_header_obu, 0, size);

  ret = gst_av1_bit_writer_frame_header_obu (&inter_frame, &sequence, 1, 0,
      TRUE, frame_header_obu, &size);
  fail_if (ret != GST_AV1_BIT_WRITER_OK);

  /* Parse it again */
  res = gst_av1_parser_identify_one_obu (parser,
      frame_header_obu, size, &obu, &consumed);
  assert_equals_int (res, GST_AV1_PARSER_OK);
  assert_equals_int (obu.obu_type, GST_AV1_OBU_FRAME_HEADER);

  res = gst_av1_parser_parse_frame_header_obu (parser, &obu, &frame_header);
  assert_equals_int (res, GST_AV1_PARSER_OK);

#define CHECK_FIELD(FIELD)  fail_if(frame_header.FIELD != inter_frame.FIELD)
  CHECK_FIELD (show_existing_frame);
  CHECK_FIELD (frame_type);
  CHECK_FIELD (show_frame);
  CHECK_FIELD (error_resilient_mode);
  CHECK_FIELD (disable_cdf_update);
  CHECK_FIELD (allow_screen_content_tools);
  CHECK_FIELD (frame_size_override_flag);
  CHECK_FIELD (order_hint);
  CHECK_FIELD (primary_ref_frame);
  CHECK_FIELD (refresh_frame_flags);
  CHECK_FIELD (frame_refs_short_signaling);
  for (i = 0; i < 7; i++)
    CHECK_FIELD (ref_frame_idx[i]);
  CHECK_FIELD (render_and_frame_size_different);
  CHECK_FIELD (allow_high_precision_mv);
  CHECK_FIELD (is_filter_switchable);
  CHECK_FIELD (is_motion_mode_switchable);
  CHECK_FIELD (use_ref_frame_mvs);
  CHECK_FIELD (disable_frame_end_update_cdf);

  CHECK_FIELD (tile_info.uniform_tile_spacing_flag);
  CHECK_FIELD (tile_info.tile_cols_log2);
  CHECK_FIELD (tile_info.tile_rows_log2);
  CHECK_FIELD (tile_info.context_update_tile_id);
  CHECK_FIELD (tile_info.tile_size_bytes_minus_1);

  CHECK_FIELD (quantization_params.base_q_idx);
  CHECK_FIELD (quantization_params.delta_q_y_dc);
  CHECK_FIELD (quantization_params.delta_q_u_dc);
  CHECK_FIELD (quantization_params.delta_q_u_ac);
  CHECK_FIELD (quantization_params.using_qmatrix);
  CHECK_FIELD (quantization_params.delta_q_present);

  CHECK_FIELD (segmentation_params.segmentation_enabled);

  for (i = 0; i < 2; i++)
    CHECK_FIELD (loop_filter_params.loop_filter_level[i]);
  CHECK_FIELD (loop_filter_params.loop_filter_sharpness);
  CHECK_FIELD (loop_filter_params.loop_filter_delta_enabled);

  CHECK_FIELD (cdef_params.cdef_damping);
  CHECK_FIELD (cdef_params.cdef_bits);
  for (i = 0; i < 8; i++)
    CHECK_FIELD (cdef_params.cdef_y_pri_strength[i]);
  for (i = 0; i < 8; i++) {
    if (frame_header.cdef_params.cdef_y_sec_strength[i] == 4)
      frame_header.cdef_params.cdef_y_sec_strength[i]--;
    CHECK_FIELD (cdef_params.cdef_y_sec_strength[i]);
  }
  for (i = 0; i < 8; i++)
    CHECK_FIELD (cdef_params.cdef_uv_pri_strength[i]);
  for (i = 0; i < 8; i++) {
    if (frame_header.cdef_params.cdef_uv_sec_strength[i] == 4)
      frame_header.cdef_params.cdef_uv_sec_strength[i]--;
    CHECK_FIELD (cdef_params.cdef_uv_sec_strength[i]);
  }

  CHECK_FIELD (loop_restoration_params.uses_lr);
  for (i = 0; i < 3; i++)
    CHECK_FIELD (loop_restoration_params.frame_restoration_type[i]);
  CHECK_FIELD (loop_restoration_params.lr_unit_shift);
  CHECK_FIELD (loop_restoration_params.lr_uv_shift);

  CHECK_FIELD (tx_mode);
  CHECK_FIELD (reference_select);
  CHECK_FIELD (allow_warped_motion);
  CHECK_FIELD (reduced_tx_set);

  for (i = 0; i < 8; i++)
    CHECK_FIELD (global_motion_params.gm_type[i]);
#undef CHECK_FIELD

  ret = gst_av1_parser_reference_frame_update (parser, &frame_header);
  assert_equals_int (res, GST_AV1_PARSER_OK);

  /* Append a TD */
  size = sizeof (td_obu);
  ret = gst_av1_bit_writer_temporal_delimiter_obu (TRUE, td_obu, &size);
  fail_if (ret != GST_AV1_BIT_WRITER_OK);

  res = gst_av1_parser_identify_one_obu (parser, td_obu, size, &obu, &consumed);
  assert_equals_int (res, GST_AV1_PARSER_OK);
  assert_equals_int (obu.obu_type, GST_AV1_OBU_TEMPORAL_DELIMITER);

  res = gst_av1_parser_parse_temporal_delimiter_obu (parser, &obu);
  assert_equals_int (res, GST_AV1_PARSER_OK);

  /* Show existing frame */
  size = sizeof (frame_header_obu);
  memset (frame_header_obu, 0, size);

  ret = gst_av1_bit_writer_frame_header_obu (&show_existing_frame, &sequence, 1,
      0, TRUE, frame_header_obu, &size);
  fail_if (ret != GST_AV1_BIT_WRITER_OK);

  /* Parse it again */
  res = gst_av1_parser_identify_one_obu (parser,
      frame_header_obu, size, &obu, &consumed);
  assert_equals_int (res, GST_AV1_PARSER_OK);
  assert_equals_int (obu.obu_type, GST_AV1_OBU_FRAME_HEADER);

  res = gst_av1_parser_parse_frame_header_obu (parser, &obu, &frame_header);
  assert_equals_int (res, GST_AV1_PARSER_OK);

  fail_if (frame_header.show_existing_frame !=
      show_existing_frame.show_existing_frame);
  fail_if (frame_header.frame_to_show_map_idx !=
      show_existing_frame.frame_to_show_map_idx);

  gst_av1_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_av1_bitwriter_metadata)
{
  GstAV1ParserResult res;
  gboolean ret;
  guint size;
  guint i;
  GstAV1Parser *const parser = gst_av1_parser_new ();
  GstAV1OBU obu;
  guint32 consumed;
  GstAV1MetadataOBU metadata = { 0, };
  guint8 meta_obu[128] = { 0, };

  size = sizeof (meta_obu);

  ret = gst_av1_bit_writer_metadata_obu (&hdr_mdcv, 0, 0,
      TRUE, meta_obu, &size);
  fail_if (ret != GST_AV1_BIT_WRITER_OK);

  /* Parse it again */
  res = gst_av1_parser_identify_one_obu (parser,
      meta_obu, size, &obu, &consumed);
  assert_equals_int (res, GST_AV1_PARSER_OK);
  assert_equals_int (obu.obu_type, GST_AV1_OBU_METADATA);

  res = gst_av1_parser_parse_metadata_obu (parser, &obu, &metadata);
  assert_equals_int (res, GST_AV1_PARSER_OK);

#define CHECK_FIELD(FIELD)  fail_if(metadata.hdr_mdcv.FIELD != hdr_mdcv.hdr_mdcv.FIELD)
  fail_if (metadata.metadata_type != hdr_mdcv.metadata_type);
  for (i = 0; i < 3; i++) {
    CHECK_FIELD (primary_chromaticity_x[i]);
    CHECK_FIELD (primary_chromaticity_y[i]);
  }
  CHECK_FIELD (white_point_chromaticity_x);
  CHECK_FIELD (white_point_chromaticity_y);
  CHECK_FIELD (luminance_max);
  CHECK_FIELD (luminance_min);
#undef CHECK_FIELD

  size = sizeof (meta_obu);
  memset (meta_obu, 0, size);

  ret = gst_av1_bit_writer_metadata_obu (&hdr_cll, 0, 0, TRUE, meta_obu, &size);
  fail_if (ret != GST_AV1_BIT_WRITER_OK);

  /* Parse it again */
  res = gst_av1_parser_identify_one_obu (parser,
      meta_obu, size, &obu, &consumed);
  assert_equals_int (res, GST_AV1_PARSER_OK);
  assert_equals_int (obu.obu_type, GST_AV1_OBU_METADATA);

  res = gst_av1_parser_parse_metadata_obu (parser, &obu, &metadata);
  assert_equals_int (res, GST_AV1_PARSER_OK);

  fail_if (metadata.metadata_type != hdr_cll.metadata_type);
  fail_if (metadata.hdr_cll.max_cll != hdr_cll.hdr_cll.max_cll);
  fail_if (metadata.hdr_cll.max_fall != hdr_cll.hdr_cll.max_fall);

  gst_av1_parser_free (parser);
}

GST_END_TEST;

static Suite *
av1bitwriter_suite (void)
{
  Suite *s = suite_create ("av1 bitwriter library");

  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_av1_bitwriter_sequence_and_frame_hdr);

  tcase_add_test (tc_chain, test_av1_bitwriter_metadata);

  return s;
}

GST_CHECK_MAIN (av1bitwriter);
