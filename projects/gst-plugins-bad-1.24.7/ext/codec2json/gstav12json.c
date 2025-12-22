/*
 * gstav12json.c - AV1 parsed bistream to json
 *
 * Copyright (C) 2023 Collabora
 *   Author: Benjamin Gaignard <benjamin.gaignard@collabora.com>
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
 * SECTION:element-av12json
 * @title: av12json
 *
 * Convert AV1 bitstream parameters to JSON formated text.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/av1/file ! parsebin ! av12json ! filesink location=/path/to/json/file
 * ```
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/base/base.h>
#include <json-glib/json-glib.h>

#include "gstav12json.h"

GST_DEBUG_CATEGORY (gst_av1_2_json_debug);
#define GST_CAT_DEFAULT gst_av1_2_json_debug

struct _GstAV12json
{
  GstElement parent;

  GstPad *sinkpad, *srcpad;
  GstAV1Parser *parser;
  gboolean use_annex_b;

  JsonObject *json;
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-av1")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-json,format=av1"));

G_DEFINE_TYPE_WITH_CODE (GstAV12json, gst_av1_2_json,
    GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_av1_2_json_debug, "av12json", 0,
        "AV1 to json"));

static void
gst_av1_2_json_finalize (GObject * object)
{
  GstAV12json *self = GST_AV1_2_JSON (object);

  gst_av1_parser_free (self->parser);
  json_object_unref (self->json);
}

static gchar *
get_string_from_json_object (JsonObject * object)
{
  JsonNode *root;
  JsonGenerator *generator;
  gchar *text;

  /* Make it the root node */
  root = json_node_init_object (json_node_alloc (), object);
  generator = json_generator_new ();
  json_generator_set_indent (generator, 2);
  json_generator_set_indent_char (generator, ' ');
  json_generator_set_pretty (generator, TRUE);
  json_generator_set_root (generator, root);
  text = json_generator_to_data (generator, NULL);

  /* Release everything */
  g_object_unref (generator);
  json_node_free (root);
  return text;
}

