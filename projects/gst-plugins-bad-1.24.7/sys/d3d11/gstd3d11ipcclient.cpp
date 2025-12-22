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

#include "gstd3d11ipcclient.h"
#include <gst/d3d11/gstd3d11-private.h>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <memory>
#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (gst_d3d11_ipc_client_debug);
#define GST_CAT_DEFAULT gst_d3d11_ipc_client_debug

static GThreadPool *gc_thread_pool = nullptr;
/* *INDENT-OFF* */
static std::mutex gc_pool_lock;
/* *INDENT-ON* */

void
gst_d3d11_ipc_client_deinit (void)
{
  std::lock_guard < std::mutex > lk (gc_pool_lock);
  if (gc_thread_pool) {
    g_thread_pool_free (gc_thread_pool, FALSE, TRUE);
    gc_thread_pool = nullptr;
  }
}

/**
 * GstD3D11IpcIOMode:
 *
 * Texture import mode
 *
 * Since: 1.24
 */
GType
gst_d3d11_ipc_io_mode_get_type (void)
{
  static GType type = 0;
  static const GEnumValue io_modes[] = {
    /**
     * GstD3D11IpcIOMode::copy:
     *
     * Copy remote texture to newly allocated texture
     *
     * Since: 1.24
     */
    {GST_D3D11_IPC_IO_COPY, "Copy remote texture", "copy"},

    /**
     * GstD3D11IpcIOMode::import:
     *
     * Import remote texture to without any allocation/copy
     *
     * Since: 1.24
     */
    {GST_D3D11_IPC_IO_IMPORT, "Import remote texture", "import"},
    {0, nullptr, nullptr}
  };

  GST_D3D11_CALL_ONCE_BEGIN {
    type = g_enum_register_static ("GstD3D11IpcIOMode", io_modes);
  } GST_D3D11_CALL_ONCE_END;

  return type;
}

/* *INDENT-OFF* */
struct GstD3D11IpcClientConn : public OVERLAPPED
{
  GstD3D11IpcClientConn (GstD3D11IpcClient * client, HANDLE pipe_handle)
      : client (client), pipe (pipe_handle)
  {
    OVERLAPPED *parent = static_cast<OVERLAPPED *> (this);
    parent->Internal = 0;
    parent->InternalHigh = 0;
    parent->Offset = 0;
    parent->OffsetHigh = 0;

    client_msg.resize (GST_D3D11_IPC_PKT_HEADER_SIZE);
    server_msg.resize (GST_D3D11_IPC_PKT_HEADER_SIZE);
  }

  ~GstD3D11IpcClientConn ()
  {
    if (pipe != INVALID_HANDLE_VALUE) {
      CancelIo (pipe);
      CloseHandle (pipe);
    }
  }

  GstD3D11IpcClient *client;

  HANDLE pipe = INVALID_HANDLE_VALUE;

  GstD3D11IpcPktType type;
  std::vector<guint8> client_msg;
  std::vector<guint8> server_msg;
};

struct GstD3D11IpcImportData
{
  ~GstD3D11IpcImportData ()
  {
    GST_LOG_OBJECT (client, "Release handle \"%p\"", server_handle);
    gst_object_unref (client);
  }

  GstD3D11IpcClient *client;
  ComPtr<ID3D11Texture2D> texture;
  ComPtr<IDXGIKeyedMutex> mutex;
  GstD3D11IpcMemLayout layout;
  HANDLE server_handle = nullptr;
};

struct GstD3D11IpcReleaseData
{
  GstD3D11IpcClient *self;
  std::shared_ptr<GstD3D11IpcImportData> imported;
};

struct GstD3D11IpcClientPrivate
{
  GstD3D11IpcClientPrivate ()
  {
    wakeup_event = CreateEvent (nullptr, FALSE, FALSE, nullptr);
    cancellable = CreateEvent (nullptr, TRUE, FALSE, nullptr);

    shutdown = false;
    io_pending = true;
  }

  ~GstD3D11IpcClientPrivate ()
  {
    gst_clear_caps (&caps);
    if (pool) {
      gst_buffer_pool_set_active (pool, FALSE);
      gst_object_unref (pool);
    }

    gst_clear_object (&device);

    CloseHandle (wakeup_event);
    CloseHandle (cancellable);
    if (server_process)
      CloseHandle (server_process);
  }

