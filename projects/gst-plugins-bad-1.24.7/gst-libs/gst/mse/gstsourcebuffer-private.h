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

#include <glib.h>
#include "gstsourcebuffer.h"

G_BEGIN_DECLS

typedef struct
{
  void (*duration_changed) (GstSourceBuffer * self, gpointer user_data);
  void (*received_init_segment) (GstSourceBuffer * self, gpointer user_data);
  void (*active_state_changed) (GstSourceBuffer * self, gpointer user_data);
} GstSourceBufferCallbacks;

GST_MSE_PRIVATE
GstSourceBuffer * gst_source_buffer_new (const gchar * content_type,
    GstObject * parent, GError ** error);

GST_MSE_PRIVATE
GstSourceBuffer * gst_source_buffer_new_with_callbacks (
    const gchar * content_type, GstObject * parent,
    GstSourceBufferCallbacks *callbacks, gpointer user_data, GError ** error);

GST_MSE_PRIVATE
void gst_source_buffer_teardown (GstSourceBuffer * self);

GST_MSE_PRIVATE
gboolean gst_source_buffer_has_init_segment (GstSourceBuffer * self);

GST_MSE_PRIVATE
gboolean gst_source_buffer_is_buffered (GstSourceBuffer * self, GstClockTime
    time);

GST_MSE_PRIVATE
gboolean gst_source_buffer_is_range_buffered (GstSourceBuffer * self,
    GstClockTime start, GstClockTime end);

GST_MSE_PRIVATE
GstClockTime gst_source_buffer_get_duration (GstSourceBuffer * self);

GST_MSE_PRIVATE
GPtrArray * gst_source_buffer_get_all_tracks (GstSourceBuffer * self);

GST_MSE_PRIVATE
void gst_source_buffer_seek (GstSourceBuffer * self, GstClockTime time);

GST_MSE_PRIVATE
gboolean gst_source_buffer_get_active (GstSourceBuffer * self);

G_END_DECLS
