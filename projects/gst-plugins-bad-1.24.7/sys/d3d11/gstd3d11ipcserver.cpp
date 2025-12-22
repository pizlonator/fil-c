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

#include "gstd3d11ipcserver.h"
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <string>
#include <memory>

GST_DEBUG_CATEGORY_STATIC (gst_d3d11_ipc_server_debug);
#define GST_CAT_DEFAULT gst_d3d11_ipc_server_debug

/* *INDENT-OFF* */
struct GstD3D11IpcServerData
{
  ~GstD3D11IpcServerData ()
  {
    if (sample)
      gst_sample_unref (sample);
  }

  GstSample *sample = nullptr;
  HANDLE handle = nullptr;
  GstD3D11IpcMemLayout layout;
  GstClockTime pts;
  guint64 seq_num;
};

struct GstD3D11IpcServerConn : public OVERLAPPED
{
  GstD3D11IpcServerConn (HANDLE pipe_handle) : pipe (pipe_handle)
  {
    OVERLAPPED *parent = static_cast<OVERLAPPED *> (this);
    parent->Internal = 0;
    parent->InternalHigh = 0;
    parent->Offset = 0;
    parent->OffsetHigh = 0;

    client_msg.resize (GST_D3D11_IPC_PKT_HEADER_SIZE);
    server_msg.resize (GST_D3D11_IPC_PKT_HEADER_SIZE);
  }

  ~GstD3D11IpcServerConn()
  {
    if (pipe != INVALID_HANDLE_VALUE) {
      CancelIo (pipe);
      DisconnectNamedPipe (pipe);
      CloseHandle (pipe);
    }

    gst_clear_caps (&caps);
  }

  GstD3D11IpcServer *server;

  HANDLE pipe;

  GstD3D11IpcPktType type;
  std::vector<guint8> client_msg;
  std::vector<guint8> server_msg;
  std::shared_ptr<GstD3D11IpcServerData> data;
  std::vector<std::shared_ptr<GstD3D11IpcServerData>> peer_handles;
  GstCaps *caps = nullptr;
  guint64 seq_num = 0;
  guint id;
  bool eos = false;
  bool pending_have_data = false;
  bool configured = false;
};

struct GstD3D11IpcServerPrivate
{
  GstD3D11IpcServerPrivate ()
  {
    cancellable = CreateEvent (nullptr, TRUE, FALSE, nullptr);
    wakeup_event = CreateEvent (nullptr, FALSE, FALSE, nullptr);

    shutdown = false;
    aborted = false;
  }

  ~GstD3D11IpcServerPrivate ()
  {
    CloseHandle (cancellable);
    CloseHandle (wakeup_event);
    gst_clear_object (&device);
  }

  GstD3D11Device *device = nullptr;
  gint64 adapter_luid = 0;
  std::mutex lock;
  guint64 seq_num = 0;
  guint next_conn_id = 0;
  std::unordered_map<guint, std::shared_ptr<GstD3D11IpcServerConn>> conn_map;
  GThread *loop_thread = nullptr;
  std::atomic<bool>shutdown;
  std::atomic<bool>aborted;
  std::shared_ptr<GstD3D11IpcServerData> data;
  std::string address;
  HANDLE cancellable;
  HANDLE wakeup_event;
  DWORD pid;
};
/* *INDENT-ON* */

struct _GstD3D11IpcServer
{
  GstObject parent;

  GstD3D11IpcServerPrivate *priv;
};

static void gst_d3d11_ipc_server_dispose (GObject * object);
static void gst_d3d11_ipc_server_finalize (GObject * object);
static void gst_d3d11_ipc_server_on_idle (GstD3D11IpcServer * self);
static void gst_d3d11_ipc_server_send_msg (GstD3D11IpcServer * self,
    GstD3D11IpcServerConn * conn);
static void gst_d3d11_ipc_server_wait_msg (GstD3D11IpcServer * self,
    GstD3D11IpcServerConn * conn);

#define gst_d3d11_ipc_server_parent_class parent_class
G_DEFINE_TYPE (GstD3D11IpcServer, gst_d3d11_ipc_server, GST_TYPE_OBJECT);

static void
gst_d3d11_ipc_server_class_init (GstD3D11IpcServerClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_d3d11_ipc_server_finalize;
  object_class->dispose = gst_d3d11_ipc_server_dispose;

  GST_DEBUG_CATEGORY_INIT (gst_d3d11_ipc_server_debug, "d3d11ipcserver",
      0, "d3d11ipcserver");
}

