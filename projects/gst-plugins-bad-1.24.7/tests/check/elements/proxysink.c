/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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
#include <gst/check/gstharness.h>

GST_START_TEST (test_flush_before_buffer)
{
  GstElement *sink, *src;
  GstHarness *h_in;
  GstHarness *h_out;
  GstEvent *event;
  GstSegment segment;
  GstCaps *caps;
  GstBuffer *buf;

  sink = gst_element_factory_make ("proxysink", NULL);
  src = gst_element_factory_make ("proxysrc", NULL);

  g_object_set (src, "proxysink", sink, NULL);

  h_in = gst_harness_new_with_element (sink, "sink", NULL);
  h_out = gst_harness_new_with_element (src, NULL, "src");
  gst_object_unref (sink);
  gst_object_unref (src);

  /* Activate only input side first, then push sticky events
   * without buffer */
  gst_harness_play (h_in);

  event = gst_event_new_stream_start ("proxy-test-stream-start");
  fail_unless (gst_harness_push_event (h_in, event));

  caps = gst_caps_from_string ("foo/bar");
  event = gst_event_new_caps (caps);
  gst_caps_unref (caps);
  fail_unless (gst_harness_push_event (h_in, event));

  gst_segment_init (&segment, GST_FORMAT_TIME);
  event = gst_event_new_segment (&segment);
  fail_unless (gst_harness_push_event (h_in, event));

  /* Now activate output side, sticky event and buffers should be
   * serialized */
  gst_harness_play (h_out);

  event = gst_event_new_flush_start ();
  fail_unless (gst_harness_push_event (h_in, event));

  event = gst_event_new_flush_stop (TRUE);
  fail_unless (gst_harness_push_event (h_in, event));

  event = gst_event_new_segment (&segment);
  fail_unless (gst_harness_push_event (h_in, event));

  buf = gst_buffer_new_and_alloc (4);
  GST_BUFFER_PTS (buf) = 0;
  GST_BUFFER_DTS (buf) = 0;

  /* There must be no critical warning regarding
   * sticky-event and buffer flow order*/
  fail_unless_equals_int (gst_harness_push (h_in, buf), GST_FLOW_OK);

  event = gst_event_new_eos ();
  fail_unless (gst_harness_push_event (h_in, event));

  /* make sure everything has been forwarded */
  fail_unless (gst_harness_pull_until_eos (h_out, &buf));
  gst_buffer_unref (buf);

  gst_harness_teardown (h_in);
  gst_harness_teardown (h_out);
}

GST_END_TEST;

static Suite *
proxysink_suite (void)
{
  Suite *s = suite_create ("proxysink");
  TCase *tc_basic = tcase_create ("general");

  suite_add_tcase (s, tc_basic);
  tcase_add_test (tc_basic, test_flush_before_buffer);

  return s;
}

GST_CHECK_MAIN (proxysink);