static void
gst_av1_2_json_sequence_header (GstAV12json * self,
    GstAV1SequenceHeaderOBU * seq_header)
{
  JsonObject *json = self->json;
  JsonObject *hdr = json_object_new ();
  JsonArray *operating_points;
  JsonObject *color_config;
  guint i;

  json_object_set_int_member (hdr, "seq profile", seq_header->seq_profile);
  json_object_set_boolean_member (hdr, "still picture",
      seq_header->still_picture);
  json_object_set_int_member (hdr, "reduced still picture header",
      seq_header->reduced_still_picture_header);
  json_object_set_int_member (hdr, "frame width bits minus 1",
      seq_header->frame_width_bits_minus_1);
  json_object_set_int_member (hdr, "frame height bits minus 1",
      seq_header->frame_height_bits_minus_1);
  json_object_set_int_member (hdr, "max frame width minus 1",
      seq_header->max_frame_width_minus_1);
  json_object_set_int_member (hdr, "max frame height minus 1",
      seq_header->max_frame_height_minus_1);
  json_object_set_boolean_member (hdr, "frame id numbers present flag",
      seq_header->frame_id_numbers_present_flag);
  json_object_set_int_member (hdr, "delta frame id length minus 2",
      seq_header->delta_frame_id_length_minus_2);
  json_object_set_int_member (hdr, "additional frame id length minus 1",
      seq_header->additional_frame_id_length_minus_1);
  json_object_set_boolean_member (hdr, "use 128x128 superblock",
      seq_header->use_128x128_superblock);
  json_object_set_boolean_member (hdr, "enable filter intra",
      seq_header->enable_filter_intra);
  json_object_set_boolean_member (hdr, "enable intra edge filter",
      seq_header->enable_intra_edge_filter);
  json_object_set_boolean_member (hdr, "enable interintra compound",
      seq_header->enable_interintra_compound);
  json_object_set_boolean_member (hdr, "enable masked compound",
      seq_header->enable_masked_compound);
  json_object_set_boolean_member (hdr, "enable warped motion",
      seq_header->enable_warped_motion);
  json_object_set_boolean_member (hdr, "enable order hint",
      seq_header->enable_order_hint);
  json_object_set_boolean_member (hdr, "enable dual filter",
      seq_header->enable_dual_filter);
  json_object_set_boolean_member (hdr, "enable jnt comp",
      seq_header->enable_jnt_comp);
  json_object_set_boolean_member (hdr, "enable ref frame mvs",
      seq_header->enable_ref_frame_mvs);
  json_object_set_boolean_member (hdr, "seq choose screen content tools",
      seq_header->seq_choose_screen_content_tools);
  json_object_set_int_member (hdr, "seq force screen content tools",
      seq_header->seq_force_screen_content_tools);
  json_object_set_boolean_member (hdr, "seq choose integer mv",
      seq_header->seq_choose_integer_mv);
  json_object_set_int_member (hdr, "seq force integer mv",
      seq_header->seq_force_integer_mv);
  json_object_set_int_member (hdr, "order hint bits minus 1",
      seq_header->order_hint_bits_minus_1);
  json_object_set_boolean_member (hdr, "enable superres",
      seq_header->enable_superres);
  json_object_set_boolean_member (hdr, "enable cdef", seq_header->enable_cdef);
  json_object_set_boolean_member (hdr, "enable restoration",
      seq_header->enable_restoration);
  json_object_set_int_member (hdr, "film grain params present",
      seq_header->film_grain_params_present);
  json_object_set_int_member (hdr, "operating points cnt minus 1",
      seq_header->operating_points_cnt_minus_1);

  operating_points = json_array_new ();
  for (i = 0; i < seq_header->operating_points_cnt_minus_1 + 1; i++) {
    JsonObject *operating_point = json_object_new ();

    json_object_set_int_member (operating_point, "seq level idx",
        seq_header->operating_points[i].seq_level_idx);
    json_object_set_int_member (operating_point, "seq tier",
        seq_header->operating_points[i].seq_tier);
    json_object_set_int_member (operating_point, "idc",
        seq_header->operating_points[i].idc);
    json_object_set_boolean_member (operating_point,
        "decoder model present for this op",
        seq_header->operating_points[i].decoder_model_present_for_this_op);
    json_object_set_int_member (operating_point, "decoder buffer delay",
        seq_header->operating_points[i].decoder_buffer_delay);
    json_object_set_int_member (operating_point, "encoder buffer delay",
        seq_header->operating_points[i].encoder_buffer_delay);
    json_object_set_boolean_member (operating_point, "low delay mode flag",
        seq_header->operating_points[i].low_delay_mode_flag);
    json_object_set_boolean_member (operating_point,
        "initial display delay present for this op",
        seq_header->
        operating_points[i].initial_display_delay_present_for_this_op);
    json_object_set_int_member (operating_point,
        "initial display delay minus 1",
        seq_header->operating_points[i].initial_display_delay_minus_1);
    json_array_add_object_element (operating_points, operating_point);
  }
  json_object_set_array_member (hdr, "operating points", operating_points);

  json_object_set_boolean_member (hdr, "decoder model info present flag",
      seq_header->decoder_model_info_present_flag);
  if (seq_header->decoder_model_info_present_flag) {
    JsonObject *decoder_model_info = json_object_new ();

    json_object_set_int_member (decoder_model_info,
        "buffer delay length minus 1",
        seq_header->decoder_model_info.buffer_delay_length_minus_1);
    json_object_set_int_member (decoder_model_info,
        "num units in decoding tick",
        seq_header->decoder_model_info.num_units_in_decoding_tick);
    json_object_set_int_member (decoder_model_info,
        "buffer removal time length minus 1",
        seq_header->decoder_model_info.buffer_removal_time_length_minus_1);
    json_object_set_int_member (decoder_model_info,
        "frame presentation time length minus 1",
        seq_header->decoder_model_info.frame_presentation_time_length_minus_1);
    json_object_set_object_member (hdr, "decoder model info",
        decoder_model_info);
  }

  json_object_set_int_member (hdr, "initial display delay present flag",
      seq_header->initial_display_delay_present_flag);

  json_object_set_boolean_member (hdr, "timing info present flag",
      seq_header->timing_info_present_flag);
  if (seq_header->timing_info_present_flag) {
    JsonObject *timing_info = json_object_new ();

    json_object_set_int_member (timing_info, "num units in display tick",
        seq_header->timing_info.num_units_in_display_tick);
    json_object_set_int_member (timing_info, "time scale",
        seq_header->timing_info.time_scale);
    json_object_set_boolean_member (timing_info, "equal picture interval",
        seq_header->timing_info.equal_picture_interval);
    json_object_set_int_member (timing_info, "num ticks per picture minus 1",
        seq_header->timing_info.num_ticks_per_picture_minus_1);
    json_object_set_object_member (hdr, "timing info", timing_info);
  }

  color_config = json_object_new ();
  json_object_set_boolean_member (color_config, "high bitdepth",
      seq_header->color_config.high_bitdepth);
  json_object_set_boolean_member (color_config, "twelve bit",
      seq_header->color_config.twelve_bit);
  json_object_set_boolean_member (color_config, "mono chrome",
      seq_header->color_config.mono_chrome);
  json_object_set_boolean_member (color_config,
      "color description present flag",
      seq_header->color_config.color_description_present_flag);
  json_object_set_int_member (color_config, "color primaries",
      seq_header->color_config.color_primaries);
  json_object_set_int_member (color_config, "transfer characteristics",
      seq_header->color_config.transfer_characteristics);
  json_object_set_int_member (color_config, "matrix coefficients",
      seq_header->color_config.matrix_coefficients);
  json_object_set_boolean_member (color_config, "color range",
      seq_header->color_config.color_range);
  json_object_set_int_member (color_config, "subsampling x",
      seq_header->color_config.subsampling_x);
  json_object_set_int_member (color_config, "subsampling y",
      seq_header->color_config.subsampling_y);
  json_object_set_int_member (color_config, "chroma sample position",
      seq_header->color_config.chroma_sample_position);
  json_object_set_boolean_member (color_config, "separate uv delta q",
      seq_header->color_config.separate_uv_delta_q);
  json_object_set_object_member (hdr, "color config", color_config);

  json_object_set_int_member (hdr, "order hint bits",
      seq_header->order_hint_bits);
  json_object_set_int_member (hdr, "bit depth", seq_header->bit_depth);
  json_object_set_int_member (hdr, "num planes", seq_header->num_planes);

  json_object_set_object_member (json, "sequence header", hdr);
}

