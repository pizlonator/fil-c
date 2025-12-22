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

#include "gstsourcebuffer.h"

G_BEGIN_DECLS

#define GST_TYPE_SOURCE_BUFFER_LIST (gst_source_buffer_list_get_type())

GST_MSE_API
G_DECLARE_FINAL_TYPE (GstSourceBufferList, gst_source_buffer_list, GST,
    SOURCE_BUFFER_LIST, GstObject);

GST_MSE_API
GstSourceBuffer *gst_source_buffer_list_index (GstSourceBufferList * self,
                                               guint index);
GST_MSE_API
guint gst_source_buffer_list_get_length (GstSourceBufferList * self);

G_END_DECLS
