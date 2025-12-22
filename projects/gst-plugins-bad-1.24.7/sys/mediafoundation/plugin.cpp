/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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

/**
 * SECTION:plugin-mediafoundation
 *
 * Microsoft MediaFoundation plugin.
 *
 * This plugin consists of various hardware/software video encoders
 * software audio encoders, and video capture (from webcam) elements.
 *
 * GstMediaFoundation plugin supports H.264/AVC, H.265/HEVC, VP9, codecs for
 * hardware-accelerate encoding.
 *
 * However, depending on the hardware it runs on, some elements might not be
 * registered in case that underlying hardware doesn't support the feature.
 *
 * Moreover, depending on hardware vendor's MediaFoundation implementation,
 * secendary GPU may not be usable. In that case, user could use vendor
 * specific plugins, Intel Media SDK and NVCODEC plugins for example.
 *
 * For a system with multiple MediaFoundation compatible hardwares (i.e., GPU),
 * there can be multiple plugin features having the same role.
 * Also, there would be additional software video encoder element the system
 * meets requirement.
 *
 * The naming rule for the non-primary encoder is `mf{codec}device{index}enc`
 * where `index` is an arbitrary index number of hardware starting from 1.
 *
 * To get a list of all available elements, user can run
 * ```sh
 * gst-inspect-1.0.exe mediafoundation
 * ```
 *
 * Since: 1.18
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstmfconfig.h"

#include <winapifamily.h>
#include <wrl.h>

#include <gst/gst.h>
#include <gst/video/video.h>
#include "gstmfvideosrc.h"
#include "gstmfdevice.h"
#include "gstmfutils.h"
#include "gstmfh264enc.h"
#include "gstmfh265enc.h"
#include "gstmfvp9enc.h"
#include "gstmfaacenc.h"
#include "gstmfmp3enc.h"
#include "gstmfaacdec.h"
#include "gstmfmp3dec.h"

#if GST_MF_HAVE_D3D11
#include <gst/d3d11/gstd3d11.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <gstmfplatloader.h>
#endif

GST_DEBUG_CATEGORY (gst_mf_debug);
GST_DEBUG_CATEGORY (gst_mf_utils_debug);
GST_DEBUG_CATEGORY (gst_mf_source_object_debug);
GST_DEBUG_CATEGORY (gst_mf_transform_debug);
GST_DEBUG_CATEGORY (gst_mf_video_buffer_debug);
GST_DEBUG_CATEGORY (gst_mf_video_encoder_debug);

#define GST_CAT_DEFAULT gst_mf_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

static void
plugin_deinit (gpointer data)
{
  MFShutdown ();
}

#if GST_MF_HAVE_D3D11
static GList *
get_d3d11_devices (void)
{
  GList *ret = nullptr;
  guint i;
  HRESULT hr;
  ComPtr < IMFVideoSampleAllocatorEx > allocator;

  /* Check whether we can use IMFVideoSampleAllocatorEx interface */
  hr = GstMFCreateVideoSampleAllocatorEx (IID_IMFVideoSampleAllocatorEx,
      &allocator);
  if (!gst_mf_result (hr)) {
    GST_DEBUG ("IMFVideoSampleAllocatorEx interface is unavailable");
    return nullptr;
  }

  /* AMD seems supporting up to 12 cards, and 8 for NVIDIA */
  for (i = 0; i < 12; i++) {
    GstD3D11Device *device;
    gboolean is_hardware = FALSE;
    GstD3D11Format d3d11_format;
    ID3D11Device *device_handle;
    D3D11_FEATURE_DATA_D3D11_OPTIONS4 options = { 0, };
    UINT supported = 0;

    device = gst_d3d11_device_new (i, D3D11_CREATE_DEVICE_VIDEO_SUPPORT);

    if (!device)
      break;

    g_object_get (device, "hardware", &is_hardware, nullptr);

    if (!is_hardware) {
      GST_DEBUG_OBJECT (device, "Given d3d11 device is not for hardware");
      gst_object_unref (device);
      continue;
    }

    /* device can support NV12 format? */
    if (!gst_d3d11_device_get_format (device,
            GST_VIDEO_FORMAT_NV12, &d3d11_format) ||
        d3d11_format.dxgi_format != DXGI_FORMAT_NV12) {
      GST_DEBUG_OBJECT (device,
          "Given d3d11 device cannot support NV12 format");
      gst_object_unref (device);
      continue;
    }

    /* device can support ExtendedNV12SharedTextureSupported?
     *
     * NOTE: we will make use of per encoder object d3d11 device without
     * sharing it in a pipeline because MF needs D3D11_CREATE_DEVICE_VIDEO_SUPPORT
     * but the flag doesn't used for the other our use cases.
     * So we need texture sharing feature so that we can copy d3d11 texture into
     * MF specific texture pool without download texture */

    device_handle = gst_d3d11_device_get_device_handle (device);
    hr = device_handle->CheckFeatureSupport (D3D11_FEATURE_D3D11_OPTIONS4,
        &options, sizeof (options));
    if (!gst_d3d11_result (hr, device) ||
        !options.ExtendedNV12SharedTextureSupported) {
      GST_DEBUG_OBJECT (device,
          "Given d3d11 device cannot support NV12 format for shared texture");
      gst_object_unref (device);
      continue;
    }

    /* can we bind NV12 texture for encoder? */
    hr = device_handle->CheckFormatSupport (DXGI_FORMAT_NV12, &supported);

    if (!gst_d3d11_result (hr, device)) {
      GST_DEBUG_OBJECT (device, "Couldn't query format support");
      gst_object_unref (device);
      continue;
    } else if ((supported & D3D11_FORMAT_SUPPORT_VIDEO_ENCODER) == 0) {
      GST_DEBUG_OBJECT (device, "We cannot bind NV12 format for encoding");
      gst_object_unref (device);
      continue;
    }

    ret = g_list_append (ret, device);
  }

  return ret;
}
#endif

