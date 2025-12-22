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
#define STREAM_ID 0xAABBCCDDEEFF0000

static gboolean
check_nal_filling (GstBuffer * buffer, guint8 first)
{
  GstMapInfo map;
  gboolean result = TRUE;
  gsize offset = 5;             /* 4 bytes for the nal size and one with nal type */
  int i;

  gst_buffer_map (buffer, &map, GST_MAP_READ);

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
fill_nal (guint8 * buf, gsize size, guint8 first)
{
  while (size--) {
    *buf++ = first++;
  }
}

static gsize
nal_size (GstBuffer * buffer)
{
  guint8 nal_size[4];

  gst_buffer_extract (buffer, 0, nal_size, 4);
  return GST_READ_UINT32_BE (nal_size);
}

static gsize
nal_type (GstBuffer * buffer)
{
  guint8 nal_type;

  gst_buffer_extract (buffer, 4, &nal_type, 1);
  return nal_type & 0x1f;
}

static GstBuffer *
fetch_nal (GstBuffer * buffer, gsize * offset)
{
  gsize nal_size;
  GstBuffer *ret;
  guint8 buf[4];

  if (*offset >= (gst_buffer_get_size (buffer) - 4))
    return NULL;

  gst_buffer_extract (buffer, *offset, buf, 4);
  nal_size = GST_READ_UINT32_BE (buf);

  ret =
      gst_buffer_copy_region (buffer, GST_BUFFER_COPY_MEMORY, *offset,
      nal_size + 4);
  *offset += nal_size + 4;

  return ret;
}

GST_START_TEST (test_depayloader_fragment_and_single)
{
  GstHarness *h;
  GstBuffer *in;
  const gint DATA_LEN = sizeof (guint32) + 10;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse ("avtpcvfdepay ! fakesink num-buffers=1");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 10);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  /* Start with a single NAL */
  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x1;
  gst_buffer_unmap (in, &map);

  /* We push a copy so that we can change only what is necessary on our buffer */
  fail_unless_equals_int (gst_harness_push (h, gst_buffer_copy (in)),
      GST_FLOW_OK);
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Then a fragment */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 3 << 5 | 28;    /* NAL type FU-A, NRI 3 */
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 7) | 4;       /* S = 1, type 4 */
  gst_buffer_unmap (in, &map);

  fail_unless_equals_int (gst_harness_push (h, gst_buffer_copy (in)),
      GST_FLOW_OK);

  /* Third and last AVTPDU, again a single NAL */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x1;
  gst_buffer_unmap (in, &map);

  fail_unless_equals_int (gst_harness_push (h, gst_buffer_copy (in)),
      GST_FLOW_EOS);

  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_depayloader_fragmented_two_start_eos)
{
  GstHarness *h;
  GstBuffer *in;
  const gint DATA_LEN = sizeof (guint32) + 10;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse ("avtpcvfdepay ! fakesink num-buffers=1");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 10);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  /* Start with a single NAL */
  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x1;
  gst_buffer_unmap (in, &map);

  /* We push a copy so that we can change only what is necessary on our buffer */
  fail_unless_equals_int (gst_harness_push (h, gst_buffer_copy (in)),
      GST_FLOW_OK);
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Then a fragment */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 3 << 5 | 28;    /* NAL type FU-A, NRI 3 */
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 7) | 4;       /* S = 1, type 4 */
  gst_buffer_unmap (in, &map);

  fail_unless_equals_int (gst_harness_push (h, gst_buffer_copy (in)),
      GST_FLOW_OK);

  /* Third and last AVTPDU, another fragment with start bit set */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 7) | 4;       /* S = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 16);
  gst_buffer_unmap (in, &map);

  fail_unless_equals_int (gst_harness_push (h, gst_buffer_copy (in)),
      GST_FLOW_EOS);

  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_depayloader_multiple_lost_eos)
{
  GstHarness *h;
  GstBuffer *in;
  const gint DATA_LEN = sizeof (guint32) + 4;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse ("avtpcvfdepay ! fakesink num-buffers=1");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU header */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 4);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x7;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  /* We push a copy so that we can change only what is necessary on our buffer */
  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send second AVTPDU, but skipping one seqnum */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x1;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  fail_unless_equals_int (gst_harness_push (h, gst_buffer_copy (in)),
      GST_FLOW_EOS);

  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_depayloader_fragmented_eos)
{
  GstHarness *h;
  GstBuffer *in;
  const gint DATA_LEN = sizeof (guint32) + 10;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse ("avtpcvfdepay ! fakesink num-buffers=1");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 10);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 3 << 5 | 28;    /* NAL type FU-A, NRI 3 */
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 7) | 4;       /* S = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 0);
  gst_buffer_unmap (in, &map);

  /* We push a copy so that we can change only what is necessary on our buffer */
  fail_unless_equals_int (gst_harness_push (h, gst_buffer_copy (in)),
      GST_FLOW_OK);
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send second and last AVTPDU */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 6) | 4;       /* E = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 16);
  gst_buffer_unmap (in, &map);

  fail_unless_equals_int (gst_harness_push (h, gst_buffer_copy (in)),
      GST_FLOW_EOS);

  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

/* Tests a big fragmented NAL scenario */
GST_START_TEST (test_depayloader_single_eos)
{
  GstHarness *h;
  GstBuffer *in;
  const gint DATA_LEN = sizeof (guint32) + 4;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse ("avtpcvfdepay ! fakesink num-buffers=1");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU header */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 4);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x1;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  fail_unless_equals_int (gst_harness_push (h, in), GST_FLOW_EOS);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_depayloader_invalid_avtpdu)
{
  GstHarness *h;
  GstBuffer *in, *small;
  const gint DATA_LEN = sizeof (guint32) + 4;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new ("avtpcvfdepay");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU header */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 4);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_MJPEG);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x1;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  /* Invalid CVF subtype */
  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid subtype */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_FORMAT_SUBTYPE,
      AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_pdu_set ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE,
      AVTP_SUBTYPE_CRF);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid CVF type */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_pdu_set ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE,
      AVTP_SUBTYPE_CVF);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_FORMAT, 0);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid AVTP version */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_FORMAT, AVTP_CVF_FORMAT_RFC);
  avtp_pdu_set ((struct avtp_common_pdu *) pdu, AVTP_FIELD_VERSION, 3);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid SV  */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_pdu_set ((struct avtp_common_pdu *) pdu, AVTP_FIELD_VERSION, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SV, 0);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid stream id  */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, 0xAABBCCDDEEFF0001);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid stream data len  */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, 100);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid NAL type (STAP-A)  */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 24;
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid NAL type (STAP-B)  */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 25;
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid NAL type (MTAP16)  */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 26;
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid NAL type (MTAP24)  */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 27;
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid NAL type (FU-B)  */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 29;
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid NAL type (STAP-A)  */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 24;
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid buffer size (too small to fit an AVTP header) */
  small = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE / 2);
  gst_harness_push (h, small);
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  /* Invalid buffer size (too small to fit a fragment header) */
  small = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 1);
  gst_buffer_map (small, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, sizeof (guint32) + 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 28;
  gst_buffer_unmap (small, &map);

  gst_harness_push (h, small);
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 0);

  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

