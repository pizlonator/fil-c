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
#include <avtp_crf.h>
#include <avtp_aaf.h>
#include <avtp_cvf.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include "../../../ext/avtp/gstavtpcrfutil.h"

#define STREAM_ID 0xDEADC0DEDEADC0DE

static GstHarness *
setup_harness (void)
{
  GstHarness *h;

  h = gst_harness_new_parse
      ("avtpcrfcheck streamid=0xDEADC0DEDEADC0DE drop_invalid=1");

  if (!h)
    GST_ERROR ("Cannot create harness!");
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
  else
    fill_buffer_video_data (pdu);

  gst_buffer_unmap (buf, &info);

  return buf;
}

static void
set_buffer_tstamps (GstBuffer * buf, GstClockTime avtp_tstamp,
    GstClockTime h264_tstamp)
{
  struct avtp_stream_pdu *pdu;
  GstMapInfo info;
  guint32 type;
  gint r;

  gst_buffer_map (buf, &info, GST_MAP_WRITE);
  pdu = (struct avtp_stream_pdu *) info.data;

  r = avtp_pdu_get ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE, &type);
  g_assert (r == 0);
  if (type == AVTP_SUBTYPE_AAF)
    avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_TIMESTAMP, avtp_tstamp);
  else if (type == AVTP_SUBTYPE_CVF) {
    avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, avtp_tstamp);
    avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, h264_tstamp);
  }
  gst_buffer_unmap (buf, &info);
}

static void
test_crf_tstamps (GstHarness * h, GstBuffer * buf, GstClockTime avtp_tstamp,
    GstClockTime h264_tstamp, guint expected_buffers)
{
  set_buffer_tstamps (buf, avtp_tstamp, h264_tstamp);
  gst_harness_push (h, gst_buffer_copy (buf));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h),
      expected_buffers);
}

GST_START_TEST (test_properties)
{
  const guint64 streamid = 0xAABBCCDDEEFF0001;
  const gchar *address = "01:AA:BB:CC:DD:EE";
  const gchar *ifname = "enp1s0";
  const gboolean drop_invalid = TRUE;
  GstElement *element;
  guint64 val64;
  gboolean val;
  gchar *str;

  element = gst_check_setup_element ("avtpcrfcheck");

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
  fail_unless (val64 == streamid);

  g_object_set (G_OBJECT (element), "drop-invalid", drop_invalid, NULL);
  g_object_get (G_OBJECT (element), "drop-invalid", &val, NULL);
  fail_unless (val == drop_invalid);

  gst_object_unref (element);
}

GST_END_TEST;

GST_START_TEST (test_crf_cvf_data)
{
  GstAvtpCrfBase *avtpcrfbase;
  GstBuffer *buf;
  GstHarness *h;

  h = setup_harness ();

  buf = create_input_buffer (h, AVTP_SUBTYPE_CVF);
  avtpcrfbase = (GstAvtpCrfBase *) gst_harness_find_element (h, "avtpcrfcheck");
  avtpcrfbase->thread_data.average_period = 3300;
  avtpcrfbase->thread_data.current_ts = 110000;
  gst_object_unref (avtpcrfbase);

  test_crf_tstamps (h, buf, 110000, 109204, 1);
  test_crf_tstamps (h, buf, 113600, 119400, 2);
  test_crf_tstamps (h, buf, 218000, 119400, 2);
  test_crf_tstamps (h, buf, 218000, 102000, 2);

  gst_buffer_unref (buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_crf_aaf_data)
{
  GstAvtpCrfBase *avtpcrfbase;
  GstBuffer *buf;
  GstHarness *h;

  h = setup_harness ();

  buf = create_input_buffer (h, AVTP_SUBTYPE_AAF);
  avtpcrfbase = (GstAvtpCrfBase *) gst_harness_find_element (h, "avtpcrfcheck");
  avtpcrfbase->thread_data.average_period = 3300;
  avtpcrfbase->thread_data.current_ts = 110000;
  gst_object_unref (avtpcrfbase);

  test_crf_tstamps (h, buf, 113300, 0, 1);
  test_crf_tstamps (h, buf, 112900, 0, 2);
  test_crf_tstamps (h, buf, 210000, 0, 2);

  gst_buffer_unref (buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_crf_period_zero)
{
  GstAvtpCrfBase *avtpcrfbase;
  GstBuffer *buf;
  GstHarness *h;

  h = setup_harness ();

  buf = create_input_buffer (h, AVTP_SUBTYPE_CVF);
  avtpcrfbase = (GstAvtpCrfBase *) gst_harness_find_element (h, "avtpcrfcheck");
  avtpcrfbase->thread_data.average_period = 0.0;
  avtpcrfbase->thread_data.current_ts = 110;
  gst_object_unref (avtpcrfbase);

  test_crf_tstamps (h, buf, 112, 110, 1);

  gst_buffer_unref (buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
avtpcrfcheck_suite (void)
{
  Suite *s = suite_create ("avtpcrfcheck");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_properties);
  tcase_add_test (tc_chain, test_crf_cvf_data);
  tcase_add_test (tc_chain, test_crf_aaf_data);
  tcase_add_test (tc_chain, test_crf_period_zero);

  return s;
}

GST_CHECK_MAIN (avtpcrfcheck);