static void
gst_d3d11_ipc_server_init (GstD3D11IpcServer * self)
{
  self->priv = new GstD3D11IpcServerPrivate ();
  self->priv->pid = GetCurrentProcessId ();
}

static void
gst_d3d11_ipc_server_dispose (GObject * object)
{
  GstD3D11IpcServer *self = GST_D3D11_IPC_SERVER (object);
  GstD3D11IpcServerPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "dispose");

  SetEvent (priv->cancellable);

  g_clear_pointer (&priv->loop_thread, g_thread_join);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_d3d11_ipc_server_finalize (GObject * object)
{
  GstD3D11IpcServer *self = GST_D3D11_IPC_SERVER (object);

  GST_DEBUG_OBJECT (self, "finalize");

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static HANDLE
gst_cuda_ipc_server_win32_create_pipe (GstD3D11IpcServer * self,
    OVERLAPPED * overlap, bool &io_pending)
{
  GstD3D11IpcServerPrivate *priv = self->priv;
  HANDLE pipe = CreateNamedPipeA (priv->address.c_str (),
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES, 1024, 1024, 5000, nullptr);

  if (pipe == INVALID_HANDLE_VALUE) {
    guint last_err = GetLastError ();
    auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
    GST_ERROR_OBJECT (self, "CreateNamedPipeA failed with 0x%x (%s)",
        last_err, err.c_str ());
    return INVALID_HANDLE_VALUE;
  }

  if (ConnectNamedPipe (pipe, overlap)) {
    guint last_err = GetLastError ();
    auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
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
      auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
      GST_ERROR_OBJECT (self, "ConnectNamedPipe failed with 0x%x (%s)",
          last_err, err.c_str ());
      CloseHandle (pipe);
      return INVALID_HANDLE_VALUE;
    }
  }

  return pipe;
}

static void
gst_d3d11_ipc_server_close_connection (GstD3D11IpcServer * self,
    GstD3D11IpcServerConn * conn)
{
  GstD3D11IpcServerPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Closing conn-id %u", conn->id);

  priv->conn_map.erase (conn->id);

  if (priv->shutdown && priv->conn_map.empty ()) {
    GST_DEBUG_OBJECT (self, "All connection were closed");
    SetEvent (priv->cancellable);
  }
}

static void
gst_d3d11_ipc_server_have_data (GstD3D11IpcServer * self,
    GstD3D11IpcServerConn * conn)
{
  GstCaps *caps;

  if (!conn->data) {
    GST_ERROR_OBJECT (self, "Have no data to send, conn-id: %u", conn->id);
    gst_d3d11_ipc_server_close_connection (self, conn);
    return;
  }

  conn->pending_have_data = false;
  conn->seq_num = conn->data->seq_num + 1;

  caps = gst_sample_get_caps (conn->data->sample);
  if (!conn->caps || !gst_caps_is_equal (conn->caps, caps)) {
    GST_DEBUG_OBJECT (self, "Sending caps %" GST_PTR_FORMAT " to conn-id %u",
        caps, conn->id);
    gst_caps_replace (&conn->caps, caps);
  } else {
    caps = nullptr;
  }

  GST_LOG_OBJECT (self, "Sending HAVE-DATA with handle \"%p\", conn-id :%u",
      conn->data->handle, conn->id);

  if (!gst_d3d11_ipc_pkt_build_have_data (conn->server_msg, conn->data->pts,
          conn->data->layout, conn->data->handle, caps)) {
    GST_ERROR_OBJECT (self, "Couldn't build HAVE-DATA pkt, conn-id: %u",
        conn->id);
    gst_d3d11_ipc_server_close_connection (self, conn);
    return;
  }

  conn->type = GstD3D11IpcPktType::HAVE_DATA;
  gst_d3d11_ipc_server_send_msg (self, conn);
}

static bool
gst_d3d11_ipc_server_on_release_data (GstD3D11IpcServer * self,
    GstD3D11IpcServerConn * conn)
{
  bool found = false;
  HANDLE handle = nullptr;

  if (!gst_d3d11_ipc_pkt_parse_release_data (conn->client_msg, handle)) {
    GST_ERROR_OBJECT (self, "Couldn't parse RELEASE-DATA, conn-id: %u",
        conn->id);
    return false;
  }

  GST_LOG_OBJECT (self, "RELEASE-DATA \"%p\", conn-id: %u", handle, conn->id);

  for (auto it = conn->peer_handles.begin (); it != conn->peer_handles.end ();
      it++) {
    auto other = (*it)->handle;
    if (handle == other) {
      found = true;
      conn->peer_handles.erase (it);
      break;
    }
  }

  if (!found) {
    GST_WARNING_OBJECT (self,
        "Unexpected name to remove, conn-id: %u", conn->id);
    return false;
  }

  GST_LOG_OBJECT (self, "Client is holding %" G_GSIZE_FORMAT " handles",
      conn->peer_handles.size ());

  return true;
}