/*
 * This test will send some invalid fragments, but with valid seqnum
 * (misbehaving payloader).*/
GST_START_TEST (test_depayloader_lost_fragments)
{
  GstHarness *h;
  GstBuffer *in, *out, *nal;
  const gint DATA_LEN = sizeof (guint32) + 10;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;
  gsize offset;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new ("avtpcvfdepay");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 10);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  /* First fragment doesn't have start bit set, so it should be ignored */
  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 3 << 5 | 28;    /* NAL type FU-A, NRI 3 */
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = 4;  /* S = 0, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 0);
  gst_buffer_unmap (in, &map);

  /* We push a copy so that we can change only what is necessary on our buffer */
  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send second AVTPDU - but this should be also ignored as it doesn't have the
   * start bit set */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = 4;  /* type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 8);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send third AVTPDU, with end bit set, but it should be discarded as there
   * was no start fragment */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 6) | 4;       /* E = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 16);
  gst_buffer_unmap (in, &map);

  /* Ensure no buffer came out */
  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Now, let's send an invalid one, with both start and end bits set */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 3);
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (3 << 6) | 4;       /* S = E = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 24);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send a fragment with proper start */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 4);
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 7) | 4;       /* S = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 32);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* But send start again. Previous one should be dropped */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 5);
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 7) | 4;       /* S = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 40);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Finally, send ending fragment. It should come out a buffer
   * whose content starts on 40 (starting of start fragment) */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 6);
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 6) | 4;       /* E = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 48);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));

  out = gst_harness_pull (h);
  fail_unless_equals_uint64 (GST_BUFFER_DTS (out), 1000000);
  fail_unless_equals_uint64 (GST_BUFFER_PTS (out), 2000000);

  /* NAL is composed of 8 bytes fragment + reconstructed NAL header, so 17 bytes */
  offset = 0;
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 17);
  fail_unless (check_nal_filling (nal, 40) == TRUE);
  fail_unless_equals_uint64 (nal_type (nal), 4);
  gst_buffer_unref (nal);

  /* Ensure no other NAL units are present */
  nal = fetch_nal (out, &offset);
  fail_unless (nal == NULL);

  gst_buffer_unref (out);
  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

