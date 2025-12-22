/* Gstreamer
 * Copyright (C) <2011> Intel Corporation
 * Copyright (C) <2011> Collabora Ltd.
 * Copyright (C) <2011> Thibault Saunier <thibault.saunier@collabora.com>
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

#include <gst/check/gstcheck.h>
#include <gst/codecparsers/gstmpegvideoparser.h>

/* actually seq + gop */
static const guint8 mpeg2_seq[] = {
  0x00, 0x00, 0x01, 0xb3, 0x02, 0x00, 0x18, 0x15, 0xff, 0xff, 0xe0, 0x28,
  0x00, 0x00, 0x01, 0xb3, 0x78, 0x04, 0x38, 0x37, 0xff, 0xff, 0xf0, 0x00,
  0x00, 0x00, 0x01, 0xb5, 0x14, 0x8a, 0x00, 0x11, 0x03, 0x71,
  0x00, 0x00, 0x01, 0xb8, 0x00, 0x08, 0x00, 0x00,
  0x00, 0x00, 0x01, 0x03, 0x00, 0x08, 0x00, 0x00
};

static const guint8 mis_identified_datas[] = {
  0x00, 0x00, 0x01, 0x1f, 0x4a, 0xf4, 0xd4, 0xd8, 0x08, 0x23, 0xdd,
  0x7c, 0xd3, 0x75, 0x21, 0x43, 0x85, 0x31, 0x43, 0x04, 0x24, 0x30,
  0x18, 0x43, 0xba, 0x1a, 0x50, 0x60, 0xbb, 0x53, 0x56, 0x80, 0x41,
  0xb9, 0xd4, 0x25, 0x42, 0xea, 0x71, 0xb7, 0x49, 0x84, 0x0b, 0x14,
  0x24, 0xc2, 0xaa, 0xba, 0xf9, 0xf7, 0x5b, 0x78, 0xa2, 0xba, 0xd3,
  0xc7, 0x12, 0xee, 0xbe, 0xba, 0xfa, 0xeb, 0xeb, 0xaf, 0xbe, 0x6f,
  0xce, 0x92, 0x05, 0x15, 0x22, 0x44, 0xf4, 0xc9, 0x1b, 0xcd, 0x84,
  0x80, 0x87, 0x35, 0x6c, 0x07, 0x82, 0xaf, 0x3c, 0x3a, 0x89, 0x48,
  0x3a, 0x26, 0x00, 0x64, 0x03, 0x12, 0x60, 0x03, 0xf4, 0x8c, 0x21,
  0x16, 0xbe, 0x3c, 0x7c, 0x18, 0x03, 0x10, 0x0c, 0x80, 0xa0, 0x05,
  0xe1, 0x85, 0x94, 0x90, 0xc4, 0x74, 0x05, 0x72, 0x80, 0x7a, 0x8e,
  0x3e, 0x00, 0x30,
  /* The accelerated version of scan_for_start_codes()
   * mis-identifies the following as a start code */
  0x01, 0x00, 0x01, 0x80, 0x68, 0x14,
  0x26, 0xe4, 0x80, 0x98, 0x0a, 0xba, 0x77, 0x01, 0xc2, 0x42, 0x12,
  0xc4, 0x59, 0x2a, 0xbb, 0x49, 0xf2, 0xc5, 0xa8, 0xd9, 0x30, 0x33,
  0x16, 0x50, 0x60, 0x61, 0x41, 0xaa, 0x0d, 0x41, 0x5b, 0x17, 0x77,
  0x76, 0x1a, 0x14, 0x3a, 0x08, 0x19, 0x3d, 0x6c, 0x94, 0x55, 0xd0,
  0x94, 0x5a, 0xeb, 0x61, 0x22, 0xa7, 0xa6, 0x83, 0x47, 0x6d, 0x4d,
  0x84, 0xc4, 0x6f, 0x78, 0xd8, 0x3a, 0xb4, 0x02, 0x0c, 0x36, 0xa6,
  0x0b, 0x18, 0x49, 0xf7, 0xad, 0x00, 0x82, 0x09, 0xba, 0x12, 0xba,
  0x1d, 0x44, 0x94, 0x0a, 0x1b, 0x03, 0xbb, 0xa2, 0x53, 0x02, 0xc0,
  0x41, 0xac, 0x22,
  /* the real start code is here */
  0x00, 0x00, 0x01, 0x20, 0x4a, 0xfd, 0xf5, 0x50
};

static GstMpegVideoPacketTypeCode ordercode[] = {
  GST_MPEG_VIDEO_PACKET_SEQUENCE,
  GST_MPEG_VIDEO_PACKET_EXTENSION,
  GST_MPEG_VIDEO_PACKET_GOP,
};

