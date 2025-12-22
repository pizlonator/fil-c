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

#define GST_TYPE_MEDIA_SOURCE_SAMPLE_MAP \
    (gst_media_source_sample_map_get_type())

GST_MSE_PRIVATE
G_DECLARE_FINAL_TYPE (GstMediaSourceSampleMap, gst_media_source_sample_map, GST,
    MEDIA_SOURCE_SAMPLE_MAP, GstObject);

GST_MSE_PRIVATE
GstMediaSourceSampleMap *gst_media_source_sample_map_new (void);

GST_MSE_PRIVATE
void gst_media_source_sample_map_add (GstMediaSourceSampleMap * self,
    GstSample * sample);

GST_MSE_PRIVATE
void gst_media_source_sample_map_remove (GstMediaSourceSampleMap * self,
    GstSample * sample);

GST_MSE_PRIVATE
gsize gst_media_source_sample_map_remove_range (GstMediaSourceSampleMap * self,
    GstClockTime earliest, GstClockTime latest);

GST_MSE_PRIVATE
gsize
gst_media_source_sample_map_remove_range_from_start (GstMediaSourceSampleMap
    * self, GstClockTime latest_dts);

GST_MSE_PRIVATE
gsize
gst_media_source_sample_map_remove_range_from_end (GstMediaSourceSampleMap
    * self, GstClockTime earliest_dts);

GST_MSE_PRIVATE
gboolean gst_media_source_sample_map_contains (GstMediaSourceSampleMap * self,
    GstSample * sample);

GST_MSE_PRIVATE
GSequenceIter * gst_media_source_sample_map_get_begin_iter_by_pts (
    GstMediaSourceSampleMap * self);

GST_MSE_PRIVATE
GstClockTime gst_media_source_sample_map_get_highest_end_time (
    GstMediaSourceSampleMap * self);

GST_MSE_PRIVATE
guint gst_media_source_sample_map_get_size (GstMediaSourceSampleMap * self);

GST_MSE_PRIVATE
gsize gst_media_source_sample_map_get_storage_size (
    GstMediaSourceSampleMap * self);

GST_MSE_PRIVATE
GstIterator *
gst_media_source_sample_map_iter_samples_by_dts (GstMediaSourceSampleMap * map,
    GMutex * lock, guint32 * master_cookie, GstClockTime start_dts,
    GstSample * start_sample);

GST_MSE_PRIVATE
GstIterator *
gst_media_source_sample_map_iter_samples_by_pts (GstMediaSourceSampleMap * map,
    GMutex * lock, guint32 * master_cookie, GstClockTime start_pts,
    GstSample *start_sample);

G_END_DECLS
