/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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
#include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <stdlib.h>

#include "../key-handler.h"

#define DEFAULT_VIDEO_SINK "autovideosink"

static GMainLoop *loop = NULL;
static gint width = 320;
static gint height = 240;
static gint bitrate = 2000;

typedef struct
{
  GstElement *pipeline;
  GstElement *capsfilter;
  GstElement *nvenc;
  gulong probe_id;

  gint prev_width;
  gint prev_height;
} TestCallbackData;

static void
print_keyboard_help (void)
{
  /* *INDENT-OFF* */
  static struct
  {
    const gchar *key_desc;
    const gchar *key_help;
  } key_controls[] = {
    {
    "q", "Quit"}, {
    "right arrow", "Increase Width"}, {
    "left arrow", "Decrease Width"}, {
    "up arrow", "Increase Height"} , {
    "down arrow", "Decrease Height"}, {
    ">", "Increase encoding bitrate by 100 kbit/sec"}, {
    "<", "Decrease encoding bitrate by 100 kbit/sec"}, {
    "k", "show keyboard shortcuts"}
  };
  /* *INDENT-ON* */

  guint i, chars_to_pad, desc_len, max_desc_len = 0;

  g_print ("\n\n%s\n\n", "Keyboard controls:");

  for (i = 0; i < G_N_ELEMENTS (key_controls); ++i) {
    desc_len = g_utf8_strlen (key_controls[i].key_desc, -1);
    max_desc_len = MAX (max_desc_len, desc_len);
  }
  ++max_desc_len;

  for (i = 0; i < G_N_ELEMENTS (key_controls); ++i) {
    chars_to_pad = max_desc_len - g_utf8_strlen (key_controls[i].key_desc, -1);
    g_print ("\t%s", key_controls[i].key_desc);
    g_print ("%-*s: ", chars_to_pad, "");
    g_print ("%s\n", key_controls[i].key_help);
  }
  g_print ("\n");
}

static void
keyboard_cb (gchar input, gboolean is_ascii, gpointer user_data)
{
  TestCallbackData *data = (TestCallbackData *) user_data;

  if (is_ascii) {
    switch (input) {
      case 'k':
        print_keyboard_help ();
        break;
      case 'q':
      case 'Q':
        gst_element_send_event (data->pipeline, gst_event_new_eos ());
        g_main_loop_quit (loop);
        break;
      case '>':
        bitrate += 100;
        bitrate = MIN (bitrate, 2048000);
        g_print ("Increase encoding bitrate to %d\n", bitrate);
        g_object_set (G_OBJECT (data->nvenc), "bitrate", bitrate, NULL);
        break;
      case '<':
        bitrate -= 100;
        bitrate = MAX (bitrate, 100);
        g_print ("Decrease encoding bitrate to %d\n", bitrate);
        g_object_set (G_OBJECT (data->nvenc), "bitrate", bitrate, NULL);
        break;
      default:
        break;
    }

    return;
  }

  switch (input) {
    case KB_ARROW_RIGHT:
      g_print ("Increase width to %d\n", ++width);
      break;
    case KB_ARROW_LEFT:
      g_print ("Decrease width to %d\n", --width);
      break;
    case KB_ARROW_UP:
      g_print ("Increase height to %d\n", ++height);
      break;
    case KB_ARROW_DOWN:
      g_print ("Decrease height to %d\n", --height);
      break;
    default:
      break;
  }
}

