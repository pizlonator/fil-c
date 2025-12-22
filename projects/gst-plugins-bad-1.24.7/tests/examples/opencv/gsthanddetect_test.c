/* GStreamer
 * Copyright (C) 2012 Andol Li <andol@andol.info>
 *
 *  gsthanddetect_test: gstreamer handdetect plugin demo application,
 *  part work of GSoc 2012 project
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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>

GstElement *playbin, *pipeline;
GstElement *v4l2src, *videoscale, *videoconvert_in, *handdetect,
    *videoconvert_out, *xvimagesink;

static GstBusSyncReply
bus_sync_handler (GstBus * bus, GstMessage * message, GstPipeline * pipeline)
{
  const GstStructure *structure;
  gint64 position, length;
  GstFormat format = GST_FORMAT_TIME;
  const GValue *x_value, *y_value;
  gint x, i, y;
  /* select msg */
  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT ||
      !gst_structure_has_name (gst_message_get_structure (message),
          "hand-gesture"))
    return GST_BUS_PASS;

  /* parse msg structure */
  structure = gst_message_get_structure (message);

  /* if PALM gesture detected */
  if (structure &&
      strcmp (gst_structure_get_name (structure), "hand-gesture") == 0 &&
      strcmp (gst_structure_get_string (structure, "gesture"), "palm") == 0) {
    /* media operation - closed palm to stop media play */
    gst_element_set_state (playbin, GST_STATE_PAUSED);
  }

  /* if FIST gesture detected */
  if (structure &&
      strcmp (gst_structure_get_name (structure), "hand-gesture") == 0 &&
      strcmp (gst_structure_get_string (structure, "gesture"), "fist") == 0) {
    /* print message type and structure name */
    g_print ("%s{{%s}}\n", gst_message_type_get_name (message->type),
        gst_structure_get_name (structure));
    /* print msg structure names&values */
    for (i = 0; i < gst_structure_n_fields (structure); i++) {
      const gchar *name = gst_structure_nth_field_name (structure, i);
      GType type = gst_structure_get_field_type (structure, name);
      const GValue *value = gst_structure_get_value (structure, name);
      type == G_TYPE_STRING ?
          g_print ("-%s[%s]{%s}\n", name, g_type_name (type),
          g_value_get_string (value)) : g_print ("-%s[%s]{%d}\n", name,
          g_type_name (type), g_value_get_uint (value));
    }
    g_print ("\n");

    /* get X,Y positions in frame */
    x_value = gst_structure_get_value (structure, "x");
    x = g_value_get_uint (x_value);
    y_value = gst_structure_get_value (structure, "y");
    y = g_value_get_uint (y_value);

    /* set object volumes [0-10] based on Y */
    g_object_set (G_OBJECT (playbin), "volume", (gdouble) (10 - y / 24), NULL);

    /* seek playback positions */
    gst_element_query_duration (playbin, format, &length);
    /* Width = 320 is specified in caps */
    position = (gint64) length *x / 320;
    gst_element_set_state (playbin, GST_STATE_PAUSED);
    gst_element_seek (GST_ELEMENT (playbin),
        1.0,
        format,
        GST_SEEK_FLAG_FLUSH,
        GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
    gst_element_set_state (GST_ELEMENT (playbin), GST_STATE_PLAYING);
  }

  gst_message_unref (message);
  return GST_BUS_DROP;
}

int
main (gint argc, gchar ** argv)
{
  static GMainLoop *loop;
  GstCaps *caps;
  GstBus *bus;
  gchar *uri;

  if (argc < 2) {
    fprintf (stderr, "oops, please give a file to play\n");
    return -1;
  }

  uri = g_filename_to_uri (argv[1], NULL, NULL);
  if (!uri) {
    fprintf (stderr, "failed to create the uri\n");
    return -1;
  }

  /* init gst */
  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);
  /* init elements */
  playbin = gst_element_factory_make ("playbin", "app_playbin");
  pipeline = gst_pipeline_new ("app_pipeline");
  v4l2src = gst_element_factory_make ("v4l2src", "app_v4l2src");
  videoscale = gst_element_factory_make ("videoscale", "app_videoscale");
  videoconvert_in =
      gst_element_factory_make ("videoconvert", "app_videoconvert_in");
  handdetect = gst_element_factory_make ("handdetect", "app_handdetect");
  videoconvert_out =
      gst_element_factory_make ("videoconvert", "app_videoconvert_out");
  xvimagesink = gst_element_factory_make ("xvimagesink", "app_xvimagesink");

  /* check init results */
  if (!playbin || !pipeline || !v4l2src || !videoscale || !videoconvert_in
      || !handdetect || !videoconvert_out || !xvimagesink)
    g_error ("ERROR: element init failed.\n");

  /* set values */
  g_object_set (G_OBJECT (playbin), "uri", uri, NULL);

  /* set caps */
  caps =
      gst_caps_from_string
      ("video/x-raw, format=(string)RGB, width=320, height=240, framerate=(fraction)30/1");

  /* set bus */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_set_sync_handler (bus, (GstBusSyncHandler) bus_sync_handler, pipeline,
      NULL);
  gst_object_unref (bus);

  /* add elements to pipeline */
  gst_bin_add_many (GST_BIN (pipeline),
      v4l2src,
      videoscale,
      videoconvert_in, handdetect, videoconvert_out, xvimagesink, NULL);

  /* negotiate caps */
  if (!gst_element_link_filtered (v4l2src, videoscale, caps)) {
    g_printerr ("ERROR:v4l2src -> videoscale caps\n");
    return 0;
  }
  gst_caps_unref (caps);

  /* link elements */
  gst_element_link_many (videoscale,
      videoconvert_in, handdetect, videoconvert_out, xvimagesink, NULL);

  /* change states */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* start main loop */
  g_main_loop_run (loop);

  /* clean all */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (playbin));

  return 0;
}
