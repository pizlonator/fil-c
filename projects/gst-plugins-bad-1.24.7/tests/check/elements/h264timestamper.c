/*
 * GStreamer
 *
 * unit test for h264timestamper
 *
 * Copyright (C) 2022 Matthew Waters <matthew@centricular.com>
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

#include <gst/check/check.h>
#include <gst/video/video.h>
/* SPS */
static guint8 h264_sps[] = {
  0x00, 0x00, 0x00, 0x01, 0x67, 0x4d, 0x40, 0x15,
  0xec, 0xa4, 0xbf, 0x2e, 0x02, 0x20, 0x00, 0x00,
  0x03, 0x00, 0x2e, 0xe6, 0xb2, 0x80, 0x01, 0xe2,
  0xc5, 0xb2, 0xc0
};

/* PPS */
static guint8 h264_pps[] = {
  0x00, 0x00, 0x00, 0x01, 0x68, 0xeb, 0xec, 0xb2
};

/* keyframes all around */
static guint8 h264_idrframe[] = {
  0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00,
  0x10, 0xff, 0xfe, 0xf6, 0xf0, 0xfe, 0x05, 0x36,
  0x56, 0x04, 0x50, 0x96, 0x7b, 0x3f, 0x53, 0xe1
};

static GstBuffer *
create_keyframe_with_sps_pps (void)
{
  gsize size =
      G_N_ELEMENTS (h264_sps) + G_N_ELEMENTS (h264_pps) +
      G_N_ELEMENTS (h264_idrframe);
  GstBuffer *buffer = gst_buffer_new_allocate (NULL, size, NULL);
  GstMapInfo map_info;
  gsize offset = 0;

  g_assert (gst_buffer_map (buffer, &map_info, GST_MAP_WRITE));
  memcpy (&map_info.data[offset], h264_sps, G_N_ELEMENTS (h264_sps));
  offset += G_N_ELEMENTS (h264_sps);
  memcpy (&map_info.data[offset], h264_pps, G_N_ELEMENTS (h264_pps));
  offset += G_N_ELEMENTS (h264_pps);
  memcpy (&map_info.data[offset], h264_idrframe, G_N_ELEMENTS (h264_idrframe));
  offset += G_N_ELEMENTS (h264_idrframe);

  gst_buffer_unmap (buffer, &map_info);

  return buffer;
}

GST_START_TEST (test_input_dts_none)
{
  GstHarness *h = gst_harness_new ("h264timestamper");
  GstBuffer *buffer;
  int i;

  gst_harness_set_src_caps_str (h,
      "video/x-h264,stream-format=byte-stream,alignment=au");
  gst_harness_set_sink_caps_str (h,
      "video/x-h264,stream-format=byte-stream,alignment=au");

  buffer = create_keyframe_with_sps_pps ();
  GST_BUFFER_PTS (buffer) = 0;
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);;
  buffer = gst_buffer_new_memdup (h264_idrframe, G_N_ELEMENTS (h264_idrframe));
  GST_BUFFER_PTS (buffer) = 1 * GST_MSECOND;
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);;
  buffer = gst_buffer_new_memdup (h264_idrframe, G_N_ELEMENTS (h264_idrframe));
  GST_BUFFER_PTS (buffer) = 2 * GST_MSECOND;
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);;
  buffer = gst_buffer_new_memdup (h264_idrframe, G_N_ELEMENTS (h264_idrframe));
  GST_BUFFER_PTS (buffer) = 3 * GST_MSECOND;
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);;
  buffer = gst_buffer_new_memdup (h264_idrframe, G_N_ELEMENTS (h264_idrframe));
  GST_BUFFER_PTS (buffer) = 4 * GST_MSECOND;
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);;

  gst_harness_push_event (h, gst_event_new_eos ());

  for (i = 0; i < 5; i++) {
    buffer = gst_harness_pull (h);
    fail_unless (buffer != NULL);
    fail_unless (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_PTS (buffer)));
    fail_unless (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_DTS (buffer)));
    fail_unless (GST_BUFFER_PTS (buffer) >= GST_BUFFER_DTS (buffer));
    gst_buffer_unref (buffer);
  }

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_input_pts_none)
{
  GstHarness *h = gst_harness_new ("h264timestamper");
  GstBuffer *buffer;
  int i;

  gst_harness_set_src_caps_str (h,
      "video/x-h264,stream-format=byte-stream,alignment=au");
  gst_harness_set_sink_caps_str (h,
      "video/x-h264,stream-format=byte-stream,alignment=au");

  buffer = create_keyframe_with_sps_pps ();
  GST_BUFFER_PTS (buffer) = 0;
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);;
  buffer = gst_buffer_new_memdup (h264_idrframe, G_N_ELEMENTS (h264_idrframe));
  GST_BUFFER_PTS (buffer) = GST_CLOCK_TIME_NONE;
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);;
  buffer = gst_buffer_new_memdup (h264_idrframe, G_N_ELEMENTS (h264_idrframe));
  GST_BUFFER_PTS (buffer) = 2 * GST_MSECOND;
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);;
  buffer = gst_buffer_new_memdup (h264_idrframe, G_N_ELEMENTS (h264_idrframe));
  GST_BUFFER_PTS (buffer) = GST_CLOCK_TIME_NONE;
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);;
  buffer = gst_buffer_new_memdup (h264_idrframe, G_N_ELEMENTS (h264_idrframe));
  GST_BUFFER_PTS (buffer) = 4 * GST_MSECOND;
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);;

  gst_harness_push_event (h, gst_event_new_eos ());

  for (i = 0; i < 5; i++) {
    buffer = gst_harness_pull (h);
    fail_unless (buffer != NULL);
    fail_unless (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_PTS (buffer)));
    fail_unless (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_DTS (buffer)));
    fail_unless (GST_BUFFER_PTS (buffer) >= GST_BUFFER_DTS (buffer));
    gst_buffer_unref (buffer);
  }

  gst_harness_teardown (h);
}

GST_END_TEST;
static Suite *
h264timestamper_suite (void)
{
  Suite *s = suite_create ("h264timestamper");
  TCase *tc = tcase_create ("general");

  tcase_add_test (tc, test_input_dts_none);
  tcase_add_test (tc, test_input_pts_none);

  suite_add_tcase (s, tc);

  return s;
}

GST_CHECK_MAIN (h264timestamper);
