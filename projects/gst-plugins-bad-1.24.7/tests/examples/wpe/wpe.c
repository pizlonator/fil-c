/* Copyright (C) <2018, 2019> Philippe Normand <philn@igalia.com>
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
static GstElement *pipe1;
static GstBus *bus1;

static gboolean
_bus_watch (GstBus * bus, GstMessage * msg, GstElement * pipe)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_ELEMENT (msg->src) == pipe) {
        GstState old, new, pending;

        gst_message_parse_state_changed (msg, &old, &new, &pending);

        {
          gchar *dump_name = g_strconcat ("state_changed-",
              gst_element_state_get_name (old), "_",
              gst_element_state_get_name (new), NULL);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (msg->src),
              GST_DEBUG_GRAPH_SHOW_ALL, dump_name);
          g_free (dump_name);
        }
      }
      break;
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *dbg_info = NULL;

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "error");

      gst_message_parse_error (msg, &err, &dbg_info);
      g_printerr ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (msg->src), err->message);
      g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
      g_error_free (err);
      g_free (dbg_info);
      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_EOS:{
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "eos");
      g_print ("EOS received\n");
      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static void
_wpe_pad_added (GstElement * src, GstPad * new_pad, GstElement * pipe)
{
  GstElement *out;
  GstPad *sink;
  gchar *name = gst_pad_get_name (new_pad);
  gchar *bin_name;

  out = gst_parse_bin_from_description
      ("audioresample ! audioconvert ! autoaudiosink", TRUE, NULL);
  bin_name = g_strdup_printf ("%s-bin", name);
  g_free (name);

  gst_object_set_name (GST_OBJECT_CAST (out), bin_name);
  g_free (bin_name);

  gst_bin_add (GST_BIN (pipe), out);
  sink = out->sinkpads->data;
  gst_pad_link (new_pad, sink);
  gst_element_sync_state_with_parent (out);
}

static void
_wpe_pad_removed (GstElement * src, GstPad * pad, GstElement * pipe)
{
  gchar *name = gst_pad_get_name (pad);
  gchar *bin_name = g_strdup_printf ("%s-bin", name);
  GstElement *bin = gst_bin_get_by_name (GST_BIN_CAST (pipe), bin_name);

  if (GST_IS_ELEMENT (bin)) {
    gst_bin_remove (GST_BIN_CAST (pipe), bin);
    gst_element_set_state (bin, GST_STATE_NULL);
  }
  g_free (name);
  g_free (bin_name);
}

int
main (int argc, char *argv[])
{
  GstElement *src;

  if (argc < 2) {
    g_printerr ("Usage: %s <website url>\n", argv[0]);
    return 1;
  }

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);
  pipe1 =
      gst_parse_launch
      ("wpesrc name=wpesrc ! queue ! glcolorconvert ! gtkglsink enable-last-sample=0",
      NULL);
  bus1 = gst_pipeline_get_bus (GST_PIPELINE (pipe1));
  gst_bus_add_watch (bus1, (GstBusFunc) _bus_watch, pipe1);

  src = gst_bin_get_by_name (GST_BIN (pipe1), "wpesrc");

  gst_element_set_state (GST_ELEMENT (pipe1), GST_STATE_READY);

  g_signal_connect (src, "pad-added", G_CALLBACK (_wpe_pad_added), pipe1);
  g_signal_connect (src, "pad-removed", G_CALLBACK (_wpe_pad_removed), pipe1);

  g_object_set (src, "location", argv[1], NULL);
  gst_clear_object (&src);

  g_print ("Starting pipeline\n");
  gst_element_set_state (GST_ELEMENT (pipe1), GST_STATE_PLAYING);

  g_main_loop_run (loop);

  gst_element_set_state (GST_ELEMENT (pipe1), GST_STATE_NULL);
  g_print ("Pipeline stopped\n");

  gst_bus_remove_watch (bus1);
  gst_object_unref (bus1);
  gst_object_unref (pipe1);

  gst_deinit ();

  return 0;
}
