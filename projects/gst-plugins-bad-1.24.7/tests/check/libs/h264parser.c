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
#include <gst/codecparsers/gsth264parser.h>

static guint8 slice_dpa[] = {
  0x00, 0x00, 0x01, 0x02, 0x00, 0x02, 0x01, 0x03, 0x00,
  0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x09, 0x00, 0x0a, 0x00,
  0x0b, 0x00, 0x0c, 0x00, 0x0d, 0x00, 0x0e, 0x00, 0x0f, 0x00, 0x10, 0x00,
  0x11, 0x00, 0x12, 0x00, 0x13, 0x00, 0x14, 0x00, 0x15, 0x00, 0x16, 0x00,
  0x17, 0x00, 0x18, 0x00, 0x19, 0x00, 0x1a, 0x00, 0x1b, 0x00, 0x1c, 0x00,
  0x1d, 0x00, 0x1e, 0x00, 0x1f, 0x00, 0x20, 0x00, 0x21, 0x00, 0x22, 0x00,
  0x23, 0x00, 0x24, 0x00, 0x25, 0x00, 0x26, 0x00, 0x27, 0x00, 0x28, 0x00,
  0x29, 0x00, 0x2a, 0x00, 0x2b, 0x00, 0x2c, 0x00, 0x2d, 0x00, 0x2e, 0x00,
  0x2f, 0x00, 0x30, 0x00, 0x31, 0x00, 0x32, 0x00, 0x33, 0x00, 0x34, 0x00,
  0x35, 0x00, 0x36, 0x00, 0x37, 0x00, 0x38, 0x00, 0x39, 0x00, 0x3a, 0x00,
  0x3b, 0x00, 0x3c, 0x00, 0x3d, 0x00, 0x3e, 0x00, 0x3f, 0x00, 0x40, 0x00,
  0x41, 0x00, 0x42, 0x00, 0x43, 0x00, 0x44, 0x00, 0x45, 0x00, 0x46, 0x00,
  0x47, 0x00, 0x48, 0x00, 0x49, 0x00, 0x4a, 0x00, 0x4b, 0x00, 0x4c, 0x00,
  0x4d, 0x00, 0x4e, 0x00, 0x4f, 0x00, 0x50, 0x00, 0x51, 0x00, 0x52, 0x00,
  0x53, 0x00, 0x54, 0x00, 0x55, 0x00, 0x56, 0x00, 0x57, 0x00, 0x58, 0x00,
  0x59, 0x00, 0x5a, 0x00, 0x5b, 0x00, 0x5c, 0x00, 0x5d, 0x00, 0x5e, 0x00,
  0x5f, 0x00, 0x60, 0x00, 0x61, 0x01, 0x04, 0x00, 0xc4, 0x00, 0xa6, 0x00,
  0xc5, 0x00, 0xab, 0x00, 0x82, 0x00, 0xc2, 0x00, 0xd8, 0x00, 0xc6, 0x00,
  0xe4, 0x00, 0xbe, 0x00, 0xb0, 0x00, 0xe6, 0x00, 0xb6, 0x00, 0xb7, 0x00,
  0xb4, 0x00, 0xb5, 0x00, 0x87, 0x00, 0xb2, 0x00, 0xb3, 0x00, 0xd9, 0x00,
  0x8c, 0x00, 0xe5, 0x00, 0xbf, 0x00, 0xb1, 0x00, 0xe7, 0x00, 0xbb, 0x00,
  0xa3, 0x00, 0x84, 0x00, 0x85, 0x00, 0xbd, 0x00, 0x96, 0x00, 0xe8, 0x00,
  0x86, 0x00, 0x8e, 0x00, 0x8b, 0x00, 0x9d, 0x00, 0xa9, 0x00, 0x8a, 0x01,
  0x05, 0x00, 0x83, 0x00, 0xf2, 0x00, 0xf3, 0x00, 0x8d, 0x00, 0x97, 0x00,
  0x88, 0x00, 0xde, 0x00, 0xf1, 0x00, 0x9e, 0x00, 0xaa, 0x00, 0xf5, 0x00,
  0xf4, 0x00, 0xf6, 0x00, 0xa2, 0x00, 0xad, 0x00, 0xc9, 0x00, 0xc7, 0x00,
  0xae, 0x00, 0x62, 0x00, 0x63, 0x00, 0x90, 0x00, 0x64, 0x00, 0xcb, 0x00,
  0x65, 0x00, 0xc8, 0x00, 0xca, 0x00, 0xcf, 0x00, 0xcc, 0x00, 0xcd, 0x00,
  0xce, 0x00, 0xe9, 0x00, 0x66, 0x00, 0xd3, 0x00, 0xd0, 0x00, 0xd1, 0x00,
  0xaf, 0x00, 0x67, 0x00, 0x91, 0x00, 0xd6, 0x00, 0xd4, 0x00, 0xd5, 0x00,
  0x68, 0x00, 0xeb, 0x00, 0xed, 0x00, 0x89, 0x00, 0x6a, 0x00, 0x69, 0x00,
  0x6b, 0x00, 0x6d, 0x00, 0x6c, 0x00, 0x6e, 0x00, 0xa0, 0x00, 0x6f, 0x00,
  0x71, 0x00, 0x70, 0x00, 0x72, 0x00, 0x73, 0x00, 0x75, 0x00, 0x74, 0x00,
  0x76, 0x00, 0x77, 0x00, 0xea, 0x00, 0x78, 0x00, 0x7a, 0x00, 0x79, 0x00,
  0x7b, 0x00, 0x7d, 0x00, 0x7c, 0x00, 0xa1, 0x00, 0x7f, 0x00, 0x7e, 0x00,
  0x80, 0x00, 0x81, 0x00, 0xec, 0x00, 0xee, 0x00, 0xba, 0x01, 0x06, 0x00,
  0xef, 0x00, 0xe1, 0x00, 0xe0, 0x00, 0xdc, 0x01, 0x07, 0x01, 0x08, 0x01,
  0x09, 0x01, 0x0a, 0x01, 0x0b, 0x01, 0x0c, 0x00, 0xdb, 0x00, 0xe2, 0x01,
  0x0d, 0x01, 0x0e, 0x01, 0x0f, 0x01, 0x10, 0x01, 0x11, 0x01, 0x12, 0x00,
  0xdf, 0x01, 0x13, 0x01, 0x14, 0x01, 0x15, 0x01, 0x16, 0x01, 0x17, 0x00,
  0xfd, 0x00, 0xff, 0x01, 0x18, 0x01, 0x19, 0x01, 0x1a, 0x01, 0x1b, 0x01,
  0x1c, 0x01, 0x1d, 0x01, 0x1e, 0x01, 0x1f, 0x01, 0x20, 0x01, 0x21, 0x01,
  0x22, 0x01, 0x23, 0x01, 0x24, 0x01, 0x25, 0x01, 0x26, 0x00, 0xfe, 0x01,
  0x00, 0x01, 0x27, 0x01, 0x28, 0x01, 0x29, 0x01, 0x2a, 0x01, 0x2b, 0x01,
  0x2c, 0x01, 0x2d, 0x01, 0x2e, 0x01, 0x2f, 0x01, 0x30, 0x01, 0x31, 0x00,
  0xe3, 0x00, 0xd7, 0x01, 0x32, 0x00, 0xf8, 0x00, 0xf9, 0x01, 0x33, 0x01,
  0x34, 0x01, 0x35, 0x01, 0x36, 0x01, 0x37, 0x01, 0x38, 0x01, 0x39, 0x01,
  0x3a, 0x01, 0x3b, 0x01, 0x3c, 0x01, 0x3d, 0x01, 0x3e, 0x01, 0x3f, 0x01,
  0x40, 0x01, 0x41, 0x01, 0x42, 0x01, 0x43, 0x01, 0x44, 0x01, 0x45, 0x01,
  0x46, 0x01, 0x47, 0x01, 0x48, 0x01, 0x49, 0x01, 0x4a, 0x01, 0x4b, 0x01,
  0x4c, 0x00, 0x08, 0x05, 0x2e, 0x6e, 0x75, 0x6c, 0x6c, 0x0c, 0x76, 0x69,
  0x73, 0x69, 0x62, 0x6c, 0x65, 0x73, 0x70, 0x61, 0x63, 0x65, 0x04, 0x45,
  0x75, 0x72, 0x6f, 0x06, 0x6d, 0x61, 0x63, 0x72, 0x6f, 0x6e, 0x0a, 0x62,
  0x75, 0x6c, 0x6c, 0x65, 0x74, 0x6d, 0x61, 0x74, 0x68, 0x06, 0x53, 0x61,
  0x63, 0x75, 0x74, 0x65, 0x06, 0x54, 0x63, 0x61, 0x72, 0x6f, 0x6e, 0x06,
  0x5a, 0x61, 0x63, 0x75, 0x74, 0x65, 0x06, 0x73, 0x61, 0x63, 0x75, 0x74,
  0x65, 0x06, 0x74, 0x63, 0x61, 0x72, 0x6f, 0x6e, 0x06, 0x7a, 0x61, 0x63,
  0x75, 0x74, 0x65, 0x07, 0x41, 0x6f, 0x67, 0x6f, 0x6e, 0x65, 0x6b, 0x07,
  0x61, 0x6f, 0x67, 0x6f, 0x6e, 0x65, 0x6b, 0x0c, 0x73, 0x63, 0x6f, 0x6d,
  0x6d, 0x61, 0x61, 0x63, 0x63, 0x65, 0x6e, 0x74, 0x0c, 0x53, 0x63, 0x6f,
  0x6d, 0x6d, 0x61, 0x61, 0x63, 0x63, 0x65, 0x6e, 0x74, 0x0a, 0x5a, 0x64,
  0x6f, 0x74, 0x61, 0x63, 0x63, 0x65, 0x6e, 0x74, 0x06, 0x4c, 0x63, 0x61,
  0x72, 0x6f, 0x6e, 0x06, 0x6c, 0x63, 0x61, 0x72, 0x6f, 0x6e, 0x0a, 0x7a,
  0x64, 0x6f, 0x74, 0x61, 0x63, 0x63, 0x65, 0x6e, 0x74, 0x06, 0x52, 0x61,
  0x63, 0x75, 0x74, 0x65, 0x06, 0x41, 0x62, 0x72, 0x65, 0x76, 0x65, 0x06,
  0x4c, 0x61, 0x63, 0x75, 0x74, 0x65, 0x07, 0x45, 0x6f, 0x67, 0x6f, 0x6e,
  0x65, 0x6b, 0x06, 0x45, 0x63, 0x61, 0x72, 0x6f, 0x6e, 0x06, 0x44, 0x63,
  0x61, 0x72, 0x6f, 0x6e, 0x07, 0x44, 0x6d, 0x61, 0x63, 0x72, 0x6f, 0x6e,
  0x06, 0x4e, 0x61, 0x63, 0x75, 0x74, 0x65, 0x06, 0x4e, 0x63, 0x61, 0x72,
  0x6f, 0x6e, 0x0d, 0x4f, 0x68, 0x75, 0x6e, 0x67, 0x61, 0x72, 0x75, 0x6d,
  0x6c, 0x61, 0x75, 0x74, 0x06, 0x52, 0x63, 0x61, 0x72, 0x6f, 0x6e, 0x05,
  0x55, 0x72, 0x69, 0x6e, 0x67, 0x09, 0x6e, 0x75, 0x6e, 0x67, 0x61, 0x64,
  0x65, 0x73, 0x68, 0x0d, 0x55, 0x68, 0x75, 0x6e, 0x67, 0x61, 0x72, 0x75,
  0x6d, 0x6c, 0x61, 0x75, 0x74, 0x0c, 0x54, 0x63, 0x6f, 0x6d, 0x6d, 0x61,
  0x61, 0x63, 0x63, 0x65, 0x6e, 0x74, 0x06, 0x72, 0x61, 0x63, 0x75, 0x74,
  0x65, 0x06, 0x61, 0x62, 0x72, 0x65, 0x76, 0x65, 0x06, 0x6c, 0x61, 0x63,
  0x75, 0x74, 0x65, 0x07, 0x65, 0x6f, 0x67, 0x6f, 0x6e, 0x65, 0x6b, 0x06,
  0x65, 0x63, 0x61, 0x72, 0x6f, 0x6e, 0x06, 0x64, 0x63, 0x61, 0x72, 0x6f,
  0x6e, 0x07, 0x64, 0x6d, 0x61, 0x63, 0x72, 0x6f, 0x6e, 0x06, 0x6e, 0x61,
  0x63, 0x75, 0x74, 0x65, 0x06, 0x6e, 0x63, 0x61, 0x72, 0x6f, 0x6e, 0x0d,
  0x6f, 0x68, 0x75, 0x6e, 0x67, 0x61, 0x72, 0x75, 0x6d, 0x6c, 0x61, 0x75,
  0x74, 0x06, 0x72, 0x63, 0x61, 0x72, 0x6f, 0x6e, 0x05, 0x75, 0x72, 0x69,
  0x6e, 0x67, 0x0d, 0x75, 0x68, 0x75, 0x6e, 0x67, 0x61, 0x72, 0x75, 0x6d,
  0x6c, 0x61, 0x75, 0x74, 0x0c, 0x74, 0x63, 0x6f, 0x6d, 0x6d, 0x61, 0x61,
  0x63, 0x63, 0x65, 0x6e, 0x74, 0x0a, 0x49, 0x64, 0x6f, 0x74, 0x61, 0x63,
  0x63, 0x65, 0x6e, 0x74, 0x0c, 0x52, 0x63, 0x6f, 0x6d, 0x6d, 0x61, 0x61,
  0x63, 0x63, 0x65, 0x6e, 0x74, 0x0c, 0x72, 0x63, 0x6f, 0x6d, 0x6d, 0x61,
  0x61, 0x63, 0x63, 0x65, 0x6e, 0x74, 0x07, 0x49, 0x6f, 0x67, 0x6f, 0x6e,
  0x65, 0x6b, 0x07, 0x41, 0x6d, 0x61, 0x63, 0x72, 0x6f, 0x6e, 0x07, 0x45,
  0x6d, 0x61, 0x63, 0x72, 0x6f, 0x6e, 0x0a, 0x45, 0x64, 0x6f, 0x74, 0x61,
  0x63, 0x63, 0x65, 0x6e, 0x74, 0x0c, 0x47, 0x63, 0x6f, 0x6d, 0x6d, 0x61,
  0x61, 0x63, 0x63, 0x65, 0x6e, 0x74, 0x0c, 0x4b, 0x63, 0x6f, 0x6d, 0x6d,
  0x61, 0x61, 0x63, 0x63, 0x65, 0x6e, 0x74, 0x07, 0x49, 0x6d, 0x61, 0x63,
  0x72, 0x6f, 0x6e, 0x0c, 0x4c, 0x63, 0x6f, 0x6d, 0x6d, 0x61, 0x61, 0x63,
  0x63, 0x65, 0x6e, 0x74, 0x0c, 0x4e, 0x63, 0x6f, 0x6d, 0x6d, 0x61, 0x61,
  0x63, 0x63, 0x65, 0x6e, 0x74, 0x07, 0x4f, 0x6d, 0x61, 0x63, 0x72, 0x6f,
  0x6e, 0x07, 0x55, 0x6f, 0x67, 0x6f, 0x6e, 0x65, 0x6b, 0x07, 0x55, 0x6d,
  0x61, 0x63, 0x72, 0x6f, 0x6e, 0x07, 0x69, 0x6f, 0x67, 0x6f, 0x6e, 0x65,
  0x6b, 0x07, 0x61, 0x6d, 0x61, 0x63, 0x72, 0x6f, 0x6e, 0x07, 0x65, 0x6d,
  0x61, 0x63, 0x72, 0x6f, 0x6e, 0x0a, 0x65, 0x64, 0x6f, 0x74, 0x61, 0x63,
  0x63, 0x65, 0x6e, 0x74, 0x0c, 0x67, 0x63, 0x6f, 0x6d, 0x6d, 0x61, 0x61,
  0x63, 0x63, 0x65, 0x6e, 0x74, 0x0c, 0x6b, 0x63, 0x6f, 0x6d, 0x6d, 0x61,
  0x61, 0x63, 0x63, 0x65, 0x6e, 0x74, 0x07, 0x69, 0x6d, 0x61, 0x63, 0x72,
  0x6f, 0x6e, 0x0c, 0x6c, 0x63, 0x6f, 0x6d, 0x6d, 0x61, 0x61, 0x63, 0x63,
  0x65, 0x6e, 0x74, 0x0c, 0x6e, 0x63, 0x6f, 0x6d, 0x6d, 0x61, 0x61, 0x63,
  0x63, 0x65, 0x6e, 0x74, 0x07, 0x6f, 0x6d, 0x61, 0x63, 0x72, 0x6f, 0x6e,
  0x07, 0x75, 0x6f, 0x67, 0x6f, 0x6e, 0x65, 0x6b, 0x07, 0x75, 0x6d, 0x61,
  0x63, 0x72, 0x6f, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02
};

