/* GStreamer
 * Copyright (C) 2019 Thibault Saunier <tsaunier@igalia.com>
 *
 * gsttranscodebin.c:
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
#  include "config.h"
#endif

#include "gsttranscoding.h"
#include "gsttranscodeelements.h"
#include <glib/gi18n-lib.h>
#include <gst/pbutils/pbutils.h>

#include <gst/pbutils/missing-plugins.h>

/**
 * GstTranscodeBin!sink_%u:
 *
 * Extra sinkpads for the parallel transcoding of auxiliary streams.
 *
 * Since: 1.20
 */
static GstStaticPadTemplate transcode_bin_sinks_template =
GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate transcode_bin_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

/**
 * GstTranscodeBin!src_%u:
 *
 * The sometimes source pad, it will be exposed depending on the
 * #transcodebin:profile in use.
 *
 * Note: in GStreamer 1.18 it was a static
 * srcpad but in the the 1.20 cycle it was decided that we should make it a
 * sometimes pad as part of the development of #encodebin2.
 *
 * Since: 1.20
 */
static GstStaticPadTemplate transcode_bin_src_template =
GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

typedef struct
{
  const gchar *stream_id;
  GstStream *stream;
  GstPad *encodebin_pad;
} TranscodingStream;

static TranscodingStream *
transcoding_stream_new (GstStream * stream, GstPad * encodebin_pad)
{
  TranscodingStream *tstream = g_new0 (TranscodingStream, 1);

  tstream->stream_id = gst_stream_get_stream_id (stream);
  tstream->stream = gst_object_ref (stream);
  tstream->encodebin_pad = encodebin_pad;

  return tstream;
}

static void
transcoding_stream_free (TranscodingStream * tstream)
{
  gst_object_unref (tstream->stream);
  gst_object_unref (tstream->encodebin_pad);

  g_free (tstream);
}

typedef struct
{
  GstBin parent;

  GstElement *decodebin;
  GstElement *encodebin;

  GstEncodingProfile *profile;
  gboolean avoid_reencoding;
  GstPad *sinkpad;

  GstElement *audio_filter;
  GstElement *video_filter;

  GPtrArray *transcoding_streams;
  gboolean upstream_selected;
} GstTranscodeBin;

typedef struct
{
  GstBinClass parent;

} GstTranscodeBinClass;

/* *INDENT-OFF* */
#define GST_TYPE_TRANSCODE_BIN (gst_transcode_bin_get_type ())
#define GST_TRANSCODE_BIN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TRANSCODE_BIN, GstTranscodeBin))

#define DEFAULT_AVOID_REENCODING   FALSE

GST_DEBUG_CATEGORY_STATIC(gst_transcodebin_debug);
#define GST_CAT_DEFAULT gst_transcodebin_debug

G_DEFINE_TYPE (GstTranscodeBin, gst_transcode_bin, GST_TYPE_BIN);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (transcodebin, "transcodebin", GST_RANK_NONE,
      GST_TYPE_TRANSCODE_BIN, transcodebin_element_init (plugin));

static GstPad *
get_encodebin_pad_from_stream (GstTranscodeBin * self, GstStream * stream);

enum
{
 PROP_0,
 PROP_PROFILE,
 PROP_AVOID_REENCODING,
 PROP_VIDEO_FILTER,
 PROP_AUDIO_FILTER,
 LAST_PROP
};

static void
post_missing_plugin_error (GstElement * dec, const gchar * element_name)
{
  GstMessage *msg;

  msg = gst_missing_element_message_new (dec, element_name);
  gst_element_post_message (dec, msg);

  GST_ELEMENT_ERROR (dec, CORE, MISSING_PLUGIN,
      ("Missing element '%s' - check your GStreamer installation.",
          element_name), (NULL));
}
/* *INDENT-ON* */

static gboolean
filter_handles_any (GstElement * filter)
{
  GList *tmp;

  for (tmp = gst_element_get_pad_template_list (filter); tmp; tmp = tmp->next) {
    GstPadTemplate *tmpl = tmp->data;
    GstCaps *caps = gst_pad_template_get_caps (tmpl);

    if (!gst_caps_is_any (caps)) {
      gst_caps_unref (caps);
      return FALSE;
    }

    gst_caps_unref (caps);
  }

  return gst_element_get_pad_template_list (filter) != NULL;
}

