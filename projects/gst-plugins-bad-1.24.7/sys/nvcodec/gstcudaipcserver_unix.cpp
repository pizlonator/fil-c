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

#include "gstcudaipcserver_unix.h"
#include <string.h>
#include <string>

#include <gio/gunixsocketaddress.h>
#include <gio/gunixconnection.h>
#include <glib/gstdio.h>

GST_DEBUG_CATEGORY_EXTERN (cuda_ipc_server_debug);
#define GST_CAT_DEFAULT cuda_ipc_server_debug

/* *INDENT-OFF* */
struct GstCudaIpcServerConnUnix : public GstCudaIpcServerConn
{
  GstCudaIpcServerConnUnix (GSocketConnection * conn)
  {
    socket_conn = (GSocketConnection *) g_object_ref (conn);
    istream = g_io_stream_get_input_stream (G_IO_STREAM (socket_conn));
    ostream = g_io_stream_get_output_stream (G_IO_STREAM (socket_conn));
  }

  ~GstCudaIpcServerConnUnix ()
  {
    g_clear_object (&socket_conn);
  }

  void CloseConn ()
  {
    if (socket_conn)
      g_io_stream_close (G_IO_STREAM (socket_conn), nullptr, nullptr);
  }

  /* Holds ref */
  GSocketConnection *socket_conn = nullptr;

  /* Owned by socket_conn */
  GInputStream *istream;
  GOutputStream *ostream;
};
struct GstCudaIpcServerUnixPrivate
{
  GstCudaIpcServerUnixPrivate ()
  {
    main_context = g_main_context_new ();
    main_loop = g_main_loop_new (main_context, FALSE);
    cancellable = g_cancellable_new ();
  }

  ~GstCudaIpcServerUnixPrivate ()
  {
    g_main_loop_unref (main_loop);
    g_main_context_unref (main_context);
    g_object_unref (cancellable);
  }

  std::string address;
  GMainLoop *main_loop;
  GMainContext *main_context;
  GCancellable *cancellable;
};
/* *INDENT-ON* */

struct _GstCudaIpcServerUnix
{
  GstCudaIpcServer parent;

  GstCudaIpcServerUnixPrivate *priv;
};

static void gst_cuda_ipc_server_unix_finalize (GObject * object);
static void gst_cuda_ipc_server_unix_loop (GstCudaIpcServer * server);
static void gst_cuda_ipc_server_unix_terminate (GstCudaIpcServer * server);
static void gst_cuda_ipc_server_unix_invoke (GstCudaIpcServer * server);
static bool gst_cuda_ipc_server_unix_wait_msg (GstCudaIpcServer * server,
    GstCudaIpcServerConn * conn);
static bool gst_cuda_ipc_server_unix_send_msg (GstCudaIpcServer * server,
    GstCudaIpcServerConn * conn);
static bool gst_cuda_ipc_server_unix_send_mmap_msg (GstCudaIpcServer * server,
    GstCudaIpcServerConn * conn, GstCudaSharableHandle handle);

#define gst_cuda_ipc_server_unix_parent_class parent_class
G_DEFINE_TYPE (GstCudaIpcServerUnix,
    gst_cuda_ipc_server_unix, GST_TYPE_CUDA_IPC_SERVER);

static void
gst_cuda_ipc_server_unix_class_init (GstCudaIpcServerUnixClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstCudaIpcServerClass *server_class = GST_CUDA_IPC_SERVER_CLASS (klass);

  object_class->finalize = gst_cuda_ipc_server_unix_finalize;

  server_class->loop = GST_DEBUG_FUNCPTR (gst_cuda_ipc_server_unix_loop);
  server_class->terminate =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_server_unix_terminate);
  server_class->invoke = GST_DEBUG_FUNCPTR (gst_cuda_ipc_server_unix_invoke);
  server_class->wait_msg =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_server_unix_wait_msg);
  server_class->send_msg =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_server_unix_send_msg);
  server_class->send_mmap_msg =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_server_unix_send_mmap_msg);
}

static void
gst_cuda_ipc_server_unix_init (GstCudaIpcServerUnix * self)
{
  self->priv = new GstCudaIpcServerUnixPrivate ();
}

static void
gst_cuda_ipc_server_unix_finalize (GObject * object)
{
  GstCudaIpcServerUnix *self = GST_CUDA_IPC_SERVER_UNIX (object);

  GST_DEBUG_OBJECT (self, "finalize");

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_cuda_ipc_server_unix_terminate (GstCudaIpcServer * server)
{
  GstCudaIpcServerUnix *self = GST_CUDA_IPC_SERVER_UNIX (server);
  GstCudaIpcServerUnixPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "terminate");

  g_main_loop_quit (priv->main_loop);
}

