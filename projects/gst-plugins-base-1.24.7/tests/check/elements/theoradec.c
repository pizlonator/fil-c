/* GStreamer
 *
 * Copyright (C) <2021> Michael Olbrich <m.olbrich@pengutronix.de>
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

#include <gst/video/video.h>
#include <gst/check/gstcheck.h>

static GstPadProbeReturn
query_handler (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstQuery *query = GST_PAD_PROBE_INFO_QUERY (info);
  GstBufferPool *pool;
  GstStructure *config;
  GstVideoInfo vinfo;
  GstCaps *caps;

  if (GST_QUERY_TYPE (query) != GST_QUERY_ALLOCATION)
    return GST_PAD_PROBE_OK;

  gst_query_parse_allocation (query, &caps, NULL);
  fail_unless (caps != NULL);

  gst_video_info_init (&vinfo);
  gst_video_info_from_caps (&vinfo, caps);

  pool = gst_video_buffer_pool_new ();
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, vinfo.size, 0, 1);
  gst_buffer_pool_set_config (pool, config);
  /* activate the pool to ensure that gst_buffer_pool_set_config() will
   * fail later */
  gst_buffer_pool_set_active (pool, TRUE);
  gst_query_add_allocation_pool (query, pool, vinfo.size, 0, 1);
  gst_object_unref (pool);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);

  return GST_PAD_PROBE_OK;
}

static void
demux_pad_added_cb (GstElement * dec, GstPad * pad, gpointer user_data)
{
  GstElement *sink = user_data;
  GstPad *sinkpad;

  sinkpad = gst_element_get_static_pad (sink, "sink");
  gst_pad_link (pad, sinkpad);
  gst_object_unref (sinkpad);
}

GST_START_TEST (test_decide_allocation)
{
  GstElement *pipe, *src, *demux, *decode, *sink;
  GstStateChangeReturn sret;
  GstMessage *msg;
  GstPad *pad;
  gchar *path;

  pipe = gst_pipeline_new (NULL);

  src = gst_element_factory_make ("filesrc", NULL);
  fail_unless (src != NULL, "Failed to create filesrc element");

  demux = gst_element_factory_make ("oggdemux", NULL);
  fail_unless (demux != NULL, "Failed to create oggdemux element");

  decode = gst_element_factory_make ("theoradec", NULL);
  fail_unless (decode != NULL, "Failed to create theoradec element");

  sink = gst_element_factory_make ("fakesink", NULL);
  fail_unless (sink != NULL, "Failed to create fakesink element");

  gst_bin_add_many (GST_BIN (pipe), src, demux, decode, sink, NULL);
  gst_element_link (src, demux);
  gst_element_link (decode, sink);

  path = g_build_filename (GST_TEST_FILES_PATH, "theora.ogg", NULL);
  g_object_set (src, "location", path, NULL);
  g_free (path);

  g_signal_connect (demux, "pad-added",
      G_CALLBACK (demux_pad_added_cb), decode);

  pad = gst_element_get_static_pad (decode, "src");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM,
      query_handler, NULL, NULL);
  gst_object_unref (pad);

  sret = gst_element_set_state (pipe, GST_STATE_PLAYING);
  fail_unless_equals_int (sret, GST_STATE_CHANGE_ASYNC);

  /* wait for EOS or error */
  msg = gst_bus_timed_pop_filtered (GST_ELEMENT_BUS (pipe),
      GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
  fail_unless (msg != NULL);
  fail_unless (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_EOS);
  gst_message_unref (msg);

  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);
}

GST_END_TEST;


static Suite *
theoradec_suite (void)
{
  Suite *s = suite_create ("theoradec");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_decide_allocation);

  return s;
}

GST_CHECK_MAIN (theoradec);
