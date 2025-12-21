/* GStreamer
 * Copyright (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
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

#include <locale.h>

#include <stdlib.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/audio/audio.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#define MAX_INDENT 40

/* *INDENT-OFF* */
static void my_g_string_append_printf (GString * str, int depth, const gchar * format, ...) G_GNUC_PRINTF (3, 4);
/* *INDENT-ON* */

static gboolean async = FALSE;
static gboolean show_toc = FALSE;
static gboolean verbose = FALSE;

typedef struct
{
  GstDiscoverer *dc;
  int argc;
  char **argv;
} PrivStruct;

static gboolean
structure_remove_buffers_ip (GQuark field_id, GValue * value,
    gpointer user_data)
{
  if (G_VALUE_HOLDS (value, GST_TYPE_BUFFER))
    return FALSE;

  if (GST_VALUE_HOLDS_ARRAY (value)) {
    gint i;

    for (i = 0; i < gst_value_array_get_size (value); i++) {
      if (structure_remove_buffers_ip (0,
              (GValue *) gst_value_array_get_value (value, i), user_data))
        return TRUE;
    }

    return FALSE;
  }
  return TRUE;
}

static gboolean
caps_remove_buffers_ip (GstCapsFeatures * features, GstStructure * structure,
    gpointer user_data)
{
  gst_structure_filter_and_map_in_place (structure,
      structure_remove_buffers_ip, NULL);

  return TRUE;
}

static void
my_g_string_append_printf (GString * str, int depth, const gchar * format, ...)
{
  va_list args;

  while (depth-- > 0) {
    g_string_append (str, "  ");
  }

  va_start (args, format);
  g_string_append_vprintf (str, format, args);
  va_end (args);
}

static gchar *
caps_to_string (GstCaps * caps)
{
  gchar *res = NULL;

  if (verbose) {
    res = gst_caps_to_string (caps);
    goto done;
  }

  caps = gst_caps_make_writable (caps);

  gst_caps_map_in_place (caps, caps_remove_buffers_ip, NULL);
  res = gst_caps_to_string (caps);

done:
  gst_caps_unref (caps);
  return res;
}

static void
gst_stream_information_to_string (GstDiscovererStreamInfo * info, GString * s,
    guint depth)
{
  gchar *tmp;
  GstCaps *caps;
#ifndef GST_DISABLE_DEPRECATED
  const GstStructure *misc;
#endif

  if (verbose) {
    my_g_string_append_printf (s, depth, "Codec:\n");
    caps = gst_discoverer_stream_info_get_caps (info);
    tmp = caps_to_string (caps);
    my_g_string_append_printf (s, depth, "  %s\n", tmp);
    g_free (tmp);
  }
#ifndef GST_DISABLE_DEPRECATED
  if (verbose) {
    misc = gst_discoverer_stream_info_get_misc (info);
    if (misc) {
      my_g_string_append_printf (s, depth, "Additional info:\n");
      tmp = gst_structure_to_string (misc);
      my_g_string_append_printf (s, depth, "  %s\n", tmp);
      g_free (tmp);
    }
  }
#endif

  my_g_string_append_printf (s, depth, "Stream ID: %s\n",
      gst_discoverer_stream_info_get_stream_id (info));
}

static void
print_tag_foreach (const GstTagList * tags, const gchar * tag,
    gpointer user_data)
{
  GValue val = { 0, };
  gchar *str;
  guint depth = GPOINTER_TO_UINT (user_data);

  if (!gst_tag_list_copy_value (&val, tags, tag))
    return;

  if (G_VALUE_HOLDS_STRING (&val)) {
    str = g_value_dup_string (&val);
  } else if (G_VALUE_TYPE (&val) == GST_TYPE_SAMPLE) {
    GstSample *sample = gst_value_get_sample (&val);
    GstBuffer *img = gst_sample_get_buffer (sample);
    GstCaps *caps = gst_sample_get_caps (sample);

    if (img) {
      if (caps) {
        gchar *caps_str;

        caps_str = caps_to_string (gst_caps_ref (caps));
        str = g_strdup_printf ("buffer of %" G_GSIZE_FORMAT " bytes, "
            "type: %s", gst_buffer_get_size (img), caps_str);
        g_free (caps_str);
      } else {
        str = g_strdup_printf ("buffer of %" G_GSIZE_FORMAT " bytes",
            gst_buffer_get_size (img));
      }
    } else {
      str = g_strdup ("NULL buffer");
    }
  } else {
    str = gst_value_serialize (&val);
  }

  g_print ("%*s%s: %s\n", 2 * depth, " ", gst_tag_get_nick (tag), str);
  g_free (str);

  g_value_unset (&val);
}

