/* GStreamer
 *
 * unit test for jpegparse
 *
 * Copyright (C) <2009> Arnout Vandecappelle (Essensium/Mind) <arnout@mind.be>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <unistd.h>

#include <gst/check/gstcheck.h>

/* This test doesn't use actual JPEG data, but some fake data that we know
   will trigger certain paths in jpegparse. */

static guint8 test_data_garbage[] = { 0x00, 0x01, 0xff, 0x32, 0x00, 0xff };
static guint8 test_data_short_frame[] = { 0xff, 0xd8, 0xff, 0xd9 };

static guint8 test_data_normal_frame[] = { 0xff, 0xd8, 0xff, 0x12, 0x00, 0x03,
  0x33, 0xff, 0xd9
};

static guint8 test_data_entropy[] = { 0xff, 0xd8, 0xff, 0xda, 0x00, 0x04, 0x22,
  0x33, 0x44, 0xff, 0x00, 0x55, 0xff, 0x04, 0x00, 0x04, 0x22, 0x33, 0xff, 0xd9
};
static guint8 test_data_ff[] = { 0xff, 0xff };

static guint8 test_data_extra_ff[] = { 0xff, 0xd8, 0xff, 0xff, 0xff, 0x12, 0x00,
  0x03, 0x33, 0xff, 0xff, 0xff, 0xd9
};

static guint8 test_data_soi[] = { 0xff, 0xd8 };

static guint8 test_data_app1_exif[] = {
  0xff, 0xe1,
  0x00, 0xd2,                   /* length = 210 */
  0x45, 0x78, 0x69, 0x66, 0x00, /* Exif */
  0x00,
  0x49, 0x49,
  0x2a, 0x00,
  0x08,
  0x00, 0x00, 0x00,
  0x09,                         /* number of entries */
  0x00,
  0x0e, 0x01,                   /* tag 0x10e */
  0x02, 0x00,                   /* type 2 */
  0x0b, 0x00,                   /* count 11 */
  0x00, 0x00,
  0x7a,                         /* offset 122 (0x7a) */
  0x00, 0x00, 0x00,
  0x0f, 0x01,                   /* tag 0x10f */
  0x02, 0x00,                   /* type 2 */
  0x06, 0x00,                   /* count 6 */
  0x00, 0x00,
  0x85,                         /* offset 133 (0x85) */
  0x00, 0x00, 0x00,
  0x10, 0x01,                   /* tag 0x110 */
  0x02, 0x00,                   /* type 2 */
  0x05, 0x00,                   /* count 5 */
  0x00, 0x00,
  0x8b,                         /* offset 139 (0x8b) */
  0x00, 0x00, 0x00,
  0x12, 0x01,                   /* tag 0x112 */
  0x03, 0x00,                   /* type 3 */
  0x01, 0x00,                   /* count 1 */
  0x00, 0x00,
  0x01, 0x00, 0x30, 0x2c,       /* offset (0x2c300001) */
  0x1a, 0x01,                   /* tag 0x11a */
  0x05, 0x00,                   /* type 5 */
  0x01, 0x00,                   /* count 1 */
  0x00, 0x00,
  0x90,                         /* offset 144 (0x90) */
  0x00, 0x00, 0x00,
  0x1b, 0x01,                   /* tag 0x11b */
  0x05, 0x00,                   /* type 5 */
  0x01, 0x00,                   /* count 1 */
  0x00, 0x00,
  0x98,                         /* offset 152 (0x98) */
  0x00, 0x00, 0x00,
  0x28, 0x01,                   /* tag 0x128 */
  0x03, 0x00,                   /* type 3 */
  0x01, 0x00,                   /* count 1 */
  0x00, 0x00,
  0x02, 0x00, 0x31, 0x2f,       /* offset (0x2f310002) */
  0x31, 0x01,                   /* tag 0x131 */
  0x02, 0x00,                   /* type 2 */
  0x08, 0x00,                   /* count 8 */
  0x00, 0x00,
  0xa0,                         /* offset 160 (0xa0) */
  0x00, 0x00, 0x00,
  0x32, 0x01,                   /* tag 0x132 */
  0x02, 0x00,                   /* type 2 */
  0x14, 0x00,                   /* count 20 */
  0x00, 0x00,
  0xa8,                         /* offset 168 (0xa8)  */
  0x00, 0x00, 0x00,
  0x00, 0x00, 0x00,
  0x00,
  /* string */
  /* 122: */ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00,
  /* string (NIKON) */
  /* 133: */ 0x4e, 0x49, 0x4b, 0x4f, 0x4e, 0x00,
  /* string (E800) */
  /* 139: */ 0x45, 0x38, 0x30, 0x30, 0x00,
  /* 144: */ 0x00, 0x00, 0x80, 0x25, /* / */ 0x00, 0x00, 0x20, 0x00,
  /* 152: */ 0x00, 0x00, 0x80, 0x25, /* / */ 0x00, 0x00, 0x20, 0x00,
  /* string (v984-75) */
  /* 160: */ 0x76, 0x39, 0x38, 0x34, 0x2d, 0x37, 0x35, 0x00,
  /* string (2001:08:18 21:44:21) */
  /* 168: */ 0x32, 0x30, 0x30, 0x31, 0x3a, 0x30, 0x38, 0x3a,
  0x31, 0x38, 0x20, 0x32, 0x31, 0x3a, 0x34, 0x34,
  0x3a, 0x32, 0x31, 0x00,

  0x1e, 0x21, 0x1f, 0x1e, 0x21, 0x1c, 0x20, 0x21, 0x22, 0x24, 0x24, 0x27,
  0x22, 0x20,
};

