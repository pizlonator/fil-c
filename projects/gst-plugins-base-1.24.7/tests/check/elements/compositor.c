/* GStreamer
 *
 * unit test for compositor
 *
 * Copyright (C) <2005> Thomas Vander Stichele <thomas at apestaart dot org>
 * Copyright (C) <2013> Thibault Saunier <thibault.saunier@collabora.com>
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
# include <config.h>
#endif

#ifdef HAVE_VALGRIND
# include <valgrind/valgrind.h>
#endif

#include <gst/check/gstcheck.h>
#include <gst/check/gstconsistencychecker.h>
#include <gst/check/gstharness.h>
#include <gst/video/gstvideometa.h>
#include <gst/base/gstbasesrc.h>

#define VIDEO_CAPS_STRING               \
    "video/x-raw, "                 \
    "width = (int) 320, "               \
    "height = (int) 240, "              \
    "framerate = (fraction) 25/1 , "    \
    "format = (string) I420"

static GMainLoop *main_loop;

static GstCaps *
_compositor_get_all_supported_caps (void)
{
  return gst_caps_from_string (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL));
}

static GstCaps *
_compositor_get_non_alpha_supported_caps (void)
{
  gint j;
  GValue all_formats = G_VALUE_INIT;
  GValue nonalpha_formats = G_VALUE_INIT;
  GstCaps *all_caps = _compositor_get_all_supported_caps ();

  g_value_init (&all_formats, GST_TYPE_LIST);
  g_value_init (&nonalpha_formats, GST_TYPE_LIST);
  gst_value_deserialize (&all_formats, GST_VIDEO_FORMATS_ALL);

  for (j = 0; j < gst_value_list_get_size (&all_formats); j++) {
    const GValue *v1 = gst_value_list_get_value (&all_formats, j);
    GstVideoFormat f = gst_video_format_from_string (g_value_get_string (v1));
    GstVideoFormatInfo *format_info =
        (GstVideoFormatInfo *) gst_video_format_get_info (f);
    if (!GST_VIDEO_FORMAT_INFO_HAS_ALPHA (format_info))
      gst_value_list_append_value (&nonalpha_formats, v1);
  }

  gst_structure_set_value (gst_caps_get_structure (all_caps, 0), "format",
      &nonalpha_formats);

  g_value_unset (&all_formats);
  g_value_unset (&nonalpha_formats);

  return all_caps;
}

/* make sure downstream gets a CAPS event before buffers are sent */
GST_START_TEST (test_caps)
{
  GstElement *pipeline, *src, *compositor, *sink;
  GstStateChangeReturn state_res;
  GstCaps *caps;
  GstPad *pad;

  /* build pipeline */
  pipeline = gst_pipeline_new ("pipeline");

  src = gst_element_factory_make ("videotestsrc", "src1");
  compositor = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (pipeline), src, compositor, sink, NULL);

  fail_unless (gst_element_link_many (src, compositor, sink, NULL));

  /* prepare playing */
  state_res = gst_element_set_state (pipeline, GST_STATE_PAUSED);
  fail_unless_equals_int (state_res, GST_STATE_CHANGE_ASYNC);

  /* wait for preroll */
  state_res = gst_element_get_state (pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
  fail_unless_equals_int (state_res, GST_STATE_CHANGE_SUCCESS);

  /* check caps on fakesink */
  pad = gst_element_get_static_pad (sink, "sink");
  caps = gst_pad_get_current_caps (pad);
  fail_unless (caps != NULL);
  gst_caps_unref (caps);
  gst_object_unref (pad);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

GST_END_TEST;

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


static GstFormat format = GST_FORMAT_UNDEFINED;
static gint64 position = -1;

static void
test_event_message_received (GstBus * bus, GstMessage * message,
    GstPipeline * bin)
{
  GST_INFO ("bus message from \"%" GST_PTR_FORMAT "\": %" GST_PTR_FORMAT,
      GST_MESSAGE_SRC (message), message);

  switch (message->type) {
    case GST_MESSAGE_SEGMENT_DONE:
      gst_message_parse_segment_done (message, &format, &position);
      GST_INFO ("received segment_done : %" G_GINT64_FORMAT, position);
      g_main_loop_quit (main_loop);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}


GST_START_TEST (test_event)
{
  GstElement *bin, *src1, *src2, *compositor, *sink;
  GstBus *bus;
  GstEvent *seek_event;
  GstStateChangeReturn state_res;
  gboolean res;
  GstPad *srcpad, *sinkpad;
  GstStreamConsistency *chk_1, *chk_2, *chk_3;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src1 = gst_element_factory_make ("videotestsrc", "src1");
  src2 = gst_element_factory_make ("videotestsrc", "src2");
  compositor = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src1, src2, compositor, sink, NULL);

  res = gst_element_link (src1, compositor);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (src2, compositor);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (compositor, sink);
  fail_unless (res == TRUE, NULL);

  srcpad = gst_element_get_static_pad (compositor, "src");
  chk_3 = gst_consistency_checker_new (srcpad);
  gst_object_unref (srcpad);

  /* create consistency checkers for the pads */
  srcpad = gst_element_get_static_pad (src1, "src");
  chk_1 = gst_consistency_checker_new (srcpad);
  sinkpad = gst_pad_get_peer (srcpad);
  gst_consistency_checker_add_pad (chk_3, sinkpad);
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  srcpad = gst_element_get_static_pad (src2, "src");
  chk_2 = gst_consistency_checker_new (srcpad);
  sinkpad = gst_pad_get_peer (srcpad);
  gst_consistency_checker_add_pad (chk_3, sinkpad);
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  seek_event = gst_event_new_seek (1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_FLUSH,
      GST_SEEK_TYPE_SET, (GstClockTime) 0,
      GST_SEEK_TYPE_SET, (GstClockTime) 2 * GST_SECOND);

  format = GST_FORMAT_UNDEFINED;
  position = -1;

  main_loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (bus, "message::segment-done",
      (GCallback) test_event_message_received, bin);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  GST_INFO ("starting test");

  /* prepare playing */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* wait for completion */
  state_res = gst_element_get_state (bin, NULL, NULL, GST_CLOCK_TIME_NONE);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  res = gst_element_send_event (bin, seek_event);
  fail_unless (res == TRUE, NULL);

  /* run pipeline */
  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  GST_INFO ("running main loop");
  g_main_loop_run (main_loop);

  state_res = gst_element_set_state (bin, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  ck_assert_int_eq (position, 2 * GST_SECOND);

  /* cleanup */
  g_main_loop_unref (main_loop);
  gst_consistency_checker_free (chk_1);
  gst_consistency_checker_free (chk_2);
  gst_consistency_checker_free (chk_3);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

typedef struct _ProbeEvent
{
  gboolean received;
  gdouble x_pos, y_pos;
} ProbeEvent;

static GstPadProbeReturn
probe_nav_event (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstEvent *event = gst_pad_probe_info_get_event (info);
  ProbeEvent *probe_ev = (ProbeEvent *) user_data;

  if (GST_EVENT_TYPE (event) == GST_EVENT_NAVIGATION) {
    probe_ev->received = TRUE;
    gst_navigation_event_parse_mouse_move_event (event,
        &(probe_ev->x_pos), &(probe_ev->y_pos));
  }

  return GST_PAD_PROBE_OK;
}

GST_START_TEST (test_navigation_events)
{
  GstElement *bin, *src1, *src2, *src3, *filter1, *filter2, *filter3;
  GstElement *compositor, *sink;
  GstPad *srcpad, *sinkpad;
  GstCaps *caps;
  gboolean res;
  ProbeEvent probe_events[3];
  GstStateChangeReturn state_res;
  GstEvent *event;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  src1 = gst_element_factory_make ("videotestsrc", "src1");
  src2 = gst_element_factory_make ("videotestsrc", "src2");
  src3 = gst_element_factory_make ("videotestsrc", "src3");
  filter1 = gst_element_factory_make ("capsfilter", "filter1");
  filter2 = gst_element_factory_make ("capsfilter", "filter2");
  filter3 = gst_element_factory_make ("capsfilter", "filter3");
  compositor = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src1, src2, src3, filter1, filter2, filter3,
      compositor, sink, NULL);

  /* configure capsfilters */
  caps = gst_caps_from_string ("video/x-raw,width=800,height=400");
  g_object_set (filter1, "caps", caps, NULL);
  gst_caps_unref (caps);
  caps = gst_caps_from_string ("video/x-raw,width=400,height=200");
  g_object_set (filter2, "caps", caps, NULL);
  gst_caps_unref (caps);
  caps = gst_caps_from_string ("video/x-raw,width=200,height=50");
  g_object_set (filter3, "caps", caps, NULL);
  gst_caps_unref (caps);

  res = gst_element_link_many (src1, filter1, compositor, NULL);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link_many (src2, filter2, compositor, NULL);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link_many (src3, filter3, compositor, NULL);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (compositor, sink);
  fail_unless (res == TRUE, NULL);

  srcpad = gst_element_get_static_pad (compositor, "src");
  gst_object_unref (srcpad);

  /* configure pads and add probes */
  srcpad = gst_element_get_static_pad (filter1, "src");
  sinkpad = gst_pad_get_peer (srcpad);
  g_object_set (sinkpad,
      "width", 400, "height", 300, "xpos", 200, "ypos", 100, NULL);
  probe_events[0].received = FALSE;
  gst_pad_add_probe (srcpad, GST_PAD_PROBE_TYPE_EVENT_UPSTREAM,
      probe_nav_event, (gpointer) probe_events, NULL);
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  srcpad = gst_element_get_static_pad (filter2, "src");
  sinkpad = gst_pad_get_peer (srcpad);
  g_object_set (sinkpad,
      "width", 400, "height", 200, "xpos", 20, "ypos", 0, NULL);
  probe_events[1].received = FALSE;
  gst_pad_add_probe (srcpad, GST_PAD_PROBE_TYPE_EVENT_UPSTREAM,
      probe_nav_event, (gpointer) (probe_events + 1), NULL);
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  srcpad = gst_element_get_static_pad (filter3, "src");
  sinkpad = gst_pad_get_peer (srcpad);
  g_object_set (sinkpad,
      "width", 200, "height", 50, "xpos", 0, "ypos", 0, NULL);
  probe_events[2].received = FALSE;
  gst_pad_add_probe (srcpad, GST_PAD_PROBE_TYPE_EVENT_UPSTREAM,
      probe_nav_event, (gpointer) (probe_events + 2), NULL);
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  event =
      gst_event_new_navigation (gst_structure_new
      ("application/x-gst-navigation", "event", G_TYPE_STRING, "mouse-move",
          "button", G_TYPE_INT, 0, "pointer_x", G_TYPE_DOUBLE, 350.0,
          "pointer_y", G_TYPE_DOUBLE, 100.0, NULL));

  GST_INFO ("starting test");

  /* prepare playing */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* wait for completion */
  state_res = gst_element_get_state (bin, NULL, NULL, GST_CLOCK_TIME_NONE);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* send event and validate */
  res = gst_element_send_event (sink, event);
  fail_unless (res == TRUE, NULL);

  /* check received events */
  ck_assert_msg (probe_events[0].received);
  ck_assert_msg (probe_events[1].received);
  ck_assert_msg (!probe_events[2].received);

  ck_assert_int_eq ((gint) probe_events[0].x_pos, 300);
  ck_assert_int_eq ((gint) probe_events[0].y_pos, 0);
  ck_assert_int_eq ((gint) probe_events[1].x_pos, 330);
  ck_assert_int_eq ((gint) probe_events[1].y_pos, 100);

  state_res = gst_element_set_state (bin, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  gst_object_unref (bin);
}

GST_END_TEST;

static GstBuffer *
create_video_buffer (GstCaps * caps, gint ts_in_seconds)
{
  GstVideoInfo info;
  guint size;
  GstBuffer *buf;
  GstMapInfo mapinfo;

  fail_unless (gst_video_info_from_caps (&info, caps));

  size = GST_VIDEO_INFO_WIDTH (&info) * GST_VIDEO_INFO_HEIGHT (&info);

  switch (GST_VIDEO_INFO_FORMAT (&info)) {
    case GST_VIDEO_FORMAT_RGB:
      size *= 3;
      break;
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_ARGB:
      size *= 4;
      break;
    case GST_VIDEO_FORMAT_I420:
      size *= 2;
      break;
    default:
      fail ("Unsupported test format");
  }

  buf = gst_buffer_new_and_alloc (size);
  /* write something to avoid uninitialized error issues (valgrind) */
  gst_buffer_map (buf, &mapinfo, GST_MAP_WRITE);
  memset (mapinfo.data, 0, mapinfo.size);
  gst_buffer_unmap (buf, &mapinfo);

  GST_BUFFER_PTS (buf) = ts_in_seconds * GST_SECOND;
  GST_BUFFER_DURATION (buf) = GST_SECOND;
  return buf;
}


GST_START_TEST (test_caps_query)
{
  GstElement *compositor, *capsfilter, *sink;
  GstElement *pipeline;
  gboolean res;
  GstStateChangeReturn state_res;
  GstPad *sinkpad;
  GstCaps *caps, *restriction_caps;
  GstCaps *all_caps, *non_alpha_caps;

  /* initial setup */
  all_caps = _compositor_get_all_supported_caps ();
  non_alpha_caps = _compositor_get_non_alpha_supported_caps ();

  compositor = gst_element_factory_make ("compositor", "compositor");
  capsfilter = gst_element_factory_make ("capsfilter", "out-cf");
  sink = gst_element_factory_make ("fakesink", "sink");
  pipeline = gst_pipeline_new ("test-pipeline");

  gst_bin_add_many (GST_BIN (pipeline), compositor, capsfilter, sink, NULL);
  res = gst_element_link (compositor, capsfilter);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (capsfilter, sink);
  fail_unless (res == TRUE, NULL);

  sinkpad = gst_element_request_pad_simple (compositor, "sink_%u");

  state_res = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  fail_if (state_res == GST_STATE_CHANGE_FAILURE);

  /* try an unrestricted caps query, should return all formats */
  caps = gst_pad_query_caps (sinkpad, NULL);
  fail_unless (gst_caps_is_equal (caps, all_caps));
  gst_caps_unref (caps);

  /* now restrict downstream to a single alpha format, it should still
   * be able to convert anything else to it */
  restriction_caps = gst_caps_from_string ("video/x-raw, format=(string)AYUV");
  g_object_set (capsfilter, "caps", restriction_caps, NULL);
  caps = gst_pad_query_caps (sinkpad, NULL);
  fail_unless (gst_caps_is_equal (caps, all_caps));
  gst_caps_unref (caps);
  gst_caps_unref (restriction_caps);

  /* now restrict downstream to a non-alpha format, it should
   * be able to accept non-alpha formats */
  restriction_caps = gst_caps_from_string ("video/x-raw, format=(string)I420");
  g_object_set (capsfilter, "caps", restriction_caps, NULL);
  caps = gst_pad_query_caps (sinkpad, NULL);
  fail_unless (gst_caps_is_equal (caps, non_alpha_caps));
  gst_caps_unref (caps);
  gst_caps_unref (restriction_caps);

  /* check that compositor proxies downstream interlace-mode */
  restriction_caps =
      gst_caps_from_string ("video/x-raw, interlace-mode=(string)interleaved");
  g_object_set (capsfilter, "caps", restriction_caps, NULL);
  caps = gst_pad_query_caps (sinkpad, NULL);
  fail_unless (gst_caps_is_subset (caps, restriction_caps));
  gst_caps_unref (caps);
  gst_caps_unref (restriction_caps);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_element_release_request_pad (compositor, sinkpad);
  gst_object_unref (sinkpad);
  gst_object_unref (pipeline);
  gst_caps_unref (all_caps);
  gst_caps_unref (non_alpha_caps);
}

GST_END_TEST;


GST_START_TEST (test_caps_query_interlaced)
{
  GstElement *compositor, *sink;
  GstElement *pipeline;
  gboolean res;
  GstStateChangeReturn state_res;
  GstPad *sinkpad;
  GstCaps *caps;
  GstCaps *caps_mixed, *caps_progressive, *caps_interleaved;
  GstEvent *caps_event;
  GstQuery *drain;

  caps_interleaved =
      gst_caps_from_string ("video/x-raw, interlace-mode=interleaved");
  caps_mixed = gst_caps_from_string ("video/x-raw, interlace-mode=mixed");
  caps_progressive =
      gst_caps_from_string ("video/x-raw, interlace-mode=progressive");

  /* initial setup */
  compositor = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("fakesink", "sink");
  pipeline = gst_pipeline_new ("test-pipeline");

  gst_bin_add_many (GST_BIN (pipeline), compositor, sink, NULL);
  res = gst_element_link (compositor, sink);
  fail_unless (res == TRUE, NULL);
  sinkpad = gst_element_request_pad_simple (compositor, "sink_%u");

  state_res = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  fail_if (state_res == GST_STATE_CHANGE_FAILURE);

  /* try an unrestricted caps query, should be compatible with all formats */
  caps = gst_pad_query_caps (sinkpad, NULL);
  fail_unless (gst_caps_can_intersect (caps, caps_interleaved));
  fail_unless (gst_caps_can_intersect (caps, caps_progressive));
  fail_unless (gst_caps_can_intersect (caps, caps_mixed));
  gst_caps_unref (caps);

  /* now set caps on the pad, it should restrict the interlace-mode for
   * future caps */
  caps = gst_caps_from_string ("video/x-raw, width=100, height=100, "
      "format=RGB, framerate=1/1, interlace-mode=progressive");
  caps_event = gst_event_new_caps (caps);
  gst_caps_unref (caps);
  fail_unless (gst_pad_send_event (sinkpad, caps_event));

  /* Send drain query to make sure this is processed */
  drain = gst_query_new_drain ();
  gst_pad_query (sinkpad, drain);
  gst_query_unref (drain);

  /* now recheck the interlace-mode */
  gst_object_unref (sinkpad);
  sinkpad = gst_element_request_pad_simple (compositor, "sink_%u");
  caps = gst_pad_query_caps (sinkpad, NULL);
  fail_if (gst_caps_can_intersect (caps, caps_interleaved));
  fail_unless (gst_caps_can_intersect (caps, caps_progressive));
  fail_if (gst_caps_can_intersect (caps, caps_mixed));
  gst_object_unref (sinkpad);
  gst_caps_unref (caps);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  gst_caps_unref (caps_interleaved);
  gst_caps_unref (caps_mixed);
  gst_caps_unref (caps_progressive);
}

GST_END_TEST;

static void
add_interlaced_mode_to_caps (GstCaps * caps, const gchar * mode)
{
  GstStructure *s;
  gint i;

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    s = gst_caps_get_structure (caps, i);
    gst_structure_set (s, "interlace-mode", G_TYPE_STRING, mode, NULL);
  }
}

#define MODE_ALL 1
#define MODE_NON_ALPHA 2

static void
run_late_caps_query_test (GstCaps * input_caps, GstCaps * output_allowed_caps,
    gint expected_caps_mode)
{
  GstElement *compositor, *capsfilter, *sink;
  GstElement *pipeline;
  gboolean res;
  GstStateChangeReturn state_res;
  GstPad *srcpad1, *srcpad2;
  GstPad *sinkpad1, *sinkpad2;
  GstSegment segment;
  GstCaps *caps, *all_caps, *non_alpha_caps;

  all_caps = _compositor_get_all_supported_caps ();
  non_alpha_caps = _compositor_get_non_alpha_supported_caps ();

  /* add progressive mode as it is what is used in the test, otherwise
   * is_equal checks would fail */
  add_interlaced_mode_to_caps (all_caps, "progressive");
  add_interlaced_mode_to_caps (non_alpha_caps, "progressive");

  compositor = gst_element_factory_make ("compositor", "compositor");
  capsfilter = gst_element_factory_make ("capsfilter", "out-cf");
  sink = gst_element_factory_make ("fakesink", "sink");
  pipeline = gst_pipeline_new ("test-pipeline");

  gst_bin_add_many (GST_BIN (pipeline), compositor, capsfilter, sink, NULL);
  res = gst_element_link (compositor, capsfilter);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (capsfilter, sink);
  fail_unless (res == TRUE, NULL);

  sinkpad1 = gst_element_request_pad_simple (compositor, "sink_%u");
  srcpad1 = gst_pad_new ("src1", GST_PAD_SRC);
  fail_unless (gst_pad_link (srcpad1, sinkpad1) == GST_PAD_LINK_OK);
  gst_pad_set_active (srcpad1, TRUE);

  state_res = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  fail_if (state_res == GST_STATE_CHANGE_FAILURE);

  if (output_allowed_caps)
    g_object_set (capsfilter, "caps", output_allowed_caps, NULL);

  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (srcpad1,
          gst_event_new_stream_start ("test-1")));
  fail_unless (gst_pad_push_event (srcpad1, gst_event_new_caps (input_caps)));
  fail_unless (gst_pad_push_event (srcpad1, gst_event_new_segment (&segment)));
  fail_unless (gst_pad_push (srcpad1,
          create_video_buffer (input_caps, 0)) == GST_FLOW_OK);
  fail_unless (gst_pad_push (srcpad1,
          create_video_buffer (input_caps, 1)) == GST_FLOW_OK);

  /* now comes the second pad */
  sinkpad2 = gst_element_request_pad_simple (compositor, "sink_%u");
  srcpad2 = gst_pad_new ("src2", GST_PAD_SRC);
  fail_unless (gst_pad_link (srcpad2, sinkpad2) == GST_PAD_LINK_OK);
  gst_pad_set_active (srcpad2, TRUE);
  fail_unless (gst_pad_push_event (srcpad2,
          gst_event_new_stream_start ("test-2")));

  caps = gst_pad_peer_query_caps (srcpad2, NULL);
  fail_unless (gst_caps_is_equal (caps,
          expected_caps_mode == MODE_ALL ? all_caps : non_alpha_caps));
  gst_caps_unref (caps);

  gst_pad_set_active (srcpad1, FALSE);
  gst_pad_set_active (srcpad2, FALSE);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_element_release_request_pad (compositor, sinkpad1);
  gst_element_release_request_pad (compositor, sinkpad2);
  gst_object_unref (sinkpad1);
  gst_object_unref (sinkpad2);
  gst_object_unref (pipeline);
  gst_object_unref (srcpad1);
  gst_object_unref (srcpad2);
  gst_caps_unref (all_caps);
  gst_caps_unref (non_alpha_caps);
}

GST_START_TEST (test_late_caps_query)
{
  GstCaps *rgb_caps;
  GstCaps *non_alpha_caps;

  rgb_caps = gst_caps_from_string ("video/x-raw, format=(string)RGB, "
      "width=(int)100, height=(int)100, framerate=(fraction)1/1");
  non_alpha_caps = gst_caps_from_string ("video/x-raw, format=(string)RGB");

  /* check that a 2nd pad that is added late to compositor will be able to
   * negotiate to formats that depend only on downstream caps and not on what
   * the other pads have already negotiated */
  run_late_caps_query_test (rgb_caps, NULL, MODE_ALL);
  run_late_caps_query_test (rgb_caps, non_alpha_caps, MODE_NON_ALPHA);

  gst_caps_unref (non_alpha_caps);
  gst_caps_unref (rgb_caps);
}

GST_END_TEST;

static void
run_late_caps_set_test (GstCaps * first_caps, GstCaps * expected_query_caps,
    GstCaps * second_caps, gboolean accept_caps)
{
  GstElement *capsfilter_1;
  GstElement *compositor;
  GstElement *pipeline;
  GstStateChangeReturn state_res;
  GstPad *sinkpad_2;
  GstCaps *caps;
  GstBus *bus;
  GstMessage *msg;

  pipeline =
      gst_parse_launch ("videotestsrc num-buffers=10 ! capsfilter name=cf1 !"
      " compositor name=c ! fakesink sync=true", NULL);
  fail_unless (pipeline != NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  compositor = gst_bin_get_by_name (GST_BIN (pipeline), "c");
  capsfilter_1 = gst_bin_get_by_name (GST_BIN (pipeline), "cf1");

  g_object_set (capsfilter_1, "caps", first_caps, NULL);

  state_res = gst_element_set_state (pipeline, GST_STATE_PAUSED);
  fail_if (state_res == GST_STATE_CHANGE_FAILURE);

  /* wait for pipeline to get to paused */
  msg =
      gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_ASYNC_DONE);
  fail_unless (msg != NULL);
  fail_unless (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ASYNC_DONE);
  gst_message_unref (msg);

  /* try to set the second caps */
  sinkpad_2 = gst_element_request_pad_simple (compositor, "sink_%u");
  caps = gst_pad_query_caps (sinkpad_2, NULL);
  fail_unless (gst_caps_is_subset (expected_query_caps, caps));
  gst_caps_unref (caps);
  caps = gst_pad_query_caps (sinkpad_2, second_caps);
  fail_unless (gst_caps_is_empty (caps) != accept_caps);
  gst_caps_unref (caps);
  gst_object_unref (sinkpad_2);

  gst_object_unref (bus);
  gst_object_unref (compositor);
  gst_object_unref (capsfilter_1);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

GST_START_TEST (test_late_caps_different_interlaced)
{
  GstCaps *non_interlaced_caps;
  GstCaps *interlaced_caps;

  non_interlaced_caps =
      gst_caps_from_string ("video/x-raw, interlace-mode=progressive, "
      "format=RGB, width=100, height=100, framerate=1/1");
  interlaced_caps =
      gst_caps_from_string ("video/x-raw, interlace-mode=interleaved, "
      "format=RGB, width=100, height=100, framerate=1/1");

  run_late_caps_set_test (non_interlaced_caps, non_interlaced_caps,
      interlaced_caps, FALSE);

  gst_caps_unref (non_interlaced_caps);
  gst_caps_unref (interlaced_caps);
}

GST_END_TEST;

static guint play_count = 0;
static GstEvent *play_seek_event = NULL;

static void
test_play_twice_message_received (GstBus * bus, GstMessage * message,
    GstPipeline * bin)
{
  gboolean res;
  GstStateChangeReturn state_res;

  GST_INFO ("bus message from \"%" GST_PTR_FORMAT "\": %" GST_PTR_FORMAT,
      GST_MESSAGE_SRC (message), message);

  switch (message->type) {
    case GST_MESSAGE_SEGMENT_DONE:
      play_count++;
      if (play_count == 1) {
        state_res = gst_element_set_state (GST_ELEMENT (bin), GST_STATE_READY);
        ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

        /* prepare playing again */
        state_res = gst_element_set_state (GST_ELEMENT (bin), GST_STATE_PAUSED);
        ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

        /* wait for completion */
        state_res =
            gst_element_get_state (GST_ELEMENT (bin), NULL, NULL,
            GST_CLOCK_TIME_NONE);
        ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

        gst_event_set_seqnum (play_seek_event, gst_util_seqnum_next ());
        res = gst_element_send_event (GST_ELEMENT (bin),
            gst_event_ref (play_seek_event));
        fail_unless (res == TRUE, NULL);

        state_res =
            gst_element_set_state (GST_ELEMENT (bin), GST_STATE_PLAYING);
        ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);
      } else {
        g_main_loop_quit (main_loop);
      }
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}


GST_START_TEST (test_play_twice)
{
  GstElement *bin, *src1, *src2, *compositor, *sink;
  GstBus *bus;
  gboolean res;
  GstStateChangeReturn state_res;
  GstPad *srcpad;
  GstStreamConsistency *consist;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src1 = gst_element_factory_make ("videotestsrc", "src1");
  src2 = gst_element_factory_make ("videotestsrc", "src2");
  compositor = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src1, src2, compositor, sink, NULL);

  res = gst_element_link (src1, compositor);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (src2, compositor);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (compositor, sink);
  fail_unless (res == TRUE, NULL);

  srcpad = gst_element_get_static_pad (compositor, "src");
  consist = gst_consistency_checker_new (srcpad);
  gst_object_unref (srcpad);

  play_seek_event = gst_event_new_seek (1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_FLUSH,
      GST_SEEK_TYPE_SET, (GstClockTime) 0,
      GST_SEEK_TYPE_SET, (GstClockTime) 2 * GST_SECOND);

  play_count = 0;

  main_loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (bus, "message::segment-done",
      (GCallback) test_play_twice_message_received, bin);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  GST_INFO ("starting test");

  /* prepare playing */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* wait for completion */
  state_res =
      gst_element_get_state (GST_ELEMENT (bin), NULL, NULL,
      GST_CLOCK_TIME_NONE);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  gst_event_set_seqnum (play_seek_event, gst_util_seqnum_next ());
  res = gst_element_send_event (bin, gst_event_ref (play_seek_event));
  fail_unless (res == TRUE, NULL);

  GST_INFO ("seeked");

  /* run pipeline */
  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  g_main_loop_run (main_loop);

  state_res = gst_element_set_state (bin, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  ck_assert_int_eq (play_count, 2);

  /* cleanup */
  g_main_loop_unref (main_loop);
  gst_consistency_checker_free (consist);
  gst_event_unref (play_seek_event);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

GST_START_TEST (test_play_twice_then_add_and_play_again)
{
  GstElement *bin, *src1, *src2, *src3, *compositor, *sink;
  GstBus *bus;
  gboolean res;
  GstStateChangeReturn state_res;
  gint i;
  GstPad *srcpad;
  GstStreamConsistency *consist;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src1 = gst_element_factory_make ("videotestsrc", "src1");
  src2 = gst_element_factory_make ("videotestsrc", "src2");
  compositor = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src1, src2, compositor, sink, NULL);

  srcpad = gst_element_get_static_pad (compositor, "src");
  consist = gst_consistency_checker_new (srcpad);
  gst_object_unref (srcpad);

  res = gst_element_link (src1, compositor);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (src2, compositor);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (compositor, sink);
  fail_unless (res == TRUE, NULL);

  play_seek_event = gst_event_new_seek (1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_FLUSH,
      GST_SEEK_TYPE_SET, (GstClockTime) 0,
      GST_SEEK_TYPE_SET, (GstClockTime) 2 * GST_SECOND);

  main_loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (bus, "message::segment-done",
      (GCallback) test_play_twice_message_received, bin);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  /* run it twice */
  for (i = 0; i < 2; i++) {
    play_count = 0;

    GST_INFO ("starting test-loop %d", i);

    /* prepare playing */
    state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
    ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

    /* wait for completion */
    state_res =
        gst_element_get_state (GST_ELEMENT (bin), NULL, NULL,
        GST_CLOCK_TIME_NONE);
    ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

    gst_event_set_seqnum (play_seek_event, gst_util_seqnum_next ());
    res = gst_element_send_event (bin, gst_event_ref (play_seek_event));
    fail_unless (res == TRUE, NULL);

    GST_INFO ("seeked");

    /* run pipeline */
    state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
    ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

    g_main_loop_run (main_loop);

    state_res = gst_element_set_state (bin, GST_STATE_READY);
    ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

    ck_assert_int_eq (play_count, 2);

    /* plug another source */
    if (i == 0) {
      src3 = gst_element_factory_make ("videotestsrc", "src3");
      gst_bin_add (GST_BIN (bin), src3);

      res = gst_element_link (src3, compositor);
      fail_unless (res == TRUE, NULL);
    }

    gst_consistency_checker_reset (consist);
  }

  state_res = gst_element_set_state (bin, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* cleanup */
  g_main_loop_unref (main_loop);
  gst_event_unref (play_seek_event);
  gst_consistency_checker_free (consist);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

/* check if adding pads work as expected */
GST_START_TEST (test_add_pad)
{
  GstElement *bin, *src1, *src2, *compositor, *sink;
  GstBus *bus;
  GstPad *srcpad;
  gboolean res;
  GstStateChangeReturn state_res;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src1 = gst_element_factory_make ("videotestsrc", "src1");
  g_object_set (src1, "num-buffers", 4, NULL);
  src2 = gst_element_factory_make ("videotestsrc", "src2");
  /* one buffer less, we connect with 1 buffer of delay */
  g_object_set (src2, "num-buffers", 3, NULL);
  compositor = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src1, compositor, sink, NULL);

  res = gst_element_link (src1, compositor);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (compositor, sink);
  fail_unless (res == TRUE, NULL);

  srcpad = gst_element_get_static_pad (compositor, "src");
  gst_object_unref (srcpad);

  main_loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (bus, "message::segment-done", (GCallback) message_received,
      bin);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  GST_INFO ("starting test");

  /* prepare playing */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* wait for completion */
  state_res =
      gst_element_get_state (GST_ELEMENT (bin), NULL, NULL,
      GST_CLOCK_TIME_NONE);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* add other element */
  gst_bin_add_many (GST_BIN (bin), src2, NULL);

  /* now link the second element */
  res = gst_element_link (src2, compositor);
  fail_unless (res == TRUE, NULL);

  /* set to PAUSED as well */
  state_res = gst_element_set_state (src2, GST_STATE_PAUSED);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* now play all */
  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  g_main_loop_run (main_loop);

  state_res = gst_element_set_state (bin, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* cleanup */
  g_main_loop_unref (main_loop);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

/* check if removing pads work as expected */
GST_START_TEST (test_remove_pad)
{
  GstElement *bin, *src, *compositor, *sink;
  GstBus *bus;
  GstPad *pad, *srcpad;
  gboolean res;
  GstStateChangeReturn state_res;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src = gst_element_factory_make ("videotestsrc", "src");
  g_object_set (src, "num-buffers", 4, NULL);
  compositor = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src, compositor, sink, NULL);

  res = gst_element_link (src, compositor);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (compositor, sink);
  fail_unless (res == TRUE, NULL);

  /* create an unconnected sinkpad in compositor */
  pad = gst_element_request_pad_simple (compositor, "sink_%u");
  fail_if (pad == NULL, NULL);

  srcpad = gst_element_get_static_pad (compositor, "src");
  gst_object_unref (srcpad);

  main_loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (bus, "message::segment-done", (GCallback) message_received,
      bin);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  GST_INFO ("starting test");

  /* prepare playing, this will not preroll as compositor is waiting
   * on the unconnected sinkpad. */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* wait for completion for one second, will return ASYNC */
  state_res = gst_element_get_state (GST_ELEMENT (bin), NULL, NULL, GST_SECOND);
  ck_assert_int_eq (state_res, GST_STATE_CHANGE_ASYNC);

  /* get rid of the pad now, compositor should stop waiting on it and
   * continue the preroll */
  gst_element_release_request_pad (compositor, pad);
  gst_object_unref (pad);

  /* wait for completion, should work now */
  state_res =
      gst_element_get_state (GST_ELEMENT (bin), NULL, NULL,
      GST_CLOCK_TIME_NONE);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* now play all */
  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  g_main_loop_run (main_loop);

  state_res = gst_element_set_state (bin, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* cleanup */
  g_main_loop_unref (main_loop);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (G_OBJECT (bus));
  gst_object_unref (G_OBJECT (bin));
}

GST_END_TEST;


static GstBuffer *handoff_buffer = NULL;

static gboolean
_quit (GMainLoop * ml)
{
  g_main_loop_quit (ml);

  return G_SOURCE_REMOVE;
}

static void
handoff_buffer_cb (GstElement * fakesink, GstBuffer * buffer, GstPad * pad,
    gpointer user_data)
{
  GST_DEBUG ("got buffer %p", buffer);
  gst_buffer_replace (&handoff_buffer, buffer);

  if (main_loop)
    g_idle_add ((GSourceFunc) _quit, main_loop);
}

/* check if clipping works as expected */
GST_START_TEST (test_clip)
{
  GstSegment segment;
  GstElement *bin, *compositor, *sink;
  GstBus *bus;
  GstPad *sinkpad;
  gboolean res;
  GstStateChangeReturn state_res;
  GstFlowReturn ret;
  GstEvent *event;
  GstBuffer *buffer;
  GstCaps *caps;
  GMainLoop *local_mainloop;

  GST_INFO ("preparing test");

  local_mainloop = g_main_loop_new (NULL, FALSE);
  main_loop = NULL;

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  /* just an compositor and a fakesink */
  compositor = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("fakesink", "sink");
  g_object_set (sink, "signal-handoffs", TRUE, NULL);
  g_signal_connect (sink, "handoff", (GCallback) handoff_buffer_cb, NULL);
  gst_bin_add_many (GST_BIN (bin), compositor, sink, NULL);

  res = gst_element_link (compositor, sink);
  fail_unless (res == TRUE, NULL);

  /* set to playing */
  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* create an unconnected sinkpad in compositor, should also automatically activate
   * the pad */
  sinkpad = gst_element_request_pad_simple (compositor, "sink_%u");
  fail_if (sinkpad == NULL, NULL);

  gst_pad_send_event (sinkpad, gst_event_new_stream_start ("test"));

  caps = gst_caps_from_string (VIDEO_CAPS_STRING);

  gst_pad_set_caps (sinkpad, caps);
  gst_caps_unref (caps);

  /* send segment to compositor */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  segment.start = GST_SECOND;
  segment.stop = 2 * GST_SECOND;
  segment.time = 0;
  event = gst_event_new_segment (&segment);
  gst_pad_send_event (sinkpad, event);

  /* should be clipped and ok */
  buffer = gst_buffer_new_and_alloc (115200);
  GST_BUFFER_TIMESTAMP (buffer) = 0;
  GST_BUFFER_DURATION (buffer) = 250 * GST_MSECOND;
  GST_DEBUG ("pushing buffer %p", buffer);
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);
  fail_unless (handoff_buffer == NULL);

  /* should be partially clipped */
  buffer = gst_buffer_new_and_alloc (115200);
  GST_BUFFER_TIMESTAMP (buffer) = 900 * GST_MSECOND;
  GST_BUFFER_DURATION (buffer) = 250 * GST_MSECOND;
  GST_DEBUG ("pushing buffer %p", buffer);

  main_loop = local_mainloop;
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);
  g_main_loop_run (main_loop);
  gst_buffer_replace (&handoff_buffer, NULL);

  /* should not be clipped */
  buffer = gst_buffer_new_and_alloc (115200);
  GST_BUFFER_TIMESTAMP (buffer) = 1 * GST_SECOND;
  GST_BUFFER_DURATION (buffer) = 250 * GST_MSECOND;
  GST_DEBUG ("pushing buffer %p", buffer);
  ret = gst_pad_chain (sinkpad, buffer);
  g_main_loop_run (main_loop);
  main_loop = NULL;
  ck_assert_int_eq (ret, GST_FLOW_OK);
  fail_unless (handoff_buffer != NULL);
  gst_buffer_replace (&handoff_buffer, NULL);

  /* should be clipped and ok */
  buffer = gst_buffer_new_and_alloc (115200);
  GST_BUFFER_TIMESTAMP (buffer) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buffer) = 250 * GST_MSECOND;
  GST_DEBUG ("pushing buffer %p", buffer);
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);
  fail_unless (handoff_buffer == NULL);

  gst_object_unref (sinkpad);
  gst_element_set_state (bin, GST_STATE_NULL);
  g_main_loop_unref (local_mainloop);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

GST_START_TEST (test_duration_is_max)
{
  GstElement *bin, *src[3], *compositor, *sink;
  GstStateChangeReturn state_res;
  GstFormat format = GST_FORMAT_TIME;
  gboolean res;
  gint64 duration;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");

  /* 3 sources, an compositor and a fakesink */
  src[0] = gst_element_factory_make ("videotestsrc", NULL);
  src[1] = gst_element_factory_make ("videotestsrc", NULL);
  src[2] = gst_element_factory_make ("videotestsrc", NULL);
  compositor = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src[0], src[1], src[2], compositor, sink,
      NULL);

  gst_element_link (src[0], compositor);
  gst_element_link (src[1], compositor);
  gst_element_link (src[2], compositor);
  gst_element_link (compositor, sink);

  /* irks, duration is reset on basesrc */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  fail_unless (state_res != GST_STATE_CHANGE_FAILURE, NULL);

  /* set durations on src */
  GST_BASE_SRC (src[0])->segment.duration = 1000;
  GST_BASE_SRC (src[1])->segment.duration = 3000;
  GST_BASE_SRC (src[2])->segment.duration = 2000;

  /* set to playing */
  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  fail_unless (state_res != GST_STATE_CHANGE_FAILURE, NULL);

  /* wait for completion */
  state_res =
      gst_element_get_state (GST_ELEMENT (bin), NULL, NULL,
      GST_CLOCK_TIME_NONE);
  fail_unless (state_res != GST_STATE_CHANGE_FAILURE, NULL);

  res = gst_element_query_duration (GST_ELEMENT (bin), format, &duration);
  fail_unless (res, NULL);

  ck_assert_int_eq (duration, 3000);

  gst_element_set_state (bin, GST_STATE_NULL);
  gst_object_unref (bin);
}

GST_END_TEST;

GST_START_TEST (test_duration_unknown_overrides)
{
  GstElement *bin, *src[3], *compositor, *sink;
  GstStateChangeReturn state_res;
  GstFormat format = GST_FORMAT_TIME;
  gboolean res;
  gint64 duration;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");

  /* 3 sources, an compositor and a fakesink */
  src[0] = gst_element_factory_make ("videotestsrc", NULL);
  src[1] = gst_element_factory_make ("videotestsrc", NULL);
  src[2] = gst_element_factory_make ("videotestsrc", NULL);
  compositor = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src[0], src[1], src[2], compositor, sink,
      NULL);

  gst_element_link (src[0], compositor);
  gst_element_link (src[1], compositor);
  gst_element_link (src[2], compositor);
  gst_element_link (compositor, sink);

  /* irks, duration is reset on basesrc */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  fail_unless (state_res != GST_STATE_CHANGE_FAILURE, NULL);

  /* set durations on src */
  GST_BASE_SRC (src[0])->segment.duration = GST_CLOCK_TIME_NONE;
  GST_BASE_SRC (src[1])->segment.duration = 3000;
  GST_BASE_SRC (src[2])->segment.duration = 2000;

  /* set to playing */
  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  fail_unless (state_res != GST_STATE_CHANGE_FAILURE, NULL);

  /* wait for completion */
  state_res =
      gst_element_get_state (GST_ELEMENT (bin), NULL, NULL,
      GST_CLOCK_TIME_NONE);
  fail_unless (state_res != GST_STATE_CHANGE_FAILURE, NULL);

  res = gst_element_query_duration (GST_ELEMENT (bin), format, &duration);
  fail_unless (res, NULL);

  ck_assert_int_eq (duration, GST_CLOCK_TIME_NONE);

  gst_element_set_state (bin, GST_STATE_NULL);
  gst_object_unref (bin);
}

