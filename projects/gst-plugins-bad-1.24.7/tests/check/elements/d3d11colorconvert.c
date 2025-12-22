/* GStreamer
 *
 * unit test for d3d11convert element
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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

static const gchar *run_visual_test = NULL;

/* enable this define to see color conversion result with videosink */
#define RUN_VISUAL_TEST 0

typedef struct _TestFrame
{
  gint width;
  gint height;
  GstVideoFormat v_format;
  guint8 *data[GST_VIDEO_MAX_PLANES];
} TestFrame;

#define IGNORE_MAGIC 0x05

static const guint8 rgba_reorder_data[] = { 0x49, 0x24, 0x72, 0xff };
static const guint8 bgra_reorder_data[] = { 0x72, 0x24, 0x49, 0xff };

static const gchar *YUV_FORMATS[] = {
  "VUYA", "NV12", "P010_10LE", "P012_LE", "P016_LE", "I420", "I420_10LE",
  "I420_12LE", "YV12", "NV21", "Y444", "Y444_10LE", "Y444_12LE", "Y444_16LE",
  "Y42B", "I422_10LE", "I422_12LE", "AYUV", "AYUV64"
};

static const gchar *RGB_FORMATS[] = {
  "BGRA", "RGBA", "RGB10A2_LE", "BGRx", "RGBx", "RGBA64_LE", "RGBP", "BGRP",
  "GBR", "GBR_10LE", "GBR_12LE", "GBRA", "GBRA_10LE", "GBRA_12LE"
};

static const gchar *PACKED_YUV_FORMATS[] = {
  "Y410", "YUY2",
};

static const gchar *GRAY_FORMATS[] = {
  "GRAY8", "GRAY16_LE"
};

static TestFrame test_rgba_reorder[] = {
  {1, 1, GST_VIDEO_FORMAT_RGBA, {(guint8 *) & rgba_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_BGRA, {(guint8 *) & bgra_reorder_data}},
};

GST_START_TEST (test_d3d11_convert_rgba_reorder)
{
  GstHarness *h =
      gst_harness_new_parse ("d3d11upload ! d3d11convert ! d3d11download");
  gint i, j, k;

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
        guint8 *expected = test_rgba_reorder[j].data[0];
        GST_DEBUG ("%i 0x%x =? 0x%x", k, expected[k], (guint) map_info.data[k]);
        fail_unless (expected[k] == map_info.data[k]);
      }
      gst_buffer_unmap (outbuf, &map_info);
      gst_buffer_unref (outbuf);
    }
  }

  gst_harness_teardown (h);
}

GST_END_TEST;

static gboolean
bus_cb (GstBus * bus, GstMessage * message, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *debug = NULL;

      gst_message_parse_error (message, &err, &debug);

      GST_ERROR ("Error: %s : %s", err->message, debug);
      g_error_free (err);
      g_free (debug);

      fail_if (TRUE, "failed");
      g_main_loop_quit (loop);
    }
      break;
    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      break;
    default:
      break;
  }
  return TRUE;
}

static void
run_convert_pipelne (const gchar * in_format, const gchar * out_format)
{
  GstBus *bus;
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  gchar *pipeline_str =
      g_strdup_printf ("videotestsrc num-buffers=1 is-live=true ! "
      "video/x-raw,format=%s,framerate=3/1,width=128,height=64,"
      "pixel-aspect-ratio=1/1 ! d3d11upload ! "
      "d3d11convert border-color=0xffffaaaaaaaaaaaa ! d3d11download ! "
      "video/x-raw,format=%s,width=320,height=240,pixel-aspect-ratio=1/1 ! "
      "videoconvert ! %s", in_format, out_format,
      run_visual_test ? "d3d11videosink" : "fakesink");
  GstElement *pipeline;

  pipeline = gst_parse_launch (pipeline_str, NULL);
  fail_unless (pipeline != NULL);
  g_free (pipeline_str);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, (GstBusFunc) bus_cb, loop);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (loop);
  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_bus_remove_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (pipeline);
  g_main_loop_unref (loop);
}

