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
#include "../../../ext/avtp/gstavtpcrfbase.c"
#undef GST_CAT_DEFAULT

#include <glib.h>
#include <avtp_crf.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

static struct avtp_crf_pdu *
generate_crf_pdu (int data_len, guint64 first_tstamp)
{
  const guint64 base_freq = 48000;
  const guint64 interval = 160;
  const gdouble interval_time = 1.0e9 / base_freq * interval;

  struct avtp_crf_pdu *crf_pdu =
      g_malloc0 (sizeof (struct avtp_crf_pdu) + data_len);

  avtp_crf_pdu_init (crf_pdu);
  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_SV, 1);
  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_STREAM_ID, 0xABCD1234ABCD1234);
  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_TYPE, AVTP_CRF_TYPE_AUDIO_SAMPLE);
  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_BASE_FREQ, base_freq);
  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_PULL, 1);
  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_CRF_DATA_LEN, data_len);
  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_TIMESTAMP_INTERVAL, interval);
  for (int i = 0; i < data_len / 8; i++) {
    const guint64 offset = i * interval_time;
    crf_pdu->crf_data[i] = htobe64 (first_tstamp + offset);
  }

  return crf_pdu;
}

GST_START_TEST (test_validate_crf_pdu_success)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == TRUE);

  /* Validate thread_data fields. */
  fail_unless (thread_data->base_freq == 48000);
  fail_unless (thread_data->pull == 1);
  fail_unless (thread_data->type == AVTP_CRF_TYPE_AUDIO_SAMPLE);
  fail_unless (thread_data->mr == 0);
  fail_unless (thread_data->timestamp_interval == 160);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_multiple_packets_success)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu1 = generate_crf_pdu (64, 1000);
  struct avtp_crf_pdu *crf_pdu2 = generate_crf_pdu (64, 1800);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu1,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == TRUE);

  /* Validate thread_data fields. */
  fail_unless (thread_data->base_freq == 48000);
  fail_unless (thread_data->pull == 1);
  fail_unless (thread_data->type == AVTP_CRF_TYPE_AUDIO_SAMPLE);
  fail_unless (thread_data->mr == 0);
  fail_unless (thread_data->timestamp_interval == 160);

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu2,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == TRUE);

  g_free (crf_pdu1);
  g_free (crf_pdu2);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_wrong_subtype)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;

  avtp_pdu_set ((struct avtp_common_pdu *) crf_pdu, AVTP_FIELD_SUBTYPE,
      AVTP_SUBTYPE_CVF);

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_streamid_invalid)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;

  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_SV, 0);

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_streamid_different)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCDABCD;

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_data_len_too_long)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 40);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_timestamp_interval_zero)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;

  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_TIMESTAMP_INTERVAL, 0);

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_base_freq_zero)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;

  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_BASE_FREQ, 0);

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_pull_invalid)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;

  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_PULL, 7);

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_type_invalid)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;

  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_TYPE, 8);

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_data_len_invalid)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;

  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_CRF_DATA_LEN, 20);

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_timestamp_interval_mismatch)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;
  thread_data->timestamp_interval = 120;
  thread_data->base_freq = 48000;
  thread_data->pull = 1;
  thread_data->type = AVTP_CRF_TYPE_AUDIO_SAMPLE;
  thread_data->num_pkt_tstamps = 8;

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_base_freq_mismatch)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;
  thread_data->timestamp_interval = 160;
  thread_data->base_freq = 44100;
  thread_data->pull = 1;
  thread_data->type = AVTP_CRF_TYPE_AUDIO_SAMPLE;
  thread_data->num_pkt_tstamps = 8;

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_pull_mismatch)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;
  thread_data->timestamp_interval = 160;
  thread_data->base_freq = 48000;
  thread_data->pull = 2;
  thread_data->type = AVTP_CRF_TYPE_AUDIO_SAMPLE;
  thread_data->num_pkt_tstamps = 8;

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_type_mismatch)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (64, 1000);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;
  thread_data->timestamp_interval = 160;
  thread_data->base_freq = 48000;
  thread_data->pull = 1;
  thread_data->type = AVTP_CRF_TYPE_VIDEO_FRAME;
  thread_data->num_pkt_tstamps = 8;

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + 64);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_data_len_mismatch)
{
  int data_len = 48;
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (data_len, 1000);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;
  thread_data->timestamp_interval = 160;
  thread_data->base_freq = 48000;
  thread_data->pull = 1;
  thread_data->type = AVTP_CRF_TYPE_AUDIO_SAMPLE;
  thread_data->num_pkt_tstamps = 6;

  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_CRF_DATA_LEN, 20);

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + data_len);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_validate_crf_pdu_tstamps_not_monotonic)
{
  int data_len = 48;
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (data_len, 1000);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;
  gboolean ret;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;
  thread_data->timestamp_interval = 160;
  thread_data->base_freq = 48000;
  thread_data->pull = 1;
  thread_data->type = AVTP_CRF_TYPE_AUDIO_SAMPLE;
  thread_data->num_pkt_tstamps = 6;

  crf_pdu->crf_data[3] = 1145;

  ret =
      validate_crf_pdu (avtpcrfbase, crf_pdu,
      sizeof (struct avtp_crf_pdu) + data_len);
  fail_unless (ret == FALSE);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

GST_START_TEST (test_gst_base_freq_multiplier)
{
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  gdouble ret;

  ret = get_base_freq_multiplier (avtpcrfbase, 0);
  fail_unless_equals_float (ret, 1.0);

  ret = get_base_freq_multiplier (avtpcrfbase, 1);
  fail_unless_equals_float (ret, 1 / 1.001);

  ret = get_base_freq_multiplier (avtpcrfbase, 2);
  fail_unless_equals_float (ret, 1.001);

  ret = get_base_freq_multiplier (avtpcrfbase, 3);
  fail_unless_equals_float (ret, 24.0 / 25);

  ret = get_base_freq_multiplier (avtpcrfbase, 4);
  fail_unless_equals_float (ret, 25.0 / 24);

  ret = get_base_freq_multiplier (avtpcrfbase, 5);
  fail_unless_equals_float (ret, 1.0 / 8);

  ret = get_base_freq_multiplier (avtpcrfbase, 6);
  fail_unless_equals_float (ret, -1);
  gst_object_unref (avtpcrfbase);

}

GST_END_TEST;

static void
setup_thread_defaults (GstAvtpCrfBase * avtpcrfbase, gdouble * past_periods)
{
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;

  avtpcrfbase->streamid = 0xABCD1234ABCD1234;
  thread_data->base_freq = 48000;
  thread_data->pull = 1;
  thread_data->type = AVTP_CRF_TYPE_AUDIO_SAMPLE;
  thread_data->past_periods = past_periods;
}

/*
 * Test for more than 1 timestamp per CRF AVTPDU. This is just a simple success
 * case.
 */
GST_START_TEST (test_calculate_average_period_multiple_crf_tstamps)
{
  int data_len = 64;
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (data_len, 1000);
  gdouble past_periods[10] = { 21000, 20500, 0, 0, 0, 0, 0, 0, 0, 0 };
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;

  setup_thread_defaults (avtpcrfbase, past_periods);

  thread_data->timestamp_interval = 160;
  thread_data->num_pkt_tstamps = 6;
  thread_data->past_periods_iter = 2;
  thread_data->periods_stored = 2;

  calculate_average_period (avtpcrfbase, crf_pdu);
  fail_unless_equals_float (thread_data->average_period, 20777.7775);
  fail_unless_equals_float (thread_data->past_periods[2], 20833.3325);
  fail_unless_equals_uint64 (thread_data->current_ts, 1000);

  gst_object_unref (avtpcrfbase);
  g_free (crf_pdu);
}

GST_END_TEST;

/*
 * Test for rounding error
 */
GST_START_TEST (test_calculate_average_period_rounding_error)
{
  /* the presentation time in ns */
  const GstClockTimeDiff ptime = 50000000;
  /* the time in ns of one sync event e.g. one audio sample @48kHz */
  const gdouble event_interval = 1.0e9 / 48000;
  /* the presentation time measured in sync events (e.g. sample rate)
   * for class B traffic with a presentation time of 50ms.
   */
  const GstClockTime ptime_in_events = ptime / event_interval;

  /* With 4 timestamps generate_crf_pdu() multiples the interval time
   * with 3. This results into an integer time stamp in nsi without decimal
   * digits. Therefore the rounding issue when generating the timestamps for
   * the CRF PDU is avoided here.
   */
  int data_len = 32;
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (data_len, 1000);
  gdouble past_periods[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;

  setup_thread_defaults (avtpcrfbase, past_periods);

  thread_data->timestamp_interval = 160;
  thread_data->num_pkt_tstamps = data_len / sizeof (uint64_t);
  thread_data->past_periods_iter = 0;
  thread_data->periods_stored = 0;

  calculate_average_period (avtpcrfbase, crf_pdu);

  /* When internally using integer for average_period calculation the following
   * multiplication will result to (20833 * 2400=) 49999200ns. This value
   * differs by 800ns from the original presentation time of 50ms. When using
   * double this rounding error is avoided.
   */
  fail_unless_equals_float ((thread_data->average_period * ptime_in_events),
      ptime);

  gst_object_unref (avtpcrfbase);
  g_free (crf_pdu);
}

GST_END_TEST;

/*
 * Test for an overflow in the 64-bit CRF timestamp in the CRF AVTPDU when
 * there are multiple CRF timestamps in a packet.
 */
GST_START_TEST
    (test_calculate_average_period_multiple_crf_tstamps_64_bit_overflow) {
  int data_len = 64;
  struct avtp_crf_pdu *crf_pdu =
      generate_crf_pdu (data_len, 18446744073709501615ULL);
  gdouble past_periods[10] =
      { 21000, 20500, 21220, 21345, 20990, 21996, 20220, 20915, 21324, 23123 };
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;

  setup_thread_defaults (avtpcrfbase, past_periods);

  thread_data->timestamp_interval = 160;
  thread_data->num_pkt_tstamps = 6;
  thread_data->past_periods_iter = 5;
  thread_data->periods_stored = 10;

  calculate_average_period (avtpcrfbase, crf_pdu);
  fail_unless_equals_float (thread_data->average_period, 21147.03325);
  fail_unless_equals_float (thread_data->past_periods[5], 20833.3325);
  fail_unless_equals_uint64 (thread_data->current_ts, 18446744073709501615ULL);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

/*
 * Test case for single timestamp per CRF AVTPDU. This is just a simple success
 * case.
 */
GST_START_TEST (test_calculate_average_period_single_crf_tstamp)
{
  int data_len = 8;
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (data_len, 21833);
  gdouble past_periods[10] = { 21000, 20500, 0, 0, 0, 0, 0, 0, 0, 0 };
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;

  setup_thread_defaults (avtpcrfbase, past_periods);

  thread_data->timestamp_interval = 1;
  thread_data->num_pkt_tstamps = 1;
  thread_data->past_periods_iter = 2;
  thread_data->periods_stored = 2;
  thread_data->last_received_tstamp = 1000;
  thread_data->last_seqnum = 9;

  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_SEQ_NUM, 10);

  calculate_average_period (avtpcrfbase, crf_pdu);
  fail_unless_equals_float (thread_data->average_period, 20777.6666666);
  fail_unless_equals_float (thread_data->past_periods[2], 20833);
  fail_unless_equals_uint64 (thread_data->last_seqnum, 10);
  fail_unless_equals_uint64 (thread_data->last_received_tstamp, 21833);
  fail_unless_equals_uint64 (thread_data->current_ts, 21833);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

/* 
 * Test to ensure all members of thread_data struct are initialized as expected
 * when receiving multiple CRF AVTPDUs with single CRF timestamp.
 */
GST_START_TEST (test_calculate_average_period_single_crf_tstamp_init)
{
  int data_len = 8;
  struct avtp_crf_pdu *crf_pdu1 = generate_crf_pdu (data_len, 1000);
  struct avtp_crf_pdu *crf_pdu2 = generate_crf_pdu (data_len, 21833);
  gdouble past_periods[10] = { 0 };
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;

  setup_thread_defaults (avtpcrfbase, past_periods);

  thread_data->timestamp_interval = 1;
  thread_data->num_pkt_tstamps = 1;

  avtp_crf_pdu_set (crf_pdu1, AVTP_CRF_FIELD_SEQ_NUM, 10);
  avtp_crf_pdu_set (crf_pdu2, AVTP_CRF_FIELD_SEQ_NUM, 11);

  calculate_average_period (avtpcrfbase, crf_pdu1);
  fail_unless_equals_float (thread_data->past_periods[0], 0);
  fail_unless_equals_uint64 (thread_data->last_seqnum, 10);
  fail_unless_equals_float (thread_data->average_period, 20854);
  fail_unless_equals_uint64 (thread_data->current_ts, 1000);

  calculate_average_period (avtpcrfbase, crf_pdu2);
  fail_unless_equals_float (thread_data->past_periods[0], 20833);
  fail_unless_equals_uint64 (thread_data->last_seqnum, 11);
  fail_unless_equals_float (thread_data->average_period, 20833);
  fail_unless_equals_uint64 (thread_data->current_ts, 21833);

  g_free (crf_pdu1);
  g_free (crf_pdu2);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

/*
 * Test to ensure average_period is calculated correctly
 * when receiving multiple CRF AVTPDUs with single CRF timestamp
 * with timestamp_interval > 1
 */
GST_START_TEST (test_calculate_average_period_single_crf_tstamp_interval)
{
  int data_len = 8;
  struct avtp_crf_pdu *crf_pdu1 = generate_crf_pdu (data_len, 1000);
  /* Used timestamp
   * = sample_time * timestamp_interval + first_tstamp
   * = 1/48kHz * 160 + 1000
   */
  struct avtp_crf_pdu *crf_pdu2 = generate_crf_pdu (data_len, 3334280);
  gdouble past_periods[10] = { 0 };
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;

  setup_thread_defaults (avtpcrfbase, past_periods);

  thread_data->timestamp_interval = 160;
  thread_data->num_pkt_tstamps = 1;

  avtp_crf_pdu_set (crf_pdu1, AVTP_CRF_FIELD_SEQ_NUM, 10);
  avtp_crf_pdu_set (crf_pdu2, AVTP_CRF_FIELD_SEQ_NUM, 11);

  calculate_average_period (avtpcrfbase, crf_pdu1);
  fail_unless_equals_float (thread_data->past_periods[0], 0);
  fail_unless_equals_uint64 (thread_data->last_seqnum, 10);
  fail_unless_equals_float (thread_data->average_period, 20854);
  fail_unless_equals_uint64 (thread_data->current_ts, 1000);

  calculate_average_period (avtpcrfbase, crf_pdu2);
  fail_unless_equals_float (thread_data->past_periods[0], 20833);
  fail_unless_equals_uint64 (thread_data->last_seqnum, 11);
  fail_unless_equals_float (thread_data->average_period, 20833);
  fail_unless_equals_uint64 (thread_data->current_ts, 3334280);

  g_free (crf_pdu1);
  g_free (crf_pdu2);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

/*
 * Test for an overflow in the 64-bit CRF timestamp in the CRF AVTPDU when
 * there is a single CRF timestamp in a packet.
 */
GST_START_TEST (test_calculate_average_period_single_crf_tstamp_64_bit_overflow)
{
  int data_len = 8;
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (data_len, 20833);
  gdouble past_periods[10] = { 21000, 20500, 0, 0, 0, 0, 0, 0, 0, 0 };
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;

  setup_thread_defaults (avtpcrfbase, past_periods);

  thread_data->timestamp_interval = 1;
  thread_data->num_pkt_tstamps = 1;
  thread_data->past_periods_iter = 2;
  thread_data->periods_stored = 2;
  thread_data->last_received_tstamp = 18446744073709551615ULL;
  thread_data->last_seqnum = 9;

  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_SEQ_NUM, 10);

  calculate_average_period (avtpcrfbase, crf_pdu);
  fail_unless_equals_float (thread_data->average_period, 20778);
  fail_unless_equals_float (thread_data->past_periods[2], 20834);
  fail_unless_equals_uint64 (thread_data->last_seqnum, 10);
  fail_unless_equals_uint64 (thread_data->last_received_tstamp, 20833);
  fail_unless_equals_uint64 (thread_data->current_ts, 20833);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

/*
 * Test to ensure expected behevior when a sequence number is skipped (probably
 * due to packet loss or delay) in CRF AVTPDUs with single timestamp per
 * AVTPDU.
 */
GST_START_TEST (test_calculate_average_period_single_crf_tstamp_seq_num_skip)
{
  int data_len = 8;
  struct avtp_crf_pdu *crf_pdu = generate_crf_pdu (data_len, 21833);
  gdouble past_periods[10] = { 21000, 20500, 0, 0, 0, 0, 0, 0, 0, 0 };
  GstAvtpCrfBase *avtpcrfbase = g_object_new (GST_TYPE_AVTP_CRF_BASE, NULL);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;

  setup_thread_defaults (avtpcrfbase, past_periods);

  thread_data->timestamp_interval = 1;
  thread_data->num_pkt_tstamps = 1;
  thread_data->past_periods_iter = 2;
  thread_data->last_received_tstamp = 1000;
  thread_data->last_seqnum = 9;
  thread_data->average_period = 20750;

  avtp_crf_pdu_set (crf_pdu, AVTP_CRF_FIELD_SEQ_NUM, 12);

  calculate_average_period (avtpcrfbase, crf_pdu);
  fail_unless_equals_float (thread_data->average_period, 20750);
  fail_unless_equals_float (thread_data->past_periods[2], 0);
  fail_unless_equals_uint64 (thread_data->last_seqnum, 12);
  fail_unless_equals_uint64 (thread_data->last_received_tstamp, 21833);
  fail_unless_equals_uint64 (thread_data->current_ts, 21833);

  g_free (crf_pdu);
  gst_object_unref (avtpcrfbase);
}

GST_END_TEST;

static Suite *
avtpcrfbase_suite (void)
{
  Suite *s = suite_create ("avtpcrfbase");
  TCase *tc_chain = tcase_create ("general");

  GST_DEBUG_CATEGORY_INIT (avtpcrfbase_debug, "avtpcrfbase", 0, "CRF Base");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_validate_crf_pdu_success);
  tcase_add_test (tc_chain, test_validate_crf_pdu_multiple_packets_success);
  tcase_add_test (tc_chain, test_validate_crf_pdu_wrong_subtype);
  tcase_add_test (tc_chain, test_validate_crf_pdu_streamid_invalid);
  tcase_add_test (tc_chain, test_validate_crf_pdu_streamid_different);
  tcase_add_test (tc_chain, test_validate_crf_pdu_data_len_too_long);
  tcase_add_test (tc_chain, test_validate_crf_pdu_timestamp_interval_zero);
  tcase_add_test (tc_chain, test_validate_crf_pdu_base_freq_zero);
  tcase_add_test (tc_chain, test_validate_crf_pdu_pull_invalid);
  tcase_add_test (tc_chain, test_validate_crf_pdu_type_invalid);
  tcase_add_test (tc_chain, test_validate_crf_pdu_data_len_invalid);
  tcase_add_test (tc_chain, test_validate_crf_pdu_timestamp_interval_mismatch);
  tcase_add_test (tc_chain, test_validate_crf_pdu_base_freq_mismatch);
  tcase_add_test (tc_chain, test_validate_crf_pdu_pull_mismatch);
  tcase_add_test (tc_chain, test_validate_crf_pdu_type_mismatch);
  tcase_add_test (tc_chain, test_validate_crf_pdu_data_len_mismatch);
  tcase_add_test (tc_chain, test_validate_crf_pdu_tstamps_not_monotonic);
  tcase_add_test (tc_chain, test_gst_base_freq_multiplier);
  tcase_add_test (tc_chain, test_calculate_average_period_multiple_crf_tstamps);
  tcase_add_test (tc_chain, test_calculate_average_period_rounding_error);
  tcase_add_test (tc_chain,
      test_calculate_average_period_multiple_crf_tstamps_64_bit_overflow);
  tcase_add_test (tc_chain, test_calculate_average_period_single_crf_tstamp);
  tcase_add_test (tc_chain,
      test_calculate_average_period_single_crf_tstamp_init);
  tcase_add_test (tc_chain,
      test_calculate_average_period_single_crf_tstamp_interval);
  tcase_add_test (tc_chain,
      test_calculate_average_period_single_crf_tstamp_64_bit_overflow);
  tcase_add_test (tc_chain,
      test_calculate_average_period_single_crf_tstamp_seq_num_skip);

  return s;
}

GST_CHECK_MAIN (avtpcrfbase);
