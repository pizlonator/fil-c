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

HRESULT gst_d3d11_shader_cache_get_pixel_shader_blob (gint64 token,
                                                      const gchar * source,
                                                      gsize source_size,
                                                      const gchar * entry_point,
                                                      const D3D_SHADER_MACRO * defines,
                                                      ID3DBlob ** blob);

HRESULT gst_d3d11_shader_cache_get_vertex_shader_blob (gint64 token,
                                                       const gchar * source,
                                                       gsize source_size,
                                                       const gchar * entry_point,
                                                       ID3DBlob ** blob);

HRESULT gst_d3d11_shader_cache_get_compute_shader_blob (gint64 token,
                                                        const gchar * source,
                                                        gsize source_size,
                                                        const gchar * entry_point,
                                                        const D3D_SHADER_MACRO * defines,
                                                        ID3DBlob ** blob);

G_END_DECLS

