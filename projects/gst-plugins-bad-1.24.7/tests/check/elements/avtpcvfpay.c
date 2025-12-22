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

#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

#include <avtp.h>
#include <avtp_cvf.h>

#define AVTP_CVF_H264_HEADER_SIZE (sizeof(struct avtp_stream_pdu) + sizeof(guint32))
#define STREAM_ID 0xAABBCCDDEEFF0001

/* Simple codec data, with only the NAL size len, no SPS/PPS. */
static GstCaps *
generate_caps (guint8 nal_size_len)
{
  GstBuffer *codec_data;
  GstMapInfo map;
  GstCaps *caps;

  /* 7 is the minimal codec_data size, when no SPS/PPS is sent */
  codec_data = gst_buffer_new_allocate (NULL, 7, NULL);
  gst_buffer_map (codec_data, &map, GST_MAP_READWRITE);

  memset (map.data, 0, map.size);
  map.data[0] = 1;              /* version */
  map.data[4] = (nal_size_len - 1) | 0xfc;      /* Other 6 bits are 1 */
  map.data[5] = 0xe0;           /* first 3 bits are 1 */

  gst_buffer_unmap (codec_data, &map);

  caps = gst_caps_new_simple ("video/x-h264",
      "stream-format", G_TYPE_STRING, "avc",
      "alignment", G_TYPE_STRING, "au",
      "codec_data", GST_TYPE_BUFFER, codec_data, NULL);
  gst_buffer_unref (codec_data);

  return caps;
}

static void
fill (guint8 * buf, gsize size)
{
  guint8 i = 0;

  while (size--)
    *buf++ = i++;
}

static gboolean
check_nal_filling (GstBuffer * buffer, guint8 first)
{
  GstMapInfo map;
  gint i;
  gsize offset = AVTP_CVF_H264_HEADER_SIZE + 1;
  gboolean result = TRUE;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  if ((map.data[AVTP_CVF_H264_HEADER_SIZE] & 0x1f) == 28)
    offset++;                   /* Fragmented NALs have 2 bytes header */

  for (i = offset; i < map.size; i++) {
    if (map.data[i] != first++) {
      result = FALSE;
      break;
    }
  }

  gst_buffer_unmap (buffer, &map);

  return result;
}

static void
add_nal (GstBuffer * buffer, gsize size, guint type, gsize offset)
{
  GstMapInfo map;

  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);

  map.data[offset] = map.data[offset + 1] = 0;
  map.data[offset + 2] = size >> 8;
  map.data[offset + 3] = size & 0xff;
  map.data[offset + 4] = type & 0x1f;
  fill (&map.data[offset + 5], size - 1);

  gst_buffer_unmap (buffer, &map);
}

/* This function assumes that NAL size len is 2 */
static void
add_nal_2 (GstBuffer * buffer, gsize size, guint type, gsize offset)
{
  GstMapInfo map;

  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);

  map.data[offset] = size >> 8;
  map.data[offset + 1] = size & 0xff;
  map.data[offset + 2] = type & 0x1f;
  fill (&map.data[offset + 3], size - 1);

  gst_buffer_unmap (buffer, &map);
}

static gboolean
compare_h264_avtpdu (struct avtp_stream_pdu *pdu, GstBuffer * buffer)
{
  GstMapInfo map;
  gboolean result;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  /* buffer must have at least the header size */
  if (map.size < AVTP_CVF_H264_HEADER_SIZE)
    return FALSE;

  result = memcmp (map.data, pdu, AVTP_CVF_H264_HEADER_SIZE) == 0;

  gst_buffer_unmap (buffer, &map);

  return result;
}