/* IDR slice, SEQ_END, IDR slice, STREAM_END */
static guint8 slice_eoseq_slice[] = {
  0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00,
  0x10, 0xff, 0xfe, 0xf6, 0xf0, 0xfe, 0x05, 0x36,
  0x56, 0x04, 0x50, 0x96, 0x7b, 0x3f, 0x53, 0xe1,
  0x00, 0x00, 0x00, 0x01, 0x0a,
  0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00,
  0x10, 0xff, 0xfe, 0xf6, 0xf0, 0xfe, 0x05, 0x36,
  0x56, 0x04, 0x50, 0x96, 0x7b, 0x3f, 0x53, 0xe1,
  0x00, 0x00, 0x00, 0x01, 0x0b
};

GST_START_TEST (test_h264_parse_slice_dpa)
{
  GstH264ParserResult res;
  GstH264NalUnit nalu;

  GstH264NalParser *parser = gst_h264_nal_parser_new ();

  res = gst_h264_parser_identify_nalu (parser, slice_dpa, 0,
      sizeof (slice_dpa), &nalu);

  assert_equals_int (res, GST_H264_PARSER_OK);
  assert_equals_int (nalu.type, GST_H264_NAL_SLICE_DPA);

  gst_h264_nal_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h264_parse_slice_eoseq_slice)
{
  GstH264ParserResult res;
  GstH264NalUnit nalu;
  GstH264NalParser *const parser = gst_h264_nal_parser_new ();
  const guint8 *buf = slice_eoseq_slice;
  guint n, buf_size = sizeof (slice_eoseq_slice);

  res = gst_h264_parser_identify_nalu (parser, buf, 0, buf_size, &nalu);

  assert_equals_int (res, GST_H264_PARSER_OK);
  assert_equals_int (nalu.type, GST_H264_NAL_SLICE_IDR);
  assert_equals_int (nalu.size, 20);

  n = nalu.offset + nalu.size;
  buf += n;
  buf_size -= n;

  res = gst_h264_parser_identify_nalu (parser, buf, 0, buf_size, &nalu);

  assert_equals_int (res, GST_H264_PARSER_OK);
  assert_equals_int (nalu.type, GST_H264_NAL_SEQ_END);
  assert_equals_int (nalu.size, 1);

  n = nalu.offset + nalu.size;
  buf += n;
  buf_size -= n;

  res = gst_h264_parser_identify_nalu (parser, buf, 0, buf_size, &nalu);

  assert_equals_int (res, GST_H264_PARSER_OK);
  assert_equals_int (nalu.type, GST_H264_NAL_SLICE_IDR);
  assert_equals_int (nalu.size, 20);

  n = nalu.offset + nalu.size;
  buf += n;
  buf_size -= n;

  res = gst_h264_parser_identify_nalu (parser, buf, 0, buf_size, &nalu);

  assert_equals_int (res, GST_H264_PARSER_OK);
  assert_equals_int (nalu.type, GST_H264_NAL_STREAM_END);
  assert_equals_int (nalu.size, 1);

  gst_h264_nal_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h264_parse_slice_5bytes)
{
  GstH264ParserResult res;
  GstH264NalUnit nalu;
  GstH264NalParser *const parser = gst_h264_nal_parser_new ();
  const guint8 *buf = slice_eoseq_slice;

  res = gst_h264_parser_identify_nalu (parser, buf, 0, 5, &nalu);

  assert_equals_int (res, GST_H264_PARSER_NO_NAL_END);
  assert_equals_int (nalu.type, GST_H264_NAL_SLICE_IDR);
  assert_equals_int (nalu.size, 1);

  gst_h264_nal_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h264_parse_identify_nalu_avc)
{
  GstH264ParserResult res;
  GstH264NalUnit nalu;
  GstH264NalParser *const parser = gst_h264_nal_parser_new ();
  /* Skip 3 bytes for the start code */
  const gsize nal_size = sizeof (slice_dpa) - 3;
  const gsize buf_size = 4 + nal_size;
  guint8 *buf = g_new (guint8, buf_size);

  memcpy (buf + 4, slice_dpa + 3, nal_size);

  GST_WRITE_UINT16_BE (buf + 2, nal_size);
  res = gst_h264_parser_identify_nalu_avc (parser, buf, 2, buf_size, 2, &nalu);

  assert_equals_int (res, GST_H264_PARSER_OK);
  assert_equals_int (nalu.type, GST_H264_NAL_SLICE_DPA);
  assert_equals_int (nalu.offset, 4);
  assert_equals_int (nalu.size, nal_size);

  GST_WRITE_UINT32_BE (buf, nal_size);
  res = gst_h264_parser_identify_nalu_avc (parser, buf, 0, buf_size, 4, &nalu);

  assert_equals_int (res, GST_H264_PARSER_OK);
  assert_equals_int (nalu.type, GST_H264_NAL_SLICE_DPA);
  assert_equals_int (nalu.offset, 4);
  assert_equals_int (nalu.size, nal_size);

  GST_WRITE_UINT32_BE (buf, G_MAXUINT32);
  res = gst_h264_parser_identify_nalu_avc (parser, buf, 0, buf_size, 4, &nalu);

  assert_equals_int (res, GST_H264_PARSER_BROKEN_DATA);

  GST_WRITE_UINT32_BE (buf, G_MAXUINT32 - 2);
  res = gst_h264_parser_identify_nalu_avc (parser, buf, 0, buf_size, 4, &nalu);

  assert_equals_int (res, GST_H264_PARSER_BROKEN_DATA);

  GST_WRITE_UINT32_BE (buf, G_MAXUINT32 - 3);
  res = gst_h264_parser_identify_nalu_avc (parser, buf, 0, buf_size, 4, &nalu);

  assert_equals_int (res, GST_H264_PARSER_BROKEN_DATA);

  GST_WRITE_UINT32_BE (buf, G_MAXUINT32 - 4);
  res = gst_h264_parser_identify_nalu_avc (parser, buf, 0, buf_size, 4, &nalu);

  assert_equals_int (res, GST_H264_PARSER_NO_NAL_END);

  GST_WRITE_UINT32_BE (buf, G_MAXUINT32 - 6);
  res = gst_h264_parser_identify_nalu_avc (parser, buf, 0, buf_size, 4, &nalu);

  assert_equals_int (res, GST_H264_PARSER_NO_NAL_END);

  g_free (buf);
  gst_h264_nal_parser_free (parser);
}