GST_END_TEST;


static gboolean looped = FALSE;

static void
loop_segment_done (GstBus * bus, GstMessage * message, GstElement * bin)
{
  GST_INFO ("bus message from \"%" GST_PTR_FORMAT "\": %" GST_PTR_FORMAT,
      GST_MESSAGE_SRC (message), message);

  if (looped) {
    g_main_loop_quit (main_loop);
  } else {
    GstEvent *seek_event;
    gboolean res;

    seek_event = gst_event_new_seek (1.0, GST_FORMAT_TIME,
        GST_SEEK_FLAG_SEGMENT,
        GST_SEEK_TYPE_SET, (GstClockTime) 0,
        GST_SEEK_TYPE_SET, (GstClockTime) 1 * GST_SECOND);

    res = gst_element_send_event (bin, seek_event);
    fail_unless (res == TRUE, NULL);
    looped = TRUE;
  }
}

GST_START_TEST (test_loop)
{
  GstElement *bin, *src1, *src2, *compositor, *sink;
  GstBus *bus;
  GstEvent *seek_event;
  GstStateChangeReturn state_res;
  gboolean res;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src1 = gst_element_factory_make ("videotestsrc", "src1");
  src2 = gst_element_factory_make ("videotestsrc", "src2");
  compositor = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src1, src2, compositor, sink, NULL);

  res = gst_element_link (src1, compositor);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (src2, compositor);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (compositor, sink);
  fail_unless (res == TRUE, NULL);

  seek_event = gst_event_new_seek (1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_FLUSH,
      GST_SEEK_TYPE_SET, (GstClockTime) 0, GST_SEEK_TYPE_SET,
      (GstClockTime) 2 * GST_SECOND);

  main_loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (bus, "message::segment-done",
      (GCallback) loop_segment_done, bin);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  GST_INFO ("starting test");

  /* prepare playing */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* wait for completion */
  state_res =
      gst_element_get_state (GST_ELEMENT (bin), NULL, NULL,
      GST_CLOCK_TIME_NONE);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  res = gst_element_send_event (bin, seek_event);
  fail_unless (res == TRUE, NULL);

  /* run pipeline */
  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  GST_INFO ("running main loop");
  g_main_loop_run (main_loop);

  state_res = gst_element_set_state (bin, GST_STATE_NULL);

  /* cleanup */
  g_main_loop_unref (main_loop);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

