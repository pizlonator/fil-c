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

#include "gstcudaipcclient_win32.h"
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>

GST_DEBUG_CATEGORY_EXTERN (cuda_ipc_client_debug);
#define GST_CAT_DEFAULT cuda_ipc_client_debug

/* *INDENT-OFF* */
struct GstCudaIpcClientConnWin32 : public GstCudaIpcClientConn
{
  GstCudaIpcClientConnWin32 (HANDLE pipe) : pipe (pipe)
  {
    OVERLAPPED *parent = static_cast<OVERLAPPED *> (this);
    parent->Internal = 0;
    parent->InternalHigh = 0;
    parent->Offset = 0;
    parent->OffsetHigh = 0;
  }

  ~GstCudaIpcClientConnWin32 ()
  {
    if (pipe != INVALID_HANDLE_VALUE) {
      CancelIo (pipe);
      CloseHandle (pipe);
    }
  }

  HANDLE pipe;
};

struct GstCudaIpcClientWin32Private
{
  GstCudaIpcClientWin32Private ()
  {
    wakeup_event = CreateEvent (nullptr, FALSE, FALSE, nullptr);
    cancellable = CreateEvent (nullptr, TRUE, FALSE, nullptr);
  }

  ~GstCudaIpcClientWin32Private ()
  {
    CloseHandle (wakeup_event);
    CloseHandle (cancellable);
    if (server_process)
      CloseHandle (server_process);
  }

  std::string address;
  GstClockTime timeout;
  std::mutex lock;
  std::condition_variable cond;

  HANDLE wakeup_event;
  HANDLE cancellable;
  HANDLE server_process = nullptr;
  guint last_err = ERROR_SUCCESS;
  bool flushing = false;
};
/* *INDENT-ON* */

struct _GstCudaIpcClientWin32
{
  GstCudaIpcClient parent;

  GstCudaIpcClientWin32Private *priv;
};

static void gst_cuda_ipc_client_win32_finalize (GObject * object);
static bool gst_cuda_ipc_client_win32_send_msg (GstCudaIpcClient * client,
    GstCudaIpcClientConn * conn);
static bool gst_cuda_ipc_client_win32_wait_msg (GstCudaIpcClient * client,
    GstCudaIpcClientConn * conn);
static void gst_cuda_ipc_client_win32_terminate (GstCudaIpcClient * client);
static void gst_cuda_ipc_client_win32_invoke (GstCudaIpcClient * client);
static void gst_cuda_ipc_client_win32_set_flushing (GstCudaIpcClient * client,
    bool flushing);
static void gst_cuda_ipc_client_win32_loop (GstCudaIpcClient * client);
static bool gst_cuda_ipc_client_win32_config (GstCudaIpcClient * client,
    GstCudaPid pid, gboolean use_mmap);

#define gst_cuda_ipc_client_win32_parent_class parent_class
G_DEFINE_TYPE (GstCudaIpcClientWin32,
    gst_cuda_ipc_client_win32, GST_TYPE_CUDA_IPC_CLIENT);

static void
gst_cuda_ipc_client_win32_class_init (GstCudaIpcClientWin32Class * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstCudaIpcClientClass *client_class = GST_CUDA_IPC_CLIENT_CLASS (klass);

  object_class->finalize = gst_cuda_ipc_client_win32_finalize;

  client_class->send_msg =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_win32_send_msg);
  client_class->wait_msg =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_win32_wait_msg);
  client_class->terminate =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_win32_terminate);
  client_class->invoke = GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_win32_invoke);
  client_class->set_flushing =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_win32_set_flushing);
  client_class->loop = GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_win32_loop);
  client_class->config = GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_win32_config);
}

static void
gst_cuda_ipc_client_win32_init (GstCudaIpcClientWin32 * self)
{
  self->priv = new GstCudaIpcClientWin32Private ();
}