  std::string address;
  GstD3D11IpcIOMode io_mode = GST_D3D11_IPC_IO_COPY;
  GstClockTime timeout;
  HANDLE wakeup_event;
  HANDLE cancellable;
  HANDLE server_process = nullptr;
  std::mutex lock;
  std::condition_variable cond;
  GstD3D11Device *device = nullptr;
  GstCaps *caps = nullptr;
  GstBufferPool *pool = nullptr;
  GstVideoInfo info;
  bool server_eos = false;
  bool flushing = false;
  bool aborted = false;
  bool sent_fin = false;
  std::atomic<bool> shutdown;
  std::atomic<bool> io_pending;
  GThread *loop_thread = nullptr;
  std::queue <GstSample *> samples;
  std::shared_ptr<GstD3D11IpcClientConn> conn;
  std::queue<HANDLE> unused_data;
  std::vector<std::weak_ptr<GstD3D11IpcImportData>> imported;
};
/* *INDENT-ON* */

struct _GstD3D11IpcClient
{
  GstObject parent;

  GstD3D11IpcClientPrivate *priv;
};

static void gst_d3d11_ipc_client_dispose (GObject * object);
static void gst_d3d11_ipc_client_finalize (GObject * object);
static void gst_d3d11_ipc_client_continue (GstD3D11IpcClient * self);
static void gst_d3d11_ipc_client_send_msg (GstD3D11IpcClient * self);

#define gst_d3d11_ipc_client_parent_class parent_class
G_DEFINE_TYPE (GstD3D11IpcClient, gst_d3d11_ipc_client, GST_TYPE_OBJECT);

static void
gst_d3d11_ipc_client_class_init (GstD3D11IpcClientClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gst_d3d11_ipc_client_dispose;
  object_class->finalize = gst_d3d11_ipc_client_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_d3d11_ipc_client_debug, "d3d11ipcclient",
      0, "d3d11ipcclient");
}

static void
gst_d3d11_ipc_client_init (GstD3D11IpcClient * self)
{
  self->priv = new GstD3D11IpcClientPrivate ();
}

static void
gst_d3d11_ipc_client_dispose (GObject * object)
{
  GstD3D11IpcClient *self = GST_D3D11_IPC_CLIENT (object);
  GstD3D11IpcClientPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "dispose");

  SetEvent (priv->cancellable);

  g_clear_pointer (&priv->loop_thread, g_thread_join);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_d3d11_ipc_client_finalize (GObject * object)
{
  GstD3D11IpcClient *self = GST_D3D11_IPC_CLIENT (object);

  GST_DEBUG_OBJECT (self, "finalize");

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d11_ipc_client_abort (GstD3D11IpcClient * self)
{
  GstD3D11IpcClientPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);
  priv->aborted = true;
  priv->cond.notify_all ();
}

