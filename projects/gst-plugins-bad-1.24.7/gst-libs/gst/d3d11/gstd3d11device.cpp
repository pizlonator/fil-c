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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d11device.h"
#include "gstd3d11device-private.h"
#include "gstd3d11utils.h"
#include "gstd3d11format.h"
#include "gstd3d11-private.h"
#include "gstd3d11memory.h"
#include "gstd3d11compile.h"
#include "gstd3d11shadercache.h"
#include <gmodule.h>
#include <wrl.h>

#include <windows.h>
#include <versionhelpers.h>
#include <map>
#include <utility>
#include <atomic>
#include <mutex>
#include <string.h>
#include <unordered_map>

/**
 * SECTION:gstd3d11device
 * @title: GstD3D11Device
 * @short_description: Direct3D11 device abstraction
 *
 * #GstD3D11Device wraps ID3D11Device and ID3D11DeviceContext for GPU resources
 * to be able to be shared among various elements. Caller can get native
 * Direct3D11 handles via getter method.
 * Basically Direct3D11 API doesn't require dedicated thread like that of
 * OpenGL context, and ID3D11Device APIs are supposed to be thread-safe.
 * But concurrent call for ID3D11DeviceContext and DXGI API are not allowed.
 * To protect such object, callers need to make use of gst_d3d11_device_lock()
 * and gst_d3d11_device_unlock()
 */

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

#if HAVE_D3D11SDKLAYERS_H
#include <d3d11sdklayers.h>

/* mingw header does not define D3D11_RLDO_IGNORE_INTERNAL
 * D3D11_RLDO_SUMMARY = 0x1,
   D3D11_RLDO_DETAIL = 0x2,
 * D3D11_RLDO_IGNORE_INTERNAL = 0x4
 */
#define GST_D3D11_RLDO_FLAGS (0x2 | 0x4)
#endif

#if HAVE_DXGIDEBUG_H
#include <dxgidebug.h>
typedef HRESULT (WINAPI * DXGIGetDebugInterface_t) (REFIID riid,
    void **ppDebug);
static DXGIGetDebugInterface_t GstDXGIGetDebugInterface = nullptr;

#endif

#if (HAVE_D3D11SDKLAYERS_H || HAVE_DXGIDEBUG_H)
GST_DEBUG_CATEGORY_STATIC (gst_d3d11_debug_layer_debug);
#endif
GST_DEBUG_CATEGORY_STATIC (gst_d3d11_device_debug);
#define GST_CAT_DEFAULT gst_d3d11_device_debug

enum
{
  PROP_0,
  PROP_ADAPTER,
  PROP_DEVICE_ID,
  PROP_VENDOR_ID,
  PROP_HARDWARE,
  PROP_DESCRIPTION,
  PROP_CREATE_FLAGS,
  PROP_ADAPTER_LUID,
};

#define DEFAULT_ADAPTER 0
#define DEFAULT_CREATE_FLAGS 0

/* *INDENT-OFF* */
struct _GstD3D11DevicePrivate
{
  guint adapter = 0;
  guint device_id = 0;
  guint vendor_id = 0;
  gboolean hardware = 0;
  gchar *description = nullptr;
  guint create_flags = 0;
  gint64 adapter_luid = 0;

  ID3D11Device *device = nullptr;
  ID3D11Device5 *device5 = nullptr;
  ID3D11DeviceContext *device_context = nullptr;
  ID3D11DeviceContext4 *device_context4 = nullptr;

  ID3D11VideoDevice *video_device = nullptr;
  ID3D11VideoContext *video_context = nullptr;

  IDXGIFactory1 *factory = nullptr;
  std::unordered_map<GstVideoFormat, GstD3D11Format> format_table;

  std::recursive_mutex extern_lock;
  std::mutex resource_lock;

  LARGE_INTEGER frequency;

  D3D_FEATURE_LEVEL feature_level;
  std::map <gint64, ComPtr<ID3D11PixelShader>> ps_cache;
  std::map <gint64,
      std::pair<ComPtr<ID3D11VertexShader>, ComPtr<ID3D11InputLayout>>> vs_cache;
  std::map <D3D11_FILTER, ComPtr<ID3D11SamplerState>> sampler_cache;

  ID3D11RasterizerState *rs = nullptr;
  ID3D11RasterizerState *rs_msaa = nullptr;

#if HAVE_D3D11SDKLAYERS_H
  ID3D11Debug *d3d11_debug = nullptr;
  ID3D11InfoQueue *d3d11_info_queue = nullptr;
#endif

#if HAVE_DXGIDEBUG_H
  IDXGIDebug *dxgi_debug = nullptr;
  IDXGIInfoQueue *dxgi_info_queue = nullptr;
#endif
};
/* *INDENT-ON* */

static void
debug_init_once (void)
{
  GST_D3D11_CALL_ONCE_BEGIN {
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_device_debug,
        "d3d11device", 0, "d3d11 device object");
#if defined(HAVE_D3D11SDKLAYERS_H) || defined(HAVE_DXGIDEBUG_H)
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_debug_layer_debug,
        "d3d11debuglayer", 0, "native d3d11 and dxgi debug");
#endif
  } GST_D3D11_CALL_ONCE_END;
}

#define gst_d3d11_device_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstD3D11Device, gst_d3d11_device, GST_TYPE_OBJECT,
    debug_init_once ());

static void gst_d3d11_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_d3d11_device_dispose (GObject * object);
static void gst_d3d11_device_finalize (GObject * object);

#if HAVE_D3D11SDKLAYERS_H
static gboolean
gst_d3d11_device_enable_d3d11_debug (void)
{
  static GModule *d3d11_debug_module = nullptr;
  /* If all below libraries are unavailable, d3d11 device would fail with
   * D3D11_CREATE_DEVICE_DEBUG flag */
  static const gchar *sdk_dll_names[] = {
    "d3d11sdklayers.dll",
    "d3d11_1sdklayers.dll",
    "d3d11_2sdklayers.dll",
    "d3d11_3sdklayers.dll",
  };

  GST_D3D11_CALL_ONCE_BEGIN {
    for (guint i = 0; i < G_N_ELEMENTS (sdk_dll_names); i++) {
      d3d11_debug_module = g_module_open (sdk_dll_names[i], G_MODULE_BIND_LAZY);
      if (d3d11_debug_module)
        return;
    }
  }
  GST_D3D11_CALL_ONCE_END;

  if (d3d11_debug_module)
    return TRUE;

  return FALSE;
}

static inline GstDebugLevel
d3d11_message_severity_to_gst (D3D11_MESSAGE_SEVERITY level)
{
  switch (level) {
    case D3D11_MESSAGE_SEVERITY_CORRUPTION:
    case D3D11_MESSAGE_SEVERITY_ERROR:
      return GST_LEVEL_ERROR;
    case D3D11_MESSAGE_SEVERITY_WARNING:
      return GST_LEVEL_WARNING;
    case D3D11_MESSAGE_SEVERITY_INFO:
      return GST_LEVEL_INFO;
    case D3D11_MESSAGE_SEVERITY_MESSAGE:
      return GST_LEVEL_DEBUG;
    default:
      break;
  }

  return GST_LEVEL_LOG;
}

