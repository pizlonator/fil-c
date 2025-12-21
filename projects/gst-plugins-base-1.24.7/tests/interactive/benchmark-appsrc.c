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

#define NUM_BUFFERS 40000000

int
main (int argc, char **argv)
{
  GstElement *src, *sink, *pipeline;
  GstBuffer *buf;
  gint i;

  gst_init (&argc, &argv);

  pipeline = gst_pipeline_new (NULL);

  src = gst_element_factory_make ("appsrc", NULL);
  sink = gst_element_factory_make ("fakesink", NULL);

  gst_bin_add_many (GST_BIN (pipeline), src, sink, NULL);
  gst_element_link_many (src, sink, NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  buf = gst_buffer_new ();

  for (i = 0; i < NUM_BUFFERS; ++i) {
    gst_app_src_push_buffer (GST_APP_SRC (src), gst_buffer_ref (buf));
  }
  gst_app_src_end_of_stream (GST_APP_SRC (src));

  gst_buffer_unref (buf);
  gst_element_set_state (pipeline, GST_STATE_NULL);

  return 0;
}
