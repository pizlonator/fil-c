/* GStreamer
 *
 * Copyright (C) 2015 Thibault Saunier <tsaunier@gnome.org>
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

#include <string.h>

#include "utils.h"
#include <gst/transcoder/gsttranscoder.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

static const gchar *HELP_SUMMARY =
    "gst-transcoder-1.0 transcodes a stream defined by its first <input-uri>\n"
    "argument to the place defined by its second <output-uri> argument\n"
    "into the format described in its third <encoding-format> argument,\n"
    "or using the given <output-uri> file extension.\n"
    "\n"
    "The <encoding-format> argument:\n"
    "===============================\n"
    "\n"
    "If the encoding format is not defined, it will be guessed with\n"
    "the given <output-uri> file extension."
    "\n"
    "<encoding-format> describe the media format into which the\n"
    "input stream is going to be transcoded. We have two different\n"
    "ways of describing the format:\n"
    "\n"
    "GstEncodingProfile serialization format\n"
    "---------------------------------------\n"
    "\n"
    "GStreamer encoding profiles can be described with a quite extensive\n"
    "syntax which is described in the GstEncodingProfile documentation.\n"
    "\n"
    "The simple case looks like:\n"
    "\n"
    "    muxer_source_caps:videoencoder_source_caps:audioencoder_source_caps\n"
    "\n"
    "Name and category of serialized GstEncodingTarget\n"
    "-------------------------------------------------\n"
    "\n"
    "Encoding targets describe well known formats which\n"
    "those are provided in '.gep' files. You can list\n"
    "available ones using the `--list-targets` argument.\n";

typedef struct
{
  gint cpu_usage, rate;
  gboolean list;
  GstEncodingProfile *profile;
  gchar *src_uri, *dest_uri, *encoding_format, *size;
  gchar *framerate;
} Settings;

#ifdef G_OS_UNIX
static guint signal_watch_hup_id;
static guint signal_watch_intr_id;

static gboolean
intr_handler (gpointer user_data)
{
  GstTranscoder *self = GST_TRANSCODER (user_data);
  GstElement *pipeline = gst_transcoder_get_pipeline (self);

  g_print ("handling interrupt.\n");

  if (pipeline) {
    gst_element_send_event (pipeline, gst_event_new_eos ());
    g_object_unref (pipeline);
  }

  signal_watch_intr_id = 0;
  return G_SOURCE_REMOVE;
}

static gboolean
hup_handler (gpointer user_data)
{
  GstTranscoder *self = GST_TRANSCODER (user_data);
  GstElement *pipeline = gst_transcoder_get_pipeline (self);

  g_print ("handling hang up.\n");

  if (pipeline) {
    gst_element_send_event (pipeline, gst_event_new_eos ());
    g_object_unref (pipeline);
  }

  signal_watch_intr_id = 0;
  return G_SOURCE_REMOVE;
}
#endif /* G_OS_UNIX */

static void
position_updated_cb (GstTranscoder * transcoder, GstClockTime pos)
{
  GstClockTime dur = -1;
  gchar status[64] = { 0, };

  g_object_get (transcoder, "duration", &dur, NULL);

  memset (status, ' ', sizeof (status) - 1);

  if (pos != -1 && dur > 0 && dur != -1) {
    gchar dstr[32], pstr[32];

    /* FIXME: pretty print in nicer format */
    g_snprintf (pstr, 32, "%" GST_TIME_FORMAT, GST_TIME_ARGS (pos));
    pstr[9] = '\0';
    g_snprintf (dstr, 32, "%" GST_TIME_FORMAT, GST_TIME_ARGS (dur));
    dstr[9] = '\0';
    g_print ("%s / %s %s\r", pstr, dstr, status);
  }
}

