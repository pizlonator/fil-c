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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "d3d11device.h"
#include <d3dcompiler.h>
#include <wrl.h>
#include <string.h>

using namespace Microsoft::WRL;

HRESULT
prepare_d3d11_device (ID3D11Device ** d3d11_device,
    ID3D11DeviceContext ** d3d11_context, IDXGIFactory2 ** dxgi_factory)
{
  HRESULT hr;
  static const D3D_FEATURE_LEVEL feature_levels[] = {
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_9_2,
    D3D_FEATURE_LEVEL_9_1
  };
  D3D_FEATURE_LEVEL selected_level;

  ComPtr<IDXGIFactory1> factory;
  hr = CreateDXGIFactory1 (IID_PPV_ARGS (&factory));
  if (FAILED (hr)) {
    gst_printerrln ("IDXGIFactory1 is unavailable, hr 0x%x", (guint) hr);
    return hr;
  }

  ComPtr<IDXGIFactory2> factory2;
  hr = factory.As (&factory2);
  if (FAILED (hr)) {
    gst_printerrln ("IDXGIFactory2 is unavailable, hr 0x%x", (guint) hr);
    return hr;
  }

  ComPtr<IDXGIAdapter1> adapter;
  hr = factory->EnumAdapters1 (0, &adapter);
  if (FAILED (hr)) {
    gst_printerrln ("IDXGIAdapter1 is unavailable, hr 0x%x", (guint) hr);
    return hr;
  }

  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  hr = D3D11CreateDevice (adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN,
      NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, feature_levels,
      G_N_ELEMENTS (feature_levels), D3D11_SDK_VERSION, &device,
      &selected_level, &context);

  /* Try again with excluding D3D_FEATURE_LEVEL_11_1 */
  if (FAILED (hr)) {
    hr = D3D11CreateDevice (adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN,
      NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, &feature_levels[1],
      G_N_ELEMENTS (feature_levels) - 1, D3D11_SDK_VERSION, &device,
      &selected_level, &context);
  }

  if (FAILED (hr)) {
    gst_printerrln ("ID3D11Device is unavailable, hr 0x%x", (guint) hr);
    return hr;
  }

  if (d3d11_device)
    *d3d11_device = device.Detach();
  if (d3d11_context)
    *d3d11_context = context.Detach();
  if (dxgi_factory)
    *dxgi_factory = factory2.Detach();

  return hr;
}

HRESULT
prepare_shared_texture (ID3D11Device * d3d11_device, guint width,
    guint height, DXGI_FORMAT format, UINT misc_flags,
    ID3D11Texture2D ** texture, ID3D11ShaderResourceView ** srv,
    IDXGIKeyedMutex ** keyed_mutex, HANDLE * shared_handle)
{
  D3D11_TEXTURE2D_DESC texture_desc = { 0, };
  HRESULT hr;

  /* Texture size doesn't need to be identical to that of backbuffer */
  texture_desc.Width = width;
  texture_desc.Height = height;
  texture_desc.MipLevels = 1;
  texture_desc.Format = format;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.ArraySize = 1;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags =
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  texture_desc.MiscFlags = misc_flags;

  ComPtr<ID3D11Texture2D> shared_texture;
  hr = d3d11_device->CreateTexture2D (&texture_desc, nullptr, &shared_texture);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create ID3D11Texture2D");
    return hr;
  }

  ComPtr<ID3D11ShaderResourceView> shader_resource_view;
  if (srv) {
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = { DXGI_FORMAT_UNKNOWN, };
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;
    srv_desc.Format = format;

    hr = d3d11_device->CreateShaderResourceView (shared_texture.Get(), &srv_desc,
        &shader_resource_view);
    if (FAILED (hr)) {
      gst_printerrln ("Couldn't create ID3D11ShaderResourceView");
      return hr;
    }
  }

  ComPtr<IDXGIKeyedMutex> keyed;
  if ((misc_flags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) != 0 && keyed_mutex) {
    hr = shared_texture.As (&keyed);
    if (FAILED (hr)) {
      gst_printerrln ("Couldn't get IDXGIKeyedMutex");
      return hr;
    }
  }

  ComPtr<IDXGIResource> dxgi_resource;
  hr = shared_texture.As (&dxgi_resource);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't get IDXGIResource handle");
    return hr;
  }

  HANDLE handle;
  if ((misc_flags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) != 0) {
    ComPtr<IDXGIResource1> dxgi_resource1;
    hr = dxgi_resource.As (&dxgi_resource1);

    if (FAILED (hr)) {
      gst_printerrln ("Couldn't get IDXGIResource1");
      return hr;
    }

    hr = dxgi_resource1->CreateSharedHandle (nullptr,
      DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &handle);
  } else {
    hr = dxgi_resource->GetSharedHandle (&handle);
  }

  if (FAILED (hr)) {
    gst_printerrln ("Couldn't get shared handle from texture");
    return hr;
  }

  *texture = shared_texture.Detach();
  if (srv)
    *srv = shader_resource_view.Detach();
  *shared_handle = handle;
  if (keyed && keyed_mutex)
    *keyed_mutex = keyed.Detach();

  return S_OK;
}

