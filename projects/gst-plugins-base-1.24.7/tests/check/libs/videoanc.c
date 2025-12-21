/* GStreamer
 *
 * Copyright (C) 2018 Sebastian Dröge <sebastian@centricular.com>
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
#include <gst/gst.h>
#include <gst/check/gstcheck.h>
#include <gst/video/video.h>

GST_START_TEST (parse_8bit)
{
  GstVideoVBIParser *parser;
  guint8 line[2560] = { 0, };
  GstVideoAncillary vanc;

  parser = gst_video_vbi_parser_new (GST_VIDEO_FORMAT_UYVY, 1280);
  fail_unless (parser != NULL);

  /* empty line */
  gst_video_vbi_parser_add_line (parser, line);
  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_DONE);

  /* add a single ADF in the chroma with some arbitrary DID/SDID and 8 bytes of data */
  line[16] = 0x00;
  line[18] = 0xff;
  line[20] = 0xff;
  line[22] = 0x23;              /* DID */
  line[24] = 0x24;              /* SDID */
  line[26] = 0x08;              /* DC */
  line[28] = 0x01;
  line[30] = 0x02;
  line[32] = 0x03;
  line[34] = 0x04;
  line[36] = 0x50;
  line[38] = 0x60;
  line[40] = 0x70;
  line[42] = 0x80;
  line[44] = 0xf9;              /* checksum */

  gst_video_vbi_parser_add_line (parser, line);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_OK);
  fail_unless_equals_int (GST_VIDEO_ANCILLARY_DID16 (&vanc), 0x2324);
  fail_unless_equals_int (vanc.data_count, 8);
  fail_unless_equals_int (vanc.data[0], 0x01);
  fail_unless_equals_int (vanc.data[1], 0x02);
  fail_unless_equals_int (vanc.data[2], 0x03);
  fail_unless_equals_int (vanc.data[3], 0x04);
  fail_unless_equals_int (vanc.data[4], 0x50);
  fail_unless_equals_int (vanc.data[5], 0x60);
  fail_unless_equals_int (vanc.data[6], 0x70);
  fail_unless_equals_int (vanc.data[7], 0x80);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_DONE);

  /* add a second ADF in the luma with 4 bytes data count */
  line[17] = 0x00;
  line[19] = 0xff;
  line[21] = 0xff;
  line[23] = 0x33;              /* DID */
  line[25] = 0x34;              /* SDID */
  line[27] = 0x04;              /* DC */
  line[29] = 0x04;
  line[31] = 0x03;
  line[33] = 0x02;
  line[35] = 0x01;
  line[37] = 0x75;              /* checksum */

  gst_video_vbi_parser_add_line (parser, line);

  /* first we should get the luma data */
  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_OK);
  fail_unless_equals_int (GST_VIDEO_ANCILLARY_DID16 (&vanc), 0x3334);
  fail_unless_equals_int (vanc.data_count, 4);
  fail_unless_equals_int (vanc.data[0], 0x04);
  fail_unless_equals_int (vanc.data[1], 0x03);
  fail_unless_equals_int (vanc.data[2], 0x02);
  fail_unless_equals_int (vanc.data[3], 0x01);

  /* then the chroma data */
  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_OK);
  fail_unless_equals_int (GST_VIDEO_ANCILLARY_DID16 (&vanc), 0x2324);
  fail_unless_equals_int (vanc.data_count, 8);
  fail_unless_equals_int (vanc.data[0], 0x01);
  fail_unless_equals_int (vanc.data[1], 0x02);
  fail_unless_equals_int (vanc.data[2], 0x03);
  fail_unless_equals_int (vanc.data[3], 0x04);
  fail_unless_equals_int (vanc.data[4], 0x50);
  fail_unless_equals_int (vanc.data[5], 0x60);
  fail_unless_equals_int (vanc.data[6], 0x70);
  fail_unless_equals_int (vanc.data[7], 0x80);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_DONE);

  gst_video_vbi_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (parse_10bit)
{
  GstVideoVBIParser *parser;
  guint8 line[3414] = { 0, };
  GstVideoAncillary vanc;

  parser = gst_video_vbi_parser_new (GST_VIDEO_FORMAT_v210, 1280);
  fail_unless (parser != NULL);

  /* empty line */
  gst_video_vbi_parser_add_line (parser, line);
  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_DONE);

  /* add a single ADF in the chroma with some arbitrary DID/SDID and 8 bytes
   * of data. See above for explanation */
  GST_WRITE_UINT32_LE (line + 16, (0x000 << 0) | (0x3ff << 20));
  GST_WRITE_UINT32_LE (line + 20, (0x3ff << 10));
  GST_WRITE_UINT32_LE (line + 24, (0x123 << 0) | (0x224 << 20));
  GST_WRITE_UINT32_LE (line + 28, (0x108 << 10));

  GST_WRITE_UINT32_LE (line + 32, (0x101 << 0) | (0x102 << 20));
  GST_WRITE_UINT32_LE (line + 36, (0x203 << 10));
  GST_WRITE_UINT32_LE (line + 40, (0x104 << 0) | (0x250 << 20));
  GST_WRITE_UINT32_LE (line + 44, (0x260 << 10));

  GST_WRITE_UINT32_LE (line + 48, (0x170 << 0) | (0x180 << 20));
  GST_WRITE_UINT32_LE (line + 52, (0x2f9 << 10));

  gst_video_vbi_parser_add_line (parser, line);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_OK);
  fail_unless_equals_int (GST_VIDEO_ANCILLARY_DID16 (&vanc), 0x2324);
  fail_unless_equals_int (vanc.data_count, 8);
  fail_unless_equals_int (vanc.data[0], 0x01);
  fail_unless_equals_int (vanc.data[1], 0x02);
  fail_unless_equals_int (vanc.data[2], 0x03);
  fail_unless_equals_int (vanc.data[3], 0x04);
  fail_unless_equals_int (vanc.data[4], 0x50);
  fail_unless_equals_int (vanc.data[5], 0x60);
  fail_unless_equals_int (vanc.data[6], 0x70);
  fail_unless_equals_int (vanc.data[7], 0x80);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_DONE);

  /* add a second ADF in the luma with 4 bytes data count */
  GST_WRITE_UINT32_LE (line + 16, (0x000 << 0) | (0x3ff << 20) | (0x000 << 10));
  GST_WRITE_UINT32_LE (line + 20, (0x3ff << 10) | (0x3ff << 0) | (0x3ff << 20));
  GST_WRITE_UINT32_LE (line + 24, (0x123 << 0) | (0x224 << 20) | (0x233 << 10));
  GST_WRITE_UINT32_LE (line + 28, (0x108 << 10) | (0x134 << 0) | (0x204 << 20));

  GST_WRITE_UINT32_LE (line + 32, (0x101 << 0) | (0x102 << 20) | (0x104 << 10));
  GST_WRITE_UINT32_LE (line + 36, (0x203 << 10) | (0x203 << 0) | (0x102 << 20));
  GST_WRITE_UINT32_LE (line + 40, (0x104 << 0) | (0x250 << 20) | (0x101 << 10));
  GST_WRITE_UINT32_LE (line + 44, (0x275 << 0) | (0x260 << 10));

  GST_WRITE_UINT32_LE (line + 48, (0x170 << 0) | (0x180 << 20));
  GST_WRITE_UINT32_LE (line + 52, (0x2f9 << 10));

  gst_video_vbi_parser_add_line (parser, line);

  /* first we should get the luma data */
  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_OK);
  fail_unless_equals_int (GST_VIDEO_ANCILLARY_DID16 (&vanc), 0x3334);
  fail_unless_equals_int (vanc.data_count, 4);
  fail_unless_equals_int (vanc.data[0], 0x04);
  fail_unless_equals_int (vanc.data[1], 0x03);
  fail_unless_equals_int (vanc.data[2], 0x02);
  fail_unless_equals_int (vanc.data[3], 0x01);

  /* then the chroma data */
  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_OK);
  fail_unless_equals_int (GST_VIDEO_ANCILLARY_DID16 (&vanc), 0x2324);
  fail_unless_equals_int (vanc.data_count, 8);
  fail_unless_equals_int (vanc.data[0], 0x01);
  fail_unless_equals_int (vanc.data[1], 0x02);
  fail_unless_equals_int (vanc.data[2], 0x03);
  fail_unless_equals_int (vanc.data[3], 0x04);
  fail_unless_equals_int (vanc.data[4], 0x50);
  fail_unless_equals_int (vanc.data[5], 0x60);
  fail_unless_equals_int (vanc.data[6], 0x70);
  fail_unless_equals_int (vanc.data[7], 0x80);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_DONE);

  gst_video_vbi_parser_free (parser);
}

