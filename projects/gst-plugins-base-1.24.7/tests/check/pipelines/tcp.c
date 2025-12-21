/* GStreamer
 *
 * Copyright (C) 2014 William Manley <will@williammanley.net>
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

#include <gio/gio.h>
#include <gst/check/gstcheck.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/net/gstnetcontrolmessagemeta.h>

#ifdef HAVE_GIO_UNIX_2_0
#include <gio/gunixfdmessage.h>
#endif /*  HAVE_GIO_UNIX_2_0 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>

static gboolean
g_socketpair (GSocketFamily family, GSocketType type, GSocketProtocol protocol,
    GSocket * gsv[2], GError ** error);

typedef struct
{
  GstElement *sink;
  GstElement *src;

  GstPipeline *sink_pipeline;
  GstPipeline *src_pipeline;
  GstAppSrc *sink_src;
  GstAppSink *src_sink;
} SymmetryTest;

static void
symmetry_test_setup (SymmetryTest * st, GstElement * sink, GstElement * src)
{
  GstStateChangeReturn ret;
  GstCaps *caps;
  st->sink = sink;
  g_object_set (sink, "sync", FALSE, NULL);
  st->src = src;

  st->sink_pipeline = GST_PIPELINE (gst_pipeline_new (NULL));
  st->src_pipeline = GST_PIPELINE (gst_pipeline_new (NULL));

  st->sink_src = GST_APP_SRC (gst_element_factory_make ("appsrc", NULL));
  fail_unless (st->sink_src != NULL);
  caps = gst_caps_from_string ("application/x-gst-check");
  gst_app_src_set_caps (st->sink_src, caps);
  gst_caps_unref (caps);

  gst_bin_add_many (GST_BIN (st->sink_pipeline), GST_ELEMENT (st->sink_src),
      st->sink, NULL);
  fail_unless (gst_element_link_many (GST_ELEMENT (st->sink_src), st->sink,
          NULL));

  st->src_sink = GST_APP_SINK (gst_element_factory_make ("appsink", NULL));
  fail_unless (st->src_sink != NULL);
  gst_bin_add_many (GST_BIN (st->src_pipeline), st->src,
      GST_ELEMENT (st->src_sink), NULL);
  fail_unless (gst_element_link_many (st->src, GST_ELEMENT (st->src_sink),
          NULL));

  ret =
      gst_element_set_state (GST_ELEMENT (st->sink_pipeline),
      GST_STATE_PLAYING);
  fail_if (ret == GST_STATE_CHANGE_FAILURE);
  ret =
      gst_element_set_state (GST_ELEMENT (st->src_pipeline), GST_STATE_PLAYING);
  fail_if (ret == GST_STATE_CHANGE_FAILURE);
}

static void
symmetry_test_teardown (SymmetryTest * st)
{
  fail_unless (gst_element_set_state (GST_ELEMENT (st->sink_pipeline),
          GST_STATE_NULL) != GST_STATE_CHANGE_FAILURE);
  fail_unless (gst_element_set_state (GST_ELEMENT (st->src_pipeline),
          GST_STATE_NULL) != GST_STATE_CHANGE_FAILURE);

  gst_object_unref (st->sink_pipeline);
  gst_object_unref (st->src_pipeline);

  memset (st, 0, sizeof (*st));
}

static void
symmetry_test_assert_passthrough (SymmetryTest * st, GstBuffer * in)
{
  gpointer copy;
  gsize data_size;
  GstSample *out;

  gst_buffer_extract_dup (in, 0, -1, &copy, &data_size);

  fail_unless (gst_app_src_push_buffer (st->sink_src, in) == GST_FLOW_OK);
  in = NULL;
  out = gst_app_sink_pull_sample (st->src_sink);
  fail_unless (out != NULL);

  fail_unless (gst_buffer_get_size (gst_sample_get_buffer (out)) == data_size);
  fail_unless (gst_buffer_memcmp (gst_sample_get_buffer (out), 0, copy,
          data_size) == 0);
  g_free (copy);
  gst_sample_unref (out);
}

static gboolean
g_socketpair (GSocketFamily family, GSocketType type, GSocketProtocol protocol,
    GSocket * gsv[2], GError ** error)
{
  int ret;
  int sv[2];

  ret = socketpair (family, type, protocol, sv);
  if (ret != 0) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "socketpair failed: %s",
        g_strerror (errno));
    return FALSE;
  }

  gsv[0] = g_socket_new_from_fd (sv[0], error);
  if (gsv[0] == NULL) {
    close (sv[0]);
    close (sv[1]);
    return FALSE;
  }
  gsv[1] = g_socket_new_from_fd (sv[1], error);
  if (gsv[1] == NULL) {
    g_object_unref (gsv[0]);
    gsv[0] = NULL;
    close (sv[1]);
    return FALSE;
  }
  return TRUE;
}

static void
setup_multisocketsink_and_socketsrc (SymmetryTest * st)
{
  GSocket *sockets[2] = { NULL, NULL };
  GError *err = NULL;

  st->sink = gst_check_setup_element ("multisocketsink");
  st->src = gst_check_setup_element ("socketsrc");

  fail_unless (g_socketpair (G_SOCKET_FAMILY_UNIX,
          G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, sockets, &err));

  g_object_set (st->src, "socket", sockets[0], NULL);
  g_object_unref (sockets[0]);
  sockets[0] = NULL;

  symmetry_test_setup (st, st->sink, st->src);

  g_signal_emit_by_name (st->sink, "add", sockets[1], NULL);
  g_object_unref (sockets[1]);
  sockets[1] = NULL;
}

GST_START_TEST (test_that_socketsrc_and_multisocketsink_are_symmetrical)
{
  SymmetryTest st = { 0 };
  setup_multisocketsink_and_socketsrc (&st);
  symmetry_test_assert_passthrough (&st,
      gst_buffer_new_wrapped (g_strdup ("hello"), 5));
  symmetry_test_teardown (&st);
}

GST_END_TEST;


GST_START_TEST (test_that_tcpclientsink_and_tcpserversrc_are_symmetrical)
{
  SymmetryTest st = { 0 };
  GstElement *serversrc = gst_check_setup_element ("tcpserversrc");

  gst_element_set_state (serversrc, GST_STATE_PAUSED);
  symmetry_test_setup (&st, gst_check_setup_element ("tcpclientsink"),
      serversrc);

  symmetry_test_assert_passthrough (&st,
      gst_buffer_new_wrapped (g_strdup ("hello"), 5));

  symmetry_test_teardown (&st);
}

GST_END_TEST;


GST_START_TEST (test_that_tcpserversink_and_tcpclientsrc_are_symmetrical)
{
  SymmetryTest st = { 0 };
  GstElement *serversink = gst_check_setup_element ("tcpserversink");
  guint timeout = 100;

  symmetry_test_setup (&st, serversink,
      gst_check_setup_element ("tcpclientsrc"));

  /* Wait for the client to *actually* be connected before doing the
   * test.  The socket connection from the client might very well
   * succeed, but that doesn't mean the server has accepted it yet. If
   * we don't wait for the server to have accepted the connection, we
   * would end up dropping the buffer (because no one is "connected")
   * and the receiving side would wait forever. */
  while (timeout) {
    guint handles;
    g_object_get (serversink, "num-handles", &handles, NULL);
    if (handles > 0)
      break;
    /* Wait for 10ms to see if client connected */
    g_usleep (G_USEC_PER_SEC / 100);
    timeout--;
  }

  symmetry_test_assert_passthrough (&st,
      gst_buffer_new_wrapped (g_strdup ("hello"), 5));
  symmetry_test_teardown (&st);
}

