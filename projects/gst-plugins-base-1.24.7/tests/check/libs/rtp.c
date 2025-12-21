/* GStreamer unit tests for the RTP support library
 *
 * Copyright (C) 2007 Tim-Philipp Müller <tim centricular net>
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

#include <gst/check/gstcheck.h>

#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtphdrext.h>
#include <gst/rtp/gstrtcpbuffer.h>
#include <string.h>

#define RTP_HEADER_LEN 12

static GstBuffer *create_feedback_buffer (gboolean with_padding);

GST_START_TEST (test_rtp_buffer)
{
  GstBuffer *buf;
  GstMapInfo map;
  guint8 *data;
  gsize size;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  /* check GstRTPHeader structure alignment and packing */
  buf = gst_rtp_buffer_new_allocate (16, 4, 0);
  fail_unless (buf != NULL);
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  data = map.data;
  size = map.size;
  fail_unless_equals_int (size, RTP_HEADER_LEN + 16 + 4);

  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);

  /* check defaults */
  fail_unless_equals_int (gst_rtp_buffer_get_version (&rtp), 2);
  fail_unless (gst_rtp_buffer_get_padding (&rtp) == TRUE);
  fail_unless (gst_rtp_buffer_get_extension (&rtp) == FALSE);
  fail_unless_equals_int (gst_rtp_buffer_get_csrc_count (&rtp), 0);
  fail_unless (gst_rtp_buffer_get_marker (&rtp) == FALSE);
  fail_unless (gst_rtp_buffer_get_payload_type (&rtp) == 0);
  fail_unless_equals_int (GST_READ_UINT16_BE (data), 0xa000);

  /* check version in bitfield */
  gst_rtp_buffer_set_version (&rtp, 3);
  fail_unless_equals_int (gst_rtp_buffer_get_version (&rtp), 3);
  fail_unless_equals_int ((data[0] & 0xC0) >> 6, 3);
  gst_rtp_buffer_set_version (&rtp, 2);
  fail_unless_equals_int (gst_rtp_buffer_get_version (&rtp), 2);
  fail_unless_equals_int ((data[0] & 0xC0) >> 6, 2);

  /* check padding bit */
  gst_rtp_buffer_set_padding (&rtp, TRUE);
  fail_unless (gst_rtp_buffer_get_padding (&rtp) == TRUE);
  fail_unless_equals_int ((data[0] & 0x20) >> 5, 1);
  gst_rtp_buffer_set_padding (&rtp, FALSE);
  fail_unless (gst_rtp_buffer_get_padding (&rtp) == FALSE);
  fail_unless_equals_int ((data[0] & 0x20) >> 5, 0);

  /* check marker bit */
  gst_rtp_buffer_set_marker (&rtp, TRUE);
  fail_unless (gst_rtp_buffer_get_marker (&rtp) == TRUE);
  fail_unless_equals_int ((data[1] & 0x80) >> 7, 1);
  gst_rtp_buffer_set_marker (&rtp, FALSE);
  fail_unless (gst_rtp_buffer_get_marker (&rtp) == FALSE);
  fail_unless_equals_int ((data[1] & 0x80) >> 7, 0);

  /* check sequence offset */
  gst_rtp_buffer_set_seq (&rtp, 0xF2C9);
  fail_unless_equals_int (gst_rtp_buffer_get_seq (&rtp), 0xF2C9);
  fail_unless_equals_int (GST_READ_UINT16_BE (data + 2), 0xF2C9);
  gst_rtp_buffer_set_seq (&rtp, 0);
  fail_unless_equals_int (gst_rtp_buffer_get_seq (&rtp), 0);
  fail_unless_equals_int (GST_READ_UINT16_BE (data + 2), 0);

  /* check timestamp offset */
  gst_rtp_buffer_set_timestamp (&rtp, 432191);
  fail_unless_equals_int (GST_READ_UINT32_BE (data + 4), 432191);
  fail_unless_equals_int (gst_rtp_buffer_get_timestamp (&rtp), 432191);
  gst_rtp_buffer_set_timestamp (&rtp, 0);
  fail_unless_equals_int (gst_rtp_buffer_get_timestamp (&rtp), 0);
  fail_unless_equals_int (GST_READ_UINT32_BE (data + 4), 0);

  /* check ssrc offset */
  gst_rtp_buffer_set_ssrc (&rtp, 0xf04043C2);
  fail_unless_equals_int (gst_rtp_buffer_get_ssrc (&rtp), (gint) 0xf04043c2);
  fail_unless_equals_int (GST_READ_UINT32_BE (data + 4 + 4), (gint) 0xf04043c2);
  gst_rtp_buffer_set_ssrc (&rtp, 0);
  fail_unless_equals_int (gst_rtp_buffer_get_ssrc (&rtp), 0);
  fail_unless_equals_int (GST_READ_UINT32_BE (data + 4 + 4), 0);

  /* check csrc bits */
  fail_unless_equals_int (gst_rtp_buffer_get_csrc_count (&rtp), 0);
  ASSERT_CRITICAL (gst_rtp_buffer_get_csrc (&rtp, 0));
  fail_unless_equals_int (data[0] & 0xf, 0);

  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unmap (buf, &map);
  gst_buffer_unref (buf);

  /* and again, this time with CSRCs */
  buf = gst_rtp_buffer_new_allocate (16, 4, 3);
  fail_unless (buf != NULL);
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  data = map.data;
  size = map.size;
  fail_unless_equals_int (size, RTP_HEADER_LEN + 16 + 4 + 4 * 3);

  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);

  fail_unless_equals_int (gst_rtp_buffer_get_csrc_count (&rtp), 3);
  ASSERT_CRITICAL (gst_rtp_buffer_get_csrc (&rtp, 3));
  fail_unless_equals_int (data[0] & 0xf, 3);
  fail_unless_equals_int (gst_rtp_buffer_get_csrc (&rtp, 0), 0);
  fail_unless_equals_int (gst_rtp_buffer_get_csrc (&rtp, 1), 0);
  fail_unless_equals_int (gst_rtp_buffer_get_csrc (&rtp, 2), 0);
  fail_unless_equals_int (gst_rtp_buffer_get_header_len (&rtp),
      RTP_HEADER_LEN + 4 * 3);
  fail_unless_equals_int (gst_rtp_buffer_get_payload_len (&rtp), 16);

  data += RTP_HEADER_LEN;       /* skip the other header stuff */
  gst_rtp_buffer_set_csrc (&rtp, 0, 0xf7c0);
  fail_unless_equals_int (GST_READ_UINT32_BE (data + 0 * 4), 0xf7c0);
  gst_rtp_buffer_set_csrc (&rtp, 1, 0xf7c1);
  fail_unless_equals_int (GST_READ_UINT32_BE (data + 1 * 4), 0xf7c1);
  gst_rtp_buffer_set_csrc (&rtp, 2, 0xf7c2);
  fail_unless_equals_int (GST_READ_UINT32_BE (data + 2 * 4), 0xf7c2);
  ASSERT_CRITICAL (gst_rtp_buffer_set_csrc (&rtp, 3, 0xf123));

  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unmap (buf, &map);
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_rtp_buffer_validate_corrupt)
{
  GstBuffer *buf;
  guint8 corrupt_rtp_packet[58] = {
    0x90, 0x7a, 0xbf, 0x28, 0x3a, 0x8a, 0x0a, 0xf4, 0x69, 0x6b, 0x76, 0xc0,
    0x21, 0xe0, 0xe0, 0x60, 0x81, 0x10, 0x84, 0x30, 0x21, 0x52, 0x06, 0xc2,
    0xb8, 0x30, 0x10, 0x4c, 0x08, 0x62, 0x67, 0xc2, 0x6e, 0x1a, 0x53, 0x3f,
    0xaf, 0xd6, 0x1b, 0x29, 0x40, 0xe0, 0xa5, 0x83, 0x01, 0x4b, 0x04, 0x02,
    0xb0, 0x97, 0x63, 0x08, 0x10, 0x4b, 0x43, 0x85, 0x37, 0x2c
  };
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  buf = gst_buffer_new_and_alloc (sizeof (corrupt_rtp_packet));
  gst_buffer_fill (buf, 0, corrupt_rtp_packet, sizeof (corrupt_rtp_packet));
  fail_if (gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp));
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_rtp_buffer_validate_padding)
{
  GstBuffer *buf;
  guint8 packet_with_padding[] = {
    0xa0, 0x60, 0x6c, 0x49, 0x58, 0xab, 0xaa, 0x65, 0x65, 0x2e, 0xaf, 0xce,
    0x68, 0xce, 0x3c, 0x80, 0x00, 0x00, 0x00, 0x04
  };
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  buf = gst_buffer_new_and_alloc (sizeof (packet_with_padding));
  gst_buffer_fill (buf, 0, packet_with_padding, sizeof (packet_with_padding));
  fail_unless (gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp));
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (buf);

  /* Set the padding to something invalid */
  buf = gst_buffer_new_and_alloc (sizeof (packet_with_padding));
  gst_buffer_fill (buf, 0, packet_with_padding, sizeof (packet_with_padding));
  gst_buffer_memset (buf, gst_buffer_get_size (buf) - 1, 0xff, 1);
  fail_if (gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp));

  memset (&rtp, 0, sizeof (rtp));
  fail_unless (gst_rtp_buffer_map (buf, GST_MAP_READ |
          GST_RTP_BUFFER_MAP_FLAG_SKIP_PADDING, &rtp));
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (buf);
}

GST_END_TEST;

#if 0
GST_START_TEST (test_rtp_buffer_list)
{
  GstBuffer *rtp_header;
  GstBuffer *rtp_payload;
  GstBufferList *list = NULL;
  GstBufferListIterator *it;
  guint i;

  list = gst_buffer_list_new ();
  it = gst_buffer_list_iterate (list);

  /* Creating a list of two RTP packages */

  /* Create first group to hold the rtp header and the payload */
  gst_buffer_list_iterator_add_group (it);
  rtp_header = gst_rtp_buffer_new_allocate (0, 0, 0);
  gst_buffer_list_iterator_add (it, rtp_header);
  rtp_payload = gst_buffer_new_and_alloc (42);
  gst_buffer_list_iterator_add (it, rtp_payload);

  /* Create second group to hold an rtp header and a payload */
  gst_buffer_list_iterator_add_group (it);
  rtp_header = gst_rtp_buffer_new_allocate (0, 0, 0);
  gst_buffer_list_iterator_add (it, rtp_header);
  rtp_payload = gst_buffer_new_and_alloc (42);
  gst_buffer_list_iterator_add (it, rtp_payload);

  gst_buffer_list_iterator_free (it);

  /* Test SEQ number */
  i = gst_rtp_buffer_list_set_seq (list, 1024);
  fail_if (1026 != i);
  fail_if (!gst_rtp_buffer_list_validate (list));

  /* Timestamp */
  gst_rtp_buffer_list_set_timestamp (list, 432191);
  fail_unless_equals_int (gst_rtp_buffer_list_get_timestamp (list), 432191);

  /* SSRC */
  gst_rtp_buffer_list_set_ssrc (list, 0xf04043C2);
  fail_unless_equals_int (gst_rtp_buffer_list_get_ssrc (list),
      (gint) 0xf04043c2);

  /* Payload type */
  gst_rtp_buffer_list_set_payload_type (list, 127);
  fail_unless_equals_int (gst_rtp_buffer_list_get_payload_type (list), 127);

  gst_buffer_list_unref (list);
}

GST_END_TEST;
#endif