static GstPad *
_insert_filter (GstTranscodeBin * self, GstPad * sinkpad, GstPad * pad,
    const GstCaps * filtercaps)
{
  GstPad *filter_src = NULL, *filter_sink = NULL, *convert_sink, *convert_src;
  GstElement *filter = NULL, *convert;
  GstObject *filter_parent;
  const gchar *media_type;
  gboolean audio = TRUE;
  GstCaps *caps;

  media_type = gst_structure_get_name (gst_caps_get_structure (filtercaps, 0));

  if (self->video_filter && g_str_has_prefix (media_type, "video")) {
    audio = FALSE;

    if (!g_strcmp0 (media_type, "video/x-raw")
        || filter_handles_any (self->video_filter))
      filter = self->video_filter;
    else
      GST_ERROR_OBJECT (pad, "decodebin pad does not produce raw data (%"
          GST_PTR_FORMAT "), cannot add video filter '%s'", filtercaps,
          GST_ELEMENT_NAME (self->video_filter));
  } else if (self->audio_filter && g_str_has_prefix (media_type, "audio")) {
    if (!g_strcmp0 (media_type, "audio/x-raw")
        || filter_handles_any (self->audio_filter))
      filter = self->audio_filter;
    else
      GST_ERROR_OBJECT (pad, "decodebin pad does not produce raw data (%"
          GST_PTR_FORMAT "), cannot add audio filter '%s'", filtercaps,
          GST_ELEMENT_NAME (self->audio_filter));
  }

  if (!filter)
    return pad;

  filter_parent = gst_object_get_parent (GST_OBJECT (filter));
  if (filter_parent != GST_OBJECT_CAST (self)) {
    GST_WARNING_OBJECT (self,
        "Filter already in use (inside %" GST_PTR_FORMAT ").", filter_parent);
    GST_FIXME_OBJECT (self,
        "Handle transcoding several streams of a same kind.");
    gst_object_unref (filter_parent);

    return pad;
  }

  gst_object_unref (filter_parent);
  /* We are guaranteed filters only have 1 unique sinkpad and srcpad */
  GST_OBJECT_LOCK (filter);
  filter_sink = filter->sinkpads->data;
  filter_src = filter->srcpads->data;
  GST_OBJECT_UNLOCK (filter);

  if (filter_handles_any (filter))
    convert = gst_element_factory_make ("identity", NULL);
  else if (audio)
    convert = gst_element_factory_make ("audioconvert", NULL);
  else
    convert = gst_element_factory_make ("videoconvert", NULL);

  if (!convert) {
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN,
        (_("Missing element '%s' - check your GStreamer installation."),
            audio ? "audioconvert" : "videoconvert"),
        ("Cannot add filter as %s element is missing",
            audio ? "audioconvert" : "videoconvert"));
    return pad;
  }

  gst_bin_add_many (GST_BIN (self), convert, NULL);

  convert_sink = gst_element_get_static_pad (convert, "sink");
  g_assert (convert_sink);

  if (G_UNLIKELY (gst_pad_link (pad, convert_sink) != GST_PAD_LINK_OK)) {
    GstCaps *othercaps = gst_pad_get_pad_template_caps (convert_sink);
    caps = gst_pad_get_current_caps (pad);

    GST_ELEMENT_ERROR (self, CORE, PAD,
        (NULL),
        ("Couldn't link pads \n\n %" GST_PTR_FORMAT ": %" GST_PTR_FORMAT
            "\n\n  and \n\n %" GST_PTR_FORMAT ": %" GST_PTR_FORMAT
            "\n\n", pad, caps, convert_sink, othercaps));

    gst_object_unref (convert_sink);
    gst_caps_unref (caps);
    gst_caps_unref (othercaps);
  }

  gst_object_unref (convert_sink);

  convert_src = gst_element_get_static_pad (convert, "src");
  g_assert (convert_src);

  if (G_UNLIKELY (gst_pad_link (convert_src, filter_sink) != GST_PAD_LINK_OK)) {
    GstCaps *othercaps = gst_pad_get_pad_template_caps (filter_sink);
    caps = gst_pad_get_pad_template_caps (convert_src);

    GST_ELEMENT_ERROR (self, CORE, PAD,
        (NULL),
        ("Couldn't link pads \n\n %" GST_PTR_FORMAT ": %" GST_PTR_FORMAT
            "\n\n  and \n\n %" GST_PTR_FORMAT ": %" GST_PTR_FORMAT
            "\n\n", convert_src, caps, filter_sink, othercaps));

    gst_object_unref (convert_src);
    gst_caps_unref (caps);
    gst_caps_unref (othercaps);
  }

  gst_object_unref (convert_src);

  gst_element_sync_state_with_parent (convert);
  gst_element_sync_state_with_parent (filter);

  GST_DEBUG_OBJECT (self, "added %s filter '%s'",
      audio ? "audio" : "video", GST_ELEMENT_NAME (filter));

  return filter_src;
}

