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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d11on12.h"
#include <d3d11on12.h>
#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

HRESULT
GstD3D11On12CreateDevice (IUnknown * device, IUnknown * command_queue,
    IUnknown ** d3d11on12)
{
  static const D3D_FEATURE_LEVEL feature_levels[] = {
    D3D_FEATURE_LEVEL_12_1,
    D3D_FEATURE_LEVEL_12_0,
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
  };

  g_return_val_if_fail (device, E_INVALIDARG);
  g_return_val_if_fail (command_queue, E_INVALIDARG);
  g_return_val_if_fail (d3d11on12, E_INVALIDARG);

  IUnknown *cq[] = { command_queue };

  ComPtr < ID3D11Device > d3d11device;
  auto hr = D3D11On12CreateDevice (device, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      feature_levels, G_N_ELEMENTS (feature_levels), cq, 1, 0, &d3d11device,
      nullptr, nullptr);

  if (FAILED (hr))
    return hr;

  ComPtr < ID3D11On12Device > d3d11on12device;
  hr = d3d11device.As (&d3d11on12device);
  if (FAILED (hr))
    return hr;

  *d3d11on12 = d3d11on12device.Detach ();
  return S_OK;
}

HRESULT
GstD3D11On12CreateWrappedResource (IUnknown * d3d11on12, IUnknown * resource12,
    UINT bind_flags, UINT misc_flags, UINT cpu_access_flags,
    UINT structure_byte_stride, UINT in_state, UINT out_state,
    ID3D11Resource ** resource11)
{
  g_return_val_if_fail (d3d11on12, E_INVALIDARG);
  g_return_val_if_fail (resource12, E_INVALIDARG);
  g_return_val_if_fail (resource11, E_INVALIDARG);

  ComPtr < ID3D11On12Device > device;
  auto hr = d3d11on12->QueryInterface (IID_PPV_ARGS (&device));
  if (FAILED (hr))
    return hr;

  D3D11_RESOURCE_FLAGS flags = { };
  flags.BindFlags = bind_flags;
  flags.MiscFlags = misc_flags;
  flags.CPUAccessFlags = cpu_access_flags;
  flags.StructureByteStride = structure_byte_stride;

  ComPtr < ID3D11Resource > resource;
  hr = device->CreateWrappedResource (resource12, &flags,
      (D3D12_RESOURCE_STATES) in_state, (D3D12_RESOURCE_STATES) out_state,
      IID_PPV_ARGS (&resource));
  if (FAILED (hr))
    return hr;

  *resource11 = resource.Detach ();

  return S_OK;
}

HRESULT
GstD3D11On12ReleaseWrappedResource (IUnknown * d3d11on12,
    ID3D11Resource ** resources, guint num_resources)
{
  g_return_val_if_fail (d3d11on12, E_INVALIDARG);

  ComPtr < ID3D11On12Device > device;
  auto hr = d3d11on12->QueryInterface (IID_PPV_ARGS (&device));
  if (FAILED (hr))
    return hr;

  device->ReleaseWrappedResources (resources, num_resources);

  return S_OK;
}

HRESULT
GstD3D11On12AcquireWrappedResource (IUnknown * d3d11on12,
    ID3D11Resource ** resources, guint num_resources)
{
  g_return_val_if_fail (d3d11on12, E_INVALIDARG);

  ComPtr < ID3D11On12Device > device;
  auto hr = d3d11on12->QueryInterface (IID_PPV_ARGS (&device));
  if (FAILED (hr))
    return hr;

  device->AcquireWrappedResources (resources, num_resources);

  return S_OK;
}
