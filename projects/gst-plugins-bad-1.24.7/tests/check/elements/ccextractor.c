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
#include <gst/check/gstharness.h>
#include <gst/video/video.h>

#include <string.h>

#define VIDEO_CAPS_STR "video/x-raw, " \
  "format = (string) UYVY, " \
  "width = (int) 1920, " \
  "height = (int) 1080, " \
  "framerate = (fraction) 30/1"

static GstStaticCaps video_caps = GST_STATIC_CAPS (VIDEO_CAPS_STR);
static GstStaticCaps cea708_cc_data_caps =
    GST_STATIC_CAPS
    ("closedcaption/x-cea-708,format=(string) cc_data, framerate = (fraction) 30/1");
static GstStaticCaps cea708_cdp_caps =
    GST_STATIC_CAPS
    ("closedcaption/x-cea-708,format=(string) cdp, framerate = (fraction) 30/1");

GST_START_TEST (no_captions)
{
  GstHarness *h;
  GstBuffer *buf, *outbuf;
  GstCaps *caps;

  h = gst_harness_new ("ccextractor");

  gst_harness_set_src_caps_str (h, VIDEO_CAPS_STR);

  buf = gst_buffer_new_and_alloc (128);
  outbuf = gst_harness_push_and_pull (h, gst_buffer_ref (buf));

  fail_unless (outbuf != NULL);
  fail_unless (outbuf == buf);
  fail_unless (gst_element_get_static_pad (h->element, "caption") == NULL);

  caps = gst_pad_get_current_caps (h->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&video_caps)));
  gst_caps_unref (caps);

  gst_buffer_unref (buf);
  gst_buffer_unref (outbuf);

  gst_harness_teardown (h);
}

GST_END_TEST;

static void
on_caption_pad_added (GstElement * ccextractor, GstPad * pad, GstHarness * h)
{
  fail_unless (strcmp (GST_OBJECT_NAME (pad), "caption") == 0);
  gst_harness_add_element_src_pad (h, pad);
}

