/* GStreamer
 *
 * Copyright (C) 2010 Wim Taymans <wim.taymans@collabora.co.uk>
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

/* An example of synchronized playback and recording.
 * The trick is to wait for the playback pipeline to preroll before starting
 * playback and recording.
 */
#include <string.h>

#include <gst/gst.h>

/* Define to run the asynchronous version, which has the benefit that it doesn't
 * block the main thread but it produces slightly less clear code. */
#define ASYNC_VERSION

static GMainLoop *loop;
static GstElement *pipeline = NULL;
static GstElement *play_bin, *play_source, *play_sink;
static GstElement *rec_bin, *rec_source, *rec_sink;

static gboolean
message_handler (GstBus * bus, GstMessage * message, gpointer user_data)
{

  switch (GST_MESSAGE_TYPE (message)) {
#ifdef ASYNC_VERSION
    case GST_MESSAGE_ELEMENT:{
      const GstStructure *str;

      str = gst_message_get_structure (message);

      if (gst_structure_has_name (str, "GstBinForwarded")) {
        GstMessage *orig;

        /* unwrap the element message */
        gst_structure_get (str, "message", GST_TYPE_MESSAGE, &orig, NULL);
        g_assert (orig);

        switch (GST_MESSAGE_TYPE (orig)) {
          case GST_MESSAGE_ASYNC_DONE:
            g_print ("ASYNC done %s\n", GST_MESSAGE_SRC_NAME (orig));
            if (GST_MESSAGE_SRC (orig) == GST_OBJECT_CAST (play_bin)) {
              g_print
                  ("prerolled, starting synchronized playback and recording\n");
              /* returns ASYNC because the sink linked to the live source is not
               * prerolled */
              if (gst_element_set_state (pipeline,
                      GST_STATE_PLAYING) != GST_STATE_CHANGE_ASYNC) {
                g_warning ("State change failed");
              }
            }
            break;
          default:
            break;
        }
      }
      break;
    }
#endif
    case GST_MESSAGE_EOS:
      g_print ("EOS\n");
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;

      gst_message_parse_error (message, &err, NULL);
      g_print ("error: %s\n", err->message);
      g_clear_error (&err);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GstBus *bus;
  gint watch_id;

  gst_init (NULL, NULL);

  loop = g_main_loop_new (NULL, TRUE);

  pipeline = gst_pipeline_new ("pipeline");
#ifdef ASYNC_VERSION
  /* this enables messages of individual elements inside the pipeline */
  g_object_set (pipeline, "message-forward", TRUE, NULL);
#endif

  /* make a bin with the playback elements this is a non-live pipeline */
  play_bin = gst_bin_new ("play_bin");
  play_source = gst_element_factory_make ("audiotestsrc", "play_source");
  play_sink = gst_element_factory_make ("autoaudiosink", "play_sink");

  gst_bin_add (GST_BIN (play_bin), play_source);
  gst_bin_add (GST_BIN (play_bin), play_sink);

  gst_element_link (play_source, play_sink);

  /* make bin with the record elements, this is a live pipeline */
  rec_bin = gst_bin_new ("rec_bin");
  rec_source = gst_element_factory_make ("autoaudiosrc", "rec_source");
  rec_sink = gst_element_factory_make ("fakesink", "rec_sink");

  gst_bin_add (GST_BIN (rec_bin), rec_source);
  gst_bin_add (GST_BIN (rec_bin), rec_sink);

  gst_element_link (rec_source, rec_sink);

  gst_bin_add (GST_BIN (pipeline), play_bin);
  gst_bin_add (GST_BIN (pipeline), rec_bin);

  bus = gst_element_get_bus (pipeline);
  watch_id = gst_bus_add_watch (bus, message_handler, NULL);
  gst_object_unref (bus);

  g_print ("going to PAUSED\n");
  /* returns NO_PREROLL because we have a live element */
  if (gst_element_set_state (pipeline,
          GST_STATE_PAUSED) != GST_STATE_CHANGE_NO_PREROLL) {
    g_warning ("Expected state change NO_PREROLL result");
  }

  g_print ("waiting for playback preroll\n");
#ifndef ASYNC_VERSION
  /* sync wait for preroll on the playback bin and then go to PLAYING */
  if (gst_element_get_state (play_bin, NULL, NULL,
          GST_CLOCK_TIME_NONE) != GST_STATE_CHANGE_SUCCESS) {
    g_warning ("Error while waiting for state change");
  }
  g_print ("prerolled, starting synchronized playback and recording\n");
  /* returns ASYNC because the sink linked to the live source is not
   * prerolled */
  if (gst_element_set_state (pipeline,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_ASYNC) {
    g_warning ("Expected state change NO_PREROLL result");
  }
#endif

  g_main_loop_run (loop);

  g_source_remove (watch_id);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  g_main_loop_unref (loop);

  return 0;
}