GST_END_TEST;

static guint8 nalu_sps_with_vui[] = {
  0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x28,
  0xac, 0xd9, 0x40, 0x78, 0x04, 0x4f, 0xde, 0x03,
  0xd2, 0x02, 0x02, 0x02, 0x80, 0x00, 0x01, 0xf4,
  0x80, 0x00, 0x75, 0x30, 0x4f, 0x8b, 0x16, 0xcb
};

static guint8 nalu_sei_pic_timing[] = {
  0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x01, 0x32, 0x80
};

static guint8 nalu_chained_sei[] = {
  0x00, 0x00, 0x01, 0x06, 0x01, 0x02, 0x32, 0x80,
  0x06, 0x01, 0xc4, 0x80
};

/* Content light level information SEI message */
static guint8 h264_sei_cll[] = {
  0x00, 0x00, 0x00, 0x01, 0x06, 0x90, 0x04, 0x03, 0xe8, 0x01, 0x90, 0x80
};

/* Mastering display colour volume information SEI message */
static guint8 h264_sei_mdcv[] = {
  0x00, 0x00, 0x00, 0x01, 0x06, 0x89, 0x18, 0x84,
  0xd0, 0x3e, 0x80, 0x33, 0x90, 0x86, 0xc4, 0x1d,
  0x4c, 0x0b, 0xb8, 0x3d, 0x13, 0x40, 0x42, 0x00,
  0x98, 0x96, 0x80, 0x00, 0x00, 0x03, 0x00, 0x01,
  0x80
};

