/* GStreamer
 *
 * example program for the ipcpipelinesrc/ipcpipelinesink elements
 *
 * Copyright (C) 2013-2014 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2013 Collabora Ltd.
 * Copyright (C) 2015 Centricular Ltd
 * Copyright (C) 2015-2017 YouView TV Ltd
 *   Author: George Kiagiadakis <george.kiagiadakis@collabora.com>
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

/*
 * Based on gst-play and ipcpipeline1. This program will play any URI
 * while splitting the pipeline in two processes, running the source & demuxer
 * on the master process and the decoders & sinks on the slave.
 * See keyboard_cb() for the various keyboard shortcuts you can use to
 * interact with it while the video window is focused.
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <gst/gst.h>
#include <gst/video/navigation.h>

static GMainLoop *loop;
static int pipes[2] = { -1, -1 };

static const char *arg_video_sink = "autovideosink";
static const char *arg_audio_sink = "autoaudiosink";

/******* MASTER *******/

#define GST_PLAY_KB_ARROW_UP    "\033[A"
#define GST_PLAY_KB_ARROW_DOWN  "\033[B"
#define GST_PLAY_KB_ARROW_RIGHT "\033[C"
#define GST_PLAY_KB_ARROW_LEFT  "\033[D"

typedef enum
{
  GST_PLAY_TRICK_MODE_NONE = 0,
  GST_PLAY_TRICK_MODE_DEFAULT,
  GST_PLAY_TRICK_MODE_DEFAULT_NO_AUDIO,
  GST_PLAY_TRICK_MODE_KEY_UNITS,
  GST_PLAY_TRICK_MODE_KEY_UNITS_NO_AUDIO,
  GST_PLAY_TRICK_MODE_LAST
} GstPlayTrickMode;

static GstPlayTrickMode trick_mode = GST_PLAY_TRICK_MODE_NONE;
static gdouble cur_rate = 1.0;
static gboolean buffering = FALSE;
static GstState desired_state = GST_STATE_PLAYING;

static gboolean play_do_seek (GstElement * pipeline, gint64 pos, gdouble rate,
    GstPlayTrickMode mode);

static void
toggle_paused (GstElement * pipeline)
{
  if (desired_state == GST_STATE_PLAYING)
    desired_state = GST_STATE_PAUSED;
  else
    desired_state = GST_STATE_PLAYING;

  if (!buffering) {
    gst_element_set_state (pipeline, desired_state);
  } else if (desired_state == GST_STATE_PLAYING) {
    g_print ("\nWill play as soon as buffering finishes)\n");
  }
}

static void
relative_seek (GstElement * pipeline, gdouble percent)
{
  GstQuery *query;
  gboolean seekable = FALSE;
  gint64 dur = -1, pos = -1, step;

  g_return_if_fail (percent >= -1.0 && percent <= 1.0);

  if (!gst_element_query_position (pipeline, GST_FORMAT_TIME, &pos))
    goto seek_failed;

  query = gst_query_new_seeking (GST_FORMAT_TIME);
  if (!gst_element_query (pipeline, query)) {
    gst_query_unref (query);
    goto seek_failed;
  }

  gst_query_parse_seeking (query, NULL, &seekable, NULL, &dur);
  gst_query_unref (query);

  if (!seekable || dur <= 0)
    goto seek_failed;

  step = dur * percent;
  if (ABS (step) < GST_SECOND)
    step = (percent < 0) ? -GST_SECOND : GST_SECOND;

  pos = pos + step;
  if (pos > dur) {
    g_print ("\nReached end of play list.\n");
    g_main_loop_quit (loop);
  } else {
    if (pos < 0)
      pos = 0;

    play_do_seek (pipeline, pos, cur_rate, trick_mode);
  }

  return;

seek_failed:
  {
    g_print ("\nCould not seek.\n");
  }
}

