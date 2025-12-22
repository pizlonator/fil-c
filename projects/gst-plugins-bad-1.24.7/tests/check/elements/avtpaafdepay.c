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

const guint8 audio_data[] = { 0xAA, 0xBB, 0xCC, 0xDD };

static GstHarness *
setup_harness (void)
{
  GstHarness *h;

  h = gst_harness_new_parse ("avtpaafdepay streamid=0xDEADC0DEDEADC0DE");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  return h;
}

static GstBuffer *
create_input_buffer (GstHarness * h)
{
  GstBuffer *buf;
  GstMapInfo info;
  struct avtp_stream_pdu *pdu;

  buf = gst_harness_create_buffer (h, sizeof (struct avtp_stream_pdu) +
      sizeof (audio_data));
  gst_buffer_map (buf, &info, GST_MAP_WRITE);
  pdu = (struct avtp_stream_pdu *) info.data;
  avtp_aaf_pdu_init (pdu);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_TV, 1);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_STREAM_ID, 0xDEADC0DEDEADC0DE);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_FORMAT, AVTP_AAF_FORMAT_INT_16BIT);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_NSR, AVTP_AAF_PCM_NSR_48KHZ);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_CHAN_PER_FRAME, 2);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_BIT_DEPTH, 16);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_TIMESTAMP, 3000);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_STREAM_DATA_LEN, sizeof (audio_data));
  memcpy (pdu->avtp_payload, audio_data, sizeof (audio_data));
  gst_buffer_unmap (buf, &info);

  return buf;
}

GST_START_TEST (test_invalid_audio_features)
{
  GstHarness *h;
  GstMapInfo map;
  GstBuffer *buf;
  struct avtp_stream_pdu *pdu;

  h = setup_harness ();
  buf = create_input_buffer (h);

  gst_harness_push (h, gst_buffer_copy (buf));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 1);

  /* Don't care about the first buffer - it only sets what should
   * be accepted from now on */
  gst_buffer_unref (gst_harness_pull (h));

  /* Invalid rate */
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_NSR, AVTP_AAF_PCM_NSR_16KHZ);
  gst_buffer_unmap (buf, &map);

  gst_harness_push (h, gst_buffer_copy (buf));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 1);

  /* Invalid depth */
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_NSR, AVTP_AAF_PCM_NSR_48KHZ);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_BIT_DEPTH, 32);
  gst_buffer_unmap (buf, &map);

  gst_harness_push (h, gst_buffer_copy (buf));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 1);

  /* Invalid format */
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_BIT_DEPTH, 16);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_FORMAT, AVTP_AAF_FORMAT_INT_32BIT);
  gst_buffer_unmap (buf, &map);

  gst_harness_push (h, gst_buffer_copy (buf));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 1);

  /* Invalid channels */
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_FORMAT, AVTP_AAF_FORMAT_INT_16BIT);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_CHAN_PER_FRAME, 4);
  gst_buffer_unmap (buf, &map);

  gst_harness_push (h, gst_buffer_copy (buf));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 1);

  gst_buffer_unref (buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_invalid_avtpdu)
{
  GstHarness *h;
  GstMapInfo map;
  GstBuffer *buf, *small;
  struct avtp_stream_pdu *pdu;

  h = setup_harness ();
  buf = create_input_buffer (h);

  /* Invalid subtype */
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_pdu_set ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE,
      AVTP_SUBTYPE_CRF);
  gst_buffer_unmap (buf, &map);

  gst_harness_push (h, gst_buffer_copy (buf));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid AVTP version */
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_pdu_set ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE,
      AVTP_SUBTYPE_AAF);
  avtp_pdu_set ((struct avtp_common_pdu *) pdu, AVTP_FIELD_VERSION, 3);
  gst_buffer_unmap (buf, &map);

  gst_harness_push (h, gst_buffer_copy (buf));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid SV */
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_pdu_set ((struct avtp_common_pdu *) pdu, AVTP_FIELD_VERSION, 0);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_SV, 0);
  gst_buffer_unmap (buf, &map);

  gst_harness_push (h, gst_buffer_copy (buf));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid stream id */
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_SV, 0);
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_STREAM_ID, 0xAABBCCDDEEFF0001);
  gst_buffer_unmap (buf, &map);

  gst_harness_push (h, gst_buffer_copy (buf));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid stream data len  */
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_STREAM_ID, 0xDEADC0DEDEADC0DE);
  gst_buffer_unmap (buf, &map);

  gst_harness_push (h, gst_buffer_copy (buf));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid buffer size (too small to fit an AVTP header) */
  small = gst_harness_create_buffer (h, sizeof (struct avtp_stream_pdu) / 2);
  gst_harness_push (h, small);
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  gst_buffer_unref (buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_events)
{
  gchar *str;
  GstHarness *h;
  GstBuffer *buf;
  GstEvent *event;
  guint event_count;
  GstCaps *caps;
  const GstSegment *segment;

  h = setup_harness ();
  buf = create_input_buffer (h);
  gst_harness_push (h, buf);

  event_count = gst_harness_events_in_queue (h);
  fail_unless (event_count == 3);

  event = gst_harness_pull_event (h);
  fail_unless (GST_EVENT_TYPE (event) == GST_EVENT_STREAM_START);
  gst_event_unref (event);

  event = gst_harness_pull_event (h);
  fail_unless (GST_EVENT_TYPE (event) == GST_EVENT_CAPS);
  gst_event_parse_caps (event, &caps);
  str = gst_caps_to_string (caps);
  fail_unless (strcmp (str,
          "audio/x-raw, format=(string)S16BE, rate=(int)48000, channels=(int)2, layout=(string)interleaved")
      == 0);
  g_free (str);
  gst_event_unref (event);

  event = gst_harness_pull_event (h);
  fail_unless (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT);
  gst_event_parse_segment (event, &segment);
  fail_unless (segment->format == GST_FORMAT_TIME);
  fail_unless (segment->base == 3000);
  fail_unless (segment->start == 3000);
  fail_unless (segment->stop == -1);
  gst_event_unref (event);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_buffer)
{
  GstHarness *h;
  GstMapInfo info;
  GstBuffer *in, *out;

  h = setup_harness ();
  in = create_input_buffer (h);
  out = gst_harness_push_and_pull (h, in);

  fail_unless (gst_buffer_get_size (out) == sizeof (audio_data));

  gst_buffer_map (out, &info, GST_MAP_READ);
  fail_unless (memcmp (info.data, audio_data, info.size) == 0);
  gst_buffer_unmap (out, &info);

  gst_buffer_unref (out);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_property)
{
  guint64 val;
  GstHarness *h;
  GstElement *element;
  const guint64 streamid = 0xAABBCCDDEEFF0001;

  h = setup_harness ();
  element = gst_harness_find_element (h, "avtpaafdepay");

  g_object_set (G_OBJECT (element), "streamid", streamid, NULL);
  g_object_get (G_OBJECT (element), "streamid", &val, NULL);
  fail_unless (val == streamid);

  gst_object_unref (element);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
avtpaafdepay_suite (void)
{
  Suite *s = suite_create ("avtpaafdepay");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_buffer);
  tcase_add_test (tc_chain, test_events);
  tcase_add_test (tc_chain, test_property);
  tcase_add_test (tc_chain, test_invalid_avtpdu);
  tcase_add_test (tc_chain, test_invalid_audio_features);

  return s;
}

GST_CHECK_MAIN (avtpaafdepay);
