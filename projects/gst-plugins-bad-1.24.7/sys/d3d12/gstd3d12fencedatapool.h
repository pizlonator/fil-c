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

#define GST_TYPE_D3D12_FENCE_DATA_POOL (gst_d3d12_fence_data_pool_get_type())
G_DECLARE_FINAL_TYPE (GstD3D12FenceDataPool,
    gst_d3d12_fence_data_pool, GST, D3D12_FENCE_DATA_POOL, GstObject);

GstD3D12FenceDataPool * gst_d3d12_fence_data_pool_new (void);

gboolean                gst_d3d12_fence_data_pool_acquire (GstD3D12FenceDataPool * pool,
                                                           GstD3D12FenceData ** data);

void                    gst_d3d12_fence_data_add_notify (GstD3D12FenceData * data,
                                                         gpointer user_data,
                                                         GDestroyNotify notify);

void                    gst_d3d12_fence_data_add_notify_com (GstD3D12FenceData * data,
                                                             gpointer unknown);

void                    gst_d3d12_fence_data_add_notify_mini_object (GstD3D12FenceData * data,
                                                                     gpointer object);

GstD3D12FenceData *     gst_d3d12_fence_data_ref (GstD3D12FenceData * data);

void                    gst_d3d12_fence_data_unref (GstD3D12FenceData * data);

void                    gst_clear_d3d12_fence_data (GstD3D12FenceData ** data);

G_END_DECLS