static void
gst_av1_2_json_frame_header (GstAV12json * self,
    GstAV1FrameHeaderOBU * frame_header)
{
  JsonObject *json = self->json;
  JsonArray *buffer_removal_time, *ref_order_hint, *ref_frame_idx,
      *loop_filter_level, *loop_filter_ref_deltas, *loop_filter_mode_deltas,
      *feature_enabled, *feature_data, *width_in_sbs_minus_1,
      *height_in_sbs_minus_1, *mi_col_starts, *mi_row_starts,
      *cdef_y_pri_strength, *cdef_y_sec_strength, *cdef_uv_pri_strength,
      *cdef_uv_sec_strength, *frame_restoration_type, *loop_restoration_size,
      *is_global, *is_rot_zoom, *is_translation, *gm_params, *gm_type, *invalid,
      *point_y_value, *point_y_scaling, *point_cb_value, *point_cb_scaling,
      *point_cr_value, *point_cr_scaling, *ar_coeffs_y_plus_128,
      *ar_coeffs_cb_plus_128, *ar_coeffs_cr_plus_128, *order_hints,
      *ref_frame_sign_bias, *lossless_array, *seg_qm_level, *skip_mode_frame;
  JsonObject *hdr = json_object_new ();
  JsonObject *loop_filter_params, *quantization_params, *segmentation_params,
      *tile_info, *cdef_params, *loop_restoration_params, *global_motion_params,
      *film_grain_params;
  guint i, j;

  json_object_set_boolean_member (hdr, "show existing frame",
      frame_header->show_existing_frame);
  json_object_set_int_member (hdr, "frame to show map idx",
      frame_header->frame_to_show_map_idx);
  json_object_set_int_member (hdr, "frame presentation time",
      frame_header->frame_presentation_time);
  json_object_set_int_member (hdr, "tu presentation delay",
      frame_header->tu_presentation_delay);
  json_object_set_int_member (hdr, "display frame id",
      frame_header->display_frame_id);
  switch (frame_header->frame_type) {
    case GST_AV1_KEY_FRAME:
      json_object_set_string_member (hdr, "frame type", "key frame");
      break;
    case GST_AV1_INTER_FRAME:
      json_object_set_string_member (hdr, "frame type", "inter frame");
      break;
    case GST_AV1_INTRA_ONLY_FRAME:
      json_object_set_string_member (hdr, "frame type", "intra only frame");
      break;
    case GST_AV1_SWITCH_FRAME:
      json_object_set_string_member (hdr, "frame type", "switch frame");
      break;
  }
  json_object_set_boolean_member (hdr, "show frame", frame_header->show_frame);
  json_object_set_boolean_member (hdr, "showable frame",
      frame_header->showable_frame);
  json_object_set_boolean_member (hdr, "error resilient mode",
      frame_header->error_resilient_mode);
  json_object_set_boolean_member (hdr, "disable cdf update",
      frame_header->disable_cdf_update);
  json_object_set_int_member (hdr, "allow screen content tools",
      frame_header->allow_screen_content_tools);
  json_object_set_boolean_member (hdr, "force integer_mv",
      frame_header->force_integer_mv);
  json_object_set_int_member (hdr, "current frame id",
      frame_header->current_frame_id);
  json_object_set_boolean_member (hdr, "frame size override flag",
      frame_header->frame_size_override_flag);
  json_object_set_int_member (hdr, "order hint", frame_header->order_hint);
  json_object_set_int_member (hdr, "primary ref_frame",
      frame_header->primary_ref_frame);
  json_object_set_boolean_member (hdr, "buffer removal time present flag",
      frame_header->buffer_removal_time_present_flag);

  buffer_removal_time = json_array_new ();
  for (i = 0; i < GST_AV1_MAX_OPERATING_POINTS; i++)
    json_array_add_int_element (buffer_removal_time,
        frame_header->buffer_removal_time[i]);
  json_object_set_array_member (hdr, "buffer removal time",
      buffer_removal_time);

  json_object_set_int_member (hdr, "refresh frame flags",
      frame_header->refresh_frame_flags);

  ref_order_hint = json_array_new ();
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++)
    json_array_add_int_element (ref_order_hint,
        frame_header->ref_order_hint[i]);
  json_object_set_array_member (hdr, "ref order hint", ref_order_hint);

  json_object_set_boolean_member (hdr, "allow intrabc",
      frame_header->allow_intrabc);
  json_object_set_boolean_member (hdr, "frame refs short signaling",
      frame_header->frame_refs_short_signaling);
  json_object_set_int_member (hdr, "last frame idx",
      frame_header->last_frame_idx);
  json_object_set_int_member (hdr, "gold frame idx",
      frame_header->gold_frame_idx);

  ref_frame_idx = json_array_new ();
  for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++)
    json_array_add_int_element (ref_frame_idx, frame_header->ref_frame_idx[i]);
  json_object_set_array_member (hdr, "ref frame idx", ref_frame_idx);

  json_object_set_boolean_member (hdr, "allow high precision mv",
      frame_header->allow_high_precision_mv);
  json_object_set_boolean_member (hdr, "is motion mode switchable",
      frame_header->is_motion_mode_switchable);
  json_object_set_boolean_member (hdr, "use ref frame mvs",
      frame_header->use_ref_frame_mvs);
  json_object_set_boolean_member (hdr, "disable frame end update cdf",
      frame_header->disable_frame_end_update_cdf);
  json_object_set_boolean_member (hdr, "allow warped motion",
      frame_header->allow_warped_motion);
  json_object_set_boolean_member (hdr, "reduced tx set",
      frame_header->reduced_tx_set);
  json_object_set_boolean_member (hdr, "render and frame size different",
      frame_header->render_and_frame_size_different);
  json_object_set_boolean_member (hdr, "use superres",
      frame_header->use_superres);
  json_object_set_boolean_member (hdr, "is filter switchable",
      frame_header->is_filter_switchable);
  json_object_set_int_member (hdr, "interpolation filter",
      frame_header->interpolation_filter);

  loop_filter_params = json_object_new ();
  loop_filter_level = json_array_new ();
  for (i = 0; i < 4; i++)
    json_array_add_int_element (loop_filter_level,
        frame_header->loop_filter_params.loop_filter_level[i]);
  json_object_set_array_member (loop_filter_params, "loop filter level",
      loop_filter_level);
  json_object_set_int_member (loop_filter_params, "loop filter sharpness",
      frame_header->loop_filter_params.loop_filter_sharpness);
  json_object_set_boolean_member (loop_filter_params,
      "loop filter delta enabled",
      frame_header->loop_filter_params.loop_filter_delta_enabled);
  json_object_set_boolean_member (loop_filter_params,
      "loop filter delta update",
      frame_header->loop_filter_params.loop_filter_delta_update);

  loop_filter_ref_deltas = json_array_new ();
  for (i = 0; i < GST_AV1_TOTAL_REFS_PER_FRAME; i++)
    json_array_add_int_element (loop_filter_ref_deltas,
        frame_header->loop_filter_params.loop_filter_ref_deltas[i]);
  json_object_set_array_member (loop_filter_params, "loop filter ref deltas",
      loop_filter_ref_deltas);

  loop_filter_mode_deltas = json_array_new ();
  for (i = 0; i < 2; i++)
    json_array_add_int_element (loop_filter_mode_deltas,
        frame_header->loop_filter_params.loop_filter_mode_deltas[i]);
  json_object_set_array_member (loop_filter_params, "loop filter mode deltas",
      loop_filter_mode_deltas);
  json_object_set_boolean_member (loop_filter_params, "delta lf present",
      frame_header->loop_filter_params.delta_lf_present);
  json_object_set_int_member (loop_filter_params, "delta lf res",
      frame_header->loop_filter_params.delta_lf_res);
  json_object_set_int_member (loop_filter_params, "delta lf multi",
      frame_header->loop_filter_params.delta_lf_multi);
  json_object_set_object_member (hdr, "loop filter params", loop_filter_params);

  quantization_params = json_object_new ();
  json_object_set_int_member (quantization_params, "base q idx",
      frame_header->quantization_params.base_q_idx);
  json_object_set_boolean_member (quantization_params, "diff uv delta",
      frame_header->quantization_params.diff_uv_delta);
  json_object_set_boolean_member (quantization_params, "using qmatrix",
      frame_header->quantization_params.using_qmatrix);
  json_object_set_int_member (quantization_params, "qm y",
      frame_header->quantization_params.qm_y);
  json_object_set_int_member (quantization_params, "qm u",
      frame_header->quantization_params.qm_u);
  json_object_set_int_member (quantization_params, "qm v",
      frame_header->quantization_params.qm_v);
  json_object_set_boolean_member (quantization_params, "delta q present",
      frame_header->quantization_params.delta_q_present);
  json_object_set_int_member (quantization_params, "delta q res",
      frame_header->quantization_params.delta_q_res);
  json_object_set_int_member (quantization_params, "delta q y dc",
      frame_header->quantization_params.delta_q_y_dc);
  json_object_set_int_member (quantization_params, "delta q u dc",
      frame_header->quantization_params.delta_q_u_dc);
  json_object_set_int_member (quantization_params, "delta q u ac",
      frame_header->quantization_params.delta_q_u_ac);
  json_object_set_int_member (quantization_params, "delta q v dc",
      frame_header->quantization_params.delta_q_v_dc);
  json_object_set_int_member (quantization_params, "delta q v ac",
      frame_header->quantization_params.delta_q_v_ac);
  json_object_set_object_member (hdr, "quantization params",
      quantization_params);

  segmentation_params = json_object_new ();
  json_object_set_boolean_member (segmentation_params, "segmentation enabled",
      frame_header->segmentation_params.segmentation_enabled);
  json_object_set_int_member (segmentation_params, "segmentation update map",
      frame_header->segmentation_params.segmentation_update_map);
  json_object_set_int_member (segmentation_params,
      "segmentation temporal update",
      frame_header->segmentation_params.segmentation_temporal_update);
  json_object_set_int_member (segmentation_params, "segmentation update data",
      frame_header->segmentation_params.segmentation_update_data);
  feature_enabled = json_array_new ();
  feature_data = json_array_new ();
  for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
    for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
      json_array_add_int_element (feature_enabled,
          frame_header->segmentation_params.feature_enabled[i][j]);
      json_array_add_int_element (feature_data,
          frame_header->segmentation_params.feature_data[i][j]);
    }
  }
  json_object_set_array_member (segmentation_params, "feature enabled",
      feature_enabled);
  json_object_set_array_member (segmentation_params, "feature data",
      feature_data);
  json_object_set_int_member (segmentation_params, "seg id pre skip",
      frame_header->segmentation_params.seg_id_pre_skip);
  json_object_set_int_member (segmentation_params, "last active seg id",
      frame_header->segmentation_params.last_active_seg_id);
  json_object_set_object_member (hdr, "segmentation params",
      segmentation_params);

  tile_info = json_object_new ();
  json_object_set_int_member (tile_info, "uniform tile spacing flag",
      frame_header->tile_info.uniform_tile_spacing_flag);
  json_object_set_int_member (tile_info, "increment tile rows log2",
      frame_header->tile_info.increment_tile_rows_log2);
  width_in_sbs_minus_1 = json_array_new ();
  height_in_sbs_minus_1 = json_array_new ();
  for (i = 0; i < GST_AV1_MAX_TILE_COLS; i++) {
    json_array_add_int_element (width_in_sbs_minus_1,
        frame_header->tile_info.width_in_sbs_minus_1[i]);
    json_array_add_int_element (height_in_sbs_minus_1,
        frame_header->tile_info.height_in_sbs_minus_1[i]);
  }
  json_object_set_array_member (tile_info, "width in sbs minus 1",
      width_in_sbs_minus_1);
  json_object_set_array_member (tile_info, "height in sbs minus 1",
      height_in_sbs_minus_1);
  json_object_set_int_member (tile_info, "tile size bytes minus 1",
      frame_header->tile_info.tile_size_bytes_minus_1);
  json_object_set_int_member (tile_info, "context update tile id",
      frame_header->tile_info.context_update_tile_id);
  mi_col_starts = json_array_new ();
  mi_row_starts = json_array_new ();
  for (i = 0; i < GST_AV1_MAX_TILE_COLS + 1; i++) {
    json_array_add_int_element (mi_col_starts,
        frame_header->tile_info.mi_col_starts[i]);
    json_array_add_int_element (mi_row_starts,
        frame_header->tile_info.mi_row_starts[i]);
  }
  json_object_set_array_member (tile_info, "mi col starts", mi_col_starts);
  json_object_set_array_member (tile_info, "mi row starts", mi_row_starts);
  json_object_set_int_member (tile_info, "tile cols log2",
      frame_header->tile_info.tile_cols_log2);
  json_object_set_int_member (tile_info, "tile cols",
      frame_header->tile_info.tile_cols);
  json_object_set_int_member (tile_info, "tile rows log2",
      frame_header->tile_info.tile_rows_log2);
  json_object_set_int_member (tile_info, "tile rows",
      frame_header->tile_info.tile_rows);
  json_object_set_int_member (tile_info, "tile size bytes",
      frame_header->tile_info.tile_size_bytes);
  json_object_set_object_member (hdr, "tile_info", tile_info);

  cdef_params = json_object_new ();
  json_object_set_int_member (cdef_params, "cdef damping",
      frame_header->cdef_params.cdef_damping);
  json_object_set_int_member (cdef_params, "cdef bits",
      frame_header->cdef_params.cdef_bits);
  cdef_y_pri_strength = json_array_new ();
  cdef_y_sec_strength = json_array_new ();
  cdef_uv_pri_strength = json_array_new ();
  cdef_uv_sec_strength = json_array_new ();
  for (i = 0; i < GST_AV1_CDEF_MAX; i++) {
    json_array_add_int_element (cdef_y_pri_strength,
        frame_header->cdef_params.cdef_y_pri_strength[i]);
    json_array_add_int_element (cdef_y_sec_strength,
        frame_header->cdef_params.cdef_y_sec_strength[i]);
    json_array_add_int_element (cdef_uv_pri_strength,
        frame_header->cdef_params.cdef_uv_pri_strength[i]);
    json_array_add_int_element (cdef_uv_sec_strength,
        frame_header->cdef_params.cdef_uv_sec_strength[i]);
  }
  json_object_set_array_member (cdef_params, "cdef y pri strength",
      cdef_y_pri_strength);
  json_object_set_array_member (cdef_params, "cdef y sec strength",
      cdef_y_sec_strength);
  json_object_set_array_member (cdef_params, "cdef uv pri_strength",
      cdef_uv_pri_strength);
  json_object_set_array_member (cdef_params, "cdef uv sec_strength",
      cdef_uv_sec_strength);
  json_object_set_object_member (hdr, "cdef params", cdef_params);

  loop_restoration_params = json_object_new ();
  json_object_set_int_member (loop_restoration_params, "lr unit shift",
      frame_header->loop_restoration_params.lr_unit_shift);
  json_object_set_int_member (loop_restoration_params, "lr uv shift",
      frame_header->loop_restoration_params.lr_uv_shift);
  frame_restoration_type = json_array_new ();
  loop_restoration_size = json_array_new ();
  for (i = 0; i < GST_AV1_MAX_NUM_PLANES; i++) {
    json_array_add_int_element (frame_restoration_type,
        frame_header->loop_restoration_params.frame_restoration_type[i]);
    json_array_add_int_element (loop_restoration_size,
        frame_header->loop_restoration_params.loop_restoration_size[i]);
  }
  json_object_set_array_member (loop_restoration_params,
      "frame restoration type", frame_restoration_type);
  json_object_set_array_member (loop_restoration_params,
      "loop restoration size", loop_restoration_size);
  json_object_set_int_member (loop_restoration_params, "uses lr",
      frame_header->loop_restoration_params.uses_lr);
  json_object_set_object_member (hdr, "loop restoration params",
      loop_restoration_params);

  json_object_set_boolean_member (hdr, "tx mode select",
      frame_header->tx_mode_select);
  json_object_set_boolean_member (hdr, "skip mode present",
      frame_header->skip_mode_present);
  json_object_set_boolean_member (hdr, "reference select",
      frame_header->reference_select);

  global_motion_params = json_object_new ();
  is_global = json_array_new ();
  is_rot_zoom = json_array_new ();
  is_translation = json_array_new ();
  gm_params = json_array_new ();
  gm_type = json_array_new ();
  invalid = json_array_new ();
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    json_array_add_boolean_element (is_global,
        frame_header->global_motion_params.is_global[i]);
    json_array_add_boolean_element (is_rot_zoom,
        frame_header->global_motion_params.is_rot_zoom[i]);
    json_array_add_boolean_element (is_translation,
        frame_header->global_motion_params.is_translation[i]);
    for (j = 0; j < 6; j++)
      json_array_add_int_element (gm_params,
          frame_header->global_motion_params.gm_params[i][j]);
    json_array_add_int_element (gm_type,
        frame_header->global_motion_params.gm_type[i]);
    json_array_add_boolean_element (invalid,
        frame_header->global_motion_params.invalid[i]);
  }
  json_object_set_array_member (global_motion_params, "is global", is_global);
  json_object_set_array_member (global_motion_params, "is rot zoom",
      is_rot_zoom);
  json_object_set_array_member (global_motion_params, "is translation",
      is_translation);
  json_object_set_array_member (global_motion_params, "gm params", gm_params);
  json_object_set_array_member (global_motion_params, "gm type", gm_type);
  json_object_set_array_member (global_motion_params, "invalid", invalid);
  json_object_set_object_member (hdr, "global motion params",
      global_motion_params);

  film_grain_params = json_object_new ();
  json_object_set_boolean_member (film_grain_params, "apply grain",
      frame_header->film_grain_params.apply_grain);
  json_object_set_int_member (film_grain_params, "grain seed",
      frame_header->film_grain_params.grain_seed);
  json_object_set_boolean_member (film_grain_params, "update grain",
      frame_header->film_grain_params.update_grain);
  json_object_set_int_member (film_grain_params, "film grain params ref idx",
      frame_header->film_grain_params.film_grain_params_ref_idx);
  json_object_set_int_member (film_grain_params, "num y points",
      frame_header->film_grain_params.num_y_points);
  point_y_value = json_array_new ();
  point_y_scaling = json_array_new ();
  for (i = 0; i < GST_AV1_MAX_NUM_Y_POINTS; i++) {
    json_array_add_int_element (point_y_value,
        frame_header->film_grain_params.point_y_value[i]);
    json_array_add_int_element (point_y_scaling,
        frame_header->film_grain_params.point_y_scaling[i]);
  }
  json_object_set_array_member (film_grain_params, "point y value",
      point_y_value);
  json_object_set_array_member (film_grain_params, "point y scaling",
      point_y_scaling);
  json_object_set_int_member (film_grain_params, "chroma scaling from luma",
      frame_header->film_grain_params.chroma_scaling_from_luma);
  json_object_set_int_member (film_grain_params, "num cb points",
      frame_header->film_grain_params.num_cb_points);
  point_cb_value = json_array_new ();
  point_cb_scaling = json_array_new ();
  for (i = 0; i < GST_AV1_MAX_NUM_CB_POINTS; i++) {
    json_array_add_int_element (point_cb_value,
        frame_header->film_grain_params.point_cb_value[i]);
    json_array_add_int_element (point_cb_scaling,
        frame_header->film_grain_params.point_cb_scaling[i]);
  }
  json_object_set_array_member (film_grain_params, "point cb value",
      point_cb_value);
  json_object_set_array_member (film_grain_params, "point cb scaling",
      point_cb_scaling);
  json_object_set_int_member (film_grain_params, "num cr points",
      frame_header->film_grain_params.num_cr_points);
  point_cr_value = json_array_new ();
  point_cr_scaling = json_array_new ();
  for (i = 0; i < GST_AV1_MAX_NUM_CR_POINTS; i++) {
    json_array_add_int_element (point_cr_value,
        frame_header->film_grain_params.point_cr_value[i]);
    json_array_add_int_element (point_cr_scaling,
        frame_header->film_grain_params.point_cr_scaling[i]);
  }
  json_object_set_array_member (film_grain_params, "point cr value",
      point_cr_value);
  json_object_set_array_member (film_grain_params, "point cr scaling",
      point_cr_scaling);
  json_object_set_int_member (film_grain_params, "grain scaling minus 8",
      frame_header->film_grain_params.grain_scaling_minus_8);
  json_object_set_int_member (film_grain_params, "ar coeff lag",
      frame_header->film_grain_params.ar_coeff_lag);
  ar_coeffs_y_plus_128 = json_array_new ();
  ar_coeffs_cb_plus_128 = json_array_new ();
  ar_coeffs_cr_plus_128 = json_array_new ();
  for (i = 0; i < GST_AV1_MAX_NUM_POS_LUMA; i++) {
    json_array_add_int_element (ar_coeffs_y_plus_128,
        frame_header->film_grain_params.ar_coeffs_y_plus_128[i]);
    json_array_add_int_element (ar_coeffs_cb_plus_128,
        frame_header->film_grain_params.ar_coeffs_cb_plus_128[i]);
    json_array_add_int_element (ar_coeffs_cr_plus_128,
        frame_header->film_grain_params.ar_coeffs_cr_plus_128[i]);
  }
  json_object_set_array_member (film_grain_params, "ar coeffs y plus 128",
      ar_coeffs_y_plus_128);
  json_object_set_array_member (film_grain_params, "ar coeffs cb plus 128",
      ar_coeffs_cb_plus_128);
  json_object_set_array_member (film_grain_params, "ar coeffs cr plus 128",
      ar_coeffs_cr_plus_128);
  json_object_set_int_member (film_grain_params, "ar coeff shift minus 6",
      frame_header->film_grain_params.ar_coeff_shift_minus_6);
  json_object_set_int_member (film_grain_params, "grain scale shift",
      frame_header->film_grain_params.grain_scale_shift);
  json_object_set_int_member (film_grain_params, "cb mult",
      frame_header->film_grain_params.cb_mult);
  json_object_set_int_member (film_grain_params, "cb luma mult",
      frame_header->film_grain_params.cb_luma_mult);
  json_object_set_int_member (film_grain_params, "cb offset",
      frame_header->film_grain_params.cb_offset);
  json_object_set_int_member (film_grain_params, "cr mult",
      frame_header->film_grain_params.cr_mult);
  json_object_set_int_member (film_grain_params, "cr luma mult",
      frame_header->film_grain_params.cr_luma_mult);
  json_object_set_int_member (film_grain_params, "cr offset",
      frame_header->film_grain_params.cr_offset);
  json_object_set_boolean_member (film_grain_params, "overlap flag",
      frame_header->film_grain_params.overlap_flag);
  json_object_set_boolean_member (film_grain_params, "clip to restricted range",
      frame_header->film_grain_params.clip_to_restricted_range);
  json_object_set_object_member (hdr, "film grain params", film_grain_params);

  json_object_set_int_member (hdr, "superres denom",
      frame_header->superres_denom);
  json_object_set_int_member (hdr, "frame is intra",
      frame_header->frame_is_intra);

  order_hints = json_array_new ();
  ref_frame_sign_bias = json_array_new ();
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    json_array_add_int_element (order_hints, frame_header->order_hints[i]);
    json_array_add_int_element (ref_frame_sign_bias,
        frame_header->ref_frame_sign_bias[i]);
  }
  json_object_set_array_member (hdr, "order hints", order_hints);
  json_object_set_array_member (hdr, "ref frame sign bias",
      ref_frame_sign_bias);

  json_object_set_int_member (hdr, "coded lossless",
      frame_header->coded_lossless);
  json_object_set_int_member (hdr, "all lossless", frame_header->all_lossless);

  lossless_array = json_array_new ();
  for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++)
    json_array_add_int_element (lossless_array,
        frame_header->lossless_array[i]);
  json_object_set_array_member (hdr, "lossless array", lossless_array);

  seg_qm_level = json_array_new ();
  for (i = 0; i < 3; i++)
    for (j = 0; j < GST_AV1_MAX_SEGMENTS; j++)
      json_array_add_int_element (seg_qm_level,
          frame_header->seg_qm_Level[i][j]);
  json_object_set_array_member (hdr, "seg qm level", seg_qm_level);

  json_object_set_int_member (hdr, "upscaled width",
      frame_header->upscaled_width);
  json_object_set_int_member (hdr, "frame width", frame_header->frame_width);
  json_object_set_int_member (hdr, "frame height", frame_header->frame_height);
  json_object_set_int_member (hdr, "render width", frame_header->render_width);
  json_object_set_int_member (hdr, "render height",
      frame_header->render_height);
  json_object_set_int_member (hdr, "tx mode", frame_header->tx_mode);

  skip_mode_frame = json_array_new ();
  json_array_add_int_element (skip_mode_frame,
      frame_header->skip_mode_frame[0]);
  json_array_add_int_element (skip_mode_frame,
      frame_header->skip_mode_frame[1]);
  json_object_set_array_member (hdr, "skip mode frame", skip_mode_frame);

  json_object_set_object_member (json, "frame header", hdr);
}

