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
#include <windows.h>
#include <d3d11.h>

G_BEGIN_DECLS

HRESULT GstD3D11On12CreateDevice (IUnknown * device,
                                  IUnknown * command_queue,
                                  IUnknown ** d3d11on12);

HRESULT GstD3D11On12CreateWrappedResource (IUnknown * d3d11on12,
                                           IUnknown * resource12,
                                           UINT bind_flags,
                                           UINT misc_flags,
                                           UINT cpu_access_flags,
                                           UINT structure_byte_stride,
                                           UINT in_state,
                                           UINT out_state,
                                           ID3D11Resource ** resource11);

HRESULT GstD3D11On12ReleaseWrappedResource (IUnknown * d3d11on12,
                                            ID3D11Resource ** resources,
                                            guint num_resources);

HRESULT GstD3D11On12AcquireWrappedResource (IUnknown * d3d11on12,
                                            ID3D11Resource ** resources,
                                            guint num_resources);

G_END_DECLS