static GList *
get_profiles_of_type (GstEncodingProfile * profile, GType profile_type)
{
  GList *tmp, *profiles = NULL;

  if (GST_IS_ENCODING_CONTAINER_PROFILE (profile)) {
    for (tmp = (GList *)
        gst_encoding_container_profile_get_profiles
        (GST_ENCODING_CONTAINER_PROFILE (profile)); tmp; tmp = tmp->next) {
      if (G_OBJECT_TYPE (tmp->data) == profile_type)
        profiles = g_list_prepend (profiles, tmp->data);
    }
  } else if (GST_IS_ENCODING_VIDEO_PROFILE (profile)) {
    profiles = g_list_prepend (profiles, profile);
  }

  return profiles;
}

static gboolean
set_video_settings (Settings * settings)
{
  GList *video_profiles, *tmp;
  gchar *p, *tmpstr, **vsize;
  gint width = 0, height = 0;
  GValue framerate = G_VALUE_INIT;

  if (!settings->size && !settings->framerate)
    return TRUE;

  if (settings->size) {
    p = tmpstr = g_strdup (settings->size);

    for (; *p; ++p)
      *p = g_ascii_tolower (*p);

    vsize = g_strsplit (tmpstr, "x", -1);
    g_free (tmpstr);

    if (!vsize[1] || vsize[2]) {
      g_strfreev (vsize);
      error ("Video size should be in the form: WxH, got %s", settings->size);

      return FALSE;
    }

    width = g_ascii_strtoull (vsize[0], NULL, 0);
    height = g_ascii_strtoull (vsize[1], NULL, 10);
    g_strfreev (vsize);
  }

  if (settings->framerate) {
    g_value_init (&framerate, GST_TYPE_FRACTION);
    if (!gst_value_deserialize (&framerate, settings->framerate)) {
      error ("Video framerate should be either a fraction or an integer"
          " not: %s", settings->framerate);
      return FALSE;
    }
  }

  video_profiles = get_profiles_of_type (settings->profile,
      GST_TYPE_ENCODING_VIDEO_PROFILE);
  for (tmp = video_profiles; tmp; tmp = tmp->next) {
    GstCaps *rest = gst_encoding_profile_get_restriction (tmp->data);

    if (!rest)
      rest = gst_caps_new_empty_simple ("video/x-raw");
    else
      rest = gst_caps_copy (rest);

    if (settings->size) {
      gst_caps_set_simple (rest, "width", G_TYPE_INT, width,
          "height", G_TYPE_INT, height, NULL);
    }
    if (settings->framerate)
      gst_caps_set_value (rest, "framerate", &framerate);

    gst_encoding_profile_set_restriction (tmp->data, rest);
  }

  return TRUE;
}

static gboolean
set_audio_settings (Settings * settings)
{
  GList *audio_profiles, *tmp;

  if (settings->rate < 0)
    return TRUE;

  audio_profiles =
      get_profiles_of_type (settings->profile, GST_TYPE_ENCODING_AUDIO_PROFILE);
  for (tmp = audio_profiles; tmp; tmp = tmp->next) {
    GstCaps *rest = gst_encoding_profile_get_restriction (tmp->data);

    if (!rest)
      rest = gst_caps_new_empty_simple ("audio/x-raw");
    else
      rest = gst_caps_copy (rest);

    gst_caps_set_simple (rest, "rate", G_TYPE_INT, settings->rate, NULL);
    gst_encoding_profile_set_restriction (tmp->data, rest);
  }

  return TRUE;
}

static void
list_encoding_targets (void)
{
  GList *tmp, *targets = gst_encoding_list_all_targets (NULL);

  for (tmp = targets; tmp; tmp = tmp->next) {
    GstEncodingTarget *target = tmp->data;
    GList *usable_profiles = get_usable_profiles (target);

    if (usable_profiles) {
      GList *tmpprof;

      g_print ("\n%s (%s): %s\n * Profiles:\n",
          gst_encoding_target_get_name (target),
          gst_encoding_target_get_category (target),
          gst_encoding_target_get_description (target));

      for (tmpprof = usable_profiles; tmpprof; tmpprof = tmpprof->next)
        g_print ("     - %s: %s",
            gst_encoding_profile_get_name (tmpprof->data),
            gst_encoding_profile_get_description (tmpprof->data));

      g_print ("\n");
      g_list_free (usable_profiles);
    }
  }

  g_list_free_full (targets, (GDestroyNotify) g_object_unref);
}