static HRESULT
d3d_compile (const gchar * source, gboolean is_pixel_shader, ID3DBlob ** code)
{
  HRESULT hr;
  const gchar *shader_target = "ps_4_0";

  if (!is_pixel_shader)
    shader_target = "vs_4_0";

  ComPtr<ID3DBlob> blob;
  ComPtr<ID3DBlob> error;
  hr = D3DCompile (source, strlen (source), nullptr, nullptr,
      nullptr, "main", shader_target, 0, 0, &blob, &error);

  if (FAILED (hr)) {
    const gchar *err = nullptr;
    if (error)
      err = (const gchar *) error->GetBufferPointer ();

    gst_printerrln ("Couldn't compile pixel shader, error: %s",
        GST_STR_NULL (err));
    return hr;
  }

  *code = blob.Detach();

  return S_OK;
}

HRESULT
prepare_shader (ID3D11Device * d3d11_device, ID3D11DeviceContext * context,
    ID3D11SamplerState ** sampler, ID3D11PixelShader ** ps,
    ID3D11VertexShader ** vs, ID3D11InputLayout ** layout,
    ID3D11Buffer ** vertex, ID3D11Buffer ** index)
{
  static const gchar ps_code[] =
    "Texture2D shaderTexture;\n"
    "SamplerState samplerState;\n"
    "\n"
    "struct PS_INPUT\n"
    "{\n"
    "  float4 Position: SV_POSITION;\n"
    "  float3 Texture: TEXCOORD0;\n"
    "};\n"
    "\n"
    "struct PS_OUTPUT\n"
    "{\n"
    "  float4 Plane: SV_Target;\n"
    "};\n"
    "\n"
    "PS_OUTPUT main(PS_INPUT input)\n"
    "{\n"
    "  PS_OUTPUT output;\n"
    "  output.Plane = shaderTexture.Sample(samplerState, input.Texture);\n"
    "  return output;\n"
    "}\n";

  static const gchar vs_code[] =
    "struct VS_INPUT\n"
    "{\n"
    "  float4 Position : POSITION;\n"
    "  float4 Texture : TEXCOORD0;\n"
    "};\n"
    "\n"
    "struct VS_OUTPUT\n"
    "{\n"
    "  float4 Position: SV_POSITION;\n"
    "  float4 Texture: TEXCOORD0;\n"
    "};\n"
    "\n"
    "VS_OUTPUT main(VS_INPUT input)\n"
    "{\n"
    "  return input;\n"
    "}\n";

  D3D11_SAMPLER_DESC sampler_desc = { D3D11_FILTER_MIN_MAG_MIP_POINT, };
  sampler_desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
  sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
  sampler_desc.MinLOD = 0;
  sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

  ComPtr<ID3D11SamplerState> sampler_state;
  HRESULT hr = d3d11_device->CreateSamplerState (&sampler_desc, &sampler_state);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create ID3D11SamplerState");
    return hr;
  }

  ComPtr<ID3DBlob> code;
  hr = d3d_compile (ps_code, TRUE, &code);
  if (FAILED (hr))
    return hr;

  ComPtr<ID3D11PixelShader> pixel_shader;
  hr = d3d11_device->CreatePixelShader (code->GetBufferPointer(),
      code->GetBufferSize(), nullptr, &pixel_shader);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create ID3D11PixelShader");
    return hr;
  }

  hr = d3d_compile (vs_code, FALSE, code.ReleaseAndGetAddressOf());
  if (FAILED (hr))
    return hr;

  ComPtr<ID3D11VertexShader> vertex_shader;
  hr = d3d11_device->CreateVertexShader (code->GetBufferPointer(),
      code->GetBufferSize(), nullptr, &vertex_shader);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create ID3D11VertexShader");
    return hr;
  }

  D3D11_INPUT_ELEMENT_DESC input_desc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
  };

  ComPtr<ID3D11InputLayout> input_layout;
  hr = d3d11_device->CreateInputLayout (input_desc, G_N_ELEMENTS (input_desc),
      code->GetBufferPointer(), code->GetBufferSize(), &input_layout);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create ID3D11InputLayout");
    return hr;
  }

  D3D11_BUFFER_DESC buffer_desc = { 0, };
  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (VertexData) * 4;
  buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  ComPtr<ID3D11Buffer> vertex_buffer;
  hr = d3d11_device->CreateBuffer (&buffer_desc, nullptr, &vertex_buffer);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create ID3D11Buffer for vertex buffer");
    return hr;
  }

  D3D11_MAPPED_SUBRESOURCE map;
  hr = context->Map (vertex_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't map vertex buffer");
    return hr;
  }

  VertexData *vertex_data = (VertexData *) map.pData;
  vertex_data[0].position.x = -1.0f;
  vertex_data[0].position.y = -1.0f;
  vertex_data[0].position.z = 0.0f;
  vertex_data[0].texture.x = 0.0f;
  vertex_data[0].texture.y = 1.0f;

  vertex_data[1].position.x = -1.0f;
  vertex_data[1].position.y = 1.0f;
  vertex_data[1].position.z = 0.0f;
  vertex_data[1].texture.x = 0.0f;
  vertex_data[1].texture.y = 0.0f;

  vertex_data[2].position.x = 1.0f;
  vertex_data[2].position.y = 1.0f;
  vertex_data[2].position.z = 0.0f;
  vertex_data[2].texture.x = 1.0f;
  vertex_data[2].texture.y = 0.0f;

  vertex_data[3].position.x = 1.0f;
  vertex_data[3].position.y = -1.0f;
  vertex_data[3].position.z = 0.0f;
  vertex_data[3].texture.x = 1.0f;
  vertex_data[3].texture.y = 1.0f;

  context->Unmap (vertex_buffer.Get(), 0);

  ComPtr<ID3D11Buffer> index_buffer;
  buffer_desc.ByteWidth = sizeof (WORD) * 2 * 3;
  buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  hr = d3d11_device->CreateBuffer (&buffer_desc, nullptr, &index_buffer);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create ID3D11Buffer for index buffer");
    return hr;
  }

  hr = context->Map (index_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't map index buffer");
    return hr;
  }

  WORD *indices = (WORD *) map.pData;
  indices[0] = 0;
  indices[1] = 1;
  indices[2] = 2;

  indices[3] = 3;
  indices[4] = 0;
  indices[5] = 2;

  context->Unmap (index_buffer.Get(), 0);

  *sampler = sampler_state.Detach();
  *ps = pixel_shader.Detach();
  *vs = vertex_shader.Detach();
  *layout = input_layout.Detach();
  *vertex = vertex_buffer.Detach();
  *index = index_buffer.Detach();

  return S_OK;
}