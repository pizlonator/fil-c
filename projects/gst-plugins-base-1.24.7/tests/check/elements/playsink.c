/* GStreamer unit tests for playsink
 * Copyright (C) 2015 Tim-Philipp Müller <tim centricular com>
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
# include <config.h>
#endif

#include <gst/check/gstcheck.h>


GST_START_TEST (test_volume_in_sink)
{
  GstElement *pipe, *audiosink, *playsink, *fakesink, *volume, *src;
  GstPad *sinkpad;
  GstMessage *msg;

  pipe = gst_pipeline_new (NULL);
  playsink = gst_element_factory_make ("playsink", NULL);

  audiosink = gst_bin_new ("audiosink");
  volume = gst_element_factory_make ("volume", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);
  gst_bin_add_many (GST_BIN (audiosink), volume, fakesink, NULL);
  gst_element_link_many (volume, fakesink, NULL);
  sinkpad = gst_element_get_static_pad (volume, "sink");
  gst_element_add_pad (audiosink, gst_ghost_pad_new ("sink", sinkpad));
  gst_object_unref (sinkpad);

  g_object_set (playsink, "audio-sink", audiosink, NULL);

  src = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (src, "num-buffers", 5, NULL);


  gst_bin_add (GST_BIN (pipe), src);
  gst_bin_add (GST_BIN (pipe), playsink);

  if (!gst_element_link (src, playsink))
    g_error ("oops");

  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  /* wait for eos */
  msg = gst_bus_timed_pop_filtered (GST_ELEMENT_BUS (pipe), GST_CLOCK_TIME_NONE,
      GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
  fail_if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR);
  gst_message_unref (msg);

  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (pipe);
}

GST_END_TEST;


static Suite *
playsink_suite (void)
{
  Suite *s = suite_create ("playsink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_volume_in_sink);

  return s;
}

GST_CHECK_MAIN (playsink);
