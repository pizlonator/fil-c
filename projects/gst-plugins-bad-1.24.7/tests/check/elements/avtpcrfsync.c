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
#include "../../../ext/avtp/gstavtpcrfsync.c"
#undef GST_CAT_DEFAULT

#include <avtp.h>
#include <avtp_crf.h>
#include <avtp_aaf.h>
#include <avtp_cvf.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include "../../../ext/avtp/gstavtpcrfutil.h"

#define STREAM_ID 0xDEADC0DEDEADC0DE

struct buffer_tstamps
{
  GstClockTime buf_pts;
  GstClockTime buf_dts;
  GstClockTime avtp_ts;
  GstClockTime h264_ts;
};

static GstHarness *
setup_harness (void)
{
  GstHarness *h;

  h = gst_harness_new_parse ("avtpcrfsync streamid=0xDEADC0DEDEADC0DE");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  return h;
}

static void
fill_buffer_video_data (struct avtp_stream_pdu *pdu)
{
  const gint DATA_LEN = sizeof (guint32) + 3;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
}

static void
fill_buffer_audio_data (struct avtp_stream_pdu *pdu)
{
  const int DATA_LEN = 4;

  avtp_aaf_pdu_init (pdu);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_TV, 1);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_STREAM_ID, 0xDEADC0DEDEADC0DE);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_FORMAT, AVTP_AAF_FORMAT_INT_16BIT);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_NSR, AVTP_AAF_PCM_NSR_48KHZ);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_CHAN_PER_FRAME, 2);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_BIT_DEPTH, 16);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_SP, AVTP_AAF_PCM_SP_NORMAL);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_TIMESTAMP, 0);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_STREAM_DATA_LEN, DATA_LEN);
}

static GstBuffer *
create_input_buffer (GstHarness * h, guint32 subtype)
{
  struct avtp_stream_pdu *pdu;
  GstMapInfo info;
  GstBuffer *buf;
  const gint DATA_LEN = sizeof (guint32) + 3;

  buf = gst_harness_create_buffer (h, sizeof (struct avtp_stream_pdu) +
      DATA_LEN);

  gst_buffer_map (buf, &info, GST_MAP_WRITE);
  pdu = (struct avtp_stream_pdu *) info.data;

  if (subtype == AVTP_SUBTYPE_AAF)
    fill_buffer_audio_data (pdu);
  else if (subtype == AVTP_SUBTYPE_CVF)
    fill_buffer_video_data (pdu);

  gst_buffer_unmap (buf, &info);

  return buf;
}

static void
set_buffer_tstamps (GstBuffer * buf, struct buffer_tstamps *orig)
{
  struct avtp_stream_pdu *pdu;
  GstMapInfo info;
  guint32 type;
  gint r;

  gst_buffer_map (buf, &info, GST_MAP_WRITE);
  pdu = (struct avtp_stream_pdu *) info.data;

  GST_BUFFER_PTS (buf) = orig->buf_pts;
  GST_BUFFER_DTS (buf) = orig->buf_dts;

  r = avtp_pdu_get ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE, &type);
  g_assert (r == 0);
  if (type == AVTP_SUBTYPE_AAF)
    avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_TIMESTAMP, orig->avtp_ts);
  else if (type == AVTP_SUBTYPE_CVF) {
    avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, orig->avtp_ts);
    avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, orig->h264_ts);
  }
  gst_buffer_unmap (buf, &info);
}

static void
validate_tstamps (GstAvtpCrfBase * avtpcrfbase, GstBuffer * buf,
    struct buffer_tstamps *expected)
{
  GstClockTime tstamp;
  struct avtp_stream_pdu *pdu;
  GstMapInfo info;
  int res;

  gst_buffer_map (buf, &info, GST_MAP_READ);
  pdu = (struct avtp_stream_pdu *) info.data;

  fail_unless_equals_uint64 (GST_BUFFER_PTS (buf), expected->buf_pts);
  fail_unless_equals_uint64 (GST_BUFFER_DTS (buf), expected->buf_dts);

  tstamp = get_avtp_tstamp (avtpcrfbase, pdu);
  fail_unless_equals_uint64 (tstamp, expected->avtp_ts);

  if (h264_tstamp_valid (pdu)) {
    res = avtp_cvf_pdu_get (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, &tstamp);
    fail_unless (res == 0);
    fail_unless_equals_uint64 (tstamp, expected->h264_ts);
  }

  gst_buffer_unmap (buf, &info);
}