static guint8 test_data_comment[] = {
  0xff, 0xfe,
  0x00, 0x08,                   /* size */
  /* xxxxx */
  0x78, 0x78, 0x78, 0x78, 0x78, 0x00,
};

static guint8 test_data_sof0[] = {
  0xff, 0xc0,                   /* baseline dct-based */
  0x00, 0x11,                   /* size */
  0x08,                         /* precision */
  0x00, 0x3c,                   /* width */
  0x00, 0x50,                   /* height */
  0x03,                         /* number of components */
  0x01, 0x22, 0x00,             /* component 1 */
  0x02, 0x11, 0x01,             /* component 2 */
  0x03, 0x11, 0x01,             /* component 3 */
};

static guint8 test_data_eoi[] = { 0xff, 0xd9 };

static GList *
_make_buffers_in (GList * buffer_in, guint8 * test_data, gsize test_data_size)
{
  GstBuffer *buffer;
  gsize i;

  for (i = 0; i < test_data_size; i++) {
    buffer =
        gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, test_data + i, 1,
        0, 1, NULL, NULL);
    buffer_in = g_list_append (buffer_in, buffer);
  }
  return buffer_in;
}

#define make_buffers_in(buffer_in, test_data) \
    _make_buffers_in(buffer_in, test_data, sizeof(test_data))

static GList *
_make_buffers_out (GList * buffer_out, guint8 * test_data, gsize test_data_size)
{
  GstBuffer *buffer;

  buffer =
      gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, test_data,
      test_data_size, 0, test_data_size, NULL, NULL);
  buffer_out = g_list_append (buffer_out, buffer);

  return buffer_out;
}

#define make_buffers_out(buffer_out, test_data) \
    _make_buffers_out(buffer_out, test_data, sizeof(test_data))