/* This test jumps one seq_num, thus simulating a lost packet */
GST_START_TEST (test_depayloader_lost_packet)
{
  GstHarness *h;
  GstBuffer *in, *out, *nal;
  const gint DATA_LEN = sizeof (guint32) + 4;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;
  gsize offset;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new ("avtpcvfdepay");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU header */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 4);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x7;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  /* We push a copy so that we can change only what is necessary on our buffer */
  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send second AVTPDU */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send third and last AVTPDU, but jumping one SEQ_NUM.
   * This should make the first two NAL units to be flushed,
   * despite M not being set on this third packet.
   * Also, this NAL is not filled from 0, so if it somehow
   * leaks - it's not supposed to go outside of the avtpcvdepay
   * as it doesn't have M bit set - we can catch on checks below */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 3);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x1;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 5);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_int (gst_harness_buffers_received (h), 1);

  out = gst_harness_pull (h);
  fail_unless_equals_uint64 (GST_BUFFER_DTS (out), 1000000);
  fail_unless_equals_uint64 (GST_BUFFER_PTS (out), 2000000);

  /* Validate each NAL unit size and content */
  offset = 0;
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 4);
  fail_unless (check_nal_filling (nal, 0) == TRUE);
  fail_unless_equals_uint64 (nal_type (nal), 7);
  gst_buffer_unref (nal);

  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 4);
  fail_unless (check_nal_filling (nal, 0) == TRUE);
  fail_unless_equals_uint64 (nal_type (nal), 7);
  gst_buffer_unref (nal);

  /* Ensure no other NAL units are present */
  nal = fetch_nal (out, &offset);
  fail_unless (nal == NULL);

  gst_buffer_unref (out);
  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

/* This test simulates a scenario in which one single NAL unit is sent,
 * followed by a fragment without start bit set, so fragment is discarded
 * and previous single NAL is sent to the pipeline, as avtpcvfdepay is not
 * sure about the sanity of the data anymore - but hopes h264decoder knows
 * what to do. This scenario emerges from misbehaving payloaders. */
