/*
 * GStreamer
 * Copyright (C) 2017 Thibault Saunier <thibault.saunier@osg-samsung.com>
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

/**
 * Simple crossfade example using the compositor element.
 *
 * Takes two video files and crossfades them for 10 seconds and returns.
 */

#include <stdlib.h>
#include <gst/gst.h>
#include <gst/controller/gstdirectcontrolbinding.h>
#include <gst/controller/gstinterpolationcontrolsource.h>

typedef struct
{
  GstElement *compositor;
  guint z_order;
} VideoInfo;

static gchar *
ensure_uri (const gchar * location)
{
  if (gst_uri_is_valid (location))
    return g_strdup (location);
  else
    return gst_filename_to_uri (location, NULL);
}

static void
_pad_added_cb (GstElement * decodebin, GstPad * pad, VideoInfo * info)
{
  GstPad *sinkpad =
      gst_element_request_pad_simple (GST_ELEMENT (info->compositor),
      "sink_%u");
  GstControlSource *control_source;
  gboolean is_last = info->z_order == 1;

  control_source = gst_interpolation_control_source_new ();

  gst_util_set_object_arg (G_OBJECT (sinkpad), "operator",
      info->z_order == 0 ? "source" : "add");
  gst_object_add_control_binding (GST_OBJECT (sinkpad),
      gst_direct_control_binding_new_absolute (GST_OBJECT (sinkpad), "alpha",
          control_source));

  g_object_set (control_source, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  gst_timed_value_control_source_set (GST_TIMED_VALUE_CONTROL_SOURCE
      (control_source), 0, is_last ? 0.0 : 1.0);
  gst_timed_value_control_source_set (GST_TIMED_VALUE_CONTROL_SOURCE
      (control_source), 10 * GST_SECOND, is_last ? 1.0 : 0.0);
  g_object_set (sinkpad, "zorder", info->z_order, NULL);

  gst_pad_link (pad, sinkpad);

  g_free (info);
}

int
main (int argc, char *argv[])
{
  gint i;
  GstMessage *message;
  GstElement *compositor, *sink, *pipeline;
  GstBus *bus;

  if (argc != 3) {
    g_error ("Need to provide 2 input videos");
    return -1;
  }

  gst_init (&argc, &argv);
  pipeline = gst_element_factory_make ("pipeline", NULL);
  compositor = gst_element_factory_make ("compositor", NULL);
  sink =
      gst_parse_bin_from_description ("videoconvert ! autovideosink", TRUE,
      NULL);

  gst_util_set_object_arg (G_OBJECT (compositor), "background", "black");

  gst_bin_add_many (GST_BIN (pipeline), compositor, sink, NULL);
  g_assert (gst_element_link (compositor, sink));

  for (i = 1; i < 3; i++) {
    gchar *uri = ensure_uri (argv[i]);
    VideoInfo *info = g_malloc0 (sizeof (VideoInfo));
    GstElement *uridecodebin = gst_element_factory_make ("uridecodebin", NULL);

    g_object_set (uridecodebin, "uri", uri, "expose-all-streams", FALSE,
        "caps", gst_caps_from_string ("video/x-raw(ANY)"), NULL);

    info->compositor = compositor;
    info->z_order = i - 1;
    g_signal_connect (uridecodebin, "pad-added", (GCallback) _pad_added_cb,
        info);

    gst_bin_add (GST_BIN (pipeline), uridecodebin);
  }

  bus = gst_element_get_bus (pipeline);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  message =
      gst_bus_timed_pop_filtered (bus, 11 * GST_SECOND,
      GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "go");
  if (message)
    gst_print ("%" GST_PTR_FORMAT "\n", message);
  else
    gst_print ("Timeout\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  return 0;
}