static void
_error_cb (GstTranscoder * transcoder, GError * err, GstStructure * details)
{
  if (g_error_matches (err, GST_CORE_ERROR, GST_CORE_ERROR_PAD)) {
    GstPadLinkReturn lret;
    GType type;

    if (details && gst_structure_get (details, "linking-error",
            GST_TYPE_PAD_LINK_RETURN, &lret,
            "msg-source-type", G_TYPE_GTYPE, &type, NULL) &&
        type == g_type_from_name ("GstTranscodeBin")) {
      const gchar *debug = gst_structure_get_string (details, "debug");

      error ("\nCould not setup transcoding pipeline,"
          " make sure that your transcoding format parameters"
          " are compatible with the input stream.\n\n%s", debug);
      return;
    }
  }

  error ("\nFAILURE: %s", err->message);
}

static void
_warning_cb (GstTranscoder * transcoder, GError * error, GstStructure * details)
{
  gboolean cant_encode;
  GstCaps *caps = NULL;
  gchar *stream_id = NULL;

  if (details && gst_structure_get (details, "can-t-encode-stream",
          G_TYPE_BOOLEAN, &cant_encode, "stream-caps", GST_TYPE_CAPS,
          &caps, "stream-id", G_TYPE_STRING, &stream_id, NULL)) {
    gchar *source_uri = gst_transcoder_get_source_uri (transcoder);

    warn ("WARNING: Input stream %s: WON'T BE ENCODED.\n"
        "Make sure the encoding settings are valid and that"
        " any preset you set actually exists.\n"
        "For more information about that stream, you can inspect"
        " the source stream with:\n\n"
        "    gst-discoverer-1.0 -v %s\n", stream_id, source_uri);
    gst_caps_unref (caps);
    g_free (stream_id);;
    g_free (source_uri);;

    return;
  }
  warn ("Got warning: %s", error->message);
}

static int
real_main (int argc, char *argv[])
{
  gint res = 0;
  GError *err = NULL;
  GstTranscoder *transcoder;
  GOptionContext *ctx;
  GstTranscoderSignalAdapter *signal_adapter;
  Settings settings = {
    .cpu_usage = 100,
    .rate = -1,
    .encoding_format = NULL,
    .size = NULL,
    .framerate = NULL,
  };
  GOptionEntry options[] = {
    {"cpu-usage", 'c', 0, G_OPTION_ARG_INT, &settings.cpu_usage,
        "The CPU usage to target in the transcoding process", NULL},
    {"list-targets", 'l', G_OPTION_ARG_NONE, 0, &settings.list,
        "List all encoding targets", NULL},
    {"size", 's', 0, G_OPTION_ARG_STRING, &settings.size,
        "set frame size (WxH or abbreviation)", NULL},
    {"audio-rate", 'r', 0, G_OPTION_ARG_INT, &settings.rate,
        "set audio sampling rate (in Hz)", NULL},
    {"framerate", 'f', 0, G_OPTION_ARG_STRING, &settings.framerate,
        "set video framerate as a fraction (24/1 for 24fps)"
          " or a single number (24 for 24fps))", NULL},
    {"video-encoder", 'v', 0, G_OPTION_ARG_STRING, &settings.size,
        "The video encoder to use.", NULL},
    {NULL}
  };

  g_set_prgname ("gst-transcoder");

  ctx = g_option_context_new ("<source uri> <destination uri> "
      "[<encoding format>[/<encoding profile name>]]");
  g_option_context_set_summary (ctx, HELP_SUMMARY);

  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());