static TranscodingStream *
find_stream (GstTranscodeBin * self, const gchar * stream_id, GstPad * pad)
{
  gint i;
  TranscodingStream *res = NULL;

  GST_OBJECT_LOCK (self);
  GST_DEBUG_OBJECT (self,
      "Looking for stream %s in %u existing transcoding streams",
      stream_id, self->transcoding_streams->len);
  for (i = 0; i < self->transcoding_streams->len; i = i + 1) {
    TranscodingStream *s = self->transcoding_streams->pdata[i];

    if (stream_id && !g_strcmp0 (s->stream_id, stream_id)) {
      res = s;
      goto done;
    } else if (pad && s->encodebin_pad == pad) {
      res = s;
      goto done;
    }
  }

done:
  GST_OBJECT_UNLOCK (self);
  GST_DEBUG_OBJECT (self, "Look-up result: %p", res);

  return res;
}

static TranscodingStream *
setup_stream (GstTranscodeBin * self, GstStream * stream)
{
  TranscodingStream *res = NULL;
  GstPad *encodebin_pad = get_encodebin_pad_from_stream (self, stream);

  GST_DEBUG_OBJECT (self,
      "Encodebin pad for stream %" GST_PTR_FORMAT " : %" GST_PTR_FORMAT, stream,
      encodebin_pad);
  if (encodebin_pad) {
    GST_INFO_OBJECT (self,
        "Going to transcode stream %s (encodebin pad: %" GST_PTR_FORMAT ")",
        gst_stream_get_stream_id (stream), encodebin_pad);

    res = transcoding_stream_new (stream, encodebin_pad);
    GST_OBJECT_LOCK (self);
    g_ptr_array_add (self->transcoding_streams, res);
    GST_OBJECT_UNLOCK (self);
  }

  return res;
}

static void
gst_transcode_bin_link_encodebin_pad (GstTranscodeBin * self, GstPad * pad,
    GstEvent * sstart)
{
  GstCaps *caps, *filtercaps;
  GstPadLinkReturn lret;
  const gchar *stream_id;
  TranscodingStream *stream;

  gst_event_parse_stream_start (sstart, &stream_id);
  stream = find_stream (self, stream_id, NULL);

  if (!stream) {
    if (self->upstream_selected) {
      GstStream *tmpstream;

      gst_event_parse_stream (sstart, &tmpstream);

      stream = setup_stream (self, tmpstream);

      gst_object_unref (tmpstream);
    }

    if (!stream) {
      GST_ERROR_OBJECT (self, "Could not find any stream with ID: %s",
          stream_id);
      return;
    }
  }

  filtercaps = gst_pad_query_caps (pad, NULL);
  pad = _insert_filter (self, stream->encodebin_pad, pad, filtercaps);
  gst_caps_unref (filtercaps);
  lret = gst_pad_link (pad, stream->encodebin_pad);
  switch (lret) {
    case GST_PAD_LINK_OK:
      break;
    case GST_PAD_LINK_WAS_LINKED:
      GST_FIXME_OBJECT (self, "Pad %" GST_PTR_FORMAT " was already linked",
          stream->encodebin_pad);
      break;
    default:
    {
      GstCaps *othercaps = gst_pad_query_caps (stream->encodebin_pad, NULL);
      caps = gst_pad_get_current_caps (pad);

      if (!caps)
        caps = gst_pad_query_caps (pad, NULL);

      GST_ELEMENT_ERROR_WITH_DETAILS (self, CORE, PAD,
          (NULL),
          ("Couldn't link pads:\n    %" GST_PTR_FORMAT ": %" GST_PTR_FORMAT
              "\nand:\n"
              "    %" GST_PTR_FORMAT ": %" GST_PTR_FORMAT "\n\n Error: %s\n",
              pad, caps, stream->encodebin_pad, othercaps,
              gst_pad_link_get_name (lret)),
          ("linking-error", GST_TYPE_PAD_LINK_RETURN, lret,
              "source-pad", GST_TYPE_PAD, pad,
              "source-caps", GST_TYPE_CAPS, caps,
              "sink-pad", GST_TYPE_PAD, stream->encodebin_pad,
              "sink-caps", GST_TYPE_CAPS, othercaps, NULL));

      gst_clear_caps (&caps);
      if (othercaps)
        gst_caps_unref (othercaps);
    }
  }
}