GST_START_TEST (test_payloader_spread_ts)
{
  GstHarness *h;
  GstBuffer *in, *out;
  gint i, total_fragments, max_interval_frames;
  guint64 first_tx_time, final_dts, measurement_interval = 250000;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse
      ("avtpcvfpay streamid=0xAABBCCDDEEFF0001 mtt=2000000 tu=125000 processing-deadline=0 mtu=128 measurement-interval=250000 max-interval-frames=3");
  gst_harness_set_src_caps (h, generate_caps (4));

  /* A 980 bytes NAL with mtu=128 should generate 10 fragments */
  in = gst_harness_create_buffer (h, 980 + 4);
  add_nal (in, 980, 7, 0);
  GST_BUFFER_DTS (in) = final_dts = 1000000;
  GST_BUFFER_PTS (in) = 2000000;

  /* We now push the buffer, and check if we got ten from the avtpcvfpay */
  gst_harness_push (h, in);
  fail_unless_equals_int (gst_harness_buffers_received (h), 10);

  /* Using max-interval-frames=3, we'll need 4 measurement intervals to send
   * all fragments, with last one just about current DTS, and others
   * progressively before that.
   *
   * So we should have something like:
   *
   * |  1st  |  2nd  |  3rd  |  4th |  Intervals
   * | 1 2 3 | 4 5 6 | 7 8 9 |  10  |  AVTPDUs in each interval (sharing same DTS/PTS)
   *
   * And PTS/DTS should increment by a
   * measurement-interval / max-interval-frames for each AVTPDU.
   */
  i = 0;
  total_fragments = 10;
  max_interval_frames = 3;
  first_tx_time =
      final_dts -
      (measurement_interval / max_interval_frames) * (total_fragments - 1);
  for (i = 0; i < 10; i++) {
    out = gst_harness_pull (h);
    fail_unless_equals_uint64 (GST_BUFFER_DTS (out), first_tx_time);
    gst_buffer_unref (out);

    first_tx_time += measurement_interval / max_interval_frames;
  }


  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_payloader_downstream_eos)
{
  GstHarness *h;
  GstBuffer *in;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse
      ("avtpcvfpay streamid=0xAABBCCDDEEFF0001 mtt=1000000 tu=1000000 processing-deadline=0 ! fakesink num-buffers=1");
  gst_harness_set_src_caps (h, generate_caps (4));

  /* Buffer must have the nal len (4 bytes) and the nal (4 bytes) */
  in = gst_harness_create_buffer (h, 8);
  add_nal (in, 4, 1, 0);
  GST_BUFFER_DTS (in) = 1000000;
  GST_BUFFER_PTS (in) = 2000000;

  fail_unless_equals_int (gst_harness_push (h, in), GST_FLOW_EOS);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_payloader_zero_sized_nal)
{
  GstHarness *h;
  GstBuffer *in;
  GstMapInfo map;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse
      ("avtpcvfpay streamid=0xAABBCCDDEEFF0001 mtt=1000000 tu=1000000 processing-deadline=0");
  gst_harness_set_src_caps (h, generate_caps (4));

  /* We have the buffer with the nal size (4 bytes) and the nal (4 bytes), but
   * nal size will be zero */
  in = gst_harness_create_buffer (h, 8);
  GST_BUFFER_DTS (in) = 1000000;
  GST_BUFFER_PTS (in) = 2000000;

  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  map.data[0] = map.data[1] = map.data[2] = map.data[3] = 0;    /* Set NAL size to 0 */
  map.data[4] = 1;              /* Some dummy vcl NAL type */
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, in);

  /* No buffer shuld come out */
  fail_unless_equals_int (gst_harness_buffers_received (h), 0);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_payloader_no_codec_data)
{
  GstHarness *h;
  GstCaps *caps;
  GstBuffer *in;

  /* Caps without codec_data */
  caps = gst_caps_new_simple ("video/x-h264",
      "stream-format", G_TYPE_STRING, "avc",
      "alignment", G_TYPE_STRING, "au", NULL);

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse
      ("avtpcvfpay streamid=0xAABBCCDDEEFF0001 mtt=1000000 tu=1000000 processing-deadline=0");
  gst_harness_set_src_caps (h, caps);

  /* No buffer should come out when we send input */
  in = gst_harness_create_buffer (h, 8);
  add_nal (in, 4, 1, 0);
  GST_BUFFER_DTS (in) = 1000000;
  GST_BUFFER_PTS (in) = 2000000;

  gst_harness_push (h, in);
  fail_unless_equals_int (gst_harness_buffers_received (h), 0);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_payloader_invalid_caps)
{
  GstBuffer *codec_data;
  GstElement *element;
  GstPad *sinkpad;
  GstMapInfo map;
  GstCaps *caps;
  GstHarness *h;

  /* 7 is the minimal codec_data size, when no SPS/PPS is sent */
  codec_data = gst_buffer_new_allocate (NULL, 7, NULL);
  gst_buffer_map (codec_data, &map, GST_MAP_READWRITE);

  memset (map.data, 0, map.size);
  map.data[0] = 0;              /* version */
  map.data[4] = 0x03 | 0xfc;    /* Other 6 bits are 1 */
  map.data[5] = 0xe0;           /* first 3 bits are 1 */

  gst_buffer_unmap (codec_data, &map);

  caps = gst_caps_new_simple ("video/x-h264",
      "stream-format", G_TYPE_STRING, "avc",
      "alignment", G_TYPE_STRING, "au",
      "codec_data", GST_TYPE_BUFFER, codec_data, NULL);
  gst_buffer_unref (codec_data);

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse
      ("avtpcvfpay streamid=0xAABBCCDDEEFF0001 mtt=1000000 tu=1000000");
  element = gst_harness_find_element (h, "avtpcvfpay");
  sinkpad = gst_element_get_static_pad (element, "sink");

  /* 'codec_data' caps has invalid version */
  gst_harness_push_event (h, gst_event_new_caps (caps));
  fail_unless (gst_pad_get_current_caps (sinkpad) == NULL);
  gst_caps_unref (caps);

  /* Send a 'codec_data' too small */
  codec_data = gst_buffer_new_allocate (NULL, 6, NULL);
  caps = gst_caps_new_simple ("video/x-h264",
      "stream-format", G_TYPE_STRING, "avc",
      "alignment", G_TYPE_STRING, "au",
      "codec_data", GST_TYPE_BUFFER, codec_data, NULL);
  gst_buffer_unref (codec_data);

  gst_harness_push_event (h, gst_event_new_caps (caps));
  fail_unless (gst_pad_get_current_caps (sinkpad) == NULL);
  gst_caps_unref (caps);

  gst_object_unref (sinkpad);
  gst_object_unref (element);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_payloader_incomplete_nal)
{
  GstHarness *h;
  GstBuffer *in, *out;
  GstMapInfo map;
  struct avtp_stream_pdu *pdu = alloca (AVTP_CVF_H264_HEADER_SIZE);
  const gint DATA_LEN = sizeof (guint32) + 3;

  /* Create the 'expected' header */
  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 3000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 4000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse
      ("avtpcvfpay streamid=0xAABBCCDDEEFF0001 mtt=1000000 tu=1000000 processing-deadline=0");
  gst_harness_set_src_caps (h, generate_caps (4));

  /* Buffer must have the nal len (4 bytes) and the nal (3 bytes) */
  in = gst_harness_create_buffer (h, 7);
  GST_BUFFER_DTS (in) = 1000000;
  GST_BUFFER_PTS (in) = 2000000;

  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  map.data[0] = map.data[1] = map.data[2] = 0;
  map.data[3] = 8;              /* Lie that NAL size is 8, when buffer is only 7 (so NAL is 3) */
  map.data[4] = 1;              /* Some dummy vcl NAL type */
  map.data[5] = 0x0;
  map.data[6] = 0x1;
  gst_buffer_unmap (in, &map);

  out = gst_harness_push_and_pull (h, in);

  /* avtpcvfpay will happily payload the three byte nal. Now, we check it */
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  fail_unless (check_nal_filling (out, 0) == TRUE);

  gst_buffer_unref (out);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_payloader_properties)
{
  GstHarness *h;
  GstElement *element;
  guint mtu, mtt, tu, max_interval_frames;
  guint64 streamid, processing_deadline, measurement_interval;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse
      ("avtpcvfpay streamid=0xAABBCCDDEEFF0001 mtt=1000000 tu=2000000 mtu=100 processing-deadline=5000 measurement-interval=125000 max-interval-frames=3");

  /* Check if all properties were properly set up */
  element = gst_harness_find_element (h, "avtpcvfpay");
  g_object_get (G_OBJECT (element), "mtt", &mtt, NULL);
  fail_unless_equals_uint64 (mtt, 1000000);

  g_object_get (G_OBJECT (element), "mtu", &mtu, NULL);
  fail_unless_equals_uint64 (mtu, 100);

  g_object_get (G_OBJECT (element), "tu", &tu, NULL);
  fail_unless_equals_uint64 (tu, 2000000);

  g_object_get (G_OBJECT (element), "streamid", &streamid, NULL);
  fail_unless_equals_uint64 (streamid, 0xAABBCCDDEEFF0001);

  g_object_get (G_OBJECT (element), "processing-deadline", &processing_deadline,
      NULL);
  fail_unless_equals_uint64 (processing_deadline, 5000);

  g_object_get (G_OBJECT (element), "measurement-interval",
      &measurement_interval, NULL);
  fail_unless_equals_uint64 (measurement_interval, 125000);

  g_object_get (G_OBJECT (element), "max-interval-frames",
      &max_interval_frames, NULL);
  fail_unless_equals_uint64 (max_interval_frames, 3);

  gst_object_unref (element);
  gst_harness_teardown (h);
}

