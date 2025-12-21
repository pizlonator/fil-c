/* GStreamer
 *
 * sprinkle.c: sample application to dynamically mix tones with adder
 *
 * Copyright (C) <2009> Wim Taymans <wim dot taymans at gmail dot com>
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
 * Produces a sweeping sprinkle of tones by dynamically adding and removing
 * elements to adder.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

static GstElement *pipeline, *adder;
static GMainLoop *loop;

typedef struct
{
  GstElement *element;
  GstPad *srcpad;
  GstPad *sinkpad;
  gdouble freq;
} SourceInfo;

/* dynamically add the source to the pipeline and link it to a new pad on
 * adder */
static SourceInfo *
add_source (gdouble freq)
{
  SourceInfo *info;

  info = g_new0 (SourceInfo, 1);
  info->freq = freq;

  /* make source with unique name */
  info->element = gst_element_factory_make ("audiotestsrc", NULL);

  g_object_set (info->element, "freq", freq, NULL);

  /* add to the bin */
  gst_bin_add (GST_BIN (pipeline), info->element);

  /* get pad from the element */
  info->srcpad = gst_element_get_static_pad (info->element, "src");

  /* get new pad from adder, adder will now wait for data on this pad */
  info->sinkpad = gst_element_request_pad_simple (adder, "sink_%u");

  /* link pad to adder */
  gst_pad_link (info->srcpad, info->sinkpad);

  /* and play the element */
  gst_element_set_state (info->element, GST_STATE_PLAYING);

  g_print ("added freq %f\n", info->freq);

  return info;
}

/* remove the source from the pipeline after removing it from adder */
static void
remove_source (SourceInfo * info)
{
  g_print ("remove freq %f\n", info->freq);

  /* lock the state so that we can put it to NULL without the parent messing
   * with our state */
  gst_element_set_locked_state (info->element, TRUE);

  /* first stop the source. Remember that this might block when in the PAUSED
   * state. Alternatively one could send EOS to the source, install an event
   * probe and schedule a state change/unlink/release from the mainthread.
   * Note that changing the state of a source makes it emit an EOS, which can
   * make adder go EOS. */
  gst_element_set_state (info->element, GST_STATE_NULL);

  /* unlink from adder */
  gst_pad_unlink (info->srcpad, info->sinkpad);
  gst_object_unref (info->srcpad);

  /* remove from the bin */
  gst_bin_remove (GST_BIN (pipeline), info->element);

  /* give back the pad */
  gst_element_release_request_pad (adder, info->sinkpad);
  gst_object_unref (info->sinkpad);

  g_free (info);
}

/* we'll keep the state of the sources in this structure. We keep 3 sources
 * alive */
typedef struct
{
  guint count;
  SourceInfo *infos[3];
} SprinkleState;

static SprinkleState *
create_state (void)
{
  SprinkleState *state;

  state = g_new0 (SprinkleState, 1);

  return state;
}

static void
free_state (SprinkleState * state)
{
  SourceInfo *info;
  gint i;

  for (i = 0; i < 3; i++) {
    info = state->infos[i];
    if (info)
      remove_source (info);
  }

  g_free (state);
}

static gboolean
do_sprinkle (SprinkleState * state)
{
  SourceInfo *info;
  gint i;

  /* first remove the oldest info */
  info = state->infos[2];

  if (info)
    remove_source (info);

  /* move sources */
  for (i = 2; i > 0; i--) {
    state->infos[i] = state->infos[i - 1];
  }

  /* add new source, stop adding sources after 10 rounds. */
  if (state->count < 10) {
    state->infos[0] = add_source ((state->count * 100) + 200);
    state->count++;
  } else {
    state->infos[0] = NULL;

    /* if no more sources left, quit */
    if (!state->infos[2])
      g_main_loop_quit (loop);
  }

  return TRUE;
}

static void
message_received (GstBus * bus, GstMessage * message, GstPipeline * pipeline)
{
  const GstStructure *s;

  s = gst_message_get_structure (message);
  g_print ("message from \"%s\" (%s): ",
      GST_STR_NULL (GST_ELEMENT_NAME (GST_MESSAGE_SRC (message))),
      gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
  if (s) {
    gchar *sstr;

    sstr = gst_structure_to_string (s);
    g_print ("%s\n", sstr);
    g_free (sstr);
  } else {
    g_print ("no message details\n");
  }
}

static void
eos_message_received (GstBus * bus, GstMessage * message,
    GstPipeline * pipeline)
{
  message_received (bus, message, pipeline);
  g_main_loop_quit (loop);
}

int
main (int argc, char *argv[])
{
  GstBus *bus;
  GstElement *filter, *convert, *sink;
  GstCaps *caps;
  gboolean linked;
  SprinkleState *state;

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, TRUE);

  pipeline = gst_pipeline_new ("pipeline");

  /* add the fixed part to the pipeline. Remember that we need a capsfilter
   * after adder so that multiple sources are not racing to negotiate
   * a format */
  adder = gst_element_factory_make ("adder", "adder");
  filter = gst_element_factory_make ("capsfilter", "filter");
  convert = gst_element_factory_make ("audioconvert", "convert");
  sink = gst_element_factory_make ("autoaudiosink", "sink");

  caps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, "S16LE",
      "channels", G_TYPE_INT, 1, "rate", G_TYPE_INT, 44100, NULL);
  g_object_set (filter, "caps", caps, NULL);
  gst_caps_unref (caps);

  gst_bin_add_many (GST_BIN (pipeline), adder, filter, convert, sink, NULL);

  linked = gst_element_link_many (adder, filter, convert, sink, NULL);
  g_assert (linked);

  /* setup message handling */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);
  g_signal_connect (bus, "message::error", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::warning", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::eos", (GCallback) eos_message_received,
      pipeline);

  /* we set the pipeline to PLAYING, the pipeline will not yet preroll because
   * there is no source providing data for it yet */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* and add the function that modifies the pipeline every 100ms */
  state = create_state ();
  g_timeout_add (100, (GSourceFunc) do_sprinkle, state);

  /* go to main loop */
  g_main_loop_run (loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  free_state (state);
  gst_object_unref (bus);
  gst_object_unref (pipeline);

  return 0;
}
