/*
 * gsth2642json.c - H.264 parsed bistream to json
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
 * SECTION:element-h2642json
 * @title: h2642json
 *
 * Convert H.264 bitstream parameters to JSON formated text.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/h.264/file ! parsebin ! h2642json ! filesink location=/path/to/json/file
 * ```
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/base/base.h>
#include <json-glib/json-glib.h>

#include "gsth2642json.h"

GST_DEBUG_CATEGORY (gst_h264_2_json_debug);
#define GST_CAT_DEFAULT gst_h264_2_json_debug

struct _GstH2642json
{
  GstElement parent;

  GstPad *sinkpad, *srcpad;
  GstH264NalParser *parser;

  guint nal_length_size;

  gboolean use_avc;

  JsonObject *json;
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-json,format=h264"));

G_DEFINE_TYPE_WITH_CODE (GstH2642json, gst_h264_2_json,
    GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_h264_2_json_debug, "h2642json", 0,
        "H.264 to json"));

static void
gst_h264_2_json_finalize (GObject * object)
{
  GstH2642json *self = GST_H264_2_JSON (object);

  json_object_unref (self->json);
  gst_h264_nal_parser_free (self->parser);
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

static GstFlowReturn
gst_h264_2_json_parse_sps (GstH2642json * self, GstH264NalUnit * nalu)
{
  JsonObject *json = self->json;
  JsonObject *sps;
  JsonArray *scaling_lists_4x4, *scaling_lists_8x8, *offset_for_ref_frame;
  GstH264SPS h264_sps;
  GstH264ParserResult pres;
  GstFlowReturn ret = GST_FLOW_OK;
  gint i, j;

  pres = gst_h264_parse_sps (nalu, &h264_sps);
  if (pres != GST_H264_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to parse SPS, result %d", pres);
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (self, "SPS parsed");

  if (gst_h264_parser_update_sps (self->parser,
          &h264_sps) != GST_H264_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to update SPS");
    ret = GST_FLOW_ERROR;
    goto error;
  }

  sps = json_object_new ();

  json_object_set_int_member (sps, "id", h264_sps.id);
  json_object_set_int_member (sps, "profile idc", h264_sps.profile_idc);
  json_object_set_boolean_member (sps, "constraint set0 flag",
      h264_sps.constraint_set0_flag);
  json_object_set_boolean_member (sps, "constraint set1 flag",
      h264_sps.constraint_set1_flag);
  json_object_set_boolean_member (sps, "constraint set2 flag",
      h264_sps.constraint_set2_flag);
  json_object_set_boolean_member (sps, "constraint set3 flag",
      h264_sps.constraint_set3_flag);
  json_object_set_boolean_member (sps, "constraint set4 flag",
      h264_sps.constraint_set4_flag);
  json_object_set_boolean_member (sps, "constraint set5 flag",
      h264_sps.constraint_set5_flag);
  json_object_set_int_member (sps, "level idc", h264_sps.level_idc);
  json_object_set_int_member (sps, "chroma format idc",
      h264_sps.chroma_format_idc);
  json_object_set_boolean_member (sps, "separate colour plane flag",
      h264_sps.separate_colour_plane_flag);
  json_object_set_int_member (sps, "bit depth luma minus8",
      h264_sps.bit_depth_luma_minus8);
  json_object_set_int_member (sps, "bit depth chroma minus8",
      h264_sps.bit_depth_chroma_minus8);
  json_object_set_boolean_member (sps, "qpprime y zero transform bypass flag",
      h264_sps.qpprime_y_zero_transform_bypass_flag);

  json_object_set_boolean_member (sps, "scaling matrix present flag",
      h264_sps.scaling_matrix_present_flag);

  scaling_lists_4x4 = json_array_new ();
  for (i = 0; i < 6; i++)
    for (j = 0; j < 16; j++)
      json_array_add_int_element (scaling_lists_4x4,
          h264_sps.scaling_lists_4x4[i][j]);
  json_object_set_array_member (sps, "scaling lists 4x4", scaling_lists_4x4);

  scaling_lists_8x8 = json_array_new ();
  for (i = 0; i < 6; i++)
    for (j = 0; j < 64; j++)
      json_array_add_int_element (scaling_lists_8x8,
          h264_sps.scaling_lists_8x8[i][j]);
  json_object_set_array_member (sps, "scaling lists 8x8", scaling_lists_8x8);

  json_object_set_int_member (sps, "log2 max frame num minus4",
      h264_sps.log2_max_frame_num_minus4);
  json_object_set_int_member (sps, "pic order cnt type",
      h264_sps.pic_order_cnt_type);
  json_object_set_int_member (sps, "log2 max pic order cnt lsb minus4",
      h264_sps.log2_max_pic_order_cnt_lsb_minus4);
  json_object_set_boolean_member (sps, "delta pic order always zero flag",
      h264_sps.delta_pic_order_always_zero_flag);
  json_object_set_int_member (sps, "offset for non ref pic",
      h264_sps.offset_for_non_ref_pic);
  json_object_set_int_member (sps, "offset for top to bottom field",
      h264_sps.offset_for_top_to_bottom_field);
  json_object_set_int_member (sps, "num ref frames in pic order cnt cycle",
      h264_sps.num_ref_frames_in_pic_order_cnt_cycle);

  offset_for_ref_frame = json_array_new ();
  for (i = 0; i < 255; i++)
    json_array_add_int_element (offset_for_ref_frame,
        h264_sps.offset_for_ref_frame[i]);
  json_object_set_array_member (sps, "offset for ref frame",
      offset_for_ref_frame);

  json_object_set_int_member (sps, "max num ref frames",
      h264_sps.num_ref_frames);
  json_object_set_boolean_member (sps, "gaps in frame num value allowed flag",
      h264_sps.gaps_in_frame_num_value_allowed_flag);

  json_object_set_int_member (sps, "pic width in mbs minus1",
      h264_sps.pic_width_in_mbs_minus1);
  json_object_set_int_member (sps, "pic height in map units minus1",
      h264_sps.pic_height_in_map_units_minus1);
  json_object_set_boolean_member (sps, "frame mbs only flag",
      h264_sps.frame_mbs_only_flag);

  json_object_set_boolean_member (sps, "mb adaptive frame field flag",
      h264_sps.mb_adaptive_frame_field_flag);
  json_object_set_boolean_member (sps, "direct 8x8 inference flag",
      h264_sps.direct_8x8_inference_flag);
  json_object_set_boolean_member (sps, "frame cropping flag",
      h264_sps.frame_cropping_flag);

  json_object_set_int_member (sps, "frame crop left offset",
      h264_sps.frame_crop_left_offset);
  json_object_set_int_member (sps, "frame crop right offset",
      h264_sps.frame_crop_right_offset);
  json_object_set_int_member (sps, "frame crop top offset",
      h264_sps.frame_crop_top_offset);
  json_object_set_int_member (sps, "frame crop bottom offset",
      h264_sps.frame_crop_bottom_offset);

  json_object_set_boolean_member (sps, "vui parameters present flag",
      h264_sps.vui_parameters_present_flag);

  if (h264_sps.vui_parameters_present_flag) {
    JsonObject *vui = json_object_new ();
    GstH264VUIParams *vui_parameters = &h264_sps.vui_parameters;

    json_object_set_boolean_member (vui, "aspect ratio info present flag",
        vui_parameters->aspect_ratio_info_present_flag);
    json_object_set_int_member (vui, "aspect ratio idc",
        vui_parameters->aspect_ratio_idc);
    if (vui_parameters->aspect_ratio_idc == 255) {
      json_object_set_int_member (vui, "sar width", vui_parameters->sar_width);
      json_object_set_int_member (vui, "sar height",
          vui_parameters->sar_height);
    }

    json_object_set_boolean_member (vui, "overscan info present flag",
        vui_parameters->overscan_info_present_flag);
    if (vui_parameters->overscan_info_present_flag)
      json_object_set_boolean_member (vui, "overscan appropriate flag",
          vui_parameters->overscan_appropriate_flag);

    json_object_set_boolean_member (vui, "video signal type present flag",
        vui_parameters->video_signal_type_present_flag);
    json_object_set_int_member (vui, "video_format",
        vui_parameters->video_format);
    json_object_set_boolean_member (vui, "video_full_range_flag",
        vui_parameters->video_full_range_flag);
    json_object_set_boolean_member (vui, "colour description present flag",
        vui_parameters->colour_description_present_flag);
    json_object_set_int_member (vui, "colour primaries",
        vui_parameters->colour_primaries);
    json_object_set_int_member (vui, "transfer characteristics",
        vui_parameters->transfer_characteristics);
    json_object_set_int_member (vui, "matrix coefficients",
        vui_parameters->matrix_coefficients);
    json_object_set_boolean_member (vui, "chroma loc info present flag",
        vui_parameters->chroma_loc_info_present_flag);
    json_object_set_int_member (vui, "chroma sample loc type top field",
        vui_parameters->chroma_sample_loc_type_top_field);
    json_object_set_int_member (vui, "chroma sample loc type bottom field",
        vui_parameters->chroma_sample_loc_type_bottom_field);

    json_object_set_boolean_member (vui, "timing_info_present_flag",
        vui_parameters->timing_info_present_flag);
    if (vui_parameters->timing_info_present_flag) {
      json_object_set_int_member (vui, "num units in tick",
          vui_parameters->num_units_in_tick);
      json_object_set_int_member (vui, "time scale",
          vui_parameters->time_scale);
      json_object_set_boolean_member (vui, "fixed frame rate flag",
          vui_parameters->fixed_frame_rate_flag);
    }

    json_object_set_boolean_member (vui, "nal hrd parameters present flag",
        vui_parameters->nal_hrd_parameters_present_flag);
    if (vui_parameters->nal_hrd_parameters_present_flag) {
      JsonObject *nal_hrd_parameters = json_object_new ();
      JsonArray *bit_rate_value_minus1, *cpb_size_value_minus1, *cbr_flag;

      json_object_set_int_member (nal_hrd_parameters, "cpb cnt minus1",
          vui_parameters->nal_hrd_parameters.cpb_cnt_minus1);
      json_object_set_int_member (nal_hrd_parameters, "bit rate scale",
          vui_parameters->nal_hrd_parameters.bit_rate_scale);
      json_object_set_int_member (nal_hrd_parameters, "cpb size scale",
          vui_parameters->nal_hrd_parameters.cpb_size_scale);

      bit_rate_value_minus1 = json_array_new ();
      for (i = 0; i < 32; i++)
        json_array_add_int_element (bit_rate_value_minus1,
            vui_parameters->nal_hrd_parameters.bit_rate_value_minus1[i]);
      json_object_set_array_member (nal_hrd_parameters, "bit rate value minus1",
          bit_rate_value_minus1);

      cpb_size_value_minus1 = json_array_new ();
      for (i = 0; i < 32; i++)
        json_array_add_int_element (cpb_size_value_minus1,
            vui_parameters->nal_hrd_parameters.cpb_size_value_minus1[i]);
      json_object_set_array_member (nal_hrd_parameters, "cpb size value minus1",
          cpb_size_value_minus1);

      cbr_flag = json_array_new ();
      for (i = 0; i < 32; i++)
        json_array_add_boolean_element (cbr_flag,
            vui_parameters->nal_hrd_parameters.cbr_flag[i]);
      json_object_set_array_member (nal_hrd_parameters, "cbr flag", cbr_flag);

      json_object_set_int_member (nal_hrd_parameters,
          "initial cpb removal delay length minus1",
          vui_parameters->
          nal_hrd_parameters.initial_cpb_removal_delay_length_minus1);
      json_object_set_int_member (nal_hrd_parameters,
          "cpb removal delay length minus1",
          vui_parameters->nal_hrd_parameters.cpb_removal_delay_length_minus1);
      json_object_set_int_member (nal_hrd_parameters,
          "dpb output delay length minus1",
          vui_parameters->nal_hrd_parameters.dpb_output_delay_length_minus1);
      json_object_set_int_member (nal_hrd_parameters, "time offset length",
          vui_parameters->nal_hrd_parameters.time_offset_length);

      json_object_set_object_member (vui, "nal hrd parameters",
          nal_hrd_parameters);
    }

    json_object_set_boolean_member (vui, "vcl_hrd_parameters_present_flag",
        vui_parameters->vcl_hrd_parameters_present_flag);
    if (vui_parameters->vcl_hrd_parameters_present_flag) {
      JsonObject *vcl_hrd_parameters = json_object_new ();
      JsonArray *bit_rate_value_minus1, *cpb_size_value_minus1, *cbr_flag;

      json_object_set_int_member (vcl_hrd_parameters, "cpb cnt minus1",
          vui_parameters->vcl_hrd_parameters.cpb_cnt_minus1);
      json_object_set_int_member (vcl_hrd_parameters, "bit rate scale",
          vui_parameters->vcl_hrd_parameters.bit_rate_scale);
      json_object_set_int_member (vcl_hrd_parameters, "cpb size scale",
          vui_parameters->vcl_hrd_parameters.cpb_size_scale);

      bit_rate_value_minus1 = json_array_new ();
      for (i = 0; i < 32; i++)
        json_array_add_int_element (bit_rate_value_minus1,
            vui_parameters->vcl_hrd_parameters.bit_rate_value_minus1[i]);
      json_object_set_array_member (vcl_hrd_parameters, "bit rate value minus1",
          bit_rate_value_minus1);

      cpb_size_value_minus1 = json_array_new ();
      for (i = 0; i < 32; i++)
        json_array_add_int_element (cpb_size_value_minus1,
            vui_parameters->vcl_hrd_parameters.cpb_size_value_minus1[i]);
      json_object_set_array_member (vcl_hrd_parameters, "cpb size value minus1",
          cpb_size_value_minus1);

      cbr_flag = json_array_new ();
      for (i = 0; i < 32; i++)
        json_array_add_boolean_element (cbr_flag,
            vui_parameters->vcl_hrd_parameters.cbr_flag[i]);
      json_object_set_array_member (vcl_hrd_parameters, "cbr flag", cbr_flag);

      json_object_set_int_member (vcl_hrd_parameters,
          "initial cpb removal delay length minus1",
          vui_parameters->
          vcl_hrd_parameters.initial_cpb_removal_delay_length_minus1);
      json_object_set_int_member (vcl_hrd_parameters,
          "cpb removal delay length minus1",
          vui_parameters->vcl_hrd_parameters.cpb_removal_delay_length_minus1);
      json_object_set_int_member (vcl_hrd_parameters,
          "dpb output delay length minus1",
          vui_parameters->vcl_hrd_parameters.dpb_output_delay_length_minus1);
      json_object_set_int_member (vcl_hrd_parameters, "time offset length",
          vui_parameters->vcl_hrd_parameters.time_offset_length);

      json_object_set_object_member (vui, "vcl hrd parameters",
          vcl_hrd_parameters);
    }

    json_object_set_boolean_member (vui, "low delay hrd flag",
        vui_parameters->low_delay_hrd_flag);
    json_object_set_boolean_member (vui, "pic struct present flag",
        vui_parameters->pic_struct_present_flag);

    json_object_set_boolean_member (vui, "bitstream restriction flag",
        vui_parameters->bitstream_restriction_flag);
    if (vui_parameters->bitstream_restriction_flag) {
      json_object_set_boolean_member (vui,
          "motion vectors over pic boundaries flag",
          vui_parameters->motion_vectors_over_pic_boundaries_flag);
      json_object_set_int_member (vui, "max bytes per pic denom",
          vui_parameters->max_bytes_per_pic_denom);
      json_object_set_int_member (vui, "max bits per mb denom",
          vui_parameters->max_bits_per_mb_denom);
      json_object_set_int_member (vui, "log2 max mv length horizontal",
          vui_parameters->log2_max_mv_length_horizontal);
      json_object_set_int_member (vui, "log2 max mv length vertical",
          vui_parameters->log2_max_mv_length_vertical);
      json_object_set_int_member (vui, "num reorder frames",
          vui_parameters->num_reorder_frames);
      json_object_set_int_member (vui, "max dec frame buffering",
          vui_parameters->max_dec_frame_buffering);
    }
    json_object_set_object_member (sps, "VUI params", vui);
  }

  json_object_set_int_member (sps, "extension type", h264_sps.extension_type);
  json_object_set_object_member (json, "sps", sps);

error:
  gst_h264_sps_clear (&h264_sps);

  return ret;
}

static GstFlowReturn
gst_h264_2_json_parse_pps (GstH2642json * self, GstH264NalUnit * nalu)
{
  GstH264PPS h264_pps;
  JsonObject *pps;
  GstH264ParserResult pres;
  JsonObject *json = self->json;
  GstFlowReturn ret = GST_FLOW_OK;
  JsonArray *scaling_lists_4x4, *scaling_lists_8x8;
  gint i, j;

  pres = gst_h264_parse_pps (self->parser, nalu, &h264_pps);
  if (pres != GST_H264_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to parse PPS, result %d", pres);
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (self, "PPS parsed");

  if (h264_pps.num_slice_groups_minus1 > 0) {
    GST_FIXME_OBJECT (self, "FMO is not supported");
    ret = GST_FLOW_ERROR;
    goto error;
  } else if (gst_h264_parser_update_pps (self->parser,
          &h264_pps) != GST_H264_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to update PPS");
    ret = GST_FLOW_ERROR;
    goto error;
  }

  pps = json_object_new ();

  json_object_set_boolean_member (pps, "entropy coding mode flag",
      h264_pps.entropy_coding_mode_flag);
  json_object_set_boolean_member (pps, "pic order present flag",
      h264_pps.pic_order_present_flag);

  json_object_set_int_member (pps, "num slice groups minus1",
      h264_pps.num_slice_groups_minus1);
  if (h264_pps.num_slice_groups_minus1 > 0) {
    json_object_set_int_member (pps, "slice group map type",
        h264_pps.slice_group_map_type);
    switch (h264_pps.slice_group_map_type) {
      case 0:
      {
        JsonArray *run_length_minus1 = json_array_new ();

        for (i = 0; i < 8; i++)
          json_array_add_int_element (run_length_minus1,
              h264_pps.run_length_minus1[i]);
        json_object_set_array_member (pps, "run lengthminus1",
            run_length_minus1);
        break;
      }
      case 2:
      {
        JsonArray *top_left = json_array_new ();
        JsonArray *bottom_right = json_array_new ();
        for (i = 0; i < 8; i++) {
          json_array_add_int_element (top_left, h264_pps.top_left[i]);
          json_array_add_int_element (bottom_right, h264_pps.bottom_right[i]);
        }
        json_object_set_array_member (pps, "top left", top_left);
        json_object_set_array_member (pps, "bottom right", bottom_right);
        break;
      }
      case 3:
      case 4:
      case 5:
      {
        json_object_set_boolean_member (pps,
            "slice group change direction flag",
            h264_pps.slice_group_change_direction_flag);
        json_object_set_int_member (pps, "slice group change rate minus1",
            h264_pps.slice_group_change_rate_minus1);
        break;
      }
      case 6:
      {
        json_object_set_int_member (pps, "pic size in map units minus1",
            h264_pps.pic_size_in_map_units_minus1);
        break;
      }
    }
  }

  json_object_set_int_member (pps, "num ref idx l0 default active minus1",
      h264_pps.num_ref_idx_l0_active_minus1);
  json_object_set_int_member (pps, "num ref idx l1 default active minus1",
      h264_pps.num_ref_idx_l1_active_minus1);
  json_object_set_boolean_member (pps, "weighted pred flag",
      h264_pps.weighted_pred_flag);
  json_object_set_int_member (pps, "weighted bipred idc",
      h264_pps.weighted_bipred_idc);
  json_object_set_int_member (pps, "pic init qp minus26",
      h264_pps.pic_init_qp_minus26);
  json_object_set_int_member (pps, "pic init qs minus26",
      h264_pps.pic_init_qs_minus26);
  json_object_set_int_member (pps, "chroma qp index offset",
      h264_pps.chroma_qp_index_offset);
  json_object_set_boolean_member (pps, "deblocking filter control present flag",
      h264_pps.deblocking_filter_control_present_flag);
  json_object_set_boolean_member (pps, "constrained intra pred flag",
      h264_pps.constrained_intra_pred_flag);
  json_object_set_boolean_member (pps, "redundant pic cnt present flag",
      h264_pps.redundant_pic_cnt_present_flag);

  json_object_set_boolean_member (pps, "transform 8x8 mode flag",
      h264_pps.transform_8x8_mode_flag);

  json_object_set_int_member (pps, "second chroma qp index offset",
      h264_pps.second_chroma_qp_index_offset);
  json_object_set_boolean_member (pps, "pic scaling matrix present flag",
      h264_pps.pic_scaling_matrix_present_flag);

  scaling_lists_4x4 = json_array_new ();
  for (i = 0; i < 6; i++)
    for (j = 0; j < 16; j++)
      json_array_add_int_element (scaling_lists_4x4,
          h264_pps.scaling_lists_4x4[i][j]);
  json_object_set_array_member (pps, "scaling lists 4x4", scaling_lists_4x4);

  scaling_lists_8x8 = json_array_new ();
  for (i = 0; i < 6; i++)
    for (j = 0; j < 64; j++)
      json_array_add_int_element (scaling_lists_8x8,
          h264_pps.scaling_lists_8x8[i][j]);
  json_object_set_array_member (pps, "scaling lists 8x8", scaling_lists_8x8);

  json_object_set_object_member (json, "pps", pps);

error:
  gst_h264_pps_clear (&h264_pps);

  return ret;
}

static GstFlowReturn
gst_h264_2_json_parse_slice (GstH2642json * self, GstH264NalUnit * nalu)
{
  GstH264SliceHdr slice;
  GstH264PPS *pps;
  GstH264SPS *sps;
  GstH264ParserResult pres = GST_H264_PARSER_OK;
  JsonObject *json = self->json;
  JsonArray *delta_pic_order_cnt, *ref_pic_list_modification_l0,
      *ref_pic_list_modification_l1, *luma_weight_l0, *luma_offset_l0;
  JsonObject *hdr, *pred_weight_table;
  gint i, j;

  pres =
      gst_h264_parser_parse_slice_hdr (self->parser, nalu, &slice, TRUE, TRUE);

  if (pres != GST_H264_PARSER_OK) {
    GST_ERROR_OBJECT (self, "Failed to parse slice header, ret %d", pres);
    return GST_FLOW_ERROR;
  }

  pps = slice.pps;
  sps = pps->sequence;

  hdr = json_object_new ();

  json_object_set_int_member (hdr, "first mb in slice",
      slice.first_mb_in_slice);
  json_object_set_int_member (hdr, "type", slice.type);

  if (sps->separate_colour_plane_flag)
    json_object_set_int_member (hdr, "colour plane id", slice.colour_plane_id);

  json_object_set_int_member (hdr, "frame num", slice.frame_num);

  json_object_set_boolean_member (hdr, "field pic flag", slice.field_pic_flag);
  json_object_set_boolean_member (hdr, "bottom field flag",
      slice.bottom_field_flag);

  if (nalu->type == GST_H264_NAL_SLICE_IDR)
    json_object_set_int_member (hdr, "idr pic id", slice.idr_pic_id);

  if (sps->pic_order_cnt_type == 0)
    json_object_set_int_member (hdr, "pic order cnt lsb",
        slice.pic_order_cnt_lsb);

  if (pps->pic_order_present_flag && !slice.field_pic_flag)
    json_object_set_int_member (hdr, "delta pic order cnt bottom",
        slice.delta_pic_order_cnt_bottom);

  delta_pic_order_cnt = json_array_new ();
  for (i = 0; i < 2; i++)
    json_array_add_int_element (delta_pic_order_cnt,
        slice.delta_pic_order_cnt[i]);
  json_object_set_array_member (hdr, "delta pic order cnt",
      delta_pic_order_cnt);

  json_object_set_int_member (hdr, "redundant pic cnt",
      slice.redundant_pic_cnt);

  if (GST_H264_IS_B_SLICE (&slice))
    json_object_set_boolean_member (hdr, "direct spatial mv pred flag",
        slice.direct_spatial_mv_pred_flag);

  json_object_set_int_member (hdr, "num ref idx l0 active minus1",
      slice.num_ref_idx_l0_active_minus1);
  json_object_set_int_member (hdr, "num ref idx l1 active minus1",
      slice.num_ref_idx_l1_active_minus1);

  json_object_set_int_member (hdr, "ref pic list modification flag l0",
      slice.ref_pic_list_modification_flag_l0);
  json_object_set_int_member (hdr, "n ref pic list modification l0",
      slice.n_ref_pic_list_modification_l0);
  ref_pic_list_modification_l0 = json_array_new ();
  for (i = 0; i < 32; i++) {
    JsonObject *modification = json_object_new ();

    json_object_set_int_member (modification, "modification of pic nums idc",
        slice.ref_pic_list_modification_l0[i].modification_of_pic_nums_idc);
    switch (slice.ref_pic_list_modification_l0[i].modification_of_pic_nums_idc) {
      case 0:
      case 1:
      {
        json_object_set_int_member (modification, "abs diff pic num minus1",
            slice.ref_pic_list_modification_l0[i].
            value.abs_diff_pic_num_minus1);
        break;
      }
      case 2:
      {
        json_object_set_int_member (modification, "long term pic num",
            slice.ref_pic_list_modification_l0[i].value.long_term_pic_num);
        break;
      }
      case 4:
      case 5:
      {
        json_object_set_int_member (modification, "abs diff view idx minus1",
            slice.ref_pic_list_modification_l0[i].
            value.abs_diff_view_idx_minus1);
        break;
      }
    }
    json_array_add_object_element (ref_pic_list_modification_l0, modification);
  }
  json_object_set_array_member (hdr, "ref pic list modification l0",
      ref_pic_list_modification_l0);

  json_object_set_int_member (hdr, "ref pic list modification flag l0",
      slice.ref_pic_list_modification_flag_l1);
  json_object_set_int_member (hdr, "n ref pic list modification l0",
      slice.n_ref_pic_list_modification_l1);
  ref_pic_list_modification_l1 = json_array_new ();
  for (i = 0; i < 32; i++) {
    JsonObject *modification = json_object_new ();

    json_object_set_int_member (modification, "modification of pic nums idc",
        slice.ref_pic_list_modification_l1[i].modification_of_pic_nums_idc);
    switch (slice.ref_pic_list_modification_l1[i].modification_of_pic_nums_idc) {
      case 0:
      case 1:
      {
        json_object_set_int_member (modification, "abs diff pic num minus1",
            slice.ref_pic_list_modification_l1[i].
            value.abs_diff_pic_num_minus1);
        break;
      }
      case 2:
      {
        json_object_set_int_member (modification, "long term pic num",
            slice.ref_pic_list_modification_l1[i].value.long_term_pic_num);
        break;
      }
      case 4:
      case 5:
      {
        json_object_set_int_member (modification, "abs diff view idx minus1",
            slice.ref_pic_list_modification_l1[i].
            value.abs_diff_view_idx_minus1);
        break;
      }
    }
    json_array_add_object_element (ref_pic_list_modification_l1, modification);
  }
  json_object_set_array_member (hdr, "ref pic list modification l1",
      ref_pic_list_modification_l1);

  pred_weight_table = json_object_new ();
  json_object_set_int_member (pred_weight_table, "luma log2 weight denom",
      slice.pred_weight_table.luma_log2_weight_denom);
  json_object_set_int_member (pred_weight_table, "chroma log2 weight denom",
      slice.pred_weight_table.chroma_log2_weight_denom);

  luma_weight_l0 = json_array_new ();
  luma_offset_l0 = json_array_new ();
  for (i = 0; i < 32; i++) {
    json_array_add_int_element (luma_weight_l0,
        slice.pred_weight_table.luma_weight_l0[i]);
    json_array_add_int_element (luma_offset_l0,
        slice.pred_weight_table.luma_offset_l0[i]);
  }
  json_object_set_array_member (pred_weight_table, "luma weight l0",
      luma_weight_l0);
  json_object_set_array_member (pred_weight_table, "luma offset l0",
      luma_offset_l0);

  if (sps->chroma_array_type != 0) {
    JsonArray *chroma_weight_l0 = json_array_new ();
    JsonArray *chroma_offset_l0 = json_array_new ();

    for (i = 0; i < 32; i++) {
      for (j = 0; j < 2; j++) {
        json_array_add_int_element (chroma_weight_l0,
            slice.pred_weight_table.chroma_weight_l0[i][j]);
        json_array_add_int_element (chroma_offset_l0,
            slice.pred_weight_table.chroma_offset_l0[i][j]);
      }
    }
    json_object_set_array_member (pred_weight_table, "chroma weight l0",
        chroma_weight_l0);
    json_object_set_array_member (pred_weight_table, "chroma offset l0",
        chroma_offset_l0);
  }

  if (GST_H264_IS_B_SLICE (&slice)) {
    JsonArray *luma_weight_l1 = json_array_new ();
    JsonArray *luma_offset_l1 = json_array_new ();
    for (i = 0; i < 32; i++) {
      json_array_add_int_element (luma_weight_l1,
          slice.pred_weight_table.luma_weight_l1[i]);
      json_array_add_int_element (luma_offset_l1,
          slice.pred_weight_table.luma_offset_l1[i]);
    }
    json_object_set_array_member (pred_weight_table, "luma weight l1",
        luma_weight_l1);
    json_object_set_array_member (pred_weight_table, "luma offset l1",
        luma_offset_l1);

    if (sps->chroma_array_type != 0) {
      JsonArray *chroma_weight_l1 = json_array_new ();
      JsonArray *chroma_offset_l1 = json_array_new ();

      for (i = 0; i < 32; i++) {
        for (j = 0; j < 2; j++) {
          json_array_add_int_element (chroma_weight_l1,
              slice.pred_weight_table.chroma_weight_l1[i][j]);
          json_array_add_int_element (chroma_offset_l1,
              slice.pred_weight_table.chroma_offset_l1[i][j]);
        }
      }
      json_object_set_array_member (pred_weight_table, "chroma weight l1",
          chroma_weight_l1);
      json_object_set_array_member (pred_weight_table, "chroma offset l1",
          chroma_offset_l1);
    }
  }
  json_object_set_object_member (hdr, "pred weight table", pred_weight_table);

  if (nalu->ref_idc != 0) {
    JsonObject *dec_ref_pic_marking = json_object_new ();
    JsonArray *ref_pic_marking = json_array_new ();

    if (nalu->idr_pic_flag) {
      json_object_set_boolean_member (dec_ref_pic_marking,
          "no output of prior pics flag",
          slice.dec_ref_pic_marking.no_output_of_prior_pics_flag);
      json_object_set_boolean_member (dec_ref_pic_marking,
          "long term reference flag",
          slice.dec_ref_pic_marking.long_term_reference_flag);
    }
    json_object_set_boolean_member (dec_ref_pic_marking,
        "adaptive ref pic marking mode flag",
        slice.dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag);

    for (i = 0; i < 10; i++) {
      GstH264RefPicMarking *m = &slice.dec_ref_pic_marking.ref_pic_marking[i];
      JsonObject *marking = json_object_new ();

      json_object_set_int_member (marking,
          "memory management control operation",
          m->memory_management_control_operation);
      json_object_set_int_member (marking, "difference of pic nums minus1",
          m->difference_of_pic_nums_minus1);
      json_object_set_int_member (marking, "long term pic num",
          m->long_term_pic_num);
      json_object_set_int_member (marking, "long term frame idx",
          m->long_term_frame_idx);
      json_object_set_int_member (marking, "max long term frame idx plus1",
          m->max_long_term_frame_idx_plus1);
      json_array_add_object_element (ref_pic_marking, marking);
    }
    json_object_set_array_member (dec_ref_pic_marking, "ref pic marking",
        ref_pic_marking);

    json_object_set_int_member (dec_ref_pic_marking, "n ref pic marking",
        slice.dec_ref_pic_marking.n_ref_pic_marking);
    json_object_set_int_member (dec_ref_pic_marking, "bit size",
        slice.dec_ref_pic_marking.bit_size);

    json_object_set_object_member (hdr, "dec ref pic marking",
        dec_ref_pic_marking);
  }

  json_object_set_int_member (hdr, "cabac init idc", slice.cabac_init_idc);
  json_object_set_int_member (hdr, "slice qp delta", slice.slice_qp_delta);
  json_object_set_int_member (hdr, "slice qs delta", slice.slice_qs_delta);
  json_object_set_int_member (hdr, "disable deblocking filter idc",
      slice.disable_deblocking_filter_idc);
  json_object_set_int_member (hdr, "slice alpha c0 offset div2",
      slice.slice_alpha_c0_offset_div2);
  json_object_set_int_member (hdr, "slice beta offset div2",
      slice.slice_beta_offset_div2);
  json_object_set_int_member (hdr, "slice group change cycle",
      slice.slice_group_change_cycle);

  json_object_set_object_member (json, "slice header", hdr);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_h264_2_json_decode_nal (GstH2642json * self, GstH264NalUnit * nalu)
{
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (self, "Parsed nal type: %d, offset %d, size %d",
      nalu->type, nalu->offset, nalu->size);

  switch (nalu->type) {
    case GST_H264_NAL_SPS:
      ret = gst_h264_2_json_parse_sps (self, nalu);
      break;
    case GST_H264_NAL_PPS:
      ret = gst_h264_2_json_parse_pps (self, nalu);
      break;
    case GST_H264_NAL_SLICE:
    case GST_H264_NAL_SLICE_DPA:
    case GST_H264_NAL_SLICE_DPB:
    case GST_H264_NAL_SLICE_DPC:
    case GST_H264_NAL_SLICE_IDR:
    case GST_H264_NAL_SLICE_EXT:
      ret = gst_h264_2_json_parse_slice (self, nalu);
      break;
    default:
      break;
  }

  return ret;
}

static GstFlowReturn
gst_h264_2_json_chain (GstPad * sinkpad, GstObject * object, GstBuffer * in_buf)
{
  GstH2642json *self = GST_H264_2_JSON (object);
  JsonObject *json = self->json;
  GstBuffer *out_buf;
  gchar *json_string;
  guint length;
  GstH264NalUnit nalu;
  GstH264ParserResult pres;
  GstMapInfo in_map, out_map;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!gst_buffer_map (in_buf, &in_map, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Cannot map buffer");
    return GST_FLOW_ERROR;
  }

  if (self->use_avc) {
    pres = gst_h264_parser_identify_nalu_avc (self->parser,
        in_map.data, 0, in_map.size, self->nal_length_size, &nalu);

    while (pres == GST_H264_PARSER_OK && ret == GST_FLOW_OK) {
      ret = gst_h264_2_json_decode_nal (self, &nalu);

      pres = gst_h264_parser_identify_nalu_avc (self->parser,
          in_map.data, nalu.offset + nalu.size, in_map.size,
          self->nal_length_size, &nalu);
    }
  } else {
    pres = gst_h264_parser_identify_nalu (self->parser,
        in_map.data, 0, in_map.size, &nalu);

    if (pres == GST_H264_PARSER_NO_NAL_END)
      pres = GST_H264_PARSER_OK;

    while (pres == GST_H264_PARSER_OK && ret == GST_FLOW_OK) {
      ret = gst_h264_2_json_decode_nal (self, &nalu);

      pres = gst_h264_parser_identify_nalu (self->parser,
          in_map.data, nalu.offset + nalu.size, in_map.size, &nalu);

      if (pres == GST_H264_PARSER_NO_NAL_END)
        pres = GST_H264_PARSER_OK;
    }
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

  gst_buffer_unmap (in_buf, &in_map);
  gst_buffer_unref (in_buf);

  return ret;
}

static GstFlowReturn
gst_h264_2_json_parse_codec_data (GstH2642json * self, const guint8 * data,
    gsize size)
{
  GstH264DecoderConfigRecord *config = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  GstH264NalUnit *nalu;
  guint i;

  if (gst_h264_parser_parse_decoder_config_record (self->parser, data, size,
          &config) != GST_H264_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to parse codec-data");
    return GST_FLOW_ERROR;
  }

  self->nal_length_size = config->length_size_minus_one + 1;
  for (i = 0; i < config->sps->len; i++) {
    nalu = &g_array_index (config->sps, GstH264NalUnit, i);

    /* TODO: handle subset sps for SVC/MVC. That would need to be stored in
     * separate array instead of putting SPS/subset-SPS into a single array */
    if (nalu->type != GST_H264_NAL_SPS)
      continue;

    ret = gst_h264_2_json_parse_sps (self, nalu);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "Failed to parse SPS");
      goto out;
    }
  }

  for (i = 0; i < config->pps->len; i++) {
    nalu = &g_array_index (config->pps, GstH264NalUnit, i);
    if (nalu->type != GST_H264_NAL_PPS)
      continue;

    ret = gst_h264_2_json_parse_pps (self, nalu);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "Failed to parse PPS");
      goto out;
    }
  }

