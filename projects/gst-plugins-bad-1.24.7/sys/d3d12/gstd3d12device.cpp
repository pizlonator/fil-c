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

#include "gstd3d12.h"
#include "gstd3d12-private.h"
#include "gstd3d11on12.h"
#include <directx/d3dx12.h>
#include <wrl.h>
#include <vector>
#include <string.h>
#include <mutex>
#include <condition_variable>
#include <string>
#include <locale>
#include <codecvt>
#include <algorithm>
#include <d3d12sdklayers.h>
#include <memory>
#include <queue>
#include <unordered_map>
#include <thread>

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_device_debug);
GST_DEBUG_CATEGORY_STATIC (gst_d3d12_sdk_debug);
#define GST_CAT_DEFAULT gst_d3d12_device_debug

enum
{
  PROP_0,
  PROP_ADAPTER_INDEX,
  PROP_ADAPTER_LUID,
  PROP_DEVICE_ID,
  PROP_VENDOR_ID,
  PROP_HARDWARE,
  PROP_DESCRIPTION,
};

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

struct _GstD3D12DevicePrivate
{
  ~_GstD3D12DevicePrivate ()
  {
    auto hr = device->GetDeviceRemovedReason ();
    /* If device were not removed, make sure no pending command in queue */
    if (hr == S_OK) {
      if (direct_queue)
        gst_d3d12_command_queue_fence_wait (direct_queue, G_MAXUINT64, nullptr);

      if (copy_queue)
        gst_d3d12_command_queue_fence_wait (copy_queue, G_MAXUINT64, nullptr);
    }

    gst_clear_object (&direct_queue);
    gst_clear_object (&copy_queue);

    gst_clear_object (&direct_ca_pool);
    gst_clear_object (&direct_cl_pool);

    gst_clear_object (&copy_ca_pool);
    gst_clear_object (&copy_cl_pool);

    factory = nullptr;
    adapter = nullptr;
    d3d11on12 = nullptr;

    if (info_queue && device) {
      ComPtr <ID3D12DebugDevice> debug_dev;
      device.As (&debug_dev);
      if (debug_dev) {
        debug_dev->ReportLiveDeviceObjects (D3D12_RLDO_DETAIL |
            D3D12_RLDO_IGNORE_INTERNAL);

        UINT64 num_msg = info_queue->GetNumStoredMessages ();
        for (UINT64 i = 0; i < num_msg; i++) {
          HRESULT hr;
          SIZE_T msg_len;
          D3D12_MESSAGE *msg;

          hr = info_queue->GetMessage (i, nullptr, &msg_len);
          if (FAILED (hr) || msg_len == 0)
            continue;

          msg = (D3D12_MESSAGE *) g_malloc0 (msg_len);
          hr = info_queue->GetMessage (i, msg, &msg_len);
          if (FAILED (hr) || msg_len == 0) {
            g_free (msg);
            continue;
          }

          gst_debug_log (gst_d3d12_sdk_debug, GST_LEVEL_INFO,
              __FILE__, GST_FUNCTION, __LINE__, nullptr,
              "D3D12InfoQueue: %s", msg->pDescription);
          g_free (msg);
        }

        info_queue->ClearStoredMessages ();
      }
    }
  }

  ComPtr<ID3D12Device> device;
  ComPtr<IDXGIAdapter1> adapter;
  ComPtr<IDXGIFactory2> factory;
  std::unordered_map<GstVideoFormat, GstD3D12Format> format_table;
  std::recursive_mutex extern_lock;
  std::mutex lock;

  ComPtr<ID3D12InfoQueue> info_queue;

  ComPtr<IUnknown> d3d11on12;

  GstD3D12CommandQueue *direct_queue = nullptr;
  GstD3D12CommandQueue *copy_queue = nullptr;

  GstD3D12CommandListPool *direct_cl_pool = nullptr;
  GstD3D12CommandAllocatorPool *direct_ca_pool = nullptr;

  GstD3D12CommandListPool *copy_cl_pool = nullptr;
  GstD3D12CommandAllocatorPool *copy_ca_pool = nullptr;

  guint rtv_inc_size;

  guint adapter_index = 0;
  guint device_id = 0;
  guint vendor_id = 0;
  std::string description;
  gint64 adapter_luid = 0;
};

enum GstD3D12DeviceConstructType
{
  GST_D3D12_DEVICE_CONSTRUCT_FOR_INDEX,
  GST_D3D12_DEVICE_CONSTRUCT_FOR_LUID,
};

struct GstD3D12DeviceConstructData
{
  union
  {
    guint index;
    gint64 luid;
  } data;
  GstD3D12DeviceConstructType type;
};

static GstD3D12Device *
gst_d3d12_device_new_internal (const GstD3D12DeviceConstructData * data);