GST_END_TEST;
GST_START_TEST (test_payloader_single_and_fragment_edge)
{
  GstHarness *h;
  GstBuffer *in, *out;
  struct avtp_stream_pdu *pdu = alloca (AVTP_CVF_H264_HEADER_SIZE);
  const gint DATA_LEN_1 = sizeof (guint32) + 100;
  const gint DATA_LEN_2 = sizeof (guint32) + 100;
  const gint DATA_LEN_3 = sizeof (guint32) + 4;

  /* Create the 'expected' header */
  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 3000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 4000000);

  /* Create the harness for the avtpcvfpay. Setting mtu=128 ensures that
   * NAL units will be broken roughly at 100 bytes. More details below. */
  h = gst_harness_new_parse
      ("avtpcvfpay streamid=0xAABBCCDDEEFF0001 mtt=1000000 tu=1000000 mtu=128 processing-deadline=0");
  gst_harness_set_src_caps (h, generate_caps (4));

  /* Create a buffer to contain the multiple NAL units. This buffer
   * will hold two NAL units, with 100 and 101 bytes, each preceded
   * by a 4 bytes header */
  in = gst_harness_create_buffer (h, 100 + 101 + 2 * 4);
  add_nal (in, 100, 7, 0);
  add_nal (in, 101, 1, 104);
  GST_BUFFER_DTS (in) = 1000000;
  GST_BUFFER_PTS (in) = 2000000;

  /* We now push the buffer, and check if we get three from the avtpcvfpay */
  gst_harness_push (h, in);
  fail_unless (gst_harness_buffers_received (h) == 3);

  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_1);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  gst_buffer_unref (out);

  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_2);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 0);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  gst_buffer_unref (out);

  /* DATA_LEN_3 is 4 because only 98 bytes from the original NAL unit are
   * sent on the first buffer (due 2 bytes header), and the two remaining
   * bytes are preceded by the 2 bytes header. Note that the first byte of
   * the NAL is stripped before the fragmentation (see comment on
   * test_payloader_single_and_fragment below for more details). */
  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_3);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 3000000);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  gst_buffer_unref (out);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_payloader_single_and_fragment)
{
  GstHarness *h;
  GstBuffer *in, *out;
  struct avtp_stream_pdu *pdu = alloca (AVTP_CVF_H264_HEADER_SIZE);
  const gint DATA_LEN_1 = sizeof (guint32) + 4;
  const gint DATA_LEN_2 = sizeof (guint32) + 100;
  const gint DATA_LEN_3 = sizeof (guint32) + 100;
  const gint DATA_LEN_4 = sizeof (guint32) + 55;

  /* Create the 'expected' header */
  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 4000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 5000000);

  /* Create the harness for the avtpcvfpay. Setting mtu=128 ensures that
   * NAL units will be broken roughly at 100 bytes. More details below. */
  h = gst_harness_new_parse
      ("avtpcvfpay streamid=0xAABBCCDDEEFF0001 mtt=1000000 tu=1000000 processing-deadline=1000000 mtu=128");
  gst_harness_set_src_caps (h, generate_caps (4));

  /* Create a buffer to contain the multiple NAL units. This buffer
   * will hold two NAL units, with 4 and 250 bytes, each preceded
   * by a 4 bytes header */
  in = gst_harness_create_buffer (h, 4 + 250 + 2 * 4);
  add_nal (in, 4, 7, 0);
  add_nal (in, 250, 1, 8);
  GST_BUFFER_DTS (in) = 1000000;
  GST_BUFFER_PTS (in) = 2000000;

  /* We now push the buffer, and check if we get four from the avtpcvfpay */
  gst_harness_push (h, in);
  fail_unless (gst_harness_buffers_received (h) == 4);

  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_1);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  fail_unless (check_nal_filling (out, 0) == TRUE);
  gst_buffer_unref (out);

  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_2);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 0);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  fail_unless (check_nal_filling (out, 0) == TRUE);
  gst_buffer_unref (out);

  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_3);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 0);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  fail_unless (check_nal_filling (out, 98) == TRUE);
  gst_buffer_unref (out);

  /* For those wondering why DATA_LEN_4 is 55 and not 50 - or why
   * comment above states that NAL units are broken "roughly" at 100 bytes:
   * With mtu=128, there are only 100 bytes left for NAL units, so anything
   * bigger will be broken. But AVTP NAL units fragments have a header with
   * two bytes, so NAL units will use only 98 bytes. This leaves the last
   * fragment with 54 bytes. However, instead of being 56 (54 bytes plus
   * 2 bytes header), it is 55 (53 bytes plus 2 bytes header) due to the
   * fact that the first byte of the NAL unit (the NAL unit header) is
   * in fact stripped from the NAL unit before the fragmentation. */
  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_4);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 3);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 4000000);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  fail_unless (check_nal_filling (out, 196) == TRUE);
  gst_buffer_unref (out);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_payloader_multiple_single_2)
{
  GstHarness *h;
  GstBuffer *in, *out;
  struct avtp_stream_pdu *pdu = alloca (AVTP_CVF_H264_HEADER_SIZE);
  const gint DATA_LEN_1 = sizeof (guint32) + 32;
  const gint DATA_LEN_2 = sizeof (guint32) + 16;
  const gint DATA_LEN_3 = sizeof (guint32) + 8;

  /* Create the 'expected' header */
  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 3000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 4000000);

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse
      ("avtpcvfpay streamid=0xAABBCCDDEEFF0001 mtt=1000000 tu=1000000 processing-deadline=0");
  gst_harness_set_src_caps (h, generate_caps (2));

  /* Create a buffer to contain the multiple NAL units. This buffer
   * will hold three NAL units, with 32, 16 and 8 bytes, each preceded
   * by a 2 bytes header */
  in = gst_harness_create_buffer (h, 32 + 16 + 8 + 4 * 2);
  add_nal_2 (in, 32, 7, 0);
  add_nal_2 (in, 16, 7, 34);
  add_nal_2 (in, 8, 1, 52);
  GST_BUFFER_DTS (in) = 1000000;
  GST_BUFFER_PTS (in) = 2000000;

  /* We now push the buffer, and check if we get three from the avtpcvfpay */
  gst_harness_push (h, in);
  fail_unless (gst_harness_buffers_received (h) == 3);

  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_1);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  fail_unless (check_nal_filling (out, 0) == TRUE);
  gst_buffer_unref (out);

  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_2);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  fail_unless (check_nal_filling (out, 0) == TRUE);
  gst_buffer_unref (out);

  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_3);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  fail_unless (check_nal_filling (out, 0) == TRUE);
  gst_buffer_unref (out);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_payloader_multiple_single)
{
  GstHarness *h;
  GstBuffer *in, *out;
  struct avtp_stream_pdu *pdu = alloca (AVTP_CVF_H264_HEADER_SIZE);
  const gint DATA_LEN_1 = sizeof (guint32) + 32;
  const gint DATA_LEN_2 = sizeof (guint32) + 16;
  const gint DATA_LEN_3 = sizeof (guint32) + 8;

  /* Create the 'expected' header */
  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 3000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 4000000);

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse
      ("avtpcvfpay streamid=0xAABBCCDDEEFF0001 mtt=1000000 tu=1000000 processing-deadline=0");
  gst_harness_set_src_caps (h, generate_caps (4));

  /* Create a buffer to contain the multiple NAL units. This buffer
   * will hold three NAL units, with 32, 16 and 8 bytes, each preceded
   * by a 4 bytes header */
  in = gst_harness_create_buffer (h, 32 + 16 + 8 + 4 * 4);
  add_nal (in, 32, 7, 0);
  add_nal (in, 16, 7, 36);
  add_nal (in, 8, 1, 56);
  GST_BUFFER_DTS (in) = 1000000;
  GST_BUFFER_PTS (in) = 2000000;

  /* We now push the buffer, and check if we get three from the avtpcvfpay */
  gst_harness_push (h, in);
  fail_unless (gst_harness_buffers_received (h) == 3);

  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_1);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  fail_unless (check_nal_filling (out, 0) == TRUE);
  gst_buffer_unref (out);

  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_2);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  fail_unless (check_nal_filling (out, 0) == TRUE);
  gst_buffer_unref (out);

  out = gst_harness_pull (h);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN_3);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  fail_unless (check_nal_filling (out, 0) == TRUE);
  gst_buffer_unref (out);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_payloader_single)
{
  GstHarness *h;
  GstBuffer *in, *out;
  struct avtp_stream_pdu *pdu = alloca (AVTP_CVF_H264_HEADER_SIZE);
  const gint DATA_LEN = sizeof (guint32) + 4;

  /* Create the 'expected' header */
  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 3000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 4000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse
      ("avtpcvfpay streamid=0xAABBCCDDEEFF0001 mtt=1000000 tu=1000000 processing-deadline=0");
  gst_harness_set_src_caps (h, generate_caps (4));

  /* Buffer must have the nal len (4 bytes) and the nal (4 bytes) */
  in = gst_harness_create_buffer (h, 8);
  add_nal (in, 4, 1, 0);
  GST_BUFFER_DTS (in) = 1000000;
  GST_BUFFER_PTS (in) = 2000000;

  out = gst_harness_push_and_pull (h, in);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  gst_buffer_unref (out);

  /* Now test if, when nal_type is not vcl (not between 1 and 5), M is not set.
   * Also, as we're using the same element, seqnum should increase by one */
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);

  in = gst_harness_create_buffer (h, 8);
  add_nal (in, 4, 6, 0);
  GST_BUFFER_DTS (in) = 1000000;
  GST_BUFFER_PTS (in) = 2000000;

  out = gst_harness_push_and_pull (h, in);
  fail_unless (compare_h264_avtpdu (pdu, out) == TRUE);
  fail_unless (check_nal_filling (out, 0) == TRUE);
  gst_buffer_unref (out);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
avtpcvfpay_suite (void)
{
  Suite *s = suite_create ("avtpcvfpay");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_payloader_single);
  tcase_add_test (tc_chain, test_payloader_multiple_single);
  tcase_add_test (tc_chain, test_payloader_multiple_single_2);
  tcase_add_test (tc_chain, test_payloader_single_and_fragment);
  tcase_add_test (tc_chain, test_payloader_single_and_fragment_edge);
  tcase_add_test (tc_chain, test_payloader_incomplete_nal);
  tcase_add_test (tc_chain, test_payloader_invalid_caps);
  tcase_add_test (tc_chain, test_payloader_properties);
  tcase_add_test (tc_chain, test_payloader_no_codec_data);
  tcase_add_test (tc_chain, test_payloader_zero_sized_nal);
  tcase_add_test (tc_chain, test_payloader_downstream_eos);
  tcase_add_test (tc_chain, test_payloader_spread_ts);

  return s;
}

GST_CHECK_MAIN (avtpcvfpay);
