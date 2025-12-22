/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2023 Collabora Ltd.
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
#include <gst/base/gstdataqueue.h>
#include <gst/mse/mse-prelude.h>

G_BEGIN_DECLS

typedef void (*GstMseEventQueueCallback) (GstDataQueueItem *, gpointer);

#define GST_TYPE_MSE_EVENT_QUEUE (gst_mse_event_queue_get_type())

G_DECLARE_FINAL_TYPE (GstMseEventQueue, gst_mse_event_queue, GST,
    MSE_EVENT_QUEUE, GstObject);

GST_MSE_PRIVATE
GstMseEventQueue * gst_mse_event_queue_new (GstMseEventQueueCallback callback,
    gpointer user_data);

GST_MSE_PRIVATE
gboolean gst_mse_event_queue_push (GstMseEventQueue * self,
    GstDataQueueItem * item);

G_END_DECLS
