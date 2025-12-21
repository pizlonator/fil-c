/* GStreamer appsink benchmark
 * Copyright (C) 2018 Tim-Philipp Müller <tim centricular com>

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
#include <gst/app/app.h>

#define NUM_BUFFERS 10000000

int
main (int argc, char **argv)
{
  GstElement *src, *sink, *pipeline;
  GstSample *sample;

  gst_init (&argc, &argv);

  pipeline = gst_pipeline_new (NULL);

  src = gst_element_factory_make ("fakesrc", NULL);
  g_object_set (src, "num-buffers", NUM_BUFFERS, NULL);

  sink = gst_element_factory_make ("appsink", NULL);

  gst_bin_add_many (GST_BIN (pipeline), src, sink, NULL);
  gst_element_link_many (src, sink, NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  while ((sample = gst_app_sink_pull_sample (GST_APP_SINK (sink))))
    gst_sample_unref (sample);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  return 0;
}
