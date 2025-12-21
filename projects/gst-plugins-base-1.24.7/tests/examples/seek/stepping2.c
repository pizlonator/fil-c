/* GStreamer
 *
 * stepping.c: stepping sample application
 *
 * Copyright (C) 2009 Wim Taymans <wim.taymans@gmail.com>
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

#include <stdlib.h>
#include <math.h>

#include <gst/gst.h>

static GMainLoop *loop;

static gdouble period = 0.0;

static void
do_step (GstElement * bin)
{
  gdouble rate;

  rate = sin (period);

  period += G_PI / 150;

  rate += 1.2;

  gst_element_send_event (bin,
      gst_event_new_step (GST_FORMAT_TIME, 40 * GST_MSECOND, rate, FALSE,
          FALSE));
}

static void
handle_sync_message (GstBus * bus, GstMessage * message, gpointer data)
{
  GstElement *bin;

  bin = GST_ELEMENT_CAST (data);

  switch (message->type) {
    case GST_MESSAGE_STEP_DONE:
      do_step (bin);
      break;
    default:
      break;
  }
}

static void
handle_message (GstBus * bus, GstMessage * message, gpointer data)
{
  switch (message->type) {
    case GST_MESSAGE_EOS:
      g_message ("got EOS");
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_WARNING:
    case GST_MESSAGE_ERROR:
    {
      GError *gerror;
      gchar *debug;

      if (message->type == GST_MESSAGE_ERROR)
        gst_message_parse_error (message, &gerror, &debug);
      else
        gst_message_parse_warning (message, &gerror, &debug);

      gst_object_default_error (GST_MESSAGE_SRC (message), gerror, debug);
      g_error_free (gerror);
      g_free (debug);
      if (message->type == GST_MESSAGE_ERROR)
        g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }
}

int
main (int argc, char *argv[])
{
  GstElement *bin;
  GstBus *bus;

  gst_init (&argc, &argv);

  if (argc < 2) {
    g_print ("usage: %s <uri>\n", argv[0]);
    return -1;
  }

  /* create a new bin to hold the elements */
  bin = gst_element_factory_make ("playbin", "bin");
  g_assert (bin);
  g_object_set (bin, "uri", argv[1], NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (bin));
  gst_bus_add_signal_watch (bus);
  gst_bus_enable_sync_message_emission (bus);

  g_signal_connect (bus, "message", (GCallback) handle_message, bin);
  g_signal_connect (bus, "sync-message", (GCallback) handle_sync_message, bin);

  /* go to the PAUSED state and wait for preroll */
  g_message ("prerolling first frame");
  gst_element_set_state (bin, GST_STATE_PAUSED);
  gst_element_get_state (bin, NULL, NULL, -1);

  /* queue step */
  do_step (bin);

  gst_element_set_state (bin, GST_STATE_PLAYING);

  loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (loop);

  g_message ("finished");

  /* stop the bin */
  gst_element_set_state (bin, GST_STATE_NULL);

  g_main_loop_unref (loop);
  gst_object_unref (bus);

  exit (0);
}
