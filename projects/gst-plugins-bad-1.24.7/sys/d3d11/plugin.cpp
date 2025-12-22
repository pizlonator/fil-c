/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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
 * SECTION:plugin-d3d11
 *
 * Microsoft Direct3D11 plugin.
 *
 * This plugin consists of various video filter, screen capture source,
 * video sink, and video decoder elements.
 *
 * GstD3D11 plugin supports H.264/AVC, H.265/HEVC, VP8, VP9, H.262/MPEG-2 video,
 * and AV1 codecs for decoding as well as hardware-accelerated video
 * deinterlacing. Note that minimum required OS version for video decoder and
 * deinterlacing elements is Windows 8.
 *
 * Plugin feature names of decoders:
 * - d3d11h264dec
 * - d3d11h265dec
 * - d3d11vp8dec
 * - d3d11vp9dec
 * - d3d11mpeg2dec
 * - d3d11av1dec
 *
 * Similar to the video decoder case, deinterlacing element would be registered
 * only if its supported by hardware with the feature name `d3d11deinterlace`
 *
 * However, depending on the hardware it runs on, some elements might not be
 * registered in case that underlying hardware doesn't support the feature.
 * For a system with multiple Direct3D11 compatible hardwares (i.e., GPU),
 * there can be multiple plugin features having the same role.
 * The naming rule for the non-primary decoder element is
 * `d3d11{codec}device{index}dec` where `index` is an arbitrary index number of
 * hardware starting from 1.
 *
 * To get a list of all available elements, user can run
 * ```sh
 * gst-inspect-1.0.exe d3d11
 * ```
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/d3d11/gstd3d11.h>
#include "gstd3d11videosink.h"
#include "gstd3d11upload.h"
#include "gstd3d11download.h"
#include "gstd3d11convert.h"
#include "gstd3d11compositor.h"
#include "gstd3d11h264dec.h"
#include "gstd3d11h265dec.h"
#include "gstd3d11vp9dec.h"
#include "gstd3d11vp8dec.h"
#include "gstd3d11mpeg2dec.h"
#include "gstd3d11av1dec.h"
#include "gstd3d11deinterlace.h"
#include "gstd3d11testsrc.h"
#include "gstd3d11overlay.h"
#include "gstd3d11ipcsink.h"
#include "gstd3d11ipcsrc.h"
#include "gstd3d11ipcclient.h"

#if !GST_D3D11_WINAPI_ONLY_APP
#include "gstd3d11screencapturesrc.h"
#include "gstd3d11screencapturedevice.h"
#endif

#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY (gst_d3d11_debug);
GST_DEBUG_CATEGORY (gst_d3d11_plugin_utils_debug);
GST_DEBUG_CATEGORY (gst_d3d11_format_debug);
GST_DEBUG_CATEGORY (gst_d3d11_device_debug);
GST_DEBUG_CATEGORY (gst_d3d11_overlay_compositor_debug);
GST_DEBUG_CATEGORY (gst_d3d11_window_debug);
GST_DEBUG_CATEGORY (gst_d3d11_video_processor_debug);
GST_DEBUG_CATEGORY (gst_d3d11_decoder_debug);
GST_DEBUG_CATEGORY (gst_d3d11_h264_dec_debug);
GST_DEBUG_CATEGORY (gst_d3d11_h265_dec_debug);
GST_DEBUG_CATEGORY (gst_d3d11_vp9_dec_debug);
GST_DEBUG_CATEGORY (gst_d3d11_vp8_dec_debug);
GST_DEBUG_CATEGORY (gst_d3d11_mpeg2_dec_debug);
GST_DEBUG_CATEGORY (gst_d3d11_av1_dec_debug);
GST_DEBUG_CATEGORY (gst_d3d11_deinterlace_debug);

#if !GST_D3D11_WINAPI_ONLY_APP
GST_DEBUG_CATEGORY (gst_d3d11_screen_capture_debug);
GST_DEBUG_CATEGORY (gst_d3d11_screen_capture_device_debug);
#endif

#define GST_CAT_DEFAULT gst_d3d11_debug