static void
query_upstream_selectable (GstTranscodeBin * self, GstPad * pad)
{
  GstQuery *query;
  gboolean result;

  /* Query whether upstream can handle stream selection or not */
  query = gst_query_new_selectable ();
  result = GST_PAD_IS_SINK (pad) ? gst_pad_peer_query (pad, query)
      : gst_pad_query (pad, query);
  if (result) {
    GST_FIXME_OBJECT (self,
        "We force `transcodebin` to upstream selection"
        " mode if *any* of the inputs is. This means things might break if"
        " there's a mix");
    gst_query_parse_selectable (query, &self->upstream_selected);
    GST_DEBUG_OBJECT (pad, "Upstream is selectable : %d",
        self->upstream_selected);
  } else {
    self->upstream_selected = FALSE;
    GST_DEBUG_OBJECT (pad, "Upstream does not handle SELECTABLE query");
  }
  gst_query_unref (query);
}

static GstPadProbeReturn
wait_stream_start_probe (GstPad * pad,
    GstPadProbeInfo * info, GstTranscodeBin * self)
{
  if (GST_EVENT_TYPE (info->data) != GST_EVENT_STREAM_START)
    return GST_PAD_PROBE_OK;

  GST_INFO_OBJECT (self,
      "Got pad %" GST_PTR_FORMAT " with stream:: %" GST_PTR_FORMAT, pad,
      info->data);
  query_upstream_selectable (self, pad);
  gst_transcode_bin_link_encodebin_pad (self, pad, info->data);

  return GST_PAD_PROBE_REMOVE;
}

static void
decodebin_pad_added_cb (GstElement * decodebin, GstPad * pad,
    GstTranscodeBin * self)
{
  const gchar *stream_id;
  GstEvent *sstart_event;

  if (GST_PAD_IS_SINK (pad))
    return;

  sstart_event = gst_pad_get_sticky_event (pad, GST_EVENT_STREAM_START, -1);
  if (sstart_event) {
    gst_event_parse_stream_start (sstart_event, &stream_id);
    GST_INFO_OBJECT (self, "Got pad %" GST_PTR_FORMAT " with stream ID: %s",
        pad, stream_id);
    query_upstream_selectable (self, pad);
    gst_transcode_bin_link_encodebin_pad (self, pad, sstart_event);
    return;
  }

  GST_INFO_OBJECT (self, "Waiting for stream ID for pad %" GST_PTR_FORMAT, pad);
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      (GstPadProbeCallback) wait_stream_start_probe, self, NULL);
}

static void
encodebin_pad_added_cb (GstElement * encodebin, GstPad * pad, GstElement * self)
{
  GstPadTemplate *template;
  GstPad *new_pad;
  gchar *name;

  if (!GST_PAD_IS_SRC (pad))
    return;

  template = gst_element_get_pad_template (self, "src_%u");

  GST_OBJECT_LOCK (self);
  name = g_strdup_printf ("src_%u", GST_ELEMENT (self)->numsrcpads);
  GST_OBJECT_UNLOCK (self);
  new_pad = gst_ghost_pad_new_from_template (name, pad, template);
  g_free (name);
  GST_DEBUG_OBJECT (self, "Encodebin exposed srcpad: %" GST_PTR_FORMAT, pad);

  gst_element_add_pad (self, new_pad);
}

