/*
 * GStreamer
 * Copyright (C) 2017 Thibault Saunier <thibault.saunier@osg-samsung.com>
 * Copyright (C) 2020 Jan Schmidt <jan@centricular.com>
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
 * Simple example using the compositor element.
 *
 * Takes two video files and display them side-by-side as a mosaic
 */

#include <stdlib.h>
#include <gst/gst.h>
#include <gst/video/video.h>

typedef struct
{
  GstElement *compositor;
  gint x, y, w, h;
  gint zorder;
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
  GstStructure *converter_config;
  GstPad *sinkpad =
      gst_element_request_pad_simple (GST_ELEMENT (info->compositor),
      "sink_%u");

  converter_config = gst_structure_new ("GstVideoConverter",
      GST_VIDEO_CONVERTER_OPT_THREADS, G_TYPE_UINT, 0,
      GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD, GST_TYPE_VIDEO_RESAMPLER_METHOD,
      GST_VIDEO_RESAMPLER_METHOD_NEAREST, GST_VIDEO_CONVERTER_OPT_DEST_X,
      G_TYPE_INT, 0, GST_VIDEO_CONVERTER_OPT_DEST_Y, G_TYPE_INT, 0,
      GST_VIDEO_CONVERTER_OPT_DEST_WIDTH, G_TYPE_INT, info->w,
      GST_VIDEO_CONVERTER_OPT_DEST_HEIGHT, G_TYPE_INT, info->h, NULL);

  g_object_set (sinkpad, "xpos", info->x, "ypos", info->y, "width", info->w,
      "height", info->h, "converter-config", converter_config, NULL);

  gst_structure_free (converter_config);

  gst_pad_link (pad, sinkpad);

  g_free (info);
}

int
main (int argc, char *argv[])
{
  gint i;
  GstMessage *message;
  GstElement *compositor, *pipeline;
  GstBus *bus;

  if (argc != 3) {
    g_error ("Need to provide 2 input videos");
    return -1;
  }

  gst_init (&argc, &argv);
  pipeline =
      gst_parse_launch
      ("videotestsrc pattern=black is-live=true !  video/x-raw,width=1,height=1,format=AYUV ! compositor name=comp start-time-selection=first ! video/x-raw,format=AYUV,width=1275,height=833,framerate=25/1 ! videoconvert ! autovideosink",
      NULL);

  g_assert (pipeline != NULL);
  compositor = gst_bin_get_by_name (GST_BIN (pipeline), "comp");

  gst_util_set_object_arg (G_OBJECT (compositor), "background", "black");

  for (i = 1; i < 3; i++) {
    gchar *uri = ensure_uri (argv[i]);
    VideoInfo *info = g_malloc0 (sizeof (VideoInfo));
    GstElement *uridecodebin = gst_element_factory_make ("uridecodebin", NULL);

    g_object_set (uridecodebin, "uri", uri, "expose-all-streams", FALSE,
        "caps", gst_caps_from_string ("video/x-raw(ANY)"), NULL);

    info->compositor = compositor;
    if (i == 1) {
      info->x = 326;
      info->y = 155;
      info->w = 930;
      info->h = 523;
      info->zorder = 2;
    } else {
      info->x = 19;
      info->y = 155;
      info->w = 288;
      info->h = 162;
      info->zorder = 3;
    }
    g_signal_connect (uridecodebin, "pad-added", (GCallback) _pad_added_cb,
        info);

    gst_bin_add (GST_BIN (pipeline), uridecodebin);
  }

  bus = gst_element_get_bus (pipeline);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  message =
      gst_bus_timed_pop_filtered (bus, 60 * GST_SECOND,
      GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL | GST_DEBUG_GRAPH_SHOW_VERBOSE, "go");
  if (message)
    gst_print ("%" GST_PTR_FORMAT "\n", message);
  else
    gst_print ("Timeout\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  return 0;
}
