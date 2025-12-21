/* GStreamer
 *
 * unit test for adder
 *
 * Copyright (C) <2005> Thomas Vander Stichele <thomas at apestaart dot org>
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
#include <gst/base/gstbasesrc.h>
#include <gst/audio/audio.h>

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
setup_pipeline (GstElement * adder, gint num_srcs)
{
  GstElement *pipeline, *src, *sink;
  gint i;

  pipeline = gst_pipeline_new ("pipeline");
  if (!adder) {
    adder = gst_element_factory_make ("adder", "adder");
  }

  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (pipeline), adder, sink, NULL);
  gst_element_link (adder, sink);

  for (i = 0; i < num_srcs; i++) {
    src = gst_element_factory_make ("audiotestsrc", NULL);
    g_object_set (src, "wave", 4, NULL);        /* silence */
    gst_bin_add (GST_BIN (pipeline), src);
    gst_element_link (src, adder);
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

static void
play_and_wait (GstElement * pipeline)
{
  GstStateChangeReturn state_res;

  state_res = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

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
new_buffer (gsize num_bytes, GstClockTime ts, GstClockTime dur)
{
  GstBuffer *buffer = gst_buffer_new_and_alloc (num_bytes);

  GST_BUFFER_TIMESTAMP (buffer) = ts;
  GST_BUFFER_DURATION (buffer) = dur;
  GST_DEBUG ("created buffer %p", buffer);
  return buffer;
}

/* make sure downstream gets a CAPS event before buffers are sent */
GST_START_TEST (test_caps)
{
  GstElement *pipeline;
  GstCaps *caps;

  /* build pipeline */
  pipeline = setup_pipeline (NULL, 1);

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
  GstElement *pipeline, *adder;
  GstCaps *filter_caps, *caps;

  filter_caps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
      "layout", G_TYPE_STRING, "interleaved",
      "rate", G_TYPE_INT, 44100, "channels", G_TYPE_INT, 1, NULL);

  /* build pipeline */
  adder = gst_element_factory_make ("adder", "adder");
  g_object_set (adder, "caps", filter_caps, NULL);
  pipeline = setup_pipeline (adder, 1);

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
  GstElement *bin, *src1, *src2, *adder, *sink;
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
  adder = gst_element_factory_make ("adder", "adder");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src1, src2, adder, sink, NULL);

  res = gst_element_link (src1, adder);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (src2, adder);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (adder, sink);
  fail_unless (res == TRUE, NULL);

  srcpad = gst_element_get_static_pad (adder, "src");
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
  GstElement *bin, *adder;
  GstBus *bus;
  gboolean res;
  GstPad *srcpad;
  GstStreamConsistency *consist;

  GST_INFO ("preparing test");

  /* build pipeline */
  adder = gst_element_factory_make ("adder", "adder");
  bin = setup_pipeline (adder, 2);
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  srcpad = gst_element_get_static_pad (adder, "src");
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
  GstElement *bin, *src, *adder;
  GstBus *bus;
  gboolean res;
  GstStateChangeReturn state_res;
  gint i;
  GstPad *srcpad;
  GstStreamConsistency *consist;

  GST_INFO ("preparing test");

  /* build pipeline */
  adder = gst_element_factory_make ("adder", "adder");
  bin = setup_pipeline (adder, 2);
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  srcpad = gst_element_get_static_pad (adder, "src");
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

      res = gst_element_link (src, adder);
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
  GstElement *bin, *src1 = NULL, *src2, *ac1, *ac2, *adder, *sink;
  GstBus *bus;
  gboolean res;
  GstPad *srcpad;
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

  ac1 = gst_element_factory_make ("audioconvert", "ac1");
  src2 = gst_element_factory_make ("audiotestsrc", "src2");
  g_object_set (src2, "wave", 4, NULL); /* silence */
  ac2 = gst_element_factory_make ("audioconvert", "ac2");
  adder = gst_element_factory_make ("adder", "adder");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src1, ac1, src2, ac2, adder, sink, NULL);

  res = gst_element_link_many (src1, ac1, adder, NULL);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link_many (src2, ac2, adder, NULL);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (adder, sink);
  fail_unless (res == TRUE, NULL);

  play_seek_event = gst_event_new_seek (1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_FLUSH,
      GST_SEEK_TYPE_SET, (GstClockTime) 0,
      GST_SEEK_TYPE_SET, (GstClockTime) 2 * GST_SECOND);

  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  srcpad = gst_element_get_static_pad (adder, "src");
  consist = gst_consistency_checker_new (srcpad);
  gst_object_unref (srcpad);

  GST_INFO ("starting test");

  /* run it twice */
  for (i = 0; i < 2; i++) {

    GST_INFO ("starting test-loop %d", i);

    /* prepare playing */
    set_state_and_wait (bin, GST_STATE_PAUSED);

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
  GstElement *bin, *src1, *src2, *adder, *sink;
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
  adder = gst_element_factory_make ("adder", "adder");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src1, adder, sink, NULL);

  res = gst_element_link (src1, adder);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (adder, sink);
  fail_unless (res == TRUE, NULL);

  srcpad = gst_element_get_static_pad (adder, "src");
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
  res = gst_element_link (src2, adder);
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
  GstElement *bin, *src, *adder, *sink;
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
  adder = gst_element_factory_make ("adder", "adder");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src, adder, sink, NULL);

  res = gst_element_link (src, adder);
  fail_unless (res == TRUE, NULL);
  res = gst_element_link (adder, sink);
  fail_unless (res == TRUE, NULL);

  /* create an unconnected sinkpad in adder */
  pad = gst_element_request_pad_simple (adder, "sink_%u");
  fail_if (pad == NULL, NULL);

  srcpad = gst_element_get_static_pad (adder, "src");
  gst_object_unref (srcpad);

  g_signal_connect (bus, "message::segment-done", (GCallback) message_received,
      bin);
  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  GST_INFO ("starting test");

  /* prepare playing, this will not preroll as adder is waiting
   * on the unconnected sinkpad. */
  state_res = gst_element_set_state (bin, GST_STATE_PAUSED);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* wait for completion for one second, will return ASYNC */
  state_res = gst_element_get_state (GST_ELEMENT (bin), NULL, NULL, GST_SECOND);
  ck_assert_int_eq (state_res, GST_STATE_CHANGE_ASYNC);

  /* get rid of the pad now, adder should stop waiting on it and
   * continue the preroll */
  gst_element_release_request_pad (adder, pad);
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
  GST_DEBUG ("got buffer %p", buffer);
  gst_buffer_replace (&handoff_buffer, buffer);
}

