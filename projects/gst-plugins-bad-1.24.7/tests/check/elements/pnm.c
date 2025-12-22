/* GStreamer
 *
 * unit test for PNM encoder / decoder
 *
 * Copyright (C) <2016> Jan Schmidt <jan@centricular.com>
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
#include <gst/app/gstappsink.h>

/* Create a pnmenc ! pnmdec and push in 
 * frames, checking that what comes out is what
 * went in */
GST_START_TEST (test_pnm_enc_dec)
{
  GstElement *pipeline;
  GstElement *incf, *outcf, *enc;
  GstElement *sink;
  GstSample *sample;
  GstBuffer *buffer;
  gint i, n;

  struct
  {
    const gchar *in_fmt;
    const gchar *out_fmt;
  } test_formats[] = {
    {
        "RGB", "RGB"}, {
        "GRAY8", "GRAY8"}, {
        "GRAY16_BE", "GRAY16_BE"}, {
        "GRAY16_BE", "GRAY16_LE"}, {
        "GRAY16_LE", "GRAY16_BE"}, {
        "GRAY16_LE", "GRAY16_LE"}
  };

  pipeline =
      gst_parse_launch
      ("videotestsrc num-buffers=1 ! capsfilter name=incf ! pnmenc name=enc ! pnmdec ! capsfilter name=outcf ! appsink name=sink",
      NULL);
  g_assert (pipeline != NULL);

  incf = gst_bin_get_by_name (GST_BIN (pipeline), "incf");
  enc = gst_bin_get_by_name (GST_BIN (pipeline), "enc");
  outcf = gst_bin_get_by_name (GST_BIN (pipeline), "outcf");
  sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");

  for (n = 0; n < 2; n++) {
    for (i = 0; i < G_N_ELEMENTS (test_formats); i++) {
      GstCaps *incaps = gst_caps_new_simple ("video/x-raw", "width", G_TYPE_INT,
          320, "height", G_TYPE_INT, 240, "framerate",
          GST_TYPE_FRACTION, 1, 1, "format", G_TYPE_STRING,
          test_formats[i].in_fmt, NULL);
      GstCaps *outcaps =
          gst_caps_new_simple ("video/x-raw", "width", G_TYPE_INT,
          320, "height", G_TYPE_INT, 240, "framerate",
          GST_TYPE_FRACTION, 1, 1, "format", G_TYPE_STRING,
          test_formats[i].out_fmt, NULL);

      GST_DEBUG ("Setting in caps %" GST_PTR_FORMAT, incaps);
      g_object_set (G_OBJECT (incf), "caps", incaps, NULL);
      GST_DEBUG ("Setting out caps %" GST_PTR_FORMAT, outcaps);
      g_object_set (G_OBJECT (outcf), "caps", outcaps, NULL);

      gst_caps_unref (incaps);
      gst_caps_unref (outcaps);

      gst_element_set_state (pipeline, GST_STATE_PLAYING);

      sample = gst_app_sink_pull_sample (GST_APP_SINK (sink));

      fail_unless (sample != NULL);
      buffer = gst_sample_get_buffer (sample);
      fail_unless (buffer != NULL);
      gst_sample_unref (sample);

      gst_element_set_state (pipeline, GST_STATE_NULL);
    }

    g_object_set (enc, "ascii", TRUE, NULL);
  }

  gst_object_unref (pipeline);
  gst_object_unref (sink);
  gst_object_unref (outcf);
  gst_object_unref (enc);
  gst_object_unref (incf);

}

GST_END_TEST;

static Suite *
pnm_suite (void)
{
  Suite *s = suite_create ("pnm");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_pnm_enc_dec);

  return s;
}

GST_CHECK_MAIN (pnm);
