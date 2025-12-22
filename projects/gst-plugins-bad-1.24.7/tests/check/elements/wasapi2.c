/* GStreamer
 *
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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
#include <string.h>

typedef struct
{
  GMainLoop *loop;
  GstElement *pipeline;
  guint n_buffers;
  guint restart_count;
  GstState reuse_state;
} SrcReuseTestData;

static gboolean
src_reuse_bus_handler (GstBus * bus, GstMessage * message,
    SrcReuseTestData * data)
{
  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {
    GST_ERROR ("Got error message from pipeline");
    g_main_loop_quit (data->loop);
  }

  return TRUE;
}

static void
start_pipeline (SrcReuseTestData * data)
{
  GstStateChangeReturn ret;

  GST_INFO ("Start pipeline");
  ret = gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
  fail_unless (ret != GST_STATE_CHANGE_FAILURE);
}

static gboolean
restart_pipeline (SrcReuseTestData * data)
{
  data->restart_count++;
  start_pipeline (data);

  return G_SOURCE_REMOVE;
}

static gboolean
handle_handoff (SrcReuseTestData * data)
{
  data->n_buffers++;

  /* Restart every 10 packets */
  if (data->n_buffers > 10) {
    GstStateChangeReturn ret;
    data->n_buffers = 0;

    ret = gst_element_set_state (data->pipeline, data->reuse_state);
    fail_unless (ret != GST_STATE_CHANGE_FAILURE);

    if (data->restart_count < 2) {
      GST_INFO ("Restart pipeline, current restart count %d",
          data->restart_count);
      g_timeout_add_seconds (1, (GSourceFunc) restart_pipeline, data);
    } else {
      GST_INFO ("Finish test");
      g_main_loop_quit (data->loop);
    }
  }

  return G_SOURCE_REMOVE;
}

static void
on_sink_handoff (GstElement * element, GstBuffer * buffer, GstPad * pad,
    SrcReuseTestData * data)
{
  g_idle_add ((GSourceFunc) handle_handoff, data);
}

static void
wasapi2src_reuse (GstState reuse_state)
{
  GstBus *bus;
  GstElement *sink;
  SrcReuseTestData data;

  memset (&data, 0, sizeof (SrcReuseTestData));

  data.loop = g_main_loop_new (NULL, FALSE);

  data.pipeline = gst_parse_launch ("wasapi2src provide-clock=false ! queue ! "
      "fakesink name=sink async=false", NULL);
  fail_unless (data.pipeline != NULL);
  data.reuse_state = reuse_state;

  sink = gst_bin_get_by_name (GST_BIN (data.pipeline), "sink");
  fail_unless (sink);

  g_object_set (G_OBJECT (sink), "signal-handoffs", TRUE, NULL);
  g_signal_connect (sink, "handoff", G_CALLBACK (on_sink_handoff), &data);

  bus = gst_element_get_bus (GST_ELEMENT (data.pipeline));
  fail_unless (bus != NULL);

  gst_bus_add_watch (bus, (GstBusFunc) src_reuse_bus_handler, &data);
  start_pipeline (&data);
  g_main_loop_run (data.loop);

  fail_unless (data.restart_count == 2);

  gst_element_set_start_time (data.pipeline, GST_STATE_NULL);
  gst_bus_remove_watch (bus);
  gst_object_unref (bus);

  gst_object_unref (data.pipeline);
  g_main_loop_unref (data.loop);
}

/* https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/issues/1110 */
GST_START_TEST (test_wasapi2src_reuse_null)
{
  wasapi2src_reuse (GST_STATE_NULL);
}

GST_END_TEST;

GST_START_TEST (test_wasapi2src_reuse_ready)
{
  wasapi2src_reuse (GST_STATE_READY);
}

GST_END_TEST;

typedef struct
{
  GMainLoop *loop;
  GstElement *pipe;
  guint rem_st_changes;
  GstState reuse_state;
} SinkPlayReadyTData;

