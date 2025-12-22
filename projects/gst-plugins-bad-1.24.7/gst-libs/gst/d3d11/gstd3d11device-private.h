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
#include <gst/d3d11/gstd3d11_fwd.h>

G_BEGIN_DECLS

GST_D3D11_API
gint64  gst_d3d11_pixel_shader_token_new (void);

GST_D3D11_API
gint64  gst_d3d11_vertex_shader_token_new (void);

GST_D3D11_API
gint64  gst_d3d11_compute_shader_token_new (void);

GST_D3D11_API
HRESULT gst_d3d11_device_get_pixel_shader  (GstD3D11Device * device,
                                            gint64 token,
                                            const void * bytecode,
                                            gsize bytecode_size,
                                            const gchar * source,
                                            gsize source_size,
                                            const gchar * entry_point,
                                            const D3D_SHADER_MACRO * defines,
                                            ID3D11PixelShader ** ps);

GST_D3D11_API
HRESULT gst_d3d11_device_get_pixel_shader_uncached (GstD3D11Device * device,
                                                    gint64 token,
                                                    const void * bytecode,
                                                    gsize bytecode_size,
                                                    const gchar * source,
                                                    gsize source_size,
                                                    const gchar * entry_point,
                                                    const D3D_SHADER_MACRO * defines,
                                                    ID3D11PixelShader ** ps);

GST_D3D11_API
HRESULT gst_d3d11_device_get_vertex_shader (GstD3D11Device * device,
                                            gint64 token,
                                            const void * bytecode,
                                            gsize bytecode_size,
                                            const gchar * source,
                                            gsize source_size,
                                            const gchar * entry_point,
                                            const D3D11_INPUT_ELEMENT_DESC * input_desc,
                                            guint desc_len,
                                            ID3D11VertexShader ** vs,
                                            ID3D11InputLayout ** layout);

GST_D3D11_API
HRESULT gst_d3d11_device_get_sampler       (GstD3D11Device * device,
                                            D3D11_FILTER filter,
                                            ID3D11SamplerState ** sampler);

GST_D3D11_API
HRESULT gst_d3d11_device_get_rasterizer    (GstD3D11Device * device,
                                            ID3D11RasterizerState ** rasterizer);

GST_D3D11_API
HRESULT gst_d3d11_device_get_rasterizer_msaa (GstD3D11Device * device,
                                              ID3D11RasterizerState ** rasterizer);

G_END_DECLS

