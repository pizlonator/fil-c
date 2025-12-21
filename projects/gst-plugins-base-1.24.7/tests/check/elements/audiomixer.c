/* GStreamer
 *
 * unit test for audiomixer
 *
 * Copyright (C) 2005 Thomas Vander Stichele <thomas at apestaart dot org>
 * Copyright (C) 2013 Sebastian Dröge <sebastian@centricular.com>
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

#include <gst/check/gstharness.h>

#include <gst/check/gstcheck.h>
#include <gst/check/gstconsistencychecker.h>
#include <gst/audio/audio.h>
#include <gst/base/gstbasesrc.h>
#include <gst/controller/gstdirectcontrolbinding.h>
#include <gst/controller/gstinterpolationcontrolsource.h>

static GMainLoop *main_loop;

/* fixtures */

static void
test_setup (void)
{
  main_loop = g_main_loop_new (NULL, FALSE);
}

static void
test_teardown (void)
{
  g_main_loop_unref (main_loop);
  main_loop = NULL;
}


/* some test helpers */

static GstElement *
setup_pipeline (GstElement * audiomixer, gint num_srcs, GstElement * capsfilter)
{
  GstElement *pipeline, *src, *sink;
  gint i;

  pipeline = gst_pipeline_new ("pipeline");
  if (!audiomixer) {
    audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  }

  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (pipeline), audiomixer, sink, NULL);

  if (capsfilter) {
    gst_bin_add (GST_BIN (pipeline), capsfilter);
    gst_element_link_many (audiomixer, capsfilter, sink, NULL);
  } else {
    gst_element_link (audiomixer, sink);
  }

  for (i = 0; i < num_srcs; i++) {
    src = gst_element_factory_make ("audiotestsrc", NULL);
    g_object_set (src, "wave", 4, NULL);        /* silence */
    gst_bin_add (GST_BIN (pipeline), src);
    gst_element_link (src, audiomixer);
  }
  return pipeline;
}

static GstCaps *
get_element_sink_pad_caps (GstElement * pipeline, const gchar * element_name)
{
  GstElement *sink;
  GstCaps *caps;
  GstPad *pad;

  sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
  pad = gst_element_get_static_pad (sink, "sink");
  caps = gst_pad_get_current_caps (pad);
  gst_object_unref (pad);
  gst_object_unref (sink);

  return caps;
}

static void
set_state_and_wait (GstElement * pipeline, GstState state)
{
  GstStateChangeReturn state_res;

  /* prepare paused/playing */
  state_res = gst_element_set_state (pipeline, state);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* wait for preroll */
  state_res = gst_element_get_state (pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);
}

static gboolean
set_playing (GstElement * element)
{
  GstStateChangeReturn state_res;

  state_res = gst_element_set_state (element, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  return FALSE;
}

static void
play_and_wait (GstElement * pipeline)
{
  GstStateChangeReturn state_res;

  g_idle_add ((GSourceFunc) set_playing, pipeline);

  GST_INFO ("running main loop");
  g_main_loop_run (main_loop);

  state_res = gst_element_set_state (pipeline, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);
}

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

static GstBuffer *
new_buffer (gsize num_bytes, gint data, GstClockTime ts, GstClockTime dur,
    GstBufferFlags flags)
{
  GstMapInfo map;
  GstBuffer *buffer = gst_buffer_new_and_alloc (num_bytes);

  gst_buffer_map (buffer, &map, GST_MAP_WRITE);
  memset (map.data, data, map.size);
  gst_buffer_unmap (buffer, &map);
  GST_BUFFER_TIMESTAMP (buffer) = ts;
  GST_BUFFER_DURATION (buffer) = dur;
  if (flags)
    GST_BUFFER_FLAG_SET (buffer, flags);
  GST_DEBUG ("created buffer %p", buffer);
  return buffer;
}

/* make sure downstream gets a CAPS event before buffers are sent */
GST_START_TEST (test_caps)
{
  GstElement *pipeline;
  GstCaps *caps;

  /* build pipeline */
  pipeline = setup_pipeline (NULL, 1, NULL);

  /* prepare playing */
  set_state_and_wait (pipeline, GST_STATE_PAUSED);

  /* check caps on fakesink */
  caps = get_element_sink_pad_caps (pipeline, "sink");
  fail_unless (caps != NULL);
  gst_caps_unref (caps);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

GST_END_TEST;

/* check that caps set on the property are honoured */
GST_START_TEST (test_filter_caps)
{
  GstElement *pipeline, *audiomixer, *capsfilter;
  GstCaps *filter_caps, *caps;

  filter_caps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
      "layout", G_TYPE_STRING, "interleaved",
      "rate", G_TYPE_INT, 44100, "channels", G_TYPE_INT, 1,
      "channel-mask", GST_TYPE_BITMASK, (guint64) 0x04, NULL);

  capsfilter = gst_element_factory_make ("capsfilter", NULL);

  /* build pipeline */
  audiomixer = gst_element_factory_make ("audiomixer", NULL);
  g_object_set (capsfilter, "caps", filter_caps, NULL);
  pipeline = setup_pipeline (audiomixer, 1, capsfilter);

  /* prepare playing */
  set_state_and_wait (pipeline, GST_STATE_PAUSED);

  /* check caps on fakesink */
  caps = get_element_sink_pad_caps (pipeline, "sink");
  fail_unless (caps != NULL);
  GST_INFO_OBJECT (pipeline, "received caps: %" GST_PTR_FORMAT, caps);
  fail_unless (gst_caps_is_equal_fixed (caps, filter_caps));
  gst_caps_unref (caps);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  gst_caps_unref (filter_caps);
}

GST_END_TEST;

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
  GstElement *bin, *src1, *src2, *audiomixer, *sink;
  GstBus *bus;
  GstEvent *seek_event;
  gboolean res;
  GstPad *srcpad, *sinkpad;
  GstStreamConsistency *chk_1, *chk_2, *chk_3;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src1 = gst_element_factory_make ("audiotestsrc", "src1");
  g_object_set (src1, "wave", 4, NULL); /* silence */
  src2 = gst_element_factory_make ("audiotestsrc", "src2");
  g_object_set (src2, "wave", 4, NULL); /* silence */
  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src1, src2, audiomixer, sink, NULL);

  res = gst_element_link (src1, audiomixer);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (src2, audiomixer);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (audiomixer, sink);
  fail_unless (res == TRUE, NULL);

  srcpad = gst_element_get_static_pad (audiomixer, "src");
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

  g_signal_connect (bus, "message::segment-done",
      (GCallback) test_event_message_received, bin);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  GST_INFO ("starting test");

  /* prepare playing */
  set_state_and_wait (bin, GST_STATE_PAUSED);

  res = gst_element_send_event (bin, seek_event);
  fail_unless (res == TRUE, NULL);

  /* run pipeline */
  play_and_wait (bin);

  ck_assert_int_eq (position, 2 * GST_SECOND);

  /* cleanup */
  gst_consistency_checker_free (chk_1);
  gst_consistency_checker_free (chk_2);
  gst_consistency_checker_free (chk_3);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

static guint play_count = 0;
static GstEvent *play_seek_event = NULL;

