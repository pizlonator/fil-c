/*
 * gsth2652json.c - H.265 parsed bistream to json
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
 * SECTION:element-h2652json
 * @title: h2652json
 *
 * Convert H.265 bitstream parameters to JSON formated text.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/h.265/file ! parsebin ! h2652json ! filesink location=/path/to/json/file
 * ```
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/base/base.h>
#include <json-glib/json-glib.h>

#include "gsth2652json.h"

GST_DEBUG_CATEGORY (gst_h265_2_json_debug);
#define GST_CAT_DEFAULT gst_h265_2_json_debug

struct _GstH2652json
{
  GstElement parent;

  GstPad *sinkpad, *srcpad;
  GstH265Parser *parser;

  GArray *split_nalu;

  gint nal_length_size;
  gboolean use_hevc;

  JsonObject *json;
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h265")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-json,format=h265"));

G_DEFINE_TYPE_WITH_CODE (GstH2652json, gst_h265_2_json,
    GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_h265_2_json_debug, "h2652json", 0,
        "H.265 to json"));

static void
gst_h265_2_json_finalize (GObject * object)
{
  GstH2652json *self = GST_H265_2_JSON (object);

  json_object_unref (self->json);
  gst_h265_parser_free (self->parser);
  g_array_unref (self->split_nalu);
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

static JsonObject *
gst_h265_2_json_hrd_params (GstH265HRDParams * hrd_params,
    int max_sub_layers_minus1)
{
  JsonObject *hrd = json_object_new ();
  JsonArray *fixed_pic_rate_general_flag, *fixed_pic_rate_within_cvs_flag;
  JsonArray *elemental_duration_in_tc_minus1, *low_delay_hrd_flag,
      *cpb_cnt_minus1;
  JsonArray *sublayer_hrd_params;
  gint i, j;

  json_object_set_boolean_member (hrd, "nal hrd parameters present flag",
      hrd_params->nal_hrd_parameters_present_flag);
  json_object_set_boolean_member (hrd, "vcl hrd parameters present flag",
      hrd_params->vcl_hrd_parameters_present_flag);

  if (hrd_params->nal_hrd_parameters_present_flag
      || hrd_params->vcl_hrd_parameters_present_flag) {
    json_object_set_boolean_member (hrd, "sub pic hrd params present flag",
        hrd_params->sub_pic_hrd_params_present_flag);

    if (hrd_params->sub_pic_hrd_params_present_flag) {
      json_object_set_int_member (hrd, "tick divisor minus2",
          hrd_params->tick_divisor_minus2);
      json_object_set_int_member (hrd,
          "du cpb removal delay increment length minus1",
          hrd_params->du_cpb_removal_delay_increment_length_minus1);
      json_object_set_boolean_member (hrd,
          "sub pic cpb params in pic timing sei flag",
          hrd_params->sub_pic_cpb_params_in_pic_timing_sei_flag);
      json_object_set_int_member (hrd, "dpb output delay du length minus1",
          hrd_params->dpb_output_delay_du_length_minus1);
    }

    json_object_set_int_member (hrd, "bit rate scale",
        hrd_params->bit_rate_scale);
    json_object_set_int_member (hrd, "cpb size scale",
        hrd_params->cpb_size_scale);
    if (hrd_params->sub_pic_hrd_params_present_flag)
      json_object_set_int_member (hrd, "cpb size du scale",
          hrd_params->cpb_size_du_scale);

    json_object_set_int_member (hrd, "initial cpb removal delay length minus1",
        hrd_params->initial_cpb_removal_delay_length_minus1);
    json_object_set_int_member (hrd, "au cpb removal delay length minus1",
        hrd_params->au_cpb_removal_delay_length_minus1);
    json_object_set_int_member (hrd, "dpb output delay length minus1",
        hrd_params->dpb_output_delay_length_minus1);
  }

  fixed_pic_rate_general_flag = json_array_new ();
  fixed_pic_rate_within_cvs_flag = json_array_new ();
  elemental_duration_in_tc_minus1 = json_array_new ();
  low_delay_hrd_flag = json_array_new ();
  cpb_cnt_minus1 = json_array_new ();
  sublayer_hrd_params = json_array_new ();

  for (i = 0; i <= max_sub_layers_minus1 && i < 7; i++) {
    GstH265SubLayerHRDParams *subparams = &hrd_params->sublayer_hrd_params[i];

    json_array_add_boolean_element (fixed_pic_rate_general_flag,
        hrd_params->fixed_pic_rate_general_flag[i]);
    json_array_add_boolean_element (fixed_pic_rate_within_cvs_flag,
        hrd_params->fixed_pic_rate_within_cvs_flag[i]);
    json_array_add_int_element (elemental_duration_in_tc_minus1,
        hrd_params->elemental_duration_in_tc_minus1[i]);
    json_array_add_boolean_element (low_delay_hrd_flag,
        hrd_params->low_delay_hrd_flag[i]);
    json_array_add_int_element (cpb_cnt_minus1, hrd_params->cpb_cnt_minus1[i]);

    for (j = 0; j < 32; j++) {
      JsonObject *subparam = json_object_new ();

      json_object_set_int_member (subparam, "bit rate value minus1",
          subparams->bit_rate_value_minus1[j]);
      json_object_set_int_member (subparam, "cpb size value minus1",
          subparams->cpb_size_value_minus1[j]);
      json_object_set_int_member (subparam, "cpb size du value minus1",
          subparams->cpb_size_du_value_minus1[j]);
      json_object_set_int_member (subparam, "bit rate du value minus1",
          subparams->bit_rate_du_value_minus1[j]);
      json_object_set_boolean_member (subparam, "cbr flag",
          subparams->cbr_flag[j]);
      json_array_add_object_element (sublayer_hrd_params, subparam);
    }
  }
  json_object_set_array_member (hrd, "fixed pic rate general flag",
      fixed_pic_rate_general_flag);
  json_object_set_array_member (hrd, "fixed pic rate within cvs flag",
      fixed_pic_rate_within_cvs_flag);
  json_object_set_array_member (hrd, "elemental duration in tc minus1",
      elemental_duration_in_tc_minus1);
  json_object_set_array_member (hrd, "low delay hrd flag", low_delay_hrd_flag);
  json_object_set_array_member (hrd, "cpb cnt minus1", cpb_cnt_minus1);
  json_object_set_array_member (hrd, "sublayer hrd params",
      sublayer_hrd_params);

  return hrd;
}

static JsonObject *
gst_h265_2_json_profile_tier_level (const GstH265ProfileTierLevel * ptl)
{
  JsonObject *profile_tier_level = json_object_new ();
  JsonArray *profile_compatibility_flag;
  JsonArray *sub_layer_profile_present_flag, *sub_layer_level_present_flag,
      *sub_layer_profile_space;
  JsonArray *sub_layer_tier_flag, *sub_layer_profile_idc,
      *sub_layer_profile_compatibility_flag;
  JsonArray *sub_layer_progressive_source_flag,
      *sub_layer_interlaced_source_flag, *sub_layer_non_packed_constraint_flag;
  JsonArray *sub_layer_frame_only_constraint_flag, *sub_layer_level_idc;
  gint i, j;

  json_object_set_int_member (profile_tier_level, "profile space",
      ptl->profile_space);
  json_object_set_int_member (profile_tier_level, "tier flag", ptl->tier_flag);
  json_object_set_int_member (profile_tier_level, "profile idc",
      ptl->profile_idc);
  profile_compatibility_flag = json_array_new ();
  for (i = 0; i < 32; i++)
    json_array_add_boolean_element (profile_compatibility_flag,
        ptl->profile_compatibility_flag[i]);
  json_object_set_array_member (profile_tier_level,
      "profile compatibility flag", profile_compatibility_flag);
  json_object_set_boolean_member (profile_tier_level, "progressive source flag",
      ptl->progressive_source_flag);
  json_object_set_boolean_member (profile_tier_level, "interlaced source flag",
      ptl->interlaced_source_flag);
  json_object_set_boolean_member (profile_tier_level,
      "non packed constraint flag", ptl->non_packed_constraint_flag);
  json_object_set_boolean_member (profile_tier_level,
      "frame only constraint flag", ptl->frame_only_constraint_flag);
  json_object_set_boolean_member (profile_tier_level,
      "max 12bit constraint flag", ptl->max_12bit_constraint_flag);
  json_object_set_boolean_member (profile_tier_level,
      "max 10bit constraint flag", ptl->max_10bit_constraint_flag);
  json_object_set_boolean_member (profile_tier_level,
      "max 8bit constraint flag", ptl->max_8bit_constraint_flag);
  json_object_set_boolean_member (profile_tier_level,
      "max 422chroma constraint flag", ptl->max_422chroma_constraint_flag);
  json_object_set_boolean_member (profile_tier_level,
      "max 420chroma constraint flag", ptl->max_420chroma_constraint_flag);
  json_object_set_boolean_member (profile_tier_level,
      "max monochrome constraint flag", ptl->max_monochrome_constraint_flag);
  json_object_set_boolean_member (profile_tier_level, "intra constraint flag",
      ptl->intra_constraint_flag);
  json_object_set_boolean_member (profile_tier_level,
      "one picture only constraint flag",
      ptl->one_picture_only_constraint_flag);
  json_object_set_boolean_member (profile_tier_level,
      "lower bit rate constraint flag", ptl->lower_bit_rate_constraint_flag);
  json_object_set_boolean_member (profile_tier_level,
      "max 14bit constraint flag", ptl->max_14bit_constraint_flag);
  json_object_set_int_member (profile_tier_level, "level idc", ptl->level_idc);

  sub_layer_profile_present_flag = json_array_new ();
  sub_layer_level_present_flag = json_array_new ();
  sub_layer_profile_space = json_array_new ();
  sub_layer_tier_flag = json_array_new ();
  sub_layer_profile_idc = json_array_new ();
  sub_layer_profile_compatibility_flag = json_array_new ();
  sub_layer_progressive_source_flag = json_array_new ();
  sub_layer_interlaced_source_flag = json_array_new ();
  sub_layer_non_packed_constraint_flag = json_array_new ();
  sub_layer_frame_only_constraint_flag = json_array_new ();
  sub_layer_level_idc = json_array_new ();

  for (i = 0; i < 6; i++) {
    json_array_add_boolean_element (sub_layer_profile_present_flag,
        ptl->sub_layer_profile_present_flag[i]);
    json_array_add_boolean_element (sub_layer_level_present_flag,
        ptl->sub_layer_level_present_flag[i]);
    json_array_add_int_element (sub_layer_profile_space,
        ptl->sub_layer_profile_space[i]);
    json_array_add_int_element (sub_layer_tier_flag,
        ptl->sub_layer_tier_flag[i]);
    json_array_add_int_element (sub_layer_profile_idc,
        ptl->sub_layer_profile_idc[i]);
    for (j = 0; j < 32; j++)
      json_array_add_boolean_element (sub_layer_profile_compatibility_flag,
          ptl->sub_layer_profile_compatibility_flag[i][j]);
    json_array_add_boolean_element (sub_layer_progressive_source_flag,
        ptl->sub_layer_progressive_source_flag[i]);
    json_array_add_boolean_element (sub_layer_interlaced_source_flag,
        ptl->sub_layer_interlaced_source_flag[i]);
    json_array_add_boolean_element (sub_layer_non_packed_constraint_flag,
        ptl->sub_layer_non_packed_constraint_flag[i]);
    json_array_add_boolean_element (sub_layer_frame_only_constraint_flag,
        ptl->sub_layer_frame_only_constraint_flag[i]);
    json_array_add_int_element (sub_layer_level_idc,
        ptl->sub_layer_level_idc[i]);
  }

  json_object_set_array_member (profile_tier_level,
      "sub layer profile present flag", sub_layer_profile_present_flag);
  json_object_set_array_member (profile_tier_level,
      "sub layer level present flag", sub_layer_level_present_flag);
  json_object_set_array_member (profile_tier_level, "sub layer profile space",
      sub_layer_profile_space);
  json_object_set_array_member (profile_tier_level, "sub layer tier flag",
      sub_layer_tier_flag);
  json_object_set_array_member (profile_tier_level, "sub layer profile idc",
      sub_layer_profile_idc);
  json_object_set_array_member (profile_tier_level,
      "sub layer profile compatibility flag",
      sub_layer_profile_compatibility_flag);
  json_object_set_array_member (profile_tier_level,
      "sub layer progressive source flag", sub_layer_progressive_source_flag);
  json_object_set_array_member (profile_tier_level,
      "sub layer interlaced source flag", sub_layer_interlaced_source_flag);
  json_object_set_array_member (profile_tier_level,
      "sub layer non packed constraint flag",
      sub_layer_non_packed_constraint_flag);
  json_object_set_array_member (profile_tier_level,
      "sub layer frame_only constraint flag",
      sub_layer_frame_only_constraint_flag);
  json_object_set_array_member (profile_tier_level, "sub layer level idc",
      sub_layer_level_idc);

  return profile_tier_level;
}

static JsonObject *
gst_h265_2_json_scaling_list (GstH265ScalingList * sl)
{
  JsonObject *scaling_list = json_object_new ();
  JsonArray *scaling_list_dc_coef_minus8_16x16;
  JsonArray *scaling_list_dc_coef_minus8_32x32;
  JsonArray *scaling_lists_4x4;
  JsonArray *scaling_lists_8x8;
  JsonArray *scaling_lists_16x16;
  JsonArray *scaling_lists_32x32;
  gint i, j;

  scaling_list_dc_coef_minus8_16x16 = json_array_new ();
  for (i = 0; i < 6; i++)
    json_array_add_int_element (scaling_list_dc_coef_minus8_16x16,
        sl->scaling_list_dc_coef_minus8_16x16[i]);
  json_object_set_array_member (scaling_list,
      "scaling list dc coef minus8 16x16", scaling_list_dc_coef_minus8_16x16);

  scaling_list_dc_coef_minus8_32x32 = json_array_new ();
  for (i = 0; i < 2; i++)
    json_array_add_int_element (scaling_list_dc_coef_minus8_32x32,
        sl->scaling_list_dc_coef_minus8_32x32[i]);
  json_object_set_array_member (scaling_list,
      "scaling list dc coef minus8 32x32", scaling_list_dc_coef_minus8_32x32);

  scaling_lists_4x4 = json_array_new ();
  for (i = 0; i < 6; i++)
    for (j = 0; j < 16; j++)
      json_array_add_int_element (scaling_lists_4x4,
          sl->scaling_lists_4x4[i][j]);
  json_object_set_array_member (scaling_list, "scaling lists 4x4",
      scaling_lists_4x4);

  scaling_lists_8x8 = json_array_new ();
  for (i = 0; i < 6; i++)
    for (j = 0; j < 64; j++)
      json_array_add_int_element (scaling_lists_8x8,
          sl->scaling_lists_8x8[i][j]);
  json_object_set_array_member (scaling_list, "scaling lists 8x8",
      scaling_lists_8x8);

  scaling_lists_16x16 = json_array_new ();
  for (i = 0; i < 6; i++)
    for (j = 0; j < 64; j++)
      json_array_add_int_element (scaling_lists_16x16,
          sl->scaling_lists_16x16[i][j]);
  json_object_set_array_member (scaling_list, "scaling lists 16x16",
      scaling_lists_16x16);

  scaling_lists_32x32 = json_array_new ();
  for (i = 0; i < 2; i++)
    for (j = 0; j < 64; j++)
      json_array_add_int_element (scaling_lists_32x32,
          sl->scaling_lists_32x32[i][j]);
  json_object_set_array_member (scaling_list, "scaling lists 32x32",
      scaling_lists_32x32);

  return scaling_list;
}

static GstFlowReturn
gst_h265_2_json_parse_sps (GstH2652json * self, GstH265NalUnit * nalu)
{
  JsonObject *json = self->json;
  JsonObject *sps, *profile_tier_level, *scaling_list;
  JsonArray *max_dec_pic_buffering_minus1;
  JsonArray *max_num_reorder_pics;
  JsonArray *max_latency_increase_plus1;
  JsonArray *short_term_ref_pic_set;
  GstH265SPS h265_sps;
  GstH265ParserResult pres;
  gint i, j;

  pres = gst_h265_parser_parse_sps (self->parser, nalu, &h265_sps, TRUE);
  if (pres != GST_H265_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to parse SPS, result %d", pres);
    return GST_FLOW_ERROR;
  }

  pres = gst_h265_parser_update_sps (self->parser, &h265_sps);
  if (pres != GST_H265_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to update SPS, result %d", pres);
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (self, "SPS parsed");

  sps = json_object_new ();

  json_object_set_int_member (sps, "vps id", h265_sps.vps_id);
  json_object_set_int_member (sps, "max sub layers minus1",
      h265_sps.max_sub_layers_minus1);
  json_object_set_boolean_member (sps, "temporal id nesting flag",
      h265_sps.temporal_id_nesting_flag);

  profile_tier_level =
      gst_h265_2_json_profile_tier_level (&h265_sps.profile_tier_level);
  json_object_set_object_member (sps, "profile tier level", profile_tier_level);

  json_object_set_int_member (sps, "chroma format idc",
      h265_sps.chroma_format_idc);
  json_object_set_boolean_member (sps, "separate colour plane flag",
      h265_sps.separate_colour_plane_flag);
  json_object_set_int_member (sps, "pic width in luma samples",
      h265_sps.pic_width_in_luma_samples);
  json_object_set_int_member (sps, "pic height in luma_samples",
      h265_sps.pic_height_in_luma_samples);

  json_object_set_boolean_member (sps, "conformance window flag",
      h265_sps.conformance_window_flag);
  if (h265_sps.conformance_window_flag) {
    json_object_set_int_member (sps, "conf win left offset",
        h265_sps.conf_win_left_offset);
    json_object_set_int_member (sps, "conf win right offset",
        h265_sps.conf_win_right_offset);
    json_object_set_int_member (sps, "conf win top offset",
        h265_sps.conf_win_top_offset);
    json_object_set_int_member (sps, "conf win bottom offset",
        h265_sps.conf_win_bottom_offset);
  }

  json_object_set_int_member (sps, "bit depth luma minus8",
      h265_sps.bit_depth_luma_minus8);
  json_object_set_int_member (sps, "bit depth chroma minus8",
      h265_sps.bit_depth_chroma_minus8);
  json_object_set_int_member (sps, "log2 max pic order cnt lsb minus4",
      h265_sps.log2_max_pic_order_cnt_lsb_minus4);
  json_object_set_boolean_member (sps, "sub_layer_ordering_info_present_flag",
      h265_sps.sub_layer_ordering_info_present_flag);

  max_dec_pic_buffering_minus1 = json_array_new ();
  for (i = 0; i < GST_H265_MAX_SUB_LAYERS; i++)
    json_array_add_int_element (max_dec_pic_buffering_minus1,
        h265_sps.max_dec_pic_buffering_minus1[i]);
  json_object_set_array_member (sps, "max dec pic buffering minus1",
      max_dec_pic_buffering_minus1);

  max_num_reorder_pics = json_array_new ();
  for (i = 0; i < GST_H265_MAX_SUB_LAYERS; i++)
    json_array_add_int_element (max_num_reorder_pics,
        h265_sps.max_num_reorder_pics[i]);
  json_object_set_array_member (sps, "max num reorder pics",
      max_num_reorder_pics);

  max_latency_increase_plus1 = json_array_new ();
  for (i = 0; i < GST_H265_MAX_SUB_LAYERS; i++)
    json_array_add_int_element (max_latency_increase_plus1,
        h265_sps.max_latency_increase_plus1[i]);
  json_object_set_array_member (sps, "max_latency_increase_plus1",
      max_latency_increase_plus1);

  json_object_set_int_member (sps, "log2 min luma coding block size minus3",
      h265_sps.log2_min_luma_coding_block_size_minus3);
  json_object_set_int_member (sps, "log2 diff max min luma coding block size",
      h265_sps.log2_diff_max_min_luma_coding_block_size);
  json_object_set_int_member (sps, "log2 min transform block size minus2",
      h265_sps.log2_min_transform_block_size_minus2);
  json_object_set_int_member (sps, "log2 diff max min transform block size",
      h265_sps.log2_diff_max_min_transform_block_size);
  json_object_set_int_member (sps, "max transform hierarchy depth inter",
      h265_sps.max_transform_hierarchy_depth_inter);
  json_object_set_int_member (sps, "max transform hierarchy depth intra",
      h265_sps.max_transform_hierarchy_depth_intra);

  json_object_set_boolean_member (sps, "scaling list enabled flag",
      h265_sps.scaling_list_enabled_flag);
  if (h265_sps.scaling_list_enabled_flag)
    json_object_set_boolean_member (sps, "scaling list data present flag",
        h265_sps.scaling_list_data_present_flag);

  scaling_list = gst_h265_2_json_scaling_list (&h265_sps.scaling_list);
  json_object_set_object_member (sps, "scaling list", scaling_list);

  json_object_set_boolean_member (sps, "amp enabled flag",
      h265_sps.amp_enabled_flag);
  json_object_set_boolean_member (sps, "sample adaptive offset enabled flag",
      h265_sps.sample_adaptive_offset_enabled_flag);
  json_object_set_boolean_member (sps, "pcm enabled flag",
      h265_sps.pcm_enabled_flag);
  if (h265_sps.pcm_enabled_flag) {
    json_object_set_int_member (sps, "pcm sample bit depth luma minus1",
        h265_sps.pcm_sample_bit_depth_luma_minus1);
    json_object_set_int_member (sps, "pcm sample bit depth chroma minus1",
        h265_sps.pcm_sample_bit_depth_chroma_minus1);
    json_object_set_int_member (sps,
        "log2 min pcm luma coding block size minus3",
        h265_sps.log2_min_pcm_luma_coding_block_size_minus3);
    json_object_set_int_member (sps,
        "log2 diff max min pcm luma coding block size",
        h265_sps.log2_diff_max_min_pcm_luma_coding_block_size);
    json_object_set_boolean_member (sps, "pcm loop filter disabled flag",
        h265_sps.pcm_loop_filter_disabled_flag);
  }

  json_object_set_int_member (sps, "num short term ref pic sets",
      h265_sps.num_short_term_ref_pic_sets);
  short_term_ref_pic_set = json_array_new ();
  for (i = 0; i < h265_sps.num_short_term_ref_pic_sets; i++) {
    JsonObject *pic_set = json_object_new ();

    json_object_set_boolean_member (pic_set,
        "inter ref pic set prediction flag",
        h265_sps.short_term_ref_pic_set[i].inter_ref_pic_set_prediction_flag);
    json_object_set_int_member (pic_set, "delta idx minus1",
        h265_sps.short_term_ref_pic_set[i].delta_idx_minus1);
    json_object_set_int_member (pic_set, "delta rps sign",
        h265_sps.short_term_ref_pic_set[i].delta_rps_sign);
    json_object_set_int_member (pic_set, "abs delta rps minus1",
        h265_sps.short_term_ref_pic_set[i].abs_delta_rps_minus1);

    json_array_add_object_element (short_term_ref_pic_set, pic_set);
  }
  json_object_set_array_member (sps, "num short term ref pic sets",
      short_term_ref_pic_set);

  json_object_set_boolean_member (sps, "long term ref pics present flag",
      h265_sps.long_term_ref_pics_present_flag);
  if (h265_sps.long_term_ref_pics_present_flag) {
    JsonArray *lt_ref_pic_poc_lsb_sps = json_array_new ();
    JsonArray *used_by_curr_pic_lt_sps_flag = json_array_new ();

    json_object_set_int_member (sps, "num long term ref pics sps",
        h265_sps.num_long_term_ref_pics_sps);

    for (j = 0; j < h265_sps.num_long_term_ref_pics_sps; j++) {
      json_array_add_int_element (lt_ref_pic_poc_lsb_sps,
          h265_sps.lt_ref_pic_poc_lsb_sps[j]);
      json_array_add_int_element (used_by_curr_pic_lt_sps_flag,
          h265_sps.used_by_curr_pic_lt_sps_flag[i]);
    }
    json_object_set_array_member (sps, "lt ref pic poc lsb sps",
        lt_ref_pic_poc_lsb_sps);
    json_object_set_array_member (sps, "used by curr pic lt sps flag",
        used_by_curr_pic_lt_sps_flag);
  }

  json_object_set_boolean_member (sps, "temporal mvp enabled flag",
      h265_sps.temporal_mvp_enabled_flag);
  json_object_set_boolean_member (sps, "strong intra smoothing enabled flag",
      h265_sps.strong_intra_smoothing_enabled_flag);
  json_object_set_boolean_member (sps, "vui parameters present flag",
      h265_sps.vui_parameters_present_flag);

  if (h265_sps.vui_parameters_present_flag) {
    GstH265VUIParams *params = &h265_sps.vui_params;
    JsonObject *vui = json_object_new ();

    json_object_set_boolean_member (vui, "aspect ratio info present flag",
        params->aspect_ratio_info_present_flag);
    json_object_set_int_member (vui, "aspect ratio idc",
        params->aspect_ratio_idc);
    if (params->aspect_ratio_idc == 255) {
      json_object_set_int_member (vui, "sar width", params->sar_width);
      json_object_set_int_member (vui, "sar height", params->sar_height);
    }

    json_object_set_boolean_member (vui, "overscan info present flag",
        params->overscan_info_present_flag);
    if (params->overscan_info_present_flag)
      json_object_set_boolean_member (vui, "overscan appropriate flag",
          params->overscan_appropriate_flag);

    json_object_set_boolean_member (vui, "video signal type present flag",
        params->video_signal_type_present_flag);
    if (params->video_signal_type_present_flag) {
      json_object_set_int_member (vui, "video format", params->video_format);
      json_object_set_boolean_member (vui, "video full range flag",
          params->video_full_range_flag);
      json_object_set_boolean_member (vui, "colour description present flag",
          params->colour_description_present_flag);
      json_object_set_int_member (vui, "colour primaries",
          params->colour_primaries);
      json_object_set_int_member (vui, "transfer characteristics",
          params->transfer_characteristics);
      json_object_set_int_member (vui, "matrix coefficients",
          params->matrix_coefficients);
    }

    json_object_set_boolean_member (vui, "chroma loc info present flag",
        params->chroma_loc_info_present_flag);
    if (params->chroma_loc_info_present_flag) {
      json_object_set_int_member (vui, "chroma sample loc type top field",
          params->chroma_sample_loc_type_top_field);
      json_object_set_int_member (vui, "chroma sample loc type bottom field",
          params->chroma_sample_loc_type_bottom_field);
    }

    json_object_set_boolean_member (vui, "neutral chroma indication flag",
        params->neutral_chroma_indication_flag);
    json_object_set_boolean_member (vui, "field seq flag",
        params->field_seq_flag);
    json_object_set_boolean_member (vui, "frame field info present flag",
        params->frame_field_info_present_flag);

    json_object_set_boolean_member (vui, "default display window flag",
        params->default_display_window_flag);
    if (params->default_display_window_flag) {
      json_object_set_int_member (vui, "def disp win left offset",
          params->def_disp_win_left_offset);
      json_object_set_int_member (vui, "def disp win right offset",
          params->def_disp_win_right_offset);
      json_object_set_int_member (vui, "def disp win top offset",
          params->def_disp_win_top_offset);
      json_object_set_int_member (vui, "def disp win bottom offset",
          params->def_disp_win_bottom_offset);
    }

    json_object_set_boolean_member (vui, "timing info present flag",
        params->timing_info_present_flag);
    if (params->timing_info_present_flag) {
      json_object_set_int_member (vui, "num units in tick",
          params->num_units_in_tick);
      json_object_set_int_member (vui, "time scale", params->time_scale);

      json_object_set_boolean_member (vui, "poc proportional to timing flag",
          params->poc_proportional_to_timing_flag);
      if (params->poc_proportional_to_timing_flag)
        json_object_set_int_member (vui, "num ticks poc diff one minus1",
            params->num_ticks_poc_diff_one_minus1);

      json_object_set_boolean_member (vui, "hrd_parameters_present_flag",
          params->hrd_parameters_present_flag);
      if (params->hrd_parameters_present_flag) {
        JsonObject *hrd = gst_h265_2_json_hrd_params (&params->hrd_params,
            h265_sps.max_sub_layers_minus1);

        json_object_set_object_member (vui, "hrd params", hrd);
      }
    }

    json_object_set_boolean_member (vui, "bitstream restriction flag",
        params->bitstream_restriction_flag);
    if (params->bitstream_restriction_flag) {
      json_object_set_boolean_member (vui, "tiles fixed structure flag",
          params->tiles_fixed_structure_flag);
      json_object_set_boolean_member (vui,
          "motion vectors over pic boundaries flag",
          params->motion_vectors_over_pic_boundaries_flag);
      json_object_set_boolean_member (vui, "restricted ref pic lists flag",
          params->restricted_ref_pic_lists_flag);
      json_object_set_int_member (vui, "min spatial segmentation idc",
          params->min_spatial_segmentation_idc);
      json_object_set_int_member (vui, "max bytes per pic denom",
          params->max_bytes_per_pic_denom);
      json_object_set_int_member (vui, "max bits per min cu denom",
          params->max_bits_per_min_cu_denom);
      json_object_set_int_member (vui, "log2 max mv length horizontal",
          params->log2_max_mv_length_horizontal);
      json_object_set_int_member (vui, "log2 max mv length vertical",
          params->log2_max_mv_length_vertical);
    }

    json_object_set_object_member (sps, "vui params", vui);
  }

  json_object_set_boolean_member (sps, "sps extension flag",
      h265_sps.sps_extension_flag);
  if (h265_sps.sps_extension_flag) {
    json_object_set_boolean_member (sps, "sps range extension flag",
        h265_sps.sps_range_extension_flag);
    json_object_set_boolean_member (sps, "sps multilayer extension_flag",
        h265_sps.sps_multilayer_extension_flag);
    json_object_set_boolean_member (sps, "sps 3d extension flag",
        h265_sps.sps_3d_extension_flag);
    json_object_set_boolean_member (sps, "sps scc extension flag",
        h265_sps.sps_scc_extension_flag);
    json_object_set_int_member (sps, "sps extension 4bits",
        h265_sps.sps_extension_4bits);

    if (h265_sps.sps_range_extension_flag) {
      json_object_set_boolean_member (sps,
          "transform skip rotation enabled flag",
          h265_sps.sps_extension_params.transform_skip_rotation_enabled_flag);
      json_object_set_boolean_member (sps,
          "transform skip context enabled flag",
          h265_sps.sps_extension_params.transform_skip_context_enabled_flag);
      json_object_set_boolean_member (sps, "implicit rdpcm enabled flag",
          h265_sps.sps_extension_params.implicit_rdpcm_enabled_flag);
      json_object_set_boolean_member (sps, "explicit rdpcm enabled flag",
          h265_sps.sps_extension_params.explicit_rdpcm_enabled_flag);
      json_object_set_boolean_member (sps, "extended precision processing flag",
          h265_sps.sps_extension_params.extended_precision_processing_flag);
      json_object_set_boolean_member (sps, "intra smoothing disabled flag",
          h265_sps.sps_extension_params.intra_smoothing_disabled_flag);
      json_object_set_boolean_member (sps,
          "high precision offsets enabled flag",
          h265_sps.sps_extension_params.high_precision_offsets_enabled_flag);
      json_object_set_boolean_member (sps,
          "persistent rice adaptation enabled flag",
          h265_sps.
          sps_extension_params.persistent_rice_adaptation_enabled_flag);
      json_object_set_boolean_member (sps,
          "cabac bypass alignment enabled flag",
          h265_sps.sps_extension_params.cabac_bypass_alignment_enabled_flag);
    }

    if (h265_sps.sps_scc_extension_flag) {
      JsonArray *sps_palette_predictor_initializer = json_array_new ();

      json_object_set_boolean_member (sps, "sps curr pic ref enabled flag",
          h265_sps.sps_scc_extension_params.sps_curr_pic_ref_enabled_flag);
      json_object_set_boolean_member (sps, "palette mode enabled flag",
          h265_sps.sps_scc_extension_params.palette_mode_enabled_flag);
      json_object_set_int_member (sps, "palette max size",
          h265_sps.sps_scc_extension_params.palette_max_size);
      json_object_set_int_member (sps, "delta palette max_predictor size",
          h265_sps.sps_scc_extension_params.delta_palette_max_predictor_size);
      json_object_set_boolean_member (sps,
          "sps palette predictor initializers present flag",
          h265_sps.
          sps_scc_extension_params.sps_palette_predictor_initializers_present_flag);
      json_object_set_int_member (sps,
          "sps num palette predictor initializer minus1",
          h265_sps.
          sps_scc_extension_params.sps_num_palette_predictor_initializer_minus1);

      for (i = 0; i < 3; i++) {
        JsonArray *sub = json_array_new ();
        for (j = 0; j < 128; j++)
          json_array_add_int_element (sub,
              h265_sps.
              sps_scc_extension_params.sps_palette_predictor_initializer[i][j]);
        json_array_add_array_element (sps_palette_predictor_initializer, sub);
      }
      json_object_set_array_member (sps, "sps palette predictor initializer",
          sps_palette_predictor_initializer);
      json_object_set_int_member (sps, "motion vector resolution control idc",
          h265_sps.
          sps_scc_extension_params.motion_vector_resolution_control_idc);
      json_object_set_boolean_member (sps,
          "intra boundary filtering disabled flag",
          h265_sps.
          sps_scc_extension_params.intra_boundary_filtering_disabled_flag);
    }
  }

  json_object_set_object_member (json, "sps", sps);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_h265_2_json_parse_vps (GstH2652json * self, GstH265NalUnit * nalu)
{
  GstH265VPS h265_vps;
  JsonObject *vps;
  GstH265ParserResult pres;
  JsonObject *json = self->json;
  JsonObject *profile_tier_level, *hrd;
  JsonArray *max_dec_pic_buffering_minus1, *max_num_reorder_pics,
      *max_latency_increase_plus1;
  gint i;

  pres = gst_h265_parser_parse_vps (self->parser, nalu, &h265_vps);
  if (pres != GST_H265_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to parse VPS, result %d", pres);
    return GST_FLOW_ERROR;
  }

  pres = gst_h265_parser_update_vps (self->parser, &h265_vps);
  if (pres != GST_H265_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to update VPS, result %d", pres);
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (self, "VPS parsed");

  vps = json_object_new ();

  json_object_set_boolean_member (vps, "base layer internal flag",
      h265_vps.base_layer_internal_flag);
  json_object_set_boolean_member (vps, "base layer available flag",
      h265_vps.base_layer_available_flag);

  json_object_set_int_member (vps, "max layers minus1",
      h265_vps.max_layers_minus1);
  json_object_set_int_member (vps, "max sub layers minus1",
      h265_vps.max_sub_layers_minus1);
  json_object_set_boolean_member (vps, "temporal id nesting flag",
      h265_vps.temporal_id_nesting_flag);

  profile_tier_level =
      gst_h265_2_json_profile_tier_level (&h265_vps.profile_tier_level);
  json_object_set_object_member (vps, "profile tier level", profile_tier_level);

  json_object_set_boolean_member (vps, "sub layer ordering info present flag",
      h265_vps.sub_layer_ordering_info_present_flag);

  max_dec_pic_buffering_minus1 = json_array_new ();
  max_num_reorder_pics = json_array_new ();
  max_latency_increase_plus1 = json_array_new ();

  for (i = 0; i < GST_H265_MAX_SUB_LAYERS; i++) {
    json_array_add_int_element (max_dec_pic_buffering_minus1,
        h265_vps.max_dec_pic_buffering_minus1[i]);
    json_array_add_int_element (max_num_reorder_pics,
        h265_vps.max_num_reorder_pics[i]);
    json_array_add_int_element (max_latency_increase_plus1,
        h265_vps.max_latency_increase_plus1[i]);
  }
  json_object_set_array_member (vps, "max dec pic buffering minus1",
      max_dec_pic_buffering_minus1);
  json_object_set_array_member (vps, "max num reorder pics",
      max_num_reorder_pics);
  json_object_set_array_member (vps, "max latency increase plus1",
      max_latency_increase_plus1);

  json_object_set_int_member (vps, "max layer id", h265_vps.max_layer_id);
  json_object_set_int_member (vps, "num layer sets minus1",
      h265_vps.num_layer_sets_minus1);

  json_object_set_boolean_member (vps, "timing info present flag",
      h265_vps.timing_info_present_flag);
  json_object_set_int_member (vps, "num units in tick",
      h265_vps.num_units_in_tick);
  json_object_set_int_member (vps, "time scale", h265_vps.time_scale);
  json_object_set_boolean_member (vps, "poc proportional to timing flag",
      h265_vps.poc_proportional_to_timing_flag);
  json_object_set_int_member (vps, "num ticks poc diff one minus1",
      h265_vps.num_ticks_poc_diff_one_minus1);

  json_object_set_int_member (vps, "hrd layer set idx",
      h265_vps.hrd_layer_set_idx);
  json_object_set_boolean_member (vps, "cprms present flag",
      h265_vps.cprms_present_flag);

  json_object_set_int_member (vps, "vps extension", h265_vps.vps_extension);

  hrd = gst_h265_2_json_hrd_params (&h265_vps.hrd_params, 0);
  json_object_set_object_member (vps, "hrd params", hrd);

  json_object_set_object_member (json, "vps", vps);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_h265_2_json_parse_pps (GstH2652json * self, GstH265NalUnit * nalu)
{
  GstH265PPS h265_pps;
  JsonObject *pps;
  GstH265ParserResult pres;
  JsonObject *json = self->json;
  JsonObject *scaling_list;
  JsonArray *column_width_minus1, *row_height_minus1;
  gint i, j;

  pres = gst_h265_parser_parse_pps (self->parser, nalu, &h265_pps);
  if (pres != GST_H265_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to parse PPS, result %d", pres);
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (self, "PPS parsed");

  pps = json_object_new ();

  json_object_set_int_member (pps, "sps id", h265_pps.sps_id);

  json_object_set_boolean_member (pps, "dependent slice segments enabled flag",
      h265_pps.dependent_slice_segments_enabled_flag);
  json_object_set_boolean_member (pps, "output flag present flag",
      h265_pps.output_flag_present_flag);
  json_object_set_int_member (pps, "num extra slice header bits",
      h265_pps.num_extra_slice_header_bits);
  json_object_set_boolean_member (pps, "sign data hiding enabled flag",
      h265_pps.sign_data_hiding_enabled_flag);
  json_object_set_boolean_member (pps, "cabac init present flag",
      h265_pps.cabac_init_present_flag);
  json_object_set_int_member (pps, "num ref idx l0 default active minus1",
      h265_pps.num_ref_idx_l0_default_active_minus1);
  json_object_set_int_member (pps, "num ref idx l1 default active minus1",
      h265_pps.num_ref_idx_l1_default_active_minus1);
  json_object_set_int_member (pps, "init qp minus26", h265_pps.init_qp_minus26);
  json_object_set_boolean_member (pps, "constrained intra pred flag",
      h265_pps.constrained_intra_pred_flag);
  json_object_set_boolean_member (pps, "transform skip enabled flag",
      h265_pps.transform_skip_enabled_flag);
  json_object_set_boolean_member (pps, "cu qp delta enabled flag",
      h265_pps.cu_qp_delta_enabled_flag);
  if (h265_pps.cu_qp_delta_enabled_flag)
    json_object_set_int_member (pps, "diff cu qp delta depth",
        h265_pps.diff_cu_qp_delta_depth);

  json_object_set_int_member (pps, "cb qp offset", h265_pps.cb_qp_offset);
  json_object_set_int_member (pps, "cr qp offset", h265_pps.cr_qp_offset);
  json_object_set_boolean_member (pps, "slice chroma qp offsets present flag",
      h265_pps.slice_chroma_qp_offsets_present_flag);
  json_object_set_boolean_member (pps, "weighted pred flag",
      h265_pps.weighted_pred_flag);
  json_object_set_boolean_member (pps, "weighted bipred flag",
      h265_pps.weighted_bipred_flag);
  json_object_set_boolean_member (pps, "transquant bypass enabled flag",
      h265_pps.transquant_bypass_enabled_flag);
  json_object_set_boolean_member (pps, "tiles enabled flag",
      h265_pps.tiles_enabled_flag);
  json_object_set_boolean_member (pps, "entropy_coding_sync_enabled_flag",
      h265_pps.entropy_coding_sync_enabled_flag);

  json_object_set_int_member (pps, "num tile columns minus1",
      h265_pps.num_tile_columns_minus1);
  json_object_set_int_member (pps, "num tile rows minus1",
      h265_pps.num_tile_rows_minus1);
  json_object_set_boolean_member (pps, "uniform spacing flag",
      h265_pps.uniform_spacing_flag);

  column_width_minus1 = json_array_new ();
  for (i = 0; i < 20; i++)
    json_array_add_int_element (column_width_minus1,
        h265_pps.column_width_minus1[i]);
  json_object_set_array_member (pps, "column width minus1",
      column_width_minus1);

  row_height_minus1 = json_array_new ();
  for (i = 0; i < 22; i++)
    json_array_add_int_element (row_height_minus1,
        h265_pps.row_height_minus1[i]);
  json_object_set_array_member (pps, "row height minus1", row_height_minus1);

  json_object_set_boolean_member (pps, "loop filter across tiles enabled flag",
      h265_pps.loop_filter_across_tiles_enabled_flag);
  json_object_set_boolean_member (pps, "loop filter across slices enabled flag",
      h265_pps.loop_filter_across_slices_enabled_flag);
  json_object_set_boolean_member (pps, "deblocking filter control present flag",
      h265_pps.deblocking_filter_control_present_flag);
  json_object_set_boolean_member (pps,
      "deblocking filter override enabled_flag",
      h265_pps.deblocking_filter_override_enabled_flag);
  json_object_set_boolean_member (pps, "deblocking filter disabled flag",
      h265_pps.deblocking_filter_disabled_flag);
  json_object_set_int_member (pps, "beta offset div2",
      h265_pps.beta_offset_div2);
  json_object_set_int_member (pps, "tc offset div2", h265_pps.tc_offset_div2);

  json_object_set_boolean_member (pps, "scaling list data present flag",
      h265_pps.scaling_list_data_present_flag);

  scaling_list = gst_h265_2_json_scaling_list (&h265_pps.scaling_list);
  json_object_set_object_member (pps, "scaling list", scaling_list);

  json_object_set_boolean_member (pps, "lists modification present_flag",
      h265_pps.lists_modification_present_flag);
  json_object_set_int_member (pps, "log2 parallel merge level minus2",
      h265_pps.log2_parallel_merge_level_minus2);
  json_object_set_boolean_member (pps,
      "slice segment header extension present flag",
      h265_pps.slice_segment_header_extension_present_flag);

  json_object_set_boolean_member (pps, "pps extension flag",
      h265_pps.pps_extension_flag);
  if (h265_pps.pps_extension_flag) {
    json_object_set_boolean_member (pps, "pps range extension flag",
        h265_pps.pps_range_extension_flag);
    json_object_set_boolean_member (pps, "pps multilayer extension flag",
        h265_pps.pps_multilayer_extension_flag);
    json_object_set_boolean_member (pps, "pps 3d extension flag",
        h265_pps.pps_3d_extension_flag);
    json_object_set_boolean_member (pps, "pps scc extension flag",
        h265_pps.pps_scc_extension_flag);
    json_object_set_int_member (pps, "pps extension 4bits",
        h265_pps.pps_extension_4bits);
  }

  if (h265_pps.pps_range_extension_flag) {
    GstH265PPSExtensionParams *p = &h265_pps.pps_extension_params;
    JsonObject *params = json_object_new ();
    JsonArray *cb_qp_offset_list = json_array_new ();
    JsonArray *cr_qp_offset_list = json_array_new ();

    json_object_set_int_member (params,
        "log2 max transform skip block size minus2",
        p->log2_max_transform_skip_block_size_minus2);
    json_object_set_boolean_member (params,
        "cross component prediction enabled flag",
        p->cross_component_prediction_enabled_flag);
    json_object_set_boolean_member (params,
        "chroma qp offset list enabled flag",
        p->chroma_qp_offset_list_enabled_flag);
    json_object_set_int_member (params, "diff cu chroma qp offset depth",
        p->diff_cu_chroma_qp_offset_depth);
    json_object_set_int_member (params, "chroma qp offset list len_minus1",
        p->chroma_qp_offset_list_len_minus1);

    for (i = 0; i < 6; i++) {
      json_array_add_int_element (cb_qp_offset_list, p->cb_qp_offset_list[i]);
      json_array_add_int_element (cr_qp_offset_list, p->cr_qp_offset_list[i]);
    }
    json_object_set_array_member (params, "cb qp offset list",
        cb_qp_offset_list);
    json_object_set_array_member (params, "cr qp offset list",
        cr_qp_offset_list);

    json_object_set_int_member (params, "log2 sao offset scale luma",
        p->log2_sao_offset_scale_luma);
    json_object_set_int_member (params, "log2 sao offset scale chroma",
        p->log2_sao_offset_scale_chroma);

    json_object_set_object_member (pps, "pps extension params", params);
  }

  if (h265_pps.pps_scc_extension_flag) {
    GstH265PPSSccExtensionParams *p = &h265_pps.pps_scc_extension_params;
    JsonObject *params = json_object_new ();
    JsonArray *initializer = json_array_new ();

    json_object_set_boolean_member (params, "pps curr pic ref enabled flag",
        p->pps_curr_pic_ref_enabled_flag);
    json_object_set_boolean_member (params,
        "residual adaptive colour transform enabled flag",
        p->residual_adaptive_colour_transform_enabled_flag);
    json_object_set_boolean_member (params,
        "pps slice act qp offsets present flag",
        p->pps_slice_act_qp_offsets_present_flag);
    json_object_set_int_member (params, "pps act y qp offset plus5",
        p->pps_act_y_qp_offset_plus5);
    json_object_set_int_member (params, "pps act cb qp offset plus5",
        p->pps_act_cb_qp_offset_plus5);
    json_object_set_int_member (params, "pps act cr qp offset plus3",
        p->pps_act_cr_qp_offset_plus3);
    json_object_set_boolean_member (params,
        "pps palette predictor initializers present flag",
        p->pps_palette_predictor_initializers_present_flag);
    json_object_set_int_member (params, "pps num palette predictor initializer",
        p->pps_num_palette_predictor_initializer);
    json_object_set_boolean_member (params, "monochrome palette flag",
        p->monochrome_palette_flag);
    json_object_set_int_member (params, "luma bit depth entry minus8",
        p->luma_bit_depth_entry_minus8);
    json_object_set_int_member (params, "chroma bit depth entry minus8",
        p->chroma_bit_depth_entry_minus8);

    for (i = 0; i < 3; i++) {
      JsonArray *sub = json_array_new ();
      for (j = 0; j < 128; j++)
        json_array_add_int_element (sub,
            p->pps_palette_predictor_initializer[i][j]);
      json_array_add_array_element (initializer, sub);
    }
    json_object_set_array_member (params, "pps palette predictor initializer",
        initializer);
    json_object_set_object_member (pps, "pps scc extension_params", params);
  }

  json_object_set_object_member (json, "pps", pps);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_h265_2_json_parse_sei (GstH2652json * self, GstH265NalUnit * nalu)
{
  JsonObject *sei;
  GstH265ParserResult pres;
  JsonObject *json = self->json;
  GArray *messages = NULL;
  gint i;

  pres = gst_h265_parser_parse_sei (self->parser, nalu, &messages);
  if (pres != GST_H265_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to parse SEI, result %d", pres);

    /* XXX: Ignore error from SEI parsing, it might be malformed bitstream,
     * or our fault. But shouldn't be critical  */
    g_clear_pointer (&messages, g_array_unref);
    return GST_FLOW_OK;
  }

  sei = json_object_new ();

  GST_LOG_OBJECT (self, "SEI parsed");

  for (i = 0; i < messages->len; i++) {
    GstH265SEIMessage *m = &g_array_index (messages, GstH265SEIMessage, i);

    switch (m->payloadType) {
      case GST_H265_SEI_PIC_TIMING:
      {
        JsonObject *timing = json_object_new ();

        json_object_set_int_member (timing, "pic struct",
            m->payload.pic_timing.pic_struct);
        json_object_set_int_member (timing, "source scan type",
            m->payload.pic_timing.source_scan_type);
        json_object_set_boolean_member (timing, "duplicate flag",
            m->payload.pic_timing.duplicate_flag);
        json_object_set_object_member (sei, "timing", timing);
        break;
      }
      default:
        break;
    }
  }

  g_array_free (messages, TRUE);

  json_object_set_object_member (json, "sei", sei);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_h265_2_json_parse_slice (GstH2652json * self, GstH265NalUnit * nalu)
{
  GstH265ParserResult pres = GST_H265_PARSER_OK;
  GstH265SliceHdr slice_hdr;
  GstH265PPS *pps;
  GstH265SPS *sps;
  JsonObject *json = self->json;
  JsonObject *hdr, *ref_pic_list_modification, *pred_weight_table;
  JsonArray *lt_idx_sps;
  JsonArray *poc_lsb_lt;
  JsonArray *used_by_curr_pic_lt_flag;
  JsonArray *delta_poc_msb_present_flag;
  JsonArray *delta_poc_msb_cycle_lt;
  JsonArray *luma_weight_l0_flag;
  JsonArray *chroma_weight_l0_flag;
  JsonArray *delta_luma_weight_l0;
  JsonArray *luma_offset_l0;
  JsonArray *delta_chroma_weight_l0;
  JsonArray *delta_chroma_offset_l0;
  JsonArray *luma_weight_l1_flag;
  JsonArray *chroma_weight_l1_flag;
  JsonArray *delta_luma_weight_l1;
  JsonArray *luma_offset_l1;
  JsonArray *delta_chroma_weight_l1;
  JsonArray *delta_chroma_offset_l1;

  GstH265RefPicListModification *m;
  GstH265PredWeightTable *pwt;
  gint i, j;

  pres = gst_h265_parser_parse_slice_hdr (self->parser, nalu, &slice_hdr);

  if (pres != GST_H265_PARSER_OK) {
    GST_ERROR_OBJECT (self, "Failed to parse slice header, ret %d", pres);
    return GST_FLOW_ERROR;
  }

  pps = slice_hdr.pps;
  sps = pps->sps;

  hdr = json_object_new ();

  json_object_set_boolean_member (hdr, "dependent slice segment flag",
      slice_hdr.dependent_slice_segment_flag);
  json_object_set_int_member (hdr, "segment address",
      slice_hdr.segment_address);
  json_object_set_int_member (hdr, "type", slice_hdr.type);

  json_object_set_boolean_member (hdr, "pic output flag",
      slice_hdr.pic_output_flag);
  json_object_set_int_member (hdr, "colour plane id",
      slice_hdr.colour_plane_id);
  json_object_set_int_member (hdr, "pic_order_cnt_lsb",
      slice_hdr.pic_order_cnt_lsb);

  json_object_set_boolean_member (hdr, "short term ref pic set sps flag",
      slice_hdr.short_term_ref_pic_set_sps_flag);
  if (!slice_hdr.short_term_ref_pic_set_sps_flag) {
    JsonObject *st_rps = json_object_new ();

    json_object_set_boolean_member (st_rps, "inter ref pic set prediction flag",
        slice_hdr.short_term_ref_pic_sets.inter_ref_pic_set_prediction_flag);
    json_object_set_int_member (st_rps, "delta idx minus1",
        slice_hdr.short_term_ref_pic_sets.delta_idx_minus1);
    json_object_set_int_member (st_rps, "delta rps sign",
        slice_hdr.short_term_ref_pic_sets.delta_rps_sign);
    json_object_set_int_member (st_rps, "abs delta rps minus1",
        slice_hdr.short_term_ref_pic_sets.abs_delta_rps_minus1);

    json_object_set_object_member (hdr, "short term ref pic sets", st_rps);
  } else if (sps->num_short_term_ref_pic_sets > 1) {
    json_object_set_int_member (hdr, "short term ref pic set idx",
        slice_hdr.short_term_ref_pic_set_idx);
  }

  json_object_set_int_member (hdr, "num long term sps",
      slice_hdr.num_long_term_sps);
  json_object_set_int_member (hdr, "num long term pics",
      slice_hdr.num_long_term_pics);

  lt_idx_sps = json_array_new ();
  poc_lsb_lt = json_array_new ();
  used_by_curr_pic_lt_flag = json_array_new ();
  delta_poc_msb_present_flag = json_array_new ();
  delta_poc_msb_cycle_lt = json_array_new ();
  for (i = 0; i < 16; i++) {
    json_array_add_int_element (lt_idx_sps, slice_hdr.lt_idx_sps[i]);
    json_array_add_int_element (poc_lsb_lt, slice_hdr.poc_lsb_lt[i]);
    json_array_add_boolean_element (used_by_curr_pic_lt_flag,
        slice_hdr.used_by_curr_pic_lt_flag[i]);
    json_array_add_boolean_element (delta_poc_msb_present_flag,
        slice_hdr.delta_poc_msb_present_flag[i]);
    json_array_add_int_element (delta_poc_msb_cycle_lt,
        slice_hdr.delta_poc_msb_cycle_lt[i]);
  }
  json_object_set_array_member (hdr, "lt idx sps", lt_idx_sps);
  json_object_set_array_member (hdr, "poc lsb lt", poc_lsb_lt);
  json_object_set_array_member (hdr, "used by curr pic lt flag",
      used_by_curr_pic_lt_flag);
  json_object_set_array_member (hdr, "delta poc msb present flag",
      delta_poc_msb_present_flag);
  json_object_set_array_member (hdr, "delta poc msb cycle lt",
      delta_poc_msb_cycle_lt);

  json_object_set_boolean_member (hdr, "temporal mvp enabled flag",
      slice_hdr.temporal_mvp_enabled_flag);
  json_object_set_boolean_member (hdr, "sao luma flag",
      slice_hdr.sao_luma_flag);
  json_object_set_boolean_member (hdr, "sao chroma flag",
      slice_hdr.sao_chroma_flag);
  json_object_set_boolean_member (hdr, "num ref idx active override flag",
      slice_hdr.num_ref_idx_active_override_flag);
  json_object_set_int_member (hdr, "num ref idx l0 active minus1",
      slice_hdr.num_ref_idx_l0_active_minus1);
  json_object_set_int_member (hdr, "num ref idx l1 active minus1",
      slice_hdr.num_ref_idx_l1_active_minus1);

  m = &slice_hdr.ref_pic_list_modification;
  ref_pic_list_modification = json_object_new ();
  json_object_set_boolean_member (ref_pic_list_modification,
      "ref pic list modification flag l0",
      m->ref_pic_list_modification_flag_l0);
  if (m->ref_pic_list_modification_flag_l0) {
    JsonArray *list_entry_l0 = json_array_new ();

    for (i = 0; i < slice_hdr.num_ref_idx_l0_active_minus1 && i < 15; i++)
      json_array_add_int_element (list_entry_l0, m->list_entry_l0[i]);

    json_object_set_array_member (ref_pic_list_modification, "list entry l0",
        list_entry_l0);

    if (GST_H265_IS_B_SLICE (&slice_hdr)) {
      json_object_set_boolean_member (ref_pic_list_modification,
          "ref pic list modification flag l1",
          m->ref_pic_list_modification_flag_l1);

      if (m->ref_pic_list_modification_flag_l1) {
        JsonArray *list_entry_l1 = json_array_new ();

        for (i = 0; i < slice_hdr.num_ref_idx_l1_active_minus1 && i < 15; i++)
          json_array_add_int_element (list_entry_l1, m->list_entry_l1[i]);

        json_object_set_array_member (ref_pic_list_modification,
            "list entry l1", list_entry_l1);
      }
    }
  }
  json_object_set_object_member (hdr, "ref pic list modification",
      ref_pic_list_modification);

  json_object_set_boolean_member (hdr, "mvd l1 zero flag",
      slice_hdr.mvd_l1_zero_flag);
  json_object_set_boolean_member (hdr, "cabac init flag",
      slice_hdr.cabac_init_flag);
  json_object_set_boolean_member (hdr, "collocated from l0 flag",
      slice_hdr.collocated_from_l0_flag);
  json_object_set_int_member (hdr, "collocated ref idx",
      slice_hdr.collocated_ref_idx);

  pred_weight_table = json_object_new ();
  pwt = &slice_hdr.pred_weight_table;
  luma_weight_l0_flag = json_array_new ();
  chroma_weight_l0_flag = json_array_new ();
  delta_luma_weight_l0 = json_array_new ();
  luma_offset_l0 = json_array_new ();
  delta_chroma_weight_l0 = json_array_new ();
  delta_chroma_offset_l0 = json_array_new ();
  luma_weight_l1_flag = json_array_new ();
  chroma_weight_l1_flag = json_array_new ();
  delta_luma_weight_l1 = json_array_new ();
  luma_offset_l1 = json_array_new ();
  delta_chroma_weight_l1 = json_array_new ();
  delta_chroma_offset_l1 = json_array_new ();

  json_object_set_int_member (pred_weight_table, "luma log2 weight denom",
      pwt->luma_log2_weight_denom);
  json_object_set_int_member (pred_weight_table,
      "delta chroma log2 weight denom", pwt->delta_chroma_log2_weight_denom);
  for (i = 0; i < 15; i++) {
    json_array_add_boolean_element (luma_weight_l0_flag,
        pwt->luma_weight_l0_flag[i]);
    json_array_add_boolean_element (chroma_weight_l0_flag,
        pwt->chroma_weight_l0_flag[i]);
    json_array_add_int_element (delta_luma_weight_l0,
        pwt->delta_luma_weight_l0[i]);
    json_array_add_int_element (luma_offset_l0, pwt->luma_offset_l0[i]);
    for (j = 0; j < 2; j++) {
      json_array_add_int_element (delta_chroma_weight_l0,
          pwt->delta_chroma_weight_l0[i][j]);
      json_array_add_int_element (delta_chroma_offset_l0,
          pwt->delta_chroma_offset_l0[i][j]);
    }
    json_array_add_boolean_element (luma_weight_l1_flag,
        pwt->luma_weight_l1_flag[i]);
    json_array_add_boolean_element (chroma_weight_l1_flag,
        pwt->chroma_weight_l1_flag[i]);
    json_array_add_int_element (delta_luma_weight_l1,
        pwt->delta_luma_weight_l1[i]);
    json_array_add_int_element (luma_offset_l1, pwt->luma_offset_l1[i]);
    for (j = 0; j < 2; j++) {
      json_array_add_int_element (delta_chroma_weight_l1,
          pwt->delta_chroma_weight_l1[i][j]);
      json_array_add_int_element (delta_chroma_offset_l1,
          pwt->delta_chroma_offset_l1[i][j]);
    }
  }
  json_object_set_array_member (pred_weight_table, "luma weight l0 flag",
      luma_weight_l0_flag);
  json_object_set_array_member (pred_weight_table, "chroma weight l0 flag",
      chroma_weight_l0_flag);
  json_object_set_array_member (pred_weight_table, "delta luma weight l0",
      delta_luma_weight_l0);
  json_object_set_array_member (pred_weight_table, "luma offset l0",
      luma_offset_l0);
  json_object_set_array_member (pred_weight_table, "delta chroma weight l0",
      delta_chroma_weight_l0);
  json_object_set_array_member (pred_weight_table, "delta chroma offset l0",
      delta_chroma_offset_l0);
  json_object_set_array_member (pred_weight_table, "luma weight l1 flag",
      luma_weight_l1_flag);
  json_object_set_array_member (pred_weight_table, "chroma weight l1 flag",
      chroma_weight_l1_flag);
  json_object_set_array_member (pred_weight_table, "delta luma weight l1",
      delta_luma_weight_l1);
  json_object_set_array_member (pred_weight_table, "luma offset l1",
      luma_offset_l1);
  json_object_set_array_member (pred_weight_table, "delta chroma weight l1",
      delta_chroma_weight_l1);
  json_object_set_array_member (pred_weight_table, "delta chroma offset l1",
      delta_chroma_offset_l1);

  json_object_set_object_member (hdr, "pred weight table", pred_weight_table);

  json_object_set_int_member (hdr, "five minus max num merge cand",
      slice_hdr.five_minus_max_num_merge_cand);
  json_object_set_boolean_member (hdr, "use integer mv flag",
      slice_hdr.use_integer_mv_flag);

  json_object_set_int_member (hdr, "qp delta", slice_hdr.qp_delta);
  json_object_set_int_member (hdr, "cb qp offset", slice_hdr.cb_qp_offset);
  json_object_set_int_member (hdr, "cr qp offset", slice_hdr.cr_qp_offset);
  json_object_set_int_member (hdr, "slice act y qp offset",
      slice_hdr.slice_act_y_qp_offset);
  json_object_set_int_member (hdr, "slice act cb qp offset",
      slice_hdr.slice_act_cb_qp_offset);
  json_object_set_int_member (hdr, "slice act cr qp offset",
      slice_hdr.slice_act_cr_qp_offset);

  json_object_set_boolean_member (hdr, "cu chroma qp offset enabled flag",
      slice_hdr.cu_chroma_qp_offset_enabled_flag);

  json_object_set_boolean_member (hdr, "deblocking filter override flag",
      slice_hdr.deblocking_filter_override_flag);
  json_object_set_boolean_member (hdr, "deblocking filter disabled flag",
      slice_hdr.deblocking_filter_disabled_flag);
  json_object_set_int_member (hdr, "beta offset div2",
      slice_hdr.beta_offset_div2);
  json_object_set_int_member (hdr, "tc offset div2", slice_hdr.tc_offset_div2);

  json_object_set_boolean_member (hdr, "loop filter across slices enabled flag",
      slice_hdr.loop_filter_across_slices_enabled_flag);

  json_object_set_int_member (hdr, "num entry point offsets",
      slice_hdr.num_entry_point_offsets);
  if (slice_hdr.num_entry_point_offsets) {
    JsonArray *entry_point_offset_minus1 = json_array_new ();

    json_object_set_int_member (hdr, "offset len minus1",
        slice_hdr.offset_len_minus1);
    for (i = 0; i < slice_hdr.num_entry_point_offsets; i++)
      json_array_add_int_element (entry_point_offset_minus1,
          slice_hdr.entry_point_offset_minus1[i]);

    json_object_set_array_member (hdr, "entry point offset minus1",
        entry_point_offset_minus1);
  }

  json_object_set_object_member (json, "slice header", hdr);

  gst_h265_slice_hdr_free (&slice_hdr);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_h265_2_json_decode_nal (GstH2652json * self, GstH265NalUnit * nalu)
{
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (self, "Parsed nal type: %d, offset %d, size %d",
      nalu->type, nalu->offset, nalu->size);

  switch (nalu->type) {
    case GST_H265_NAL_VPS:
      ret = gst_h265_2_json_parse_vps (self, nalu);
      break;
    case GST_H265_NAL_SPS:
      ret = gst_h265_2_json_parse_sps (self, nalu);
      break;
    case GST_H265_NAL_PPS:
      ret = gst_h265_2_json_parse_pps (self, nalu);
      break;
    case GST_H265_NAL_PREFIX_SEI:
    case GST_H265_NAL_SUFFIX_SEI:
      ret = gst_h265_2_json_parse_sei (self, nalu);
      break;
    case GST_H265_NAL_SLICE_TRAIL_N:
    case GST_H265_NAL_SLICE_TRAIL_R:
    case GST_H265_NAL_SLICE_TSA_N:
    case GST_H265_NAL_SLICE_TSA_R:
    case GST_H265_NAL_SLICE_STSA_N:
    case GST_H265_NAL_SLICE_STSA_R:
    case GST_H265_NAL_SLICE_RADL_N:
    case GST_H265_NAL_SLICE_RADL_R:
    case GST_H265_NAL_SLICE_RASL_N:
    case GST_H265_NAL_SLICE_RASL_R:
    case GST_H265_NAL_SLICE_BLA_W_LP:
    case GST_H265_NAL_SLICE_BLA_W_RADL:
    case GST_H265_NAL_SLICE_BLA_N_LP:
    case GST_H265_NAL_SLICE_IDR_W_RADL:
    case GST_H265_NAL_SLICE_IDR_N_LP:
    case GST_H265_NAL_SLICE_CRA_NUT:
      ret = gst_h265_2_json_parse_slice (self, nalu);
      break;
    case GST_H265_NAL_EOB:
    case GST_H265_NAL_EOS:
    default:
      break;
  }

  return ret;
}

static GstFlowReturn
gst_h265_2_json_chain (GstPad * sinkpad, GstObject * object, GstBuffer * in_buf)
{
  GstH2652json *self = GST_H265_2_JSON (object);
  JsonObject *json = self->json;
  GstBuffer *out_buf;
  gchar *json_string;
  guint length;
  GstH265NalUnit nalu;
  GstH265ParserResult pres;
  GstMapInfo in_map, out_map;
  GstFlowReturn ret = GST_FLOW_OK;
  gint i;

  if (!gst_buffer_map (in_buf, &in_map, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Cannot map buffer");
    return GST_FLOW_ERROR;
  }

  if (self->use_hevc) {
    guint offset = 0;
    gsize consumed;

    do {
      pres = gst_h265_parser_identify_and_split_nalu_hevc (self->parser,
          in_map.data, offset, in_map.size, self->nal_length_size,
          self->split_nalu, &consumed);
      if (pres != GST_H265_PARSER_OK)
        break;

      for (i = 0; i < self->split_nalu->len; i++) {
        GstH265NalUnit *nl =
            &g_array_index (self->split_nalu, GstH265NalUnit, i);
        ret = gst_h265_2_json_decode_nal (self, nl);
        if (ret != GST_FLOW_OK)
          break;
      }

      if (pres != GST_H265_PARSER_OK)
        break;

      offset += consumed;
    } while (pres == GST_H265_PARSER_OK);
  } else {
    pres = gst_h265_parser_identify_nalu (self->parser,
        in_map.data, 0, in_map.size, &nalu);

    if (pres == GST_H265_PARSER_NO_NAL_END)
      pres = GST_H265_PARSER_OK;

    while (pres == GST_H265_PARSER_OK) {
      ret = gst_h265_2_json_decode_nal (self, &nalu);
      if (ret != GST_FLOW_OK)
        break;

      pres = gst_h265_parser_identify_nalu (self->parser,
          in_map.data, nalu.offset + nalu.size, in_map.size, &nalu);
      if (pres == GST_H265_PARSER_NO_NAL_END)
        pres = GST_H265_PARSER_OK;
    }
  }

  if (ret != GST_FLOW_OK)
    goto err;

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

err:
  gst_buffer_unmap (in_buf, &in_map);
  gst_buffer_unref (in_buf);

  return ret;
}

static GstFlowReturn
gst_h265_2_json_parse_codec_data (GstH2652json * self, const guint8 * data,
    gsize size)
{
  guint num_nal_arrays;
  guint off;
  guint num_nals, i, j;
  GstH265ParserResult pres;
  GstH265NalUnit nalu;

  /* parse the hvcC data */
  if (size < 23) {
    GST_WARNING_OBJECT (self, "hvcC too small");
    return GST_FLOW_ERROR;
  }

  /* wrong hvcC version */
  if (data[0] != 0 && data[0] != 1) {
    return GST_FLOW_ERROR;
  }

  self->nal_length_size = (data[21] & 0x03) + 1;
  GST_DEBUG_OBJECT (self, "nal length size %u", self->nal_length_size);

  num_nal_arrays = data[22];
  off = 23;

  for (i = 0; i < num_nal_arrays; i++) {
    if (off + 3 >= size) {
      GST_WARNING_OBJECT (self, "hvcC too small");
      return GST_FLOW_ERROR;
    }

    num_nals = GST_READ_UINT16_BE (data + off + 1);
    off += 3;
    for (j = 0; j < num_nals; j++) {
      GstFlowReturn ret;

      pres = gst_h265_parser_identify_nalu_hevc (self->parser,
          data, off, size, 2, &nalu);

      if (pres != GST_H265_PARSER_OK) {
        GST_WARNING_OBJECT (self, "hvcC too small");
        return GST_FLOW_ERROR;
      }

      ret = gst_h265_2_json_decode_nal (self, &nalu);
      if (ret != GST_FLOW_OK)
        return ret;

      off = nalu.offset + nalu.size;
    }
  }

  return GST_FLOW_OK;
}

static void
gst_h265_2_json_get_codec_data (GstH2652json * self, GstCaps * caps)
{
  if (caps && gst_caps_get_size (caps) > 0) {
    GstStructure *s = gst_caps_get_structure (caps, 0);

    if (gst_structure_has_field (s, "codec_data")) {
      GST_WARNING_OBJECT (self, "get codec-data");

      const GValue *h = gst_structure_get_value (s, "codec_data");
      GstBuffer *codec_data = gst_value_get_buffer (h);
      GstMapInfo map;

      gst_buffer_map (codec_data, &map, GST_MAP_READ);
      if (gst_h265_2_json_parse_codec_data (self, map.data,
              map.size) != GST_FLOW_OK) {
        /* keep going without error.
         * Probably inband SPS/PPS might be valid data */
        GST_WARNING_OBJECT (self, "Failed to handle codec data");
      }
      gst_buffer_unmap (codec_data, &map);
    }
  }
}

static void
gst_h265_2_json_use_hevc (GstH2652json * self, GstCaps * caps)
{
  if (caps && gst_caps_get_size (caps) > 0) {
    GstStructure *s = gst_caps_get_structure (caps, 0);
    const gchar *str_stream = NULL;

    str_stream = gst_structure_get_string (s, "stream-format");

    self->use_hevc = FALSE;
    if (str_stream && (g_strcmp0 (str_stream, "hvc1") == 0
            || g_strcmp0 (str_stream, "hev1") == 0)) {
      self->use_hevc = TRUE;
      return;
    }
  }
}

static gboolean
gst_h265_2_json_set_caps (GstH2652json * self, GstCaps * caps)
{
  GstCaps *src_caps =
      gst_caps_new_simple ("text/x-json", "format", G_TYPE_STRING, "h265",
      NULL);
  GstEvent *event;

  event = gst_event_new_caps (src_caps);
  gst_caps_unref (src_caps);

  gst_h265_2_json_use_hevc (self, caps);

  gst_h265_2_json_get_codec_data (self, caps);

  return gst_pad_push_event (self->srcpad, event);
}

static gboolean
gst_h265_2_json_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstH2652json *self = GST_H265_2_JSON (parent);
  gboolean res = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      res = gst_h265_2_json_set_caps (self, caps);
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
gst_h265_2_json_class_init (GstH2652jsonClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->finalize = gst_h265_2_json_finalize;

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gst_element_class_set_static_metadata (gstelement_class, "H2652json",
      "Transform",
      "H265 to json element",
      "Benjamin Gaignard <benjamin.gaignard@collabora.com>");
}

static void
gst_h265_2_json_init (GstH2652json * self)
{
  self->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_chain_function (self->sinkpad, gst_h265_2_json_chain);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_h265_2_json_sink_event));

  self->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  self->json = json_object_new ();

  self->parser = gst_h265_parser_new ();
  self->nal_length_size = 4;
  self->split_nalu = g_array_new (FALSE, FALSE, sizeof (GstH265NalUnit));
}
