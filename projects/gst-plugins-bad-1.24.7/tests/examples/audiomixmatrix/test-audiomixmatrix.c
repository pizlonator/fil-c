/*
 * GStreamer
 * Copyright (C) 2017 Vivia Nikolaidou <vivia@toolsonair.com>
 *
 * test-audiomixmatrix.c
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

static gboolean
message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *debug = NULL;

      gst_message_parse_error (message, &err, &debug);
      gst_printerrln ("Error message received: %s", err->message);
      gst_printerrln ("Debug info: %s", debug);
      g_error_free (err);
      g_free (debug);
    }
    case GST_MESSAGE_EOS:
      g_main_loop_quit (user_data);
      break;
    default:
      break;
  }
  return TRUE;
}

static GstPadProbeReturn
_event_received (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstCaps *caps;
  gchar *caps_str;
  GstEvent *event = GST_EVENT (info->data);

  if (GST_EVENT_TYPE (event) != GST_EVENT_CAPS)
    return GST_PAD_PROBE_OK;

  gst_event_parse_caps (event, &caps);
  if (!gst_caps_is_fixed (caps))
    return GST_PAD_PROBE_OK;

  caps_str = gst_caps_to_string (caps);

  g_print ("Caps received on %s: %s\n",
      GST_PAD_IS_SRC (pad) ? "source" : "sink", caps_str);

  g_free (caps_str);

  return GST_PAD_PROBE_OK;
}

int
main (int argc, char **argv)
{
  GstElement *audiotestsrc, *capsfilter, *audiomixmatrix, *audioconvert, *sink;
  GstPad *srcpad, *sinkpad;
  GstBus *bus;
  GMainLoop *loop;
  GstCaps *caps;
  GValue v2 = G_VALUE_INIT;
  GValue v3 = G_VALUE_INIT;
  GstElement *pipeline;
  GValue v = G_VALUE_INIT;
  gchar *serialized_matrix;

  gst_init (&argc, &argv);

  audiotestsrc = gst_element_factory_make ("audiotestsrc", "audiotestsrc");
  capsfilter = gst_element_factory_make ("capsfilter", "capsfilter");
  caps =
      gst_caps_from_string
      ("audio/x-raw,channels=4,channel-mask=(bitmask)0,format=S32LE");
  g_object_set (capsfilter, "caps", caps, NULL);
  gst_caps_unref (caps);
  audiomixmatrix =
      gst_element_factory_make ("audiomixmatrix", "audiomixmatrix");
  g_object_set (audiomixmatrix, "in-channels", 4, NULL);
  g_object_set (audiomixmatrix, "out-channels", 2, NULL);
  g_object_set (audiomixmatrix, "channel-mask", 3, NULL);
  /* So the serialized matrix will be: < < 1, 0, 0, 0 >, < 0, 1, 0, 0 > > */
  g_value_init (&v, GST_TYPE_ARRAY);
  g_value_init (&v2, GST_TYPE_ARRAY);
  g_value_init (&v3, G_TYPE_DOUBLE);
  g_value_set_double (&v3, 1);
  gst_value_array_append_value (&v2, &v3);
  g_value_unset (&v3);
  g_value_init (&v3, G_TYPE_DOUBLE);
  g_value_set_double (&v3, 0);
  gst_value_array_append_value (&v2, &v3);
  g_value_unset (&v3);
  g_value_init (&v3, G_TYPE_DOUBLE);
  g_value_set_double (&v3, 0);
  gst_value_array_append_value (&v2, &v3);
  g_value_unset (&v3);
  g_value_init (&v3, G_TYPE_DOUBLE);
  g_value_set_double (&v3, 0);
  gst_value_array_append_value (&v2, &v3);
  g_value_unset (&v3);
  gst_value_array_append_value (&v, &v2);
  g_value_unset (&v2);
  g_value_init (&v2, GST_TYPE_ARRAY);
  g_value_init (&v3, G_TYPE_DOUBLE);
  g_value_set_double (&v3, 0);
  gst_value_array_append_value (&v2, &v3);
  g_value_unset (&v3);
  g_value_init (&v3, G_TYPE_DOUBLE);
  g_value_set_double (&v3, 1);
  gst_value_array_append_value (&v2, &v3);
  g_value_unset (&v3);
  g_value_init (&v3, G_TYPE_DOUBLE);
  g_value_set_double (&v3, 0);
  gst_value_array_append_value (&v2, &v3);
  g_value_unset (&v3);
  g_value_init (&v3, G_TYPE_DOUBLE);
  g_value_set_double (&v3, 0);
  gst_value_array_append_value (&v2, &v3);
  g_value_unset (&v3);
  gst_value_array_append_value (&v, &v2);
  g_value_unset (&v2);
  g_object_set_property (G_OBJECT (audiomixmatrix), "matrix", &v);
  /* Alternatively: gst_util_set_object_arg (audiomixmatrix, "matrix", "< < 1, 0> ..."); */
  serialized_matrix = gst_value_serialize (&v);
  gst_printerrln ("Serialized matrix: %s", serialized_matrix);
  g_free (serialized_matrix);
  g_value_unset (&v);
  audioconvert = gst_element_factory_make ("audioconvert", "audioconvert");
  sink = gst_element_factory_make ("autoaudiosink", "sink");
  pipeline = gst_pipeline_new ("pipe");
  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, capsfilter,
      audiomixmatrix, audioconvert, sink, NULL);
  gst_element_link_many (audiotestsrc, capsfilter,
      audiomixmatrix, audioconvert, sink, NULL);

  srcpad = gst_element_get_static_pad (audiomixmatrix, "src");
  gst_pad_add_probe (srcpad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      _event_received, NULL, NULL);
  gst_object_unref (srcpad);
  sinkpad = gst_element_get_static_pad (audiomixmatrix, "sink");
  gst_pad_add_probe (sinkpad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      _event_received, NULL, NULL);
  gst_object_unref (sinkpad);

  if (gst_element_set_state (pipeline, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    g_printerr ("ERROR: Could not change state in pipeline!\n");
    gst_object_unref (pipeline);
    return 1;
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  loop = g_main_loop_new (NULL, FALSE);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (G_OBJECT (bus), "message", G_CALLBACK (message_cb), loop);
  g_main_loop_run (loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (bus);
  gst_object_unref (pipeline);


  return 0;

}
