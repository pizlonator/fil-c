/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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
#include <gst/d3d11/gstd3d11_fwd.h>
#include <d3dcompiler.h>

G_BEGIN_DECLS

GST_D3D11_API
gboolean gst_d3d11_compile_init (void);

GST_D3D11_API
HRESULT gst_d3d11_compile (LPCVOID src_data,
                           SIZE_T src_data_size,
                           LPCSTR source_name,
                           CONST D3D_SHADER_MACRO * defines,
                           ID3DInclude * include,
                           LPCSTR entry_point,
                           LPCSTR target,
                           UINT flags1,
                           UINT flags2,
                           ID3DBlob ** code,
                           ID3DBlob ** error_msgs);

GST_D3D11_API
HRESULT gst_d3d11_create_pixel_shader_simple (GstD3D11Device * device,
                                              const gchar * source,
                                              const gchar * entry_point,
                                              ID3D11PixelShader ** shader);

GST_D3D11_API
HRESULT gst_d3d11_create_vertex_shader_simple (GstD3D11Device * device,
                                               const gchar * source,
                                               const gchar * entry_point,
                                               const D3D11_INPUT_ELEMENT_DESC * input_desc,
                                               guint desc_len,
                                               ID3D11VertexShader ** shader,
                                               ID3D11InputLayout ** layout);

G_END_DECLS