void
gst_d3d11_device_d3d11_debug (GstD3D11Device * device,
    const gchar * file, const gchar * function, gint line)
{
  GstD3D11DevicePrivate *priv = device->priv;
  D3D11_MESSAGE *msg;
  SIZE_T msg_len = 0;
  HRESULT hr;
  UINT64 num_msg, i;
  ID3D11InfoQueue *info_queue = priv->d3d11_info_queue;

  if (!info_queue)
    return;

  num_msg = info_queue->GetNumStoredMessages ();

  for (i = 0; i < num_msg; i++) {
    GstDebugLevel level;

    hr = info_queue->GetMessage (i, NULL, &msg_len);

    if (FAILED (hr) || msg_len == 0) {
      return;
    }

    msg = (D3D11_MESSAGE *) g_malloc0 (msg_len);
    hr = info_queue->GetMessage (i, msg, &msg_len);

    level = d3d11_message_severity_to_gst (msg->Severity);
    if (msg->Category == D3D11_MESSAGE_CATEGORY_STATE_CREATION &&
        level > GST_LEVEL_ERROR) {
      /* Do not warn for live object, since there would be live object
       * when ReportLiveDeviceObjects was called */
      level = GST_LEVEL_INFO;
    }

    gst_debug_log (gst_d3d11_debug_layer_debug, level, file, function, line,
        G_OBJECT (device), "D3D11InfoQueue: %s", msg->pDescription);
    g_free (msg);
  }

  info_queue->ClearStoredMessages ();

  return;
}
#else
void
gst_d3d11_device_d3d11_debug (GstD3D11Device * device,
    const gchar * file, const gchar * function, gint line)
{
  /* do nothing */
  return;
}
#endif

#if HAVE_DXGIDEBUG_H
static gboolean
gst_d3d11_device_enable_dxgi_debug (void)
{
#if (!GST_D3D11_WINAPI_ONLY_APP)
  static GModule *dxgi_debug_module = nullptr;

  GST_D3D11_CALL_ONCE_BEGIN {
    dxgi_debug_module = g_module_open ("dxgidebug.dll", G_MODULE_BIND_LAZY);

    if (dxgi_debug_module)
      g_module_symbol (dxgi_debug_module,
          "DXGIGetDebugInterface", (gpointer *) & GstDXGIGetDebugInterface);
  }
  GST_D3D11_CALL_ONCE_END;

  if (!GstDXGIGetDebugInterface)
    return FALSE;
#endif

  return TRUE;
}

static HRESULT
gst_d3d11_device_dxgi_get_device_interface (REFIID riid, void **debug)
{
#if (!GST_D3D11_WINAPI_ONLY_APP)
  if (GstDXGIGetDebugInterface) {
    return GstDXGIGetDebugInterface (riid, debug);
  } else {
    return E_NOINTERFACE;
  }
#else
  return DXGIGetDebugInterface1 (0, riid, debug);
#endif
}

static inline GstDebugLevel
dxgi_info_queue_message_severity_to_gst (DXGI_INFO_QUEUE_MESSAGE_SEVERITY level)
{
  switch (level) {
    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION:
    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR:
      return GST_LEVEL_ERROR;
    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING:
      return GST_LEVEL_WARNING;
    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO:
      return GST_LEVEL_INFO;
    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_MESSAGE:
      return GST_LEVEL_DEBUG;
    default:
      break;
  }

  return GST_LEVEL_LOG;
}

void
gst_d3d11_device_dxgi_debug (GstD3D11Device * device,
    const gchar * file, const gchar * function, gint line)
{
  GstD3D11DevicePrivate *priv = device->priv;
  DXGI_INFO_QUEUE_MESSAGE *msg;
  SIZE_T msg_len = 0;
  HRESULT hr;
  UINT64 num_msg, i;
  IDXGIInfoQueue *info_queue = priv->dxgi_info_queue;

  if (!info_queue)
    return;

  num_msg = info_queue->GetNumStoredMessages (DXGI_DEBUG_ALL);

  for (i = 0; i < num_msg; i++) {
    GstDebugLevel level;

    hr = info_queue->GetMessage (DXGI_DEBUG_ALL, i, NULL, &msg_len);

    if (FAILED (hr) || msg_len == 0) {
      return;
    }

    msg = (DXGI_INFO_QUEUE_MESSAGE *) g_malloc0 (msg_len);
    hr = info_queue->GetMessage (DXGI_DEBUG_ALL, i, msg, &msg_len);

    level = dxgi_info_queue_message_severity_to_gst (msg->Severity);
    gst_debug_log (gst_d3d11_debug_layer_debug, level, file, function, line,
        G_OBJECT (device), "DXGIInfoQueue: %s", msg->pDescription);
    g_free (msg);
  }

  info_queue->ClearStoredMessages (DXGI_DEBUG_ALL);

  return;
}
#else
void
gst_d3d11_device_dxgi_debug (GstD3D11Device * device,
    const gchar * file, const gchar * function, gint line)
{
  /* do nothing */
  return;
}
#endif

static void
gst_d3d11_device_class_init (GstD3D11DeviceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamFlags readable_flags =
      (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  gobject_class->get_property = gst_d3d11_device_get_property;
  gobject_class->dispose = gst_d3d11_device_dispose;
  gobject_class->finalize = gst_d3d11_device_finalize;

  g_object_class_install_property (gobject_class, PROP_ADAPTER,
      g_param_spec_uint ("adapter", "Adapter",
          "DXGI Adapter index for creating device",
          0, G_MAXUINT32, DEFAULT_ADAPTER, readable_flags));

  g_object_class_install_property (gobject_class, PROP_DEVICE_ID,
      g_param_spec_uint ("device-id", "Device Id",
          "DXGI Device ID", 0, G_MAXUINT32, 0, readable_flags));

  g_object_class_install_property (gobject_class, PROP_VENDOR_ID,
      g_param_spec_uint ("vendor-id", "Vendor Id",
          "DXGI Vendor ID", 0, G_MAXUINT32, 0, readable_flags));

  g_object_class_install_property (gobject_class, PROP_HARDWARE,
      g_param_spec_boolean ("hardware", "Hardware",
          "Whether hardware device or not", TRUE, readable_flags));

  g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
      g_param_spec_string ("description", "Description",
          "Human readable device description", NULL, readable_flags));

  g_object_class_install_property (gobject_class, PROP_ADAPTER_LUID,
      g_param_spec_int64 ("adapter-luid", "Adapter LUID",
          "DXGI Adapter LUID (Locally Unique Identifier) of created device",
          G_MININT64, G_MAXINT64, 0, readable_flags));

  gst_d3d11_memory_init_once ();
}

static void
gst_d3d11_device_init (GstD3D11Device * self)
{
  self->priv = new GstD3D11DevicePrivate ();
}

static gboolean
is_windows_8_or_greater (void)
{
  static gboolean ret = FALSE;

  GST_D3D11_CALL_ONCE_BEGIN {
#if (!GST_D3D11_WINAPI_ONLY_APP)
    if (IsWindows8OrGreater ())
      ret = TRUE;
#else
    ret = TRUE;
#endif
  } GST_D3D11_CALL_ONCE_END;

  return ret;
}

static guint
check_format_support (GstD3D11Device * self, DXGI_FORMAT format)
{
  GstD3D11DevicePrivate *priv = self->priv;
  ID3D11Device *handle = priv->device;
  HRESULT hr;
  UINT format_support;

  hr = handle->CheckFormatSupport (format, &format_support);
  if (FAILED (hr) || format_support == 0)
    return 0;

  return format_support;
}

static void
dump_format (GstD3D11Device * self, GstD3D11Format * format)
{
  gchar *format_support_str = g_flags_to_string (GST_TYPE_D3D11_FORMAT_SUPPORT,
      format->format_support[0]);

  GST_LOG_OBJECT (self, "%s -> %s (%d), "
      "resource format: %s (%d), %s (%d), %s (%d), %s (%d), flags (0x%x) %s",
      gst_video_format_to_string (format->format),
      gst_d3d11_dxgi_format_to_string (format->dxgi_format),
      format->dxgi_format,
      gst_d3d11_dxgi_format_to_string (format->resource_format[0]),
      format->resource_format[0],
      gst_d3d11_dxgi_format_to_string (format->resource_format[1]),
      format->resource_format[1],
      gst_d3d11_dxgi_format_to_string (format->resource_format[2]),
      format->resource_format[2],
      gst_d3d11_dxgi_format_to_string (format->resource_format[3]),
      format->resource_format[3], format->format_support[0],
      format_support_str);

  g_free (format_support_str);
}