static gboolean
bus_msg (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  TestCallbackData *data = (TestCallbackData *) user_data;
  GstElement *pipeline = data->pipeline;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_MESSAGE_SRC (msg) == GST_OBJECT_CAST (pipeline)) {
        gchar *state_transition_name;
        GstState old, new, pending;

        gst_message_parse_state_changed (msg, &old, &new, &pending);

        state_transition_name = g_strdup_printf ("%s_%s",
            gst_element_state_get_name (old), gst_element_state_get_name (new));

        /* dump graph for (some) pipeline state changes */
        {
          gchar *dump_name = g_strconcat ("nvcodec.", state_transition_name,
              NULL);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
              GST_DEBUG_GRAPH_SHOW_ALL, dump_name);
          g_free (dump_name);
        }
        g_free (state_transition_name);
      }
      break;
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "nvcodec.error");

      gst_message_parse_error (msg, &err, &dbg);
      g_printerr ("ERROR %s \n", err->message);
      if (dbg != NULL)
        g_printerr ("ERROR debug information: %s\n", dbg);
      g_clear_error (&err);
      g_free (dbg);

      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_ELEMENT:
    {
      GstNavigationMessageType mtype = gst_navigation_message_get_type (msg);
      if (mtype == GST_NAVIGATION_MESSAGE_EVENT) {
        GstEvent *ev = NULL;

        if (gst_navigation_message_parse_event (msg, &ev)) {
          GstNavigationEventType e_type = gst_navigation_event_get_type (ev);
          if (e_type == GST_NAVIGATION_EVENT_KEY_PRESS) {
            const gchar *key;
            gchar val = 0;

            if (gst_navigation_event_parse_key_event (ev, &key)) {
              gboolean ascii = TRUE;

              GST_INFO ("Key press: %s", key);

              val = key[0];

              if (g_strcmp0 (key, "Left") == 0) {
                val = KB_ARROW_LEFT;
                ascii = FALSE;
              } else if (g_strcmp0 (key, "Right") == 0) {
                val = KB_ARROW_RIGHT;
                ascii = FALSE;
              } else if (g_strcmp0 (key, "Up") == 0) {
                val = KB_ARROW_UP;
                ascii = FALSE;
              } else if (g_strcmp0 (key, "Down") == 0) {
                val = KB_ARROW_DOWN;
                ascii = FALSE;
              } else if (strlen (key) > 1) {
                break;
              }

              keyboard_cb (val, ascii, user_data);
            }
          }
        }
        if (ev)
          gst_event_unref (ev);
      }
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static gboolean
check_nvcodec_available (const gchar * encoder_name)
{
  gboolean ret = TRUE;
  GstElement *elem;

  elem = gst_element_factory_make (encoder_name, NULL);
  if (!elem) {
    GST_WARNING ("%s is not available, possibly driver load failure",
        encoder_name);
    return FALSE;
  }

  /* GST_STATE_READY is meaning that driver could be loaded */
  if (gst_element_set_state (elem,
          GST_STATE_PAUSED) != GST_STATE_CHANGE_SUCCESS) {
    GST_WARNING ("cannot open device");
    ret = FALSE;
  }

  gst_element_set_state (elem, GST_STATE_NULL);
  gst_object_unref (elem);

  if (ret) {
    elem = gst_element_factory_make ("nvh264dec", NULL);
    if (!elem) {
      GST_WARNING ("nvh264dec is not available, possibly driver load failure");
      return FALSE;
    }

    /* GST_STATE_READY is meaning that driver could be loaded */
    if (gst_element_set_state (elem,
            GST_STATE_PAUSED) != GST_STATE_CHANGE_SUCCESS) {
      GST_WARNING ("cannot open device");
      ret = FALSE;
    }

    gst_element_set_state (elem, GST_STATE_NULL);
    gst_object_unref (elem);
  }

  return ret;
}

static GstPadProbeReturn
resolution_change_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;
  TestCallbackData *data = (TestCallbackData *) user_data;

  if (GST_IS_BUFFER (GST_PAD_PROBE_INFO_DATA (info))) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);
    GstPad *peer = gst_pad_get_peer (pad);
    GstFlowReturn flow_ret = GST_FLOW_OK;

    ret = GST_PAD_PROBE_HANDLED;

    if (peer) {
      flow_ret = gst_pad_chain (peer, buffer);

      if (flow_ret != GST_FLOW_OK) {
        gst_pad_remove_probe (pad, data->probe_id);
        data->probe_id = 0;
      } else {
        if (data->prev_width != width || data->prev_height != height) {
          GstCaps *caps = NULL;
          gint next_width, next_height;

          next_width = width;
          next_height = height;

          g_object_get (data->capsfilter, "caps", &caps, NULL);
          caps = gst_caps_make_writable (caps);
          gst_caps_set_simple (caps,
              "width", G_TYPE_INT, next_width, "height", G_TYPE_INT,
              next_height, NULL);
          g_object_set (data->capsfilter, "caps", caps, NULL);
          gst_caps_unref (caps);

          data->prev_width = next_width;
          data->prev_height = next_height;
        }
      }
    }
  }

  return ret;
}