GST_START_TEST (test_rtp_buffer_set_extension_data)
{
  GstBuffer *buf;
  guint8 *data;
  guint16 bits;
  guint size;
  guint8 misc_data[4] = { 1, 2, 3, 4 };
  gpointer pointer;
  guint8 appbits;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  /* check GstRTPHeader structure alignment and packing */
  buf = gst_rtp_buffer_new_allocate (4, 0, 0);

  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);

  /* should be possible to set the extension data */
  fail_unless (gst_rtp_buffer_set_extension_data (&rtp, 270, 4) == TRUE);
  fail_unless (gst_rtp_buffer_get_extension (&rtp) == TRUE);
  gst_rtp_buffer_get_extension_data (&rtp, &bits, &pointer, &size);
  fail_unless (bits == 270);
  fail_unless (size == 4);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (buf);

  buf = gst_rtp_buffer_new_allocate (20, 0, 0);
  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);

  fail_unless (gst_rtp_buffer_get_extension (&rtp) == FALSE);
  fail_unless (gst_rtp_buffer_set_extension_data (&rtp, 333, 2) == TRUE);
  fail_unless (gst_rtp_buffer_get_extension (&rtp) == TRUE);
  gst_rtp_buffer_get_extension_data (&rtp, &bits, &pointer, &size);
  fail_unless (bits == 333);
  fail_unless (size == 2);

  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (buf);

  /* Test header extensions with a one byte header */
  buf = gst_rtp_buffer_new_allocate (20, 0, 0);
  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);

  fail_unless (gst_rtp_buffer_get_extension (&rtp) == FALSE);

  fail_unless (gst_rtp_buffer_add_extension_onebyte_header (&rtp, 5,
          misc_data, 2) == TRUE);
  fail_unless (gst_rtp_buffer_get_extension_data (&rtp, &bits, &pointer,
          &size));
  fail_unless (bits == 0xBEDE);
  fail_unless (size == 1);
  data = (guint8 *) pointer;
  fail_unless (data[0] == ((5 << 4) | 1));
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 2,
          1, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 5,
          1, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 5,
          0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);

  fail_unless (gst_rtp_buffer_add_extension_onebyte_header (&rtp, 5,
          misc_data, 4) == TRUE);
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 5,
          0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 5,
          1, &pointer, &size) == TRUE);
  fail_unless (size == 4);
  fail_unless (memcmp (pointer, misc_data, 4) == 0);
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 5,
          2, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 2,
          1, &pointer, &size) == FALSE);

  fail_unless (gst_rtp_buffer_add_extension_onebyte_header (&rtp, 6,
          misc_data, 2) == TRUE);
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 5,
          0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 5,
          1, &pointer, &size) == TRUE);
  fail_unless (size == 4);
  fail_unless (memcmp (pointer, misc_data, 4) == 0);
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 5,
          3, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 2,
          1, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 6,
          2, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 5,
          0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);

  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (buf);

  /* Test header extensions with a two bytes header */
  buf = gst_rtp_buffer_new_allocate (20, 0, 0);
  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);

  fail_unless (gst_rtp_buffer_get_extension (&rtp) == FALSE);

  fail_unless (gst_rtp_buffer_add_extension_twobytes_header (&rtp, 0, 5,
          misc_data, 2) == TRUE);
  fail_unless (gst_rtp_buffer_get_extension_data (&rtp, &bits, &pointer,
          &size));
  fail_unless (bits == 0x100 << 4);
  fail_unless (size == 1);
  data = (guint8 *) pointer;
  fail_unless (data[0] == 5);
  fail_unless (data[1] == 2);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 2,
          0, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 5,
          1, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 5,
          0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);

  fail_unless (gst_rtp_buffer_add_extension_twobytes_header (&rtp, 0, 5,
          misc_data, 4) == TRUE);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 5,
          0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 5,
          1, &pointer, &size) == TRUE);
  fail_unless (size == 4);
  fail_unless (memcmp (pointer, misc_data, 4) == 0);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 5,
          2, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 2,
          0, &pointer, &size) == FALSE);

  fail_unless (gst_rtp_buffer_add_extension_twobytes_header (&rtp, 0, 6,
          misc_data, 2) == TRUE);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 5,
          0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 5,
          1, &pointer, &size) == TRUE);
  fail_unless (size == 4);
  fail_unless (memcmp (pointer, misc_data, 4) == 0);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 5,
          2, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 2,
          0, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 6,
          1, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_get_extension_twobytes_header (&rtp, &appbits, 5,
          0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);

  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_rtp_buffer_set_extension_data_shrink_data)
{
  GstBuffer *buf;
  guint16 bits;
  guint size;
  gpointer pointer;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 scratch_cmp[4 * 4] = { 0, };
  GstMapInfo info = GST_MAP_INFO_INIT;
  gsize i;

  buf = gst_rtp_buffer_new_allocate (20, 0, 0);
  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);

  fail_unless (gst_rtp_buffer_set_extension_data (&rtp, 270, 4) == TRUE);
  fail_unless (gst_rtp_buffer_get_extension (&rtp) == TRUE);
  gst_rtp_buffer_get_extension_data (&rtp, &bits, &pointer, &size);
  GST_MEMDUMP ("", pointer, size * 4);
  fail_unless (bits == 270);
  fail_unless (size == 4);
  for (i = 0; i < size * 4; i++) {
    guint8 *bytes = pointer;
    bytes[i] = i;
  }
  memcpy (scratch_cmp, pointer, size * 4);
  fail_unless_equals_int64 ((guint64) gst_buffer_get_size (buf), 52);
  gst_rtp_buffer_unmap (&rtp);

  /* ensure that the mapped buffer size matches */
  gst_buffer_map (buf, &info, GST_MAP_READ);
  GST_MEMDUMP ("", info.data, info.size);
  fail_unless_equals_int64 ((guint64) info.size, 52);
  gst_buffer_unmap (buf, &info);

  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);
  /* shrinking the extension data should still succeed and only output the
   * relevant data */
  fail_unless (gst_rtp_buffer_set_extension_data (&rtp, 180, 2) == TRUE);
  gst_rtp_buffer_get_extension_data (&rtp, &bits, &pointer, &size);
  GST_MEMDUMP ("", pointer, size * 4);
  fail_unless (bits == 180);
  fail_unless (size == 2);
  fail_unless_equals_int64 ((guint64) gst_buffer_get_size (buf), 44);
  for (i = 0; i < 8; i++) {
    guint8 *ext_data = pointer;
    fail_unless_equals_int_hex (ext_data[i], scratch_cmp[i]);
  }
  gst_rtp_buffer_unmap (&rtp);

  gst_buffer_map (buf, &info, GST_MAP_READ);
  GST_MEMDUMP ("", info.data, info.size);
  fail_unless_equals_int64 ((guint64) info.size, 44);
  gst_buffer_unmap (buf, &info);

  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);
  fail_unless (gst_rtp_buffer_set_extension_data (&rtp, 308, 3) == TRUE);
  gst_rtp_buffer_get_extension_data (&rtp, &bits, &pointer, &size);
  GST_MEMDUMP ("", pointer, size * 4);
  fail_unless (bits == 308);
  fail_unless (size == 3);
  for (i = 0; i < 8; i++) {
    guint8 *ext_data = pointer;
    fail_unless_equals_int_hex (ext_data[i], scratch_cmp[i]);
  }
  /* new data will be zero-initialized */
  for (i = 8; i < size * 4; i++) {
    guint8 *ext_data = pointer;
    fail_unless_equals_int_hex (ext_data[i], 0);
  }
  fail_unless_equals_int64 ((guint64) gst_buffer_get_size (buf), 48);
  gst_rtp_buffer_unmap (&rtp);

  gst_buffer_map (buf, &info, GST_MAP_READ);
  GST_MEMDUMP ("", info.data, info.size);
  fail_unless_equals_int64 ((guint64) info.size, 48);
  gst_buffer_unmap (buf, &info);
  gst_buffer_unref (buf);
}

GST_END_TEST;

#if 0
GST_START_TEST (test_rtp_buffer_list_set_extension)
{
  GstBufferList *list;
  GstBuffer *buf;
  guint8 *data;
  guint16 bits;
  guint size;
  guint8 misc_data[4] = { 1, 2, 3, 4 };
  gpointer pointer;
  guint8 appbits;
  GstBufferListIterator *it;

  buf = gst_rtp_buffer_new_allocate (20, 0, 0);
  list = gst_rtp_buffer_list_from_buffer (buf);
  gst_buffer_unref (buf);

  it = gst_buffer_list_iterate (list);
  fail_unless (gst_buffer_list_iterator_next_group (it));
  buf = gst_buffer_list_iterator_next (it);
  fail_unless (buf != NULL);
  fail_unless (GST_BUFFER_SIZE (buf) == 12);
  buf = gst_buffer_list_iterator_next (it);
  fail_unless (buf != NULL);
  fail_unless (GST_BUFFER_SIZE (buf) == 20);
  gst_buffer_list_iterator_free (it);

  /* Test header extensions with a one byte header */
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 2,
          1, &pointer, &size) == FALSE);

  it = gst_buffer_list_iterate (list);
  fail_unless (gst_buffer_list_iterator_next_group (it));
  fail_unless (gst_buffer_list_iterator_next (it) != NULL);
  fail_unless (gst_rtp_buffer_list_add_extension_onebyte_header (it, 5,
          misc_data, 2) == TRUE);
  gst_buffer_list_iterator_free (it);
  it = gst_buffer_list_iterate (list);
  fail_unless (gst_buffer_list_iterator_next_group (it));
  buf = gst_buffer_list_iterator_next (it);
  fail_unless (gst_rtp_buffer_get_extension_data (buf, &bits, &pointer, &size));
  fail_unless (bits == 0xBEDE);
  fail_unless (size == 1);
  data = (guint8 *) pointer;
  fail_unless (data[0] == ((5 << 4) | 1));
  gst_buffer_list_iterator_free (it);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 2,
          0, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 5,
          1, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 5,
          0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);

  it = gst_buffer_list_iterate (list);
  fail_unless (gst_buffer_list_iterator_next_group (it));
  fail_unless (gst_buffer_list_iterator_next (it) != NULL);
  fail_unless (gst_rtp_buffer_list_add_extension_onebyte_header (it, 5,
          misc_data, 4) == TRUE);
  gst_buffer_list_iterator_free (it);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 5,
          0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 5,
          1, &pointer, &size) == TRUE);
  fail_unless (size == 4);
  fail_unless (memcmp (pointer, misc_data, 4) == 0);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 5,
          2, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 2,
          0, &pointer, &size) == FALSE);

  it = gst_buffer_list_iterate (list);
  fail_unless (gst_buffer_list_iterator_next_group (it));
  fail_unless (gst_buffer_list_iterator_next (it) != NULL);
  fail_unless (gst_rtp_buffer_list_add_extension_onebyte_header (it, 6,
          misc_data, 2) == TRUE);
  gst_buffer_list_iterator_free (it);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 5,
          0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 5,
          1, &pointer, &size) == TRUE);
  fail_unless (size == 4);
  fail_unless (memcmp (pointer, misc_data, 4) == 0);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 5,
          2, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 2,
          0, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 6,
          1, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_list_get_extension_onebyte_header (list, 0, 5,
          0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);
  gst_buffer_list_unref (list);


  /* Test header extensions with a two bytes header */
  buf = gst_rtp_buffer_new_allocate (20, 0, 0);
  list = gst_rtp_buffer_list_from_buffer (buf);
  gst_buffer_unref (buf);

  it = gst_buffer_list_iterate (list);
  fail_unless (gst_buffer_list_iterator_next_group (it));
  fail_unless (gst_buffer_list_iterator_next (it) != NULL);
  fail_unless (gst_rtp_buffer_list_add_extension_twobytes_header (it, 0, 5,
          misc_data, 2) == TRUE);
  gst_buffer_list_iterator_free (it);

  it = gst_buffer_list_iterate (list);
  fail_unless (gst_buffer_list_iterator_next_group (it));
  buf = gst_buffer_list_iterator_next (it);
  fail_unless (gst_rtp_buffer_get_extension_data (buf, &bits, &pointer, &size));
  fail_unless (bits == 0x100 << 4);
  fail_unless (size == 1);
  data = (guint8 *) pointer;
  fail_unless (data[0] == 5);
  fail_unless (data[1] == 2);
  gst_buffer_list_iterator_free (it);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 2, 0, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 5, 1, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 5, 0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);

  it = gst_buffer_list_iterate (list);
  fail_unless (gst_buffer_list_iterator_next_group (it));
  fail_unless (gst_buffer_list_iterator_next (it) != NULL);
  fail_unless (gst_rtp_buffer_list_add_extension_twobytes_header (it, 0, 5,
          misc_data, 4) == TRUE);
  gst_buffer_list_iterator_free (it);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 5, 0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 5, 1, &pointer, &size) == TRUE);
  fail_unless (size == 4);
  fail_unless (memcmp (pointer, misc_data, 4) == 0);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 5, 2, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 2, 0, &pointer, &size) == FALSE);

  it = gst_buffer_list_iterate (list);
  fail_unless (gst_buffer_list_iterator_next_group (it));
  fail_unless (gst_buffer_list_iterator_next (it) != NULL);
  fail_unless (gst_rtp_buffer_list_add_extension_twobytes_header (it, 0, 6,
          misc_data, 2) == TRUE);
  gst_buffer_list_iterator_free (it);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 5, 0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 5, 1, &pointer, &size) == TRUE);
  fail_unless (size == 4);
  fail_unless (memcmp (pointer, misc_data, 4) == 0);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 5, 2, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 2, 0, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 6, 1, &pointer, &size) == FALSE);
  fail_unless (gst_rtp_buffer_list_get_extension_twobytes_header (list, 0,
          &appbits, 5, 0, &pointer, &size) == TRUE);
  fail_unless (size == 2);
  fail_unless (memcmp (pointer, misc_data, 2) == 0);

  gst_buffer_list_unref (list);
}

