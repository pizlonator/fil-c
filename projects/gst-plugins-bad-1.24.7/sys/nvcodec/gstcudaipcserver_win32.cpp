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

#include "gstcudaipcserver_win32.h"
#include <string.h>
#include <string>

GST_DEBUG_CATEGORY_EXTERN (cuda_ipc_server_debug);
#define GST_CAT_DEFAULT cuda_ipc_server_debug

/* *INDENT-OFF* */
struct GstCudaIpcServerConnWin32 : public GstCudaIpcServerConn
{
  GstCudaIpcServerConnWin32 (HANDLE pipe_handle) : pipe (pipe_handle)
  {
    OVERLAPPED *parent = static_cast<OVERLAPPED *> (this);
    parent->Internal = 0;
    parent->InternalHigh = 0;
    parent->Offset = 0;
    parent->OffsetHigh = 0;
  }

  virtual ~GstCudaIpcServerConnWin32 ()
  {
    CloseConn ();
  }

  void CloseConn ()
  {
    if (pipe != INVALID_HANDLE_VALUE) {
      CancelIo (pipe);
      DisconnectNamedPipe (pipe);
      CloseHandle (pipe);
      pipe = INVALID_HANDLE_VALUE;
    }
  }

  HANDLE pipe = INVALID_HANDLE_VALUE;
};

struct GstCudaIpcServerWin32Private
{
  GstCudaIpcServerWin32Private ()
  {
    cancellable = CreateEvent (nullptr, TRUE, FALSE, nullptr);
    wakeup_event = CreateEvent (nullptr, FALSE, FALSE, nullptr);
  }

  ~GstCudaIpcServerWin32Private ()
  {
    CloseHandle (cancellable);
    CloseHandle (wakeup_event);
  }

  std::string address;
  HANDLE cancellable;
  HANDLE wakeup_event;
};
/* *INDENT-ON* */

struct _GstCudaIpcServerWin32
{
  GstCudaIpcServer parent;

  GstCudaIpcServerWin32Private *priv;
};

static void gst_cuda_ipc_server_win32_finalize (GObject * object);
static void gst_cuda_ipc_server_win32_loop (GstCudaIpcServer * server);
static void gst_cuda_ipc_server_win32_terminate (GstCudaIpcServer * server);
static void gst_cuda_ipc_server_win32_invoke (GstCudaIpcServer * server);
static bool gst_cuda_ipc_server_win32_wait_msg (GstCudaIpcServer * server,
    GstCudaIpcServerConn * conn);
static bool gst_cuda_ipc_server_win32_send_msg (GstCudaIpcServer * server,
    GstCudaIpcServerConn * conn);

#define gst_cuda_ipc_server_win32_parent_class parent_class
G_DEFINE_TYPE (GstCudaIpcServerWin32,
    gst_cuda_ipc_server_win32, GST_TYPE_CUDA_IPC_SERVER);

static void
gst_cuda_ipc_server_win32_class_init (GstCudaIpcServerWin32Class * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstCudaIpcServerClass *server_class = GST_CUDA_IPC_SERVER_CLASS (klass);

  object_class->finalize = gst_cuda_ipc_server_win32_finalize;

  server_class->loop = GST_DEBUG_FUNCPTR (gst_cuda_ipc_server_win32_loop);
  server_class->terminate =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_server_win32_terminate);
  server_class->invoke = GST_DEBUG_FUNCPTR (gst_cuda_ipc_server_win32_invoke);
  server_class->wait_msg =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_server_win32_wait_msg);
  server_class->send_msg =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_server_win32_send_msg);
}

static void
gst_cuda_ipc_server_win32_init (GstCudaIpcServerWin32 * self)
{
  self->priv = new GstCudaIpcServerWin32Private ();
}

