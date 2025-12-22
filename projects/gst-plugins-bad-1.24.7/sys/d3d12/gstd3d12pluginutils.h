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
#include "gstd3d12.h"
#include "gstd3d12-private.h"

G_BEGIN_DECLS

enum GstD3D12SamplingMethod
{
  GST_D3D12_SAMPLING_METHOD_NEAREST,
  GST_D3D12_SAMPLING_METHOD_BILINEAR,
  GST_D3D12_SAMPLING_METHOD_LINEAR_MINIFICATION,
  GST_D3D12_SAMPLING_METHOD_ANISOTROPIC,
};

#define GST_TYPE_D3D12_SAMPLING_METHOD (gst_d3d12_sampling_method_get_type())
GType gst_d3d12_sampling_method_get_type (void);

D3D12_FILTER  gst_d3d12_sampling_method_to_native (GstD3D12SamplingMethod method);

enum GstD3D12MSAAMode
{
  GST_D3D12_MSAA_DISABLED,
  GST_D3D12_MSAA_2X,
  GST_D3D12_MSAA_4X,
  GST_D3D12_MSAA_8X,
};

#define GST_TYPE_D3D12_MSAA_MODE (gst_d3d12_msaa_mode_get_type())
GType gst_d3d12_msaa_mode_get_type (void);

void          gst_d3d12_buffer_after_write (GstBuffer * buffer,
                                            guint64 fence_value);

gboolean      gst_d3d12_need_transform (gfloat rotation_x,
                                        gfloat rotation_y,
                                        gfloat rotation_z,
                                        gfloat scale_x,
                                        gfloat scale_y);

G_END_DECLS