GST_END_TEST;
#endif

GST_START_TEST (test_rtp_seqnum_compare)
{
#define ASSERT_COMP(a,b,c) fail_unless (gst_rtp_buffer_compare_seqnum ((guint16)a,(guint16)b) == c);
  ASSERT_COMP (0xfffe, 0xfffd, -1);
  ASSERT_COMP (0xffff, 0xfffe, -1);
  ASSERT_COMP (0x0000, 0xffff, -1);
  ASSERT_COMP (0x0001, 0x0000, -1);
  ASSERT_COMP (0x0002, 0x0001, -1);

  ASSERT_COMP (0xffff, 0xfffd, -2);
  ASSERT_COMP (0x0000, 0xfffd, -3);
  ASSERT_COMP (0x0001, 0xfffd, -4);
  ASSERT_COMP (0x0002, 0xfffd, -5);

  ASSERT_COMP (0x7ffe, 0x7ffd, -1);
  ASSERT_COMP (0x7fff, 0x7ffe, -1);
  ASSERT_COMP (0x8000, 0x7fff, -1);
  ASSERT_COMP (0x8001, 0x8000, -1);
  ASSERT_COMP (0x8002, 0x8001, -1);

  ASSERT_COMP (0x7fff, 0x7ffd, -2);
  ASSERT_COMP (0x8000, 0x7ffd, -3);
  ASSERT_COMP (0x8001, 0x7ffd, -4);
  ASSERT_COMP (0x8002, 0x7ffd, -5);

  ASSERT_COMP (0x7ffd, 0xffff, -0x7ffe);
  ASSERT_COMP (0x7ffe, 0x0000, -0x7ffe);
  ASSERT_COMP (0x7fff, 0x0001, -0x7ffe);
  ASSERT_COMP (0x7fff, 0x0000, -0x7fff);
  ASSERT_COMP (0x8000, 0x0001, -0x7fff);
  ASSERT_COMP (0x8001, 0x0002, -0x7fff);

  ASSERT_COMP (0xfffd, 0x7ffe, -0x7fff);
  ASSERT_COMP (0xfffe, 0x7fff, -0x7fff);
  ASSERT_COMP (0xffff, 0x8000, -0x7fff);
  ASSERT_COMP (0x0000, 0x8001, -0x7fff);
  ASSERT_COMP (0x0001, 0x8002, -0x7fff);

  ASSERT_COMP (0xfffe, 0x7ffe, -0x8000);
  ASSERT_COMP (0xffff, 0x7fff, -0x8000);
  ASSERT_COMP (0x0000, 0x8000, -0x8000);
  ASSERT_COMP (0x0001, 0x8001, -0x8000);

  ASSERT_COMP (0x7ffe, 0xfffe, -0x8000);
  ASSERT_COMP (0x7fff, 0xffff, -0x8000);
  ASSERT_COMP (0x8000, 0x0000, -0x8000);
  ASSERT_COMP (0x8001, 0x0001, -0x8000);

  ASSERT_COMP (0x0001, 0x0002, 1);
  ASSERT_COMP (0x0000, 0x0001, 1);
  ASSERT_COMP (0xffff, 0x0000, 1);
  ASSERT_COMP (0xfffe, 0xffff, 1);
  ASSERT_COMP (0xfffd, 0xfffe, 1);

  ASSERT_COMP (0x0000, 0x0002, 2);
  ASSERT_COMP (0xffff, 0x0002, 3);
  ASSERT_COMP (0xfffe, 0x0002, 4);
  ASSERT_COMP (0xfffd, 0x0002, 5);

  ASSERT_COMP (0x8001, 0x8002, 1);
  ASSERT_COMP (0x8000, 0x8001, 1);
  ASSERT_COMP (0x7fff, 0x8000, 1);
  ASSERT_COMP (0x7ffe, 0x7fff, 1);
  ASSERT_COMP (0x7ffd, 0x7ffe, 1);

  ASSERT_COMP (0x8000, 0x8002, 2);
  ASSERT_COMP (0x7fff, 0x8002, 3);
  ASSERT_COMP (0x7ffe, 0x8002, 4);
  ASSERT_COMP (0x7ffd, 0x8002, 5);

  ASSERT_COMP (0xfffe, 0x7ffd, 0x7fff);
  ASSERT_COMP (0xffff, 0x7ffe, 0x7fff);
  ASSERT_COMP (0x0000, 0x7fff, 0x7fff);
  ASSERT_COMP (0x0001, 0x8000, 0x7fff);
  ASSERT_COMP (0x0002, 0x8001, 0x7fff);

  ASSERT_COMP (0x7ffe, 0xfffd, 0x7fff);
  ASSERT_COMP (0x7fff, 0xfffe, 0x7fff);
  ASSERT_COMP (0x8000, 0xffff, 0x7fff);
  ASSERT_COMP (0x8001, 0x0000, 0x7fff);
  ASSERT_COMP (0x8002, 0x0001, 0x7fff);
#undef ASSERT_COMP
}

GST_END_TEST;

GST_START_TEST (test_rtcp_sdes_type)
{
  GstRTCPSDESType i;

  for (i = 1; i < GST_RTCP_SDES_MID; i++) {
    GstRTCPSDESType sdes_type;
    const char *sdes_name;

    sdes_name = gst_rtcp_sdes_type_to_name (i);
    GST_DEBUG ("%u (0x%x) -> \'%s\'", i, i, sdes_name);
    fail_unless (sdes_name != NULL);
    sdes_type = gst_rtcp_sdes_name_to_type (sdes_name);

    fail_unless_equals_int (sdes_type, i);
  }
}

GST_END_TEST;

GST_START_TEST (test_rtcp_buffer)
{
  GstBuffer *buf;
  GstRTCPPacket packet;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  gsize offset;
  gsize maxsize;

  buf = gst_rtcp_buffer_new (1400);
  fail_unless (buf != NULL);
  fail_unless_equals_int (gst_buffer_get_sizes (buf, &offset, &maxsize), 0);
  fail_unless_equals_int (offset, 0);
  fail_unless_equals_int (maxsize, 1400);

  gst_rtcp_buffer_map (buf, GST_MAP_READWRITE, &rtcp);

  fail_unless (gst_rtcp_buffer_validate (buf) == FALSE);
  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet) == FALSE);
  fail_unless (gst_rtcp_buffer_get_packet_count (&rtcp) == 0);

  /* add an SR packet */
  fail_unless (gst_rtcp_buffer_add_packet (&rtcp, GST_RTCP_TYPE_SR,
          &packet) == TRUE);

  fail_unless (gst_rtcp_packet_get_padding (&packet) == 0);
  fail_unless (gst_rtcp_packet_get_count (&packet) == 0);
  fail_unless (gst_rtcp_packet_get_type (&packet) == GST_RTCP_TYPE_SR);
  fail_unless (gst_rtcp_packet_get_length (&packet) == 6);

  gst_rtcp_packet_sr_set_sender_info (&packet, 0x44556677,
      G_GUINT64_CONSTANT (1), 0x11111111, 101, 123456);
  {
    guint32 ssrc;
    guint64 ntptime;
    guint32 rtptime;
    guint32 packet_count;
    guint32 octet_count;

    gst_rtcp_packet_sr_get_sender_info (&packet, &ssrc, &ntptime, &rtptime,
        &packet_count, &octet_count);

    fail_unless (ssrc == 0x44556677);
    fail_unless (ntptime == G_GUINT64_CONSTANT (1));
    fail_unless (rtptime == 0x11111111);
    fail_unless (packet_count == 101);
    fail_unless (octet_count == 123456);
  }

  /* go to first packet, this should be the packet we just added */
  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet) == TRUE);

  fail_unless (gst_rtcp_packet_get_padding (&packet) == 0);
  fail_unless (gst_rtcp_packet_get_count (&packet) == 0);
  fail_unless (gst_rtcp_packet_get_type (&packet) == GST_RTCP_TYPE_SR);
  fail_unless (gst_rtcp_packet_get_length (&packet) == 6);

  fail_unless (gst_rtcp_packet_move_to_next (&packet) == FALSE);

  /* add some SDES */
  fail_unless (gst_rtcp_buffer_add_packet (&rtcp, GST_RTCP_TYPE_SDES,
          &packet) == TRUE);
  fail_unless (gst_rtcp_packet_sdes_add_item (&packet, 0xff658743) == TRUE);
  fail_unless (gst_rtcp_packet_sdes_add_entry (&packet, GST_RTCP_SDES_CNAME,
          sizeof ("test@foo.bar"), (guint8 *) "test@foo.bar") == TRUE);

  /* add some BYE */
  fail_unless (gst_rtcp_buffer_add_packet (&rtcp, GST_RTCP_TYPE_BYE,
          &packet) == TRUE);
  fail_unless (gst_rtcp_packet_bye_add_ssrc (&packet, 0x5613212f) == TRUE);
  fail_unless (gst_rtcp_packet_bye_add_ssrc (&packet, 0x00112233) == TRUE);
  fail_unless (gst_rtcp_packet_bye_get_ssrc_count (&packet) == 2);

  fail_unless (gst_rtcp_packet_get_padding (&packet) == 0);
  fail_unless (gst_rtcp_packet_get_count (&packet) == 2);
  fail_unless (gst_rtcp_packet_get_type (&packet) == GST_RTCP_TYPE_BYE);
  fail_unless (gst_rtcp_packet_get_length (&packet) == 2);

  /* move to SDES */
  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet) == TRUE);
  fail_unless (gst_rtcp_packet_move_to_next (&packet) == TRUE);

  fail_unless (gst_rtcp_packet_get_padding (&packet) == 0);
  fail_unless (gst_rtcp_packet_get_count (&packet) == 1);
  fail_unless (gst_rtcp_packet_get_type (&packet) == GST_RTCP_TYPE_SDES);
  fail_unless (gst_rtcp_packet_get_length (&packet) == 5);

  /* remove the SDES */
  fail_unless (gst_rtcp_packet_remove (&packet) == TRUE);

  /* we are now at the BYE packet */
  fail_unless (gst_rtcp_packet_get_padding (&packet) == 0);
  fail_unless (gst_rtcp_packet_get_count (&packet) == 2);
  fail_unless (gst_rtcp_packet_get_type (&packet) == GST_RTCP_TYPE_BYE);
  fail_unless (gst_rtcp_packet_get_length (&packet) == 2);

  /* close and validate */
  gst_rtcp_buffer_unmap (&rtcp);
  fail_unless (gst_rtcp_buffer_validate (buf) == TRUE);
  fail_unless (gst_rtcp_buffer_validate_reduced (buf) == TRUE);
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_rtcp_reduced_buffer)
{
  GstBuffer *buf;
  GstRTCPPacket packet;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  gsize offset;
  gsize maxsize;

  buf = gst_rtcp_buffer_new (1400);
  fail_unless (buf != NULL);
  fail_unless_equals_int (gst_buffer_get_sizes (buf, &offset, &maxsize), 0);
  fail_unless_equals_int (offset, 0);
  fail_unless_equals_int (maxsize, 1400);

  gst_rtcp_buffer_map (buf, GST_MAP_READWRITE, &rtcp);

  fail_unless (gst_rtcp_buffer_validate (buf) == FALSE);
  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet) == FALSE);
  fail_unless (gst_rtcp_buffer_get_packet_count (&rtcp) == 0);

  /* add an SR packet */
  fail_unless (gst_rtcp_buffer_add_packet (&rtcp, GST_RTCP_TYPE_PSFB,
          &packet) == TRUE);

  /* close and validate */
  gst_rtcp_buffer_unmap (&rtcp);
  fail_unless (gst_rtcp_buffer_validate (buf) == FALSE);
  fail_unless (gst_rtcp_buffer_validate_reduced (buf) == TRUE);
  gst_buffer_unref (buf);
}

