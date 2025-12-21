/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
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
#include <stdlib.h>             /* exit() */
#include <gst/gst.h>

#define UPDATE_INTERVAL 500

static int arg_count;
static int max_count;

static gboolean
update_scale (GstElement * element)
{
  gint64 duration = -1;
  gint64 position = -1;
  gchar dur_str[32], pos_str[32];

  if (gst_element_query_position (element, GST_FORMAT_TIME, &position) &&
      position != -1) {
    g_snprintf (pos_str, 32, "%" GST_TIME_FORMAT, GST_TIME_ARGS (position));
  } else {
    g_snprintf (pos_str, 32, "-:--:--.---------");
  }

  if (gst_element_query_duration (element, GST_FORMAT_TIME, &duration) &&
      duration != -1) {
    g_snprintf (dur_str, 32, "%" GST_TIME_FORMAT, GST_TIME_ARGS (duration));
  } else {
    g_snprintf (dur_str, 32, "-:--:--.---------");
  }

  g_print ("%s / %s\n", pos_str, dur_str);

  return TRUE;
}

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
error_cb (GstBus * bus, GstMessage * msg, GMainLoop * main_loop)
{
  GError *err = NULL;
  gchar *dbg = NULL;

  gst_message_parse_error (msg, &err, &dbg);

  g_printerr ("ERROR: %s (%s)\n", err->message, (dbg) ? dbg : "no details");

  g_main_loop_quit (main_loop);

  g_error_free (err);
  g_free (dbg);
}

static void
eos_cb (GstBus * bus, GstMessage * msg, GMainLoop * main_loop)
{
  g_print ("EOS\n");
  g_main_loop_quit (main_loop);
}

static void
new_clock_cb (GstBus * bus, GstMessage * msg, gpointer nothing)
{
  GstClock *clock;

  gst_message_parse_new_clock (msg, &clock);
  g_print ("NEW CLOCK: %s\n", GST_OBJECT_NAME (clock));
}

static void
clock_lost_cb (GstBus * bus, GstMessage * msg, GstElement * playbin)
{
  GstClock *clock;

  gst_message_parse_clock_lost (msg, &clock);
  g_print ("CLOCK LOST: %s\n", GST_OBJECT_NAME (clock));

  gst_element_set_state (playbin, GST_STATE_PAUSED);
  gst_element_set_state (playbin, GST_STATE_PLAYING);
}

static void
about_to_finish_cb (GstElement * element, gchar * uri[])
{
  if (arg_count < max_count) {
    g_object_set (G_OBJECT (element), "uri", uri[arg_count], NULL);
    arg_count++;
  }
}

gint
main (gint argc, gchar * argv[])
{
  GstStateChangeReturn res;
  GstElement *player;
  GMainLoop *loop;
  GstBus *bus;

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, TRUE);

  if (argc < 2) {
    g_print ("usage: %s <uri> [<uri> ... ]\n", argv[0]);
    exit (-1);
  }

  player = gst_element_factory_make ("playbin", "player");
  g_assert (player);

  bus = gst_pipeline_get_bus (GST_PIPELINE (player));
  gst_bus_add_signal_watch (bus);

  g_signal_connect (bus, "message::eos", G_CALLBACK (eos_cb), loop);
  g_signal_connect (bus, "message::error", G_CALLBACK (error_cb), loop);
  g_signal_connect (bus, "message::warning", G_CALLBACK (warning_cb), NULL);
  g_signal_connect (bus, "message::new-clock", G_CALLBACK (new_clock_cb), NULL);
  g_signal_connect (bus, "message::clock-lost", G_CALLBACK (clock_lost_cb),
      player);

  g_object_set (G_OBJECT (player), "uri", argv[1], NULL);

  arg_count = 2;
  max_count = argc;
  g_signal_connect (player, "about-to-finish", G_CALLBACK (about_to_finish_cb),
      argv);

  res = gst_element_set_state (player, GST_STATE_PLAYING);
  if (res == GST_STATE_CHANGE_FAILURE) {
    g_print ("could not play\n");
    return -1;
  }

  g_timeout_add (UPDATE_INTERVAL, (GSourceFunc) update_scale, player);

  g_main_loop_run (loop);

  /* tidy up */
  gst_element_set_state (player, GST_STATE_NULL);
  gst_object_unref (player);
  gst_object_unref (bus);

  return 0;
}
