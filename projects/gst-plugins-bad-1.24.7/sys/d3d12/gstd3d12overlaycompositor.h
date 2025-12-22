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
 * Boston, MA 02120-1301, USA.
 */

#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include "gstd3d12.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D12_OVERLAY_COMPOSITOR (gst_d3d12_overlay_compositor_get_type())
G_DECLARE_FINAL_TYPE (GstD3D12OverlayCompositor, gst_d3d12_overlay_compositor,
    GST, D3D12_OVERLAY_COMPOSITOR, GstObject)

GstD3D12OverlayCompositor * gst_d3d12_overlay_compositor_new  (GstD3D12Device * device,
                                                               const GstVideoInfo * info);

gboolean                    gst_d3d12_overlay_compositor_upload (GstD3D12OverlayCompositor * compositor,
                                                                 GstBuffer * buf);

gboolean                    gst_d3d12_overlay_compositor_update_viewport (GstD3D12OverlayCompositor * compositor,
                                                                          GstVideoRectangle * viewport);

gboolean                    gst_d3d12_overlay_compositor_draw (GstD3D12OverlayCompositor * compositor,
                                                               GstBuffer * buf,
                                                               GstD3D12FenceData * fence_data,
                                                               ID3D12GraphicsCommandList * command_list);

G_END_DECLS