GST_END_TEST;


GST_START_TEST (test_rtcp_validate_with_padding)
{
  /* Compound packet with padding in the last packet. Padding is included in
   * the length of the last packet. */
  guint8 rtcp_pkt[] = {
    0x80, 0xC9, 0x00, 0x07,     /* Type RR, length = 7 */
    0x97, 0x6d, 0x21, 0x6a,
    0x4d, 0x16, 0xaf, 0x14,
    0x10, 0x1f, 0xd9, 0x91,
    0x0f, 0xb7, 0x50, 0x88,
    0x3b, 0x79, 0x31, 0x50,
    0xbe, 0x19, 0x12, 0xa8,
    0xbb, 0xce, 0x9e, 0x3e,
    0xA0, 0xCA, 0x00, 0x0A,     /* P=1, Type SDES, length = 10 (includes padding) */
    0x97, 0x6d, 0x21, 0x6a,
    0x01, 0x0F, 0x00, 0x00,     /* Type 1 (CNAME), length 15 */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x09, 0x00,     /* Type 2 (NAME), length 9 */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,     /* Type 0 (no length, 2 unused bytes) */
    0x00, 0x00, 0x00, 0x04      /* RTCP padding */
  };

  fail_unless (gst_rtcp_buffer_validate_data (rtcp_pkt, sizeof (rtcp_pkt)));
}

GST_END_TEST;

GST_START_TEST (test_rtcp_validate_with_padding_wrong_padlength)
{
  /* Compound packet with padding in the last packet. Padding is included in
   * the length of the last packet. */
  guint8 rtcp_pkt[] = {
    0x80, 0xC9, 0x00, 0x07,     /* Type RR, length = 7 */
    0x97, 0x6d, 0x21, 0x6a,
    0x4d, 0x16, 0xaf, 0x14,
    0x10, 0x1f, 0xd9, 0x91,
    0x0f, 0xb7, 0x50, 0x88,
    0x3b, 0x79, 0x31, 0x50,
    0xbe, 0x19, 0x12, 0xa8,
    0xbb, 0xce, 0x9e, 0x3e,
    0xA0, 0xCA, 0x00, 0x0A,     /* P=1, Type SDES, length = 10 (includes padding) */
    0x97, 0x6d, 0x21, 0x6a,
    0x01, 0x0F, 0x00, 0x00,     /* Type 1 (CNAME), length 15 */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x09, 0x00,     /* Type 2 (NAME), length 9 */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,     /* Type 0 (no length, 2 unused bytes) */
    0x00, 0x00, 0x00, 0x03      /* RTCP padding (wrong length) */
  };

  fail_if (gst_rtcp_buffer_validate_data (rtcp_pkt, sizeof (rtcp_pkt)));
}

GST_END_TEST;

GST_START_TEST (test_rtcp_validate_with_padding_excluded_from_length)
{
  /* Compound packet with padding in the last packet. Padding is not included
   * in the length. */
  guint8 rtcp_pkt[] = {
    0x80, 0xC9, 0x00, 0x07,     /* Type RR, length = 7 */
    0x97, 0x6d, 0x21, 0x6a,
    0x4d, 0x16, 0xaf, 0x14,
    0x10, 0x1f, 0xd9, 0x91,
    0x0f, 0xb7, 0x50, 0x88,
    0x3b, 0x79, 0x31, 0x50,
    0xbe, 0x19, 0x12, 0xa8,
    0xbb, 0xce, 0x9e, 0x3e,
    0xA0, 0xCA, 0x00, 0x09,     /* P=1, Type SDES, length = 9 (excludes padding) */
    0x97, 0x6d, 0x21, 0x6a,
    0x01, 0x0F, 0x00, 0x00,     /* Type 1 (CNAME), length 15 */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x09, 0x00,     /* Type 2 (NAME), length 9 */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,     /* Type 0 (no length, 2 unused bytes) */
    0x00, 0x00, 0x00, 0x04      /* RTCP padding */
  };

  fail_if (gst_rtcp_buffer_validate_data (rtcp_pkt, sizeof (rtcp_pkt)));
}

GST_END_TEST;

GST_START_TEST (test_rtcp_validate_with_padding_set_in_first_packet)
{
  /* Compound packet with padding in the last packet but with the pad
     bit set on first packet */
  guint8 rtcp_pkt[] = {
    0xA0, 0xC9, 0x00, 0x07,     /* P=1, Type RR, length = 7 */
    0x97, 0x6d, 0x21, 0x6a,
    0x4d, 0x16, 0xaf, 0x14,
    0x10, 0x1f, 0xd9, 0x91,
    0x0f, 0xb7, 0x50, 0x88,
    0x3b, 0x79, 0x31, 0x50,
    0xbe, 0x19, 0x12, 0xa8,
    0xbb, 0xce, 0x9e, 0x3e,
    0x80, 0xCA, 0x00, 0x0a,     /* Type SDES, length = 10 (include padding) */
    0x97, 0x6d, 0x21, 0x6a,
    0x01, 0x0F, 0x00, 0x00,     /* Type 1 (CNAME), length 15 */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x09, 0x00,     /* Type 2 (NAME), length 9 */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,     /* Type 0 (no length, 2 unused bytes) */
    0x00, 0x00, 0x00, 0x04      /* RTCP padding */
  };

  fail_if (gst_rtcp_buffer_validate_data (rtcp_pkt, sizeof (rtcp_pkt)));
}

GST_END_TEST;

GST_START_TEST (test_rtcp_validate_reduced_without_padding)
{
  /* Reduced size packet without padding */
  guint8 rtcp_pkt[] = {
    0x80, 0xcd, 0x00, 0x07,     /* Type FB, length = 8 */
    0x97, 0x6d, 0x21, 0x6a,
    0x4d, 0x16, 0xaf, 0x14,
    0x10, 0x1f, 0xd9, 0x91,
    0x0f, 0xb7, 0x50, 0x88,
    0x3b, 0x79, 0x31, 0x50,
    0xbe, 0x19, 0x12, 0xa8,
    0xbb, 0xce, 0x9e, 0x3e,
  };

  fail_unless (gst_rtcp_buffer_validate_data_reduced (rtcp_pkt,
          sizeof (rtcp_pkt)));
}

GST_END_TEST;

