/* GStreamer
 *
 * Copyright (C) 2019 Mathieu Duponchelle <mathieu@centricular.com>
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
#include <gst/video/video.h>

GST_START_TEST (basic)
{
  GstHarness *h;
  GstBuffer *buf, *outbuf;
  GstVideoInfo info;
  GstVideoCaptionMeta *out_cc_meta;
  guint i;
  guint8 empty_data[] = { 0x8c, 0x80, 0x80, 0x0, 0x80, 0x80 };
  guint8 full_data[] = { 0x8c, 0x42, 0x43, 0x0, 0x44, 0x45 };
  GstCaps *caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, "I420",
      "width", G_TYPE_INT, 720,
      "height", G_TYPE_INT, 525,
      "interlace-mode", G_TYPE_STRING, "interleaved",
      NULL);

  h = gst_harness_new_parse
      ("line21encoder remove-caption-meta=true ! line21decoder");
  gst_harness_set_caps (h, gst_caps_ref (caps), gst_caps_ref (caps));

  gst_video_info_from_caps (&info, caps);

  gst_caps_unref (caps);

  buf = gst_buffer_new_and_alloc (info.size);
  outbuf = gst_harness_push_and_pull (h, buf);

  fail_unless (outbuf != NULL);
  fail_unless_equals_int (gst_buffer_get_n_meta (outbuf,
          GST_VIDEO_CAPTION_META_API_TYPE), 1);

  out_cc_meta = gst_buffer_get_video_caption_meta (outbuf);
  fail_unless (out_cc_meta != NULL);
  fail_unless (out_cc_meta->size == 6);

  for (i = 0; i < out_cc_meta->size; i++)
    fail_unless_equals_int (out_cc_meta->data[i], empty_data[i]);

  gst_buffer_unref (outbuf);

  buf = gst_buffer_new_and_alloc (info.size);
  gst_buffer_add_video_caption_meta (buf, GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A,
      full_data, 6);

  outbuf = gst_harness_push_and_pull (h, buf);

  fail_unless (outbuf != NULL);
  fail_unless_equals_int (gst_buffer_get_n_meta (outbuf,
          GST_VIDEO_CAPTION_META_API_TYPE), 1);

  out_cc_meta = gst_buffer_get_video_caption_meta (outbuf);
  fail_unless (out_cc_meta != NULL);

  for (i = 0; i < out_cc_meta->size; i++)
    fail_unless (out_cc_meta->data[i] == full_data[i]);

  gst_buffer_unref (outbuf);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (remove_caption_meta)
{
  GstHarness *h;
  GstBuffer *buf, *outbuf;
  GstVideoInfo info;
  GstVideoCaptionMeta *out_cc_meta;
  guint8 full_data[] = { 0x8c, 0x42, 0x43, 0x0, 0x44, 0x45 };
  GstCaps *caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, "I420",
      "width", G_TYPE_INT, 720,
      "height", G_TYPE_INT, 525,
      "interlace-mode", G_TYPE_STRING, "interleaved",
      NULL);

  h = gst_harness_new_parse ("line21encoder remove-caption-meta=true");
  gst_harness_set_caps (h, gst_caps_ref (caps), gst_caps_ref (caps));

  gst_video_info_from_caps (&info, caps);

  gst_caps_unref (caps);

  buf = gst_buffer_new_and_alloc (info.size);
  gst_buffer_add_video_caption_meta (buf, GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A,
      full_data, 6);

  outbuf = gst_harness_push_and_pull (h, buf);
  fail_unless (outbuf != NULL);
  fail_unless_equals_int (gst_buffer_get_n_meta (outbuf,
          GST_VIDEO_CAPTION_META_API_TYPE), 0);

  out_cc_meta = gst_buffer_get_video_caption_meta (outbuf);
  fail_unless (out_cc_meta == NULL);

  gst_buffer_unref (outbuf);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
line21_suite (void)
{
  Suite *s = suite_create ("line21");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);

  tcase_add_test (tc, basic);
  tcase_add_test (tc, remove_caption_meta);

  return s;
}

GST_CHECK_MAIN (line21);
