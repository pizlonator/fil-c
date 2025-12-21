/* sample application for testing decodebin3
 *
 * Copyright (C) 2015 Centricular Ltd
 *  @author:  Edward Hervey <edward@centricular.com>
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#include <gst/gst.h>

/* Global structure */

typedef struct _AppData
{
  GMainLoop *mainloop;
  GstElement *pipeline;

  GstElement *decodebin;

  /* Current collection */
  GstStreamCollection *collection;
  guint notify_id;
} AppData;

static void
print_tag_foreach (const GstTagList * tags, const gchar * tag,
    gpointer user_data)
{
  GValue val = { 0, };
  gchar *str;
  gint depth = GPOINTER_TO_INT (user_data);

  if (!gst_tag_list_copy_value (&val, tags, tag))
    return;

  if (G_VALUE_HOLDS_STRING (&val))
    str = g_value_dup_string (&val);
  else
    str = gst_value_serialize (&val);

  gst_print ("%*s%s: %s\n", 2 * depth, " ", gst_tag_get_nick (tag), str);
  g_free (str);

  g_value_unset (&val);
}

static void
dump_collection (GstStreamCollection * collection)
{
  guint i;
  GstTagList *tags;
  GstCaps *caps;

  for (i = 0; i < gst_stream_collection_get_size (collection); i++) {
    GstStream *stream = gst_stream_collection_get_stream (collection, i);
    gst_print (" Stream %u type %s flags 0x%x\n", i,
        gst_stream_type_get_name (gst_stream_get_stream_type (stream)),
        gst_stream_get_stream_flags (stream));
    gst_print ("  ID: %s\n", gst_stream_get_stream_id (stream));

    caps = gst_stream_get_caps (stream);
    if (caps) {
      gchar *caps_str = gst_caps_to_string (caps);
      gst_print ("  caps: %s\n", caps_str);
      g_free (caps_str);
      gst_caps_unref (caps);
    }

    tags = gst_stream_get_tags (stream);
    if (tags) {
      gst_print ("  tags:\n");
      gst_tag_list_foreach (tags, print_tag_foreach, GUINT_TO_POINTER (3));
      gst_tag_list_unref (tags);
    }
  }
}

static gboolean
activate_all_av_streams (AppData * data)
{
  guint i, num_streams;
  gint num_videos = 0, num_audios = 0, num_texts = 0, num_unknowns = 0;
  GList *streams = NULL;
  GstEvent *event;
  gboolean ret;

  num_streams = gst_stream_collection_get_size (data->collection);
  for (i = 0; i < num_streams; i++) {
    GstStream *stream = gst_stream_collection_get_stream (data->collection, i);
    GstStreamType stype = gst_stream_get_stream_type (stream);
    if (stype == GST_STREAM_TYPE_VIDEO) {
      streams = g_list_append (streams,
          (gchar *) gst_stream_get_stream_id (stream));
      num_videos++;
    } else if (stype == GST_STREAM_TYPE_AUDIO) {
      streams = g_list_append (streams,
          (gchar *) gst_stream_get_stream_id (stream));
      num_audios++;
    } else if (stype == GST_STREAM_TYPE_TEXT) {
      num_texts++;
    } else {
      /* Unknown, container or complex type */
      num_unknowns++;
    }
  }

  gst_println ("Have %d streams (video: %d, audio: %d, text: %d, unknown %d)",
      num_streams, num_videos, num_audios, num_texts, num_unknowns);

  if (!num_videos && !num_audios) {
    gst_println ("No AV stream to expose");
    return FALSE;
  }

  event = gst_event_new_select_streams (streams);
  ret = gst_element_send_event (data->decodebin, event);

  gst_println ("Sent select-streams event ret %d", ret);

  return TRUE;
}

static void
stream_notify_cb (GstStreamCollection * collection, GstStream * stream,
    GParamSpec * pspec, guint * val)
{
  gst_print ("Got stream-notify from stream %s for %s (collection %p)\n",
      stream->stream_id, pspec->name, collection);
  if (g_str_equal (pspec->name, "caps")) {
    GstCaps *caps = gst_stream_get_caps (stream);
    gchar *caps_str = gst_caps_to_string (caps);
    gst_print (" New caps: %s\n", caps_str);
    g_free (caps_str);
    gst_caps_unref (caps);
  }
}

static GstBusSyncReply
_on_bus_message (GstBus * bus, GstMessage * message, AppData * data)
{
  GstObject *src = GST_MESSAGE_SRC (message);
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *name = gst_object_get_path_string (GST_MESSAGE_SRC (message));
      gst_message_parse_error (message, &err, NULL);

      gst_printerr ("ERROR: from element %s: %s\n", name, err->message);
      g_error_free (err);
      g_free (name);

      gst_println ("Stopping");
      g_main_loop_quit (data->mainloop);
      break;
    }
    case GST_MESSAGE_EOS:
      gst_println ("EOS ! Stopping");
      g_main_loop_quit (data->mainloop);
      break;
    case GST_MESSAGE_STREAM_COLLECTION:
    {
      GstStreamCollection *collection = NULL;
      gst_message_parse_stream_collection (message, &collection);
      if (collection) {
        /* Replace stream collection with new one */
        gst_println ("Got a collection from %s",
            src ? GST_OBJECT_NAME (src) : "Unknown");

        dump_collection (collection);

        if (data->collection && data->notify_id) {
          g_signal_handler_disconnect (data->collection, data->notify_id);
          data->notify_id = 0;
        }

        gst_object_replace ((GstObject **) & data->collection,
            (GstObject *) collection);

        if (data->collection) {
          data->notify_id =
              g_signal_connect (data->collection, "stream-notify",
              (GCallback) stream_notify_cb, data);
        }

        /* Try to expose all audio/video streams */
        if (!activate_all_av_streams (data))
          g_main_loop_quit (data->mainloop);
      }
      break;
    }
    default:
      break;
  }

  return GST_BUS_PASS;
}

