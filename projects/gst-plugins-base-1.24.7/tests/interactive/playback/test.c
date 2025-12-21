/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
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
#include <gst/gst.h>

#include <stdlib.h>

static GMainLoop *loop;

static GstElement *
gen_video_element (void)
{
  GstElement *element;
  GstElement *conv;
  GstElement *sink;
  GstPad *pad;

  element = gst_bin_new ("vbin");
  conv = gst_element_factory_make ("videoconvert", "conv");
  sink = gst_element_factory_make (DEFAULT_VIDEOSINK, "sink");
  g_assert (sink);

  gst_bin_add (GST_BIN (element), conv);
  gst_bin_add (GST_BIN (element), sink);
  gst_element_link_pads (conv, "src", sink, "sink");

  pad = gst_element_get_static_pad (conv, "sink");
  gst_element_add_pad (element, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (pad);

  return element;
}

static GstElement *
gen_audio_element (void)
{
  GstElement *element;
  GstElement *conv;
  GstElement *sink;
  GstPad *pad;

  element = gst_bin_new ("abin");
  conv = gst_element_factory_make ("audioconvert", "conv");
  sink = gst_element_factory_make (DEFAULT_AUDIOSINK, "sink");
  g_assert (sink);

  gst_bin_add (GST_BIN (element), conv);
  gst_bin_add (GST_BIN (element), sink);
  gst_element_link_pads (conv, "src", sink, "sink");

  pad = gst_element_get_static_pad (conv, "sink");
  gst_element_add_pad (element, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (pad);

  return element;
}

static void
pad_added_cb (GstElement * decodebin, GstPad * pad, gpointer data)
{
  GstCaps *caps;
  GstStructure *str;
  GstPad *sinkpad;
  GstElement *sink;
  GstElement *pipeline;
  const gchar *name;
  GstStateChangeReturn ret;
  GstPadLinkReturn lret;

  /* check media type */
  caps = gst_pad_query_caps (pad, NULL);
  str = gst_caps_get_structure (caps, 0);

  name = gst_structure_get_name (str);
  g_print ("name: %s\n", name);

  if (g_strrstr (name, "audio")) {
    sink = gen_audio_element ();
  } else if (g_strrstr (name, "video")) {
    sink = gen_video_element ();
  } else {
    sink = NULL;
  }
  gst_caps_unref (caps);

  if (sink) {
    pipeline = GST_ELEMENT_CAST (data);

    /* add new sink to the pipeline */
    gst_bin_add (GST_BIN_CAST (pipeline), sink);

    /* set the new sink tp PAUSED as well */
    ret = gst_element_set_state (sink, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE)
      goto state_error;

    /* get the ghostpad of the sink bin */
    sinkpad = gst_element_get_static_pad (sink, "sink");

    /* link'n'play */
    lret = gst_pad_link (pad, sinkpad);
    if (lret != GST_PAD_LINK_OK)
      goto link_failed;

    gst_object_unref (sinkpad);
  }
  return;

  /* ERRORS */
state_error:
  {
    gst_bin_remove (GST_BIN_CAST (pipeline), sink);
    g_warning ("could not change state of new sink (%d)", ret);
    return;
  }
link_failed:
  {
    g_warning ("could not link pad and sink (%d)", lret);
    return;
  }
}

static void
error_eos_cb (GstBus * bus, GstMessage * msg, GMainLoop * main_loop)
{
  g_main_loop_quit (main_loop);
}

gint
main (gint argc, gchar * argv[])
{
  GstElement *pipeline, *filesrc, *decodebin;
  GstStateChangeReturn res;
  GstBus *bus;

  gst_init (&argc, &argv);

  pipeline = gst_pipeline_new ("pipeline");

  filesrc = gst_element_factory_make ("filesrc", "filesrc");
  g_assert (filesrc);
  decodebin = gst_element_factory_make ("decodebin", "decodebin");
  g_assert (decodebin);

  loop = g_main_loop_new (NULL, TRUE);
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);

  g_signal_connect (bus, "message::eos", G_CALLBACK (error_eos_cb), loop);
  g_signal_connect (bus, "message::error", G_CALLBACK (error_eos_cb), loop);

  g_signal_connect (G_OBJECT (decodebin), "pad-added",
      G_CALLBACK (pad_added_cb), pipeline);

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

  /* set to paused, decodebin will autoplug and signal new_pad callbacks */
  res = gst_element_set_state (pipeline, GST_STATE_PAUSED);
  if (res == GST_STATE_CHANGE_FAILURE) {
    g_print ("could not pause\n");
    return -1;
  }
  /* wait for paused to complete */
  res = gst_element_get_state (pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
  if (res == GST_STATE_CHANGE_FAILURE) {
    g_print ("could not pause\n");
    return -1;
  }

  /* play, now all the sinks are added to the pipeline and are prerolled and
   * ready to play. */
  res = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (res == GST_STATE_CHANGE_FAILURE) {
    g_print ("could not play\n");
    return -1;
  }

  /* go in the mainloop now */
  g_main_loop_run (loop);

  return 0;
}
