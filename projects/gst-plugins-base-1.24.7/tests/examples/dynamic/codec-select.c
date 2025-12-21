/* GStreamer
 *
 * codec-select.c: sample application to dynamically select a codec
 *
 * Copyright (C) <2008> Wim Taymans <wim dot taymans at gmail dot com>
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

/*
 * This example sets up a pipeline to 'encode' an audiotestsrc into 3 different
 * formats. The format can be selected dynamically at runtime.
 *
 * Each of the encoders require the audio in a specific different format.
 *
 * This example uses identity as the encoder and enforces the caps on identity
 * with a capsfilter.
 *
 * This is a good example of input and output selector and how these elements
 * preserve segment and timing information while switching between streams.
 */

#include <string.h>
#include <gst/gst.h>

/* Create an encoder element.
 * We make a bin containing:
 *
 * audioresample ! <enccaps> ! identity
 *
 * The sinkpad of audioresample and source pad of identity are ghosted on the
 * bin.
 */
static GstElement *
make_encoder (const GstCaps * caps)
{
  GstElement *result;
  GstElement *audioresample;
  GstElement *capsfilter;
  GstElement *identity;
  GstPad *pad;

  /* create result bin */
  result = gst_bin_new (NULL);
  g_assert (result);

  /* create elements */
  audioresample = gst_element_factory_make ("audioresample", NULL);
  g_assert (audioresample);

  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  g_assert (capsfilter);
  g_object_set (capsfilter, "caps", caps, NULL);

  identity = gst_element_factory_make ("identity", NULL);
  g_assert (identity);
  g_object_set (identity, "silent", TRUE, NULL);

  /* add elements to result bin */
  gst_bin_add (GST_BIN (result), audioresample);
  gst_bin_add (GST_BIN (result), capsfilter);
  gst_bin_add (GST_BIN (result), identity);

  /* link elements */
  gst_element_link_pads (audioresample, "src", capsfilter, "sink");
  gst_element_link_pads (capsfilter, "src", identity, "sink");

  /* ghost src and sink pads */
  pad = gst_element_get_static_pad (audioresample, "sink");
  gst_element_add_pad (result, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (pad);

  pad = gst_element_get_static_pad (identity, "src");
  gst_element_add_pad (result, gst_ghost_pad_new ("src", pad));
  gst_object_unref (pad);

  return result;
}

/*
 * We generate:
 *
 * audiotestsrc ! <audiocaps> ! output-selector ! [enc1 .. enc3] ! input-selector
 * select-all = true ! fakesink
 *
 * <audiocaps> makes sure we only produce one format from the audiotestsrc.
 *
 * Each encX element consists of:
 *
 *  audioresample ! <enccaps> ! identity !
 *
 * This way we can simply switch encoders without having to renegotiate.
 */
static GstElement *
make_pipeline (void)
{
  GstElement *result;
  GstElement *audiotestsrc;
  GstElement *audiocaps;
  GstElement *outputselect;
  GstElement *inputselect;
  GstElement *sink;
  GstCaps *caps;
  GstCaps *capslist[3];
  gint i;

  /* create result pipeline */
  result = gst_pipeline_new (NULL);
  g_assert (result);

  /* create various elements */
  audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (audiotestsrc, "num-buffers", 1000, NULL);
  g_assert (audiotestsrc);

  audiocaps = gst_element_factory_make ("capsfilter", NULL);
  g_assert (audiocaps);

  caps =
      gst_caps_from_string ("audio/x-raw,format=S16LE,rate=48000,channels=1");
  g_object_set (audiocaps, "caps", caps, NULL);
  gst_caps_unref (caps);

  outputselect = gst_element_factory_make ("output-selector", "select");
  g_assert (outputselect);

  inputselect = gst_element_factory_make ("input-selector", NULL);
  g_assert (inputselect);
  g_object_set (inputselect, "select-all", TRUE, NULL);

  sink = gst_element_factory_make ("fakesink", NULL);
  g_object_set (sink, "sync", TRUE, NULL);
  g_object_set (sink, "silent", TRUE, NULL);
  g_assert (sink);

  /* add elements */
  gst_bin_add (GST_BIN (result), audiotestsrc);
  gst_bin_add (GST_BIN (result), audiocaps);
  gst_bin_add (GST_BIN (result), outputselect);
  gst_bin_add (GST_BIN (result), inputselect);
  gst_bin_add (GST_BIN (result), sink);

  /* link elements */
  gst_element_link_pads (audiotestsrc, "src", audiocaps, "sink");
  gst_element_link_pads (audiocaps, "src", outputselect, "sink");
  gst_element_link_pads (inputselect, "src", sink, "sink");

  /* make caps */
  capslist[0] =
      gst_caps_from_string ("audio/x-raw,format=S16LE,rate=48000,channels=1");
  capslist[1] =
      gst_caps_from_string ("audio/x-raw,format=S16LE,rate=16000,channels=1");
  capslist[2] =
      gst_caps_from_string ("audio/x-raw,format=S16LE,rate=8000,channels=1");

  /* create encoder elements */
  for (i = 0; i < 3; i++) {
    GstElement *encoder;
    GstPad *srcpad, *sinkpad;

    encoder = make_encoder (capslist[i]);
    g_assert (encoder);

    gst_bin_add (GST_BIN (result), encoder);

    srcpad = gst_element_request_pad_simple (outputselect, "src_%u");
    sinkpad = gst_element_get_static_pad (encoder, "sink");
    gst_pad_link (srcpad, sinkpad);
    gst_object_unref (srcpad);
    gst_object_unref (sinkpad);

    srcpad = gst_element_get_static_pad (encoder, "src");
    sinkpad = gst_element_request_pad_simple (inputselect, "sink_%u");
    gst_pad_link (srcpad, sinkpad);
    gst_object_unref (srcpad);
    gst_object_unref (sinkpad);
  }

  return result;
}

static gboolean
do_switch (GstElement * pipeline)
{
  gint rand;
  GstElement *select;
  gchar *name;
  GstPad *pad;

  rand = g_random_int_range (0, 3);

  g_print ("switching to %d\n", rand);

  /* find the selector */
  select = gst_bin_get_by_name (GST_BIN (pipeline), "select");

  /* get the named pad */
  name = g_strdup_printf ("src_%u", rand);
  pad = gst_element_get_static_pad (select, name);
  g_free (name);

  /* set the active pad */
  g_object_set (select, "active-pad", pad, NULL);
  gst_object_unref (select);

  return TRUE;
}

static gboolean
my_bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  GstElement *sender = (GstElement *) GST_MESSAGE_SRC (message);
  gchar *name = gst_element_get_name (sender);
  GMainLoop *loop = (GMainLoop *) data;

  g_print ("Got %s message from %s\n", GST_MESSAGE_TYPE_NAME (message), name);
  g_free (name);

  switch (GST_MESSAGE_TYPE (message)) {

    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      g_print ("Error: %s (%s)\n", err->message, debug);
      g_error_free (err);
      g_free (debug);

      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      g_main_loop_quit (loop);
      break;
    default:
      /* unhandled message */
      break;
  }

  return TRUE;
}

gint
main (gint argc, gchar * argv[])
{
  GstElement *pipeline;
  GstBus *bus;
  GMainLoop *loop;

  /* init GStreamer */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* set up */
  pipeline = make_pipeline ();

  g_signal_connect (pipeline, "deep_notify",
      G_CALLBACK (gst_object_default_deep_notify), NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, my_bus_callback, loop);
  gst_object_unref (bus);

  g_print ("Starting pipeline\n");

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* add a timeout to cycle between the formats */
  g_timeout_add_seconds (1, (GSourceFunc) do_switch, pipeline);

  /* now run */
  g_main_loop_run (loop);

  g_print ("Nulling pipeline\n");

  /* also clean up */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  return 0;
}