static void
gst_d3d11_device_setup_format_table (GstD3D11Device * self)
{
  GstD3D11DevicePrivate *priv = self->priv;

  for (guint i = 0; i < G_N_ELEMENTS (_gst_d3d11_default_format_map); i++) {
    const GstD3D11Format *iter = &_gst_d3d11_default_format_map[i];
    GstD3D11Format format;
    guint support[GST_VIDEO_MAX_PLANES] = { 0, };
    gboolean native = TRUE;

    switch (iter->format) {
        /* RGB/GRAY */
      case GST_VIDEO_FORMAT_BGRA:
      case GST_VIDEO_FORMAT_BGRx:
      case GST_VIDEO_FORMAT_RGBA:
      case GST_VIDEO_FORMAT_RGBx:
      case GST_VIDEO_FORMAT_RGB10A2_LE:
      case GST_VIDEO_FORMAT_RGBA64_LE:
      case GST_VIDEO_FORMAT_GRAY8:
      case GST_VIDEO_FORMAT_GRAY16_LE:
        support[0] = check_format_support (self, iter->dxgi_format);
        if (!support[0]) {
          const gchar *format_name =
              gst_d3d11_dxgi_format_to_string (iter->dxgi_format);
          GST_INFO_OBJECT (self, "DXGI_FORMAT_%s (%d) for %s is not supported",
              format_name, (guint) iter->dxgi_format,
              gst_video_format_to_string (iter->format));
          continue;
        }
        break;
        /* YUV DXGI native formats */
      case GST_VIDEO_FORMAT_VUYA:
      case GST_VIDEO_FORMAT_Y410:
      case GST_VIDEO_FORMAT_NV12:
      case GST_VIDEO_FORMAT_P010_10LE:
      case GST_VIDEO_FORMAT_P012_LE:
      case GST_VIDEO_FORMAT_P016_LE:
      case GST_VIDEO_FORMAT_YUY2:
      case GST_VIDEO_FORMAT_Y210:
      case GST_VIDEO_FORMAT_Y212_LE:
      case GST_VIDEO_FORMAT_Y412_LE:
      case GST_VIDEO_FORMAT_BGRA64_LE:
      case GST_VIDEO_FORMAT_BGR10A2_LE:
      case GST_VIDEO_FORMAT_RBGA:
      {
        gboolean supported = TRUE;

        if (is_windows_8_or_greater ())
          support[0] = check_format_support (self, iter->dxgi_format);

        if (!support[0]) {
          GST_DEBUG_OBJECT (self,
              "DXGI_FORMAT_%s (%d) for %s is not supported, "
              "checking resource format",
              gst_d3d11_dxgi_format_to_string (iter->dxgi_format),
              (guint) iter->dxgi_format,
              gst_video_format_to_string (iter->format));

          native = FALSE;
          for (guint j = 0; j < GST_VIDEO_MAX_PLANES; j++) {
            if (iter->resource_format[j] == DXGI_FORMAT_UNKNOWN)
              break;

            support[j] = check_format_support (self, iter->resource_format[j]);
            if (support[j] == 0) {
              supported = FALSE;
              break;
            }
          }

          if (!supported) {
            GST_INFO_OBJECT (self, "%s is not supported",
                gst_video_format_to_string (iter->format));
            continue;
          }
        }
        break;
      }
        /* non-DXGI native formats */
      case GST_VIDEO_FORMAT_NV21:
      case GST_VIDEO_FORMAT_I420:
      case GST_VIDEO_FORMAT_YV12:
      case GST_VIDEO_FORMAT_I420_10LE:
      case GST_VIDEO_FORMAT_I420_12LE:
      case GST_VIDEO_FORMAT_Y42B:
      case GST_VIDEO_FORMAT_I422_10LE:
      case GST_VIDEO_FORMAT_I422_12LE:
      case GST_VIDEO_FORMAT_Y444:
      case GST_VIDEO_FORMAT_Y444_10LE:
      case GST_VIDEO_FORMAT_Y444_12LE:
      case GST_VIDEO_FORMAT_Y444_16LE:
      case GST_VIDEO_FORMAT_AYUV:
      case GST_VIDEO_FORMAT_AYUV64:
      case GST_VIDEO_FORMAT_UYVY:
      case GST_VIDEO_FORMAT_VYUY:
      case GST_VIDEO_FORMAT_YVYU:
      case GST_VIDEO_FORMAT_ARGB:
      case GST_VIDEO_FORMAT_xRGB:
      case GST_VIDEO_FORMAT_ABGR:
      case GST_VIDEO_FORMAT_xBGR:
      case GST_VIDEO_FORMAT_RGB:
      case GST_VIDEO_FORMAT_BGR:
      case GST_VIDEO_FORMAT_v210:
      case GST_VIDEO_FORMAT_v216:
      case GST_VIDEO_FORMAT_v308:
      case GST_VIDEO_FORMAT_IYU2:
      case GST_VIDEO_FORMAT_RGB16:
      case GST_VIDEO_FORMAT_BGR16:
      case GST_VIDEO_FORMAT_RGB15:
      case GST_VIDEO_FORMAT_BGR15:
      case GST_VIDEO_FORMAT_r210:
        /* RGB planar formats */
      case GST_VIDEO_FORMAT_RGBP:
      case GST_VIDEO_FORMAT_BGRP:
      case GST_VIDEO_FORMAT_GBR:
      case GST_VIDEO_FORMAT_GBR_10LE:
      case GST_VIDEO_FORMAT_GBR_12LE:
      case GST_VIDEO_FORMAT_GBR_16LE:
      case GST_VIDEO_FORMAT_GBRA:
      case GST_VIDEO_FORMAT_GBRA_10LE:
      case GST_VIDEO_FORMAT_GBRA_12LE:
      {
        gboolean supported = TRUE;

        native = FALSE;
        for (guint j = 0; j < GST_VIDEO_MAX_PLANES; j++) {
          if (iter->resource_format[j] == DXGI_FORMAT_UNKNOWN)
            break;

          support[j] = check_format_support (self, iter->resource_format[j]);
          if (support[j] == 0) {
            supported = FALSE;
            break;
          }
        }

        if (!supported) {
          GST_INFO_OBJECT (self, "%s is not supported",
              gst_video_format_to_string (iter->format));
          continue;
        }
        break;
      }
      default:
        g_assert_not_reached ();
        return;
    }

    format = *iter;

    if (!native)
      format.dxgi_format = DXGI_FORMAT_UNKNOWN;

    for (guint j = 0; j < GST_VIDEO_MAX_PLANES; j++)
      format.format_support[j] = support[j];

#ifndef GST_DISABLE_GST_DEBUG
    if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_LOG)
      dump_format (self, &format);
#endif

    priv->format_table[format.format] = format;
  }
}

