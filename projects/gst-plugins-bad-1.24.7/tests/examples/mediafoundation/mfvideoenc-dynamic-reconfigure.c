/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

static GMainLoop *loop = NULL;
static gint width = 640;
static gint height = 480;
static guint bitrate = 1000;

G_LOCK_DEFINE_STATIC (input_lock);

typedef struct
{
  GstElement *pipeline;
  GstElement *capsfilter;
  GstElement *encoder;
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
    "up arrow", "Increase Height"}, {
    "down arrow", "Decrease Height"}, {
    ">", "Increase bitrate by 100 kbit/sec"}, {
    "<", "Decrease bitrate by 100 kbit/sec"}, {
    "k", "show keyboard shortcuts"}
  };
  /* *INDENT-ON* */

  guint i, chars_to_pad, desc_len, max_desc_len = 0;

  gst_print ("\n\n%s\n\n", "Keyboard controls:");

  for (i = 0; i < G_N_ELEMENTS (key_controls); ++i) {
    desc_len = g_utf8_strlen (key_controls[i].key_desc, -1);
    max_desc_len = MAX (max_desc_len, desc_len);
  }
  ++max_desc_len;

  for (i = 0; i < G_N_ELEMENTS (key_controls); ++i) {
    chars_to_pad = max_desc_len - g_utf8_strlen (key_controls[i].key_desc, -1);
    gst_print ("\t%s", key_controls[i].key_desc);
    gst_print ("%-*s: ", chars_to_pad, "");
    gst_print ("%s\n", key_controls[i].key_help);
  }
  gst_print ("\n");
}

