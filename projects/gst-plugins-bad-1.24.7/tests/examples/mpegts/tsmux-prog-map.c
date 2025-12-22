/* GStreamer
 *
 * tsmux-prog-map.c: sample application to show how to construct the
 * mpegtsmux prog-map property.
 *
 * MIT License
 *
 * Copyright (C) 2021 Jan Schmidt <jan@centricular.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.*
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

/* Test pipeline with h264video on PID 101 and AAC audio on PID 102
 * The streams will be assigned to Program #2, with PMT on PID 100, and
 * the PCR on the video stream */
#define TEST_PIPELINE "videotestsrc num-buffers=90 ! video/x-raw,framerate=30/1 ! x264enc ! queue ! .sink_101 mpegtsmux name=mux ! fakesink audiotestsrc samplesperbuffer=4800 num-buffers=30 ! audio/x-raw,rate=48000 ! fdkaacenc ! aacparse ! mux.sink_102"

static void
_on_bus_message (GstBus * bus, GstMessage * message, GMainLoop * mainloop)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
    case GST_MESSAGE_EOS:
      g_main_loop_quit (mainloop);
      break;
    default:
      break;
  }
}

int
main (int argc, char **argv)
{
  GstElement *pipeline = NULL;
  GError *error = NULL;
  GstBus *bus;
  GMainLoop *mainloop;
  GstElement *muxer;
  GstStructure *prog_map;

  gst_init (&argc, &argv);

  pipeline = gst_parse_launch (TEST_PIPELINE, &error);
  if (error) {
    g_print ("Error constructing pipeline: %s\n", error->message);
    g_clear_error (&error);
    return 1;
  }

  mainloop = g_main_loop_new (NULL, FALSE);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", (GCallback) _on_bus_message, mainloop);

  /* Configure the program map here. The elementary streams get their PIDs from the pad name.
   * Assign them to program # 2, with PMT on PID 100 */

  muxer = gst_bin_get_by_name (GST_BIN (pipeline), "mux");

  prog_map = gst_structure_new ("x-prog-map",
      "sink_101", G_TYPE_INT, 2, "sink_102", G_TYPE_INT, 2,
      "PMT_2", G_TYPE_UINT, 100, "PCR_2", G_TYPE_STRING, "sink_101", NULL);
  g_object_set (muxer, "prog-map", prog_map, NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_main_loop_run (mainloop);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (pipeline);
  gst_object_unref (bus);

  return 0;
}
