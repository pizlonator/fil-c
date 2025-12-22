/* Gstreamer
 * Copyright (C) <2018> Collabora Ltd.
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
#include <gst/codecparsers/gsth265parser.h>
#include <string.h>

unsigned char slice_eos_slice_eob[] = {
  0x00, 0x00, 0x00, 0x01, 0x26, 0x01, 0xaf, 0x06, 0xb8, 0x63, 0xef, 0x3a,
  0x7f, 0x3e, 0x53, 0xff, 0xff, 0xf2, 0x4a, 0xef, 0xff, 0xfe, 0x6a, 0x5d,
  0x60, 0xbc, 0xf8, 0x29, 0xeb, 0x9c, 0x4a, 0xb5, 0xcc, 0x76, 0x30, 0xa0,
  0x7c, 0xd3, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x19, 0x30,
  0x00, 0x00, 0x00, 0x01, 0x48, 0x01,
  0x00, 0x00, 0x00, 0x01, 0x26, 0x01, 0xaf, 0x06, 0xb8, 0x63, 0xef, 0x3a,
  0x7f, 0x3e, 0x53, 0xff, 0xff, 0xf2, 0x4a, 0xef, 0xff, 0xfe, 0x6a, 0x5d,
  0x60, 0xbc, 0xf8, 0x29, 0xeb, 0x9c, 0x4a, 0xb5, 0xcc, 0x76, 0x30, 0xa0,
  0x7c, 0xd3, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x19, 0x30,
  0x00, 0x00, 0x00, 0x01, 0x4a, 0x01,
};

static const guint8 h265_vps_with_nonzero_max_layer_id[] = {
  0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01,
  0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
  0xb0, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
  0x5d, 0xac, 0x59
};

static const guint8 h265_pps_with_range_extension[] = {
  0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0x30, 0x42, 0x13, 0x1c, 0x0c, 0x60,
  0xe1, 0xd9, 0x38, 0x83, 0xb6, 0x38, 0x2c, 0x19, 0x29, 0x82, 0x42, 0xee,
  0x61, 0xec, 0x28, 0x11, 0x1a, 0x51, 0xc1, 0x60, 0xc9, 0x4c, 0x12, 0x17,
  0x73, 0x0f, 0x61, 0x40, 0x88, 0xd1, 0x05, 0x38, 0x20, 0x28, 0x94, 0xc5,
  0x1c, 0x26, 0x70, 0xb0, 0x44, 0x20, 0x30, 0x69, 0x4a, 0x16, 0x12, 0x2c,
  0x20, 0x83, 0xe3, 0x06, 0x87, 0x87, 0xc7, 0x30, 0xa9, 0x22, 0xd0, 0xb1,
  0x01, 0x40, 0x98, 0xa1, 0x02, 0x47, 0x33, 0x85, 0x43, 0xc1, 0x31, 0x01,
  0x18, 0x68, 0x2e, 0x3a, 0x20, 0x22, 0x20, 0x48, 0xc0, 0xd8, 0xe0, 0xa8,
  0xa1, 0xc5, 0x04, 0x05, 0x12, 0x98, 0xa3, 0x84, 0xce, 0x16, 0x08, 0x84,
  0x06, 0x0d, 0x29, 0x42, 0xc2, 0x45, 0x84, 0x10, 0x7c, 0x60, 0xd0, 0xf0,
  0xf8, 0xe6, 0x15, 0x24, 0x5a, 0x16, 0x20, 0x28, 0x13, 0x14, 0x20, 0x48,
  0xe6, 0x70, 0xa8, 0x78, 0x26, 0x20, 0x23, 0x0d, 0x05, 0xc7, 0x44, 0x04,
  0x44, 0x09, 0x18, 0x1b, 0x1c, 0x15, 0x14, 0x3a, 0x08, 0x0a, 0x25, 0x31,
  0x47, 0x09, 0x9c, 0x2c, 0x11, 0x08, 0x0c, 0x1a, 0x52, 0x85, 0x84, 0x8b,
  0x08, 0x20, 0xf8, 0xc1, 0xa1, 0xe1, 0xf1, 0xcc, 0x2a, 0x48, 0xb4, 0x2c,
  0x40, 0x50, 0x26, 0x28, 0x40, 0x91, 0xcc, 0xe1, 0x50, 0xf0, 0x4c, 0x40,
  0x46, 0x1a, 0x0b, 0x8e, 0x88, 0x08, 0x88, 0x12, 0x30, 0x36, 0x38, 0x2a,
  0x28, 0x71, 0x41, 0x01, 0x44, 0xa6, 0x28, 0xe1, 0x33, 0x85, 0x82, 0x21,
  0x01, 0x83, 0x4a, 0x50, 0xb0, 0x91, 0x61, 0x04, 0x1f, 0x18, 0x34, 0x3c,
  0x3e, 0x39, 0x85, 0x49, 0x16, 0x85, 0x88, 0x0a, 0x04, 0xc5, 0x08, 0x12,
  0x39, 0x9c, 0x2a, 0x1e, 0x09, 0x88, 0x08, 0xc3, 0x41, 0x71, 0xd1, 0x01,
  0x11, 0x02, 0x46, 0x06, 0xc7, 0x05, 0x45, 0x0e, 0x82, 0x00, 0x88, 0xc0,
  0xa9, 0xc3, 0x08, 0xc1, 0xf0, 0xf1, 0x43, 0xe2, 0x04, 0x04, 0x83, 0x28,
  0x51, 0x03, 0x64, 0x20, 0x70, 0x22, 0x13, 0x08, 0x18, 0x68, 0xd1, 0xc3,
  0x04, 0x8d, 0x87, 0x85, 0x86, 0x43, 0x81, 0x50, 0xd0, 0xf0, 0x98, 0x70,
  0xa6, 0x1e, 0x34, 0x31, 0x0d, 0x87, 0x82, 0xe8, 0xf0, 0xc0, 0xd2, 0x94,
  0xa1, 0x20, 0xcb, 0x31, 0x88, 0xa0, 0x80, 0x22, 0x30, 0x2a, 0x70, 0xc2,
  0x30, 0x7c, 0x3c, 0x50, 0xf8, 0x81, 0x01, 0x20, 0xca, 0x14, 0x40, 0xd9,
  0x08, 0x1c, 0x08, 0x84, 0xc2, 0x06, 0x1a, 0x34, 0x70, 0xc1, 0x23, 0x61,
  0xe1, 0x61, 0x90, 0xe0, 0x54, 0x34, 0x3c, 0x26, 0x1c, 0x29, 0x87, 0x8d,
  0x0c, 0x43, 0x61, 0xe0, 0xba, 0x3c, 0x30, 0x34, 0xa5, 0x28, 0x48, 0x32,
  0xcc, 0x68, 0x20, 0x08, 0x8c, 0x0a, 0x9c, 0x30, 0x8c, 0x1f, 0x0f, 0x14,
  0x3e, 0x20, 0x40, 0x48, 0x32, 0x85, 0x10, 0x36, 0x42, 0x07, 0x02, 0x21,
  0x30, 0x81, 0x86, 0x8d, 0x1c, 0x30, 0x48, 0xd8, 0x78, 0x58, 0x64, 0x38,
  0x15, 0x0d, 0x0f, 0x09, 0x87, 0x0a, 0x61, 0xe3, 0x43, 0x10, 0xd8, 0x78,
  0x2e, 0x8f, 0x0c, 0x0d, 0x29, 0x4a, 0x12, 0x0c, 0xb3, 0x1a, 0x08, 0x02,
  0x23, 0x02, 0xa7, 0x0c, 0x23, 0x07, 0xc3, 0xc5, 0x0f, 0x88, 0x10, 0x12,
  0x0c, 0xa1, 0x44, 0x0d, 0x90, 0x81, 0xc0, 0x88, 0x4c, 0x20, 0x61, 0xa3,
  0x47, 0x0c, 0x12, 0x36, 0x1e, 0x16, 0x19, 0x0e, 0x05, 0x43, 0x43, 0xc2,
  0x61, 0xc2, 0x98, 0x78, 0xd0, 0xc4, 0x36, 0x1e, 0x0b, 0xa3, 0xc3, 0x03,
  0x4a, 0x52, 0x84, 0x83, 0x2c, 0xc6, 0x4a, 0x56, 0x01, 0x46, 0x89, 0x0c,
  0xce, 0x25, 0x04, 0x83, 0x21, 0x96, 0x3b, 0x80,
};

static const guint8 h265_with_scc_extension[] = {
  0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x09, 0x00, 0x40,
  0x00, 0x00, 0x0e, 0x0c, 0x00, 0x00, 0x03, 0x00, 0x00, 0x3c, 0x9b, 0x02, 0x40,
  0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x09, 0x00, 0x40, 0x00, 0x00, 0x0e,
  0x0c, 0x00, 0x00, 0x03, 0x00, 0x00, 0x3c, 0xa0, 0x0d, 0x08, 0x0f, 0x1f, 0xe5,
  0x9b, 0x92, 0x46, 0xd8, 0x79, 0x79, 0x24, 0x93, 0xf9, 0xe7, 0xf3, 0xcb, 0xff,
  0xff, 0xff, 0x3f, 0x9f, 0xcf, 0xcf, 0xe7, 0x6d, 0x90, 0xf3, 0x60, 0x40, 0x02,
  0x12, 0xc0, 0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xc1, 0x94, 0x95, 0x81, 0x14,
  0x42, 0x40, 0x0a,
};

static guint8 h265_sps[] = {
  0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x21, 0x60, 0x00, 0x00, 0x03,
  0x00, 0xb0, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x99, 0xa0, 0x01,
  0xe0, 0x20, 0x02, 0x1c, 0x59, 0x4b, 0x92, 0x42, 0x96, 0x11, 0x80, 0xb5,
  0x01, 0x01, 0x01, 0x14, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x03,
  0x00, 0xf3, 0xf2, 0x00, 0x6e, 0x00, 0x17, 0xbd, 0xf8, 0x00, 0x02, 0x94,
  0xb4, 0x00, 0x06, 0x9b, 0x60, 0x00, 0xd3, 0x6c, 0x00, 0x01, 0x4a, 0x5a,
  0x40, 0x00, 0x14, 0xa5, 0xa0, 0x00, 0x34, 0xdb, 0x00, 0x06, 0x9b, 0x60,
  0x00, 0x0a, 0x52, 0xd0, 0x40,
};

static guint8 h265_sei_pic_timing[] = {
  0x00, 0x00, 0x00, 0x01, 0x4e, 0x01, 0x01, 0x10, 0x04, 0x00, 0x00, 0x03, 0x00,
  0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x08, 0xaf, 0xff, 0xff,
  0xff, 0xfe, 0x80
};

/* hdr10plus dynamic metadata */
static guint8 h265_sei_user_data_registered[] = {
  0x00, 0x00, 0x00, 0x01, 0x4e, 0x01, 0x04, 0x40, 0xb5, 0x00, 0x3c, 0x00, 0x01,
  0x04, 0x01, 0x40, 0x00, 0x0c, 0x80, 0x8b, 0x4c, 0x41, 0xff, 0x1b, 0xd6,
  0x01, 0x03, 0x64, 0x08, 0x00, 0x0c, 0x28, 0xdb, 0x20, 0x50, 0x00, 0xac,
  0xc8, 0x00, 0xe1, 0x90, 0x03, 0x6e, 0x58, 0x10, 0x32, 0xd0, 0x2a, 0x6a,
  0xf8, 0x48, 0xf3, 0x18, 0xe1, 0xb4, 0x00, 0x40, 0x44, 0x10, 0x25, 0x09,
  0xa6, 0xae, 0x5c, 0x83, 0x50, 0xdd, 0xf9, 0x8e, 0xc7, 0xbd, 0x00, 0x80
};