GST_START_TEST (test_depayloader_single_and_messed_fragments)
{
  GstHarness *h;
  GstBuffer *in, *out, *nal;
  const gint DATA_LEN = sizeof (guint32) + 4;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;
  gsize offset;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new ("avtpcvfdepay");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU header */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 4);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x1;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  /* First, we send a single NAL with M = 0, so nothing should come out */
  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Then, we send invalid fragment */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 3 << 5 | 28;    /* NAL type FU-A, NRI 3 */
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = 4;  /* S = 0, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 2, 0);
  gst_buffer_unmap (in, &map);

  /* When we push it, it should be discarded, but previous single NAL
   * should come out */
  out = gst_harness_push_and_pull (h, gst_buffer_copy (in));

  /* Check that we got the right one */
  fail_unless_equals_uint64 (GST_BUFFER_DTS (out), 1000000);
  fail_unless_equals_uint64 (GST_BUFFER_PTS (out), 2000000);

  offset = 0;
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 4);
  fail_unless_equals_uint64 (nal_type (nal), 1);
  fail_unless (check_nal_filling (nal, 0) == TRUE);
  gst_buffer_unref (nal);

  /* Ensure no other NAL units are present */
  nal = fetch_nal (out, &offset);
  fail_unless (nal == NULL);

  gst_buffer_unref (out);
  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

/* This test explores the case in which a fragment is followed by
 * a single NAL - and not by an ending fragment. Fragments stored
 * so far are dropped, and things shall flow normally for the single NAL.
 * This can be created by a misbehaving payloader */
GST_START_TEST (test_depayloader_single_and_messed_fragments_2)
{
  GstHarness *h;
  GstBuffer *in, *out, *nal;
  const gint DATA_LEN = sizeof (guint32) + 4;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;
  gsize offset;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new ("avtpcvfdepay");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU header */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 4);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 3 << 5 | 28;    /* NAL type FU-A, NRI 3 */
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 7) | 4;       /* S = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 2, 0);
  gst_buffer_unmap (in, &map);

  /* Send a perfectly valid start fragment */
  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Then, we send a single NAL. Previous fragment should be dropped */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x2;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 5);
  gst_buffer_unmap (in, &map);

  /* When we push it, it should come out as it has M = 1 */
  out = gst_harness_push_and_pull (h, gst_buffer_copy (in));

  /* Check that we got the right one - its NAL filling should start with 5 */
  fail_unless_equals_uint64 (GST_BUFFER_DTS (out), 1000000);
  fail_unless_equals_uint64 (GST_BUFFER_PTS (out), 2000000);

  offset = 0;
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 4);
  fail_unless_equals_uint64 (nal_type (nal), 2);
  fail_unless (check_nal_filling (nal, 5) == TRUE);
  gst_buffer_unref (nal);

  /* Ensure no other NAL units are present */
  nal = fetch_nal (out, &offset);
  fail_unless (nal == NULL);

  /* To be really sure, send an ending fragment. It should be dropped,
   * as there should not be any previous fragment on the wait */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 3 << 5 | 28;    /* NAL type FU-A, NRI 3 */
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 6) | 4;       /* E = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 2, 2);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  gst_buffer_unref (out);
  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

/* This test ensures that, if a fragment is dropped due arrival of a single
 * NAL (and fragment was never completed), any previous single NAL waiting
 * for M set NAL are flushed to the pipeline. avtpcvfdepay never sents known
 * incomplete NAL units to the pipeline, but should not hold forever NALs
 * waiting for an M set NAL - specially after something wrong already happened */
