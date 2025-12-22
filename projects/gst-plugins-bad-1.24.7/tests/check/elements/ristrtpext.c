/*
 * rtpristext.c
 *
 * Copyright (C) 2019 Net Insight AB
 *     Author: Olivier Crete <olivier.crete@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <gst/check/check.h>
#include <gst/rtp/rtp.h>

static const guint8 ts_packet[] = {
  0x47, 0x40, 0x41, 0x12, 0x00, 0x00, 0x01, 0xe0, 0x0f, 0x96, 0x81, 0xc0,
  0x0a, 0x31, 0x4d, 0x41, 0x0f, 0xbf, 0x11, 0x4d, 0x3f, 0x9a, 0x93, 0x00,
  0x00, 0x00, 0x01, 0x09, 0x30, 0x00, 0x00, 0x01, 0x41, 0x9a, 0x24, 0x6c,
  0x41, 0xaf, 0xfe, 0xda, 0xa6, 0x58, 0x00, 0x09, 0xcf, 0x64, 0x41, 0xf5,
  0x7c, 0x67, 0x65, 0x1d, 0x00, 0x23, 0xd3, 0x7c, 0xf2, 0xd2, 0xf8, 0x2f,
  0x30, 0x20, 0xfe, 0x2b, 0xad, 0x61, 0x0b, 0xd4, 0x47, 0x22, 0x82, 0x2a,
  0x46, 0xe2, 0xc3, 0x4c, 0x6a, 0xb4, 0x1d, 0x07, 0xc9, 0x77, 0x6c, 0xc9,
  0xc3, 0x6d, 0x37, 0x14, 0x86, 0x45, 0xb1, 0x0b, 0x44, 0xc4, 0xee, 0x03,
  0x95, 0xd6, 0x7f, 0x09, 0x54, 0x51, 0xb9, 0xcb, 0xe4, 0xea, 0x6b, 0xc9,
  0x2f, 0xfc, 0xa2, 0xb3, 0xef, 0x46, 0x86, 0xa0, 0xd9, 0x72, 0x93, 0x20,
  0xee, 0x5d, 0x31, 0xe2, 0xa1, 0x59, 0x9a, 0xbd, 0x17, 0x25, 0x77, 0x72,
  0x2d, 0xc4, 0xc4, 0x29, 0xf8, 0x6e, 0x36, 0x9c, 0xe8, 0x3f, 0x61, 0x3b,
  0x83, 0xc8, 0xc1, 0x0c, 0x53, 0xc9, 0xe1, 0x6a, 0x99, 0xcb, 0x0f, 0xb4,
  0x2f, 0x53, 0x30, 0x4a, 0xec, 0xec, 0x3d, 0xe4, 0x8f, 0x3c, 0xe3, 0xe4,
  0xec, 0x13, 0x18, 0x87, 0xed, 0xc4, 0x3f, 0xee, 0x26, 0xcf, 0xd4, 0x5b,
  0xfd, 0x1c, 0x32, 0x5f, 0xc5, 0xb9, 0xc0, 0x4b
};

static const guint8 null_ts_packet[] = {
  0x47, 0x1f, 0xff, 0x10, 0x55, 0x33, 0x41, 0xd8, 0x99, 0x92, 0x09, 0xc5,
  0xd9, 0x74, 0x2f, 0xaf, 0x61, 0xa6, 0xda, 0x36, 0x95, 0xac, 0x72, 0x82,
  0xa7, 0xda, 0xb9, 0x57, 0x91, 0x66, 0x6e, 0x64, 0xec, 0x75, 0xa4, 0x51,
  0x31, 0xac, 0x10, 0x4a, 0x33, 0xa6, 0xb9, 0x3f, 0x50, 0x7c, 0xb5, 0x81,
  0x57, 0x9c, 0x00, 0x32, 0x61, 0x77, 0x70, 0x4e, 0xe6, 0x95, 0x9b, 0xe3,
  0xe9, 0xd1, 0x9b, 0xa5, 0x81, 0xbc, 0x95, 0x03, 0x24, 0x7a, 0x60, 0x36,
  0x0d, 0xbf, 0x0d, 0xfd, 0x56, 0x7f, 0xec, 0x73, 0x47, 0x88, 0x5c, 0x52,
  0x77, 0x24, 0xdc, 0xcb, 0xba, 0x24, 0xc3, 0xbb, 0xa4, 0xa5, 0x2e, 0xd8,
  0x5b, 0x85, 0x0f, 0x98, 0x1d, 0xb6, 0xe4, 0xb2, 0x5c, 0x14, 0x57, 0x54,
  0xb2, 0xce, 0xe0, 0x76, 0x86, 0x0b, 0x90, 0xbf, 0x1b, 0x54, 0x98, 0x4f,
  0xae, 0x77, 0x18, 0x3d, 0x81, 0x10, 0x3e, 0xe6, 0x73, 0xf1, 0xb9, 0xed,
  0x5e, 0xde, 0x8b, 0xe0, 0x5f, 0x6b, 0xc7, 0xe8, 0x9b, 0xe6, 0x53, 0xf3,
  0xa0, 0x85, 0x13, 0xcb, 0x46, 0x56, 0x07, 0xe7, 0xfa, 0xb5, 0x3d, 0x5f,
  0xa4, 0x74, 0x4b, 0xf1, 0x84, 0xdb, 0x94, 0xb4, 0xd7, 0x25, 0x99, 0xa3,
  0xbe, 0xcb, 0x11, 0x5d, 0xcb, 0x69, 0xe0, 0xb5, 0xd1, 0xda, 0x50, 0x24,
  0xca, 0x96, 0x09, 0x23, 0xcb, 0x1f, 0xbe, 0x00
};

static GstBuffer *
alloc_ts_buffer (guint num_ts_packets)
{
  GstBuffer *buf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  buf = gst_rtp_buffer_new_allocate (188 * num_ts_packets, 0, 0);
  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);
  gst_rtp_buffer_set_version (&rtp, 2);
  gst_rtp_buffer_set_ssrc (&rtp, 12);
  gst_rtp_buffer_set_seq (&rtp, 44);
  gst_rtp_buffer_set_timestamp (&rtp, 55);
  gst_rtp_buffer_set_payload_type (&rtp, 33);
  gst_rtp_buffer_unmap (&rtp);

  return buf;
}

static void
validate_ts_buffer_full (GstRTPBuffer * rtp, guint num_ts_packets, guint16 seq,
    gboolean extension)
{
  fail_unless_equals_int (gst_rtp_buffer_get_payload_len (rtp),
      188 * num_ts_packets);
  fail_unless_equals_int (gst_rtp_buffer_get_ssrc (rtp), 12);
  fail_unless_equals_int (gst_rtp_buffer_get_payload_type (rtp), 33);
  fail_unless_equals_int (gst_rtp_buffer_get_seq (rtp), seq);
  fail_unless_equals_int (gst_rtp_buffer_get_timestamp (rtp), 55);
  fail_unless_equals_int (gst_rtp_buffer_get_extension (rtp), extension);
}

static void
validate_ts_buffer_seq (GstRTPBuffer * rtp, guint num_ts_packets, guint16 seq)
{
  validate_ts_buffer_full (rtp, num_ts_packets, seq, TRUE);
}

static void
validate_ts_buffer (GstRTPBuffer * rtp, guint num_ts_packets)
{
  validate_ts_buffer_full (rtp, num_ts_packets, 44, TRUE);
}

static void
validate_ts_buffer_noext (GstRTPBuffer * rtp, guint num_ts_packets)
{
  validate_ts_buffer_full (rtp, num_ts_packets, 44, FALSE);
}

static void
validate_ext (GstRTPBuffer * rtp, gboolean wanted_has_drop_null,
    gboolean wanted_has_seqnum_ext, guint wanted_orig_ts_packet_count,
    guint wanted_ts_packet_size, guint wanted_npd_bits, guint16 wanted_ext)
{
  guint extlen;
  gpointer extdata;
  guint16 bits;
  gboolean has_seqnum_ext;
  gboolean has_drop_null;
  guint orig_ts_packet_count;
  gboolean ts_packet_size;
  guint8 *data;
  guint8 npd_bits;

  fail_unless (gst_rtp_buffer_get_extension_data (rtp, &bits, &extdata,
          &extlen));

  fail_unless_equals_int (bits, 'R' << 8 | 'I');
  fail_unless (extlen == 1);

  data = extdata;

  has_drop_null = (data[0] >> 7) & 1;   /* N */
  has_seqnum_ext = (data[0] >> 6) & 1;  /* E */
  orig_ts_packet_count = (data[0] >> 3) & 7;    /* Size */
  ts_packet_size = ((data[1] >> 7) & 1) ? 204 : 188;
  npd_bits = data[1] & 0x7F;

  fail_unless_equals_int (has_drop_null, wanted_has_drop_null);
  fail_unless_equals_int (has_seqnum_ext, wanted_has_seqnum_ext);
  fail_unless_equals_int (orig_ts_packet_count, wanted_orig_ts_packet_count);
  fail_unless_equals_int (ts_packet_size, wanted_ts_packet_size);
  fail_unless_equals_int (npd_bits, wanted_npd_bits);

  if (wanted_has_seqnum_ext) {
    guint16 ext;

    ext = GST_READ_UINT16_BE (data + 2);
    fail_unless_equals_int (ext, wanted_ext);
  }
}

