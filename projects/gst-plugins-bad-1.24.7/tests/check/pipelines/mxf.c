/* GStreamer
 *
 * unit test for mxfmux ! mxfdemux pipelines
 *
 * Copyright (C) <2009> Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
#include <string.h>

static const gchar *
get_mpeg2enc_element_name (void)
{
  GstElementFactory *factory = NULL;

  if ((factory = gst_element_factory_find ("mpeg2enc"))) {
    gst_object_unref (factory);
    return "mpeg2enc";
  } else if ((factory = gst_element_factory_find ("avenc_mpeg2video"))) {
    gst_object_unref (factory);
    return "avenc_mpeg2video";
  } else {
    return NULL;
  }
}

typedef struct
{
  GMainLoop *loop;
  gboolean eos;
} OnMessageUserData;

static void
on_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  OnMessageUserData *d = user_data;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
    case GST_MESSAGE_WARNING:
      g_assert_not_reached ();
      break;
    case GST_MESSAGE_EOS:
      g_main_loop_quit (d->loop);
      d->eos = TRUE;
      break;
    default:
      break;
  }
}

static void
on_pad_added (GstElement * element, GstPad * pad, gpointer user_data)
{
  gint *n_pads = user_data;

  *n_pads = *n_pads + 1;
}

static void
run_test (const gchar * pipeline_string, gint n_pads_expected)
{
  GstElement *pipeline;
  GstBus *bus;
  GMainLoop *loop;
  OnMessageUserData omud = { NULL, };
  GstStateChangeReturn ret;
  GstElement *demux;
  gint n_pads = 0;

  GST_DEBUG ("Testing pipeline '%s'", pipeline_string);

  pipeline = gst_parse_launch (pipeline_string, NULL);
  fail_unless (pipeline != NULL);
  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);

  demux = gst_bin_get_by_name (GST_BIN (pipeline), "demux");
  fail_unless (demux != NULL);
  g_signal_connect (demux, "pad-added", (GCallback) on_pad_added, &n_pads);
  gst_object_unref (demux);

  loop = g_main_loop_new (NULL, FALSE);

  bus = gst_element_get_bus (pipeline);
  fail_unless (bus != NULL);
  gst_bus_add_signal_watch (bus);

  omud.loop = loop;
  omud.eos = FALSE;

  g_signal_connect (bus, "message", (GCallback) on_message_cb, &omud);

  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  fail_unless (ret == GST_STATE_CHANGE_SUCCESS
      || ret == GST_STATE_CHANGE_ASYNC);

  g_main_loop_run (loop);

  fail_unless (gst_element_set_state (pipeline,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS);

  fail_unless (omud.eos == TRUE);
  fail_unless_equals_int (n_pads, n_pads_expected);

  gst_object_unref (pipeline);
  g_main_loop_unref (loop);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
}

GST_START_TEST (test_mpeg2)
{
  const gchar *mpeg2enc_name = get_mpeg2enc_element_name ();
  gchar *pipeline;

  if (!mpeg2enc_name)
    return;

  pipeline = g_strdup_printf ("videotestsrc num-buffers=250 ! "
      "video/x-raw,framerate=25/1 ! "
      "%s ! " "mxfmux name=mux ! "
      "mxfdemux name=demux ! " "fakesink", mpeg2enc_name);

  run_test (pipeline, 1);
  g_free (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_raw_video_raw_audio)
{
  gchar *pipeline;

  pipeline = g_strdup_printf ("videotestsrc num-buffers=250 ! "
      "video/x-raw,format=(string)v308,width=1920,height=1080,framerate=25/1 ! "
      "mxfmux name=mux ! "
      "mxfdemux name=demux ! "
      "fakesink  "
      "audiotestsrc num-buffers=250 ! "
      "audioconvert ! " "audio/x-raw,rate=48000,channels=2 ! " "mux. ");

  run_test (pipeline, 2);
  g_free (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_raw_video_stride_transform)
{
  gchar *pipeline;

  pipeline = g_strdup_printf ("videotestsrc num-buffers=250 ! "
      "video/x-raw,format=(string)v308,width=1001,height=501,framerate=25/1 ! "
      "mxfmux name=mux ! " "mxfdemux name=demux ! " "fakesink");

  run_test (pipeline, 1);
  g_free (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_jpeg2000_alaw)
{
  gchar *pipeline;
  GstElementFactory *factory = NULL;

  if ((factory = gst_element_factory_find ("openjpegenc")) == NULL)
    return;
  gst_object_unref (factory);
  if ((factory = gst_element_factory_find ("alawenc")) == NULL)
    return;
  gst_object_unref (factory);

  pipeline = g_strdup_printf ("videotestsrc num-buffers=250 ! "
      "video/x-raw,framerate=25/1 ! "
      "openjpegenc ! "
      "mxfmux name=mux ! "
      "mxfdemux name=demux ! "
      "fakesink  "
      "audiotestsrc num-buffers=250 ! " "audioconvert ! " "alawenc ! " "mux. ");

  run_test (pipeline, 2);
  g_free (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_dnxhd_mp3)
{
  gchar *pipeline;
  GstElementFactory *factory = NULL;

  if ((factory = gst_element_factory_find ("avenc_dnxhd")) == NULL)
    return;
  gst_object_unref (factory);
  if ((factory = gst_element_factory_find ("lamemp3enc")) == NULL)
    return;
  gst_object_unref (factory);
  if ((factory = gst_element_factory_find ("mpegaudioparse")) == NULL)
    return;
  gst_object_unref (factory);

  pipeline = g_strdup_printf ("videotestsrc num-buffers=250 ! "
      "video/x-raw,format=(string)Y42B,width=1920,height=1080,framerate=25/1 ! "
      "avenc_dnxhd bitrate=36000000 ! "
      "mxfmux name=mux ! "
      "mxfdemux name=demux ! "
      "fakesink  "
      "audiotestsrc num-buffers=250 ! "
      "audioconvert ! "
      "audio/x-raw,channels=2 ! lamemp3enc ! mpegaudioparse ! mux. ");

  run_test (pipeline, 2);
  g_free (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_multiple_av_streams)
{
  gchar *pipeline;

  pipeline = g_strdup_printf ("videotestsrc num-buffers=250 ! "
      "video/x-raw,format=(string)v308,width=1920,height=1080,framerate=25/1 ! "
      "mxfmux name=mux ! "
      "mxfdemux name=demux ! "
      "fakesink  "
      "audiotestsrc num-buffers=250 ! "
      "audioconvert ! "
      "audio/x-raw,rate=48000,channels=2 ! "
      "mux. "
      "videotestsrc num-buffers=100 ! "
      "video/x-raw,format=(string)v308,width=1920,height=1080,framerate=25/1 ! "
      "mux. "
      "audiotestsrc num-buffers=100 ! "
      "audioconvert ! "
      "audio/x-raw,rate=48000,channels=2 ! "
      "mux. "
      "audiotestsrc num-buffers=250 ! "
      "audioconvert ! " "audio/x-raw,rate=48000,channels=2 ! " "mux. ");

  run_test (pipeline, 5);
  g_free (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_h264_raw_audio)
{
  gchar *pipeline;
  GstElementFactory *factory = NULL;

  if ((factory = gst_element_factory_find ("x264enc")) == NULL)
    return;
  gst_object_unref (factory);
  if ((factory = gst_element_factory_find ("h264parse")) == NULL)
    return;
  gst_object_unref (factory);

  pipeline = g_strdup_printf ("videotestsrc num-buffers=250 ! "
      "video/x-raw,framerate=25/1 ! "
      "x264enc ! h264parse ! "
      "mxfmux name=mux ! "
      "mxfdemux name=demux ! "
      "fakesink  "
      "audiotestsrc num-buffers=250 ! "
      "audioconvert ! " "audio/x-raw,format=S24LE,channels=2 ! mux. ");

  run_test (pipeline, 2);
  g_free (pipeline);
}

GST_END_TEST;

static Suite *
mxf_suite (void)
{
  Suite *s = suite_create ("mxf");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_set_timeout (tc_chain, 180);

  tcase_add_test (tc_chain, test_mpeg2);
  tcase_add_test (tc_chain, test_raw_video_raw_audio);
  tcase_add_test (tc_chain, test_raw_video_stride_transform);
  tcase_add_test (tc_chain, test_jpeg2000_alaw);
  tcase_add_test (tc_chain, test_dnxhd_mp3);
  tcase_add_test (tc_chain, test_h264_raw_audio);
  tcase_add_test (tc_chain, test_multiple_av_streams);

  return s;
}

GST_CHECK_MAIN (mxf);