struct DeviceCache
{
  ~DeviceCache()
  {
    if (priv)
      delete priv;
  }

  GstD3D12Device *device = nullptr;
  GstD3D12DevicePrivate *priv = nullptr;
  guint64 token = 0;
};

/* Because ID3D12Device instance is a singleton per adapter,
 * this DeviceCacheManager object will cache GstD3D12Device object and
 * will return the same GstD3D12Device object for create request
 * if instanted object exists already.
 *
 * Another role of this object dtor thread management.
 * GstD3D12CommandQueue object held by GstD3D12Device will run background
 * garbage collection thread and releasing garbage collection data
 * could result in releasing GstD3D12Device object, which can cause self-thread
 * joining. This manager will run one background thread to avoid it
 */
class DeviceCacheManager
{
public:
  DeviceCacheManager (const DeviceCacheManager &) = delete;
  DeviceCacheManager& operator= (const DeviceCacheManager &) = delete;
  static DeviceCacheManager * GetInstance()
  {
    static DeviceCacheManager *inst = nullptr;
    GST_D3D12_CALL_ONCE_BEGIN {
      inst = new DeviceCacheManager ();
    } GST_D3D12_CALL_ONCE_END;

    return inst;
  }

  void InitThread ()
  {
    std::lock_guard <std::mutex> lk (lock_);
    if (!thread_)
      thread_ = new std::thread (&DeviceCacheManager::threadFunc, this);
  }

  void Sync ()
  {
    guint64 to_wait = 0;

    {
      std::lock_guard <std::mutex> lk (lock_);
      if (!thread_)
        return;

      token_++;
      to_wait = token_;

      auto empty_item = std::make_shared <DeviceCache> ();
      empty_item->token = to_wait;

      std::lock_guard <std::mutex> olk (thread_lock_);
      to_remove_.push (std::move (empty_item));
      thread_cond_.notify_one ();
    }

    std::unique_lock <std::mutex> olk (token_lock_);
    while (token_synced_ < to_wait)
      token_cond_.wait (olk);
  }

  GstD3D12Device * Create (const GstD3D12DeviceConstructData * data)
  {
    std::lock_guard <std::mutex> lk (lock_);
    auto it = std::find_if (list_.begin (), list_.end (),
        [&] (const auto & cache) {
          const auto priv = cache->priv;
          if (data->type == GST_D3D12_DEVICE_CONSTRUCT_FOR_INDEX)
            return priv->adapter_index == data->data.index;

          return priv->adapter_luid == data->data.luid;
        });

    if (it != list_.end ())
      return (GstD3D12Device *) gst_object_ref ((*it)->device);

    auto device = gst_d3d12_device_new_internal (data);
    if (!device)
      return nullptr;

    gst_object_ref_sink (device);

    auto item = std::make_shared <DeviceCache> ();
    item->device = device;
    item->priv = device->priv;

    g_object_weak_ref (G_OBJECT (device), DeviceCacheManager::NotifyCb, this);
    list_.push_back (item);

    return device;
  }

  static void NotifyCb (gpointer data, GObject * device)
  {
    auto self = (DeviceCacheManager *) data;
    self->remove ((GstD3D12Device *) device);
  }

private:
  DeviceCacheManager () {}
  ~DeviceCacheManager () {}

  void threadFunc ()
  {
    while (true) {
      std::unique_lock <std::mutex> lk (thread_lock_);
      while (to_remove_.empty ())
        thread_cond_.wait (lk);

      while (!to_remove_.empty ()) {
        guint64 token;

        {
          auto item = to_remove_.front ();
          to_remove_.pop ();
          token = item->token;
        }

        std::lock_guard <std::mutex> olk (token_lock_);
        token_synced_ = token;
        token_cond_.notify_all ();
      }
    }
  }

  void remove (GstD3D12Device * device)
  {
    std::lock_guard <std::mutex> lk (lock_);
    auto it = std::find_if (list_.begin (), list_.end (),
        [&] (const auto & cache) {
          return cache->device == device;
        });

    std::shared_ptr<DeviceCache> cached;
    if (it != list_.end ()) {
      cached = *it;
      list_.erase (it);
    } else {
      GST_WARNING ("Couldn't find device from cache");
    }

    if (cached && thread_) {
      std::lock_guard <std::mutex> tlk (thread_lock_);
      token_++;
      cached->token = token_;
      to_remove_.push (std::move (cached));
      thread_cond_.notify_one ();
    }
  }

private:
  std::mutex lock_;
  std::vector<std::shared_ptr<DeviceCache>> list_;
  std::mutex thread_lock_;
  std::condition_variable thread_cond_;
  std::thread *thread_ = nullptr;
  std::queue<std::shared_ptr<DeviceCache>> to_remove_;
  std::mutex token_lock_;
  std::condition_variable token_cond_;
  guint64 token_ = 0;
  guint64 token_synced_ = 0;
};
/* *INDENT-ON* */