static gboolean
play_set_rate_and_trick_mode (GstElement * pipeline, gdouble rate,
    GstPlayTrickMode mode)
{
  gint64 pos = -1;

  g_return_val_if_fail (rate != 0, FALSE);

  if (!gst_element_query_position (pipeline, GST_FORMAT_TIME, &pos))
    return FALSE;

  return play_do_seek (pipeline, pos, rate, mode);
}

static gboolean
play_do_seek (GstElement * pipeline, gint64 pos, gdouble rate,
    GstPlayTrickMode mode)
{
  GstSeekFlags seek_flags;
  GstQuery *query;
  GstEvent *seek;
  gboolean seekable = FALSE;

  query = gst_query_new_seeking (GST_FORMAT_TIME);
  if (!gst_element_query (pipeline, query)) {
    gst_query_unref (query);
    return FALSE;
  }

  gst_query_parse_seeking (query, NULL, &seekable, NULL, NULL);
  gst_query_unref (query);

  if (!seekable)
    return FALSE;

  seek_flags = GST_SEEK_FLAG_FLUSH;

  switch (mode) {
    case GST_PLAY_TRICK_MODE_DEFAULT:
      seek_flags |= GST_SEEK_FLAG_TRICKMODE;
      break;
    case GST_PLAY_TRICK_MODE_DEFAULT_NO_AUDIO:
      seek_flags |= GST_SEEK_FLAG_TRICKMODE | GST_SEEK_FLAG_TRICKMODE_NO_AUDIO;
      break;
    case GST_PLAY_TRICK_MODE_KEY_UNITS:
      seek_flags |= GST_SEEK_FLAG_TRICKMODE_KEY_UNITS;
      break;
    case GST_PLAY_TRICK_MODE_KEY_UNITS_NO_AUDIO:
      seek_flags |=
          GST_SEEK_FLAG_TRICKMODE_KEY_UNITS | GST_SEEK_FLAG_TRICKMODE_NO_AUDIO;
      break;
    case GST_PLAY_TRICK_MODE_NONE:
    default:
      break;
  }

  if (rate >= 0)
    seek = gst_event_new_seek (rate, GST_FORMAT_TIME,
        seek_flags | GST_SEEK_FLAG_ACCURATE,
        /* start */ GST_SEEK_TYPE_SET, pos,
        /* stop */ GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
  else
    seek = gst_event_new_seek (rate, GST_FORMAT_TIME,
        seek_flags | GST_SEEK_FLAG_ACCURATE,
        /* start */ GST_SEEK_TYPE_SET, 0,
        /* stop */ GST_SEEK_TYPE_SET, pos);

  if (!gst_element_send_event (pipeline, seek))
    return FALSE;

  cur_rate = rate;
  trick_mode = mode;
  return TRUE;
}

static void
play_set_playback_rate (GstElement * pipeline, gdouble rate)
{
  if (play_set_rate_and_trick_mode (pipeline, rate, trick_mode)) {
    g_print ("Playback rate: %.2f", rate);
    g_print ("                               \n");
  } else {
    g_print ("\n");
    g_print ("Could not change playback rate to %.2f", rate);
    g_print (".\n");
  }
}

static void
play_set_relative_playback_rate (GstElement * pipeline, gdouble rate_step,
    gboolean reverse_direction)
{
  gdouble new_rate = cur_rate + rate_step;

  if (reverse_direction)
    new_rate *= -1.0;

  play_set_playback_rate (pipeline, new_rate);
}

static const gchar *
trick_mode_get_description (GstPlayTrickMode mode)
{
  switch (mode) {
    case GST_PLAY_TRICK_MODE_NONE:
      return "normal playback, trick modes disabled";
    case GST_PLAY_TRICK_MODE_DEFAULT:
      return "trick mode: default";
    case GST_PLAY_TRICK_MODE_DEFAULT_NO_AUDIO:
      return "trick mode: default, no audio";
    case GST_PLAY_TRICK_MODE_KEY_UNITS:
      return "trick mode: key frames only";
    case GST_PLAY_TRICK_MODE_KEY_UNITS_NO_AUDIO:
      return "trick mode: key frames only, no audio";
    default:
      break;
  }
  return "unknown trick mode";
}

static void
play_switch_trick_mode (GstElement * pipeline)
{
  GstPlayTrickMode new_mode = ++trick_mode;
  const gchar *mode_desc;

  if (new_mode == GST_PLAY_TRICK_MODE_LAST)
    new_mode = GST_PLAY_TRICK_MODE_NONE;

  mode_desc = trick_mode_get_description (new_mode);

  if (play_set_rate_and_trick_mode (pipeline, cur_rate, new_mode)) {
    g_print ("Rate: %.2f (%s)                      \n", cur_rate, mode_desc);
  } else {
    g_print ("\nCould not change trick mode to %s.\n", mode_desc);
  }
}

static void
keyboard_cb (const gchar * key_input, GstElement * pipeline)
{
  gchar key = '\0';

  /* only want to switch/case on single char, not first char of string */
  if (key_input[0] != '\0' && key_input[1] == '\0')
    key = g_ascii_tolower (key_input[0]);

  switch (key) {
    case ' ':
      toggle_paused (pipeline);
      break;
    case 'q':
    case 'Q':
      g_main_loop_quit (loop);
      break;
    case 'p':
      if (cur_rate > -0.2 && cur_rate < 0.0)
        play_set_relative_playback_rate (pipeline, 0.0, TRUE);
      else if (ABS (cur_rate) < 2.0)
        play_set_relative_playback_rate (pipeline, 0.1, FALSE);
      else if (ABS (cur_rate) < 4.0)
        play_set_relative_playback_rate (pipeline, 0.5, FALSE);
      else
        play_set_relative_playback_rate (pipeline, 1.0, FALSE);
      break;
    case 'o':
      if (cur_rate > 0.0 && cur_rate < 0.20)
        play_set_relative_playback_rate (pipeline, 0.0, TRUE);
      else if (ABS (cur_rate) <= 2.0)
        play_set_relative_playback_rate (pipeline, -0.1, FALSE);
      else if (ABS (cur_rate) <= 4.0)
        play_set_relative_playback_rate (pipeline, -0.5, FALSE);
      else
        play_set_relative_playback_rate (pipeline, -1.0, FALSE);
      break;
    case 'd':
      play_set_relative_playback_rate (pipeline, 0.0, TRUE);
      break;
    case 't':
      play_switch_trick_mode (pipeline);
      break;
    case 27:                   /* ESC */
      if (key_input[1] == '\0') {
        g_main_loop_quit (loop);
        break;
      }
    case '0':
      play_do_seek (pipeline, 0, cur_rate, trick_mode);
      break;
    case 'r':
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "ipc.master.requested");
      break;
    default:
      if (strcmp (key_input, GST_PLAY_KB_ARROW_RIGHT) == 0) {
        relative_seek (pipeline, +0.08);
      } else if (strcmp (key_input, GST_PLAY_KB_ARROW_LEFT) == 0) {
        relative_seek (pipeline, -0.01);
      } else {
        GST_INFO ("keyboard input:");
        for (; *key_input != '\0'; ++key_input)
          GST_INFO ("  code %3d", *key_input);
      }
      break;
  }
}

