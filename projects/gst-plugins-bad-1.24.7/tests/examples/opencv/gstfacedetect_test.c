/* GStreamer
 * Copyright (C) 2015 Vanessa Chipirrás <vchipirras6@gmail.com>
 *
 *  gstfacedetect_test: gstreamer facedetect plugin demo application,
 *  part work of Outreachy 2015 project
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
GstElement *v4l2src, *videoscale, *videoconvert_in, *facedetect,
    *videoconvert_out, *autovideosink;
static gboolean ctrlvol = FALSE;
static gboolean silent = FALSE;

static GstBusSyncReply
bus_sync_handler (GstBus * bus, GstMessage * message, GstPipeline * pipeline)
{
  const GstStructure *structure;
  const GValue *value;
  gchar *contents;
  gint i;
  guint size = 0;

  /* select msg */
  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT ||
      !gst_structure_has_name (gst_message_get_structure (message),
          "facedetect"))
    return GST_BUS_PASS;

  /* parse msg structure */
  structure = gst_message_get_structure (message);

  /* if facedetect is into buffer */
  if (structure &&
      strcmp (gst_structure_get_name (structure), "facedetect") == 0) {
    if (!silent) {
      /* print message type and structure name */
      g_print ("Type message, name message: %s{{%s}}\n",
          gst_message_type_get_name (message->type),
          gst_structure_get_name (structure));

      /* print msg structure names and type */
      for (i = 0; i < gst_structure_n_fields (structure); i++) {
        const gchar *name = gst_structure_nth_field_name (structure, i);
        GType type = gst_structure_get_field_type (structure, name);
        g_print ("-Name field, type: %s[%s]\n", name, g_type_name (type));
      }
    }

    /* get structure of faces */
    value = gst_structure_get_value (structure, "faces");
    /* obtain the contents into the structure */
    contents = g_strdup_value_contents (value);
    if (!silent)
      g_print ("Detected objects: %s\n\n", *(&contents));

    /* list size */
    size = gst_value_list_get_size (value);

    /* if face is detected, obtain the values X and Y of mouth and of nose. */
    if (size != 0) {
      GstState state;

      /* if paused, set to playing */
      gst_element_get_state (GST_ELEMENT (playbin), &state, NULL,
          GST_CLOCK_TIME_NONE);
      if (state != GST_STATE_PLAYING) {
        gst_element_set_state (GST_ELEMENT (playbin), GST_STATE_PLAYING);
      }

      if (ctrlvol) {
        gdouble volume;

        const GValue *faces_value = gst_value_list_get_value (value, 0);
        const GstStructure *faces_structure =
            gst_value_get_structure (faces_value);
        gboolean have_mouth_y =
            gst_structure_has_field (faces_structure, "mouth->y");
        gboolean have_mouth_x =
            gst_structure_has_field (faces_structure, "mouth->x");
        gboolean have_nose_y =
            gst_structure_has_field (faces_structure, "nose->y");
        gboolean have_nose_x =
            gst_structure_has_field (faces_structure, "nose->x");

        /* get the volume value */
        g_object_get (G_OBJECT (playbin), "volume", &volume, NULL);

        /* media operation - hide your mouth for down the volume of the video */
        if (have_mouth_y == 0 && have_mouth_x == 0) {
          volume = volume - 0.5;
          if (volume <= 0.5)
            volume = 0.0;
          g_object_set (G_OBJECT (playbin), "volume", volume, NULL);
        }
        /* media operation - hide your nose for up the volume of the video */
        if (have_nose_y == 0 && have_nose_x == 0) {
          volume = volume + 0.5;
          if (volume >= 9.5)
            volume = 10.0;
          g_object_set (G_OBJECT (playbin), "volume", volume, NULL);
        }
      }
      /* if face is not detected */
    } else {
      /* media operation - hide your face to stop media play */
      gst_element_set_state (playbin, GST_STATE_PAUSED);
    }
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

  GOptionEntry options[] = {
    {"control-volume", 'c', 0, G_OPTION_ARG_NONE, &ctrlvol,
        "Control the volume by hiding the nose or mouth", NULL},
    {"silent", 's', 0, G_OPTION_ARG_NONE, &silent,
        "Don't output the messages and detected faces structure", NULL},
    {NULL}
  };
  GOptionContext *ctx;
  GError *err = NULL;

  ctx = g_option_context_new ("<video file>\n\nfacedetect test application.");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", err->message);
    g_option_context_free (ctx);
    g_clear_error (&err);
    exit (1);
  }
  g_option_context_free (ctx);

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
  facedetect = gst_element_factory_make ("facedetect", "app_facedetect");
  videoconvert_out =
      gst_element_factory_make ("videoconvert", "app_videoconvert_out");
  autovideosink =
      gst_element_factory_make ("autovideosink", "app_autovideosink");

  /* check init results */
  if (!playbin || !pipeline || !v4l2src || !videoscale || !videoconvert_in
      || !facedetect || !videoconvert_out || !autovideosink)
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
      videoconvert_in, facedetect, videoconvert_out, autovideosink, NULL);

  /* negotiate caps */
  if (!gst_element_link_filtered (v4l2src, videoscale, caps)) {
    g_printerr ("ERROR:v4l2src -> videoscale caps\n");
    return 0;
  }
  gst_caps_unref (caps);

  /* link elements */
  gst_element_link_many (videoscale,
      videoconvert_in, facedetect, videoconvert_out, autovideosink, NULL);

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
