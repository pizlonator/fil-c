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
#include <gst/video/gstvideoaggregator.h>
#include "gstd3d12.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D12_COMPOSITOR_PAD (gst_d3d12_compositor_pad_get_type())
G_DECLARE_FINAL_TYPE (GstD3D12CompositorPad, gst_d3d12_compositor_pad,
    GST, D3D12_COMPOSITOR_PAD, GstVideoAggregatorPad)

#define GST_TYPE_D3D12_COMPOSITOR (gst_d3d12_compositor_get_type())
G_DECLARE_FINAL_TYPE (GstD3D12Compositor, gst_d3d12_compositor,
    GST, D3D12_COMPOSITOR, GstVideoAggregator)

G_END_DECLS

