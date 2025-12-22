/* GStreamer
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#include <string.h>

#include <gst/check/gstcheck.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>

static gboolean
bus_handler (GstBus * bus, GstMessage * message, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (message->type) {
    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_WARNING:
    case GST_MESSAGE_ERROR:{
      GError *gerror;
      gchar *debug;

      if (message->type == GST_MESSAGE_WARNING)
        gst_message_parse_warning (message, &gerror, &debug);
      else
        gst_message_parse_error (message, &gerror, &debug);
      gst_object_default_error (GST_MESSAGE_SRC (message), gerror, debug);
      gst_message_unref (message);
      g_error_free (gerror);
      g_free (debug);
      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

typedef struct
{
  GstClockTime ts;
  GstClockTime duration;
  const gchar buf[];
} TestBuffer;

static const TestBuffer buf0 = {
  0,
  0,
  {"[Script Info]\n"
        "; This is a Sub Station Alpha v4 script.\n"
        "; For Sub Station Alpha info and downloads,\n"
        "; go to http://www.eswat.demon.co.uk/\n"
        "Title: Some Test\n"
        "Script Updated By: version 2.8.01\n"
        "ScriptType: v4.00\n"
        "Collisions: Normal\n"
        "PlayResY: 200\n"
        "PlayDepth: 0\n"
        "Timer: 100,0000\n"
        " \n"
        "[V4 Styles]\n"
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, TertiaryColour, BackColour, Bold, Italic, BorderStyle, Outline, Shadow, \n"
        "   Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding\n"
        "Style: DefaultVCD, Arial,28,11861244,11861244,11861244,-2147483640,-1,0,1,1,2,2,30,30,30,0,0\n"
        " \n"
        "[Events]\n"
        "Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text"}
};

static const TestBuffer buf1 = {
  40 * GST_MSECOND,
  60 * GST_MSECOND,
  {"1,,DefaultVCD, NTP,0000,0000,0000,,Some Test Blabla"}
};

static void
sink_handoff_cb_xRGB (GstElement * object, GstBuffer * buffer, GstPad * pad,
    gpointer user_data)
{
  guint *sink_pos = (guint *) user_data;
  gboolean contains_text = (*sink_pos == 1 || *sink_pos == 2);
  guint i, j;
  GstMapInfo map;
  gboolean all_red = TRUE;

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  fail_unless_equals_int (map.size, 640 * 480 * 4);

  for (i = 0; i < 640; i++) {
    for (j = 0; j < 480; j++) {
      all_red = all_red && (map.data[i * 480 * 4 + j * 4 + 1] == 255 &&
          map.data[i * 480 * 4 + j * 4 + 2] == 0 &&
          map.data[i * 480 * 4 + j * 4 + 3] == 0);
    }
  }
  gst_buffer_unmap (buffer, &map);

  fail_unless (contains_text != all_red,
      "Frame %d is incorrect (all red %d, contains text %d)", *sink_pos,
      all_red, contains_text);
  *sink_pos = *sink_pos + 1;
}

static void
sink_handoff_cb_I420 (GstElement * object, GstBuffer * buffer, GstPad * pad,
    gpointer user_data)
{
  guint *sink_pos = (guint *) user_data;
  gboolean contains_text = (*sink_pos == 1 || *sink_pos == 2);
  guint c, i, j;
  gboolean all_red = TRUE;
  guint8 *comp;
  gint comp_stride, comp_width, comp_height;
  const guint8 color[] = { 81, 90, 240 };
  GstVideoInfo info;
  GstVideoFrame frame;

  gst_video_info_init (&info);
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_I420, 640, 480);

  gst_video_frame_map (&frame, &info, buffer, GST_MAP_READ);

  for (c = 0; c < 3; c++) {
    comp = GST_VIDEO_FRAME_COMP_DATA (&frame, c);
    comp_stride = GST_VIDEO_FRAME_COMP_STRIDE (&frame, c);
    comp_width = GST_VIDEO_FRAME_COMP_WIDTH (&frame, c);
    comp_height = GST_VIDEO_FRAME_COMP_HEIGHT (&frame, c);

    for (i = 0; i < comp_height; i++) {
      for (j = 0; j < comp_width; j++) {
        all_red = all_red && (comp[i * comp_stride + j] == color[c]);
      }
    }
  }
  gst_video_frame_unmap (&frame);

  fail_unless (contains_text != all_red,
      "Frame %d is incorrect (all red %d, contains text %d)", *sink_pos,
      all_red, contains_text);
  *sink_pos = *sink_pos + 1;
}

static gulong probe_id = 0;

static GstPadProbeReturn
src_buffer_probe_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);
  GstPad *otherpad = GST_PAD (user_data);

  if (GST_BUFFER_TIMESTAMP (buffer) == buf1.ts)
    gst_pad_remove_probe (otherpad, probe_id);

  return GST_PAD_PROBE_OK;
}

#define CREATE_BASIC_TEST(format) \
GST_START_TEST (test_assrender_basic_##format) \
{ \
  GstElement *pipeline; \
  GstElement *appsrc, *videotestsrc, *capsfilter, *assrender, *fakesink; \
  guint sink_pos = 0; \
  GstCaps *video_caps; \
  GstCaps *text_caps; \
  GstBuffer *buf; \
  GstBus *bus; \
  GMainLoop *loop; \
  GstPad *pad, *blocked_pad; \
  guint bus_watch = 0; \
  GstVideoInfo info; \
  \
  pipeline = gst_pipeline_new ("pipeline"); \
  fail_unless (pipeline != NULL); \
  \
  capsfilter = gst_element_factory_make ("capsfilter", NULL); \
  fail_unless (capsfilter != NULL); \
  gst_video_info_init (&info); \
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_##format, 640, 480); \
  info.fps_n = 25; \
  info.fps_d = 1; \
  video_caps = gst_video_info_to_caps (&info); \
  g_object_set (capsfilter, "caps", video_caps, NULL); \
  gst_caps_unref (video_caps); \
  blocked_pad = gst_element_get_static_pad (capsfilter, "src"); \
  gst_pad_add_probe (blocked_pad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM, NULL, NULL, NULL); \
  \
  appsrc = gst_element_factory_make ("appsrc", NULL); \
  fail_unless (appsrc != NULL); \
  buf = gst_buffer_new_and_alloc (strlen (buf0.buf) + 1); \
  gst_buffer_fill (buf, 0, buf0.buf, strlen (buf0.buf) + 1); \
  GST_BUFFER_TIMESTAMP (buf) = buf0.ts; \
  GST_BUFFER_DURATION (buf) = buf0.duration; \
  text_caps = \
      gst_caps_new_simple ("application/x-ssa", "codec_data", GST_TYPE_BUFFER, \
      buf, NULL); \
  gst_buffer_unref (buf); \
  gst_app_src_set_caps (GST_APP_SRC (appsrc), text_caps); \
  g_object_set (appsrc, "format", GST_FORMAT_TIME, NULL); \
  pad = gst_element_get_static_pad (appsrc, "src"); \
  probe_id = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, src_buffer_probe_cb, \
      gst_object_ref (blocked_pad), (GDestroyNotify) gst_object_unref); \
  gst_object_unref (blocked_pad); \
  gst_object_unref (pad); \
  \
  videotestsrc = gst_element_factory_make ("videotestsrc", NULL); \
  fail_unless (videotestsrc != NULL); \
  g_object_set (videotestsrc, "num-buffers", 5, "pattern", 4, NULL); \
  \
  assrender = gst_element_factory_make ("assrender", NULL); \
  fail_unless (assrender != NULL); \
  \
  fakesink = gst_element_factory_make ("fakesink", NULL); \
  fail_unless (fakesink != NULL); \
  g_object_set (fakesink, "signal-handoffs", TRUE, "async", FALSE, NULL); \
  g_signal_connect (fakesink, "handoff", G_CALLBACK (sink_handoff_cb_##format), \
      &sink_pos); \
  \
  gst_bin_add_many (GST_BIN (pipeline), appsrc, videotestsrc, capsfilter, \
      assrender, fakesink, NULL); \
  \
  fail_unless (gst_element_link_pads (appsrc, "src", assrender, "text_sink")); \
  fail_unless (gst_element_link_pads (videotestsrc, "src", capsfilter, "sink")); \
  fail_unless (gst_element_link_pads (capsfilter, "src", assrender, \
          "video_sink")); \
  fail_unless (gst_element_link_pads (assrender, "src", fakesink, "sink")); \
  \
  loop = g_main_loop_new (NULL, TRUE); \
  fail_unless (loop != NULL); \
  \
  bus = gst_element_get_bus (pipeline); \
  fail_unless (bus != NULL); \
  bus_watch = gst_bus_add_watch (bus, bus_handler, loop); \
  gst_object_unref (bus); \
  \
  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING), \
      GST_STATE_CHANGE_SUCCESS); \
  \
  buf = gst_buffer_new_and_alloc (strlen (buf1.buf) + 1); \
  gst_buffer_fill (buf, 0, buf1.buf, strlen (buf1.buf) + 1); \
  GST_BUFFER_TIMESTAMP (buf) = buf1.ts; \
  GST_BUFFER_DURATION (buf) = buf1.duration; \
  gst_app_src_push_buffer (GST_APP_SRC (appsrc), buf); \
  gst_caps_unref (text_caps); \
  gst_app_src_end_of_stream (GST_APP_SRC (appsrc)); \
  \
  g_main_loop_run (loop); \
  \
  gst_element_set_state (pipeline, GST_STATE_NULL); \
  \
  fail_unless_equals_int (sink_pos, 5); \
  \
  g_object_unref (pipeline); \
  g_main_loop_unref (loop); \
  g_source_remove (bus_watch); \
} \
\
GST_END_TEST;

CREATE_BASIC_TEST (xRGB);
CREATE_BASIC_TEST (I420);

static Suite *
assrender_suite (void)
{
  Suite *s = suite_create ("assrender");
  TCase *tc_chain = tcase_create ("linear");

  /* time out after 120s, not the default 3 */
  tcase_set_timeout (tc_chain, 120);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_assrender_basic_xRGB);
  tcase_add_test (tc_chain, test_assrender_basic_I420);

  return s;
}

GST_CHECK_MAIN (assrender);
