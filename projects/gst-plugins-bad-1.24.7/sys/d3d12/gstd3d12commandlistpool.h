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

#define GST_TYPE_D3D12_COMMAND_LIST_POOL (gst_d3d12_command_list_pool_get_type())
G_DECLARE_FINAL_TYPE (GstD3D12CommandListPool,
    gst_d3d12_command_list_pool, GST, D3D12_COMMAND_LIST_POOL, GstObject);

typedef struct _GstD3D12CommandList GstD3D12CommandList;

GstD3D12CommandListPool * gst_d3d12_command_list_pool_new (GstD3D12Device * device,
                                                           D3D12_COMMAND_LIST_TYPE type);

gboolean                  gst_d3d12_command_list_pool_acquire (GstD3D12CommandListPool * pool,
                                                               ID3D12CommandAllocator * ca,
                                                               GstD3D12CommandList ** cmd);

GstD3D12CommandList *     gst_d3d12_command_list_ref (GstD3D12CommandList * cmd);

void                      gst_d3d12_command_list_unref (GstD3D12CommandList * cmd);

void                      gst_clear_d3d12_command_list (GstD3D12CommandList ** cmd);

D3D12_COMMAND_LIST_TYPE   gst_d3d12_command_list_get_command_type (GstD3D12CommandList * cmd);

gboolean                  gst_d3d12_command_list_get_handle (GstD3D12CommandList * cmd,
                                                             ID3D12CommandList ** cl);

G_END_DECLS

