/* GstInterTest
 * Copyright (C) 2011 David Schleef <ds@schleef.org>
 * Copyright (C) 2010 Entropy Wave Inc
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <stdlib.h>

//#define GETTEXT_PACKAGE "intertest"


typedef struct _GstInterTest GstInterTest;
struct _GstInterTest
{
  GstElement *pipeline;
  GstBus *bus;
  GMainLoop *main_loop;

  GstElement *source_element;
  GstElement *sink_element;

  gboolean paused_for_buffering;
  guint timer_id;
};

GstInterTest *gst_inter_test_new (void);
void gst_inter_test_free (GstInterTest * intertest);
void gst_inter_test_create_pipeline_server (GstInterTest * intertest);
void gst_inter_test_create_pipeline_vts (GstInterTest * intertest);
void gst_inter_test_create_pipeline_playbin (GstInterTest * intertest,
    const char *uri);
void gst_inter_test_start (GstInterTest * intertest);
void gst_inter_test_stop (GstInterTest * intertest);

static gboolean gst_inter_test_handle_message (GstBus * bus,
    GstMessage * message, gpointer data);
static gboolean onesecond_timer (gpointer priv);


gboolean verbose;

static GOptionEntry entries[] = {
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL},

  {NULL}

};

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;
  GstInterTest *intertest1;
  GstInterTest *intertest2;
  GMainLoop *main_loop;

  context = g_option_context_new ("- Internal src/sink test");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_print ("option parsing failed: %s\n", error->message);
    g_option_context_free (context);
    g_clear_error (&error);
    exit (1);
  }
  g_option_context_free (context);

  intertest1 = gst_inter_test_new ();
  gst_inter_test_create_pipeline_server (intertest1);
  gst_inter_test_start (intertest1);

  intertest2 = gst_inter_test_new ();
  gst_inter_test_create_pipeline_playbin (intertest2, NULL);
  gst_inter_test_start (intertest2);

  main_loop = g_main_loop_new (NULL, TRUE);
  intertest1->main_loop = main_loop;
  intertest2->main_loop = main_loop;

  g_main_loop_run (main_loop);
  g_main_loop_unref (main_loop);

  exit (0);
}


GstInterTest *
gst_inter_test_new (void)
{
  GstInterTest *intertest;

  intertest = g_new0 (GstInterTest, 1);

  return intertest;
}

void
gst_inter_test_free (GstInterTest * intertest)
{
  if (intertest->source_element) {
    gst_object_unref (intertest->source_element);
    intertest->source_element = NULL;
  }
  if (intertest->sink_element) {
    gst_object_unref (intertest->sink_element);
    intertest->sink_element = NULL;
  }

  if (intertest->bus) {
    gst_object_unref (intertest->bus);
    intertest->bus = NULL;
  }

  if (intertest->pipeline) {
    gst_element_set_state (intertest->pipeline, GST_STATE_NULL);
    gst_object_unref (intertest->pipeline);
    intertest->pipeline = NULL;
  }
  g_free (intertest);
}

void
gst_inter_test_create_pipeline_playbin (GstInterTest * intertest,
    const char *uri)
{
  GstElement *pipeline;

  if (uri == NULL) {
    gst_inter_test_create_pipeline_vts (intertest);
    return;
  }

  pipeline = gst_pipeline_new (NULL);
  gst_bin_add (GST_BIN (pipeline),
      gst_element_factory_make ("playbin", "source"));

  intertest->pipeline = pipeline;

  gst_pipeline_set_auto_flush_bus (GST_PIPELINE (pipeline), FALSE);
  intertest->bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (intertest->bus, gst_inter_test_handle_message, intertest);

  intertest->source_element =
      gst_bin_get_by_name (GST_BIN (pipeline), "source");
  g_print ("source_element is %p\n", intertest->source_element);

  g_print ("setting uri to %s\n", uri);
  g_object_set (intertest->source_element, "uri", uri, NULL);
}

void
gst_inter_test_create_pipeline_vts (GstInterTest * intertest)
{
  GString *pipe_desc;
  GstElement *pipeline;
  GError *error = NULL;

  pipe_desc = g_string_new ("");

  g_string_append (pipe_desc, "videotestsrc name=source num-buffers=100 ! ");
  g_string_append (pipe_desc,
      "video/x-raw,format=(string)I420,width=320,height=240 ! ");
  g_string_append (pipe_desc, "timeoverlay ! ");
  g_string_append (pipe_desc, "intervideosink name=sink sync=true ");
  g_string_append (pipe_desc,
      "audiotestsrc samplesperbuffer=1600 num-buffers=100 ! audioconvert ! ");
  g_string_append (pipe_desc, "interaudiosink sync=true ");

  if (verbose)
    g_print ("pipeline: %s\n", pipe_desc->str);

  pipeline = (GstElement *) gst_parse_launch (pipe_desc->str, &error);
  g_string_free (pipe_desc, TRUE);

  if (error) {
    g_print ("pipeline parsing error: %s\n", error->message);
    gst_object_unref (pipeline);
    g_clear_error (&error);
    return;
  }

  intertest->pipeline = pipeline;

  gst_pipeline_set_auto_flush_bus (GST_PIPELINE (pipeline), FALSE);
  intertest->bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (intertest->bus, gst_inter_test_handle_message, intertest);

  intertest->source_element =
      gst_bin_get_by_name (GST_BIN (pipeline), "source");
  intertest->sink_element = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
}

void
gst_inter_test_create_pipeline_server (GstInterTest * intertest)
{
  GString *pipe_desc;
  GstElement *pipeline;
  GError *error = NULL;

  pipe_desc = g_string_new ("");

  g_string_append (pipe_desc, "intervideosrc ! queue ! ");
  g_string_append (pipe_desc, "xvimagesink name=sink ");
  g_string_append (pipe_desc, "interaudiosrc ! queue ! ");
  g_string_append (pipe_desc, "alsasink ");

  if (verbose)
    g_print ("pipeline: %s\n", pipe_desc->str);

  pipeline = (GstElement *) gst_parse_launch (pipe_desc->str, &error);
  g_string_free (pipe_desc, TRUE);

  if (error) {
    g_print ("pipeline parsing error: %s\n", error->message);
    gst_object_unref (pipeline);
    g_clear_error (&error);
    return;
  }

  intertest->pipeline = pipeline;

  gst_pipeline_set_auto_flush_bus (GST_PIPELINE (pipeline), FALSE);
  intertest->bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (intertest->bus, gst_inter_test_handle_message, intertest);

  intertest->source_element =
      gst_bin_get_by_name (GST_BIN (pipeline), "source");
  intertest->sink_element = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
}

void
gst_inter_test_start (GstInterTest * intertest)
{
  gst_element_set_state (intertest->pipeline, GST_STATE_READY);

  intertest->timer_id = g_timeout_add (1000, onesecond_timer, intertest);
}

void
gst_inter_test_stop (GstInterTest * intertest)
{
  gst_element_set_state (intertest->pipeline, GST_STATE_NULL);

  g_source_remove (intertest->timer_id);
}

static void
gst_inter_test_handle_eos (GstInterTest * intertest)
{
  gst_inter_test_stop (intertest);
}

static void
gst_inter_test_handle_error (GstInterTest * intertest, GError * error,
    const char *debug)
{
  g_print ("error: %s\n", error->message);
  gst_inter_test_stop (intertest);
}

static void
gst_inter_test_handle_warning (GstInterTest * intertest, GError * error,
    const char *debug)
{
  g_print ("warning: %s\n", error->message);
}

static void
gst_inter_test_handle_info (GstInterTest * intertest, GError * error,
    const char *debug)
{
  g_print ("info: %s\n", error->message);
}

static void
gst_inter_test_handle_null_to_ready (GstInterTest * intertest)
{
  gst_element_set_state (intertest->pipeline, GST_STATE_PAUSED);

}

static void
gst_inter_test_handle_ready_to_paused (GstInterTest * intertest)
{
  if (!intertest->paused_for_buffering) {
    gst_element_set_state (intertest->pipeline, GST_STATE_PLAYING);
  }
}

static void
gst_inter_test_handle_paused_to_playing (GstInterTest * intertest)
{

}

static void
gst_inter_test_handle_playing_to_paused (GstInterTest * intertest)
{

}

static void
gst_inter_test_handle_paused_to_ready (GstInterTest * intertest)
{

}

static void
gst_inter_test_handle_ready_to_null (GstInterTest * intertest)
{
  //g_main_loop_quit (intertest->main_loop);

}


static gboolean
gst_inter_test_handle_message (GstBus * bus, GstMessage * message,
    gpointer data)
{
  GstInterTest *intertest = (GstInterTest *) data;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      gst_inter_test_handle_eos (intertest);
      break;
    case GST_MESSAGE_ERROR:
    {
      GError *error = NULL;
      gchar *debug;

      gst_message_parse_error (message, &error, &debug);
      gst_inter_test_handle_error (intertest, error, debug);
      g_clear_error (&error);
      g_free (debug);
    }
      break;
    case GST_MESSAGE_WARNING:
    {
      GError *error = NULL;
      gchar *debug;

      gst_message_parse_warning (message, &error, &debug);
      gst_inter_test_handle_warning (intertest, error, debug);
      g_clear_error (&error);
      g_free (debug);
    }
      break;
    case GST_MESSAGE_INFO:
    {
      GError *error = NULL;
      gchar *debug;

      gst_message_parse_info (message, &error, &debug);
      gst_inter_test_handle_info (intertest, error, debug);
      g_clear_error (&error);
      g_free (debug);
    }
      break;
    case GST_MESSAGE_TAG:
    {
      GstTagList *tag_list;

      gst_message_parse_tag (message, &tag_list);
      if (verbose)
        g_print ("tag\n");
    }
      break;
    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState oldstate, newstate, pending;

      gst_message_parse_state_changed (message, &oldstate, &newstate, &pending);
      if (GST_ELEMENT (message->src) == intertest->pipeline) {
        if (verbose)
          g_print ("state change from %s to %s\n",
              gst_element_state_get_name (oldstate),
              gst_element_state_get_name (newstate));
        switch (GST_STATE_TRANSITION (oldstate, newstate)) {
          case GST_STATE_CHANGE_NULL_TO_READY:
            gst_inter_test_handle_null_to_ready (intertest);
            break;
          case GST_STATE_CHANGE_READY_TO_PAUSED:
            gst_inter_test_handle_ready_to_paused (intertest);
            break;
          case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
            gst_inter_test_handle_paused_to_playing (intertest);
            break;
          case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
            gst_inter_test_handle_playing_to_paused (intertest);
            break;
          case GST_STATE_CHANGE_PAUSED_TO_READY:
            gst_inter_test_handle_paused_to_ready (intertest);
            break;
          case GST_STATE_CHANGE_READY_TO_NULL:
            gst_inter_test_handle_ready_to_null (intertest);
            break;
          default:
            if (verbose)
              g_print ("unknown state change from %s to %s\n",
                  gst_element_state_get_name (oldstate),
                  gst_element_state_get_name (newstate));
        }
      }
    }
      break;
    case GST_MESSAGE_BUFFERING:
    {
      int percent;
      gst_message_parse_buffering (message, &percent);
      //g_print("buffering %d\n", percent);
      if (!intertest->paused_for_buffering && percent < 100) {
        g_print ("pausing for buffing\n");
        intertest->paused_for_buffering = TRUE;
        gst_element_set_state (intertest->pipeline, GST_STATE_PAUSED);
      } else if (intertest->paused_for_buffering && percent == 100) {
        g_print ("unpausing for buffing\n");
        intertest->paused_for_buffering = FALSE;
        gst_element_set_state (intertest->pipeline, GST_STATE_PLAYING);
      }
    }
      break;
    case GST_MESSAGE_STATE_DIRTY:
    case GST_MESSAGE_CLOCK_PROVIDE:
    case GST_MESSAGE_CLOCK_LOST:
    case GST_MESSAGE_NEW_CLOCK:
    case GST_MESSAGE_STRUCTURE_CHANGE:
    case GST_MESSAGE_STREAM_STATUS:
      break;
    case GST_MESSAGE_STEP_DONE:
    case GST_MESSAGE_APPLICATION:
    case GST_MESSAGE_ELEMENT:
    case GST_MESSAGE_SEGMENT_START:
    case GST_MESSAGE_SEGMENT_DONE:
    case GST_MESSAGE_LATENCY:
    case GST_MESSAGE_ASYNC_START:
    case GST_MESSAGE_ASYNC_DONE:
    case GST_MESSAGE_REQUEST_STATE:
    case GST_MESSAGE_STEP_START:
    default:
      if (verbose) {
        g_print ("message: %s\n", GST_MESSAGE_TYPE_NAME (message));
      }
      break;
    case GST_MESSAGE_QOS:
      break;
  }

  return TRUE;
}



static gboolean
onesecond_timer (gpointer priv)
{
  //GstInterTest *intertest = (GstInterTest *)priv;

  g_print (".\n");

  return TRUE;
}



/* helper functions */

#if 0
gboolean
have_element (const gchar * element_name)
{
  GstPluginFeature *feature;

  feature = gst_default_registry_find_feature (element_name,
      GST_TYPE_ELEMENT_FACTORY);
  if (feature) {
    g_object_unref (feature);
    return TRUE;
  }
  return FALSE;
}
#endif
