/* GStreamer
 * Copyright (C) 2019 Yeongjin Jeong <yeongjin.jeong@navercorp.com>
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

#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

GST_START_TEST (test_encode_simple)
{
  GstHarness *h;
  GstCaps *outcaps, *caps;
  gint i = 0;

  h = gst_harness_new_parse ("svthevcenc speed=9 bitrate=1000 ! h265parse");

  gst_harness_add_src_parse (h, "videotestsrc is-live=true ! "
      "capsfilter caps=\"video/x-raw,format=I420,width=320,height=240,framerate=25/1\"",
      TRUE);

  /* Push 25 buffers into the encoder */
  fail_unless_equals_int (GST_FLOW_OK,
      gst_harness_src_crank_and_push_many (h, 25, 25));

  /* EOS will cause the remaining buffers to be drained */
  fail_unless (gst_harness_push_event (h, gst_event_new_eos ()));
  fail_unless_equals_int (gst_harness_buffers_received (h), 25);

  outcaps =
      gst_caps_from_string
      ("video/x-h265,width=(int)320,height=(int)240,framerate=(fraction)25/1");

  caps = gst_pad_get_current_caps (h->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps, outcaps));

  for (i = 0; i < 25; i++) {
    GstBuffer *buffer = gst_harness_pull (h);

    fail_unless_equals_uint64 (GST_BUFFER_DURATION (buffer),
        gst_util_uint64_scale (1, GST_SECOND, 25));

    gst_buffer_unref (buffer);
  }
  gst_caps_unref (outcaps);
  gst_caps_unref (caps);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_reuse)
{
  GstHarness *h;
  GstCaps *srccaps, *outcaps, *caps;
  GstBuffer *in_buf = NULL;
  GstFlowReturn ret;
  GstSegment seg;
  gint loop, i;

  h = gst_harness_new_parse ("svthevcenc speed=9 bitrate=1000");

  srccaps =
      gst_caps_from_string
      ("video/x-raw,format=I420,width=(int)320,height=(int)240,framerate=(fraction)25/1");
  outcaps =
      gst_caps_from_string
      ("video/x-h265,width=(int)320,height=(int)240,framerate=(fraction)25/1");

  in_buf = gst_buffer_new_and_alloc ((320 * 240) * 3 / 2);
  gst_buffer_memset (in_buf, 0, 0, -1);

  GST_BUFFER_DURATION (in_buf) = gst_util_uint64_scale (1, GST_SECOND, 25);
  GST_BUFFER_DTS (in_buf) = GST_CLOCK_TIME_NONE;

  gst_segment_init (&seg, GST_FORMAT_TIME);

  for (loop = 0; loop < 2; loop++) {
    gst_harness_play (h);

    fail_unless (gst_harness_push_event (h,
            gst_event_new_stream_start ("test")));
    fail_unless (gst_harness_push_event (h, gst_event_new_caps (srccaps)));
    fail_unless (gst_harness_push_event (h, gst_event_new_segment (&seg)));

    for (i = 0; i < 25; i++) {
      GST_BUFFER_PTS (in_buf) = gst_util_uint64_scale (i, GST_SECOND, 25);

      ret = gst_harness_push (h, gst_buffer_ref (in_buf));
      fail_unless (ret == GST_FLOW_OK, "GstFlowReturn was %s",
          gst_flow_get_name (ret));
    }

    /* EOS will cause the remaining buffers to be drained */
    fail_unless (gst_harness_push_event (h, gst_event_new_eos ()));
    fail_unless_equals_int (gst_harness_buffers_received (h), (loop + 1) * 25);

    caps = gst_pad_get_current_caps (h->sinkpad);
    fail_unless (caps != NULL);
    fail_unless (gst_caps_can_intersect (caps, outcaps));

    for (i = 0; i < 25; i++) {
      GstBuffer *buffer = gst_harness_pull (h);

      fail_unless_equals_uint64 (GST_BUFFER_DURATION (buffer),
          gst_util_uint64_scale (1, GST_SECOND, 25));

      gst_buffer_unref (buffer);
    }
    gst_caps_unref (caps);

    ASSERT_SET_STATE (h->element, GST_STATE_READY, GST_STATE_CHANGE_SUCCESS);
  }
  gst_caps_unref (srccaps);
  gst_caps_unref (outcaps);
  gst_buffer_unref (in_buf);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_no_encoding)
{
  GstHarness *h;
  GstCaps *caps;
  GstEvent *event;

  h = gst_harness_new_parse ("svthevcenc");
  fail_unless (h != NULL);

  gst_harness_play (h);

  caps = gst_caps_from_string
      ("video/x-raw,format=I420,width=(int)320,height=(int)240,framerate=(fraction)25/1");
  gst_harness_set_src_caps (h, caps);

  /* Check if draining are performed well without a buffer push */
  fail_unless (gst_harness_push_event (h, gst_event_new_eos ()));

  do {
    gboolean term = FALSE;
    event = gst_harness_pull_event (h);
    /* wait until get EOS event */
    if (event) {
      if (GST_EVENT_TYPE (event) == GST_EVENT_EOS)
        term = TRUE;

      gst_event_unref (event);

      if (term)
        break;
    }
  } while (TRUE);

  gst_harness_teardown (h);
}

