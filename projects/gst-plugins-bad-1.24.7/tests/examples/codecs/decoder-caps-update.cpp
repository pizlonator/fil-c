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
#include <string>
#include <mutex>
#include "../key-handler.h"

static GMainLoop *loop = nullptr;
static std::mutex input_lock;
static gint par = 1;
static gint fps = 30;
static gboolean set_hdr10 = FALSE;
static GstElement *setter = nullptr;
static gboolean updated = FALSE;

static void
print_keyboard_help (void)
{
  static struct
  {
    const gchar *key_desc;
    const gchar *key_help;
  } key_controls[] = {
    {
    "q", "Quit"}, {
    "right arrow", "Increase framerate"}, {
    "left arrow", "Decrease framerate"}, {
    "up arrow", "Increase pixel-aspect-ratio"}, {
    "down arrow", "Decrease pixel-aspect-ratio"}, {
    "m", "Toggle HDR10 metadata"}, {
    "k", "show keyboard shortcuts"}
  };

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
  std::lock_guard<std::mutex> lk (input_lock);

  if (!is_ascii) {
    switch (input) {
      case KB_ARROW_UP:
        par++;
        updated = TRUE;
        gst_println ("Increasing pixel-aspect-ratio to %d", par);
        break;
      case KB_ARROW_DOWN:
        if (par == 1)
          return;
        par--;
        updated = TRUE;
        gst_println ("Decreasing pixel-aspect-ratio to %d", par);
        break;
      case KB_ARROW_RIGHT:
        fps++;
        updated = TRUE;
        gst_println ("Increasing framerate to %d", fps);
        break;
      case KB_ARROW_LEFT:
        if (fps == 1)
          return;
        fps--;
        updated = TRUE;
        gst_println ("Decreasing framerate to %d", fps);
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
        g_main_loop_quit (loop);
        break;
      case 'm':
      case 'M':
        set_hdr10 = !set_hdr10;
        updated = TRUE;
        gst_println ("%sable HDR10 metadata", set_hdr10 ? "En" : "Dis");
        break;
      default:
        break;
    }
  }

  if (updated && setter) {
    GstPad *pad = gst_element_get_static_pad (setter, "sink");
    GstCaps *caps = gst_pad_get_current_caps (pad);
    gst_object_unref (pad);

    if (!caps)
      return;

    if (gst_caps_is_any (caps) || gst_caps_is_empty (caps)) {
      gst_caps_unref (caps);
      return;
    }

    caps = gst_caps_make_writable (caps);
    gst_caps_set_simple (caps, "pixel-aspect-ratio", GST_TYPE_FRACTION,
        par, 1, "framerate", GST_TYPE_FRACTION, fps, 1, nullptr);

    if (set_hdr10) {
      gst_caps_set_simple (caps, "mastering-display-info", G_TYPE_STRING,
          "34000:16000:13250:34500:7500:3000:15635:16450:10000000:1",
          "content-light-level", G_TYPE_STRING, "1000:400", nullptr);
    }

    g_object_set (setter, "caps", caps, nullptr);
    gst_caps_unref (caps);
  }
}

static void
decoder_caps_notify (GstPad * pad, GParamSpec * pspec, gpointer user_data)
{
  GstCaps *caps;
  gchar *caps_str;

  g_object_get (pad, "caps", &caps, nullptr);
  if (!caps)
    return;

  caps_str = gst_caps_to_string (caps);
  gst_println ("\nDecoder output caps\n%s\n", caps_str);

  g_free (caps_str);
  gst_caps_unref (caps);
}

static gboolean
bus_msg (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      gst_printerrln ("ERROR %s", err->message);
      if (dbg != NULL)
        gst_printerrln ("ERROR debug information: %s", dbg);
      g_clear_error (&err);
      g_free (dbg);

      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_EOS:
      gst_println ("Got EOS");
      g_main_loop_quit (loop);
      break;
    default:
      break;
  }

  return TRUE;
}

