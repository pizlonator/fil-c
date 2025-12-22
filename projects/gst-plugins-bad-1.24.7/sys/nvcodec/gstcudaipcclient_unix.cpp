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

#include "gstcudaipcclient_unix.h"
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include <gio/gunixsocketaddress.h>
#include <gio/gunixconnection.h>

GST_DEBUG_CATEGORY_EXTERN (cuda_ipc_client_debug);
#define GST_CAT_DEFAULT cuda_ipc_client_debug

/* *INDENT-OFF* */
struct GstCudaIpcClientConnUnix : public GstCudaIpcClientConn
{
  GstCudaIpcClientConnUnix (GSocketConnection * socket_conn,
      GCancellable * cancel)
  {
    conn = socket_conn;
    cancellable = (GCancellable *) g_object_ref (cancel);
    istream = g_io_stream_get_input_stream (G_IO_STREAM (conn));
    ostream = g_io_stream_get_output_stream (G_IO_STREAM (conn));
  }

  ~GstCudaIpcClientConnUnix ()
  {
    g_cancellable_cancel (cancellable);
    g_object_unref (conn);
    g_object_unref (cancellable);
  }

  GSocketConnection *conn;
  GInputStream *istream;
  GOutputStream *ostream;
  GCancellable *cancellable;
};

struct GstCudaIpcClientUnixPrivate
{
  GstCudaIpcClientUnixPrivate ()
  {
    main_context = g_main_context_new ();
    main_loop = g_main_loop_new (main_context, FALSE);
    cancellable = g_cancellable_new ();
  }

  ~GstCudaIpcClientUnixPrivate ()
  {
    g_main_loop_unref (main_loop);
    g_main_context_unref (main_context);
  }

  std::string address;
  GstClockTime timeout;
  std::mutex lock;
  std::condition_variable cond;

  GMainLoop *main_loop;
  GMainContext *main_context;
  GCancellable *cancellable;
  bool flushing = false;
};
/* *INDENT-ON* */

struct _GstCudaIpcClientUnix
{
  GstCudaIpcClient parent;

  GstCudaIpcClientUnixPrivate *priv;
};

static void gst_cuda_ipc_client_unix_finalize (GObject * object);
static bool gst_cuda_ipc_client_unix_send_msg (GstCudaIpcClient * client,
    GstCudaIpcClientConn * conn);
static bool gst_cuda_ipc_client_unix_wait_msg (GstCudaIpcClient * client,
    GstCudaIpcClientConn * conn);
static void gst_cuda_ipc_client_unix_terminate (GstCudaIpcClient * client);
static void gst_cuda_ipc_client_unix_invoke (GstCudaIpcClient * client);
static void gst_cuda_ipc_client_unix_set_flushing (GstCudaIpcClient * client,
    bool flushing);
static void gst_cuda_ipc_client_unix_loop (GstCudaIpcClient * client);

#define gst_cuda_ipc_client_unix_parent_class parent_class
G_DEFINE_TYPE (GstCudaIpcClientUnix,
    gst_cuda_ipc_client_unix, GST_TYPE_CUDA_IPC_CLIENT);

static void
gst_cuda_ipc_client_unix_class_init (GstCudaIpcClientUnixClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstCudaIpcClientClass *client_class = GST_CUDA_IPC_CLIENT_CLASS (klass);

  object_class->finalize = gst_cuda_ipc_client_unix_finalize;

  client_class->send_msg =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_unix_send_msg);
  client_class->wait_msg =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_unix_wait_msg);
  client_class->terminate =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_unix_terminate);
  client_class->invoke = GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_unix_invoke);
  client_class->set_flushing =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_unix_set_flushing);
  client_class->loop = GST_DEBUG_FUNCPTR (gst_cuda_ipc_client_unix_loop);
}

static void
gst_cuda_ipc_client_unix_init (GstCudaIpcClientUnix * self)
{
  self->priv = new GstCudaIpcClientUnixPrivate ();
}