GST_END_TEST;

GST_START_TEST (encode_8bit)
{
  GstVideoVBIParser *parser;
  GstVideoVBIEncoder *encoder;
  guint8 line[2560] = { 0, };
  const guint8 data1[] = { 0x01, 0x02, 0x03, 0x04, 0x50, 0x60, 0x70, 0x80 };
  const guint8 data2[] = { 0x04, 0x03, 0x02, 0x01 };
  GstVideoAncillary vanc;

  parser = gst_video_vbi_parser_new (GST_VIDEO_FORMAT_UYVY, 1280);
  fail_unless (parser != NULL);

  encoder = gst_video_vbi_encoder_new (GST_VIDEO_FORMAT_UYVY, 1280);
  fail_unless (encoder != NULL);

  /* Write a single ADF packet and try to parse it back again */
  fail_unless (gst_video_vbi_encoder_add_ancillary (encoder, FALSE, 0x23, 0x24,
          data1, sizeof (data1)));
  gst_video_vbi_encoder_write_line (encoder, line);

  gst_video_vbi_parser_add_line (parser, line);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_OK);
  fail_unless_equals_int (GST_VIDEO_ANCILLARY_DID16 (&vanc), 0x2324);
  fail_unless_equals_int (vanc.data_count, 8);
  fail_unless (memcmp (vanc.data, data1, sizeof (data1)) == 0);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_DONE);

  /* Write two ADF packets now */
  fail_unless (gst_video_vbi_encoder_add_ancillary (encoder, FALSE, 0x23, 0x24,
          data1, sizeof (data1)));
  fail_unless (gst_video_vbi_encoder_add_ancillary (encoder, FALSE, 0x33, 0x34,
          data2, sizeof (data2)));
  gst_video_vbi_encoder_write_line (encoder, line);

  gst_video_vbi_parser_add_line (parser, line);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_OK);
  fail_unless_equals_int (GST_VIDEO_ANCILLARY_DID16 (&vanc), 0x2324);
  fail_unless_equals_int (vanc.data_count, 8);
  fail_unless (memcmp (vanc.data, data1, sizeof (data1)) == 0);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_OK);
  fail_unless_equals_int (GST_VIDEO_ANCILLARY_DID16 (&vanc), 0x3334);
  fail_unless_equals_int (vanc.data_count, 4);
  fail_unless (memcmp (vanc.data, data2, sizeof (data2)) == 0);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_DONE);

  gst_video_vbi_encoder_free (encoder);
  gst_video_vbi_parser_free (parser);
}

