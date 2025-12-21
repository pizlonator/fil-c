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
state_cb (GstBus * bus, GstMessage * msg, GstElement * pipeline)
{
  if (msg->src == GST_OBJECT (pipeline)) {
    GstState old_state, new_state, pending_state;

    gst_message_parse_state_changed (msg, &old_state, &new_state,
        &pending_state);
    if (new_state == GST_STATE_PLAYING) {
      g_print ("Decoding ...\n");
    }
  }
}

static void
pad_added_cb (GstElement * decodebin, GstPad * pad, GstElement * pipeline)
{
  GstPadLinkReturn ret;
  GstElement *fakesink;
  GstPad *fakesink_pad;

  fakesink = gst_element_factory_make ("fakesink", NULL);
  fakesink_pad = gst_element_get_static_pad (fakesink, "sink");

  gst_bin_add (GST_BIN (pipeline), fakesink);

  /* this doesn't really seem right, but it makes things work for me */
  gst_element_set_state (fakesink, GST_STATE_PLAYING);

  ret = gst_pad_link (pad, fakesink_pad);
  if (!GST_PAD_LINK_SUCCESSFUL (ret)) {
    g_printerr ("Failed to link %s:%s to %s:%s (ret = %d)\n",
        GST_DEBUG_PAD_NAME (pad), GST_DEBUG_PAD_NAME (fakesink_pad), ret);
  } else {
    g_printerr ("Linked %s:%s to %s:%s\n", GST_DEBUG_PAD_NAME (pad),
        GST_DEBUG_PAD_NAME (fakesink_pad));
  }

  gst_object_unref (fakesink_pad);
}

gint
main (gint argc, gchar * argv[])
{
  GstElement *decoder;
  GstElement *source;
  GstElement *pipeline;
  GstStateChangeReturn res;
  GMainLoop *loop;
  GstBus *bus;

  gst_init (&argc, &argv);

  if (argc != 2) {
    g_printerr ("Decode file from start to end.\n");
    g_printerr ("Usage: %s URI\n\n", argv[0]);
    return 1;
  }

  loop = g_main_loop_new (NULL, TRUE);

  pipeline = gst_pipeline_new ("pipeline");

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);

  g_signal_connect (bus, "message::eos", G_CALLBACK (eos_cb), loop);
  g_signal_connect (bus, "message::error", G_CALLBACK (error_cb), loop);
  g_signal_connect (bus, "message::warning", G_CALLBACK (warning_cb), NULL);
  g_signal_connect (bus, "message::state-changed", G_CALLBACK (state_cb),
      pipeline);

  source = gst_element_factory_make ("giosrc", "source");
  g_assert (source);

  if (argv[1] && strstr (argv[1], "://") != NULL) {
    g_object_set (G_OBJECT (source), "location", argv[1], NULL);
  } else if (argv[1]) {
    gchar *uri = g_strdup_printf ("file://%s", argv[1]);

    g_object_set (G_OBJECT (source), "location", uri, NULL);
    g_free (uri);
  }

  decoder = gst_element_factory_make ("decodebin", "decoder");
  g_assert (decoder);

  gst_bin_add (GST_BIN (pipeline), source);
  gst_bin_add (GST_BIN (pipeline), decoder);

  gst_element_link_pads (source, "src", decoder, "sink");

  g_signal_connect (decoder, "pad-added", G_CALLBACK (pad_added_cb), pipeline);

  res = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (res == GST_STATE_CHANGE_FAILURE) {
    g_print ("could not play\n");
    return -1;
  }

  g_main_loop_run (loop);

  /* tidy up */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  gst_object_unref (bus);

  return 0;
}