/* closed caption data */
static guint8 h264_sei_user_data_registered[] = {
  0x00, 0x00, 0x00, 0x01, 0x06, 0x04, 0x47, 0xb5, 0x00, 0x31, 0x47, 0x41,
  0x39, 0x34, 0x03, 0xd4,
  0xff, 0xfc, 0x80, 0x80, 0xfd, 0x80, 0x80, 0xfa, 0x00, 0x00, 0xfa, 0x00,
  0x00, 0xfa, 0x00, 0x00,
  0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
  0xfa, 0x00, 0x00, 0xfa,
  0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa,
  0x00, 0x00, 0xfa, 0x00,
  0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00,
  0x00, 0xff, 0x80
};

static guint8 h264_sei_user_data_unregistered[] = {
  0x00, 0x00, 0x00, 0x01, 0x06,
  0x05,                         // Payload type.
  0x18,                         // Payload size.
  0x4D, 0x49, 0x53, 0x50, 0x6D, 0x69, 0x63, 0x72, 0x6F, 0x73, 0x65, 0x63,
  0x74, 0x69, 0x6D, 0x65,       // UUID.
  0x70, 0x69, 0x67, 0x73, 0x20, 0x66, 0x6c, 0x79,       // Payload data
  0x80
};

/* frame packing, side-by-side */
static guint8 h264_sei_frame_packing[] = {
  0x00, 0x00, 0x00, 0x01, 0x06, 0x2d, 0x07, 0x81, 0x81, 0x00, 0x00, 0x03,
  0x00, 0x01, 0x20, 0x80
};

GST_START_TEST (test_h264_parse_invalid_sei)
{
  GstH264ParserResult res;
  GstH264NalUnit nalu;
  GstH264NalParser *const parser = gst_h264_nal_parser_new ();
  const guint8 *buf = nalu_sps_with_vui;
  GArray *seis = NULL;
  GstH264SEIMessage *sei;

  /* First try parsing the SEI, which will fail because there's no SPS yet */
  res =
      gst_h264_parser_identify_nalu (parser, nalu_sei_pic_timing, 0,
      sizeof (nalu_sei_pic_timing), &nalu);
  assert_equals_int (res, GST_H264_PARSER_NO_NAL_END);
  assert_equals_int (nalu.type, GST_H264_NAL_SEI);

  res = gst_h264_parser_parse_sei (parser, &nalu, &seis);
  assert_equals_int (res, GST_H264_PARSER_BROKEN_LINK);
  g_array_free (seis, TRUE);

  /* Inject SPS */
  res =
      gst_h264_parser_identify_nalu (parser, buf, 0, sizeof (nalu_sps_with_vui),
      &nalu);
  assert_equals_int (res, GST_H264_PARSER_NO_NAL_END);
  assert_equals_int (nalu.type, GST_H264_NAL_SPS);
  assert_equals_int (nalu.size, 28);

  res = gst_h264_parser_parse_nal (parser, &nalu);
  assert_equals_int (res, GST_H264_PARSER_OK);

  /* Parse the SEI again */
  res =
      gst_h264_parser_identify_nalu (parser, nalu_sei_pic_timing, 0,
      sizeof (nalu_sei_pic_timing), &nalu);
  assert_equals_int (res, GST_H264_PARSER_NO_NAL_END);
  assert_equals_int (nalu.type, GST_H264_NAL_SEI);

  res = gst_h264_parser_parse_sei (parser, &nalu, &seis);
  assert_equals_int (res, GST_H264_PARSER_OK);
  fail_if (seis == NULL);
  assert_equals_int (seis->len, 1);
  g_array_free (seis, TRUE);

  /* Parse NALU with 2 chained SEI */
  res =
      gst_h264_parser_identify_nalu (parser, nalu_chained_sei, 0,
      sizeof (nalu_chained_sei), &nalu);
  assert_equals_int (res, GST_H264_PARSER_NO_NAL_END);
  assert_equals_int (nalu.type, GST_H264_NAL_SEI);

  res = gst_h264_parser_parse_sei (parser, &nalu, &seis);
  assert_equals_int (res, GST_H264_PARSER_OK);
  fail_if (seis == NULL);
  assert_equals_int (seis->len, 2);
  sei = &g_array_index (seis, GstH264SEIMessage, 0);
  assert_equals_int (sei->payloadType, GST_H264_SEI_PIC_TIMING);

  sei = &g_array_index (seis, GstH264SEIMessage, 1);
  assert_equals_int (sei->payloadType, GST_H264_SEI_RECOVERY_POINT);

  g_array_free (seis, TRUE);

  gst_h264_nal_parser_free (parser);
}

