/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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
#include <gst/video/video.h>
#include "gstd3d12_fwd.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D12_COMMAND_QUEUE (gst_d3d12_command_queue_get_type())
G_DECLARE_FINAL_TYPE (GstD3D12CommandQueue,
    gst_d3d12_command_queue, GST, D3D12_COMMAND_QUEUE, GstObject);

GstD3D12CommandQueue * gst_d3d12_command_queue_new (GstD3D12Device * device,
                                                    const D3D12_COMMAND_QUEUE_DESC * desc,
                                                    guint queue_size);

gboolean               gst_d3d12_command_queue_get_handle (GstD3D12CommandQueue * queue,
                                                           ID3D12CommandQueue ** handle);

gboolean               gst_d3d12_command_queue_get_fence  (GstD3D12CommandQueue * queue,
                                                           ID3D12Fence ** handle);

HRESULT                gst_d3d12_command_queue_execute_command_lists (GstD3D12CommandQueue * queue,
                                                                      guint num_command_lists,
                                                                      ID3D12CommandList ** command_lists,
                                                                      guint64 * fence_value);

HRESULT                gst_d3d12_command_queue_execute_wait (GstD3D12CommandQueue * queue,
                                                             ID3D12Fence * fence,
                                                             guint64 fence_value);

guint64                gst_d3d12_command_queue_get_completed_value (GstD3D12CommandQueue * queue);

HRESULT                gst_d3d12_command_queue_fence_wait   (GstD3D12CommandQueue * queue,
                                                             guint64 fence_value,
                                                             HANDLE event_handle);


void                   gst_d3d12_command_queue_set_notify (GstD3D12CommandQueue * queue,
                                                           guint64 fence_value,
                                                           gpointer fence_data,
                                                           GDestroyNotify notify);

G_END_DECLS