void
gst_d3d12_init_background_thread (void)
{
  auto manager = DeviceCacheManager::GetInstance ();
  manager->InitThread ();
}

void
gst_d3d12_sync_background_thread (void)
{
  auto manager = DeviceCacheManager::GetInstance ();
  manager->Sync ();
}

static gboolean
gst_d3d12_device_enable_debug (void)
{
  static gboolean enabled = FALSE;

  GST_D3D12_CALL_ONCE_BEGIN {
    GST_DEBUG_CATEGORY_INIT (gst_d3d12_sdk_debug,
        "d3d12debuglayer", 0, "d3d12 SDK layer debug");

    /* Enables debug layer only if it's requested, otherwise
     * already configured d3d12 devices (e.g., owned by application)
     * will be invalidated by ID3D12Debug::EnableDebugLayer() */
    if (!g_getenv ("GST_ENABLE_D3D12_DEBUG"))
      return;

    HRESULT hr;
    ComPtr < ID3D12Debug > d3d12_debug;
    hr = D3D12GetDebugInterface (IID_PPV_ARGS (&d3d12_debug));
    if (FAILED (hr))
      return;

    d3d12_debug->EnableDebugLayer ();
    enabled = TRUE;

    GST_INFO ("D3D12 debug layer is enabled");

    ComPtr < ID3D12Debug5 > d3d12_debug5;
    hr = d3d12_debug.As (&d3d12_debug5);
    if (SUCCEEDED (hr))
      d3d12_debug5->SetEnableAutoName (TRUE);

    ComPtr < ID3D12Debug1 > d3d12_debug1;
    hr = d3d12_debug.As (&d3d12_debug1);
    if (FAILED (hr))
      return;

    d3d12_debug1->SetEnableSynchronizedCommandQueueValidation (TRUE);

    GST_INFO ("Enabled synchronized command queue validation");

    if (!g_getenv ("GST_ENABLE_D3D12_DEBUG_GPU_VALIDATION"))
      return;

    d3d12_debug1->SetEnableGPUBasedValidation (TRUE);

    GST_INFO ("Enabled GPU based validation");
  }
  GST_D3D12_CALL_ONCE_END;

  return enabled;
}

#define gst_d3d12_device_parent_class parent_class
G_DEFINE_TYPE (GstD3D12Device, gst_d3d12_device, GST_TYPE_OBJECT);

static void gst_d3d12_device_finalize (GObject * object);
static void gst_d3d12_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_d3d12_device_setup_format_table (GstD3D12Device * self);

static void
gst_d3d12_device_class_init (GstD3D12DeviceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamFlags readable_flags =
      (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  gobject_class->finalize = gst_d3d12_device_finalize;
  gobject_class->get_property = gst_d3d12_device_get_property;

  g_object_class_install_property (gobject_class, PROP_ADAPTER_INDEX,
      g_param_spec_uint ("adapter-index", "Adapter Index",
          "DXGI Adapter index for creating device",
          0, G_MAXUINT32, 0, readable_flags));

  g_object_class_install_property (gobject_class, PROP_ADAPTER_LUID,
      g_param_spec_int64 ("adapter-luid", "Adapter LUID",
          "DXGI Adapter LUID (Locally Unique Identifier) of created device",
          0, G_MAXINT64, 0, readable_flags));

  g_object_class_install_property (gobject_class, PROP_DEVICE_ID,
      g_param_spec_uint ("device-id", "Device Id",
          "DXGI Device ID", 0, G_MAXUINT32, 0, readable_flags));

  g_object_class_install_property (gobject_class, PROP_VENDOR_ID,
      g_param_spec_uint ("vendor-id", "Vendor Id",
          "DXGI Vendor ID", 0, G_MAXUINT32, 0, readable_flags));

  g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
      g_param_spec_string ("description", "Description",
          "Human readable device description", nullptr, readable_flags));
}

static void
gst_d3d12_device_init (GstD3D12Device * self)
{
  self->priv = new GstD3D12DevicePrivate ();
}