static void
test_play_twice_message_received (GstBus * bus, GstMessage * message,
    GstElement * bin)
{
  gboolean res;
  GstStateChangeReturn state_res;

  GST_INFO ("bus message from \"%" GST_PTR_FORMAT "\": %" GST_PTR_FORMAT,
      GST_MESSAGE_SRC (message), message);

  switch (message->type) {
    case GST_MESSAGE_SEGMENT_DONE:
      play_count++;
      if (play_count == 1) {
        state_res = gst_element_set_state (bin, GST_STATE_READY);
        ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

        /* prepare playing again */
        set_state_and_wait (bin, GST_STATE_PAUSED);

        gst_event_set_seqnum (play_seek_event, gst_util_seqnum_next ());
        res = gst_element_send_event (bin, gst_event_ref (play_seek_event));
        fail_unless (res == TRUE, NULL);

        state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
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
  GstElement *bin, *audiomixer;
  GstBus *bus;
  gboolean res;
  GstPad *srcpad;
  GstStreamConsistency *consist;

  GST_INFO ("preparing test");

  /* build pipeline */
  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  bin = setup_pipeline (audiomixer, 2, NULL);
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  srcpad = gst_element_get_static_pad (audiomixer, "src");
  consist = gst_consistency_checker_new (srcpad);
  gst_object_unref (srcpad);

  play_seek_event = gst_event_new_seek (1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_FLUSH,
      GST_SEEK_TYPE_SET, (GstClockTime) 0,
      GST_SEEK_TYPE_SET, (GstClockTime) 2 * GST_SECOND);

  play_count = 0;

  g_signal_connect (bus, "message::segment-done",
      (GCallback) test_play_twice_message_received, bin);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  GST_INFO ("starting test");

  /* prepare playing */
  set_state_and_wait (bin, GST_STATE_PAUSED);

  gst_event_set_seqnum (play_seek_event, gst_util_seqnum_next ());
  res = gst_element_send_event (bin, gst_event_ref (play_seek_event));
  fail_unless (res == TRUE, NULL);

  GST_INFO ("seeked");

  /* run pipeline */
  play_and_wait (bin);

  ck_assert_int_eq (play_count, 2);

  /* cleanup */
  gst_consistency_checker_free (consist);
  gst_event_unref (play_seek_event);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

GST_START_TEST (test_play_twice_then_add_and_play_again)
{
  GstElement *bin, *src, *audiomixer;
  GstBus *bus;
  gboolean res;
  GstStateChangeReturn state_res;
  gint i;
  GstPad *srcpad;
  GstStreamConsistency *consist;

  GST_INFO ("preparing test");

  /* build pipeline */
  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  bin = setup_pipeline (audiomixer, 2, NULL);
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  srcpad = gst_element_get_static_pad (audiomixer, "src");
  consist = gst_consistency_checker_new (srcpad);
  gst_object_unref (srcpad);

  play_seek_event = gst_event_new_seek (1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_FLUSH,
      GST_SEEK_TYPE_SET, (GstClockTime) 0,
      GST_SEEK_TYPE_SET, (GstClockTime) 2 * GST_SECOND);

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
    set_state_and_wait (bin, GST_STATE_PAUSED);

    gst_event_set_seqnum (play_seek_event, gst_util_seqnum_next ());
    res = gst_element_send_event (bin, gst_event_ref (play_seek_event));
    fail_unless (res == TRUE, NULL);

    GST_INFO ("seeked");

    /* run pipeline */
    play_and_wait (bin);

    ck_assert_int_eq (play_count, 2);

    /* plug another source */
    if (i == 0) {
      src = gst_element_factory_make ("audiotestsrc", NULL);
      g_object_set (src, "wave", 4, NULL);      /* silence */
      gst_bin_add (GST_BIN (bin), src);

      res = gst_element_link (src, audiomixer);
      fail_unless (res == TRUE, NULL);
    }

    gst_consistency_checker_reset (consist);
  }

  state_res = gst_element_set_state (bin, GST_STATE_NULL);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* cleanup */
  gst_event_unref (play_seek_event);
  gst_consistency_checker_free (consist);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

/* test failing seeks on live-sources */
GST_START_TEST (test_live_seeking)
{
  GstElement *bin, *src1 = NULL, *cf, *src2, *audiomixer, *sink;
  GstCaps *caps;
  GstBus *bus;
  gboolean res;
  GstPad *srcpad;
  GstPad *sinkpad;
  gint i;
  GstStreamConsistency *consist;

  GST_INFO ("preparing test");
  play_seek_event = NULL;

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src1 = gst_element_factory_make ("audiotestsrc", "src1");
  g_object_set (src1, "wave", 4, "is-live", TRUE, NULL);        /* silence */

  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  cf = gst_element_factory_make ("capsfilter", "capsfilter");
  sink = gst_element_factory_make ("fakesink", "sink");

  gst_bin_add_many (GST_BIN (bin), src1, cf, audiomixer, sink, NULL);
  res = gst_element_link_many (src1, cf, audiomixer, sink, NULL);
  fail_unless (res == TRUE, NULL);

  /* get the caps for the livesrc, we'll reuse this for the non-live source */
  set_state_and_wait (bin, GST_STATE_PLAYING);

  sinkpad = gst_element_get_static_pad (sink, "sink");
  fail_unless (sinkpad != NULL);
  caps = gst_pad_get_current_caps (sinkpad);
  fail_unless (caps != NULL);
  gst_object_unref (sinkpad);

  gst_element_set_state (bin, GST_STATE_NULL);

  g_object_set (cf, "caps", caps, NULL);

  src2 = gst_element_factory_make ("audiotestsrc", "src2");
  g_object_set (src2, "wave", 4, NULL); /* silence */
  gst_bin_add (GST_BIN (bin), src2);

  res = gst_element_link_filtered (src2, audiomixer, caps);
  fail_unless (res == TRUE, NULL);

  gst_caps_unref (caps);

  play_seek_event = gst_event_new_seek (1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_FLUSH,
      GST_SEEK_TYPE_SET, (GstClockTime) 0,
      GST_SEEK_TYPE_SET, (GstClockTime) 2 * GST_SECOND);

  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  srcpad = gst_element_get_static_pad (audiomixer, "src");
  consist = gst_consistency_checker_new (srcpad);
  gst_object_unref (srcpad);

  GST_INFO ("starting test");

  /* run it twice */
  for (i = 0; i < 2; i++) {

    GST_INFO ("starting test-loop %d", i);

    /* prepare playing */
    set_state_and_wait (bin, GST_STATE_PAUSED);

    gst_event_set_seqnum (play_seek_event, gst_util_seqnum_next ());
    res = gst_element_send_event (bin, gst_event_ref (play_seek_event));
    fail_unless (res == TRUE, NULL);

    GST_INFO ("seeked");

    /* run pipeline */
    play_and_wait (bin);

    gst_consistency_checker_reset (consist);
  }

  /* cleanup */
  GST_INFO ("cleaning up");
  gst_consistency_checker_free (consist);
  if (play_seek_event)
    gst_event_unref (play_seek_event);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

/* check if adding pads work as expected */
GST_START_TEST (test_add_pad)
{
  GstElement *bin, *src1, *src2, *audiomixer, *sink;
  GstBus *bus;
  GstPad *srcpad;
  gboolean res;
  GstStateChangeReturn state_res;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src1 = gst_element_factory_make ("audiotestsrc", "src1");
  g_object_set (src1, "num-buffers", 4, "wave", /* silence */ 4, NULL);
  src2 = gst_element_factory_make ("audiotestsrc", "src2");
  /* one buffer less, we connect with 1 buffer of delay */
  g_object_set (src2, "num-buffers", 3, "wave", /* silence */ 4, NULL);
  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src1, audiomixer, sink, NULL);

  res = gst_element_link (src1, audiomixer);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (audiomixer, sink);
  fail_unless (res == TRUE, NULL);

  srcpad = gst_element_get_static_pad (audiomixer, "src");
  gst_object_unref (srcpad);

  g_signal_connect (bus, "message::segment-done", (GCallback) message_received,
      bin);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  GST_INFO ("starting test");

  /* prepare playing */
  set_state_and_wait (bin, GST_STATE_PAUSED);

  /* add other element */
  gst_bin_add_many (GST_BIN (bin), src2, NULL);

  /* now link the second element */
  res = gst_element_link (src2, audiomixer);
  fail_unless (res == TRUE, NULL);

  /* set to PAUSED as well */
  state_res = gst_element_set_state (src2, GST_STATE_PAUSED);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* now play all */
  play_and_wait (bin);

  /* cleanup */
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

/* check if removing pads work as expected */
GST_START_TEST (test_remove_pad)
{
  GstElement *bin, *src, *audiomixer, *sink;
  GstBus *bus;
  GstPad *pad, *srcpad;
  gboolean res;
  GstStateChangeReturn state_res;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  src = gst_element_factory_make ("audiotestsrc", "src");
  g_object_set (src, "num-buffers", 4, "wave", 4, NULL);
  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src, audiomixer, sink, NULL);

  res = gst_element_link (src, audiomixer);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (audiomixer, sink);
  fail_unless (res == TRUE, NULL);

  /* create an unconnected sinkpad in audiomixer */
  pad = gst_element_request_pad_simple (audiomixer, "sink_%u");
  fail_if (pad == NULL, NULL);

  srcpad = gst_element_get_static_pad (audiomixer, "src");
  gst_object_unref (srcpad);

  g_signal_connect (bus, "message::segment-done", (GCallback) message_received,
      bin);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  GST_INFO ("starting test");

  /* prepare playing, this will not preroll as audiomixer is waiting
   * on the unconnected sinkpad. */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* wait for completion for one second, will return ASYNC */
  state_res = gst_element_get_state (GST_ELEMENT (bin), NULL, NULL, GST_SECOND);
  ck_assert_int_eq (state_res, GST_STATE_CHANGE_ASYNC);

  /* get rid of the pad now, audiomixer should stop waiting on it and
   * continue the preroll */
  gst_element_release_request_pad (audiomixer, pad);
  gst_object_unref (pad);

  /* wait for completion, should work now */
  state_res =
      gst_element_get_state (GST_ELEMENT (bin), NULL, NULL,
      GST_CLOCK_TIME_NONE);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* now play all */
  play_and_wait (bin);

  /* cleanup */
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (G_OBJECT (bus));
  gst_object_unref (G_OBJECT (bin));
}

GST_END_TEST;


static GstBuffer *handoff_buffer = NULL;

static void
handoff_buffer_cb (GstElement * fakesink, GstBuffer * buffer, GstPad * pad,
    gpointer user_data)
{
  GST_DEBUG ("got buffer -- SIZE: %" G_GSIZE_FORMAT
      " -- %p PTS is %" GST_TIME_FORMAT " END is %" GST_TIME_FORMAT,
      gst_buffer_get_size (buffer), buffer,
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer)),
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer) + GST_BUFFER_DURATION (buffer)));

  gst_buffer_replace (&handoff_buffer, buffer);
}