static void
print_tags_topology (guint depth, const GstTagList * tags)
{
  if (!verbose)
    return;

  g_print ("%*sTags:\n", 2 * depth, " ");
  if (tags) {
    gst_tag_list_foreach (tags, print_tag_foreach,
        GUINT_TO_POINTER (depth + 1));
  } else {
    g_print ("%*sNone\n", 2 * (depth + 1), " ");
  }
  g_print ("%*s\n", 2 * depth, " ");
}

static gchar *
format_channel_mask (GstDiscovererAudioInfo * ainfo)
{
  GString *s = g_string_sized_new (32);
  GstAudioChannelPosition position[64];
  guint channels = gst_discoverer_audio_info_get_channels (ainfo);
  GEnumClass *enum_class = g_type_class_ref (GST_TYPE_AUDIO_CHANNEL_POSITION);
  guint i;
  guint64 channel_mask;

  if (channels == 0)
    goto done;

  channel_mask = gst_discoverer_audio_info_get_channel_mask (ainfo);

  if (channel_mask != 0) {
    gst_audio_channel_positions_from_mask (channels, channel_mask, position);

    for (i = 0; i < channels; i++) {
      GEnumValue *value = g_enum_get_value (enum_class, position[i]);
      my_g_string_append_printf (s, 0, "%s%s", value->value_nick,
          i + 1 == channels ? "" : ", ");
    }
  } else {
    g_string_append (s, "unknown layout");
  }

  g_type_class_unref (enum_class);

done:
  return g_string_free (s, FALSE);
}

static gchar *
gst_stream_audio_information_to_string (GstDiscovererStreamInfo * info,
    guint depth)
{
  GstDiscovererAudioInfo *audio_info;
  GString *s;
  const gchar *ctmp;
  int len = 400;
  const GstTagList *tags;
  gchar *channel_positions;

  g_return_val_if_fail (info != NULL, NULL);

  s = g_string_sized_new (len);

  gst_stream_information_to_string (info, s, depth);

  audio_info = (GstDiscovererAudioInfo *) info;
  ctmp = gst_discoverer_audio_info_get_language (audio_info);
  my_g_string_append_printf (s, depth, "Language: %s\n",
      ctmp ? ctmp : "<unknown>");

  channel_positions = format_channel_mask (audio_info);
  my_g_string_append_printf (s, depth, "Channels: %u (%s)\n",
      gst_discoverer_audio_info_get_channels (audio_info), channel_positions);
  g_free (channel_positions);

  my_g_string_append_printf (s, depth, "Sample rate: %u\n",
      gst_discoverer_audio_info_get_sample_rate (audio_info));
  my_g_string_append_printf (s, depth, "Depth: %u\n",
      gst_discoverer_audio_info_get_depth (audio_info));

  my_g_string_append_printf (s, depth, "Bitrate: %u\n",
      gst_discoverer_audio_info_get_bitrate (audio_info));
  my_g_string_append_printf (s, depth, "Max bitrate: %u\n",
      gst_discoverer_audio_info_get_max_bitrate (audio_info));

  tags = gst_discoverer_stream_info_get_tags (info);
  print_tags_topology (depth, tags);

  return g_string_free (s, FALSE);
}

static gchar *
gst_stream_video_information_to_string (GstDiscovererStreamInfo * info,
    guint depth)
{
  GstDiscovererVideoInfo *video_info;
  GString *s;
  int len = 500;
  const GstTagList *tags;

  g_return_val_if_fail (info != NULL, NULL);

  s = g_string_sized_new (len);

  gst_stream_information_to_string (info, s, depth);

  video_info = (GstDiscovererVideoInfo *) info;
  my_g_string_append_printf (s, depth, "Width: %u\n",
      gst_discoverer_video_info_get_width (video_info));
  my_g_string_append_printf (s, depth, "Height: %u\n",
      gst_discoverer_video_info_get_height (video_info));
  my_g_string_append_printf (s, depth, "Depth: %u\n",
      gst_discoverer_video_info_get_depth (video_info));

  my_g_string_append_printf (s, depth, "Frame rate: %u/%u\n",
      gst_discoverer_video_info_get_framerate_num (video_info),
      gst_discoverer_video_info_get_framerate_denom (video_info));

  my_g_string_append_printf (s, depth, "Pixel aspect ratio: %u/%u\n",
      gst_discoverer_video_info_get_par_num (video_info),
      gst_discoverer_video_info_get_par_denom (video_info));

  my_g_string_append_printf (s, depth, "Interlaced: %s\n",
      gst_discoverer_video_info_is_interlaced (video_info) ? "true" : "false");

  my_g_string_append_printf (s, depth, "Bitrate: %u\n",
      gst_discoverer_video_info_get_bitrate (video_info));
  my_g_string_append_printf (s, depth, "Max bitrate: %u\n",
      gst_discoverer_video_info_get_max_bitrate (video_info));

  tags = gst_discoverer_stream_info_get_tags (info);
  print_tags_topology (depth, tags);

  return g_string_free (s, FALSE);
}