GST_START_TEST (test_parse_single_byte)
{
  GList *buffer_in = NULL, *buffer_out = NULL;
  GstCaps *caps_in, *caps_out;

  caps_in = gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, FALSE,
      NULL);
  caps_out = gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, TRUE,
      "framerate", GST_TYPE_FRACTION, 0, 1, "interlace-mode", G_TYPE_STRING,
      "progressive", "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, NULL);

  /* Push the data byte by byte, injecting some garbage. */
  buffer_in = make_buffers_in (buffer_in, test_data_garbage);
  buffer_in = make_buffers_in (buffer_in, test_data_short_frame);
  buffer_in = make_buffers_in (buffer_in, test_data_garbage);
  buffer_in = make_buffers_in (buffer_in, test_data_normal_frame);
  buffer_in = make_buffers_in (buffer_in, test_data_ff);
  buffer_in = make_buffers_in (buffer_in, test_data_entropy);
  buffer_in = make_buffers_in (buffer_in, test_data_extra_ff);

  buffer_out = make_buffers_out (buffer_out, test_data_short_frame);
  buffer_out = make_buffers_out (buffer_out, test_data_normal_frame);
  buffer_out = make_buffers_out (buffer_out, test_data_entropy);
  buffer_out = make_buffers_out (buffer_out, test_data_extra_ff);
  gst_check_element_push_buffer_list ("jpegparse", buffer_in, caps_in,
      buffer_out, caps_out, GST_FLOW_OK);

  gst_caps_unref (caps_in);
  gst_caps_unref (caps_out);
}

GST_END_TEST;



GST_START_TEST (test_parse_all_in_one_buf)
{
  GList *buffer_in = NULL, *buffer_out = NULL;
  GstBuffer *buffer = NULL;
  gsize total_size = 0;
  gsize offset = 0;
  GstCaps *caps_in, *caps_out;

  /* Push the data in a single buffer, injecting some garbage. */
  total_size += sizeof (test_data_garbage);
  total_size += sizeof (test_data_short_frame);
  total_size += sizeof (test_data_garbage);
  total_size += sizeof (test_data_normal_frame);
  total_size += sizeof (test_data_ff);
  total_size += sizeof (test_data_entropy);
  total_size += sizeof (test_data_extra_ff);
  buffer = gst_buffer_new_and_alloc (total_size);
  gst_buffer_fill (buffer, offset, test_data_garbage,
      sizeof (test_data_garbage));
  offset += sizeof (test_data_garbage);
  gst_buffer_fill (buffer, offset, test_data_short_frame,
      sizeof (test_data_short_frame));
  offset += sizeof (test_data_short_frame);
  gst_buffer_fill (buffer, offset, test_data_garbage,
      sizeof (test_data_garbage));
  offset += sizeof (test_data_garbage);
  gst_buffer_fill (buffer, offset, test_data_normal_frame,
      sizeof (test_data_normal_frame));
  offset += sizeof (test_data_normal_frame);
  gst_buffer_fill (buffer, offset, test_data_ff, sizeof (test_data_ff));
  offset += sizeof (test_data_ff);
  gst_buffer_fill (buffer, offset, test_data_entropy,
      sizeof (test_data_entropy));
  offset += sizeof (test_data_entropy);
  gst_buffer_fill (buffer, offset, test_data_extra_ff,
      sizeof (test_data_extra_ff));
  offset += sizeof (test_data_extra_ff);

  caps_in = gst_caps_new_simple ("image/jpeg", "parsed",
      G_TYPE_BOOLEAN, FALSE, NULL);
  GST_LOG ("Pushing single buffer of %u bytes.", (guint) total_size);
  buffer_in = g_list_append (buffer_in, buffer);

  caps_out = gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, TRUE,
      "framerate", GST_TYPE_FRACTION, 0, 1, "interlace-mode", G_TYPE_STRING,
      "progressive", "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, NULL);
  buffer_out = make_buffers_out (buffer_out, test_data_short_frame);
  buffer_out = make_buffers_out (buffer_out, test_data_normal_frame);
  buffer_out = make_buffers_out (buffer_out, test_data_entropy);
  buffer_out = make_buffers_out (buffer_out, test_data_extra_ff);

  gst_check_element_push_buffer_list ("jpegparse", buffer_in, caps_in,
      buffer_out, caps_out, GST_FLOW_OK);

  gst_caps_unref (caps_in);
  gst_caps_unref (caps_out);
}

GST_END_TEST;