/* check if clipping works as expected */
GST_START_TEST (test_clip)
{
  GstSegment segment;
  GstElement *bin, *audiomixer, *sink;
  GstBus *bus;
  GstPad *sinkpad;
  gboolean res;
  GstStateChangeReturn state_res;
  GstFlowReturn ret;
  GstEvent *event;
  GstBuffer *buffer;
  GstCaps *caps;
  GstQuery *drain = gst_query_new_drain ();

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  /* just an audiomixer and a fakesink */
  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  g_object_set (audiomixer, "output-buffer-duration", 50 * GST_MSECOND, NULL);
  sink = gst_element_factory_make ("fakesink", "sink");
  g_object_set (sink, "signal-handoffs", TRUE, NULL);
  g_signal_connect (sink, "handoff", (GCallback) handoff_buffer_cb, NULL);
  gst_bin_add_many (GST_BIN (bin), audiomixer, sink, NULL);

  res = gst_element_link (audiomixer, sink);
  fail_unless (res == TRUE, NULL);

  /* set to playing */
  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* create an unconnected sinkpad in audiomixer, should also automatically activate
   * the pad */
  sinkpad = gst_element_request_pad_simple (audiomixer, "sink_%u");
  fail_if (sinkpad == NULL, NULL);

  gst_pad_send_event (sinkpad, gst_event_new_stream_start ("test"));

  caps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (S16),
      "layout", G_TYPE_STRING, "interleaved",
      "rate", G_TYPE_INT, 44100, "channels", G_TYPE_INT, 2, NULL);

  gst_pad_set_caps (sinkpad, caps);
  gst_caps_unref (caps);

  /* send segment to audiomixer */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  segment.start = GST_SECOND;
  segment.stop = 2 * GST_SECOND;
  segment.time = 0;
  event = gst_event_new_segment (&segment);
  gst_pad_send_event (sinkpad, event);

  /* should be clipped and ok */
  buffer = new_buffer (44100, 0, 0, 250 * GST_MSECOND, 0);
  GST_DEBUG ("pushing buffer %p END is %" GST_TIME_FORMAT,
      buffer,
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer) + GST_BUFFER_DURATION (buffer)));
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);
  /* The aggregation is done in a dedicated thread, so we can't
   * know when it is actually going to happen, so we use a DRAIN query
   * to wait for it to complete.
   */
  gst_pad_query (sinkpad, drain);
  fail_unless (handoff_buffer == NULL);

  /* should be partially clipped */
  buffer = new_buffer (44100, 0, 900 * GST_MSECOND, 250 * GST_MSECOND,
      GST_BUFFER_FLAG_DISCONT);
  GST_DEBUG ("pushing buffer %p START %" GST_TIME_FORMAT " -- DURATION is %"
      GST_TIME_FORMAT, buffer, GST_TIME_ARGS (GST_BUFFER_PTS (buffer)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)));
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);
  gst_pad_query (sinkpad, drain);

  fail_unless (handoff_buffer != NULL);
  ck_assert_int_eq (GST_BUFFER_PTS (handoff_buffer) +
      GST_BUFFER_DURATION (handoff_buffer), 150 * GST_MSECOND);
  gst_buffer_replace (&handoff_buffer, NULL);

  /* should not be clipped */
  buffer = new_buffer (44100, 0, 1150 * GST_MSECOND, 250 * GST_MSECOND, 0);
  GST_DEBUG ("pushing buffer %p END is %" GST_TIME_FORMAT,
      buffer,
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer) + GST_BUFFER_DURATION (buffer)));
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);
  gst_pad_query (sinkpad, drain);
  fail_unless (handoff_buffer != NULL);
  ck_assert_int_eq (GST_BUFFER_PTS (handoff_buffer) +
      GST_BUFFER_DURATION (handoff_buffer), 400 * GST_MSECOND);
  gst_buffer_replace (&handoff_buffer, NULL);
  fail_unless (handoff_buffer == NULL);

  /* should be clipped and ok */
  buffer = new_buffer (44100, 0, 2 * GST_SECOND, 250 * GST_MSECOND,
      GST_BUFFER_FLAG_DISCONT);
  GST_DEBUG ("pushing buffer %p PTS is %" GST_TIME_FORMAT
      " END is %" GST_TIME_FORMAT,
      buffer,
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer)),
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer) + GST_BUFFER_DURATION (buffer)));
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);
  gst_pad_query (sinkpad, drain);
  fail_unless (handoff_buffer == NULL);

  gst_element_release_request_pad (audiomixer, sinkpad);
  gst_object_unref (sinkpad);
  gst_element_set_state (bin, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
  gst_query_unref (drain);
}

GST_END_TEST;

GST_START_TEST (test_duration_is_max)
{
  GstElement *bin, *src[3], *audiomixer, *sink;
  GstStateChangeReturn state_res;
  GstFormat format = GST_FORMAT_TIME;
  gboolean res;
  gint64 duration;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");

  /* 3 sources, an audiomixer and a fakesink */
  src[0] = gst_element_factory_make ("audiotestsrc", NULL);
  src[1] = gst_element_factory_make ("audiotestsrc", NULL);
  src[2] = gst_element_factory_make ("audiotestsrc", NULL);
  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src[0], src[1], src[2], audiomixer, sink,
      NULL);

  gst_element_link (src[0], audiomixer);
  gst_element_link (src[1], audiomixer);
  gst_element_link (src[2], audiomixer);
  gst_element_link (audiomixer, sink);

  /* irks, duration is reset on basesrc */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  fail_unless (state_res != GST_STATE_CHANGE_FAILURE, NULL);

  /* set durations on src */
  GST_BASE_SRC (src[0])->segment.duration = 1000;
  GST_BASE_SRC (src[1])->segment.duration = 3000;
  GST_BASE_SRC (src[2])->segment.duration = 2000;

  /* set to playing */
  set_state_and_wait (bin, GST_STATE_PLAYING);

  res = gst_element_query_duration (GST_ELEMENT (bin), format, &duration);
  fail_unless (res, NULL);

  ck_assert_int_eq (duration, 3000);

  gst_element_set_state (bin, GST_STATE_NULL);
  gst_object_unref (bin);
}

GST_END_TEST;