static guint8 h265_sei_user_data_unregistered[] = {
  0x00, 0x00, 0x00, 0x01, 0x4e, 0x01,
  0x05,                         // Payload type.
  0x18,                         // Payload size.
  0x4D, 0x49, 0x53, 0x50, 0x6D, 0x69, 0x63, 0x72, 0x6F, 0x73, 0x65, 0x63,
  0x74, 0x69, 0x6D, 0x65,       // UUID.
  0x70, 0x69, 0x67, 0x73, 0x20, 0x66, 0x6c, 0x79,       // Payload data
  0x80
};

static guint8 h265_sei_time_code[] = {
  0x00, 0x00, 0x00, 0x01, 0x4e, 0x01, 0x88, 0x06, 0x60, 0x40, 0x00, 0x00, 0x03,
  0x00, 0x10, 0x80
};

static guint8 h265_sei_mdcv[] = {
  0x00, 0x00, 0x00, 0x01, 0x4e, 0x01, 0x89, 0x18, 0x33, 0xc2, 0x86, 0xc4, 0x1d,
  0x4c, 0x0b, 0xb8, 0x84, 0xd0, 0x3e, 0x80, 0x3d, 0x13, 0x40, 0x42, 0x00, 0x98,
  0x96, 0x80, 0x00, 0x00, 0x03, 0x00, 0x01, 0x80
};

static guint8 h265_sei_cll[] = {
  0x00, 0x00, 0x00, 0x01, 0x4e, 0x01, 0x90, 0x04, 0x03, 0xe8, 0x01, 0x90, 0x80
};