static void
gst_cuda_ipc_client_win32_finalize (GObject * object)
{
  GstCudaIpcClientWin32 *self = GST_CUDA_IPC_CLIENT_WIN32 (object);

  GST_DEBUG_OBJECT (self, "finalize");

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void WINAPI
gst_cuda_ipc_client_win32_send_msg_finish (DWORD error_code, DWORD size,
    OVERLAPPED * overlap)
{
  GstCudaIpcClientConnWin32 *win32_conn =
      static_cast < GstCudaIpcClientConnWin32 * >(overlap);
  GstCudaIpcClient *client = win32_conn->client;
  bool ret = true;

  if (error_code != ERROR_SUCCESS) {
    auto err = gst_cuda_ipc_win32_error_to_string (error_code);
    GST_WARNING_OBJECT (client, "WriteFileEx callback failed with 0x%x (%s)",
        (guint) error_code, err.c_str ());
    ret = false;
  }

  gst_cuda_ipc_client_send_msg_finish (client, ret);
}

static bool
gst_cuda_ipc_client_win32_send_msg (GstCudaIpcClient * client,
    GstCudaIpcClientConn * conn)
{
  GstCudaIpcClientConnWin32 *win32_conn =
      static_cast < GstCudaIpcClientConnWin32 * >(conn);

  if (!WriteFileEx (win32_conn->pipe, &conn->client_msg[0],
          conn->client_msg.size (), win32_conn,
          gst_cuda_ipc_client_win32_send_msg_finish)) {
    guint last_err = GetLastError ();
    auto err = gst_cuda_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (client, "WriteFileEx failed with 0x%x (%s)",
        last_err, err.c_str ());
    return false;
  }

  return true;
}

static void
gst_cuda_ipc_client_win32_finish_have_mmap_data (GstCudaIpcClient * client,
    GstCudaIpcClientConnWin32 * win32_conn)
{
  GstCudaIpcClientWin32 *self = GST_CUDA_IPC_CLIENT_WIN32 (client);
  GstCudaIpcClientWin32Private *priv = self->priv;
  GstClockTime pts;
  GstCudaIpcMemLayout layout;
  GstCudaSharableHandle server_handle = nullptr;
  GstCudaSharableHandle client_handle = nullptr;
  GstCaps *caps = nullptr;
  std::vector < guint8 > meta;

  if (!priv->server_process) {
    GST_ERROR_OBJECT (self, "Server process handle is not available");
    goto error;
  }

  if (!gst_cuda_ipc_pkt_parse_have_mmap_data (win32_conn->server_msg, pts,
          layout, &server_handle, &caps, meta)) {
    GST_ERROR_OBJECT (self, "Couldn't parse MMAP-DATA");
    goto error;
  }

  if (!DuplicateHandle (priv->server_process, server_handle,
          GetCurrentProcess (), &client_handle, 0, FALSE,
          DUPLICATE_SAME_ACCESS)) {
    DWORD error_code = GetLastError ();
    auto err = gst_cuda_ipc_win32_error_to_string (error_code);
    GST_ERROR_OBJECT (self, "Couldn't duplicate handle, 0x%x (%s)",
        (guint) error_code, err.c_str ());
    goto error;
  }

  gst_cuda_ipc_client_have_mmap_data (client, pts, layout, caps,
      server_handle, client_handle, meta);
  return;

error:
  gst_cuda_ipc_client_wait_msg_finish (client, false);
}

static void WINAPI
gst_cuda_ipc_client_win32_payload_finish (DWORD error_code, DWORD size,
    OVERLAPPED * overlap)
{
  GstCudaIpcClientConnWin32 *win32_conn =
      static_cast < GstCudaIpcClientConnWin32 * >(overlap);
  GstCudaIpcClient *client = win32_conn->client;
  GstCudaIpcPacketHeader header;
  bool ret = true;

  if (error_code != ERROR_SUCCESS) {
    auto err = gst_cuda_ipc_win32_error_to_string (error_code);
    GST_WARNING_OBJECT (client, "ReadFileEx callback failed with 0x%x (%s)",
        (guint) error_code, err.c_str ());
    ret = false;
  } else if (!gst_cuda_ipc_pkt_identify (win32_conn->server_msg, header)) {
    GST_ERROR_OBJECT (client, "Broken header");
    ret = false;
  } else if (header.type == GstCudaIpcPktType::HAVE_MMAP_DATA) {
    gst_cuda_ipc_client_win32_finish_have_mmap_data (client, win32_conn);
    return;
  }

  gst_cuda_ipc_client_wait_msg_finish (client, ret);
}

static void WINAPI
gst_cuda_ipc_client_win32_wait_msg_finish (DWORD error_code, DWORD size,
    OVERLAPPED * overlap)
{
  GstCudaIpcClientConnWin32 *win32_conn =
      static_cast < GstCudaIpcClientConnWin32 * >(overlap);
  GstCudaIpcClient *client = win32_conn->client;
  GstCudaIpcPacketHeader header;

  if (error_code != ERROR_SUCCESS) {
    auto err = gst_cuda_ipc_win32_error_to_string (error_code);
    GST_WARNING_OBJECT (client, "ReadFileEx callback failed with 0x%x (%s)",
        (guint) error_code, err.c_str ());
    gst_cuda_ipc_client_wait_msg_finish (client, false);
    return;
  }

  if (!gst_cuda_ipc_pkt_identify (win32_conn->server_msg, header)) {
    GST_ERROR_OBJECT (client, "Broken header");
    gst_cuda_ipc_client_wait_msg_finish (client, false);
    return;
  }

  if (header.payload_size == 0) {
    gst_cuda_ipc_client_wait_msg_finish (client, true);
    return;
  }

  GST_LOG_OBJECT (client, "Reading payload");

  if (!ReadFileEx (win32_conn->pipe, &win32_conn->server_msg[0] +
          GST_CUDA_IPC_PKT_HEADER_SIZE, header.payload_size, win32_conn,
          gst_cuda_ipc_client_win32_payload_finish)) {
    guint last_err = GetLastError ();
    auto err = gst_cuda_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (client, "ReadFileEx failed with 0x%x (%s)",
        last_err, err.c_str ());
    gst_cuda_ipc_client_wait_msg_finish (client, false);
  }
}

static bool
gst_cuda_ipc_client_win32_wait_msg (GstCudaIpcClient * client,
    GstCudaIpcClientConn * conn)
{
  GstCudaIpcClientConnWin32 *win32_conn =
      static_cast < GstCudaIpcClientConnWin32 * >(conn);

  if (!ReadFileEx (win32_conn->pipe, &conn->server_msg[0],
          GST_CUDA_IPC_PKT_HEADER_SIZE, win32_conn,
          gst_cuda_ipc_client_win32_wait_msg_finish)) {
    guint last_err = GetLastError ();
    auto err = gst_cuda_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (client, "ReadFileEx failed with 0x%x (%s)",
        last_err, err.c_str ());
    return false;
  }

  return true;
}

static void
gst_cuda_ipc_client_win32_terminate (GstCudaIpcClient * client)
{
  GstCudaIpcClientWin32 *self = GST_CUDA_IPC_CLIENT_WIN32 (client);
  GstCudaIpcClientWin32Private *priv = self->priv;

  SetEvent (priv->cancellable);
}

static void
gst_cuda_ipc_client_win32_invoke (GstCudaIpcClient * client)
{
  GstCudaIpcClientWin32 *self = GST_CUDA_IPC_CLIENT_WIN32 (client);
  GstCudaIpcClientWin32Private *priv = self->priv;

  SetEvent (priv->wakeup_event);
}

static void
gst_cuda_ipc_client_win32_set_flushing (GstCudaIpcClient * client,
    bool flushing)
{
  GstCudaIpcClientWin32 *self = GST_CUDA_IPC_CLIENT_WIN32 (client);
  GstCudaIpcClientWin32Private *priv = self->priv;

  std::lock_guard < std::mutex > lk (priv->lock);
  priv->flushing = flushing;
  priv->cond.notify_all ();
}

static bool
gst_cuda_ipc_client_win32_config (GstCudaIpcClient * client, GstCudaPid pid,
    gboolean use_mmap)
{
  GstCudaIpcClientWin32 *self = GST_CUDA_IPC_CLIENT_WIN32 (client);
  GstCudaIpcClientWin32Private *priv = self->priv;

  if (!use_mmap)
    return TRUE;

  if (priv->server_process) {
    GST_WARNING_OBJECT (self, "Have server process already");
    CloseHandle (priv->server_process);
  }

  priv->server_process = OpenProcess (PROCESS_DUP_HANDLE, FALSE, pid);

  if (priv->server_process)
    return true;

  DWORD error_code = GetLastError ();
  auto err = gst_cuda_ipc_win32_error_to_string (error_code);
  GST_ERROR_OBJECT (self, "Couldn't open server process, 0x%x (%s)",
      (guint) error_code, err.c_str ());

  return false;
}

static void
gst_cuda_ipc_client_win32_loop (GstCudaIpcClient * client)
{
  GstCudaIpcClientWin32 *self = GST_CUDA_IPC_CLIENT_WIN32 (client);
  GstCudaIpcClientWin32Private *priv = self->priv;
  DWORD mode = PIPE_READMODE_MESSAGE;
  guint wait_ret;
  HANDLE pipe = INVALID_HANDLE_VALUE;
  GstClockTime start_time = gst_util_get_timestamp ();

  std::unique_lock < std::mutex > lk (priv->lock);
  do {
    GstClockTime diff;

    if (priv->flushing) {
      GST_DEBUG_OBJECT (self, "We are flushing");
      gst_cuda_ipc_client_abort (client);
      return;
    }

    pipe = CreateFileA (priv->address.c_str (),
        GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED, nullptr);
    if (pipe != INVALID_HANDLE_VALUE)
      break;

    if (priv->timeout > 0) {
      diff = gst_util_get_timestamp () - start_time;
      if (diff > priv->timeout) {
        GST_WARNING_OBJECT (self, "Timeout");
        gst_cuda_ipc_client_abort (client);
        return;
      }
    }

    /* Retry per 100ms */
    GST_DEBUG_OBJECT (self, "Sleep for next retry");
    priv->cond.wait_for (lk, std::chrono::milliseconds (100));
  } while (true);
  lk.unlock ();

  g_assert (pipe != INVALID_HANDLE_VALUE);

  if (!SetNamedPipeHandleState (pipe, &mode, nullptr, nullptr)) {
    priv->last_err = GetLastError ();
    auto err = gst_cuda_ipc_win32_error_to_string (priv->last_err);
    GST_WARNING_OBJECT (self, "SetNamedPipeHandleState failed with 0x%x (%s)",
        priv->last_err, err.c_str ());

    CloseHandle (pipe);
    gst_cuda_ipc_client_abort (client);
    return;
  }

  auto conn = std::make_shared < GstCudaIpcClientConnWin32 > (pipe);
  gst_cuda_ipc_client_new_connection (client, conn);

  HANDLE waitables[] = { priv->cancellable, priv->wakeup_event };
  do {
    /* Enters alertable thread state and wait for I/O completion event
     * or cancellable event */
    wait_ret = WaitForMultipleObjectsEx (G_N_ELEMENTS (waitables), waitables,
        FALSE, INFINITE, TRUE);
    if (wait_ret == WAIT_OBJECT_0) {
      GST_DEBUG ("Operation cancelled");
      return;
    }

    switch (wait_ret) {
      case WAIT_IO_COMPLETION:
        break;
      case WAIT_OBJECT_0 + 1:
        gst_cuda_ipc_client_on_idle (client);
        break;
      default:
        GST_WARNING ("Unexpected wait return 0x%x", wait_ret);
        gst_cuda_ipc_client_abort (client);
        return;
    }
  } while (true);
}

GstCudaIpcClient *
gst_cuda_ipc_client_new (const gchar * address, GstCudaContext * context,
    GstCudaStream * stream, GstCudaIpcIOMode io_mode, guint timeout,
    guint buffer_size)
{
  GstCudaIpcClient *client;
  GstCudaIpcClientWin32 *self;

  g_return_val_if_fail (address, nullptr);
  g_return_val_if_fail (GST_IS_CUDA_CONTEXT (context), nullptr);

  self = (GstCudaIpcClientWin32 *)
      g_object_new (GST_TYPE_CUDA_IPC_CLIENT_WIN32, nullptr);
  gst_object_ref_sink (self);

  self->priv->address = address;
  self->priv->timeout = timeout * GST_SECOND;

  client = GST_CUDA_IPC_CLIENT (self);
  client->context = (GstCudaContext *) gst_object_ref (context);
  if (stream)
    client->stream = gst_cuda_stream_ref (stream);
  client->io_mode = io_mode;
  client->buffer_size = buffer_size;

  return client;
}