GST_START_TEST (test_depayloader_single_and_messed_fragments_3)
{
  GstHarness *h;
  GstBuffer *in, *out, *nal;
  const gint DATA_LEN = sizeof (guint32) + 4;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;
  gsize offset;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new ("avtpcvfdepay");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU header */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 4);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x2;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  /* Send a single NAL with M = 0, so nothing will come out */
  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send a valid start fragment */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 3 << 5 | 28;    /* NAL type FU-A, NRI 3 */
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 7) | 4;       /* S = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 2, 0);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send a single NAL without ending fragment. So, both first NAL and second should
   * come out, on two different buffers. Fragment should be gone. */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x3;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 7);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_uint64 (gst_harness_buffers_received (h), 2);

  /* Check that we got the right ones. First has nal_type 2, and second 3.
   * Second also has its nal filling starting from 7  */
  out = gst_harness_pull (h);
  fail_unless_equals_uint64 (GST_BUFFER_DTS (out), 1000000);
  fail_unless_equals_uint64 (GST_BUFFER_PTS (out), 2000000);

  offset = 0;
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 4);
  fail_unless_equals_uint64 (nal_type (nal), 2);
  fail_unless (check_nal_filling (nal, 0) == TRUE);
  gst_buffer_unref (nal);

  /* Ensure no other NAL units are present */
  nal = fetch_nal (out, &offset);
  fail_unless (nal == NULL);
  gst_buffer_unref (out);

  out = gst_harness_pull (h);
  fail_unless_equals_uint64 (GST_BUFFER_DTS (out), 1000000);
  fail_unless_equals_uint64 (GST_BUFFER_PTS (out), 2000000);

  offset = 0;
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 4);
  fail_unless_equals_uint64 (nal_type (nal), 3);
  fail_unless (check_nal_filling (nal, 7) == TRUE);
  gst_buffer_unref (nal);

  /* Ensure no other NAL units are present */
  nal = fetch_nal (out, &offset);
  fail_unless (nal == NULL);
  gst_buffer_unref (out);

  /* To be really sure, send an ending fragment. It should be dropped,
   * as there should not be any previous fragment on the wait */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 3);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 3 << 5 | 28;    /* NAL type FU-A, NRI 3 */
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 6) | 4;       /* E = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 2, 2);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_depayloader_property)
{
  GstHarness *h;
  GstElement *element;
  guint64 streamid;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new_parse ("avtpcvfdepay streamid=0xAABBCCDDEEFF0001");

  /* Check if property was properly set up */
  element = gst_harness_find_element (h, "avtpcvfdepay");
  g_object_get (G_OBJECT (element), "streamid", &streamid, NULL);
  fail_unless_equals_uint64 (streamid, 0xAABBCCDDEEFF0001);

  gst_object_unref (element);
  gst_harness_teardown (h);
}

GST_END_TEST;

/* Tests if everything goes right when a single NAL unit without M bit is
 * followed by fragments that, when merged, have the M bit set */
GST_START_TEST (test_depayloader_single_and_fragmented)
{
  GstHarness *h;
  GstBuffer *in, *out, *nal;
  const gint DATA_LEN = sizeof (guint32) + 4;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;
  gsize offset;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new ("avtpcvfdepay");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU header */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 4);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x1;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  /* First, we send a single NAL with M = 0, so nothing should come out */
  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Then, we send first fragment */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 3 << 5 | 28;    /* NAL type FU-A, NRI 3 */
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 7) | 4;       /* S = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 2, 0);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* And last */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 6) | 4;       /* E = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 2, 2);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_int (gst_harness_buffers_received (h), 1);

  out = gst_harness_pull (h);
  fail_unless_equals_uint64 (GST_BUFFER_DTS (out), 1000000);
  fail_unless_equals_uint64 (GST_BUFFER_PTS (out), 2000000);

  /* Validate each NAL unit size and content */
  offset = 0;
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 4);
  fail_unless (check_nal_filling (nal, 0) == TRUE);
  fail_unless_equals_uint64 (nal_type (nal), 1);
  gst_buffer_unref (nal);
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 5);
  fail_unless (check_nal_filling (nal, 0) == TRUE);
  fail_unless_equals_uint64 (nal_type (nal), 4);
  gst_buffer_unref (nal);

  /* Ensure no other NAL units are present */
  nal = fetch_nal (out, &offset);
  fail_unless (nal == NULL);

  gst_buffer_unref (out);
  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

