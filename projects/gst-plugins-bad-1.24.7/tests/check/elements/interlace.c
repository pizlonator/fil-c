/* GStreamer
 * unit test for interlace
 *
 * Copyright (C) 2021 Vivia Nikolaidou <vivia at ahiru dot eu>
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
#include <gst/video/video.h>

GST_START_TEST (test_passthrough)
{
  GstBuffer *buffer;
  GstHarness *h;

  h = gst_harness_new ("interlace");

  gst_harness_set (h, "interlace", "field-pattern", 1, "top-field-first", TRUE,
      NULL);
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=interleaved,field-order=top-field-first,format=AYUV,height=1,width=1,framerate=1/1");
  buffer = gst_harness_create_buffer (h, 4);
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);

  gst_harness_set (h, "interlace", "field-pattern", 1, "top-field-first", FALSE,
      NULL);
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=interleaved,field-order=bottom-field-first,format=AYUV,width=1,height=1,framerate=1/1");
  buffer = gst_harness_create_buffer (h, 4);
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_reject_passthrough_mixed)
{
  GstHarness *h;
  GstBuffer *buffer;

  h = gst_harness_new ("interlace");
  gst_harness_play (h);

  gst_harness_set (h, "interlace", "field-pattern", 3, NULL);
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=mixed,format=AYUV,width=1,height=1,framerate=1/1");
  buffer = gst_harness_create_buffer (h, 4);
  fail_unless_equals_int (gst_harness_push (h, buffer),
      GST_FLOW_NOT_NEGOTIATED);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_field_switch)
{
  GstHarness *h;
  GstBuffer *buffer;

  h = gst_harness_new ("interlace");

  gst_harness_set (h, "interlace", "field-pattern", 1, "top-field-first", FALSE,
      NULL);
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=interleaved,field-order=top-field-first,format=AYUV,width=1,height=1,framerate=1/1");
  buffer = gst_harness_create_buffer (h, 4);
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);

  gst_harness_set (h, "interlace", "field-pattern", 1, "top-field-first", TRUE,
      NULL);
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=interleaved,field-order=bottom-field-first,format=AYUV,width=1,height=1,framerate=1/1");
  buffer = gst_harness_create_buffer (h, 4);
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_framerate_2_2)
{
  GstHarness *h;
  GstBuffer *buffer;

  h = gst_harness_new ("interlace");

  gst_harness_set (h, "interlace", "field-pattern", 1, "top-field-first", TRUE,
      NULL);
  gst_harness_set_sink_caps_str (h, "video/x-raw,framerate=1/1");
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=progressive,format=AYUV,width=1,height=1,framerate=1/1");
  buffer = gst_harness_create_buffer (h, 4);
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);

  gst_harness_set_sink_caps_str (h, "video/x-raw,framerate=1/1");
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=progressive,format=AYUV,width=1,height=1,framerate=2/1");
  buffer = gst_harness_create_buffer (h, 4);
  fail_unless_equals_int (gst_harness_push (h, buffer),
      GST_FLOW_NOT_NEGOTIATED);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_framerate_1_1)
{
  GstHarness *h;
  GstBuffer *buffer;

  h = gst_harness_new ("interlace");

  gst_harness_set (h, "interlace", "field-pattern", 0, "top-field-first", TRUE,
      NULL);
  gst_harness_set_sink_caps_str (h, "video/x-raw,framerate=1/1");
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=progressive,format=AYUV,width=1,height=1,framerate=1/1");
  buffer = gst_harness_create_buffer (h, 4);
  fail_unless_equals_int (gst_harness_push (h, buffer),
      GST_FLOW_NOT_NEGOTIATED);

  gst_harness_set_sink_caps_str (h, "video/x-raw,framerate=1/1");
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=progressive,format=AYUV,width=1,height=1,framerate=2/1");
  buffer = gst_harness_create_buffer (h, 4);
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_framerate_3_2)
{
  GstHarness *h;
  GstBuffer *buffer;

  h = gst_harness_new ("interlace");

  gst_harness_set (h, "interlace", "field-pattern", 2, NULL);
  gst_harness_set_sink_caps_str (h, "video/x-raw,framerate=30/1");
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=progressive,format=AYUV,width=1,height=1,framerate=24/1");
  buffer = gst_harness_create_buffer (h, 4);
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);

  gst_harness_set_sink_caps_str (h, "video/x-raw,framerate=1/1");
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=progressive,format=AYUV,width=1,height=1,framerate=1/1");
  buffer = gst_harness_create_buffer (h, 4);
  fail_unless_equals_int (gst_harness_push (h, buffer),
      GST_FLOW_NOT_NEGOTIATED);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_framerate_empty_not_negotiated)
{
  GstHarness *h;
  GstBuffer *buffer;

  h = gst_harness_new ("interlace");

  gst_harness_set_sink_caps_str (h, "EMPTY");
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=progressive,format=AYUV,width=1,height=1,framerate=24/1");
  buffer = gst_harness_create_buffer (h, 4);
  fail_unless_equals_int (gst_harness_push (h, buffer),
      GST_FLOW_NOT_NEGOTIATED);

  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
interlace_suite (void)
{
  Suite *s = suite_create ("interlace");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_passthrough);
  tcase_add_test (tc_chain, test_reject_passthrough_mixed);
  tcase_add_test (tc_chain, test_field_switch);
  tcase_add_test (tc_chain, test_framerate_2_2);
  tcase_add_test (tc_chain, test_framerate_1_1);
  tcase_add_test (tc_chain, test_framerate_3_2);
  tcase_add_test (tc_chain, test_framerate_empty_not_negotiated);

  return s;
}

GST_CHECK_MAIN (interlace);