static gboolean
make_encodebin (GstTranscodeBin * self)
{
  GST_INFO_OBJECT (self, "making new encodebin");

  if (!self->profile)
    goto no_profile;

  self->encodebin = gst_element_factory_make ("encodebin2", NULL);
  if (!self->encodebin)
    goto no_encodebin;

  gst_bin_add (GST_BIN (self), self->encodebin);

  g_signal_connect (self->encodebin, "pad-added",
      G_CALLBACK (encodebin_pad_added_cb), self);

  g_object_set (self->encodebin, "profile", self->profile, NULL);

  return gst_element_sync_state_with_parent (self->encodebin);

  /* ERRORS */
no_encodebin:
  {
    post_missing_plugin_error (GST_ELEMENT_CAST (self), "encodebin");

    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("No encodebin element, check your installation"));

    return FALSE;
  }
  /* ERRORS */
no_profile:
  {
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("No GstEncodingProfile set, can not run."));

    return FALSE;
  }
}

static GstPad *
get_encodebin_pad_for_caps (GstTranscodeBin * self, const GstCaps * srccaps)
{
  GstPad *res = NULL;
  GstIterator *pads;
  gboolean done = FALSE;
  GValue paditem = { 0, };

  if (G_UNLIKELY (srccaps == NULL))
    goto no_caps;

  pads = gst_element_iterate_sink_pads (self->encodebin);

  GST_DEBUG_OBJECT (self, "srccaps %" GST_PTR_FORMAT, srccaps);

  while (!done) {
    switch (gst_iterator_next (pads, &paditem)) {
      case GST_ITERATOR_OK:
      {
        GstPad *testpad = g_value_get_object (&paditem);

        if (!gst_pad_is_linked (testpad) && !find_stream (self, NULL, testpad)) {
          GstCaps *sinkcaps = gst_pad_query_caps (testpad, NULL);

          GST_DEBUG_OBJECT (self, "sinkccaps %" GST_PTR_FORMAT, sinkcaps);

          if (gst_caps_can_intersect (srccaps, sinkcaps)) {
            res = gst_object_ref (testpad);
            done = TRUE;
          }
          gst_caps_unref (sinkcaps);
        }
        g_value_reset (&paditem);
      }
        break;
      case GST_ITERATOR_DONE:
      case GST_ITERATOR_ERROR:
        done = TRUE;
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (pads);
        break;
    }
  }
  g_value_reset (&paditem);
  gst_iterator_free (pads);

  if (!res)
    g_signal_emit_by_name (self->encodebin, "request-pad", srccaps, &res);

  return res;

no_caps:
  {
    GST_DEBUG_OBJECT (self, "No caps, can't do anything");
    return NULL;
  }
}

static gboolean
caps_is_raw (GstCaps * caps, GstStreamType stype)
{
  const gchar *media_type;

  if (!caps || !gst_caps_get_size (caps))
    return FALSE;

  media_type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  if (stype == GST_STREAM_TYPE_VIDEO)
    return !g_strcmp0 (media_type, "video/x-raw");
  else if (stype == GST_STREAM_TYPE_AUDIO)
    return !g_strcmp0 (media_type, "audio/x-raw");
  /* FIXME: Handle more types ? */

  return FALSE;
}

static GstPad *
get_encodebin_pad_from_stream (GstTranscodeBin * self, GstStream * stream)
{
  GstCaps *caps = gst_stream_get_caps (stream);
  GstPad *sinkpad = get_encodebin_pad_for_caps (self, caps);

  if (!sinkpad && !caps_is_raw (caps, gst_stream_get_stream_type (stream))) {
    gst_clear_caps (&caps);
    switch (gst_stream_get_stream_type (stream)) {
      case GST_STREAM_TYPE_AUDIO:
        caps = gst_caps_from_string ("audio/x-raw");
        break;
      case GST_STREAM_TYPE_VIDEO:
        caps = gst_caps_from_string ("video/x-raw");
        break;
      default:
        GST_INFO_OBJECT (self, "Unsupported stream type: %" GST_PTR_FORMAT,
            stream);
        return NULL;
    }
    sinkpad = get_encodebin_pad_for_caps (self, caps);
  }

  gst_caps_unref (caps);
  return sinkpad;
}

