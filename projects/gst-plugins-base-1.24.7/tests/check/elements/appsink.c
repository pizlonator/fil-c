/* GStreamer
 *
 * Copyright (C) 2009, Axis Communications AB, LUND, SWEDEN
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
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

gint global_testdata;

static GstPad *mysrcpad;

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-gst-check")
    );

static GstElement *
setup_appsink (void)
{
  GstElement *appsink;
  GstCaps *caps;

  GST_DEBUG ("setup_appsink");
  appsink = gst_check_setup_element ("appsink");
  mysrcpad = gst_check_setup_src_pad (appsink, &srctemplate);
  gst_pad_set_active (mysrcpad, TRUE);

  caps = gst_caps_new_empty_simple ("application/x-gst-check");
  gst_check_setup_events (mysrcpad, appsink, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);

  return appsink;
}

static void
cleanup_appsink (GstElement * appsink)
{
  GST_DEBUG ("cleanup_appsink");

  gst_check_teardown_src_pad (appsink);
  gst_check_teardown_element (appsink);
}

/* This function does an operation to it's indata argument and returns it.
 * The exact operation performed doesn't matter. Currently it multiplies with
 * two, but it could do anything. The idea is to use the function to verify
 * that the code calling it gets run. */
static gint
operate_on_data (gint indata)
{
  return indata * 2;
}

static GstFlowReturn
callback_function (GstAppSink * appsink, gpointer callback_data)
{
  global_testdata = operate_on_data (*((gint *) callback_data));

  return GST_FLOW_OK;
}

static void
notify_function (gpointer callback_data)
{
  global_testdata = operate_on_data (*((gint *) callback_data));
}

