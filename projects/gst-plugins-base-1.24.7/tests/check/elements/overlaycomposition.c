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

#define VIDEO_WIDTH 320
#define VIDEO_HEIGHT 240
#define OVERLAY_WIDTH 16
#define OVERLAY_HEIGHT 16

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define VIDEO_FORMAT_STR "BGRA"
#define VIDEO_FORMAT GST_VIDEO_FORMAT_BGRA
#else
#define VIDEO_FORMAT_STR "ARGB"
#define VIDEO_FORMAT GST_VIDEO_FORMAT_ARGB
#endif

#define VIDEO_CAPS "video/x-raw, " \
  "format = (string) " VIDEO_FORMAT_STR ", " \
  "width = (int) 320, " \
  "height = (int) 240, " \
  "framerate = (fraction) 30/1"

#define VIDEO_CAPS_WITH_META "video/x-raw(" GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION "), " \
  "format = (string) " VIDEO_FORMAT_STR ", " \
  "width = (int) 320, " \
  "height = (int) 240, " \
  "framerate = (fraction) 30/1"

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
#define GST_READ_UINT32_NATIVE(data) GST_READ_UINT32_BE(data)
#define GST_WRITE_UINT32_NATIVE(data,val) GST_WRITE_UINT32_BE(data,val)
#else
#define GST_READ_UINT32_NATIVE(data) GST_READ_UINT32_LE(data)
#define GST_WRITE_UINT32_NATIVE(data,val) GST_WRITE_UINT32_LE(data,val)
#endif

static GstBuffer *
create_video_frame (void)
{
  GstBuffer *buffer;
  GstMapInfo map;
  guint i;

  buffer = gst_buffer_new_and_alloc (VIDEO_WIDTH * VIDEO_HEIGHT * 4);
  gst_buffer_add_video_meta (buffer, GST_VIDEO_FRAME_FLAG_NONE, VIDEO_FORMAT,
      VIDEO_WIDTH, VIDEO_HEIGHT);

  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
  for (i = 0; i < map.size; i += 4)
    GST_WRITE_UINT32_NATIVE (map.data + i, 0xff000000);
  gst_buffer_unmap (buffer, &map);

  return buffer;
}

static GstBuffer *
create_overlay_frame (guint32 color)
{
  GstBuffer *buffer;
  GstMapInfo map;
  guint i;

  buffer = gst_buffer_new_and_alloc (VIDEO_WIDTH * VIDEO_HEIGHT * 4);
  gst_buffer_add_video_meta (buffer, GST_VIDEO_FRAME_FLAG_NONE, VIDEO_FORMAT,
      VIDEO_WIDTH, VIDEO_HEIGHT);

  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
  for (i = 0; i < map.size; i += 4)
    GST_WRITE_UINT32_NATIVE (map.data + i, color);
  gst_buffer_unmap (buffer, &map);

  return buffer;
}

typedef struct
{
  gboolean valid;
  GstVideoInfo info;

  guint expected_window_width, expected_window_height;

  GstVideoOverlayComposition *comp;
} State;

static void
on_caps_changed (GstElement * element, GstCaps * caps, guint window_width,
    guint window_height, State * s)
{
  fail_unless (gst_video_info_from_caps (&s->info, caps));
  s->valid = TRUE;

  fail_unless_equals_int (s->expected_window_width, window_width);
  fail_unless_equals_int (s->expected_window_height, window_height);
}

static GstVideoOverlayComposition *
on_draw (GstElement * element, GstSample * sample, State * s)
{
  fail_unless (s->valid);
  fail_unless (GST_IS_SAMPLE (sample));

  return gst_video_overlay_composition_ref (s->comp);
}