GST_START_TEST (test_rtcp_validate_reduced_with_padding)
{
  GstRTCPPacket packet;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  GstBuffer *buffer = create_feedback_buffer (TRUE);

  gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp);
  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet));
  fail_unless (gst_rtcp_packet_get_padding (&packet));
  gst_rtcp_buffer_unmap (&rtcp);

  fail_unless (gst_rtcp_buffer_validate_reduced (buffer));
  fail_if (gst_rtcp_buffer_validate (buffer));

  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_rtcp_buffer_profile_specific_extension)
{
  GstBuffer *buf;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  GstRTCPPacket packet;
  const guint8 pse[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };
  const guint8 pse2[] = { 0x01, 0x23, 0x45, 0x67 };

  fail_unless ((buf = gst_rtcp_buffer_new (1400)) != NULL);
  gst_rtcp_buffer_map (buf, GST_MAP_READWRITE, &rtcp);

  fail_unless (gst_rtcp_buffer_validate (buf) == FALSE);
  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet) == FALSE);
  fail_unless (gst_rtcp_buffer_get_packet_count (&rtcp) == 0);

  /* add an SR packet with sender info */
  fail_unless (gst_rtcp_buffer_add_packet (&rtcp, GST_RTCP_TYPE_SR, &packet));
  gst_rtcp_packet_sr_set_sender_info (&packet, 0x44556677,
      G_GUINT64_CONSTANT (1), 0x11111111, 101, 123456);
  fail_unless_equals_int (0,
      gst_rtcp_packet_get_profile_specific_ext_length (&packet));
  fail_unless_equals_int (6, gst_rtcp_packet_get_length (&packet));

  /* add profile-specific extension */
  fail_unless (gst_rtcp_packet_add_profile_specific_ext (&packet,
          pse, sizeof (pse)));
  {
    guint8 *data = NULL;
    guint len = 0;

    fail_unless_equals_int (8, gst_rtcp_packet_get_length (&packet));
    fail_unless_equals_int (sizeof (pse) / 4,
        gst_rtcp_packet_get_profile_specific_ext_length (&packet));

    /* gst_rtcp_packet_get_profile_specific_ext */
    fail_unless (gst_rtcp_packet_get_profile_specific_ext (&packet, &data,
            &len));
    fail_unless_equals_int (sizeof (pse), len);
    fail_unless (data != NULL);
    fail_unless_equals_int (0, memcmp (pse, data, sizeof (pse)));

    /* gst_rtcp_packet_copy_profile_specific_ext */
    fail_unless (gst_rtcp_packet_copy_profile_specific_ext (&packet, &data,
            &len));
    fail_unless_equals_int (sizeof (pse), len);
    fail_unless (data != NULL);
    fail_unless_equals_int (0, memcmp (pse, data, sizeof (pse)));
    g_free (data);
  }

  /* append more profile-specific extension */
  fail_unless (gst_rtcp_packet_add_profile_specific_ext (&packet,
          pse2, sizeof (pse2)));
  {
    guint8 *data = NULL;
    guint len = 0;
    guint concat_len;
    guint8 *concat_pse;

    /* Expect the second extension to be appended to the first */
    concat_len = sizeof (pse) + sizeof (pse2);
    concat_pse = g_malloc (concat_len);
    memcpy (concat_pse, pse, sizeof (pse));
    memcpy (concat_pse + sizeof (pse), pse2, sizeof (pse2));

    fail_unless_equals_int (9, gst_rtcp_packet_get_length (&packet));
    fail_unless_equals_int (concat_len / 4,
        gst_rtcp_packet_get_profile_specific_ext_length (&packet));

    /* gst_rtcp_packet_get_profile_specific_ext */
    fail_unless (gst_rtcp_packet_get_profile_specific_ext (&packet, &data,
            &len));
    fail_unless_equals_int (concat_len, len);
    fail_unless (data != NULL);
    fail_unless_equals_int (0, memcmp (concat_pse, data, len));

    /* gst_rtcp_packet_copy_profile_specific_ext */
    fail_unless (gst_rtcp_packet_copy_profile_specific_ext (&packet, &data,
            &len));
    fail_unless_equals_int (concat_len, len);
    fail_unless (data != NULL);
    fail_unless_equals_int (0, memcmp (concat_pse, data, len));
    g_free (data);
    g_free (concat_pse);
  }

  /* close and validate */
  gst_rtcp_buffer_unmap (&rtcp);
  fail_unless (gst_rtcp_buffer_validate (buf) == TRUE);
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_rtcp_buffer_app)
{
  GstBuffer *buf;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  GstRTCPPacket packet;
  guint mtu = 1000;
  const guint8 data[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };
  guint max_data_length = (mtu - 12) / 4;
  guint8 *data_ptr;

  fail_unless ((buf = gst_rtcp_buffer_new (mtu)) != NULL);
  gst_rtcp_buffer_map (buf, GST_MAP_READWRITE, &rtcp);

  /* Not a valid packet yet */
  fail_if (gst_rtcp_buffer_validate (buf));
  fail_if (gst_rtcp_buffer_get_first_packet (&rtcp, &packet));
  fail_unless_equals_int (gst_rtcp_buffer_get_packet_count (&rtcp), 0);

  /* Add APP packet  */
  fail_unless (gst_rtcp_buffer_add_packet (&rtcp, GST_RTCP_TYPE_APP, &packet));
  gst_rtcp_packet_app_set_subtype (&packet, 0x15);
  gst_rtcp_packet_app_set_ssrc (&packet, 0x01234567);
  gst_rtcp_packet_app_set_name (&packet, "Test");

  /* Check maximum allowed data */
  fail_if (gst_rtcp_packet_app_set_data_length (&packet, max_data_length + 1));
  fail_unless (gst_rtcp_packet_app_set_data_length (&packet, max_data_length));

  /* Add data */
  fail_unless (gst_rtcp_packet_app_set_data_length (&packet,
          (sizeof (data) + 3) / 4));
  fail_unless_equals_int (gst_rtcp_packet_app_get_data_length (&packet), 2);
  fail_unless ((data_ptr = gst_rtcp_packet_app_get_data (&packet)));
  memcpy (data_ptr, data, sizeof (data));

  gst_rtcp_buffer_unmap (&rtcp);

  /* Map again with only the READ flag and check fields */
  gst_rtcp_buffer_map (buf, GST_MAP_READ, &rtcp);
  fail_unless_equals_int (gst_rtcp_packet_app_get_subtype (&packet), 0x15);
  fail_unless_equals_int (gst_rtcp_packet_app_get_ssrc (&packet), 0x01234567);
  fail_unless (memcmp (gst_rtcp_packet_app_get_name (&packet), "Test", 4) == 0);
  fail_unless_equals_int (gst_rtcp_packet_app_get_data_length (&packet), 2);
  fail_unless ((data_ptr = gst_rtcp_packet_app_get_data (&packet)));
  fail_unless (memcmp (data_ptr, data, sizeof (data)) == 0);
  gst_rtcp_buffer_unmap (&rtcp);

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_rtcp_buffer_xr)
{
  GstBuffer *buffer;
  GstRTCPPacket packet;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  guint8 rtcp_pkt[] = {
    0x80, 0xCF, 0x00, 0x0e,     /* Type XR, length = 14 */
    0x97, 0x6d, 0x21, 0x6a,
    0x01, 0x00, 0x00, 0x03,     /* Loss RLE, No thining, length = 3 */
    0x97, 0x6d, 0x21, 0x6a,     /* SSRC of source */
    0x00, 0x01, 0x00, 0x02,
    0xcf, 0xb7, 0x8f, 0xb7,
    0x02, 0x00, 0x00, 0x03,     /* Dup RLE, No thining, length = 3 */
    0x97, 0x6d, 0x21, 0x6a,     /* SSRC of source */
    0x00, 0x01, 0x00, 0x02,
    0xcf, 0xb7, 0x8f, 0xb7,
    0x03, 0x00, 0x00, 0x04,     /* Packet Receipt Times, No thining, length = 4 */
    0x97, 0x6d, 0x21, 0x6a,     /* SSRC of source */
    0x00, 0x01, 0x00, 0x02,
    0x59, 0xf9, 0xdd, 0x7e,
    0x59, 0xf9, 0xdd, 0x7e,
  };

  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
      rtcp_pkt, sizeof (rtcp_pkt), 0, sizeof (rtcp_pkt), NULL, NULL);

  fail_unless (gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp));

  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet));
  fail_unless (gst_rtcp_packet_get_type (&packet) == GST_RTCP_TYPE_XR);
  fail_unless (gst_rtcp_packet_xr_get_ssrc (&packet) ==
      GST_READ_UINT32_BE (rtcp_pkt + 12));
  fail_unless (gst_rtcp_packet_xr_first_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_LRLE);
  fail_unless (gst_rtcp_packet_xr_next_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_DRLE);
  fail_unless (gst_rtcp_packet_xr_next_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_PRT);

  fail_if (gst_rtcp_packet_xr_next_rb (&packet));

  gst_rtcp_buffer_unmap (&rtcp);
  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_rtcp_buffer_xr_rle)
{
  GstBuffer *buffer;
  GstRTCPPacket packet;
  guint32 ssrc, chunk_count;
  guint8 thining;
  guint16 begin_seq, end_seq, chunk;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  guint8 rtcp_pkt[] = {
    0x80, 0xCF, 0x00, 0x0a,     /* Type XR, length = 10 */
    0x97, 0x6d, 0x21, 0x6a,
    0x01, 0x00, 0x00, 0x03,     /* Loss RLE, No thining, length = 3 */
    0x97, 0x6d, 0x21, 0x6a,     /* SSRC of source */
    0x00, 0x01, 0x00, 0x02,
    0x80, 0x12, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x04,     /* Dup RLE, No thining, length = 4 */
    0x97, 0x6d, 0x21, 0x7b,     /* SSRC of source */
    0x00, 0x01, 0x00, 0x04,
    0x8f, 0x21, 0x8f, 0x22,
    0x8f, 0x23, 0x8f, 0x24
  };
  guint8 rtcp_pkt_invalid_pkt_length[] = {
    0x80, 0xCF, 0x00, 0x04,     /* Type XR, length = 4 */
    0x97, 0x6d, 0x21, 0x6a,
    0x01, 0x00, 0x00, 0x02,     /* Loss RLE, No thining, length = 1 (but really 3) */
    0x97, 0x6d, 0x21, 0x6a,     /* SSRC of source */
    0x00, 0x01, 0x00, 0x02,
  };

  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
      rtcp_pkt, sizeof (rtcp_pkt), 0, sizeof (rtcp_pkt), NULL, NULL);
  gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp);

  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet));

  /* check LRLE */
  fail_unless (gst_rtcp_packet_xr_first_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_LRLE);
  fail_unless (gst_rtcp_packet_xr_get_rle_info (&packet, &ssrc, &thining,
          &begin_seq, &end_seq, &chunk_count));
  fail_unless_equals_int (ssrc, GST_READ_UINT32_BE (rtcp_pkt + 12));
  fail_unless_equals_int (thining, 0);
  fail_unless_equals_int (begin_seq, 0x0001);
  fail_unless_equals_int (end_seq, 0x0002);
  fail_unless_equals_int (chunk_count, 2);

  gst_rtcp_packet_xr_get_rle_nth_chunk (&packet, 0, &chunk);
  fail_unless_equals_int (chunk, 0x8012);

  gst_rtcp_packet_xr_get_rle_nth_chunk (&packet, 1, &chunk);
  fail_unless_equals_int (chunk, 0x0);

  /* check DRLE */
  fail_unless (gst_rtcp_packet_xr_next_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_DRLE);
  fail_unless (gst_rtcp_packet_xr_get_rle_info (&packet, &ssrc, &thining,
          &begin_seq, &end_seq, &chunk_count));
  fail_unless_equals_int (ssrc, GST_READ_UINT32_BE (rtcp_pkt + 28));
  fail_unless_equals_int (thining, 0);
  fail_unless_equals_int (begin_seq, 0x0001);
  fail_unless_equals_int (end_seq, 0x0004);
  fail_unless_equals_int (chunk_count, 4);

  gst_rtcp_packet_xr_get_rle_nth_chunk (&packet, 1, &chunk);
  fail_unless_equals_int (chunk, 0x8f22);

  gst_rtcp_packet_xr_get_rle_nth_chunk (&packet, 2, &chunk);
  fail_unless_equals_int (chunk, 0x8f23);

  gst_rtcp_buffer_unmap (&rtcp);
  gst_buffer_unref (buffer);

  /* Test invalid length */
  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
      rtcp_pkt_invalid_pkt_length, sizeof (rtcp_pkt_invalid_pkt_length), 0,
      sizeof (rtcp_pkt_invalid_pkt_length), NULL, NULL);
  gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp);
  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet));

  /* check LRLE (should fail because length is too short) */
  fail_unless (gst_rtcp_packet_xr_first_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_LRLE);
  fail_if (gst_rtcp_packet_xr_get_rle_info (&packet, &ssrc, &thining,
          &begin_seq, &end_seq, &chunk_count));
  gst_rtcp_buffer_unmap (&rtcp);
  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_rtcp_buffer_xr_prt)
{
  GstBuffer *buffer;
  GstRTCPPacket packet;
  guint32 ssrc, receipt_time;
  guint8 thining;
  guint16 begin_seq, end_seq;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  guint8 rtcp_pkt[] = {
    0x80, 0xCF, 0x00, 0x06,     /* Type XR, length = 6 */
    0x97, 0x6d, 0x21, 0x6a,
    0x03, 0x00, 0x00, 0x04,     /* Packet Receipt Times, No thining, length = 4 */
    0x97, 0x6d, 0x21, 0x6a,     /* SSRC of source */
    0x00, 0x01, 0x00, 0x03,
    0x59, 0xf9, 0xdd, 0x7e,
    0x59, 0xf9, 0xde, 0x00,
  };
  guint8 rtcp_pkt_invalid_pkt_length[] = {
    0x80, 0xCF, 0x00, 0x04,     /* Type XR, length = 4 */
    0x97, 0x6d, 0x21, 0x6a,
    0x03, 0x00, 0x00, 0x02,     /* Packet Receipt Times, No thining, length = 2 (but should be 4) */
    0x97, 0x6d, 0x21, 0x6a,     /* SSRC of source */
    0x00, 0x01, 0x00, 0x03,
  };
  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
      rtcp_pkt, sizeof (rtcp_pkt), 0, sizeof (rtcp_pkt), NULL, NULL);
  gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp);

  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet));

  fail_unless (gst_rtcp_packet_xr_first_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_PRT);

  fail_unless (gst_rtcp_packet_xr_get_prt_info (&packet, &ssrc, &thining,
          &begin_seq, &end_seq));
  fail_unless (gst_rtcp_packet_xr_get_prt_by_seq (&packet, 2, &receipt_time));
  fail_unless_equals_int_hex (receipt_time, 0x59f9de00L);

  gst_rtcp_buffer_unmap (&rtcp);
  gst_buffer_unref (buffer);

  /* Test for invalid length */
  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
      rtcp_pkt_invalid_pkt_length, sizeof (rtcp_pkt_invalid_pkt_length), 0,
      sizeof (rtcp_pkt_invalid_pkt_length), NULL, NULL);
  gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp);

  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet));

  fail_unless (gst_rtcp_packet_xr_first_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_PRT);

  fail_if (gst_rtcp_packet_xr_get_prt_info (&packet, &ssrc, &thining,
          &begin_seq, &end_seq));
  gst_rtcp_buffer_unmap (&rtcp);
  gst_buffer_unref (buffer);

}

GST_END_TEST;