GST_START_TEST (test_non_clients)
{
  GstElement *sink;
  GstBuffer *buffer;

  sink = setup_appsink ();

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  GST_DEBUG ("cleaning up appsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

/* Verifies that the handoff callback gets run one time when passing a buffer */
GST_START_TEST (test_handoff_callback)
{
  GstElement *sink;
  GstBuffer *buffer;
  gint testdata;
  GstAppSinkCallbacks callbacks = { NULL };

  sink = setup_appsink ();

  global_testdata = 0;
  testdata = 5;                 /* Arbitrary value */

  callbacks.new_sample = callback_function;

  gst_app_sink_set_callbacks (GST_APP_SINK (sink), &callbacks, &testdata, NULL);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  buffer = gst_buffer_new_and_alloc (4);
  /* Pushing a buffer should run our callback */
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  testdata = operate_on_data (testdata);

  /* If both test_data & global_testdata have been operated on, we're happy. */
  fail_unless (testdata == global_testdata);

  GST_DEBUG ("cleaning up appsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

/* Verifies that the notify function gets executed when the sink is destroyed */
GST_START_TEST (test_notify0)
{
  GstElement *sink;
  gint testdata;
  GstAppSinkCallbacks callbacks = { NULL };

  sink = gst_element_factory_make ("appsink", NULL);

  global_testdata = 0;
  testdata = 17;                /* Arbitrary value */

  gst_app_sink_set_callbacks (GST_APP_SINK (sink), &callbacks,
      &testdata, (*notify_function));

  GST_DEBUG ("cleaning up appsink");
  /* Destroying sink should call our notify_function */
  gst_object_unref (sink);

  testdata = operate_on_data (testdata);

  /* If both test_data & global_testdata have been operated on, we're happy. */
  fail_unless (testdata == global_testdata);
}

GST_END_TEST;


/* Verifies that the notify function gets executed when
 * gst_app_sink_set_callbacks () gets called */
GST_START_TEST (test_notify1)
{
  GstElement *sink;
  gint testdata;
  GstAppSinkCallbacks callbacks = { NULL };

  sink = gst_element_factory_make ("appsink", NULL);

  global_testdata = 0;
  testdata = 42;                /* Arbitrary value */

  gst_app_sink_set_callbacks (GST_APP_SINK (sink), &callbacks,
      &testdata, (*notify_function));
  /* Setting new callbacks should trigger the destroy of the old data */
  gst_app_sink_set_callbacks (GST_APP_SINK (sink), &callbacks, &testdata, NULL);

  testdata = operate_on_data (testdata);

  /* If both test_data & global_testdata have been operated on, we're happy. */
  fail_unless (testdata == global_testdata);

  GST_DEBUG ("cleaning up appsink");
  gst_object_unref (sink);
}

GST_END_TEST;

static const gint values[] = { 1, 2, 4 };

static GstBufferList *
create_buffer_list_with_buffer_duration (GstClockTime duration)
{
  guint len;
  GstBuffer *buffer;
  GstBufferList *mylist;

  mylist = gst_buffer_list_new ();
  fail_if (mylist == NULL);

  len = gst_buffer_list_length (mylist);
  fail_if (len != 0);

  buffer = gst_buffer_new_and_alloc (sizeof (gint));
  gst_buffer_fill (buffer, 0, &values[0], sizeof (gint));
  gst_buffer_list_add (mylist, buffer);

  buffer = gst_buffer_new_and_alloc (sizeof (gint));
  gst_buffer_fill (buffer, 0, &values[1], sizeof (gint));
  gst_buffer_list_add (mylist, buffer);

  buffer = gst_buffer_new_and_alloc (sizeof (gint));
  gst_buffer_fill (buffer, 0, &values[2], sizeof (gint));
  gst_buffer_list_add (mylist, buffer);

  if (GST_CLOCK_TIME_IS_VALID (duration)) {
    guint i;
    GstClockTime dts = 0;
    GstBuffer *current;

    for (i = 0; i < gst_buffer_list_length (mylist); ++i) {
      current = gst_buffer_list_get (mylist, i);
      GST_BUFFER_DTS (current) = dts;
      dts += duration;
    }
  }

  return mylist;
}

static GstBufferList *
create_buffer_list (void)
{
  return create_buffer_list_with_buffer_duration (GST_CLOCK_TIME_NONE);
}

static GstFlowReturn
callback_function_sample_fallback (GstAppSink * appsink, gpointer p_counter)
{
  GstSample *sample;
  GstBuffer *buf;
  gint *p_int_counter = p_counter;

  sample = gst_app_sink_pull_sample (appsink);
  buf = gst_sample_get_buffer (sample);
  fail_unless (GST_IS_BUFFER (buf));

  /* buffer list has 3 buffers in two groups */
  switch (*p_int_counter) {
    case 0:
      fail_unless_equals_int (gst_buffer_get_size (buf), sizeof (gint));
      gst_check_buffer_data (buf, &values[0], sizeof (gint));
      break;
    case 1:
      fail_unless_equals_int (gst_buffer_get_size (buf), sizeof (gint));
      gst_check_buffer_data (buf, &values[1], sizeof (gint));
      break;
    case 2:
      fail_unless_equals_int (gst_buffer_get_size (buf), sizeof (gint));
      gst_check_buffer_data (buf, &values[2], sizeof (gint));
      break;
    default:
      g_warn_if_reached ();
      break;
  }

  gst_sample_unref (sample);

  *p_int_counter += 1;

  return GST_FLOW_OK;
}

static GstFlowReturn
callback_function_sample (GstAppSink * appsink, gpointer p_counter)
{
  GstSample *sample;
  GstBufferList *list;
  gint *p_int_counter = p_counter;
  guint len;
  gint i;

  sample = gst_app_sink_pull_sample (appsink);
  list = gst_sample_get_buffer_list (sample);
  fail_unless (GST_IS_BUFFER_LIST (list));
  len = gst_buffer_list_length (list);
  fail_unless_equals_int (len, 3);

  for (i = 0; i < len; i++) {
    GstBuffer *buf = gst_buffer_list_get (list, i);
    fail_unless_equals_int (gst_buffer_get_size (buf), sizeof (gint));
    gst_check_buffer_data (buf, &values[i], sizeof (gint));
  }

  gst_sample_unref (sample);

  *p_int_counter += 1;

  return GST_FLOW_OK;
}

GST_START_TEST (test_buffer_list_fallback)
{
  GstElement *sink;
  GstBufferList *list;
  GstAppSinkCallbacks callbacks = { NULL };
  gint counter = 0;
  gboolean buffer_list_support;

  sink = setup_appsink ();

  /* verify that the buffer list support is disabled per default */
  g_object_get (sink, "buffer-list", &buffer_list_support, NULL);
  fail_unless (buffer_list_support == FALSE);


  callbacks.new_sample = callback_function_sample_fallback;

  gst_app_sink_set_callbacks (GST_APP_SINK (sink), &callbacks, &counter, NULL);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  list = create_buffer_list ();
  fail_unless (gst_pad_push_list (mysrcpad, list) == GST_FLOW_OK);

  fail_unless_equals_int (counter, 3);

  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

GST_START_TEST (test_buffer_list_support)
{
  GstElement *sink;
  GstBufferList *list;
  GstAppSinkCallbacks callbacks = { NULL };
  gint counter = 0;

  sink = setup_appsink ();

  /* enable buffer list support */
  g_object_set (sink, "buffer-list", TRUE, NULL);

  callbacks.new_sample = callback_function_sample;

  gst_app_sink_set_callbacks (GST_APP_SINK (sink), &callbacks, &counter, NULL);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  list = create_buffer_list ();
  fail_unless (gst_pad_push_list (mysrcpad, list) == GST_FLOW_OK);

  fail_unless_equals_int (counter, 1);

  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

GST_START_TEST (test_buffer_list_fallback_signal)
{
  GstElement *sink;
  GstBufferList *list;
  gint counter = 0;

  sink = setup_appsink ();

  /* C calling convention to the rescue.. */
  g_signal_connect (sink, "new-sample",
      G_CALLBACK (callback_function_sample_fallback), &counter);

  g_object_set (sink, "emit-signals", TRUE, NULL);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  list = create_buffer_list ();
  fail_unless (gst_pad_push_list (mysrcpad, list) == GST_FLOW_OK);

  fail_unless_equals_int (counter, 3);

  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

GST_START_TEST (test_buffer_list_signal)
{
  GstElement *sink;
  GstBufferList *list;
  gint counter = 0;

  sink = setup_appsink ();

  /* enable buffer list support */
  g_object_set (sink, "buffer-list", TRUE, NULL);

  /* C calling convention to the rescue.. */
  g_signal_connect (sink, "new-sample", G_CALLBACK (callback_function_sample),
      &counter);

  g_object_set (sink, "emit-signals", TRUE, NULL);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  list = create_buffer_list ();
  fail_unless (gst_pad_push_list (mysrcpad, list) == GST_FLOW_OK);

  fail_unless_equals_int (counter, 1);

  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

GST_START_TEST (test_segment)
{
  GstElement *sink;
  GstSegment segment;
  GstBuffer *buffer;
  GstSample *pulled_preroll;
  GstSample *pulled_sample;

  sink = setup_appsink ();

  gst_segment_init (&segment, GST_FORMAT_TIME);
  segment.start = 2 * GST_SECOND;
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  g_signal_emit_by_name (sink, "pull-preroll", &pulled_preroll);
  fail_unless (gst_segment_is_equal (&segment,
          gst_sample_get_segment (pulled_preroll)));
  gst_sample_unref (pulled_preroll);

  g_signal_emit_by_name (sink, "pull-sample", &pulled_sample);
  fail_unless (gst_segment_is_equal (&segment,
          gst_sample_get_segment (pulled_sample)));
  gst_sample_unref (pulled_sample);

  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

GST_START_TEST (test_pull_with_timeout)
{
  GstElement *sink;
  GstBuffer *buffer;
  GstSample *s;
  guint64 t1, tdiff;

  sink = setup_appsink ();

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  /* Check that it actually waits for a bit */
  t1 = gst_util_get_timestamp ();
  s = gst_app_sink_try_pull_preroll (GST_APP_SINK (sink), GST_SECOND / 20);
  tdiff = gst_util_get_timestamp () - t1;
  GST_LOG ("tdiff: %" GST_TIME_FORMAT, GST_TIME_ARGS (tdiff));
  fail_unless (s == NULL);
  fail_unless (tdiff > (GST_SECOND / (20 * 2)));

  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  s = gst_app_sink_try_pull_preroll (GST_APP_SINK (sink), GST_SECOND / 20);
  fail_unless (s != NULL);
  gst_sample_unref (s);

  s = gst_app_sink_try_pull_sample (GST_APP_SINK (sink), 500 * GST_SECOND);
  fail_unless (s != NULL);
  gst_sample_unref (s);

  /* No waiting */
  s = gst_app_sink_try_pull_sample (GST_APP_SINK (sink), 0);
  fail_unless (s == NULL);

  /* Check that it actually waits for a bit */
  t1 = gst_util_get_timestamp ();
  s = gst_app_sink_try_pull_sample (GST_APP_SINK (sink), GST_SECOND / 20);
  tdiff = gst_util_get_timestamp () - t1;
  GST_LOG ("tdiff: %" GST_TIME_FORMAT, GST_TIME_ARGS (tdiff));
  fail_unless (s == NULL);
  fail_unless (tdiff > (GST_SECOND / (20 * 2)));

  /* No waiting, with buffer pending */
  buffer = gst_buffer_new_and_alloc (5);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  s = gst_app_sink_try_pull_sample (GST_APP_SINK (sink), 0);
  fail_unless (s != NULL);
  gst_sample_unref (s);

  /* With timeout, with buffer pending */
  buffer = gst_buffer_new_and_alloc (6);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  s = gst_app_sink_try_pull_sample (GST_APP_SINK (sink), GST_SECOND / 20);
  fail_unless (s != NULL);
  gst_sample_unref (s);

  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

GST_START_TEST (test_pull_preroll)
{
  GstElement *sink = NULL;
  GstBuffer *buffer = NULL;
  GstSample *pulled_preroll = NULL;

  sink = setup_appsink ();

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  pulled_preroll = gst_app_sink_pull_preroll (GST_APP_SINK (sink));
  fail_unless (pulled_preroll);
  gst_sample_unref (pulled_preroll);

  fail_if (gst_app_sink_try_pull_preroll (GST_APP_SINK (sink), 0));

  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

GST_START_TEST (test_do_not_care_preroll)
{
  GstElement *sink = NULL;
  GstBuffer *buffer = NULL;
  GstSample *pulled_sample = NULL;

  sink = setup_appsink ();

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  pulled_sample = gst_app_sink_pull_sample (GST_APP_SINK (sink));
  fail_unless (pulled_sample);
  gst_sample_unref (pulled_sample);

  fail_if (gst_app_sink_try_pull_preroll (GST_APP_SINK (sink), 0));

  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

typedef struct
{
  GMutex mutex;
  GCond cond;
  GstAppSink *appsink;
  gboolean check;
} TestQueryDrainContext;

#define TEST_QUERY_DRAIN_CONTEXT_INIT { { 0, }, }

static gpointer
my_app_thread (TestQueryDrainContext * ctx)
{
  GstSample *pulled_preroll = NULL;
  GstSample *pulled_sample = NULL;

  /* Wait for the query to reach appsink. */
  g_mutex_lock (&ctx->mutex);
  while (!ctx->check)
    g_cond_wait (&ctx->cond, &ctx->mutex);
  g_mutex_unlock (&ctx->mutex);

  pulled_preroll = gst_app_sink_pull_preroll (ctx->appsink);
  fail_unless (pulled_preroll);
  gst_sample_unref (pulled_preroll);

  pulled_sample = gst_app_sink_pull_sample (ctx->appsink);
  fail_unless (pulled_sample);
  gst_sample_unref (pulled_sample);

  pulled_sample = gst_app_sink_pull_sample (ctx->appsink);
  fail_unless (pulled_sample);
  gst_sample_unref (pulled_sample);

  return NULL;
}

static GstPadProbeReturn
query_handler (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstQuery *query = GST_PAD_PROBE_INFO_QUERY (info);
  TestQueryDrainContext *ctx = (TestQueryDrainContext *) user_data;
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DRAIN:
    {
      if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_PUSH) {
        g_mutex_lock (&ctx->mutex);
        ctx->check = TRUE;
        g_cond_signal (&ctx->cond);
        g_mutex_unlock (&ctx->mutex);
      } else if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_PULL) {
        /* Check that there is no pending buffers when drain query is done. */
        fail_if (gst_app_sink_try_pull_preroll (ctx->appsink, 0));
        fail_if (gst_app_sink_try_pull_sample (ctx->appsink, 0));
      }
      break;
    }
    default:
      break;
  }
  return GST_PAD_PROBE_OK;
}

GST_START_TEST (test_query_drain)
{
  GstElement *sink = NULL;
  GstBuffer *buffer = NULL;
  GstPad *sinkpad = NULL;
  GThread *thread = NULL;
  GstQuery *query = NULL;
  TestQueryDrainContext ctx = TEST_QUERY_DRAIN_CONTEXT_INIT;

  sink = setup_appsink ();

  g_mutex_init (&ctx.mutex);
  g_cond_init (&ctx.cond);
  ctx.appsink = GST_APP_SINK (sink);
  ctx.check = FALSE;

  sinkpad = gst_element_get_static_pad (sink, "sink");
  gst_pad_add_probe (sinkpad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM,
      query_handler, (gpointer) & ctx, NULL);
  gst_object_unref (sinkpad);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  thread = g_thread_new ("appthread", (GThreadFunc) my_app_thread, &ctx);
  fail_unless (thread != NULL);

  query = gst_query_new_drain ();
  fail_unless (gst_pad_peer_query (mysrcpad, query));
  gst_query_unref (query);

  g_thread_join (thread);

  g_mutex_clear (&ctx.mutex);
  g_cond_clear (&ctx.cond);

  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

GST_START_TEST (test_pull_sample_refcounts)
{
  GstElement *sink;
  GstBuffer *buffer;
  GstSample *s1, *s2, *s3;

  sink = setup_appsink ();

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  s1 = gst_app_sink_pull_sample (GST_APP_SINK (sink));
  fail_unless (s1 != NULL);
  fail_unless (gst_buffer_get_size (gst_sample_get_buffer (s1)) == 4);
  gst_sample_unref (s1);

  buffer = gst_buffer_new_and_alloc (6);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  s2 = gst_app_sink_pull_sample (GST_APP_SINK (sink));
  fail_unless (s2 != NULL);
  fail_unless (gst_buffer_get_size (gst_sample_get_buffer (s2)) == 6);

  /* We unreffed s1, appsink should thus reuse the same sample,
   * avoiding an extra allocation */
  fail_unless (s1 == s2);

  buffer = gst_buffer_new_and_alloc (8);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  s3 = gst_app_sink_pull_sample (GST_APP_SINK (sink));
  fail_unless (s3 != NULL);
  fail_unless (gst_buffer_get_size (gst_sample_get_buffer (s2)) == 6);
  fail_unless (gst_buffer_get_size (gst_sample_get_buffer (s3)) == 8);


  /* We didn't unref s2, appsink should thus have created a new sample */
  fail_unless (s2 != s3);

  gst_sample_unref (s2);
  gst_sample_unref (s3);

  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

static gboolean
new_event_cb (GstAppSink * appsink, gpointer callback_data)
{
  guint *new_event_count = callback_data;
  *new_event_count += 1;
  return TRUE;
}

/* Verifies that the event callback is called */
GST_START_TEST (test_event_callback)
{
  GstElement *sink;
  GstPad *sinkpad;
  GstBuffer *buffer;
  guint new_event_count;
  GstAppSinkCallbacks callbacks = { NULL };
  GstMiniObject *object;
  GstAppSink *app_sink;

  sink = setup_appsink ();
  app_sink = GST_APP_SINK (sink);

  callbacks.new_event = new_event_cb;

  gst_app_sink_set_callbacks (app_sink, &callbacks, &new_event_count, NULL);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  /* push a buffer so pending events are pushed */
  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* flush pending events from the queue */
  while ((object = gst_app_sink_try_pull_object (app_sink, 0)))
    gst_mini_object_unref (object);
  new_event_count = 0;

  /* push a buffer */
  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* push custom event */
  sinkpad = gst_element_get_static_pad (sink, "sink");
  fail_unless (sinkpad);
  fail_unless (gst_pad_send_event (sinkpad,
          gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
              gst_structure_new ("custom", NULL, NULL))));
  fail_unless_equals_int (new_event_count, 1);
  gst_object_unref (sinkpad);

  /* push a second buffer */
  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* check if the samples and events are pulled in the right order */
  object = gst_app_sink_pull_object (app_sink);
  fail_unless (GST_IS_SAMPLE (object));
  gst_mini_object_unref (object);

  object = gst_app_sink_pull_object (app_sink);
  fail_unless (GST_IS_EVENT (object));
  fail_unless_equals_int (GST_EVENT_TYPE (object), GST_EVENT_CUSTOM_DOWNSTREAM);
  gst_mini_object_unref (object);

  object = gst_app_sink_pull_object (app_sink);
  fail_unless (GST_IS_SAMPLE (object));
  gst_mini_object_unref (object);

  GST_DEBUG ("cleaning up appsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;


GST_START_TEST (test_event_signals)
{
  GstElement *sink;
  GstPad *sinkpad;
  GstBuffer *buffer;
  GstMiniObject *object;
  GstAppSink *app_sink;
  guint new_event_count = 0;

  sink = setup_appsink ();
  app_sink = GST_APP_SINK (sink);

  g_object_set (sink, "emit-signals", TRUE, NULL);

  g_signal_connect (sink, "new-serialized-event", G_CALLBACK (new_event_cb),
      &new_event_count);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  /* push a buffer so pending events are pushed */
  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* flush pending events from the queue */
  while ((object = gst_app_sink_try_pull_object (app_sink, 0)))
    gst_mini_object_unref (object);
  new_event_count = 0;

  /* push a buffer */
  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* push custom event */
  sinkpad = gst_element_get_static_pad (sink, "sink");
  fail_unless (sinkpad);
  fail_unless (gst_pad_send_event (sinkpad,
          gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
              gst_structure_new ("custom", NULL, NULL))));
  fail_unless_equals_int (new_event_count, 1);
  gst_object_unref (sinkpad);

  /* push a second buffer */
  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* check if the buffers and events are pulled in the right order */
  g_signal_emit_by_name (sink, "try-pull-object", GST_CLOCK_TIME_NONE, &object);
  fail_unless (GST_IS_SAMPLE (object));
  gst_mini_object_unref (object);

  g_signal_emit_by_name (sink, "try-pull-object", GST_CLOCK_TIME_NONE, &object);
  fail_unless (GST_IS_EVENT (object));
  fail_unless_equals_int (GST_EVENT_TYPE (object), GST_EVENT_CUSTOM_DOWNSTREAM);
  gst_mini_object_unref (object);

  g_signal_emit_by_name (sink, "try-pull-object", GST_CLOCK_TIME_NONE, &object);
  fail_unless (GST_IS_SAMPLE (object));
  gst_mini_object_unref (object);

  GST_DEBUG ("cleaning up appsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);

}

GST_END_TEST;

/* try pulling events when appsink is in PAUSED */
GST_START_TEST (test_event_paused)
{
  GstElement *sink;
  guint new_event_count = 0;
  GstAppSinkCallbacks callbacks = { NULL };
  GstMiniObject *object;
  GstAppSink *app_sink;
  GstCaps *caps;

  sink = setup_appsink ();
  app_sink = GST_APP_SINK (sink);

  callbacks.new_event = new_event_cb;

  gst_app_sink_set_callbacks (app_sink, &callbacks, &new_event_count, NULL);

  ASSERT_SET_STATE (sink, GST_STATE_PAUSED, GST_STATE_CHANGE_ASYNC);

  /* push a couple of events while in PAUSED */
  gst_pad_push_event (mysrcpad, gst_event_new_stream_start ("test"));
  caps = gst_caps_new_simple ("audio/x-raw", NULL, NULL);
  gst_pad_push_event (mysrcpad, gst_event_new_caps (caps));
  gst_caps_unref (caps);

  fail_unless_equals_int (new_event_count, 2);

  /* check pulled events */
  object = gst_app_sink_pull_object (app_sink);
  fail_unless (GST_IS_EVENT (object));
  fail_unless_equals_int (GST_EVENT_TYPE (object), GST_EVENT_STREAM_START);
  gst_mini_object_unref (object);

  object = gst_app_sink_pull_object (app_sink);
  fail_unless (GST_IS_EVENT (object));
  fail_unless_equals_int (GST_EVENT_TYPE (object), GST_EVENT_CAPS);
  gst_mini_object_unref (object);

  object = gst_app_sink_try_pull_object (app_sink, 0);
  fail_if (object);

  GST_DEBUG ("cleaning up appsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

GST_START_TEST (test_reverse_stepping)
{
  GstElement *pipeline;
  GstStateChangeReturn state_ret;
  GstState state = GST_STATE_NULL;
  gboolean ret;
  GstEvent *event;
  GstAppSink *sink;
  GstSample *sample;
  GstBuffer *buffer;
  GstClockTime running_time;

  pipeline =
      gst_parse_launch ("videotestsrc name=src ! video/x-raw,framerate=1/1 "
      "! appsink name=sink max-buffers=1", NULL);
  fail_unless (pipeline != NULL);

  sink = (GstAppSink *) gst_bin_get_by_name (GST_BIN (pipeline), "sink");
  fail_unless (sink != NULL);

  /* Pause and ensure preroll */
  state_ret = gst_element_set_state (pipeline, GST_STATE_PAUSED);
  fail_unless (state_ret != GST_STATE_CHANGE_FAILURE);

  state_ret =
      gst_element_get_state (pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
  fail_unless (state_ret == GST_STATE_CHANGE_SUCCESS);
  fail_unless (state == GST_STATE_PAUSED);

  ret = gst_element_seek (pipeline, -1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_NONE,
      -1, GST_SEEK_TYPE_SET, 10 * GST_SECOND);
  fail_unless (ret != FALSE);

  state_ret =
      gst_element_get_state (pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
  fail_unless (state_ret == GST_STATE_CHANGE_SUCCESS);
  fail_unless (state == GST_STATE_PAUSED);

  sample = gst_app_sink_pull_preroll (sink);
  fail_unless (GST_IS_SAMPLE (sample));
  buffer = gst_sample_get_buffer (sample);
  fail_unless (GST_IS_BUFFER (buffer));

  /* start running time */
  running_time = GST_BUFFER_PTS (buffer);
  gst_sample_unref (sample);

  do {
    /* timestamp of new preroll buffer should be
     * "previous running time - buffer duration"
     */
    running_time -= GST_SECOND;
    event = gst_event_new_step (GST_FORMAT_BUFFERS, 1, 1.0, TRUE, FALSE);
    ret = gst_element_send_event (pipeline, event);
    fail_unless (ret);
    state_ret =
        gst_element_get_state (pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
    fail_unless (state_ret == GST_STATE_CHANGE_SUCCESS);
    fail_unless (state == GST_STATE_PAUSED);

    sample = gst_app_sink_pull_preroll (sink);
    /* EOS */
    if (!sample)
      break;

    fail_unless (GST_IS_SAMPLE (sample));
    buffer = gst_sample_get_buffer (sample);
    fail_unless (GST_IS_BUFFER (buffer));
    fail_unless_equals_uint64 (running_time, GST_BUFFER_PTS (buffer));
    gst_sample_unref (sample);
  } while (sample);

  state_ret = gst_element_set_state (pipeline, GST_STATE_NULL);
  fail_unless (state_ret == GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (sink);
  gst_object_unref (pipeline);
}

GST_END_TEST;

static void
push_caps_with_type (gint caps_type)
{
  GstCaps *caps;

  caps =
      gst_caps_new_simple ("application/x-gst-check", "type", G_TYPE_INT,
      caps_type, NULL);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_caps (caps)));

  gst_caps_unref (caps);
}

static void
push_buffer_with_number (gint buffer_number)
{
  GstBuffer *buffer;

  buffer = gst_buffer_new_and_alloc (sizeof (gint));
  gst_buffer_fill (buffer, 0, &buffer_number, sizeof (gint));
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
}

static void
pull_and_check_sample (GstElement * appsink, gint expected_buffer_number,
    gint expected_caps_type)
{
  GstSample *sample;
  GstCaps *caps;
  GstBuffer *buffer;
  GstStructure *structure;
  gint actual_caps_type;

  sample = gst_app_sink_pull_sample (GST_APP_SINK (appsink));

  caps = gst_sample_get_caps (sample);
  fail_unless (structure = gst_caps_get_structure (caps, 0));
  fail_unless (gst_structure_get_int (structure, "type", &actual_caps_type));
  assert_equals_int (actual_caps_type, expected_caps_type);

  buffer = gst_sample_get_buffer (sample);
  gst_check_buffer_data (buffer, &expected_buffer_number, sizeof (gint));

  gst_sample_unref (sample);
}

GST_START_TEST (test_caps_before_flush_race_condition)
{
  GstElement *sink;
  GstSegment segment;

  sink = setup_appsink ();

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  // Push a series of buffers, and at the end, a new caps event.
  push_caps_with_type (1);
  push_buffer_with_number (10);
  push_buffer_with_number (11);
  push_caps_with_type (2);

  pull_and_check_sample (sink, 10, 1);

  // Then, let a flush happen.
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_flush_start ()));
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_flush_stop (TRUE)));
  // Sinks downgrade state to PAUSED after a flush, let's up it to PLAYING again to avoid gst_pad_push becoming blocking.
  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);
  // A segment must be sent after a flush.
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  // Send a buffer now, and check that when pulled by the appsink user, it didn't come with the wrong old caps.
  push_buffer_with_number (20);
  pull_and_check_sample (sink, 20, 2);

  cleanup_appsink (sink);
}

GST_END_TEST;

static gboolean
propose_allocation_cb (GstAppSink * appsink, GstQuery * query,
    gpointer callback_data)
{
  guint *allocation_query_count = callback_data;
  *allocation_query_count += 1;
  fail_unless (gst_query_is_writable (query));
  fail_unless (GST_QUERY_TYPE (query) == GST_QUERY_ALLOCATION);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  return TRUE;
}

/* Verifies that the allocation query callback is called */
GST_START_TEST (test_query_allocation_callback)
{
  GstElement *sink;
  GstAppSinkCallbacks callbacks = { NULL };
  GstAppSink *app_sink;
  GstQuery *query = NULL;
  guint allocation_query_count = 0;
  GstPad *sinkpad;

  sink = setup_appsink ();
  app_sink = GST_APP_SINK (sink);

  sinkpad = gst_element_get_static_pad (sink, "sink");
  fail_unless (sinkpad);

  callbacks.propose_allocation = propose_allocation_cb;
  gst_app_sink_set_callbacks (app_sink, &callbacks, &allocation_query_count,
      NULL);

  query = gst_query_new_allocation (NULL, FALSE);
  fail_unless (gst_pad_query (sinkpad, query));

  fail_unless_equals_int (allocation_query_count, 1);
  fail_unless (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE,
          NULL));

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  gst_object_unref (sinkpad);
  gst_query_unref (query);

  GST_DEBUG ("cleaning up appsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

GST_START_TEST (test_query_allocation_signals)
{
  GstElement *sink;
  GstQuery *query = NULL;
  guint allocation_query_count = 0;
  GstPad *sinkpad;

  sink = setup_appsink ();

  g_object_set (sink, "emit-signals", TRUE, NULL);
  g_signal_connect (sink, "propose-allocation",
      G_CALLBACK (propose_allocation_cb), &allocation_query_count);

  sinkpad = gst_element_get_static_pad (sink, "sink");
  fail_unless (sinkpad);
  query = gst_query_new_allocation (NULL, FALSE);
  fail_unless (gst_pad_query (sinkpad, query));

  fail_unless_equals_int (allocation_query_count, 1);
  fail_unless (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE,
          NULL));

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  gst_object_unref (sinkpad);
  if (query)
    gst_query_unref (query);

  GST_DEBUG ("cleaning up appsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

struct TestBufferingLimitsParams
{
  guint64 max_time;
  guint max_buffers;
  guint max_bytes;
  guint expected_num_samples;
};

static struct TestBufferingLimitsParams test_buffering_limit_params[] = {
  /* no limits */
  {0, 0, 0, 3},
  /* exceeded time limit and max-time is not a multiple of buffer duration:
   * this effectively means the queue will have to pass one additional buffer before blocking/dropping */
  {50 * GST_MSECOND, 0, 0, 3},
  /* exceeded buffers limit */
  {0, 2, 0, 2},
  /* exceeded bytes limit */
  {0, 0, 2 * sizeof (guint), 2},
  /* time and bytes; time is exceeded first */
  {20 * GST_MSECOND, 0, 2 * sizeof (guint), 1},
  /* time, buffers and bytes; bytes are exceeded first */
  {60 * GST_MSECOND, 2, 1 * sizeof (guint), 1},
};

GST_START_TEST (test_buffering_limits)
{
  const gboolean use_lists = __i__ % 2;
  struct TestBufferingLimitsParams *param =
      &test_buffering_limit_params[__i__ / 2];
  GstAppSink *app_sink = GST_APP_SINK (setup_appsink ());
  guint num_samples = 0;
  GstSample *queued_sample;
  GstBufferList *list;
  gint j;

  ASSERT_SET_STATE (app_sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  gst_app_sink_set_max_time (app_sink, param->max_time);
  gst_app_sink_set_max_bytes (app_sink, param->max_bytes);
  gst_app_sink_set_max_buffers (app_sink, param->max_buffers);
  gst_app_sink_set_drop (app_sink, TRUE);

  list = create_buffer_list_with_buffer_duration (20 * GST_MSECOND);

  if (use_lists) {
    gst_buffer_list_ref (list);
    gst_pad_push_list (mysrcpad, list);
  } else {
    for (j = 0; j < gst_buffer_list_length (list); j++) {
      GstBuffer *buf = gst_buffer_list_get (list, j);
      gst_buffer_ref (buf);
      gst_pad_push (mysrcpad, buf);
    }
  }

  while ((queued_sample = gst_app_sink_try_pull_sample (app_sink, 0))) {
    num_samples++;
    gst_sample_unref (queued_sample);
  }

  fail_unless_equals_int (num_samples, param->expected_num_samples);

  ASSERT_SET_STATE (app_sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (GST_ELEMENT_CAST (app_sink));
  gst_buffer_list_unref (list);
}

GST_END_TEST;

static Suite *
appsink_suite (void)
{
  Suite *s = suite_create ("appsink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_non_clients);
  tcase_add_test (tc_chain, test_handoff_callback);
  tcase_add_test (tc_chain, test_notify0);
  tcase_add_test (tc_chain, test_notify1);
  tcase_add_test (tc_chain, test_buffer_list_fallback);
  tcase_add_test (tc_chain, test_buffer_list_support);
  tcase_add_test (tc_chain, test_buffer_list_fallback_signal);
  tcase_add_test (tc_chain, test_buffer_list_signal);
  tcase_add_test (tc_chain, test_segment);
  tcase_add_test (tc_chain, test_pull_with_timeout);
  tcase_add_test (tc_chain, test_query_drain);
  tcase_add_test (tc_chain, test_pull_preroll);
  tcase_add_test (tc_chain, test_do_not_care_preroll);
  tcase_add_test (tc_chain, test_pull_sample_refcounts);
  tcase_add_test (tc_chain, test_event_callback);
  tcase_add_test (tc_chain, test_event_signals);
  tcase_add_test (tc_chain, test_event_paused);
  tcase_add_test (tc_chain, test_reverse_stepping);
  tcase_add_test (tc_chain, test_caps_before_flush_race_condition);
  tcase_add_test (tc_chain, test_query_allocation_callback);
  tcase_add_test (tc_chain, test_query_allocation_signals);
  tcase_add_loop_test (tc_chain, test_buffering_limits, 0,
      G_N_ELEMENTS (test_buffering_limit_params) * 2);

  return s;
}

GST_CHECK_MAIN (appsink);