static gboolean
plugin_init (GstPlugin * plugin)
{
  HRESULT hr;
  guint rank = GST_RANK_SECONDARY;
  GList *device_list = nullptr;

  GST_DEBUG_CATEGORY_INIT (gst_mf_debug, "mf", 0, "media foundation");
  GST_DEBUG_CATEGORY_INIT (gst_mf_utils_debug,
      "mfutils", 0, "media foundation utility functions");
  GST_DEBUG_CATEGORY_INIT (gst_mf_source_object_debug,
      "mfsourceobject", 0, "mfsourceobject");
  GST_DEBUG_CATEGORY_INIT (gst_mf_transform_debug,
      "mftransform", 0, "mftransform");
  GST_DEBUG_CATEGORY_INIT (gst_mf_video_buffer_debug,
      "mfvideobuffer", 0, "mfvideobuffer");
  GST_DEBUG_CATEGORY_INIT (gst_mf_video_encoder_debug,
      "mfvideoencoder", 0, "mfvideoencoder");

  hr = MFStartup (MF_VERSION, MFSTARTUP_NOSOCKET);
  if (!gst_mf_result (hr)) {
    GST_WARNING ("MFStartup failure, hr: 0x%x", hr);
    return TRUE;
  }

  /* mfvideosrc should be primary rank for UWP */
#if (GST_MF_WINAPI_APP && !GST_MF_WINAPI_DESKTOP)
  rank = GST_RANK_PRIMARY + 1;
#endif

  /* FIXME: In order to create MFT for a specific GPU, MFTEnum2() API is
   * required API but it's desktop only.
   * So, resulting MFT and D3D11 might not be compatible in case of multi-GPU
   * environment on UWP. */
#if GST_MF_HAVE_D3D11
  if (gst_mf_plat_load_library ())
    device_list = get_d3d11_devices ();
#endif

  gst_element_register (plugin, "mfvideosrc", rank, GST_TYPE_MF_VIDEO_SRC);
  gst_device_provider_register (plugin, "mfdeviceprovider",
      rank, GST_TYPE_MF_DEVICE_PROVIDER);

  gst_mf_h264_enc_plugin_init (plugin, GST_RANK_SECONDARY, device_list);
  gst_mf_h265_enc_plugin_init (plugin, GST_RANK_SECONDARY, device_list);
  gst_mf_vp9_enc_plugin_init (plugin, GST_RANK_SECONDARY, device_list);

  if (device_list)
    g_list_free_full (device_list, gst_object_unref);

  gst_mf_aac_enc_plugin_init (plugin, GST_RANK_SECONDARY);
  gst_mf_mp3_enc_plugin_init (plugin, GST_RANK_SECONDARY);
  gst_mf_aac_dec_plugin_init (plugin, GST_RANK_SECONDARY);
  gst_mf_mp3_dec_plugin_init (plugin, GST_RANK_SECONDARY);

  /* So that call MFShutdown() when this plugin is no more used
   * (i.e., gst_deinit). Otherwise valgrind-like tools would complain
   * about un-released media foundation resources.
   *
   * NOTE: MFStartup and MFShutdown can be called multiple times, but the number
   * of each MFStartup and MFShutdown call should be identical. This rule is
   * simliar to that of CoInitialize/CoUninitialize pair */
  g_object_set_data_full (G_OBJECT (plugin),
      "plugin-mediafoundation-shutdown", (gpointer) "shutdown-data",
      (GDestroyNotify) plugin_deinit);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    mediafoundation,
    "Microsoft Media Foundation plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
