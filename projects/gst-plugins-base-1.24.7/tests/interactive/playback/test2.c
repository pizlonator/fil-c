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

static GMainLoop *loop;

static void
error_eos_cb (GstBus * bus, GstMessage * msg, GMainLoop * main_loop)
{
  g_main_loop_quit (main_loop);
}

gint
main (gint argc, gchar * argv[])
{
  GstElement *player;
  GstStateChangeReturn res;
  GstBus *bus;

  gst_init (&argc, &argv);

  player = gst_element_factory_make ("playbin", "player");
  g_assert (player);

  g_object_set (G_OBJECT (player), "uri", argv[1], NULL);

  loop = g_main_loop_new (NULL, TRUE);
  bus = gst_pipeline_get_bus (GST_PIPELINE (player));
  gst_bus_add_signal_watch (bus);

  g_signal_connect (bus, "message::eos", G_CALLBACK (error_eos_cb), loop);
  g_signal_connect (bus, "message::error", G_CALLBACK (error_eos_cb), loop);

  res = gst_element_set_state (player, GST_STATE_PLAYING);
  if (res == GST_STATE_CHANGE_FAILURE) {
    g_print ("could not play\n");
    return -1;
  }

  g_main_loop_run (loop);

  return 0;
}
