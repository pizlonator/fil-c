/* Test example which plays a given file forward, then
 * at EOS, plays the entire file in reverse
 * and checks that reverse playback generates the same
 * output as forward playback but reversed
 */
/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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
#include <gst/gst.h>
#include <string.h>

typedef struct _PlayState PlayState;

typedef struct _StreamTSRange
{
  GstClockTime start;
  GstClockTime end;
} StreamTSRange;

typedef struct _StreamInfo
{
  PlayState *state;
  GstPad *pad;

  GstSegment seg;

  GArray *fwd_times;
  GArray *bkwd_times;
} StreamInfo;

struct _PlayState
{
  GstElement *pipe;
  GMainLoop *loop;
  gboolean fwd_play;
  gint n_sinks;

  GMutex output_lock;
};

static void
warning_cb (GstBus * bus, GstMessage * msg, gpointer foo)
{
  GError *err = NULL;
  gchar *dbg = NULL;

  gst_message_parse_warning (msg, &err, &dbg);

  g_printerr ("WARNING: %s (%s)\n", err->message, (dbg) ? dbg : "no details");

  g_error_free (err);
  g_free (dbg);
}

static void
error_cb (GstBus * bus, GstMessage * msg, PlayState * state)
{
  GError *err = NULL;
  gchar *dbg = NULL;

  gst_message_parse_error (msg, &err, &dbg);

  g_printerr ("ERROR: %s (%s)\n", err->message, (dbg) ? dbg : "no details");

  g_main_loop_quit (state->loop);

  g_error_free (err);
  g_free (dbg);
}

static void
eos_cb (GstBus * bus, GstMessage * msg, PlayState * state)
{
  if (state->fwd_play) {
    g_print ("EOS - finished forward play. Starting reverse\n");
    state->fwd_play = FALSE;
    gst_element_seek (state->pipe, -1.0, GST_FORMAT_TIME,
        GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH,
        GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_END, 0);

    return;
  }
  g_print ("EOS - exiting\n");
  g_main_loop_quit (state->loop);
}

static void
state_cb (GstBus * bus, GstMessage * msg, PlayState * state)
{
  if (msg->src == GST_OBJECT (state->pipe)) {
    GstState old_state, new_state, pending_state;

    gst_message_parse_state_changed (msg, &old_state, &new_state,
        &pending_state);
    if (new_state == GST_STATE_PLAYING)
      g_print ("Decoding ...\n");
  }
}

static void
_destroy_stream_info (StreamInfo * si)
{
  g_array_free (si->fwd_times, TRUE);
  g_array_free (si->bkwd_times, TRUE);
  g_object_unref (si->pad);
  g_free (si);
}

static void
extend_times (StreamInfo * si, GstClockTime start, GstClockTime end)
{
  PlayState *state = si->state;
  StreamTSRange *ts = NULL;
  StreamTSRange tsn;
  GArray *a;
  guint i, n;

  /* Set up new entry, in case we need it */
  tsn.start = start;
  tsn.end = end;

  if (state->fwd_play) {
    a = si->fwd_times;
    n = a->len;
    /* if playing forward, see if this new time extends the last entry */
    i = n - 1;
  } else {
    a = si->bkwd_times;
    n = a->len;
    /* if playing backward, see if this new time extends the earliest entry */
    i = 0;
  }

  if (n > 0) {
    ts = &g_array_index (a, StreamTSRange, i);
    if (start > ts->start) {
      /* This entry is after the most recent entry */
      /* Tolerance of 1 millisecond allowed for imprecision */
      if (ts->end + GST_MSECOND >= start) {
        GST_LOG ("%p extending entry %d to %" GST_TIME_FORMAT,
            si, i, GST_TIME_ARGS (end));
        ts->end = end;
        return;
      }

      /* new start > ts->end, so this new entry goes after the first one */
      GST_LOG ("%p inserting new entry %d %" GST_TIME_FORMAT
          " to %" GST_TIME_FORMAT, si, i + 1, GST_TIME_ARGS (start),
          GST_TIME_ARGS (end));
      g_array_insert_val (a, i + 1, tsn);
      return;
    } else if (end + GST_MSECOND > ts->start) {
      /* This entry precedes the current one, but overlaps it */
      GST_LOG ("%p pre-extending entry %d to %" GST_TIME_FORMAT,
          si, i, GST_TIME_ARGS (start));
      ts->start = start;
      return;
    }
  } else {
    i = 0;
  }

  /* otherwise insert a new entry before/at the start */
  GST_LOG ("%p New entry %d - %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT,
      si, i, GST_TIME_ARGS (start), GST_TIME_ARGS (end));
  g_array_insert_val (a, i, tsn);
}

static void
dump_times (StreamInfo * si)
{
  PlayState *state = si->state;
  guint i;
  GArray *a;

  g_mutex_lock (&state->output_lock);
  if (state->fwd_play)
    a = si->fwd_times;
  else
    a = si->bkwd_times;

  g_print ("Pad %s times:\n", GST_PAD_NAME (si->pad));
  for (i = 0; i < a->len; i++) {
    StreamTSRange *ts = &g_array_index (a, StreamTSRange, i);

    g_print ("  %u %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT "\n",
        i, GST_TIME_ARGS (ts->start), GST_TIME_ARGS (ts->end));
  }
  g_mutex_unlock (&state->output_lock);
}