static void
gst_d3d12_device_finalize (GObject * object)
{
  auto self = GST_D3D12_DEVICE (object);

  GST_DEBUG_OBJECT (self, "Finalize");

  /* Don't delete private struct. DeviceCacheManager will destroy it */

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d12_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D12Device *self = GST_D3D12_DEVICE (object);
  GstD3D12DevicePrivate *priv = self->priv;

  switch (prop_id) {
    case PROP_ADAPTER_INDEX:
      g_value_set_uint (value, priv->adapter_index);
      break;
    case PROP_ADAPTER_LUID:
      g_value_set_int64 (value, priv->adapter_luid);
      break;
    case PROP_DEVICE_ID:
      g_value_set_uint (value, priv->device_id);
      break;
    case PROP_VENDOR_ID:
      g_value_set_uint (value, priv->vendor_id);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, priv->description.c_str ());
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
check_format_support (GstD3D12Device * self, DXGI_FORMAT format,
    guint flags, D3D12_FEATURE_DATA_FORMAT_SUPPORT * support)
{
  ID3D12Device *device = self->priv->device.Get ();
  HRESULT hr;

  support->Format = format;
  hr = device->CheckFeatureSupport (D3D12_FEATURE_FORMAT_SUPPORT, support,
      sizeof (D3D12_FEATURE_DATA_FORMAT_SUPPORT));
  if (FAILED (hr)) {
    GST_INFO_OBJECT (self,
        "Failed to check feature support for DXGI format %d", format);
    return FALSE;
  }

  if (((guint) support->Support1 & flags) != flags) {
    GST_INFO_OBJECT (self,
        "DXGI format %d supports1 flag 0x%x, required 0x%x", format,
        support->Support1, flags);

    return FALSE;
  }

  return TRUE;
}

static void
gst_d3d12_device_setup_format_table (GstD3D12Device * self)
{
  auto priv = self->priv;

  for (guint i = 0; i < GST_D3D12_N_FORMATS; i++) {
    const auto iter = &g_gst_d3d12_default_format_map[i];
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support[GST_VIDEO_MAX_PLANES];
    gboolean native = true;

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
        if (!check_format_support (self, iter->dxgi_format,
                iter->format_support1[0], &support[0])) {
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
        if (!check_format_support (self, iter->dxgi_format,
                iter->format_support1[0], &support[0])) {
          bool supported = true;
          for (guint j = 0; j < GST_VIDEO_MAX_PLANES; j++) {
            if (iter->resource_format[j] == DXGI_FORMAT_UNKNOWN)
              break;

            if (!check_format_support (self, iter->resource_format[j],
                    iter->format_support1[0], &support[j])) {
              supported = false;
              break;
            }
          }

          if (!supported)
            continue;

          native = false;
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
        bool supported = true;
        native = false;

        for (guint j = 0; j < GST_VIDEO_MAX_PLANES; j++) {
          if (iter->resource_format[j] == DXGI_FORMAT_UNKNOWN)
            break;

          if (!check_format_support (self, iter->resource_format[j],
                  iter->format_support1[0], &support[j])) {
            supported = false;
            break;
          }
        }

        if (!supported)
          continue;
        break;
      }
      default:
        g_assert_not_reached ();
        return;
    }

    auto format = *iter;

    if (!native)
      format.dxgi_format = DXGI_FORMAT_UNKNOWN;

    for (guint j = 0; j < GST_VIDEO_MAX_PLANES; j++) {
      format.format_support1[j] = support[j].Support1;
      format.format_support2[j] = support[j].Support2;
    }

    priv->format_table[format.format] = format;
  }
}

static HRESULT
gst_d3d12_device_find_adapter (const GstD3D12DeviceConstructData * data,
    IDXGIFactory2 * factory, guint * index, IDXGIAdapter1 ** rst)
{
  HRESULT hr;

  switch (data->type) {
    case GST_D3D12_DEVICE_CONSTRUCT_FOR_INDEX:{
      ComPtr < IDXGIAdapter1 > adapter;
      hr = factory->EnumAdapters1 (data->data.index, &adapter);
      if (FAILED (hr))
        return hr;

      *index = data->data.index;
      *rst = adapter.Detach ();
      return S_OK;
    }
    case GST_D3D12_DEVICE_CONSTRUCT_FOR_LUID:
      for (UINT i = 0;; i++) {
        ComPtr < IDXGIAdapter1 > adapter;
        DXGI_ADAPTER_DESC1 desc;

        hr = factory->EnumAdapters1 (i, &adapter);
        if (FAILED (hr))
          return hr;

        hr = adapter->GetDesc1 (&desc);
        if (FAILED (hr))
          return hr;

        if (gst_d3d12_luid_to_int64 (&desc.AdapterLuid) != data->data.luid) {
          continue;
        }

        *index = i;
        *rst = adapter.Detach ();

        return S_OK;
      }
    default:
      g_assert_not_reached ();
      break;
  }

  return E_FAIL;
}

static GstD3D12Device *
gst_d3d12_device_new_internal (const GstD3D12DeviceConstructData * data)
{
  ComPtr < IDXGIFactory2 > factory;
  ComPtr < IDXGIAdapter1 > adapter;
  ComPtr < ID3D12Device > device;
  HRESULT hr;
  UINT factory_flags = 0;
  guint index = 0;

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_device_debug,
      "d3d12device", 0, "d3d12 device object");

  gst_d3d12_device_enable_debug ();

  hr = CreateDXGIFactory2 (factory_flags, IID_PPV_ARGS (&factory));
  if (FAILED (hr)) {
    GST_WARNING ("Could create dxgi factory, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  hr = gst_d3d12_device_find_adapter (data, factory.Get (), &index, &adapter);
  if (FAILED (hr)) {
    GST_WARNING ("Could not find adapter, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  DXGI_ADAPTER_DESC1 desc;
  hr = adapter->GetDesc1 (&desc);
  if (FAILED (hr)) {
    GST_WARNING ("Could not get adapter desc, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  hr = D3D12CreateDevice (adapter.Get (), D3D_FEATURE_LEVEL_11_0,
      IID_PPV_ARGS (&device));
  if (FAILED (hr)) {
    GST_WARNING ("Could not create device, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  GstD3D12Device *self = (GstD3D12Device *)
      g_object_new (GST_TYPE_D3D12_DEVICE, nullptr);
  GstD3D12DevicePrivate *priv = self->priv;

  priv->factory = factory;
  priv->adapter = adapter;
  priv->device = device;
  priv->adapter_luid = gst_d3d12_luid_to_int64 (&desc.AdapterLuid);
  priv->vendor_id = desc.VendorId;
  priv->device_id = desc.DeviceId;
  priv->adapter_index = index;

  std::wstring_convert < std::codecvt_utf8 < wchar_t >, wchar_t >converter;
  priv->description = converter.to_bytes (desc.Description);

  GST_INFO_OBJECT (self,
      "adapter index %d: D3D12 device vendor-id: 0x%04x, device-id: 0x%04x, "
      "Flags: 0x%x, adapter-luid: %" G_GINT64_FORMAT ", %s",
      priv->adapter_index, desc.VendorId, desc.DeviceId, desc.Flags,
      priv->adapter_luid, priv->description.c_str ());

  gst_d3d12_device_setup_format_table (self);

  if (gst_d3d12_device_enable_debug ()) {
    ComPtr < ID3D12InfoQueue > info_queue;
    device.As (&info_queue);
    priv->info_queue = info_queue;
  }


  D3D12_COMMAND_QUEUE_DESC queue_desc = { };
  queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

  priv->direct_queue = gst_d3d12_command_queue_new (self, &queue_desc, 0);
  if (!priv->direct_queue)
    goto error;

  priv->direct_cl_pool = gst_d3d12_command_list_pool_new (self,
      D3D12_COMMAND_LIST_TYPE_DIRECT);
  if (!priv->direct_cl_pool)
    goto error;

  priv->direct_ca_pool = gst_d3d12_command_allocator_pool_new (self,
      D3D12_COMMAND_LIST_TYPE_DIRECT);
  if (!priv->direct_ca_pool)
    goto error;

  queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
  priv->copy_queue = gst_d3d12_command_queue_new (self, &queue_desc, 0);
  if (!priv->copy_queue)
    goto error;

  priv->copy_cl_pool = gst_d3d12_command_list_pool_new (self,
      D3D12_COMMAND_LIST_TYPE_COPY);
  if (!priv->copy_cl_pool)
    goto error;

  priv->copy_ca_pool = gst_d3d12_command_allocator_pool_new (self,
      D3D12_COMMAND_LIST_TYPE_COPY);
  if (!priv->copy_ca_pool)
    goto error;

  priv->rtv_inc_size =
      device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  return self;

error:
  gst_object_unref (self);
  return nullptr;
}

GstD3D12Device *
gst_d3d12_device_new (guint adapter_index)
{
  auto manager = DeviceCacheManager::GetInstance ();
  GstD3D12DeviceConstructData data;
  data.data.index = adapter_index;
  data.type = GST_D3D12_DEVICE_CONSTRUCT_FOR_INDEX;

  return manager->Create (&data);
}

GstD3D12Device *
gst_d3d12_device_new_for_adapter_luid (gint64 adapter_luid)
{
  auto manager = DeviceCacheManager::GetInstance ();
  GstD3D12DeviceConstructData data;
  data.data.luid = adapter_luid;
  data.type = GST_D3D12_DEVICE_CONSTRUCT_FOR_LUID;

  return manager->Create (&data);
}

ID3D12Device *
gst_d3d12_device_get_device_handle (GstD3D12Device * device)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);

  return device->priv->device.Get ();
}

IDXGIAdapter1 *
gst_d3d12_device_get_adapter_handle (GstD3D12Device * device)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);

  return device->priv->adapter.Get ();
}

IDXGIFactory2 *
gst_d3d12_device_get_factory_handle (GstD3D12Device * device)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);

  return device->priv->factory.Get ();
}

gboolean
gst_d3d12_device_get_d3d11on12_device (GstD3D12Device * device,
    IUnknown ** d3d11on12)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), FALSE);
  g_return_val_if_fail (d3d11on12, FALSE);

  auto priv = device->priv;

  std::lock_guard < std::mutex > lk (priv->lock);
  if (!priv->d3d11on12) {
    ComPtr < ID3D12CommandQueue > cq;
    gst_d3d12_command_queue_get_handle (priv->direct_queue, &cq);
    auto hr = GstD3D11On12CreateDevice (priv->device.Get (), cq.Get (),
        &priv->d3d11on12);
    if (!gst_d3d12_result (hr, device)) {
      GST_ERROR_OBJECT (device, "Couldn't create d3d11on12 device");
      return FALSE;
    }
  }

  *d3d11on12 = priv->d3d11on12.Get ();
  (*d3d11on12)->AddRef ();

  return TRUE;
}

