/* GStreamer unit tests for the GstRTSPConnection API (RTSP support
 * library)
 *
 * Copyright (C) 2014 Ognyan Tonchev <ognyan axis com>
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

#include <gst/check/gstcheck.h>

#include <gst/rtsp/gstrtspconnection.h>
#include <string.h>


static const gchar *get_msg =
    "GET /example/url HTTP/1.0\r\n"
    "Host: 127.0.0.1\r\n" "x-sessioncookie: 805849328\r\n\r\n";
static const gchar *post_msg =
    "POST /example/url HTTP/1.0\r\n"
    "Host: 127.0.0.1\r\n"
    "x-sessioncookie: 805849328\r\n"
    "Content-Length: 0\r\n"
    "Content-Type: application/x-rtsp-tunnelled\r\n\r\n";

static guint tunnel_get_count;
static guint tunnel_post_count;
static guint tunnel_lost_count;
static guint closed_count;
static guint message_sent_count;

typedef struct
{
  GMainLoop *loop;
  guint16 port;
  GSocketConnection *conn;
  GMutex mutex;
  GCond cond;
  gboolean started;
} ServiceData;

static gboolean
incoming_callback (GSocketService * service, GSocketConnection * connection,
    GObject * source_object, gpointer user_data)
{
  ServiceData *data = user_data;

  GST_DEBUG ("new incoming connection");
  data->conn = g_object_ref (connection);
  g_main_loop_quit (data->loop);
  return FALSE;
}

static gpointer
service_thread_func (gpointer user_data)
{
  ServiceData *data = user_data;
  GMainContext *service_context;
  GSocketService *service;

  service_context = g_main_context_new ();
  g_main_context_push_thread_default (service_context);

  data->loop = g_main_loop_new (service_context, FALSE);

  /* find available port and start service */
  service = g_socket_service_new ();
  data->port = g_socket_listener_add_any_inet_port ((GSocketListener *) service,
      NULL, NULL);
  fail_unless (data->port != 0);

  /* get notified upon new connection */
  g_signal_connect (service, "incoming", G_CALLBACK (incoming_callback), data);

  g_socket_service_start (service);

  /* service is started */
  g_mutex_lock (&data->mutex);
  data->started = TRUE;
  g_cond_signal (&data->cond);
  g_mutex_unlock (&data->mutex);

  /* our service will run in the main context of this main loop */
  g_main_loop_run (data->loop);

  g_main_context_pop_thread_default (service_context);

  g_main_loop_unref (data->loop);
  data->loop = NULL;

  return NULL;
}

static void
create_connection (GSocketConnection ** client_conn,
    GSocketConnection ** server_conn)
{
  ServiceData *data;
  GThread *service_thread;
  GSocketClient *client = g_socket_client_new ();

  data = g_new0 (ServiceData, 1);
  g_mutex_init (&data->mutex);
  g_cond_init (&data->cond);

  service_thread = g_thread_new ("service thread", service_thread_func, data);
  fail_unless (service_thread != NULL);

  /* wait for the service to start */
  g_mutex_lock (&data->mutex);
  while (!data->started) {
    g_cond_wait (&data->cond, &data->mutex);
  }
  g_mutex_unlock (&data->mutex);

  /* create the tcp link */
  *client_conn = g_socket_client_connect_to_host (client, (gchar *) "localhost",
      data->port, NULL, NULL);
  fail_unless (*client_conn != NULL);
  fail_unless (g_socket_connection_is_connected (*client_conn));

  g_thread_join (service_thread);
  *server_conn = data->conn;
  data->conn = NULL;
  fail_unless (g_socket_connection_is_connected (*server_conn));

  g_mutex_clear (&data->mutex);
  g_cond_clear (&data->cond);
  g_free (data);
  g_object_unref (client);
}

static GstRTSPStatusCode
tunnel_get (GstRTSPWatch * watch, gpointer user_data)
{
  tunnel_get_count++;
  return GST_RTSP_STS_OK;
}

static GstRTSPResult
tunnel_post (GstRTSPWatch * watch, gpointer user_data)
{
  tunnel_post_count++;
  return GST_RTSP_OK;
}

static GstRTSPResult
tunnel_lost (GstRTSPWatch * watch, gpointer user_data)
{
  tunnel_lost_count++;
  return GST_RTSP_OK;
}

static GstRTSPResult
closed (GstRTSPWatch * watch, gpointer user_data)
{
  closed_count++;
  return GST_RTSP_OK;
}

static GstRTSPResult
message_sent (GstRTSPWatch * watch, guint id, gpointer user_data)
{
  message_sent_count++;
  return GST_RTSP_OK;
}