static void
gst_d3d11_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11Device *self = GST_D3D11_DEVICE (object);
  GstD3D11DevicePrivate *priv = self->priv;

  switch (prop_id) {
    case PROP_ADAPTER:
      g_value_set_uint (value, priv->adapter);
      break;
    case PROP_DEVICE_ID:
      g_value_set_uint (value, priv->device_id);
      break;
    case PROP_VENDOR_ID:
      g_value_set_uint (value, priv->vendor_id);
      break;
    case PROP_HARDWARE:
      g_value_set_boolean (value, priv->hardware);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, priv->description);
      break;
    case PROP_ADAPTER_LUID:
      g_value_set_int64 (value, priv->adapter_luid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

void
gst_d3d11_device_log_live_objects (GstD3D11Device * device,
    const gchar * file, const gchar * function, gint line)
{
#if HAVE_D3D11SDKLAYERS_H
  if (device->priv->d3d11_debug) {
    device->priv->d3d11_debug->ReportLiveDeviceObjects ((D3D11_RLDO_FLAGS)
        GST_D3D11_RLDO_FLAGS);
  }

  if (device->priv->d3d11_info_queue)
    gst_d3d11_device_d3d11_debug (device, file, function, line);
#endif

#if HAVE_DXGIDEBUG_H
  if (device->priv->dxgi_debug) {
    device->priv->dxgi_debug->ReportLiveObjects (DXGI_DEBUG_ALL,
        (DXGI_DEBUG_RLO_FLAGS) GST_D3D11_RLDO_FLAGS);
  }

  if (device->priv->dxgi_info_queue)
    gst_d3d11_device_dxgi_debug (device, file, function, line);
#endif
}

static void
gst_d3d11_device_dispose (GObject * object)
{
  GstD3D11Device *self = GST_D3D11_DEVICE (object);
  GstD3D11DevicePrivate *priv = self->priv;

  GST_LOG_OBJECT (self, "dispose");

  priv->ps_cache.clear ();
  priv->vs_cache.clear ();
  priv->sampler_cache.clear ();

  GST_D3D11_CLEAR_COM (priv->rs);
  GST_D3D11_CLEAR_COM (priv->rs_msaa);
  GST_D3D11_CLEAR_COM (priv->device5);
  GST_D3D11_CLEAR_COM (priv->device_context4);
  GST_D3D11_CLEAR_COM (priv->video_device);
  GST_D3D11_CLEAR_COM (priv->video_context);
  GST_D3D11_CLEAR_COM (priv->device);
  GST_D3D11_CLEAR_COM (priv->device_context);
  GST_D3D11_CLEAR_COM (priv->factory);
  gst_d3d11_device_log_live_objects (self, __FILE__, GST_FUNCTION, __LINE__);

#if HAVE_D3D11SDKLAYERS_H
  GST_D3D11_CLEAR_COM (priv->d3d11_debug);
  GST_D3D11_CLEAR_COM (priv->d3d11_info_queue);
#endif

#if HAVE_DXGIDEBUG_H
  GST_D3D11_CLEAR_COM (priv->dxgi_debug);
  GST_D3D11_CLEAR_COM (priv->dxgi_info_queue);
#endif

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_d3d11_device_finalize (GObject * object)
{
  GstD3D11Device *self = GST_D3D11_DEVICE (object);
  GstD3D11DevicePrivate *priv = self->priv;

  GST_LOG_OBJECT (self, "finalize");

  g_free (priv->description);

  delete priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

typedef enum
{
  DEVICE_CONSTRUCT_FOR_ADAPTER_INDEX,
  DEVICE_CONSTRUCT_FOR_ADAPTER_LUID,
  DEVICE_CONSTRUCT_WRAPPED,
} GstD3D11DeviceConstructType;

typedef struct _GstD3D11DeviceConstructData
{
  union
  {
    guint adapter_index;
    gint64 adapter_luid;
    ID3D11Device *device;
  } data;
  GstD3D11DeviceConstructType type;
  UINT create_flags;
} GstD3D11DeviceConstructData;

static HRESULT
_gst_d3d11_device_get_adapter (const GstD3D11DeviceConstructData * data,
    IDXGIFactory1 * factory, guint * index, DXGI_ADAPTER_DESC * adapter_desc,
    IDXGIAdapter1 ** dxgi_adapter)
{
  HRESULT hr = S_OK;
  ComPtr < IDXGIAdapter1 > adapter1;
  DXGI_ADAPTER_DESC desc;

  switch (data->type) {
    case DEVICE_CONSTRUCT_FOR_ADAPTER_INDEX:
    {
      hr = factory->EnumAdapters1 (data->data.adapter_index, &adapter1);
      if (FAILED (hr))
        return hr;

      hr = adapter1->GetDesc (&desc);
      if (FAILED (hr))
        return hr;

      *index = data->data.adapter_index;
      *adapter_desc = desc;
      *dxgi_adapter = adapter1.Detach ();

      return S_OK;
    }
    case DEVICE_CONSTRUCT_FOR_ADAPTER_LUID:
    {
      for (guint i = 0;; i++) {
        gint64 luid;

        adapter1 = nullptr;

        hr = factory->EnumAdapters1 (i, &adapter1);
        if (FAILED (hr))
          return hr;

        hr = adapter1->GetDesc (&desc);
        if (FAILED (hr))
          continue;

        luid = gst_d3d11_luid_to_int64 (&desc.AdapterLuid);
        if (luid != data->data.adapter_luid)
          continue;

        *index = i;
        *adapter_desc = desc;
        *dxgi_adapter = adapter1.Detach ();

        return S_OK;
      }

      return E_FAIL;
    }
    case DEVICE_CONSTRUCT_WRAPPED:
    {
      ComPtr < IDXGIDevice > dxgi_device;
      ComPtr < IDXGIAdapter > adapter;
      ID3D11Device *device = data->data.device;

      hr = device->QueryInterface (IID_PPV_ARGS (&dxgi_device));
      if (FAILED (hr))
        return hr;

      hr = dxgi_device->GetAdapter (&adapter);
      if (FAILED (hr))
        return hr;

      hr = adapter.As (&adapter1);
      if (FAILED (hr))
        return hr;

      hr = adapter1->GetDesc (&desc);
      if (FAILED (hr))
        return hr;

      auto luid = gst_d3d11_luid_to_int64 (&desc.AdapterLuid);

      for (guint i = 0;; i++) {
        DXGI_ADAPTER_DESC tmp_desc;
        ComPtr < IDXGIAdapter1 > tmp;

        hr = factory->EnumAdapters1 (i, &tmp);
        if (FAILED (hr))
          return hr;

        hr = tmp->GetDesc (&tmp_desc);
        if (FAILED (hr))
          continue;

        if (luid != gst_d3d11_luid_to_int64 (&tmp_desc.AdapterLuid))
          continue;

        *index = i;
        *adapter_desc = desc;
        *dxgi_adapter = adapter1.Detach ();

        return S_OK;
      }

      return E_FAIL;
    }
    default:
      g_assert_not_reached ();
      break;
  }

  return E_FAIL;
}

static void
gst_d3d11_device_setup_debug_layer (GstD3D11Device * self)
{
#if HAVE_DXGIDEBUG_H
  if (gst_debug_category_get_threshold (gst_d3d11_debug_layer_debug) >
      GST_LEVEL_ERROR) {
    GstD3D11DevicePrivate *priv = self->priv;

    if (gst_d3d11_device_enable_dxgi_debug ()) {
      IDXGIDebug *debug = nullptr;
      IDXGIInfoQueue *info_queue = nullptr;
      HRESULT hr;

      GST_CAT_INFO_OBJECT (gst_d3d11_debug_layer_debug, self,
          "dxgi debug library was loaded");
      hr = gst_d3d11_device_dxgi_get_device_interface (IID_PPV_ARGS (&debug));

      if (SUCCEEDED (hr)) {
        GST_CAT_INFO_OBJECT (gst_d3d11_debug_layer_debug, self,
            "IDXGIDebug interface available");
        priv->dxgi_debug = debug;

        hr = gst_d3d11_device_dxgi_get_device_interface (IID_PPV_ARGS
            (&info_queue));
        if (SUCCEEDED (hr)) {
          GST_CAT_INFO_OBJECT (gst_d3d11_debug_layer_debug, self,
              "IDXGIInfoQueue interface available");
          priv->dxgi_info_queue = info_queue;
        }
      }
    } else {
      GST_CAT_INFO_OBJECT (gst_d3d11_debug_layer_debug, self,
          "couldn't load dxgi debug library");
    }
  }
#endif

#if HAVE_D3D11SDKLAYERS_H
  if ((self->priv->create_flags & D3D11_CREATE_DEVICE_DEBUG) != 0) {
    GstD3D11DevicePrivate *priv = self->priv;
    ID3D11Debug *debug;
    ID3D11InfoQueue *info_queue;
    HRESULT hr;

    hr = priv->device->QueryInterface (IID_PPV_ARGS (&debug));

    if (SUCCEEDED (hr)) {
      GST_CAT_INFO_OBJECT (gst_d3d11_debug_layer_debug, self,
          "D3D11Debug interface available");
      priv->d3d11_debug = debug;

      hr = priv->device->QueryInterface (IID_PPV_ARGS (&info_queue));
      if (SUCCEEDED (hr)) {
        GST_CAT_INFO_OBJECT (gst_d3d11_debug_layer_debug, self,
            "ID3D11InfoQueue interface available");
        priv->d3d11_info_queue = info_queue;
      }
    }
  }
#endif
}

static GstD3D11Device *
gst_d3d11_device_new_internal (const GstD3D11DeviceConstructData * data)
{
  ComPtr < IDXGIAdapter1 > adapter;
  ComPtr < IDXGIFactory1 > factory;
  ComPtr < ID3D11Device > device;
  ComPtr < ID3D11Device5 > device5;
  ComPtr < ID3D11DeviceContext > device_context;
  ComPtr < ID3D11DeviceContext4 > device_context4;
  HRESULT hr;
  UINT create_flags;
  guint adapter_index = 0;
  DXGI_ADAPTER_DESC adapter_desc;
  static const D3D_FEATURE_LEVEL feature_levels[] = {
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
  };
  D3D_FEATURE_LEVEL selected_level;

  debug_init_once ();

  hr = CreateDXGIFactory1 (IID_PPV_ARGS (&factory));
  if (!gst_d3d11_result (hr, NULL)) {
    GST_WARNING ("cannot create dxgi factory, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  create_flags = 0;
  if (data->type != DEVICE_CONSTRUCT_WRAPPED) {
    create_flags = data->create_flags;
#if HAVE_D3D11SDKLAYERS_H
    if (gst_debug_category_get_threshold (gst_d3d11_debug_layer_debug) >
        GST_LEVEL_ERROR) {
      /* DirectX SDK should be installed on system for this */
      if (gst_d3d11_device_enable_d3d11_debug ()) {
        GST_CAT_INFO (gst_d3d11_debug_layer_debug,
            "d3d11 debug library was loaded");
        create_flags |= D3D11_CREATE_DEVICE_DEBUG;
      } else {
        GST_CAT_INFO (gst_d3d11_debug_layer_debug,
            "couldn't load d3d11 debug library");
      }
    }
#endif
  }

  /* Ensure valid device handle */
  if (data->type == DEVICE_CONSTRUCT_WRAPPED) {
    ID3D11Device *external_device = data->data.device;

    hr = external_device->QueryInterface (IID_PPV_ARGS (&device));
    if (FAILED (hr)) {
      GST_WARNING ("Not a valid external ID3D11Device handle");
      return nullptr;
    }

    selected_level = device->GetFeatureLevel ();
    if (selected_level < D3D_FEATURE_LEVEL_10_0) {
      GST_ERROR ("Feature level 0x%x is not supported", (guint) selected_level);
      return nullptr;
    }

    device->GetImmediateContext (&device_context);
  }

  hr = _gst_d3d11_device_get_adapter (data, factory.Get (), &adapter_index,
      &adapter_desc, &adapter);
  if (FAILED (hr)) {
    GST_INFO ("Failed to get DXGI adapter");
    return nullptr;
  }

  if (data->type != DEVICE_CONSTRUCT_WRAPPED) {
    hr = D3D11CreateDevice (adapter.Get (), D3D_DRIVER_TYPE_UNKNOWN,
        NULL, create_flags, feature_levels, G_N_ELEMENTS (feature_levels),
        D3D11_SDK_VERSION, &device, &selected_level, &device_context);

    if (FAILED (hr)) {
      /* Retry if the system could not recognize D3D_FEATURE_LEVEL_11_1 */
      hr = D3D11CreateDevice (adapter.Get (), D3D_DRIVER_TYPE_UNKNOWN,
          NULL, create_flags, &feature_levels[1],
          G_N_ELEMENTS (feature_levels) - 1, D3D11_SDK_VERSION, &device,
          &selected_level, &device_context);
    }

    /* if D3D11_CREATE_DEVICE_DEBUG was enabled but couldn't create device,
     * try it without the flag again */
    if (FAILED (hr) && (create_flags & D3D11_CREATE_DEVICE_DEBUG) != 0) {
      create_flags &= ~D3D11_CREATE_DEVICE_DEBUG;

      hr = D3D11CreateDevice (adapter.Get (), D3D_DRIVER_TYPE_UNKNOWN,
          NULL, create_flags, feature_levels, G_N_ELEMENTS (feature_levels),
          D3D11_SDK_VERSION, &device, &selected_level, &device_context);

      if (FAILED (hr)) {
        /* Retry if the system could not recognize D3D_FEATURE_LEVEL_11_1 */
        hr = D3D11CreateDevice (adapter.Get (), D3D_DRIVER_TYPE_UNKNOWN,
            NULL, create_flags, &feature_levels[1],
            G_N_ELEMENTS (feature_levels) - 1, D3D11_SDK_VERSION, &device,
            &selected_level, &device_context);
      }
    }
  }

  if (FAILED (hr)) {
    switch (data->type) {
      case DEVICE_CONSTRUCT_FOR_ADAPTER_INDEX:
      {
        GST_INFO ("Failed to create d3d11 device for adapter index %d"
            " with flags 0x%x, hr: 0x%x", data->data.adapter_index,
            create_flags, (guint) hr);
        return nullptr;
      }
      case DEVICE_CONSTRUCT_FOR_ADAPTER_LUID:
      {
        GST_WARNING ("Failed to create d3d11 device for adapter luid %"
            G_GINT64_FORMAT " with flags 0x%x, hr: 0x%x",
            data->data.adapter_luid, create_flags, (guint) hr);
        return nullptr;
      }
      default:
        break;
    }

    return nullptr;
  }

  GstD3D11Device *self = nullptr;
  GstD3D11DevicePrivate *priv;

  self = (GstD3D11Device *) g_object_new (GST_TYPE_D3D11_DEVICE, nullptr);
  gst_object_ref_sink (self);

  priv = self->priv;

  hr = device.As (&device5);
  if (SUCCEEDED (hr))
    hr = device_context.As (&device_context4);
  if (SUCCEEDED (hr)) {
    priv->device5 = device5.Detach ();
    priv->device_context4 = device_context4.Detach ();
  }

  priv->adapter = adapter_index;
  priv->device = device.Detach ();
  priv->device_context = device_context.Detach ();
  priv->factory = factory.Detach ();

  priv->vendor_id = adapter_desc.VendorId;
  priv->device_id = adapter_desc.DeviceId;
  priv->description = g_utf16_to_utf8 ((gunichar2 *) adapter_desc.Description,
      -1, nullptr, nullptr, nullptr);
  priv->adapter_luid = gst_d3d11_luid_to_int64 (&adapter_desc.AdapterLuid);
  priv->feature_level = priv->device->GetFeatureLevel ();

  DXGI_ADAPTER_DESC1 desc1;
  hr = adapter->GetDesc1 (&desc1);

  /* DXGI_ADAPTER_FLAG_SOFTWARE is missing in dxgi.h of mingw */
  if (SUCCEEDED (hr) && (desc1.Flags & 0x2) != 0x2)
    priv->hardware = TRUE;

  priv->create_flags = create_flags;
  gst_d3d11_device_setup_format_table (self);
  gst_d3d11_device_setup_debug_layer (self);

  BOOL ret = QueryPerformanceFrequency (&priv->frequency);
  g_assert (ret);

  return self;
}

/**
 * gst_d3d11_device_new:
 * @adapter_index: the index of adapter for creating d3d11 device
 * @flags: a D3D11_CREATE_DEVICE_FLAG value used for creating d3d11 device
 *
 * Returns: (transfer full) (nullable): a new #GstD3D11Device for @adapter_index
 * or %NULL when failed to create D3D11 device with given adapter index.
 *
 * Since: 1.22
 */
GstD3D11Device *
gst_d3d11_device_new (guint adapter_index, guint flags)
{
  GstD3D11DeviceConstructData data;

  data.data.adapter_index = adapter_index;
  data.type = DEVICE_CONSTRUCT_FOR_ADAPTER_INDEX;
  data.create_flags = flags;

  return gst_d3d11_device_new_internal (&data);
}

/**
 * gst_d3d11_device_new_for_adapter_luid:
 * @adapter_luid: an int64 representation of the DXGI adapter LUID
 * @flags: a D3D11_CREATE_DEVICE_FLAG value used for creating d3d11 device
 *
 * Returns: (transfer full) (nullable): a new #GstD3D11Device for @adapter_luid
 * or %NULL when failed to create D3D11 device with given adapter luid.
 *
 * Since: 1.22
 */
GstD3D11Device *
gst_d3d11_device_new_for_adapter_luid (gint64 adapter_luid, guint flags)
{
  GstD3D11DeviceConstructData data;

  data.data.adapter_luid = adapter_luid;
  data.type = DEVICE_CONSTRUCT_FOR_ADAPTER_LUID;
  data.create_flags = flags;

  return gst_d3d11_device_new_internal (&data);
}

/**
 * gst_d3d11_device_new_wrapped:
 * @device: (transfer none): an existing ID3D11Device handle
 *
 * Returns: (transfer full) (nullable): a new #GstD3D11Device for @device
 * or %NULL if an error occurred
 *
 * Since: 1.22
 */
GstD3D11Device *
gst_d3d11_device_new_wrapped (ID3D11Device * device)
{
  GstD3D11DeviceConstructData data;

  g_return_val_if_fail (device != nullptr, nullptr);

  data.data.device = device;
  data.type = DEVICE_CONSTRUCT_WRAPPED;
  data.create_flags = 0;

  return gst_d3d11_device_new_internal (&data);
}

/**
 * gst_d3d11_device_get_device_handle:
 * @device: a #GstD3D11Device
 *
 * Used for various D3D11 APIs directly. Caller must not destroy returned device
 * object.
 *
 * Returns: (transfer none): the ID3D11Device handle
 *
 * Since: 1.22
 */
ID3D11Device *
gst_d3d11_device_get_device_handle (GstD3D11Device * device)
{
  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), NULL);

  return device->priv->device;
}

/**
 * gst_d3d11_device_get_device_context_handle:
 * @device: a #GstD3D11Device
 *
 * Used for various D3D11 APIs directly. Caller must not destroy returned device
 * object. Any ID3D11DeviceContext call needs to be protected by
 * gst_d3d11_device_lock() and gst_d3d11_device_unlock() method.
 *
 * Returns: (transfer none): the immeidate ID3D11DeviceContext handle
 *
 * Since: 1.22
 */
ID3D11DeviceContext *
gst_d3d11_device_get_device_context_handle (GstD3D11Device * device)
{
  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), NULL);

  return device->priv->device_context;
}

/**
 * gst_d3d11_device_get_dxgi_factory_handle:
 * @device: a #GstD3D11Device
 *
 * Used for various D3D11 APIs directly. Caller must not destroy returned device
 * object.
 *
 * Returns: (transfer none): the IDXGIFactory1 handle
 *
 * Since: 1.22
 */
IDXGIFactory1 *
gst_d3d11_device_get_dxgi_factory_handle (GstD3D11Device * device)
{
  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), NULL);

  return device->priv->factory;
}

