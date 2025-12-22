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

#include "gstcudaipcclient.h"
#include "gstnvcodecutils.h"
#include <gst/cuda/gstcuda-private.h>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <string>

GST_DEBUG_CATEGORY (cuda_ipc_client_debug);
#define GST_CAT_DEFAULT cuda_ipc_client_debug

static GThreadPool *gc_thread_pool = nullptr;
/* *INDENT-OFF* */
static std::mutex gc_pool_lock;
static std::recursive_mutex import_lock_;
/* *INDENT-ON* */

void
gst_cuda_ipc_client_deinit (void)
{
  std::lock_guard < std::mutex > lk (gc_pool_lock);
  if (gc_thread_pool) {
    g_thread_pool_free (gc_thread_pool, FALSE, TRUE);
    gc_thread_pool = nullptr;
  }
}

/**
 * GstCudaIpcIOMode:
 *
 * Since: 1.24
 */
GType
gst_cuda_ipc_io_mode_get_type (void)
{
  static GType type = 0;
  static const GEnumValue io_modes[] = {
    {GST_CUDA_IPC_IO_COPY, "Copy remote memory", "copy"},
    {GST_CUDA_IPC_IO_IMPORT, "Import remote memory", "import"},
    {0, nullptr, nullptr}
  };

  GST_CUDA_CALL_ONCE_BEGIN {
    type = g_enum_register_static ("GstCudaIpcIOMode", io_modes);
  } GST_CUDA_CALL_ONCE_END;

  return type;
}

static void
gst_cuda_ipc_client_close_handle (GstCudaSharableHandle handle)
{
#ifdef G_OS_WIN32
  CloseHandle (handle);
#else
  close (handle);
#endif
}

/* *INDENT-OFF* */
struct GstCudaIpcHandle
{
  GstCudaIpcHandle (CUipcMemHandle mem_handle,
      CUdeviceptr device_ptr, GstCudaContext * context)
  {
    handle = mem_handle;
    dptr = device_ptr;
    ctx = (GstCudaContext *) gst_object_ref (context);
  }

  ~GstCudaIpcHandle ()
  {
    std::lock_guard <std::recursive_mutex> lk (import_lock_);
    auto handle_dump = gst_cuda_ipc_mem_handle_to_string (handle);
    GST_LOG ("Closing handle %s", handle_dump.c_str ());
    gst_cuda_context_push (ctx);
    CuIpcCloseMemHandle (dptr);
    gst_cuda_context_pop (nullptr);
    gst_object_unref (ctx);
    GST_LOG ("Closed handle %s", handle_dump.c_str ());
  }

  CUipcMemHandle handle;
  CUdeviceptr dptr;
  GstCudaContext *ctx;
};

struct GstCudaIpcImportData
{
  std::shared_ptr<GstCudaIpcHandle> handle;
};

struct GstCudaIpcReleaseData
{
  GstCudaIpcClient *self;
  std::shared_ptr<GstCudaIpcImportData> imported;
};

class GstCudaIpcImporter
{
public:
  std::shared_ptr<GstCudaIpcHandle>
  ImportHandle (CUipcMemHandle mem_handle, GstCudaContext * ctx)
  {
    std::lock_guard <std::recursive_mutex> lk (import_lock_);
    CUresult ret;
    CUdeviceptr dptr = 0;
    auto handle_dump = gst_cuda_ipc_mem_handle_to_string (mem_handle);
    GST_LOG ("Trying to import handle %s", handle_dump.c_str ());
    auto it = import_table_.begin ();
    while (it != import_table_.end ()) {
      auto data = it->lock ();
      if (!data) {
        it = import_table_.erase (it);
      } else if (gst_cuda_ipc_handle_is_equal (data->handle, mem_handle)) {
        GST_LOG ("Returning already imported data %s", handle_dump.c_str ());
        return data;
      } else {
        it++;
      }
    };

    if (!gst_cuda_context_push (ctx))
      return nullptr;

    ret = CuIpcOpenMemHandle (&dptr,
        mem_handle, CU_IPC_MEM_LAZY_ENABLE_PEER_ACCESS);
    gst_cuda_context_pop (nullptr);
    if ((ret != CUDA_ERROR_ALREADY_MAPPED && !gst_cuda_result (ret)) || !dptr) {
      GST_ERROR ("Couldn't open mem handle");
      return nullptr;
    }

    GST_LOG ("Imported handle %s", handle_dump.c_str ());

    auto rst = std::make_shared<GstCudaIpcHandle> (mem_handle, dptr, ctx);
    import_table_.push_back (rst);

    return rst;
  }

private:
  std::vector<std::weak_ptr<GstCudaIpcHandle>> import_table_;
};

