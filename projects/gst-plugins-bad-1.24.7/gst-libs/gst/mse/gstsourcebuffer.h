/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016, 2017 Igalia, S.L
 * Copyright (C) 2015, 2016, 2017 Metrological Group B.V.
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

/**
 * GstSourceBufferAppendMode:
 * @GST_SOURCE_BUFFER_APPEND_MODE_SEGMENTS:
 * @GST_SOURCE_BUFFER_APPEND_MODE_SEQUENCE:
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-appendmode)
 *
 * Since: 1.24
 */
typedef enum
{
  GST_SOURCE_BUFFER_APPEND_MODE_SEGMENTS,
  GST_SOURCE_BUFFER_APPEND_MODE_SEQUENCE,
} GstSourceBufferAppendMode;

/**
 * GstSourceBufferInterval:
 *
 * Since: 1.24
 */
typedef struct
{
  GstClockTime start;
  GstClockTime end;
} GstSourceBufferInterval;

#define GST_TYPE_SOURCE_BUFFER (gst_source_buffer_get_type())

GST_MSE_API
G_DECLARE_FINAL_TYPE (GstSourceBuffer, gst_source_buffer, GST, SOURCE_BUFFER,
    GstObject);

GST_MSE_API
GstSourceBufferAppendMode gst_source_buffer_get_append_mode (
    GstSourceBuffer * self);

GST_MSE_API
gboolean gst_source_buffer_set_append_mode (GstSourceBuffer * self,
    GstSourceBufferAppendMode mode, GError ** error);

GST_MSE_API
gchar *gst_source_buffer_get_content_type (GstSourceBuffer * self);

GST_MSE_API
gboolean gst_source_buffer_get_updating (GstSourceBuffer * self);

GST_MSE_API
GArray * gst_source_buffer_get_buffered (GstSourceBuffer * self,
                                         GError ** error);

GST_MSE_API
gboolean gst_source_buffer_set_timestamp_offset (GstSourceBuffer * self,
                                                 GstClockTime offset,
                                                 GError ** error);

GST_MSE_API
GstClockTime gst_source_buffer_get_timestamp_offset (GstSourceBuffer * self);

GST_MSE_API
gboolean gst_source_buffer_set_append_window_start (GstSourceBuffer * self,
                                                    GstClockTime start,
                                                    GError ** error);

GST_MSE_API
GstClockTime gst_source_buffer_get_append_window_start (GstSourceBuffer * self);

GST_MSE_API
gboolean gst_source_buffer_set_append_window_end (GstSourceBuffer * self,
                                                  GstClockTime end,
                                                  GError ** error);

GST_MSE_API
GstClockTime gst_source_buffer_get_append_window_end (GstSourceBuffer * self);

GST_MSE_API
gboolean gst_source_buffer_append_buffer (GstSourceBuffer * self,
    GstBuffer * buf, GError ** error);

GST_MSE_API
gboolean gst_source_buffer_abort (GstSourceBuffer * self, GError ** error);

GST_MSE_API
gboolean gst_source_buffer_change_content_type (GstSourceBuffer * self,
                                                const gchar * type,
                                                GError ** error);

GST_MSE_API
gboolean gst_source_buffer_remove (GstSourceBuffer * self, GstClockTime start,
                                   GstClockTime end, GError ** error);

G_END_DECLS