static bool
gst_d3d11_client_update_caps (GstD3D11IpcClient * self, GstCaps * caps)
{
  GstD3D11IpcClientPrivate *priv = self->priv;

  if (!caps)
    return true;

  gst_clear_caps (&priv->caps);
  priv->caps = caps;

  if (priv->pool) {
    gst_buffer_pool_set_active (priv->pool, FALSE);
    gst_clear_object (&priv->pool);
  }

  if (!gst_video_info_from_caps (&priv->info, caps)) {
    GST_ERROR_OBJECT (self, "Invalid caps");
    return false;
  }

  if (priv->io_mode == GST_D3D11_IPC_IO_COPY) {
    GstStructure *config;
    GstD3D11AllocationParams *params;
    guint bind_flags = 0;
    GstD3D11Format device_format;

    if (!gst_d3d11_device_get_format (priv->device,
            GST_VIDEO_INFO_FORMAT (&priv->info), &device_format)) {
      GST_ERROR_OBJECT (self, "Couldn't get device format");
      return false;
    }

    if ((device_format.format_support[0] &
            (guint) D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0) {
      bind_flags |= D3D11_BIND_SHADER_RESOURCE;
    }

    if ((device_format.format_support[0] &
            (guint) D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0) {
      bind_flags |= D3D11_BIND_RENDER_TARGET;
    }

    priv->pool = gst_d3d11_buffer_pool_new (priv->device);
    config = gst_buffer_pool_get_config (priv->pool);

    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

    gst_buffer_pool_config_set_params (config, priv->caps,
        GST_VIDEO_INFO_SIZE (&priv->info), 0, 0);

    params = gst_d3d11_allocation_params_new (priv->device, &priv->info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);

    gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
    gst_d3d11_allocation_params_free (params);

    if (!gst_buffer_pool_set_config (priv->pool, config)) {
      GST_ERROR_OBJECT (self, "Couldn't set pool config");
      gst_clear_object (&priv->pool);
      return false;
    }

    if (!gst_buffer_pool_set_active (priv->pool, TRUE)) {
      GST_ERROR_OBJECT (self, "Couldn't active pool");
      gst_clear_object (&priv->pool);
      return false;
    }
  }

  return true;
}

static bool
gst_d3d11_ipc_client_config_data (GstD3D11IpcClient * self)
{
  GstD3D11IpcClientPrivate *priv = self->priv;
  gint64 prev_luid, luid;
  GstCaps *caps = nullptr;
  auto conn = priv->conn;
  DWORD server_pid;
  std::lock_guard < std::mutex > lk (priv->lock);

  g_object_get (priv->device, "adapter-luid", &prev_luid, nullptr);

  if (!gst_d3d11_ipc_pkt_parse_config (conn->server_msg,
          server_pid, luid, &caps)) {
    GST_ERROR_OBJECT (self, "Couldn't parse CONFIG-DATA");
    return false;
  }

  if (priv->server_process) {
    GST_WARNING_OBJECT (self, "Have server process handle already");
    CloseHandle (priv->server_process);
  }

  priv->server_process = OpenProcess (PROCESS_DUP_HANDLE, FALSE, server_pid);
  if (!priv->server_process) {
    guint last_err = GetLastError ();
    auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
    GST_ERROR_OBJECT (self, "Couldn't open server process, 0x%x (%s)",
        last_err, err.c_str ());
    return false;
  }

  if (prev_luid != luid) {
    GstD3D11Device *device = gst_d3d11_device_new_for_adapter_luid (luid,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT);
    if (!device) {
      GST_ERROR_OBJECT (self, "Couldn't create device");
      return false;
    }

    gst_object_unref (priv->device);
    priv->device = device;
  }

  if (!gst_d3d11_client_update_caps (self, caps))
    return false;

  priv->cond.notify_all ();

  return true;
}

static void
gst_d3d11_ipc_client_release_imported_data (GstD3D11IpcReleaseData * data)
{
  GstD3D11IpcClient *self = data->self;
  GstD3D11IpcClientPrivate *priv = self->priv;
  HANDLE server_handle = data->imported->server_handle;

  GST_LOG_OBJECT (self, "Releasing data \"%p\"", server_handle);

  data->imported = nullptr;

  priv->lock.lock ();
  priv->unused_data.push (server_handle);
  priv->lock.unlock ();

  SetEvent (priv->wakeup_event);

  gst_object_unref (data->self);

  delete data;
}

static bool
gst_d3d11_ipc_client_have_data (GstD3D11IpcClient * self)
{
  GstD3D11IpcClientPrivate *priv = self->priv;
  GstBuffer *buffer;
  GstMemory *mem;
  GstSample *sample;
  GstClockTime pts;
  GstCaps *caps = nullptr;
  GstD3D11IpcMemLayout layout;
  std::shared_ptr < GstD3D11IpcImportData > import_data;
  std::unique_lock < std::mutex > lk (priv->lock);
  HANDLE server_handle = nullptr;
  HANDLE client_handle = nullptr;
  auto conn = priv->conn;
  ComPtr < ID3D11Texture2D > texture;
  HRESULT hr;

  if (!gst_d3d11_ipc_pkt_parse_have_data (conn->server_msg,
          pts, layout, server_handle, &caps)) {
    GST_ERROR_OBJECT (self, "Couldn't parse HAVE-DATA packet");
    return false;
  }

  if (!gst_d3d11_client_update_caps (self, caps))
    return false;

  if (!DuplicateHandle (priv->server_process, server_handle,
          GetCurrentProcess (), &client_handle, 0, FALSE,
          DUPLICATE_SAME_ACCESS)) {
    guint last_err = GetLastError ();
    auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
    GST_ERROR_OBJECT (self, "Couldn't duplicate handle, 0x%x (%s)",
        last_err, err.c_str ());
    return false;
  }

  GST_LOG_OBJECT (self, "Importing server handle %p", server_handle);

  ID3D11Device *device = gst_d3d11_device_get_device_handle (priv->device);
  ComPtr < ID3D11Device1 > device1;
  ComPtr < IDXGIKeyedMutex > mutex;

  hr = device->QueryInterface (IID_PPV_ARGS (&device1));
  if (!gst_d3d11_result (hr, priv->device)) {
    GST_ERROR_OBJECT (self, "ID3D11Device1 interface is not available");
    return false;
  }

  hr = device1->OpenSharedResource1 (client_handle, IID_PPV_ARGS (&texture));
  CloseHandle (client_handle);

  if (!gst_d3d11_result (hr, priv->device)) {
    GST_ERROR_OBJECT (self, "Couldn't open resource");
    return false;
  }

  hr = texture->QueryInterface (IID_PPV_ARGS (&mutex));
  if (!gst_d3d11_result (hr, priv->device)) {
    GST_ERROR_OBJECT (self, "couldn't get keyed mutex interface");
    return false;
  }

  import_data = std::make_shared < GstD3D11IpcImportData > ();
  import_data->client = (GstD3D11IpcClient *) gst_object_ref (self);
  import_data->texture = texture;
  import_data->mutex = mutex;
  import_data->layout = layout;
  import_data->server_handle = server_handle;

  if (priv->io_mode == GST_D3D11_IPC_IO_COPY) {
    ID3D11DeviceContext *context =
        gst_d3d11_device_get_device_context_handle (priv->device);
    ID3D11Texture2D *dst_texture;
    D3D11_BOX src_box = { 0, };
    D3D11_TEXTURE2D_DESC dst_desc, src_desc;
    GstMapInfo info;

    hr = import_data->mutex->AcquireSync (0, INFINITE);
    if (hr != S_OK) {
      GST_ERROR_OBJECT (self, "Couldn't acquire sync");
      return false;
    }

    gst_buffer_pool_acquire_buffer (priv->pool, &buffer, nullptr);
    mem = gst_buffer_peek_memory (buffer, 0);

    gst_memory_map (mem, &info, (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11));

    dst_texture = (ID3D11Texture2D *) info.data;
    dst_texture->GetDesc (&dst_desc);

    texture->GetDesc (&src_desc);

    src_box.left = 0;
    src_box.top = 0;
    src_box.front = 0;
    src_box.back = 1;
    src_box.right = MIN (src_desc.Width, dst_desc.Width);
    src_box.bottom = MIN (src_desc.Height, dst_desc.Height);

    context->CopySubresourceRegion (dst_texture, 0, 0, 0, 0,
        texture.Get (), 0, &src_box);

    import_data->mutex->ReleaseSync (0);

    gst_memory_unmap (mem, &info);

    priv->unused_data.push (server_handle);
  } else {
    GstMemory *mem;
    gint stride[GST_VIDEO_MAX_PLANES];
    gsize offset[GST_VIDEO_MAX_PLANES];

    for (guint i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
      stride[i] = import_data->layout.pitch;
      offset[i] = import_data->layout.offset[i];
    }

    auto data = new GstD3D11IpcReleaseData ();
    data->self = (GstD3D11IpcClient *) gst_object_ref (self);
    data->imported = import_data;

    mem = gst_d3d11_allocator_alloc_wrapped (nullptr, priv->device,
        texture.Get (), import_data->layout.size, data,
        (GDestroyNotify) gst_d3d11_ipc_client_release_imported_data);
    GST_MINI_OBJECT_FLAG_SET (mem, GST_MEMORY_FLAG_READONLY);

    buffer = gst_buffer_new ();
    gst_buffer_append_memory (buffer, mem);

    gst_buffer_add_video_meta_full (buffer, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT (&priv->info), GST_VIDEO_INFO_WIDTH (&priv->info),
        GST_VIDEO_INFO_HEIGHT (&priv->info),
        GST_VIDEO_INFO_N_PLANES (&priv->info), offset, stride);

    priv->imported.push_back (import_data);
  }

  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (buffer) = GST_CLOCK_TIME_NONE;

  sample = gst_sample_new (buffer, priv->caps, nullptr, nullptr);
  gst_buffer_unref (buffer);

  /* Drops too old samples */
  std::queue < GstSample * >drop_queue;
  while (priv->samples.size () > 2) {
    drop_queue.push (priv->samples.front ());
    priv->samples.pop ();
  }

  priv->samples.push (sample);
  priv->cond.notify_all ();
  lk.unlock ();

  import_data = nullptr;
  while (!drop_queue.empty ()) {
    auto old = drop_queue.front ();
    gst_sample_unref (old);
    drop_queue.pop ();
  }

  return true;
}

static void
gst_d3d11_ipc_client_wait_msg_finish (GstD3D11IpcClient * client)
{
  GstD3D11IpcClientPrivate *priv = client->priv;
  GstD3D11IpcPacketHeader header;
  auto conn = priv->conn;

  if (!gst_d3d11_ipc_pkt_identify (conn->server_msg, header)) {
    GST_ERROR_OBJECT (client, "Broken header");
    gst_d3d11_ipc_client_abort (client);
    return;
  }

  switch (header.type) {
    case GstD3D11IpcPktType::CONFIG:
      GST_LOG_OBJECT (client, "Got CONFIG");
      if (!gst_d3d11_ipc_client_config_data (client)) {
        gst_d3d11_ipc_client_abort (client);
        return;
      }

      gst_d3d11_ipc_client_continue (client);
      break;
    case GstD3D11IpcPktType::HAVE_DATA:
      GST_LOG_OBJECT (client, "Got HAVE-DATA");
      gst_d3d11_device_lock (priv->device);
      if (!gst_d3d11_ipc_client_have_data (client)) {
        gst_d3d11_device_unlock (priv->device);
        gst_d3d11_ipc_client_abort (client);
        return;
      }

      gst_d3d11_device_unlock (priv->device);

      GST_LOG_OBJECT (client, "Sending READ-DONE");
      gst_d3d11_ipc_pkt_build_read_done (conn->client_msg);
      conn->type = GstD3D11IpcPktType::READ_DONE;
      gst_d3d11_ipc_client_send_msg (client);
      break;
    case GstD3D11IpcPktType::EOS:
      GST_DEBUG_OBJECT (client, "Got EOS");
      priv->server_eos = true;
      priv->lock.lock ();
      priv->cond.notify_all ();
      priv->lock.unlock ();
      gst_d3d11_ipc_client_continue (client);
      break;
    default:
      GST_WARNING_OBJECT (client, "Unexpected packet type");
      gst_d3d11_ipc_client_abort (client);
      break;
  }
}

static void WINAPI
gst_d3d11_ipc_client_payload_finish (DWORD error_code, DWORD size,
    OVERLAPPED * overlap)
{
  GstD3D11IpcClientConn *conn =
      static_cast < GstD3D11IpcClientConn * >(overlap);
  GstD3D11IpcClient *self = conn->client;

  if (error_code != ERROR_SUCCESS) {
    auto err = gst_d3d11_ipc_win32_error_to_string (error_code);
    GST_WARNING_OBJECT (self, "ReadFileEx callback failed with 0x%x (%s)",
        (guint) error_code, err.c_str ());
    gst_d3d11_ipc_client_abort (self);
  }

  gst_d3d11_ipc_client_wait_msg_finish (self);
}

static void WINAPI
gst_d3d11_ipc_client_win32_wait_header_finish (DWORD error_code, DWORD size,
    OVERLAPPED * overlap)
{
  GstD3D11IpcClientConn *conn =
      static_cast < GstD3D11IpcClientConn * >(overlap);
  GstD3D11IpcClient *self = conn->client;
  GstD3D11IpcPacketHeader header;

  if (error_code != ERROR_SUCCESS) {
    auto err = gst_d3d11_ipc_win32_error_to_string (error_code);
    GST_WARNING_OBJECT (self, "ReadFileEx callback failed with 0x%x (%s)",
        (guint) error_code, err.c_str ());
    gst_d3d11_ipc_client_abort (self);
    return;
  }

  if (!gst_d3d11_ipc_pkt_identify (conn->server_msg, header)) {
    GST_ERROR_OBJECT (self, "Broken header");
    gst_d3d11_ipc_client_abort (self);
    return;
  }

  if (header.payload_size == 0) {
    gst_d3d11_ipc_client_wait_msg_finish (self);
    return;
  }

  GST_LOG_OBJECT (self, "Reading payload");

  if (!ReadFileEx (conn->pipe, &conn->server_msg[0] +
          GST_D3D11_IPC_PKT_HEADER_SIZE, header.payload_size, conn,
          gst_d3d11_ipc_client_payload_finish)) {
    guint last_err = GetLastError ();
    auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (self, "ReadFileEx failed with 0x%x (%s)",
        last_err, err.c_str ());
    gst_d3d11_ipc_client_abort (self);
  }
}

static void
gst_d3d11_ipc_client_wait_msg (GstD3D11IpcClient * self)
{
  GstD3D11IpcClientPrivate *priv = self->priv;
  auto conn = priv->conn;
  priv->io_pending = true;

  if (!ReadFileEx (conn->pipe, &conn->server_msg[0],
          GST_D3D11_IPC_PKT_HEADER_SIZE, conn.get (),
          gst_d3d11_ipc_client_win32_wait_header_finish)) {
    guint last_err = GetLastError ();
    auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (self, "ReadFileEx failed with 0x%x (%s)",
        last_err, err.c_str ());
    gst_d3d11_ipc_client_abort (self);
  }
}

static void WINAPI
gst_cuda_ipc_client_win32_send_msg_finish (DWORD error_code, DWORD size,
    OVERLAPPED * overlap)
{
  GstD3D11IpcClientConn *conn =
      static_cast < GstD3D11IpcClientConn * >(overlap);
  GstD3D11IpcClient *self = conn->client;

  if (error_code != ERROR_SUCCESS) {
    auto err = gst_d3d11_ipc_win32_error_to_string (error_code);
    GST_WARNING_OBJECT (self, "WriteFileEx callback failed with 0x%x (%s)",
        (guint) error_code, err.c_str ());
    gst_d3d11_ipc_client_abort (self);
    return;
  }

  switch (conn->type) {
    case GstD3D11IpcPktType::NEED_DATA:
      GST_LOG_OBJECT (self, "Sent NEED-DATA");
      gst_d3d11_ipc_client_wait_msg (self);
      break;
    case GstD3D11IpcPktType::READ_DONE:
      GST_LOG_OBJECT (self, "Sent READ-DONE");
      gst_d3d11_ipc_client_continue (self);
      break;
    case GstD3D11IpcPktType::RELEASE_DATA:
      GST_LOG_OBJECT (self, "Sent RELEASE-DATA");
      gst_d3d11_ipc_client_continue (self);
      break;
    case GstD3D11IpcPktType::FIN:
      GST_DEBUG_OBJECT (self, "Sent FIN");
      gst_d3d11_ipc_client_abort (self);
      break;
    default:
      GST_ERROR_OBJECT (self, "Unexpected msg type");
      gst_d3d11_ipc_client_abort (self);
      break;
  }
}

static void
gst_d3d11_ipc_client_send_msg (GstD3D11IpcClient * self)
{
  GstD3D11IpcClientPrivate *priv = self->priv;

  auto conn = priv->conn;
  priv->io_pending = true;

  if (!WriteFileEx (conn->pipe, &conn->client_msg[0],
          conn->client_msg.size (), conn.get (),
          gst_cuda_ipc_client_win32_send_msg_finish)) {
    guint last_err = GetLastError ();
    auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (self, "WriteFileEx failed with 0x%x (%s)",
        last_err, err.c_str ());
    gst_d3d11_ipc_client_abort (self);
  }
}

static void
gst_d3d11_ipc_client_run_gc (GstD3D11IpcClient * self)
{
  GstD3D11IpcClientPrivate *priv = self->priv;

  for (auto it = priv->imported.begin (); it != priv->imported.end ();) {
    auto data = it->lock ();
    if (!data) {
      it = priv->imported.erase (it);
    } else {
      it++;
    }
  }
}

static void
gst_d3d11_ipc_client_continue (GstD3D11IpcClient * self)
{
  GstD3D11IpcClientPrivate *priv = self->priv;
  std::unique_lock < std::mutex > lk (priv->lock);
  auto conn = priv->conn;

  if (!conn) {
    GST_WARNING_OBJECT (self, "No connection was made");
    priv->aborted = true;
    priv->cond.notify_all ();
    return;
  }

  if (priv->aborted) {
    priv->cond.notify_all ();
    GST_DEBUG_OBJECT (self, "Operation was aborted");
    return;
  }

  if (!priv->unused_data.empty ()) {
    HANDLE server_handle = priv->unused_data.front ();
    priv->unused_data.pop ();

    GST_LOG_OBJECT (self, "Sending RELEASE-DATA %p", server_handle);

    gst_d3d11_ipc_pkt_build_release_data (conn->client_msg, server_handle);
    conn->type = GstD3D11IpcPktType::RELEASE_DATA;
    lk.unlock ();

    gst_d3d11_ipc_client_send_msg (self);
    return;
  }

  if (priv->shutdown) {
    auto drop_queue = priv->samples;
    while (!priv->samples.empty ())
      priv->samples.pop ();
    lk.unlock ();

    while (!drop_queue.empty ()) {
      auto sample = drop_queue.front ();
      gst_sample_unref (sample);
      drop_queue.pop ();
    }
    lk.lock ();
  }

  if (priv->server_eos || priv->shutdown) {
    gst_d3d11_ipc_client_run_gc (self);

    GST_DEBUG_OBJECT (self, "Remaining imported memory %" G_GSIZE_FORMAT,
        priv->imported.size ());

    if (priv->imported.empty ()) {
      GST_DEBUG_OBJECT (self, "Drained");
      if (priv->sent_fin) {
        priv->aborted = true;
        priv->cond.notify_all ();
      } else {
        lk.unlock ();

        priv->sent_fin = true;
        gst_d3d11_ipc_pkt_build_fin (conn->client_msg);
        conn->type = GstD3D11IpcPktType::FIN;

        GST_DEBUG_OBJECT (self, "Sending FIN");
        gst_d3d11_ipc_client_send_msg (self);
        return;
      }
    } else {
      priv->io_pending = false;
    }
    return;
  }

  lk.unlock ();

  gst_d3d11_ipc_pkt_build_need_data (conn->client_msg);
  conn->type = GstD3D11IpcPktType::NEED_DATA;

  GST_LOG_OBJECT (self, "Sending NEED-DATA");
  gst_d3d11_ipc_client_send_msg (self);
}

static gpointer
gst_d3d11_ipc_client_loop_thread_func (GstD3D11IpcClient * self)
{
  GstD3D11IpcClientPrivate *priv = self->priv;
  DWORD mode = PIPE_READMODE_MESSAGE;
  guint wait_ret;
  HANDLE pipe = INVALID_HANDLE_VALUE;
  GstClockTime start_time = gst_util_get_timestamp ();
  HANDLE waitables[] = { priv->cancellable, priv->wakeup_event };
  std::wstring address = gst_d3d11_ipc_string_to_wstring (priv->address);

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
  CREATEFILE2_EXTENDED_PARAMETERS params;
  memset (&params, 0, sizeof (CREATEFILE2_EXTENDED_PARAMETERS));
  params.dwSize = sizeof (CREATEFILE2_EXTENDED_PARAMETERS);
  params.dwFileAttributes = 0;
  params.dwFileFlags = FILE_FLAG_OVERLAPPED;
  params.dwSecurityQosFlags = SECURITY_IMPERSONATION;
#endif

  GST_DEBUG_OBJECT (self, "Starting loop thread");

  std::unique_lock < std::mutex > lk (priv->lock);
  do {
    GstClockTime diff;

    if (priv->flushing) {
      GST_DEBUG_OBJECT (self, "We are flushing");
      priv->aborted = true;
      priv->cond.notify_all ();
      goto out;
    }
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    pipe = CreateFile2 (address.c_str (), GENERIC_READ | GENERIC_WRITE, 0,
        OPEN_EXISTING, &params);
#else
    pipe = CreateFileW (address.c_str (),
        GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED, nullptr);
#endif

    if (pipe != INVALID_HANDLE_VALUE)
      break;

    if (priv->timeout > 0) {
      diff = gst_util_get_timestamp () - start_time;
      if (diff > priv->timeout) {
        GST_WARNING_OBJECT (self, "Timeout");
        priv->aborted = true;
        priv->cond.notify_all ();
        goto out;
      }
    }

    /* Retry per 100ms */
    GST_DEBUG_OBJECT (self, "Sleep for next retry");
    priv->cond.wait_for (lk, std::chrono::milliseconds (100));
  } while (true);

  if (!SetNamedPipeHandleState (pipe, &mode, nullptr, nullptr)) {
    guint last_err = GetLastError ();
    auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (self, "SetNamedPipeHandleState failed with 0x%x (%s)",
        last_err, err.c_str ());

    CloseHandle (pipe);
    priv->aborted = true;
    priv->cond.notify_all ();
    goto out;
  }

  priv->conn = std::make_shared < GstD3D11IpcClientConn > (self, pipe);
  priv->cond.notify_all ();
  lk.unlock ();

  gst_d3d11_ipc_client_wait_msg (self);

  do {
    /* Enters alertable thread state and wait for I/O completion event
     * or cancellable event */
    wait_ret = WaitForMultipleObjectsEx (G_N_ELEMENTS (waitables), waitables,
        FALSE, INFINITE, TRUE);
    if (wait_ret == WAIT_OBJECT_0) {
      GST_DEBUG ("Operation cancelled");
      goto out;
    }

    switch (wait_ret) {
      case WAIT_IO_COMPLETION:
        break;
      case WAIT_OBJECT_0 + 1:
        if (!priv->io_pending)
          gst_d3d11_ipc_client_continue (self);
        break;
      default:
        GST_WARNING ("Unexpected wait return 0x%x", wait_ret);
        gst_d3d11_ipc_client_abort (self);
        goto out;
    }
  } while (true);

out:
  while (!priv->samples.empty ()) {
    auto sample = priv->samples.front ();
    gst_sample_unref (sample);
    priv->samples.pop ();
  }

  priv->conn = nullptr;

  GST_DEBUG_OBJECT (self, "Exit loop thread");

  return nullptr;
}

GstFlowReturn
gst_d3d11_ipc_client_run (GstD3D11IpcClient * client)
{
  GstD3D11IpcClientPrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_IPC_CLIENT (client), GST_FLOW_ERROR);

  priv = client->priv;
  std::unique_lock < std::mutex > lk (priv->lock);
  if (!priv->loop_thread) {
    priv->loop_thread = g_thread_new ("d3d11-ipc-client",
        (GThreadFunc) gst_d3d11_ipc_client_loop_thread_func, client);

    while (!priv->caps && !priv->aborted && !priv->flushing)
      priv->cond.wait (lk);
  }

  if (priv->flushing) {
    GST_DEBUG_OBJECT (client, "We are flushing");
    return GST_FLOW_FLUSHING;
  } else if (priv->aborted || !priv->caps) {
    GST_DEBUG_OBJECT (client, "Aborted");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

GstCaps *
gst_d3d11_ipc_client_get_caps (GstD3D11IpcClient * client)
{
  GstCaps *caps = nullptr;
  GstD3D11IpcClientPrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_IPC_CLIENT (client), nullptr);

  priv = client->priv;

  if (gst_d3d11_ipc_client_run (client) != GST_FLOW_OK)
    return nullptr;

  std::lock_guard < std::mutex > lk (priv->lock);
  if (priv->caps)
    caps = gst_caps_ref (priv->caps);

  return caps;
}

static void
gst_d3d11_ipc_client_stop_async (GstD3D11IpcClient * client, gpointer user_data)
{
  GstD3D11IpcClientPrivate *priv = client->priv;

  GST_DEBUG_OBJECT (client, "Stopping");
  std::unique_lock < std::mutex > lk (priv->lock);
  while (!priv->aborted)
    priv->cond.wait (lk);
  lk.unlock ();

  SetEvent (priv->cancellable);
  g_clear_pointer (&priv->loop_thread, g_thread_join);

  GST_DEBUG_OBJECT (client, "Stopped");

  gst_object_unref (client);
}

static void
gst_d3d11_ipc_client_push_stop_async (GstD3D11IpcClient * client)
{
  std::lock_guard < std::mutex > lk (gc_pool_lock);
  if (!gc_thread_pool) {
    gc_thread_pool = g_thread_pool_new ((GFunc) gst_d3d11_ipc_client_stop_async,
        nullptr, -1, FALSE, nullptr);
  }

  g_thread_pool_push (gc_thread_pool, gst_object_ref (client), nullptr);
}

void
gst_d3d11_ipc_client_stop (GstD3D11IpcClient * client)
{
  GstD3D11IpcClientPrivate *priv;

  g_return_if_fail (GST_IS_D3D11_IPC_CLIENT (client));

  priv = client->priv;

  GST_DEBUG_OBJECT (client, "Stopping");
  priv->shutdown = true;
  SetEvent (priv->wakeup_event);

  if (priv->io_mode == GST_D3D11_IPC_IO_COPY) {
    std::unique_lock < std::mutex > lk (priv->lock);
    while (!priv->aborted)
      priv->cond.wait (lk);
    lk.unlock ();

    GST_DEBUG_OBJECT (client, "Terminating");

    SetEvent (priv->cancellable);

    g_clear_pointer (&priv->loop_thread, g_thread_join);

    GST_DEBUG_OBJECT (client, "Stopped");
  } else {
    /* We don't know when imported memory gets released */
    gst_d3d11_ipc_client_push_stop_async (client);
  }
}

void
gst_d3d11_ipc_client_set_flushing (GstD3D11IpcClient * client, bool flushing)
{
  GstD3D11IpcClientPrivate *priv;

  g_return_if_fail (GST_IS_D3D11_IPC_CLIENT (client));

  priv = client->priv;

  std::lock_guard < std::mutex > lk (priv->lock);
  priv->flushing = flushing;
  priv->cond.notify_all ();
}

GstFlowReturn
gst_d3d11_ipc_client_get_sample (GstD3D11IpcClient * client,
    GstSample ** sample)
{
  GstD3D11IpcClientPrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_IPC_CLIENT (client), GST_FLOW_ERROR);
  g_return_val_if_fail (sample, GST_FLOW_ERROR);

  priv = client->priv;

  GST_LOG_OBJECT (client, "Waiting for sample");
  std::unique_lock < std::mutex > lk (priv->lock);
  while (!priv->flushing && !priv->aborted && !priv->server_eos &&
      priv->samples.empty ()) {
    priv->cond.wait (lk);
  }

  if (!priv->samples.empty ()) {
    *sample = priv->samples.front ();
    priv->samples.pop ();

    GST_LOG_OBJECT (client, "Have sample");
    return GST_FLOW_OK;
  }

  if (priv->flushing) {
    GST_DEBUG_OBJECT (client, "Flushing");
    return GST_FLOW_FLUSHING;
  }

  GST_DEBUG_OBJECT (client, "EOS");

  return GST_FLOW_EOS;
}

GstD3D11IpcClient *
gst_d3d11_ipc_client_new (const std::string & address, GstD3D11Device * device,
    GstD3D11IpcIOMode io_mode, guint timeout)
{
  GstD3D11IpcClient *self;
  GstD3D11IpcClientPrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), nullptr);

  self = (GstD3D11IpcClient *)
      g_object_new (GST_TYPE_D3D11_IPC_CLIENT, nullptr);
  gst_object_ref_sink (self);

  priv = self->priv;
  priv->address = address;
  priv->timeout = timeout * GST_SECOND;
  priv->io_mode = io_mode;
  priv->device = (GstD3D11Device *) gst_object_ref (device);

  return self;
}
