/* GStreamer
 *
 * unit test for shm elements
 * Copyright (C) 2013 Collabora Ltd
 *   @author: Olivier Crete <olivier.crete@collabora.com>
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

#include <gst/gst.h>
#include <gst/check/gstcheck.h>


static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GstElement *src, *sink;
GstPad *sinkpad, *srcpad;

static void
setup_shm (void)
{
  gchar *socket_path = NULL;

  sink = gst_check_setup_element ("shmsink");
  src = gst_check_setup_element ("shmsrc");

  srcpad = gst_check_setup_src_pad (sink, &src_template);
  sinkpad = gst_check_setup_sink_pad (src, &sink_template);

  g_object_set (sink, "socket-path", "shm-unit-test", NULL);

  fail_unless (gst_element_set_state (sink, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_ASYNC);

  g_object_get (sink, "socket-path", &socket_path, NULL);
  fail_unless (socket_path != NULL);
  g_object_set (src, "socket-path", socket_path, NULL);
  g_free (socket_path);

  gst_pad_set_active (srcpad, TRUE);
  gst_pad_set_active (sinkpad, TRUE);

  fail_unless (gst_element_set_state (src, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS);
}

static void
teardown_shm (void)
{
  fail_unless (gst_element_set_state (src, GST_STATE_NULL) ==
      GST_STATE_CHANGE_SUCCESS);
  gst_check_teardown_sink_pad (src);
  gst_check_teardown_src_pad (sink);
  gst_check_teardown_element (src);
  gst_check_teardown_element (sink);
}

GST_START_TEST (test_shm_sysmem_alloc)
{
  GstBuffer *buf;
  GstState state, pending;
  GstSegment segment;

  gst_pad_push_event (srcpad, gst_event_new_stream_start ("test"));
  gst_segment_init (&segment, GST_FORMAT_BYTES);
  gst_pad_push_event (srcpad, gst_event_new_segment (&segment));

  buf = gst_buffer_new_allocate (NULL, 1000, NULL);

  fail_unless (gst_pad_push (srcpad, buf) == GST_FLOW_OK);

  fail_unless (gst_element_get_state (sink, &state, &pending,
          GST_CLOCK_TIME_NONE) == GST_STATE_CHANGE_SUCCESS);
  fail_unless (state == GST_STATE_PLAYING);
  fail_unless (pending == GST_STATE_VOID_PENDING);

  g_mutex_lock (&check_mutex);
  while (buffers == NULL)
    g_cond_wait (&check_cond, &check_mutex);
  g_mutex_unlock (&check_mutex);
  fail_unless (g_list_length (buffers) == 1);

  buf = buffers->data;
  fail_unless (gst_buffer_get_size (buf) == 1000);

  gst_check_drop_buffers ();
  teardown_shm ();
}

GST_END_TEST;


GST_START_TEST (test_shm_alloc)
{
  GstBuffer *buf;
  GstQuery *query;
  GstCaps *caps = gst_caps_new_empty_simple ("application/x-test");
  GstAllocator *alloc;
  GstAllocationParams params;
  guint size;
  GstSegment segment;

  gst_pad_push_event (srcpad, gst_event_new_stream_start ("test"));
  gst_pad_push_event (srcpad, gst_event_new_caps (caps));
  gst_segment_init (&segment, GST_FORMAT_BYTES);
  gst_pad_push_event (srcpad, gst_event_new_segment (&segment));

  query = gst_query_new_allocation (caps, FALSE);
  gst_caps_unref (caps);

  fail_unless (gst_pad_peer_query (srcpad, query));

  fail_unless (gst_query_get_n_allocation_params (query) == 1);

  gst_query_parse_nth_allocation_param (query, 0, &alloc, &params);
  fail_unless (alloc != NULL);
  gst_query_unref (query);

  g_object_get (sink, "shm-size", &size, NULL);

  size -= params.align | gst_memory_alignment;

  /* alloc buffer of max size, this way, it will block forever it a copy
   * is made inside shmsink*/
  buf = gst_buffer_new_allocate (alloc, size, &params);

  gst_object_unref (alloc);

  fail_unless (gst_pad_push (srcpad, buf) == GST_FLOW_OK);


  g_mutex_lock (&check_mutex);
  while (buffers == NULL)
    g_cond_wait (&check_cond, &check_mutex);
  g_mutex_unlock (&check_mutex);
  fail_unless (g_list_length (buffers) == 1);

  buf = buffers->data;
  fail_unless (gst_buffer_get_size (buf) == size);

  gst_check_drop_buffers ();
  teardown_shm ();
}

GST_END_TEST;

GST_START_TEST (test_shm_live)
{
  GstElement *producer, *consumer;
  GstElement *src, *sink;
  gchar *socket_path = NULL;
  GstStateChangeReturn state_res;
  GstSample *sample = NULL;

  src = gst_element_factory_make ("fakesrc", NULL);
  g_object_set (src, "sizetype", 2, NULL);

  sink = gst_element_factory_make ("shmsink", NULL);
  g_object_set (sink, "socket-path", "shm-unit-test", "wait-for-connection",
      FALSE, NULL);

  producer = gst_pipeline_new ("producer-pipeline");
  gst_bin_add_many (GST_BIN (producer), src, sink, NULL);
  fail_unless (gst_element_link (src, sink));

  state_res = gst_element_set_state (producer, GST_STATE_PLAYING);
  fail_unless (state_res != GST_STATE_CHANGE_FAILURE);

  g_object_get (sink, "socket-path", &socket_path, NULL);
  fail_unless (socket_path != NULL);

  src = gst_element_factory_make ("shmsrc", NULL);
  sink = gst_element_factory_make ("appsink", NULL);
  g_object_set (src, "is-live", TRUE, NULL);
  g_object_set (sink, "async", FALSE, "enable-last-sample", FALSE, NULL);

  consumer = gst_pipeline_new ("consumer-pipeline");
  gst_bin_add_many (GST_BIN (consumer), src, sink, NULL);
  fail_unless (gst_element_link (src, sink));

  g_object_set (src, "socket-path", socket_path, NULL);

  state_res = gst_element_set_state (consumer, GST_STATE_PLAYING);
  fail_unless (state_res != GST_STATE_CHANGE_FAILURE);

  /* wait for preroll */
  state_res = gst_element_get_state (consumer, NULL, NULL, GST_CLOCK_TIME_NONE);
  fail_unless (state_res == GST_STATE_CHANGE_SUCCESS);

  g_signal_emit_by_name (sink, "pull-sample", &sample);
  gst_sample_unref (sample);

  state_res = gst_element_set_state (producer, GST_STATE_NULL);
  fail_unless (state_res != GST_STATE_CHANGE_FAILURE);

  state_res = gst_element_set_state (consumer, GST_STATE_NULL);
  fail_unless (state_res != GST_STATE_CHANGE_FAILURE);

  gst_object_unref (consumer);
  gst_object_unref (producer);

  g_free (socket_path);
}

GST_END_TEST;

static Suite *
shm_suite (void)
{
  Suite *s = suite_create ("shm");
  TCase *tc;

  tc = tcase_create ("shm");
  tcase_add_checked_fixture (tc, setup_shm, NULL);
  tcase_add_test (tc, test_shm_sysmem_alloc);
  tcase_add_test (tc, test_shm_alloc);
  suite_add_tcase (s, tc);

  tc = tcase_create ("shm2");
  tcase_add_test (tc, test_shm_live);
  suite_add_tcase (s, tc);

  return s;
}


GST_CHECK_MAIN (shm);