GST_START_TEST (test_h265_parse_slice_eos_slice_eob)
{
  GstH265ParserResult res;
  GstH265NalUnit nalu;
  GstH265Parser *parser = gst_h265_parser_new ();
  const guint8 *buf = slice_eos_slice_eob;
  guint n, buf_size = sizeof (slice_eos_slice_eob);

  res = gst_h265_parser_identify_nalu (parser, buf, 0, buf_size, &nalu);

  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_SLICE_IDR_W_RADL);
  assert_equals_int (nalu.size, buf_size / 2 - 10);     /* 2 slices, 1 start code(4) and EOx(6) */

  n = nalu.offset + nalu.size;
  buf += n;
  buf_size -= n;

  res = gst_h265_parser_identify_nalu (parser, buf, 0, buf_size, &nalu);

  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_EOS);
  assert_equals_int (nalu.size, 2);

  n = nalu.offset + nalu.size;
  buf += n;
  buf_size -= n;

  res = gst_h265_parser_identify_nalu (parser, buf, 0, buf_size, &nalu);

  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_SLICE_IDR_W_RADL);
  assert_equals_int (nalu.size, 43);

  n = nalu.offset + nalu.size;
  buf += n;
  buf_size -= n;

  res = gst_h265_parser_identify_nalu (parser, buf, 0, buf_size, &nalu);

  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_EOB);
  assert_equals_int (nalu.size, 2);

  gst_h265_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h265_parse_pic_timing)
{
  GstH265NalUnit nalu;
  GstH265Parser *parser = gst_h265_parser_new ();
  const guint8 *buf = h265_sps;
  guint i, buf_size = sizeof (h265_sps);
  GArray *messages;
  GstH265SEIMessage sei;
  GstH265SPS sps;

  assert_equals_int (gst_h265_parser_identify_nalu (parser, buf, 0, buf_size,
          &nalu), GST_H265_PARSER_NO_NAL_END);
  assert_equals_int (nalu.type, GST_H265_NAL_SPS);
  assert_equals_int (nalu.size, buf_size - 4);  /* 4 for start_code */

  assert_equals_int (gst_h265_parser_parse_sps (parser, &nalu, &sps, TRUE),
      GST_H265_PARSER_OK);

  buf = h265_sei_pic_timing;
  buf_size = sizeof (h265_sei_pic_timing);

  assert_equals_int (gst_h265_parser_identify_nalu (parser, buf, 0, buf_size,
          &nalu), GST_H265_PARSER_NO_NAL_END);
  assert_equals_int (nalu.type, GST_H265_NAL_PREFIX_SEI);
  assert_equals_int (nalu.size, buf_size - 4);  /* 4 for start_code size */

  assert_equals_int (gst_h265_parser_parse_sei (parser, &nalu, &messages),
      GST_H265_PARSER_OK);

  for (i = 0; i < messages->len; i++) {
    sei = g_array_index (messages, GstH265SEIMessage, i);
    assert_equals_int (sei.payloadType, GST_H265_SEI_PIC_TIMING);
    assert_equals_int (sei.payload.pic_timing.pic_struct, 0);
    assert_equals_int (sei.payload.pic_timing.source_scan_type, 1);
    assert_equals_int (sei.payload.pic_timing.duplicate_flag, 0);
    assert_equals_int (sei.payload.pic_timing.au_cpb_removal_delay_minus1, 0);
    assert_equals_int (sei.payload.pic_timing.pic_dpb_output_delay, 0);
    assert_equals_int (sei.payload.pic_timing.pic_dpb_output_du_delay, 0);
    assert_equals_int (sei.payload.pic_timing.num_decoding_units_minus1, 33);
    assert_equals_int (sei.payload.pic_timing.du_common_cpb_removal_delay_flag,
        1);
    assert_equals_int (sei.payload.
        pic_timing.du_common_cpb_removal_delay_increment_minus1, 0);
  }
  g_array_free (messages, TRUE);
  gst_h265_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h265_parse_slice_6bytes)
{
  GstH265ParserResult res;
  GstH265NalUnit nalu;
  GstH265Parser *const parser = gst_h265_parser_new ();
  const guint8 *buf = slice_eos_slice_eob;

  res = gst_h265_parser_identify_nalu (parser, buf, 0, 6, &nalu);

  assert_equals_int (res, GST_H265_PARSER_NO_NAL_END);
  assert_equals_int (nalu.type, GST_H265_NAL_SLICE_IDR_W_RADL);
  assert_equals_int (nalu.size, 2);

  gst_h265_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h265_parse_identify_nalu_hevc)
{
  GstH265ParserResult res;
  GstH265NalUnit nalu;
  GstH265Parser *parser = gst_h265_parser_new ();
  /* Skip 4 bytes for the start code */
  const gsize nal_size = sizeof (slice_eos_slice_eob) - 4;
  const gsize buf_size = 4 + nal_size;
  guint8 *buf = g_new (guint8, buf_size);

  memcpy (buf + 4, slice_eos_slice_eob + 4, nal_size);

  GST_WRITE_UINT16_BE (buf + 2, nal_size);
  res = gst_h265_parser_identify_nalu_hevc (parser, buf, 2, buf_size, 2, &nalu);

  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_SLICE_IDR_W_RADL);
  assert_equals_int (nalu.offset, 4);
  assert_equals_int (nalu.size, nal_size);

  GST_WRITE_UINT32_BE (buf, nal_size);
  res = gst_h265_parser_identify_nalu_hevc (parser, buf, 0, buf_size, 4, &nalu);

  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_SLICE_IDR_W_RADL);
  assert_equals_int (nalu.offset, 4);
  assert_equals_int (nalu.size, nal_size);

  GST_WRITE_UINT32_BE (buf, G_MAXUINT32);
  res = gst_h265_parser_identify_nalu_hevc (parser, buf, 0, buf_size, 4, &nalu);

  assert_equals_int (res, GST_H265_PARSER_BROKEN_DATA);

  GST_WRITE_UINT32_BE (buf, G_MAXUINT32 - 2);
  res = gst_h265_parser_identify_nalu_hevc (parser, buf, 0, buf_size, 4, &nalu);

  assert_equals_int (res, GST_H265_PARSER_BROKEN_DATA);

  GST_WRITE_UINT32_BE (buf, G_MAXUINT32 - 3);
  res = gst_h265_parser_identify_nalu_hevc (parser, buf, 0, buf_size, 4, &nalu);

  assert_equals_int (res, GST_H265_PARSER_BROKEN_DATA);

  GST_WRITE_UINT32_BE (buf, G_MAXUINT32 - 4);
  res = gst_h265_parser_identify_nalu_hevc (parser, buf, 0, buf_size, 4, &nalu);

  assert_equals_int (res, GST_H265_PARSER_NO_NAL_END);

  GST_WRITE_UINT32_BE (buf, G_MAXUINT32 - 6);
  res = gst_h265_parser_identify_nalu_hevc (parser, buf, 0, buf_size, 4, &nalu);

  assert_equals_int (res, GST_H265_PARSER_NO_NAL_END);

  g_free (buf);
  gst_h265_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h265_base_profiles)
{
  GstH265ProfileTierLevel ptl;

  memset (&ptl, 0, sizeof (ptl));

  ptl.profile_idc = 1;
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN);
  ptl.profile_idc = 2;
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_10);
  ptl.profile_idc = 3;
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_STILL_PICTURE);

  ptl.profile_idc = 42;
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_INVALID);
}

GST_END_TEST;

GST_START_TEST (test_h265_base_profiles_compat)
{
  GstH265ProfileTierLevel ptl;

  memset (&ptl, 0, sizeof (ptl));

  ptl.profile_compatibility_flag[1] = 1;
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN);
  ptl.profile_compatibility_flag[1] = 0;

  ptl.profile_compatibility_flag[2] = 1;
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_10);
  ptl.profile_compatibility_flag[2] = 0;

  ptl.profile_compatibility_flag[3] = 1;
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_STILL_PICTURE);
  ptl.profile_compatibility_flag[3] = 0;

  ptl.profile_idc = 42;
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_INVALID);
}

GST_END_TEST;

static void
set_format_range_fields (GstH265ProfileTierLevel * ptl,
    guint8 max_14bit_constraint_flag,
    guint8 max_12bit_constraint_flag,
    guint8 max_10bit_constraint_flag,
    guint8 max_8bit_constraint_flag,
    guint8 max_422chroma_constraint_flag,
    guint8 max_420chroma_constraint_flag,
    guint8 max_monochrome_constraint_flag,
    guint8 intra_constraint_flag,
    guint8 one_picture_only_constraint_flag,
    guint8 lower_bit_rate_constraint_flag)
{
  ptl->max_14bit_constraint_flag = max_14bit_constraint_flag;
  ptl->max_12bit_constraint_flag = max_12bit_constraint_flag;
  ptl->max_10bit_constraint_flag = max_10bit_constraint_flag;
  ptl->max_8bit_constraint_flag = max_8bit_constraint_flag;
  ptl->max_422chroma_constraint_flag = max_422chroma_constraint_flag;
  ptl->max_420chroma_constraint_flag = max_420chroma_constraint_flag;
  ptl->max_monochrome_constraint_flag = max_monochrome_constraint_flag;
  ptl->intra_constraint_flag = intra_constraint_flag;
  ptl->one_picture_only_constraint_flag = one_picture_only_constraint_flag;
  ptl->lower_bit_rate_constraint_flag = lower_bit_rate_constraint_flag;
}

static void
set_chroma_idc_and_depth (GstH265SPS * sps, guint8 chroma_idc,
    guint8 depth_luma, guint8 depth_chroma)
{
  sps->chroma_format_idc = chroma_idc;
  sps->bit_depth_luma_minus8 = depth_luma - 8;
  sps->bit_depth_chroma_minus8 = depth_chroma - 8;
}