/* Global IPC handle table for legacy mode, since multiple CuIpcOpenMemHandle()
 * call for the same IPC handle will return error */
static GstCudaIpcImporter _ipc_importer;

struct GstCudaIpcClientPrivate
{
  GstCudaIpcClientPrivate ()
  {
    shutdown = false;
    io_pending = true;
  }

  ~GstCudaIpcClientPrivate ()
  {
    gst_clear_caps (&caps);
    if (pool) {
      gst_buffer_pool_set_active (pool, FALSE);
      gst_object_unref (pool);
    }
  }

  std::mutex lock;
  std::condition_variable cond;
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
  std::shared_ptr<GstCudaIpcClientConn> conn;
  std::queue<CUipcMemHandle> unused_data;
  std::queue<GstCudaSharableHandle> unused_os_handle;
  std::vector<std::weak_ptr<GstCudaIpcImportData>> imported;
  int device_id = 0;
};
/* *INDENT-ON* */

static void gst_cuda_ipc_client_dispose (GObject * object);
static void gst_cuda_ipc_client_finalize (GObject * object);
static void gst_cuda_ipc_client_continue (GstCudaIpcClient * self);

#define gst_cuda_ipc_client_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstCudaIpcClient, gst_cuda_ipc_client,
    GST_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (cuda_ipc_client_debug, "cudaipcclient",
        0, "cudaipcclient"));

static void
gst_cuda_ipc_client_class_init (GstCudaIpcClientClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gst_cuda_ipc_client_dispose;
  object_class->finalize = gst_cuda_ipc_client_finalize;
}

static void
gst_cuda_ipc_client_init (GstCudaIpcClient * self)
{
  self->priv = new GstCudaIpcClientPrivate ();
}

