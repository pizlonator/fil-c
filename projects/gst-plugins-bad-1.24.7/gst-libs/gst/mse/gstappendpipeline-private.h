/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2016, 2017 Metrological Group B.V.
 * Copyright (C) 2016, 2017 Igalia S.L
 * Copyright (C) 2022, 2023 Collabora Ltd.
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

#pragma once

#include <gst/gst.h>
#include <glib-object.h>
#include <gst/mse/mse-prelude.h>
#include <gst/mse/gstmediasourcetrack-private.h>

G_BEGIN_DECLS

#define GST_TYPE_APPEND_PIPELINE (gst_append_pipeline_get_type())

GST_MSE_PRIVATE
G_DECLARE_FINAL_TYPE (GstAppendPipeline, gst_append_pipeline, GST,
  APPEND_PIPELINE, GstObject);

typedef struct
{
  void (*received_init_segment) (GstAppendPipeline   * self,
                                 gpointer              user_data);
  void (*duration_changed)      (GstAppendPipeline   * self,
                                 gpointer              user_data);
  void (*new_sample)            (GstAppendPipeline   * self,
                                 GstMediaSourceTrack * track,
                                 GstSample           * sample,
                                 gpointer              user_data);
  void (*eos)                   (GstAppendPipeline   * self,
                                 GstMediaSourceTrack * track,
                                 gpointer              user_data);
  void (*error)                 (GstAppendPipeline   * self,
                                 gpointer              user_data);
} GstAppendPipelineCallbacks;

GST_MSE_PRIVATE
GstAppendPipeline * gst_append_pipeline_new (
    GstAppendPipelineCallbacks * callbacks, gpointer user_data,
    GError ** error);

GST_MSE_PRIVATE
GstFlowReturn gst_append_pipeline_append (GstAppendPipeline * self,
    GstBuffer * buffer);

GST_MSE_PRIVATE
GstFlowReturn gst_append_pipeline_eos (GstAppendPipeline * self);

GST_MSE_PRIVATE
gboolean gst_append_pipeline_stop (GstAppendPipeline * self);

GST_MSE_PRIVATE
gboolean gst_append_pipeline_reset (GstAppendPipeline * self);

GST_MSE_PRIVATE
gsize gst_append_pipeline_n_tracks (GstAppendPipeline * self);

GST_MSE_PRIVATE
gboolean gst_append_pipeline_has_init_segment (GstAppendPipeline * self);

GST_MSE_PRIVATE
GstClockTime gst_append_pipeline_get_duration (GstAppendPipeline * self);

GST_MSE_PRIVATE
GPtrArray *gst_append_pipeline_get_audio_tracks (GstAppendPipeline * self);

GST_MSE_PRIVATE
GPtrArray *gst_append_pipeline_get_text_tracks (GstAppendPipeline * self);

GST_MSE_PRIVATE
GPtrArray *gst_append_pipeline_get_video_tracks (GstAppendPipeline * self);

GST_MSE_PRIVATE
gboolean gst_append_pipeline_get_eos (GstAppendPipeline * self);

GST_MSE_PRIVATE
void gst_append_pipeline_fail (GstAppendPipeline * self);

GST_MSE_PRIVATE
gboolean gst_append_pipeline_get_failed (GstAppendPipeline * self);

G_END_DECLS