/**
 * gst_d3d11_device_get_video_device_handle:
 * @device: a #GstD3D11Device
 *
 * Used for various D3D11 APIs directly. Caller must not destroy returned device
 * object.
 *
 * Returns: (nullable) (transfer none) : the ID3D11VideoDevice handle or %NULL
 * if ID3D11VideoDevice is unavailable.
 *
 * Since: 1.22
 */
ID3D11VideoDevice *
gst_d3d11_device_get_video_device_handle (GstD3D11Device * device)
{
  GstD3D11DevicePrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), NULL);

  priv = device->priv;
  std::lock_guard < std::mutex > lk (priv->resource_lock);
  if (!priv->video_device) {
    HRESULT hr;
    ID3D11VideoDevice *video_device = NULL;

    hr = priv->device->QueryInterface (IID_PPV_ARGS (&video_device));
    if (gst_d3d11_result (hr, device))
      priv->video_device = video_device;
  }

  return priv->video_device;
}

/**
 * gst_d3d11_device_get_video_context_handle:
 * @device: a #GstD3D11Device
 *
 * Used for various D3D11 APIs directly. Caller must not destroy returned device
 * object.
 *
 * Returns: (nullable) (transfer none): the ID3D11VideoContext handle or %NULL
 * if ID3D11VideoContext is unavailable.
 *
 * Since: 1.22
 */
