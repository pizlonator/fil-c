/* Test example for instant rate changes.
 * The example takes an input URI and runs a set of actions,
 * seeking and pausing etc.
 *
 * Copyright (C) 2015-2018 Centricular Ltd
 *  @author:  Edward Hervey <edward@centricular.com>
 *  @author:  Jan Schmidt <jan@centricular.com>
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

#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#include <gst/gst.h>
#include <stdlib.h>

/* There are several supported scenarios
0) Play rate to 1x -> Apply 'instant-rate-change' to 2x -> Apply 'instant-rate-change' to 0.25x (repeat as fast as possible for 2 sec) -> let play for 2s
1) Play rate to 1x -> Apply 'instant-rate-change' to 2x -> run for 10s, then pause -> wait 10s -> play
2) Play rate to 1x -> Apply 'instant-rate-change' to 2x -> run for 10s, then pause -> wait 10s -> Apply 'instant-rate-change' to 1x -> play
3) Play rate to 1x -> Apply 'instant-rate-change' to 2x -> run for 10s, then pause -> seeking (flush+key-unit) to 30s -> wait 10s -> play
4) Play rate to 1x -> Apply 'instant-rate-change' to 2x -> Apply 'instant-rate-change' to 0.25x (toggle every 500ms)
*/

#define PLAY_PAUSE_DELAY 10
#define IDLE_CYCLE_DELAY 2

#define TARGET_RATE_1 0.25
#define TARGET_RATE_2 2.0

/* Set DISABLE_AUDIO to run with video only */
// #define DISABLE_AUDIO

/* Set to force the use of the system clock on the pipeline */
// #define FORCE_SYSTEM_CLOCK

typedef struct _MyDataStruct
{
  GMainLoop *mainloop;
  GstElement *pipeline;
  GstBus *bus;

  gdouble rate;
  gboolean paused;

  guint scenario;
  GstClockTime start;
  GstClock *clock;

  guint timeout_id;
  guint idle_id;
} MyDataStruct;

static gboolean toggle_rate (MyDataStruct * data);

static gboolean
do_enable_disable_idle (MyDataStruct * data)
{
  if (data->idle_id) {
    g_print ("Disabling idle handler\n");
    g_source_remove (data->idle_id);
    data->idle_id = 0;
  } else {
    g_print ("Enabling idle handler\n");
    data->idle_id = g_idle_add ((GSourceFunc) toggle_rate, data);
  }

  return TRUE;
}

static gboolean
do_play_pause (MyDataStruct * data)
{
  data->paused = !data->paused;

  switch (data->scenario) {
    case 1:
      g_print ("%s\n", data->paused ? "Pausing" : "Unpausing");
      gst_element_set_state (data->pipeline,
          data->paused ? GST_STATE_PAUSED : GST_STATE_PLAYING);
      data->timeout_id = g_timeout_add_seconds (PLAY_PAUSE_DELAY,
          (GSourceFunc) do_play_pause, data);
      break;
    case 2:
      if (!data->paused) {
        gint64 pos = GST_CLOCK_TIME_NONE;
        gst_element_query_position (data->pipeline, GST_FORMAT_TIME, &pos);

        /* Change rate between 2x and 1x before unpausing */
        data->rate = (data->rate == 2.0) ? 1.0 : 2.0;
        g_print ("Switching rate to %f (position %" GST_TIME_FORMAT ")\n",
            data->rate, GST_TIME_ARGS (pos));
        gst_element_send_event (data->pipeline, gst_event_new_seek (data->rate,
                GST_FORMAT_TIME, GST_SEEK_FLAG_INSTANT_RATE_CHANGE,
                GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE, GST_SEEK_TYPE_NONE,
                GST_CLOCK_TIME_NONE));
      }

      g_print ("%s\n", data->paused ? "Pausing" : "Unpausing");
      gst_element_set_state (data->pipeline,
          data->paused ? GST_STATE_PAUSED : GST_STATE_PLAYING);
      data->timeout_id = g_timeout_add_seconds (PLAY_PAUSE_DELAY,
          (GSourceFunc) do_play_pause, data);
      break;
    case 3:
      g_print ("%s\n", data->paused ? "Pausing" : "Unpausing");
      gst_element_set_state (data->pipeline,
          data->paused ? GST_STATE_PAUSED : GST_STATE_PLAYING);
      if (data->paused) {
        /* On pause, seek to 30 seconds */
        g_print ("Seeking to 30s\n");
        gst_element_send_event (data->pipeline,
            gst_event_new_seek (data->rate, GST_FORMAT_TIME,
                GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                GST_SEEK_TYPE_SET, 30 * GST_SECOND,
                GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE));
      }
      data->timeout_id = g_timeout_add_seconds (PLAY_PAUSE_DELAY,
          (GSourceFunc) do_play_pause, data);
      break;
    default:
      break;
  }
  return FALSE;
}

static gboolean
toggle_rate (MyDataStruct * data)
{
  gint64 pos = GST_CLOCK_TIME_NONE;
  gst_element_query_position (data->pipeline, GST_FORMAT_TIME, &pos);

  /* Toggle rate between the 2 target rates */
  if (data->rate != TARGET_RATE_2)
    data->rate = TARGET_RATE_2;
  else
    data->rate = TARGET_RATE_1;
  g_print ("Switching rate to %f (position %" GST_TIME_FORMAT ")\n", data->rate,
      GST_TIME_ARGS (pos));
  gst_element_send_event (data->pipeline, gst_event_new_seek (data->rate,
          GST_FORMAT_TIME, GST_SEEK_FLAG_INSTANT_RATE_CHANGE,
          GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE, GST_SEEK_TYPE_NONE,
          GST_CLOCK_TIME_NONE));
  return TRUE;
}