GST_END_TEST;

typedef gboolean (*SEICheckFunc) (gconstpointer a, gconstpointer b);

static gboolean
check_sei_user_data_registered (const GstH264RegisteredUserData * a,
    const GstH264RegisteredUserData * b)
{
  if (a->country_code != b->country_code)
    return FALSE;

  if ((a->country_code == 0xff) &&
      (a->country_code_extension != b->country_code_extension))
    return FALSE;

  if (a->size != b->size)
    return FALSE;

  return !memcmp (a->data, b->data, a->size);
}

static gboolean
check_sei_user_data_unregistered (const GstH264UserDataUnregistered * a,
    const GstH264UserDataUnregistered * b)
{
  return a->size == b->size &&
      !memcmp (a->uuid, b->uuid, sizeof (a->uuid)) &&
      !memcmp (a->data, b->data, a->size);
}

static gboolean
check_sei_frame_packing (const GstH264FramePacking * a,
    const GstH264FramePacking * b)
{
  if ((a->frame_packing_id != b->frame_packing_id) ||
      (a->frame_packing_cancel_flag != b->frame_packing_cancel_flag))
    return FALSE;

  if (!a->frame_packing_cancel_flag) {
    if ((a->frame_packing_type != b->frame_packing_type) ||
        (a->quincunx_sampling_flag != b->quincunx_sampling_flag) ||
        (a->content_interpretation_type != b->content_interpretation_type) ||
        (a->spatial_flipping_flag != b->spatial_flipping_flag) ||
        (a->frame0_flipped_flag != b->frame0_flipped_flag) ||
        (a->field_views_flag != b->field_views_flag) ||
        (a->current_frame_is_frame0_flag != b->current_frame_is_frame0_flag) ||
        (a->frame0_self_contained_flag != b->frame0_self_contained_flag) ||
        (a->frame1_self_contained_flag != b->frame1_self_contained_flag))
      return FALSE;

    if (!a->quincunx_sampling_flag &&
        a->frame_packing_type != GST_H264_FRAME_PACKING_TEMPORAL_INTERLEAVING) {
      if ((a->frame0_grid_position_x != b->frame0_grid_position_x) ||
          (a->frame0_grid_position_y != b->frame0_grid_position_y) ||
          (a->frame1_grid_position_x != b->frame1_grid_position_x) ||
          (a->frame1_grid_position_y != b->frame1_grid_position_y))
        return FALSE;
    }

    if (a->frame_packing_repetition_period !=
        b->frame_packing_repetition_period)
      return FALSE;
  }

  return TRUE;
}

static gboolean
check_sei_mdcv (const GstH264MasteringDisplayColourVolume * a,
    const GstH264MasteringDisplayColourVolume * b)
{
  gint i;
  for (i = 0; i < 3; i++) {
    if (a->display_primaries_x[i] != b->display_primaries_x[i] ||
        a->display_primaries_y[i] != b->display_primaries_y[i])
      return FALSE;
  }

  return (a->white_point_x == b->white_point_x) &&
      (a->white_point_y == b->white_point_y) &&
      (a->max_display_mastering_luminance == b->max_display_mastering_luminance)
      && (a->min_display_mastering_luminance ==
      b->min_display_mastering_luminance);
}

static gboolean
check_sei_cll (const GstH264ContentLightLevel * a,
    const GstH264ContentLightLevel * b)
{
  return (a->max_content_light_level == b->max_content_light_level) &&
      (a->max_pic_average_light_level == b->max_pic_average_light_level);
}

static gboolean
check_sei_pic_timing (const GstH264PicTiming * a, const GstH264PicTiming * b)
{
  if (a->CpbDpbDelaysPresentFlag != b->CpbDpbDelaysPresentFlag)
    return FALSE;

  if (a->CpbDpbDelaysPresentFlag) {
    if (a->cpb_removal_delay != b->cpb_removal_delay ||
        a->cpb_removal_delay_length_minus1 != b->cpb_removal_delay_length_minus1
        || a->dpb_output_delay != b->dpb_output_delay
        || a->dpb_output_delay_length_minus1 !=
        b->dpb_output_delay_length_minus1)
      return FALSE;
  }

  if (a->pic_struct_present_flag != b->pic_struct_present_flag)
    return FALSE;

  if (a->pic_struct_present_flag) {
    const guint8 num_clock_ts_table[9] = {
      1, 1, 1, 2, 2, 3, 3, 2, 3
    };
    guint8 num_clock_num_ts;
    guint i;

    if (a->pic_struct != b->pic_struct)
      return FALSE;

    if (a->time_offset_length != b->time_offset_length)
      return FALSE;

    num_clock_num_ts = num_clock_ts_table[a->pic_struct];

    for (i = 0; i < num_clock_num_ts; i++) {
      if (a->clock_timestamp_flag[i] != b->clock_timestamp_flag[i])
        return FALSE;

      if (a->clock_timestamp_flag[i]) {
        const GstH264ClockTimestamp *ta = &a->clock_timestamp[i];
        const GstH264ClockTimestamp *tb = &b->clock_timestamp[i];

        if (ta->ct_type != tb->ct_type ||
            ta->nuit_field_based_flag != tb->nuit_field_based_flag ||
            ta->counting_type != tb->counting_type ||
            ta->discontinuity_flag != tb->discontinuity_flag ||
            ta->cnt_dropped_flag != tb->cnt_dropped_flag ||
            ta->n_frames != tb->n_frames)
          return FALSE;

        if (ta->full_timestamp_flag) {
          if (ta->seconds_value != tb->seconds_value ||
              ta->minutes_value != tb->minutes_value ||
              ta->hours_value != tb->hours_value)
            return FALSE;
        } else {
          if (ta->seconds_flag != tb->seconds_flag)
            return FALSE;

          if (ta->seconds_flag) {
            if (ta->seconds_value != tb->seconds_value ||
                ta->minutes_flag != tb->minutes_flag)
              return FALSE;

            if (ta->minutes_flag) {
              if (ta->minutes_value != tb->minutes_value ||
                  ta->hours_flag != tb->hours_flag)
                return FALSE;

              if (ta->hours_flag) {
                if (ta->hours_value != tb->hours_value)
                  return FALSE;
              }
            }
          }
        }

        if (ta->time_offset != tb->time_offset)
          return FALSE;
      }
    }
  }

  return TRUE;
}

