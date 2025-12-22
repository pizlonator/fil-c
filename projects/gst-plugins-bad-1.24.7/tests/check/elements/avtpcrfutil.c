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
#include "../../../ext/avtp/gstavtpcrfbase.h"
#include "../../../ext/avtp/gstavtpcrfutil.h"

#include <avtp.h>
#include <avtp_aaf.h>
#include <avtp_cvf.h>
#include <glib.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

GST_START_TEST (test_buffer_tstamp_valid)
{
  struct avtp_stream_pdu pdu = { 0, };
  GstMapInfo info = { 0, };
  gboolean result;

  info.data = (guint8 *) & pdu;

  avtp_pdu_set ((struct avtp_common_pdu *) &pdu, AVTP_FIELD_SUBTYPE,
      AVTP_SUBTYPE_AAF);
  info.size = 50;
  result = buffer_size_valid (&info);
  fail_unless (result == TRUE);

  avtp_pdu_set ((struct avtp_common_pdu *) &pdu, AVTP_FIELD_SUBTYPE,
      AVTP_SUBTYPE_CVF);
  avtp_cvf_pdu_set (&pdu, AVTP_CVF_FIELD_FORMAT_SUBTYPE,
      AVTP_CVF_FORMAT_SUBTYPE_H264);
  info.size = 55;
  result = buffer_size_valid (&info);
  fail_unless (result == TRUE);

  avtp_pdu_set ((struct avtp_common_pdu *) &pdu, AVTP_FIELD_SUBTYPE,
      AVTP_SUBTYPE_AAF);
  info.size = 15;
  result = buffer_size_valid (&info);
  fail_unless (result == FALSE);

  avtp_pdu_set ((struct avtp_common_pdu *) &pdu, AVTP_FIELD_SUBTYPE,
      AVTP_SUBTYPE_CVF);
  avtp_cvf_pdu_set (&pdu, AVTP_CVF_FIELD_FORMAT_SUBTYPE,
      AVTP_CVF_FORMAT_SUBTYPE_H264);
  info.size = 24;
  result = buffer_size_valid (&info);
  fail_unless (result == FALSE);
}

GST_END_TEST;

GST_START_TEST (test_get_avtp_tstamp)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_stream_pdu pdu;
  GstClockTime tstamp;

  avtp_aaf_pdu_init (&pdu);
  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_TV, 1);
  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_TIMESTAMP, 12345);
  tstamp = get_avtp_tstamp (avtpcrfbase, &pdu);
  fail_unless_equals_uint64 (tstamp, 12345);

  avtp_aaf_pdu_set (&pdu, AVTP_AAF_FIELD_TV, 0);
  tstamp = get_avtp_tstamp (avtpcrfbase, &pdu);
  fail_unless_equals_uint64 (tstamp, GST_CLOCK_TIME_NONE);

  avtp_cvf_pdu_init (&pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (&pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (&pdu, AVTP_CVF_FIELD_TIMESTAMP, 43567);
  tstamp = get_avtp_tstamp (avtpcrfbase, &pdu);
  fail_unless_equals_uint64 (tstamp, 43567);

  avtp_cvf_pdu_set (&pdu, AVTP_CVF_FIELD_TV, 0);
  tstamp = get_avtp_tstamp (avtpcrfbase, &pdu);
  fail_unless_equals_uint64 (tstamp, GST_CLOCK_TIME_NONE);

  avtp_pdu_set ((struct avtp_common_pdu *) &pdu, AVTP_FIELD_SUBTYPE,
      AVTP_SUBTYPE_TSCF);
  tstamp = get_avtp_tstamp (avtpcrfbase, &pdu);
  fail_unless_equals_uint64 (tstamp, GST_CLOCK_TIME_NONE);

  g_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_get_h264_tstamp)
{
  struct avtp_stream_pdu *pdu =
      g_malloc0 (sizeof (struct avtp_stream_pdu) + sizeof (guint32));
  gboolean tstamp_valid;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 43567);
  tstamp_valid = h264_tstamp_valid (pdu);
  fail_unless (tstamp_valid == TRUE);

  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 0);
  tstamp_valid = h264_tstamp_valid (pdu);
  fail_unless (tstamp_valid == FALSE);

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_MJPEG);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 43567);
  tstamp_valid = h264_tstamp_valid (pdu);
  fail_unless (tstamp_valid == FALSE);

  avtp_aaf_pdu_init (pdu);
  tstamp_valid = h264_tstamp_valid (pdu);
  fail_unless (tstamp_valid == FALSE);

  g_free (pdu);
}

GST_END_TEST;

static Suite *
avtpcrfutil_suite (void)
{
  Suite *s = suite_create ("avtpcrfutil");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_buffer_tstamp_valid);
  tcase_add_test (tc_chain, test_get_avtp_tstamp);
  tcase_add_test (tc_chain, test_get_h264_tstamp);

  return s;
}

GST_CHECK_MAIN (avtpcrfutil);