static gboolean
gst_cuda_ipc_server_invoke_func (GstCudaIpcServer * server)
{
  gst_cuda_ipc_server_on_idle (server);

  return G_SOURCE_REMOVE;
}

static void
gst_cuda_ipc_server_unix_invoke (GstCudaIpcServer * server)
{
  GstCudaIpcServerUnix *self = GST_CUDA_IPC_SERVER_UNIX (server);
  GstCudaIpcServerUnixPrivate *priv = self->priv;

  g_main_context_invoke (priv->main_context,
      (GSourceFunc) gst_cuda_ipc_server_invoke_func, server);
}

static void
gst_cuda_ipc_server_unix_payload_finish (GObject * source,
    GAsyncResult * result, GstCudaIpcServerConnUnix * conn)
{
  GstCudaIpcServer *server = conn->server;
  gsize size;
  GError *err = nullptr;
  bool ret = true;

  if (!g_input_stream_read_all_finish (conn->istream, result, &size, &err)) {
    GST_WARNING_OBJECT (server, "Read failed with %s, conn-id: %u",
        err->message, conn->id);
    g_clear_error (&err);
    ret = false;
  }

  gst_cuda_ipc_server_wait_msg_finish (server, conn, ret);
}

static void
gst_cuda_ipc_server_unix_wait_msg_finish (GObject * source,
    GAsyncResult * result, GstCudaIpcServerConnUnix * conn)
{
  GstCudaIpcServerUnix *self = GST_CUDA_IPC_SERVER_UNIX (conn->server);
  GstCudaIpcServerUnixPrivate *priv = self->priv;
  gsize size;
  GError *err = nullptr;
  GstCudaIpcPacketHeader header;

  if (!g_input_stream_read_all_finish (conn->istream, result, &size, &err)) {
    GST_WARNING_OBJECT (self, "Read failed with %s, conn-id: %u",
        err->message, conn->id);
    g_clear_error (&err);
    gst_cuda_ipc_server_wait_msg_finish (conn->server, conn, false);
    return;
  }

  if (!gst_cuda_ipc_pkt_identify (conn->client_msg, header)) {
    GST_ERROR_OBJECT (self, "Broken header, conn-id: %u", conn->id);
    gst_cuda_ipc_server_wait_msg_finish (conn->server, conn, false);
    return;
  }

  if (header.payload_size == 0) {
    gst_cuda_ipc_server_wait_msg_finish (conn->server, conn, true);
    return;
  }

  GST_LOG_OBJECT (self, "Reading payload");

  g_input_stream_read_all_async (conn->istream, &conn->client_msg[0] +
      GST_CUDA_IPC_PKT_HEADER_SIZE, header.payload_size, G_PRIORITY_DEFAULT,
      priv->cancellable,
      (GAsyncReadyCallback) gst_cuda_ipc_server_unix_payload_finish, conn);
}

static bool
gst_cuda_ipc_server_unix_wait_msg (GstCudaIpcServer * server,
    GstCudaIpcServerConn * conn)
{
  GstCudaIpcServerUnix *self = GST_CUDA_IPC_SERVER_UNIX (conn->server);
  GstCudaIpcServerUnixPrivate *priv = self->priv;
  GstCudaIpcServerConnUnix *unix_conn =
      static_cast < GstCudaIpcServerConnUnix * >(conn);

  GST_LOG_OBJECT (self, "Waiting for client message");

  g_input_stream_read_all_async (unix_conn->istream, &conn->client_msg[0],
      GST_CUDA_IPC_PKT_HEADER_SIZE, G_PRIORITY_DEFAULT, priv->cancellable,
      (GAsyncReadyCallback) gst_cuda_ipc_server_unix_wait_msg_finish,
      unix_conn);

  return true;
}

static void
gst_cuda_ipc_server_unix_send_msg_finish (GObject * source,
    GAsyncResult * result, GstCudaIpcServerConnUnix * conn)
{
  GstCudaIpcServerUnix *self = GST_CUDA_IPC_SERVER_UNIX (conn->server);
  gsize size;
  GError *err = nullptr;

  if (!g_output_stream_write_all_finish (conn->ostream, result, &size, &err)) {
    GST_WARNING_OBJECT (self, "Write failed with %s, conn-id: %u",
        err->message, conn->id);
    g_clear_error (&err);
    gst_cuda_ipc_server_send_msg_finish (conn->server, conn, false);
    return;
  }

  GST_LOG_OBJECT (self, "Sent message");

  gst_cuda_ipc_server_send_msg_finish (conn->server, conn, true);
}