/* Tests a simple fragmented NAL scenario */
GST_START_TEST (test_depayloader_fragmented)
{
  GstHarness *h;
  GstBuffer *in, *out, *nal;
  const gint DATA_LEN = sizeof (guint32) + 10;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;
  gsize offset;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new ("avtpcvfdepay");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 10);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 3 << 5 | 28;    /* NAL type FU-A, NRI 3 */
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 7) | 4;       /* S = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 0);
  gst_buffer_unmap (in, &map);

  /* We push a copy so that we can change only what is necessary on our buffer */
  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send second AVTPDU */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = 4;  /* type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 8);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send third and last AVTPDU */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 6) | 4;       /* E = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], 8, 16);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_int (gst_harness_buffers_received (h), 1);

  out = gst_harness_pull (h);
  fail_unless_equals_uint64 (GST_BUFFER_DTS (out), 1000000);
  fail_unless_equals_uint64 (GST_BUFFER_PTS (out), 2000000);

  offset = 0;
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 25);
  fail_unless (check_nal_filling (nal, 0) == TRUE);
  fail_unless_equals_uint64 (nal_type (nal), 4);
  gst_buffer_unref (nal);

  gst_buffer_unref (out);
  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

/* Tests a big fragmented NAL scenario */
GST_START_TEST (test_depayloader_fragmented_big)
{
  GstHarness *h;
  GstBuffer *in, *out, *nal;
  const gint DATA_LEN = 1470;
  struct avtp_stream_pdu *pdu;
  /* 12000 * 1468 > 2^24 - so we can check if nal size is retrieved correctly */
  const gint nal_count = 12000;
  guint8 seq_num = 0;
  GstMapInfo map;
  gsize offset;
  gint i;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new ("avtpcvfdepay");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + DATA_LEN);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN,
      DATA_LEN + sizeof (guint32));
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 3 << 5 | 28;    /* NAL type FU-A, NRI 3 */
  map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 7) | 4;       /* S = 1, type 4 */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], DATA_LEN - 2, 0);
  gst_buffer_unmap (in, &map);

  /* We push a copy so that we can change only what is necessary on our buffer */
  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Loop sending fragments. The idea is to create a NAL unit big enough
   * to use the 4 bytes of nal_length_size */
  for (i = 0; i < nal_count - 1; i++) {

    gst_buffer_map (in, &map, GST_MAP_READWRITE);
    pdu = (struct avtp_stream_pdu *) map.data;
    avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, ++seq_num);
    map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = 4;        /* type 4 */
    fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 2], DATA_LEN - 2,
        (guint8) ((DATA_LEN - 2) * seq_num));

    /* Last one is special - need to set M and TV, etc */
    if (i == nal_count - 2) {
      avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
      avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
      avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
      map.data[AVTP_CVF_H264_HEADER_SIZE + 1] = (1 << 6) | 4;   /* E = 1, type 4 */
    }

    gst_buffer_unmap (in, &map);

    gst_harness_push (h, gst_buffer_copy (in));
    if (i < nal_count - 2)
      fail_unless (gst_harness_try_pull (h) == NULL);
  }

  /* After last one was sent, we check everything */
  fail_unless_equals_int (gst_harness_buffers_received (h), 1);

  out = gst_harness_pull (h);
  fail_unless_equals_uint64 (GST_BUFFER_DTS (out), 1000000);
  fail_unless_equals_uint64 (GST_BUFFER_PTS (out), 2000000);

  offset = 0;
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), (DATA_LEN - 2) * nal_count + 1);
  fail_unless (check_nal_filling (nal, 0) == TRUE);
  fail_unless_equals_uint64 (nal_type (nal), 4);
  gst_buffer_unref (nal);

  gst_buffer_unref (out);
  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

/* Tests several single NAL units. They should be grouped and delivered
 * to the pipeline only when one NAL unit with M bit set arrives */