/* Don't touch this */
GST_START_TEST (test_noop)
{
  GstHarness *h = gst_harness_new ("ristrtpext");
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *ibuf;
  GstBuffer *obuf;
  guint i;
  guint8 *payload;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer (7);
  gst_rtp_buffer_map (ibuf, GST_MAP_READWRITE, &rtp);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 7; i++) {
    memcpy (payload + (i * 188), ts_packet, 188);
    payload[(i * 188) + 187] = i;
  }
  gst_rtp_buffer_unmap (&rtp);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 7);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 7; i++) {
    fail_unless_equals_int (memcmp (payload + (188 * i), ts_packet, 187), 0);
    fail_unless_equals_int (payload[(i * 188) + 187], i);
  }
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

/* No null packets */
GST_START_TEST (test_remove_null_none)
{
  GstHarness *h = gst_harness_new ("ristrtpext");
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *ibuf;
  GstBuffer *obuf;
  guint i;
  guint8 *payload;

  g_object_set (h->element, "drop-null-ts-packets", TRUE, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer (7);
  gst_rtp_buffer_map (ibuf, GST_MAP_READWRITE, &rtp);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 7; i++) {
    memcpy (payload + (i * 188), ts_packet, 188);
    payload[(i * 188) + 187] = i;
  }
  gst_rtp_buffer_unmap (&rtp);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer (&rtp, 7);
  validate_ext (&rtp, TRUE, FALSE, 7, 188, 0, 0);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 7; i++) {
    fail_unless_equals_int (memcmp (payload + (188 * i), ts_packet, 187), 0);
    fail_unless_equals_int (payload[(i * 188) + 187], i);
  }
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

