/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later
 * version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#include <avtp.h>
#include <avtp_aaf.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

static GstHarness *
setup_harness (void)
{
  GstHarness *h;

  h = gst_harness_new_parse
      ("avtpaafpay streamid=0xDEADC0DEDEADC0DE mtt=1000000 tu=1000000 processing-deadline=1000000 timestamp-mode=normal");
  gst_harness_set_src_caps_str (h,
      "audio/x-raw,format=S16BE,rate=48000,channels=2,layout=interleaved");

  return h;
}

GST_START_TEST (test_buffer)
{
  GstHarness *h;
  GstBuffer *in, *out;
  GstMapInfo info;
  struct avtp_stream_pdu pdu = { 0 };
  const int DATA_LEN = 4;

  avtp_aaf_pdu_init (&pdu);
  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_TV, 1);
  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_STREAM_ID, 0xDEADC0DEDEADC0DE);
  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_FORMAT, AVTP_AAF_FORMAT_INT_16BIT);
  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_NSR, AVTP_AAF_PCM_NSR_48KHZ);
  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_CHAN_PER_FRAME, 2);
  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_BIT_DEPTH, 16);
  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_SP, AVTP_AAF_PCM_SP_NORMAL);
  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_TIMESTAMP, 4000000);
  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_STREAM_DATA_LEN, DATA_LEN);

  h = setup_harness ();
  in = gst_harness_create_buffer (h, DATA_LEN);
  GST_BUFFER_PTS (in) = 1000000;
  out = gst_harness_push_and_pull (h, in);

  fail_unless (gst_buffer_get_size (out) ==
      sizeof (struct avtp_stream_pdu) + DATA_LEN);
  fail_unless (GST_BUFFER_PTS (in) == GST_BUFFER_PTS (out));
  gst_buffer_map (out, &info, GST_MAP_READ);
  fail_unless (memcmp (info.data, &pdu, sizeof (struct avtp_stream_pdu)) == 0);
  gst_buffer_unmap (out, &info);

  gst_buffer_unref (out);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_properties)
{
  GstHarness *h;
  guint val_uint;
  guint64 val_uint64;
  GstElement *element;
  const guint64 streamid = 0xAABBCCDDEEFF0001;
  const guint64 processing_deadline = 20000000;
  const guint tstamp_mode = 0;
  const guint mtt = 11111111;
  const guint tu = 22222222;

  h = setup_harness ();
  element = gst_harness_find_element (h, "avtpaafpay");

  g_object_set (G_OBJECT (element), "streamid", streamid, NULL);
  g_object_get (G_OBJECT (element), "streamid", &val_uint64, NULL);
  fail_unless (val_uint64 == streamid);

  g_object_set (G_OBJECT (element), "mtt", mtt, NULL);
  g_object_get (G_OBJECT (element), "mtt", &val_uint, NULL);
  fail_unless (val_uint == mtt);

  g_object_set (G_OBJECT (element), "tu", tu, NULL);
  g_object_get (G_OBJECT (element), "tu", &val_uint, NULL);
  fail_unless (val_uint == tu);

  g_object_set (G_OBJECT (element), "timestamp-mode", tstamp_mode, NULL);
  g_object_get (G_OBJECT (element), "timestamp-mode", &val_uint, NULL);
  fail_unless (val_uint == tstamp_mode);

  g_object_set (G_OBJECT (element), "processing-deadline", processing_deadline,
      NULL);
  g_object_get (G_OBJECT (element), "processing-deadline", &val_uint64, NULL);
  fail_unless (val_uint64 == processing_deadline);

  gst_object_unref (element);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
avtpaafpay_suite (void)
{
  Suite *s = suite_create ("avtpaafpay");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_buffer);
  tcase_add_test (tc_chain, test_properties);

  return s;
}

GST_CHECK_MAIN (avtpaafpay);