GST_START_TEST (test_h265_format_range_profiles_exact_match)
{
  /* Test all the combinations from Table A.2 */
  GstH265ProfileTierLevel ptl;

  memset (&ptl, 0, sizeof (ptl));
  ptl.profile_idc = 4;

  set_format_range_fields (&ptl, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MONOCHROME);

  set_format_range_fields (&ptl, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MONOCHROME_12);

  set_format_range_fields (&ptl, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MONOCHROME_16);

  set_format_range_fields (&ptl, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_12);

  set_format_range_fields (&ptl, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_422_10);

  set_format_range_fields (&ptl, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_422_12);

  set_format_range_fields (&ptl, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444);

  set_format_range_fields (&ptl, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_10);

  set_format_range_fields (&ptl, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_12);

  set_format_range_fields (&ptl, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_INTRA);
  set_format_range_fields (&ptl, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_INTRA);

  set_format_range_fields (&ptl, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_10_INTRA);
  set_format_range_fields (&ptl, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_10_INTRA);

  set_format_range_fields (&ptl, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_12_INTRA);
  set_format_range_fields (&ptl, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_12_INTRA);

  set_format_range_fields (&ptl, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_422_10_INTRA);
  set_format_range_fields (&ptl, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_422_10_INTRA);

  set_format_range_fields (&ptl, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_422_12_INTRA);
  set_format_range_fields (&ptl, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_422_12_INTRA);

  set_format_range_fields (&ptl, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_INTRA);
  set_format_range_fields (&ptl, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_INTRA);

  set_format_range_fields (&ptl, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_10_INTRA);
  set_format_range_fields (&ptl, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_10_INTRA);

  set_format_range_fields (&ptl, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_12_INTRA);
  set_format_range_fields (&ptl, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_12_INTRA);

  set_format_range_fields (&ptl, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_16_INTRA);
  set_format_range_fields (&ptl, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_16_INTRA);

  set_format_range_fields (&ptl, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_STILL_PICTURE);
  set_format_range_fields (&ptl, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_STILL_PICTURE);

  set_format_range_fields (&ptl, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_16_STILL_PICTURE);
  set_format_range_fields (&ptl, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MAIN_444_16_STILL_PICTURE);

  ptl.profile_idc = 5;
  set_format_range_fields (&ptl, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_HIGH_THROUGHPUT_444);
  set_format_range_fields (&ptl, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_HIGH_THROUGHPUT_444_10);
  set_format_range_fields (&ptl, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_HIGH_THROUGHPUT_444_14);
  set_format_range_fields (&ptl, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_HIGH_THROUGHPUT_444_16_INTRA);

  ptl.profile_idc = 6;
  set_format_range_fields (&ptl, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_MULTIVIEW_MAIN);

  ptl.profile_idc = 7;
  set_format_range_fields (&ptl, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_SCALABLE_MAIN_10);

  ptl.profile_idc = 8;
  set_format_range_fields (&ptl, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_3D_MAIN);

  ptl.profile_idc = 9;
  set_format_range_fields (&ptl, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_10);
  set_format_range_fields (&ptl, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444_10);

  ptl.profile_idc = 10;
  set_format_range_fields (&ptl, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_SCALABLE_MONOCHROME);
  set_format_range_fields (&ptl, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_SCALABLE_MONOCHROME_16);

  ptl.profile_idc = 11;
  set_format_range_fields (&ptl, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (&ptl), ==,
      GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_10);
}

GST_END_TEST;

GST_START_TEST (test_h265_format_range_profiles_partial_match)
{
  /* Test matching compatible profiles from non-standard bitstream */
  GstH265SPS sps;
  GstH265ProfileTierLevel *ptl = &sps.profile_tier_level;

  memset (&sps, 0, sizeof (sps));
  ptl->profile_idc = 4;
  set_format_range_fields (ptl, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (ptl), ==,
      GST_H265_PROFILE_MAIN_444);

  ptl->profile_idc = 5;
  /* wrong max_monochrome_constraint_flag, should still be compatible
     with GST_H265_PROFILE_HIGH_THROUGHPUT_444_10 */
  set_format_range_fields (ptl, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (ptl), ==,
      GST_H265_PROFILE_HIGH_THROUGHPUT_444_10);
  /* wrong max_12bit_constraint_flag, should still be compatible
     with GST_H265_PROFILE_HIGH_THROUGHPUT_444_14 */
  set_format_range_fields (ptl, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (ptl), ==,
      GST_H265_PROFILE_HIGH_THROUGHPUT_444_14);
  /* wrong intra_constraint_flag, GST_H265_PROFILE_HIGH_THROUGHPUT_444_14
     and GST_H265_PROFILE_HIGH_THROUGHPUT_444_16_INTRA are both compatible,
     but GST_H265_PROFILE_HIGH_THROUGHPUT_444_16_INTRA should be chosen
     because of the higher priority. */
  set_format_range_fields (ptl, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (ptl), ==,
      GST_H265_PROFILE_HIGH_THROUGHPUT_444_16_INTRA);

  ptl->profile_idc = 6;
  /* wrong max_12bit_constraint_flag, should not be compatible with any */
  set_format_range_fields (ptl, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (ptl), ==,
      GST_H265_PROFILE_INVALID);

  ptl->profile_idc = 7;
  /* wrong max_monochrome_constraint_flag, and intra_constraint_flag,
     still compatible with GST_H265_PROFILE_SCALABLE_MAIN_10 */
  set_format_range_fields (ptl, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (ptl), ==,
      GST_H265_PROFILE_SCALABLE_MAIN_10);

  ptl->profile_idc = 8;
  /* wrong one_picture_only_constraint_flag, still compatible
     with GST_H265_PROFILE_3D_MAIN */
  set_format_range_fields (ptl, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (ptl), ==,
      GST_H265_PROFILE_3D_MAIN);

  ptl->profile_idc = 9;
  /* wrong one_picture_only_constraint_flag, still compatible
     with GST_H265_PROFILE_SCREEN_EXTENDED_MAIN */
  set_format_range_fields (ptl, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (ptl), ==,
      GST_H265_PROFILE_SCREEN_EXTENDED_MAIN);
  /* wrong indications but have right chroma_format_idc and bit_depth in SPS,
     should be recognized as GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444 */
  set_format_range_fields (ptl, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (ptl), ==,
      GST_H265_PROFILE_INVALID);
  set_chroma_idc_and_depth (&sps, 3, 8, 8);
  g_assert_cmpuint (gst_h265_get_profile_from_sps (&sps), ==,
      GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444);

  ptl->profile_idc = 10;
  /* wrong max_10bit_constraint_flag, still compatible
     with GST_H265_PROFILE_SCALABLE_MONOCHROME_16 */
  set_format_range_fields (ptl, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (ptl), ==,
      GST_H265_PROFILE_SCALABLE_MONOCHROME_16);

  ptl->profile_idc = 11;
  /* wrong max_12bit_constraint_flag and max_422chroma_constraint_flag,
     should be recognized as GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_14 */
  set_format_range_fields (ptl, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1);
  g_assert_cmpuint (gst_h265_profile_tier_level_get_profile (ptl), ==,
      GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_14);

  ptl->profile_idc = 2;
  /* main and main10 compatibility flags but with 10 bith depth */
  ptl->profile_compatibility_flag[1] = 1;
  ptl->profile_compatibility_flag[2] = 1;
  set_format_range_fields (ptl, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  set_chroma_idc_and_depth (&sps, 1, 10, 10);
  g_assert_cmpuint (gst_h265_get_profile_from_sps (&sps), ==,
      GST_H265_PROFILE_MAIN_10);
}

GST_END_TEST;

GST_START_TEST (test_h265_parse_vps)
{
  /* Parsing non-zero vps_max_layer_id in VPS
   * https://bugzilla.gnome.org/show_bug.cgi?id=797279 */
  GstH265Parser *parser;
  GstH265NalUnit nalu;
  GstH265ParserResult res;
  GstH265VPS vps;
  GstH265Profile profile;

  parser = gst_h265_parser_new ();

  res = gst_h265_parser_identify_nalu_unchecked (parser,
      h265_vps_with_nonzero_max_layer_id, 0,
      sizeof (h265_vps_with_nonzero_max_layer_id), &nalu);

  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_VPS);

  res = gst_h265_parser_parse_vps (parser, &nalu, &vps);
  assert_equals_int (res, GST_H265_PARSER_OK);

  assert_equals_int (vps.id, 0);
  assert_equals_int (vps.max_layers_minus1, 0);
  assert_equals_int (vps.max_sub_layers_minus1, 0);
  assert_equals_int (vps.temporal_id_nesting_flag, 1);

  profile = gst_h265_profile_tier_level_get_profile (&vps.profile_tier_level);

  assert_equals_int (profile, GST_H265_PROFILE_MAIN);
  assert_equals_int (vps.sub_layer_ordering_info_present_flag, 1);

  assert_equals_int (vps.max_dec_pic_buffering_minus1[0], 1);
  assert_equals_int (vps.max_num_reorder_pics[0], 0);
  assert_equals_int (vps.max_latency_increase_plus1[0], 0);

  assert_equals_int (vps.max_layer_id, 5);
  assert_equals_int (vps.num_layer_sets_minus1, 0);

  assert_equals_int (vps.timing_info_present_flag, 0);
  assert_equals_int (vps.vps_extension, 0);

  gst_h265_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h265_parse_pps)
{
  GstH265Parser *parser;
  GstH265NalUnit nalu;
  GstH265ParserResult res;
  GstH265PPS pps;

  parser = gst_h265_parser_new ();

  /*init sps[15] to avoid return error in gst_h265_parser_parse_pps */
  parser->sps[15].valid = TRUE;
  parser->sps[15].log2_diff_max_min_luma_coding_block_size = 1;

  res = gst_h265_parser_identify_nalu_unchecked (parser,
      h265_pps_with_range_extension, 0,
      sizeof (h265_pps_with_range_extension), &nalu);

  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_PPS);

  res = gst_h265_parser_parse_pps (parser, &nalu, &pps);

  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (pps.pps_range_extension_flag, 1);
  assert_equals_int (pps.pps_multilayer_extension_flag, 0);
  assert_equals_int (pps.pps_3d_extension_flag, 0);
  assert_equals_int (pps.pps_extension_4bits, 0);
  assert_equals_int (pps.
      pps_extension_params.log2_max_transform_skip_block_size_minus2, 0);
  assert_equals_int (pps.
      pps_extension_params.cross_component_prediction_enabled_flag, 0);
  assert_equals_int (pps.
      pps_extension_params.chroma_qp_offset_list_enabled_flag, 1);
  assert_equals_int (pps.pps_extension_params.diff_cu_chroma_qp_offset_depth,
      1);
  assert_equals_int (pps.pps_extension_params.chroma_qp_offset_list_len_minus1,
      5);
  assert_equals_int (pps.pps_extension_params.log2_sao_offset_scale_luma, 0);
  assert_equals_int (pps.pps_extension_params.log2_sao_offset_scale_chroma, 0);

  gst_h265_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h265_parse_scc)
{
  GstH265Parser *parser;
  GstH265NalUnit nalu;
  GstH265ParserResult res;
  GstH265VPS vps;
  GstH265PPS pps;
  GstH265SPS sps;
  guint offset;
  gsize size;

  parser = gst_h265_parser_new ();

  offset = 0;
  size = sizeof (h265_with_scc_extension);

  res = gst_h265_parser_identify_nalu_unchecked (parser,
      h265_with_scc_extension, offset, size, &nalu);
  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_VPS);
  offset = nalu.offset;

  res = gst_h265_parser_parse_vps (parser, &nalu, &vps);
  assert_equals_int (res, GST_H265_PARSER_OK);

  res = gst_h265_parser_identify_nalu_unchecked (parser,
      h265_with_scc_extension, offset, size, &nalu);
  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_SPS);
  offset = nalu.offset;

  res = gst_h265_parser_parse_sps (parser, &nalu, &sps, FALSE);
  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (sps.profile_tier_level.profile_idc,
      GST_H265_PROFILE_IDC_SCREEN_CONTENT_CODING);
  assert_equals_int (sps.profile_tier_level.profile_compatibility_flag[9], 1);
  assert_equals_int (sps.sps_scc_extension_flag, 1);
  assert_equals_int (sps.sps_extension_4bits, 0);
  assert_equals_int (sps.sps_scc_extension_params.sps_curr_pic_ref_enabled_flag,
      1);
  assert_equals_int (sps.sps_scc_extension_params.palette_mode_enabled_flag, 1);
  assert_equals_int (sps.
      sps_scc_extension_params.delta_palette_max_predictor_size, 65);
  assert_equals_int (sps.
      sps_scc_extension_params.sps_palette_predictor_initializers_present_flag,
      0);
  assert_equals_int (sps.
      sps_scc_extension_params.motion_vector_resolution_control_idc, 2);
  assert_equals_int (sps.
      sps_scc_extension_params.intra_boundary_filtering_disabled_flag, 1);

  res = gst_h265_parser_identify_nalu_unchecked (parser,
      h265_with_scc_extension, offset, size, &nalu);
  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_PPS);

  res = gst_h265_parser_parse_pps (parser, &nalu, &pps);
  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (pps.pps_extension_4bits, 0);
  assert_equals_int (pps.pps_scc_extension_flag, 1);
  assert_equals_int (pps.pps_scc_extension_params.pps_curr_pic_ref_enabled_flag,
      1);
  assert_equals_int (pps.
      pps_scc_extension_params.residual_adaptive_colour_transform_enabled_flag,
      0);
  assert_equals_int (pps.
      pps_scc_extension_params.pps_palette_predictor_initializers_present_flag,
      0);

  gst_h265_parser_free (parser);
}