static gchar *
gst_stream_subtitle_information_to_string (GstDiscovererStreamInfo * info,
    guint depth)
{
  GstDiscovererSubtitleInfo *subtitle_info;
  GString *s;
  const gchar *ctmp;
  int len = 400;
  const GstTagList *tags;

  g_return_val_if_fail (info != NULL, NULL);

  s = g_string_sized_new (len);

  gst_stream_information_to_string (info, s, depth);

  subtitle_info = (GstDiscovererSubtitleInfo *) info;
  ctmp = gst_discoverer_subtitle_info_get_language (subtitle_info);
  my_g_string_append_printf (s, depth, "Language: %s\n",
      ctmp ? ctmp : "<unknown>");

  tags = gst_discoverer_stream_info_get_tags (info);
  print_tags_topology (depth, tags);

  return g_string_free (s, FALSE);
}

static void
print_stream_info (GstDiscovererStreamInfo * info, void *depth)
{
  gchar *desc = NULL;
  GstCaps *caps;

  caps = gst_discoverer_stream_info_get_caps (info);

  if (caps) {
    if (gst_caps_is_fixed (caps) && !verbose)
      desc = gst_pb_utils_get_codec_description (caps);
    else
      desc = caps_to_string (gst_caps_ref (caps));
    gst_caps_unref (caps);
  }

  g_print ("%*s%s #%d: %s\n", 2 * GPOINTER_TO_INT (depth), " ",
      gst_discoverer_stream_info_get_stream_type_nick (info),
      gst_discoverer_stream_info_get_stream_number (info), GST_STR_NULL (desc));

  if (desc) {
    g_free (desc);
    desc = NULL;
  }

  if (GST_IS_DISCOVERER_AUDIO_INFO (info))
    desc =
        gst_stream_audio_information_to_string (info,
        GPOINTER_TO_INT (depth) + 1);
  else if (GST_IS_DISCOVERER_VIDEO_INFO (info))
    desc =
        gst_stream_video_information_to_string (info,
        GPOINTER_TO_INT (depth) + 1);
  else if (GST_IS_DISCOVERER_SUBTITLE_INFO (info))
    desc =
        gst_stream_subtitle_information_to_string (info,
        GPOINTER_TO_INT (depth) + 1);
  else if (GST_IS_DISCOVERER_CONTAINER_INFO (info)) {
    const GstTagList *tags =
        gst_discoverer_container_info_get_tags (GST_DISCOVERER_CONTAINER_INFO
        (info));
    print_tags_topology (GPOINTER_TO_INT (depth) + 1, tags);
  }
  if (desc) {
    g_print ("%s", desc);
    g_free (desc);
  }
}

static void
print_topology (GstDiscovererStreamInfo * info, guint depth)
{
  GstDiscovererStreamInfo *next;

  if (!info)
    return;

  print_stream_info (info, GINT_TO_POINTER (depth));

  next = gst_discoverer_stream_info_get_next (info);
  if (next) {
    print_topology (next, depth + 1);
    gst_discoverer_stream_info_unref (next);
  } else if (GST_IS_DISCOVERER_CONTAINER_INFO (info)) {
    GList *tmp, *streams;

    streams =
        gst_discoverer_container_info_get_streams (GST_DISCOVERER_CONTAINER_INFO
        (info));
    for (tmp = streams; tmp; tmp = tmp->next) {
      GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *) tmp->data;
      print_topology (tmpinf, depth + 1);
    }
    gst_discoverer_stream_info_list_free (streams);
  }
}