static void
plugin_deinit (gpointer data)
{
  gst_d3d11_ipc_client_deinit ();
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GstRank video_sink_rank = GST_RANK_PRIMARY;
  D3D_FEATURE_LEVEL max_feature_level = D3D_FEATURE_LEVEL_9_3;
  HRESULT hr;
  ComPtr < IDXGIFactory1 > factory;

  GST_DEBUG_CATEGORY_INIT (gst_d3d11_debug, "d3d11", 0, "direct3d 11 plugin");
  GST_DEBUG_CATEGORY_INIT (gst_d3d11_plugin_utils_debug,
      "d3d11pluginutils", 0, "d3d11 plugin utility functions");
  GST_DEBUG_CATEGORY_INIT (gst_d3d11_overlay_compositor_debug,
      "d3d11overlaycompositor", 0, "d3d11overlaycompositor");
  GST_DEBUG_CATEGORY_INIT (gst_d3d11_window_debug,
      "d3d11window", 0, "d3d11window");
  GST_DEBUG_CATEGORY_INIT (gst_d3d11_video_processor_debug,
      "d3d11videoprocessor", 0, "d3d11videoprocessor");

  if (!gst_d3d11_compile_init ()) {
    GST_WARNING ("Cannot initialize d3d11 compiler");
    return TRUE;
  }

  /* DXVA2 API is availble since Windows 8 */
  if (gst_d3d11_is_windows_8_or_greater ()) {
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_decoder_debug,
        "d3d11decoder", 0, "Direct3D11 Video Decoder object");
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_h264_dec_debug,
        "d3d11h264dec", 0, "Direct3D11 H.264 Video Decoder");
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_vp9_dec_debug,
        "d3d11vp9dec", 0, "Direct3D11 VP9 Video Decoder");
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_h265_dec_debug,
        "d3d11h265dec", 0, "Direct3D11 H.265 Video Decoder");
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_vp8_dec_debug,
        "d3d11vp8dec", 0, "Direct3D11 VP8 Decoder");
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_mpeg2_dec_debug,
        "d3d11mpeg2dec", 0, "Direct3D11 MPEG2 Decoder");
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_av1_dec_debug,
        "d3d11av1dec", 0, "Direct3D11 AV1 Decoder");
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_deinterlace_debug,
        "d3d11deinterlace", 0, "Direct3D11 Deinterlacer");
  }

  hr = CreateDXGIFactory1 (IID_PPV_ARGS (&factory));
  if (FAILED (hr))
    return TRUE;

  /* Enumerate devices to register decoders per device and to get the highest
   * feature level */
  for (guint i = 0;; i++) {
    ComPtr < IDXGIAdapter1 > adapter;
    GstD3D11Device *device;
    ID3D11Device *device_handle;
    D3D_FEATURE_LEVEL feature_level;

    hr = factory->EnumAdapters1 (i, &adapter);
    if (FAILED (hr))
      break;

    device = gst_d3d11_device_new (i, D3D11_CREATE_DEVICE_BGRA_SUPPORT);
    if (!device)
      continue;

    device_handle = gst_d3d11_device_get_device_handle (device);
    feature_level = device_handle->GetFeatureLevel ();

    if (feature_level > max_feature_level)
      max_feature_level = feature_level;

    /* DXVA2 API is availble since Windows 8 */
    if (gst_d3d11_is_windows_8_or_greater () &&
        gst_d3d11_device_get_video_device_handle (device)) {
      gboolean legacy = gst_d3d11_decoder_util_is_legacy_device (device);

      /* avdec_h264 has primary rank, make this higher than it */
      gst_d3d11_h264_dec_register (plugin,
          device, GST_RANK_PRIMARY + 1, legacy);
      if (!legacy) {
        /* avdec_h265 has primary rank, make this higher than it */
        gst_d3d11_h265_dec_register (plugin, device, GST_RANK_PRIMARY + 1);
        gst_d3d11_vp9_dec_register (plugin, device, GST_RANK_PRIMARY);
        gst_d3d11_vp8_dec_register (plugin, device, GST_RANK_PRIMARY);
        /* rust dav1ddec has "primary" rank */
        gst_d3d11_av1_dec_register (plugin, device, GST_RANK_PRIMARY + 1);
        /* avdec_mpeg2video has primary rank */
        gst_d3d11_mpeg2_dec_register (plugin, device, GST_RANK_PRIMARY + 1);
      }

      gst_d3d11_deinterlace_register (plugin, device, GST_RANK_MARGINAL);
    }

    gst_object_unref (device);
  }

  /* We don't support legacy Direct9.x level GPU */
  if (max_feature_level < D3D_FEATURE_LEVEL_10_0)
    return TRUE;

  gst_d3d11_plugin_utils_init (max_feature_level);

  gst_element_register (plugin,
      "d3d11upload", GST_RANK_NONE, GST_TYPE_D3D11_UPLOAD);
  gst_element_register (plugin,
      "d3d11download", GST_RANK_NONE, GST_TYPE_D3D11_DOWNLOAD);
  gst_element_register (plugin,
      "d3d11convert", GST_RANK_NONE, GST_TYPE_D3D11_CONVERT);
  gst_element_register (plugin,
      "d3d11colorconvert", GST_RANK_NONE, GST_TYPE_D3D11_COLOR_CONVERT);
  gst_element_register (plugin,
      "d3d11scale", GST_RANK_NONE, GST_TYPE_D3D11_SCALE);
  gst_element_register (plugin,
      "d3d11videosink", video_sink_rank, GST_TYPE_D3D11_VIDEO_SINK);

  gst_element_register (plugin,
      "d3d11compositor", GST_RANK_SECONDARY, GST_TYPE_D3D11_COMPOSITOR);
  gst_element_register (plugin,
      "d3d11testsrc", GST_RANK_NONE, GST_TYPE_D3D11_TEST_SRC);
  gst_element_register (plugin,
      "d3d11overlay", GST_RANK_NONE, GST_TYPE_D3D11_OVERLAY);
  gst_element_register (plugin,
      "d3d11ipcsink", GST_RANK_NONE, GST_TYPE_D3D11_IPC_SINK);
  gst_element_register (plugin,
      "d3d11ipcsrc", GST_RANK_NONE, GST_TYPE_D3D11_IPC_SRC);

#if !GST_D3D11_WINAPI_ONLY_APP
  if (gst_d3d11_is_windows_8_or_greater ()) {
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_screen_capture_debug,
        "d3d11screencapturesrc", 0, "d3d11screencapturesrc");
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_screen_capture_device_debug,
        "d3d11screencapturedevice", 0, "d3d11screencapturedevice");

    gst_element_register (plugin,
        "d3d11screencapturesrc", GST_RANK_NONE,
        GST_TYPE_D3D11_SCREEN_CAPTURE_SRC);
    gst_device_provider_register (plugin,
        "d3d11screencapturedeviceprovider", GST_RANK_PRIMARY,
        GST_TYPE_D3D11_SCREEN_CAPTURE_DEVICE_PROVIDER);
  }
#endif

  g_object_set_data_full (G_OBJECT (plugin),
      "plugin-d3d11-shutdown", (gpointer) "shutdown-data",
      (GDestroyNotify) plugin_deinit);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    d3d11,
    "Direct3D11 plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