static gint
select_stream_cb (GstElement * decodebin,
    GstStreamCollection * collection, GstStream * stream,
    GstTranscodeBin * self)
{
  gint i;
  gboolean transcode_stream = FALSE;
  guint len = 0;

  GST_OBJECT_LOCK (self);
  len = self->transcoding_streams->len;
  GST_OBJECT_UNLOCK (self);

  if (len) {
    transcode_stream =
        find_stream (self, gst_stream_get_stream_id (stream), NULL) != NULL;
    if (transcode_stream)
      goto done;
  }

  for (i = 0; i < gst_stream_collection_get_size (collection); i++) {
    GstStream *tmpstream = gst_stream_collection_get_stream (collection, i);

    if (setup_stream (self, tmpstream) && stream == tmpstream)
      transcode_stream = TRUE;
  }

  GST_OBJECT_LOCK (self);
  len = self->transcoding_streams->len;
  GST_OBJECT_UNLOCK (self);

  if (len) {
    transcode_stream =
        find_stream (self, gst_stream_get_stream_id (stream), NULL) != NULL;
  }

done:
  if (!transcode_stream)
    GST_INFO_OBJECT (self, "Discarding stream: %" GST_PTR_FORMAT, stream);

  return transcode_stream;
}

/* Called with OBJECT_LOCK */
static void
_setup_avoid_reencoding (GstTranscodeBin * self)
{
  const GList *tmp;
  GstCaps *decodecaps;

  if (!self->avoid_reencoding)
    return;

  if (!GST_IS_ENCODING_CONTAINER_PROFILE (self->profile))
    return;

  g_object_get (self->decodebin, "caps", &decodecaps, NULL);
  decodecaps = gst_caps_make_writable (decodecaps);
  tmp =
      gst_encoding_container_profile_get_profiles
      (GST_ENCODING_CONTAINER_PROFILE (self->profile));
  for (; tmp; tmp = tmp->next) {
    GstEncodingProfile *profile = tmp->data;
    GstCaps *restrictions, *encodecaps;
    GstElement *filter = NULL;

    restrictions = gst_encoding_profile_get_restriction (profile);

    if (restrictions) {
      gboolean is_any = gst_caps_is_any (restrictions);
      gst_caps_unref (restrictions);
      if (is_any)
        continue;
    }

    encodecaps = gst_encoding_profile_get_format (profile);
    filter = NULL;

    /* Filter operates on raw data so don't allow decodebin to produce
     * encoded data if one is defined. */
    if (GST_IS_ENCODING_VIDEO_PROFILE (profile) && self->video_filter)
      filter = self->video_filter;
    else if (GST_IS_ENCODING_AUDIO_PROFILE (profile)
        && self->audio_filter)
      filter = self->audio_filter;

    if (!filter || filter_handles_any (filter)) {
      GST_DEBUG_OBJECT (self,
          "adding %" GST_PTR_FORMAT " as output caps to decodebin", encodecaps);
      gst_caps_append (decodecaps, encodecaps);
    }
  }

  GST_OBJECT_UNLOCK (self);

  g_object_set (self->decodebin, "caps", decodecaps, NULL);
  gst_caps_unref (decodecaps);

  GST_OBJECT_LOCK (self);
}

static gboolean
make_decodebin (GstTranscodeBin * self)
{
  GstPad *pad;
  GST_INFO_OBJECT (self, "making new decodebin");

  self->decodebin = gst_element_factory_make ("decodebin3", NULL);

  g_signal_connect (self->decodebin, "pad-added",
      G_CALLBACK (decodebin_pad_added_cb), self);
  g_signal_connect (self->decodebin, "select-stream",
      G_CALLBACK (select_stream_cb), self);

  gst_bin_add (GST_BIN (self), self->decodebin);
  pad = gst_element_get_static_pad (self->decodebin, "sink");
  if (!gst_ghost_pad_set_target (GST_GHOST_PAD_CAST (self->sinkpad), pad)) {

    gst_object_unref (pad);
    GST_ERROR_OBJECT (self, "Could not ghost %" GST_PTR_FORMAT " sinkpad",
        self->decodebin);

    return FALSE;
  }

  gst_object_unref (pad);
  return TRUE;

  /* ERRORS */
}