GST_START_TEST (test_h264_create_sei)
{
  GstH264NalParser *parser;
  GstH264ParserResult parse_ret;
  GstH264NalUnit nalu;
  GArray *msg_array = NULL;
  GstMemory *mem;
  gint i;
  GstMapInfo info;
  struct
  {
    guint8 *raw_data;
    guint len;
    GstH264SEIPayloadType type;
    GstH264SEIMessage parsed_message;
    SEICheckFunc check_func;
  } test_list[] = {
    /* *INDENT-OFF* */
    {h264_sei_user_data_registered, G_N_ELEMENTS (h264_sei_user_data_registered),
        GST_H264_SEI_REGISTERED_USER_DATA, {0,},
        (SEICheckFunc) check_sei_user_data_registered},
    {h264_sei_user_data_unregistered, G_N_ELEMENTS (h264_sei_user_data_unregistered),
        GST_H264_SEI_USER_DATA_UNREGISTERED, {0,},
        (SEICheckFunc) check_sei_user_data_unregistered},
    {h264_sei_frame_packing, G_N_ELEMENTS (h264_sei_frame_packing),
        GST_H264_SEI_FRAME_PACKING, {0,},
        (SEICheckFunc) check_sei_frame_packing},
    {h264_sei_mdcv, G_N_ELEMENTS (h264_sei_mdcv),
        GST_H264_SEI_MASTERING_DISPLAY_COLOUR_VOLUME, {0,},
        (SEICheckFunc) check_sei_mdcv},
    {h264_sei_cll, G_N_ELEMENTS (h264_sei_cll),
        GST_H264_SEI_CONTENT_LIGHT_LEVEL, {0,},
        (SEICheckFunc) check_sei_cll},
    {nalu_sei_pic_timing, G_N_ELEMENTS (nalu_sei_pic_timing),
        GST_H264_SEI_PIC_TIMING, {0,},
        (SEICheckFunc) check_sei_pic_timing},
    /* *INDENT-ON* */
  };

  parser = gst_h264_nal_parser_new ();

  /* inject SPS for picture timing sei */
  parse_ret =
      gst_h264_parser_identify_nalu_unchecked (parser, nalu_sps_with_vui, 0,
      sizeof (nalu_sps_with_vui), &nalu);
  assert_equals_int (parse_ret, GST_H264_PARSER_OK);
  assert_equals_int (nalu.type, GST_H264_NAL_SPS);
  assert_equals_int (nalu.size, 28);

  parse_ret = gst_h264_parser_parse_nal (parser, &nalu);
  assert_equals_int (parse_ret, GST_H264_PARSER_OK);

  /* test single sei message per sei nal unit */
  for (i = 0; i < G_N_ELEMENTS (test_list); i++) {
    gsize nal_size;

    parse_ret = gst_h264_parser_identify_nalu_unchecked (parser,
        test_list[i].raw_data, 0, test_list[i].len, &nalu);
    assert_equals_int (parse_ret, GST_H264_PARSER_OK);
    assert_equals_int (nalu.type, GST_H264_NAL_SEI);

    parse_ret = gst_h264_parser_parse_sei (parser, &nalu, &msg_array);
    assert_equals_int (parse_ret, GST_H264_PARSER_OK);
    assert_equals_int (msg_array->len, 1);

    /* test bytestream */
    mem = gst_h264_create_sei_memory (4, msg_array);
    fail_unless (mem != NULL);
    fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
    GST_MEMDUMP ("created sei nal", info.data, info.size);
    GST_MEMDUMP ("original sei nal", test_list[i].raw_data, test_list[i].len);
    assert_equals_int (info.size, test_list[i].len);
    fail_if (memcmp (info.data, test_list[i].raw_data, test_list[i].len));
    gst_memory_unmap (mem, &info);
    gst_memory_unref (mem);

    /* test packetized */
    mem = gst_h264_create_sei_memory_avc (4, msg_array);
    fail_unless (mem != NULL);
    fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
    assert_equals_int (info.size, test_list[i].len);
    fail_if (memcmp (info.data + 4, test_list[i].raw_data + 4,
            test_list[i].len - 4));
    nal_size = GST_READ_UINT32_BE (info.data);
    assert_equals_int (nal_size, info.size - 4);
    gst_memory_unmap (mem, &info);
    gst_memory_unref (mem);

    /* store parsed SEI for following tests */
    test_list[i].parsed_message =
        g_array_index (msg_array, GstH264SEIMessage, 0);
    if (test_list[i].type == GST_H264_SEI_REGISTERED_USER_DATA) {
      GstH264RegisteredUserData *dst_rud =
          &test_list[i].parsed_message.payload.registered_user_data;
      const GstH264SEIMessage *src_msg =
          &g_array_index (msg_array, GstH264SEIMessage, 0);
      const GstH264RegisteredUserData *src_rud =
          &src_msg->payload.registered_user_data;

      dst_rud->data = g_malloc (src_rud->size);
      memcpy ((guint8 *) dst_rud->data, src_rud->data, src_rud->size);
    } else if (test_list[i].type == GST_H264_SEI_USER_DATA_UNREGISTERED) {
      GstH264UserDataUnregistered *dst_udu =
          &test_list[i].parsed_message.payload.user_data_unregistered;
      const GstH264SEIMessage *src_msg =
          &g_array_index (msg_array, GstH264SEIMessage, 0);
      const GstH264UserDataUnregistered *src_udu =
          &src_msg->payload.user_data_unregistered;

      dst_udu->data = g_malloc (src_udu->size);
      memcpy ((guint8 *) dst_udu->data, src_udu->data, src_udu->size);
    }
    g_array_unref (msg_array);
  }

  /* test multiple SEI messages in a nal unit */
  msg_array = g_array_new (FALSE, FALSE, sizeof (GstH264SEIMessage));
  for (i = 0; i < G_N_ELEMENTS (test_list); i++)
    g_array_append_val (msg_array, test_list[i].parsed_message);

  mem = gst_h264_create_sei_memory (4, msg_array);
  fail_unless (mem != NULL);
  g_array_unref (msg_array);

  /* parse sei message from buffer */
  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
  parse_ret = gst_h264_parser_identify_nalu_unchecked (parser,
      info.data, 0, info.size, &nalu);
  assert_equals_int (parse_ret, GST_H264_PARSER_OK);
  assert_equals_int (nalu.type, GST_H264_NAL_SEI);
  parse_ret = gst_h264_parser_parse_sei (parser, &nalu, &msg_array);
  gst_memory_unmap (mem, &info);
  gst_memory_unref (mem);

  assert_equals_int (parse_ret, GST_H264_PARSER_OK);
  assert_equals_int (msg_array->len, G_N_ELEMENTS (test_list));
  for (i = 0; i < msg_array->len; i++) {
    GstH264SEIMessage *msg = &g_array_index (msg_array, GstH264SEIMessage, i);

    assert_equals_int (msg->payloadType, test_list[i].type);
    fail_unless (test_list[i].check_func (&msg->payload,
            &test_list[i].parsed_message.payload));
  }

  /* clean up */
  for (i = 0; i < G_N_ELEMENTS (test_list); i++)
    gst_h264_sei_clear (&test_list[i].parsed_message);

  g_array_unref (msg_array);
  gst_h264_nal_parser_free (parser);
}

GST_END_TEST;