GST_END_TEST;

typedef struct
{
  GstH265NalUnitType type;
  gboolean is_idr;
  gboolean is_irap;
  gboolean is_bla;
  gboolean is_cra;
  gboolean is_radl;
  gboolean is_rasl;
} H265NalTypeTestVector;

GST_START_TEST (test_h265_nal_type_classification)
{
  gint i;
  /* *INDENT-OFF* */
  H265NalTypeTestVector test_vector[] = {
    /*         NAL-TYPE             IDR    IRAP   BLA    CRA    RADL   RASL */
    {GST_H265_NAL_SLICE_TRAIL_N,    FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
    {GST_H265_NAL_SLICE_TRAIL_R,    FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
    {GST_H265_NAL_SLICE_TSA_N,      FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
    {GST_H265_NAL_SLICE_TSA_R,      FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
    {GST_H265_NAL_SLICE_STSA_N,     FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
    {GST_H265_NAL_SLICE_STSA_R,     FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
    {GST_H265_NAL_SLICE_RADL_N,     FALSE, FALSE, FALSE, FALSE, TRUE,  FALSE},
    {GST_H265_NAL_SLICE_RADL_R,     FALSE, FALSE, FALSE, FALSE, TRUE,  FALSE},
    {GST_H265_NAL_SLICE_RASL_N,     FALSE, FALSE, FALSE, FALSE, FALSE, TRUE },
    {GST_H265_NAL_SLICE_RASL_R,     FALSE, FALSE, FALSE, FALSE, FALSE, TRUE },
    /* 10 ~ 15: reserved non-irap sublayer nal */
    {GST_H265_NAL_SLICE_BLA_W_LP,   FALSE, TRUE,  TRUE,  FALSE, FALSE, FALSE},
    {GST_H265_NAL_SLICE_BLA_W_RADL, FALSE, TRUE,  TRUE,  FALSE, FALSE, FALSE},
    {GST_H265_NAL_SLICE_BLA_N_LP,   FALSE, TRUE,  TRUE,  FALSE, FALSE, FALSE},
    {GST_H265_NAL_SLICE_IDR_W_RADL, TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE},
    {GST_H265_NAL_SLICE_IDR_N_LP,   TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE},
    {GST_H265_NAL_SLICE_CRA_NUT,    FALSE, TRUE,  FALSE, TRUE,  FALSE, FALSE},
    /* 22 ~ 23: reserved irap nal */
    {(GstH265NalUnitType) 22,       FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE},
    {(GstH265NalUnitType) 23,       FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE},
  };
  /* *INDENT-ON* */

  for (i = 0; i < G_N_ELEMENTS (test_vector); i++) {
    assert_equals_int (GST_H265_IS_NAL_TYPE_IDR (test_vector[i].type),
        test_vector[i].is_idr);
    assert_equals_int (GST_H265_IS_NAL_TYPE_IRAP (test_vector[i].type),
        test_vector[i].is_irap);
    assert_equals_int (GST_H265_IS_NAL_TYPE_BLA (test_vector[i].type),
        test_vector[i].is_bla);
    assert_equals_int (GST_H265_IS_NAL_TYPE_CRA (test_vector[i].type),
        test_vector[i].is_cra);
    assert_equals_int (GST_H265_IS_NAL_TYPE_RADL (test_vector[i].type),
        test_vector[i].is_radl);
    assert_equals_int (GST_H265_IS_NAL_TYPE_RASL (test_vector[i].type),
        test_vector[i].is_rasl);
  }

  for (i = RESERVED_NON_IRAP_NAL_TYPE_MIN;
      i <= UNSPECIFIED_NON_VCL_NAL_TYPE_MAX; i++) {
    assert_equals_int (GST_H265_IS_NAL_TYPE_IDR (i), FALSE);
    assert_equals_int (GST_H265_IS_NAL_TYPE_IRAP (i), FALSE);
    assert_equals_int (GST_H265_IS_NAL_TYPE_BLA (i), FALSE);
    assert_equals_int (GST_H265_IS_NAL_TYPE_CRA (i), FALSE);
    assert_equals_int (GST_H265_IS_NAL_TYPE_RADL (i), FALSE);
    assert_equals_int (GST_H265_IS_NAL_TYPE_RASL (i), FALSE);
  }
}

GST_END_TEST;

GST_START_TEST (test_h265_sei_registered_user_data)
{
  GstH265ParserResult res;
  GstH265NalUnit nalu;
  GArray *messages = NULL;
  GstH265SEIMessage *sei;
  GstH265SEIMessage other_sei;
  GstH265RegisteredUserData *user_data;
  GstH265RegisteredUserData *other_user_data;
  GstH265Parser *parser = gst_h265_parser_new ();
  guint payload_size;

  res = gst_h265_parser_identify_nalu_unchecked (parser,
      h265_sei_user_data_registered, 0,
      G_N_ELEMENTS (h265_sei_user_data_registered), &nalu);
  assert_equals_int (res, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_PREFIX_SEI);

  res = gst_h265_parser_parse_sei (parser, &nalu, &messages);
  assert_equals_int (res, GST_H265_PARSER_OK);
  fail_unless (messages != NULL);
  assert_equals_int (messages->len, 1);

  sei = &g_array_index (messages, GstH265SEIMessage, 0);
  assert_equals_int (sei->payloadType, GST_H265_SEI_REGISTERED_USER_DATA);

  user_data = &sei->payload.registered_user_data;
  /* start code prefix 4 bytes
   * nalu header 2 bytes
   * payload type 1 byte
   * payload size 1 byte
   * country code 1 byte (0xb5)
   */
  payload_size = h265_sei_user_data_registered[4 + 2 + 1];

  /* excluding country_code byte */
  assert_equals_int (payload_size - 1, user_data->size);
  fail_if (memcmp (user_data->data,
          &h265_sei_user_data_registered[4 + 2 + 1 + 1 + 1], user_data->size));

  memset (&other_sei, 0, sizeof (GstH265SEIMessage));
  fail_unless (gst_h265_sei_copy (&other_sei, sei));
  assert_equals_int (other_sei.payloadType, GST_H265_SEI_REGISTERED_USER_DATA);

  other_user_data = &other_sei.payload.registered_user_data;
  fail_if (memcmp (user_data->data, other_user_data->data, user_data->size));

  g_array_unref (messages);
  gst_h265_sei_free (&other_sei);
  gst_h265_parser_free (parser);
}

GST_END_TEST;

typedef gboolean (*SEICheckFunc) (gconstpointer a, gconstpointer b);

static gboolean
check_sei_user_data_registered (const GstH265RegisteredUserData * a,
    const GstH265RegisteredUserData * b)
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
check_sei_user_data_unregistered (const GstH265UserDataUnregistered * a,
    const GstH265UserDataUnregistered * b)
{
  return a->size == b->size &&
      !memcmp (a->uuid, b->uuid, sizeof (a->uuid)) &&
      !memcmp (a->data, b->data, a->size);
}

static gboolean
check_sei_time_code (const GstH265TimeCode * a, const GstH265TimeCode * b)
{
  gint i;

  if (a->num_clock_ts != b->num_clock_ts)
    return FALSE;

  for (i = 0; i < a->num_clock_ts; i++) {
    if (a->clock_timestamp_flag[i] != b->clock_timestamp_flag[i])
      return FALSE;

    if (a->clock_timestamp_flag[i]) {
      if ((a->units_field_based_flag[i] != b->units_field_based_flag[i]) ||
          (a->counting_type[i] != b->counting_type[i]) ||
          (a->full_timestamp_flag[i] != b->full_timestamp_flag[i]) ||
          (a->discontinuity_flag[i] != b->discontinuity_flag[i]) ||
          (a->cnt_dropped_flag[i] != b->cnt_dropped_flag[i]) ||
          (a->n_frames[i] != b->n_frames[i])) {
        return FALSE;
      }

      if (a->full_timestamp_flag[i]) {
        if ((a->seconds_value[i] != b->seconds_value[i]) ||
            (a->minutes_value[i] != b->minutes_value[i]) ||
            (a->hours_value[i] != b->hours_value[i])) {
          return FALSE;
        }
      } else {
        if (a->seconds_flag[i] != b->seconds_flag[i])
          return FALSE;

        if (a->seconds_flag[i]) {
          if ((a->seconds_value[i] != b->seconds_value[i]) ||
              (a->minutes_flag[i] != b->minutes_flag[i])) {
            return FALSE;
          }

          if (a->minutes_flag[i]) {
            if ((a->minutes_value[i] != b->minutes_value[i]) ||
                (a->hours_flag[i] != b->hours_flag[i])) {
              return FALSE;
            }

            if (a->hours_flag[i]) {
              if (a->hours_value[i] != b->hours_value[i])
                return FALSE;
            }
          }
        }
      }
    }
  }

  return TRUE;
}

static gboolean
check_sei_mdcv (const GstH265MasteringDisplayColourVolume * a,
    const GstH265MasteringDisplayColourVolume * b)
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
check_sei_cll (const GstH265ContentLightLevel * a,
    const GstH265ContentLightLevel * b)
{
  return (a->max_content_light_level == b->max_content_light_level) &&
      (a->max_pic_average_light_level == b->max_pic_average_light_level);
}

GST_START_TEST (test_h265_create_sei)
{
  GstH265Parser *parser;
  GstH265ParserResult parse_ret;
  GstH265NalUnit nalu;
  GArray *msg_array = NULL;
  GstMemory *mem;
  gint i;
  GstMapInfo info;
  struct
  {
    guint8 *raw_data;
    guint len;
    GstH265SEIPayloadType type;
    GstH265SEIMessage parsed_message;
    SEICheckFunc check_func;
  } test_list[] = {
    /* *INDENT-OFF* */
    {h265_sei_user_data_registered, G_N_ELEMENTS (h265_sei_user_data_registered),
        GST_H265_SEI_REGISTERED_USER_DATA, {0,},
        (SEICheckFunc) check_sei_user_data_registered},
    {h265_sei_user_data_unregistered, G_N_ELEMENTS (h265_sei_user_data_unregistered),
        GST_H265_SEI_USER_DATA_UNREGISTERED, {0,},
        (SEICheckFunc) check_sei_user_data_unregistered},
    {h265_sei_time_code, G_N_ELEMENTS (h265_sei_time_code),
        GST_H265_SEI_TIME_CODE, {0,}, (
        SEICheckFunc) check_sei_time_code},
    {h265_sei_mdcv, G_N_ELEMENTS (h265_sei_mdcv),
        GST_H265_SEI_MASTERING_DISPLAY_COLOUR_VOLUME, {0,},
        (SEICheckFunc) check_sei_mdcv},
    {h265_sei_cll, G_N_ELEMENTS (h265_sei_cll),
        GST_H265_SEI_CONTENT_LIGHT_LEVEL, {0,},
        (SEICheckFunc) check_sei_cll},
    /* *INDENT-ON* */
  };

  parser = gst_h265_parser_new ();

  /* test single sei message per sei nal unit */
  for (i = 0; i < G_N_ELEMENTS (test_list); i++) {
    gsize nal_size;

    parse_ret = gst_h265_parser_identify_nalu_unchecked (parser,
        test_list[i].raw_data, 0, test_list[i].len, &nalu);
    assert_equals_int (parse_ret, GST_H265_PARSER_OK);
    assert_equals_int (nalu.type, GST_H265_NAL_PREFIX_SEI);

    parse_ret = gst_h265_parser_parse_sei (parser, &nalu, &msg_array);
    assert_equals_int (parse_ret, GST_H265_PARSER_OK);
    assert_equals_int (msg_array->len, 1);

    /* test bytestream */
    mem = gst_h265_create_sei_memory (nalu.layer_id,
        nalu.temporal_id_plus1, 4, msg_array);
    fail_unless (mem != NULL);
    fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
    GST_MEMDUMP ("created sei nal", info.data, info.size);
    GST_MEMDUMP ("original sei nal", test_list[i].raw_data, test_list[i].len);
    assert_equals_int (info.size, test_list[i].len);
    fail_if (memcmp (info.data, test_list[i].raw_data, test_list[i].len));
    gst_memory_unmap (mem, &info);
    gst_memory_unref (mem);

    /* test packetized */
    mem = gst_h265_create_sei_memory_hevc (nalu.layer_id,
        nalu.temporal_id_plus1, 4, msg_array);
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
    fail_unless (gst_h265_sei_copy (&test_list[i].parsed_message,
            &g_array_index (msg_array, GstH265SEIMessage, 0)));

    g_array_unref (msg_array);
  }

  /* test multiple SEI messages in a nal unit */
  msg_array = g_array_new (FALSE, FALSE, sizeof (GstH265SEIMessage));
  for (i = 0; i < G_N_ELEMENTS (test_list); i++)
    g_array_append_val (msg_array, test_list[i].parsed_message);

  mem = gst_h265_create_sei_memory (nalu.layer_id,
      nalu.temporal_id_plus1, 4, msg_array);
  fail_unless (mem != NULL);
  g_array_unref (msg_array);

  /* parse sei message from buffer */
  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
  parse_ret = gst_h265_parser_identify_nalu_unchecked (parser,
      info.data, 0, info.size, &nalu);
  assert_equals_int (parse_ret, GST_H265_PARSER_OK);
  assert_equals_int (nalu.type, GST_H265_NAL_PREFIX_SEI);
  parse_ret = gst_h265_parser_parse_sei (parser, &nalu, &msg_array);
  gst_memory_unmap (mem, &info);
  gst_memory_unref (mem);

  assert_equals_int (parse_ret, GST_H265_PARSER_OK);
  assert_equals_int (msg_array->len, G_N_ELEMENTS (test_list));
  for (i = 0; i < msg_array->len; i++) {
    GstH265SEIMessage *msg = &g_array_index (msg_array, GstH265SEIMessage, i);

    assert_equals_int (msg->payloadType, test_list[i].type);
    fail_unless (test_list[i].check_func (&msg->payload,
            &test_list[i].parsed_message.payload));
  }

  /* clean up */
  for (i = 0; i < G_N_ELEMENTS (test_list); i++)
    gst_h265_sei_free (&test_list[i].parsed_message);

  g_array_unref (msg_array);
  gst_h265_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h265_split_hevc)
{
  GstH265Parser *parser;
  GArray *array;
  GstH265NalUnit *nal;
  static const guint8 aud[] = { 0x46, 0x01, 0x10 };
  static const guint8 eos[] = { 0x48, 0x01 };
  static const guint8 sc_3bytes[] = { 0x00, 0x00, 0x01 };
  static const guint8 sc_4bytes[] = { 0x00, 0x00, 0x00, 0x01 };
  const guint8 nal_length_size = 4;
  guint8 data[128];
  gsize size;
  GstH265ParserResult ret;
  gsize consumed;
  guint off;

  parser = gst_h265_parser_new ();
  array = g_array_new (FALSE, FALSE, sizeof (GstH265NalUnit));

#define BUILD_NAL(arr) G_STMT_START { \
  memcpy (data + off, arr, sizeof (arr)); \
  off += sizeof (arr); \
} G_STMT_END

  /* 1) Complete packetized nalu */
  size = nal_length_size + sizeof (aud);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (aud));
  BUILD_NAL (aud);
  ret = gst_h265_parser_identify_and_split_nalu_hevc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H265_PARSER_OK);
  assert_equals_int (array->len, 1);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH265NalUnit, 0);
  assert_equals_int (nal->type, GST_H265_NAL_AUD);
  assert_equals_int (nal->sc_offset, 0);
  assert_equals_int (nal->offset, nal_length_size);
  assert_equals_int (nal->size, sizeof (aud));

  /* 2-1) SC (3 bytes) + nalu */
  size = nal_length_size + sizeof (sc_3bytes) + sizeof (aud);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (sc_3bytes) + sizeof (aud));
  BUILD_NAL (sc_3bytes);
  BUILD_NAL (aud);
  ret = gst_h265_parser_identify_and_split_nalu_hevc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H265_PARSER_OK);
  assert_equals_int (array->len, 1);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH265NalUnit, 0);
  assert_equals_int (nal->type, GST_H265_NAL_AUD);
  assert_equals_int (nal->sc_offset, nal_length_size);
  assert_equals_int (nal->offset, nal_length_size + sizeof (sc_3bytes));
  assert_equals_int (nal->size, sizeof (aud));

  /* 2-2) SC (4 bytes) + nalu */
  size = nal_length_size + sizeof (sc_4bytes) + sizeof (aud);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (sc_4bytes) + sizeof (aud));
  BUILD_NAL (sc_4bytes);
  BUILD_NAL (aud);
  ret = gst_h265_parser_identify_and_split_nalu_hevc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H265_PARSER_OK);
  assert_equals_int (array->len, 1);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH265NalUnit, 0);
  assert_equals_int (nal->type, GST_H265_NAL_AUD);
  assert_equals_int (nal->sc_offset, nal_length_size);
  assert_equals_int (nal->offset, nal_length_size + sizeof (sc_4bytes));
  assert_equals_int (nal->size, sizeof (aud));

  /* 3-1) nalu + trailing SC (3 bytes) */
  size = nal_length_size + sizeof (aud) + sizeof (sc_3bytes);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (aud) + sizeof (sc_3bytes));
  BUILD_NAL (aud);
  BUILD_NAL (sc_3bytes);
  ret = gst_h265_parser_identify_and_split_nalu_hevc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H265_PARSER_OK);
  assert_equals_int (array->len, 1);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH265NalUnit, 0);
  assert_equals_int (nal->type, GST_H265_NAL_AUD);
  assert_equals_int (nal->sc_offset, 0);
  assert_equals_int (nal->offset, nal_length_size);
  assert_equals_int (nal->size, sizeof (aud));

  /* 3-2) nalu + trailing SC (4 bytes) */
  size = nal_length_size + sizeof (aud) + sizeof (sc_4bytes);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (aud) + sizeof (sc_4bytes));
  BUILD_NAL (aud);
  BUILD_NAL (sc_4bytes);
  ret = gst_h265_parser_identify_and_split_nalu_hevc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H265_PARSER_OK);
  assert_equals_int (array->len, 1);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH265NalUnit, 0);
  assert_equals_int (nal->type, GST_H265_NAL_AUD);
  assert_equals_int (nal->sc_offset, 0);
  assert_equals_int (nal->offset, nal_length_size);
  assert_equals_int (nal->size, sizeof (aud));

  /* 4-1) SC + nalu + SC + nalu */
  size = nal_length_size + sizeof (sc_3bytes) + sizeof (aud) +
      sizeof (sc_4bytes) + sizeof (eos);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (sc_3bytes) + sizeof (aud) +
      sizeof (sc_4bytes) + sizeof (eos));
  BUILD_NAL (sc_3bytes);
  BUILD_NAL (aud);
  BUILD_NAL (sc_4bytes);
  BUILD_NAL (eos);
  ret = gst_h265_parser_identify_and_split_nalu_hevc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H265_PARSER_OK);
  assert_equals_int (array->len, 2);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH265NalUnit, 0);
  assert_equals_int (nal->type, GST_H265_NAL_AUD);
  assert_equals_int (nal->sc_offset, nal_length_size);
  assert_equals_int (nal->offset, nal_length_size + sizeof (sc_3bytes));
  assert_equals_int (nal->size, sizeof (aud));
  nal = &g_array_index (array, GstH265NalUnit, 1);
  assert_equals_int (nal->type, GST_H265_NAL_EOS);
  assert_equals_int (nal->sc_offset, nal_length_size + sizeof (sc_3bytes)
      + sizeof (aud));
  assert_equals_int (nal->offset, nal_length_size + sizeof (sc_3bytes)
      + sizeof (aud) + sizeof (sc_4bytes));
  assert_equals_int (nal->size, sizeof (eos));

  /* 4-2) SC + nalu + SC + nalu + trailing SC */
  size = nal_length_size + sizeof (sc_3bytes) + sizeof (aud) +
      sizeof (sc_4bytes) + sizeof (eos) + sizeof (sc_3bytes);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (sc_3bytes) + sizeof (aud) +
      sizeof (sc_4bytes) + sizeof (eos) + sizeof (sc_3bytes));
  BUILD_NAL (sc_3bytes);
  BUILD_NAL (aud);
  BUILD_NAL (sc_4bytes);
  BUILD_NAL (eos);
  BUILD_NAL (sc_3bytes);
  ret = gst_h265_parser_identify_and_split_nalu_hevc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H265_PARSER_OK);
  assert_equals_int (array->len, 2);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH265NalUnit, 0);
  assert_equals_int (nal->type, GST_H265_NAL_AUD);
  assert_equals_int (nal->sc_offset, nal_length_size);
  assert_equals_int (nal->offset, nal_length_size + sizeof (sc_3bytes));
  assert_equals_int (nal->size, sizeof (aud));
  nal = &g_array_index (array, GstH265NalUnit, 1);
  assert_equals_int (nal->type, GST_H265_NAL_EOS);
  assert_equals_int (nal->sc_offset, nal_length_size + sizeof (sc_3bytes)
      + sizeof (aud));
  assert_equals_int (nal->offset, nal_length_size + sizeof (sc_3bytes)
      + sizeof (aud) + sizeof (sc_4bytes));
  assert_equals_int (nal->size, sizeof (eos));

  /* 4-3) nalu + SC + nalu */
  size = nal_length_size + sizeof (aud) + sizeof (sc_4bytes) + sizeof (eos);
  off = nal_length_size;
  GST_WRITE_UINT32_BE (data, sizeof (aud) + sizeof (sc_4bytes) + sizeof (eos));
  BUILD_NAL (aud);
  BUILD_NAL (sc_4bytes);
  BUILD_NAL (eos);
  ret = gst_h265_parser_identify_and_split_nalu_hevc (parser, data,
      0, size, nal_length_size, array, &consumed);
  assert_equals_int (ret, GST_H265_PARSER_OK);
  assert_equals_int (array->len, 2);
  assert_equals_int (consumed, size);
  nal = &g_array_index (array, GstH265NalUnit, 0);
  assert_equals_int (nal->type, GST_H265_NAL_AUD);
  assert_equals_int (nal->sc_offset, 0);
  assert_equals_int (nal->offset, nal_length_size);
  assert_equals_int (nal->size, sizeof (aud));
  nal = &g_array_index (array, GstH265NalUnit, 1);
  assert_equals_int (nal->type, GST_H265_NAL_EOS);
  assert_equals_int (nal->sc_offset, nal_length_size + sizeof (aud));
  assert_equals_int (nal->offset,
      nal_length_size + sizeof (aud) + sizeof (sc_4bytes));
  assert_equals_int (nal->size, sizeof (eos));

