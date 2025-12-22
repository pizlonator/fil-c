/*
 * GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#include <gst/gst.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

typedef struct
{
  struct {
    FLOAT x;
    FLOAT y;
    FLOAT z;
  } position;
  struct {
    FLOAT x;
    FLOAT y;
  } texture;
} VertexData;

HRESULT
prepare_d3d11_device (ID3D11Device ** d3d11_device,
                      ID3D11DeviceContext ** d3d11_context,
                      IDXGIFactory2 ** dxgi_factory);

HRESULT
prepare_shared_texture (ID3D11Device * d3d11_device,
                        guint width,
                        guint height,
                        DXGI_FORMAT format,
                        UINT misc_flags,
                        ID3D11Texture2D ** texture,
                        ID3D11ShaderResourceView ** srv,
                        IDXGIKeyedMutex ** keyed_mutex,
                        HANDLE * shared_handle);

HRESULT
prepare_shader (ID3D11Device * d3d11_device,
                ID3D11DeviceContext * context,
                ID3D11SamplerState ** sampler,
                ID3D11PixelShader ** ps,
                ID3D11VertexShader ** vs,
                ID3D11InputLayout ** layout,
                ID3D11Buffer ** vertex,
                ID3D11Buffer ** index);