static gboolean
sink_reuse_bus_watch_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  fail_unless (message->type != GST_MESSAGE_ERROR);

  return G_SOURCE_CONTINUE;
}

static gboolean
state_timer_cb (gpointer user_data)
{
  SinkPlayReadyTData *tdata = user_data;
  GstState nxt_st = tdata->rem_st_changes % 2 == 1 ?
      tdata->reuse_state : GST_STATE_PLAYING;

  ASSERT_SET_STATE (tdata->pipe, nxt_st, GST_STATE_CHANGE_SUCCESS);
  tdata->rem_st_changes--;

  if (tdata->rem_st_changes == 0) {
    g_main_loop_quit (tdata->loop);
    return G_SOURCE_REMOVE;
  }
  return G_SOURCE_CONTINUE;
}

/* Test that the wasapisink can survive the state change
 * from PLAYING to READY and then back to PLAYING */
static void
wasapi2sink_reuse (GstState reuse_state)
{
  SinkPlayReadyTData tdata;
  GstBus *bus;

  tdata.pipe =
      gst_parse_launch ("audiotestsrc ! wasapi2sink async=false", NULL);
  fail_unless (tdata.pipe != NULL);
  bus = gst_element_get_bus (tdata.pipe);
  fail_unless (bus != NULL);
  gst_bus_add_watch (bus, sink_reuse_bus_watch_cb, NULL);

  tdata.reuse_state = reuse_state;

  ASSERT_SET_STATE (tdata.pipe, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);
  tdata.rem_st_changes = 3;     /* -> READY -> PLAYING -> QUIT */
  g_timeout_add_seconds (1, state_timer_cb, &tdata);

  tdata.loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (tdata.loop);

  g_main_loop_unref (tdata.loop);
  gst_bus_remove_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (tdata.pipe);
}

GST_START_TEST (test_wasapi2sink_reuse_null)
{
  wasapi2sink_reuse (GST_STATE_NULL);
}

GST_END_TEST;

GST_START_TEST (test_wasapi2sink_reuse_ready)
{
  wasapi2sink_reuse (GST_STATE_READY);
}

GST_END_TEST;

static gboolean
check_wasapi2_element (gboolean is_src)
{
  gboolean ret = TRUE;
  GstElement *elem;
  const gchar *elem_name;

  if (is_src)
    elem_name = "wasapi2src";
  else
    elem_name = "wasapi2sink";

  elem = gst_element_factory_make (elem_name, NULL);
  if (!elem) {
    GST_INFO ("%s is not available", elem_name);
    return FALSE;
  }

  /* GST_STATE_READY is meaning that camera is available */
  if (gst_element_set_state (elem, GST_STATE_READY) != GST_STATE_CHANGE_SUCCESS) {
    GST_INFO ("cannot open device");
    ret = FALSE;
  }

  gst_element_set_state (elem, GST_STATE_NULL);
  gst_object_unref (elem);

  return ret;
}

static Suite *
wasapi2_suite (void)
{
  Suite *s = suite_create ("wasapi2");
  TCase *tc_basic = tcase_create ("general");
  gboolean have_src = FALSE;
  gboolean have_sink = FALSE;

  suite_add_tcase (s, tc_basic);

  have_src = check_wasapi2_element (TRUE);
  have_sink = check_wasapi2_element (FALSE);

  if (!have_src && !have_sink) {
    GST_INFO ("Skipping tests, wasapi2src/wasapi2sink are unavailable");
  } else {
    if (have_src) {
      tcase_add_test (tc_basic, test_wasapi2src_reuse_null);
      tcase_add_test (tc_basic, test_wasapi2src_reuse_ready);
    }

    if (have_sink) {
      tcase_add_test (tc_basic, test_wasapi2sink_reuse_null);
      tcase_add_test (tc_basic, test_wasapi2sink_reuse_ready);
    }
  }

  return s;
}

GST_CHECK_MAIN (wasapi2);