gint
main (gint argc, gchar ** argv)
{
  GstElement *pipeline;
  GError *error = nullptr;
  GOptionContext *option_ctx;
  gchar *decoder_name = nullptr;
  gchar *encoder_name = nullptr;
  gchar *sink_name = nullptr;
  gchar *location = nullptr;
  GOptionEntry options[] = {
    {"decoder", 0, 0, G_OPTION_ARG_STRING, &decoder_name,
        "Video decoder to use"},
    {"encoder", 0, 0, G_OPTION_ARG_STRING, &encoder_name,
        "Video encoder description. Ignored if \"location\" is set "
        "(example: \"x264enc speed-preset=ultrafast\""},
    {"videosink", 0, 0, G_OPTION_ARG_STRING, &sink_name,
        "Video sink to use"},
    {"location", 0, 0, G_OPTION_ARG_STRING, &location, "File location"},
    {nullptr}
  };

  option_ctx =
      g_option_context_new ("Video decoder caps update example");
  g_option_context_add_main_entries (option_ctx, options, nullptr);
  g_option_context_set_help_enabled (option_ctx, TRUE);
  if (!g_option_context_parse (option_ctx, &argc, &argv, &error)) {
    gst_printerrln ("option parsing failed: %s\n", error->message);
    g_clear_error (&error);
    exit (1);
  }

  g_option_context_free (option_ctx);
  gst_init (nullptr, nullptr);

  if (!decoder_name) {
    gst_printerrln ("Decoder must be specified");
    exit (1);
  }

  if (!encoder_name && !location) {
    gst_printerrln ("Encoder or file location must be specified");
    exit (1);
  }

  std::string pipeline_desc;
  if (location) {
    pipeline_desc = "filesrc location=" + std::string (location)
      + " ! parsebin ! capssetter name=setter ! "
      + std::string (decoder_name) + " name=dec";
  } else {
    pipeline_desc = "videotestsrc ! " + std::string (encoder_name)
      + " ! parsebin ! capssetter name=setter ! "
      + std::string (decoder_name) + " name=dec";
  }

  if (sink_name) {
    pipeline_desc += " ! " + std::string (sink_name);
  } else {
    pipeline_desc += " ! fakevideosink";
  }

  gst_println ("Constructing test pipeline \"%s\"", pipeline_desc.c_str());

  loop = g_main_loop_new (nullptr, FALSE);
  pipeline = gst_parse_launch (pipeline_desc.c_str(), &error);
  if (error) {
    gst_printerrln ("Could not construct pipeline, error: %s",
        error->message);
    exit(1);
  }

  setter = gst_bin_get_by_name (GST_BIN (pipeline), "setter");
  if (!setter) {
    gst_printerrln ("Could not get capssetter from pipeline");
    exit(1);
  }

  GstElement *dec = gst_bin_get_by_name (GST_BIN (pipeline), "dec");
  if (!dec) {
    gst_printerrln ("Could not get decoder from pipeline");
    exit(1);
  }

  GstPad *pad = gst_element_get_static_pad (dec, "src");
  g_signal_connect (pad, "notify::caps", G_CALLBACK (decoder_caps_notify),
      nullptr);
  gst_object_unref (pad);
  gst_object_unref (dec);

  gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), bus_msg, nullptr);

  /* run the pipeline */
  GstStateChangeReturn ret =
      gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    gst_printerrln ("Pipeline doesn't want to playing");
  } else {
    set_key_handler ((KeyInputCallback) keyboard_cb, nullptr);
    gst_println ("Press k to see supported keyboard inputs");
    g_main_loop_run (loop);
    unset_key_handler ();
  }

  input_lock.lock();
  gst_clear_object (&setter);
  input_lock.unlock();

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_watch (GST_ELEMENT_BUS (pipeline));

  gst_object_unref (pipeline);
  g_main_loop_unref (loop);

  g_free (decoder_name);
  g_free (encoder_name);
  g_free (location);

  return 0;
}