static void
keyboard_cb (gchar input, gboolean is_ascii, gpointer user_data)
{
  TestCallbackData *data = (TestCallbackData *) user_data;

  G_LOCK (input_lock);

  if (!is_ascii) {
    switch (input) {
      case KB_ARROW_UP:
        height += 2;
        gst_print ("Increase height to %d\n", height);
        break;
      case KB_ARROW_DOWN:
        height -= 2;
        height = MAX (height, 16);
        gst_print ("Decrease height to %d\n", height);
        break;
      case KB_ARROW_LEFT:
        width -= 2;
        width = MAX (width, 16);
        gst_print ("Decrease width to %d\n", width);
        break;
      case KB_ARROW_RIGHT:
        width += 2;
        gst_print ("Increase width to %d\n", width);
        break;
      default:
        break;
    }
  } else {
    switch (input) {
      case 'k':
      case 'K':
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
        gst_print ("Increase bitrate to %d\n", bitrate);
        g_object_set (data->encoder, "bitrate", bitrate, NULL);
        break;
      case '<':
        bitrate -= 100;
        bitrate = MAX (bitrate, 100);
        gst_print ("Decrease bitrate to %d\n", bitrate);
        g_object_set (data->encoder, "bitrate", bitrate, NULL);
        break;
      default:
        break;
    }
  }

  G_UNLOCK (input_lock);
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
          gchar *dump_name = g_strconcat ("mfvideoenc.", state_transition_name,
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
          GST_DEBUG_GRAPH_SHOW_ALL, "mfvideoenc.error");

      gst_message_parse_error (msg, &err, &dbg);
      gst_printerr ("ERROR %s \n", err->message);
      if (dbg != NULL)
        gst_printerr ("ERROR debug information: %s\n", dbg);
      g_clear_error (&err);
      g_free (dbg);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static gboolean
check_mfvideoenc_available (const gchar * encoder_name)
{
  gboolean ret = TRUE;
  GstElement *elem;

  elem = gst_element_factory_make (encoder_name, NULL);
  if (!elem) {
    GST_WARNING ("%s is not available", encoder_name);
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

  return ret;
}

static GstPadProbeReturn
resolution_change_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;
  TestCallbackData *data = (TestCallbackData *) user_data;

  G_LOCK (input_lock);

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

  G_UNLOCK (input_lock);

  return ret;
}

gint
main (gint argc, gchar ** argv)
{
  GstElement *pipeline;
  GstElement *src, *capsfilter, *enc, *enc_queue, *dec, *parser, *queue, *sink;
  GstStateChangeReturn sret;
  GError *error = NULL;
  GOptionContext *option_ctx;
  GstCaps *caps;
  TestCallbackData data = { 0, };
  GstPad *pad;
  gchar *encoder_name = NULL;
  /* *INDENT-OFF* */
  GOptionEntry options[] = {
    {"encoder", 0, 0, G_OPTION_ARG_STRING, &encoder_name,
        "MediaFoundation encoder element to test, default: mfh264enc"},
    {NULL}
  };
  /* *INDENT-ON* */

#define MAKE_ELEMENT_AND_ADD(elem, name) G_STMT_START { \
  GstElement *_elem = gst_element_factory_make (name, NULL); \
  if (!_elem) { \
    gst_printerrln ("%s is not available", name); \
    exit (1); \
  } \
  gst_println ("Adding element %s", name); \
  elem = _elem; \
  gst_bin_add (GST_BIN (pipeline), elem); \
} G_STMT_END

  option_ctx =
      g_option_context_new
      ("MediaFoundation video encoder dynamic reconfigure example");
  g_option_context_add_main_entries (option_ctx, options, NULL);
  g_option_context_set_help_enabled (option_ctx, TRUE);
  if (!g_option_context_parse (option_ctx, &argc, &argv, &error)) {
    gst_printerrln ("option parsing failed: %s\n", error->message);
    g_clear_error (&error);
    exit (1);
  }

  g_option_context_free (option_ctx);
  gst_init (NULL, NULL);

  if (!encoder_name)
    encoder_name = g_strdup ("mfh264enc");

  if (!check_mfvideoenc_available (encoder_name)) {
    gst_printerrln ("Cannot load %s plugin", encoder_name);
    exit (1);
  }

  /* prepare the pipeline */
  loop = g_main_loop_new (NULL, FALSE);

  pipeline = gst_pipeline_new (NULL);

  MAKE_ELEMENT_AND_ADD (src, "videotestsrc");
  g_object_set (src, "pattern", 1, NULL);

  MAKE_ELEMENT_AND_ADD (capsfilter, "capsfilter");
  MAKE_ELEMENT_AND_ADD (enc, encoder_name);

  g_object_set (G_OBJECT (enc), "bitrate", bitrate, "gop-size", 30,
      "rc-mode", 0, NULL);

  MAKE_ELEMENT_AND_ADD (enc_queue, "queue");
  if (g_strrstr (encoder_name, "h265")) {
    MAKE_ELEMENT_AND_ADD (parser, "h265parse");
    MAKE_ELEMENT_AND_ADD (dec, "d3d11h265dec");
  } else if (g_strrstr (encoder_name, "vp9")) {
    MAKE_ELEMENT_AND_ADD (parser, "vp9parse");
    MAKE_ELEMENT_AND_ADD (dec, "d3d11vp9dec");
  } else {
    MAKE_ELEMENT_AND_ADD (parser, "h264parse");
    MAKE_ELEMENT_AND_ADD (dec, "d3d11h264dec");
  }
  MAKE_ELEMENT_AND_ADD (queue, "queue");
  MAKE_ELEMENT_AND_ADD (sink, "d3d11videosink");

  if (!gst_element_link_many (src, capsfilter, enc, enc_queue,
          parser, dec, queue, sink, NULL)) {
    gst_printerrln ("Failed to link element");
    exit (1);
  }

  caps = gst_caps_new_simple ("video/x-raw", "width", G_TYPE_INT,
      width, "height", G_TYPE_INT, height, NULL);
  g_object_set (capsfilter, "caps", caps, NULL);
  gst_caps_unref (caps);

  data.pipeline = pipeline;
  data.capsfilter = capsfilter;
  data.encoder = enc;

  pad = gst_element_get_static_pad (capsfilter, "src");
  data.probe_id = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) resolution_change_probe, &data, NULL);
  gst_object_unref (pad);
  data.prev_width = width;
  data.prev_height = height;

  gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), bus_msg, &data);

  /* run the pipeline */
  sret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (sret == GST_STATE_CHANGE_FAILURE) {
    gst_printerrln ("Pipeline doesn't want to playing\n");
  } else {
    set_key_handler ((KeyInputCallback) keyboard_cb, &data);
    g_main_loop_run (loop);
    unset_key_handler ();
  }

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_watch (GST_ELEMENT_BUS (pipeline));

  gst_object_unref (pipeline);
  g_main_loop_unref (loop);
  g_free (encoder_name);

  return 0;
}
