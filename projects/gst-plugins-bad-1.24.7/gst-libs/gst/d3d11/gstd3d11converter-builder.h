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
#include <vector>
#include <wrl.h>
#include <memory>
#include <vector>

enum class CONVERT_TYPE
{
  IDENTITY,
  SIMPLE,
  RANGE,
  GAMMA,
  PRIMARY,
};

struct PixelShader
{
  Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
  guint num_rtv;
};

typedef std::vector<std::shared_ptr<PixelShader>> PixelShaderList;

PixelShaderList
gst_d3d11_get_converter_pixel_shader (GstD3D11Device * device,
                                      GstVideoFormat in_format,
                                      GstVideoFormat out_format,
                                      gboolean in_premul,
                                      gboolean out_premul,
                                      CONVERT_TYPE type);

HRESULT gst_d3d11_get_converter_vertex_shader (GstD3D11Device * device,
                                               ID3D11VertexShader ** vs,
                                               ID3D11InputLayout ** layout);