static GstPadProbeReturn
handle_output (GstPad * pad, GstPadProbeInfo * info, StreamInfo * si)
{
  GstClockTime start, end;
  GstBuffer *buf;

  GST_LOG_OBJECT (pad, "Fired probe type 0x%x", info->type);

  if (info->type & GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    g_warning ("Buffer list handling not implemented");
    return GST_PAD_PROBE_DROP;
  }

  if (info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
    GstEvent *event = gst_pad_probe_info_get_event (info);
    switch (GST_EVENT_TYPE (event)) {
      case GST_EVENT_SEGMENT:
        gst_event_copy_segment (event, &si->seg);
        break;
      case GST_EVENT_EOS:
        dump_times (si);
        break;
      default:
        break;
    }
    return GST_PAD_PROBE_PASS;
  }

  buf = gst_pad_probe_info_get_buffer (info);
  if (!GST_BUFFER_PTS_IS_VALID (buf))
    goto done;
  end = start = GST_BUFFER_PTS (buf);

  if (GST_BUFFER_DURATION_IS_VALID (buf))
    end += GST_BUFFER_DURATION (buf);

  gst_segment_clip (&si->seg, GST_FORMAT_TIME, start, end, &start, &end);
  start = gst_segment_to_stream_time (&si->seg, GST_FORMAT_TIME, start);
  end = gst_segment_to_stream_time (&si->seg, GST_FORMAT_TIME, end);

  GST_DEBUG_OBJECT (pad, "new buffer %" GST_TIME_FORMAT
      " to %" GST_TIME_FORMAT, GST_TIME_ARGS (start), GST_TIME_ARGS (end));

  /* Now extend measured time range to include new times */
  extend_times (si, start, end);

done:
  return GST_PAD_PROBE_PASS;
}

static void
pad_added_cb (GstElement * decodebin, GstPad * pad, PlayState * state)
{
  GstPadLinkReturn ret;
  GstElement *fakesink;
  GstPad *fakesink_pad;
  StreamInfo *si;

  fakesink = gst_element_factory_make ("fakesink", NULL);
#if 0
  if (state->n_sinks == 1)
    g_object_set (fakesink, "silent", FALSE, NULL);
#endif

  si = g_new0 (StreamInfo, 1);
  si->pad = g_object_ref (pad);
  si->state = state;
  si->fwd_times = g_array_new (FALSE, TRUE, sizeof (StreamTSRange));
  si->bkwd_times = g_array_new (FALSE, TRUE, sizeof (StreamTSRange));

  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM,
      (GstPadProbeCallback) handle_output, si, (GDestroyNotify)
      _destroy_stream_info);

  state->n_sinks++;
  gst_bin_add (GST_BIN (state->pipe), fakesink);

  gst_element_sync_state_with_parent (fakesink);

  fakesink_pad = gst_element_get_static_pad (fakesink, "sink");

  ret = gst_pad_link (pad, fakesink_pad);
  if (!GST_PAD_LINK_SUCCESSFUL (ret)) {
    g_printerr ("Failed to link %s:%s to %s:%s (ret = %d)\n",
        GST_DEBUG_PAD_NAME (pad), GST_DEBUG_PAD_NAME (fakesink_pad), ret);
  } else {
    GstCaps *caps = gst_pad_get_current_caps (pad);
    gchar *s = gst_caps_to_string (caps);

    g_print ("Linked %s:%s to %s:%s caps %s\n", GST_DEBUG_PAD_NAME (pad),
        GST_DEBUG_PAD_NAME (fakesink_pad), s);
    gst_caps_unref (caps);
    g_free (s);
  }

  gst_object_unref (fakesink_pad);
}

gint
main (gint argc, gchar * argv[])
{
  PlayState state;
  GstElement *decoder;
  GstStateChangeReturn res;
  GstBus *bus;

  gst_init (&argc, &argv);

  if (argc != 2) {
    g_printerr ("Decode file from start to end.\n");
    g_printerr ("Usage: %s URI\n\n", argv[0]);
    return 1;
  }
  /* Start with zeroed-state */
  memset (&state, 0, sizeof (PlayState));

  state.loop = g_main_loop_new (NULL, TRUE);
  state.pipe = gst_pipeline_new ("pipeline");
  state.fwd_play = TRUE;
  g_mutex_init (&state.output_lock);

  bus = gst_pipeline_get_bus (GST_PIPELINE (state.pipe));
  gst_bus_add_signal_watch (bus);

  g_signal_connect (bus, "message::eos", G_CALLBACK (eos_cb), &state);
  g_signal_connect (bus, "message::error", G_CALLBACK (error_cb), &state);
  g_signal_connect (bus, "message::warning", G_CALLBACK (warning_cb), NULL);
  g_signal_connect (bus, "message::state-changed", G_CALLBACK (state_cb),
      &state);

#if 0
  g_signal_connect (state.pipe, "deep-notify",
      G_CALLBACK (gst_object_default_deep_notify), NULL);
#endif

  decoder = gst_element_factory_make ("uridecodebin", "decoder");
  g_assert (decoder);
  gst_bin_add (GST_BIN (state.pipe), decoder);

  if (argv[1] && strstr (argv[1], "://") != NULL) {
    g_object_set (G_OBJECT (decoder), "uri", argv[1], NULL);
  } else if (argv[1]) {
    gchar *uri = g_strdup_printf ("file://%s", argv[1]);
    g_object_set (G_OBJECT (decoder), "uri", uri, NULL);
    g_free (uri);
  } else {
    g_print ("Usage: %s <filename|uri>\n", argv[0]);
    return -1;
  }

  g_signal_connect (decoder, "pad-added", G_CALLBACK (pad_added_cb), &state);

  res = gst_element_set_state (state.pipe, GST_STATE_PLAYING);
  if (res == GST_STATE_CHANGE_FAILURE) {
    g_print ("could not play\n");
    return -1;
  }

  g_main_loop_run (state.loop);

  /* tidy up */
  gst_element_set_state (state.pipe, GST_STATE_NULL);
  gst_object_unref (state.pipe);
  gst_object_unref (bus);

  return 0;
}