GST_START_TEST (captions)
{
  GstHarness *h, *h2;
  GstBuffer *buf, *outbuf;
  const guint8 caption_data[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  GstCaps *caps;
  GstVideoTimeCode *tc;
  GstVideoTimeCodeMeta *tc_meta;

  h = gst_harness_new ("ccextractor");
  h2 = gst_harness_new_with_element (h->element, NULL, NULL);

  g_signal_connect (h->element, "pad-added", G_CALLBACK (on_caption_pad_added),
      h2);

  gst_harness_set_src_caps_str (h, VIDEO_CAPS_STR);

  buf = gst_buffer_new_and_alloc (128);
  gst_buffer_add_video_caption_meta (buf, GST_VIDEO_CAPTION_TYPE_CEA708_RAW,
      caption_data, sizeof (caption_data));

  tc = gst_video_time_code_new (30, 1, NULL, GST_VIDEO_TIME_CODE_FLAGS_NONE, 0,
      0, 0, 0, 0);
  gst_buffer_add_video_time_code_meta (buf, tc);

  outbuf = gst_harness_push_and_pull (h, gst_buffer_ref (buf));

  fail_unless (outbuf != NULL);
  fail_unless (outbuf == buf);

  tc_meta = gst_buffer_get_video_time_code_meta (outbuf);
  fail_unless (tc_meta != NULL);
  fail_unless_equals_int (gst_video_time_code_compare (&tc_meta->tc, tc), 0);

  gst_buffer_unref (outbuf);
  gst_buffer_unref (buf);

  fail_unless (h2->sinkpad != NULL);
  outbuf = gst_harness_pull (h2);
  fail_unless (outbuf != NULL);
  fail_unless (gst_buffer_memcmp (outbuf, 0, caption_data,
          sizeof (caption_data)) == 0);

  tc_meta = gst_buffer_get_video_time_code_meta (outbuf);
  fail_unless (tc_meta != NULL);
  fail_unless_equals_int (gst_video_time_code_compare (&tc_meta->tc, tc), 0);
  gst_video_time_code_free (tc);

  gst_buffer_unref (outbuf);

  caps = gst_pad_get_current_caps (h->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&video_caps)));
  gst_caps_unref (caps);

  caps = gst_pad_get_current_caps (h2->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&cea708_cc_data_caps)));
  gst_caps_unref (caps);

  buf = gst_buffer_new_and_alloc (128);
  gst_buffer_add_video_caption_meta (buf, GST_VIDEO_CAPTION_TYPE_CEA708_RAW,
      caption_data, sizeof (caption_data));

  tc = gst_video_time_code_new (30, 1, NULL, GST_VIDEO_TIME_CODE_FLAGS_NONE, 0,
      0, 0, 1, 0);
  gst_buffer_add_video_time_code_meta (buf, tc);

  outbuf = gst_harness_push_and_pull (h, gst_buffer_ref (buf));

  fail_unless (outbuf != NULL);
  fail_unless (outbuf == buf);

  tc_meta = gst_buffer_get_video_time_code_meta (outbuf);
  fail_unless (tc_meta != NULL);
  fail_unless_equals_int (gst_video_time_code_compare (&tc_meta->tc, tc), 0);

  gst_buffer_unref (outbuf);
  gst_buffer_unref (buf);

  fail_unless (h2->sinkpad != NULL);
  outbuf = gst_harness_pull (h2);
  fail_unless (outbuf != NULL);
  fail_unless (gst_buffer_memcmp (outbuf, 0, caption_data,
          sizeof (caption_data)) == 0);

  tc_meta = gst_buffer_get_video_time_code_meta (outbuf);
  fail_unless (tc_meta != NULL);
  fail_unless_equals_int (gst_video_time_code_compare (&tc_meta->tc, tc), 0);
  gst_video_time_code_free (tc);

  gst_buffer_unref (outbuf);

  caps = gst_pad_get_current_caps (h->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&video_caps)));
  gst_caps_unref (caps);

  caps = gst_pad_get_current_caps (h2->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&cea708_cc_data_caps)));
  gst_caps_unref (caps);

  gst_harness_teardown (h);
  gst_harness_teardown (h2);
}

GST_END_TEST;

GST_START_TEST (no_captions_at_beginning_and_end)
{
  GstHarness *h, *h2;
  GstBuffer *buf, *outbuf;
  const guint8 caption_data[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  GstCaps *caps;

  h = gst_harness_new ("ccextractor");
  h2 = gst_harness_new_with_element (h->element, NULL, NULL);

  g_signal_connect (h->element, "pad-added", G_CALLBACK (on_caption_pad_added),
      h2);

  gst_harness_set_src_caps_str (h, VIDEO_CAPS_STR);

  buf = gst_buffer_new_and_alloc (128);

  outbuf = gst_harness_push_and_pull (h, gst_buffer_ref (buf));

  fail_unless (outbuf != NULL);
  fail_unless (outbuf == buf);
  gst_buffer_unref (outbuf);
  gst_buffer_unref (buf);

  buf = gst_buffer_new_and_alloc (128);
  gst_buffer_add_video_caption_meta (buf, GST_VIDEO_CAPTION_TYPE_CEA708_RAW,
      caption_data, sizeof (caption_data));

  outbuf = gst_harness_push_and_pull (h, gst_buffer_ref (buf));

  fail_unless (outbuf != NULL);
  fail_unless (outbuf == buf);
  gst_buffer_unref (outbuf);
  gst_buffer_unref (buf);

  caps = gst_pad_get_current_caps (h->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&video_caps)));
  gst_caps_unref (caps);

  fail_unless (h2->sinkpad != NULL);
  outbuf = gst_harness_pull (h2);
  fail_unless (outbuf != NULL);
  fail_unless (gst_buffer_memcmp (outbuf, 0, caption_data,
          sizeof (caption_data)) == 0);
  gst_buffer_unref (outbuf);

  caps = gst_pad_get_current_caps (h->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&video_caps)));
  gst_caps_unref (caps);

  caps = gst_pad_get_current_caps (h2->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&cea708_cc_data_caps)));
  gst_caps_unref (caps);

  buf = gst_buffer_new_and_alloc (128);

  outbuf = gst_harness_push_and_pull (h, gst_buffer_ref (buf));

  fail_unless (outbuf != NULL);
  fail_unless (outbuf == buf);
  gst_buffer_unref (outbuf);
  gst_buffer_unref (buf);

  fail_if (gst_harness_try_pull (h2) != NULL);

  caps = gst_pad_get_current_caps (h->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&video_caps)));
  gst_caps_unref (caps);

  caps = gst_pad_get_current_caps (h2->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&cea708_cc_data_caps)));
  gst_caps_unref (caps);

  gst_harness_teardown (h);
  gst_harness_teardown (h2);
}