static gboolean
master_bus_msg (GstBus * bus, GstMessage * msg, gpointer data)
{
  GstPipeline *pipeline = data;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      g_printerr ("MASTER: ERROR: %s\n", err->message);
      if (dbg != NULL)
        g_printerr ("MASTER: ERROR debug information: %s\n", dbg);
      g_error_free (err);
      g_free (dbg);

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "ipc.master.error");

      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_WARNING:{
      GError *err;
      gchar *dbg;

      gst_message_parse_warning (msg, &err, &dbg);
      g_printerr ("MASTER: WARNING: %s\n", err->message);
      if (dbg != NULL)
        g_printerr ("MASTER: WARNING debug information: %s\n", dbg);
      g_error_free (err);
      g_free (dbg);

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "ipc.master.warning");
      break;
    }
    case GST_MESSAGE_ASYNC_DONE:
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "ipc.master.async-done");
      break;
    case GST_MESSAGE_EOS:
      g_print ("EOS on master\n");
      gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_BUFFERING:{
      gint percent;
      GstBufferingMode bufmode;

      if (!buffering)
        g_print ("\n");

      gst_message_parse_buffering (msg, &percent);
      g_print ("%s %d%%  \r", "Buffering...", percent);

      gst_message_parse_buffering_stats (msg, &bufmode, NULL, NULL, NULL);

      /* no state management needed for live pipelines */
      if (bufmode != GST_BUFFERING_LIVE) {
        if (percent == 100) {
          /* a 100% message means buffering is done */
          if (buffering) {
            buffering = FALSE;
            gst_element_set_state (GST_ELEMENT (pipeline), desired_state);
            g_print ("\n%s\n", gst_element_state_get_name (desired_state));
          }
        } else {
          /* buffering... */
          if (!buffering) {
            gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PAUSED);
            buffering = TRUE;
          }
        }
      }
      break;
    }
    case GST_MESSAGE_CLOCK_LOST:{
      g_print ("Clock lost, selecting a new one\n");
      gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PAUSED);
      gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
      break;
    }
    case GST_MESSAGE_LATENCY:
    {
      gst_bin_recalculate_latency (GST_BIN (pipeline));
      break;
    }
    case GST_MESSAGE_REQUEST_STATE:{
      GstState state;
      gchar *name;

      name = gst_object_get_path_string (GST_MESSAGE_SRC (msg));

      gst_message_parse_request_state (msg, &state);

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_VERBOSE, "ipc.slave.reqstate");

      g_print ("Setting state to %s as requested by %s...\n",
          gst_element_state_get_name (state), name);

      gst_element_set_state (GST_ELEMENT (pipeline), state);
      g_free (name);
      break;
    }
    case GST_MESSAGE_ELEMENT:
    {
      GstNavigationMessageType mtype = gst_navigation_message_get_type (msg);
      if (mtype == GST_NAVIGATION_MESSAGE_EVENT) {
        GstEvent *ev = NULL;

        if (gst_navigation_message_parse_event (msg, &ev)) {
          GstNavigationEventType e_type = gst_navigation_event_get_type (ev);
          switch (e_type) {
            case GST_NAVIGATION_EVENT_KEY_PRESS:
            {
              const gchar *key;

              if (gst_navigation_event_parse_key_event (ev, &key)) {
                GST_INFO ("Key press: %s", key);

                if (strcmp (key, "Left") == 0)
                  key = GST_PLAY_KB_ARROW_LEFT;
                else if (strcmp (key, "Right") == 0)
                  key = GST_PLAY_KB_ARROW_RIGHT;
                else if (strcmp (key, "Up") == 0)
                  key = GST_PLAY_KB_ARROW_UP;
                else if (strcmp (key, "Down") == 0)
                  key = GST_PLAY_KB_ARROW_DOWN;
                else if (strcmp (key, "space") == 0)
                  key = " ";
                else if (strlen (key) > 1)
                  break;

                keyboard_cb (key, GST_ELEMENT (pipeline));
              }
              break;
            }
            case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:
            {
              gint button;
              if (gst_navigation_event_parse_mouse_button_event (ev, &button,
                      NULL, NULL)) {
                if (button == 4) {
                  /* wheel up */
                  relative_seek (GST_ELEMENT (pipeline), +0.08);
                } else if (button == 5) {
                  /* wheel down */
                  relative_seek (GST_ELEMENT (pipeline), -0.01);
                }
              }
              break;
            }
            default:
              break;
          }
        }
        if (ev)
          gst_event_unref (ev);
      }
      break;
    }
    default:
      break;
  }
  return TRUE;
}