static bool
gst_cuda_ipc_server_unix_send_msg (GstCudaIpcServer * server,
    GstCudaIpcServerConn * conn)
{
  GstCudaIpcServerUnix *self = GST_CUDA_IPC_SERVER_UNIX (conn->server);
  GstCudaIpcServerUnixPrivate *priv = self->priv;
  GstCudaIpcServerConnUnix *unix_conn =
      static_cast < GstCudaIpcServerConnUnix * >(conn);

  GST_LOG_OBJECT (self, "Sending message");

  g_output_stream_write_all_async (unix_conn->ostream, &conn->server_msg[0],
      conn->server_msg.size (), G_PRIORITY_DEFAULT, priv->cancellable,
      (GAsyncReadyCallback) gst_cuda_ipc_server_unix_send_msg_finish,
      unix_conn);

  return true;
}

static bool
gst_cuda_ipc_server_unix_send_mmap_msg (GstCudaIpcServer * server,
    GstCudaIpcServerConn * conn, GstCudaSharableHandle handle)
{
  GstCudaIpcServerUnix *self = GST_CUDA_IPC_SERVER_UNIX (conn->server);
  GstCudaIpcServerUnixPrivate *priv = self->priv;
  GstCudaIpcServerConnUnix *unix_conn =
      static_cast < GstCudaIpcServerConnUnix * >(conn);
  GError *err = nullptr;

  GST_LOG_OBJECT (self, "Sending mmap message");

  if (!g_output_stream_write_all (unix_conn->ostream, &conn->server_msg[0],
          conn->server_msg.size (), nullptr, priv->cancellable, &err)) {
    GST_WARNING_OBJECT (self, "Couldn't write mmap data, %s", err->message);
    g_clear_error (&err);
    return false;
  }

  if (!g_unix_connection_send_fd (G_UNIX_CONNECTION (unix_conn->socket_conn),
          handle, priv->cancellable, &err)) {
    GST_WARNING ("Couldn't send fd, %s", err->message);
    g_clear_error (&err);
    return false;
  }

  gst_cuda_ipc_server_send_msg_finish (server, conn, true);

  return true;
}

static gboolean
gst_cuda_ipc_server_unix_on_incoming (GSocketService * service,
    GSocketConnection * socket_conn, GObject * source_obj,
    GstCudaIpcServer * self)
{
  GST_DEBUG_OBJECT (self, "Got new connection");

  auto conn = std::make_shared < GstCudaIpcServerConnUnix > (socket_conn);
  gst_cuda_ipc_server_on_incoming_connection (self, conn);

  return TRUE;
}

static void
gst_cuda_ipc_server_unix_loop (GstCudaIpcServer * server)
{
  GstCudaIpcServerUnix *self = GST_CUDA_IPC_SERVER_UNIX (server);
  GstCudaIpcServerUnixPrivate *priv = self->priv;
  GError *err = nullptr;
  GSocketAddress *addr;
  GSocketService *service;
  gboolean ret;

  g_main_context_push_thread_default (priv->main_context);

  service = g_socket_service_new ();
  addr = g_unix_socket_address_new (priv->address.c_str ());

  GST_DEBUG_OBJECT (self, "Creating service with address \"%s\"",
      priv->address.c_str ());

  ret = g_socket_listener_add_address (G_SOCKET_LISTENER (service),
      addr, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, nullptr,
      nullptr, &err);
  g_object_unref (addr);

  if (!ret) {
    GST_ERROR_OBJECT (self, "Setup failed, error: %s", err->message);
    g_clear_error (&err);
    g_clear_object (&service);
    gst_cuda_ipc_server_abort (server);
  } else {
    g_signal_connect (service,
        "incoming", G_CALLBACK (gst_cuda_ipc_server_unix_on_incoming), self);
    g_socket_service_start (service);
  }

  GST_DEBUG_OBJECT (self, "Starting loop");

  g_main_loop_run (priv->main_loop);

  GST_DEBUG_OBJECT (self, "Loop stopped");

  if (service) {
    g_cancellable_cancel (priv->cancellable);
    g_unlink (priv->address.c_str ());
    g_clear_object (&service);
  }

  g_main_context_pop_thread_default (priv->main_context);
}

GstCudaIpcServer *
gst_cuda_ipc_server_new (const gchar * address, GstCudaContext * context,
    GstCudaIpcMode ipc_mode)
{
  GstCudaIpcServerUnix *self;
  GstCudaIpcServer *server;

  g_return_val_if_fail (address, nullptr);
  g_return_val_if_fail (GST_IS_CUDA_CONTEXT (context), nullptr);

  self = (GstCudaIpcServerUnix *)
      g_object_new (GST_TYPE_CUDA_IPC_SERVER_UNIX, nullptr);
  gst_object_ref_sink (self);

  self->priv->address = address;
  server = GST_CUDA_IPC_SERVER (self);
  server->context = (GstCudaContext *) gst_object_ref (context);
  server->ipc_mode = ipc_mode;
  server->pid = getpid ();

  gst_cuda_ipc_server_run (server);

  return server;
}
