/*
 *  vacompositor.c - GStreamer unit test for the vacompositor element
 *
 *  Copyright (C) 2022 Intel Corporation
 *    Author: U. Artie Eoff <ullysses.a.eoff@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/video/video.h>

static GMainLoop *main_loop;

static void
message_received (GstBus * bus, GstMessage * message, GstPipeline * bin)
{
  GST_INFO ("bus message from \"%" GST_PTR_FORMAT "\": %" GST_PTR_FORMAT,
      GST_MESSAGE_SRC (message), message);

  switch (message->type) {
    case GST_MESSAGE_EOS:
      g_main_loop_quit (main_loop);
      break;
    case GST_MESSAGE_WARNING:{
      GError *gerror;
      gchar *debug;

      gst_message_parse_warning (message, &gerror, &debug);
      gst_object_default_error (GST_MESSAGE_SRC (message), gerror, debug);
      g_error_free (gerror);
      g_free (debug);
      break;
    }
    case GST_MESSAGE_ERROR:{
      GError *gerror;
      gchar *debug;

      gst_message_parse_error (message, &gerror, &debug);
      gst_object_default_error (GST_MESSAGE_SRC (message), gerror, debug);
      g_error_free (gerror);
      g_free (debug);
      g_main_loop_quit (main_loop);
      break;
    }
    default:
      break;
  }
}

static GstBuffer *handoff_buffer = NULL;
static void
on_handoff (GstElement * element, GstBuffer * buffer, GstPad * pad,
    gpointer data)
{
  gst_buffer_replace (&handoff_buffer, buffer);
}

#define TEST_PATTERN_RED 4
#define TEST_PATTERN_GREEN 5

GST_START_TEST (test_composition_position)
{
  GstElement *bin, *src1, *filter1, *src2, *filter2, *comp, *sink;
  GstBus *bus;
  GstPad *pad, *srcpad, *sinkpad;
  GstCaps *caps;
  GstVideoFrame frame;
  GstVideoInfo vinfo;

  /* Check if vacompositor is available, since it is only available
   * for iHD vaapi driver */
  comp = gst_element_factory_make ("vacompositor", "compositor");
  if (!comp)
    return;

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src1 = gst_element_factory_make ("videotestsrc", "src1");
  g_object_set (src1, "num-buffers", 1, NULL);
  g_object_set (src1, "pattern", TEST_PATTERN_GREEN, NULL);
  filter1 = gst_element_factory_make ("capsfilter", "filter1");
  caps = gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING, "NV12",
      "width", G_TYPE_INT, 320, "height", G_TYPE_INT, 240, NULL);
  g_object_set (filter1, "caps", caps, NULL);
  gst_caps_unref (caps);

  src2 = gst_element_factory_make ("videotestsrc", "src2");
  g_object_set (src2, "num-buffers", 1, NULL);
  g_object_set (src2, "pattern", TEST_PATTERN_RED, NULL);
  filter2 = gst_element_factory_make ("capsfilter", "filter2");
  caps = gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING, "NV12",
      "width", G_TYPE_INT, 20, "height", G_TYPE_INT, 20, NULL);
  g_object_set (filter2, "caps", caps, NULL);
  gst_caps_unref (caps);

  sink = gst_element_factory_make ("fakesink", "sink");
  g_object_set (sink, "signal-handoffs", TRUE, NULL);
  g_signal_connect (sink, "handoff", G_CALLBACK (on_handoff), NULL);

  gst_bin_add_many (GST_BIN (bin), src1, filter1, src2, filter2, comp,
      sink, NULL);
  gst_element_link (src1, filter1);
  gst_element_link (src2, filter2);
  gst_element_link (comp, sink);

  srcpad = gst_element_get_static_pad (filter1, "src");
  sinkpad = gst_element_request_pad_simple (comp, "sink_0");
  g_object_set (sinkpad, "xpos", 0, "ypos", 0, "alpha", 1.0, NULL);
  gst_pad_link (srcpad, sinkpad);
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  srcpad = gst_element_get_static_pad (filter2, "src");
  sinkpad = gst_element_request_pad_simple (comp, "sink_1");
  g_object_set (sinkpad, "xpos", 10, "ypos", 10, "alpha", 1.0, NULL);
  gst_pad_link (srcpad, sinkpad);
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  /* setup and run the main loop */
  main_loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);
  gst_element_set_state (bin, GST_STATE_PAUSED);
  gst_element_get_state (bin, NULL, NULL, GST_CLOCK_TIME_NONE);
  gst_element_set_state (bin, GST_STATE_PLAYING);
  g_main_loop_run (main_loop);

  /* validate output buffer */
  fail_unless (handoff_buffer != NULL);
  pad = gst_element_get_static_pad (sink, "sink");
  caps = gst_pad_get_current_caps (pad);
  gst_video_info_from_caps (&vinfo, caps);
  gst_caps_unref (caps);
  gst_object_unref (pad);

  gst_video_frame_map (&frame, &vinfo, handoff_buffer, GST_MAP_READ);
  {
    guint i, j, n_planes, plane;
    n_planes = GST_VIDEO_FRAME_N_PLANES (&frame);

    for (plane = 0; plane < n_planes; plane++) {
      guint8 *pd = GST_VIDEO_FRAME_PLANE_DATA (&frame, plane);
      gint w = GST_VIDEO_FRAME_COMP_WIDTH (&frame, plane)
          * GST_VIDEO_FRAME_COMP_PSTRIDE (&frame, plane);
      gint h = GST_VIDEO_FRAME_COMP_HEIGHT (&frame, plane);
      gint ps = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, plane);

      for (j = 0; j < h; ++j) {
        for (i = 0; i < w; ++i) {
          guint8 actual = GST_READ_UINT8 (pd + i);
          guint8 expect = 0xff;
          if (plane == 0) {
            if (i >= 10 && i < 30 && j >= 10 && j < 30)
              expect = 0x51;
            else
              expect = 0x91;
          } else {
            if (i >= 10 && i < 30 && j >= 5 && j < 15)
              expect = (i % 2) ? 0xf0 : 0x5a;
            else
              expect = (i % 2) ? 0x22 : 0x36;
          }
          fail_unless (actual == expect,
              "Expected 0x%02x but got 0x%02x at (%u,%u,%u)", expect, actual,
              plane, i, j);
        }
        pd += ps;
      }
    }
  }
  gst_video_frame_unmap (&frame);

  /* cleanup */
  gst_buffer_replace (&handoff_buffer, NULL);
  gst_element_set_state (bin, GST_STATE_NULL);
  g_main_loop_unref (main_loop);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

static Suite *
vacompositor_suite (void)
{
  Suite *s = suite_create ("vacompositor");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_composition_position);

  return s;
}

GST_CHECK_MAIN (vacompositor);