GST_START_TEST (test_rtcp_buffer_xr_rrt)
{
  GstBuffer *buffer;
  GstRTCPPacket packet;
  guint64 ntptime;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  guint8 rtcp_pkt[] = {
    0x80, 0xCF, 0x00, 0x04,     /* Type XR, length = 4 */
    0x97, 0x6d, 0x21, 0x6a,
    0x04, 0x00, 0x00, 0x02,     /* Receiver Reference Time, length = 2 */
    0x01, 0x23, 0x45, 0x67,
    0x89, 0x01, 0x23, 0x45
  };
  guint8 rtcp_pkt_invalid_pkt_length[] = {
    0x80, 0xCF, 0x00, 0x04,     /* Type XR, length = 4 */
    0x97, 0x6d, 0x21, 0x6a,
    0x04, 0x00, 0x00, 0x01,     /* Receiver Reference Time, length = 1 */
    0x01, 0x23, 0x45, 0x67,
    0x89, 0x01, 0x23, 0x45
  };

  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
      rtcp_pkt, sizeof (rtcp_pkt), 0, sizeof (rtcp_pkt), NULL, NULL);
  gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp);

  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet));

  fail_unless (gst_rtcp_packet_xr_first_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_RRT);

  fail_unless (gst_rtcp_packet_xr_get_rrt (&packet, &ntptime));
  fail_unless_equals_uint64_hex (ntptime, 0x0123456789012345LL);

  gst_rtcp_buffer_unmap (&rtcp);
  gst_buffer_unref (buffer);

  /* Test invalid length */
  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
      rtcp_pkt_invalid_pkt_length, sizeof (rtcp_pkt_invalid_pkt_length), 0,
      sizeof (rtcp_pkt_invalid_pkt_length), NULL, NULL);
  gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp);

  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet));

  fail_unless (gst_rtcp_packet_xr_first_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_RRT);

  fail_if (gst_rtcp_packet_xr_get_rrt (&packet, &ntptime));

  gst_rtcp_buffer_unmap (&rtcp);
  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_rtcp_buffer_xr_dlrr)
{
  GstBuffer *buffer;
  GstRTCPPacket packet;
  guint32 ssrc, last_rr, delay;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  guint8 rtcp_pkt[] = {
    0x80, 0xCF, 0x00, 0x08,     /* Type XR, length = 8 */
    0x97, 0x6d, 0x21, 0x6a,
    0x05, 0x00, 0x00, 0x06,     /* DLRR, length = 6 */
    0x97, 0x6d, 0x21, 0x6a,     /* SSRC of source */
    0x01, 0x23, 0x45, 0x67,
    0x89, 0x01, 0x23, 0x45,
    0x97, 0x6d, 0x21, 0x6b,     /* SSRC of source */
    0x01, 0x23, 0x45, 0x67,
    0x89, 0x01, 0x23, 0x45
  };

  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
      rtcp_pkt, sizeof (rtcp_pkt), 0, sizeof (rtcp_pkt), NULL, NULL);
  gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp);

  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet));

  fail_unless (gst_rtcp_packet_xr_first_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_DLRR);

  fail_unless (gst_rtcp_packet_xr_get_dlrr_block (&packet, 0, &ssrc, &last_rr,
          &delay));
  fail_unless_equals_int_hex (ssrc, GST_READ_UINT32_BE (rtcp_pkt + 12));
  fail_unless (gst_rtcp_packet_xr_get_dlrr_block (&packet, 1, &ssrc, &last_rr,
          &delay));
  fail_unless_equals_int_hex (ssrc, GST_READ_UINT32_BE (rtcp_pkt + 24));

  /* it has only two sub-blocks. */
  fail_if (gst_rtcp_packet_xr_get_dlrr_block (&packet, 2, &ssrc, &last_rr,
          &delay));

  gst_rtcp_buffer_unmap (&rtcp);
  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_rtcp_buffer_xr_ssumm)
{
  GstBuffer *buffer;
  GstRTCPPacket packet;
  guint32 ssrc, lost_packets, dup_packets;
  guint16 begin_seq, end_seq;
  guint32 min_jitter, max_jitter, mean_jitter, dev_jitter;
  guint8 min_ttl, max_ttl, mean_ttl, dev_ttl;
  gboolean ipv4;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  guint8 rtcp_pkt[] = {
    0x80, 0xCF, 0x00, 0x0b,     /* Type XR, length = 11 */
    0x97, 0x6d, 0x21, 0x6a,
    0x06, 0xe8, 0x00, 0x09,     /* Statistics summary, length = 9 */
    0x97, 0x6d, 0x21, 0x6a,     /* SSRC of source */
    0x00, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x05,
    0x00, 0x00, 0x00, 0x06,
    0x01, 0x80, 0x0f, 0x8f
  };

  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
      rtcp_pkt, sizeof (rtcp_pkt), 0, sizeof (rtcp_pkt), NULL, NULL);
  gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp);

  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet));

  fail_unless (gst_rtcp_packet_xr_first_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_SSUMM);

  fail_unless (gst_rtcp_packet_xr_get_summary_info (&packet, &ssrc, &begin_seq,
          &end_seq));
  fail_unless_equals_int_hex (ssrc, GST_READ_UINT32_BE (rtcp_pkt + 12));
  fail_unless_equals_int (begin_seq, GST_READ_UINT16_BE (rtcp_pkt + 16));
  fail_unless_equals_int (end_seq, GST_READ_UINT16_BE (rtcp_pkt + 18));

  fail_unless (gst_rtcp_packet_xr_get_summary_pkt (&packet, &lost_packets,
          &dup_packets));
  fail_unless_equals_int (lost_packets, GST_READ_UINT32_BE (rtcp_pkt + 20));
  fail_unless_equals_int (dup_packets, GST_READ_UINT32_BE (rtcp_pkt + 24));

  fail_unless (gst_rtcp_packet_xr_get_summary_jitter (&packet, &min_jitter,
          &max_jitter, &mean_jitter, &dev_jitter));
  fail_unless_equals_int (min_jitter, GST_READ_UINT32_BE (rtcp_pkt + 28));
  fail_unless_equals_int (max_jitter, GST_READ_UINT32_BE (rtcp_pkt + 32));
  fail_unless_equals_int (mean_jitter, GST_READ_UINT32_BE (rtcp_pkt + 36));
  fail_unless_equals_int (dev_jitter, GST_READ_UINT32_BE (rtcp_pkt + 40));

  fail_unless (gst_rtcp_packet_xr_get_summary_ttl (&packet, &ipv4, &min_ttl,
          &max_ttl, &mean_ttl, &dev_ttl));
  fail_unless (ipv4);
  fail_unless_equals_int (min_ttl, rtcp_pkt[44]);
  fail_unless_equals_int (max_ttl, rtcp_pkt[45]);
  fail_unless_equals_int (mean_ttl, rtcp_pkt[46]);
  fail_unless_equals_int (dev_ttl, rtcp_pkt[47]);

  gst_rtcp_buffer_unmap (&rtcp);
  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_rtcp_buffer_xr_voipmtrx)
{
  GstBuffer *buffer;
  GstRTCPPacket packet;
  guint32 ssrc;
  guint8 loss_rate, discard_rate, burst_density, gap_density;
  guint8 signal_level, noise_level, rerl, gmin;
  guint8 r_factor, ext_r_factor, mos_lq, mos_cq, rx_config;
  guint16 burst_duration, gap_duration;
  guint16 roundtrip_delay, end_system_delay;
  guint16 jb_nominal, jb_maximum, jb_abs_max;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  guint8 rtcp_pkt[] = {
    0x80, 0xCF, 0x00, 0x0a,     /* Type XR, length = 10 */
    0x97, 0x6d, 0x21, 0x6a,
    0x07, 0x00, 0x00, 0x08,     /* VoIP Metrics, length = 8 */
    0x97, 0x6d, 0x21, 0x6a,     /* SSRC of source */
    0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c,
    0x0d, 0x0e, 0x0f, 0x10,
    0x11, 0x12, 0x13, 0x14,
    0x15, 0x00, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b
  };

  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
      rtcp_pkt, sizeof (rtcp_pkt), 0, sizeof (rtcp_pkt), NULL, NULL);
  gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp);

  fail_unless (gst_rtcp_buffer_get_first_packet (&rtcp, &packet));

  fail_unless (gst_rtcp_packet_xr_first_rb (&packet));
  fail_unless (gst_rtcp_packet_xr_get_block_type (&packet) ==
      GST_RTCP_XR_TYPE_VOIP_METRICS);
  fail_unless (gst_rtcp_packet_xr_get_voip_metrics_ssrc (&packet, &ssrc));
  fail_unless_equals_int_hex (ssrc, GST_READ_UINT32_BE (rtcp_pkt + 12));

  fail_unless (gst_rtcp_packet_xr_get_voip_packet_metrics (&packet, &loss_rate,
          &discard_rate));
  fail_unless_equals_int (loss_rate, rtcp_pkt[16]);
  fail_unless_equals_int (discard_rate, rtcp_pkt[17]);

  fail_unless (gst_rtcp_packet_xr_get_voip_burst_metrics (&packet,
          &burst_density, &gap_density, &burst_duration, &gap_duration));
  fail_unless_equals_int (burst_density, rtcp_pkt[18]);
  fail_unless_equals_int (gap_density, rtcp_pkt[19]);
  fail_unless_equals_int (burst_duration, GST_READ_UINT16_BE (rtcp_pkt + 20));
  fail_unless_equals_int (gap_duration, GST_READ_UINT16_BE (rtcp_pkt + 22));

  fail_unless (gst_rtcp_packet_xr_get_voip_delay_metrics (&packet,
          &roundtrip_delay, &end_system_delay));
  fail_unless_equals_int (roundtrip_delay, GST_READ_UINT16_BE (rtcp_pkt + 24));
  fail_unless_equals_int (end_system_delay, GST_READ_UINT16_BE (rtcp_pkt + 26));

  fail_unless (gst_rtcp_packet_xr_get_voip_signal_metrics (&packet,
          &signal_level, &noise_level, &rerl, &gmin));
  fail_unless_equals_int (signal_level, rtcp_pkt[28]);
  fail_unless_equals_int (noise_level, rtcp_pkt[29]);
  fail_unless_equals_int (rerl, rtcp_pkt[30]);
  fail_unless_equals_int (gmin, rtcp_pkt[31]);

  fail_unless (gst_rtcp_packet_xr_get_voip_quality_metrics (&packet, &r_factor,
          &ext_r_factor, &mos_lq, &mos_cq));
  fail_unless_equals_int (r_factor, rtcp_pkt[32]);
  fail_unless_equals_int (ext_r_factor, rtcp_pkt[33]);
  fail_unless_equals_int (mos_lq, rtcp_pkt[34]);
  fail_unless_equals_int (mos_cq, rtcp_pkt[35]);

  fail_unless (gst_rtcp_packet_xr_get_voip_configuration_params (&packet, &gmin,
          &rx_config));
  fail_unless_equals_int (gmin, rtcp_pkt[31]);
  fail_unless_equals_int (rx_config, rtcp_pkt[36]);

  fail_unless (gst_rtcp_packet_xr_get_voip_jitter_buffer_params (&packet,
          &jb_nominal, &jb_maximum, &jb_abs_max));
  fail_unless_equals_int (jb_nominal, GST_READ_UINT16_BE (rtcp_pkt + 38));
  fail_unless_equals_int (jb_maximum, GST_READ_UINT16_BE (rtcp_pkt + 40));
  fail_unless_equals_int (jb_abs_max, GST_READ_UINT16_BE (rtcp_pkt + 42));

  gst_rtcp_buffer_unmap (&rtcp);
  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_rtp_ntp64_extension)
{
  GstBuffer *buf;
  gpointer data;
  guint size;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 bytes[] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45 };
  guint64 ntptime;
  guint8 hdrext_ntp64[GST_RTP_HDREXT_NTP_64_SIZE];

  buf = gst_rtp_buffer_new_allocate (0, 0, 0);

  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);

  /* format extension data */
  gst_rtp_hdrext_set_ntp_64 (hdrext_ntp64, GST_RTP_HDREXT_NTP_64_SIZE,
      0x0123456789012345LL);
  fail_unless (memcmp (bytes, hdrext_ntp64, sizeof (bytes)) == 0);

  /* add as 1byte header */
  gst_rtp_buffer_add_extension_onebyte_header (&rtp, 1, hdrext_ntp64,
      GST_RTP_HDREXT_NTP_64_SIZE);

  /* get extension again */
  gst_rtp_buffer_get_extension_onebyte_header (&rtp, 1, 0, &data, &size);

  /* and check */
  fail_unless (size == GST_RTP_HDREXT_NTP_64_SIZE);
  fail_unless (memcmp (data, hdrext_ntp64, size) == 0);

  gst_rtp_hdrext_get_ntp_64 (data, size, &ntptime);
  fail_unless (ntptime == 0x0123456789012345LL);

  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_rtp_ntp56_extension)
{
  GstBuffer *buf;
  gpointer data;
  guint size;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 bytes[] = { 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45 };
  guint64 ntptime;
  guint8 hdrext_ntp56[GST_RTP_HDREXT_NTP_56_SIZE];

  buf = gst_rtp_buffer_new_allocate (0, 0, 0);

  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);

  /* format extension data */
  gst_rtp_hdrext_set_ntp_56 (hdrext_ntp56, GST_RTP_HDREXT_NTP_56_SIZE,
      0x0123456789012345LL);
  /* truncates top bits */
  fail_unless (memcmp (bytes, hdrext_ntp56, sizeof (bytes)) == 0);

  /* add as 1byte header */
  gst_rtp_buffer_add_extension_onebyte_header (&rtp, 1, hdrext_ntp56,
      GST_RTP_HDREXT_NTP_56_SIZE);

  /* get extension again */
  gst_rtp_buffer_get_extension_onebyte_header (&rtp, 1, 0, &data, &size);

  /* and check */
  fail_unless (size == GST_RTP_HDREXT_NTP_56_SIZE);
  fail_unless (memcmp (data, hdrext_ntp56, size) == 0);

  gst_rtp_hdrext_get_ntp_56 (data, size, &ntptime);
  fail_unless (ntptime == 0x23456789012345LL);

  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_rtp_buffer_get_extension_bytes)
{
  GstBuffer *buf;
  guint16 bits;
  guint size;
  guint8 misc_data[4] = { 1, 2, 3, 4 };
  gpointer pointer;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GBytes *gb;
  gsize gb_size;

  /* create RTP buffer without extension header */
  buf = gst_rtp_buffer_new_allocate (4, 0, 0);
  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);
  fail_if (gst_rtp_buffer_get_extension (&rtp));

  /* verify that obtaining extension data returns NULL and bits are unchanged */
  bits = 0xabcd;
  gb = gst_rtp_buffer_get_extension_bytes (&rtp, &bits);
  fail_unless (gb == NULL);
  fail_unless (bits == 0xabcd);

  g_bytes_unref (gb);

  /* add extension header without data and verify that
   * an empty GBytes is returned */
  fail_unless (gst_rtp_buffer_set_extension_data (&rtp, 270, 0));
  fail_unless (gst_rtp_buffer_get_extension (&rtp));
  gb = gst_rtp_buffer_get_extension_bytes (&rtp, &bits);
  fail_unless (gb != NULL);
  fail_unless_equals_int (g_bytes_get_size (gb), 0);

  g_bytes_unref (gb);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (buf);

  /* create RTP buffer with extension header and extension data */
  buf = gst_rtp_buffer_new_allocate (4, 0, 0);
  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);
  fail_unless (gst_rtp_buffer_add_extension_onebyte_header (&rtp, 5,
          misc_data, 2));
  fail_unless (gst_rtp_buffer_get_extension (&rtp));

  /* verify that gst_rtp_buffer_get_extension_bytes returns the same
   * header bits and data as does gst_rtp_buffer_get_extension_data */
  fail_unless (gst_rtp_buffer_get_extension_data (&rtp, &bits, &pointer,
          &size));
  fail_unless (bits == 0xBEDE);
  fail_unless (size == 1);
  gb = gst_rtp_buffer_get_extension_bytes (&rtp, &bits);
  fail_unless (gb != NULL);
  fail_unless (bits == 0xBEDE);
  fail_unless_equals_int (g_bytes_get_size (gb), size * 4);
  fail_unless (memcmp (pointer, g_bytes_get_data (gb, &gb_size),
          size * 4) == 0);
  fail_unless_equals_int (gb_size, size * 4);

  g_bytes_unref (gb);
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_rtp_buffer_get_payload_bytes)
{
  guint8 rtppacket[] = {
    0x80, 0xe0, 0xdf, 0xd7, 0xef, 0x84, 0xbe, 0xed, 0x9b, 0xc5, 0x29, 0x14,
    'H', 'e', 'l', 'l', 'o', '\0'
  };

  GstBuffer *buf;
  GstMapInfo map;
  gconstpointer data;
  gsize size;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GBytes *gb;

  /* create empty RTP buffer, i.e. no payload */
  buf = gst_rtp_buffer_new_allocate (0, 4, 0);
  fail_unless (buf != NULL);
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  fail_unless_equals_int (map.size, RTP_HEADER_LEN + 4);
  fail_unless (gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp));

  /* verify that requesting payload data returns an empty GBytes */
  gb = gst_rtp_buffer_get_payload_bytes (&rtp);
  fail_unless (gb != NULL);
  fail_unless_equals_int (g_bytes_get_size (gb), 0);

  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unmap (buf, &map);
  gst_buffer_unref (buf);
  g_bytes_unref (gb);

  /* create RTP buffer containing RTP packet */
  buf = gst_buffer_new_and_alloc (sizeof (rtppacket));
  fail_unless (buf != NULL);
  gst_buffer_fill (buf, 0, rtppacket, sizeof (rtppacket));
  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  fail_unless_equals_int (map.size, sizeof (rtppacket));
  fail_unless (gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp));

  /* verify that the returned GBytes contains the correct payload data */
  gb = gst_rtp_buffer_get_payload_bytes (&rtp);
  fail_unless (gb != NULL);
  data = g_bytes_get_data (gb, &size);
  fail_unless (data != NULL);
  fail_unless (size == (sizeof (rtppacket) - RTP_HEADER_LEN));
  fail_unless_equals_string ("Hello", data);
  g_bytes_unref (gb);

  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unmap (buf, &map);
  gst_buffer_unref (buf);
}