/* 1 null packet in middle */
GST_START_TEST (test_remove_null_middle)
{
  GstHarness *h = gst_harness_new ("ristrtpext");
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *ibuf;
  GstBuffer *obuf;
  guint i;
  guint8 *payload;

  g_object_set (h->element, "drop-null-ts-packets", TRUE, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer (7);
  gst_rtp_buffer_map (ibuf, GST_MAP_READWRITE, &rtp);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 3; i++) {
    memcpy (payload + (i * 188), ts_packet, 188);
    payload[(i * 188) + 187] = i;
  }
  memcpy (payload + (3 * 188), null_ts_packet, 188);
  for (i = 4; i < 7; i++) {
    memcpy (payload + (i * 188), ts_packet, 188);
    payload[(i * 188) + 187] = i;
  }
  gst_rtp_buffer_unmap (&rtp);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer (&rtp, 6);
  validate_ext (&rtp, TRUE, FALSE, 7, 188, 1 << 3, 0);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 6; i++) {
    fail_unless_equals_int (memcmp (payload + (188 * i), ts_packet, 187), 0);
    if (i < 3) {
      fail_unless_equals_int (payload[(i * 188) + 187], i);
    } else {
      fail_unless_equals_int (payload[(i * 188) + 187], i + 1);
    }
  }

  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

