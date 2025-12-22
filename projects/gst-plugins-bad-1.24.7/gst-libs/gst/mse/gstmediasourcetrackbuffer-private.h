/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
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

G_BEGIN_DECLS

#define GST_TYPE_MEDIA_SOURCE_TRACK_BUFFER \
    (gst_media_source_track_buffer_get_type())

GST_MSE_PRIVATE
G_DECLARE_FINAL_TYPE (GstMediaSourceTrackBuffer, gst_media_source_track_buffer,
    GST, MEDIA_SOURCE_TRACK_BUFFER, GstObject);

GST_MSE_PRIVATE
GstMediaSourceTrackBuffer *gst_media_source_track_buffer_new (void);

GST_MSE_PRIVATE
gboolean gst_media_source_track_buffer_is_empty (
    GstMediaSourceTrackBuffer * self);

GST_MSE_PRIVATE
gint gst_media_source_track_buffer_get_size (GstMediaSourceTrackBuffer * self);

GST_MSE_PRIVATE
GstClockTime gst_media_source_track_buffer_get_highest_end_time (
    GstMediaSourceTrackBuffer * self);

GST_MSE_PRIVATE
GArray * gst_media_source_track_buffer_get_ranges (
    GstMediaSourceTrackBuffer * self);

GST_MSE_PRIVATE
void gst_media_source_track_buffer_set_group_start (GstMediaSourceTrackBuffer
    * self, GstClockTime group_start);

GST_MSE_PRIVATE
void gst_media_source_track_buffer_process_init_segment (
    GstMediaSourceTrackBuffer * self, gboolean sequence_mode);

GST_MSE_PRIVATE
void gst_media_source_track_buffer_add (GstMediaSourceTrackBuffer * self,
    GstSample * sample);

GST_MSE_PRIVATE
void gst_media_source_track_buffer_remove (GstMediaSourceTrackBuffer * self,
    GstSample * sample);

GST_MSE_PRIVATE
gsize
gst_media_source_track_buffer_remove_range (GstMediaSourceTrackBuffer * self,
    GstClockTime start, GstClockTime end);

GST_MSE_PRIVATE
void gst_media_source_track_buffer_clear (GstMediaSourceTrackBuffer * self);

GST_MSE_PRIVATE
void gst_media_source_track_buffer_eos (GstMediaSourceTrackBuffer * self);

GST_MSE_PRIVATE
gboolean gst_media_source_track_buffer_is_eos (
    GstMediaSourceTrackBuffer * self);

GST_MSE_PRIVATE
gboolean gst_media_source_track_buffer_await_eos_until (
    GstMediaSourceTrackBuffer * self, gint64 deadline);

GST_MSE_PRIVATE
gsize gst_media_source_track_buffer_get_storage_size (
    GstMediaSourceTrackBuffer * self);

GST_MSE_PRIVATE
GstIterator * gst_media_source_track_buffer_iter_samples (
    GstMediaSourceTrackBuffer * buffer, GstClockTime start_dts,
    GstSample * start_sample);

G_END_DECLS