GST_END_TEST;


GST_START_TEST (encode_10bit)
{
  GstVideoVBIParser *parser;
  GstVideoVBIEncoder *encoder;
  guint8 line[3414] = { 0, };
  const guint8 data1[] = { 0x01, 0x02, 0x03, 0x04, 0x50, 0x60, 0x70, 0x80 };
  const guint8 data2[] = { 0x04, 0x03, 0x02, 0x01 };
  GstVideoAncillary vanc;

  parser = gst_video_vbi_parser_new (GST_VIDEO_FORMAT_v210, 1280);
  fail_unless (parser != NULL);

  encoder = gst_video_vbi_encoder_new (GST_VIDEO_FORMAT_v210, 1280);
  fail_unless (encoder != NULL);

  /* Write a single ADF packet and try to parse it back again */
  fail_unless (gst_video_vbi_encoder_add_ancillary (encoder, FALSE, 0x23, 0x24,
          data1, sizeof (data1)));
  gst_video_vbi_encoder_write_line (encoder, line);

  gst_video_vbi_parser_add_line (parser, line);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_OK);
  fail_unless_equals_int (GST_VIDEO_ANCILLARY_DID16 (&vanc), 0x2324);
  fail_unless_equals_int (vanc.data_count, 8);
  fail_unless (memcmp (vanc.data, data1, sizeof (data1)) == 0);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_DONE);

  /* Write two ADF packets now */
  fail_unless (gst_video_vbi_encoder_add_ancillary (encoder, FALSE, 0x23, 0x24,
          data1, sizeof (data1)));
  fail_unless (gst_video_vbi_encoder_add_ancillary (encoder, FALSE, 0x33, 0x34,
          data2, sizeof (data2)));
  gst_video_vbi_encoder_write_line (encoder, line);

  gst_video_vbi_parser_add_line (parser, line);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_OK);
  fail_unless_equals_int (GST_VIDEO_ANCILLARY_DID16 (&vanc), 0x2324);
  fail_unless_equals_int (vanc.data_count, 8);
  fail_unless (memcmp (vanc.data, data1, sizeof (data1)) == 0);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_OK);
  fail_unless_equals_int (GST_VIDEO_ANCILLARY_DID16 (&vanc), 0x3334);
  fail_unless_equals_int (vanc.data_count, 4);
  fail_unless (memcmp (vanc.data, data2, sizeof (data2)) == 0);

  fail_unless_equals_int (gst_video_vbi_parser_get_ancillary (parser, &vanc),
      GST_VIDEO_VBI_PARSER_RESULT_DONE);

  gst_video_vbi_encoder_free (encoder);
  gst_video_vbi_parser_free (parser);
}

GST_END_TEST;

static Suite *
gst_videoanc_suite (void)
{
  Suite *s = suite_create ("GstVideoAnc");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);

  tcase_add_test (tc, parse_8bit);
  tcase_add_test (tc, parse_10bit);

  tcase_add_test (tc, encode_8bit);
  tcase_add_test (tc, encode_10bit);

  return s;
}

GST_CHECK_MAIN (gst_videoanc);