GST_END_TEST;

GST_START_TEST (captions_format_change)
{
  GstHarness *h, *h2;
  GstBuffer *buf, *outbuf;
  const guint8 caption_data[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  GstCaps *caps;

  h = gst_harness_new ("ccextractor");
  h2 = gst_harness_new_with_element (h->element, NULL, NULL);

  g_signal_connect (h->element, "pad-added", G_CALLBACK (on_caption_pad_added),
      h2);

  gst_harness_set_src_caps_str (h, VIDEO_CAPS_STR);

  buf = gst_buffer_new_and_alloc (128);
  gst_buffer_add_video_caption_meta (buf, GST_VIDEO_CAPTION_TYPE_CEA708_RAW,
      caption_data, sizeof (caption_data));

  outbuf = gst_harness_push_and_pull (h, gst_buffer_ref (buf));

  fail_unless (outbuf != NULL);
  fail_unless (outbuf == buf);
  gst_buffer_unref (outbuf);
  gst_buffer_unref (buf);

  fail_unless (h2->sinkpad != NULL);
  outbuf = gst_harness_pull (h2);
  fail_unless (outbuf != NULL);
  fail_unless (gst_buffer_memcmp (outbuf, 0, caption_data,
          sizeof (caption_data)) == 0);
  gst_buffer_unref (outbuf);

  caps = gst_pad_get_current_caps (h->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&video_caps)));
  gst_caps_unref (caps);

  caps = gst_pad_get_current_caps (h2->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&cea708_cc_data_caps)));
  gst_caps_unref (caps);

  buf = gst_buffer_new_and_alloc (128);
  gst_buffer_add_video_caption_meta (buf, GST_VIDEO_CAPTION_TYPE_CEA708_CDP,
      caption_data, sizeof (caption_data));

  outbuf = gst_harness_push_and_pull (h, gst_buffer_ref (buf));

  fail_unless (outbuf != NULL);
  fail_unless (outbuf == buf);
  gst_buffer_unref (outbuf);
  gst_buffer_unref (buf);

  fail_unless (h2->sinkpad != NULL);
  outbuf = gst_harness_pull (h2);
  fail_unless (outbuf != NULL);
  fail_unless (gst_buffer_memcmp (outbuf, 0, caption_data,
          sizeof (caption_data)) == 0);
  gst_buffer_unref (outbuf);

  caps = gst_pad_get_current_caps (h->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&video_caps)));
  gst_caps_unref (caps);

  caps = gst_pad_get_current_caps (h2->sinkpad);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_can_intersect (caps,
          gst_static_caps_get (&cea708_cdp_caps)));
  gst_caps_unref (caps);

  gst_harness_teardown (h);
  gst_harness_teardown (h2);
}

GST_END_TEST;

static Suite *
ccextractor_suite (void)
{
  Suite *s = suite_create ("ccextractor");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);

  tcase_add_test (tc, no_captions);
  tcase_add_test (tc, captions);
  tcase_add_test (tc, no_captions_at_beginning_and_end);
  tcase_add_test (tc, captions_format_change);

  return s;
}

GST_CHECK_MAIN (ccextractor);