GST_START_TEST (test_mpeg_parse)
{
  gint i, off;
  GstMpegVideoPacket packet;

  off = 12;
  for (i = 0; i < 4; ++i) {
    fail_unless (gst_mpeg_video_parse (&packet, mpeg2_seq, sizeof (mpeg2_seq),
            off));
    fail_unless (packet.offset == off + 4);
    if (i == 3) {
      fail_unless (GST_MPEG_VIDEO_PACKET_SLICE_MIN <= packet.type &&
          packet.type <= GST_MPEG_VIDEO_PACKET_SLICE_MAX);
      fail_unless (packet.size < 0);
    } else {
      assert_equals_int (ordercode[i], packet.type);
    }
    off = packet.offset + packet.size;
  }
}

GST_END_TEST;

GST_START_TEST (test_mpeg_parse_sequence_header)
{
  GstMpegVideoSequenceHdr seqhdr;
  GstMpegVideoPacket packet;

  gst_mpeg_video_parse (&packet, mpeg2_seq, sizeof (mpeg2_seq), 12);

  fail_unless (packet.type == GST_MPEG_VIDEO_PACKET_SEQUENCE);
  fail_unless (gst_mpeg_video_packet_parse_sequence_header (&packet, &seqhdr));
  assert_equals_int (seqhdr.width, 1920);
  assert_equals_int (seqhdr.height, 1080);
  assert_equals_int (seqhdr.aspect_ratio_info, 3);
  assert_equals_int (seqhdr.par_w, 64);
  assert_equals_int (seqhdr.par_h, 45);
  assert_equals_int (seqhdr.frame_rate_code, 7);
  assert_equals_int (seqhdr.fps_n, 60000);
  assert_equals_int (seqhdr.fps_d, 1001);
  assert_equals_int (seqhdr.bitrate_value, 262143);
  assert_equals_int (seqhdr.bitrate, 0);
  assert_equals_int (seqhdr.vbv_buffer_size_value, 512);
  fail_unless (seqhdr.constrained_parameters_flag == FALSE);
}

GST_END_TEST;

GST_START_TEST (test_mpeg_parse_sequence_extension)
{
  GstMpegVideoSequenceExt seqext;
  GstMpegVideoPacket packet;

  gst_mpeg_video_parse (&packet, mpeg2_seq, sizeof (mpeg2_seq), 24);

  fail_unless (packet.type == GST_MPEG_VIDEO_PACKET_EXTENSION);
  fail_unless (gst_mpeg_video_packet_parse_sequence_extension (&packet,
          &seqext));
  assert_equals_int (seqext.profile, 4);
  assert_equals_int (seqext.level, 8);
  assert_equals_int (seqext.progressive, 1);
  assert_equals_int (seqext.chroma_format, 1);
  assert_equals_int (seqext.horiz_size_ext, 0);
  assert_equals_int (seqext.vert_size_ext, 0);
  assert_equals_int (seqext.vert_size_ext, 0);
  assert_equals_int (seqext.bitrate_ext, 8);
  assert_equals_int (seqext.vbv_buffer_size_extension, 3);
  assert_equals_int (seqext.low_delay, 0);
  assert_equals_int (seqext.fps_n_ext, 3);
  assert_equals_int (seqext.fps_d_ext, 2);
}

GST_END_TEST;

GST_START_TEST (test_mis_identified_datas)
{
  GstMpegVideoPacket packet = { 0, };
  const guint8 *data = mis_identified_datas;
  gint i, off;

  off = 0;
  for (i = 0; i < 2; i++) {
    gst_mpeg_video_parse (&packet, mis_identified_datas,
        sizeof (mis_identified_datas), off);
    assert_equals_int (data[packet.offset - 4], 0);
    assert_equals_int (data[packet.offset - 3], 0);
    assert_equals_int (data[packet.offset - 2], 1);
    off = packet.offset + packet.size;
    if (i == 1)
      fail_unless (packet.size < 0);
    else
      fail_unless (packet.size > 0);
  }
}

GST_END_TEST;

static Suite *
mpegvideoparsers_suite (void)
{
  Suite *s = suite_create ("Video Parsers library");

  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_mpeg_parse);
  tcase_add_test (tc_chain, test_mpeg_parse_sequence_header);
  tcase_add_test (tc_chain, test_mpeg_parse_sequence_extension);
  tcase_add_test (tc_chain, test_mis_identified_datas);

  return s;
}

GST_CHECK_MAIN (mpegvideoparsers);
