/* GStreamer
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 *               2001 Steve Baker <stevebaker_org@yahoo.co.uk>
 *               2003 Andy Wingo <wingo at pobox.com>
 *               2016 Thibault Saunier <thibault.saunier@collabora.com>
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

#ifndef __GST_LV2_H__
#define __GST_LV2_H__

#include <lilv/lilv.h>
#include <gst/gst.h>

#include "gstlv2utils.h"

G_GNUC_INTERNAL extern LilvWorld *gst_lv2_world_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_audio_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_control_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_cv_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_event_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_input_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_output_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_preset_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_state_iface_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_state_uri_node;

G_GNUC_INTERNAL extern LilvNode *gst_lv2_integer_prop_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_toggled_prop_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_designation_pred_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_in_place_broken_pred_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_optional_pred_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_group_pred_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_supports_event_pred_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_label_pred_node;

G_GNUC_INTERNAL extern LilvNode *gst_lv2_center_role_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_left_role_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_right_role_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_rear_center_role_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_rear_left_role_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_rear_right_role_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_lfe_role_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_center_left_role_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_center_right_role_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_side_left_role_node;
G_GNUC_INTERNAL extern LilvNode *gst_lv2_side_right_role_node;

G_GNUC_INTERNAL extern GstStructure *lv2_meta_all;

void gst_lv2_filter_register_element (GstPlugin *plugin,
                                      GstStructure * lv2_meta);
void gst_lv2_source_register_element (GstPlugin *plugin,
                                      GstStructure * lv2_meta);
#endif /* __GST_LV2_H__ */