static void
test_crf_tstamps (GstHarness * h, GstBuffer * buf, struct buffer_tstamps *orig,
    struct buffer_tstamps *expected)
{
  GstAvtpCrfBase *avtpcrfbase;
  GstBuffer *bufout;

  avtpcrfbase = (GstAvtpCrfBase *) gst_harness_find_element (h, "avtpcrfsync");
  set_buffer_tstamps (buf, orig);

  bufout = gst_harness_push_and_pull (h, buf);

  validate_tstamps (avtpcrfbase, bufout, expected);

  gst_object_unref (avtpcrfbase);
}

GST_START_TEST (test_properties)
{
  const guint64 streamid = 0xAABBCCDDEEFF0001;
  const gchar *address = "01:AA:BB:CC:DD:EE";
  const gchar *ifname = "enp1s0";
  GstElement *element;
  guint64 val64;
  gchar *str;

  element = gst_check_setup_element ("avtpcrfsync");

  g_object_set (G_OBJECT (element), "ifname", ifname, NULL);
  g_object_get (G_OBJECT (element), "ifname", &str, NULL);
  fail_unless_equals_string (str, ifname);
  g_free (str);

  g_object_set (G_OBJECT (element), "address", address, NULL);
  g_object_get (G_OBJECT (element), "address", &str, NULL);
  fail_unless_equals_string (str, address);
  g_free (str);

  g_object_set (G_OBJECT (element), "streamid", streamid, NULL);
  g_object_get (G_OBJECT (element), "streamid", &val64, NULL);
  fail_unless_equals_uint64_hex (val64, streamid);

  gst_object_unref (element);
}

GST_END_TEST;

GST_START_TEST (test_set_avtp_tstamp)
{
  GstAvtpCrfSync *avtpcrfsync = g_object_new (GST_TYPE_AVTP_CRF_SYNC, NULL);
  struct avtp_stream_pdu pdu;
  GstClockTime tstamp;
  int res;

  avtp_aaf_pdu_init (&pdu);
  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_TV, 1);
  set_avtp_tstamp (avtpcrfsync, &pdu, 12345);
  res = avtp_aaf_pdu_get (&pdu, AVTP_AAF_FIELD_TIMESTAMP, &tstamp);
  fail_unless (res == 0);
  fail_unless_equals_uint64 (tstamp, 12345);

  avtp_cvf_pdu_init (&pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (&pdu, AVTP_CVF_FIELD_TV, 1);
  set_avtp_tstamp (avtpcrfsync, &pdu, 12345);
  res = avtp_cvf_pdu_get (&pdu, AVTP_CVF_FIELD_TIMESTAMP, &tstamp);
  fail_unless (res == 0);
  fail_unless_equals_uint64 (tstamp, 12345);

  g_object_unref (avtpcrfsync);
}

GST_END_TEST;