static GstAV1ParserResult
gst_av1_2_json_handle_one_obu (GstAV12json * self, GstAV1OBU * obu)
{
  GstAV1ParserResult pres = GST_AV1_PARSER_OK;
  GstAV1FrameHeaderOBU frame_header;
  GstAV1FrameOBU frame;

  switch (obu->obu_type) {
    case GST_AV1_OBU_TEMPORAL_DELIMITER:
      pres = gst_av1_parser_parse_temporal_delimiter_obu (self->parser, obu);
      break;

    case GST_AV1_OBU_SEQUENCE_HEADER:
    {
      GstAV1SequenceHeaderOBU seq_header;

      pres = gst_av1_parser_parse_sequence_header_obu (self->parser,
          obu, &seq_header);

      if (pres == GST_AV1_PARSER_OK)
        gst_av1_2_json_sequence_header (self, &seq_header);
      break;
    }

    case GST_AV1_OBU_REDUNDANT_FRAME_HEADER:
    case GST_AV1_OBU_FRAME_HEADER:
      pres = gst_av1_parser_parse_frame_header_obu (self->parser, obu,
          &frame_header);
      if (pres == GST_AV1_PARSER_OK)
        gst_av1_2_json_frame_header (self, &frame_header);
      break;

    case GST_AV1_OBU_FRAME:
      pres = gst_av1_parser_parse_frame_obu (self->parser, obu, &frame);

      if (pres == GST_AV1_PARSER_OK)
        gst_av1_2_json_frame_header (self, &frame.frame_header);
      break;

    case GST_AV1_OBU_METADATA:
    {
      GstAV1MetadataOBU metadata;

      pres = gst_av1_parser_parse_metadata_obu (self->parser, obu, &metadata);
      break;
    }

    case GST_AV1_OBU_TILE_GROUP:
    {
      GstAV1TileGroupOBU tile_group;

      pres =
          gst_av1_parser_parse_tile_group_obu (self->parser, obu, &tile_group);
      break;
    }

    case GST_AV1_OBU_TILE_LIST:
    {
      GstAV1TileListOBU tile_list;

      pres = gst_av1_parser_parse_tile_list_obu (self->parser, obu, &tile_list);
      break;
    }

    case GST_AV1_OBU_PADDING:
      break;
    default:
      GST_WARNING_OBJECT (self, "an unrecognized obu type %d", obu->obu_type);
      pres = GST_AV1_PARSER_BITSTREAM_ERROR;
      break;
  }

  if (obu->obu_type == GST_AV1_OBU_FRAME_HEADER
      || obu->obu_type == GST_AV1_OBU_FRAME
      || obu->obu_type == GST_AV1_OBU_REDUNDANT_FRAME_HEADER) {
    GstAV1FrameHeaderOBU *fh = &frame_header;

    if (obu->obu_type == GST_AV1_OBU_FRAME)
      fh = &frame.frame_header;

    if (!fh->show_existing_frame || fh->frame_type == GST_AV1_KEY_FRAME)
      pres = gst_av1_parser_reference_frame_update (self->parser, fh);
  }

  return pres;
}

