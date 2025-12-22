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

/**
 * plugin-amfcodec:
 *
 * Since: 1.22
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/d3d11/gstd3d11.h>
#include <wrl.h>
#include <core/Factory.h>
#include <versionhelpers.h>
#include "gstamfutils.h"
#include "gstamfh264enc.h"
#include "gstamfh265enc.h"
#include "gstamfav1enc.h"

#include <glib/gi18n-lib.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
using namespace amf;
/* *INDENT-ON* */

static gboolean
plugin_init (GstPlugin * plugin)
{
  AMFFactory *amf_factory;
  ComPtr < IDXGIFactory1 > factory;
  HRESULT hr;
  bool have_amd_gpu = false;

  if (!IsWindows8OrGreater ()) {
    gst_plugin_add_status_warning (plugin,
        N_("This plugin requires at least Windows 8 or newer."));
    return TRUE;
  }

  if (!gst_amf_init_once () || !gst_amf_get_factory ()) {
    gst_plugin_add_status_warning (plugin,
        "AMF runtime library \"" AMF_DLL_NAMEA "\" was not found.");
    return TRUE;
  }

  amf_factory = (AMFFactory *) gst_amf_get_factory ();

  hr = CreateDXGIFactory1 (IID_PPV_ARGS (&factory));
  if (FAILED (hr)) {
    /* Shouldn't happen, this means all DirectX bits would not be functional */
    gst_plugin_add_status_error (plugin, N_("Unable to enumerate GPUs."));
    return TRUE;
  }

  /* Enumerate AMD GPUs */
  for (guint idx = 0;; idx++) {
    ComPtr < IDXGIAdapter1 > adapter;
    AMFContextPtr context;
    DXGI_ADAPTER_DESC desc;
    gint64 luid;
    GstD3D11Device *device;
    ID3D11Device *device_handle;
    AMF_RESULT result;
    D3D_FEATURE_LEVEL feature_level;
    AMF_DX_VERSION dx_ver = AMF_DX11_1;

    hr = factory->EnumAdapters1 (idx, &adapter);
    if (FAILED (hr))
      break;

    hr = adapter->GetDesc (&desc);
    if (FAILED (hr))
      continue;

    if (desc.VendorId != 0x1002 && desc.VendorId != 0x1022)
      continue;

    have_amd_gpu = true;
    luid = gst_d3d11_luid_to_int64 (&desc.AdapterLuid);
    device = gst_d3d11_device_new_for_adapter_luid (luid,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT);

    if (!device)
      continue;

    device_handle = gst_d3d11_device_get_device_handle (device);
    feature_level = device_handle->GetFeatureLevel ();
    if (feature_level >= D3D_FEATURE_LEVEL_11_1)
      dx_ver = AMF_DX11_1;
    else
      dx_ver = AMF_DX11_0;

    result = amf_factory->CreateContext (&context);
    if (result == AMF_OK)
      result = context->InitDX11 (device_handle, dx_ver);

    if (result == AMF_OK) {
      gst_amf_h264_enc_register_d3d11 (plugin, device,
          (gpointer) context.GetPtr (), GST_RANK_PRIMARY);
      gst_amf_h265_enc_register_d3d11 (plugin, device,
          (gpointer) context.GetPtr (), GST_RANK_PRIMARY);
      gst_amf_av1_enc_register_d3d11 (plugin, device,
          (gpointer) context.GetPtr (), GST_RANK_NONE);
    }

    gst_clear_object (&device);
  }

  if (!have_amd_gpu) {
    /* A little unexpected case. AMF runtime (part of driver) is installed
     * but AMD GPU is not detected */
    gst_plugin_add_status_warning (plugin,
        N_("No AMD graphics cards detected!"));
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    amfcodec,
    "AMD AMF Codec plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
