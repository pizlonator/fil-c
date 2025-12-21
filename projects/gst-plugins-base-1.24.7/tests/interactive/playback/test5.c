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

static GMainLoop *loop;

static void
pad_added_cb (GstElement * element, GstPad * pad, GstElement * sink)
{
  g_print ("New pad...\n");
}

static void
no_more_pads (GstElement * element)
{
  g_print ("No more pads...\n");
  g_main_loop_quit (loop);
}

static gboolean
start_finding (GstElement * pipeline)
{
  GstStateChangeReturn res;

  g_print ("finding caps...\n");
  res = gst_element_set_state (pipeline, GST_STATE_PAUSED);
  if (res == GST_STATE_CHANGE_FAILURE) {
    g_print ("could not pause\n");
    exit (-1);
  }
  return FALSE;
}

static void
dump_element_stats (GstElement * element)
{
  GstIterator *it;
  GValue data = { 0, };

  it = gst_element_iterate_src_pads (element);
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
}

gint
main (gint argc, gchar * argv[])
{
  GstElement *pipeline, *filesrc, *decodebin;

  gst_init (&argc, &argv);

  pipeline = gst_pipeline_new ("pipeline");

  filesrc = gst_element_factory_make ("filesrc", "filesrc");
  g_assert (filesrc);

  decodebin = gst_element_factory_make ("decodebin", "decodebin");
  g_assert (decodebin);

  g_signal_connect (G_OBJECT (decodebin), "pad-added",
      G_CALLBACK (pad_added_cb), NULL);
  g_signal_connect (G_OBJECT (decodebin), "no-more-pads",
      G_CALLBACK (no_more_pads), NULL);

  gst_bin_add_many (GST_BIN (pipeline), filesrc, decodebin, NULL);
  gst_element_link (filesrc, decodebin);

  if (argc < 2) {
    g_print ("usage: %s <uri>\n", argv[0]);
    exit (-1);
  }

  if (!g_str_has_prefix (argv[1], "file://")) {
    g_object_set (G_OBJECT (filesrc), "location", argv[1], NULL);
  } else {
    g_object_set (G_OBJECT (filesrc), "location", argv[1] + 7, NULL);
  }

  /* event based programming approach */
  loop = g_main_loop_new (NULL, TRUE);
  g_idle_add ((GSourceFunc) start_finding, pipeline);
  g_main_loop_run (loop);

  dump_element_stats (decodebin);

  return 0;
}
