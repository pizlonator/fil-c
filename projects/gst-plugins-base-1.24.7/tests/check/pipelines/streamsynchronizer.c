/* GStreamer
 * Copyright (C) 2014 Song Bing <b06498@freescale.com>
 *
 * streamsynchronizer.c: Unit test for streamsynchronizer
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

static gboolean have_group_id = FALSE;
static guint group_id_pre;
static GMutex test_mutex;

static GstPadProbeReturn
event_probe (GstPad * pad, GstPadProbeInfo * info, gpointer udata)
{
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;
  GstEvent *event = GST_PAD_PROBE_INFO_DATA (info);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:{
      guint group_id;

      g_mutex_lock (&test_mutex);
      fail_unless (gst_event_parse_group_id (event, &group_id));

      if (have_group_id) {
        if (group_id_pre != group_id) {
          event = gst_event_copy (event);
          gst_event_set_group_id (event, group_id_pre);
          gst_event_replace ((GstEvent **) & info->data, event);
          gst_event_unref (event);
        }
      } else {
        group_id_pre = group_id;
        have_group_id = TRUE;
      }
      g_mutex_unlock (&test_mutex);
      break;
    }
    default:
      break;
  }

  return ret;
}

static void
run_streamsynchronizer_handle_eos (const gchar * launch_line)
{
  GstElement *pipeline;
  GstElement *audiosrc;
  GstElement *videosrc;
  GstMessage *msg;
  GstPad *pad;
  GstBus *bus;

  pipeline = gst_parse_launch (launch_line, NULL);
  fail_unless (pipeline != NULL);

  videosrc = gst_bin_get_by_name (GST_BIN (pipeline), "videosrc");
  fail_unless (videosrc != NULL);

  pad = gst_element_get_static_pad (videosrc, "src");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, event_probe,
      videosrc, NULL);
  gst_object_unref (pad);
  gst_object_unref (videosrc);

  audiosrc = gst_bin_get_by_name (GST_BIN (pipeline), "audiosrc");
  fail_unless (audiosrc != NULL);

  pad = gst_element_get_static_pad (audiosrc, "src");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, event_probe,
      audiosrc, NULL);
  gst_object_unref (pad);
  gst_object_unref (audiosrc);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  fail_unless (gst_element_set_state (pipeline, GST_STATE_PLAYING) !=
      GST_STATE_CHANGE_FAILURE);

  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR);

  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ASYNC_DONE);
  gst_message_unref (msg);

  fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL,
          GST_CLOCK_TIME_NONE), GST_STATE_CHANGE_SUCCESS);

  fail_unless (gst_element_set_state (pipeline, GST_STATE_PAUSED) !=
      GST_STATE_CHANGE_FAILURE);

  /* can't ensure can received async-done message when call state change very quickly. */
  fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL,
          GST_CLOCK_TIME_NONE), GST_STATE_CHANGE_SUCCESS);

  fail_unless (gst_element_set_state (pipeline, GST_STATE_PLAYING) !=
      GST_STATE_CHANGE_FAILURE);

  fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL,
          GST_CLOCK_TIME_NONE), GST_STATE_CHANGE_SUCCESS);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (bus);
  gst_object_unref (pipeline);
}

GST_START_TEST (test_streamsynchronizer_normal)
{
  run_streamsynchronizer_handle_eos ("videotestsrc name=videosrc ! "
      "streamsynchronizer name=streamsync audiotestsrc name=audiosrc ! "
      "streamsync. streamsync. ! fakesink sync=true streamsync. ! fakesink sync=true");
}

GST_END_TEST;

GST_START_TEST (test_streamsynchronizer_track_with_less_data)
{
  run_streamsynchronizer_handle_eos ("videotestsrc name=videosrc ! "
      "streamsynchronizer name=streamsync audiotestsrc name=audiosrc num-buffers=1 ! "
      "streamsync. streamsync. ! fakesink sync=true streamsync. ! fakesink sync=true");
}

GST_END_TEST;

GST_START_TEST (test_streamsynchronizer_track_without_data)
{
  run_streamsynchronizer_handle_eos ("videotestsrc name=videosrc ! "
      "streamsynchronizer name=streamsync audiotestsrc name=audiosrc num-buffers=0 ! "
      "streamsync. streamsync. ! fakesink sync=true streamsync. ! fakesink sync=true");
}

GST_END_TEST;

static Suite *
streamsynchronizer_handle_eos_suite (void)
{
  Suite *s = suite_create ("streamsynchronizer");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_streamsynchronizer_normal);
  tcase_add_test (tc_chain, test_streamsynchronizer_track_with_less_data);
  tcase_add_test (tc_chain, test_streamsynchronizer_track_without_data);
  return s;
}

GST_CHECK_MAIN (streamsynchronizer_handle_eos);