/* check if clipping works as expected */
GST_START_TEST (test_clip)
{
  GstSegment segment;
  GstElement *bin, *adder, *sink;
  GstBus *bus;
  GstPad *sinkpad;
  gboolean res;
  GstStateChangeReturn state_res;
  GstFlowReturn ret;
  GstEvent *event;
  GstBuffer *buffer;
  GstCaps *caps;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (bin);
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);

  g_signal_connect (bus, "message::error", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::warning", (GCallback) message_received, bin);
  g_signal_connect (bus, "message::eos", (GCallback) message_received, bin);

  /* just an adder and a fakesink */
  adder = gst_element_factory_make ("adder", "adder");
  sink = gst_element_factory_make ("fakesink", "sink");
  g_object_set (sink, "signal-handoffs", TRUE, NULL);
  g_signal_connect (sink, "handoff", (GCallback) handoff_buffer_cb, NULL);
  gst_bin_add_many (GST_BIN (bin), adder, sink, NULL);

  res = gst_element_link (adder, sink);
  fail_unless (res == TRUE, NULL);

  /* set to playing */
  state_res = gst_element_set_state (bin, GST_STATE_PLAYING);
  ck_assert_int_ne (state_res, GST_STATE_CHANGE_FAILURE);

  /* create an unconnected sinkpad in adder, should also automatically activate
   * the pad */
  sinkpad = gst_element_request_pad_simple (adder, "sink_%u");
  fail_if (sinkpad == NULL, NULL);

  gst_pad_send_event (sinkpad, gst_event_new_stream_start ("test"));

  caps = gst_caps_new_simple ("audio/x-raw",
#if G_BYTE_ORDER == G_BIG_ENDIAN
      "format", G_TYPE_STRING, "S16BE",
#else
      "format", G_TYPE_STRING, "S16LE",
#endif
      "layout", G_TYPE_STRING, "interleaved",
      "rate", G_TYPE_INT, 44100, "channels", G_TYPE_INT, 2, NULL);

  gst_pad_set_caps (sinkpad, caps);
  gst_caps_unref (caps);

  /* send segment to adder */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  segment.start = GST_SECOND;
  segment.stop = 2 * GST_SECOND;
  segment.time = 0;
  event = gst_event_new_segment (&segment);
  gst_pad_send_event (sinkpad, event);

  /* should be clipped and ok */
  buffer = new_buffer (44100, 0, 250 * GST_MSECOND);
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);
  fail_unless (handoff_buffer == NULL);

  /* should be partially clipped */
  buffer = new_buffer (44100, 900 * GST_MSECOND, 250 * GST_MSECOND);
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);
  fail_unless (handoff_buffer != NULL);
  gst_buffer_replace (&handoff_buffer, NULL);

  /* should not be clipped */
  buffer = new_buffer (44100, 1 * GST_SECOND, 250 * GST_MSECOND);
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);
  fail_unless (handoff_buffer != NULL);
  gst_buffer_replace (&handoff_buffer, NULL);

  /* should be clipped and ok */
  buffer = new_buffer (44100, 2 * GST_SECOND, 250 * GST_MSECOND);
  ret = gst_pad_chain (sinkpad, buffer);
  ck_assert_int_eq (ret, GST_FLOW_OK);
  fail_unless (handoff_buffer == NULL);

  gst_element_release_request_pad (adder, sinkpad);
  gst_object_unref (sinkpad);
  gst_element_set_state (bin, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (bin);
}