GST_START_TEST (test_set_avtp_mr_bit)
{
  GstAvtpCrfSync *avtpcrfsync = g_object_new (GST_TYPE_AVTP_CRF_SYNC, NULL);
  struct avtp_stream_pdu pdu;
  guint64 mr_bit;
  int res;

  avtp_aaf_pdu_init (&pdu);
  set_avtp_mr_bit (avtpcrfsync, &pdu, 1);
  res = avtp_aaf_pdu_get (&pdu, AVTP_AAF_FIELD_MR, &mr_bit);
  fail_unless (res == 0);
  fail_unless_equals_uint64 (mr_bit, 1);

  avtp_cvf_pdu_init (&pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  set_avtp_mr_bit (avtpcrfsync, &pdu, 1);
  res = avtp_cvf_pdu_get (&pdu, AVTP_CVF_FIELD_MR, &mr_bit);
  fail_unless (res == 0);
  fail_unless_equals_uint64 (mr_bit, 1);

  g_object_unref (avtpcrfsync);
}

GST_END_TEST;

GST_START_TEST (test_crf_cvf_data)
{
  struct buffer_tstamps orig, expected;
  GstAvtpCrfBase *avtpcrfbase;
  GstBuffer *buf;
  GstHarness *h;

  h = setup_harness ();

  buf = create_input_buffer (h, AVTP_SUBTYPE_CVF);
  avtpcrfbase = (GstAvtpCrfBase *) gst_harness_find_element (h, "avtpcrfsync");
  avtpcrfbase->thread_data.average_period = 3300;
  avtpcrfbase->thread_data.current_ts = 110000;
  gst_object_unref (avtpcrfbase);

  orig = (struct buffer_tstamps) {
    .buf_pts = 103000,.buf_dts = 100000,.avtp_ts = 110000,.h264_ts = 108000
  };
  expected = (struct buffer_tstamps) {
    .buf_pts = 104204,.buf_dts = 100000,.avtp_ts = 110000,.h264_ts = 109204
  };
  test_crf_tstamps (h, buf, &orig, &expected);

  orig = (struct buffer_tstamps) {
    .buf_pts = 107000,.buf_dts = 105000,.avtp_ts = 113000,.h264_ts = 118500
  };
  expected = (struct buffer_tstamps) {
    .buf_pts = 108400,.buf_dts = 105300,.avtp_ts = 113300,.h264_ts = 119900
  };
  test_crf_tstamps (h, buf, &orig, &expected);

  /* test for invalid DTS */
  orig = (struct buffer_tstamps) {
    .buf_pts = 107000,.buf_dts = GST_CLOCK_TIME_NONE,.avtp_ts =
        113000,.h264_ts = 118500
  };
  expected = (struct buffer_tstamps) {
    .buf_pts = 108400,.buf_dts = GST_CLOCK_TIME_NONE,.avtp_ts =
        113300,.h264_ts = 119900
  };
  test_crf_tstamps (h, buf, &orig, &expected);

  gst_buffer_unref (buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_crf_aaf_data)
{
  struct buffer_tstamps orig, expected;
  GstAvtpCrfBase *avtpcrfbase;
  GstBuffer *buf;
  GstHarness *h;

  h = setup_harness ();

  buf = create_input_buffer (h, AVTP_SUBTYPE_AAF);
  avtpcrfbase = (GstAvtpCrfBase *) gst_harness_find_element (h, "avtpcrfsync");
  avtpcrfbase->thread_data.average_period = 3300;
  avtpcrfbase->thread_data.current_ts = 110000;
  gst_object_unref (avtpcrfbase);

  orig = (struct buffer_tstamps) {
    .buf_pts = 108000,.buf_dts = 0,.avtp_ts = 110000,.h264_ts = 0
  };
  expected = (struct buffer_tstamps) {
    .buf_pts = 108000,.buf_dts = 0,.avtp_ts = 110000,.h264_ts = 0
  };
  test_crf_tstamps (h, buf, &orig, &expected);

  orig = (struct buffer_tstamps) {
    .buf_pts = 110000,.buf_dts = 0,.avtp_ts = 113000,.h264_ts = 0
  };
  expected = (struct buffer_tstamps) {
    .buf_pts = 110300,.buf_dts = 0,.avtp_ts = 113300,.h264_ts = 0
  };
  test_crf_tstamps (h, buf, &orig, &expected);

  gst_buffer_unref (buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_crf_period_zero)
{
  struct buffer_tstamps orig, expected;
  GstAvtpCrfBase *avtpcrfbase;
  GstBuffer *buf;
  GstHarness *h;

  h = setup_harness ();

  buf = create_input_buffer (h, AVTP_SUBTYPE_CVF);
  avtpcrfbase = (GstAvtpCrfBase *) gst_harness_find_element (h, "avtpcrfsync");
  avtpcrfbase->thread_data.average_period = 0.0;
  avtpcrfbase->thread_data.current_ts = 110;
  gst_object_unref (avtpcrfbase);

  orig = (struct buffer_tstamps) {
    .buf_pts = 100,.buf_dts = 105,.avtp_ts = 112,.h264_ts = 110
  };
  expected = (struct buffer_tstamps) {
    .buf_pts = 100,.buf_dts = 105,.avtp_ts = 112,.h264_ts = 110
  };
  test_crf_tstamps (h, buf, &orig, &expected);

  gst_buffer_unref (buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
avtpcrfsync_suite (void)
{
  Suite *s = suite_create ("avtpcrfsync");
  TCase *tc_chain = tcase_create ("general");

  GST_DEBUG_CATEGORY_INIT (avtpcrfsync_debug, "avtpcrfsync", 0,
      "CRF Synchronizer");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_properties);
  tcase_add_test (tc_chain, test_set_avtp_tstamp);
  tcase_add_test (tc_chain, test_set_avtp_mr_bit);
  tcase_add_test (tc_chain, test_crf_cvf_data);
  tcase_add_test (tc_chain, test_crf_aaf_data);
  tcase_add_test (tc_chain, test_crf_period_zero);

  return s;
}

GST_CHECK_MAIN (avtpcrfsync);