static void
decodebin_pad_added_cb (GstElement * dbin, GstPad * pad, AppData * data)
{
  gchar *pad_name = gst_pad_get_name (pad);
  GstStream *stream;
  GstStreamType type;

  gst_println ("New pad %s added, try linking with sink", pad_name);
  g_free (pad_name);

  stream = gst_pad_get_stream (pad);
  if (!stream) {
    g_error ("New pad was exposed without GstStream object");
    g_main_loop_quit (data->mainloop);
    return;
  }

  type = gst_stream_get_stream_type (stream);

  switch (type) {
    case GST_STREAM_TYPE_VIDEO:{
      GstElement *queue;
      GstElement *convert;
      GstElement *sink;
      GstPad *sinkpad;

      queue = gst_element_factory_make ("queue", NULL);
      if (!queue) {
        gst_println ("queue element is unavailable");
        g_main_loop_quit (data->mainloop);
        return;
      }
      gst_bin_add (GST_BIN_CAST (data->pipeline), queue);

      convert = gst_element_factory_make ("videoconvert", NULL);
      if (!convert) {
        gst_println ("videoconvert element is unavailable");
        goto error;
      }
      gst_bin_add (GST_BIN_CAST (data->pipeline), convert);

      sink = gst_element_factory_make ("autovideosink", NULL);
      if (!sink) {
        gst_println ("autovideosink element is unavailable");
        goto error;
      }
      gst_bin_add (GST_BIN_CAST (data->pipeline), sink);

      sinkpad = gst_element_get_static_pad (queue, "sink");
      gst_pad_set_active (sinkpad, TRUE);

      gst_element_link_many (queue, convert, sink, NULL);
      gst_pad_link (pad, sinkpad);
      gst_object_unref (sinkpad);
      gst_element_sync_state_with_parent (queue);
      gst_element_sync_state_with_parent (convert);
      gst_element_sync_state_with_parent (sink);

      break;
    }
    case GST_STREAM_TYPE_AUDIO:{
      GstElement *queue;
      GstElement *convert;
      GstElement *resample;
      GstElement *sink;
      GstPad *sinkpad;

      queue = gst_element_factory_make ("queue", NULL);
      if (!queue) {
        gst_println ("queue element is unavailable");
        goto error;
      }
      gst_bin_add (GST_BIN_CAST (data->pipeline), queue);

      convert = gst_element_factory_make ("audioconvert", NULL);
      if (!convert) {
        gst_println ("audioconvert element is unavailable");
        goto error;
      }
      gst_bin_add (GST_BIN_CAST (data->pipeline), convert);

      resample = gst_element_factory_make ("audioresample", NULL);
      if (!resample) {
        gst_println ("audioresample element is unavailable");
        goto error;
      }
      gst_bin_add (GST_BIN_CAST (data->pipeline), resample);

      sink = gst_element_factory_make ("autoaudiosink", NULL);
      if (!sink) {
        gst_println ("autoaudiosink element is unavailable");
        goto error;
      }

      gst_bin_add (GST_BIN_CAST (data->pipeline), sink);

      sinkpad = gst_element_get_static_pad (queue, "sink");
      gst_pad_set_active (sinkpad, TRUE);

      gst_element_link_many (queue, convert, resample, sink, NULL);
      gst_pad_link (pad, sinkpad);
      gst_object_unref (sinkpad);
      gst_element_sync_state_with_parent (queue);
      gst_element_sync_state_with_parent (convert);
      gst_element_sync_state_with_parent (resample);
      gst_element_sync_state_with_parent (sink);

      break;
    }
    default:
      gst_println ("Ignore non video/audio stream %s (0x%x)",
          gst_stream_type_get_name (type), type);
      break;

  }

  gst_object_unref (stream);
  return;

error:
  gst_object_unref (stream);
  g_main_loop_quit (data->mainloop);

  return;
}

int
main (int argc, gchar ** argv)
{
  GstBus *bus;
  AppData *data;

  gst_init (&argc, &argv);

  if (argc < 2) {
    gst_print ("Usage: uridecodebin3 URI\n");
    return 1;
  }

  data = g_new0 (AppData, 1);

  data->pipeline = gst_pipeline_new ("pipeline");
  data->decodebin = gst_element_factory_make ("uridecodebin3", NULL);

  g_object_set (data->decodebin, "uri", argv[1], NULL);

  gst_bin_add (GST_BIN_CAST (data->pipeline), data->decodebin);

  g_signal_connect (data->decodebin, "pad-added",
      (GCallback) decodebin_pad_added_cb, data);
  data->mainloop = g_main_loop_new (NULL, FALSE);

  /* Put a bus handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (data->pipeline));
  gst_bus_set_sync_handler (bus, (GstBusSyncHandler) _on_bus_message, data,
      NULL);

  /* Start pipeline */
  gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
  g_main_loop_run (data->mainloop);

  gst_element_set_state (data->pipeline, GST_STATE_NULL);

  gst_object_unref (data->pipeline);
  gst_clear_object (&data->collection);
  gst_object_unref (bus);

  g_free (data);

  return 0;
}