GST_END_TEST;

static void
on_connection_closed (GstElement * socketsrc, gpointer user_data)
{
  GSocket *socket = (GSocket *) user_data;

  g_object_set (socketsrc, "socket", socket, NULL);
}

GST_START_TEST (test_that_we_can_provide_new_socketsrc_sockets_during_signal)
{
  GSocket *sockets[4] = { NULL, NULL };

  GstPipeline *pipeline = NULL;
  GstAppSink *appsink = NULL;
  GstElement *socketsrc = NULL;
  GstSample *sample = NULL;

  socketsrc = gst_check_setup_element ("socketsrc");

  fail_unless (g_socketpair (G_SOCKET_FAMILY_UNIX,
          G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, &sockets[0], NULL));

  fail_unless (g_socket_send (sockets[0], "hello", 5, NULL, NULL) == 5);
  fail_unless (g_socket_shutdown (sockets[0], FALSE, TRUE, NULL));

  fail_unless (g_socketpair (G_SOCKET_FAMILY_UNIX,
          G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, &sockets[2], NULL));
  fail_unless (g_socket_send (sockets[2], "goodbye", 7, NULL, NULL) == 7);
  fail_unless (g_socket_shutdown (sockets[2], FALSE, TRUE, NULL));

  g_object_set (socketsrc, "socket", sockets[1], NULL);

  g_signal_connect (socketsrc, "connection-closed-by-peer",
      G_CALLBACK (on_connection_closed), sockets[3]);

  pipeline = (GstPipeline *) gst_pipeline_new (NULL);
  appsink = GST_APP_SINK (gst_check_setup_element ("appsink"));
  gst_bin_add_many (GST_BIN (pipeline), socketsrc, GST_ELEMENT (appsink), NULL);
  fail_unless (gst_element_link_many (socketsrc, GST_ELEMENT (appsink), NULL));

  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);

  fail_unless ((sample = gst_app_sink_pull_sample (appsink)) != NULL);
  gst_buffer_memcmp (gst_sample_get_buffer (sample), 0, "hello", 5);
  gst_sample_unref (sample);

  fail_unless ((sample = gst_app_sink_pull_sample (appsink)) != NULL);
  gst_buffer_memcmp (gst_sample_get_buffer (sample), 0, "goodbye", 7);
  gst_sample_unref (sample);

  fail_unless (NULL == gst_app_sink_pull_sample (appsink));
  fail_unless (gst_app_sink_is_eos (appsink));

  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);
  g_clear_object (&sockets[0]);
  g_clear_object (&sockets[1]);
  g_clear_object (&sockets[2]);
  g_clear_object (&sockets[3]);
  gst_object_unref (pipeline);
}