/* one null packet at the start and one in the end */
GST_START_TEST (test_remove_null_start_and_end)
{
  GstHarness *h = gst_harness_new ("ristrtpext");
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *ibuf;
  GstBuffer *obuf;
  guint i;
  guint8 *payload;

  g_object_set (h->element, "drop-null-ts-packets", TRUE, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer (7);
  gst_rtp_buffer_map (ibuf, GST_MAP_READWRITE, &rtp);
  payload = gst_rtp_buffer_get_payload (&rtp);
  memcpy (payload + (0 * 188), null_ts_packet, 188);
  for (i = 1; i < 6; i++) {
    memcpy (payload + (i * 188), ts_packet, 188);
    payload[(i * 188) + 187] = i;
  }
  memcpy (payload + (6 * 188), null_ts_packet, 188);
  gst_rtp_buffer_unmap (&rtp);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer (&rtp, 5);
  validate_ext (&rtp, TRUE, FALSE, 7, 188, 1 << 6 | 1, 0);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 5; i++) {
    fail_unless_equals_int (memcmp (payload + (188 * i), ts_packet, 187), 0);
    fail_unless_equals_int (payload[(i * 188) + 187], i + 1);
  }
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;


/* All null packets */
GST_START_TEST (test_remove_null_all)
{
  GstHarness *h = gst_harness_new ("ristrtpext");
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *ibuf;
  GstBuffer *obuf;
  guint i;
  guint8 *payload;

  g_object_set (h->element, "drop-null-ts-packets", TRUE, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer (7);
  gst_rtp_buffer_map (ibuf, GST_MAP_READWRITE, &rtp);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 7; i++) {
    memcpy (payload + (i * 188), null_ts_packet, 188);
    payload[(i * 188) + 187] = i;
  }
  gst_rtp_buffer_unmap (&rtp);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer (&rtp, 0);
  validate_ext (&rtp, TRUE, FALSE, 7, 188, 0x7F, 0);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_remove_null_not_ts)
{
  GstHarness *h = gst_harness_new ("ristrtpext");
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *ibuf;
  GstBuffer *obuf;

  g_object_set (h->element, "drop-null-ts-packets", TRUE, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer (7);
  gst_rtp_buffer_map (ibuf, GST_MAP_READWRITE, &rtp);
  gst_rtp_buffer_set_payload_type (&rtp, 96);
  gst_rtp_buffer_unmap (&rtp);

  obuf = gst_harness_push_and_pull (h, ibuf);

  fail_unless (obuf == ibuf);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;


/* Eight null packets */
GST_START_TEST (test_remove_null_all_8_packets)
{
  GstHarness *h = gst_harness_new ("ristrtpext");
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *ibuf;
  GstBuffer *obuf;
  guint i;
  guint8 *payload;

  g_object_set (h->element, "drop-null-ts-packets", TRUE, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer (8);
  gst_rtp_buffer_map (ibuf, GST_MAP_READWRITE, &rtp);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 8; i++) {
    memcpy (payload + (i * 188), null_ts_packet, 188);
    payload[(i * 188) + 187] = i;
  }
  gst_rtp_buffer_unmap (&rtp);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer (&rtp, 1);
  validate_ext (&rtp, TRUE, FALSE, 0, 188, 0x7F, 0);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

/* 204 bytes null packets */
GST_START_TEST (test_remove_null_all_204bytes)
{
  GstHarness *h = gst_harness_new ("ristrtpext");
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *ibuf;
  GstBuffer *obuf;
  guint i;
  guint8 *payload;

  g_object_set (h->element, "drop-null-ts-packets", TRUE, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer (7);
  gst_buffer_append_memory (ibuf, gst_allocator_alloc (NULL, (204 - 188) * 7,
          NULL));
  gst_rtp_buffer_map (ibuf, GST_MAP_READWRITE, &rtp);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 7; i++) {
    memcpy (payload + (i * 204), null_ts_packet, 188);
    payload[(i * 204) + 187] = i;
  }
  gst_rtp_buffer_unmap (&rtp);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer (&rtp, 0);
  validate_ext (&rtp, TRUE, FALSE, 7, 204, 0x7F, 0);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

/* 2 null packet out of 3 */
GST_START_TEST (test_remove_null_two_of_three)
{
  GstHarness *h = gst_harness_new ("ristrtpext");
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *ibuf;
  GstBuffer *obuf;
  guint8 *payload;

  g_object_set (h->element, "drop-null-ts-packets", TRUE, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer (3);
  gst_rtp_buffer_map (ibuf, GST_MAP_READWRITE, &rtp);
  payload = gst_rtp_buffer_get_payload (&rtp);
  memcpy (payload + (0 * 188), null_ts_packet, 188);
  memcpy (payload + (1 * 188), ts_packet, 188);
  payload[188 + 187] = 33;
  memcpy (payload + (2 * 188), null_ts_packet, 188);

  gst_rtp_buffer_unmap (&rtp);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer (&rtp, 1);
  validate_ext (&rtp, TRUE, FALSE, 3, 188, 1 << 6 | 1 << 4, 0);
  payload = gst_rtp_buffer_get_payload (&rtp);
  fail_unless_equals_int (memcmp (payload, ts_packet, 187), 0);
  fail_unless_equals_int (payload[187], 33);

  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

static void
push_one_seqnum (GstHarness * h, guint16 seqnum, guint16 wanted_ext)
{
  GstBuffer *ibuf;
  GstBuffer *obuf;
  static const int NUM_PACKETS = 5;
  guint i;
  guint8 *payload;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  ibuf = alloc_ts_buffer (NUM_PACKETS);
  gst_rtp_buffer_map (ibuf, GST_MAP_READWRITE, &rtp);
  gst_rtp_buffer_set_seq (&rtp, seqnum);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < NUM_PACKETS; i++) {
    memcpy (payload + (i * 188), ts_packet, 188);
    payload[(i * 188) + 187] = i;
  }

  gst_rtp_buffer_unmap (&rtp);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_seq (&rtp, NUM_PACKETS, seqnum);
  validate_ext (&rtp, FALSE, TRUE, 0, 188, 0, wanted_ext);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < NUM_PACKETS; i++) {
    fail_unless_equals_int (memcmp (payload + (188 * i), ts_packet, 187), 0);
    fail_unless_equals_int (payload[(i * 188) + 187], i);
  }
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);
}

GST_START_TEST (test_add_seqnum_ext)
{
  GstHarness *h = gst_harness_new ("ristrtpext");

  g_object_set (h->element, "sequence-number-extension", TRUE, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  push_one_seqnum (h, 44, 0);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_add_seqnum_ext_roll_over)
{
  GstHarness *h = gst_harness_new ("ristrtpext");

  g_object_set (h->element, "sequence-number-extension", TRUE, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  /* push one */
  push_one_seqnum (h, 0xA123, 0);

  /* Now roll over */
  push_one_seqnum (h, 0x0123, 1);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_add_seqnum_ext_roll_back)
{
  GstHarness *h = gst_harness_new ("ristrtpext");

  g_object_set (h->element, "sequence-number-extension", TRUE, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  /* Send one packet */
  push_one_seqnum (h, 0xA123, 0);

  /* Now roll over */
  push_one_seqnum (h, 0x0123, 1);

  /* Now roll back */
  push_one_seqnum (h, 0xF123, 0);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_add_seqnum_ext_roll_over_twice)
{
  GstHarness *h = gst_harness_new ("ristrtpext");

  g_object_set (h->element, "sequence-number-extension", TRUE, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  /* Send one packet */
  push_one_seqnum (h, 0xF123, 0);

  /* Now roll over */
  push_one_seqnum (h, 0x2123, 1);

  /* Now go foward */
  push_one_seqnum (h, 0x9123, 1);

  /* Now roll back */
  push_one_seqnum (h, 0x0123, 2);

  gst_harness_teardown (h);
}

GST_END_TEST;


static GstBuffer *
alloc_ts_buffer_with_ext (guint num_ts_packets, gboolean has_drop_null,
    gboolean has_seqnum_ext, guint orig_ts_packet_count,
    guint ts_packet_size, guint npd_bits, guint16 extseq)
{
  GstBuffer *buf = alloc_ts_buffer (num_ts_packets);
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 *payload;
  guint8 *data;
  guint i;

  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);
  gst_rtp_buffer_set_extension_data (&rtp, 'R' << 8 | 'I', 1);
  gst_rtp_buffer_get_extension_data (&rtp, NULL, (void **) &data, NULL);

  data[0] = has_drop_null << 7; /* N */
  data[0] |= has_seqnum_ext << 6;       /* E */
  data[0] |= (MIN (orig_ts_packet_count, 7) & 7) << 2;  /* Size */
  data[1] = (ts_packet_size == 204) << 7;       /* T */
  data[1] |= (npd_bits & 0x7F);

  if (has_seqnum_ext && extseq != 0xFFFF)
    GST_WRITE_UINT16_BE (data + 2, extseq);
  else
    GST_WRITE_UINT16_BE (data + 2, 0);


  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < num_ts_packets; i++) {
    memcpy (payload + (i * 188), ts_packet, 188);
    payload[(i * 188) + 187] = i;
  }

  gst_rtp_buffer_unmap (&rtp);

  return buf;
}

GST_START_TEST (test_deext_noop)
{
  GstHarness *h = gst_harness_new ("ristrtpdeext");
  GstBuffer *ibuf, *obuf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer_with_ext (7, FALSE, FALSE, 7, 188, 0, 0);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 7);
  gst_rtp_buffer_unmap (&rtp);

  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_deext_restore_middle)
{
  GstHarness *h = gst_harness_new ("ristrtpdeext");
  GstBuffer *ibuf, *obuf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 *payload;
  guint i;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer_with_ext (6, TRUE, FALSE, 7, 188, 1 << 3, 0);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 7);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 7; i++) {
    if (i < 3) {
      fail_unless_equals_int (memcmp (payload + (188 * i), ts_packet, 187), 0);
      fail_unless_equals_int (payload[(i * 188) + 187], i);
    } else if (i > 3) {
      fail_unless_equals_int (memcmp (payload + (188 * i), ts_packet, 187), 0);
      fail_unless_equals_int (payload[(i * 188) + 187], i - 1);
    } else {
      fail_unless_equals_int (memcmp (payload + (188 * i), null_ts_packet, 4),
          0);
    }
  }
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;


GST_START_TEST (test_deext_restore_start_and_end)
{
  GstHarness *h = gst_harness_new ("ristrtpdeext");
  GstBuffer *ibuf, *obuf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 *payload;
  guint i;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer_with_ext (5, TRUE, FALSE, 7, 188, 1 << 6 | 1, 0);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 7);
  payload = gst_rtp_buffer_get_payload (&rtp);
  fail_unless_equals_int (memcmp (payload, null_ts_packet, 4), 0);
  for (i = 1; i < 6; i++) {
    fail_unless_equals_int (memcmp (payload + (188 * i), ts_packet, 187), 0);
    fail_unless_equals_int (payload[(i * 188) + 187], i - 1);
  }
  fail_unless_equals_int (memcmp (payload + (188 * 6), null_ts_packet, 4), 0);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_deext_restore_middle_no_origcnt)
{
  GstHarness *h = gst_harness_new ("ristrtpdeext");
  GstBuffer *ibuf, *obuf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 *payload;
  guint i;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer_with_ext (6, TRUE, FALSE, 0, 188, 1 << 3, 0);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 7);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 7; i++) {
    if (i < 3) {
      fail_unless_equals_int (memcmp (payload + (188 * i), ts_packet, 187), 0);
      fail_unless_equals_int (payload[(i * 188) + 187], i);
    } else if (i > 3) {
      fail_unless_equals_int (memcmp (payload + (188 * i), ts_packet, 187), 0);
      fail_unless_equals_int (payload[(i * 188) + 187], i - 1);
    } else {
      fail_unless_equals_int (memcmp (payload + (188 * i), null_ts_packet, 4),
          0);
    }
  }
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_deext_restore_all)
{
  GstHarness *h = gst_harness_new ("ristrtpdeext");
  GstBuffer *ibuf, *obuf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 *payload;
  guint i;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer_with_ext (0, TRUE, FALSE, 7, 188, 0x7F, 0);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 7);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 7; i++)
    fail_unless_equals_int (memcmp (payload + (188 * i), null_ts_packet, 4), 0);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_deext_restore_all_8)
{
  GstHarness *h = gst_harness_new ("ristrtpdeext");
  GstBuffer *ibuf, *obuf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 *payload;
  guint i;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer_with_ext (1, TRUE, FALSE, 0, 188, 0x7F, 0);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 8);
  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 7; i++)
    fail_unless_equals_int (memcmp (payload + (188 * i), null_ts_packet, 4), 0);
  fail_unless_equals_int (memcmp (payload + (188 * i), ts_packet, 187), 0);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_deext_restore_all_204bytes)
{
  GstHarness *h = gst_harness_new ("ristrtpdeext");
  GstBuffer *ibuf, *obuf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 *payload;
  guint i;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer_with_ext (0, TRUE, FALSE, 7, 204, 0x7F, 0);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);

  fail_unless_equals_int (gst_rtp_buffer_get_payload_len (&rtp), 204 * 7);
  fail_unless_equals_int (gst_rtp_buffer_get_ssrc (&rtp), 12);
  fail_unless_equals_int (gst_rtp_buffer_get_payload_type (&rtp), 33);
  fail_unless_equals_int (gst_rtp_buffer_get_seq (&rtp), 44);
  fail_unless_equals_int (gst_rtp_buffer_get_timestamp (&rtp), 55);
  fail_unless_equals_int (gst_rtp_buffer_get_extension (&rtp), FALSE);

  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < 7; i++)
    fail_unless_equals_int (memcmp (payload + (204 * i), null_ts_packet, 4), 0);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;