GST_START_TEST (test_depayloader_multiple_single)
{
  GstHarness *h;
  GstBuffer *in, *out, *nal;
  const gint DATA_LEN = sizeof (guint32) + 4;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;
  gsize offset;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new ("avtpcvfdepay");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU header */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 4);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 0);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x7;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  /* We push a copy so that we can change only what is necessary on our buffer */
  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send second AVTPDU */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 1);
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless (gst_harness_try_pull (h) == NULL);

  /* Send third and last AVTPDU */
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM, 2);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x1;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  gst_harness_push (h, gst_buffer_copy (in));
  fail_unless_equals_int (gst_harness_buffers_received (h), 1);

  out = gst_harness_pull (h);
  fail_unless_equals_uint64 (GST_BUFFER_DTS (out), 1000000);
  fail_unless_equals_uint64 (GST_BUFFER_PTS (out), 2000000);

  /* Validate each NAL unit size and content */
  offset = 0;
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 4);
  fail_unless (check_nal_filling (nal, 0) == TRUE);
  fail_unless_equals_uint64 (nal_type (nal), 7);
  gst_buffer_unref (nal);
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 4);
  fail_unless (check_nal_filling (nal, 0) == TRUE);
  fail_unless_equals_uint64 (nal_type (nal), 7);
  gst_buffer_unref (nal);
  nal = fetch_nal (out, &offset);
  fail_unless_equals_uint64 (nal_size (nal), 4);
  fail_unless (check_nal_filling (nal, 0) == TRUE);
  fail_unless_equals_uint64 (nal_type (nal), 1);
  gst_buffer_unref (nal);

  /* Ensure no other NAL units are present */
  nal = fetch_nal (out, &offset);
  fail_unless (nal == NULL);

  gst_buffer_unref (out);
  gst_buffer_unref (in);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_depayloader_single)
{
  GstHarness *h;
  GstBuffer *in, *out;
  const gint DATA_LEN = sizeof (guint32) + 4;
  struct avtp_stream_pdu *pdu;
  GstMapInfo map;

  /* Create the harness for the avtpcvfpay */
  h = gst_harness_new ("avtpcvfdepay");
  gst_harness_set_src_caps_str (h, "application/x-avtp");

  /* Create the input AVTPDU header */
  in = gst_harness_create_buffer (h, AVTP_CVF_H264_HEADER_SIZE + 4);
  gst_buffer_map (in, &map, GST_MAP_READWRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID, STREAM_ID);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, 1000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, 2000000);
  avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN, DATA_LEN);
  map.data[AVTP_CVF_H264_HEADER_SIZE] = 0x1;    /* Add NAL type */
  fill_nal (&map.data[AVTP_CVF_H264_HEADER_SIZE + 1], 3, 0);
  gst_buffer_unmap (in, &map);

  out = gst_harness_push_and_pull (h, in);

  fail_unless_equals_uint64 (GST_BUFFER_DTS (out), 1000000);
  fail_unless_equals_uint64 (GST_BUFFER_PTS (out), 2000000);
  fail_unless_equals_uint64 (nal_size (out), 4);
  fail_unless_equals_uint64 (nal_type (out), 1);
  fail_unless (check_nal_filling (out, 0) == TRUE);
  gst_buffer_unref (out);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
avtpcvfdepay_suite (void)
{
  Suite *s = suite_create ("avtpcvfdepay");
  TCase *tc_chain = tcase_create ("general");
  TCase *tc_slow = tcase_create ("slow");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_depayloader_single);
  tcase_add_test (tc_chain, test_depayloader_multiple_single);
  tcase_add_test (tc_chain, test_depayloader_fragmented);
  tcase_add_test (tc_chain, test_depayloader_single_and_fragmented);
  tcase_add_test (tc_chain, test_depayloader_property);
  tcase_add_test (tc_chain, test_depayloader_lost_packet);
  tcase_add_test (tc_chain, test_depayloader_lost_fragments);
  tcase_add_test (tc_chain, test_depayloader_single_and_messed_fragments);
  tcase_add_test (tc_chain, test_depayloader_single_and_messed_fragments_2);
  tcase_add_test (tc_chain, test_depayloader_single_and_messed_fragments_3);
  tcase_add_test (tc_chain, test_depayloader_invalid_avtpdu);
  tcase_add_test (tc_chain, test_depayloader_single_eos);
  tcase_add_test (tc_chain, test_depayloader_fragmented_eos);
  tcase_add_test (tc_chain, test_depayloader_fragmented_two_start_eos);
  tcase_add_test (tc_chain, test_depayloader_multiple_lost_eos);
  tcase_add_test (tc_chain, test_depayloader_fragment_and_single);

  suite_add_tcase (s, tc_slow);
  /* 'fragmented_big' may take some time to run, so give it a bit more time */
  tcase_set_timeout (tc_slow, 20);
  tcase_add_test (tc_slow, test_depayloader_fragmented_big);

  return s;
}

GST_CHECK_MAIN (avtpcvfdepay);