static GstRTSPWatchFuncs watch_funcs = {
  NULL,
  message_sent,
  closed,
  NULL,
  tunnel_get,
  tunnel_post,
  NULL,
  tunnel_lost
};

/* sets up a new tunnel, then disconnects the read connection and creates it
 * again */
GST_START_TEST (test_rtspconnection_tunnel_setup)
{
  GstRTSPConnection *rtsp_conn1 = NULL;
  GstRTSPConnection *rtsp_conn2 = NULL;
  GstRTSPWatch *watch1;
  GstRTSPWatch *watch2;
  GstRTSPResult res;
  GSocketConnection *client_get = NULL;
  GSocketConnection *server_get = NULL;
  GSocketConnection *client_post = NULL;
  GSocketConnection *server_post = NULL;
  GSocket *server_sock;
  GOutputStream *ostream_get;
  GInputStream *istream_get;
  GOutputStream *ostream_post;
  gsize size = 0;
  gchar buffer[1024];

  /* create GET connection */
  create_connection (&client_get, &server_get);
  server_sock = g_socket_connection_get_socket (server_get);
  fail_unless (server_sock != NULL);

  res = gst_rtsp_connection_create_from_socket (server_sock, "127.0.0.1", 4444,
      NULL, &rtsp_conn1);
  fail_unless (res == GST_RTSP_OK);
  fail_unless (rtsp_conn1 != NULL);

  watch1 = gst_rtsp_watch_new (rtsp_conn1, &watch_funcs, NULL, NULL);
  fail_unless (watch1 != NULL);
  fail_unless (gst_rtsp_watch_attach (watch1, NULL) > 0);
  g_source_unref ((GSource *) watch1);

  ostream_get = g_io_stream_get_output_stream (G_IO_STREAM (client_get));
  fail_unless (ostream_get != NULL);

  istream_get = g_io_stream_get_input_stream (G_IO_STREAM (client_get));
  fail_unless (istream_get != NULL);

  /* initiate the tunnel by sending HTTP GET */
  fail_unless (g_output_stream_write_all (ostream_get, get_msg,
          strlen (get_msg), &size, NULL, NULL));
  fail_unless (size == strlen (get_msg));

  while (!g_main_context_iteration (NULL, TRUE));
  fail_unless (tunnel_get_count == 1);
  fail_unless (tunnel_post_count == 0);
  fail_unless (tunnel_lost_count == 0);
  fail_unless (closed_count == 0);

  /* read the HTTP GET response */
  size = g_input_stream_read (istream_get, buffer, 1024, NULL, NULL);
  fail_unless (size > 0);
  buffer[size] = 0;
  fail_unless (g_strrstr (buffer, "HTTP/1.0 200 OK") != NULL);

  /* create POST channel */
  create_connection (&client_post, &server_post);
  server_sock = g_socket_connection_get_socket (server_post);
  fail_unless (server_sock != NULL);

  res = gst_rtsp_connection_create_from_socket (server_sock, "127.0.0.1", 4444,
      NULL, &rtsp_conn2);
  fail_unless (res == GST_RTSP_OK);
  fail_unless (rtsp_conn2 != NULL);

  watch2 = gst_rtsp_watch_new (rtsp_conn2, &watch_funcs, NULL, NULL);
  fail_unless (watch2 != NULL);
  fail_unless (gst_rtsp_watch_attach (watch2, NULL) > 0);
  g_source_unref ((GSource *) watch2);

  ostream_post = g_io_stream_get_output_stream (G_IO_STREAM (client_post));
  fail_unless (ostream_post != NULL);

  /* complete the tunnel by sending HTTP POST */
  fail_unless (g_output_stream_write_all (ostream_post, post_msg,
          strlen (post_msg), &size, NULL, NULL));
  fail_unless (size == strlen (post_msg));

  while (!g_main_context_iteration (NULL, TRUE));
  fail_unless (tunnel_get_count == 1);
  fail_unless (tunnel_post_count == 1);
  fail_unless (tunnel_lost_count == 0);
  fail_unless (closed_count == 0);

  /* merge the two connections together */
  fail_unless (gst_rtsp_connection_do_tunnel (rtsp_conn1, rtsp_conn2) ==
      GST_RTSP_OK);
  gst_rtsp_watch_reset (watch1);
  g_source_destroy ((GSource *) watch2);
  gst_rtsp_connection_free (rtsp_conn2);
  rtsp_conn2 = NULL;

  /* it must be possible to reconnect the POST channel */
  g_object_unref (client_post);
  while (!g_main_context_iteration (NULL, TRUE));
  fail_unless (tunnel_get_count == 1);
  fail_unless (tunnel_post_count == 1);
  fail_unless (tunnel_lost_count == 1);
  fail_unless (closed_count == 0);
  g_object_unref (server_post);

  /* no other source should get dispatched */
  fail_if (g_main_context_iteration (NULL, FALSE));

  /* create new POST connection */
  create_connection (&client_post, &server_post);
  server_sock = g_socket_connection_get_socket (server_post);
  fail_unless (server_sock != NULL);

  res = gst_rtsp_connection_create_from_socket (server_sock, "127.0.0.1", 4444,
      NULL, &rtsp_conn2);
  fail_unless (res == GST_RTSP_OK);
  fail_unless (rtsp_conn2 != NULL);

  watch2 = gst_rtsp_watch_new (rtsp_conn2, &watch_funcs, NULL, NULL);
  fail_unless (watch2 != NULL);
  fail_unless (gst_rtsp_watch_attach (watch2, NULL) > 0);
  g_source_unref ((GSource *) watch2);

  ostream_post = g_io_stream_get_output_stream (G_IO_STREAM (client_post));
  fail_unless (ostream_post != NULL);

  /* complete the tunnel by sending HTTP POST */
  fail_unless (g_output_stream_write_all (ostream_post, post_msg,
          strlen (post_msg), &size, NULL, NULL));
  fail_unless (size == strlen (post_msg));

  while (!g_main_context_iteration (NULL, TRUE));
  fail_unless (tunnel_get_count == 1);
  fail_unless (tunnel_post_count == 2);
  fail_unless (tunnel_lost_count == 1);
  fail_unless (closed_count == 0);

  /* merge the two connections together */
  fail_unless (gst_rtsp_connection_do_tunnel (rtsp_conn1, rtsp_conn2) ==
      GST_RTSP_OK);
  gst_rtsp_watch_reset (watch1);
  g_source_destroy ((GSource *) watch2);
  gst_rtsp_connection_free (rtsp_conn2);
  rtsp_conn2 = NULL;

  /* check if rtspconnection can detect close of the get channel */
  g_object_unref (client_get);
  while (!g_main_context_iteration (NULL, TRUE));
  fail_unless (tunnel_get_count == 1);
  fail_unless (tunnel_post_count == 2);
  fail_unless (tunnel_lost_count == 1);
  fail_unless (closed_count == 1);

  fail_unless (gst_rtsp_connection_close (rtsp_conn1) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_free (rtsp_conn1) == GST_RTSP_OK);

  g_object_unref (client_post);
  g_object_unref (server_post);
  g_object_unref (server_get);
}