#undef BUILD_NAL

  gst_h265_parser_free (parser);
  g_array_unref (array);
}

GST_END_TEST;

/* Captured from Apple's HLS test stream
 * http://devstreaming-cdn.apple.com/videos/streaming/examples/bipbop_adv_example_hevc/v14/prog_index.m3u8
 */
static const guint8 h265_codec_data[] = {
  0x01, 0x02, 0x00, 0x00, 0x00, 0x04, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x7b, 0xf0, 0x00, 0xfc, 0xfd, 0xfa, 0xfa, 0x00, 0x00, 0x0f, 0x03, 0xa0,
  0x00, 0x01, 0x00, 0x18, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x02, 0x20,
  0x00, 0x00, 0x03, 0x00, 0xb0, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
  0x7b, 0x18, 0xb0, 0x24, 0xa1, 0x00, 0x01, 0x00, 0x3c, 0x42, 0x01, 0x01,
  0x02, 0x20, 0x00, 0x00, 0x03, 0x00, 0xb0, 0x00, 0x00, 0x03, 0x00, 0x00,
  0x03, 0x00, 0x7b, 0xa0, 0x07, 0x82, 0x00, 0x88, 0x7d, 0xb6, 0x71, 0x8b,
  0x92, 0x44, 0x80, 0x53, 0x88, 0x88, 0x92, 0xcf, 0x24, 0xa6, 0x92, 0x72,
  0xc9, 0x12, 0x49, 0x22, 0xdc, 0x91, 0xaa, 0x48, 0xfc, 0xa2, 0x23, 0xff,
  0x00, 0x01, 0x00, 0x01, 0x6a, 0x02, 0x02, 0x02, 0x01, 0xa2, 0x00, 0x01,
  0x00, 0x08, 0x44, 0x01, 0xc0, 0x25, 0x2f, 0x05, 0x32, 0x40
};

