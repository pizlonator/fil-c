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
#include <gst/mse/mse-prelude.h>

G_BEGIN_DECLS

typedef enum
{
  GST_MEDIA_SOURCE_TRACK_TYPE_AUDIO,
  GST_MEDIA_SOURCE_TRACK_TYPE_VIDEO,
  GST_MEDIA_SOURCE_TRACK_TYPE_TEXT,
  GST_MEDIA_SOURCE_TRACK_TYPE_OTHER,
} GstMediaSourceTrackType;

#define GST_TYPE_MEDIA_SOURCE_TRACK (gst_media_source_track_get_type())

GST_MSE_PRIVATE
G_DECLARE_FINAL_TYPE (GstMediaSourceTrack, gst_media_source_track, GST,
    MEDIA_SOURCE_TRACK, GstObject);

GST_MSE_PRIVATE
GstMediaSourceTrackType gst_media_source_track_get_track_type (
    GstMediaSourceTrack * self);

GST_MSE_PRIVATE
const gchar *
gst_media_source_track_get_id (GstMediaSourceTrack * self);

GST_MSE_PRIVATE
gboolean gst_media_source_track_get_active (GstMediaSourceTrack * self);

GST_MSE_PRIVATE
GstMediaSourceTrack *gst_media_source_track_new (GstMediaSourceTrackType type,
    const gchar * track_id);

GST_MSE_PRIVATE
GstMediaSourceTrack *
gst_media_source_track_new_with_initial_caps (GstMediaSourceTrackType type,
    const gchar * track_id, GstCaps * initial_caps);

GST_MSE_PRIVATE
GstMediaSourceTrack * gst_media_source_track_new_with_size (
    GstMediaSourceTrackType type, const gchar * track_id, gsize size);

GST_MSE_PRIVATE
GstCaps *gst_media_source_track_get_initial_caps (GstMediaSourceTrack * self);

GST_MSE_PRIVATE
GstStreamType
gst_media_source_track_get_stream_type (GstMediaSourceTrack * self);

GST_MSE_API
void gst_media_source_track_set_active (GstMediaSourceTrack * self,
    gboolean active);

GST_MSE_PRIVATE
GstMiniObject *gst_media_source_track_pop (GstMediaSourceTrack * self);

GST_MSE_PRIVATE
gboolean gst_media_source_track_push (GstMediaSourceTrack * self,
    GstSample * sample);

GST_MSE_PRIVATE
gboolean gst_media_source_track_try_push (GstMediaSourceTrack * self,
    GstSample * sample);

GST_MSE_PRIVATE
gboolean gst_media_source_track_push_eos (GstMediaSourceTrack * self);

GST_MSE_PRIVATE
void gst_media_source_track_flush (GstMediaSourceTrack * self);

GST_MSE_PRIVATE
void gst_media_source_track_resume (GstMediaSourceTrack * self);

GST_MSE_PRIVATE
gboolean gst_media_source_track_is_empty (GstMediaSourceTrack * self);

GST_MSE_PRIVATE
GstStreamType gst_media_source_track_type_to_stream_type (
    GstMediaSourceTrackType type);

GST_MSE_PRIVATE
GstMediaSourceTrackType gst_media_source_track_type_from_stream_type (
    GstStreamType type);

G_END_DECLS