GST_START_TEST (test_deext_restore_empty)
{
  GstHarness *h = gst_harness_new ("ristrtpdeext");
  GstBuffer *ibuf, *obuf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer_with_ext (0, TRUE, FALSE, 0, 188, 0, 0);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 0);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;


GST_START_TEST (test_deext_restore_invalid_origcnt)
{
  GstHarness *h = gst_harness_new ("ristrtpdeext");
  GstBuffer *ibuf, *obuf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 *payload;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer_with_ext (2, TRUE, FALSE, 5, 188, 1 << 6 | 1 << 4, 0);

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 4);
  payload = gst_rtp_buffer_get_payload (&rtp);

  fail_unless_equals_int (memcmp (payload + (188 * 0), null_ts_packet, 4), 0);

  fail_unless_equals_int (memcmp (payload + (188 * 1), ts_packet, 187), 0);
  fail_unless_equals_int (payload[(188 * 1) + 187], 0);

  fail_unless_equals_int (memcmp (payload + (188 * 2), null_ts_packet, 4), 0);

  fail_unless_equals_int (memcmp (payload + (188 * 3), ts_packet, 187), 0);
  fail_unless_equals_int (payload[(188 * 3) + 187], 1);

  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;


GST_START_TEST (test_deext_noop_invalid_size)
{
  GstHarness *h = gst_harness_new ("ristrtpdeext");
  GstBuffer *ibuf, *obuf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer_with_ext (7, FALSE, FALSE, 7, 188, 0, 0);

  gst_buffer_append_memory (ibuf, gst_allocator_alloc (NULL, 5, NULL));

  obuf = gst_harness_push_and_pull (h, ibuf);

  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);

  fail_unless_equals_int (gst_rtp_buffer_get_payload_len (&rtp), 188 * 7 + 5);
  fail_unless_equals_int (gst_rtp_buffer_get_ssrc (&rtp), 12);
  fail_unless_equals_int (gst_rtp_buffer_get_payload_type (&rtp), 33);
  fail_unless_equals_int (gst_rtp_buffer_get_seq (&rtp), 44);
  fail_unless_equals_int (gst_rtp_buffer_get_timestamp (&rtp), 55);
  fail_unless_equals_int (gst_rtp_buffer_get_extension (&rtp), FALSE);

  gst_rtp_buffer_unmap (&rtp);

  gst_buffer_unref (obuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_deext_seq_base)
{
  GstHarness *h = gst_harness_new ("ristrtpdeext");
  GstBuffer *ibuf, *obuf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint max_seqnum;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer_with_ext (7, FALSE, TRUE, 7, 188, 0, 0);
  obuf = gst_harness_push_and_pull (h, ibuf);
  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 7);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  g_object_get (h->element, "max-ext-seqnum", &max_seqnum, NULL);
  fail_unless_equals_int (max_seqnum, 44);

  ibuf = alloc_ts_buffer_with_ext (7, FALSE, TRUE, 7, 188, 0, 1);
  obuf = gst_harness_push_and_pull (h, ibuf);
  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 7);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  g_object_get (h->element, "max-ext-seqnum", &max_seqnum, NULL);
  fail_unless_equals_int (max_seqnum, 65536 + 44);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_deext_seq_drop)
{
  GstHarness *h = gst_harness_new ("ristrtpdeext");
  GstBuffer *ibuf, *obuf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint max_seqnum;

  gst_harness_set_src_caps_str (h, "application/x-rtp, payload=33,"
      "clock-rate=90000, encoding-name=MP2T");

  ibuf = alloc_ts_buffer_with_ext (7, FALSE, TRUE, 7, 188, 0, 0);
  obuf = gst_harness_push_and_pull (h, ibuf);
  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 7);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  g_object_get (h->element, "max-ext-seqnum", &max_seqnum, NULL);
  fail_unless_equals_int (max_seqnum, 44);


  ibuf = alloc_ts_buffer_with_ext (7, FALSE, TRUE, 7, 188, 0, 2);
  obuf = gst_harness_push_and_pull (h, ibuf);
  gst_rtp_buffer_map (obuf, GST_MAP_READ, &rtp);
  validate_ts_buffer_noext (&rtp, 7);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (obuf);

  g_object_get (h->element, "max-ext-seqnum", &max_seqnum, NULL);
  fail_unless_equals_int (max_seqnum, 65536 + 65536 + 44);

  ibuf = alloc_ts_buffer_with_ext (7, FALSE, TRUE, 7, 188, 0, 0);
  fail_unless_equals_int (gst_harness_push (h, ibuf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 0);

  g_object_get (h->element, "max-ext-seqnum", &max_seqnum, NULL);
  fail_unless_equals_int (max_seqnum, 65536 + 65536 + 44);

  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
ristrtpext_suite (void)
{
  Suite *s = suite_create ("ristrtpext");
  TCase *tc;

  tc = tcase_create ("ext");
  suite_add_tcase (s, tc);

  tcase_add_test (tc, test_noop);
  tcase_add_test (tc, test_remove_null_none);
  tcase_add_test (tc, test_remove_null_middle);
  tcase_add_test (tc, test_remove_null_start_and_end);
  tcase_add_test (tc, test_remove_null_all);
  tcase_add_test (tc, test_remove_null_not_ts);
  tcase_add_test (tc, test_remove_null_all_8_packets);
  tcase_add_test (tc, test_remove_null_all_204bytes);
  tcase_add_test (tc, test_remove_null_two_of_three);

  tcase_add_test (tc, test_add_seqnum_ext);
  tcase_add_test (tc, test_add_seqnum_ext_roll_over);
  tcase_add_test (tc, test_add_seqnum_ext_roll_back);
  tcase_add_test (tc, test_add_seqnum_ext_roll_over_twice);

  tc = tcase_create ("deext");
  suite_add_tcase (s, tc);

  tcase_add_test (tc, test_deext_noop);
  tcase_add_test (tc, test_deext_restore_middle);
  tcase_add_test (tc, test_deext_restore_start_and_end);
  tcase_add_test (tc, test_deext_restore_middle_no_origcnt);
  tcase_add_test (tc, test_deext_restore_all);
  tcase_add_test (tc, test_deext_restore_all_8);
  tcase_add_test (tc, test_deext_restore_all_204bytes);
  tcase_add_test (tc, test_deext_restore_empty);
  tcase_add_test (tc, test_deext_restore_invalid_origcnt);
  tcase_add_test (tc, test_deext_noop_invalid_size);

  tcase_add_test (tc, test_deext_seq_base);
  tcase_add_test (tc, test_deext_seq_drop);

  return s;
}

GST_CHECK_MAIN (ristrtpext);