static GstFlowReturn
gst_av1_2_json_chain (GstPad * sinkpad, GstObject * object, GstBuffer * in_buf)
{
  GstAV12json *self = GST_AV1_2_JSON (object);
  JsonObject *json = self->json;
  GstAV1ParserResult pres;
  GstAV1OBU obu;
  GstBuffer *out_buf;
  gchar *json_string;
  guint32 offset = 0, consumed;
  guint length;
  GstMapInfo in_map, out_map;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!gst_buffer_map (in_buf, &in_map, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Cannot map buffer");
    return GST_FLOW_ERROR;
  }

  while (offset < in_map.size) {
    pres =
        gst_av1_parser_identify_one_obu (self->parser, in_map.data + offset,
        in_map.size - offset, &obu, &consumed);

    if (pres != GST_AV1_PARSER_OK) {
      GST_WARNING_OBJECT (self, "Cannot get OBU");
      ret = GST_FLOW_ERROR;
      goto unmap;
    }

    pres = gst_av1_2_json_handle_one_obu (self, &obu);
    if (pres != GST_AV1_PARSER_OK) {
      GST_WARNING_OBJECT (self, "Cannot parse frame header");
      ret = GST_FLOW_ERROR;
      goto unmap;
    }
    offset += consumed;
  }

  json_string = get_string_from_json_object (json);
  length = strlen (json_string);
  out_buf = gst_buffer_new_allocate (NULL, length, NULL);
  gst_buffer_map (out_buf, &out_map, GST_MAP_WRITE);
  if (length)
    memcpy (&out_map.data[0], json_string, length);
  gst_buffer_unmap (out_buf, &out_map);

  g_free (json_string);

  gst_buffer_copy_into (out_buf, in_buf,
      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS |
      GST_BUFFER_COPY_METADATA, 0, -1);
  ret = gst_pad_push (self->srcpad, out_buf);

unmap:
  switch (pres) {
    case GST_AV1_PARSER_BITSTREAM_ERROR:
    case GST_AV1_PARSER_MISSING_OBU_REFERENCE:
    case GST_AV1_PARSER_NO_MORE_DATA:
      if (self->use_annex_b)
        gst_av1_parser_reset_annex_b (self->parser);
      break;
    default:
      break;
  }
  gst_buffer_unmap (in_buf, &in_map);
  gst_buffer_unref (in_buf);

  return ret;
}

