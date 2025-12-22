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
#include <gst/d3d11/gstd3d11_fwd.h>
#include <gst/d3d11/gstd3d11format.h>
#include <gst/d3d11/gstd3d11memory.h>

G_BEGIN_DECLS

struct _GstD3D11AllocationParams
{
  GstVideoInfo info;
  GstVideoInfo aligned_info;
  GstD3D11Format d3d11_format;
  GstD3D11AllocationFlags flags;
  guint bind_flags;
  guint misc_flags;
  guint array_size;
  guint sample_count;
  guint sample_quality;
};

G_END_DECLS