static inline GstBuffer *
make_my_input_buffer (guint8 * test_data_header, gsize test_data_size)
{
  GstBuffer *buffer;
  gsize total_size = 0, offset = 0;

  total_size += sizeof (test_data_soi);
  total_size += test_data_size;
  total_size += sizeof (test_data_sof0);
  total_size += sizeof (test_data_eoi);

  buffer = gst_buffer_new_and_alloc (total_size);

  gst_buffer_fill (buffer, offset, test_data_soi, sizeof (test_data_soi));
  offset += sizeof (test_data_soi);
  gst_buffer_fill (buffer, offset, test_data_header, test_data_size);
  offset += test_data_size;
  gst_buffer_fill (buffer, offset, test_data_sof0, sizeof (test_data_sof0));
  offset += sizeof (test_data_sof0);
  gst_buffer_fill (buffer, offset, test_data_eoi, sizeof (test_data_eoi));
  offset += sizeof (test_data_eoi);

  return buffer;
}

static inline GstBuffer *
make_my_output_buffer (GstBuffer * buffer_in)
{
  GstBuffer *buffer;
  GstMapInfo map;

  gst_buffer_map (buffer_in, &map, GST_MAP_READ);
  buffer = gst_buffer_new_and_alloc (map.size);
  gst_buffer_fill (buffer, 0, map.data, map.size);
  gst_buffer_unmap (buffer_in, &map);

  return buffer;
}


GST_START_TEST (test_parse_app1_exif)
{
  GstBuffer *buffer_in, *buffer_out;
  GstCaps *caps_in, *caps_out;

  caps_in = gst_caps_new_simple ("image/jpeg", "parsed",
      G_TYPE_BOOLEAN, FALSE, NULL);

  caps_out = gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, TRUE,
      "framerate", GST_TYPE_FRACTION, 0, 1, "width", G_TYPE_INT, 80, "height",
      G_TYPE_INT, 60, "sof-marker", G_TYPE_INT, 0, "colorspace", G_TYPE_STRING,
      "sYUV", "sampling", G_TYPE_STRING, "YCbCr-4:2:0", "interlace-mode",
      G_TYPE_STRING, "progressive", "pixel-aspect-ratio", GST_TYPE_FRACTION, 1,
      1, NULL);

  buffer_in = make_my_input_buffer (test_data_app1_exif,
      sizeof (test_data_app1_exif));
  buffer_out = make_my_output_buffer (buffer_in);

  gst_check_element_push_buffer ("jpegparse", buffer_in, caps_in, buffer_out,
      caps_out);

  gst_caps_unref (caps_in);
  gst_caps_unref (caps_out);
}

GST_END_TEST;

GST_START_TEST (test_parse_comment)
{
  GstBuffer *buffer_in, *buffer_out;
  GstCaps *caps_in, *caps_out;

  caps_in = gst_caps_new_simple ("image/jpeg", "parsed",
      G_TYPE_BOOLEAN, FALSE, NULL);

  caps_out = gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, TRUE,
      "framerate", GST_TYPE_FRACTION, 0, 1, "width", G_TYPE_INT, 80, "height",
      G_TYPE_INT, 60, "sof-marker", G_TYPE_INT, 0, "colorspace", G_TYPE_STRING,
      "sYUV", "sampling", G_TYPE_STRING, "YCbCr-4:2:0", "interlace-mode",
      G_TYPE_STRING, "progressive", "pixel-aspect-ratio", GST_TYPE_FRACTION, 1,
      1, NULL);

  buffer_in = make_my_input_buffer (test_data_comment,
      sizeof (test_data_comment));
  buffer_out = make_my_output_buffer (buffer_in);

  gst_check_element_push_buffer ("jpegparse", buffer_in, caps_in, buffer_out,
      caps_out);

  gst_caps_unref (caps_in);
  gst_caps_unref (caps_out);
}

GST_END_TEST;

static Suite *
jpegparse_suite (void)
{
  Suite *s = suite_create ("jpegparse");
  TCase *tc_chain = tcase_create ("jpegparse");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_parse_single_byte);
  tcase_add_test (tc_chain, test_parse_all_in_one_buf);
  tcase_add_test (tc_chain, test_parse_app1_exif);
  tcase_add_test (tc_chain, test_parse_comment);

  return s;
}

GST_CHECK_MAIN (jpegparse);