GST_END_TEST;

/* sets up a new tunnel, starting with the read channel,
 * then disconnects the read connection and creates it again
 * ideally this test should be merged with test_rtspconnection_tunnel_setup but
 * but it became quite messy */
GST_START_TEST (test_rtspconnection_tunnel_setup_post_first)
{
  GstRTSPConnection *rtsp_conn1 = NULL;
  GstRTSPConnection *rtsp_conn2 = NULL;
  GstRTSPWatch *watch1;
  GstRTSPWatch *watch2;
  GstRTSPResult res;
  GSocketConnection *client_get = NULL;
  GSocketConnection *server_get = NULL;
  GSocketConnection *client_post = NULL;
  GSocketConnection *server_post = NULL;
  GSocket *server_sock;
  GOutputStream *ostream_get;
  GInputStream *istream_get;
  GOutputStream *ostream_post;
  gsize size = 0;
  gchar buffer[1024];

  /* create POST channel */
  create_connection (&client_post, &server_post);
  server_sock = g_socket_connection_get_socket (server_post);
  fail_unless (server_sock != NULL);

  res = gst_rtsp_connection_create_from_socket (server_sock, "127.0.0.1", 4444,
      NULL, &rtsp_conn1);
  fail_unless (res == GST_RTSP_OK);
  fail_unless (rtsp_conn1 != NULL);

  watch1 = gst_rtsp_watch_new (rtsp_conn1, &watch_funcs, NULL, NULL);
  fail_unless (watch1 != NULL);
  fail_unless (gst_rtsp_watch_attach (watch1, NULL) > 0);
  g_source_unref ((GSource *) watch1);

  ostream_post = g_io_stream_get_output_stream (G_IO_STREAM (client_post));
  fail_unless (ostream_post != NULL);

  /* initiate the tunnel by sending HTTP POST */
  fail_unless (g_output_stream_write_all (ostream_post, post_msg,
          strlen (post_msg), &size, NULL, NULL));
  fail_unless (size == strlen (post_msg));

  while (!g_main_context_iteration (NULL, TRUE));
  fail_unless (tunnel_get_count == 0);
  fail_unless (tunnel_post_count == 1);
  fail_unless (tunnel_lost_count == 0);
  fail_unless (closed_count == 0);

  /* create GET connection */
  create_connection (&client_get, &server_get);
  server_sock = g_socket_connection_get_socket (server_get);
  fail_unless (server_sock != NULL);

  res = gst_rtsp_connection_create_from_socket (server_sock, "127.0.0.1", 4444,
      NULL, &rtsp_conn2);
  fail_unless (res == GST_RTSP_OK);
  fail_unless (rtsp_conn2 != NULL);

  watch2 = gst_rtsp_watch_new (rtsp_conn2, &watch_funcs, NULL, NULL);
  fail_unless (watch2 != NULL);
  fail_unless (gst_rtsp_watch_attach (watch2, NULL) > 0);
  g_source_unref ((GSource *) watch2);

  ostream_get = g_io_stream_get_output_stream (G_IO_STREAM (client_get));
  fail_unless (ostream_get != NULL);

  istream_get = g_io_stream_get_input_stream (G_IO_STREAM (client_get));
  fail_unless (istream_get != NULL);

  /* complete the tunnel by sending HTTP GET */
  fail_unless (g_output_stream_write_all (ostream_get, get_msg,
          strlen (get_msg), &size, NULL, NULL));
  fail_unless (size == strlen (get_msg));

  while (!g_main_context_iteration (NULL, TRUE));
  fail_unless (tunnel_get_count == 1);
  fail_unless (tunnel_post_count == 1);
  fail_unless (tunnel_lost_count == 0);
  fail_unless (closed_count == 0);

  /* read the HTTP GET response */
  size = g_input_stream_read (istream_get, buffer, 1024, NULL, NULL);
  fail_unless (size > 0);
  buffer[size] = 0;
  fail_unless (g_strrstr (buffer, "HTTP/1.0 200 OK") != NULL);

  /* merge the two connections together */
  fail_unless (gst_rtsp_connection_do_tunnel (rtsp_conn1, rtsp_conn2) ==
      GST_RTSP_OK);
  gst_rtsp_watch_reset (watch1);
  g_source_destroy ((GSource *) watch2);
  gst_rtsp_connection_free (rtsp_conn2);
  rtsp_conn2 = NULL;

  /* it must be possible to reconnect the POST channel */
  g_object_unref (client_post);
  while (!g_main_context_iteration (NULL, TRUE));
  fail_unless (tunnel_get_count == 1);
  fail_unless (tunnel_post_count == 1);
  fail_unless (tunnel_lost_count == 1);
  fail_unless (closed_count == 0);
  g_object_unref (server_post);

  /* no other source should get dispatched */
  fail_if (g_main_context_iteration (NULL, FALSE));

  /* create new POST connection */
  create_connection (&client_post, &server_post);
  server_sock = g_socket_connection_get_socket (server_post);
  fail_unless (server_sock != NULL);

  res = gst_rtsp_connection_create_from_socket (server_sock, "127.0.0.1", 4444,
      NULL, &rtsp_conn2);
  fail_unless (res == GST_RTSP_OK);
  fail_unless (rtsp_conn2 != NULL);

  watch2 = gst_rtsp_watch_new (rtsp_conn2, &watch_funcs, NULL, NULL);
  fail_unless (watch2 != NULL);
  fail_unless (gst_rtsp_watch_attach (watch2, NULL) > 0);
  g_source_unref ((GSource *) watch2);

  ostream_post = g_io_stream_get_output_stream (G_IO_STREAM (client_post));
  fail_unless (ostream_post != NULL);

  /* complete the tunnel by sending HTTP POST */
  fail_unless (g_output_stream_write_all (ostream_post, post_msg,
          strlen (post_msg), &size, NULL, NULL));
  fail_unless (size == strlen (post_msg));

  while (!g_main_context_iteration (NULL, TRUE));
  fail_unless (tunnel_get_count == 1);
  fail_unless (tunnel_post_count == 2);
  fail_unless (tunnel_lost_count == 1);
  fail_unless (closed_count == 0);

  /* merge the two connections together */
  fail_unless (gst_rtsp_connection_do_tunnel (rtsp_conn1, rtsp_conn2) ==
      GST_RTSP_OK);
  gst_rtsp_watch_reset (watch1);
  g_source_destroy ((GSource *) watch2);
  gst_rtsp_connection_free (rtsp_conn2);
  rtsp_conn2 = NULL;

  /* check if rtspconnection can detect close of the get channel */
  g_object_unref (client_get);
  while (!g_main_context_iteration (NULL, TRUE));
  fail_unless (tunnel_get_count == 1);
  fail_unless (tunnel_post_count == 2);
  fail_unless (tunnel_lost_count == 1);
  fail_unless (closed_count == 1);

  fail_unless (gst_rtsp_connection_close (rtsp_conn1) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_free (rtsp_conn1) == GST_RTSP_OK);

  g_object_unref (client_post);
  g_object_unref (server_post);
  g_object_unref (server_get);
}