GST_END_TEST
#ifdef HAVE_GIO_UNIX_2_0
static GSocketControlMessage *
get_control_message_meta (GstBuffer * buf)
{
  GstMeta *meta;
  gpointer iter_state = NULL;

  while ((meta = gst_buffer_iterate_meta (buf, &iter_state)) != NULL) {
    if (meta->info->api == GST_NET_CONTROL_MESSAGE_META_API_TYPE)
      return ((GstNetControlMessageMeta *) meta)->message;
  }
  fail ("Expected GSocketControlMessage attached to buffer");
  return NULL;
}


GST_START_TEST (test_that_multisocketsink_and_socketsrc_preserve_meta)
{
  GstBuffer *buf;
  GSocketControlMessage *msg;
  SymmetryTest st = { 0 };
  char tmpfilename[] = "/tmp/tcp-test.XXXXXX";
  GstSample *out;
  int orig_fd, *new_fds, new_fds_len;
  struct stat orig_stat, new_stat;

  setup_multisocketsink_and_socketsrc (&st);

  orig_fd = mkstemp (tmpfilename);
  fail_unless (orig_fd > 0);
  fail_unless (unlink (tmpfilename) == 0);
  fstat (orig_fd, &orig_stat);

  msg = g_unix_fd_message_new ();
  fail_unless (g_unix_fd_message_append_fd ((GUnixFDMessage *) msg, orig_fd,
          NULL));
  close (orig_fd);
  orig_fd = -1;

  buf = gst_buffer_new_wrapped (g_strdup ("hello"), 5);
  gst_buffer_add_net_control_message_meta (buf, msg);
  g_clear_object (&msg);

  fail_unless (gst_app_src_push_buffer (st.sink_src, buf) == GST_FLOW_OK);
  buf = NULL;

  out = gst_app_sink_pull_sample (st.src_sink);
  fail_unless (out != NULL);

  fail_unless (gst_buffer_get_size (gst_sample_get_buffer (out)) == 5);
  fail_unless (gst_buffer_memcmp (gst_sample_get_buffer (out), 0, "hello",
          5) == 0);

  msg = get_control_message_meta (gst_sample_get_buffer (out));
  fail_unless (g_socket_control_message_get_msg_type (msg) == SCM_RIGHTS);
  new_fds = g_unix_fd_message_steal_fds ((GUnixFDMessage *) msg, &new_fds_len);
  fail_unless (new_fds_len == 1);

  fstat (new_fds[0], &new_stat);
  fail_unless (orig_stat.st_ino, new_stat.st_ino);

  close (new_fds[0]);
  g_free (new_fds);

  gst_sample_unref (out);

  symmetry_test_teardown (&st);
}

GST_END_TEST;
#endif /* HAVE_GIO_UNIX_2_0 */

static Suite *
socketintegrationtest_suite (void)
{
  Suite *s = suite_create ("socketintegrationtest");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain,
      test_that_socketsrc_and_multisocketsink_are_symmetrical);
  tcase_add_test (tc_chain,
      test_that_tcpclientsink_and_tcpserversrc_are_symmetrical);
  tcase_add_test (tc_chain,
      test_that_tcpserversink_and_tcpclientsrc_are_symmetrical);
  tcase_add_test (tc_chain,
      test_that_we_can_provide_new_socketsrc_sockets_during_signal);
#ifdef HAVE_GIO_UNIX_2_0
  tcase_add_test (tc_chain,
      test_that_multisocketsink_and_socketsrc_preserve_meta);
#endif /* HAVE_GIO_UNIX_2_0 */

  return s;
}

GST_CHECK_MAIN (socketintegrationtest);