GST_START_TEST (test_segment_base_handling)
{
  GstElement *pipeline, *sink, *mix, *src1, *src2;
  GstPad *srcpad, *sinkpad;
  GstClockTime end_time;
  GstSample *last_sample = NULL;
  GstSample *sample;
  GstBuffer *buf;
  GstCaps *caps;

  caps = gst_caps_new_simple ("video/x-raw", "width", G_TYPE_INT, 16,
      "height", G_TYPE_INT, 16, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);

  /* each source generates 5 seconds of data, src2 shifted by 5 seconds */
  pipeline = gst_pipeline_new ("pipeline");
  mix = gst_element_factory_make ("compositor", "compositor");
  sink = gst_element_factory_make ("appsink", "sink");
  g_object_set (sink, "caps", caps, "sync", FALSE, NULL);
  gst_caps_unref (caps);
  src1 = gst_element_factory_make ("videotestsrc", "src1");
  g_object_set (src1, "num-buffers", 30 * 5, "pattern", 2, NULL);
  src2 = gst_element_factory_make ("videotestsrc", "src2");
  g_object_set (src2, "num-buffers", 30 * 5, "pattern", 2, NULL);
  gst_bin_add_many (GST_BIN (pipeline), src1, src2, mix, sink, NULL);
  fail_unless (gst_element_link (mix, sink));

  srcpad = gst_element_get_static_pad (src1, "src");
  sinkpad = gst_element_request_pad_simple (mix, "sink_1");
  fail_unless (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK);
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  srcpad = gst_element_get_static_pad (src2, "src");
  sinkpad = gst_element_request_pad_simple (mix, "sink_2");
  fail_unless (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK);
  gst_pad_set_offset (sinkpad, 5 * GST_SECOND);
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  do {
    g_signal_emit_by_name (sink, "pull-sample", &sample);
    if (sample == NULL)
      break;
    if (last_sample)
      gst_sample_unref (last_sample);
    last_sample = sample;
  } while (TRUE);

  buf = gst_sample_get_buffer (last_sample);
  end_time = GST_BUFFER_TIMESTAMP (buf) + GST_BUFFER_DURATION (buf);
  fail_unless_equals_int64 (end_time, 10 * GST_SECOND);
  gst_sample_unref (last_sample);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

GST_END_TEST;

static gboolean buffer_mapped;
static gboolean (*default_map) (GstVideoMeta * meta, guint plane,
    GstMapInfo * info, gpointer * data, gint * stride, GstMapFlags flags);

static gboolean
test_obscured_new_videometa_map (GstVideoMeta * meta, guint plane,
    GstMapInfo * info, gpointer * data, gint * stride, GstMapFlags flags)
{
  buffer_mapped = TRUE;
  return default_map (meta, plane, info, data, stride, flags);
}

static GstPadProbeReturn
test_obscured_pad_probe_cb (GstPad * srcpad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstBuffer *obuf, *nbuf;
  GstVideoMeta *meta;

  GST_DEBUG ("pad probe called");
  /* We need to deep-copy the buffer here because videotestsrc reuses buffers
   * and hence the GstVideoMap associated with the buffers, and that causes a
   * segfault inside videotestsrc when it tries to reuse the buffer */
  obuf = GST_PAD_PROBE_INFO_BUFFER (info);
  nbuf = gst_buffer_new ();
  gst_buffer_copy_into (nbuf, obuf, GST_BUFFER_COPY_ALL | GST_BUFFER_COPY_DEEP,
      0, -1);
  meta = gst_buffer_get_video_meta (nbuf);
  /* Override the default map() function to set also buffer_mapped */
  default_map = meta->map;
  meta->map = test_obscured_new_videometa_map;
  /* Replace the buffer that's going downstream */
  GST_PAD_PROBE_INFO_DATA (info) = nbuf;
  gst_buffer_unref (obuf);

  return GST_PAD_PROBE_PASS;
}

static void
_test_obscured (const gchar * caps_str, gint xpos0, gint ypos0, gint width0,
    gint height0, gdouble alpha0, gint xpos1, gint ypos1, gint width1,
    gint height1, gdouble alpha1, gint out_width, gint out_height)
{
  GstElement *pipeline, *sink, *mix, *src0, *cfilter0, *src1, *cfilter1;
  GstElement *out_cfilter;
  GstPad *srcpad, *sinkpad;
  GstSample *last_sample = NULL;
  GstSample *sample;
  GstCaps *caps;

  GST_INFO ("preparing test");

  pipeline = gst_pipeline_new ("pipeline");
  src0 = gst_element_factory_make ("videotestsrc", "src0");
  g_object_set (src0, "num-buffers", 5, NULL);
  cfilter0 = gst_element_factory_make ("capsfilter", "capsfilter0");
  caps = gst_caps_from_string (caps_str);
  g_object_set (cfilter0, "caps", caps, NULL);
  gst_caps_unref (caps);

  src1 = gst_element_factory_make ("videotestsrc", "src1");
  g_object_set (src1, "num-buffers", 5, NULL);
  cfilter1 = gst_element_factory_make ("capsfilter", "capsfilter1");
  caps = gst_caps_from_string (caps_str);
  g_object_set (cfilter1, "caps", caps, NULL);
  gst_caps_unref (caps);

  mix = gst_element_factory_make ("compositor", "compositor");
  out_cfilter = gst_element_factory_make ("capsfilter", "out_capsfilter");
  caps = gst_caps_from_string (caps_str);
  if (out_width > 0)
    gst_caps_set_simple (caps, "width", G_TYPE_INT, out_width, NULL);
  if (out_height > 0)
    gst_caps_set_simple (caps, "height", G_TYPE_INT, out_height, NULL);
  g_object_set (out_cfilter, "caps", caps, NULL);
  gst_caps_unref (caps);
  sink = gst_element_factory_make ("appsink", "sink");

  gst_bin_add_many (GST_BIN (pipeline), src0, cfilter0, src1, cfilter1, mix,
      out_cfilter, sink, NULL);
  fail_unless (gst_element_link (src0, cfilter0));
  fail_unless (gst_element_link (src1, cfilter1));
  fail_unless (gst_element_link (mix, out_cfilter));
  fail_unless (gst_element_link (out_cfilter, sink));

  srcpad = gst_element_get_static_pad (cfilter0, "src");
  sinkpad = gst_element_request_pad_simple (mix, "sink_0");
  g_object_set (sinkpad, "xpos", xpos0, "ypos", ypos0, "width", width0,
      "height", height0, "alpha", alpha0, NULL);
  fail_unless (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK);
  gst_pad_add_probe (srcpad, GST_PAD_PROBE_TYPE_BUFFER,
      test_obscured_pad_probe_cb, NULL, NULL);
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  srcpad = gst_element_get_static_pad (cfilter1, "src");
  sinkpad = gst_element_request_pad_simple (mix, "sink_1");
  g_object_set (sinkpad, "xpos", xpos1, "ypos", ypos1, "width", width1,
      "height", height1, "alpha", alpha1, NULL);
  fail_unless (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK);
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  GST_INFO ("sample prepared");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  do {
    GST_DEBUG ("sample pulling");
    g_signal_emit_by_name (sink, "pull-sample", &sample);
    if (sample == NULL)
      break;
    if (last_sample)
      gst_sample_unref (last_sample);
    last_sample = sample;
    GST_DEBUG ("sample pulled");
  } while (TRUE);
  gst_sample_unref (last_sample);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

GST_START_TEST (test_obscured_skipped)
{
  gint xpos0, xpos1;
  gint ypos0, ypos1;
  gint width0, width1;
  gint height0, height1;
  gint out_width, out_height;
  gdouble alpha0, alpha1;
  const gchar *caps_str;

  caps_str = "video/x-raw, format=I420";
  buffer_mapped = FALSE;
  /* Set else to compositor defaults */
  alpha0 = alpha1 = 1.0;
  xpos0 = xpos1 = ypos0 = ypos1 = 0;
  width0 = width1 = height0 = height1 = 0;
  out_width = out_height = 0;

  GST_INFO ("testing defaults");
  /* With everything at defaults, sink_1 will obscure sink_0, so buffers from
   * sink_0 will never get mapped by compositor. To verify, run with
   * GST_DEBUG=compositor:6 and look for "Obscured by" messages */
  _test_obscured (caps_str, xpos0, ypos0, width0, height0, alpha0, xpos1, ypos1,
      width1, height1, alpha1, out_width, out_height);
  fail_unless (buffer_mapped == FALSE);
  buffer_mapped = FALSE;

  caps_str = "video/x-raw,format=ARGB";
  GST_INFO ("testing video with alpha channel");
  _test_obscured (caps_str, xpos0, ypos0, width0, height0, alpha0, xpos1, ypos1,
      width1, height1, alpha1, out_width, out_height);
  fail_unless (buffer_mapped == TRUE);
  caps_str = "video/x-raw, format=I420";
  buffer_mapped = FALSE;

  alpha1 = 0.0;
  GST_INFO ("testing alpha1 = %.2g", alpha1);
  _test_obscured (caps_str, xpos0, ypos0, width0, height0, alpha0, xpos1, ypos1,
      width1, height1, alpha1, out_width, out_height);
  fail_unless (buffer_mapped == TRUE);
  alpha1 = 1.0;
  buffer_mapped = FALSE;

  /* Test 0.1, ..., 0.9 */
  for (alpha1 = 1; alpha1 < 10; alpha1 += 1) {
    GST_INFO ("testing alpha1 = %.2g", alpha1 / 10);
    _test_obscured (caps_str, xpos0, ypos0, width0, height0, alpha0, xpos1,
        ypos1, width1, height1, alpha1 / 10, out_width, out_height);
    fail_unless (buffer_mapped == TRUE);
  }
  alpha1 = 1.0;
  buffer_mapped = FALSE;

  width1 = height1 = 10;
  GST_INFO ("testing smaller sink_1");
  _test_obscured (caps_str, xpos0, ypos0, width0, height0, alpha0, xpos1, ypos1,
      width1, height1, alpha1, out_width, out_height);
  fail_unless (buffer_mapped == TRUE);
  width1 = height1 = 0;
  buffer_mapped = FALSE;

  width0 = height0 = width1 = height1 = 10;
  GST_INFO ("testing smaller sink_1 and sink0 (same sizes)");
  _test_obscured (caps_str, xpos0, ypos0, width0, height0, alpha0, xpos1, ypos1,
      width1, height1, alpha1, out_width, out_height);
  fail_unless (buffer_mapped == FALSE);
  width0 = height0 = width1 = height1 = 0;
  buffer_mapped = FALSE;

  width0 = height0 = 20;
  width1 = height1 = 10;
  GST_INFO ("testing smaller sink_1 and sink0 (sink_0 > sink_1)");
  _test_obscured (caps_str, xpos0, ypos0, width0, height0, alpha0, xpos1, ypos1,
      width1, height1, alpha1, out_width, out_height);
  fail_unless (buffer_mapped == TRUE);
  width0 = height0 = width1 = height1 = 0;
  buffer_mapped = FALSE;

  width0 = height0 = 10;
  width1 = height1 = 20;
  GST_INFO ("testing smaller sink_1 and sink0 (sink_0 < sink_1)");
  _test_obscured (caps_str, xpos0, ypos0, width0, height0, alpha0, xpos1, ypos1,
      width1, height1, alpha1, out_width, out_height);
  fail_unless (buffer_mapped == FALSE);
  width0 = height0 = width1 = height1 = 0;
  buffer_mapped = FALSE;

  xpos0 = ypos0 = 10;
  xpos1 = ypos1 = 20;
  GST_INFO ("testing offset");
  _test_obscured (caps_str, xpos0, ypos0, width0, height0, alpha0, xpos1, ypos1,
      width1, height1, alpha1, out_width, out_height);
  fail_unless (buffer_mapped == TRUE);
  xpos0 = ypos0 = xpos1 = ypos1 = 0;
  buffer_mapped = FALSE;

  xpos1 = ypos1 = 0;
  xpos0 = ypos0 = width0 = height0 = width1 = height1 = 10;
  out_width = out_height = 20;
  GST_INFO ("testing bug 754107");
  _test_obscured (caps_str, xpos0, ypos0, width0, height0, alpha0, xpos1, ypos1,
      width1, height1, alpha1, out_width, out_height);
  fail_unless (buffer_mapped == TRUE);
  xpos0 = ypos0 = xpos1 = ypos1 = width0 = height0 = width1 = height1 = 0;
  out_width = out_height = 0;
  buffer_mapped = FALSE;

  xpos1 = -1;
  xpos0 = ypos0 = width0 = height0 = width1 = height1 = 10;
  out_width = out_height = 20;
  GST_INFO ("testing bug 754576");
  _test_obscured (caps_str, xpos0, ypos0, width0, height0, alpha0, xpos1, ypos1,
      width1, height1, alpha1, out_width, out_height);
  fail_unless (buffer_mapped == TRUE);
  xpos0 = xpos1 = ypos1 = width0 = height0 = width1 = height1 = 0;
  out_width = out_height = 0;
  buffer_mapped = FALSE;

  xpos0 = ypos0 = 10000;
  out_width = 320;
  out_height = 240;
  GST_INFO ("testing sink_0 outside the frame");
  _test_obscured (caps_str, xpos0, ypos0, width0, height0, alpha0, xpos1, ypos1,
      width1, height1, alpha1, out_width, out_height);
  fail_unless (buffer_mapped == FALSE);
  xpos0 = ypos0 = out_width = out_height = 0;
  buffer_mapped = FALSE;
}

GST_END_TEST;

static void
_pipeline_eos (GstBus * bus, GstMessage * message, GstPipeline * bin)
{
  GST_INFO ("pipeline EOS");
  g_main_loop_quit (main_loop);
}

static GstFlowReturn
_buffer_recvd (GstElement * appsink, gint * buffers_recvd)
{
  GstSample *sample;

  g_signal_emit_by_name (appsink, "pull-sample", &sample);
  ck_assert_msg (sample != NULL, "NULL sample received!");

  (*buffers_recvd)++;
  GST_INFO ("buffer recvd");
  gst_sample_unref (sample);

  if (*buffers_recvd > 5)
    g_main_loop_quit (main_loop);

  return GST_FLOW_OK;
}

static void
_link_videotestsrc_with_compositor (GstElement * src, GstElement * compositor,
    gboolean repeat_after_eos)
{
  GstPad *srcpad, *sinkpad;
  GstPadLinkReturn link_res;

  srcpad = gst_element_get_static_pad (src, "src");
  sinkpad = gst_element_request_pad_simple (compositor, "sink_%u");
  /* When "repeat-after-eos" is set, compositor will keep sending the last buffer even
   * after EOS, so we will receive more buffers than we sent. */
  g_object_set (sinkpad, "repeat-after-eos", repeat_after_eos, NULL);
  link_res = gst_pad_link (srcpad, sinkpad);
  ck_assert_msg (GST_PAD_LINK_SUCCESSFUL (link_res), "videotestsrc -> "
      "compositor pad  link failed: %i", link_res);
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);
}

static void
run_test_repeat_after_eos (gint num_buffers1, gboolean repeat_after_eos1,
    gint num_buffers2, gboolean repeat_after_eos2, gint num_buffers3,
    gboolean repeat_after_eos3, gboolean result_equal)
{
  gboolean res;
  gint buffers_recvd, buffers_cnt;
  GstStateChangeReturn state_res;
  GstElement *bin, *src, *src2, *src3, *compositor, *appsink;
  GstBus *bus;

  GST_INFO ("preparing test");

  /* _buffer_recvd assumes we don't deal with buffer count larger than 5 */
  ck_assert_int_le (num_buffers1, 5);
  ck_assert_int_le (num_buffers2, 5);
  ck_assert_int_le (num_buffers3, 5);
  buffers_cnt = MAX (num_buffers1, MAX (num_buffers2, num_buffers3));

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src = gst_element_factory_make ("videotestsrc", NULL);
  g_object_set (src, "num-buffers", num_buffers1, NULL);
  compositor = gst_element_factory_make ("compositor", NULL);
  appsink = gst_element_factory_make ("appsink", NULL);
  g_object_set (appsink, "emit-signals", TRUE, NULL);
  gst_bin_add_many (GST_BIN (bin), src, compositor, appsink, NULL);
  if (num_buffers2) {
    src2 = gst_element_factory_make ("videotestsrc", NULL);
    g_object_set (src2, "num-buffers", num_buffers2, NULL);
    gst_bin_add (GST_BIN (bin), src2);
  }
  if (num_buffers3) {
    src3 = gst_element_factory_make ("videotestsrc", NULL);
    g_object_set (src3, "num-buffers", num_buffers3, NULL);
    gst_bin_add (GST_BIN (bin), src3);
  }

  res = gst_element_link (compositor, appsink);
  ck_assert_msg (res == TRUE, "Could not link compositor with appsink");
  _link_videotestsrc_with_compositor (src, compositor, repeat_after_eos1);
  if (num_buffers2) {
    _link_videotestsrc_with_compositor (src2, compositor, repeat_after_eos2);
  }
  if (num_buffers3) {
    _link_videotestsrc_with_compositor (src3, compositor, repeat_after_eos3);
  }

  GST_INFO ("pipeline built, connecting signals");

  buffers_recvd = 0;
  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_msg (state_res != GST_STATE_CHANGE_FAILURE, "Pipeline didn't play");

  main_loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (bus, "message::error", G_CALLBACK (message_received), bin);
  g_signal_connect (bus, "message::warning", G_CALLBACK (message_received),
      bin);
  g_signal_connect (bus, "message::eos", G_CALLBACK (_pipeline_eos), bin);
  g_signal_connect (appsink, "new-sample", G_CALLBACK (_buffer_recvd),
      &buffers_recvd);

  GST_INFO ("starting test");
  g_main_loop_run (main_loop);

  state_res = gst_element_set_state (bin, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  if (result_equal) {
    ck_assert_msg (buffers_recvd == buffers_cnt,
        "Did not receive equal amount of buffers than were sent");
  } else {
    ck_assert_msg (buffers_recvd > buffers_cnt,
        "Did not receive more buffers than were sent");
  }

  /* cleanup */
  g_main_loop_unref (main_loop);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_START_TEST (test_repeat_after_eos_1pad)
{
  run_test_repeat_after_eos (5, TRUE, 0, FALSE, 0, FALSE, FALSE);
}

GST_END_TEST;

GST_START_TEST (test_repeat_after_eos_2pads_repeating_first)
{
  run_test_repeat_after_eos (2, TRUE, 5, FALSE, 0, FALSE, TRUE);
}

GST_END_TEST;

GST_START_TEST (test_repeat_after_eos_2pads_repeating_last)
{
  run_test_repeat_after_eos (5, FALSE, 2, TRUE, 0, FALSE, TRUE);
}

GST_END_TEST;

GST_START_TEST (test_repeat_after_eos_3pads)
{
  run_test_repeat_after_eos (5, FALSE, 2, TRUE, 3, FALSE, TRUE);
}

GST_END_TEST;

GST_START_TEST (test_repeat_after_eos_3pads_repeat_eos_last)
{
  run_test_repeat_after_eos (3, FALSE, 2, FALSE, 5, TRUE, TRUE);
}

GST_END_TEST;

GST_START_TEST (test_repeat_after_eos_3pads_all_repeating)
{
  run_test_repeat_after_eos (2, TRUE, 5, TRUE, 3, TRUE, FALSE);
}

GST_END_TEST;

GST_START_TEST (test_repeat_after_eos_3pads_no_repeating)
{
  run_test_repeat_after_eos (2, FALSE, 5, FALSE, 3, FALSE, TRUE);
}

GST_END_TEST;

/* Test that the GST_ELEMENT(vagg)->sinkpads GList is always sorted by zorder */
GST_START_TEST (test_pad_z_order)
{
  GstElement *compositor;
  GstPad *sinkpad1, *sinkpad2, *sinkpad3;
  guint zorder1, zorder2;
  GList *sinkpads;

  GST_INFO ("preparing test");

  compositor = gst_element_factory_make ("compositor", NULL);
  sinkpad1 = gst_element_request_pad_simple (compositor, "sink_%u");
  sinkpad2 = gst_element_request_pad_simple (compositor, "sink_%u");

  /* Pads requested later have a higher z-order than earlier ones by default */
  g_object_get (sinkpad1, "zorder", &zorder1, NULL);
  g_object_get (sinkpad2, "zorder", &zorder2, NULL);
  ck_assert_int_gt (zorder2, zorder1);
  sinkpads = GST_ELEMENT (compositor)->sinkpads;
  ck_assert_ptr_eq (sinkpads->data, sinkpad1);
  ck_assert_ptr_eq (sinkpads->next->data, sinkpad2);

  /* Make sinkpad1's zorder the largest, which should re-sort the sinkpads */
  g_object_set (sinkpad1, "zorder", zorder2 + 1, NULL);
  sinkpads = GST_ELEMENT (compositor)->sinkpads;
  ck_assert_ptr_eq (sinkpads->data, sinkpad2);
  ck_assert_ptr_eq (sinkpads->next->data, sinkpad1);

  /* Get a new pad, which should be the highest pad now */
  sinkpad3 = gst_element_request_pad_simple (compositor, "sink_%u");
  sinkpads = GST_ELEMENT (compositor)->sinkpads;
  ck_assert_ptr_eq (sinkpads->data, sinkpad2);
  ck_assert_ptr_eq (sinkpads->next->data, sinkpad1);
  ck_assert_ptr_eq (sinkpads->next->next->data, sinkpad3);

  /* cleanup */
  gst_object_unref (compositor);
  gst_object_unref (sinkpad1);
  gst_object_unref (sinkpad2);
  gst_object_unref (sinkpad3);
}

GST_END_TEST;

/*
 * Test that the pad numbering assigned by aggregator behaves as follows:
 * 1. If a pad number is requested, it must be assigned if it is available
 * 2. When numbering automatically, the largest available pad number is used
 * 3. Pad names must be unique
 */
GST_START_TEST (test_pad_numbering)
{
  GstElement *mixer;
  GstPad *sinkpad1, *sinkpad2, *sinkpad3, *sinkpad4;

  GST_INFO ("preparing test");

  mixer = gst_element_factory_make ("compositor", NULL);
  sinkpad1 = gst_element_request_pad_simple (mixer, "sink_%u");
  sinkpad2 = gst_element_request_pad_simple (mixer, "sink_7");
  sinkpad3 = gst_element_request_pad_simple (mixer, "sink_1");
  sinkpad4 = gst_element_request_pad_simple (mixer, "sink_%u");

  ck_assert_str_eq (GST_PAD_NAME (sinkpad1), "sink_0");
  ck_assert_str_eq (GST_PAD_NAME (sinkpad2), "sink_7");
  ck_assert_str_eq (GST_PAD_NAME (sinkpad3), "sink_1");
  ck_assert_str_eq (GST_PAD_NAME (sinkpad4), "sink_8");

  /* cleanup */
  gst_object_unref (mixer);
  gst_object_unref (sinkpad1);
  gst_object_unref (sinkpad2);
  gst_object_unref (sinkpad3);
  gst_object_unref (sinkpad4);
}

GST_END_TEST;

typedef struct
{
  gint buffers_sent;
  GstClockTime first_pts;
  gboolean first;
  gboolean drop;
} TestStartTimeSelectionData;

static GstPadProbeReturn
drop_buffer_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  TestStartTimeSelectionData *data = user_data;

  if (data->drop) {
    data->buffers_sent = data->buffers_sent + 1;
    if (data->buffers_sent < 4)
      return GST_PAD_PROBE_DROP;
  }

  data->first_pts = GST_BUFFER_PTS (info->data);

  return GST_PAD_PROBE_REMOVE;
}

static GstFlowReturn
first_buffer_received_cb (GstElement * appsink, gpointer user_data)
{
  TestStartTimeSelectionData *data = user_data;
  GstSample *sample;
  GstBuffer *buffer;

  g_signal_emit_by_name (appsink, "pull-sample", &sample);
  ck_assert_msg (sample != NULL, "NULL sample received!");

  buffer = gst_sample_get_buffer (sample);
  if (!data->first) {
    fail_unless_equals_uint64 (GST_BUFFER_PTS (buffer), 0);
  } else {
    fail_unless_equals_uint64 (GST_BUFFER_PTS (buffer), data->first_pts);
  }

  gst_sample_unref (sample);

  g_main_loop_quit (main_loop);

  return GST_FLOW_EOS;
}

static void
run_test_start_time (gboolean first, gboolean drop, gboolean unlinked)
{
  gboolean res;
  GstPadLinkReturn link_res;
  GstStateChangeReturn state_res;
  GstElement *bin, *src, *compositor, *appsink;
  GstPad *srcpad, *sinkpad;
  GstBus *bus;
  TestStartTimeSelectionData data = { 0, GST_CLOCK_TIME_NONE, first, drop };

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src = gst_element_factory_make ("videotestsrc", NULL);

  srcpad = gst_element_get_static_pad (src, "src");
  gst_pad_add_probe (srcpad, GST_PAD_PROBE_TYPE_BUFFER, drop_buffer_cb, &data,
      NULL);
  gst_object_unref (srcpad);

  g_object_set (src, "is-live", TRUE, NULL);
  compositor = gst_element_factory_make ("compositor", NULL);
  g_object_set (compositor, "start-time-selection", (first ? 1 : 0), NULL);
  appsink = gst_element_factory_make ("appsink", NULL);
  g_object_set (appsink, "emit-signals", TRUE, NULL);
  gst_bin_add_many (GST_BIN (bin), src, compositor, appsink, NULL);

  res = gst_element_link (compositor, appsink);
  ck_assert_msg (res == TRUE, "Could not link compositor with appsink");
  srcpad = gst_element_get_static_pad (src, "src");
  sinkpad = gst_element_request_pad_simple (compositor, "sink_%u");
  link_res = gst_pad_link (srcpad, sinkpad);
  ck_assert_msg (GST_PAD_LINK_SUCCESSFUL (link_res), "videotestsrc -> "
      "compositor pad  link failed: %i", link_res);
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  if (unlinked) {
    sinkpad = gst_element_request_pad_simple (compositor, "sink_%u");
    gst_object_unref (sinkpad);
  }

  GST_INFO ("pipeline built, connecting signals");

  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_msg (state_res != GST_STATE_CHANGE_FAILURE, "Pipeline didn't play");

  main_loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (bus, "message::error", G_CALLBACK (message_received), bin);
  g_signal_connect (bus, "message::warning", G_CALLBACK (message_received),
      bin);
  g_signal_connect (bus, "message::eos", G_CALLBACK (_pipeline_eos), bin);
  g_signal_connect (appsink, "new-sample",
      G_CALLBACK (first_buffer_received_cb), &data);

  GST_INFO ("starting test");
  g_main_loop_run (main_loop);

  state_res = gst_element_set_state (bin, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* cleanup */
  g_main_loop_unref (main_loop);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_START_TEST (test_start_time_zero_live_drop_0)
{
  run_test_start_time (FALSE, FALSE, FALSE);
}

GST_END_TEST;

GST_START_TEST (test_start_time_zero_live_drop_3)
{
  run_test_start_time (FALSE, TRUE, FALSE);
}

GST_END_TEST;

GST_START_TEST (test_start_time_zero_live_drop_3_unlinked_1)
{
  run_test_start_time (FALSE, TRUE, TRUE);
}

GST_END_TEST;

GST_START_TEST (test_start_time_first_live_drop_0)
{
  run_test_start_time (TRUE, FALSE, FALSE);
}

GST_END_TEST;

GST_START_TEST (test_start_time_first_live_drop_3)
{
  run_test_start_time (TRUE, TRUE, FALSE);
}

GST_END_TEST;

GST_START_TEST (test_start_time_first_live_drop_3_unlinked_1)
{
  run_test_start_time (TRUE, TRUE, TRUE);
}

GST_END_TEST;

GST_START_TEST (test_gap_events)
{
  GstBuffer *buf;
  GstElement *comp = gst_element_factory_make ("compositor", NULL);
  GstHarness *h = gst_harness_new_with_element (comp, "sink_%u", "src");
  GstMapInfo info;

  g_object_set (comp, "background", 1, NULL);

  gst_harness_set_src_caps_str (h,
      "video/x-raw, format=RGBA, width=1, height=1, framerate=25/1");

  gst_harness_play (h);

  gst_harness_push_event (h, gst_event_new_gap (0, 40 * GST_MSECOND));

  buf = gst_buffer_new_allocate (NULL, 4, NULL);

  gst_buffer_map (buf, &info, GST_MAP_WRITE);
  memset (info.data, 42, info.size);
  info.data[3] = 255;
  gst_buffer_unmap (buf, &info);

  GST_BUFFER_PTS (buf) = 40 * GST_MSECOND;
  GST_BUFFER_DURATION (buf) = 40 * GST_MSECOND;
  gst_harness_push (h, buf);

  buf = gst_harness_pull (h);
  gst_buffer_map (buf, &info, GST_MAP_READ);
  fail_unless (info.data[0] == 0);
  gst_buffer_unmap (buf, &info);
  gst_buffer_unref (buf);

  buf = gst_harness_pull (h);
  gst_buffer_map (buf, &info, GST_MAP_READ);
  fail_unless (info.data[0] == 42);
  gst_buffer_unmap (buf, &info);
  gst_buffer_unref (buf);

  gst_harness_teardown (h);
  gst_object_unref (comp);
}

GST_END_TEST;

static GstBuffer *expected_selected_buffer = NULL;

static void
samples_selected_cb (GstAggregator * agg, GstSegment * segment,
    GstClockTime pts, GstClockTime dts, GstClockTime duration,
    GstStructure * info, gint * called)
{
  GstPad *pad;
  GstSample *sample;

  pad = gst_element_get_static_pad (GST_ELEMENT (agg), "sink_0");
  sample = gst_aggregator_peek_next_sample (agg, GST_AGGREGATOR_PAD (pad));
  fail_unless (sample != NULL);
  fail_unless (gst_sample_get_buffer (sample) == expected_selected_buffer);
  gst_sample_unref (sample);
  gst_object_unref (pad);

  *called += 1;
}

static void
buffer_consumed_cb (GstAggregator * agg, GstBuffer * unused, gint * called)
{
  *called += 1;
}

GST_START_TEST (test_signals)
{
  gint samples_selected_called = 0;
  gint buffer_consumed_called = 0;
  GstBuffer *buf;
  GstElement *comp = gst_element_factory_make ("compositor", NULL);
  GstHarness *h = gst_harness_new_with_element (comp, "sink_%u", "src");
  GstPad *pad;

  g_object_set (comp, "emit-signals", TRUE, NULL);
  g_signal_connect (comp, "samples-selected", G_CALLBACK (samples_selected_cb),
      &samples_selected_called);

  pad = gst_element_get_static_pad (comp, "sink_0");
  g_object_set (pad, "emit-signals", TRUE, NULL);
  g_signal_connect (pad, "buffer-consumed", G_CALLBACK (buffer_consumed_cb),
      &buffer_consumed_called);
  gst_object_unref (pad);

  gst_harness_set_sink_caps_str (h,
      "video/x-raw, format=RGBA, width=1, height=1, framerate=1/1");
  gst_harness_set_src_caps_str (h,
      "video/x-raw, format=RGBA, width=1, height=1, framerate=2/1");

  gst_harness_play (h);

  buf = gst_buffer_new_allocate (NULL, 4, NULL);
  GST_BUFFER_PTS (buf) = 0;
  GST_BUFFER_DURATION (buf) = GST_SECOND / 2;
  expected_selected_buffer = buf;
  gst_harness_push (h, buf);
  buf = gst_harness_pull (h);
  gst_buffer_unref (buf);
  fail_unless_equals_int (samples_selected_called, 1);
  fail_unless_equals_int (buffer_consumed_called, 1);

  /* This next buffer should be discarded */
  buf = gst_buffer_new_allocate (NULL, 4, NULL);
  GST_BUFFER_PTS (buf) = GST_SECOND / 2;
  GST_BUFFER_DURATION (buf) = GST_SECOND / 2;
  gst_harness_push (h, buf);

  buf = gst_buffer_new_allocate (NULL, 4, NULL);
  GST_BUFFER_PTS (buf) = GST_SECOND;
  GST_BUFFER_DURATION (buf) = GST_SECOND / 2;
  expected_selected_buffer = buf;
  gst_harness_push (h, buf);
  buf = gst_harness_pull (h);
  gst_buffer_unref (buf);
  fail_unless_equals_int (samples_selected_called, 2);
  fail_unless_equals_int (buffer_consumed_called, 3);

  gst_harness_teardown (h);
  gst_object_unref (comp);
}

GST_END_TEST;

static void
on_reverse_handoff (GstElement * sink, GstBuffer * buffer, GstPad * pad,
    GstClockTime * pos)
{
  GstClockTime pts = GST_BUFFER_PTS (buffer);
  GstClockTime dur = GST_BUFFER_DURATION (buffer);

  fail_unless (GST_CLOCK_TIME_IS_VALID (pts));
  fail_unless_equals_clocktime (dur, GST_MSECOND * 100);

  if (!GST_CLOCK_TIME_IS_VALID (*pos)) {
    *pos = pts;
  } else {
    fail_unless (pts < *pos);
    *pos = pts;
  }
}

GST_START_TEST (test_reverse)
{
  GstElement *bin, *src1, *src2, *compositor, *sink;
  GstElement *cp1, *cp2, *cp3;
  GstCaps *caps;
  GstBus *bus;
  GstEvent *seek_event;
  GstStateChangeReturn state_res;
  gboolean res;
  GstClockTime pos = GST_CLOCK_TIME_NONE;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src1 = gst_element_factory_make ("videotestsrc", "src1");
  src2 = gst_element_factory_make ("videotestsrc", "src2");
  compositor = gst_element_factory_make ("compositor", "compositor");
  cp1 = gst_element_factory_make ("capsfilter", "cp1");
  cp2 = gst_element_factory_make ("capsfilter", "cp2");
  cp3 = gst_element_factory_make ("capsfilter", "cp3");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src1, src2, compositor, sink, cp1, cp2,
      cp3, NULL);

  res = gst_element_link_many (src1, cp1, compositor, NULL);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link_many (src2, cp2, compositor, NULL);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link_many (compositor, cp3, sink, NULL);
  fail_unless (res == TRUE, NULL);

  caps = gst_caps_from_string ("video/x-raw,width=(int)64,height=(int)64,"
      "framerate=(fraction)10/1");
  fail_unless (caps != NULL, NULL);

  g_object_set (cp1, "caps", caps, NULL);
  g_object_set (cp2, "caps", caps, NULL);
  g_object_set (cp3, "caps", caps, NULL);
  gst_caps_unref (caps);

  seek_event = gst_event_new_seek (-1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_TRICKMODE,
      GST_SEEK_TYPE_SET, (GstClockTime) 0,
      GST_SEEK_TYPE_SET, (GstClockTime) 2 * GST_SECOND);

  main_loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  GST_INFO ("starting test");

  /* prepare playing */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* wait for completion */
  state_res = gst_element_get_state (bin, NULL, NULL, GST_CLOCK_TIME_NONE);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  g_object_set (sink, "signal-handoffs", TRUE, NULL);
  g_signal_connect (sink, "handoff", G_CALLBACK (on_reverse_handoff), &pos);

  res = gst_element_send_event (bin, seek_event);
  fail_unless (res == TRUE, NULL);

  /* run pipeline */
  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  GST_INFO ("running main loop");
  g_main_loop_run (main_loop);

  state_res = gst_element_set_state (bin, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  fail_unless_equals_clocktime (pos, 0);

  /* cleanup */
  g_main_loop_unref (main_loop);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

static void
eos_test_msg_cb (GstBus * bus, GstMessage * message, gboolean * got_eos)
{
  switch (message->type) {
    case GST_MESSAGE_EOS:
      *got_eos = TRUE;
      g_main_loop_quit (main_loop);
      break;
    case GST_MESSAGE_ERROR:
      g_main_loop_quit (main_loop);
      break;
    default:
      break;
  }
}

typedef struct
{
  GMutex lock;
  GCond cond;
  GstBuffer *buf;
} EosTestData;

static void
after_eos_handoff_buffer_cb (GstElement * sink, GstBuffer * buf, GstPad * pad,
    EosTestData * data)
{
  g_mutex_lock (&data->lock);
  gst_buffer_replace (&data->buf, buf);
  g_cond_signal (&data->cond);
  g_mutex_unlock (&data->lock);
}

/* This test checks that aggregator can restart after outputting EOS if
 * one of its input streams does so.
 */
GST_START_TEST (test_stream_start_after_eos)
{
  GstElement *bin, *src1, *src2, *compositor, *sink;
  GstElement *cp1, *cp2, *cp3;
  GstCaps *caps;
  GstBus *bus;
  GstStateChangeReturn state_res;
  gboolean res;
  gboolean got_eos = FALSE;
  EosTestData test_data;

  g_mutex_init (&test_data.lock);
  g_cond_init (&test_data.cond);
  test_data.buf = NULL;

  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src1 = gst_element_factory_make ("videotestsrc", NULL);
  src2 = gst_element_factory_make ("videotestsrc", NULL);
  compositor = gst_element_factory_make ("compositor", NULL);
  cp1 = gst_element_factory_make ("capsfilter", NULL);
  cp2 = gst_element_factory_make ("capsfilter", NULL);
  cp3 = gst_element_factory_make ("capsfilter", NULL);
  sink = gst_element_factory_make ("fakesink", NULL);
  gst_bin_add_many (GST_BIN (bin), src1, src2, compositor, sink, cp1, cp2,
      cp3, NULL);

  g_object_set (src1, "num-buffers", 1, "is-live", TRUE, NULL);
  g_object_set (src2, "num-buffers", 1, "is-live", TRUE, NULL);

  res = gst_element_link_many (src1, cp1, compositor, NULL);
  fail_unless (res);
  res = gst_element_link_many (src2, cp2, compositor, NULL);
  fail_unless (res);
  res = gst_element_link_many (compositor, cp3, sink, NULL);
  fail_unless (res);

  caps = gst_caps_from_string ("video/x-raw,width=(int)64,height=(int)64,"
      "framerate=(fraction)10/1");
  fail_unless (caps != NULL);

  g_object_set (cp1, "caps", caps, NULL);
  g_object_set (cp2, "caps", caps, NULL);
  g_object_set (cp3, "caps", caps, NULL);
  gst_caps_unref (caps);

  main_loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (bus, "message::error", (GCallback) eos_test_msg_cb,
      &got_eos);
  g_signal_connect (bus, "message::eos", (GCallback) eos_test_msg_cb, &got_eos);

  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  g_main_loop_run (main_loop);

  /* Wait for EOS */
  fail_unless (got_eos);

  /* Configure new input */
  g_object_set (sink, "signal-handoffs", TRUE, NULL);
  g_signal_connect (sink, "handoff", (GCallback) after_eos_handoff_buffer_cb,
      &test_data);

  /* Run a source again */
  state_res = gst_element_set_state (src1, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);
  gst_element_sync_state_with_parent (src1);

  g_mutex_lock (&test_data.lock);
  while (!test_data.buf)
    g_cond_wait (&test_data.cond, &test_data.lock);
  g_mutex_unlock (&test_data.lock);

  state_res = gst_element_set_state (bin, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* cleanup */
  gst_buffer_unref (test_data.buf);
  g_main_loop_unref (main_loop);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);

  g_mutex_clear (&test_data.lock);
  g_cond_clear (&test_data.cond);
}

GST_END_TEST;

/* This test checks that aggregator restarts after outputting an EOS
 * when a new input stream is added
 */
GST_START_TEST (test_new_pad_after_eos)
{
  GstElement *bin, *src1, *src2, *src3, *compositor, *sink;
  GstElement *cp1, *cp2, *cp3, *cp4;
  GstCaps *caps;
  GstBus *bus;
  GstStateChangeReturn state_res;
  gboolean res;
  gboolean got_eos = FALSE;
  EosTestData test_data;

  g_mutex_init (&test_data.lock);
  g_cond_init (&test_data.cond);
  test_data.buf = NULL;

  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src1 = gst_element_factory_make ("videotestsrc", NULL);
  src2 = gst_element_factory_make ("videotestsrc", NULL);
  src3 = gst_element_factory_make ("videotestsrc", NULL);
  compositor = gst_element_factory_make ("compositor", NULL);
  cp1 = gst_element_factory_make ("capsfilter", NULL);
  cp2 = gst_element_factory_make ("capsfilter", NULL);
  cp3 = gst_element_factory_make ("capsfilter", NULL);
  cp4 = gst_element_factory_make ("capsfilter", NULL);
  sink = gst_element_factory_make ("fakesink", NULL);
  gst_bin_add_many (GST_BIN (bin), src1, src2, compositor, sink, cp1, cp2,
      cp3, NULL);

  g_object_set (src1, "num-buffers", 1, "is-live", TRUE, NULL);
  g_object_set (src2, "num-buffers", 1, "is-live", TRUE, NULL);
  g_object_set (src3, "num-buffers", 1, "is-live", TRUE, NULL);

  res = gst_element_link_many (src1, cp1, compositor, NULL);
  fail_unless (res);
  res = gst_element_link_many (src2, cp2, compositor, NULL);
  fail_unless (res);
  res = gst_element_link_many (compositor, cp3, sink, NULL);
  fail_unless (res);

  caps = gst_caps_from_string ("video/x-raw,width=(int)64,height=(int)64,"
      "framerate=(fraction)10/1");
  fail_unless (caps != NULL);

  g_object_set (cp1, "caps", caps, NULL);
  g_object_set (cp2, "caps", caps, NULL);
  g_object_set (cp3, "caps", caps, NULL);
  g_object_set (cp4, "caps", caps, NULL);
  gst_caps_unref (caps);

  main_loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (bus, "message::error", (GCallback) eos_test_msg_cb,
      &got_eos);
  g_signal_connect (bus, "message::eos", (GCallback) eos_test_msg_cb, &got_eos);

  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  g_main_loop_run (main_loop);

  /* Wait for EOS */
  fail_unless (got_eos);

  /* Configure new input */
  g_object_set (sink, "signal-handoffs", TRUE, NULL);
  g_signal_connect (sink, "handoff", (GCallback) after_eos_handoff_buffer_cb,
      &test_data);

  gst_bin_add_many (GST_BIN (bin), src3, cp4, NULL);
  res = gst_element_link_many (src3, cp4, compositor, NULL);
  fail_unless (res);

  gst_element_sync_state_with_parent (cp4);
  gst_element_sync_state_with_parent (src3);

  g_mutex_lock (&test_data.lock);
  while (!test_data.buf)
    g_cond_wait (&test_data.cond, &test_data.lock);
  g_mutex_unlock (&test_data.lock);

  state_res = gst_element_set_state (bin, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* cleanup */
  gst_buffer_unref (test_data.buf);
  g_main_loop_unref (main_loop);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);

  g_mutex_clear (&test_data.lock);
  g_cond_clear (&test_data.cond);
}

GST_END_TEST;

static Suite *
compositor_suite (void)
{
  Suite *s = suite_create ("compositor");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_caps);
  tcase_add_test (tc_chain, test_event);
  tcase_add_test (tc_chain, test_navigation_events);
  tcase_add_test (tc_chain, test_caps_query);
  tcase_add_test (tc_chain, test_caps_query_interlaced);
  tcase_add_test (tc_chain, test_late_caps_query);
  tcase_add_test (tc_chain, test_late_caps_different_interlaced);
  tcase_add_test (tc_chain, test_play_twice);
  tcase_add_test (tc_chain, test_play_twice_then_add_and_play_again);
  tcase_add_test (tc_chain, test_add_pad);
  tcase_add_test (tc_chain, test_remove_pad);
  tcase_add_test (tc_chain, test_clip);
  tcase_add_test (tc_chain, test_duration_is_max);
  tcase_add_test (tc_chain, test_duration_unknown_overrides);
  tcase_add_test (tc_chain, test_loop);
  tcase_add_test (tc_chain, test_segment_base_handling);
  tcase_add_test (tc_chain, test_obscured_skipped);
  tcase_add_test (tc_chain, test_repeat_after_eos_1pad);
  tcase_add_test (tc_chain, test_repeat_after_eos_2pads_repeating_first);
  tcase_add_test (tc_chain, test_repeat_after_eos_2pads_repeating_last);
  tcase_add_test (tc_chain, test_repeat_after_eos_3pads);
  tcase_add_test (tc_chain, test_repeat_after_eos_3pads_repeat_eos_last);
  tcase_add_test (tc_chain, test_repeat_after_eos_3pads_all_repeating);
  tcase_add_test (tc_chain, test_repeat_after_eos_3pads_no_repeating);
  tcase_add_test (tc_chain, test_pad_z_order);
  tcase_add_test (tc_chain, test_pad_numbering);
  tcase_add_test (tc_chain, test_start_time_zero_live_drop_0);
  tcase_add_test (tc_chain, test_start_time_zero_live_drop_3);
  tcase_add_test (tc_chain, test_start_time_zero_live_drop_3_unlinked_1);
  tcase_add_test (tc_chain, test_start_time_first_live_drop_0);
  tcase_add_test (tc_chain, test_start_time_first_live_drop_3);
  tcase_add_test (tc_chain, test_start_time_first_live_drop_3_unlinked_1);
  tcase_add_test (tc_chain, test_gap_events);
  tcase_add_test (tc_chain, test_signals);
  tcase_add_test (tc_chain, test_reverse);
  tcase_add_test (tc_chain, test_stream_start_after_eos);
  tcase_add_test (tc_chain, test_new_pad_after_eos);

  return s;
}

GST_CHECK_MAIN (compositor);