ID3D11VideoContext *
gst_d3d11_device_get_video_context_handle (GstD3D11Device * device)
{
  GstD3D11DevicePrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), NULL);

  priv = device->priv;
  std::lock_guard < std::mutex > lk (priv->resource_lock);
  if (!priv->video_context) {
    HRESULT hr;
    ID3D11VideoContext *video_context = NULL;

    hr = priv->device_context->QueryInterface (IID_PPV_ARGS (&video_context));
    if (gst_d3d11_result (hr, device))
      priv->video_context = video_context;
  }

  return priv->video_context;
}

/**
 * gst_d3d11_device_lock:
 * @device: a #GstD3D11Device
 *
 * Take lock for @device. Any thread-unsafe API call needs to be
 * protected by this method. This call must be paired with
 * gst_d3d11_device_unlock()
 *
 * Since: 1.22
 */
void
gst_d3d11_device_lock (GstD3D11Device * device)
{
  GstD3D11DevicePrivate *priv;

  g_return_if_fail (GST_IS_D3D11_DEVICE (device));

  priv = device->priv;

  GST_TRACE_OBJECT (device, "device locking");
  priv->extern_lock.lock ();
  GST_TRACE_OBJECT (device, "device locked");
}

/**
 * gst_d3d11_device_unlock:
 * @device: a #GstD3D11Device
 *
 * Release lock for @device. This call must be paired with
 * gst_d3d11_device_lock()
 *
 * Since: 1.22
 */
void
gst_d3d11_device_unlock (GstD3D11Device * device)
{
  GstD3D11DevicePrivate *priv;

  g_return_if_fail (GST_IS_D3D11_DEVICE (device));

  priv = device->priv;

  priv->extern_lock.unlock ();
  GST_TRACE_OBJECT (device, "device unlocked");
}

/**
 * gst_d3d11_device_get_format:
 * @device: a #GstD3D11Device
 * @format: a #GstVideoFormat
 * @device_format: (out caller-allocates) (nullable): a #GstD3D11Format
 *
 * Converts @format to #GstD3D11Format if the @format is supported
 * by device
 *
 * Returns: %TRUE if @format is supported by @device
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_device_get_format (GstD3D11Device * device, GstVideoFormat format,
    GstD3D11Format * device_format)
{
  GstD3D11DevicePrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), FALSE);

  priv = device->priv;

  const auto & target = priv->format_table.find (format);
  if (target == priv->format_table.end ()) {
    if (device_format)
      gst_d3d11_format_init (device_format);

    return FALSE;
  }

  if (device_format)
    *device_format = target->second;

  return TRUE;
}

GST_DEFINE_MINI_OBJECT_TYPE (GstD3D11Fence, gst_d3d11_fence);

struct _GstD3D11FencePrivate
{
  UINT64 fence_value;
  ID3D11Fence *fence;
  ID3D11Query *query;
  HANDLE event_handle;
  gboolean signalled;
  gboolean synced;
};

static void
_gst_d3d11_fence_free (GstD3D11Fence * fence)
{
  GstD3D11FencePrivate *priv = fence->priv;

  GST_D3D11_CLEAR_COM (priv->fence);
  GST_D3D11_CLEAR_COM (priv->query);
  if (priv->event_handle)
    CloseHandle (priv->event_handle);

  gst_clear_object (&fence->device);

  g_free (priv);
  g_free (fence);
}

/**
 * gst_d3d11_device_create_fence:
 * @device: a #GstD3D11Device
 *
 * Creates fence object (i.e., ID3D11Fence) if available, otherwise
 * ID3D11Query with D3D11_QUERY_EVENT is created.
 *
 * Returns: a #GstD3D11Fence object
 *
 * Since: 1.22
 */
