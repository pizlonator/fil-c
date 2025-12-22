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
#include "gstd3d12format.h"
#include "gstd3d12memory.h"

G_BEGIN_DECLS

struct _GstD3D12AllocationParams
{
  GstVideoInfo info;
  GstVideoInfo aligned_info;
  GstD3D12Format d3d12_format;
  GstD3D12AllocationFlags flags;
  D3D12_HEAP_FLAGS heap_flags;
  D3D12_RESOURCE_FLAGS resource_flags;
  guint array_size;
};

G_END_DECLS