static guint8 h264_avc_codec_data[] = {
  0x01, 0x4d, 0x40, 0x15, 0xff, 0xe1, 0x00, 0x17,
  0x67, 0x4d, 0x40, 0x15, 0xec, 0xa4, 0xbf, 0x2e,
  0x02, 0x20, 0x00, 0x00, 0x03, 0x00, 0x2e, 0xe6,
  0xb2, 0x80, 0x01, 0xe2, 0xc5, 0xb2, 0xc0, 0x01,
  0x00, 0x04, 0x68, 0xeb, 0xec, 0xb2
};

/* *INDENT-OFF* */
static guint8 h264_avc3_codec_data[] = {
  0x01, /* config version, always == 1 */
  0x4d, /* profile */
  0x40, /* profile compatibility */
  0x15, /* level */
  0xff, /* 6 reserved bits, lengthSizeMinusOne */
  0xe0, /* 3 reserved bits, numSPS */
  0x00  /* numPPS */
};

static guint8 h264_wrong_version_codec_data[] = {
  0x00, /* config version, wrong value 0 */
  0x4d, /* profile */
  0x40, /* profile compatibility */
  0x15, /* level */
  0xff, /* 6 reserved bits, lengthSizeMinusOne */
  0xe0, /* 3 reserved bits, numSPS */
  0x00  /* numPPS */
};

static guint8 h264_wrong_length_size_codec_data[] = {
  0x01, /* config version, always == 1 */
  0x4d, /* profile */
  0x40, /* profile compatibility */
  0x15, /* level */
  0xfe, /* 6 reserved bits, invalid lengthSizeMinusOne 3 */
  0xe0, /* 3 reserved bits, numSPS */
  0x00  /* numPPS */
};
/* *INDENT-ON* */