static int
sendfd (int s, int fd)
{
  char buf[1];
  struct iovec iov;
  struct msghdr msg;
  struct cmsghdr *cmsg;
  int n;
  char cms[CMSG_SPACE (sizeof (int))];

  buf[0] = 0;
  iov.iov_base = buf;
  iov.iov_len = 1;

  memset (&msg, 0, sizeof msg);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = (caddr_t) cms;
  msg.msg_controllen = CMSG_LEN (sizeof (int));

  cmsg = CMSG_FIRSTHDR (&msg);
  cmsg->cmsg_len = CMSG_LEN (sizeof (int));
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  memmove (CMSG_DATA (cmsg), &fd, sizeof (int));

  if ((n = sendmsg (s, &msg, 0)) != iov.iov_len)
    return -1;
  return 0;
}

static gint
find_ipcpipelinesink (gconstpointer e, gconstpointer c)
{
  const GValue *elem = e;
  const gchar *caps_name = c;
  const gchar *n = g_object_get_data (g_value_get_object (elem),
      "ipcpipelinesink-caps-name");
  return g_strcmp0 (caps_name, n);
}

/* in HLS the decodebin pads are destroyed and re-created every time
 * the stream changes bitrate. This trick here ensures that the new
 * pads that will appear will go and link to the same ipcpipelinesinks,
 * avoiding the creation of new pipelines in the slave. */
