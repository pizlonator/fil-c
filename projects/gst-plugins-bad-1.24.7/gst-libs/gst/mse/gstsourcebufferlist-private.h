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
#include "gstsourcebufferlist.h"
#include "gstsourcebuffer.h"

G_BEGIN_DECLS

GST_MSE_PRIVATE
GstSourceBufferList *gst_source_buffer_list_new (void);

GST_MSE_PRIVATE
gboolean gst_source_buffer_list_contains (GstSourceBufferList * self,
    GstSourceBuffer * buf);

GST_MSE_PRIVATE
void gst_source_buffer_list_append (GstSourceBufferList * self,
    GstSourceBuffer * buf);

GST_MSE_PRIVATE
gboolean gst_source_buffer_list_remove (GstSourceBufferList * self,
    GstSourceBuffer * buf);

GST_MSE_PRIVATE
void gst_source_buffer_list_remove_all (GstSourceBufferList * self);

GST_MSE_PRIVATE
void gst_source_buffer_list_notify_freeze (GstSourceBufferList * self);

GST_MSE_PRIVATE
void gst_source_buffer_list_notify_cancel (GstSourceBufferList * self);

GST_MSE_PRIVATE
void gst_source_buffer_list_notify_added (GstSourceBufferList * self);

GST_MSE_PRIVATE
void gst_source_buffer_list_notify_removed (GstSourceBufferList * self);

GST_MSE_PRIVATE
void gst_source_buffer_list_notify_thaw (GstSourceBufferList * self);

G_END_DECLS
