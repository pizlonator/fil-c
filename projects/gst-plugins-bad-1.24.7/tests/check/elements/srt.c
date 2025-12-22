/* GStreamer
 * Copyright (C) 2023 Jonas Danielsson <jonas.danielsson@spiideo.com>
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
 */

/* Using GValueArray for stats */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

#include "../ext/srt/gstsrt-enums.h"

static const gchar elements[][8] = { "srtsrc", "srtsink" };

static void
check_play (const gchar * src_uri,
    GstSRTConnectionMode src_mode,
    const gchar * sink_uri, GstSRTConnectionMode sink_mode)
{
  GstHarness *h_src, *h_sink;
  GstStructure *stats;
  gint64 packets_received;
  GstBuffer *in_buf, *out_buf;
  gchar *src_launchline, *sink_launchline;
  GstElement *src_element;
  guint8 data[1316] = { 0 };

  sink_launchline = g_strdup_printf ("srtsink uri=%s", sink_uri);
  h_sink = gst_harness_new_parse (sink_launchline);
  g_free (sink_launchline);

  src_launchline = g_strdup_printf ("srtsrc name=src uri=%s", src_uri);
  h_src = gst_harness_new_parse (src_launchline);
  g_free (src_launchline);

  gst_harness_set_src_caps_str (h_sink, "video/mpegts");

  if (src_mode == GST_SRT_CONNECTION_MODE_LISTENER) {
    fail_unless (gst_element_set_state (h_src->element,
            GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
    fail_unless (gst_element_set_state (h_sink->element,
            GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
  } else {
    fail_unless (gst_element_set_state (h_sink->element,
            GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
    fail_unless (gst_element_set_state (h_src->element,
            GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
  }

  in_buf = gst_buffer_new ();
  gst_buffer_append_memory (in_buf,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
          data, sizeof (data), 0, sizeof (data), NULL, NULL));
  gst_harness_push (h_sink, in_buf);
  out_buf = gst_harness_pull (h_src);

  src_element = gst_bin_get_by_name (GST_BIN (h_src->element), "src");
  g_object_get (src_element, "stats", &stats, NULL);
  g_assert_cmpstr (gst_structure_get_name (stats), ==,
      "application/x-srt-statistics");

  if (src_mode == GST_SRT_CONNECTION_MODE_CALLER) {
    fail_unless (gst_structure_get_int64 (stats,
            "packets-received", &packets_received));
    g_assert_cmpint (packets_received, ==, 1);
  } else {
    const GValue *array_value;
    const GValue *value;
    GValueArray *callers;
    GstStructure *caller_stats;

    fail_unless ((array_value = gst_structure_get_value (stats, "callers")));
    callers = (GValueArray *) g_value_get_boxed (array_value);
    value = g_value_array_get_nth (callers, 0);
    caller_stats = GST_STRUCTURE (g_value_get_boxed (value));
    fail_unless (gst_structure_get_int64 (caller_stats,
            "packets-received", &packets_received));
    g_assert_cmpint (packets_received, ==, 1);
  }

  gst_element_set_state (h_src->element, GST_STATE_NULL);
  gst_element_set_state (h_sink->element, GST_STATE_NULL);

  gst_buffer_unref (out_buf);
  gst_structure_free (stats);
  gst_object_unref (src_element);
  gst_harness_teardown (h_src);
  gst_harness_teardown (h_sink);
}

GST_START_TEST (test_create_and_unref)
{
  GstElement *e;

  e = gst_element_factory_make (elements[__i__], NULL);
  g_assert_nonnull (e);

  gst_element_set_state (e, GST_STATE_NULL);
  gst_object_unref (e);

  e = gst_element_factory_make (elements[__i__], NULL);
  g_assert_nonnull (e);

  gst_element_set_state (e, GST_STATE_NULL);
  gst_object_unref (e);
}

GST_END_TEST;

GST_START_TEST (test_uri_to_properties)
{
  GstElement *element;
  gint latency = 0, poll_timeout = 0, mode = 0, pbkeylen = 0;
  guint localport = 0;
  gchar *streamid = NULL, *localaddress = NULL;

  element = gst_element_factory_make (elements[__i__], NULL);

  /* Sets properties to non-default values (make sure this stays in sync) */
  g_object_set (element, "uri", "srt://83.0.2.14:4847?"
      "latency=300" "&mode=listener" "&streamid=the-stream-id"
      "&pbkeylen=32" "&poll-timeout=500", NULL);

  g_object_get (element,
      "latency", &latency, "mode", &mode, "streamid", &streamid,
      "pbkeylen", &pbkeylen, "poll-timeout", &poll_timeout,
      "localport", &localport, "localaddress", &localaddress, NULL);

  /* Make sure these values are in sync with the one from the URI. */
  g_assert_cmpint (latency, ==, 300);
  g_assert_cmpint (mode, ==, 2);
  g_assert_cmpstr (streamid, ==, "the-stream-id");
  g_assert_cmpint (pbkeylen, ==, 32);
  g_assert_cmpint (poll_timeout, ==, 500);
  g_assert_cmpstr (localaddress, ==, "83.0.2.14");
  g_assert_cmpint (localport, ==, 4847);

  g_free (streamid);
  g_free (localaddress);
  gst_object_unref (element);
}

GST_END_TEST;


GST_START_TEST (test_src_caller_sink_listener)
{
  check_play ("srt://127.0.0.1:3434?mode=caller",
      GST_SRT_CONNECTION_MODE_CALLER,
      "srt://:3434?mode=listener", GST_SRT_CONNECTION_MODE_LISTENER);
}

GST_END_TEST;

GST_START_TEST (test_src_listener_sink_caller)
{
  check_play ("srt://:4242?mode=listener",
      GST_SRT_CONNECTION_MODE_LISTENER,
      "srt://127.0.0.1:4242?mode=caller", GST_SRT_CONNECTION_MODE_CALLER);
}

GST_END_TEST;

static Suite *
srt_suite (void)
{
  Suite *s = suite_create ("srt");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_loop_test (tc_chain, test_create_and_unref, 0,
      G_N_ELEMENTS (elements));
  tcase_add_loop_test (tc_chain, test_uri_to_properties, 0,
      G_N_ELEMENTS (elements));
  tcase_add_test (tc_chain, test_src_caller_sink_listener);
  tcase_add_test (tc_chain, test_src_listener_sink_caller);

  return s;
}

GST_CHECK_MAIN (srt);