static void
on_pad_unlinked (GstPad * pad, GstPad * peer, GstElement * pipeline)
{
  GstCaps *caps;
  const GstStructure *structure;

  caps = gst_pad_get_current_caps (pad);
  structure = gst_caps_get_structure (caps, 0);

  g_object_set_data_full (G_OBJECT (GST_OBJECT_PARENT (peer)),
      "ipcpipelinesink-caps-name",
      g_strdup (gst_structure_get_name (structure)), g_free);
}

static void
on_pad_added (GstElement * element, GstPad * pad, GstElement * pipeline)
{
  GstElement *ipcpipelinesink;
  GstPad *sinkpad;
  GstCaps *caps;
  const GstStructure *structure;
  GstIterator *it;
  GValue elem = G_VALUE_INIT;
  int sockets[2];
  gboolean create_sockets;

  caps = gst_pad_get_current_caps (pad);
  structure = gst_caps_get_structure (caps, 0);

  it = gst_bin_iterate_sinks (GST_BIN (pipeline));
  if (gst_iterator_find_custom (it, find_ipcpipelinesink, &elem,
          (gpointer) gst_structure_get_name (structure))) {
    ipcpipelinesink = g_value_get_object (&elem);
    create_sockets = FALSE;
    g_value_reset (&elem);
  } else {
    ipcpipelinesink = gst_element_factory_make ("ipcpipelinesink", NULL);
    gst_bin_add (GST_BIN (pipeline), ipcpipelinesink);
    create_sockets = TRUE;
  }

  sinkpad = gst_element_get_static_pad (ipcpipelinesink, "sink");
  if (gst_pad_link (pad, sinkpad) != GST_PAD_LINK_OK) {
    fprintf (stderr, "Failed to link ipcpipelinesink\n");
    exit (1);
  }
  gst_object_unref (sinkpad);

  g_signal_connect (pad, "unlinked", (GCallback) on_pad_unlinked, pipeline);

  if (create_sockets) {
    if (socketpair (AF_UNIX, SOCK_STREAM, 0, sockets)) {
      fprintf (stderr, "Error creating sockets: %s\n", strerror (errno));
      exit (1);
    }
    if (fcntl (sockets[0], F_SETFL, O_NONBLOCK) < 0 ||
        fcntl (sockets[1], F_SETFL, O_NONBLOCK) < 0) {
      fprintf (stderr, "Error setting O_NONBLOCK on sockets: %s\n",
          strerror (errno));
      exit (1);
    }
    g_object_set (ipcpipelinesink, "fdin", sockets[0], "fdout", sockets[0],
        NULL);

    printf ("new socket %d\n", sockets[1]);
    sendfd (pipes[1], sockets[1]);
  }

  gst_element_set_state (ipcpipelinesink, GST_STATE_PLAYING);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "pad.added");
}