static void
gst_d3d11_ipc_server_wait_msg_finish (GstD3D11IpcServer * server,
    GstD3D11IpcServerConn * conn)
{
  GstD3D11IpcPacketHeader header;

  if (!gst_d3d11_ipc_pkt_identify (conn->client_msg, header)) {
    GST_ERROR_OBJECT (server, "Broken header, conn-id: %u", conn->id);
    gst_d3d11_ipc_server_close_connection (server, conn);
    return;
  }

  switch (header.type) {
    case GstD3D11IpcPktType::NEED_DATA:
      GST_LOG_OBJECT (server, "NEED-DATA, conn-id: %u", conn->id);
      if (!conn->data) {
        GST_LOG_OBJECT (server, "Wait for available data, conn-id: %u",
            conn->id);
        conn->pending_have_data = true;
        gst_d3d11_ipc_server_on_idle (server);
        return;
      }
      gst_d3d11_ipc_server_have_data (server, conn);
      break;
    case GstD3D11IpcPktType::READ_DONE:
      GST_LOG_OBJECT (server, "READ-DONE, conn-id: %u", conn->id);

      if (!conn->data) {
        GST_ERROR_OBJECT (server, "Unexpected READ-DATA, conn-id: %u",
            conn->id);
        gst_d3d11_ipc_server_close_connection (server, conn);
        return;
      }

      conn->peer_handles.push_back (conn->data);
      conn->data = nullptr;
      gst_d3d11_ipc_server_wait_msg (server, conn);
      break;
    case GstD3D11IpcPktType::RELEASE_DATA:
      GST_LOG_OBJECT (server, "RELEASE-DATA, conn-id: %u", conn->id);
      if (!gst_d3d11_ipc_server_on_release_data (server, conn))
        gst_d3d11_ipc_server_close_connection (server, conn);
      else
        gst_d3d11_ipc_server_wait_msg (server, conn);
      break;
    case GstD3D11IpcPktType::FIN:
      GST_DEBUG_OBJECT (server, "FIN, conn-id %u", conn->id);
      gst_d3d11_ipc_server_close_connection (server, conn);
      break;
    default:
      GST_ERROR_OBJECT (server, "Unexpected packet, conn-id: %u", conn->id);
      gst_d3d11_ipc_server_close_connection (server, conn);
      break;
  }
}

static void WINAPI
gst_d3d11_ipc_server_payload_finish (DWORD error_code, DWORD size,
    OVERLAPPED * overlap)
{
  GstD3D11IpcServerConn *conn =
      static_cast < GstD3D11IpcServerConn * >(overlap);
  GstD3D11IpcServer *self = conn->server;

  if (error_code != ERROR_SUCCESS) {
    auto err = gst_d3d11_ipc_win32_error_to_string (error_code);
    GST_WARNING_OBJECT (self, "ReadFileEx callback failed with 0x%x (%s)",
        (guint) error_code, err.c_str ());
    gst_d3d11_ipc_server_close_connection (self, conn);
    return;
  }

  gst_d3d11_ipc_server_wait_msg_finish (self, conn);
}

static void WINAPI
gst_d3d11_ipc_server_wait_msg_header_finish (DWORD error_code, DWORD size,
    OVERLAPPED * overlap)
{
  GstD3D11IpcServerConn *conn =
      static_cast < GstD3D11IpcServerConn * >(overlap);
  GstD3D11IpcServer *self = conn->server;
  GstD3D11IpcPacketHeader header;

  if (error_code != ERROR_SUCCESS) {
    auto err = gst_d3d11_ipc_win32_error_to_string (error_code);
    GST_WARNING_OBJECT (self, "ReadFileEx callback failed with 0x%x (%s)",
        (guint) error_code, err.c_str ());
    gst_d3d11_ipc_server_close_connection (self, conn);
    return;
  }

  if (!gst_d3d11_ipc_pkt_identify (conn->client_msg, header)) {
    GST_ERROR_OBJECT (self, "Broken header");
    gst_d3d11_ipc_server_close_connection (self, conn);
    return;
  }

  if (header.payload_size == 0) {
    gst_d3d11_ipc_server_wait_msg_finish (conn->server, conn);
    return;
  }

  GST_LOG_OBJECT (self, "Reading payload");

  if (!ReadFileEx (conn->pipe, &conn->client_msg[0] +
          GST_D3D11_IPC_PKT_HEADER_SIZE, header.payload_size, conn,
          gst_d3d11_ipc_server_payload_finish)) {
    guint last_err = GetLastError ();
    auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (self, "ReadFileEx failed with 0x%x (%s)",
        last_err, err.c_str ());
    gst_d3d11_ipc_server_close_connection (self, conn);
  }
}

