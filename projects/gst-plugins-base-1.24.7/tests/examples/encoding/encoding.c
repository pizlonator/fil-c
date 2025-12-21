/* GStreamer
 *
 * encoding.c: example application for using GstProfile and encodebin
 *
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

#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/pbutils/encoding-profile.h>
#include "gstcapslist.h"

static gboolean silent = FALSE;

static void
list_codecs (void)
{
  GstCaps *l, *caps;
  GstStructure *st;
  guint i, len;
  gchar *tmpstr, *desc;

  caps = gst_caps_new_empty ();

  g_print ("Available container formats:\n");
  l = gst_caps_list_container_formats (GST_RANK_NONE);
  len = gst_caps_get_size (l);
  for (i = 0; i < len; i++) {
    st = gst_caps_steal_structure (l, 0);
    gst_caps_append_structure (caps, st);

    tmpstr = gst_caps_to_string (caps);
    desc = gst_pb_utils_get_codec_description (caps);
    g_print ("  %s - %s\n", desc, tmpstr);
    g_free (tmpstr);
    g_free (desc);
    gst_caps_remove_structure (caps, 0);
  }
  g_print ("\n");
  gst_caps_unref (l);

  g_print ("Available video codecs:\n");
  l = gst_caps_list_video_encoding_formats (GST_RANK_NONE);
  len = gst_caps_get_size (l);
  for (i = 0; i < len; i++) {
    st = gst_caps_steal_structure (l, 0);
    gst_caps_append_structure (caps, st);

    tmpstr = gst_caps_to_string (caps);
    desc = gst_pb_utils_get_codec_description (caps);
    g_print ("  %s - %s\n", desc, tmpstr);
    g_free (tmpstr);
    g_free (desc);
    gst_caps_remove_structure (caps, 0);
  }
  g_print ("\n");
  gst_caps_unref (l);

  g_print ("Available audio codecs:\n");
  l = gst_caps_list_audio_encoding_formats (GST_RANK_NONE);
  len = gst_caps_get_size (l);
  for (i = 0; i < len; i++) {
    st = gst_caps_steal_structure (l, 0);
    gst_caps_append_structure (caps, st);

    tmpstr = gst_caps_to_string (caps);
    desc = gst_pb_utils_get_codec_description (caps);
    g_print ("  %s - %s\n", desc, tmpstr);
    g_free (tmpstr);
    g_free (desc);
    gst_caps_remove_structure (caps, 0);
  }
  g_print ("\n");
  gst_caps_unref (l);

  gst_caps_unref (caps);
}

static gchar *
generate_filename (const GstCaps * container, const GstCaps * vcodec,
    const GstCaps * acodec)
{
  gchar *a, *b, *c;
  gchar *res = NULL;
  guint i;

  a = gst_pb_utils_get_codec_description (container);
  b = gst_pb_utils_get_codec_description (vcodec);
  c = gst_pb_utils_get_codec_description (acodec);

  if (!a)
    a = g_strdup_printf ("%.10s",
        g_uri_escape_string (gst_caps_to_string (container), NULL, FALSE));
  if (!b)
    b = g_strdup_printf ("%.10s",
        g_uri_escape_string (gst_caps_to_string (vcodec), NULL, FALSE));
  if (!c)
    c = g_strdup_printf ("%.10s",
        g_uri_escape_string (gst_caps_to_string (acodec), NULL, FALSE));

  for (i = 0; i < 256 && res == NULL; i++) {
    res = g_strdup_printf ("%s-%s-%s-%d.file", a, b, c, i);
    if (g_file_test (res, G_FILE_TEST_EXISTS)) {
      g_free (res);
      res = NULL;
    }
  }
  /* Make sure file doesn't already exist */

  g_free (a);
  g_free (b);
  g_free (c);

  return res;
}

static GstEncodingProfile *
create_profile (GstCaps * cf, GstCaps * vf, GstCaps * af)
{
  GstEncodingContainerProfile *cprof = NULL;

  cprof =
      gst_encoding_container_profile_new ((gchar *) "test-application-profile",
      NULL, cf, NULL);

  if (vf)
    gst_encoding_container_profile_add_profile (cprof,
        (GstEncodingProfile *) gst_encoding_video_profile_new (vf,
            NULL, NULL, 0));
  if (af)
    gst_encoding_container_profile_add_profile (cprof, (GstEncodingProfile *)
        gst_encoding_audio_profile_new (af, NULL, NULL, 0));

  /* Let's print out some info */
  if (!silent) {
    gchar *desc = gst_pb_utils_get_codec_description (cf);
    gchar *cd = gst_caps_to_string (cf);
    g_print ("Encoding parameters\n");
    g_print ("  Container format : %s (%s)\n", desc, cd);
    g_free (desc);
    g_free (cd);
    if (vf) {
      desc = gst_pb_utils_get_codec_description (vf);
      cd = gst_caps_to_string (vf);
      g_print ("  Video format : %s (%s)\n", desc, cd);
      g_free (desc);
      g_free (cd);
    }
    if (af) {
      desc = gst_pb_utils_get_codec_description (af);
      cd = gst_caps_to_string (af);
      g_print ("  Audio format : %s (%s)\n", desc, cd);
      g_free (desc);
      g_free (cd);
    }
  }

  return (GstEncodingProfile *) cprof;
}