typedef enum
{
  GST_AUTOPLUG_SELECT_TRY,
  GST_AUTOPLUG_SELECT_EXPOSE,
  GST_AUTOPLUG_SELECT_SKIP
} GstAutoplugSelectResult;

static GstAutoplugSelectResult
on_autoplug_select (GstElement * uridecodebin, GstPad * pad, GstCaps * caps,
    GstElementFactory * factory, GstElement * pipeline)
{
  /* if decodebin is about to plug a decoder,
   * stop it right there and expose the pad;
   * the slave's decodebin will take it from there... */
  if (gst_element_factory_list_is_type (factory,
          GST_ELEMENT_FACTORY_TYPE_DECODER)) {
    gchar *capsstr = gst_caps_to_string (caps);
    g_print (" exposing to slave: %s\n", capsstr);
    g_free (capsstr);
    return GST_AUTOPLUG_SELECT_EXPOSE;
  }
  return GST_AUTOPLUG_SELECT_TRY;
}

static void
start_source (const gchar * uri)
{
  GstElement *pipeline;
  GstElement *uridecodebin;
  GstBus *bus;

  pipeline = gst_pipeline_new (NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, master_bus_msg, pipeline);
  gst_object_unref (bus);

  uridecodebin = gst_element_factory_make ("uridecodebin", NULL);
  g_object_set (uridecodebin, "uri", uri, NULL);
  g_signal_connect (uridecodebin, "pad-added", G_CALLBACK (on_pad_added),
      pipeline);
  g_signal_connect (uridecodebin, "autoplug-select",
      G_CALLBACK (on_autoplug_select), pipeline);

  gst_bin_add (GST_BIN (pipeline), uridecodebin);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

/*********** SLAVE ***********/

static gboolean
slave_bus_msg (GstBus * bus, GstMessage * msg, gpointer data)
{
  GstPipeline *pipeline = data;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      g_printerr ("SLAVE: ERROR: %s\n", err->message);
      if (dbg != NULL)
        g_printerr ("SLAVE: ERROR debug information: %s\n", dbg);
      g_error_free (err);
      g_free (dbg);

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "ipc.slave.error");
      break;
    }
    case GST_MESSAGE_WARNING:{
      GError *err;
      gchar *dbg;

      gst_message_parse_warning (msg, &err, &dbg);
      g_printerr ("SLAVE: WARNING: %s\n", err->message);
      if (dbg != NULL)
        g_printerr ("SLAVE: WARNING debug information: %s\n", dbg);
      g_error_free (err);
      g_free (dbg);

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "ipc.slave.warning");
      break;
    }
    case GST_MESSAGE_ASYNC_START:
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_VERBOSE, "ipc.slave.async-start");
      break;
    case GST_MESSAGE_ASYNC_DONE:
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "ipc.slave.async-done");
      break;
    default:
      break;
  }
  return TRUE;
}