static void
gst_d3d11_ipc_server_wait_msg (GstD3D11IpcServer * self,
    GstD3D11IpcServerConn * conn)
{
  if (!ReadFileEx (conn->pipe, &conn->client_msg[0],
          GST_D3D11_IPC_PKT_HEADER_SIZE, conn,
          gst_d3d11_ipc_server_wait_msg_header_finish)) {
    guint last_err = GetLastError ();
    auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (self, "ReadFileEx failed with 0x%x (%s)",
        last_err, err.c_str ());
    gst_d3d11_ipc_server_close_connection (self, conn);
  }
}

static void
gst_d3d11_ipc_server_eos (GstD3D11IpcServer * self,
    GstD3D11IpcServerConn * conn)
{
  gst_d3d11_ipc_pkt_build_eos (conn->server_msg);
  conn->eos = true;
  conn->type = GstD3D11IpcPktType::EOS;

  gst_d3d11_ipc_server_send_msg (self, conn);
}

static void
gst_d3d11_ipc_server_config_data (GstD3D11IpcServer * self,
    GstD3D11IpcServerConn * conn)
{
  GstD3D11IpcServerPrivate *priv = self->priv;
  GstCaps *caps = gst_sample_get_caps (conn->data->sample);

  gst_caps_replace (&conn->caps, caps);

  gst_d3d11_ipc_pkt_build_config (conn->server_msg,
      priv->pid, priv->adapter_luid, conn->caps);
  conn->type = GstD3D11IpcPktType::CONFIG;

  GST_LOG_OBJECT (self, "Sending CONFIG, conn-id %u", conn->id);
  gst_d3d11_ipc_server_send_msg (self, conn);
}

static void
gst_d3d11_ipc_server_on_idle (GstD3D11IpcServer * self)
{
  GstD3D11IpcServerPrivate *priv = self->priv;

  GST_LOG_OBJECT (self, "idle");

  if (priv->shutdown) {
    GST_DEBUG_OBJECT (self, "We are stopping");

    if (priv->conn_map.empty ()) {
      GST_DEBUG_OBJECT (self, "All connections were closed");
      SetEvent (priv->cancellable);
      return;
    }

    std::vector < std::shared_ptr < GstD3D11IpcServerConn >> to_send_eos;
    /* *INDENT-OFF* */
    for (auto it : priv->conn_map) {
      auto conn = it.second;
      if (conn->eos || !conn->pending_have_data)
        continue;

      to_send_eos.push_back (conn);
    }

    for (auto it : to_send_eos) {
      GST_DEBUG_OBJECT (self, "Sending EOS to conn-id: %u", it->id);
      gst_d3d11_ipc_server_eos (self, it.get ());
    }

    GST_DEBUG_OBJECT (self, "Have %" G_GSIZE_FORMAT " alive connections",
        priv->conn_map.size());

    size_t num_closed = 0;
    for (auto it : priv->conn_map) {
      auto conn = it.second;
      GST_DEBUG_OBJECT (self, "conn-id %u"
          " peer handle size %" G_GSIZE_FORMAT, conn->id,
          conn->peer_handles.size ());

      /* Cannot erase conn since it's still referenced.
       * Manually close connection */
      if (conn->peer_handles.empty ()) {
        if (conn->pipe != INVALID_HANDLE_VALUE) {
          CancelIo (conn->pipe);
          DisconnectNamedPipe (conn->pipe);
          CloseHandle (conn->pipe);
          conn->pipe = INVALID_HANDLE_VALUE;
        }

        num_closed++;
      }
    }
    /* *INDENT-ON* */

    if (priv->conn_map.size () == num_closed) {
      GST_DEBUG_OBJECT (self, "All connections were closed");
      SetEvent (priv->cancellable);
    }

    return;
  }

  if (priv->conn_map.empty ()) {
    GST_LOG_OBJECT (self, "Have no connection");
    return;
  }

  std::unique_lock < std::mutex > lk (priv->lock);
  if (!priv->data)
    return;

  /* *INDENT-OFF* */
  std::vector < std::shared_ptr < GstD3D11IpcServerConn >> to_config_data;
  std::vector < std::shared_ptr < GstD3D11IpcServerConn >> to_send_have_data;
  for (auto it : priv->conn_map) {
    auto conn = it.second;
    if (!conn->configured) {
      conn->configured = true;
      conn->data = priv->data;
      to_config_data.push_back (conn);
    } else if (conn->pending_have_data && conn->seq_num <= priv->data->seq_num) {
      conn->data = priv->data;
      to_send_have_data.push_back (conn);
    }
  }
  lk.unlock ();

  for (auto it: to_config_data)
    gst_d3d11_ipc_server_config_data (self, it.get ());

  for (auto it: to_send_have_data)
    gst_d3d11_ipc_server_have_data (self, it.get ());
  /* *INDENT-ON* */
}

