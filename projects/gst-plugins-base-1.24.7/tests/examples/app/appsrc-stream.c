/* GStreamer
 *
 * appsrc-stream.c: example for using appsrc in streaming mode.
 *
 * Copyright (C) 2008 Wim Taymans <wim.taymans@gmail.com>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY (appsrc_playbin_debug);
#define GST_CAT_DEFAULT appsrc_playbin_debug

/*
 * an example application of using appsrc in streaming push mode. We simply push
 * buffers into appsrc. The size of the buffers we push can be any size we
 * choose.
 *
 * This example is very close to how one would deal with a streaming webserver
 * that does not support range requests or does not report the total file size.
 *
 * Some optimisations are done so that we don't push too much data. We connect
 * to the need-data and enough-data signals to start/stop sending buffers.
 *
 * Appsrc in streaming mode (the default) does not support seeking so we don't
 * have to handle any seek callbacks.
 *
 * Some formats are able to estimate the duration of the media file based on the
 * file length (mp3, mpeg,..), others report an unknown length (ogg,..).
 */
typedef struct _App App;

struct _App
{
  GstElement *playbin;
  GstElement *appsrc;

  GMainLoop *loop;
  guint sourceid;

  GMappedFile *file;
  guint8 *data;
  gsize length;
  guint64 offset;
};

App s_app;

#define CHUNK_SIZE  4096

/* This method is called by the idle GSource in the mainloop. We feed CHUNK_SIZE
 * bytes into appsrc.
 * The ide handler is added to the mainloop when appsrc requests us to start
 * sending data (need-data signal) and is removed when appsrc has enough data
 * (enough-data signal).
 */
static gboolean
read_data (App * app)
{
  GstBuffer *buffer;
  guint len;
  GstFlowReturn ret;

  if (app->offset >= app->length) {
    /* we are EOS, send end-of-stream and remove the source */
    g_signal_emit_by_name (app->appsrc, "end-of-stream", &ret);
    return FALSE;
  }

  /* read the next chunk */
  buffer = gst_buffer_new ();

  len = CHUNK_SIZE;
  if (app->offset + len > app->length)
    len = app->length - app->offset;

  gst_buffer_append_memory (buffer,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
          app->data, app->length, app->offset, len, NULL, NULL));

  GST_DEBUG ("feed buffer %p, offset %" G_GUINT64_FORMAT "-%u", buffer,
      app->offset, len);
  g_signal_emit_by_name (app->appsrc, "push-buffer", buffer, &ret);
  gst_buffer_unref (buffer);
  if (ret != GST_FLOW_OK) {
    /* some error, stop sending data */
    return FALSE;
  }

  app->offset += len;

  return TRUE;
}

/* This signal callback is called when appsrc needs data, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
static void
start_feed (GstElement * playbin, guint size, App * app)
{
  if (app->sourceid == 0) {
    GST_DEBUG ("start feeding");
    app->sourceid = g_idle_add ((GSourceFunc) read_data, app);
  }
}

/* This callback is called when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void
stop_feed (GstElement * playbin, App * app)
{
  if (app->sourceid != 0) {
    GST_DEBUG ("stop feeding");
    g_source_remove (app->sourceid);
    app->sourceid = 0;
  }
}

/* this callback is called when playbin has constructed a source object to read
 * from. Since we provided the appsrc:// uri to playbin, this will be the
 * appsrc that we must handle. We set up some signals to start and stop pushing
 * data into appsrc */
static void
found_source (GObject * object, GObject * orig, GParamSpec * pspec, App * app)
{
  /* get a handle to the appsrc */
  g_object_get (orig, pspec->name, &app->appsrc, NULL);

  GST_DEBUG ("got appsrc %p", app->appsrc);

  /* we can set the length in appsrc. This allows some elements to estimate the
   * total duration of the stream. It's a good idea to set the property when you
   * can but it's not required. */
  g_object_set (app->appsrc, "size", (gint64) app->length, NULL);

  /* configure the appsrc, we will push data into the appsrc from the
   * mainloop. */
  g_signal_connect (app->appsrc, "need-data", G_CALLBACK (start_feed), app);
  g_signal_connect (app->appsrc, "enough-data", G_CALLBACK (stop_feed), app);
}

static gboolean
bus_message (GstBus * bus, GstMessage * message, App * app)
{
  GST_DEBUG ("got message %s",
      gst_message_type_get_name (GST_MESSAGE_TYPE (message)));

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      g_error ("received error");
      g_main_loop_quit (app->loop);
      break;
    case GST_MESSAGE_EOS:
      g_main_loop_quit (app->loop);
      break;
    default:
      break;
  }
  return TRUE;
}

int
main (int argc, char *argv[])
{
  App *app = &s_app;
  GError *error = NULL;
  GstBus *bus;

  gst_init (&argc, &argv);

  GST_DEBUG_CATEGORY_INIT (appsrc_playbin_debug, "appsrc-playbin", 0,
      "appsrc playbin example");

  if (argc < 2) {
    g_print ("usage: %s <filename>\n", argv[0]);
    return -1;
  }

  /* try to open the file as an mmapped file */
  app->file = g_mapped_file_new (argv[1], FALSE, &error);
  if (error) {
    g_print ("failed to open file: %s\n", error->message);
    g_error_free (error);
    return -2;
  }
  /* get some vitals, this will be used to read data from the mmapped file and
   * feed it to appsrc. */
  app->length = g_mapped_file_get_length (app->file);
  app->data = (guint8 *) g_mapped_file_get_contents (app->file);
  app->offset = 0;

  /* create a mainloop to get messages and to handle the idle handler that will
   * feed data to appsrc. */
  app->loop = g_main_loop_new (NULL, TRUE);

  app->playbin = gst_element_factory_make ("playbin", NULL);
  g_assert (app->playbin);

  bus = gst_pipeline_get_bus (GST_PIPELINE (app->playbin));

  /* add watch for messages */
  gst_bus_add_watch (bus, (GstBusFunc) bus_message, app);

  /* set to read from appsrc */
  g_object_set (app->playbin, "uri", "appsrc://", NULL);

  /* get notification when the source is created so that we get a handle to it
   * and can configure it */
  g_signal_connect (app->playbin, "deep-notify::source",
      (GCallback) found_source, app);

  /* go to playing and wait in a mainloop. */
  gst_element_set_state (app->playbin, GST_STATE_PLAYING);

  /* this mainloop is stopped when we receive an error or EOS */
  g_main_loop_run (app->loop);

  GST_DEBUG ("stopping");

  gst_element_set_state (app->playbin, GST_STATE_NULL);

  /* free the file */
  g_mapped_file_unref (app->file);

  gst_object_unref (bus);
  g_main_loop_unref (app->loop);

  return 0;
}