static void
on_decoded_pad_added (GstElement * element, GstPad * pad, gpointer data)
{
  GstBin *pipeline = data;
  GstCaps *caps;
  GstPad *cpad;
  const gchar *type;
  gchar *capsstr;

  caps = gst_pad_get_current_caps (pad);
  capsstr = gst_caps_to_string (caps);
  printf (" caps: %s\n", capsstr);
  g_free (capsstr);

  type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  if (!strcmp (type, "video/x-raw")) {
    GstElement *c, *s;
    c = gst_element_factory_make ("videoconvert", NULL);
    s = gst_element_factory_make (arg_video_sink, NULL);
    gst_bin_add_many (GST_BIN (pipeline), c, s, NULL);
    gst_element_link_many (c, s, NULL);
    cpad = gst_element_get_static_pad (c, "sink");
    gst_pad_link (pad, cpad);
    gst_object_unref (cpad);
    gst_element_set_state (s, GST_STATE_PLAYING);
    gst_element_set_state (c, GST_STATE_PLAYING);
  } else if (!strcmp (type, "audio/x-raw")) {
    GstElement *c, *s;
    c = gst_element_factory_make ("audioconvert", NULL);
    s = gst_element_factory_make (arg_audio_sink, NULL);
    gst_bin_add_many (GST_BIN (pipeline), c, s, NULL);
    gst_element_link_many (c, s, NULL);
    cpad = gst_element_get_static_pad (c, "sink");
    gst_pad_link (pad, cpad);
    gst_object_unref (cpad);
    gst_element_set_state (s, GST_STATE_PLAYING);
    gst_element_set_state (c, GST_STATE_PLAYING);
  } else {
    GstElement *s;
    s = gst_element_factory_make ("fakesink", NULL);
    g_object_set (s, "sync", TRUE, "async", TRUE, NULL);
    gst_bin_add_many (GST_BIN (pipeline), s, NULL);
    cpad = gst_element_get_static_pad (s, "sink");
    gst_pad_link (pad, cpad);
    gst_object_unref (cpad);
    gst_element_set_state (s, GST_STATE_PLAYING);
  }

  gst_caps_unref (caps);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "decoded.pad.added");
}

static int
recvfd (int s)
{
  int n;
  int fd;
  char buf[1];
  struct iovec iov;
  struct msghdr msg;
  struct cmsghdr *cmsg;
  char cms[CMSG_SPACE (sizeof (int))];

  iov.iov_base = buf;
  iov.iov_len = 1;

  memset (&msg, 0, sizeof msg);
  msg.msg_name = 0;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  msg.msg_control = (caddr_t) cms;
  msg.msg_controllen = sizeof cms;

  if ((n = recvmsg (s, &msg, 0)) < 0)
    return -1;
  if (n == 0) {
    perror ("unexpected EOF");
    return -1;
  }
  cmsg = CMSG_FIRSTHDR (&msg);
  memmove (&fd, CMSG_DATA (cmsg), sizeof (int));
  return fd;
}

static gboolean
pipe_reader (gpointer data)
{
  GstElement *pipeline = data;
  GstElement *ipcpipelinesrc, *mq, *decodebin;
  GstPad *rpad, *sink_pad;
  int fd;
  fd_set set;
  struct timeval tv;
  int ret;
  static int idx = 0;
  char name[32];

  FD_ZERO (&set);
  FD_SET (pipes[0], &set);
  tv.tv_sec = tv.tv_usec = 0;
  ret = select (pipes[0] + 1, &set, NULL, NULL, &tv);
  if (ret < 0) {
    fprintf (stderr, "Failed to select: %s\n", strerror (errno));
    return TRUE;
  }
  if (!FD_ISSET (pipes[0], &set))
    return TRUE;

  fd = recvfd (pipes[0]);
  ipcpipelinesrc = gst_element_factory_make ("ipcpipelinesrc", NULL);
  gst_bin_add (GST_BIN (pipeline), ipcpipelinesrc);
  g_object_set (ipcpipelinesrc, "fdin", fd, "fdout", fd, NULL);


  mq = gst_bin_get_by_name (GST_BIN (pipeline), "mq");
  if (!mq) {
    fprintf (stderr, "Failed to get mq\n");
    return TRUE;
  }
  if (!gst_element_link (ipcpipelinesrc, mq)) {
    fprintf (stderr, "Failed to link ipcpipelinesrc and mq\n");
    return TRUE;
  }

  snprintf (name, sizeof (name), "src_%u", idx++);
  rpad = gst_element_get_static_pad (mq, name);
  if (!rpad) {
    fprintf (stderr, "Failed to get mq request pad\n");
    return TRUE;
  }

  decodebin = gst_element_factory_make ("decodebin", NULL);
  gst_bin_add (GST_BIN (pipeline), decodebin);
  sink_pad = gst_element_get_static_pad (decodebin, "sink");
  gst_pad_link (rpad, sink_pad);
  gst_object_unref (sink_pad);

  g_signal_connect (decodebin, "pad-added", G_CALLBACK (on_decoded_pad_added),
      pipeline);

  /* dynamically added elements should be synced manually
   * to the state of the slave pipeline */
  gst_element_sync_state_with_parent (ipcpipelinesrc);
  gst_element_sync_state_with_parent (decodebin);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "ipc.slave.added");
  gst_object_unref (mq);

  return TRUE;
}