GST_END_TEST;

GST_START_TEST (test_rtspconnection_send_receive)
{
  GSocketConnection *input_conn = NULL;
  GSocketConnection *output_conn = NULL;
  GSocket *input_sock;
  GSocket *output_sock;
  GstRTSPConnection *rtsp_output_conn;
  GstRTSPConnection *rtsp_input_conn;
  GstRTSPMessage *msg;
  gchar body[] = "message body";
  gchar *recv_body;
  guint recv_body_len;

  create_connection (&input_conn, &output_conn);
  input_sock = g_socket_connection_get_socket (input_conn);
  fail_unless (input_sock != NULL);
  output_sock = g_socket_connection_get_socket (output_conn);
  fail_unless (output_sock != NULL);

  fail_unless (gst_rtsp_connection_create_from_socket (input_sock, "127.0.0.1",
          4444, NULL, &rtsp_input_conn) == GST_RTSP_OK);
  fail_unless (rtsp_input_conn != NULL);

  fail_unless (gst_rtsp_connection_create_from_socket (output_sock, "127.0.0.1",
          4444, NULL, &rtsp_output_conn) == GST_RTSP_OK);
  fail_unless (rtsp_output_conn != NULL);

  /* send data message */
  fail_unless (gst_rtsp_message_new_data (&msg, 1) == GST_RTSP_OK);
  fail_unless (gst_rtsp_message_set_body (msg, (guint8 *) body,
          sizeof (body)) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_send (rtsp_output_conn, msg,
          NULL) == GST_RTSP_OK);
  fail_unless (gst_rtsp_message_free (msg) == GST_RTSP_OK);
  msg = NULL;

  /* receive data message and make sure it is correct */
  fail_unless (gst_rtsp_message_new (&msg) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_receive (rtsp_input_conn, msg, NULL) ==
      GST_RTSP_OK);
  fail_unless (gst_rtsp_message_get_type (msg) == GST_RTSP_MESSAGE_DATA);
  fail_unless (gst_rtsp_message_get_body (msg, (guint8 **) & recv_body,
          &recv_body_len) == GST_RTSP_OK);
  /* RTSPConnection adds an extra byte for the trailing '\0' */
  fail_unless_equals_int (recv_body_len, sizeof (body) + 1);
  fail_unless_equals_string (recv_body, body);
  fail_unless (gst_rtsp_message_free (msg) == GST_RTSP_OK);
  msg = NULL;

  /* send request message */
  fail_unless (gst_rtsp_message_new_request (&msg, GST_RTSP_OPTIONS,
          "example.org") == GST_RTSP_OK);
  fail_unless (gst_rtsp_message_set_body (msg, (guint8 *) body,
          sizeof (body)) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_send (rtsp_output_conn, msg,
          NULL) == GST_RTSP_OK);
  fail_unless (gst_rtsp_message_free (msg) == GST_RTSP_OK);
  msg = NULL;

  /* receive request message and make sure it is correct */
  fail_unless (gst_rtsp_message_new (&msg) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_receive (rtsp_input_conn, msg, NULL) ==
      GST_RTSP_OK);
  fail_unless (gst_rtsp_message_get_type (msg) == GST_RTSP_MESSAGE_REQUEST);
  fail_unless (gst_rtsp_message_get_body (msg, (guint8 **) & recv_body,
          &recv_body_len) == GST_RTSP_OK);
  /* RTSPConnection adds an extra byte for the trailing '\0' */
  fail_unless_equals_int (recv_body_len, sizeof (body) + 1);
  fail_unless_equals_string (recv_body, body);
  fail_unless (gst_rtsp_message_free (msg) == GST_RTSP_OK);
  msg = NULL;

  fail_unless (gst_rtsp_connection_close (rtsp_input_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_free (rtsp_input_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_close (rtsp_output_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_free (rtsp_output_conn) == GST_RTSP_OK);

  g_object_unref (input_conn);
  g_object_unref (output_conn);
}

GST_END_TEST;

GST_START_TEST (test_rtspconnection_send_receive_check_headers)
{
  GSocketConnection *input_conn = NULL;
  GSocketConnection *output_conn = NULL;
  GSocket *input_sock;
  GSocket *output_sock;
  GstRTSPConnection *rtsp_output_conn;
  GstRTSPConnection *rtsp_input_conn;
  GstRTSPMessage *msg;
  gchar *header_val;

  create_connection (&input_conn, &output_conn);
  input_sock = g_socket_connection_get_socket (input_conn);
  fail_unless (input_sock != NULL);
  output_sock = g_socket_connection_get_socket (output_conn);
  fail_unless (output_sock != NULL);

  fail_unless (gst_rtsp_connection_create_from_socket (input_sock, "127.0.0.1",
          4444, NULL, &rtsp_input_conn) == GST_RTSP_OK);
  fail_unless (rtsp_input_conn != NULL);

  fail_unless (gst_rtsp_connection_create_from_socket (output_sock, "127.0.0.1",
          4444, NULL, &rtsp_output_conn) == GST_RTSP_OK);
  fail_unless (rtsp_output_conn != NULL);

  /* send request message */
  fail_unless (gst_rtsp_message_new_request (&msg, GST_RTSP_SETUP,
          "rtsp://example.com/") == GST_RTSP_OK);
  fail_unless (gst_rtsp_message_add_header (msg, GST_RTSP_HDR_BLOCKSIZE,
          "1024") == GST_RTSP_OK);
  fail_unless (gst_rtsp_message_add_header_by_name (msg, "Custom-Header",
          "lol") == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_send (rtsp_output_conn, msg,
          NULL) == GST_RTSP_OK);
  fail_unless (gst_rtsp_message_free (msg) == GST_RTSP_OK);
  msg = NULL;

  /* receive request message and make sure it is correct */
  fail_unless (gst_rtsp_message_new (&msg) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_receive (rtsp_input_conn, msg, NULL) ==
      GST_RTSP_OK);
  fail_unless (gst_rtsp_message_get_type (msg) == GST_RTSP_MESSAGE_REQUEST);
  /* check headers */
  fail_unless (gst_rtsp_message_get_header (msg, GST_RTSP_HDR_BLOCKSIZE,
          &header_val, 0) == GST_RTSP_OK);
  fail_unless (!g_strcmp0 (header_val, "1024"));
  fail_unless (gst_rtsp_message_get_header_by_name (msg, "Custom-Header",
          &header_val, 0) == GST_RTSP_OK);
  fail_unless (!g_strcmp0 (header_val, "lol"));
  fail_unless (gst_rtsp_message_free (msg) == GST_RTSP_OK);
  msg = NULL;

  fail_unless (gst_rtsp_connection_close (rtsp_input_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_free (rtsp_input_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_close (rtsp_output_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_free (rtsp_output_conn) == GST_RTSP_OK);

  g_object_unref (input_conn);
  g_object_unref (output_conn);
}

GST_END_TEST;

GST_START_TEST (test_rtspconnection_connect)
{
  ServiceData *data;
  GThread *service_thread;
  GSocketConnection *socket_conn;
  GstRTSPConnection *rtsp_conn = NULL;
  GstRTSPUrl *url = NULL;
  gchar *path;

  data = g_new0 (ServiceData, 1);
  g_mutex_init (&data->mutex);
  g_cond_init (&data->cond);

  /* create socket service */
  service_thread = g_thread_new ("service thread", service_thread_func, data);
  fail_unless (service_thread != NULL);

  /* wait for the service to start */
  g_mutex_lock (&data->mutex);
  while (!data->started) {
    g_cond_wait (&data->cond, &data->mutex);
  }
  g_mutex_unlock (&data->mutex);

  /* connect to our service using the RTSPConnection API */
  path = g_strdup_printf ("rtsp://localhost:%d", data->port);
  fail_unless (gst_rtsp_url_parse (path, &url) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_create (url, &rtsp_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_connect (rtsp_conn, NULL) == GST_RTSP_OK);
  g_free (path);
  gst_rtsp_url_free (url);

  /* wait for the other end and check whether it is connected */
  g_thread_join (service_thread);
  socket_conn = data->conn;
  data->conn = NULL;
  fail_unless (g_socket_connection_is_connected (socket_conn));

  fail_unless (gst_rtsp_connection_close (rtsp_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_free (rtsp_conn) == GST_RTSP_OK);
  g_object_unref (socket_conn);
  g_mutex_clear (&data->mutex);
  g_cond_clear (&data->cond);
  g_free (data);
}

GST_END_TEST;

GST_START_TEST (test_rtspconnection_poll)
{
  GSocketConnection *conn1 = NULL;
  GSocketConnection *conn2 = NULL;
  GSocket *sock;
  GstRTSPConnection *rtsp_conn;
  GstRTSPEvent event;
  GOutputStream *ostream;
  gsize size;
  gint64 timeout;

  create_connection (&conn1, &conn2);
  sock = g_socket_connection_get_socket (conn1);
  fail_unless (sock != NULL);

  ostream = g_io_stream_get_output_stream (G_IO_STREAM (conn2));
  fail_unless (ostream != NULL);

  fail_unless (gst_rtsp_connection_create_from_socket (sock, "127.0.0.1",
          4444, NULL, &rtsp_conn) == GST_RTSP_OK);
  fail_unless (rtsp_conn != NULL);

  /* should be possible to write on socket */
  fail_unless (gst_rtsp_connection_poll (rtsp_conn, GST_RTSP_EV_WRITE, &event,
          NULL) == GST_RTSP_OK);
  fail_unless (event & GST_RTSP_EV_WRITE);

  /* but not read, add timeout so that we don't block forever */
  timeout = G_USEC_PER_SEC;
  fail_unless (gst_rtsp_connection_poll_usec (rtsp_conn, GST_RTSP_EV_READ,
          &event, timeout) == GST_RTSP_ETIMEOUT);
  fail_if (event & GST_RTSP_EV_READ);

  /* write on the other end and make sure socket can be read */
  fail_unless (g_output_stream_write_all (ostream, "data", 5, &size, NULL,
          NULL));
  fail_unless (gst_rtsp_connection_poll (rtsp_conn, GST_RTSP_EV_READ, &event,
          NULL) == GST_RTSP_OK);
  fail_unless (event & GST_RTSP_EV_READ);

  fail_unless (gst_rtsp_connection_close (rtsp_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_free (rtsp_conn) == GST_RTSP_OK);
  g_object_unref (conn1);
  g_object_unref (conn2);
}

GST_END_TEST;

GST_START_TEST (test_rtspconnection_backlog)
{
  GSocketConnection *conn1 = NULL;
  GSocketConnection *conn2 = NULL;
  GSocket *sock;
  GstRTSPConnection *rtsp_conn = NULL;
  GstRTSPWatch *watch;
  GInputStream *istream;
  guint8 *buffer;
  guint8 recv[1024];
  gsize count;
  GstRTSPResult res = GST_RTSP_OK;
  guint num_queued;
  guint num_sent;

  create_connection (&conn1, &conn2);
  sock = g_socket_connection_get_socket (conn1);
  fail_unless (sock != NULL);

  fail_unless (gst_rtsp_connection_create_from_socket (sock, "127.0.0.1",
          4444, NULL, &rtsp_conn) == GST_RTSP_OK);
  fail_unless (rtsp_conn != NULL);

  watch = gst_rtsp_watch_new (rtsp_conn, &watch_funcs, NULL, NULL);
  fail_unless (watch != NULL);
  fail_unless (gst_rtsp_watch_attach (watch, NULL) > 0);
  g_source_unref ((GSource *) watch);

  gst_rtsp_watch_set_send_backlog (watch, 1024, 0);

  /* write until we fill tcp window and writes result in would_block,
   * data will then start getting queued until the backlog also gets full */
  num_queued = 0;
  num_sent = 0;
  while (res == GST_RTSP_OK) {
    guint id = 0;
    buffer = malloc (1024);
    memset (buffer, 0, 1024);
    res = gst_rtsp_watch_write_data (watch, buffer, 1024, &id);
    if (id > 0)
      num_queued++;
    if (res == GST_RTSP_OK)
      num_sent++;
  }

  /* make sure we got enomem and at least 1 message got queued */
  fail_unless (res == GST_RTSP_ENOMEM);
  fail_unless (num_queued > 0);

  istream = g_io_stream_get_input_stream (G_IO_STREAM (conn2));
  fail_unless (istream != NULL);

  /* read a bit from the socket and make sure queued data gets sent */
  while (num_queued > 0) {
    fail_unless (g_input_stream_read_all (istream, recv, 1024, &count, NULL,
            NULL));
    num_sent--;

    g_main_context_iteration (NULL, FALSE);
    num_queued -= message_sent_count;
    fail_unless (num_queued >= 0);
  }

  /* make sure we can read the rest of the data */
  while (num_sent > 0) {
    fail_unless (g_input_stream_read_all (istream, recv, 1024, &count, NULL,
            NULL));
    num_sent--;
  }

  g_source_destroy ((GSource *) watch);
  fail_unless (gst_rtsp_connection_close (rtsp_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_free (rtsp_conn) == GST_RTSP_OK);
  g_object_unref (conn1);
  g_object_unref (conn2);
}

GST_END_TEST;

GST_START_TEST (test_rtspconnection_ip)
{
  GstRTSPConnection *conn = NULL;
  GstRTSPUrl *url = NULL;

  fail_unless (gst_rtsp_url_parse ("rtsp://127.0.0.1:42", &url) == GST_RTSP_OK);
  fail_unless (url != NULL);
  fail_unless (gst_rtsp_connection_create (url, &conn) == GST_RTSP_OK);
  fail_unless (conn != NULL);

  gst_rtsp_connection_set_ip (conn, "127.0.0.1");
  fail_unless_equals_string (gst_rtsp_connection_get_ip (conn), "127.0.0.1");

  gst_rtsp_url_free (url);
  fail_unless (gst_rtsp_connection_free (conn) == GST_RTSP_OK);
}

GST_END_TEST;

GST_START_TEST (test_rtspconnection_send_receive_content_length)
{
  GSocketConnection *input_conn = NULL;
  GSocketConnection *output_conn = NULL;
  GSocket *input_sock;
  GSocket *output_sock;
  GstRTSPConnection *rtsp_output_conn;
  GstRTSPConnection *rtsp_input_conn;
  GstRTSPMessage *msg;

  create_connection (&input_conn, &output_conn);
  input_sock = g_socket_connection_get_socket (input_conn);
  fail_unless (input_sock != NULL);
  output_sock = g_socket_connection_get_socket (output_conn);
  fail_unless (output_sock != NULL);

  fail_unless (gst_rtsp_connection_create_from_socket (input_sock, "127.0.0.1",
          4444, NULL, &rtsp_input_conn) == GST_RTSP_OK);
  fail_unless (rtsp_input_conn != NULL);

  fail_unless (gst_rtsp_connection_create_from_socket (output_sock, "127.0.0.1",
          4444, NULL, &rtsp_output_conn) == GST_RTSP_OK);
  fail_unless (rtsp_output_conn != NULL);

  /* send request message with to big payload */
  fail_unless (gst_rtsp_message_new_request (&msg, GST_RTSP_SETUP,
          "rtsp://example.com/") == GST_RTSP_OK);
  fail_unless (gst_rtsp_message_add_header (msg, GST_RTSP_HDR_CONTENT_LENGTH,
          "2000") == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_send (rtsp_output_conn, msg,
          NULL) == GST_RTSP_OK);
  fail_unless (gst_rtsp_message_free (msg) == GST_RTSP_OK);
  msg = NULL;

  /* receive request message, expect ENOMEM */
  gst_rtsp_connection_set_content_length_limit (rtsp_input_conn, 1000);
  fail_unless (gst_rtsp_message_new (&msg) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_receive (rtsp_input_conn, msg, NULL) ==
      GST_RTSP_ENOMEM);
  fail_unless (gst_rtsp_message_free (msg) == GST_RTSP_OK);
  msg = NULL;

  /* send request message with negative payload */
  fail_unless (gst_rtsp_message_new_request (&msg, GST_RTSP_SETUP,
          "rtsp://example.com/") == GST_RTSP_OK);
  fail_unless (gst_rtsp_message_add_header (msg, GST_RTSP_HDR_CONTENT_LENGTH,
          "-2000") == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_send (rtsp_output_conn, msg,
          NULL) == GST_RTSP_OK);
  fail_unless (gst_rtsp_message_free (msg) == GST_RTSP_OK);
  msg = NULL;

  /* receive request message, expect EPARSE */
  fail_unless (gst_rtsp_message_new (&msg) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_receive (rtsp_input_conn, msg, NULL) ==
      GST_RTSP_EPARSE);
  fail_unless (gst_rtsp_message_free (msg) == GST_RTSP_OK);
  msg = NULL;

  fail_unless (gst_rtsp_connection_close (rtsp_input_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_free (rtsp_input_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_close (rtsp_output_conn) == GST_RTSP_OK);
  fail_unless (gst_rtsp_connection_free (rtsp_output_conn) == GST_RTSP_OK);

  g_object_unref (input_conn);
  g_object_unref (output_conn);
}

GST_END_TEST;

static Suite *
rtspconnection_suite (void)
{
  Suite *s = suite_create ("rtsp support library(rtspconnection)");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_rtspconnection_tunnel_setup);
  tcase_add_test (tc_chain, test_rtspconnection_tunnel_setup_post_first);
  tcase_add_test (tc_chain, test_rtspconnection_send_receive);
  tcase_add_test (tc_chain, test_rtspconnection_send_receive_check_headers);
  tcase_add_test (tc_chain, test_rtspconnection_connect);
  tcase_add_test (tc_chain, test_rtspconnection_poll);
  tcase_add_test (tc_chain, test_rtspconnection_backlog);
  tcase_add_test (tc_chain, test_rtspconnection_ip);
  tcase_add_test (tc_chain, test_rtspconnection_send_receive_content_length);

  return s;
}

GST_CHECK_MAIN (rtspconnection);