GST_END_TEST;


GST_START_TEST (test_rtp_buffer_empty_payload)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *paybuf, *outbuf;

  paybuf = gst_rtp_buffer_new_allocate (0, 0, 0);

  gst_rtp_buffer_map (paybuf, GST_MAP_READ, &rtp);
  outbuf = gst_rtp_buffer_get_payload_buffer (&rtp);
  gst_rtp_buffer_unmap (&rtp);

  gst_buffer_unref (paybuf);
  gst_buffer_unref (outbuf);
}

GST_END_TEST;

GST_START_TEST (test_rtp_buffer_extension_onebyte_header_full_padding)
{
  GstBuffer *buffer;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 *mem;
  guint size;
  guint8 *data_out;
  guint16 bits;
  guint8 *pdata;
  guint wordlen = 0;
  guint8 hdr_buffer_1[2] = { 0x1, 0x1 };

  guint8 rtp_test_buffer[] = {
    0x90, 0x7c, 0x18, 0xa6,     /* |V=2|P|X|CC|M|PT|sequence number| */
    0x7a, 0x62, 0x17, 0x0f,     /* |timestamp| */
    0x70, 0x23, 0x91, 0x38,     /* |synchronization source (SSRC) identifier| */
    0xbe, 0xde, 0x00, 0x02,     /* |0xBE|0xDE|length=2| */
    0x00, 0x00, 0x00, 0x00,     /* |0 (pad)|0 (pad)|0 (pad)|0 (pad)| */
    0x00, 0x00, 0x00, 0x00,     /* |0 (pad)|0 (pad)|0 (pad)|0 (pad)| */
    0xff, 0xff, 0xff, 0xff      /* |dummy payload| */
  };

  mem = malloc (sizeof (rtp_test_buffer));
  fail_unless (mem != NULL);
  memcpy (mem, rtp_test_buffer, sizeof (rtp_test_buffer));
  fail_unless_equals_int (memcmp (mem, rtp_test_buffer,
          sizeof (rtp_test_buffer)), 0);

  buffer = gst_buffer_new_wrapped (mem, sizeof (rtp_test_buffer));

  fail_unless (gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp));

  fail_unless (gst_rtp_buffer_get_extension_data (&rtp, &bits,
          (gpointer) & pdata, &wordlen));
  fail_unless_equals_int (bits, 0xBEDE);
  fail_unless_equals_int (wordlen, 2);
  fail_unless_equals_int (pdata[0], 0x0);
  fail_unless_equals_int (pdata[1], 0x0);
  fail_unless_equals_int (pdata[2], 0x0);
  fail_unless_equals_int (pdata[3], 0x0);
  fail_unless_equals_int (pdata[4], 0x0);
  fail_unless_equals_int (pdata[5], 0x0);
  fail_unless_equals_int (pdata[6], 0x0);
  fail_unless_equals_int (pdata[7], 0x0);

  fail_unless (gst_rtp_buffer_add_extension_onebyte_header (&rtp, 1,
          hdr_buffer_1, 2));
  fail_unless (gst_rtp_buffer_get_extension_onebyte_header (&rtp, 1, 0,
          (gpointer *) & data_out, &size));
  fail_unless_equals_int (size, 2);
  fail_unless_equals_int (data_out[0], 0x1);
  fail_unless_equals_int (data_out[1], 0x1);
  fail_unless (gst_rtp_buffer_get_extension_data (&rtp, &bits,
          (gpointer) & pdata, &wordlen));
  fail_unless_equals_int (bits, 0xBEDE);
  fail_unless_equals_int (wordlen, 2);
  fail_unless_equals_int (pdata[0], 0x11);
  fail_unless_equals_int (pdata[1], 0x1);
  fail_unless_equals_int (pdata[2], 0x1);
  fail_unless_equals_int (pdata[3], 0x0);
  fail_unless_equals_int (pdata[4], 0x0);
  fail_unless_equals_int (pdata[5], 0x0);
  fail_unless_equals_int (pdata[6], 0x0);
  fail_unless_equals_int (pdata[7], 0x0);

  gst_rtp_buffer_unmap (&rtp);

  gst_buffer_unref (buffer);
}

GST_END_TEST;


GST_START_TEST (test_ext_timestamp_basic)
{
  guint64 exttimestamp = -1;
  guint64 result = 0;

  /* no wraparound when timestamps are increasing */
  result = gst_rtp_buffer_ext_timestamp (&exttimestamp, 0);
  fail_unless_equals_uint64 (result, (G_GUINT64_CONSTANT (1) << 32) + 0);
  result = gst_rtp_buffer_ext_timestamp (&exttimestamp, 10);
  fail_unless_equals_uint64 (result, (G_GUINT64_CONSTANT (1) << 32) + 10);
  result = gst_rtp_buffer_ext_timestamp (&exttimestamp, 10);
  fail_unless_equals_uint64 (result, (G_GUINT64_CONSTANT (1) << 32) + 10);
  result = gst_rtp_buffer_ext_timestamp (&exttimestamp,
      G_GUINT64_CONSTANT (1) + G_MAXINT32);
  fail_unless_equals_uint64 (result,
      (G_GUINT64_CONSTANT (1) << 32) + G_GUINT64_CONSTANT (1) + G_MAXINT32);

  /* Even big bumps under G_MAXINT32 don't result in wrap-around */
  exttimestamp = -1;
  result = gst_rtp_buffer_ext_timestamp (&exttimestamp, 1087500);
  fail_unless_equals_uint64 (result, (G_GUINT64_CONSTANT (1) << 32) + 1087500);
  result = gst_rtp_buffer_ext_timestamp (&exttimestamp, 24);
  fail_unless_equals_uint64 (result, (G_GUINT64_CONSTANT (1) << 32) + 24);
}

GST_END_TEST;

GST_START_TEST (test_ext_timestamp_wraparound)
{
  guint64 ext_ts = -1;

  fail_unless_equals_uint64 (gst_rtp_buffer_ext_timestamp (&ext_ts,
          G_MAXUINT32 - 90000 + G_GUINT64_CONSTANT (1)),
      ((G_GUINT64_CONSTANT (1) << 32) + G_MAXUINT32 - 90000 +
          G_GUINT64_CONSTANT (1)));

  fail_unless_equals_uint64 (gst_rtp_buffer_ext_timestamp (&ext_ts, 0),
      (G_GUINT64_CONSTANT (1) << 32) + G_MAXUINT32 + G_GUINT64_CONSTANT (1));

  fail_unless_equals_uint64 (gst_rtp_buffer_ext_timestamp (&ext_ts, 90000),
      ((G_GUINT64_CONSTANT (1) << 32) + G_MAXUINT32 + G_GUINT64_CONSTANT (1) +
          90000));
}

GST_END_TEST;


GST_START_TEST (test_ext_timestamp_wraparound_disordered)
{
  guint64 ext_ts = -1;

  fail_unless_equals_uint64 (gst_rtp_buffer_ext_timestamp (&ext_ts,
          G_MAXUINT32 - 90000 + G_GUINT64_CONSTANT (1)),
      (G_GUINT64_CONSTANT (1) << 32) + G_MAXUINT32 - 90000 +
      G_GUINT64_CONSTANT (1));

  fail_unless_equals_uint64 (gst_rtp_buffer_ext_timestamp (&ext_ts, 0),
      (G_GUINT64_CONSTANT (1) << 32) + G_MAXUINT32 + G_GUINT64_CONSTANT (1));

  /* Unwrapping around */
  fail_unless_equals_uint64 (gst_rtp_buffer_ext_timestamp (&ext_ts,
          G_MAXUINT32 - 90000 + G_GUINT64_CONSTANT (1)),
      (G_GUINT64_CONSTANT (1) << 32) + G_MAXUINT32 - 90000 +
      G_GUINT64_CONSTANT (1));

  fail_unless_equals_uint64 (gst_rtp_buffer_ext_timestamp (&ext_ts, 90000),
      (G_GUINT64_CONSTANT (1) << 32) + G_MAXUINT32 + G_GUINT64_CONSTANT (1) +
      90000);
}

GST_END_TEST;

GST_START_TEST (test_ext_timestamp_wraparound_disordered_backwards)
{
  guint64 ext_ts = -1;

  fail_unless_equals_uint64 (gst_rtp_buffer_ext_timestamp (&ext_ts, 90000),
      (G_GUINT64_CONSTANT (1) << 32) + 90000);

  /* Wraps backwards */
  fail_unless_equals_uint64 (gst_rtp_buffer_ext_timestamp (&ext_ts,
          G_MAXUINT32 - 90000 + G_GUINT64_CONSTANT (1)),
      G_MAXUINT32 - 90000 + G_GUINT64_CONSTANT (1));

  fail_unless_equals_uint64 (gst_rtp_buffer_ext_timestamp (&ext_ts, 90000),
      (G_GUINT64_CONSTANT (1) << 32) + 90000);
}

GST_END_TEST;