GST_START_TEST (test_duration_unknown_overrides)
{
  GstElement *bin, *src[3], *audiomixer, *sink;
  GstStateChangeReturn state_res;
  GstFormat format = GST_FORMAT_TIME;
  gboolean res;
  gint64 duration;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");

  /* 3 sources, an audiomixer and a fakesink */
  src[0] = gst_element_factory_make ("audiotestsrc", NULL);
  src[1] = gst_element_factory_make ("audiotestsrc", NULL);
  src[2] = gst_element_factory_make ("audiotestsrc", NULL);
  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src[0], src[1], src[2], audiomixer, sink,
      NULL);

  gst_element_link (src[0], audiomixer);
  gst_element_link (src[1], audiomixer);
  gst_element_link (src[2], audiomixer);
  gst_element_link (audiomixer, sink);

  /* irks, duration is reset on basesrc */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  fail_unless (state_res != GST_STATE_CHANGE_FAILURE, NULL);

  /* set durations on src */
  GST_BASE_SRC (src[0])->segment.duration = GST_CLOCK_TIME_NONE;
  GST_BASE_SRC (src[1])->segment.duration = 3000;
  GST_BASE_SRC (src[2])->segment.duration = 2000;

  /* set to playing */
  set_state_and_wait (bin, GST_STATE_PLAYING);

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
  GstElement *bin;
  GstBus *bus;
  GstEvent *seek_event;
  gboolean res;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = setup_pipeline (NULL, 2, NULL);
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  seek_event = gst_event_new_seek (1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_FLUSH,
      GST_SEEK_TYPE_SET, (GstClockTime) 0,
      GST_SEEK_TYPE_SET, (GstClockTime) 1 * GST_SECOND);

  g_signal_connect (bus, "message::segment-done",
      (GCallback) loop_segment_done, bin);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  GST_INFO ("starting test");

  /* prepare playing */
  set_state_and_wait (bin, GST_STATE_PAUSED);

  res = gst_element_send_event (bin, seek_event);
  fail_unless (res == TRUE, NULL);

  /* run pipeline */
  play_and_wait (bin);

  fail_unless (looped);

  /* cleanup */
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

GST_START_TEST (test_flush_start_flush_stop)
{
  GstPadTemplate *sink_template;
  GstPad *tmppad, *srcpad1, *sinkpad1, *sinkpad2, *audiomixer_src;
  GstElement *pipeline, *src1, *src2, *audiomixer, *sink;

  GST_INFO ("preparing test");

  /* build pipeline */
  pipeline = gst_pipeline_new ("pipeline");
  src1 = gst_element_factory_make ("audiotestsrc", "src1");
  g_object_set (src1, "wave", 4, NULL); /* silence */
  src2 = gst_element_factory_make ("audiotestsrc", "src2");
  g_object_set (src2, "wave", 4, NULL); /* silence */
  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (pipeline), src1, src2, audiomixer, sink, NULL);

  sink_template =
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (audiomixer),
      "sink_%u");
  fail_unless (GST_IS_PAD_TEMPLATE (sink_template));
  sinkpad1 = gst_element_request_pad (audiomixer, sink_template, NULL, NULL);
  srcpad1 = gst_element_get_static_pad (src1, "src");
  gst_pad_link (srcpad1, sinkpad1);

  sinkpad2 = gst_element_request_pad (audiomixer, sink_template, NULL, NULL);
  tmppad = gst_element_get_static_pad (src2, "src");
  gst_pad_link (tmppad, sinkpad2);
  gst_object_unref (tmppad);

  gst_element_link (audiomixer, sink);

  /* prepare playing */
  set_state_and_wait (pipeline, GST_STATE_PLAYING);

  audiomixer_src = gst_element_get_static_pad (audiomixer, "src");
  fail_if (GST_PAD_IS_FLUSHING (audiomixer_src));
  gst_pad_send_event (sinkpad1, gst_event_new_flush_start ());
  fail_if (GST_PAD_IS_FLUSHING (audiomixer_src));
  fail_unless (GST_PAD_IS_FLUSHING (sinkpad1));
  /* Hold the streamlock to make sure the flush stop is not between
     the attempted push of a segment event and of the following buffer. */
  GST_PAD_STREAM_LOCK (srcpad1);
  gst_pad_send_event (sinkpad1, gst_event_new_flush_stop (TRUE));
  GST_PAD_STREAM_UNLOCK (srcpad1);
  fail_if (GST_PAD_IS_FLUSHING (audiomixer_src));
  fail_if (GST_PAD_IS_FLUSHING (sinkpad1));
  gst_object_unref (audiomixer_src);

  gst_element_release_request_pad (audiomixer, sinkpad1);
  gst_object_unref (sinkpad1);
  gst_element_release_request_pad (audiomixer, sinkpad2);
  gst_object_unref (sinkpad2);
  gst_object_unref (srcpad1);

  /* cleanup */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

GST_END_TEST;

static void
handoff_buffer_collect_cb (GstElement * fakesink, GstBuffer * buffer,
    GstPad * pad, gpointer user_data)
{
  GList **received_buffers = user_data;

  GST_DEBUG ("got buffer %p", buffer);
  *received_buffers =
      g_list_append (*received_buffers, gst_buffer_ref (buffer));
}

typedef void (*SendBuffersFunction) (GstPad * pad1, GstPad * pad2);
typedef void (*CheckBuffersFunction) (GList * buffers);