GST_END_TEST;

#define MAX_PUSH_BUFFER 300

GST_START_TEST (test_reconfigure)
{
  GstHarness *h;
  GstElement *svthevcenc;
  GstCaps *caps;
  GstEvent *event;
  GstBuffer *in_buf, *out_buf = NULL;
  GstFlowReturn ret;
  gint i = 0;

  h = gst_harness_new_parse ("svthevcenc ! h265parse");
  fail_unless (h != NULL);

  svthevcenc = gst_harness_find_element (h, "svthevcenc");
  g_object_set (svthevcenc, "speed", 9, NULL);

  gst_harness_play (h);

  caps = gst_caps_from_string
      ("video/x-raw,format=I420,width=(int)320,height=(int)240,framerate=(fraction)25/1");
  gst_harness_set_src_caps (h, caps);

  in_buf = gst_buffer_new_and_alloc ((320 * 240) * 3 / 2);
  gst_buffer_memset (in_buf, 0, 0, -1);

  GST_BUFFER_DURATION (in_buf) = GST_SECOND;
  GST_BUFFER_DTS (in_buf) = GST_CLOCK_TIME_NONE;

  /* Push bufffers until get encoder output */
  do {
    fail_if (i > MAX_PUSH_BUFFER);

    GST_BUFFER_PTS (in_buf) = i * GST_SECOND;

    ret = gst_harness_push (h, gst_buffer_ref (in_buf));
    fail_unless (ret == GST_FLOW_OK, "GstFlowReturn was %s",
        gst_flow_get_name (ret));

    out_buf = gst_harness_try_pull (h);
    i++;
  } while (out_buf == NULL);
  gst_buffer_unref (out_buf);

  /* Change property for reconfig */
  g_object_set (svthevcenc, "speed", 8, NULL);

  /* Push bufffers until get encoder output */
  do {
    fail_if (i > 2 * MAX_PUSH_BUFFER);

    GST_BUFFER_PTS (in_buf) = i * GST_SECOND;

    ret = gst_harness_push (h, gst_buffer_ref (in_buf));
    fail_unless (ret == GST_FLOW_OK, "GstFlowReturn was %s",
        gst_flow_get_name (ret));

    out_buf = gst_harness_try_pull (h);
    i++;
  } while (out_buf == NULL);
  gst_buffer_unref (out_buf);
  gst_buffer_unref (in_buf);

  /* push EOS to drain all buffers */
  fail_unless (gst_harness_push_event (h, gst_event_new_eos ()));

  do {
    gboolean term = FALSE;
    event = gst_harness_pull_event (h);
    /* wait until get EOS event */
    if (event) {
      if (GST_EVENT_TYPE (event) == GST_EVENT_EOS)
        term = TRUE;

      gst_event_unref (event);

      if (term)
        break;
    }
  } while (out_buf == NULL);

  gst_object_unref (svthevcenc);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
svthevcenc_suite (void)
{
  Suite *s = suite_create ("svthevcenc");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_encode_simple);
  tcase_add_test (tc_chain, test_reuse);
  tcase_add_test (tc_chain, test_no_encoding);
  tcase_add_test (tc_chain, test_reconfigure);

  return s;
}

GST_CHECK_MAIN (svthevcenc);