static void
on_preroll (MyDataStruct * data)
{
  if (data->timeout_id != 0)
    return;                     /* Already scheduled out scenario timer */

  switch (data->scenario) {
    case 0:
      data->idle_id = g_idle_add ((GSourceFunc) toggle_rate, data);
      data->timeout_id = g_timeout_add_seconds (IDLE_CYCLE_DELAY,
          (GSourceFunc) do_enable_disable_idle, data);
      break;
    case 1:
    case 2:
    case 3:{
      gint64 pos = GST_CLOCK_TIME_NONE;
      gst_element_query_position (data->pipeline, GST_FORMAT_TIME, &pos);

      /* Change rate to 2x and play for 10 sec, pause for 10 sec */
      data->rate = 2.0;
      g_print ("Switching rate to %f (position %" GST_TIME_FORMAT ")\n",
          data->rate, GST_TIME_ARGS (pos));
      gst_element_send_event (data->pipeline, gst_event_new_seek (data->rate,
              GST_FORMAT_TIME, GST_SEEK_FLAG_INSTANT_RATE_CHANGE,
              GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE, GST_SEEK_TYPE_NONE,
              GST_CLOCK_TIME_NONE));
      /* Instant rate change completed, schedule play/pause */
      data->timeout_id = g_timeout_add_seconds (PLAY_PAUSE_DELAY,
          (GSourceFunc) do_play_pause, data);
      break;
    }
    case 4:
      g_timeout_add (250, (GSourceFunc) toggle_rate, data);
      break;
    default:
      break;
  }
}

static gboolean
_on_bus_message (GstBus * bus, GstMessage * message, gpointer userdata)
{
  MyDataStruct *data = (MyDataStruct *) (userdata);

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *name = gst_object_get_path_string (GST_MESSAGE_SRC (message));
      gst_message_parse_error (message, &err, NULL);

      g_printerr ("ERROR: from element %s: %s\n", name, err->message);
      g_error_free (err);
      g_free (name);

      g_printf ("Stopping\n");
      g_main_loop_quit (data->mainloop);
      break;
    }
    case GST_MESSAGE_EOS:
      g_printf ("EOS ! Stopping \n");
      g_main_loop_quit (data->mainloop);
      break;
    case GST_MESSAGE_ASYNC_DONE:
      on_preroll (data);
      break;
    default:
      break;
  }

  return TRUE;
}

static gchar *
cmdline_to_uri (const gchar * arg)
{
  if (gst_uri_is_valid (arg))
    return g_strdup (arg);

  return gst_filename_to_uri (arg, NULL);
}

static void
print_usage (const char *arg0)
{
  g_print
      ("Usage: %s <0-4> URI\nSelect test scenario 0 to 4, and supply a URI to test\n",
      arg0);
  g_print ("Scenarios:\n"
      " 0) Play rate to 1x -> Apply 'instant-rate-change' to 2x -> Apply 'instant-rate-change' to 0.25x (repeat as fast as possible for 2 sec) -> let play for 2s\n"
      " 1) Play rate to 1x -> Apply 'instant-rate-change' to 2x -> run for 10s, then pause -> wait 10s -> play\n"
      " 2) Play rate to 1x -> Apply 'instant-rate-change' to 2x -> run for 10s, then pause -> wait 10s -> Apply 'instant-rate-change' to 1x -> play\n"
      " 3) Play rate to 1x -> Apply 'instant-rate-change' to 2x -> run for 10s, then pause -> seeking (flush+key-unit) to 30s -> wait 10s -> play\n"
      " 4) Play rate to 1x -> Apply 'instant-rate-change' to 2x -> Apply 'instant-rate-change' to 0.25x (toggle every 500ms)\n");
}

int
main (int argc, gchar ** argv)
{
  GstBus *bus;
  MyDataStruct *data;
  gchar *uri;
#ifdef FORCE_SYSTEM_CLOCK
  GstClock *clock;
#endif

  gst_init (&argc, &argv);

  data = g_new0 (MyDataStruct, 1);

  if (argc < 3) {
    print_usage (argv[0]);
    return 1;
  }

  data->scenario = atoi (argv[1]);
  uri = cmdline_to_uri (argv[2]);
  if (data->scenario > 4 || uri == NULL) {
    print_usage (argv[0]);
    return 1;
  }

  data->pipeline = gst_element_factory_make ("playbin", NULL);
  if (data->pipeline == NULL) {
    g_printerr ("Failed to create playbin element. Aborting");
    return 1;
  }
#ifdef FORCE_SYSTEM_CLOCK
  clock = gst_system_clock_obtain ();
  gst_pipeline_use_clock (GST_PIPELINE (data->pipeline), clock);
#endif

#ifdef DISABLE_AUDIO
  g_object_set (data->pipeline, "flags", 0x00000615, NULL);
#endif
  g_object_set (data->pipeline, "uri", uri, NULL);
  g_free (uri);

  /* Put a bus handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (data->pipeline));
  gst_bus_add_watch (bus, _on_bus_message, data);

  /* Start pipeline */
  data->mainloop = g_main_loop_new (NULL, TRUE);
  gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
  g_main_loop_run (data->mainloop);

  gst_element_set_state (data->pipeline, GST_STATE_NULL);

  gst_object_unref (data->pipeline);
  gst_object_unref (bus);

  return 0;
}