GST_START_TEST (test_h264_decoder_config_record)
{
  GstH264NalParser *parser;
  GstH264ParserResult ret;
  GstH264DecoderConfigRecord *config = NULL;
  GstH264SPS sps;
  GstH264PPS pps;
  GstH264NalUnit *nalu;

  parser = gst_h264_nal_parser_new ();

  /* avc */
  ret = gst_h264_parser_parse_decoder_config_record (parser,
      h264_avc_codec_data, sizeof (h264_avc_codec_data), &config);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  fail_unless (config != NULL);
  assert_equals_int (config->configuration_version, 1);
  assert_equals_int (config->length_size_minus_one, 3);

  assert_equals_int (config->sps->len, 1);
  nalu = &g_array_index (config->sps, GstH264NalUnit, 0);
  assert_equals_int (nalu->type, GST_H264_NAL_SPS);
  ret = gst_h264_parser_parse_sps (parser, nalu, &sps);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  gst_h264_sps_clear (&sps);

  assert_equals_int (config->pps->len, 1);
  nalu = &g_array_index (config->pps, GstH264NalUnit, 0);
  assert_equals_int (nalu->type, GST_H264_NAL_PPS);
  ret = gst_h264_parser_parse_pps (parser, nalu, &pps);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  gst_h264_pps_clear (&pps);
  g_clear_pointer (&config, gst_h264_decoder_config_record_free);

  /* avc3 */
  ret = gst_h264_parser_parse_decoder_config_record (parser,
      h264_avc3_codec_data, sizeof (h264_avc3_codec_data), &config);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  fail_unless (config != NULL);

  assert_equals_int (config->configuration_version, 1);
  assert_equals_int (config->length_size_minus_one, 3);
  assert_equals_int (config->sps->len, 0);
  assert_equals_int (config->pps->len, 0);
  g_clear_pointer (&config, gst_h264_decoder_config_record_free);

  /* avc3 wrong size, return error with null config data */
  ret = gst_h264_parser_parse_decoder_config_record (parser,
      h264_avc3_codec_data, sizeof (h264_avc3_codec_data) - 1, &config);
  assert_equals_int (ret, GST_H264_PARSER_ERROR);
  fail_unless (config == NULL);

  /* wrong version, return error with null config data */
  ret = gst_h264_parser_parse_decoder_config_record (parser,
      h264_wrong_version_codec_data, sizeof (h264_wrong_version_codec_data),
      &config);
  assert_equals_int (ret, GST_H264_PARSER_ERROR);
  fail_unless (config == NULL);

  /* wrong length size, since we still try to use it. so return ok */
  ret = gst_h264_parser_parse_decoder_config_record (parser,
      h264_wrong_length_size_codec_data,
      sizeof (h264_wrong_length_size_codec_data), &config);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  fail_unless (config != NULL);
  assert_equals_int (config->length_size_minus_one, 2);
  g_clear_pointer (&config, gst_h264_decoder_config_record_free);

  gst_h264_nal_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h264_parse_partial_nal_header)
{
  GstH264ParserResult res;
  GstH264NalUnit nalu;
  GstH264NalParser *const parser = gst_h264_nal_parser_new ();
  const guint8 buf[] = { 0x00, 0x00, 0x00, 0x01, 0x0e };
  guint buf_size = sizeof (buf);

  /* Test that incomplete prefix NAL do return NO_NAL_END and not BROKEN_NAL.
   * This also covers for SLICE_EXT. */
  res = gst_h264_parser_identify_nalu (parser, buf, 0, buf_size, &nalu);

  assert_equals_int (res, GST_H264_PARSER_NO_NAL);
  assert_equals_int (nalu.type, GST_H264_NAL_PREFIX_UNIT);
  assert_equals_int (nalu.size, 0);

  gst_h264_nal_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h264_split_avc)
{
  GstH264NalParser *parser;
  GArray *array;
  GstH264NalUnit *nal;
  static const guint8 aud[] = { 0x09, 0xf0 };
  static const guint8 seq_end[] = { 0x0a };
  static const guint8 sc_3bytes[] = { 0x00, 0x00, 0x01 };
  static const guint8 sc_4bytes[] = { 0x00, 0x00, 0x00, 0x01 };
  const guint8 nal_length_size = 4;
  guint8 data[128];
  gsize size;
  GstH264ParserResult ret;
  gsize consumed;
  guint off;

  parser = gst_h264_nal_parser_new ();
  array = g_array_new (FALSE, FALSE, sizeof (GstH264NalUnit));

#define BUILD_NAL(arr) G_STMT_START { \
  memcpy (data + off, arr, sizeof (arr)); \
  off += sizeof (arr); \
} G_STMT_END

  /* 1) Complete packetized nalu */
  size = nal_length_size + sizeof (aud);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (aud));
  BUILD_NAL (aud);
  ret = gst_h264_parser_identify_and_split_nalu_avc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  assert_equals_int (array->len, 1);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH264NalUnit, 0);
  assert_equals_int (nal->type, GST_H264_NAL_AU_DELIMITER);
  assert_equals_int (nal->sc_offset, 0);
  assert_equals_int (nal->offset, nal_length_size);
  assert_equals_int (nal->size, sizeof (aud));

  /* 2-1) SC (3 bytes) + nalu */
  size = nal_length_size + sizeof (sc_3bytes) + sizeof (aud);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (sc_3bytes) + sizeof (aud));
  BUILD_NAL (sc_3bytes);
  BUILD_NAL (aud);
  ret = gst_h264_parser_identify_and_split_nalu_avc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  assert_equals_int (array->len, 1);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH264NalUnit, 0);
  assert_equals_int (nal->type, GST_H264_NAL_AU_DELIMITER);
  assert_equals_int (nal->sc_offset, nal_length_size);
  assert_equals_int (nal->offset, nal_length_size + sizeof (sc_3bytes));
  assert_equals_int (nal->size, sizeof (aud));

  /* 2-2) SC (4 bytes) + nalu */
  size = nal_length_size + sizeof (sc_4bytes) + sizeof (aud);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (sc_4bytes) + sizeof (aud));
  BUILD_NAL (sc_4bytes);
  BUILD_NAL (aud);
  ret = gst_h264_parser_identify_and_split_nalu_avc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  assert_equals_int (array->len, 1);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH264NalUnit, 0);
  assert_equals_int (nal->type, GST_H264_NAL_AU_DELIMITER);
  assert_equals_int (nal->sc_offset, nal_length_size);
  assert_equals_int (nal->offset, nal_length_size + sizeof (sc_4bytes));
  assert_equals_int (nal->size, sizeof (aud));

  /* 3-1) nalu + trailing SC (3 bytes) */
  size = nal_length_size + sizeof (aud) + sizeof (sc_3bytes);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (aud) + sizeof (sc_3bytes));
  BUILD_NAL (aud);
  BUILD_NAL (sc_3bytes);
  ret = gst_h264_parser_identify_and_split_nalu_avc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  assert_equals_int (array->len, 1);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH264NalUnit, 0);
  assert_equals_int (nal->type, GST_H264_NAL_AU_DELIMITER);
  assert_equals_int (nal->sc_offset, 0);
  assert_equals_int (nal->offset, nal_length_size);
  assert_equals_int (nal->size, sizeof (aud));

  /* 3-2) nalu + trailing SC (4 bytes) */
  size = nal_length_size + sizeof (aud) + sizeof (sc_4bytes);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (aud) + sizeof (sc_4bytes));
  BUILD_NAL (aud);
  BUILD_NAL (sc_4bytes);
  ret = gst_h264_parser_identify_and_split_nalu_avc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  assert_equals_int (array->len, 1);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH264NalUnit, 0);
  assert_equals_int (nal->type, GST_H264_NAL_AU_DELIMITER);
  assert_equals_int (nal->sc_offset, 0);
  assert_equals_int (nal->offset, nal_length_size);
  assert_equals_int (nal->size, sizeof (aud));

  /* 4-1) SC + nalu + SC + nalu */
  size = nal_length_size + sizeof (sc_3bytes) + sizeof (aud) +
      sizeof (sc_4bytes) + sizeof (seq_end);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (sc_3bytes) + sizeof (aud) +
      sizeof (sc_4bytes) + sizeof (seq_end));
  BUILD_NAL (sc_3bytes);
  BUILD_NAL (aud);
  BUILD_NAL (sc_4bytes);
  BUILD_NAL (seq_end);
  ret = gst_h264_parser_identify_and_split_nalu_avc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  assert_equals_int (array->len, 2);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH264NalUnit, 0);
  assert_equals_int (nal->type, GST_H264_NAL_AU_DELIMITER);
  assert_equals_int (nal->sc_offset, nal_length_size);
  assert_equals_int (nal->offset, nal_length_size + sizeof (sc_3bytes));
  assert_equals_int (nal->size, sizeof (aud));
  nal = &g_array_index (array, GstH264NalUnit, 1);
  assert_equals_int (nal->type, GST_H264_NAL_SEQ_END);
  assert_equals_int (nal->sc_offset, nal_length_size + sizeof (sc_3bytes)
      + sizeof (aud));
  assert_equals_int (nal->offset, nal_length_size + sizeof (sc_3bytes)
      + sizeof (aud) + sizeof (sc_4bytes));
  assert_equals_int (nal->size, sizeof (seq_end));

  /* 4-2) SC + nalu + SC + nalu + trailing SC */
  size = nal_length_size + sizeof (sc_3bytes) + sizeof (aud) +
      sizeof (sc_4bytes) + sizeof (seq_end) + sizeof (sc_3bytes);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (sc_3bytes) + sizeof (aud) +
      sizeof (sc_4bytes) + sizeof (seq_end) + sizeof (sc_3bytes));
  BUILD_NAL (sc_3bytes);
  BUILD_NAL (aud);
  BUILD_NAL (sc_4bytes);
  BUILD_NAL (seq_end);
  BUILD_NAL (sc_3bytes);
  ret = gst_h264_parser_identify_and_split_nalu_avc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  assert_equals_int (array->len, 2);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH264NalUnit, 0);
  assert_equals_int (nal->type, GST_H264_NAL_AU_DELIMITER);
  assert_equals_int (nal->sc_offset, nal_length_size);
  assert_equals_int (nal->offset, nal_length_size + sizeof (sc_3bytes));
  assert_equals_int (nal->size, sizeof (aud));
  nal = &g_array_index (array, GstH264NalUnit, 1);
  assert_equals_int (nal->type, GST_H264_NAL_SEQ_END);
  assert_equals_int (nal->sc_offset, nal_length_size + sizeof (sc_3bytes)
      + sizeof (aud));
  assert_equals_int (nal->offset, nal_length_size + sizeof (sc_3bytes)
      + sizeof (aud) + sizeof (sc_4bytes));
  assert_equals_int (nal->size, sizeof (seq_end));

  /* 4-3) nalu + SC + nalu */
  size = nal_length_size + sizeof (aud) + sizeof (sc_4bytes) + sizeof (seq_end);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (aud) + sizeof (sc_4bytes) +
      sizeof (seq_end));
  BUILD_NAL (aud);
  BUILD_NAL (sc_4bytes);
  BUILD_NAL (seq_end);
  ret = gst_h264_parser_identify_and_split_nalu_avc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H264_PARSER_OK);
  assert_equals_int (array->len, 2);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH264NalUnit, 0);
  assert_equals_int (nal->type, GST_H264_NAL_AU_DELIMITER);
  assert_equals_int (nal->sc_offset, 0);
  assert_equals_int (nal->offset, nal_length_size);
  assert_equals_int (nal->size, sizeof (aud));
  nal = &g_array_index (array, GstH264NalUnit, 1);
  assert_equals_int (nal->type, GST_H264_NAL_SEQ_END);
  assert_equals_int (nal->sc_offset, nal_length_size + sizeof (aud));
  assert_equals_int (nal->offset,
      nal_length_size + sizeof (aud) + sizeof (sc_4bytes));
  assert_equals_int (nal->size, sizeof (seq_end));

#undef BUILD_NAL

  gst_h264_nal_parser_free (parser);
  g_array_unref (array);
}

GST_END_TEST;

static Suite *
h264parser_suite (void)
{
  Suite *s = suite_create ("H264 Parser library");

  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_h264_parse_slice_dpa);
  tcase_add_test (tc_chain, test_h264_parse_slice_eoseq_slice);
  tcase_add_test (tc_chain, test_h264_parse_slice_5bytes);
  tcase_add_test (tc_chain, test_h264_parse_identify_nalu_avc);
  tcase_add_test (tc_chain, test_h264_parse_invalid_sei);
  tcase_add_test (tc_chain, test_h264_create_sei);
  tcase_add_test (tc_chain, test_h264_decoder_config_record);
  tcase_add_test (tc_chain, test_h264_parse_partial_nal_header);
  tcase_add_test (tc_chain, test_h264_split_avc);

  return s;
}

GST_CHECK_MAIN (h264parser);