GST_START_TEST (test_h265_decoder_config_record)
{
  GstH265Parser *parser;
  GstH265ParserResult ret;
  GstH265DecoderConfigRecord *config = NULL;
  GstH265VPS vps;
  GstH265SPS sps;
  GstH265PPS pps;
  GstH265DecoderConfigRecordNalUnitArray *nalu_array;
  GstH265NalUnit *nalu;

  parser = gst_h265_parser_new ();

  ret = gst_h265_parser_parse_decoder_config_record (parser,
      h265_codec_data, sizeof (h265_codec_data), &config);
  assert_equals_int (ret, GST_H265_PARSER_OK);
  fail_unless (config != NULL);

  assert_equals_int (config->length_size_minus_one, 3);
  fail_unless (config->nalu_array != NULL);
  assert_equals_int (config->nalu_array->len, 3);

  /* VPS */
  nalu_array = &g_array_index (config->nalu_array,
      GstH265DecoderConfigRecordNalUnitArray, 0);
  fail_unless (nalu_array->nalu != NULL);
  assert_equals_int (nalu_array->nalu->len, 1);

  nalu = &g_array_index (nalu_array->nalu, GstH265NalUnit, 0);
  assert_equals_int (nalu->type, GST_H265_NAL_VPS);
  ret = gst_h265_parser_parse_vps (parser, nalu, &vps);
  assert_equals_int (ret, GST_H265_PARSER_OK);

  /* SPS */
  nalu_array = &g_array_index (config->nalu_array,
      GstH265DecoderConfigRecordNalUnitArray, 1);
  fail_unless (nalu_array->nalu != NULL);
  assert_equals_int (nalu_array->nalu->len, 1);

  nalu = &g_array_index (nalu_array->nalu, GstH265NalUnit, 0);
  assert_equals_int (nalu->type, GST_H265_NAL_SPS);
  ret = gst_h265_parser_parse_sps (parser, nalu, &sps, TRUE);
  assert_equals_int (ret, GST_H265_PARSER_OK);

  /* PPS */
  nalu_array = &g_array_index (config->nalu_array,
      GstH265DecoderConfigRecordNalUnitArray, 2);
  fail_unless (nalu_array->nalu != NULL);
  assert_equals_int (nalu_array->nalu->len, 1);

  nalu = &g_array_index (nalu_array->nalu, GstH265NalUnit, 0);
  assert_equals_int (nalu->type, GST_H265_NAL_PPS);
  ret = gst_h265_parser_parse_pps (parser, nalu, &pps);
  assert_equals_int (ret, GST_H265_PARSER_OK);

  gst_h265_decoder_config_record_free (config);
  gst_h265_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (test_h265_parse_partial_nal)
{
  GstH265ParserResult res;
  GstH265NalUnit nalu;
  GstH265Parser *parser = gst_h265_parser_new ();
  const guint8 *buf = slice_eos_slice_eob;
  const guint buf_size = 5;

  res = gst_h265_parser_identify_nalu (parser, buf, 0, buf_size, &nalu);

  /* H.265 parser is a bit different then H.264 one, and will return
   * NO_NAL if there is a start code but not enough bytes to hold the
   * header. */
  assert_equals_int (res, GST_H265_PARSER_NO_NAL);

  gst_h265_parser_free (parser);
}

GST_END_TEST;

static Suite *
h265parser_suite (void)
{
  Suite *s = suite_create ("H265 Parser library");

  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_h265_parse_slice_eos_slice_eob);
  tcase_add_test (tc_chain, test_h265_parse_pic_timing);
  tcase_add_test (tc_chain, test_h265_parse_slice_6bytes);
  tcase_add_test (tc_chain, test_h265_parse_identify_nalu_hevc);
  tcase_add_test (tc_chain, test_h265_base_profiles);
  tcase_add_test (tc_chain, test_h265_base_profiles_compat);
  tcase_add_test (tc_chain, test_h265_format_range_profiles_exact_match);
  tcase_add_test (tc_chain, test_h265_format_range_profiles_partial_match);
  tcase_add_test (tc_chain, test_h265_parse_vps);
  tcase_add_test (tc_chain, test_h265_parse_pps);
  tcase_add_test (tc_chain, test_h265_parse_scc);
  tcase_add_test (tc_chain, test_h265_nal_type_classification);
  tcase_add_test (tc_chain, test_h265_sei_registered_user_data);
  tcase_add_test (tc_chain, test_h265_create_sei);
  tcase_add_test (tc_chain, test_h265_split_hevc);
  tcase_add_test (tc_chain, test_h265_decoder_config_record);
  tcase_add_test (tc_chain, test_h265_parse_partial_nal);

  return s;
}

GST_CHECK_MAIN (h265parser);