#ifdef G_OS_WIN32
  if (!g_option_context_parse_strv (ctx, &argv, &err))
#else
  if (!g_option_context_parse (ctx, &argc, &argv, &err))
#endif
  {
    g_print ("Error initializing: %s\n", GST_STR_NULL (err->message));
    g_clear_error (&err);
    g_option_context_free (ctx);
    return 1;
  }
#ifdef G_OS_WIN32
  argc = g_strv_length (argv);
#endif

  gst_pb_utils_init ();

  if (settings.list) {
    list_encoding_targets ();
    return 0;
  }

  if (argc < 3 || argc > 4) {
    g_print ("%s", g_option_context_get_help (ctx, TRUE, NULL));
    g_option_context_free (ctx);

    return -1;
  }
  g_option_context_free (ctx);

  settings.src_uri = ensure_uri (argv[1]);
  settings.dest_uri = ensure_uri (argv[2]);

  if (argc == 3) {
    settings.encoding_format = get_file_extension (settings.dest_uri);

    if (!settings.encoding_format)
      goto no_extension;
  } else {
    settings.encoding_format = argv[3];
  }

  settings.profile = create_encoding_profile (settings.encoding_format);

  if (!settings.profile) {
    error ("Could not find any encoding format for %s\n",
        settings.encoding_format);
    warn ("You can list available targets using %s --list-targets", argv[0]);
    res = 1;
    goto done;
  }

  g_print ("Encoding to:\n\n");
  describe_encoding_profile (settings.profile);
  if (!set_video_settings (&settings)) {
    res = -1;
    goto done;
  }

  if (!set_audio_settings (&settings)) {
    res = -1;
    goto done;
  }

  transcoder = gst_transcoder_new_full (settings.src_uri, settings.dest_uri,
      settings.profile);
  gst_transcoder_set_avoid_reencoding (transcoder, TRUE);
  gst_transcoder_set_cpu_usage (transcoder, settings.cpu_usage);

  signal_adapter = gst_transcoder_get_signal_adapter (transcoder, NULL);
  g_signal_connect_swapped (signal_adapter, "position-updated",
      G_CALLBACK (position_updated_cb), transcoder);
  g_signal_connect_swapped (signal_adapter, "warning", G_CALLBACK (_warning_cb),
      transcoder);
  g_signal_connect_swapped (signal_adapter, "error", G_CALLBACK (_error_cb),
      transcoder);


#ifdef G_OS_UNIX
  signal_watch_intr_id =
      g_unix_signal_add (SIGINT, (GSourceFunc) intr_handler, transcoder);
  signal_watch_hup_id =
      g_unix_signal_add (SIGHUP, (GSourceFunc) hup_handler, transcoder);
#endif

  ok ("Starting transcoding...");
  gst_transcoder_run (transcoder, &err);
  g_object_unref (signal_adapter);
  if (!err)
    ok ("\nDONE.");

#ifdef G_OS_UNIX
  if (signal_watch_intr_id > 0)
    g_source_remove (signal_watch_intr_id);
  if (signal_watch_hup_id > 0)
    g_source_remove (signal_watch_hup_id);
#endif

done:
  g_free (settings.dest_uri);
  g_free (settings.src_uri);

  return res;

no_extension:
  error ("No <encoding-format> specified and no extension"
      " available in the output target: %s", settings.dest_uri);
  res = 1;

  goto done;
}

int
main (int argc, char *argv[])
{
  int ret;

#ifdef G_OS_WIN32
  argv = g_win32_get_command_line ();
#endif

#if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
  ret = gst_macos_main ((GstMainFunc) real_main, argc, argv, NULL);
#else
  ret = real_main (argc, argv);
#endif

#ifdef G_OS_WIN32
  g_strfreev (argv);
#endif

  return ret;
}
