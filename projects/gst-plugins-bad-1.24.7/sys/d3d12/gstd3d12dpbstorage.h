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
#include "gstd3d12.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D12_DPB_STORAGE (gst_d3d12_dpb_storage_get_type())
G_DECLARE_FINAL_TYPE (GstD3D12DpbStorage, gst_d3d12_dpb_storage,
    GST, D3D12_DPB_STORAGE, GstObject);

GstD3D12DpbStorage * gst_d3d12_dpb_storage_new (GstD3D12Device * device,
                                                guint dpb_size,
                                                gboolean use_array_of_textures,
                                                DXGI_FORMAT format,
                                                guint width,
                                                guint height,
                                                D3D12_RESOURCE_FLAGS resource_flags);

gboolean gst_d3d12_dpb_storage_acquire_frame (GstD3D12DpbStorage * storage,
                                              D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE * frame);

gboolean gst_d3d12_dpb_storage_add_frame     (GstD3D12DpbStorage * storage,
                                              D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE * frame);

gboolean gst_d3d12_dpb_storage_get_reference_frames  (GstD3D12DpbStorage * storage,
                                                      D3D12_VIDEO_ENCODE_REFERENCE_FRAMES * ref_frames);

gboolean gst_d3d12_dpb_storage_remove_oldest_frame   (GstD3D12DpbStorage * storage);

void     gst_d3d12_dpb_storage_clear_dpb     (GstD3D12DpbStorage * storage);

guint    gst_d3d12_dpb_storage_get_dpb_size  (GstD3D12DpbStorage * storage);

guint    gst_d3d12_dpb_storage_get_pool_size (GstD3D12DpbStorage * storage);

G_END_DECLS
