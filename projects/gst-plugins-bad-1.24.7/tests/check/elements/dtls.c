/* GStreamer unit tests for the srtp elements
 * Copyright (C) 2007 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2008 Sebastian Dröge <slomo@circular-chaos.org>
 * Copyright (C) 2016 Collabora Ltd <vincent.penquerch@collabora.co.uk>
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
# include "config.h"
#endif

#ifdef HAVE_VALGRIND
# include <valgrind/valgrind.h>
#endif

#include <gst/check/gstcheck.h>

#include <gst/check/gstharness.h>

GST_START_TEST (test_create_and_unref)
{
  GstElement *e;

  e = gst_element_factory_make ("dtlsenc", NULL);
  fail_unless (e != NULL);
  gst_element_set_state (e, GST_STATE_NULL);
  gst_object_unref (e);

  e = gst_element_factory_make ("dtlsdec", NULL);
  fail_unless (e != NULL);
  gst_element_set_state (e, GST_STATE_NULL);
  gst_object_unref (e);
}

GST_END_TEST;

static GMutex key_lock;
static GCond key_cond;
static int key_count;
static gboolean errored;

static GstBusSyncReply
bus_msg_handler (GstBus * bus, GstMessage * message, gpointer user_data)
{
  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {
    g_mutex_lock (&key_lock);
    errored = TRUE;
    g_cond_broadcast (&key_cond);
    g_mutex_unlock (&key_lock);
  }

  return GST_BUS_PASS;
}

static void
_on_key_received (GstElement * element, gpointer user_data)
{
  g_mutex_lock (&key_lock);
  key_count++;
  g_cond_broadcast (&key_cond);
  g_mutex_unlock (&key_lock);
}

static void
_wait_for_key_count_to_reach (int n)
{
  g_mutex_lock (&key_lock);
  while (key_count < n) {
    g_cond_wait (&key_cond, &key_lock);

    /* Check if any errors were posted */
    if (errored) {
      g_mutex_unlock (&key_lock);
      fail ("DTLS element posted an error");
      g_mutex_lock (&key_lock);
    }
  }
  g_mutex_unlock (&key_lock);
}

static gchar data[] = {
  0x00, 0x01, 0x02, 0x03,
};

GST_START_TEST (test_data_transfer)
{
  GstHarness *server, *client;
  GstElement *s_enc, *s_dec, *c_enc, *c_dec, *s_bin, *c_bin;
  GstPad *target, *ghost;
  GstBuffer *buffer, *buf2;
  GstBus *bus;

  /* setup a server and client for dtls negotiation */
  s_bin = gst_bin_new (NULL);
  c_bin = gst_bin_new (NULL);

  /* XXX: the element set states are needed to avoid a runtime warning:
   *
   *   'gst_dtls_connection_process: runtime check failed: (!priv->bio_buffer)'
   *
   * where the encoder needs to be started (and SSL initialized) before the
   * associated decoder receives any data and calls gst_dtls_connection_process().
   */
  s_dec = gst_element_factory_make ("dtlsdec", "server_dec");
  g_object_set (s_dec, "connection-id", "server", NULL);
  g_signal_connect (s_dec, "on-key-received", G_CALLBACK (_on_key_received),
      NULL);
  gst_element_set_state (s_dec, GST_STATE_PAUSED);
  gst_bin_add (GST_BIN (s_bin), s_dec);

  s_enc = gst_element_factory_make ("dtlsenc", "server_enc");
  g_object_set (s_enc, "connection-id", "server", NULL);
  g_signal_connect (s_enc, "on-key-received", G_CALLBACK (_on_key_received),
      NULL);
  gst_element_set_state (s_enc, GST_STATE_PAUSED);
  gst_bin_add (GST_BIN (c_bin), s_enc);

  c_dec = gst_element_factory_make ("dtlsdec", "client_dec");
  g_object_set (c_dec, "connection-id", "client", NULL);
  g_signal_connect (c_dec, "on-key-received", G_CALLBACK (_on_key_received),
      NULL);
  gst_element_set_state (c_dec, GST_STATE_PAUSED);
  gst_bin_add (GST_BIN (c_bin), c_dec);

  c_enc = gst_element_factory_make ("dtlsenc", "client_enc");
  g_object_set (c_enc, "connection-id", "client", "is-client", TRUE, NULL);
  g_signal_connect (c_enc, "on-key-received", G_CALLBACK (_on_key_received),
      NULL);
  gst_bin_add (GST_BIN (s_bin), c_enc);

  gst_element_link_pads (s_enc, "src", c_dec, "sink");
  gst_element_link_pads (c_enc, "src", s_dec, "sink");

  gst_element_set_state (c_enc, GST_STATE_PAUSED);

  target = gst_element_request_pad_simple (c_dec, "src");
  ghost = gst_ghost_pad_new ("src", target);
  gst_element_add_pad (s_bin, ghost);
  gst_object_unref (target);

  target = gst_element_request_pad_simple (s_enc, "sink");
  ghost = gst_ghost_pad_new ("sink", target);
  gst_element_add_pad (s_bin, ghost);
  gst_object_unref (target);

  target = gst_element_request_pad_simple (s_dec, "src");
  ghost = gst_ghost_pad_new ("src", target);
  gst_element_add_pad (c_bin, ghost);
  gst_object_unref (target);

  target = gst_element_request_pad_simple (c_enc, "sink");
  ghost = gst_ghost_pad_new ("sink", target);
  gst_element_add_pad (c_bin, ghost);
  gst_object_unref (target);

  server = gst_harness_new_with_element (s_bin, "sink", "src");
  client = gst_harness_new_with_element (c_bin, "sink", "src");

  gst_harness_set_src_caps_str (server, "application/data");
  gst_harness_set_src_caps_str (client, "application/data");

  bus = gst_bus_new ();
  gst_bus_set_sync_handler (bus, bus_msg_handler, NULL, NULL);

  gst_element_set_bus (s_bin, bus);
  gst_element_set_bus (c_bin, bus);

  gst_object_unref (bus);

  _wait_for_key_count_to_reach (4);

  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, data,
      G_N_ELEMENTS (data), 0, G_N_ELEMENTS (data), NULL, NULL);
  gst_harness_push (server, gst_buffer_ref (buffer));
  buf2 = gst_harness_pull (server);
  fail_unless_equals_int (0, gst_buffer_memcmp (buf2, 0, data,
          G_N_ELEMENTS (data)));
  gst_buffer_unref (buf2);

  gst_harness_play (client);
  gst_harness_push (client, gst_buffer_ref (buffer));
  buf2 = gst_harness_pull (client);
  fail_unless_equals_int (0, gst_buffer_memcmp (buf2, 0, data,
          G_N_ELEMENTS (data)));
  gst_buffer_unref (buf2);

  gst_object_unref (s_bin);
  gst_object_unref (c_bin);

  gst_buffer_unref (buffer);
  gst_harness_teardown (server);
  gst_harness_teardown (client);
}

GST_END_TEST;

static Suite *
dtls_suite (void)
{
  Suite *s = suite_create ("dtls");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_create_and_unref);
  tcase_add_test (tc_chain, test_data_transfer);

  return s;
}

GST_CHECK_MAIN (dtls);