static void
gst_av1_2_json_use_annexb (GstAV12json * self, GstCaps * caps)
{
  if (caps && gst_caps_get_size (caps) > 0) {
    GstStructure *s = gst_caps_get_structure (caps, 0);
    const gchar *str_align = NULL;
    const gchar *str_stream = NULL;

    str_align = gst_structure_get_string (s, "alignment");
    str_stream = gst_structure_get_string (s, "stream-format");

    self->use_annex_b = FALSE;
    if (str_stream && g_strcmp0 (str_stream, "annexb") == 0)
      if (str_align && g_strcmp0 (str_align, "tu") == 0) {
        self->use_annex_b = TRUE;
        return;
      }
  }

  gst_av1_parser_reset (self->parser, self->use_annex_b);
}


static gboolean
gst_av1_2_json_set_caps (GstAV12json * self, GstCaps * caps)
{
  GstCaps *src_caps =
      gst_caps_new_simple ("text/x-json", "format", G_TYPE_STRING, "av1", NULL);
  GstEvent *event;

  event = gst_event_new_caps (src_caps);
  gst_caps_unref (src_caps);

  gst_av1_2_json_use_annexb (self, caps);

  return gst_pad_push_event (self->srcpad, event);
}

static gboolean
gst_av1_2_json_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstAV12json *self = GST_AV1_2_JSON (parent);
  gboolean res = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      res = gst_av1_2_json_set_caps (self, caps);
      gst_event_unref (event);
      break;
    }
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }

  return res;
}

static void
gst_av1_2_json_class_init (GstAV12jsonClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->finalize = gst_av1_2_json_finalize;

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gst_element_class_set_static_metadata (gstelement_class, "Av12json",
      "Transform",
      "AV1 to json element",
      "Benjamin Gaignard <benjamin.gaignard@collabora.com>");
}

static void
gst_av1_2_json_init (GstAV12json * self)
{
  self->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_chain_function (self->sinkpad, gst_av1_2_json_chain);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_av1_2_json_sink_event));

  self->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  self->json = json_object_new ();

  self->parser = gst_av1_parser_new ();
  gst_av1_parser_reset (self->parser, FALSE);
}
