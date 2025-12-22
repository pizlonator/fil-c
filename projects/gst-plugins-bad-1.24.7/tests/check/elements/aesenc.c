/* GStreamer
 *
 * Copyright (C) 2021 Aaron Boxer <aaron.boxer@collabora.com>
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
#include <gst/check/gstharness.h>

unsigned char plain16[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

unsigned char enc16[] = {
  0xfc, 0x49, 0x14, 0xc6, 0xee, 0x06, 0xe1, 0xb1,
  0xc7, 0xa2, 0x3a, 0x05, 0x13, 0x15, 0x29, 0x27,
  0x40, 0xee, 0xfd, 0xcb, 0x3b, 0xbe, 0xf3, 0x0b,
  0xa7, 0xaf, 0x5e, 0x20, 0x87, 0x78, 0x8a, 0x45
};

unsigned char enc16_serialize[] = {
  0xe9, 0xaa, 0x8e, 0x83, 0x4d, 0x8d, 0x70, 0xb7,
  0xe0, 0xd2, 0x54, 0xff, 0x67, 0x0d, 0xd7, 0x18,
  0xfc, 0x49, 0x14, 0xc6, 0xee, 0x06, 0xe1, 0xb1,
  0xc7, 0xa2, 0x3a, 0x05, 0x13, 0x15, 0x29, 0x27,
  0x40, 0xee, 0xfd, 0xcb, 0x3b, 0xbe, 0xf3, 0x0b,
  0xa7, 0xaf, 0x5e, 0x20, 0x87, 0x78, 0x8a, 0x45
};

unsigned char enc16_serialize_no_per_buffer_padding[] = {
  0xe9, 0xaa, 0x8e, 0x83, 0x4d, 0x8d, 0x70, 0xb7,
  0xe0, 0xd2, 0x54, 0xff, 0x67, 0x0d, 0xd7, 0x18,
  0xfc, 0x49, 0x14, 0xc6, 0xee, 0x06, 0xe1, 0xb1,
  0xc7, 0xa2, 0x3a, 0x05, 0x13, 0x15, 0x29, 0x27
};


unsigned char plain17[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10
};

unsigned char enc17[] = {
  0xfc, 0x49, 0x14, 0xc6, 0xee, 0x06, 0xe1, 0xb1,
  0xc7, 0xa2, 0x3a, 0x05, 0x13, 0x15, 0x29, 0x27,
  0xe1, 0xe0, 0xaa, 0xf4, 0xe8, 0x29, 0x7c, 0x9f,
  0xc4, 0xe3, 0x11, 0x4a, 0x97, 0x58, 0x9c, 0xa5
};

unsigned char enc17_serialize[] = {
  0xe9, 0xaa, 0x8e, 0x83, 0x4d, 0x8d, 0x70, 0xb7,
  0xe0, 0xd2, 0x54, 0xff, 0x67, 0x0d, 0xd7, 0x18,
  0xfc, 0x49, 0x14, 0xc6, 0xee, 0x06, 0xe1, 0xb1,
  0xc7, 0xa2, 0x3a, 0x05, 0x13, 0x15, 0x29, 0x27,
  0xe1, 0xe0, 0xaa, 0xf4, 0xe8, 0x29, 0x7c, 0x9f,
  0xc4, 0xe3, 0x11, 0x4a, 0x97, 0x58, 0x9c, 0xa5
};

unsigned char enc17_serialize_no_per_buffer_padding[] = {
  0xe9, 0xaa, 0x8e, 0x83, 0x4d, 0x8d, 0x70, 0xb7,
  0xe0, 0xd2, 0x54, 0xff, 0x67, 0x0d, 0xd7, 0x18,
  0xfc, 0x49, 0x14, 0xc6, 0xee, 0x06, 0xe1, 0xb1,
  0xc7, 0xa2, 0x3a, 0x05, 0x13, 0x15, 0x29, 0x27,
};

static void
run (gboolean per_buffer_padding,
    gboolean serialize_iv,
    guchar * in_ref, gsize in_ref_len, guchar * out_ref, gsize out_ref_len)
{
  GstHarness *h;
  GstBuffer *buf, *outbuf;

  h = gst_harness_new ("aesenc");
  gst_harness_set_src_caps_str (h, "video/x-raw");

  g_object_set (h->element,
      "key", "1f9423681beb9a79215820f6bda73d0f",
      "iv", "e9aa8e834d8d70b7e0d254ff670dd718",
      "per-buffer-padding", per_buffer_padding,
      "serialize-iv", serialize_iv, NULL);

  buf = gst_buffer_new_and_alloc (in_ref_len);
  gst_buffer_fill (buf, 0, in_ref, in_ref_len);
  outbuf = gst_harness_push_and_pull (h, gst_buffer_ref (buf));

  fail_unless (gst_buffer_memcmp (outbuf, 0, out_ref, out_ref_len) == 0);

  gst_buffer_unref (outbuf);
  gst_buffer_unref (buf);

  gst_harness_teardown (h);
}

GST_START_TEST (text16)
{
  run (TRUE, FALSE, plain16, sizeof (plain16), enc16, sizeof (enc16));
}

GST_END_TEST;

GST_START_TEST (text16_serialize)
{
  run (TRUE, TRUE, plain16, sizeof (plain16), enc16_serialize,
      sizeof (enc16_serialize));
}

GST_END_TEST;

GST_START_TEST (text16_serialize_no_per_buffer_padding)
{
  run (FALSE, TRUE, plain16, sizeof (plain16),
      enc16_serialize_no_per_buffer_padding,
      sizeof (enc16_serialize_no_per_buffer_padding));
}

GST_END_TEST;

GST_START_TEST (text17)
{
  run (TRUE, FALSE, plain17, sizeof (plain17), enc17, sizeof (enc17));
}

GST_END_TEST;

GST_START_TEST (text17_serialize)
{
  run (TRUE, TRUE, plain17, sizeof (plain17), enc17_serialize,
      sizeof (enc17_serialize));
}

GST_END_TEST;

GST_START_TEST (text17_serialize_no_per_buffer_padding)
{
  run (FALSE, TRUE, plain17, sizeof (plain17),
      enc17_serialize_no_per_buffer_padding,
      sizeof (enc17_serialize_no_per_buffer_padding));
}

GST_END_TEST;

static Suite *
aesenc_suite (void)
{
  Suite *s = suite_create ("aesenc");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);
  tcase_add_test (tc, text16);
  tcase_add_test (tc, text16_serialize);
  tcase_add_test (tc, text16_serialize_no_per_buffer_padding);
  tcase_add_test (tc, text17);
  tcase_add_test (tc, text17_serialize);
  tcase_add_test (tc, text17_serialize_no_per_buffer_padding);
  return s;
}

GST_CHECK_MAIN (aesenc);
