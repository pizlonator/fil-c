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
#include <gst/gst.h>
#include <gst/mse/mse-prelude.h>

G_BEGIN_DECLS

typedef struct
{
  gchar *mime_type;
  GStrv codecs;
} GstMediaSourceMediaType;

#define GST_MEDIA_SOURCE_MEDIA_TYPE_INIT { 0 }

GST_MSE_PRIVATE
gboolean gst_media_source_media_type_parse (GstMediaSourceMediaType * self,
    const gchar * type);

GST_MSE_PRIVATE
gboolean gst_media_source_media_type_is_caps_supported (const GstCaps * caps);

GST_MSE_PRIVATE
gboolean gst_media_source_media_type_is_supported (
    GstMediaSourceMediaType * self);

GST_MSE_PRIVATE
gboolean gst_media_source_media_type_generates_timestamp (
    GstMediaSourceMediaType * self);

GST_MSE_PRIVATE
void gst_media_source_media_type_reset (GstMediaSourceMediaType * self);

GST_MSE_PRIVATE
void gst_media_source_media_type_free (GstMediaSourceMediaType * self);

G_END_DECLS
