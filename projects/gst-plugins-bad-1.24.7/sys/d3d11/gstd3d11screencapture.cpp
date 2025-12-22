/*
 * GStreamer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d11screencapture.h"
#include "gstd3d11pluginutils.h"
#include <string.h>

#include <wrl.h>

GST_DEBUG_CATEGORY_EXTERN (gst_d3d11_screen_capture_debug);
#define GST_CAT_DEFAULT gst_d3d11_screen_capture_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

#define gst_d3d11_screen_capture_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE (GstD3D11ScreenCapture, gst_d3d11_screen_capture,
    GST_TYPE_OBJECT);

static void
gst_d3d11_screen_capture_class_init (GstD3D11ScreenCaptureClass * klass)
{
}

static void
gst_d3d11_screen_capture_init (GstD3D11ScreenCapture * self)
{
}

GstFlowReturn
gst_d3d11_screen_capture_prepare (GstD3D11ScreenCapture * capture)
{
  GstD3D11ScreenCaptureClass *klass;

  g_return_val_if_fail (GST_IS_D3D11_SCREEN_CAPTURE (capture), GST_FLOW_ERROR);

  klass = GST_D3D11_SCREEN_CAPTURE_GET_CLASS (capture);
  g_assert (klass->prepare);

  return klass->prepare (capture);
}

gboolean
gst_d3d11_screen_capture_get_size (GstD3D11ScreenCapture * capture,
    guint * width, guint * height)
{
  GstD3D11ScreenCaptureClass *klass;

  g_return_val_if_fail (GST_IS_D3D11_SCREEN_CAPTURE (capture), FALSE);
  g_return_val_if_fail (width != nullptr, FALSE);
  g_return_val_if_fail (height != nullptr, FALSE);

  klass = GST_D3D11_SCREEN_CAPTURE_GET_CLASS (capture);
  g_assert (klass->get_size);

  return klass->get_size (capture, width, height);
}

gboolean
gst_d3d11_screen_capture_unlock (GstD3D11ScreenCapture * capture)
{
  GstD3D11ScreenCaptureClass *klass;

  g_return_val_if_fail (GST_IS_D3D11_SCREEN_CAPTURE (capture), FALSE);
  klass = GST_D3D11_SCREEN_CAPTURE_GET_CLASS (capture);

  if (klass->unlock)
    return klass->unlock (capture);

  return TRUE;
}

gboolean
gst_d3d11_screen_capture_unlock_stop (GstD3D11ScreenCapture * capture)
{
  GstD3D11ScreenCaptureClass *klass;

  g_return_val_if_fail (GST_IS_D3D11_SCREEN_CAPTURE (capture), FALSE);
  klass = GST_D3D11_SCREEN_CAPTURE_GET_CLASS (capture);

  if (klass->unlock_stop)
    return klass->unlock_stop (capture);

  return TRUE;
}

void
gst_d3d11_screen_capture_show_border (GstD3D11ScreenCapture * capture,
    gboolean show)
{
  GstD3D11ScreenCaptureClass *klass;

  g_return_if_fail (GST_IS_D3D11_SCREEN_CAPTURE (capture));
  klass = GST_D3D11_SCREEN_CAPTURE_GET_CLASS (capture);

  if (klass->show_border)
    klass->show_border (capture, show);
}

GstFlowReturn
gst_d3d11_screen_capture_do_capture (GstD3D11ScreenCapture * capture,
    GstD3D11Device * device, ID3D11Texture2D * texture,
    ID3D11RenderTargetView * rtv, ShaderResource * resource,
    D3D11_BOX * crop_box, gboolean draw_mouse)
{
  GstD3D11ScreenCaptureClass *klass;

  g_return_val_if_fail (GST_IS_D3D11_SCREEN_CAPTURE (capture), GST_FLOW_ERROR);
  g_return_val_if_fail (texture != nullptr, GST_FLOW_ERROR);

  klass = GST_D3D11_SCREEN_CAPTURE_GET_CLASS (capture);
  g_assert (klass->do_capture);

  return klass->do_capture (capture, device, texture, rtv,
      resource, crop_box, draw_mouse);
}

HRESULT
gst_d3d11_screen_capture_find_output_for_monitor (HMONITOR monitor,
    IDXGIAdapter1 ** adapter, IDXGIOutput ** output)
{
  ComPtr < IDXGIFactory1 > factory;
  HRESULT hr = S_OK;

  g_return_val_if_fail (monitor != nullptr, E_INVALIDARG);

  hr = CreateDXGIFactory1 (IID_PPV_ARGS (&factory));
  if (FAILED (hr))
    return hr;

  for (UINT adapter_idx = 0;; adapter_idx++) {
    ComPtr < IDXGIAdapter1 > adapter_tmp;

    hr = factory->EnumAdapters1 (adapter_idx, &adapter_tmp);
    if (FAILED (hr))
      break;

    for (UINT output_idx = 0;; output_idx++) {
      ComPtr < IDXGIOutput > output_tmp;
      DXGI_OUTPUT_DESC desc;

      hr = adapter_tmp->EnumOutputs (output_idx, &output_tmp);
      if (FAILED (hr))
        break;

      hr = output_tmp->GetDesc (&desc);
      if (FAILED (hr))
        continue;

      if (desc.Monitor == monitor) {
        if (adapter)
          *adapter = adapter_tmp.Detach ();
        if (output)
          *output = output_tmp.Detach ();

        return S_OK;
      }
    }
  }

  return E_FAIL;
}

HRESULT
gst_d3d11_screen_capture_find_primary_monitor (HMONITOR * monitor,
    IDXGIAdapter1 ** adapter, IDXGIOutput ** output)
{
  ComPtr < IDXGIFactory1 > factory;
  HRESULT hr = S_OK;

  hr = CreateDXGIFactory1 (IID_PPV_ARGS (&factory));
  if (FAILED (hr))
    return hr;

  for (UINT adapter_idx = 0;; adapter_idx++) {
    ComPtr < IDXGIAdapter1 > adapter_tmp;

    hr = factory->EnumAdapters1 (adapter_idx, &adapter_tmp);
    if (FAILED (hr))
      break;

    for (UINT output_idx = 0;; output_idx++) {
      ComPtr < IDXGIOutput > output_tmp;
      DXGI_OUTPUT_DESC desc;
      MONITORINFOEXW minfo;

      hr = adapter_tmp->EnumOutputs (output_idx, &output_tmp);
      if (FAILED (hr))
        break;

      hr = output_tmp->GetDesc (&desc);
      if (FAILED (hr))
        continue;

      minfo.cbSize = sizeof (MONITORINFOEXW);
      if (!GetMonitorInfoW (desc.Monitor, &minfo))
        continue;

      if ((minfo.dwFlags & MONITORINFOF_PRIMARY) != 0) {
        if (monitor)
          *monitor = desc.Monitor;
        if (adapter)
          *adapter = adapter_tmp.Detach ();
        if (output)
          *output = output_tmp.Detach ();

        return S_OK;
      }
    }
  }

  return E_FAIL;
}

HRESULT
gst_d3d11_screen_capture_find_nth_monitor (guint index, HMONITOR * monitor,
    IDXGIAdapter1 ** adapter, IDXGIOutput ** output)
{
  ComPtr < IDXGIFactory1 > factory;
  HRESULT hr = S_OK;
  guint num_found = 0;

  hr = CreateDXGIFactory1 (IID_PPV_ARGS (&factory));
  if (FAILED (hr))
    return hr;

  for (UINT adapter_idx = 0;; adapter_idx++) {
    ComPtr < IDXGIAdapter1 > adapter_tmp;

    hr = factory->EnumAdapters1 (adapter_idx, &adapter_tmp);
    if (FAILED (hr))
      break;

    for (UINT output_idx = 0;; output_idx++) {
      ComPtr < IDXGIOutput > output_tmp;
      DXGI_OUTPUT_DESC desc;
      MONITORINFOEXW minfo;

      hr = adapter_tmp->EnumOutputs (output_idx, &output_tmp);
      if (FAILED (hr))
        break;

      hr = output_tmp->GetDesc (&desc);
      if (FAILED (hr))
        continue;

      minfo.cbSize = sizeof (MONITORINFOEXW);
      if (!GetMonitorInfoW (desc.Monitor, &minfo))
        continue;

      if (num_found == index) {
        if (monitor)
          *monitor = desc.Monitor;
        if (adapter)
          *adapter = adapter_tmp.Detach ();
        if (output)
          *output = output_tmp.Detach ();

        return S_OK;
      }

      num_found++;
    }
  }

  return E_FAIL;
}