void
gst_d3d12_device_lock (GstD3D12Device * device)
{
  g_return_if_fail (GST_IS_D3D12_DEVICE (device));

  auto priv = device->priv;
  priv->extern_lock.lock ();
}

void
gst_d3d12_device_unlock (GstD3D12Device * device)
{
  g_return_if_fail (GST_IS_D3D12_DEVICE (device));

  auto priv = device->priv;
  priv->extern_lock.unlock ();
}

gboolean
gst_d3d12_device_get_format (GstD3D12Device * device,
    GstVideoFormat format, GstD3D12Format * device_format)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), FALSE);
  g_return_val_if_fail (device_format != nullptr, FALSE);

  auto priv = device->priv;
  const auto & target = priv->format_table.find (format);
  if (target == priv->format_table.end ())
    return FALSE;

  if (device_format)
    *device_format = target->second;

  return TRUE;
}

GstD3D12CommandQueue *
gst_d3d12_device_get_command_queue (GstD3D12Device * device,
    D3D12_COMMAND_LIST_TYPE queue_type)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);

  auto priv = device->priv;

  switch (queue_type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
      return priv->direct_queue;
    case D3D12_COMMAND_LIST_TYPE_COPY:
      return priv->copy_queue;
    default:
      break;
  }

  GST_ERROR_OBJECT (device, "Not supported queue type %d", queue_type);

  return nullptr;
}