static void
remove_all_children (GstTranscodeBin * self)
{
  if (self->encodebin) {
    gst_element_set_state (self->encodebin, GST_STATE_NULL);
    gst_bin_remove (GST_BIN (self), self->encodebin);
    self->encodebin = NULL;
  }

  if (self->video_filter && GST_OBJECT_PARENT (self->video_filter)) {
    gst_element_set_state (self->video_filter, GST_STATE_NULL);
    gst_bin_remove (GST_BIN (self), self->video_filter);
  }

  if (self->audio_filter && GST_OBJECT_PARENT (self->audio_filter)) {
    gst_element_set_state (self->audio_filter, GST_STATE_NULL);
    gst_bin_remove (GST_BIN (self), self->audio_filter);
  }
}

static gboolean
sink_event_function (GstPad * sinkpad, GstTranscodeBin * self, GstEvent * event)
{

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
      query_upstream_selectable (self, sinkpad);
      break;
    default:
      break;
  }

  return gst_pad_event_default (sinkpad, GST_OBJECT (self), event);
}

static GstStateChangeReturn
gst_transcode_bin_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstTranscodeBin *self = GST_TRANSCODE_BIN (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:

      if (!self->decodebin) {
        post_missing_plugin_error (GST_ELEMENT_CAST (self), "decodebin3");
        GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
            ("No decodebin element, check your installation"));

        goto setup_failed;
      }

      if (!make_encodebin (self))
        goto setup_failed;

      break;
    default:
      break;
  }

  ret =
      GST_ELEMENT_CLASS (gst_transcode_bin_parent_class)->change_state (element,
      transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto beach;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_OBJECT_LOCK (self);
      g_ptr_array_remove_range (self->transcoding_streams, 0,
          self->transcoding_streams->len);
      GST_OBJECT_UNLOCK (self);

      g_signal_handlers_disconnect_by_data (self->decodebin, self);

      remove_all_children (self);
      break;
    default:
      break;
  }

beach:
  return ret;

setup_failed:
  remove_all_children (self);
  return GST_STATE_CHANGE_FAILURE;
}

static void
gst_transcode_bin_dispose (GObject * object)
{
  GstTranscodeBin *self = (GstTranscodeBin *) object;

  g_clear_object (&self->video_filter);
  g_clear_object (&self->audio_filter);
  g_clear_pointer (&self->transcoding_streams, g_ptr_array_unref);
  gst_clear_object (&self->profile);

  G_OBJECT_CLASS (gst_transcode_bin_parent_class)->dispose (object);
}