GstD3D11Fence *
gst_d3d11_device_create_fence (GstD3D11Device * device)
{
  GstD3D11DevicePrivate *priv;
  ID3D11Fence *fence = nullptr;
  HRESULT hr = S_OK;
  GstD3D11Fence *self;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), nullptr);

  priv = device->priv;

  if (priv->device5 && priv->device_context4) {
    hr = priv->device5->CreateFence (0, D3D11_FENCE_FLAG_NONE,
        IID_PPV_ARGS (&fence));

    if (!gst_d3d11_result (hr, device))
      GST_WARNING_OBJECT (device, "Failed to create fence object");
  }

  self = g_new0 (GstD3D11Fence, 1);
  self->device = (GstD3D11Device *) gst_object_ref (device);
  self->priv = g_new0 (GstD3D11FencePrivate, 1);
  self->priv->fence = fence;
  if (fence) {
    self->priv->event_handle = CreateEventEx (nullptr, nullptr,
        0, EVENT_ALL_ACCESS);
  }

  gst_mini_object_init (GST_MINI_OBJECT_CAST (self), 0,
      GST_TYPE_D3D11_FENCE, nullptr, nullptr,
      (GstMiniObjectFreeFunction) _gst_d3d11_fence_free);

  return self;
}

/**
 * gst_d3d11_fence_signal:
 * @fence: a #GstD3D11Fence
 *
 * Sets sync point to fence for waiting.
 * Must be called with gst_d3d11_device_lock() held
 *
 * Returns: %TRUE if successful
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_fence_signal (GstD3D11Fence * fence)
{
  HRESULT hr = S_OK;
  GstD3D11Device *device;
  GstD3D11DevicePrivate *device_priv;
  GstD3D11FencePrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_FENCE (fence), FALSE);

  device = fence->device;
  device_priv = device->priv;
  priv = fence->priv;

  priv->signalled = FALSE;
  priv->synced = FALSE;

  if (priv->fence) {
    priv->fence_value++;

    GST_LOG_OBJECT (device, "Signals with fence value %" G_GUINT64_FORMAT,
        priv->fence_value);

    hr = device_priv->device_context4->Signal (priv->fence, priv->fence_value);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR_OBJECT (device, "Failed to signal fence value %"
          G_GUINT64_FORMAT, fence->priv->fence_value);
      return FALSE;
    }
  } else {
    D3D11_QUERY_DESC desc;

    GST_D3D11_CLEAR_COM (priv->query);

    desc.Query = D3D11_QUERY_EVENT;
    desc.MiscFlags = 0;

    GST_LOG_OBJECT (device, "Creating query object");

    hr = device_priv->device->CreateQuery (&desc, &priv->query);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR_OBJECT (device, "Failed to create query object");
      return FALSE;
    }

    device_priv->device_context->End (priv->query);
  }

  priv->signalled = TRUE;

  return TRUE;
}

/**
 * gst_d3d11_fence_wait:
 * @fence: a #GstD3D11Fence
 *
 * Waits until previously issued GPU commands have been completed
 * Must be called with gst_d3d11_device_lock() held
 *
 * Returns: %TRUE if successful
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_fence_wait (GstD3D11Fence * fence)
{
  HRESULT hr = S_OK;
  GstD3D11Device *device;
  GstD3D11DevicePrivate *device_priv;
  GstD3D11FencePrivate *priv;
  BOOL timer_ret;
  LARGE_INTEGER current_time, now;

  g_return_val_if_fail (GST_IS_D3D11_FENCE (fence), FALSE);

  device = fence->device;
  device_priv = device->priv;
  priv = fence->priv;

  if (!priv->signalled) {
    GST_DEBUG_OBJECT (device, "Fence is not signalled, nothing to wait");
    return TRUE;
  }

  if (priv->synced) {
    GST_DEBUG_OBJECT (device, "Already synced");
    return TRUE;
  }

  timer_ret = QueryPerformanceCounter (&current_time);
  g_assert (timer_ret);

  now = current_time;

  if (priv->fence) {
    GST_LOG_OBJECT (device, "Waiting fence value %" G_GUINT64_FORMAT,
        priv->fence_value);

    if (fence->priv->fence->GetCompletedValue () < fence->priv->fence_value) {
      hr = fence->priv->fence->SetEventOnCompletion (fence->priv->fence_value,
          fence->priv->event_handle);
      if (!gst_d3d11_result (hr, device)) {
        GST_WARNING_OBJECT (device, "Failed set event handle");
        return FALSE;
      }

      /* 20 seconds should be sufficient time */
      DWORD ret = WaitForSingleObject (priv->event_handle, 20000);
      if (ret != WAIT_OBJECT_0) {
        GST_WARNING_OBJECT (device,
            "Failed to wait object, ret 0x%x", (guint) ret);
        return FALSE;
      }
    }
  } else {
    LONGLONG timeout;
    BOOL sync_done = FALSE;

    g_assert (priv->query != nullptr);

    /* 20 sec timeout */
    timeout = now.QuadPart + 20 * device_priv->frequency.QuadPart;

    GST_LOG_OBJECT (device, "Waiting event");

    while (now.QuadPart < timeout && !sync_done) {
      hr = device_priv->device_context->GetData (priv->query,
          &sync_done, sizeof (BOOL), 0);
      if (FAILED (hr)) {
        GST_WARNING_OBJECT (device, "Failed to get event data");
        return FALSE;
      }

      if (sync_done)
        break;

      g_thread_yield ();
      timer_ret = QueryPerformanceCounter (&now);
      g_assert (timer_ret);
    }

    if (!sync_done) {
      GST_WARNING_OBJECT (device, "Timeout");
      return FALSE;
    }

    GST_D3D11_CLEAR_COM (priv->query);
  }

#ifndef GST_DISABLE_GST_DEBUG
  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_LOG) {
    GstClockTime elapsed;

    QueryPerformanceCounter (&now);
    elapsed = gst_util_uint64_scale (now.QuadPart - current_time.QuadPart,
        GST_SECOND, device_priv->frequency.QuadPart);

    GST_LOG_OBJECT (device, "Wait done, elapsed %" GST_TIME_FORMAT,
        GST_TIME_ARGS (elapsed));
  }
#endif

  priv->signalled = FALSE;
  priv->synced = TRUE;

  return TRUE;
}

gint64
gst_d3d11_pixel_shader_token_new (void)
{
  /* *INDENT-OFF* */
  static std::atomic < gint64 > token_ { 0 };
  /* *INDENT-ON* */

  return token_.fetch_add (1);
}

gint64
gst_d3d11_vertex_shader_token_new (void)
{
  /* *INDENT-OFF* */
  static std::atomic < gint64 > token_ { 0 };
  /* *INDENT-ON* */

  return token_.fetch_add (1);
}

gint64
gst_d3d11_compute_shader_token_new (void)
{
  /* *INDENT-OFF* */
  static std::atomic < gint64 > token_ { 0 };
  /* *INDENT-ON* */

  return token_.fetch_add (1);
}

HRESULT
gst_d3d11_device_get_pixel_shader_uncached (GstD3D11Device * device,
    gint64 token, const void *bytecode, gsize bytecode_size,
    const gchar * source, gsize source_size, const gchar * entry_point,
    const D3D_SHADER_MACRO * defines, ID3D11PixelShader ** ps)
{
  GstD3D11DevicePrivate *priv = device->priv;
  HRESULT hr;
  ComPtr < ID3D11PixelShader > shader;
  ComPtr < ID3DBlob > blob;
  const void *data;
  gsize size;

  GST_LOG_OBJECT (device,
      "Creating pixel shader for token %" G_GINT64_FORMAT ", source:\n%s",
      token, source);

  if (bytecode && bytecode_size > 1) {
    data = bytecode;
    size = bytecode_size;
    GST_DEBUG_OBJECT (device,
        "Creating shader \"%s\" using precompiled bytecode", entry_point);
  } else {
    hr = gst_d3d11_shader_cache_get_pixel_shader_blob (token,
        source, source_size, entry_point, defines, &blob);
    if (!gst_d3d11_result (hr, device))
      return hr;

    data = blob->GetBufferPointer ();
    size = blob->GetBufferSize ();
  }

  hr = priv->device->CreatePixelShader (data, size, nullptr, &shader);
  if (!gst_d3d11_result (hr, device))
    return hr;

  GST_DEBUG_OBJECT (device,
      "Created pixel shader \"%s\" for token %" G_GINT64_FORMAT,
      entry_point, token);
  *ps = shader.Detach ();

  return S_OK;
}