gboolean
gst_d3d12_device_execute_command_lists (GstD3D12Device * device,
    D3D12_COMMAND_LIST_TYPE queue_type, guint num_command_lists,
    ID3D12CommandList ** command_lists, guint64 * fence_value)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), FALSE);

  auto priv = device->priv;
  GstD3D12CommandQueue *queue;

  switch (queue_type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
      queue = priv->direct_queue;
      break;
    case D3D12_COMMAND_LIST_TYPE_COPY:
      queue = priv->copy_queue;
      break;
    default:
      GST_ERROR_OBJECT (device, "Not supported queue type %d", queue_type);
      return FALSE;
  }

  auto hr = gst_d3d12_command_queue_execute_command_lists (queue,
      num_command_lists, command_lists, fence_value);

  return gst_d3d12_result (hr, device);
}

guint64
gst_d3d12_device_get_completed_value (GstD3D12Device * device,
    D3D12_COMMAND_LIST_TYPE queue_type)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), G_MAXUINT64);

  auto priv = device->priv;
  GstD3D12CommandQueue *queue;

  switch (queue_type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
      queue = priv->direct_queue;
      break;
    case D3D12_COMMAND_LIST_TYPE_COPY:
      queue = priv->copy_queue;
      break;
    default:
      GST_ERROR_OBJECT (device, "Not supported queue type %d", queue_type);
      return G_MAXUINT64;
  }

  return gst_d3d12_command_queue_get_completed_value (queue);
}

gboolean
gst_d3d12_device_set_fence_notify (GstD3D12Device * device,
    D3D12_COMMAND_LIST_TYPE queue_type, guint64 fence_value,
    GstD3D12FenceData * fence_data)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), FALSE);
  g_return_val_if_fail (fence_data, FALSE);

  auto priv = device->priv;
  GstD3D12CommandQueue *queue;

  switch (queue_type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
      queue = priv->direct_queue;
      break;
    case D3D12_COMMAND_LIST_TYPE_COPY:
      queue = priv->copy_queue;
      break;
    default:
      GST_ERROR_OBJECT (device, "Not supported queue type %d", queue_type);
      return FALSE;
  }

  gst_d3d12_command_queue_set_notify (queue, fence_value, fence_data,
      (GDestroyNotify) gst_d3d12_fence_data_unref);

  return TRUE;
}

