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

#define GST_TYPE_D3D12_COMMAND_ALLOCATOR_POOL (gst_d3d12_command_allocator_pool_get_type())
G_DECLARE_FINAL_TYPE (GstD3D12CommandAllocatorPool,
    gst_d3d12_command_allocator_pool, GST, D3D12_COMMAND_ALLOCATOR_POOL, GstObject);

typedef struct _GstD3D12CommandAllocator GstD3D12CommandAllocator;

GstD3D12CommandAllocatorPool * gst_d3d12_command_allocator_pool_new (GstD3D12Device * device,
                                                                     D3D12_COMMAND_LIST_TYPE type);

gboolean                       gst_d3d12_command_allocator_pool_acquire (GstD3D12CommandAllocatorPool * pool,
                                                                         GstD3D12CommandAllocator ** cmd);

GstD3D12CommandAllocator *     gst_d3d12_command_allocator_ref (GstD3D12CommandAllocator * cmd);

void                           gst_d3d12_command_allocator_unref (GstD3D12CommandAllocator * cmd);

void                           gst_clear_d3d12_command_allocator (GstD3D12CommandAllocator ** cmd);

D3D12_COMMAND_LIST_TYPE        gst_d3d12_command_allocator_get_command_type (GstD3D12CommandAllocator * cmd);

gboolean                       gst_d3d12_command_allocator_get_handle (GstD3D12CommandAllocator * cmd,
                                                                       ID3D12CommandAllocator ** ca);

void                           gst_d3d12_command_allocator_set_user_data (GstD3D12CommandAllocator * cmd,
                                                                          gpointer user_data,
                                                                          GDestroyNotify notify);

gpointer                       gst_d3d12_command_allocator_get_user_data (GstD3D12CommandAllocator * cmd);

G_END_DECLS