static void
gst_cuda_ipc_server_win32_finalize (GObject * object)
{
  GstCudaIpcServerWin32 *self = GST_CUDA_IPC_SERVER_WIN32 (object);

  GST_DEBUG_OBJECT (self, "finalize");

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_cuda_ipc_server_win32_terminate (GstCudaIpcServer * server)
{
  GstCudaIpcServerWin32 *self = GST_CUDA_IPC_SERVER_WIN32 (server);
  GstCudaIpcServerWin32Private *priv = self->priv;

  GST_DEBUG_OBJECT (self, "terminate");

  SetEvent (priv->cancellable);
}

static void
gst_cuda_ipc_server_win32_invoke (GstCudaIpcServer * server)
{
  GstCudaIpcServerWin32 *self = GST_CUDA_IPC_SERVER_WIN32 (server);
  GstCudaIpcServerWin32Private *priv = self->priv;

  SetEvent (priv->wakeup_event);
}

static void WINAPI
gst_cuda_ipc_server_win32_payload_finish (DWORD error_code, DWORD size,
    OVERLAPPED * overlap)
{
  GstCudaIpcServerConnWin32 *win32_conn =
      static_cast < GstCudaIpcServerConnWin32 * >(overlap);
  GstCudaIpcServerWin32 *self = GST_CUDA_IPC_SERVER_WIN32 (win32_conn->server);
  bool ret = true;

  if (error_code != ERROR_SUCCESS) {
    auto err = gst_cuda_ipc_win32_error_to_string (error_code);
    GST_WARNING_OBJECT (self, "ReadFileEx callback failed with 0x%x (%s)",
        (guint) error_code, err.c_str ());
    ret = false;
  }

  gst_cuda_ipc_server_wait_msg_finish (win32_conn->server, win32_conn, ret);
}

static void WINAPI
gst_cuda_ipc_server_win32_wait_msg_finish (DWORD error_code, DWORD size,
    OVERLAPPED * overlap)
{
  GstCudaIpcServerConnWin32 *win32_conn =
      static_cast < GstCudaIpcServerConnWin32 * >(overlap);
  GstCudaIpcServerWin32 *self = GST_CUDA_IPC_SERVER_WIN32 (win32_conn->server);
  GstCudaIpcPacketHeader header;

  if (error_code != ERROR_SUCCESS) {
    auto err = gst_cuda_ipc_win32_error_to_string (error_code);
    GST_WARNING_OBJECT (self, "ReadFileEx callback failed with 0x%x (%s)",
        (guint) error_code, err.c_str ());
    gst_cuda_ipc_server_wait_msg_finish (win32_conn->server, win32_conn, false);
    return;
  }

  if (!gst_cuda_ipc_pkt_identify (win32_conn->client_msg, header)) {
    GST_ERROR_OBJECT (self, "Broken header");
    gst_cuda_ipc_server_wait_msg_finish (win32_conn->server, win32_conn, false);
    return;
  }

  if (header.payload_size == 0) {
    gst_cuda_ipc_server_wait_msg_finish (win32_conn->server, win32_conn, true);
    return;
  }

  GST_LOG_OBJECT (self, "Reading payload");

  if (!ReadFileEx (win32_conn->pipe, &win32_conn->client_msg[0] +
          GST_CUDA_IPC_PKT_HEADER_SIZE, header.payload_size, win32_conn,
          gst_cuda_ipc_server_win32_payload_finish)) {
    guint last_err = GetLastError ();
    auto err = gst_cuda_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (self, "ReadFileEx failed with 0x%x (%s)",
        last_err, err.c_str ());
    gst_cuda_ipc_server_wait_msg_finish (win32_conn->server, win32_conn, false);
  }
}

static bool
gst_cuda_ipc_server_win32_wait_msg (GstCudaIpcServer * server,
    GstCudaIpcServerConn * conn)
{
  GstCudaIpcServerWin32 *self = GST_CUDA_IPC_SERVER_WIN32 (server);
  GstCudaIpcServerConnWin32 *win32_conn =
      static_cast < GstCudaIpcServerConnWin32 * >(conn);

  GST_LOG_OBJECT (self, "Waiting for client message");

  if (!ReadFileEx (win32_conn->pipe, &conn->client_msg[0],
          GST_CUDA_IPC_PKT_HEADER_SIZE, win32_conn,
          gst_cuda_ipc_server_win32_wait_msg_finish)) {
    guint last_err = GetLastError ();
    auto err = gst_cuda_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (self, "ReadFileEx failed with 0x%x (%s)",
        last_err, err.c_str ());
    return false;
  }

  return true;
}

static void WINAPI
gst_cuda_ipc_server_win32_send_msg_finish (DWORD error_code, DWORD size,
    OVERLAPPED * overlap)
{
  GstCudaIpcServerConnWin32 *win32_conn =
      static_cast < GstCudaIpcServerConnWin32 * >(overlap);
  GstCudaIpcServerWin32 *self = GST_CUDA_IPC_SERVER_WIN32 (win32_conn->server);
  bool ret = true;

  if (error_code != ERROR_SUCCESS) {
    auto err = gst_cuda_ipc_win32_error_to_string (error_code);
    GST_WARNING_OBJECT (self, "ReadFileEx callback failed with 0x%x (%s)",
        (guint) error_code, err.c_str ());
    ret = false;
  }

  GST_LOG_OBJECT (self, "Sent message");

  gst_cuda_ipc_server_send_msg_finish (win32_conn->server, win32_conn, ret);
}

static bool
gst_cuda_ipc_server_win32_send_msg (GstCudaIpcServer * server,
    GstCudaIpcServerConn * conn)
{
  GstCudaIpcServerWin32 *self = GST_CUDA_IPC_SERVER_WIN32 (server);
  GstCudaIpcServerConnWin32 *win32_conn =
      static_cast < GstCudaIpcServerConnWin32 * >(conn);

  GST_LOG_OBJECT (self, "Sending message");

  if (!WriteFileEx (win32_conn->pipe, &conn->server_msg[0],
          conn->server_msg.size (), win32_conn,
          gst_cuda_ipc_server_win32_send_msg_finish)) {
    guint last_err = GetLastError ();
    auto err = gst_cuda_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (self, "WriteFileEx failed with 0x%x (%s)",
        last_err, err.c_str ());
    return false;
  }

  return true;
}

static HANDLE
gst_cuda_ipc_server_win32_create_pipe (GstCudaIpcServerWin32 * self,
    OVERLAPPED * overlap, bool &io_pending)
{
  GstCudaIpcServerWin32Private *priv = self->priv;
  HANDLE pipe = CreateNamedPipeA (priv->address.c_str (),
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES, 1024, 1024, 5000, nullptr);

  if (pipe == INVALID_HANDLE_VALUE) {
    guint last_err = GetLastError ();
    auto err = gst_cuda_ipc_win32_error_to_string (last_err);
    GST_ERROR_OBJECT (self, "CreateNamedPipeA failed with 0x%x (%s)",
        last_err, err.c_str ());
    return INVALID_HANDLE_VALUE;
  }

  if (ConnectNamedPipe (pipe, overlap)) {
    guint last_err = GetLastError ();
    auto err = gst_cuda_ipc_win32_error_to_string (last_err);
    GST_ERROR_OBJECT (self, "ConnectNamedPipe failed with 0x%x (%s)",
        last_err, err.c_str ());
    return INVALID_HANDLE_VALUE;
  }

  io_pending = false;
  guint last_err = GetLastError ();

  switch (last_err) {
    case ERROR_IO_PENDING:
      io_pending = true;
      break;
    case ERROR_PIPE_CONNECTED:
      SetEvent (overlap->hEvent);
      break;
    default:
    {
      auto err = gst_cuda_ipc_win32_error_to_string (last_err);
      GST_ERROR_OBJECT (self, "ConnectNamedPipe failed with 0x%x (%s)",
          last_err, err.c_str ());
      CloseHandle (pipe);
      return INVALID_HANDLE_VALUE;
    }
  }

  return pipe;
}

static void
gst_cuda_ipc_server_win32_loop (GstCudaIpcServer * server)
{
  GstCudaIpcServerWin32 *self = GST_CUDA_IPC_SERVER_WIN32 (server);
  GstCudaIpcServerWin32Private *priv = self->priv;
  bool io_pending = false;
  guint wait_ret;
  HANDLE pipe;
  OVERLAPPED overlap;

  GST_DEBUG_OBJECT (self, "Entering loop");

  memset (&overlap, 0, sizeof (OVERLAPPED));

  overlap.hEvent = CreateEvent (nullptr, TRUE, TRUE, nullptr);

  pipe = gst_cuda_ipc_server_win32_create_pipe (self, &overlap, io_pending);
  if (pipe == INVALID_HANDLE_VALUE) {
    CloseHandle (overlap.hEvent);
    gst_cuda_ipc_server_abort (server);
    return;
  }

  HANDLE waitables[] =
      { overlap.hEvent, priv->wakeup_event, priv->cancellable };

  do {
    wait_ret = WaitForMultipleObjectsEx (G_N_ELEMENTS (waitables), waitables,
        FALSE, INFINITE, TRUE);

    if (wait_ret == WAIT_OBJECT_0 + 2) {
      GST_DEBUG_OBJECT (self, "Operation cancelled");
      goto out;
    }

    switch (wait_ret) {
      case WAIT_OBJECT_0:
      {
        DWORD n_bytes;

        if (io_pending
            && !GetOverlappedResult (pipe, &overlap, &n_bytes, FALSE)) {
          guint last_err = GetLastError ();
          auto err = gst_cuda_ipc_win32_error_to_string (last_err);
          GST_WARNING_OBJECT (self, "GetOverlappedResult failed with 0x%x (%s)",
              last_err, err.c_str ());
          CloseHandle (pipe);
          pipe = INVALID_HANDLE_VALUE;
          break;
        }

        auto conn = std::make_shared < GstCudaIpcServerConnWin32 > (pipe);
        pipe = INVALID_HANDLE_VALUE;
        gst_cuda_ipc_server_on_incoming_connection (server, conn);

        pipe = gst_cuda_ipc_server_win32_create_pipe (self,
            &overlap, io_pending);
        break;
      }
      case WAIT_IO_COMPLETION:
        break;
      case WAIT_OBJECT_0 + 1:
        gst_cuda_ipc_server_on_idle (server);
        break;
      default:
      {
        guint last_err = GetLastError ();
        auto err = gst_cuda_ipc_win32_error_to_string (last_err);
        GST_ERROR_OBJECT (self,
            "WaitForMultipleObjectsEx return 0x%x, last error 0x%x (%s)",
            wait_ret, last_err, err.c_str ());
        gst_cuda_ipc_server_abort (server);

        goto out;
      }
    }
  } while (true);

out:
  if (pipe != INVALID_HANDLE_VALUE) {
    CancelIo (pipe);
    DisconnectNamedPipe (pipe);
    CloseHandle (pipe);
  }

  CloseHandle (overlap.hEvent);

  GST_DEBUG_OBJECT (self, "Exit loop");
}

GstCudaIpcServer *
gst_cuda_ipc_server_new (const gchar * address, GstCudaContext * context,
    GstCudaIpcMode ipc_mode)
{
  GstCudaIpcServerWin32 *self;
  GstCudaIpcServer *server;

  g_return_val_if_fail (address, nullptr);
  g_return_val_if_fail (GST_IS_CUDA_CONTEXT (context), nullptr);

  self = (GstCudaIpcServerWin32 *)
      g_object_new (GST_TYPE_CUDA_IPC_SERVER_WIN32, nullptr);
  gst_object_ref_sink (self);

  self->priv->address = address;
  server = GST_CUDA_IPC_SERVER (self);
  server->context = (GstCudaContext *) gst_object_ref (context);
  server->ipc_mode = ipc_mode;
  server->pid = GetCurrentProcessId ();

  gst_cuda_ipc_server_run (server);

  return server;
}