static void
gst_cuda_ipc_client_dispose (GObject * object)
{
  GstCudaIpcClient *self = GST_CUDA_IPC_CLIENT (object);
  GstCudaIpcClientPrivate *priv = self->priv;
  GstCudaIpcClientClass *klass = GST_CUDA_IPC_CLIENT_GET_CLASS (self);

  GST_DEBUG_OBJECT (self, "dispose");

  g_assert (klass->terminate);
  klass->terminate (self);

  g_clear_pointer (&priv->loop_thread, g_thread_join);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_cuda_ipc_client_finalize (GObject * object)
{
  GstCudaIpcClient *self = GST_CUDA_IPC_CLIENT (object);

  GST_DEBUG_OBJECT (self, "finalize");

  delete self->priv;

  gst_clear_cuda_stream (&self->stream);
  gst_clear_object (&self->context);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GstFlowReturn
gst_cuda_ipc_client_get_sample (GstCudaIpcClient * client, GstSample ** sample)
{
  GstCudaIpcClientPrivate *priv;

  g_return_val_if_fail (GST_IS_CUDA_IPC_CLIENT (client), GST_FLOW_ERROR);
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

void
gst_cuda_ipc_client_set_flushing (GstCudaIpcClient * client, bool flushing)
{
  GstCudaIpcClientPrivate *priv;
  GstCudaIpcClientClass *klass;

  g_return_if_fail (GST_IS_CUDA_IPC_CLIENT (client));

  priv = client->priv;
  klass = GST_CUDA_IPC_CLIENT_GET_CLASS (client);

  GST_DEBUG_OBJECT (client, "Setting flush %d", flushing);

  klass->set_flushing (client, flushing);

  std::lock_guard < std::mutex > lk (priv->lock);
  priv->flushing = flushing;
  priv->cond.notify_all ();

  GST_DEBUG_OBJECT (client, "Setting flush %d done", flushing);
}

static gpointer
gst_cuda_ipc_client_loop_thread_func (GstCudaIpcClient * self)
{
  GstCudaIpcClientPrivate *priv = self->priv;
  GstCudaIpcClientClass *klass = GST_CUDA_IPC_CLIENT_GET_CLASS (self);

  g_assert (klass->loop);

  GST_DEBUG_OBJECT (self, "Starting loop thread");

  klass->loop (self);

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
gst_cuda_ipc_client_run (GstCudaIpcClient * client)
{
  GstCudaIpcClientPrivate *priv;
  guint device_id;

  g_return_val_if_fail (GST_IS_CUDA_IPC_CLIENT (client), GST_FLOW_ERROR);

  priv = client->priv;

  if (!client->context) {
    GST_ERROR_OBJECT (client, "Context is not configured");
    return GST_FLOW_ERROR;
  }

  g_object_get (client->context, "cuda-device-id", &device_id, nullptr);
  priv->device_id = (int) device_id;

  std::unique_lock < std::mutex > lk (priv->lock);
  if (!priv->loop_thread) {
    priv->loop_thread = g_thread_new ("cuda-ipc-client",
        (GThreadFunc) gst_cuda_ipc_client_loop_thread_func, client);

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
gst_cuda_ipc_client_get_caps (GstCudaIpcClient * client)
{
  GstCaps *caps = nullptr;
  GstCudaIpcClientPrivate *priv;

  g_return_val_if_fail (GST_IS_CUDA_IPC_CLIENT (client), nullptr);

  priv = client->priv;

  if (gst_cuda_ipc_client_run (client) != GST_FLOW_OK)
    return nullptr;

  std::lock_guard < std::mutex > lk (priv->lock);
  if (priv->caps)
    caps = gst_caps_ref (priv->caps);

  return caps;
}

static void
gst_cuda_ipc_client_stop_async (GstCudaIpcClient * client, gpointer user_data)
{
  GstCudaIpcClientPrivate *priv;
  GstCudaIpcClientClass *klass;

  priv = client->priv;
  klass = GST_CUDA_IPC_CLIENT_GET_CLASS (client);

  GST_DEBUG_OBJECT (client, "Stopping");
  priv->shutdown = true;
  klass->invoke (client);

  std::unique_lock < std::mutex > lk (priv->lock);
  while (!priv->aborted)
    priv->cond.wait (lk);
  lk.unlock ();

  GST_DEBUG_OBJECT (client, "Terminating");

  klass->terminate (client);

  g_clear_pointer (&priv->loop_thread, g_thread_join);

  GST_DEBUG_OBJECT (client, "Stopped");

  gst_object_unref (client);
}

static void
gst_cuda_ipc_client_push_stop_async (GstCudaIpcClient * client)
{
  std::lock_guard < std::mutex > lk (gc_pool_lock);
  if (!gc_thread_pool) {
    gc_thread_pool = g_thread_pool_new ((GFunc) gst_cuda_ipc_client_stop_async,
        nullptr, -1, FALSE, nullptr);
  }

  g_thread_pool_push (gc_thread_pool, gst_object_ref (client), nullptr);
}

void
gst_cuda_ipc_client_stop (GstCudaIpcClient * client)
{
  GstCudaIpcClientPrivate *priv;
  GstCudaIpcClientClass *klass;

  g_return_if_fail (GST_IS_CUDA_IPC_CLIENT (client));

  if (client->io_mode == GST_CUDA_IPC_IO_COPY) {
    priv = client->priv;
    klass = GST_CUDA_IPC_CLIENT_GET_CLASS (client);

    GST_DEBUG_OBJECT (client, "Stopping");
    priv->shutdown = true;
    klass->invoke (client);

    std::unique_lock < std::mutex > lk (priv->lock);
    while (!priv->aborted)
      priv->cond.wait (lk);
    lk.unlock ();

    GST_DEBUG_OBJECT (client, "Terminating");

    klass->terminate (client);

    g_clear_pointer (&priv->loop_thread, g_thread_join);

    GST_DEBUG_OBJECT (client, "Stopped");
  } else {
    gst_cuda_ipc_client_push_stop_async (client);
  }
}

static void
gst_cuda_ipc_client_send_msg (GstCudaIpcClient * self)
{
  GstCudaIpcClientPrivate *priv = self->priv;
  GstCudaIpcClientClass *klass = GST_CUDA_IPC_CLIENT_GET_CLASS (self);
  auto conn = priv->conn;

  priv->io_pending = true;
  if (!klass->send_msg (self, conn.get ())) {
    GST_WARNING_OBJECT (self, "Send msg failed");
    priv->io_pending = false;
    gst_cuda_ipc_client_abort (self);
  }
}

static void
gst_cuda_ipc_client_wait_msg (GstCudaIpcClient * self)
{
  GstCudaIpcClientPrivate *priv = self->priv;
  GstCudaIpcClientClass *klass = GST_CUDA_IPC_CLIENT_GET_CLASS (self);
  auto conn = priv->conn;

  priv->io_pending = true;
  if (!klass->wait_msg (self, conn.get ())) {
    GST_WARNING_OBJECT (self, "Wait msg failed");
    priv->io_pending = false;
    gst_cuda_ipc_client_abort (self);
  }
}

void
gst_cuda_ipc_client_new_connection (GstCudaIpcClient * client,
    std::shared_ptr < GstCudaIpcClientConn > conn)
{
  GstCudaIpcClientPrivate *priv = client->priv;

  std::unique_lock < std::mutex > lk (priv->lock);
  if (priv->shutdown) {
    GST_DEBUG_OBJECT (client, "We are stopping now");
    return;
  }

  conn->client = client;
  priv->conn = conn;
  priv->cond.notify_all ();
  lk.unlock ();

  GST_LOG_OBJECT (client, "Waiting for CONFIG-DATA");
  gst_cuda_ipc_client_wait_msg (client);
}

void
gst_cuda_ipc_client_send_msg_finish (GstCudaIpcClient * client, bool result)
{
  GstCudaIpcClientPrivate *priv = client->priv;
  auto conn = priv->conn;

  if (!result) {
    GST_WARNING_OBJECT (client, "Send msg failed");
    gst_cuda_ipc_client_abort (client);
    return;
  }

  switch (conn->type) {
    case GstCudaIpcPktType::NEED_DATA:
      GST_LOG_OBJECT (client, "Sent NEED-DATA");
      gst_cuda_ipc_client_wait_msg (client);
      break;
    case GstCudaIpcPktType::READ_DONE:
      GST_LOG_OBJECT (client, "Sent READ-DONE");
      gst_cuda_ipc_client_continue (client);
      break;
    case GstCudaIpcPktType::RELEASE_DATA:
      GST_LOG_OBJECT (client, "Sent RELEASE-DATA");
      gst_cuda_ipc_client_continue (client);
      break;
    case GstCudaIpcPktType::RELEASE_MMAP_DATA:
      GST_LOG_OBJECT (client, "Sent RELEASE-MMAP-DATA");
      gst_cuda_ipc_client_continue (client);
      break;
    case GstCudaIpcPktType::FIN:
      GST_DEBUG_OBJECT (client, "Sent FIN");
      gst_cuda_ipc_client_abort (client);
      break;
    default:
      GST_ERROR_OBJECT (client, "Unexpected msg type");
      gst_cuda_ipc_client_abort (client);
      break;
  }
}

static void
gst_cuda_ipc_client_release_imported_data (GstCudaIpcReleaseData * data)
{
  GstCudaIpcClient *self = data->self;
  GstCudaIpcClientPrivate *priv = self->priv;
  GstCudaIpcClientClass *klass = GST_CUDA_IPC_CLIENT_GET_CLASS (self);

  auto handle = data->imported->handle->handle;
  auto handle_dump = gst_cuda_ipc_mem_handle_to_string (handle);

  GST_LOG_OBJECT (self, "Releasing data %s", handle_dump.c_str ());

  import_lock_.lock ();
  data->imported = nullptr;
  import_lock_.unlock ();

  priv->lock.lock ();
  priv->unused_data.push (handle);
  priv->lock.unlock ();

  klass->invoke (self);

  gst_object_unref (data->self);

  delete data;
}

static bool
gst_cuda_client_update_caps (GstCudaIpcClient * self, GstCaps * caps)
{
  GstCudaIpcClientPrivate *priv = self->priv;
  GstStructure *config;

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

  priv->pool = gst_cuda_buffer_pool_new (self->context);
  config = gst_buffer_pool_get_config (priv->pool);

  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  gst_buffer_pool_config_set_params (config, priv->caps,
      GST_VIDEO_INFO_SIZE (&priv->info), 0, 0);
  if (self->stream)
    gst_buffer_pool_config_set_cuda_stream (config, self->stream);

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

  return true;
}

static bool
gst_cuda_ipc_client_have_data (GstCudaIpcClient * self)
{
  GstCudaIpcClientPrivate *priv = self->priv;
  CUipcMemHandle handle;
  GstCudaIpcMemLayout layout;
  CUdeviceptr dptr;
  GstBuffer *buffer;
  GstMemory *mem;
  GstCudaMemory *cmem;
  GstMapInfo info;
  CUDA_MEMCPY2D copy_param;
  CUstream stream;
  GstSample *sample;
  GstClockTime pts;
  GstCaps *caps = nullptr;
  std::shared_ptr < GstCudaIpcImportData > import_data;
  std::unique_lock < std::mutex > lk (priv->lock);
  auto conn = priv->conn;
  std::vector < guint8 > meta;

  if (!gst_cuda_ipc_pkt_parse_have_data (conn->server_msg,
          pts, layout, handle, &caps, meta)) {
    GST_ERROR_OBJECT (self, "Couldn't parse HAVE-DATA packet");
    return false;
  }

  if (!gst_cuda_client_update_caps (self, caps))
    return false;

  auto handle_dump = gst_cuda_ipc_mem_handle_to_string (handle);
  GST_LOG_OBJECT (self,
      "Importing handle %s, size %u, pitch %u, offset %u, %u, %u, %u",
      handle_dump.c_str (), layout.size, layout.pitch, layout.offset[0],
      layout.offset[1], layout.offset[2], layout.offset[3]);

  auto import_handle = _ipc_importer.ImportHandle (handle, self->context);
  if (!import_handle) {
    GST_ERROR_OBJECT (self, "Couldn't open handle %s", handle_dump.c_str ());
    return false;
  }

  dptr = import_handle->dptr;

  if (self->io_mode != GST_CUDA_IPC_IO_COPY) {
    auto it = priv->imported.begin ();
    while (it != priv->imported.end ()) {
      auto data = it->lock ();
      if (!data) {
        it = priv->imported.erase (it);
      } else if (data->handle == import_handle) {
        import_data = data;
        break;
      } else {
        it++;
      }
    };

    if (!import_data) {
      import_data = std::make_shared < GstCudaIpcImportData > ();
      import_data->handle = import_handle;
    }
  }

  if (self->io_mode == GST_CUDA_IPC_IO_COPY) {
    if (!gst_cuda_context_push (self->context)) {
      GST_ERROR_OBJECT (self, "Couldn't push context");
      return false;
    }

    gst_buffer_pool_acquire_buffer (priv->pool, &buffer, nullptr);
    mem = gst_buffer_peek_memory (buffer, 0);
    gst_memory_map (mem, &info, (GstMapFlags) (GST_MAP_WRITE | GST_MAP_CUDA));

    memset (&copy_param, 0, sizeof (CUDA_MEMCPY2D));

    cmem = GST_CUDA_MEMORY_CAST (mem);

    copy_param.srcMemoryType = CU_MEMORYTYPE_DEVICE;
    copy_param.srcPitch = layout.pitch;

    copy_param.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    copy_param.dstPitch = cmem->info.stride[0];

    stream = gst_cuda_stream_get_handle (self->stream);

    for (guint i = 0; i < GST_VIDEO_INFO_N_PLANES (&priv->info); i++) {
      copy_param.srcDevice = (CUdeviceptr) ((guint8 *) dptr + layout.offset[i]);
      copy_param.dstDevice = (CUdeviceptr) ((guint8 *) info.data +
          cmem->info.offset[i]);
      copy_param.WidthInBytes = GST_VIDEO_INFO_COMP_WIDTH (&priv->info, i)
          * GST_VIDEO_INFO_COMP_PSTRIDE (&priv->info, i);
      copy_param.Height = GST_VIDEO_INFO_COMP_HEIGHT (&priv->info, i);
      gst_cuda_result (CuMemcpy2DAsync (&copy_param, stream));
    }

    gst_cuda_result (CuStreamSynchronize (stream));
    gst_cuda_context_pop (nullptr);

    gst_memory_unmap (mem, &info);
    GST_MINI_OBJECT_FLAG_UNSET (mem, GST_CUDA_MEMORY_TRANSFER_NEED_SYNC);

    priv->unused_data.push (handle);
  } else {
    GstVideoInfo vinfo;

    vinfo = priv->info;
    vinfo.size = layout.size;
    for (guint i = 0; i < GST_VIDEO_INFO_N_PLANES (&priv->info); i++) {
      vinfo.stride[i] = layout.pitch;
      vinfo.offset[i] = layout.offset[i];
    }

    auto data = new GstCudaIpcReleaseData ();
    data->self = (GstCudaIpcClient *) gst_object_ref (self);
    data->imported = import_data;

    mem = gst_cuda_allocator_alloc_wrapped (nullptr, self->context,
        nullptr, &vinfo, dptr, data,
        (GDestroyNotify) gst_cuda_ipc_client_release_imported_data);
    GST_MINI_OBJECT_FLAG_SET (mem, GST_MEMORY_FLAG_READONLY);

    buffer = gst_buffer_new ();
    gst_buffer_append_memory (buffer, mem);

    gst_buffer_add_video_meta_full (buffer, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT (&vinfo), GST_VIDEO_INFO_WIDTH (&vinfo),
        GST_VIDEO_INFO_HEIGHT (&vinfo), GST_VIDEO_INFO_N_PLANES (&vinfo),
        vinfo.offset, vinfo.stride);
  }

  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (buffer) = GST_CLOCK_TIME_NONE;

  while (!meta.empty ()) {
    guint32 consumed = 0;
    if (!gst_meta_deserialize (buffer, meta.data (), meta.size (),
            &consumed) || consumed == 0) {
      break;
    }

    meta.erase (meta.begin (), meta.begin () + consumed);
  }

  sample = gst_sample_new (buffer, priv->caps, nullptr, nullptr);
  gst_buffer_unref (buffer);

  /* Drops too old samples */
  std::queue < GstSample * >drop_queue;
  while (priv->samples.size () > self->buffer_size) {
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
gst_cuda_ipc_client_run_gc (GstCudaIpcClient * self)
{
  GstCudaIpcClientPrivate *priv = self->priv;

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
gst_cuda_ipc_client_continue (GstCudaIpcClient * self)
{
  GstCudaIpcClientPrivate *priv = self->priv;
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
    auto handle = priv->unused_data.front ();
    priv->unused_data.pop ();

    gst_cuda_ipc_pkt_build_release_data (conn->client_msg, handle);
    conn->type = GstCudaIpcPktType::RELEASE_DATA;

    auto handle_dump = gst_cuda_ipc_mem_handle_to_string (handle);
    GST_LOG_OBJECT (self, "Sending RELEASE-DATA %s", handle_dump.c_str ());
    lk.unlock ();

    gst_cuda_ipc_client_send_msg (self);
    return;
  }

  if (!priv->unused_os_handle.empty ()) {
    auto handle = priv->unused_os_handle.front ();
    priv->unused_os_handle.pop ();

    gst_cuda_ipc_pkt_build_release_mmap_data (conn->client_msg, handle);
    conn->type = GstCudaIpcPktType::RELEASE_MMAP_DATA;

    GST_LOG_OBJECT (self, "Sending RELEASE-MMAP-DATA %"
        GST_CUDA_OS_HANDLE_FORMAT, handle);
    lk.unlock ();

    gst_cuda_ipc_client_send_msg (self);
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
    gst_cuda_ipc_client_run_gc (self);

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
        gst_cuda_ipc_pkt_build_fin (conn->client_msg);
        conn->type = GstCudaIpcPktType::FIN;

        GST_DEBUG_OBJECT (self, "Sending FIN");
        gst_cuda_ipc_client_send_msg (self);
        return;
      }
    } else {
      priv->io_pending = false;
    }
    return;
  }

  lk.unlock ();

  gst_cuda_ipc_pkt_build_need_data (conn->client_msg);
  conn->type = GstCudaIpcPktType::NEED_DATA;

  GST_LOG_OBJECT (self, "Sending NEED-DATA");
  gst_cuda_ipc_client_send_msg (self);
}

static bool
gst_cuda_ipc_client_config_data (GstCudaIpcClient * self)
{
  GstCudaIpcClientClass *klass = GST_CUDA_IPC_CLIENT_GET_CLASS (self);
  GstCudaIpcClientPrivate *priv = self->priv;
  GstCaps *caps = nullptr;
  auto conn = priv->conn;
  std::lock_guard < std::mutex > lk (priv->lock);
  GstCudaPid server_pid;
  gboolean use_mmap;

  if (!gst_cuda_ipc_pkt_parse_config (conn->server_msg, &server_pid,
          &use_mmap, &caps)) {
    GST_ERROR_OBJECT (self, "Couldn't parse CONFIG-DATA");
    return false;
  }

  if (!gst_cuda_client_update_caps (self, caps))
    return false;

  GST_DEBUG_OBJECT (self, "Got config, server pid %u, use-mmap %d",
      (guint) server_pid, use_mmap);

  if (klass->config) {
    if (!klass->config (self, server_pid, use_mmap)) {
      GST_ERROR_OBJECT (self, "Config failed");
      return false;
    }
  }

  priv->cond.notify_all ();

  return true;
}

void
gst_cuda_ipc_client_wait_msg_finish (GstCudaIpcClient * client, bool result)
{
  GstCudaIpcClientPrivate *priv = client->priv;
  GstCudaIpcPacketHeader header;
  auto conn = priv->conn;

  if (!result) {
    GST_WARNING_OBJECT (client, "Wait msg failed");
    gst_cuda_ipc_client_abort (client);
    return;
  }

  if (!gst_cuda_ipc_pkt_identify (conn->server_msg, header)) {
    GST_ERROR_OBJECT (client, "Broken header");
    gst_cuda_ipc_client_abort (client);
    return;
  }

  switch (header.type) {
    case GstCudaIpcPktType::CONFIG:
      GST_LOG_OBJECT (client, "Got CONFIG");
      if (!gst_cuda_ipc_client_config_data (client)) {
        gst_cuda_ipc_client_abort (client);
        return;
      }

      gst_cuda_ipc_client_continue (client);
      break;
    case GstCudaIpcPktType::HAVE_DATA:
      GST_LOG_OBJECT (client, "Got HAVE-DATA");
      if (!gst_cuda_ipc_client_have_data (client)) {
        gst_cuda_ipc_client_abort (client);
        return;
      }

      GST_LOG_OBJECT (client, "Sending READ-DONE");
      gst_cuda_ipc_pkt_build_read_done (conn->client_msg);
      conn->type = GstCudaIpcPktType::READ_DONE;
      gst_cuda_ipc_client_send_msg (client);
      break;
    case GstCudaIpcPktType::EOS:
      GST_DEBUG_OBJECT (client, "Got EOS");
      priv->server_eos = true;
      priv->lock.lock ();
      priv->cond.notify_all ();
      priv->lock.unlock ();
      gst_cuda_ipc_client_continue (client);
      break;
    default:
      GST_WARNING_OBJECT (client, "Unexpected packet type");
      gst_cuda_ipc_client_abort (client);
      break;
  }
}

void
gst_cuda_ipc_client_have_mmap_data (GstCudaIpcClient * client,
    GstClockTime pts, const GstCudaIpcMemLayout & layout, GstCaps * caps,
    GstCudaSharableHandle server_handle, GstCudaSharableHandle client_handle,
    std::vector < guint8 > &meta)
{
  GstCudaIpcClientPrivate *priv = client->priv;
  std::unique_lock < std::mutex > lk (priv->lock);
  CUDA_MEMCPY2D copy_param;
  CUstream stream;
  CUresult ret;
  CUmemGenericAllocationHandle handle;
  CUdeviceptr ptr;
  CUmemAccessDesc desc;
  GstBuffer *buffer;
  GstMemory *mem;
  GstCudaMemory *cmem;
  GstMapInfo info;
  GstSample *sample;
  std::queue < GstSample * >drop_queue;
  auto conn = priv->conn;

  desc.location.id = priv->device_id;
  desc.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  desc.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;

  if (!gst_cuda_client_update_caps (client, caps))
    goto error;

  if (!gst_cuda_context_push (client->context)) {
    GST_ERROR_OBJECT (client, "Couldn't push context");
    goto error;
  }

  ret = CuMemImportFromShareableHandle (&handle,
      (void *) ((guintptr) client_handle),
#ifdef G_OS_WIN32
      CU_MEM_HANDLE_TYPE_WIN32
#else
      CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR
#endif
      );

  ret = CuMemAddressReserve (&ptr, layout.max_size, 0, 0, 0);
  if (!gst_cuda_result (ret)) {
    GST_ERROR_OBJECT (client, "Couldn't reserve memory");
    gst_cuda_context_pop (nullptr);
    goto error;
  }

  if (!gst_cuda_result (ret)) {
    GST_ERROR_OBJECT (client, "Couldn't import handle");
    CuMemAddressFree (ptr, layout.max_size);
    gst_cuda_context_pop (nullptr);
    goto error;
  }

  ret = CuMemMap (ptr, layout.max_size, 0, handle, 0);
  if (!gst_cuda_result (ret)) {
    GST_ERROR_OBJECT (client, "Couldn't reserve memory");
    CuMemRelease (handle);
    CuMemAddressFree (ptr, layout.max_size);
    gst_cuda_context_pop (nullptr);
    goto error;
  }

  /* Once it's mapped, handle is not needed anymore */
  ret = CuMemRelease (handle);
  if (!gst_cuda_result (ret)) {
    GST_ERROR_OBJECT (client, "Couldn't release handle");
    CuMemUnmap (ptr, layout.max_size);
    CuMemAddressFree (ptr, layout.max_size);
    gst_cuda_context_pop (nullptr);
    goto error;
  }

  ret = CuMemSetAccess (ptr, layout.max_size, &desc, 1);
  if (!gst_cuda_result (ret)) {
    GST_ERROR_OBJECT (client, "Couldn't set access");
    CuMemUnmap (ptr, layout.max_size);
    CuMemAddressFree (ptr, layout.max_size);
    gst_cuda_context_pop (nullptr);
    goto error;
  }

  /* All done. OS handle is not needed anymore */
  gst_cuda_ipc_client_close_handle (client_handle);

  /* XXX: mapped memory does not seem to support CUDA texture / NVENC resource.
   * Always copy to our memory */
  gst_buffer_pool_acquire_buffer (priv->pool, &buffer, nullptr);
  mem = gst_buffer_peek_memory (buffer, 0);
  gst_memory_map (mem, &info, (GstMapFlags) (GST_MAP_WRITE | GST_MAP_CUDA));

  memset (&copy_param, 0, sizeof (CUDA_MEMCPY2D));

  cmem = GST_CUDA_MEMORY_CAST (mem);

  copy_param.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  copy_param.srcPitch = layout.pitch;

  copy_param.dstMemoryType = CU_MEMORYTYPE_DEVICE;
  copy_param.dstPitch = cmem->info.stride[0];

  stream = gst_cuda_stream_get_handle (client->stream);

  for (guint i = 0; i < GST_VIDEO_INFO_N_PLANES (&priv->info); i++) {
    copy_param.srcDevice = (CUdeviceptr) ((guint8 *) ptr + layout.offset[i]);
    copy_param.dstDevice = (CUdeviceptr) ((guint8 *) info.data +
        cmem->info.offset[i]);
    copy_param.WidthInBytes = GST_VIDEO_INFO_COMP_WIDTH (&priv->info, i)
        * GST_VIDEO_INFO_COMP_PSTRIDE (&priv->info, i);
    copy_param.Height = GST_VIDEO_INFO_COMP_HEIGHT (&priv->info, i);
    gst_cuda_result (CuMemcpy2DAsync (&copy_param, stream));
  }

  gst_cuda_result (CuStreamSynchronize (stream));

  gst_memory_unmap (mem, &info);
  GST_MINI_OBJECT_FLAG_UNSET (mem, GST_CUDA_MEMORY_TRANSFER_NEED_SYNC);

  gst_cuda_result (CuMemUnmap (ptr, layout.max_size));
  gst_cuda_result (CuMemAddressFree (ptr, layout.max_size));

  gst_cuda_context_pop (nullptr);

  priv->unused_os_handle.push (server_handle);

  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (buffer) = GST_CLOCK_TIME_NONE;

  while (!meta.empty ()) {
    guint32 consumed = 0;
    if (!gst_meta_deserialize (buffer, meta.data (), meta.size (),
            &consumed) || consumed == 0) {
      break;
    }

    meta.erase (meta.begin (), meta.begin () + consumed);
  }

  sample = gst_sample_new (buffer, priv->caps, nullptr, nullptr);
  gst_buffer_unref (buffer);

  /* Drops too old samples */
  while (priv->samples.size () > client->buffer_size) {
    drop_queue.push (priv->samples.front ());
    priv->samples.pop ();
  }

  priv->samples.push (sample);
  priv->cond.notify_all ();
  lk.unlock ();

  while (!drop_queue.empty ()) {
    auto old = drop_queue.front ();
    gst_sample_unref (old);
    drop_queue.pop ();
  }

  GST_LOG_OBJECT (client, "Sending READ-DONE");
  gst_cuda_ipc_pkt_build_read_done (conn->client_msg);
  conn->type = GstCudaIpcPktType::READ_DONE;
  gst_cuda_ipc_client_send_msg (client);
  return;

error:
  gst_cuda_ipc_client_close_handle (client_handle);
  lk.unlock ();
  gst_cuda_ipc_client_abort (client);
}

void
gst_cuda_ipc_client_abort (GstCudaIpcClient * client)
{
  GstCudaIpcClientPrivate *priv = client->priv;
  std::lock_guard < std::mutex > lk (priv->lock);
  priv->aborted = true;
  priv->cond.notify_all ();
}

void
gst_cuda_ipc_client_on_idle (GstCudaIpcClient * client)
{
  GstCudaIpcClientPrivate *priv = client->priv;
  if (priv->io_pending)
    return;

  gst_cuda_ipc_client_continue (client);
}
