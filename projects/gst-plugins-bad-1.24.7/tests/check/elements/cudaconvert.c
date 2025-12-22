/* GStreamer
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

#include <gst/check/gstcheck.h>

static const gchar *run_visual_test = NULL;

static const gchar *YUV_FORMATS[] = {
  "I420", "YV12", "NV12", "NV21", "P010_10LE", "P016_LE", "I420_10LE", "Y444",
  "Y444_16LE", "Y42B", "I422_10LE", "I422_12LE",
};

static const gchar *RGB_FORMATS[] = {
  "BGRA", "RGBA", "RGBx", "BGRx", "ARGB", "ABGR", "RGB", "BGR", "BGR10A2_LE",
  "RGB10A2_LE", "RGBP", "BGRP", "GBR", "GBRA",
};

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
      "video/x-raw,format=%s,width=128,height=64,framerate=3/1,"
      "pixel-aspect-ratio=1/1 ! cudaupload ! "
      "cudaconvertscale ! cudadownload ! "
      "video/x-raw,format=%s,width=320,height=240,pixel-aspect-ratio=1/1 ! "
      "videoconvert ! %s", in_format, out_format,
      run_visual_test ? "autovideosink" : "fakesink");
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

GST_START_TEST (test_convert_yuv_yuv)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (YUV_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (YUV_FORMATS); j++) {
      gst_println ("run conversion %s to %s", YUV_FORMATS[i], YUV_FORMATS[j]);
      run_convert_pipelne (YUV_FORMATS[i], YUV_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_convert_yuv_rgb)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (YUV_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (RGB_FORMATS); j++) {
      gst_println ("run conversion %s to %s", YUV_FORMATS[i], RGB_FORMATS[j]);
      run_convert_pipelne (YUV_FORMATS[i], RGB_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_convert_rgb_yuv)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (RGB_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (YUV_FORMATS); j++) {
      gst_println ("run conversion %s to %s", RGB_FORMATS[i], YUV_FORMATS[j]);
      run_convert_pipelne (RGB_FORMATS[i], YUV_FORMATS[j]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_convert_rgb_rgb)
{
  gint i, j;

  for (i = 0; i < G_N_ELEMENTS (RGB_FORMATS); i++) {
    for (j = 0; j < G_N_ELEMENTS (RGB_FORMATS); j++) {
      gst_println ("run conversion %s to %s", RGB_FORMATS[i], RGB_FORMATS[j]);
      run_convert_pipelne (RGB_FORMATS[i], RGB_FORMATS[j]);
    }
  }
}

GST_END_TEST;

static gboolean
check_cuda_convert_available (void)
{
  gboolean ret = TRUE;
  GstElement *upload;

  upload = gst_element_factory_make ("cudaconvertscale", NULL);
  if (!upload) {
    GST_WARNING ("cudaconvertscale is not available");
    return FALSE;
  }

  gst_object_unref (upload);

  return ret;
}

static Suite *
cudaconvertscale_suite (void)
{
  Suite *s;
  TCase *tc_chain;

  /* HACK: cuda device init/deinit with fork seems to problematic */
  g_setenv ("CK_FORK", "no", TRUE);

  run_visual_test = g_getenv ("ENABLE_CUDA_VISUAL_TEST");

  s = suite_create ("cudaconvertscale");
  tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  if (!check_cuda_convert_available ()) {
    gst_println ("Skip convertscale test since cannot open device");
    goto end;
  }

  /* Only run test if explicitly enabled */
  if (g_getenv ("ENABLE_CUDA_CONVERSION_TEST")) {
    tcase_add_test (tc_chain, test_convert_yuv_yuv);
    tcase_add_test (tc_chain, test_convert_yuv_rgb);
    tcase_add_test (tc_chain, test_convert_rgb_yuv);
    tcase_add_test (tc_chain, test_convert_rgb_rgb);
  }

end:
  return s;
}

GST_CHECK_MAIN (cudaconvertscale);