GST_END_TEST;

GST_START_TEST (test_duration_is_max)
{
  GstElement *bin, *src[3], *adder, *sink;
  GstStateChangeReturn state_res;
  GstFormat format = GST_FORMAT_TIME;
  gboolean res;
  gint64 duration;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");

  /* 3 sources, an adder and a fakesink */
  src[0] = gst_element_factory_make ("audiotestsrc", NULL);
  src[1] = gst_element_factory_make ("audiotestsrc", NULL);
  src[2] = gst_element_factory_make ("audiotestsrc", NULL);
  adder = gst_element_factory_make ("adder", "adder");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src[0], src[1], src[2], adder, sink, NULL);

  gst_element_link (src[0], adder);
  gst_element_link (src[1], adder);
  gst_element_link (src[2], adder);
  gst_element_link (adder, sink);

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
  GstElement *bin, *src[3], *adder, *sink;
  GstStateChangeReturn state_res;
  GstFormat format = GST_FORMAT_TIME;
  gboolean res;
  gint64 duration;

  GST_INFO ("preparing test");

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");

  /* 3 sources, an adder and a fakesink */
  src[0] = gst_element_factory_make ("audiotestsrc", NULL);
  src[1] = gst_element_factory_make ("audiotestsrc", NULL);
  src[2] = gst_element_factory_make ("audiotestsrc", NULL);
  adder = gst_element_factory_make ("adder", "adder");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (bin), src[0], src[1], src[2], adder, sink, NULL);

  gst_element_link (src[0], adder);
  gst_element_link (src[1], adder);
  gst_element_link (src[2], adder);
  gst_element_link (adder, sink);

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
  bin = setup_pipeline (NULL, 2);
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

#if 0
GST_START_TEST (test_flush_start_flush_stop)
{
  GstPadTemplate *sink_template;
  GstPad *tmppad, *sinkpad1, *sinkpad2, *adder_src;
  GstElement *pipeline, *src1, *src2, *adder, *sink;

  GST_INFO ("preparing test");

  /* build pipeline */
  pipeline = gst_pipeline_new ("pipeline");
  src1 = gst_element_factory_make ("audiotestsrc", "src1");
  g_object_set (src1, "wave", 4, NULL); /* silence */
  src2 = gst_element_factory_make ("audiotestsrc", "src2");
  g_object_set (src2, "wave", 4, NULL); /* silence */
  adder = gst_element_factory_make ("adder", "adder");
  sink = gst_element_factory_make ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (pipeline), src1, src2, adder, sink, NULL);

  sink_template =
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (adder),
      "sink_%u");
  fail_unless (GST_IS_PAD_TEMPLATE (sink_template));
  sinkpad1 = gst_element_request_pad (adder, sink_template, NULL, NULL);
  tmppad = gst_element_get_static_pad (src1, "src");
  gst_pad_link (tmppad, sinkpad1);
  gst_object_unref (tmppad);

  sinkpad2 = gst_element_request_pad (adder, sink_template, NULL, NULL);
  tmppad = gst_element_get_static_pad (src2, "src");
  gst_pad_link (tmppad, sinkpad2);
  gst_object_unref (tmppad);

  gst_element_link (adder, sink);

  /* prepare playing */
  set_state_and_wait (bin, GST_STATE_PLAYING);

  adder_src = gst_element_get_static_pad (adder, "src");
  fail_if (GST_PAD_IS_FLUSHING (adder_src));
  gst_pad_send_event (sinkpad1, gst_event_new_flush_start ());
  fail_unless (GST_PAD_IS_FLUSHING (adder_src));
  gst_pad_send_event (sinkpad1, gst_event_new_flush_stop (TRUE));
  fail_if (GST_PAD_IS_FLUSHING (adder_src));
  gst_object_unref (adder_src);

  gst_element_release_request_pad (adder, sinkpad1);
  gst_object_unref (sinkpad1);
  gst_element_release_request_pad (adder, sinkpad2);
  gst_object_unref (sinkpad2);

  /* cleanup */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

GST_END_TEST;
#endif

static Suite *
adder_suite (void)
{
  Suite *s = suite_create ("adder");
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
  /* This test is racy and occasionally fails in interesting ways
   * https://bugzilla.gnome.org/show_bug.cgi?id=708891
   * It's unlikely that it will ever be fixed for adder, works with audiomixer */
#if 0
  tcase_add_test (tc_chain, test_flush_start_flush_stop);
#endif
  tcase_add_checked_fixture (tc_chain, test_setup, test_teardown);

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

GST_CHECK_MAIN (adder);