static void
start_sink (void)
{
  GstElement *pipeline;
  GstElement *multiqueue;

  pipeline = gst_element_factory_make ("ipcslavepipeline", NULL);
  gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), slave_bus_msg, pipeline);

  multiqueue = gst_element_factory_make ("multiqueue", "mq");
  gst_bin_add (GST_BIN (pipeline), multiqueue);

  g_timeout_add (10, &pipe_reader, gst_object_ref (pipeline));

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "ipc.sink");
  /* The state of the slave pipeline will change together with the state
   * of the master, there is no need to call gst_element_set_state() here */
}


/********** COMMON ***********/

static void
init (int *argc, char ***argv)
{
  GOptionEntry options[] = {
    {"audio-sink", 0, 0, G_OPTION_ARG_STRING, &arg_audio_sink,
        "Audio sink element to use (default autoaudiosink)", NULL},
    {"video-sink", 0, 0, G_OPTION_ARG_STRING, &arg_video_sink,
        "Video sink element to use (default autovideosink)", NULL},
    {NULL}
  };
  GOptionContext *ctx;
  GError *err = NULL;

  ctx = g_option_context_new ("");
  g_option_context_add_main_entries (ctx, options, "");
  if (!g_option_context_parse (ctx, argc, argv, &err)) {
    fprintf (stderr, "Error initializing: %s\n", err->message);
    exit (1);
  }
  g_option_context_free (ctx);
}

static void
run (pid_t pid)
{
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
  if (pid > 0)
    kill (pid, SIGTERM);
}

gint
main (gint argc, gchar ** argv)
{
  GError *error = NULL;
  gchar *uri = NULL;
  pid_t pid;

  init (&argc, &argv);

  if (argc < 2) {
    fprintf (stderr, "usage: %s [av-filename-or-url]\n", argv[0]);
    return 1;
  }

  if (!g_strstr_len (argv[1], -1, "://")) {
    uri = gst_filename_to_uri (argv[1], &error);
  } else {
    uri = g_strdup (argv[1]);
  }

  if (error) {
    fprintf (stderr, "usage: %s [av-filename-or-url]\n", argv[0]);
    g_clear_error (&error);
    return 1;
  }

  if (socketpair (AF_UNIX, SOCK_STREAM, 0, pipes)) {
    fprintf (stderr, "Error creating pipes: %s\n", strerror (errno));
    return 2;
  }
  if (fcntl (pipes[0], F_SETFL, O_NONBLOCK) < 0 ||
      fcntl (pipes[1], F_SETFL, O_NONBLOCK) < 0) {
    fprintf (stderr, "Error setting O_NONBLOCK on pipes: %s\n",
        strerror (errno));
    return 2;
  }

  pid = fork ();
  if (pid < 0) {
    fprintf (stderr, "Error forking: %s\n", strerror (errno));
    return 1;
  } else if (pid > 0) {
    setenv ("GST_DEBUG_FILE", "gstsrc.log", 1);
    gst_init (&argc, &argv);
    start_source (uri);
  } else {
    setenv ("GST_DEBUG_FILE", "gstsink.log", 1);
    gst_init (&argc, &argv);
    start_sink ();
  }

  g_free (uri);
  run (pid);

  return 0;
}