static void
print_toc_entry (gpointer data, gpointer user_data)
{
  GstTocEntry *entry = (GstTocEntry *) data;
  guint depth = GPOINTER_TO_UINT (user_data);
  guint indent = MIN (GPOINTER_TO_UINT (user_data), MAX_INDENT);
  GstTagList *tags;
  GList *subentries;
  gint64 start, stop;

  gst_toc_entry_get_start_stop_times (entry, &start, &stop);
  g_print ("%*s%s: start: %" GST_TIME_FORMAT " stop: %" GST_TIME_FORMAT "\n",
      depth, " ",
      gst_toc_entry_type_get_nick (gst_toc_entry_get_entry_type (entry)),
      GST_TIME_ARGS (start), GST_TIME_ARGS (stop));
  indent += 2;

  /* print tags */
  tags = gst_toc_entry_get_tags (entry);
  if (tags) {
    g_print ("%*sTags:\n", 2 * depth, " ");
    gst_tag_list_foreach (tags, print_tag_foreach, GUINT_TO_POINTER (indent));
  }

  /* loop over sub-toc entries */
  subentries = gst_toc_entry_get_sub_entries (entry);
  g_list_foreach (subentries, print_toc_entry, GUINT_TO_POINTER (indent));
}

static void
print_properties (GstDiscovererInfo * info, gint tab)
{
  const GstTagList *tags;
  const GstToc *toc;

  g_print ("%*sDuration: %" GST_TIME_FORMAT "\n", tab + 1, " ",
      GST_TIME_ARGS (gst_discoverer_info_get_duration (info)));
  g_print ("%*sSeekable: %s\n", tab + 1, " ",
      (gst_discoverer_info_get_seekable (info) ? "yes" : "no"));
  g_print ("%*sLive: %s\n", tab + 1, " ",
      (gst_discoverer_info_get_live (info) ? "yes" : "no"));
  if (verbose && (tags = gst_discoverer_info_get_tags (info))) {
    g_print ("%*sTags: \n", tab + 1, " ");
    gst_tag_list_foreach (tags, print_tag_foreach, GUINT_TO_POINTER (tab + 2));
  }
  if (show_toc && (toc = gst_discoverer_info_get_toc (info))) {
    GList *entries;

    g_print ("%*sTOC: \n", tab + 1, " ");
    entries = gst_toc_get_entries (toc);
    g_list_foreach (entries, print_toc_entry, GUINT_TO_POINTER (tab + 5));
  }
}

static void
print_info (GstDiscovererInfo * info, GError * err)
{
  GstDiscovererResult result;
  GstDiscovererStreamInfo *sinfo;

  if (!info) {
    g_print ("Could not discover URI\n");
    g_print (" %s\n", err->message);
    return;
  }

  result = gst_discoverer_info_get_result (info);
  g_print ("Done discovering %s\n", gst_discoverer_info_get_uri (info));
  switch (result) {
    case GST_DISCOVERER_OK:
    {
      break;
    }
    case GST_DISCOVERER_URI_INVALID:
    {
      g_print ("URI is not valid\n");
      break;
    }
    case GST_DISCOVERER_ERROR:
    {
      g_print ("An error was encountered while discovering the file\n");
      g_print (" %s\n", err->message);
      break;
    }
    case GST_DISCOVERER_TIMEOUT:
    {
      g_print ("Analyzing URI timed out\n");
      break;
    }
    case GST_DISCOVERER_BUSY:
    {
      g_print ("Discoverer was busy\n");
      break;
    }
    case GST_DISCOVERER_MISSING_PLUGINS:
    {
      gint i = 0;
      const gchar **installer_details =
          gst_discoverer_info_get_missing_elements_installer_details (info);

      g_print ("Missing plugins\n");

      while (installer_details[i]) {
        g_print (" (%s)\n", installer_details[i]);

        i++;
      }
      break;
    }
  }

  if ((sinfo = gst_discoverer_info_get_stream_info (info))) {
    g_print ("\nProperties:\n");
    print_properties (info, 1);
    print_topology (sinfo, 1);
    gst_discoverer_stream_info_unref (sinfo);
  }

  g_print ("\n");
}

static void
process_file (GstDiscoverer * dc, const gchar * filename)
{
  GError *err = NULL;
  GDir *dir;
  gchar *uri, *path;
  GstDiscovererInfo *info;

  if (!gst_uri_is_valid (filename)) {
    /* Recurse into directories */
    if ((dir = g_dir_open (filename, 0, NULL))) {
      const gchar *entry;

      while ((entry = g_dir_read_name (dir))) {
        gchar *path;
        path = g_strconcat (filename, G_DIR_SEPARATOR_S, entry, NULL);
        process_file (dc, path);
        g_free (path);
      }

      g_dir_close (dir);
      return;
    }

    if (!g_path_is_absolute (filename)) {
      gchar *cur_dir;

      cur_dir = g_get_current_dir ();
      path = g_build_filename (cur_dir, filename, NULL);
      g_free (cur_dir);
    } else {
      path = g_strdup (filename);
    }

    uri = g_filename_to_uri (path, NULL, &err);
    g_free (path);
    path = NULL;

    if (err) {
      g_warning ("Couldn't convert filename to URI: %s\n", err->message);
      g_clear_error (&err);
      return;
    }
  } else {
    uri = g_strdup (filename);
  }

  if (!async) {
    g_print ("Analyzing %s\n", uri);
    info = gst_discoverer_discover_uri (dc, uri, &err);
    print_info (info, err);
    g_clear_error (&err);
    if (info)
      gst_discoverer_info_unref (info);
  } else {
    gst_discoverer_discover_uri_async (dc, uri);
  }

  g_free (uri);
}