static void WINAPI
gst_d3d11_ipc_server_send_msg_finish (DWORD error_code, DWORD size,
    OVERLAPPED * overlap)
{
  GstD3D11IpcServerConn *conn =
      static_cast < GstD3D11IpcServerConn * >(overlap);
  GstD3D11IpcServer *self = conn->server;

  if (error_code != ERROR_SUCCESS) {
    auto err = gst_d3d11_ipc_win32_error_to_string (error_code);
    GST_WARNING_OBJECT (self, "ReadFileEx callback failed with 0x%x (%s)",
        (guint) error_code, err.c_str ());
    gst_d3d11_ipc_server_close_connection (self, conn);
    return;
  }

  GST_LOG_OBJECT (self, "Sent message");

  switch (conn->type) {
    case GstD3D11IpcPktType::CONFIG:
      GST_DEBUG_OBJECT (self, "Sent CONFIG-DATA, conn-id %u", conn->id);
      gst_d3d11_ipc_server_wait_msg (self, conn);
      break;
    case GstD3D11IpcPktType::HAVE_DATA:
      GST_LOG_OBJECT (self, "Sent HAVE-DATA, conn-id %u", conn->id);
      gst_d3d11_ipc_server_wait_msg (self, conn);
      break;
    case GstD3D11IpcPktType::EOS:
      GST_DEBUG_OBJECT (self, "Sent EOS, conn-id %u", conn->id);
      gst_d3d11_ipc_server_wait_msg (self, conn);
      break;
    default:
      GST_ERROR_OBJECT (self, "Unexpected msg type");
      gst_d3d11_ipc_server_close_connection (self, conn);
      break;
  }
}

static void
gst_d3d11_ipc_server_send_msg (GstD3D11IpcServer * self,
    GstD3D11IpcServerConn * conn)
{
  GST_LOG_OBJECT (self, "Sending message");

  if (!WriteFileEx (conn->pipe, &conn->server_msg[0],
          conn->server_msg.size (), conn,
          gst_d3d11_ipc_server_send_msg_finish)) {
    guint last_err = GetLastError ();
    auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
    GST_WARNING_OBJECT (self, "WriteFileEx failed with 0x%x (%s)",
        last_err, err.c_str ());
    gst_d3d11_ipc_server_close_connection (self, conn);
  }
}

static void
gst_d3d11_ipc_server_on_incoming_connection (GstD3D11IpcServer * self,
    std::shared_ptr < GstD3D11IpcServerConn > conn)
{
  GstD3D11IpcServerPrivate *priv = self->priv;

  priv->lock.lock ();
  conn->server = self;
  conn->id = priv->next_conn_id;
  conn->data = priv->data;
  priv->next_conn_id++;
  priv->lock.unlock ();

  GST_DEBUG_OBJECT (self, "New connection, conn-id: %u", conn->id);

  /* *INDENT-OFF* */
  priv->conn_map.insert ({conn->id, conn});
  /* *INDENT-ON* */

  if (conn->data) {
    conn->configured = true;
    gst_d3d11_ipc_server_config_data (self, conn.get ());
  } else {
    GST_DEBUG_OBJECT (self, "Have no config data yet, waiting for data");
  }
}

