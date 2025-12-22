/* GStreamer
 *
 * Copyright (C) 2024 Piotr BrzeziÅ„ski <piotr@centricular.com>
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

static void
eos_cb (GstBus * bus, GstMessage * message, GMainLoop * loop)
{
  GST_DEBUG ("Received EOS");
  g_main_loop_quit (loop);
}

static void
error_cb (GstBus * bus, GstMessage * message, GMainLoop * loop)
{
  GError *err = NULL;
  gchar *dbg = NULL;

  gst_message_parse_error (message, &err, &dbg);
  g_error ("ERROR: %s\n%s\n", err->message, dbg);
}

static void
test_element (const gchar * element)
{
  GstElement *pipeline;
  GstBus *bus;
  GError *error = NULL;
  gchar *pipe_str;
  GMainLoop *loop;

  pipe_str =
      g_strdup_printf
      ("audiotestsrc num-buffers=20 ! audio/x-raw,format=S16LE,channels=2 ! %s ! fakesink",
      element);

  pipeline = gst_parse_launch (pipe_str, &error);
  fail_unless (pipeline != NULL, "Could not create pipeline: %s",
      error->message);
  g_free (pipe_str);

  loop = g_main_loop_new (NULL, FALSE);

  bus = gst_element_get_bus (pipeline);
  fail_if (bus == NULL);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::eos", (GCallback) eos_cb, loop);
  g_signal_connect (bus, "message::error", (GCallback) error_cb, loop);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (loop);
  gst_element_set_state (pipeline, GST_STATE_NULL);

  g_main_loop_unref (loop);
  gst_object_unref (bus);
  gst_object_unref (pipeline);
}

GST_START_TEST (test_simple_pipelines)
{
  /* Simple pipeline tests to see if these elements run at all.
   * Will help catch breakages like https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/6800. */
  test_element ("wavescope");
  test_element ("spacescope");
  test_element ("spectrascope");
  test_element ("synaescope");
}

GST_END_TEST;

static Suite *
audiovisualizer_suite (void)
{
  Suite *s = suite_create ("audiovisualizer");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
#ifndef GST_DISABLE_PARSE
  tcase_add_test (tc_chain, test_simple_pipelines);
#endif

  return s;
}

GST_CHECK_MAIN (audiovisualizer);