static void
_new_discovered_uri (GstDiscoverer * dc, GstDiscovererInfo * info, GError * err)
{
  print_info (info, err);
}

static gboolean
_run_async (PrivStruct * ps)
{
  gint i;

  for (i = 1; i < ps->argc; i++)
    process_file (ps->dc, ps->argv[i]);

  return FALSE;
}

static void
_discoverer_finished (GstDiscoverer * dc, GMainLoop * ml)
{
  g_main_loop_quit (ml);
}

static int
real_main (int argc, char **argv)
{
  GError *err = NULL;
  GstDiscoverer *dc;
  gint timeout = 10;
  gboolean use_cache = FALSE, print_cache_dir = FALSE;
  GOptionEntry options[] = {
    {"async", 'a', 0, G_OPTION_ARG_NONE, &async,
        "Run asynchronously", NULL},
    {"use-cache", 0, 0, G_OPTION_ARG_NONE, &use_cache,
        "Use GstDiscovererInfo from our cache.", NULL},
    {"print-cache-dir", 0, 0, G_OPTION_ARG_NONE, &print_cache_dir,
        "Print the directory of the discoverer cache.", NULL},
    {"timeout", 't', 0, G_OPTION_ARG_INT, &timeout,
        "Specify timeout (in seconds, default 10)", "T"},
    /* {"elem", 'e', 0, G_OPTION_ARG_NONE, &elem_seek, */
    /*     "Seek on elements instead of pads", NULL}, */
    {"toc", 'c', 0, G_OPTION_ARG_NONE, &show_toc,
        "Output TOC (chapters and editions)", NULL},
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
        "Verbose properties", NULL},
    {NULL}
  };
  GOptionContext *ctx;

  setlocale (LC_ALL, "");

  ctx =
      g_option_context_new
      ("- discover files synchronously with GstDiscoverer");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());

#ifdef G_OS_WIN32
  if (!g_option_context_parse_strv (ctx, &argv, &err))
#else
  if (!g_option_context_parse (ctx, &argc, &argv, &err))
#endif
  {
    g_print ("Error initializing: %s\n", err->message);
    g_option_context_free (ctx);
    g_clear_error (&err);
    exit (1);
  }

  g_option_context_free (ctx);

#ifdef G_OS_WIN32
  argc = g_strv_length (argv);
#endif

  if (argc < 2) {
    g_print ("usage: %s <uris>\n", argv[0]);
    exit (-1);
  }

  if (print_cache_dir) {
    gchar *cache_dir =
        g_build_filename (g_get_user_cache_dir (), "gstreamer-" GST_API_VERSION,
        "discoverer", NULL);
    g_print ("\nGstDiscoverer cache directory:\n\n    %s\n\n", cache_dir);
    g_free (cache_dir);
    exit (0);
  }

  dc = gst_discoverer_new (timeout * GST_SECOND, &err);
  if (G_UNLIKELY (dc == NULL)) {
    g_print ("Error initializing: %s\n", err->message);
    g_clear_error (&err);
    exit (1);
  }

  g_object_set (dc, "use-cache", use_cache, NULL);

  if (!async) {
    gint i;
    for (i = 1; i < argc; i++)
      process_file (dc, argv[i]);
  } else {
    PrivStruct *ps = g_new0 (PrivStruct, 1);
    GMainLoop *ml = g_main_loop_new (NULL, FALSE);

    ps->dc = dc;
    ps->argc = argc;
    ps->argv = argv;

    /* adding uris will be started when the mainloop runs */
    g_idle_add ((GSourceFunc) _run_async, ps);

    /* connect signals */
    g_signal_connect (dc, "discovered", G_CALLBACK (_new_discovered_uri), NULL);
    g_signal_connect (dc, "finished", G_CALLBACK (_discoverer_finished), ml);

    gst_discoverer_start (dc);
    /* run mainloop */
    g_main_loop_run (ml);

    gst_discoverer_stop (dc);
    g_free (ps);
    g_main_loop_unref (ml);
  }
  g_object_unref (dc);

  return 0;
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
