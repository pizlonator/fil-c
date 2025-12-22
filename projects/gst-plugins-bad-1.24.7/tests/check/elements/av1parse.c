/*
 * GStreamer
 *
 * unit test for h265parse
 *
 *  Copyright (C) 2020 Intel Corporation
 *     Author: He Junyan <junyan.he@intel.com>
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
#include <gst/check/gstharness.h>
#include "av1parse.h"
#include <string.h>

static void
check_caps_event (GstHarness * h)
{
  GstEvent *event;
  GstCaps *caps = NULL;
  GstStructure *s;
  const gchar *profile;
  gint width, height;
  guint depth;

  while ((event = gst_harness_try_pull_event (h))) {
    GstCaps *event_caps;
    if (GST_EVENT_TYPE (event) != GST_EVENT_CAPS) {
      gst_event_unref (event);
      continue;
    }

    gst_event_parse_caps (event, &event_caps);
    gst_caps_replace (&caps, event_caps);
    gst_event_unref (event);
  }

  fail_unless (caps != NULL);
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_get_int (s, "width", &width));
  fail_unless (gst_structure_get_int (s, "height", &height));
  fail_unless ((profile = gst_structure_get_string (s, "profile")));
  fail_unless (gst_structure_get_uint (s, "bit-depth-chroma", &depth));

  fail_unless_equals_int (width, 400);
  fail_unless_equals_int (height, 300);
  fail_unless_equals_int (depth, 8);
  fail_unless_equals_string (profile, "main");
  gst_caps_unref (caps);
}

GST_START_TEST (test_byte_to_frame)
{
  GstHarness *h;
  GstBuffer *in_buf, *out_buf = NULL;
  GstMapInfo map;
  GstFlowReturn ret;
  gint i = 0;
  guint offset;
  guint len;
  guint output_buf_num;

  h = gst_harness_new_parse ("av1parse");
  fail_unless (h != NULL);

  gst_harness_set_sink_caps_str (h, "video/x-av1,parsed=(boolean)true,"
      "alignment=(string)frame,stream-format=(string)obu-stream");
  gst_harness_set_src_caps_str (h, "video/x-av1");

  gst_harness_play (h);

  output_buf_num = 0;
  offset = 0;
  len = stream_no_annexb_av1_len / 5;
  for (i = 0; i < 5; i++) {
    if (i == 4)
      len = stream_no_annexb_av1_len - offset;

    in_buf = gst_buffer_new_and_alloc (len);
    gst_buffer_map (in_buf, &map, GST_MAP_WRITE);
    memcpy (map.data, stream_no_annexb_av1 + offset, len);
    gst_buffer_unmap (in_buf, &map);
    offset += len;

    ret = gst_harness_push (h, in_buf);
    fail_unless (ret == GST_FLOW_OK, "GstFlowReturn was %s",
        gst_flow_get_name (ret));

    gst_clear_buffer (&out_buf);
    while ((out_buf = gst_harness_try_pull (h)) != NULL) {
      if (output_buf_num == 0)
        check_caps_event (h);

      fail_unless (gst_buffer_get_size (out_buf) ==
          stream_av1_frame_size[output_buf_num]);

      gst_clear_buffer (&out_buf);
      output_buf_num++;
    }
  }

  fail_unless (output_buf_num == 14);

  gst_harness_teardown (h);

}

GST_END_TEST;

GST_START_TEST (test_byte_to_annexb)
{
  GstHarness *h;
  GstBuffer *in_buf, *out_buf = NULL;
  GstMapInfo map;
  GstFlowReturn ret;
  gint i = 0;
  guint offset;
  guint len;
  guint output_buf_num;

  h = gst_harness_new_parse ("av1parse");
  fail_unless (h != NULL);

  gst_harness_set_sink_caps_str (h, "video/x-av1,parsed=(boolean)true,"
      "alignment=(string)tu,stream-format=(string)annexb");
  gst_harness_set_src_caps_str (h, "video/x-av1,alignment=(string)byte");

  gst_harness_play (h);

  output_buf_num = 0;
  offset = 0;
  len = stream_no_annexb_av1_len / 5;
  for (i = 0; i < 5; i++) {
    if (i == 4)
      len = stream_no_annexb_av1_len - offset;

    in_buf = gst_buffer_new_and_alloc (len);
    gst_buffer_map (in_buf, &map, GST_MAP_WRITE);
    memcpy (map.data, stream_no_annexb_av1 + offset, len);
    gst_buffer_unmap (in_buf, &map);
    offset += len;

    ret = gst_harness_push (h, in_buf);
    fail_unless (ret == GST_FLOW_OK, "GstFlowReturn was %s",
        gst_flow_get_name (ret));

    gst_clear_buffer (&out_buf);
    while ((out_buf = gst_harness_try_pull (h)) != NULL) {
      if (output_buf_num == 0)
        check_caps_event (h);

      fail_unless (gst_buffer_get_size (out_buf) ==
          stream_annexb_av1_tu_len[output_buf_num]);

      gst_clear_buffer (&out_buf);
      output_buf_num++;
    }
  }

  /* The last TU need EOS */
  fail_unless (gst_harness_push_event (h, gst_event_new_eos ()));
  out_buf = gst_harness_try_pull (h);
  fail_unless (out_buf);
  fail_unless (gst_buffer_get_size (out_buf) ==
      stream_annexb_av1_tu_len[output_buf_num]);
  output_buf_num++;
  gst_clear_buffer (&out_buf);

  fail_unless (output_buf_num == 10);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_annexb_to_frame)
{
  GstHarness *h;
  GstBuffer *in_buf, *out_buf = NULL;
  GstMapInfo map;
  GstFlowReturn ret;
  gint i = 0;
  guint offset;
  guint output_buf_num;

  h = gst_harness_new_parse ("av1parse");
  fail_unless (h != NULL);

  gst_harness_set_sink_caps_str (h, "video/x-av1,parsed=(boolean)true,"
      "alignment=(string)frame,stream-format=(string)obu-stream");
  gst_harness_set_src_caps_str (h, "video/x-av1,alignment=(string)tu,"
      "stream-format=(string)annexb");

  gst_harness_play (h);

  output_buf_num = 0;
  offset = 0;
  for (i = 0; i < G_N_ELEMENTS (stream_annexb_av1_tu_len); i++) {
    in_buf = gst_buffer_new_and_alloc (stream_annexb_av1_tu_len[i]);
    gst_buffer_map (in_buf, &map, GST_MAP_WRITE);
    memcpy (map.data, stream_annexb_av1 + offset, stream_annexb_av1_tu_len[i]);
    gst_buffer_unmap (in_buf, &map);
    offset += stream_annexb_av1_tu_len[i];

    ret = gst_harness_push (h, in_buf);
    fail_unless (ret == GST_FLOW_OK, "GstFlowReturn was %s",
        gst_flow_get_name (ret));

    gst_clear_buffer (&out_buf);
    while ((out_buf = gst_harness_try_pull (h)) != NULL) {
      if (output_buf_num == 0)
        check_caps_event (h);

      fail_unless (gst_buffer_get_size (out_buf) ==
          stream_av1_frame_size[output_buf_num]);

      gst_clear_buffer (&out_buf);
      output_buf_num++;
    }
  }

  fail_unless (output_buf_num == 14);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_annexb_to_obu)
{
  GstHarness *h;
  GstBuffer *in_buf, *out_buf = NULL;
  GstMapInfo map;
  GstFlowReturn ret;
  gint i = 0;
  guint offset;
  guint output_buf_num;

  h = gst_harness_new_parse ("av1parse");
  fail_unless (h != NULL);

  gst_harness_set_sink_caps_str (h, "video/x-av1,parsed=(boolean)true,"
      "alignment=(string)obu");
  gst_harness_set_src_caps_str (h, "video/x-av1,alignment=(string)tu,"
      "stream-format=(string)annexb");

  gst_harness_play (h);

  output_buf_num = 0;
  offset = 0;
  for (i = 0; i < G_N_ELEMENTS (stream_annexb_av1_tu_len); i++) {
    in_buf = gst_buffer_new_and_alloc (stream_annexb_av1_tu_len[i]);
    gst_buffer_map (in_buf, &map, GST_MAP_WRITE);
    memcpy (map.data, stream_annexb_av1 + offset, stream_annexb_av1_tu_len[i]);
    gst_buffer_unmap (in_buf, &map);
    offset += stream_annexb_av1_tu_len[i];

    ret = gst_harness_push (h, in_buf);
    fail_unless (ret == GST_FLOW_OK, "GstFlowReturn was %s",
        gst_flow_get_name (ret));

    gst_clear_buffer (&out_buf);
    while ((out_buf = gst_harness_try_pull (h)) != NULL) {
      if (output_buf_num == 0)
        check_caps_event (h);

      fail_unless (gst_buffer_get_size (out_buf) ==
          stream_av1_obu_size[output_buf_num]);

      gst_clear_buffer (&out_buf);
      output_buf_num++;
    }
  }

  fail_unless (output_buf_num == G_N_ELEMENTS (stream_av1_obu_size));

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_byte_to_obu)
{
  GstHarness *h;
  GstBuffer *in_buf, *out_buf = NULL;
  GstMapInfo map;
  GstFlowReturn ret;
  gint i = 0;
  guint offset;
  guint len;
  guint output_buf_num;

  h = gst_harness_new_parse ("av1parse");
  fail_unless (h != NULL);

  gst_harness_set_sink_caps_str (h, "video/x-av1,parsed=(boolean)true,"
      "alignment=(string)obu,stream-format=(string)obu-stream");
  gst_harness_set_src_caps_str (h, "video/x-av1");

  gst_harness_play (h);

  output_buf_num = 0;
  offset = 0;
  len = stream_no_annexb_av1_len / 5;
  for (i = 0; i < 5; i++) {
    if (i == 4)
      len = stream_no_annexb_av1_len - offset;

    in_buf = gst_buffer_new_and_alloc (len);
    gst_buffer_map (in_buf, &map, GST_MAP_WRITE);
    memcpy (map.data, stream_no_annexb_av1 + offset, len);
    gst_buffer_unmap (in_buf, &map);
    offset += len;

    ret = gst_harness_push (h, in_buf);
    fail_unless (ret == GST_FLOW_OK, "GstFlowReturn was %s",
        gst_flow_get_name (ret));

    gst_clear_buffer (&out_buf);
    while ((out_buf = gst_harness_try_pull (h)) != NULL) {
      if (output_buf_num == 0)
        check_caps_event (h);

      fail_unless (gst_buffer_get_size (out_buf) ==
          stream_av1_obu_size[output_buf_num]);

      gst_clear_buffer (&out_buf);
      output_buf_num++;
    }
  }

  fail_unless (output_buf_num == G_N_ELEMENTS (stream_av1_obu_size));

  gst_harness_teardown (h);

}

GST_END_TEST;

static Suite *
av1parse_suite (void)
{
  Suite *s;
  TCase *tc_chain;

  s = suite_create ("av1parse");
  tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_byte_to_frame);
  tcase_add_test (tc_chain, test_byte_to_annexb);
  tcase_add_test (tc_chain, test_annexb_to_frame);
  tcase_add_test (tc_chain, test_annexb_to_obu);
  tcase_add_test (tc_chain, test_byte_to_obu);

  return s;
}

GST_CHECK_MAIN (av1parse);