static gboolean
set_rtcp_packet (GstBuffer * buffer, GstRTCPPacket * packet)
{
  GstMapInfo map = GST_MAP_INFO_INIT;
  gboolean ret = FALSE;
  gssize fci_length;

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GST_WARNING_OBJECT (buffer, "Cannot map feedback buffer");
    return FALSE;
  }

  fci_length = (map.size / 4) /* words of 4 bytes */ -3 /* skip RCTP header */ ;
  if (fci_length <= 0) {
    GST_WARNING ("Unexpected FCI length");
    goto end;
  }

  if (!gst_rtcp_packet_fb_set_fci_length (packet, fci_length)) {
    /* No enough space in rtcp packet to add this report */
    GST_WARNING ("Could not set transport feedback FCI length");
    goto end;
  }
  // Copy the rtcp feedback message here
  memcpy (packet->rtcp->map.data + packet->offset, map.data, map.size);

  ret = TRUE;

end:
  gst_buffer_unmap (buffer, &map);

  return ret;
}

static gboolean
add_rtcp_packet (GstBuffer * rtcp_buffer, GstBuffer * buffer, GstRTCPType type)
{
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  gboolean rtcp_mapped = FALSE;
  GstRTCPPacket packet;
  gboolean ret = FALSE;

  rtcp_mapped = gst_rtcp_buffer_map (rtcp_buffer, GST_MAP_READWRITE, &rtcp);
  if (!rtcp_mapped) {
    GST_WARNING_OBJECT (rtcp_buffer, "Cannot map buffer to RTCP");
    return FALSE;
  }

  if (!gst_rtcp_buffer_add_packet (&rtcp, type, &packet)) {
    GST_DEBUG ("Cannot add RTCP packet");
    goto end;
  }

  ret = set_rtcp_packet (buffer, &packet);

end:
  if (rtcp_mapped) {
    gst_rtcp_buffer_unmap (&rtcp);
  }

  return ret;
}

static GstBuffer *
create_feedback_buffer (gboolean with_padding)
{
  if (with_padding) {
    guint8 transport_wide_cc_padding_buffer[72] = {
      0xaf, 0xcd, 0x00, 0x11,
      0x7c, 0xbf, 0x7b, 0x00,
      0x4c, 0xc1, 0xe4, 0x69,
      0x00, 0x24, 0x00, 0x30,
      0x00, 0x00, 0x2c, 0x01,
      0x20, 0x30, 0x65, 0x0c,
      0x09, 0x0c, 0x0d, 0x08,
      0x2a, 0x16, 0x14, 0x14,
      0x16, 0x14, 0xcc, 0x00,
      0x14, 0x14, 0xcc, 0x8e,
      0x01, 0xa3, 0x02, 0x14,
      0x16, 0x50, 0x00, 0x16,
      0x7b, 0x01, 0x17, 0x14,
      0x94, 0x01, 0x15, 0x11,
      0x18, 0x16, 0x15, 0x90,
      0x01, 0x13, 0x15, 0x2a,
      0x00, 0x17, 0x17, 0x4f,
      0x00, 0x14, 0x00, 0x02,
    };

    return gst_buffer_new_memdup (transport_wide_cc_padding_buffer,
        sizeof (transport_wide_cc_padding_buffer));
  } else {
    guint8 transport_wide_cc_buffer[36] = {
      0x8f, 0xcd, 0x00, 0x08,
      0x7c, 0xbf, 0x7b, 0x00,
      0x4c, 0xc1, 0xe4, 0x69,
      0x19, 0xbc, 0x00, 0x0e,
      0x00, 0x02, 0x3c, 0x33,
      0x20, 0x0e, 0x02, 0x28,
      0x15, 0x15, 0x14, 0x17,
      0x14, 0x14, 0x15, 0x29,
      0x18, 0x12, 0x15, 0x16,
    };
    return gst_buffer_new_memdup (transport_wide_cc_buffer,
        sizeof (transport_wide_cc_buffer));
  }
}

static GstBuffer *
create_remb_buffer ()
{
  guint8 remb_buffer[20] = {
    0x8f, 0xce, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x52, 0x45, 0x4d, 0x42,
    0x00, 0x0b, 0xd0, 0x90,
  };

  return gst_buffer_new_memdup (remb_buffer, sizeof (remb_buffer));
}

static gboolean
add_transport_wide_cc (GstBuffer * buffer, gboolean with_padding)
{
  GstBuffer *feedback;
  gboolean ret;

  feedback = create_feedback_buffer (with_padding);
  ret = add_rtcp_packet (buffer, feedback, GST_RTCP_TYPE_RTPFB);
  gst_buffer_unref (feedback);

  return ret;
}

static gboolean
add_remb (GstBuffer * buffer)
{
  GstBuffer *remb;
  gboolean ret;

  remb = create_remb_buffer ();
  ret = add_rtcp_packet (buffer, remb, GST_RTCP_TYPE_PSFB);
  gst_buffer_unref (remb);

  return ret;
}

GST_START_TEST (test_rtcp_compound_padding)
{
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  GstRTCPPacket *rtcp_packet = NULL;
  GstBuffer *rtcp_buffer;

  rtcp_buffer = gst_rtcp_buffer_new (1400);

  fail_unless (gst_rtcp_buffer_map (rtcp_buffer, GST_MAP_READWRITE, &rtcp));
  rtcp_packet = g_new0 (GstRTCPPacket, 1);
  fail_unless (gst_rtcp_buffer_add_packet (&rtcp, GST_RTCP_TYPE_RR,
          rtcp_packet));
  gst_rtcp_packet_rr_set_ssrc (rtcp_packet, 1);
  g_free (rtcp_packet);
  gst_rtcp_buffer_unmap (&rtcp);

  fail_unless (gst_rtcp_buffer_validate (rtcp_buffer));

  fail_unless (add_remb (rtcp_buffer));
  fail_unless (add_transport_wide_cc (rtcp_buffer, FALSE));
  /* Last packet did not have padding so we can add more packets */
  fail_unless (add_remb (rtcp_buffer));

  fail_unless (add_transport_wide_cc (rtcp_buffer, TRUE));
  /* Last packet has padding so we are not allow to add more */
  fail_if (add_remb (rtcp_buffer));

  gst_buffer_unref (rtcp_buffer);
}

GST_END_TEST;

GST_START_TEST (test_rtp_buffer_extlen_wraparound)
{
  GstBuffer *buf;
  guint8 rtp_test_buffer[] = {
    0x90, 0x7c, 0x18, 0xa6,     /* |V=2|P|X|CC|M|PT|sequence number| */
    0x7a, 0x62, 0x17, 0x0f,     /* |timestamp| */
    0x70, 0x23, 0x91, 0x38,     /* |synchronization source (SSRC) identifier| */
    0xbe, 0xde, 0x40, 0x01,     /* |0xBE|0xDE|length=16385| */
    0x00, 0x00, 0x00, 0x00,     /* |0 (pad)|0 (pad)|0 (pad)|0 (pad)| */
    0x00, 0x00, 0x00, 0x00,     /* |0 (pad)|0 (pad)|0 (pad)|0 (pad)| */
    0xff, 0xff, 0xff, 0xff      /* |dummy payload| */
  };

  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  buf = gst_buffer_new_and_alloc (sizeof (rtp_test_buffer));
  gst_buffer_fill (buf, 0, rtp_test_buffer, sizeof (rtp_test_buffer));
  fail_if (gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp));
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_rtp_buffer_remove_extension_data)
{
  GstBuffer *buf;
  GstMapInfo info;
  guint8 rtp_test_buffer[] = {
    0x90, 0x7c, 0x18, 0xa6,     /* |V=2|P|X|CC|M|PT|sequence number| */
    0x7a, 0x62, 0x17, 0x0f,     /* |timestamp| */
    0x70, 0x23, 0x91, 0x38,     /* |synchronization source (SSRC) identifier| */
    0xbe, 0xde, 0x00, 0x02,     /* |0xBE|0xDE|length=2| */
    0x00, 0x00, 0x00, 0x00,     /* |0 (pad)|0 (pad)|0 (pad)|0 (pad)| */
    0x00, 0x00, 0x00, 0x00,     /* |0 (pad)|0 (pad)|0 (pad)|0 (pad)| */
    0xff, 0xff, 0xff, 0xff      /* |dummy payload| */
  };

  guint8 expected_result[] = {
    0x80, 0x7c, 0x18, 0xa6,     /* |V=2|P|X|CC|M|PT|sequence number| */
    0x7a, 0x62, 0x17, 0x0f,     /* |timestamp| */
    0x70, 0x23, 0x91, 0x38,     /* |synchronization source (SSRC) identifier| */
    0xff, 0xff, 0xff, 0xff      /* |dummy payload| */
  };

  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  buf = gst_buffer_new_and_alloc (sizeof (rtp_test_buffer));
  gst_buffer_fill (buf, 0, rtp_test_buffer, sizeof (rtp_test_buffer));

  fail_unless (gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp));

  gst_rtp_buffer_remove_extension_data (&rtp);
  gst_rtp_buffer_unmap (&rtp);

  gst_buffer_map (buf, &info, GST_MAP_READ);

  fail_unless_equals_int (info.size, sizeof (expected_result));
  fail_unless_equals_int
      (memcmp (info.data, expected_result, sizeof (expected_result)), 0);

  gst_buffer_unmap (buf, &info);
  gst_buffer_unref (buf);
}

GST_END_TEST;

static Suite *
rtp_suite (void)
{
  Suite *s = suite_create ("rtp support library");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_rtp_buffer);
  tcase_add_test (tc_chain, test_rtp_buffer_validate_corrupt);
  tcase_add_test (tc_chain, test_rtp_buffer_validate_padding);
  tcase_add_test (tc_chain, test_rtp_buffer_set_extension_data);
  //tcase_add_test (tc_chain, test_rtp_buffer_list_set_extension);
  tcase_add_test (tc_chain, test_rtp_seqnum_compare);

  tcase_add_test (tc_chain, test_rtcp_sdes_type);
  tcase_add_test (tc_chain, test_rtcp_buffer);
  tcase_add_test (tc_chain, test_rtcp_reduced_buffer);
  tcase_add_test (tc_chain, test_rtcp_validate_with_padding);
  tcase_add_test (tc_chain, test_rtcp_validate_with_padding_wrong_padlength);
  tcase_add_test (tc_chain,
      test_rtcp_validate_with_padding_excluded_from_length);
  tcase_add_test (tc_chain,
      test_rtcp_validate_with_padding_set_in_first_packet);
  tcase_add_test (tc_chain, test_rtcp_validate_reduced_without_padding);
  tcase_add_test (tc_chain, test_rtcp_validate_reduced_with_padding);
  tcase_add_test (tc_chain, test_rtcp_buffer_profile_specific_extension);
  tcase_add_test (tc_chain, test_rtcp_buffer_app);
  tcase_add_test (tc_chain, test_rtcp_buffer_xr);
  tcase_add_test (tc_chain, test_rtcp_buffer_xr_rle);
  tcase_add_test (tc_chain, test_rtcp_buffer_xr_prt);
  tcase_add_test (tc_chain, test_rtcp_buffer_xr_rrt);
  tcase_add_test (tc_chain, test_rtcp_buffer_xr_dlrr);
  tcase_add_test (tc_chain, test_rtcp_buffer_xr_ssumm);
  tcase_add_test (tc_chain, test_rtcp_buffer_xr_voipmtrx);

  tcase_add_test (tc_chain, test_rtp_ntp64_extension);
  tcase_add_test (tc_chain, test_rtp_ntp56_extension);

  tcase_add_test (tc_chain, test_rtp_buffer_get_payload_bytes);
  tcase_add_test (tc_chain, test_rtp_buffer_get_extension_bytes);
  tcase_add_test (tc_chain, test_rtp_buffer_empty_payload);

  tcase_add_test (tc_chain,
      test_rtp_buffer_extension_onebyte_header_full_padding);

  //tcase_add_test (tc_chain, test_rtp_buffer_list);

  tcase_add_test (tc_chain, test_ext_timestamp_basic);
  tcase_add_test (tc_chain, test_ext_timestamp_wraparound);
  tcase_add_test (tc_chain, test_ext_timestamp_wraparound_disordered);
  tcase_add_test (tc_chain, test_ext_timestamp_wraparound_disordered_backwards);

  tcase_add_test (tc_chain, test_rtcp_compound_padding);
  tcase_add_test (tc_chain, test_rtp_buffer_extlen_wraparound);
  tcase_add_test (tc_chain, test_rtp_buffer_remove_extension_data);
  tcase_add_test (tc_chain, test_rtp_buffer_set_extension_data_shrink_data);

  return s;
}

GST_CHECK_MAIN (rtp);