gboolean
gst_d3d12_device_fence_wait (GstD3D12Device * device,
    D3D12_COMMAND_LIST_TYPE queue_type, guint64 fence_value,
    HANDLE event_handle)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), FALSE);

  auto priv = device->priv;
  GstD3D12CommandQueue *queue;

  switch (queue_type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
      queue = priv->direct_queue;
      break;
    case D3D12_COMMAND_LIST_TYPE_COPY:
      queue = priv->copy_queue;
      break;
    default:
      GST_ERROR_OBJECT (device, "Not supported queue type %d", queue_type);
      return FALSE;
  }

  auto hr = gst_d3d12_command_queue_fence_wait (queue,
      fence_value, event_handle);

  return gst_d3d12_result (hr, device);
}

gboolean
gst_d3d12_device_copy_texture_region (GstD3D12Device * device,
    guint num_args, const GstD3D12CopyTextureRegionArgs * args,
    D3D12_COMMAND_LIST_TYPE command_type, guint64 * fence_value)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), FALSE);
  g_return_val_if_fail (num_args > 0, FALSE);
  g_return_val_if_fail (args, FALSE);

  HRESULT hr;
  auto priv = device->priv;
  GstD3D12CommandAllocatorPool *ca_pool;
  GstD3D12CommandAllocator *gst_ca = nullptr;
  GstD3D12CommandListPool *cl_pool;
  GstD3D12CommandList *gst_cl = nullptr;
  GstD3D12CommandQueue *queue = nullptr;
  guint64 fence_val = 0;

  switch (command_type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
      queue = priv->direct_queue;
      ca_pool = priv->direct_ca_pool;
      cl_pool = priv->direct_cl_pool;
      break;
    case D3D12_COMMAND_LIST_TYPE_COPY:
      queue = priv->copy_queue;
      ca_pool = priv->copy_ca_pool;
      cl_pool = priv->copy_cl_pool;
      break;
    default:
      GST_ERROR_OBJECT (device, "Not supported command list type %d",
          command_type);
      return FALSE;
  }

  gst_d3d12_command_allocator_pool_acquire (ca_pool, &gst_ca);
  if (!gst_ca) {
    GST_ERROR_OBJECT (device, "Couldn't acquire command allocator");
    return FALSE;
  }

  ComPtr < ID3D12CommandAllocator > ca;
  gst_d3d12_command_allocator_get_handle (gst_ca, &ca);

  gst_d3d12_command_list_pool_acquire (cl_pool, ca.Get (), &gst_cl);

  if (!gst_cl) {
    GST_ERROR_OBJECT (device, "Couldn't acquire command list");
    gst_clear_d3d12_command_allocator (&gst_ca);
    return FALSE;
  }

  ComPtr < ID3D12CommandList > cl_base;
  ComPtr < ID3D12GraphicsCommandList > cl;

  gst_d3d12_command_list_get_handle (gst_cl, &cl_base);
  cl_base.As (&cl);

  for (guint i = 0; i < num_args; i++) {
    const auto arg = args[i];
    cl->CopyTextureRegion (&arg.dst, arg.dst_x, arg.dst_y, arg.dst_z,
        &arg.src, arg.src_box);
  }

  hr = cl->Close ();
  if (!gst_d3d12_result (hr, device)) {
    GST_ERROR_OBJECT (device, "Couldn't close command list");
    gst_clear_d3d12_command_list (&gst_cl);
    gst_clear_d3d12_command_allocator (&gst_ca);
    return FALSE;
  }

  ID3D12CommandList *cmd_list[] = { cl.Get () };
  hr = gst_d3d12_command_queue_execute_command_lists (queue,
      1, cmd_list, &fence_val);
  auto ret = gst_d3d12_result (hr, device);

  /* We can release command list since command list pool will hold it */
  gst_d3d12_command_list_unref (gst_cl);

  if (ret) {
    gst_d3d12_command_queue_set_notify (queue, fence_val, gst_ca,
        (GDestroyNotify) gst_d3d12_command_allocator_unref);
  } else {
    gst_d3d12_command_allocator_unref (gst_ca);
  }

  if (fence_value)
    *fence_value = fence_val;

  return ret;
}

static inline GstDebugLevel
d3d12_message_severity_to_gst (D3D12_MESSAGE_SEVERITY level)
{
  switch (level) {
    case D3D12_MESSAGE_SEVERITY_CORRUPTION:
    case D3D12_MESSAGE_SEVERITY_ERROR:
      return GST_LEVEL_ERROR;
    case D3D12_MESSAGE_SEVERITY_WARNING:
      return GST_LEVEL_WARNING;
    case D3D12_MESSAGE_SEVERITY_INFO:
      return GST_LEVEL_INFO;
    case D3D12_MESSAGE_SEVERITY_MESSAGE:
      return GST_LEVEL_DEBUG;
    default:
      break;
  }

  return GST_LEVEL_LOG;
}