HRESULT
gst_d3d11_device_get_pixel_shader (GstD3D11Device * device, gint64 token,
    const void *bytecode, gsize bytecode_size, const gchar * source,
    gsize source_size, const gchar * entry_point,
    const D3D_SHADER_MACRO * defines, ID3D11PixelShader ** ps)
{
  GstD3D11DevicePrivate *priv = device->priv;
  HRESULT hr;
  ComPtr < ID3D11PixelShader > shader;

  GST_DEBUG_OBJECT (device, "Getting pixel shader \"%s\" for token %"
      G_GINT64_FORMAT, entry_point, token);

  std::lock_guard < std::mutex > lk (priv->resource_lock);
  auto cached = priv->ps_cache.find (token);
  if (cached != priv->ps_cache.end ()) {
    GST_DEBUG_OBJECT (device,
        "Found cached pixel shader \"%s\" for token %" G_GINT64_FORMAT,
        entry_point, token);
    *ps = cached->second.Get ();
    (*ps)->AddRef ();
    return S_OK;
  }

  hr = gst_d3d11_device_get_pixel_shader_uncached (device, token, bytecode,
      bytecode_size, source, source_size, entry_point, defines, &shader);
  if (!gst_d3d11_result (hr, device))
    return hr;

  priv->ps_cache[token] = shader;
  *ps = shader.Detach ();

  return S_OK;
}

HRESULT
gst_d3d11_device_get_vertex_shader (GstD3D11Device * device, gint64 token,
    const void *bytecode, gsize bytecode_size, const gchar * source,
    gsize source_size, const gchar * entry_point,
    const D3D11_INPUT_ELEMENT_DESC * input_desc, guint desc_len,
    ID3D11VertexShader ** vs, ID3D11InputLayout ** layout)
{
  GstD3D11DevicePrivate *priv = device->priv;
  HRESULT hr;
  ComPtr < ID3D11VertexShader > shader;
  ComPtr < ID3D11InputLayout > input_layout;
  ComPtr < ID3DBlob > blob;
  const void *data;
  gsize size;

  GST_DEBUG_OBJECT (device, "Getting vertext shader \"%s\" for token %"
      G_GINT64_FORMAT, entry_point, token);

  std::lock_guard < std::mutex > lk (priv->resource_lock);
  auto cached = priv->vs_cache.find (token);
  if (cached != priv->vs_cache.end ()) {
    GST_DEBUG_OBJECT (device,
        "Found cached vertex shader \"%s\" for token %" G_GINT64_FORMAT,
        entry_point, token);
    *vs = cached->second.first.Get ();
    *layout = cached->second.second.Get ();
    (*vs)->AddRef ();
    (*layout)->AddRef ();
    return S_OK;
  }

  GST_LOG_OBJECT (device,
      "Creating vertex shader for token %" G_GINT64_FORMAT ", shader: \n%s",
      token, source);

  if (bytecode && bytecode_size > 1) {
    data = bytecode;
    size = bytecode_size;
    GST_DEBUG_OBJECT (device,
        "Creating shader \"%s\" using precompiled bytecode", entry_point);
  } else {
    hr = gst_d3d11_shader_cache_get_vertex_shader_blob (token,
        source, source_size, entry_point, &blob);
    if (!gst_d3d11_result (hr, device))
      return hr;

    data = blob->GetBufferPointer ();
    size = blob->GetBufferSize ();
  }

  hr = priv->device->CreateVertexShader (data, size, nullptr, &shader);
  if (!gst_d3d11_result (hr, device))
    return hr;

  hr = priv->device->CreateInputLayout (input_desc, desc_len, data,
      size, &input_layout);
  if (!gst_d3d11_result (hr, device))
    return hr;

  GST_DEBUG_OBJECT (device, "Created vertex shader \"%s\" for token %"
      G_GINT64_FORMAT, entry_point, token);
  priv->vs_cache[token] = std::make_pair (shader, input_layout);

  *vs = shader.Detach ();
  *layout = input_layout.Detach ();

  return S_OK;
}

HRESULT
gst_d3d11_device_get_sampler (GstD3D11Device * device, D3D11_FILTER filter,
    ID3D11SamplerState ** sampler)
{
  GstD3D11DevicePrivate *priv = device->priv;
  ComPtr < ID3D11SamplerState > state;
  D3D11_SAMPLER_DESC desc;
  HRESULT hr;

  std::lock_guard < std::mutex > lk (priv->resource_lock);
  auto cached = priv->sampler_cache.find (filter);
  if (cached != priv->sampler_cache.end ()) {
    *sampler = cached->second.Get ();
    (*sampler)->AddRef ();
    return S_OK;
  }

  memset (&desc, 0, sizeof (D3D11_SAMPLER_DESC));

  desc.Filter = filter;
  desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
  desc.MaxLOD = D3D11_FLOAT32_MAX;

  if (filter == D3D11_FILTER_ANISOTROPIC) {
    if (priv->feature_level > D3D_FEATURE_LEVEL_9_1)
      desc.MaxAnisotropy = 16;
    else
      desc.MaxAnisotropy = 2;
  }

  hr = priv->device->CreateSamplerState (&desc, &state);
  if (!gst_d3d11_result (hr, device))
    return hr;

  priv->sampler_cache[filter] = state;
  *sampler = state.Detach ();

  return S_OK;
}

HRESULT
gst_d3d11_device_get_rasterizer (GstD3D11Device * device,
    ID3D11RasterizerState ** rasterizer)
{
  GstD3D11DevicePrivate *priv = device->priv;
  D3D11_RASTERIZER_DESC desc;
  HRESULT hr;

  std::lock_guard < std::mutex > lk (priv->resource_lock);
  if (priv->rs) {
    *rasterizer = priv->rs;
    priv->rs->AddRef ();
    return S_OK;
  }

  memset (&desc, 0, sizeof (D3D11_RASTERIZER_DESC));
  desc.FillMode = D3D11_FILL_SOLID;
  desc.CullMode = D3D11_CULL_NONE;
  desc.DepthClipEnable = TRUE;

  hr = priv->device->CreateRasterizerState (&desc, rasterizer);
  if (!gst_d3d11_result (hr, device))
    return hr;

  priv->rs = *rasterizer;
  priv->rs->AddRef ();

  return S_OK;
}

HRESULT
gst_d3d11_device_get_rasterizer_msaa (GstD3D11Device * device,
    ID3D11RasterizerState ** rasterizer)
{
  GstD3D11DevicePrivate *priv = device->priv;
  D3D11_RASTERIZER_DESC desc;
  HRESULT hr;

  std::lock_guard < std::mutex > lk (priv->resource_lock);
  if (priv->rs_msaa) {
    *rasterizer = priv->rs_msaa;
    priv->rs_msaa->AddRef ();
    return S_OK;
  }

  memset (&desc, 0, sizeof (D3D11_RASTERIZER_DESC));
  desc.FillMode = D3D11_FILL_SOLID;
  desc.CullMode = D3D11_CULL_NONE;
  desc.DepthClipEnable = TRUE;
  desc.MultisampleEnable = TRUE;
  desc.AntialiasedLineEnable = TRUE;

  hr = priv->device->CreateRasterizerState (&desc, rasterizer);
  if (!gst_d3d11_result (hr, device))
    return hr;

  priv->rs_msaa = *rasterizer;
  priv->rs_msaa->AddRef ();

  return S_OK;
}