static GstEncodingProfile *
create_profile_from_string (gchar * format, gchar * vformat, gchar * aformat)
{
  GstEncodingProfile *prof = NULL;
  GstCaps *cf = NULL, *vf = NULL, *af = NULL;

  if (format)
    cf = gst_caps_from_string (format);
  if (vformat)
    vf = gst_caps_from_string (vformat);
  if (aformat)
    af = gst_caps_from_string (aformat);

  if (G_UNLIKELY ((vformat && (vf == NULL)) || (aformat && (af == NULL))))
    goto beach;

  prof = create_profile (cf, vf, af);

beach:
  if (cf)
    gst_caps_unref (cf);
  if (vf)
    gst_caps_unref (vf);
  if (af)
    gst_caps_unref (af);

  return prof;
}

static void
pad_added_cb (GstElement * uridecodebin, GstPad * pad, GstElement * encodebin)
{
  GstPad *sinkpad;

  sinkpad = gst_element_get_compatible_pad (encodebin, pad, NULL);

  if (sinkpad == NULL) {
    GstCaps *caps;

    /* Ask encodebin for a compatible pad */
    caps = gst_pad_query_caps (pad, NULL);
    g_signal_emit_by_name (encodebin, "request-pad", caps, &sinkpad);
    if (caps)
      gst_caps_unref (caps);
  }
  if (sinkpad == NULL) {
    g_print ("Couldn't get an encoding channel for pad %s:%s\n",
        GST_DEBUG_PAD_NAME (pad));
    return;
  }

  if (G_UNLIKELY (gst_pad_link (pad, sinkpad) != GST_PAD_LINK_OK)) {
    g_print ("Couldn't link pads\n");
  }

  return;
}

static gboolean
autoplug_continue_cb (GstElement * uridecodebin, GstPad * somepad,
    GstCaps * caps, GstElement * encodebin)
{
  GstPad *sinkpad;

  g_signal_emit_by_name (encodebin, "request-pad", caps, &sinkpad);

  if (sinkpad == NULL)
    return TRUE;

  return FALSE;
}

static void
bus_message_cb (GstBus * bus, GstMessage * message, GMainLoop * mainloop)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      g_print ("ERROR\n");
      gst_bus_set_flushing (bus, TRUE);
      g_main_loop_quit (mainloop);
      break;
    case GST_MESSAGE_EOS:
      g_print ("Done\n");
      g_main_loop_quit (mainloop);
      break;
    default:
      break;
  }
}

