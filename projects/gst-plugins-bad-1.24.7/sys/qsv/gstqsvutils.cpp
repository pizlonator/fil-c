/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

#include "gstqsvutils.h"

#ifdef G_OS_WIN32
#include <gst/d3d11/gstd3d11.h>
#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */
#else
#include <gst/va/gstva.h>
#endif

static mfxLoader _loader = nullptr;

mfxLoader
gst_qsv_get_loader (void)
{
  GST_QSV_CALL_ONCE_BEGIN {
    _loader = MFXLoad ();
  } GST_QSV_CALL_ONCE_END;

  return _loader;
}

void
gst_qsv_deinit (void)
{
  g_clear_pointer (&_loader, MFXUnload);
}

#ifdef G_OS_WIN32
static GList *
gst_qsv_get_d3d11_devices (void)
{
  GList *rst = nullptr;
  HRESULT hr;
  ComPtr < IDXGIFactory1 > factory;

  hr = CreateDXGIFactory1 (IID_PPV_ARGS (&factory));
  if (FAILED (hr))
    return nullptr;

  for (guint idx = 0;; idx++) {
    ComPtr < IDXGIAdapter1 > adapter;
    DXGI_ADAPTER_DESC desc;
    gint64 luid;
    GstD3D11Device *device;
    ComPtr < ID3D10Multithread > multi_thread;
    ID3D11Device *device_handle;

    hr = factory->EnumAdapters1 (idx, &adapter);
    if (FAILED (hr))
      return rst;

    hr = adapter->GetDesc (&desc);
    if (FAILED (hr))
      continue;

    if (desc.VendorId != 0x8086)
      continue;

    luid = gst_d3d11_luid_to_int64 (&desc.AdapterLuid);
    device = gst_d3d11_device_new_for_adapter_luid (luid,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT);

    if (!device)
      continue;

    device_handle = gst_d3d11_device_get_device_handle (device);
    hr = device_handle->QueryInterface (IID_PPV_ARGS (&multi_thread));
    if (FAILED (hr)) {
      gst_object_unref (device);
      continue;
    }

    /* Should enable mutithread protection layer, otherwise QSV will return
     * error code when this handle is passed to QSV via
     * MFXVideoCORE_SetHandle() */
    multi_thread->SetMultithreadProtected (TRUE);

    rst = g_list_append (rst, device);
  }

  return rst;
}
#else /* G_OS_WIN32 */
static GList *
gst_qsv_get_va_displays (void)
{
  gchar path[64];
  GList *rst = nullptr;

  for (guint i = 0; i < 8; i++) {
    GstVaDisplay *display;
    GstVaImplementation impl;

    g_snprintf (path, sizeof (path), "/dev/dri/renderD%d", 128 + i);
    if (!g_file_test (path, G_FILE_TEST_EXISTS))
      continue;

    display = gst_va_display_drm_new_from_path (path);
    if (!display)
      continue;

    impl = gst_va_display_get_implementation (display);
    if (impl != GST_VA_IMPLEMENTATION_INTEL_I965 &&
        impl != GST_VA_IMPLEMENTATION_INTEL_IHD) {
      gst_object_unref (display);
      continue;
    }

    rst = g_list_append (rst, display);
  }

  return rst;
}
#endif

GList *
gst_qsv_get_platform_devices (void)
{
#ifdef G_OS_WIN32
  return gst_qsv_get_d3d11_devices ();
#else
  return gst_qsv_get_va_displays ();
#endif
}