static void
gst_transcode_bin_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstTranscodeBin *self = GST_TRANSCODE_BIN (object);

  switch (prop_id) {
    case PROP_PROFILE:
      GST_OBJECT_LOCK (self);
      g_value_set_object (value, self->profile);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_AVOID_REENCODING:
      GST_OBJECT_LOCK (self);
      g_value_set_boolean (value, self->avoid_reencoding);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_AUDIO_FILTER:
      GST_OBJECT_LOCK (self);
      g_value_set_object (value, self->audio_filter);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_VIDEO_FILTER:
      GST_OBJECT_LOCK (self);
      g_value_set_object (value, self->video_filter);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static GstPad *
gst_transcode_bin_request_pad (GstElement * element, GstPadTemplate * temp,
    const gchar * name, const GstCaps * caps)
{
  GstTranscodeBin *self = (GstTranscodeBin *) element;
  GstPad *gpad, *decodebin_pad =
      gst_element_request_pad_simple (self->decodebin, "sink_%u");

  if (!decodebin_pad) {
    GST_ERROR_OBJECT (element,
        "Could not request decodebin3 pad for %" GST_PTR_FORMAT, caps);

    return NULL;
  }

  gpad = gst_ghost_pad_new_from_template (name, decodebin_pad, temp);
  gst_pad_set_event_function (gpad, (GstPadEventFunction) sink_event_function);
  gst_element_add_pad (element, GST_PAD (gpad));
  gst_object_unref (decodebin_pad);

  return gpad;
}

static void
_set_filter (GstTranscodeBin * self, GstElement * filter, GstElement ** mfilter)
{
  if (filter) {
    GST_OBJECT_LOCK (filter);
    if (filter->numsinkpads != 1) {
      GST_ERROR_OBJECT (self, "Can not use %" GST_PTR_FORMAT
          " as filter as it does not have "
          " one and only one sinkpad", filter);
      goto bail_out;
    } else if (filter->numsrcpads != 1) {
      GST_ERROR_OBJECT (self, "Can not use %" GST_PTR_FORMAT
          " as filter as it does not have " " one and only one srcpad", filter);
      goto bail_out;
    }
    GST_OBJECT_UNLOCK (filter);

    gst_bin_add (GST_BIN (self), gst_object_ref (filter));
  }

  GST_OBJECT_LOCK (self);
  *mfilter = filter;
  GST_OBJECT_UNLOCK (self);

  return;

bail_out:
  GST_OBJECT_UNLOCK (filter);
}

static void
_set_profile (GstTranscodeBin * self, GstEncodingProfile * profile)
{
  GST_OBJECT_LOCK (self);
  self->profile = profile;
  _setup_avoid_reencoding (self);
  GST_OBJECT_UNLOCK (self);
}

static void
gst_transcode_bin_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstTranscodeBin *self = GST_TRANSCODE_BIN (object);

  switch (prop_id) {
    case PROP_PROFILE:
      _set_profile (self, g_value_dup_object (value));
      break;
    case PROP_AVOID_REENCODING:
      GST_OBJECT_LOCK (self);
      self->avoid_reencoding = g_value_get_boolean (value);
      _setup_avoid_reencoding (self);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_AUDIO_FILTER:
      _set_filter (self, g_value_dup_object (value), &self->audio_filter);
      break;
    case PROP_VIDEO_FILTER:
      _set_filter (self, g_value_dup_object (value), &self->video_filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_transcode_bin_class_init (GstTranscodeBinClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_klass;

  object_class->dispose = gst_transcode_bin_dispose;
  object_class->get_property = gst_transcode_bin_get_property;
  object_class->set_property = gst_transcode_bin_set_property;

  GST_DEBUG_CATEGORY_INIT (gst_transcodebin_debug, "transcodebin", 0,
      "Transcodebin element");

  gstelement_klass = (GstElementClass *) klass;
  gstelement_klass->change_state =
      GST_DEBUG_FUNCPTR (gst_transcode_bin_change_state);
  gstelement_klass->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_transcode_bin_request_pad);

  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&transcode_bin_sink_template));
  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&transcode_bin_sinks_template));
  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&transcode_bin_src_template));

  gst_element_class_set_static_metadata (gstelement_klass,
      "Transcode Bin", "Generic/Bin/Encoding",
      "Autoplug and transcoder a stream",
      "Thibault Saunier <tsaunier@igalia.com>");

  /**
   * GstTranscodeBin:profile:
   *
   * The #GstEncodingProfile to use. This property must be set before going
   * to %GST_STATE_PAUSED or higher.
   */
  g_object_class_install_property (object_class, PROP_PROFILE,
      g_param_spec_object ("profile", "Profile",
          "The GstEncodingProfile to use", GST_TYPE_ENCODING_PROFILE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstTranscodeBin:avoid-reencoding:
   *
   * See #encodebin:avoid-reencoding
   */
  g_object_class_install_property (object_class, PROP_AVOID_REENCODING,
      g_param_spec_boolean ("avoid-reencoding", "Avoid re-encoding",
          "Whether to re-encode portions of compatible video streams that lay on segment boundaries",
          DEFAULT_AVOID_REENCODING,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstTranscodeBin:video-filter:
   *
   * Set the video filter element/bin to use.
   */
  g_object_class_install_property (object_class, PROP_VIDEO_FILTER,
      g_param_spec_object ("video-filter", "Video filter",
          "the video filter(s) to apply, if possible",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  /**
   * GstTranscodeBin:audio-filter:
   *
   * Set the audio filter element/bin to use.
   */
  g_object_class_install_property (object_class, PROP_AUDIO_FILTER,
      g_param_spec_object ("audio-filter", "Audio filter",
          "the audio filter(s) to apply, if possible",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
}

static void
gst_transcode_bin_init (GstTranscodeBin * self)
{
  GstPadTemplate *pad_tmpl;

  pad_tmpl = gst_static_pad_template_get (&transcode_bin_sink_template);
  self->sinkpad = gst_ghost_pad_new_no_target_from_template ("sink", pad_tmpl);
  gst_pad_set_active (self->sinkpad, TRUE);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  gst_object_unref (pad_tmpl);

  self->transcoding_streams =
      g_ptr_array_new_with_free_func ((GDestroyNotify) transcoding_stream_free);

  make_decodebin (self);
}
