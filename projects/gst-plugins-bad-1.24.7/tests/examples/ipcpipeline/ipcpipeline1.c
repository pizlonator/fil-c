/* GStreamer
 *
 * example program for the ipcpipelinesrc/ipcpipelinesink elements
 *
 * Copyright (C) 2015-2017 YouView TV Ltd
 *   Author: Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
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
 * This program shows a moving ball on a video sink, with the video sink
 * running in a different process than videotestsrc.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <gst/gst.h>

static GMainLoop *loop = NULL;

static gboolean
master_bus_msg (GstBus * bus, GstMessage * msg, gpointer data)
{
  GstPipeline *pipeline = data;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      g_printerr ("ERROR: %s\n", err->message);
      if (dbg != NULL)
        g_printerr ("ERROR debug information: %s\n", dbg);
      g_error_free (err);
      g_free (dbg);

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "ipc.error");

      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_WARNING:{
      GError *err;
      gchar *dbg;

      gst_message_parse_warning (msg, &err, &dbg);
      g_printerr ("WARNING: %s\n", err->message);
      if (dbg != NULL)
        g_printerr ("WARNING debug information: %s\n", dbg);
      g_error_free (err);
      g_free (dbg);

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "ipc.warning");
      break;
    }
    case GST_MESSAGE_ASYNC_DONE:
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "ipc.async-done");
      break;
    case GST_MESSAGE_EOS:
      gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);
      g_main_loop_quit (loop);
      break;
    default:
      break;
  }
  return TRUE;
}

static void
start_source (int fdin, int fdout)
{
  GstElement *pipeline;
  GstElement *source, *ipcpipelinesink, *capsfilter;
  GstCaps *caps;

  pipeline = gst_pipeline_new (NULL);
  gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), master_bus_msg, pipeline);

  source = gst_element_factory_make ("videotestsrc", NULL);
  g_object_set (source, "pattern", 18, "num-buffers", 50, NULL);

  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  caps =
      gst_caps_new_simple ("video/x-raw", "width", G_TYPE_INT, 640, "height",
      G_TYPE_INT, 480, NULL);
  g_object_set (capsfilter, "caps", caps, NULL);
  gst_caps_unref (caps);

  ipcpipelinesink = gst_element_factory_make ("ipcpipelinesink", NULL);
  g_object_set (ipcpipelinesink, "fdin", fdin, "fdout", fdout, NULL);

  gst_bin_add_many (GST_BIN (pipeline), source, capsfilter, ipcpipelinesink,
      NULL);
  gst_element_link_many (source, capsfilter, ipcpipelinesink, NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "ipc.src");
}

static void
start_sink (int fdin, int fdout)
{
  GstElement *pipeline;
  GstElement *ipcpipelinesrc, *navseek, *sink;

  pipeline = gst_element_factory_make ("ipcslavepipeline", NULL);
  ipcpipelinesrc = gst_element_factory_make ("ipcpipelinesrc", NULL);
  navseek = gst_element_factory_make ("navseek", NULL);
  g_object_set (navseek, "seek-offset", 1.0, NULL);
  sink = gst_element_factory_make ("autovideosink", NULL);
  g_object_set (ipcpipelinesrc, "fdin", fdin, "fdout", fdout, NULL);
  gst_bin_add_many (GST_BIN (pipeline), ipcpipelinesrc, navseek, sink, NULL);
  gst_element_link_many (ipcpipelinesrc, navseek, sink, NULL);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "ipc.sink");
  /* The state of the slave pipeline will change together with the state
   * of the master, there is no need to call gst_element_set_state() here */
}

static void
run (pid_t pid)
{
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
  if (pid > 0)
    kill (pid, SIGTERM);
}

int
main (int argc, char **argv)
{
  int sockets[2];
  pid_t pid;

  if (socketpair (AF_UNIX, SOCK_STREAM, 0, sockets)) {
    fprintf (stderr, "Error creating sockets: %s\n", strerror (errno));
    return 1;
  }
  if (fcntl (sockets[0], F_SETFL, O_NONBLOCK) < 0 ||
      fcntl (sockets[1], F_SETFL, O_NONBLOCK) < 0) {
    fprintf (stderr, "Error setting O_NONBLOCK on sockets: %s\n",
        strerror (errno));
    return 1;
  }

  pid = fork ();
  if (pid < 0) {
    fprintf (stderr, "Error forking: %s\n", strerror (errno));
    return 1;
  } else if (pid > 0) {
    setenv ("GST_DEBUG_FILE", "gstsrc.log", 1);
    gst_init (&argc, &argv);
    start_source (sockets[0], sockets[0]);
  } else {
    setenv ("GST_DEBUG_FILE", "gstsink.log", 1);
    gst_init (&argc, &argv);
    start_sink (sockets[1], sockets[1]);
  }

  run (pid);

  return 0;
}
