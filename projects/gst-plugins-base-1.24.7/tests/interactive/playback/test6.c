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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>             /* exit */

#include <gst/gst.h>

static void
pad_added_cb (GstElement * decodebin, GstPad * new_pad, GstElement * pipeline)
{
  GstElement *fakesink;
  GstPad *sinkpad;

  fakesink = gst_element_factory_make ("fakesink", NULL);
  gst_bin_add (GST_BIN (pipeline), fakesink);

  sinkpad = gst_element_get_static_pad (fakesink, "sink");
  if (GST_PAD_LINK_FAILED (gst_pad_link (new_pad, sinkpad))) {
    g_warning ("Failed to link %s:%s to %s:%s", GST_DEBUG_PAD_NAME (new_pad),
        GST_DEBUG_PAD_NAME (sinkpad));
    gst_bin_remove (GST_BIN (pipeline), fakesink);
  } else {
    gst_element_set_state (fakesink, GST_STATE_PAUSED);
  }
}

static void
show_error (const gchar * errmsg, GstBus * bus)
{
  GstMessage *msg;
  GError *err = NULL;
  gchar *dbg = NULL;

  msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, 0);
  if (msg) {
    g_assert (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR);

    gst_message_parse_error (msg, &err, &dbg);
  }

  g_print ("ERROR: %s\n", errmsg);
  g_print ("       %s\n", (err) ? err->message : "");
  if (dbg) {
    g_print ("\ndebug: %s\n\n", dbg);
    g_free (dbg);
  }

  if (err)
    g_error_free (err);
}

gint
main (gint argc, gchar * argv[])
{
  GstElement *pipeline, *filesrc, *decodebin;
  GstStateChangeReturn res;
  GstIterator *it;
  GstBus *bus;
  GValue data = { 0, };

  gst_init (&argc, &argv);

  pipeline = gst_pipeline_new ("pipeline");

  filesrc = gst_element_factory_make ("filesrc", "filesrc");
  g_assert (filesrc);

  decodebin = gst_element_factory_make ("decodebin", "decodebin");
  g_assert (decodebin);

  gst_bin_add_many (GST_BIN (pipeline), filesrc, decodebin, NULL);
  gst_element_link (filesrc, decodebin);

  if (argc < 2) {
    g_print ("usage: %s <filenames>\n", argv[0]);
    exit (-1);
  }

  if (!g_str_has_prefix (argv[1], "file://")) {
    g_object_set (G_OBJECT (filesrc), "location", argv[1], NULL);
  } else {
    g_object_set (G_OBJECT (filesrc), "location", argv[1] + 7, NULL);
  }

  /* we've got to connect fakesinks to newly decoded pads to make sure
   * buffers have actually been flowing over those pads and caps have
   * been set on them. decodebin might insert internal queues and
   * without fakesinks it's pot-luck what caps we get from the pad, because
   * it depends on whether the queues have started pushing buffers yet or not.
   * With fakesinks we make sure that the pipeline doesn't go to PAUSED state
   * before each fakesink has a buffer queued. */
  g_signal_connect (decodebin, "pad-added",
      G_CALLBACK (pad_added_cb), pipeline);

  bus = gst_element_get_bus (pipeline);

  g_print ("pause..\n");
  res = gst_element_set_state (pipeline, GST_STATE_PAUSED);
  if (res == GST_STATE_CHANGE_FAILURE) {
    show_error ("Could not go to PAUSED state", bus);
    exit (-1);
  }
  g_print ("waiting..\n");
  res = gst_element_get_state (pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
  if (res != GST_STATE_CHANGE_SUCCESS) {
    show_error ("Failed to complete state change to PAUSED", bus);
    exit (-1);
  }
  g_print ("stats..\n");

  it = gst_element_iterate_src_pads (decodebin);
  while (gst_iterator_next (it, &data) == GST_ITERATOR_OK) {
    GstPad *pad = g_value_get_object (&data);
    GstCaps *caps;
    gchar *str;
    GstQuery *query;

    g_print ("stream %s:\n", GST_OBJECT_NAME (pad));

    caps = gst_pad_query_caps (pad, NULL);
    str = gst_caps_to_string (caps);
    g_print (" caps: %s\n", str);
    g_free (str);
    gst_caps_unref (caps);

    query = gst_query_new_duration (GST_FORMAT_TIME);
    if (gst_pad_query (pad, query)) {
      gint64 duration;

      gst_query_parse_duration (query, NULL, &duration);

      g_print (" duration: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS (duration));
    }
    gst_query_unref (query);

    g_value_reset (&data);
  }
  g_value_unset (&data);
  gst_iterator_free (it);

  return 0;
}