static void
transcode_file (gchar * uri, gchar * outputuri, GstEncodingProfile * prof)
{
  GstElement *pipeline;
  GstElement *src;
  GstElement *ebin;
  GstElement *sink;
  GstBus *bus;
  GstCaps *profilecaps, *rescaps;
  GMainLoop *mainloop;

  g_print (" Input URI  : %s\n", uri);
  g_print (" Output URI : %s\n", outputuri);

  sink = gst_element_make_from_uri (GST_URI_SINK, outputuri, "sink", NULL);
  if (G_UNLIKELY (sink == NULL)) {
    g_print ("Can't create output sink, most likely invalid output URI !\n");
    return;
  }

  src = gst_element_factory_make ("uridecodebin", NULL);
  if (G_UNLIKELY (src == NULL)) {
    g_print ("Can't create uridecodebin for input URI, aborting!\n");
    return;
  }

  /* Figure out the streams that can be passed as-is to encodebin */
  g_object_get (src, "caps", &rescaps, NULL);
  rescaps = gst_caps_copy (rescaps);
  profilecaps = gst_encoding_profile_get_input_caps (prof);
  gst_caps_append (rescaps, profilecaps);

  /* Set properties */
  g_object_set (src, "uri", uri, "caps", rescaps, NULL);

  ebin = gst_element_factory_make ("encodebin", NULL);
  g_object_set (ebin, "profile", prof, NULL);

  g_signal_connect (src, "autoplug-continue", G_CALLBACK (autoplug_continue_cb),
      ebin);
  g_signal_connect (src, "pad-added", G_CALLBACK (pad_added_cb), ebin);

  pipeline = gst_pipeline_new ("encoding-pipeline");

  gst_bin_add_many (GST_BIN (pipeline), src, ebin, sink, NULL);

  gst_element_link (ebin, sink);

  mainloop = g_main_loop_new (NULL, FALSE);

  bus = gst_pipeline_get_bus ((GstPipeline *) pipeline);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_message_cb), mainloop);

  if (gst_element_set_state (pipeline,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    g_print ("Failed to start the encoding\n");
    return;
  }

  g_main_loop_run (mainloop);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

static gchar *
ensure_uri (gchar * location)
{
  gchar *res;
  gchar *path;

  if (gst_uri_is_valid (location))
    return g_strdup (location);

  if (!g_path_is_absolute (location)) {
    gchar *cur_dir;
    cur_dir = g_get_current_dir ();
    path = g_build_filename (cur_dir, location, NULL);
    g_free (cur_dir);
  } else
    path = g_strdup (location);

  res = g_filename_to_uri (path, NULL, NULL);
  g_free (path);

  return res;
}

int
main (int argc, char **argv)
{
  GError *err = NULL;
  gchar *outputuri = NULL;
  gchar *format = NULL;
  gchar *aformat = NULL;
  gchar *vformat = NULL;
  gboolean allmissing = FALSE;
  gboolean listcodecs = FALSE;
  GOptionEntry options[] = {
    {"silent", 's', 0, G_OPTION_ARG_NONE, &silent,
        "Don't output the information structure", NULL},
    {"outputuri", 'o', 0, G_OPTION_ARG_STRING, &outputuri,
        "URI to encode to", "URI (<protocol>://<location>)"},
    {"format", 'f', 0, G_OPTION_ARG_STRING, &format,
        "Container format", "<GstCaps>"},
    {"vformat", 'v', 0, G_OPTION_ARG_STRING, &vformat,
        "Video format", "<GstCaps>"},
    {"aformat", 'a', 0, G_OPTION_ARG_STRING, &aformat,
        "Audio format", "<GstCaps>"},
    {"allmissing", 'm', 0, G_OPTION_ARG_NONE, &allmissing,
        "encode to all matching format/codec that aren't specified", NULL},
    {"list-codecs", 'l', 0, G_OPTION_ARG_NONE, &listcodecs,
        "list all available codecs and container formats", NULL},
    {NULL}
  };
  GOptionContext *ctx;
  GstEncodingProfile *prof;
  gchar *inputuri;

  ctx = g_option_context_new ("- encode URIs with GstProfile and encodebin");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", err->message);
    g_option_context_free (ctx);
    g_clear_error (&err);
    exit (1);
  }

  if (listcodecs) {
    list_codecs ();
    g_option_context_free (ctx);
    exit (0);
  }

  if (outputuri == NULL || argc != 2) {
    g_print ("%s", g_option_context_get_help (ctx, TRUE, NULL));
    g_option_context_free (ctx);
    exit (-1);
  }

  g_option_context_free (ctx);

  /* Fixup outputuri to be a URI */
  inputuri = ensure_uri (argv[1]);
  outputuri = ensure_uri (outputuri);

  if (allmissing) {
    GList *muxers;
    GstCaps *formats = NULL;
    GstCaps *vformats = NULL;
    GstCaps *aformats = NULL;
    guint f, v, a, flen, vlen, alen;

    if (!format)
      formats = gst_caps_list_container_formats (GST_RANK_NONE);
    else
      formats = gst_caps_from_string (format);

    if (!vformat)
      vformats = gst_caps_list_video_encoding_formats (GST_RANK_NONE);
    else
      vformats = gst_caps_from_string (vformat);

    if (!aformat)
      aformats = gst_caps_list_audio_encoding_formats (GST_RANK_NONE);
    else
      aformats = gst_caps_from_string (aformat);
    muxers =
        gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_MUXER,
        GST_RANK_NONE);

    flen = gst_caps_get_size (formats);

    for (f = 0; f < flen; f++) {
      GstCaps *container =
          gst_caps_new_full (gst_caps_steal_structure (formats, 0), NULL);
      GstCaps *compatv =
          gst_caps_list_compatible_codecs (container, vformats, muxers);
      GstCaps *compata =
          gst_caps_list_compatible_codecs (container, aformats, muxers);

      vlen = gst_caps_get_size (compatv);
      alen = gst_caps_get_size (compata);


      for (v = 0; v < vlen; v++) {
        GstCaps *vcodec =
            gst_caps_new_full (gst_structure_copy (gst_caps_get_structure
                (compatv, v)), NULL);
        for (a = 0; a < alen; a++) {
          GstCaps *acodec =
              gst_caps_new_full (gst_structure_copy (gst_caps_get_structure
                  (compata, a)), NULL);

          prof =
              create_profile ((GstCaps *) container, (GstCaps *) vcodec,
              (GstCaps *) acodec);
          if (G_UNLIKELY (prof == NULL)) {
            g_print ("Wrong arguments\n");
            break;
          }
          outputuri =
              ensure_uri (generate_filename (container, vcodec, acodec));
          transcode_file (inputuri, outputuri, prof);
          gst_encoding_profile_unref (prof);

          gst_caps_unref (acodec);
        }
        gst_caps_unref (vcodec);
      }
      gst_caps_unref (container);
    }

  } else {

    /* Create the profile */
    prof = create_profile_from_string (format, vformat, aformat);
    if (G_UNLIKELY (prof == NULL)) {
      g_print ("Encoding arguments are not valid !\n");
      return 1;
    }

    /* Transcode file */
    transcode_file (inputuri, outputuri, prof);

    /* cleanup */
    gst_encoding_profile_unref (prof);

  }
  return 0;
}