gint
main (gint argc, gchar ** argv)
{
  GstElement *pipeline, *src, *convert, *capsfilter, *queue, *sink, *parse;
  GstElement *enc, *dec;
  GstStateChangeReturn sret;
  GError *error = NULL;
  gboolean use_gl = FALSE;
  gint exitcode = 1;
  GOptionContext *option_ctx;
  GstCaps *caps;
  TestCallbackData data = { 0, };
  GstPad *pad;
  gchar *encoder_name = NULL;
  /* *INDENT-OFF* */
  GOptionEntry options[] = {
    {"use-gl", 0, 0, G_OPTION_ARG_NONE, &use_gl,
        "Use OpenGL memory as input to the nvenc", NULL},
    {"encoder", 0, 0, G_OPTION_ARG_STRING, &encoder_name,
        "NVENC encoder element to test, default: nvh264enc"},
    {NULL}
  };
  /* *INDENT-ON* */

  option_ctx = g_option_context_new ("nvcodec dynamic reconfigure example");
  g_option_context_add_main_entries (option_ctx, options, NULL);
  g_option_context_set_help_enabled (option_ctx, TRUE);
  if (!g_option_context_parse (option_ctx, &argc, &argv, &error)) {
    g_printerr ("option parsing failed: %s\n", error->message);
    g_clear_error (&error);
    exit (1);
  }

  g_option_context_free (option_ctx);
  gst_init (NULL, NULL);

  if (!encoder_name)
    encoder_name = g_strdup ("nvh264enc");

  if (!check_nvcodec_available (encoder_name)) {
    g_printerr ("Cannot load nvcodec plugin");
    exit (1);
  }

  /* prepare the pipeline */
  loop = g_main_loop_new (NULL, FALSE);

  pipeline = gst_pipeline_new ("nvcodec-example");

  if (use_gl)
    src = gst_element_factory_make ("gltestsrc", NULL);
  else
    src = gst_element_factory_make ("videotestsrc", NULL);

  if (!src) {
    g_printerr ("%s element is not available\n",
        use_gl ? "gltestsrc" : "videotestsrc");
    goto terminate;
  }

  gst_bin_add (GST_BIN (pipeline), src);

  if (use_gl)
    convert = gst_element_factory_make ("glcolorconvert", NULL);
  else
    convert = gst_element_factory_make ("videoconvert", NULL);

  if (!convert) {
    g_printerr ("%s element is not available\n",
        use_gl ? "glcolorconvert" : "videoconvert");
    goto terminate;
  }

  gst_bin_add (GST_BIN (pipeline), convert);

  if (use_gl) {
    sink = gst_element_factory_make ("glimagesink", NULL);
  } else {
    sink = gst_element_factory_make (DEFAULT_VIDEO_SINK, NULL);
  }

  if (!sink) {
    g_printerr ("%s element is not available\n",
        use_gl ? "glimagesink" : DEFAULT_VIDEO_SINK);
    goto terminate;
  }

  gst_bin_add (GST_BIN (pipeline), sink);

  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  queue = gst_element_factory_make ("queue", NULL);
  enc = gst_element_factory_make (encoder_name, NULL);
  parse = gst_element_factory_make ("h264parse", NULL);

  g_object_set (G_OBJECT (enc), "bitrate", bitrate, NULL);

  dec = gst_element_factory_make ("nvh264dec", NULL);

  gst_bin_add_many (GST_BIN (pipeline), capsfilter, queue, enc, parse, dec,
      NULL);

  if (!use_gl) {
    GstElement *sink_convert = gst_element_factory_make ("videoconvert", NULL);
    gst_bin_add (GST_BIN (pipeline), sink_convert);

    gst_element_link_many (src,
        convert, capsfilter, enc, parse, dec, queue, sink_convert, sink, NULL);
  } else {
    gst_element_link_many (src,
        convert, capsfilter, enc, parse, dec, queue, sink, NULL);
  }

  caps = gst_caps_from_string ("video/x-raw,format=NV12");

  if (use_gl) {
    gst_caps_set_features_simple (caps,
        gst_caps_features_from_string ("memory:GLMemory"));
  }

  g_object_set (G_OBJECT (capsfilter), "caps", caps, NULL);
  gst_caps_unref (caps);

  data.pipeline = pipeline;
  data.capsfilter = capsfilter;
  data.nvenc = enc;

  pad = gst_element_get_static_pad (convert, "src");
  data.probe_id = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) resolution_change_probe, &data, NULL);
  gst_object_unref (pad);
  data.prev_width = width;
  data.prev_height = height;

  gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), bus_msg, &data);

  set_key_handler (keyboard_cb, &data);
  g_print ("Press 'k' to see a list of keyboard shortcuts.\n");
  atexit (unset_key_handler);

  /* run the pipeline */
  sret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (sret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Pipeline doesn't want to playing\n");
  } else {
    g_main_loop_run (loop);
  }

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_watch (GST_ELEMENT_BUS (pipeline));

  exitcode = 0;

terminate:

  gst_object_unref (pipeline);
  g_main_loop_unref (loop);
  g_free (encoder_name);

  return exitcode;
}