out:
  gst_h264_decoder_config_record_free (config);
  return ret;
}

static void
gst_h264_2_json_get_codec_data (GstH2642json * self, GstCaps * caps)
{
  if (caps && gst_caps_get_size (caps) > 0) {
    GstStructure *s = gst_caps_get_structure (caps, 0);

    if (gst_structure_has_field (s, "codec_data")) {
      GST_WARNING_OBJECT (self, "get codec-data");

      const GValue *h = gst_structure_get_value (s, "codec_data");
      GstBuffer *codec_data = gst_value_get_buffer (h);
      GstMapInfo map;

      gst_buffer_map (codec_data, &map, GST_MAP_READ);
      if (gst_h264_2_json_parse_codec_data (self, map.data,
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
gst_h264_2_json_use_avc (GstH2642json * self, GstCaps * caps)
{
  if (caps && gst_caps_get_size (caps) > 0) {
    GstStructure *s = gst_caps_get_structure (caps, 0);
    const gchar *str_stream = NULL;

    str_stream = gst_structure_get_string (s, "stream-format");

    self->use_avc = FALSE;
    if (str_stream && (g_strcmp0 (str_stream, "avc") == 0
            || g_strcmp0 (str_stream, "avc3") == 0)) {
      self->use_avc = TRUE;
      return;
    }
  }
}

static gboolean
gst_h264_2_json_set_caps (GstH2642json * self, GstCaps * caps)
{
  GstCaps *src_caps =
      gst_caps_new_simple ("text/x-json", "format", G_TYPE_STRING, "h264",
      NULL);
  GstEvent *event;

  event = gst_event_new_caps (src_caps);
  gst_caps_unref (src_caps);

  gst_h264_2_json_use_avc (self, caps);

  gst_h264_2_json_get_codec_data (self, caps);

  return gst_pad_push_event (self->srcpad, event);
}

static gboolean
gst_h264_2_json_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstH2642json *self = GST_H264_2_JSON (parent);
  gboolean res = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      res = gst_h264_2_json_set_caps (self, caps);
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
gst_h264_2_json_class_init (GstH2642jsonClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->finalize = gst_h264_2_json_finalize;

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gst_element_class_set_static_metadata (gstelement_class, "H2642json",
      "Transform",
      "H264 to json element",
      "Benjamin Gaignard <benjamin.gaignard@collabora.com>");
}

static void
gst_h264_2_json_init (GstH2642json * self)
{
  self->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_chain_function (self->sinkpad, gst_h264_2_json_chain);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_h264_2_json_sink_event));

  self->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  self->json = json_object_new ();

  self->parser = gst_h264_nal_parser_new ();
  self->use_avc = FALSE;
  self->nal_length_size = 4;
}