GST_START_TEST (render_fallback)
{
  GstHarness *h;
  GstVideoOverlayComposition *comp;
  GstVideoOverlayRectangle *rect;
  GstBuffer *buffer, *overlay;
  State s = { 0, };
  GstMapInfo map;
  guint x, y;

  h = gst_harness_new ("overlaycomposition");

  g_signal_connect (h->element, "draw", G_CALLBACK (on_draw), &s);
  g_signal_connect (h->element, "caps-changed", G_CALLBACK (on_caps_changed),
      &s);

  buffer = create_video_frame ();
  overlay = create_overlay_frame (0x80ffffff);
  rect =
      gst_video_overlay_rectangle_new_raw (overlay, 32, 32, OVERLAY_WIDTH,
      OVERLAY_HEIGHT, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  gst_buffer_unref (overlay);
  comp = gst_video_overlay_composition_new (rect);
  gst_video_overlay_rectangle_unref (rect);

  s.comp = comp;
  s.expected_window_width = VIDEO_WIDTH;
  s.expected_window_height = VIDEO_HEIGHT;

  gst_harness_set_src_caps_str (h, VIDEO_CAPS);

  buffer = gst_harness_push_and_pull (h, buffer);

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  fail_unless_equals_int (map.size, VIDEO_WIDTH * VIDEO_HEIGHT * 4);

  for (y = 0; y < VIDEO_HEIGHT; y++) {
    for (x = 0; x < VIDEO_WIDTH; x++) {
      guint32 val =
          GST_READ_UINT32_NATIVE (map.data + y * VIDEO_WIDTH * 4 + x * 4);
      guint32 expected_val;

      if ((x >= 32 && x < 48) && (y >= 32 && y < 48)) {
        expected_val = 0xff808080;
      } else {
        expected_val = 0xff000000;
      }

      fail_unless (val == expected_val, "Expected %08x but got %08x at (%u,%u)",
          expected_val, val, x, y);
    }
  }

  gst_buffer_unmap (buffer, &map);
  gst_buffer_unref (buffer);

  gst_video_overlay_composition_unref (s.comp);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (render_fallback_2)
{
  GstHarness *h;
  GstVideoOverlayComposition *comp;
  GstVideoOverlayRectangle *rect;
  GstBuffer *buffer, *overlay;
  State s = { 0, };
  GstMapInfo map;
  guint x, y;

  h = gst_harness_new ("overlaycomposition");

  g_signal_connect (h->element, "draw", G_CALLBACK (on_draw), &s);
  g_signal_connect (h->element, "caps-changed", G_CALLBACK (on_caps_changed),
      &s);

  overlay = create_overlay_frame (0xffff0000);
  rect =
      gst_video_overlay_rectangle_new_raw (overlay, 32, 32, OVERLAY_WIDTH,
      OVERLAY_HEIGHT, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  gst_buffer_unref (overlay);
  comp = gst_video_overlay_composition_new (rect);
  gst_video_overlay_rectangle_unref (rect);

  overlay = create_overlay_frame (0xff0000ff);
  rect =
      gst_video_overlay_rectangle_new_raw (overlay, 64, 64, OVERLAY_WIDTH,
      OVERLAY_HEIGHT, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  gst_buffer_unref (overlay);
  gst_video_overlay_composition_add_rectangle (comp, rect);
  gst_video_overlay_rectangle_unref (rect);

  s.comp = comp;
  s.expected_window_width = VIDEO_WIDTH;
  s.expected_window_height = VIDEO_HEIGHT;

  gst_harness_set_src_caps_str (h, VIDEO_CAPS);

  buffer = create_video_frame ();
  buffer = gst_harness_push_and_pull (h, buffer);

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  fail_unless_equals_int (map.size, VIDEO_WIDTH * VIDEO_HEIGHT * 4);

  for (y = 0; y < VIDEO_HEIGHT; y++) {
    for (x = 0; x < VIDEO_WIDTH; x++) {
      guint32 val =
          GST_READ_UINT32_NATIVE (map.data + y * VIDEO_WIDTH * 4 + x * 4);
      guint32 expected_val;

      if ((x >= 32 && x < 48) && (y >= 32 && y < 48)) {
        expected_val = 0xffff0000;
      } else if ((x >= 64 && x < 80) && (y >= 64 && y < 80)) {
        expected_val = 0xff0000ff;
      } else {
        expected_val = 0xff000000;
      }

      fail_unless (val == expected_val, "Expected %08x but got %08x at (%u,%u)",
          expected_val, val, x, y);
    }
  }

  gst_buffer_unmap (buffer, &map);
  gst_buffer_unref (buffer);

  gst_video_overlay_composition_unref (s.comp);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (render_meta)
{
  GstHarness *h;
  GstVideoOverlayComposition *comp;
  GstVideoOverlayRectangle *rect;
  GstBuffer *buffer, *overlay;
  State s = { 0, };
  GstMapInfo map;
  guint x, y;
  GstVideoOverlayCompositionMeta *meta;

  h = gst_harness_new ("overlaycomposition");

  g_signal_connect (h->element, "draw", G_CALLBACK (on_draw), &s);
  g_signal_connect (h->element, "caps-changed", G_CALLBACK (on_caps_changed),
      &s);

  overlay = create_overlay_frame (0xffff0000);
  rect =
      gst_video_overlay_rectangle_new_raw (overlay, 32, 32, OVERLAY_WIDTH,
      OVERLAY_HEIGHT, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  gst_buffer_unref (overlay);
  comp = gst_video_overlay_composition_new (rect);
  gst_video_overlay_rectangle_unref (rect);

  overlay = create_overlay_frame (0xff0000ff);
  rect =
      gst_video_overlay_rectangle_new_raw (overlay, 64, 64, OVERLAY_WIDTH,
      OVERLAY_HEIGHT, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  gst_buffer_unref (overlay);
  gst_video_overlay_composition_add_rectangle (comp, rect);
  gst_video_overlay_rectangle_unref (rect);

  s.comp = comp;
  s.expected_window_width = VIDEO_WIDTH;
  s.expected_window_height = VIDEO_HEIGHT;

  gst_harness_add_propose_allocation_meta (h,
      GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, NULL);
  gst_harness_set_src_caps_str (h, VIDEO_CAPS);

  buffer = create_video_frame ();
  buffer = gst_harness_push_and_pull (h, buffer);

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  fail_unless_equals_int (map.size, VIDEO_WIDTH * VIDEO_HEIGHT * 4);

  for (y = 0; y < VIDEO_HEIGHT; y++) {
    for (x = 0; x < VIDEO_WIDTH; x++) {
      guint32 val =
          GST_READ_UINT32_NATIVE (map.data + y * VIDEO_WIDTH * 4 + x * 4);
      guint32 expected_val = 0xff000000;

      fail_unless (val == expected_val, "Expected %08x but got %08x at (%u,%u)",
          expected_val, val, x, y);
    }
  }

  gst_buffer_unmap (buffer, &map);

  meta = gst_buffer_get_video_overlay_composition_meta (buffer);
  fail_unless (meta);
  fail_unless (meta->overlay == s.comp);
  gst_buffer_unref (buffer);

  gst_video_overlay_composition_unref (s.comp);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
overlaycomposition_suite (void)
{
  Suite *s = suite_create ("overlaycomposition");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);

  tcase_add_test (tc, render_fallback);
  tcase_add_test (tc, render_fallback_2);
  tcase_add_test (tc, render_meta);

  return s;
}

GST_CHECK_MAIN (overlaycomposition);
