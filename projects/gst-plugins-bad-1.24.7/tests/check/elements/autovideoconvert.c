/* GStreamer
 *
 * unit test for autovideoconvert element
 * Copyright (C) 2009 Jan Schmidt <thaytan@noraisin.net>
 * Copyright (C) 2010 ST-Ericsson SA 
 *  @author: Benjamin Gaignard <benjamin.gaignard@stericsson.com>
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
#  include "config.h"
#endif

#include <gst/gst.h>
#include <gst/check/gstcheck.h>

typedef struct
{
  GMainLoop *loop;
  gboolean eos;
} OnMessageUserData;

static void
on_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  OnMessageUserData *d = user_data;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
    case GST_MESSAGE_WARNING:
      g_assert_not_reached ();
      break;
    case GST_MESSAGE_EOS:
      g_main_loop_quit (d->loop);
      d->eos = TRUE;
      break;
    default:
      break;
  }
}

static void
run_test (const gchar * pipeline_string)
{
  GstElement *pipeline;
  GstBus *bus;
  GMainLoop *loop;
  OnMessageUserData omud = { NULL, };
  GstStateChangeReturn ret;

  GST_DEBUG ("Testing pipeline '%s'", pipeline_string);

  pipeline = gst_parse_launch (pipeline_string, NULL);
  fail_unless (pipeline != NULL);
  loop = g_main_loop_new (NULL, FALSE);

  bus = gst_element_get_bus (pipeline);
  fail_unless (bus != NULL);
  gst_bus_add_signal_watch (bus);

  omud.loop = loop;
  omud.eos = FALSE;

  g_signal_connect (bus, "message", (GCallback) on_message_cb, &omud);

  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  fail_unless (ret == GST_STATE_CHANGE_SUCCESS
      || ret == GST_STATE_CHANGE_ASYNC);

  g_main_loop_run (loop);

  fail_unless (gst_element_set_state (pipeline,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS);

  fail_unless (omud.eos == TRUE);

  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (pipeline);
  g_main_loop_unref (loop);

}

GST_START_TEST (test_autovideoconvert_rbg2bayer)
{
  gchar *pipeline;

  pipeline =
      g_strdup_printf
      ("videotestsrc num-buffers=1 ! video/x-raw,format=ARGB,depth=32,width=100,height=100,framerate=10/1 ! autovideoconvert ! video/x-bayer,width=100,height=100,format=bggr,framerate=10/1 ! fakesink");

  run_test (pipeline);
  g_free (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_autovideoconvert_videoconvert)
{
  gchar *pipeline;

  pipeline =
      g_strdup_printf
      ("videotestsrc num-buffers=1 ! video/x-raw, format=RGB,width=100,height=100,framerate=10/1 ! autovideoconvert ! video/x-raw,format=BGR,width=100,height=100,framerate=10/1 ! fakesink");

  run_test (pipeline);
  g_free (pipeline);
}

GST_END_TEST;

static Suite *
autovideoconvert_suite (void)
{
  Suite *s = suite_create ("autovideoconvert");
  TCase *tc_basic = tcase_create ("general");

  suite_add_tcase (s, tc_basic);
  tcase_add_test (tc_basic, test_autovideoconvert_rbg2bayer);
  tcase_add_test (tc_basic, test_autovideoconvert_videoconvert);

  return s;
}

GST_CHECK_MAIN (autovideoconvert);