static gpointer
gst_d3d11_ipc_server_loop_thread_func (GstD3D11IpcServer * self)
{
  GstD3D11IpcServerPrivate *priv = self->priv;
  bool io_pending = false;
  guint wait_ret;
  HANDLE pipe;
  OVERLAPPED overlap;
  HANDLE waitables[3];

  GST_DEBUG_OBJECT (self, "Entering loop");

  memset (&overlap, 0, sizeof (OVERLAPPED));

  overlap.hEvent = CreateEvent (nullptr, TRUE, TRUE, nullptr);
  pipe = gst_cuda_ipc_server_win32_create_pipe (self, &overlap, io_pending);
  if (pipe == INVALID_HANDLE_VALUE) {
    CloseHandle (overlap.hEvent);
    priv->aborted = true;
    goto out;
  }

  waitables[0] = overlap.hEvent;
  waitables[1] = priv->wakeup_event;
  waitables[2] = priv->cancellable;

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
          auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
          GST_WARNING_OBJECT (self, "GetOverlappedResult failed with 0x%x (%s)",
              last_err, err.c_str ());
          CloseHandle (pipe);
          pipe = INVALID_HANDLE_VALUE;
          break;
        }

        auto conn = std::make_shared < GstD3D11IpcServerConn > (pipe);
        conn->server = self;
        pipe = INVALID_HANDLE_VALUE;
        gst_d3d11_ipc_server_on_incoming_connection (self, conn);

        pipe = gst_cuda_ipc_server_win32_create_pipe (self,
            &overlap, io_pending);
        break;
      }
      case WAIT_IO_COMPLETION:
        break;
      case WAIT_OBJECT_0 + 1:
        gst_d3d11_ipc_server_on_idle (self);
        break;
      default:
      {
        guint last_err = GetLastError ();
        auto err = gst_d3d11_ipc_win32_error_to_string (last_err);
        GST_ERROR_OBJECT (self,
            "WaitForMultipleObjectsEx return 0x%x, last error 0x%x (%s)",
            wait_ret, last_err, err.c_str ());
        priv->aborted = true;
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

  priv->conn_map.clear ();

  GST_DEBUG_OBJECT (self, "Exit loop thread");

  return nullptr;
}

GstFlowReturn
gst_d3d11_ipc_server_send_data (GstD3D11IpcServer * server, GstSample * sample,
    const GstD3D11IpcMemLayout & layout, HANDLE handle, GstClockTime pts)
{
  GstD3D11IpcServerPrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_IPC_SERVER (server), GST_FLOW_ERROR);
  g_return_val_if_fail (GST_IS_SAMPLE (sample), GST_FLOW_ERROR);

  priv = server->priv;

  GST_LOG_OBJECT (server, "Sending data");

  std::unique_lock < std::mutex > lk (priv->lock);
  if (priv->aborted) {
    GST_DEBUG_OBJECT (server, "Was aborted");
    return GST_FLOW_ERROR;
  }

  auto data = std::make_shared < GstD3D11IpcServerData > ();
  data->sample = gst_sample_ref (sample);
  data->handle = handle;
  data->layout = layout;
  data->pts = pts;
  data->seq_num = priv->seq_num;

  priv->seq_num++;
  priv->data = data;
  lk.unlock ();

  SetEvent (priv->wakeup_event);

  return GST_FLOW_OK;
}

void
gst_d3d11_ipc_server_stop (GstD3D11IpcServer * server)
{
  GstD3D11IpcServerPrivate *priv;

  g_return_if_fail (GST_IS_D3D11_IPC_SERVER (server));

  priv = server->priv;

  GST_DEBUG_OBJECT (server, "Stopping");
  priv->shutdown = true;
  SetEvent (priv->wakeup_event);

  g_clear_pointer (&priv->loop_thread, g_thread_join);

  GST_DEBUG_OBJECT (server, "Stopped");
}

GstD3D11IpcServer *
gst_d3d11_ipc_server_new (const std::string & address, gint64 adapter_luid)
{
  GstD3D11IpcServer *self;
  GstD3D11IpcServerPrivate *priv;

  self = (GstD3D11IpcServer *)
      g_object_new (GST_TYPE_D3D11_IPC_SERVER, nullptr);
  gst_object_ref_sink (self);

  priv = self->priv;
  priv->address = address;
  priv->adapter_luid = adapter_luid;

  priv->loop_thread = g_thread_new ("d3d11-ipc-server",
      (GThreadFunc) gst_d3d11_ipc_server_loop_thread_func, self);

  return self;
}

gint64
gst_d3d11_ipc_server_get_adapter_luid (GstD3D11IpcServer * server)
{
  g_return_val_if_fail (GST_IS_D3D11_IPC_SERVER (server), 0);

  return server->priv->adapter_luid;
}
