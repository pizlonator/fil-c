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

#include "gstmediasource.h"
#include "gstmseeventqueue-private.h"

G_BEGIN_DECLS

GST_MSE_PRIVATE
gboolean gst_media_source_is_attached (GstMediaSource * self);

GST_MSE_PRIVATE
void gst_media_source_open (GstMediaSource * self);

GST_MSE_PRIVATE
GstMseSrc * gst_media_source_get_source_element (GstMediaSource * self);

GST_MSE_PRIVATE
void gst_media_source_seek (GstMediaSource * self, GstClockTime time);

struct _GstMediaSource
{
  GstObject parent_instance;

  GstMseSrc *element;
  GstMseEventQueue *event_queue;

  GstSourceBufferList *buffers;
  GstSourceBufferList *active_buffers;

  GstMediaSourceRange live_seekable_range;

  GstClockTime duration;
  GstMediaSourceReadyState ready_state;
};

G_END_DECLS