static void
run_sync_test (SendBuffersFunction send_buffers,
    CheckBuffersFunction check_buffers)
{
  GstSegment segment;
  GstElement *bin, *audiomixer, *queue1, *queue2, *sink;
  GstBus *bus;
  GstPad *sinkpad1, *sinkpad2;
  GstPad *queue1_sinkpad, *queue2_sinkpad;
  GstPad *pad;
  gboolean res;
  GstStateChangeReturn state_res;
  GstEvent *event;
  GstCaps *caps;
  GList *received_buffers = NULL;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  /* just an audiomixer and a fakesink */
  queue1 = gst_element_factory_make ("queue", "queue1");
  queue2 = gst_element_factory_make ("queue", "queue2");
  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  g_object_set (audiomixer, "output-buffer-duration", 500 * GST_MSECOND, NULL);
  sink = gst_element_factory_make ("fakesink", "sink");
  g_object_set (sink, "signal-handoffs", TRUE, NULL);
  g_signal_connect (sink, "handoff", (GCallback) handoff_buffer_collect_cb,
      &received_buffers);
  gst_bin_add_many (GST_BIN (bin), queue1, queue2, audiomixer, sink, NULL);

  res = gst_element_link (audiomixer, sink);
  fail_unless (res == TRUE, NULL);

  /* set to paused */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* create an unconnected sinkpad in audiomixer, should also automatically activate
   * the pad */
  sinkpad1 = gst_element_request_pad_simple (audiomixer, "sink_%u");
  fail_if (sinkpad1 == NULL, NULL);

  queue1_sinkpad = gst_element_get_static_pad (queue1, "sink");
  pad = gst_element_get_static_pad (queue1, "src");
  fail_unless (gst_pad_link (pad, sinkpad1) == GST_PAD_LINK_OK);
  gst_object_unref (pad);

  sinkpad2 = gst_element_request_pad_simple (audiomixer, "sink_%u");
  fail_if (sinkpad2 == NULL, NULL);

  queue2_sinkpad = gst_element_get_static_pad (queue2, "sink");
  pad = gst_element_get_static_pad (queue2, "src");
  fail_unless (gst_pad_link (pad, sinkpad2) == GST_PAD_LINK_OK);
  gst_object_unref (pad);

  gst_pad_send_event (queue1_sinkpad, gst_event_new_stream_start ("test"));
  gst_pad_send_event (queue2_sinkpad, gst_event_new_stream_start ("test"));

  caps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (S16),
      "layout", G_TYPE_STRING, "interleaved",
      "rate", G_TYPE_INT, 1000, "channels", G_TYPE_INT, 1, NULL);

  gst_pad_set_caps (queue1_sinkpad, caps);
  gst_pad_set_caps (queue2_sinkpad, caps);
  gst_caps_unref (caps);

  /* send segment to audiomixer */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  event = gst_event_new_segment (&segment);
  gst_pad_send_event (queue1_sinkpad, gst_event_ref (event));
  gst_pad_send_event (queue2_sinkpad, event);

  /* Push buffers */
  send_buffers (queue1_sinkpad, queue2_sinkpad);

  /* Set PLAYING */
  g_idle_add ((GSourceFunc) set_playing, bin);

  /* Collect buffers and messages */
  g_main_loop_run (main_loop);

  /* Here we get once we got EOS, for errors we failed */

  check_buffers (received_buffers);

  g_list_free_full (received_buffers, (GDestroyNotify) gst_buffer_unref);

  gst_element_release_request_pad (audiomixer, sinkpad1);
  gst_object_unref (sinkpad1);
  gst_object_unref (queue1_sinkpad);
  gst_element_release_request_pad (audiomixer, sinkpad2);
  gst_object_unref (sinkpad2);
  gst_object_unref (queue2_sinkpad);
  gst_element_set_state (bin, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

static void
send_buffers_sync (GstPad * pad1, GstPad * pad2)
{
  GstBuffer *buffer;
  GstFlowReturn ret;

  buffer = new_buffer (2000, 1, 1 * GST_SECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  buffer = new_buffer (2000, 1, 2 * GST_SECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  gst_pad_send_event (pad1, gst_event_new_eos ());

  buffer = new_buffer (2000, 2, 2 * GST_SECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad2, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  buffer = new_buffer (2000, 2, 3 * GST_SECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad2, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  gst_pad_send_event (pad2, gst_event_new_eos ());
}

static void
check_buffers_sync (GList * received_buffers)
{
  GstBuffer *buffer;
  GList *l;
  gint i;
  GstMapInfo map;

  /* Should have 8 * 0.5s buffers */
  fail_unless_equals_int (g_list_length (received_buffers), 8);
  for (i = 0, l = received_buffers; l; l = l->next, i++) {
    buffer = l->data;

    gst_buffer_map (buffer, &map, GST_MAP_READ);

    if (i == 0 && GST_BUFFER_TIMESTAMP (buffer) == 0) {
      fail_unless (map.data[0] == 0);
      fail_unless (map.data[map.size - 1] == 0);
    } else if (i == 1 && GST_BUFFER_TIMESTAMP (buffer) == 500 * GST_MSECOND) {
      fail_unless (map.data[0] == 0);
      fail_unless (map.data[map.size - 1] == 0);
    } else if (i == 2 && GST_BUFFER_TIMESTAMP (buffer) == 1000 * GST_MSECOND) {
      fail_unless (map.data[0] == 1);
      fail_unless (map.data[map.size - 1] == 1);
    } else if (i == 3 && GST_BUFFER_TIMESTAMP (buffer) == 1500 * GST_MSECOND) {
      fail_unless (map.data[0] == 1);
      fail_unless (map.data[map.size - 1] == 1);
    } else if (i == 4 && GST_BUFFER_TIMESTAMP (buffer) == 2000 * GST_MSECOND) {
      fail_unless (map.data[0] == 3);
      fail_unless (map.data[map.size - 1] == 3);
    } else if (i == 5 && GST_BUFFER_TIMESTAMP (buffer) == 2500 * GST_MSECOND) {
      fail_unless (map.data[0] == 3);
      fail_unless (map.data[map.size - 1] == 3);
    } else if (i == 6 && GST_BUFFER_TIMESTAMP (buffer) == 3000 * GST_MSECOND) {
      fail_unless (map.data[0] == 2);
      fail_unless (map.data[map.size - 1] == 2);
    } else if (i == 7 && GST_BUFFER_TIMESTAMP (buffer) == 3500 * GST_MSECOND) {
      fail_unless (map.data[0] == 2);
      fail_unless (map.data[map.size - 1] == 2);
    } else {
      g_assert_not_reached ();
    }

    gst_buffer_unmap (buffer, &map);

  }
}

GST_START_TEST (test_sync)
{
  run_sync_test (send_buffers_sync, check_buffers_sync);
}

GST_END_TEST;

static void
send_buffers_sync_discont (GstPad * pad1, GstPad * pad2)
{
  GstBuffer *buffer;
  GstFlowReturn ret;

  buffer = new_buffer (2000, 1, 1 * GST_SECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  buffer = new_buffer (2000, 1, 3 * GST_SECOND, 1 * GST_SECOND,
      GST_BUFFER_FLAG_DISCONT);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  gst_pad_send_event (pad1, gst_event_new_eos ());

  buffer = new_buffer (2000, 2, 2 * GST_SECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad2, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  buffer = new_buffer (2000, 2, 3 * GST_SECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad2, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  gst_pad_send_event (pad2, gst_event_new_eos ());
}

static void
check_buffers_sync_discont (GList * received_buffers)
{
  GstBuffer *buffer;
  GList *l;
  gint i;
  GstMapInfo map;

  /* Should have 8 * 0.5s buffers */
  fail_unless_equals_int (g_list_length (received_buffers), 8);
  for (i = 0, l = received_buffers; l; l = l->next, i++) {
    buffer = l->data;

    gst_buffer_map (buffer, &map, GST_MAP_READ);

    if (i == 0 && GST_BUFFER_TIMESTAMP (buffer) == 0) {
      fail_unless (map.data[0] == 0);
      fail_unless (map.data[map.size - 1] == 0);
    } else if (i == 1 && GST_BUFFER_TIMESTAMP (buffer) == 500 * GST_MSECOND) {
      fail_unless (map.data[0] == 0);
      fail_unless (map.data[map.size - 1] == 0);
    } else if (i == 2 && GST_BUFFER_TIMESTAMP (buffer) == 1000 * GST_MSECOND) {
      fail_unless (map.data[0] == 1);
      fail_unless (map.data[map.size - 1] == 1);
    } else if (i == 3 && GST_BUFFER_TIMESTAMP (buffer) == 1500 * GST_MSECOND) {
      fail_unless (map.data[0] == 1);
      fail_unless (map.data[map.size - 1] == 1);
    } else if (i == 4 && GST_BUFFER_TIMESTAMP (buffer) == 2000 * GST_MSECOND) {
      fail_unless (map.data[0] == 2);
      fail_unless (map.data[map.size - 1] == 2);
    } else if (i == 5 && GST_BUFFER_TIMESTAMP (buffer) == 2500 * GST_MSECOND) {
      fail_unless (map.data[0] == 2);
      fail_unless (map.data[map.size - 1] == 2);
    } else if (i == 6 && GST_BUFFER_TIMESTAMP (buffer) == 3000 * GST_MSECOND) {
      fail_unless (map.data[0] == 3);
      fail_unless (map.data[map.size - 1] == 3);
    } else if (i == 7 && GST_BUFFER_TIMESTAMP (buffer) == 3500 * GST_MSECOND) {
      fail_unless (map.data[0] == 3);
      fail_unless (map.data[map.size - 1] == 3);
    } else {
      g_assert_not_reached ();
    }

    gst_buffer_unmap (buffer, &map);

  }
}

GST_START_TEST (test_sync_discont)
{
  run_sync_test (send_buffers_sync_discont, check_buffers_sync_discont);
}

GST_END_TEST;


static void
send_buffers_sync_discont_backwards (GstPad * pad1, GstPad * pad2)
{
  GstBuffer *buffer;
  GstFlowReturn ret;

  buffer = new_buffer (2300, 1, 1 * GST_SECOND, 1.15 * GST_SECOND, 0);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  buffer = new_buffer (2000, 1, 2 * GST_SECOND, 1 * GST_SECOND,
      GST_BUFFER_FLAG_DISCONT);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  gst_pad_send_event (pad1, gst_event_new_eos ());

  buffer = new_buffer (2000, 1, 2 * GST_SECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad2, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);


  gst_pad_send_event (pad2, gst_event_new_eos ());
}

static void
check_buffers_sync_discont_backwards (GList * received_buffers)
{
  GstBuffer *buffer;
  GList *l;
  gint i;
  GstMapInfo map;

  /* Should have 6 * 0.5s buffers */
  fail_unless_equals_int (g_list_length (received_buffers), 6);
  for (i = 0, l = received_buffers; l; l = l->next, i++) {
    buffer = l->data;

    gst_buffer_map (buffer, &map, GST_MAP_READ);

    if (i == 0 && GST_BUFFER_TIMESTAMP (buffer) == 0) {
      fail_unless_equals_int (map.data[0], 0);
      fail_unless_equals_int (map.data[map.size - 1], 0);
    } else if (i == 1 && GST_BUFFER_TIMESTAMP (buffer) == 500 * GST_MSECOND) {
      fail_unless_equals_int (map.data[0], 0);
      fail_unless_equals_int (map.data[map.size - 1], 0);
    } else if (i == 2 && GST_BUFFER_TIMESTAMP (buffer) == 1000 * GST_MSECOND) {
      fail_unless_equals_int (map.data[0], 1);
      fail_unless_equals_int (map.data[map.size - 1], 1);
    } else if (i == 3 && GST_BUFFER_TIMESTAMP (buffer) == 1500 * GST_MSECOND) {
      fail_unless_equals_int (map.data[0], 1);
      fail_unless_equals_int (map.data[map.size - 1], 1);
    } else if (i == 4 && GST_BUFFER_TIMESTAMP (buffer) == 2000 * GST_MSECOND) {
      fail_unless_equals_int (map.data[0], 2);
      fail_unless_equals_int (map.data[map.size - 1], 2);
    } else if (i == 5 && GST_BUFFER_TIMESTAMP (buffer) == 2500 * GST_MSECOND) {
      fail_unless_equals_int (map.data[0], 2);
      fail_unless_equals_int (map.data[map.size - 1], 2);
    } else {
      g_assert_not_reached ();
    }

    gst_buffer_unmap (buffer, &map);

  }
}

GST_START_TEST (test_sync_discont_backwards)
{
  run_sync_test (send_buffers_sync_discont_backwards,
      check_buffers_sync_discont_backwards);
}

GST_END_TEST;

static void
send_buffers_sync_discont_and_drop_backwards (GstPad * pad1, GstPad * pad2)
{
  GstBuffer *buffer;
  GstFlowReturn ret;

  buffer = new_buffer (2500, 1, 1 * GST_SECOND, 1.25 * GST_SECOND, 0);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  buffer = new_buffer (400, 1, 2 * GST_SECOND, 0.2 * GST_SECOND,
      GST_BUFFER_FLAG_DISCONT);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  buffer = new_buffer (1600, 1, 2.2 * GST_SECOND, 0.8 * GST_SECOND, 0);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  gst_pad_send_event (pad1, gst_event_new_eos ());

  buffer = new_buffer (2000, 1, 2 * GST_SECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad2, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  gst_pad_send_event (pad2, gst_event_new_eos ());
}

GST_START_TEST (test_sync_discont_and_drop_backwards)
{
  run_sync_test (send_buffers_sync_discont_and_drop_backwards,
      check_buffers_sync_discont_backwards);
}

GST_END_TEST;

static void
send_buffers_sync_discont_and_drop_before_output_backwards (GstPad * pad1,
    GstPad * pad2)
{
  GstBuffer *buffer;
  GstFlowReturn ret;

  buffer = new_buffer (2500, 1, 1 * GST_SECOND, 1.25 * GST_SECOND, 0);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  buffer = new_buffer (800, 1, 1.5 * GST_SECOND, 0.4 * GST_SECOND,
      GST_BUFFER_FLAG_DISCONT);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  buffer = new_buffer (2200, 1, 1.9 * GST_SECOND, 1.1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  gst_pad_send_event (pad1, gst_event_new_eos ());

  buffer = new_buffer (2000, 1, 2 * GST_SECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad2, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  gst_pad_send_event (pad2, gst_event_new_eos ());
}

GST_START_TEST (test_sync_discont_and_drop_before_output_backwards)
{
  run_sync_test (send_buffers_sync_discont_and_drop_before_output_backwards,
      check_buffers_sync_discont_backwards);
}

GST_END_TEST;

static void
send_buffers_sync_unaligned (GstPad * pad1, GstPad * pad2)
{
  GstBuffer *buffer;
  GstFlowReturn ret;

  buffer = new_buffer (2000, 1, 750 * GST_MSECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  buffer = new_buffer (2000, 1, 1750 * GST_MSECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad1, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  gst_pad_send_event (pad1, gst_event_new_eos ());

  buffer = new_buffer (2000, 2, 1750 * GST_MSECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad2, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  buffer = new_buffer (2000, 2, 2750 * GST_MSECOND, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (pad2, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  gst_pad_send_event (pad2, gst_event_new_eos ());
}

static void
check_buffers_sync_unaligned (GList * received_buffers)
{
  GstBuffer *buffer;
  GList *l;
  gint i;
  GstMapInfo map;

  /* Should have 8 * 0.5s buffers */
  fail_unless_equals_int (g_list_length (received_buffers), 8);
  for (i = 0, l = received_buffers; l; l = l->next, i++) {
    buffer = l->data;

    gst_buffer_map (buffer, &map, GST_MAP_READ);

    if (i == 0 && GST_BUFFER_TIMESTAMP (buffer) == 0) {
      fail_unless (map.data[0] == 0);
      fail_unless (map.data[map.size - 1] == 0);
    } else if (i == 1 && GST_BUFFER_TIMESTAMP (buffer) == 500 * GST_MSECOND) {
      fail_unless (map.data[0] == 0);
      fail_unless (map.data[499] == 0);
      fail_unless (map.data[500] == 1);
      fail_unless (map.data[map.size - 1] == 1);
    } else if (i == 2 && GST_BUFFER_TIMESTAMP (buffer) == 1000 * GST_MSECOND) {
      fail_unless (map.data[0] == 1);
      fail_unless (map.data[map.size - 1] == 1);
    } else if (i == 3 && GST_BUFFER_TIMESTAMP (buffer) == 1500 * GST_MSECOND) {
      fail_unless (map.data[0] == 1);
      fail_unless (map.data[499] == 1);
      fail_unless (map.data[500] == 3);
      fail_unless (map.data[map.size - 1] == 3);
    } else if (i == 4 && GST_BUFFER_TIMESTAMP (buffer) == 2000 * GST_MSECOND) {
      fail_unless (map.data[0] == 3);
      fail_unless (map.data[499] == 3);
      fail_unless (map.data[500] == 3);
      fail_unless (map.data[map.size - 1] == 3);
    } else if (i == 5 && GST_BUFFER_TIMESTAMP (buffer) == 2500 * GST_MSECOND) {
      fail_unless (map.data[0] == 3);
      fail_unless (map.data[499] == 3);
      fail_unless (map.data[500] == 2);
      fail_unless (map.data[map.size - 1] == 2);
    } else if (i == 6 && GST_BUFFER_TIMESTAMP (buffer) == 3000 * GST_MSECOND) {
      fail_unless (map.data[0] == 2);
      fail_unless (map.data[499] == 2);
      fail_unless (map.data[500] == 2);
      fail_unless (map.data[map.size - 1] == 2);
    } else if (i == 7 && GST_BUFFER_TIMESTAMP (buffer) == 3500 * GST_MSECOND) {
      fail_unless (map.size == 500);
      fail_unless (GST_BUFFER_DURATION (buffer) == 250 * GST_MSECOND);
      fail_unless (map.data[0] == 2);
      fail_unless (map.data[499] == 2);
    } else {
      g_assert_not_reached ();
    }

    gst_buffer_unmap (buffer, &map);

  }
}

GST_START_TEST (test_sync_unaligned)
{
  run_sync_test (send_buffers_sync_unaligned, check_buffers_sync_unaligned);
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

  caps = gst_caps_new_simple ("audio/x-raw", "rate", G_TYPE_INT, 44100,
      "channels", G_TYPE_INT, 2, NULL);

  pipeline = gst_pipeline_new ("pipeline");
  mix = gst_element_factory_make ("audiomixer", "audiomixer");
  sink = gst_element_factory_make ("appsink", "sink");
  g_object_set (sink, "caps", caps, "sync", FALSE, NULL);
  gst_caps_unref (caps);
  /* 50 buffers of 1/10 sec = 5 sec */
  src1 = gst_element_factory_make ("audiotestsrc", "src1");
  g_object_set (src1, "samplesperbuffer", 4410, "num-buffers", 50, NULL);
  src2 = gst_element_factory_make ("audiotestsrc", "src2");
  g_object_set (src2, "samplesperbuffer", 4410, "num-buffers", 50, NULL);
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
  /* set a pad offset of another 5 seconds */
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

static void
set_pad_volume_fade (GstPad * pad, GstClockTime start, gdouble start_value,
    GstClockTime end, gdouble end_value)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;

  cs = gst_interpolation_control_source_new ();
  fail_unless (gst_object_add_control_binding (GST_OBJECT_CAST (pad),
          gst_direct_control_binding_new_absolute (GST_OBJECT_CAST (pad),
              "volume", cs)));

  /* set volume interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  tvcs = (GstTimedValueControlSource *) cs;
  fail_unless (gst_timed_value_control_source_set (tvcs, start, start_value));
  fail_unless (gst_timed_value_control_source_set (tvcs, end, end_value));
  gst_object_unref (cs);
}

GST_START_TEST (test_sinkpad_property_controller)
{
  GstBus *bus;
  GstMessage *msg;
  GstElement *pipeline, *sink, *mix, *src1;
  GstPad *srcpad, *sinkpad;
  GError *error = NULL;
  gchar *debug;

  pipeline = gst_pipeline_new ("pipeline");
  mix = gst_element_factory_make ("audiomixer", "audiomixer");
  sink = gst_element_factory_make ("fakesink", "sink");
  src1 = gst_element_factory_make ("audiotestsrc", "src1");
  g_object_set (src1, "num-buffers", 100, NULL);
  gst_bin_add_many (GST_BIN (pipeline), src1, mix, sink, NULL);
  fail_unless (gst_element_link (mix, sink));

  srcpad = gst_element_get_static_pad (src1, "src");
  sinkpad = gst_element_request_pad_simple (mix, "sink_0");
  fail_unless (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK);
  set_pad_volume_fade (sinkpad, 0, 0, 1.0, 2.0);
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (msg, &error, &debug);
      g_printerr ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (msg->src), error->message);
      g_printerr ("Debug info: %s\n", debug);
      g_error_free (error);
      g_free (debug);
      break;
    case GST_MESSAGE_EOS:
      break;
    default:
      g_assert_not_reached ();
  }
  gst_message_unref (msg);
  g_object_unref (bus);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

GST_END_TEST;

static void
change_src_caps (GstElement * fakesink, GstBuffer * buffer, GstPad * pad,
    GstElement * capsfilter)
{
  GstCaps *caps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (S32),
      "layout", G_TYPE_STRING, "interleaved",
      "rate", G_TYPE_INT, 10, "channels", G_TYPE_INT, 1, NULL);

  g_object_set (capsfilter, "caps", caps, NULL);
  gst_caps_unref (caps);
  g_signal_connect (fakesink, "handoff", (GCallback) handoff_buffer_cb, NULL);
  g_signal_handlers_disconnect_by_func (fakesink, change_src_caps, capsfilter);
}

/* In this test, we create an input buffer with a duration of 2 seconds,
 * and require the audiomixer to output 1 second long buffers.
 * The input buffer will thus be mixed twice, and the audiomixer will
 * output two buffers.
 *
 * After audiomixer has output a first buffer, we change its output format
 * from S8 to S32. As our sample rate stays the same at 10 fps, and we use
 * mono, the first buffer should be 10 bytes long, and the second 40.
 *
 * The input buffer is made up of 15 0-valued bytes, and 5 1-valued bytes.
 * We verify that the second buffer contains 5 0-valued integers, and
 * 5 1 << 24 valued integers.
 */
GST_START_TEST (test_change_output_caps)
{
  GstSegment segment;
  GstElement *bin, *audiomixer, *capsfilter, *sink;
  GstBus *bus;
  GstPad *sinkpad;
  gboolean res;
  GstStateChangeReturn state_res;
  GstFlowReturn ret;
  GstEvent *event;
  GstBuffer *buffer;
  GstCaps *caps;
  GstQuery *drain = gst_query_new_drain ();
  GstMapInfo inmap;
  GstMapInfo outmap;
  gsize i;

  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  g_object_set (audiomixer, "output-buffer-duration", GST_SECOND, NULL);
  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  sink = gst_element_factory_make ("fakesink", "sink");
  g_object_set (sink, "signal-handoffs", TRUE, NULL);
  g_signal_connect (sink, "handoff", (GCallback) change_src_caps, capsfilter);
  gst_bin_add_many (GST_BIN (bin), audiomixer, capsfilter, sink, NULL);

  res = gst_element_link_many (audiomixer, capsfilter, sink, NULL);
  fail_unless (res == TRUE, NULL);

  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  sinkpad = gst_element_request_pad_simple (audiomixer, "sink_%u");
  fail_if (sinkpad == NULL, NULL);

  gst_pad_send_event (sinkpad, gst_event_new_stream_start ("test"));

  caps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, "S8",
      "layout", G_TYPE_STRING, "interleaved",
      "rate", G_TYPE_INT, 10, "channels", G_TYPE_INT, 1, NULL);

  gst_pad_set_caps (sinkpad, caps);
  g_object_set (capsfilter, "caps", caps, NULL);
  gst_caps_unref (caps);

  gst_segment_init (&segment, GST_FORMAT_TIME);
  segment.start = 0;
  segment.stop = 2 * GST_SECOND;
  segment.time = 0;
  event = gst_event_new_segment (&segment);
  gst_pad_send_event (sinkpad, event);

  gst_buffer_replace (&handoff_buffer, NULL);

  buffer = new_buffer (20, 0, 0, 2 * GST_SECOND, 0);
  gst_buffer_map (buffer, &inmap, GST_MAP_WRITE);
  memset (inmap.data + 15, 1, 5);
  gst_buffer_unmap (buffer, &inmap);
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);
  gst_pad_query (sinkpad, drain);
  fail_unless (handoff_buffer != NULL);
  fail_unless_equals_int (gst_buffer_get_size (handoff_buffer), 40);

  gst_buffer_map (handoff_buffer, &outmap, GST_MAP_READ);
  for (i = 0; i < 10; i++) {
    guint32 sample;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    sample = GUINT32_FROM_LE (((guint32 *) outmap.data)[i]);
#else
    sample = GUINT32_FROM_BE (((guint32 *) outmap.data)[i]);
#endif

    if (i < 5) {
      fail_unless_equals_int (sample, 0);
    } else {
      fail_unless_equals_int (sample, 1 << 24);
    }
  }
  gst_buffer_unmap (handoff_buffer, &outmap);
  gst_clear_buffer (&handoff_buffer);

  gst_element_release_request_pad (audiomixer, sinkpad);
  gst_object_unref (sinkpad);
  gst_element_set_state (bin, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
  gst_query_unref (drain);
}

GST_END_TEST;

/* In this test, we create two input buffers with a duration of 1 second,
 * and require the audiomixer to output 1.5 second long buffers.
 *
 * After we have input two buffers, we change the output format
 * from S8 to S32, then push a last buffer.
 *
 * This makes audioaggregator convert its "half-mixed" current_buffer,
 * we can then ensure that the second output buffer is as expected.
 */
GST_START_TEST (test_change_output_caps_mid_output_buffer)
{
  GstSegment segment;
  GstElement *bin, *audiomixer, *capsfilter, *sink;
  GstBus *bus;
  GstPad *sinkpad;
  gboolean res;
  GstStateChangeReturn state_res;
  GstFlowReturn ret;
  GstEvent *event;
  GstBuffer *buffer;
  GstCaps *caps;
  GstQuery *drain;
  GstMapInfo inmap;
  GstMapInfo outmap;
  guint i;

  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  audiomixer = gst_element_factory_make ("audiomixer", "audiomixer");
  g_object_set (audiomixer, "output-buffer-duration", 1500 * GST_MSECOND, NULL);
  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), audiomixer, capsfilter, sink, NULL);

  res = gst_element_link_many (audiomixer, capsfilter, sink, NULL);
  fail_unless (res == TRUE, NULL);

  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  sinkpad = gst_element_request_pad_simple (audiomixer, "sink_%u");
  fail_if (sinkpad == NULL, NULL);

  gst_pad_send_event (sinkpad, gst_event_new_stream_start ("test"));

  caps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, "S8",
      "layout", G_TYPE_STRING, "interleaved",
      "rate", G_TYPE_INT, 10, "channels", G_TYPE_INT, 1, NULL);

  gst_pad_set_caps (sinkpad, caps);
  g_object_set (capsfilter, "caps", caps, NULL);
  gst_caps_unref (caps);

  gst_segment_init (&segment, GST_FORMAT_TIME);
  segment.start = 0;
  segment.stop = 3 * GST_SECOND;
  segment.time = 0;
  event = gst_event_new_segment (&segment);
  gst_pad_send_event (sinkpad, event);

  buffer = new_buffer (10, 0, 0, 1 * GST_SECOND, 0);
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  buffer = new_buffer (10, 0, 1 * GST_SECOND, 1 * GST_SECOND, 0);
  gst_buffer_map (buffer, &inmap, GST_MAP_WRITE);
  memset (inmap.data, 1, 10);
  gst_buffer_unmap (buffer, &inmap);
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  drain = gst_query_new_drain ();
  gst_pad_query (sinkpad, drain);
  gst_query_unref (drain);

  caps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (S32),
      "layout", G_TYPE_STRING, "interleaved",
      "rate", G_TYPE_INT, 10, "channels", G_TYPE_INT, 1, NULL);
  g_object_set (capsfilter, "caps", caps, NULL);
  gst_caps_unref (caps);

  gst_buffer_replace (&handoff_buffer, NULL);
  g_object_set (sink, "signal-handoffs", TRUE, NULL);
  g_signal_connect (sink, "handoff", (GCallback) handoff_buffer_cb, NULL);

  buffer = new_buffer (10, 0, 2 * GST_SECOND, 1 * GST_SECOND, 0);
  gst_buffer_map (buffer, &inmap, GST_MAP_WRITE);
  memset (inmap.data, 0, 10);
  gst_buffer_unmap (buffer, &inmap);
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);

  drain = gst_query_new_drain ();
  gst_pad_query (sinkpad, drain);
  gst_query_unref (drain);

  fail_unless (handoff_buffer);
  fail_unless_equals_int (gst_buffer_get_size (handoff_buffer), 60);

  gst_buffer_map (handoff_buffer, &outmap, GST_MAP_READ);
  for (i = 0; i < 15; i++) {
    guint32 sample;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    sample = GUINT32_FROM_LE (((guint32 *) outmap.data)[i]);
#else
    sample = GUINT32_FROM_BE (((guint32 *) outmap.data)[i]);
#endif

    if (i < 5) {
      fail_unless_equals_int (sample, 1 << 24);
    } else {
      fail_unless_equals_int (sample, 0);
    }
  }

  gst_buffer_unmap (handoff_buffer, &outmap);
  gst_clear_buffer (&handoff_buffer);

  gst_element_release_request_pad (audiomixer, sinkpad);
  gst_object_unref (sinkpad);
  gst_element_set_state (bin, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

static void
check_qos_message (GstMessage * msg, GstClockTime expected_timestamp,
    GstClockTime expected_duration, guint64 expected_processed,
    guint64 expected_dropped)
{
  gboolean live;
  guint64 running_time, stream_time, timestamp, duration;
  GstFormat format;
  guint64 processed, dropped;

  gst_message_parse_qos (msg, &live, &running_time, &stream_time,
      &timestamp, &duration);
  gst_message_parse_qos_stats (msg, &format, &processed, &dropped);

  fail_unless_equals_uint64 (running_time, expected_timestamp);
  fail_unless_equals_uint64 (stream_time, expected_timestamp);
  fail_unless_equals_uint64 (timestamp, expected_timestamp);
  fail_unless_equals_uint64 (duration, expected_duration);

  fail_unless_equals_int64 (format, GST_FORMAT_DEFAULT);
  fail_unless_equals_uint64 (processed, expected_processed);
  fail_unless_equals_uint64 (dropped, expected_dropped);

  gst_message_unref (msg);
}

GST_START_TEST (test_qos_message_live)
{
  GstBus *bus = gst_bus_new ();
  GstHarness *h, *h2;
  GstBuffer *b;
  static const char *caps_str = "audio/x-raw, format=(string)S16LE, "
      "rate=(int)1000, channels=(int)1, layout=(string)interleaved";
  GstMessage *msg;
  GstPad *pad;

  h = gst_harness_new_with_padnames ("audiomixer", "sink_0", "src");
  g_object_set (h->element, "output-buffer-duration", GST_SECOND, NULL);

  pad = gst_element_get_static_pad (h->element, "sink_0");
  g_object_set (pad, "qos-messages", TRUE, NULL);
  gst_object_unref (pad);

  h2 = gst_harness_new_with_element (h->element, "sink_1", NULL);
  pad = gst_element_get_static_pad (h->element, "sink_1");
  g_object_set (pad, "qos-messages", TRUE, NULL);
  gst_object_unref (pad);

  gst_element_set_bus (h->element, bus);
  gst_harness_play (h);
  gst_harness_play (h2);
  gst_harness_set_caps_str (h, caps_str, caps_str);
  gst_harness_set_src_caps_str (h2, caps_str);

  /* Push in 1.5s of data on sink_0 and 4s on sink_1 */
  gst_harness_push (h, new_buffer (3000, 0, 0, 1.5 * GST_SECOND, 0));
  gst_harness_push (h2, new_buffer (10000, 0, 0, 5 * GST_SECOND, 0));

  /* Pull a normal buffer at time 0 */
  b = gst_harness_pull (h);
  fail_unless_equals_int64 (GST_BUFFER_PTS (b), 0);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (b), GST_SECOND);
  gst_buffer_unref (b);
  msg = gst_bus_pop_filtered (bus, GST_MESSAGE_QOS);
  fail_unless (msg == NULL);

  gst_harness_crank_single_clock_wait (h);

  /* Pull a buffer a time 1, the second half is faked data */
  b = gst_harness_pull (h);
  fail_unless_equals_int64 (GST_BUFFER_PTS (b), GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (b), GST_SECOND);
  gst_buffer_unref (b);
  msg = gst_bus_pop_filtered (bus, GST_MESSAGE_QOS);
  fail_unless (msg == NULL);

  /* Push a buffer thar partially overlaps, expect a QoS message */
  b = gst_harness_push_and_pull (h, new_buffer (3000, 0, 1.5 * GST_SECOND,
          1.5 * GST_SECOND, GST_BUFFER_FLAG_DISCONT));
  fail_unless_equals_int64 (GST_BUFFER_PTS (b), 2 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (b), GST_SECOND);
  gst_buffer_unref (b);

  msg = gst_bus_pop_filtered (bus, GST_MESSAGE_QOS);
  check_qos_message (msg, 1500 * GST_MSECOND, 500 * GST_MSECOND, 1500, 500);

  /* Pull one buffer to get out the mixed data */
  gst_harness_crank_single_clock_wait (h);
  b = gst_harness_pull (h);
  fail_unless_equals_int64 (GST_BUFFER_PTS (b), 3 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (b), GST_SECOND);
  gst_buffer_unref (b);
  msg = gst_bus_pop_filtered (bus, GST_MESSAGE_QOS);
  fail_unless (msg == NULL);

  /* Pull another buffer to move the time to 4s */
  gst_harness_crank_single_clock_wait (h);
  b = gst_harness_pull (h);
  fail_unless_equals_int64 (GST_BUFFER_PTS (b), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (b), GST_SECOND);
  gst_buffer_unref (b);
  msg = gst_bus_pop_filtered (bus, GST_MESSAGE_QOS);
  fail_unless (msg == NULL);

  /* Push a buffer that totally overlaps, it should get dropped */
  gst_harness_push (h, new_buffer (1000, 0, 3 * GST_SECOND,
          500 * GST_MSECOND, 0));

  /* Crank it to get the next one, and expect message from the dropped buffer */
  gst_harness_crank_single_clock_wait (h);
  msg = gst_bus_timed_pop_filtered (bus, GST_SECOND, GST_MESSAGE_QOS);
  check_qos_message (msg, 3 * GST_SECOND, 500 * GST_MSECOND, 2500, 1000);

  gst_element_set_bus (h->element, NULL);
  gst_harness_teardown (h2);
  gst_harness_teardown (h);
  gst_object_unref (bus);
}

GST_END_TEST;

static Suite *
audiomixer_suite (void)
{
  Suite *s = suite_create ("audiomixer");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_caps);
  tcase_add_test (tc_chain, test_filter_caps);
  tcase_add_test (tc_chain, test_event);
  tcase_add_test (tc_chain, test_play_twice);
  tcase_add_test (tc_chain, test_play_twice_then_add_and_play_again);
  tcase_add_test (tc_chain, test_live_seeking);
  tcase_add_test (tc_chain, test_add_pad);
  tcase_add_test (tc_chain, test_remove_pad);
  tcase_add_test (tc_chain, test_clip);
  tcase_add_test (tc_chain, test_duration_is_max);
  tcase_add_test (tc_chain, test_duration_unknown_overrides);
  tcase_add_test (tc_chain, test_loop);
  tcase_add_test (tc_chain, test_flush_start_flush_stop);
  tcase_add_test (tc_chain, test_sync);
  tcase_add_test (tc_chain, test_sync_discont);
  tcase_add_test (tc_chain, test_sync_discont_backwards);
  tcase_add_test (tc_chain, test_sync_discont_and_drop_backwards);
  tcase_add_test (tc_chain, test_sync_discont_and_drop_before_output_backwards);
  tcase_add_test (tc_chain, test_sync_unaligned);
  tcase_add_test (tc_chain, test_segment_base_handling);
  tcase_add_test (tc_chain, test_sinkpad_property_controller);
  tcase_add_test (tc_chain, test_qos_message_live);
  tcase_add_checked_fixture (tc_chain, test_setup, test_teardown);
  tcase_add_test (tc_chain, test_change_output_caps);
  tcase_add_test (tc_chain, test_change_output_caps_mid_output_buffer);

  /* Use a longer timeout */
#ifdef HAVE_VALGRIND
  if (RUNNING_ON_VALGRIND) {
    tcase_set_timeout (tc_chain, 5 * 60);
  } else
#endif
  {
    /* this is shorter than the default 60 seconds?! (tpm) */
    /* tcase_set_timeout (tc_chain, 6); */
  }

  return s;
}

GST_CHECK_MAIN (audiomixer);