void
gst_d3d12_device_d3d12_debug (GstD3D12Device * device, const gchar * file,
    const gchar * function, gint line)
{
  g_return_if_fail (GST_IS_D3D12_DEVICE (device));

  if (!device->priv->info_queue)
    return;

  GstD3D12DevicePrivate *priv = device->priv;

  std::lock_guard < std::recursive_mutex > lk (priv->extern_lock);
  ID3D12InfoQueue *info_queue = priv->info_queue.Get ();
  UINT64 num_msg = info_queue->GetNumStoredMessages ();
  for (guint64 i = 0; i < num_msg; i++) {
    HRESULT hr;
    SIZE_T msg_len;
    D3D12_MESSAGE *msg;
    GstDebugLevel msg_level;
    GstDebugLevel selected_level;

    hr = info_queue->GetMessage (i, nullptr, &msg_len);
    if (FAILED (hr) || msg_len == 0)
      continue;

    msg = (D3D12_MESSAGE *) g_malloc0 (msg_len);
    hr = info_queue->GetMessage (i, msg, &msg_len);
    if (FAILED (hr) || msg_len == 0) {
      g_free (msg);
      continue;
    }

    msg_level = d3d12_message_severity_to_gst (msg->Severity);
    if (msg->Category == D3D12_MESSAGE_CATEGORY_STATE_CREATION &&
        msg_level > GST_LEVEL_ERROR) {
      /* Do not warn for live object, since there would be live object
       * when ReportLiveDeviceObjects was called */
      selected_level = GST_LEVEL_INFO;
    } else {
      selected_level = msg_level;
    }

    gst_debug_log (gst_d3d12_sdk_debug, selected_level, file, function, line,
        G_OBJECT (device), "D3D12InfoQueue: %s", msg->pDescription);
    g_free (msg);
  }

  info_queue->ClearStoredMessages ();
}

void
gst_d3d12_device_clear_yuv_texture (GstD3D12Device * device, GstMemory * mem)
{
  auto priv = device->priv;
  auto dmem = GST_D3D12_MEMORY_CAST (mem);
  ComPtr < ID3D12DescriptorHeap > heap;

  auto resource = gst_d3d12_memory_get_resource_handle (dmem);
  auto desc = resource->GetDesc ();

  if (desc.Format != DXGI_FORMAT_NV12 && desc.Format != DXGI_FORMAT_P010 &&
      desc.Format != DXGI_FORMAT_P016) {
    return;
  }

  gst_d3d12_memory_get_render_target_view_heap (dmem, &heap);
  if (!heap)
    return;

  D3D12_RECT rect = { };
  if (!gst_d3d12_memory_get_plane_rectangle (dmem, 1, &rect))
    return;

  GstD3D12CommandAllocator *gst_ca = nullptr;
  gst_d3d12_command_allocator_pool_acquire (priv->direct_ca_pool, &gst_ca);
  if (!gst_ca)
    return;

  ComPtr < ID3D12CommandAllocator > ca;
  gst_d3d12_command_allocator_get_handle (gst_ca, &ca);

  GstD3D12CommandList *gst_cl = nullptr;
  gst_d3d12_command_list_pool_acquire (priv->direct_cl_pool,
      ca.Get (), &gst_cl);
  if (!gst_cl) {
    gst_d3d12_command_allocator_unref (gst_ca);
    return;
  }

  ComPtr < ID3D12CommandList > cl_base;
  ComPtr < ID3D12GraphicsCommandList > cl;

  gst_d3d12_command_list_get_handle (gst_cl, &cl_base);
  cl_base.As (&cl);

  auto rtv_handle =
      CD3DX12_CPU_DESCRIPTOR_HANDLE (heap->GetCPUDescriptorHandleForHeapStart
      (),
      priv->rtv_inc_size);

  const FLOAT clear_color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
  cl->ClearRenderTargetView (rtv_handle, clear_color, 1, &rect);

  auto hr = cl->Close ();
  if (!gst_d3d12_result (hr, device)) {
    gst_clear_d3d12_command_list (&gst_cl);
    gst_clear_d3d12_command_allocator (&gst_ca);
    return;
  }

  ID3D12CommandList *cmd_list[] = { cl.Get () };
  guint64 fence_val = 0;
  hr = gst_d3d12_command_queue_execute_command_lists (priv->direct_queue,
      1, cmd_list, &fence_val);
  auto ret = gst_d3d12_result (hr, device);
  gst_d3d12_command_list_unref (gst_cl);

  if (ret) {
    gst_d3d12_command_queue_set_notify (priv->direct_queue, fence_val,
        gst_ca, (GDestroyNotify) gst_d3d12_command_allocator_unref);
    dmem->fence_value = fence_val;
  } else {
    gst_d3d12_command_allocator_unref (gst_ca);
  }
}