static void
gst_cuda_ipc_client_unix_finalize (GObject * object)
{
  GstCudaIpcClientUnix *self = GST_CUDA_IPC_CLIENT_UNIX (object);

  GST_DEBUG_OBJECT (self, "finalize");

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_cuda_ipc_client_unix_send_msg_finish (GObject * source,
    GAsyncResult * result, GstCudaIpcClientConnUnix * conn)
{
  GstCudaIpcClient *client = conn->client;
  gsize size;
  GError *err = nullptr;
  bool ret = true;

  if (!g_output_stream_write_all_finish (conn->ostream, result, &size, &err)) {
    GST_WARNING_OBJECT (client, "Write failed with %s", err->message);
    g_clear_error (&err);
    ret = false;
  }

  gst_cuda_ipc_client_send_msg_finish (client, ret);
}

static bool
gst_cuda_ipc_client_unix_send_msg (GstCudaIpcClient * client,
    GstCudaIpcClientConn * conn)
{
  GstCudaIpcClientConnUnix *unix_conn =
      static_cast < GstCudaIpcClientConnUnix * >(conn);

  g_output_stream_write_all_async (unix_conn->ostream, &conn->client_msg[0],
      conn->client_msg.size (), G_PRIORITY_DEFAULT, unix_conn->cancellable,
      (GAsyncReadyCallback) gst_cuda_ipc_client_unix_send_msg_finish,
      unix_conn);

  return true;
}

static void
gst_cuda_ipc_client_unix_finish_have_mmap_data (GstCudaIpcClient * client,
    GstCudaIpcClientConnUnix * conn)
{
  GstClockTime pts;
  GstCudaIpcMemLayout layout;
  GstCudaSharableHandle server_handle = 0;
  GstCudaSharableHandle client_handle = 0;
  GstCaps *caps = nullptr;
  GError *err = nullptr;
  std::vector < guint8 > meta;

  if (!gst_cuda_ipc_pkt_parse_have_mmap_data (conn->server_msg, pts,
          layout, &server_handle, &caps, meta)) {
    GST_ERROR_OBJECT (client, "Couldn't parse MMAP-DATA");
    goto error;
  }

  client_handle =
      g_unix_connection_receive_fd (G_UNIX_CONNECTION (conn->conn),
      conn->cancellable, &err);
  if (err) {
    GST_ERROR_OBJECT (client, "Couldn't get fd, %s", err->message);
    goto error;
  }

  gst_cuda_ipc_client_have_mmap_data (client, pts, layout, caps,
      server_handle, client_handle, meta);
  return;

error:
  gst_cuda_ipc_client_wait_msg_finish (client, false);
}

static void
gst_cuda_ipc_client_unix_payload_finish (GObject * source,
    GAsyncResult * result, GstCudaIpcClientConnUnix * conn)
{
  GstCudaIpcClient *client = conn->client;
  gsize size;
  GError *err = nullptr;
  bool ret = true;
  GstCudaIpcPacketHeader header;

  if (!g_input_stream_read_all_finish (conn->istream, result, &size, &err)) {
    GST_WARNING_OBJECT (client, "Read failed with %s", err->message);
    g_clear_error (&err);
    ret = false;
  } else if (!gst_cuda_ipc_pkt_identify (conn->server_msg, header)) {
    GST_ERROR_OBJECT (client, "Broken header");
    ret = false;
  } else if (header.type == GstCudaIpcPktType::HAVE_MMAP_DATA) {
    gst_cuda_ipc_client_unix_finish_have_mmap_data (client, conn);
    return;
  }

  gst_cuda_ipc_client_wait_msg_finish (client, ret);
}

static void
gst_cuda_ipc_client_unix_wait_finish (GObject * source,
    GAsyncResult * result, GstCudaIpcClientConnUnix * conn)
{
  GstCudaIpcClient *client = conn->client;
  gsize size;
  GError *err = nullptr;
  GstCudaIpcPacketHeader header;

  if (!g_input_stream_read_all_finish (conn->istream, result, &size, &err)) {
    GST_WARNING_OBJECT (client, "Read failed with %s", err->message);
    g_clear_error (&err);
    gst_cuda_ipc_client_wait_msg_finish (client, false);
    return;
  }

  if (!gst_cuda_ipc_pkt_identify (conn->server_msg, header)) {
    GST_ERROR_OBJECT (client, "Broken header");
    gst_cuda_ipc_client_wait_msg_finish (client, false);
    return;
  }

  if (header.payload_size == 0) {
    gst_cuda_ipc_client_wait_msg_finish (client, true);
    return;
  }

  GST_LOG_OBJECT (client, "Reading payload");

  g_input_stream_read_all_async (conn->istream,
      &conn->server_msg[0] + GST_CUDA_IPC_PKT_HEADER_SIZE, header.payload_size,
      G_PRIORITY_DEFAULT, conn->cancellable,
      (GAsyncReadyCallback) gst_cuda_ipc_client_unix_payload_finish, conn);
}

static bool
gst_cuda_ipc_client_unix_wait_msg (GstCudaIpcClient * client,
    GstCudaIpcClientConn * conn)
{
  GstCudaIpcClientConnUnix *unix_conn =
      static_cast < GstCudaIpcClientConnUnix * >(conn);

  g_input_stream_read_all_async (unix_conn->istream,
      &conn->server_msg[0], GST_CUDA_IPC_PKT_HEADER_SIZE,
      G_PRIORITY_DEFAULT, unix_conn->cancellable,
      (GAsyncReadyCallback) gst_cuda_ipc_client_unix_wait_finish, unix_conn);

  return true;
}

static void
gst_cuda_ipc_client_unix_terminate (GstCudaIpcClient * client)
{
  GstCudaIpcClientUnix *self = GST_CUDA_IPC_CLIENT_UNIX (client);
  GstCudaIpcClientUnixPrivate *priv = self->priv;

  g_main_loop_quit (priv->main_loop);
}

static gboolean
gst_cuda_ipc_client_unix_invoke_func (GstCudaIpcClient * client)
{
  gst_cuda_ipc_client_on_idle (client);

  return G_SOURCE_REMOVE;
}

static void
gst_cuda_ipc_client_unix_invoke (GstCudaIpcClient * client)
{
  GstCudaIpcClientUnix *self = GST_CUDA_IPC_CLIENT_UNIX (client);
  GstCudaIpcClientUnixPrivate *priv = self->priv;

  g_main_context_invoke (priv->main_context,
      (GSourceFunc) gst_cuda_ipc_client_unix_invoke_func, client);
}

static void
gst_cuda_ipc_client_unix_set_flushing (GstCudaIpcClient * client, bool flushing)
{
  GstCudaIpcClientUnix *self = GST_CUDA_IPC_CLIENT_UNIX (client);
  GstCudaIpcClientUnixPrivate *priv = self->priv;

  std::lock_guard < std::mutex > lk (priv->lock);
  priv->flushing = flushing;
  priv->cond.notify_all ();
}

static void
gst_cuda_ipc_client_unix_loop (GstCudaIpcClient * client)
{
  GstCudaIpcClientUnix *self = GST_CUDA_IPC_CLIENT_UNIX (client);
  GstCudaIpcClientUnixPrivate *priv = self->priv;
  GSocketConnection *socket_conn;
  GSocketClient *socket_client;
  GSocketAddress *addr;
  GError *err = nullptr;
  GstClockTime start_time = gst_util_get_timestamp ();

  g_main_context_push_thread_default (priv->main_context);

  std::unique_lock < std::mutex > lk (priv->lock);

  socket_client = g_socket_client_new ();
  addr = g_unix_socket_address_new (priv->address.c_str ());

  do {
    GstClockTime diff;

    if (priv->flushing) {
      GST_DEBUG_OBJECT (self, "We are flushing");
      gst_cuda_ipc_client_abort (client);
      return;
    }

    socket_conn = g_socket_client_connect (socket_client,
        G_SOCKET_CONNECTABLE (addr), priv->cancellable, &err);
    if (socket_conn)
      break;

    if (err->code == G_IO_ERROR_CANCELLED) {
      GST_DEBUG_OBJECT (self, "Operation cancelled");
      g_clear_error (&err);
      break;
    } else {
      GST_DEBUG_OBJECT (self, "Connection failed with error %s", err->message);
      g_clear_error (&err);
    }

    if (priv->timeout > 0) {
      diff = gst_util_get_timestamp () - start_time;
      if (diff > priv->timeout) {
        GST_WARNING_OBJECT (self, "Timeout");
        break;
      }
    }

    GST_DEBUG_OBJECT (self, "Sleep for next retry");
    priv->cond.wait_for (lk, std::chrono::milliseconds (100));
  } while (true);
  lk.unlock ();

  g_object_unref (socket_client);
  g_object_unref (addr);

  if (socket_conn) {
    GST_DEBUG_OBJECT (self, "Connection established");

    auto conn = std::make_shared < GstCudaIpcClientConnUnix > (socket_conn,
        priv->cancellable);
    gst_cuda_ipc_client_new_connection (client, conn);
  } else {
    GST_WARNING_OBJECT (self, "Connection failed");
    gst_cuda_ipc_client_abort (client);
  }

  GST_DEBUG_OBJECT (self, "Starting loop");

  g_main_loop_run (priv->main_loop);

  GST_DEBUG_OBJECT (self, "Exit loop");

  g_cancellable_cancel (priv->cancellable);

  g_main_context_pop_thread_default (priv->main_context);
}

GstCudaIpcClient *
gst_cuda_ipc_client_new (const gchar * address, GstCudaContext * context,
    GstCudaStream * stream, GstCudaIpcIOMode io_mode, guint timeout,
    guint buffer_size)
{
  GstCudaIpcClient *client;
  GstCudaIpcClientUnix *self;

  g_return_val_if_fail (address, nullptr);
  g_return_val_if_fail (GST_IS_CUDA_CONTEXT (context), nullptr);

  self = (GstCudaIpcClientUnix *)
      g_object_new (GST_TYPE_CUDA_IPC_CLIENT_UNIX, nullptr);
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