const gchar *
gst_qsv_status_to_string (mfxStatus status)
{
#define CASE(err) \
    case err: \
    return G_STRINGIFY (err);

  switch (status) {
      CASE (MFX_ERR_NONE);
      CASE (MFX_ERR_UNKNOWN);
      CASE (MFX_ERR_NULL_PTR);
      CASE (MFX_ERR_UNSUPPORTED);
      CASE (MFX_ERR_MEMORY_ALLOC);
      CASE (MFX_ERR_NOT_ENOUGH_BUFFER);
      CASE (MFX_ERR_INVALID_HANDLE);
      CASE (MFX_ERR_LOCK_MEMORY);
      CASE (MFX_ERR_NOT_INITIALIZED);
      CASE (MFX_ERR_NOT_FOUND);
      CASE (MFX_ERR_MORE_DATA);
      CASE (MFX_ERR_MORE_SURFACE);
      CASE (MFX_ERR_ABORTED);
      CASE (MFX_ERR_DEVICE_LOST);
      CASE (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
      CASE (MFX_ERR_INVALID_VIDEO_PARAM);
      CASE (MFX_ERR_UNDEFINED_BEHAVIOR);
      CASE (MFX_ERR_DEVICE_FAILED);
      CASE (MFX_ERR_MORE_BITSTREAM);
      CASE (MFX_ERR_GPU_HANG);
      CASE (MFX_ERR_REALLOC_SURFACE);
      CASE (MFX_ERR_RESOURCE_MAPPED);
      CASE (MFX_ERR_NOT_IMPLEMENTED);
      CASE (MFX_WRN_IN_EXECUTION);
      CASE (MFX_WRN_DEVICE_BUSY);
      CASE (MFX_WRN_VIDEO_PARAM_CHANGED);
      CASE (MFX_WRN_PARTIAL_ACCELERATION);
      CASE (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
      CASE (MFX_WRN_VALUE_NOT_CHANGED);
      CASE (MFX_WRN_OUT_OF_RANGE);
      CASE (MFX_WRN_FILTER_SKIPPED);
      CASE (MFX_ERR_NONE_PARTIAL_OUTPUT);
      CASE (MFX_WRN_ALLOC_TIMEOUT_EXPIRED);
    default:
      break;
  }
#undef CASE

  return "Unknown";
}

GstVideoFormat
gst_qsv_frame_info_format_to_gst (const mfxFrameInfo * info, gboolean is_gbr)
{
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;

  switch (info->FourCC) {
    case MFX_FOURCC_NV12:
      format = GST_VIDEO_FORMAT_NV12;
      break;
    case MFX_FOURCC_P010:
      format = GST_VIDEO_FORMAT_P010_10LE;
      break;
    case MFX_FOURCC_P016:
      if (info->BitDepthLuma == 12)
        format = GST_VIDEO_FORMAT_P012_LE;
      else
        format = GST_VIDEO_FORMAT_P016_LE;
      break;
    case MFX_FOURCC_YUY2:
      format = GST_VIDEO_FORMAT_YUY2;
      break;
    case MFX_FOURCC_Y210:
      format = GST_VIDEO_FORMAT_Y210;
      break;
    case MFX_FOURCC_Y216:
      format = GST_VIDEO_FORMAT_Y212_LE;
      break;
      break;
    case MFX_FOURCC_AYUV:
      if (is_gbr)
        format = GST_VIDEO_FORMAT_RBGA;
      else
        format = GST_VIDEO_FORMAT_VUYA;
      break;
    case MFX_FOURCC_Y410:
      if (is_gbr)
        format = GST_VIDEO_FORMAT_BGR10A2_LE;
      else
        format = GST_VIDEO_FORMAT_Y410;
      break;
    case MFX_FOURCC_Y416:
      if (is_gbr)
        format = GST_VIDEO_FORMAT_BGRA64_LE;
      else
        format = GST_VIDEO_FORMAT_Y412_LE;
      break;
    case MFX_FOURCC_RGB4:
      format = GST_VIDEO_FORMAT_BGRA;
      break;
    default:
      break;
  }

  return format;
}

gboolean
gst_qsv_frame_info_set_format (mfxFrameInfo * info, GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
      info->FourCC = MFX_FOURCC_NV12;
      info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
      info->BitDepthLuma = 8;
      info->BitDepthChroma = 8;
      info->Shift = 0;
      break;
    case GST_VIDEO_FORMAT_P010_10LE:
      info->FourCC = MFX_FOURCC_P010;
      info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
      info->BitDepthLuma = 10;
      info->BitDepthChroma = 10;
      info->Shift = 1;
      break;
    case GST_VIDEO_FORMAT_P012_LE:
      info->FourCC = MFX_FOURCC_P016;
      info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
      info->BitDepthLuma = 12;
      info->BitDepthChroma = 12;
      info->Shift = 1;
      break;
    case GST_VIDEO_FORMAT_P016_LE:
      info->FourCC = MFX_FOURCC_P016;
      info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
      info->BitDepthLuma = 16;
      info->BitDepthChroma = 16;
      info->Shift = 0;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      info->FourCC = MFX_FOURCC_YUY2;
      info->ChromaFormat = MFX_CHROMAFORMAT_YUV422;
      info->BitDepthLuma = 8;
      info->BitDepthChroma = 8;
      info->Shift = 0;
      break;
    case GST_VIDEO_FORMAT_Y210:
      info->FourCC = MFX_FOURCC_Y210;
      info->ChromaFormat = MFX_CHROMAFORMAT_YUV422;
      info->BitDepthLuma = 10;
      info->BitDepthChroma = 10;
      info->Shift = 1;
      break;
    case GST_VIDEO_FORMAT_Y212_LE:
      info->FourCC = MFX_FOURCC_Y216;
      info->ChromaFormat = MFX_CHROMAFORMAT_YUV422;
      info->BitDepthLuma = 12;
      info->BitDepthChroma = 12;
      info->Shift = 1;
      break;
    case GST_VIDEO_FORMAT_VUYA:
    case GST_VIDEO_FORMAT_RBGA:
      info->FourCC = MFX_FOURCC_AYUV;
      info->ChromaFormat = MFX_CHROMAFORMAT_YUV444;
      info->BitDepthLuma = 8;
      info->BitDepthChroma = 8;
      info->Shift = 0;
      break;
    case GST_VIDEO_FORMAT_Y410:
    case GST_VIDEO_FORMAT_BGR10A2_LE:
      info->FourCC = MFX_FOURCC_Y410;
      info->ChromaFormat = MFX_CHROMAFORMAT_YUV444;
      info->BitDepthLuma = 10;
      info->BitDepthChroma = 10;
      info->Shift = 0;
      break;
    case GST_VIDEO_FORMAT_Y412_LE:
    case GST_VIDEO_FORMAT_BGRA64_LE:
      info->FourCC = MFX_FOURCC_Y416;
      info->ChromaFormat = MFX_CHROMAFORMAT_YUV444;
      info->BitDepthLuma = 12;
      info->BitDepthChroma = 12;
      info->Shift = 1;
      break;
    case GST_VIDEO_FORMAT_BGRA:
      info->FourCC = MFX_FOURCC_RGB4;
      info->ChromaFormat = MFX_CHROMAFORMAT_YUV444;
      break;
    default:
      return FALSE;
  }

  return TRUE;
}
