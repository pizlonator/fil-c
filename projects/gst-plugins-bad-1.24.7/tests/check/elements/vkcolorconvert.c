/* GStreamer
 *
 * unit test for vulkancolorconvert element
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
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
#include <gst/vulkan/vulkan.h>

typedef struct _TestFrame
{
  gint width;
  gint height;
  GstVideoFormat v_format;
  guint8 *data[GST_VIDEO_MAX_PLANES];
} TestFrame;

#define IGNORE_MAGIC 0x05

static const guint8 rgba_reorder_data[] = { 0x49, 0x24, 0x72, 0xff };
static const guint8 rgbx_reorder_data[] = { 0x49, 0x24, 0x72, IGNORE_MAGIC };
static const guint8 argb_reorder_data[] = { 0xff, 0x49, 0x24, 0x72 };
static const guint8 xrgb_reorder_data[] = { IGNORE_MAGIC, 0x49, 0x24, 0x72 };
static const guint8 bgra_reorder_data[] = { 0x72, 0x24, 0x49, 0xff };
static const guint8 bgrx_reorder_data[] = { 0x72, 0x24, 0x49, IGNORE_MAGIC };
static const guint8 abgr_reorder_data[] = { 0xff, 0x72, 0x24, 0x49 };
static const guint8 xbgr_reorder_data[] = { IGNORE_MAGIC, 0x72, 0x24, 0x49 };

static TestFrame test_rgba_reorder[] = {
  {1, 1, GST_VIDEO_FORMAT_RGBA, {(guint8 *) & rgba_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_RGBx, {(guint8 *) & rgbx_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_ARGB, {(guint8 *) & argb_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_xRGB, {(guint8 *) & xrgb_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_BGRA, {(guint8 *) & bgra_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_BGRx, {(guint8 *) & bgrx_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_ABGR, {(guint8 *) & abgr_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_xBGR, {(guint8 *) & xbgr_reorder_data}},
};

GST_START_TEST (test_vulkan_color_convert_rgba_reorder)
{
  GstHarness *h =
      gst_harness_new_parse
      ("vulkanupload ! vulkancolorconvert ! vulkandownload");
  int i, j, k;

  for (i = 0; i < G_N_ELEMENTS (test_rgba_reorder); i++) {
    for (j = 0; j < G_N_ELEMENTS (test_rgba_reorder); j++) {
      GstCaps *in_caps, *out_caps;
      GstVideoInfo in_info, out_info;
      GstBuffer *inbuf, *outbuf;
      GstMapInfo map_info;

      fail_unless (gst_video_info_set_format (&in_info,
              test_rgba_reorder[i].v_format, test_rgba_reorder[i].width,
              test_rgba_reorder[i].height));
      fail_unless (gst_video_info_set_format (&out_info,
              test_rgba_reorder[j].v_format, test_rgba_reorder[j].width,
              test_rgba_reorder[j].height));

      in_caps = gst_video_info_to_caps (&in_info);
      out_caps = gst_video_info_to_caps (&out_info);

      gst_harness_set_caps (h, in_caps, out_caps);

      GST_INFO ("converting from %s to %s",
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&in_info)),
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&out_info)));

      inbuf =
          gst_buffer_new_wrapped_full (0, test_rgba_reorder[i].data[0], 4, 0, 4,
          NULL, NULL);
      outbuf = gst_harness_push_and_pull (h, inbuf);

      fail_unless (gst_buffer_map (outbuf, &map_info, GST_MAP_READ));
      fail_unless (map_info.size == out_info.size);

      for (k = 0; k < out_info.size; k++) {
        if (test_rgba_reorder[j].data[0][k] != IGNORE_MAGIC
            && map_info.data[k] != IGNORE_MAGIC) {
          guint8 *expected = test_rgba_reorder[j].data[0];
          GST_DEBUG ("%i 0x%x =? 0x%x", k, expected[k],
              (guint) map_info.data[k]);
          fail_unless (expected[k] == map_info.data[k]);
        }
      }
      gst_buffer_unmap (outbuf, &map_info);
      gst_buffer_unref (outbuf);
    }
  }

  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
vkcolorconvert_suite (void)
{
  Suite *s = suite_create ("vkcolorconvert");
  TCase *tc_basic = tcase_create ("general");
  GstVulkanInstance *instance;
  gboolean have_instance;

  suite_add_tcase (s, tc_basic);

  /* FIXME: CI doesn't have a software vulkan renderer (and none exists currently) */
  instance = gst_vulkan_instance_new ();
  have_instance = gst_vulkan_instance_open (instance, NULL);
  gst_object_unref (instance);
  if (have_instance) {
    tcase_add_test (tc_basic, test_vulkan_color_convert_rgba_reorder);
  }

  return s;
}

GST_CHECK_MAIN (vkcolorconvert);
