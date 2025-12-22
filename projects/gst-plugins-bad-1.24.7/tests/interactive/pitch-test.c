/* GStreamer
 *
 * Copyright (C) 2006 Thomas Vander Stichele <thomas at apestaart dot org>
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

/* compile with :
 * gcc -Wall $(pkg-config --cflags --libs gstreamer-0.10 gstreamer-controller-0.10) pitch.c -o pitch
 */

#include <string.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <gst/gst.h>
#include <gst/controller/gsttimedvaluecontrolsource.h>
#include <gst/controller/gstinterpolationcontrolsource.h>
#include <gst/controller/gstdirectcontrolbinding.h>

int
main (int argc, char **argv)
{
  GMainLoop *loop;
  gint i;

  GstElement *audiotestsrc;
  GstElement *audioconvert1, *audioconvert2;
  GstElement *pitch;
  GstElement *sink;
  GstElement *pipeline;
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;

  if (argc != 2) {
    g_printerr ("Usage: %s <audiosink>\n", argv[0]);
    return 1;
  }

  /* initialize GStreamer */
  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  pipeline = gst_pipeline_new ("audio-player");
  audiotestsrc = gst_element_factory_make ("audiotestsrc", "audiotestsrc");
  g_assert (audiotestsrc != NULL);
  audioconvert1 = gst_element_factory_make ("audioconvert", "audioconvert1");
  g_assert (audioconvert1 != NULL);
  audioconvert2 = gst_element_factory_make ("audioconvert", "audioconvert2");
  g_assert (audioconvert2 != NULL);
  pitch = gst_element_factory_make ("pitch", "pitch");
  g_assert (pitch != NULL);
  sink = gst_element_factory_make (argv[1], "sink");
  g_assert (sink != NULL);

  gst_bin_add_many (GST_BIN (pipeline),
      audiotestsrc, audioconvert1, pitch, audioconvert2, sink, NULL);
  gst_element_link_many (audiotestsrc, audioconvert1, pitch, audioconvert2,
      sink, NULL);

  /* set up a controller */
  cs = gst_interpolation_control_source_new ();
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  gst_object_add_control_binding (GST_OBJECT (pitch),
      gst_direct_control_binding_new (GST_OBJECT (pitch), "pitch", cs));
  tvcs = (GstTimedValueControlSource *) cs;

  for (i = 0; i < 100; ++i) {
    if (i % 2)
      gst_timed_value_control_source_set (tvcs, i * GST_SECOND, 0.5);
    else
      gst_timed_value_control_source_set (tvcs, i * GST_SECOND, 1.5);
  }

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_print ("Running\n");
  g_main_loop_run (loop);

  /* clean up nicely */
  gst_object_unref (cs);
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));

  return 0;
}