GST_START_TEST (test_d3d11_convert_yuv_yuv)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (YUV_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (YUV_FORMATS); j++) {
      GST_DEBUG ("run conversion %s to %s", YUV_FORMATS[i], YUV_FORMATS[j]);
      run_convert_pipelne (YUV_FORMATS[i], YUV_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_d3d11_convert_yuv_rgb)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (YUV_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (RGB_FORMATS); j++) {
      GST_DEBUG ("run conversion %s to %s", YUV_FORMATS[i], RGB_FORMATS[j]);
      run_convert_pipelne (YUV_FORMATS[i], RGB_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_d3d11_convert_yuv_gray)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (YUV_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (GRAY_FORMATS); j++) {
      GST_DEBUG ("run conversion %s to %s", YUV_FORMATS[i], GRAY_FORMATS[j]);
      run_convert_pipelne (YUV_FORMATS[i], GRAY_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_d3d11_convert_rgb_yuv)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (RGB_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (YUV_FORMATS); j++) {
      GST_DEBUG ("run conversion %s to %s", RGB_FORMATS[i], YUV_FORMATS[j]);
      run_convert_pipelne (RGB_FORMATS[i], YUV_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_d3d11_convert_rgb_rgb)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (RGB_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (RGB_FORMATS); j++) {
      GST_DEBUG ("run conversion %s to %s", RGB_FORMATS[i], RGB_FORMATS[j]);
      run_convert_pipelne (RGB_FORMATS[i], RGB_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_d3d11_convert_rgb_gray)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (RGB_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (GRAY_FORMATS); j++) {
      GST_DEBUG ("run conversion %s to %s", RGB_FORMATS[i], GRAY_FORMATS[j]);
      run_convert_pipelne (RGB_FORMATS[i], GRAY_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_d3d11_convert_packed_yuv_yuv)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (PACKED_YUV_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (YUV_FORMATS); j++) {
      GST_DEBUG ("run conversion %s to %s", PACKED_YUV_FORMATS[i],
          YUV_FORMATS[j]);
      run_convert_pipelne (PACKED_YUV_FORMATS[i], YUV_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_d3d11_convert_packed_yuv_rgb)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (PACKED_YUV_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (RGB_FORMATS); j++) {
      GST_DEBUG ("run conversion %s to %s", PACKED_YUV_FORMATS[i],
          RGB_FORMATS[j]);
      run_convert_pipelne (PACKED_YUV_FORMATS[i], RGB_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_d3d11_convert_packed_yuv_gray)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (PACKED_YUV_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (GRAY_FORMATS); j++) {
      GST_DEBUG ("run conversion %s to %s", PACKED_YUV_FORMATS[i],
          GRAY_FORMATS[j]);
      run_convert_pipelne (PACKED_YUV_FORMATS[i], GRAY_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_d3d11_convert_gray_yuv)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (GRAY_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (YUV_FORMATS); j++) {
      GST_DEBUG ("run conversion %s to %s", GRAY_FORMATS[i], YUV_FORMATS[j]);
      run_convert_pipelne (GRAY_FORMATS[i], YUV_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_d3d11_convert_gray_rgb)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (GRAY_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (RGB_FORMATS); j++) {
      GST_DEBUG ("run conversion %s to %s", GRAY_FORMATS[i], RGB_FORMATS[j]);
      run_convert_pipelne (GRAY_FORMATS[i], RGB_FORMATS[j]);
    }
  }
}

GST_END_TEST;

static Suite *
d3d11convert_suite (void)
{
  Suite *s = suite_create ("d3d11convert");
  TCase *tc_basic = tcase_create ("general");

  run_visual_test = g_getenv ("ENABLE_D3D11_VISUAL_TEST");

  suite_add_tcase (s, tc_basic);
  tcase_add_test (tc_basic, test_d3d11_convert_rgba_reorder);

  /* XXX: Some methods for device's capability checking and initialization
   * are plugin internal. Enable conversion tests only when it's enabled */
  if (g_getenv ("ENABLE_D3D11_CONVERSION_TEST")) {
    tcase_add_test (tc_basic, test_d3d11_convert_yuv_yuv);
    tcase_add_test (tc_basic, test_d3d11_convert_yuv_rgb);
    tcase_add_test (tc_basic, test_d3d11_convert_yuv_gray);
    tcase_add_test (tc_basic, test_d3d11_convert_rgb_yuv);
    tcase_add_test (tc_basic, test_d3d11_convert_rgb_rgb);
    tcase_add_test (tc_basic, test_d3d11_convert_rgb_gray);
    tcase_add_test (tc_basic, test_d3d11_convert_packed_yuv_yuv);
    tcase_add_test (tc_basic, test_d3d11_convert_packed_yuv_rgb);
    tcase_add_test (tc_basic, test_d3d11_convert_packed_yuv_gray);
    tcase_add_test (tc_basic, test_d3d11_convert_gray_yuv);
    tcase_add_test (tc_basic, test_d3d11_convert_gray_rgb);
  }

  return s;
}

GST_CHECK_MAIN (d3d11convert);
