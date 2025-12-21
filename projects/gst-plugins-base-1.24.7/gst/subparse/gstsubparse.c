/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2004 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2016 Philippe Normand <pnormand@igalia.com>
 * Copyright (C) 2016 Jan Schmidt <jan@centricular.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <glib.h>

#include "gstsubparse.h"

#include "gstssaparse.h"
#include "samiparse.h"
#include "tmplayerparse.h"
#include "mpl2parse.h"
#include "qttextparse.h"
#include "gstsubparseelements.h"

#define DEFAULT_ENCODING   NULL
#define ATTRIBUTE_REGEX "\\s?[a-zA-Z0-9\\. \t\\(\\)]*"
static const gchar *allowed_srt_tags[] = { "i", "b", "u", NULL };
static const gchar *allowed_vtt_tags[] =
    { "i", "b", "c", "u", "v", "ruby", "rt", NULL };

enum
{
  PROP_0,
  PROP_ENCODING,
  PROP_VIDEOFPS
};

static void
gst_sub_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_sub_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);


static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-subtitle; application/x-subtitle-sami; "
        "application/x-subtitle-tmplayer; application/x-subtitle-mpl2; "
        "application/x-subtitle-dks; application/x-subtitle-qttext;"
        "application/x-subtitle-lrc; application/x-subtitle-vtt")
    );

static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-raw, format= { pango-markup, utf8 }")
    );


static gboolean gst_sub_parse_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_sub_parse_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_sub_parse_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static GstStateChangeReturn gst_sub_parse_change_state (GstElement * element,
    GstStateChange transition);

static GstFlowReturn gst_sub_parse_chain (GstPad * sinkpad, GstObject * parent,
    GstBuffer * buf);

#define gst_sub_parse_parent_class parent_class
G_DEFINE_TYPE (GstSubParse, gst_sub_parse, GST_TYPE_ELEMENT);

GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (subparse, "subparse",
    GST_RANK_PRIMARY, GST_TYPE_SUBPARSE, sub_parse_element_init (plugin))
     static void gst_sub_parse_dispose (GObject * object)
{
  GstSubParse *subparse = GST_SUBPARSE (object);

  GST_DEBUG_OBJECT (subparse, "cleaning up subtitle parser");

  if (subparse->encoding) {
    g_free (subparse->encoding);
    subparse->encoding = NULL;
  }

  if (subparse->detected_encoding) {
    g_free (subparse->detected_encoding);
    subparse->detected_encoding = NULL;
  }

  if (subparse->adapter) {
    g_object_unref (subparse->adapter);
    subparse->adapter = NULL;
  }

  if (subparse->textbuf) {
    g_string_free (subparse->textbuf, TRUE);
    subparse->textbuf = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_sub_parse_class_init (GstSubParseClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  object_class->dispose = gst_sub_parse_dispose;
  object_class->set_property = gst_sub_parse_set_property;
  object_class->get_property = gst_sub_parse_get_property;

  gst_element_class_add_static_pad_template (element_class, &sink_templ);
  gst_element_class_add_static_pad_template (element_class, &src_templ);
  gst_element_class_set_static_metadata (element_class,
      "Subtitle parser", "Codec/Decoder/Subtitle",
      "Parses subtitle (.sub) files into text streams",
      "Gustavo J. A. M. Carneiro <gjc@inescporto.pt>, "
      "GStreamer maintainers <gstreamer-devel@lists.freedesktop.org>");

  element_class->change_state = gst_sub_parse_change_state;

  g_object_class_install_property (object_class, PROP_ENCODING,
      g_param_spec_string ("subtitle-encoding", "subtitle charset encoding",
          "Encoding to assume if input subtitles are not in UTF-8 or any other "
          "Unicode encoding. If not set, the GST_SUBTITLE_ENCODING environment "
          "variable will be checked for an encoding to use. If that is not set "
          "either, ISO-8859-15 will be assumed.", DEFAULT_ENCODING,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_VIDEOFPS,
      gst_param_spec_fraction ("video-fps", "Video framerate",
          "Framerate of the video stream. This is needed by some subtitle "
          "formats to synchronize subtitles and video properly. If not set "
          "and the subtitle format requires it subtitles may be out of sync.",
          0, 1, G_MAXINT, 1, 24000, 1001,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_sub_parse_init (GstSubParse * subparse)
{
  subparse->sinkpad = gst_pad_new_from_static_template (&sink_templ, "sink");
  gst_pad_set_chain_function (subparse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_sub_parse_chain));
  gst_pad_set_event_function (subparse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_sub_parse_sink_event));
  gst_element_add_pad (GST_ELEMENT (subparse), subparse->sinkpad);

  subparse->srcpad = gst_pad_new_from_static_template (&src_templ, "src");
  gst_pad_set_event_function (subparse->srcpad,
      GST_DEBUG_FUNCPTR (gst_sub_parse_src_event));
  gst_pad_set_query_function (subparse->srcpad,
      GST_DEBUG_FUNCPTR (gst_sub_parse_src_query));
  gst_element_add_pad (GST_ELEMENT (subparse), subparse->srcpad);

  subparse->textbuf = g_string_new (NULL);
  subparse->parser_type = GST_SUB_PARSE_FORMAT_UNKNOWN;
  subparse->strip_pango_markup = FALSE;
  subparse->flushing = FALSE;
  gst_segment_init (&subparse->segment, GST_FORMAT_TIME);
  subparse->segment_seqnum = gst_util_seqnum_next ();
  subparse->need_segment = TRUE;
  subparse->encoding = g_strdup (DEFAULT_ENCODING);
  subparse->detected_encoding = NULL;
  subparse->adapter = gst_adapter_new ();

  subparse->fps_n = 24000;
  subparse->fps_d = 1001;
}

/*
 * Source pad functions.
 */

static gboolean
gst_sub_parse_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstSubParse *self = GST_SUBPARSE (parent);
  gboolean ret = FALSE;

  GST_DEBUG ("Handling %s query", GST_QUERY_TYPE_NAME (query));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:{
      GstFormat fmt;

      gst_query_parse_position (query, &fmt, NULL);
      if (fmt != GST_FORMAT_TIME) {
        ret = gst_pad_peer_query (self->sinkpad, query);
      } else {
        ret = TRUE;
        gst_query_set_position (query, GST_FORMAT_TIME, self->segment.position);
      }
      break;
    }
    case GST_QUERY_SEEKING:
    {
      GstFormat fmt;
      gboolean seekable = FALSE;

      ret = TRUE;

      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);
      if (fmt == GST_FORMAT_TIME) {
        GstQuery *peerquery = gst_query_new_seeking (GST_FORMAT_BYTES);

        seekable = gst_pad_peer_query (self->sinkpad, peerquery);
        if (seekable)
          gst_query_parse_seeking (peerquery, NULL, &seekable, NULL, NULL);
        gst_query_unref (peerquery);
      }

      gst_query_set_seeking (query, fmt, seekable, seekable ? 0 : -1, -1);
      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

  return ret;
}

static gboolean
gst_sub_parse_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSubParse *self = GST_SUBPARSE (parent);
  gboolean ret = FALSE;

  GST_DEBUG ("Handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      GstFormat format;
      GstSeekFlags flags;
      GstSeekType start_type, stop_type;
      gint64 start, stop;
      gdouble rate;
      gboolean update;

      gst_event_parse_seek (event, &rate, &format, &flags,
          &start_type, &start, &stop_type, &stop);

      if (format != GST_FORMAT_TIME) {
        GST_WARNING_OBJECT (self, "we only support seeking in TIME format");
        gst_event_unref (event);
        goto beach;
      }

      /* Convert that seek to a seeking in bytes at position 0,
         FIXME: could use an index */
      ret = gst_pad_push_event (self->sinkpad,
          gst_event_new_seek (rate, GST_FORMAT_BYTES, flags,
              GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_NONE, 0));

      if (ret) {
        /* Apply the seek to our segment */
        gst_segment_do_seek (&self->segment, rate, format, flags,
            start_type, start, stop_type, stop, &update);

        GST_DEBUG_OBJECT (self, "segment after seek: %" GST_SEGMENT_FORMAT,
            &self->segment);

        /* will mark need_segment when receiving segment from upstream,
         * after FLUSH and all that has happened,
         * rather than racing with chain */
      } else {
        GST_WARNING_OBJECT (self, "seek to 0 bytes failed");
      }

      gst_event_unref (event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

beach:
  return ret;
}

static void
gst_sub_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSubParse *subparse = GST_SUBPARSE (object);

  GST_OBJECT_LOCK (subparse);
  switch (prop_id) {
    case PROP_ENCODING:
      g_free (subparse->encoding);
      subparse->encoding = g_value_dup_string (value);
      GST_LOG_OBJECT (object, "subtitle encoding set to %s",
          GST_STR_NULL (subparse->encoding));
      break;
    case PROP_VIDEOFPS:
    {
      subparse->fps_n = gst_value_get_fraction_numerator (value);
      subparse->fps_d = gst_value_get_fraction_denominator (value);
      GST_DEBUG_OBJECT (object, "video framerate set to %d/%d", subparse->fps_n,
          subparse->fps_d);

      if (!subparse->state.have_internal_fps) {
        subparse->state.fps_n = subparse->fps_n;
        subparse->state.fps_d = subparse->fps_d;
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (subparse);
}

static void
gst_sub_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSubParse *subparse = GST_SUBPARSE (object);

  GST_OBJECT_LOCK (subparse);
  switch (prop_id) {
    case PROP_ENCODING:
      g_value_set_string (value, subparse->encoding);
      break;
    case PROP_VIDEOFPS:
      gst_value_set_fraction (value, subparse->fps_n, subparse->fps_d);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (subparse);
}

static const gchar *
gst_sub_parse_get_format_description (GstSubParseFormat format)
{
  switch (format) {
    case GST_SUB_PARSE_FORMAT_MDVDSUB:
      return "MicroDVD";
    case GST_SUB_PARSE_FORMAT_SUBRIP:
      return "SubRip";
    case GST_SUB_PARSE_FORMAT_MPSUB:
      return "MPSub";
    case GST_SUB_PARSE_FORMAT_SAMI:
      return "SAMI";
    case GST_SUB_PARSE_FORMAT_TMPLAYER:
      return "TMPlayer";
    case GST_SUB_PARSE_FORMAT_MPL2:
      return "MPL2";
    case GST_SUB_PARSE_FORMAT_SUBVIEWER:
      return "SubViewer";
    case GST_SUB_PARSE_FORMAT_DKS:
      return "DKS";
    case GST_SUB_PARSE_FORMAT_VTT:
      return "WebVTT";
    case GST_SUB_PARSE_FORMAT_QTTEXT:
      return "QTtext";
    case GST_SUB_PARSE_FORMAT_LRC:
      return "LRC";
    default:
    case GST_SUB_PARSE_FORMAT_UNKNOWN:
      break;
  }
  return NULL;
}





static gchar *
convert_encoding (GstSubParse * self, const gchar * str, gsize len,
    gsize * consumed)
{
  const gchar *encoding;
  GError *err = NULL;
  gchar *ret = NULL;

  *consumed = 0;

  /* First try any detected encoding */
  if (self->detected_encoding) {
    ret =
        gst_sub_parse_gst_convert_to_utf8 (str, len, self->detected_encoding,
        consumed, &err);

    if (!err)
      return ret;

    GST_WARNING_OBJECT (self, "could not convert string from '%s' to UTF-8: %s",
        self->detected_encoding, err->message);
    g_free (self->detected_encoding);
    self->detected_encoding = NULL;
    g_clear_error (&err);
  }

  /* Otherwise check if it's UTF8 */
  if (self->valid_utf8) {
    if (g_utf8_validate (str, len, NULL)) {
      GST_LOG_OBJECT (self, "valid UTF-8, no conversion needed");
      *consumed = len;
      return g_strndup (str, len);
    }
    GST_INFO_OBJECT (self, "invalid UTF-8!");
    self->valid_utf8 = FALSE;
  }

  /* Else try fallback */
  encoding = self->encoding;
  if (encoding == NULL || *encoding == '\0') {
    encoding = g_getenv ("GST_SUBTITLE_ENCODING");
  }
  if (encoding == NULL || *encoding == '\0') {
    /* if local encoding is UTF-8 and no encoding specified
     * via the environment variable, assume ISO-8859-15 */
    if (g_get_charset (&encoding)) {
      encoding = "ISO-8859-15";
    }
  }

  ret = gst_sub_parse_gst_convert_to_utf8 (str, len, encoding, consumed, &err);

  if (err) {
    GST_WARNING_OBJECT (self, "could not convert string from '%s' to UTF-8: %s",
        encoding, err->message);
    g_clear_error (&err);

    /* invalid input encoding, fall back to ISO-8859-15 (always succeeds) */
    ret =
        gst_sub_parse_gst_convert_to_utf8 (str, len, "ISO-8859-15", consumed,
        NULL);
  }

  GST_LOG_OBJECT (self,
      "successfully converted %" G_GSIZE_FORMAT " characters from %s to UTF-8"
      "%s", len, encoding, (err) ? " , using ISO-8859-15 as fallback" : "");

  return ret;
}

static gchar *
get_next_line (GstSubParse * self)
{
  char *line = NULL;
  const char *line_end;
  int line_len;
  gboolean have_r = FALSE;

  line_end = strchr (self->textbuf->str, '\n');

  if (!line_end) {
    /* end-of-line not found; return for more data */
    return NULL;
  }

  /* get rid of '\r' */
  if (line_end != self->textbuf->str && *(line_end - 1) == '\r') {
    line_end--;
    have_r = TRUE;
  }

  line_len = line_end - self->textbuf->str;
  line = g_strndup (self->textbuf->str, line_len);
  self->textbuf = g_string_erase (self->textbuf, 0,
      line_len + (have_r ? 2 : 1));
  return line;
}

static gchar *
parse_mdvdsub (ParserState * state, const gchar * line)
{
  const gchar *line_split;
  gchar *line_chunk;
  guint start_frame, end_frame;
  guint64 clip_start = 0, clip_stop = 0;
  gboolean in_seg = FALSE;
  GString *markup;
  gchar *ret;

  /* style variables */
  gboolean italic;
  gboolean bold;
  guint fontsize;
  gdouble fps = 0.0;

  if (sscanf (line, "{%u}{%u}", &start_frame, &end_frame) != 2) {
    GST_WARNING ("Parsing of the following line, assumed to be in microdvd .sub"
        " format, failed:\n%s", line);
    return NULL;
  }

  /* skip the {%u}{%u} part */
  line = strchr (line, '}') + 1;
  line = strchr (line, '}') + 1;

  /* see if there's a first line with a framerate */
  if (start_frame == 1 && end_frame == 1) {
    gchar *rest, *end = NULL;

    rest = g_strdup (line);
    g_strdelimit (rest, ",", '.');
    fps = g_ascii_strtod (rest, &end);
    if (end != rest) {
      gst_util_double_to_fraction (fps, &state->fps_n, &state->fps_d);
      GST_INFO ("framerate from file: %d/%d ('%s')", state->fps_n,
          state->fps_d, rest);
    }
    g_free (rest);
    return NULL;
  }

  state->start_time =
      gst_util_uint64_scale (start_frame, GST_SECOND * state->fps_d,
      state->fps_n);
  state->duration =
      gst_util_uint64_scale (end_frame - start_frame, GST_SECOND * state->fps_d,
      state->fps_n);

  /* Check our segment start/stop */
  in_seg = gst_segment_clip (state->segment, GST_FORMAT_TIME,
      state->start_time, state->start_time + state->duration, &clip_start,
      &clip_stop);

  /* No need to parse that text if it's out of segment */
  if (in_seg) {
    state->start_time = clip_start;
    state->duration = clip_stop - clip_start;
  } else {
    return NULL;
  }

  markup = g_string_new (NULL);
  while (1) {
    italic = FALSE;
    bold = FALSE;
    fontsize = 0;
    /* parse style markup */
    if (strncmp (line, "{y:i}", 5) == 0) {
      italic = TRUE;
      line = strchr (line, '}') + 1;
    }
    if (strncmp (line, "{y:b}", 5) == 0) {
      bold = TRUE;
      line = strchr (line, '}') + 1;
    }
    if (sscanf (line, "{s:%u}", &fontsize) == 1) {
      line = strchr (line, '}') + 1;
    }
    /* forward slashes at beginning/end signify italics too */
    if (g_str_has_prefix (line, "/")) {
      italic = TRUE;
      ++line;
    }
    if ((line_split = strchr (line, '|')))
      line_chunk = g_markup_escape_text (line, line_split - line);
    else
      line_chunk = g_markup_escape_text (line, strlen (line));

    /* Remove italics markers at end of line/stanza (CHECKME: are end slashes
     * always at the end of a line or can they span multiple lines?) */
    if (g_str_has_suffix (line_chunk, "/")) {
      line_chunk[strlen (line_chunk) - 1] = '\0';
    }

    markup = g_string_append (markup, "<span");
    if (italic)
      g_string_append (markup, " style=\"italic\"");
    if (bold)
      g_string_append (markup, " weight=\"bold\"");
    if (fontsize)
      g_string_append_printf (markup, " size=\"%u\"", fontsize * 1000);
    g_string_append_printf (markup, ">%s</span>", line_chunk);
    g_free (line_chunk);
    if (line_split) {
      g_string_append (markup, "\n");
      line = line_split + 1;
    } else {
      break;
    }
  }
  ret = g_string_free (markup, FALSE);
  GST_DEBUG ("parse_mdvdsub returning (%f+%f): %s",
      state->start_time / (double) GST_SECOND,
      state->duration / (double) GST_SECOND, ret);
  return ret;
}

static void
strip_trailing_newlines (gchar * txt)
{
  if (txt) {
    guint len;

    len = strlen (txt);
    while (len > 1 && txt[len - 1] == '\n') {
      txt[len - 1] = '\0';
      --len;
    }
  }
}

/* we want to escape text in general, but retain basic markup like
 * <i></i>, <u></u>, and <b></b>. The easiest and safest way is to
 * just unescape a white list of allowed markups again after
 * escaping everything (the text between these simple markers isn't
 * necessarily escaped, so it seems best to do it like this) */
static void
subrip_unescape_formatting (gchar * txt, gconstpointer allowed_tags_ptr,
    gboolean allows_tag_attributes)
{
  gchar *res;
  GRegex *tag_regex;
  gchar *allowed_tags_pattern, *search_pattern;
  const gchar *replace_pattern;

  /* No processing needed if no escaped tag marker found in the string. */
  if (strstr (txt, "&lt;") == NULL)
    return;

  /* Build a list of alternates for our regexp.
   * FIXME: Could be built once and stored */
  allowed_tags_pattern = g_strjoinv ("|", (gchar **) allowed_tags_ptr);
  /* Look for starting/ending escaped tags with optional attributes. */
  search_pattern = g_strdup_printf ("&lt;(/)?\\ *(%s)(%s)&gt;",
      allowed_tags_pattern, ATTRIBUTE_REGEX);
  /* And unescape appropriately */
  if (allows_tag_attributes) {
    replace_pattern = "<\\1\\2\\3>";
  } else {
    replace_pattern = "<\\1\\2>";
  }

  tag_regex = g_regex_new (search_pattern, 0, 0, NULL);
  res = g_regex_replace (tag_regex, txt, strlen (txt), 0,
      replace_pattern, 0, NULL);

  /* res will always be shorter than the input or identical, so this
   * copy is OK */
  strcpy (txt, res);

  g_free (res);
  g_free (search_pattern);
  g_free (allowed_tags_pattern);

  g_regex_unref (tag_regex);
}


static gboolean
subrip_remove_unhandled_tag (gchar * start, gchar * stop)
{
  gchar *tag, saved;

  tag = start + strlen ("&lt;");
  if (*tag == '/')
    ++tag;

  if (g_ascii_tolower (*tag) < 'a' || g_ascii_tolower (*tag) > 'z')
    return FALSE;

  saved = *stop;
  *stop = '\0';
  GST_LOG ("removing unhandled tag '%s'", start);
  *stop = saved;
  memmove (start, stop, strlen (stop) + 1);
  return TRUE;
}

/* remove tags we haven't explicitly allowed earlier on, like font tags
 * for example */
static void
subrip_remove_unhandled_tags (gchar * txt)
{
  gchar *pos, *gt;

  for (pos = txt; pos != NULL && *pos != '\0'; ++pos) {
    if (strncmp (pos, "&lt;", 4) == 0 && (gt = strstr (pos + 4, "&gt;"))) {
      if (subrip_remove_unhandled_tag (pos, gt + strlen ("&gt;")))
        --pos;
    }
  }
}

/* we only allow a fixed set of tags like <i>, <u> and <b>, so let's
 * take a simple approach. This code assumes the input has been
 * escaped and subrip_unescape_formatting() has then been run over the
 * input! This function adds missing closing markup tags and removes
 * broken closing tags for tags that have never been opened. */
static void
subrip_fix_up_markup (gchar ** p_txt, gconstpointer allowed_tags_ptr)
{
  gchar *cur, *next_tag;
  GPtrArray *open_tags = NULL;
  guint num_open_tags = 0;
  const gchar *iter_tag;
  guint offset = 0;
  guint index;
  gchar *cur_tag;
  gchar *end_tag;
  GRegex *tag_regex;
  GMatchInfo *match_info;
  gchar **allowed_tags = (gchar **) allowed_tags_ptr;

  g_assert (*p_txt != NULL);

  open_tags = g_ptr_array_new_with_free_func (g_free);
  cur = *p_txt;
  while (*cur != '\0') {
    next_tag = strchr (cur, '<');
    if (next_tag == NULL)
      break;
    offset = 0;
    index = 0;
    while (index < g_strv_length (allowed_tags)) {
      iter_tag = allowed_tags[index];
      /* Look for a white listed tag */
      cur_tag = g_strconcat ("<", iter_tag, ATTRIBUTE_REGEX, ">", NULL);
      tag_regex = g_regex_new (cur_tag, 0, 0, NULL);
      (void) g_regex_match (tag_regex, next_tag, 0, &match_info);

      if (g_match_info_matches (match_info)) {
        gint start_pos, end_pos;
        gchar *word = g_match_info_fetch (match_info, 0);
        g_match_info_fetch_pos (match_info, 0, &start_pos, &end_pos);
        if (start_pos == 0) {
          offset = strlen (word);
        }
        g_free (word);
      }
      g_match_info_free (match_info);
      g_regex_unref (tag_regex);
      g_free (cur_tag);
      index++;
      if (offset) {
        /* OK we found a tag, let's keep track of it */
        g_ptr_array_add (open_tags, g_ascii_strdown (iter_tag, -1));
        ++num_open_tags;
        break;
      }
    }

    if (offset) {
      next_tag += offset;
      cur = next_tag;
      continue;
    }

    if (*next_tag == '<' && *(next_tag + 1) == '/') {
      end_tag = strchr (next_tag, '>');
      if (end_tag) {
        const gchar *last = NULL;
        if (num_open_tags > 0)
          last = g_ptr_array_index (open_tags, num_open_tags - 1);
        if (num_open_tags == 0
            || g_ascii_strncasecmp (end_tag - 1, last, strlen (last))) {
          GST_LOG ("broken input, closing tag '%s' is not open", next_tag);
          /* Move everything after the tag end, including closing \0 */
          memmove (next_tag, end_tag + 1, strlen (end_tag));
          cur = next_tag;
          continue;
        } else {
          --num_open_tags;
          g_ptr_array_remove_index (open_tags, num_open_tags);
          cur = end_tag + 1;
          continue;
        }
      }
    }
    ++next_tag;
    cur = next_tag;
  }

  if (num_open_tags > 0) {
    GString *s;

    s = g_string_new (*p_txt);
    while (num_open_tags > 0) {
      GST_LOG ("adding missing closing tag '%s'",
          (char *) g_ptr_array_index (open_tags, num_open_tags - 1));
      g_string_append_c (s, '<');
      g_string_append_c (s, '/');
      g_string_append (s, g_ptr_array_index (open_tags, num_open_tags - 1));
      g_string_append_c (s, '>');
      --num_open_tags;
    }
    g_free (*p_txt);
    *p_txt = g_string_free (s, FALSE);
  }
  g_ptr_array_free (open_tags, TRUE);
}

static gboolean
parse_subrip_time (const gchar * ts_string, GstClockTime * t)
{
  gchar s[128] = { '\0', };
  gchar *end, *p;
  guint hour, min, sec, msec, len;

  while (*ts_string == ' ')
    ++ts_string;

  g_strlcpy (s, ts_string, sizeof (s));
  if ((end = strstr (s, "-->")))
    *end = '\0';
  g_strchomp (s);

  /* ms may be in these formats:
   * hh:mm:ss,500 = 500ms
   * hh:mm:ss,  5 =   5ms
   * hh:mm:ss, 5  =  50ms
   * hh:mm:ss, 50 =  50ms
   * hh:mm:ss,5   = 500ms
   * and the same with . instead of ,.
   * sscanf() doesn't differentiate between '  5' and '5' so munge
   * the white spaces within the timestamp to '0' (I'm sure there's a
   * way to make sscanf() do this for us, but how?)
   */
  g_strdelimit (s, " ", '0');
  g_strdelimit (s, ".", ',');

  /* make sure we have exactly three digits after he comma */
  p = strchr (s, ',');
  if (p == NULL) {
    /* If there isn't a ',' the timestamp is broken */
    /* https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/issues/532#note_100179 */
    GST_WARNING ("failed to parse subrip timestamp string '%s'", s);
    return FALSE;
  }

  ++p;
  len = strlen (p);
  if (len > 3) {
    p[3] = '\0';
  } else
    while (len < 3) {
      g_strlcat (&p[len], "0", 2);
      ++len;
    }

  GST_LOG ("parsing timestamp '%s'", s);
  if (sscanf (s, "%u:%u:%u,%u", &hour, &min, &sec, &msec) != 4) {
    /* https://www.w3.org/TR/webvtt1/#webvtt-timestamp
     *
     * The hours component is optional with webVTT, for example
     * mm:ss,500 is a valid webVTT timestamp. When not present,
     * hours is 0.
     */
    hour = 0;

    if (sscanf (s, "%u:%u,%u", &min, &sec, &msec) != 3) {
      GST_WARNING ("failed to parse subrip timestamp string '%s'", s);
      return FALSE;
    }
  }

  *t = ((hour * 3600) + (min * 60) + sec) * GST_SECOND + msec * GST_MSECOND;
  return TRUE;
}

/* cue settings are part of the WebVTT specification. They are
 * declared after the time interval in the first line of the
 * cue. Example: 00:00:01,000 --> 00:00:02,000 D:vertical-lr A:start
 * See also http://www.whatwg.org/specs/web-apps/current-work/webvtt.html
 */
static void
parse_webvtt_cue_settings (ParserState * state, const gchar * settings)
{
  gchar **splitted_settings = g_strsplit_set (settings, " \t", -1);
  gint i = 0;
  gint16 text_position, text_size;
  gint16 line_position;
  gboolean vertical_found = FALSE;
  gboolean alignment_found = FALSE;

  while (i < g_strv_length (splitted_settings)) {
    gboolean valid_tag = FALSE;
    switch (splitted_settings[i][0]) {
      case 'T':
        if (sscanf (splitted_settings[i], "T:%" G_GINT16_FORMAT "%%",
                &text_position) > 0) {
          state->text_position = (guint8) text_position;
          valid_tag = TRUE;
        }
        break;
      case 'D':
        if (strlen (splitted_settings[i]) > 2) {
          vertical_found = TRUE;
          g_free (state->vertical);
          state->vertical = g_strdup (splitted_settings[i] + 2);
          valid_tag = TRUE;
        }
        break;
      case 'L':
        if (g_str_has_suffix (splitted_settings[i], "%")) {
          if (sscanf (splitted_settings[i], "L:%" G_GINT16_FORMAT "%%",
                  &line_position) > 0) {
            state->line_position = line_position;
            valid_tag = TRUE;
          }
        } else {
          if (sscanf (splitted_settings[i], "L:%" G_GINT16_FORMAT,
                  &line_position) > 0) {
            state->line_number = line_position;
            valid_tag = TRUE;
          }
        }
        break;
      case 'S':
        if (sscanf (splitted_settings[i], "S:%" G_GINT16_FORMAT "%%",
                &text_size) > 0) {
          state->text_size = (guint8) text_size;
          valid_tag = TRUE;
        }
        break;
      case 'A':
        if (strlen (splitted_settings[i]) > 2) {
          g_free (state->alignment);
          state->alignment = g_strdup (splitted_settings[i] + 2);
          alignment_found = TRUE;
          valid_tag = TRUE;
        }
        break;
      default:
        break;
    }
    if (!valid_tag) {
      GST_LOG ("Invalid or unrecognised setting found: %s",
          splitted_settings[i]);
    }
    i++;
  }
  g_strfreev (splitted_settings);
  if (!vertical_found) {
    g_free (state->vertical);
    state->vertical = g_strdup ("");
  }
  if (!alignment_found) {
    g_free (state->alignment);
    state->alignment = g_strdup ("");
  }
}

static gchar *
parse_subrip (ParserState * state, const gchar * line)
{
  gchar *ret;

  switch (state->state) {
    case 0:{
      char *endptr;
      guint64 id;

      /* looking for a single integer as a Cue ID, but we
       * don't actually use it */
      errno = 0;
      id = g_ascii_strtoull (line, &endptr, 10);
      if (id == G_MAXUINT64 && errno == ERANGE)
        state->state = 1;
      else if (id == 0 && errno == EINVAL)
        state->state = 1;
      else if (endptr != line && *endptr == '\0')
        state->state = 1;
      return NULL;
    }
    case 1:
    {
      GstClockTime ts_start, ts_end;
      gchar *end_time;

      /* looking for start_time --> end_time */
      if ((end_time = strstr (line, " --> ")) &&
          parse_subrip_time (line, &ts_start) &&
          parse_subrip_time (end_time + strlen (" --> "), &ts_end) &&
          state->start_time <= ts_end) {
        state->state = 2;
        state->start_time = ts_start;
        state->duration = ts_end - ts_start;
      } else {
        GST_DEBUG ("error parsing subrip time line '%s'", line);
        state->state = 0;
      }
      return NULL;
    }
    case 2:
    {
      /* No need to parse that text if it's out of segment */
      guint64 clip_start = 0, clip_stop = 0;
      gboolean in_seg = FALSE;

      /* Check our segment start/stop */
      in_seg = gst_segment_clip (state->segment, GST_FORMAT_TIME,
          state->start_time, state->start_time + state->duration,
          &clip_start, &clip_stop);

      if (in_seg) {
        state->start_time = clip_start;
        state->duration = clip_stop - clip_start;
      } else {
        state->state = 0;
        return NULL;
      }
    }
      /* looking for subtitle text; empty line ends this subtitle entry */
      if (state->buf->len)
        g_string_append_c (state->buf, '\n');
      g_string_append (state->buf, line);
      if (strlen (line) == 0) {
        ret = g_markup_escape_text (state->buf->str, state->buf->len);
        g_string_truncate (state->buf, 0);
        state->state = 0;
        subrip_unescape_formatting (ret, state->allowed_tags,
            state->allows_tag_attributes);
        subrip_remove_unhandled_tags (ret);
        strip_trailing_newlines (ret);
        subrip_fix_up_markup (&ret, state->allowed_tags);
        return ret;
      }
      return NULL;
    default:
      g_return_val_if_reached (NULL);
  }
}

static gchar *
parse_lrc (ParserState * state, const gchar * line)
{
  gint m, s, c;
  const gchar *start;
  gint milli;

  if (line[0] != '[')
    return NULL;

  if (sscanf (line, "[%u:%02u.%03u]", &m, &s, &c) != 3 &&
      sscanf (line, "[%u:%02u.%02u]", &m, &s, &c) != 3)
    return NULL;

  start = strchr (line, ']');
  if (start - line == 9)
    milli = 10;
  else
    milli = 1;

  state->start_time = gst_util_uint64_scale (m, 60 * GST_SECOND, 1)
      + gst_util_uint64_scale (s, GST_SECOND, 1)
      + gst_util_uint64_scale (c, milli * GST_MSECOND, 1);
  state->duration = GST_CLOCK_TIME_NONE;

  return g_strdup (start + 1);
}

/* WebVTT is a new subtitle format for the upcoming HTML5 video track
 * element. This format is similar to Subrip, the biggest differences
 * are that there can be cue settings detailing how to display the cue
 * text and more markup tags are allowed.
 * See also http://www.whatwg.org/specs/web-apps/current-work/webvtt.html
 */
static gchar *
parse_webvtt (ParserState * state, const gchar * line)
{
  /* Cue IDs are optional in WebVTT, but not in subrip,
   * so when in state 0 (cue ID), also check if we're
   * already at the start --> end time marker */
  if (state->state == 0 || state->state == 1) {
    GstClockTime ts_start, ts_end;
    gchar *end_time;
    gchar *cue_settings = NULL;

    /* looking for start_time --> end_time */
    if ((end_time = strstr (line, " --> ")) &&
        parse_subrip_time (line, &ts_start) &&
        parse_subrip_time (end_time + strlen (" --> "), &ts_end) &&
        state->start_time <= ts_end) {
      state->state = 2;
      state->start_time = ts_start;
      state->duration = ts_end - ts_start;
      cue_settings = strstr (end_time + strlen (" --> "), " ");
    } else {
      GST_DEBUG ("error parsing subrip time line '%s'", line);
      state->state = 0;
    }

    state->text_position = 0;
    state->text_size = 0;
    state->line_position = 0;
    state->line_number = 0;

    if (cue_settings)
      parse_webvtt_cue_settings (state, cue_settings + 1);
    else {
      g_free (state->vertical);
      state->vertical = g_strdup ("");
      g_free (state->alignment);
      state->alignment = g_strdup ("");
    }

    return NULL;
  } else
    return parse_subrip (state, line);
}

static void
unescape_newlines_br (gchar * read)
{
  gchar *write = read;

  /* Replace all occurrences of '[br]' with a newline as version 2
   * of the subviewer format uses this for newlines */

  if (read[0] == '\0' || read[1] == '\0' || read[2] == '\0' || read[3] == '\0')
    return;

  do {
    if (strncmp (read, "[br]", 4) == 0) {
      *write = '\n';
      read += 4;
    } else {
      *write = *read;
      read++;
    }
    write++;
  } while (*read);

  *write = '\0';
}

static gchar *
parse_subviewer (ParserState * state, const gchar * line)
{
  guint h1, m1, s1, ms1;
  guint h2, m2, s2, ms2;
  gchar *ret;

  /* TODO: Maybe also parse the fields in the header, especially DELAY.
   * For examples see the unit test or
   * http://www.doom9.org/index.html?/sub.htm */

  switch (state->state) {
    case 0:
      /* looking for start_time,end_time */
      if (sscanf (line, "%u:%u:%u.%u,%u:%u:%u.%u",
              &h1, &m1, &s1, &ms1, &h2, &m2, &s2, &ms2) == 8) {
        state->state = 1;
        state->start_time =
            (((guint64) h1) * 3600 + m1 * 60 + s1) * GST_SECOND +
            ms1 * GST_MSECOND;
        state->duration =
            (((guint64) h2) * 3600 + m2 * 60 + s2) * GST_SECOND +
            ms2 * GST_MSECOND - state->start_time;
      }
      return NULL;
    case 1:
    {
      /* No need to parse that text if it's out of segment */
      guint64 clip_start = 0, clip_stop = 0;
      gboolean in_seg = FALSE;

      /* Check our segment start/stop */
      in_seg = gst_segment_clip (state->segment, GST_FORMAT_TIME,
          state->start_time, state->start_time + state->duration,
          &clip_start, &clip_stop);

      if (in_seg) {
        state->start_time = clip_start;
        state->duration = clip_stop - clip_start;
      } else {
        state->state = 0;
        return NULL;
      }
    }
      /* looking for subtitle text; empty line ends this subtitle entry */
      if (state->buf->len)
        g_string_append_c (state->buf, '\n');
      g_string_append (state->buf, line);
      if (strlen (line) == 0) {
        ret = g_strdup (state->buf->str);
        unescape_newlines_br (ret);
        strip_trailing_newlines (ret);
        g_string_truncate (state->buf, 0);
        state->state = 0;
        return ret;
      }
      return NULL;
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

static gchar *
parse_mpsub (ParserState * state, const gchar * line)
{
  gchar *ret;
  float t1, t2;

  switch (state->state) {
    case 0:
      /* looking for two floats (offset, duration) */
      if (sscanf (line, "%f %f", &t1, &t2) == 2) {
        state->state = 1;
        state->start_time += state->duration + GST_SECOND * t1;
        state->duration = GST_SECOND * t2;
      }
      return NULL;
    case 1:
    {                           /* No need to parse that text if it's out of segment */
      guint64 clip_start = 0, clip_stop = 0;
      gboolean in_seg = FALSE;

      /* Check our segment start/stop */
      in_seg = gst_segment_clip (state->segment, GST_FORMAT_TIME,
          state->start_time, state->start_time + state->duration,
          &clip_start, &clip_stop);

      if (in_seg) {
        state->start_time = clip_start;
        state->duration = clip_stop - clip_start;
      } else {
        state->state = 0;
        return NULL;
      }
    }
      /* looking for subtitle text; empty line ends this
       * subtitle entry */
      if (state->buf->len)
        g_string_append_c (state->buf, '\n');
      g_string_append (state->buf, line);
      if (strlen (line) == 0) {
        ret = g_strdup (state->buf->str);
        g_string_truncate (state->buf, 0);
        state->state = 0;
        return ret;
      }
      return NULL;
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

static const gchar *
dks_skip_timestamp (const gchar * line)
{
  while (*line && *line != ']')
    line++;
  if (*line == ']')
    line++;
  return line;
}

static gchar *
parse_dks (ParserState * state, const gchar * line)
{
  guint h, m, s;

  switch (state->state) {
    case 0:
      /* Looking for the start time and text */
      if (sscanf (line, "[%u:%u:%u]", &h, &m, &s) == 3) {
        const gchar *text;
        state->start_time = (((guint64) h) * 3600 + m * 60 + s) * GST_SECOND;
        text = dks_skip_timestamp (line);
        if (*text) {
          state->state = 1;
          g_string_append (state->buf, text);
        }
      }
      return NULL;
    case 1:
    {
      guint64 clip_start = 0, clip_stop = 0;
      gboolean in_seg;
      gchar *ret;

      /* Looking for the end time */
      if (sscanf (line, "[%u:%u:%u]", &h, &m, &s) == 3) {
        state->state = 0;
        state->duration = (((guint64) h) * 3600 + m * 60 + s) * GST_SECOND -
            state->start_time;
      } else {
        GST_WARNING ("Failed to parse subtitle end time");
        return NULL;
      }

      /* Check if this subtitle is out of the current segment */
      in_seg = gst_segment_clip (state->segment, GST_FORMAT_TIME,
          state->start_time, state->start_time + state->duration,
          &clip_start, &clip_stop);

      if (!in_seg) {
        return NULL;
      }

      state->start_time = clip_start;
      state->duration = clip_stop - clip_start;

      ret = g_strdup (state->buf->str);
      g_string_truncate (state->buf, 0);
      unescape_newlines_br (ret);
      return ret;
    }
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

static void
parser_state_init (ParserState * state)
{
  GST_DEBUG ("initialising parser");

  if (state->buf) {
    g_string_truncate (state->buf, 0);
  } else {
    state->buf = g_string_new (NULL);
  }

  state->start_time = 0;
  state->duration = 0;
  state->max_duration = 0;      /* no limit */
  state->state = 0;
  state->segment = NULL;
}

static void
parser_state_dispose (GstSubParse * self, ParserState * state)
{
  if (state->buf) {
    g_string_free (state->buf, TRUE);
    state->buf = NULL;
  }

  g_free (state->vertical);
  state->vertical = NULL;
  g_free (state->alignment);
  state->alignment = NULL;

  if (state->user_data) {
    switch (self->parser_type) {
      case GST_SUB_PARSE_FORMAT_QTTEXT:
        qttext_context_deinit (state);
        break;
      case GST_SUB_PARSE_FORMAT_SAMI:
        sami_context_deinit (state);
        break;
      default:
        break;
    }
  }
  state->allowed_tags = NULL;
}



static GstCaps *
gst_sub_parse_format_autodetect (GstSubParse * self)
{
  gchar *data;
  GstSubParseFormat format;

  if (strlen (self->textbuf->str) < 6) {
    GST_DEBUG ("File too small to be a subtitles file");
    return NULL;
  }

  data = g_strndup (self->textbuf->str, 35);
  format = gst_sub_parse_data_format_autodetect (data);
  g_free (data);

  self->parser_type = format;
  self->subtitle_codec = gst_sub_parse_get_format_description (format);
  parser_state_init (&self->state);
  self->state.allowed_tags = NULL;

  switch (format) {
    case GST_SUB_PARSE_FORMAT_MDVDSUB:
      self->parse_line = parse_mdvdsub;
      return gst_caps_new_simple ("text/x-raw",
          "format", G_TYPE_STRING, "pango-markup", NULL);
    case GST_SUB_PARSE_FORMAT_SUBRIP:
      self->state.allowed_tags = (gpointer) allowed_srt_tags;
      self->state.allows_tag_attributes = FALSE;
      self->parse_line = parse_subrip;
      return gst_caps_new_simple ("text/x-raw",
          "format", G_TYPE_STRING, "pango-markup", NULL);
    case GST_SUB_PARSE_FORMAT_MPSUB:
      self->parse_line = parse_mpsub;
      return gst_caps_new_simple ("text/x-raw",
          "format", G_TYPE_STRING, "utf8", NULL);
    case GST_SUB_PARSE_FORMAT_SAMI:
      self->parse_line = parse_sami;
      sami_context_init (&self->state);
      return gst_caps_new_simple ("text/x-raw",
          "format", G_TYPE_STRING, "pango-markup", NULL);
    case GST_SUB_PARSE_FORMAT_TMPLAYER:
      self->parse_line = parse_tmplayer;
      self->state.max_duration = 5 * GST_SECOND;
      return gst_caps_new_simple ("text/x-raw",
          "format", G_TYPE_STRING, "utf8", NULL);
    case GST_SUB_PARSE_FORMAT_MPL2:
      self->parse_line = parse_mpl2;
      return gst_caps_new_simple ("text/x-raw",
          "format", G_TYPE_STRING, "pango-markup", NULL);
    case GST_SUB_PARSE_FORMAT_DKS:
      self->parse_line = parse_dks;
      return gst_caps_new_simple ("text/x-raw",
          "format", G_TYPE_STRING, "utf8", NULL);
    case GST_SUB_PARSE_FORMAT_VTT:
      self->state.allowed_tags = (gpointer) allowed_vtt_tags;
      self->state.allows_tag_attributes = TRUE;
      self->parse_line = parse_webvtt;
      return gst_caps_new_simple ("text/x-raw",
          "format", G_TYPE_STRING, "pango-markup", NULL);
    case GST_SUB_PARSE_FORMAT_SUBVIEWER:
      self->parse_line = parse_subviewer;
      return gst_caps_new_simple ("text/x-raw",
          "format", G_TYPE_STRING, "utf8", NULL);
    case GST_SUB_PARSE_FORMAT_QTTEXT:
      self->parse_line = parse_qttext;
      qttext_context_init (&self->state);
      return gst_caps_new_simple ("text/x-raw",
          "format", G_TYPE_STRING, "pango-markup", NULL);
    case GST_SUB_PARSE_FORMAT_LRC:
      self->parse_line = parse_lrc;
      return gst_caps_new_simple ("text/x-raw",
          "format", G_TYPE_STRING, "utf8", NULL);
    case GST_SUB_PARSE_FORMAT_UNKNOWN:
    default:
      GST_DEBUG ("no subtitle format detected");
      GST_ELEMENT_ERROR (self, STREAM, WRONG_TYPE,
          ("The input is not a valid/supported subtitle file"), (NULL));
      return NULL;
  }
}

static void
feed_textbuf (GstSubParse * self, GstBuffer * buf)
{
  gboolean discont;
  gsize consumed;
  gchar *input = NULL;
  const guint8 *data;
  gsize avail;

  discont = GST_BUFFER_IS_DISCONT (buf);

  if (GST_BUFFER_OFFSET_IS_VALID (buf) &&
      GST_BUFFER_OFFSET (buf) != self->offset) {
    self->offset = GST_BUFFER_OFFSET (buf);
    discont = TRUE;
  }

  if (discont) {
    GST_INFO ("discontinuity");
    /* flush the parser state */
    parser_state_init (&self->state);
    g_string_truncate (self->textbuf, 0);
    gst_adapter_clear (self->adapter);
    if (self->parser_type == GST_SUB_PARSE_FORMAT_SAMI)
      sami_context_reset (&self->state);
    /* we could set a flag to make sure that the next buffer we push out also
     * has the DISCONT flag set, but there's no point really given that it's
     * subtitles which are discontinuous by nature. */
  }

  self->offset += gst_buffer_get_size (buf);

  gst_adapter_push (self->adapter, buf);

  avail = gst_adapter_available (self->adapter);
  data = gst_adapter_map (self->adapter, avail);
  input = convert_encoding (self, (const gchar *) data, avail, &consumed);

  if (input && consumed > 0) {
    self->textbuf = g_string_append (self->textbuf, input);
    gst_adapter_unmap (self->adapter);
    gst_adapter_flush (self->adapter, consumed);
  } else {
    gst_adapter_unmap (self->adapter);
  }

  g_free (input);
}


static void
xml_text (GMarkupParseContext * context,
    const gchar * text, gsize text_len, gpointer user_data, GError ** error)
{
  gchar **accum = (gchar **) user_data;
  gchar *concat;

  if (*accum) {
    concat = g_strconcat (*accum, text, NULL);
    g_free (*accum);
    *accum = concat;
  } else {
    *accum = g_strdup (text);
  }
}

static gchar *
strip_pango_markup (gchar * markup, GError ** error)
{
  GMarkupParser parser = { 0, };
  GMarkupParseContext *context;
  gchar *accum = NULL;

  parser.text = xml_text;
  context = g_markup_parse_context_new (&parser, 0, &accum, NULL);

  g_markup_parse_context_parse (context, "<root>", 6, NULL);
  g_markup_parse_context_parse (context, markup, strlen (markup), error);
  g_markup_parse_context_parse (context, "</root>", 7, NULL);
  if (*error)
    goto error;

  g_markup_parse_context_end_parse (context, error);
  if (*error)
    goto error;

done:
  g_markup_parse_context_free (context);
  return accum;

error:
  g_free (accum);
  accum = NULL;
  goto done;
}

static gboolean
gst_sub_parse_negotiate (GstSubParse * self, GstCaps * preferred)
{
  GstCaps *caps;
  gboolean ret = FALSE;
  const GstStructure *s1, *s2;

  caps = gst_pad_get_allowed_caps (self->srcpad);

  s1 = gst_caps_get_structure (preferred, 0);

  if (!g_strcmp0 (gst_structure_get_string (s1, "format"), "utf8")) {
    GstCaps *intersected = gst_caps_intersect (caps, preferred);
    gst_caps_unref (caps);
    caps = intersected;
  }

  caps = gst_caps_fixate (caps);

  if (gst_caps_is_empty (caps)) {
    goto done;
  }

  s2 = gst_caps_get_structure (caps, 0);

  self->strip_pango_markup =
      !g_strcmp0 (gst_structure_get_string (s2, "format"), "utf8")
      && !g_strcmp0 (gst_structure_get_string (s1, "format"), "pango-markup");

  if (self->strip_pango_markup) {
    GST_INFO_OBJECT (self, "We will convert from pango-markup to utf8");
  }

  ret = gst_pad_set_caps (self->srcpad, caps);

done:
  gst_caps_unref (caps);
  return ret;
}

static GstFlowReturn
check_initial_events (GstSubParse * self)
{
  gboolean need_tags = FALSE;

  /* make sure we know the format */
  if (G_UNLIKELY (self->parser_type == GST_SUB_PARSE_FORMAT_UNKNOWN)) {
    GstCaps *preferred;

    if (!(preferred = gst_sub_parse_format_autodetect (self))) {
      return GST_FLOW_NOT_NEGOTIATED;
    }

    if (!gst_sub_parse_negotiate (self, preferred)) {
      gst_caps_unref (preferred);
      return GST_FLOW_NOT_NEGOTIATED;
    }

    gst_caps_unref (preferred);

    need_tags = TRUE;
  }

  /* Push newsegment if needed */
  if (self->need_segment) {
    GstEvent *segment_event = gst_event_new_segment (&self->segment);
    GST_LOG_OBJECT (self, "pushing newsegment event with %" GST_SEGMENT_FORMAT,
        &self->segment);

    gst_event_set_seqnum (segment_event, self->segment_seqnum);
    gst_pad_push_event (self->srcpad, segment_event);
    self->need_segment = FALSE;
  }

  if (need_tags) {
    /* push tags */
    if (self->subtitle_codec != NULL) {
      GstTagList *tags;

      tags = gst_tag_list_new (GST_TAG_SUBTITLE_CODEC, self->subtitle_codec,
          NULL);
      gst_pad_push_event (self->srcpad, gst_event_new_tag (tags));
    }
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
handle_buffer (GstSubParse * self, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  gchar *line, *subtitle;

  GST_DEBUG_OBJECT (self, "%" GST_PTR_FORMAT, buf);

  if (self->first_buffer) {
    GstMapInfo map;

    gst_buffer_map (buf, &map, GST_MAP_READ);
    self->detected_encoding =
        gst_sub_parse_detect_encoding ((gchar *) map.data, map.size);
    gst_buffer_unmap (buf, &map);
    self->first_buffer = FALSE;
    self->state.fps_n = self->fps_n;
    self->state.fps_d = self->fps_d;
  }

  feed_textbuf (self, buf);

  ret = check_initial_events (self);
  if (ret != GST_FLOW_OK)
    return ret;

  while (!self->flushing && (line = get_next_line (self))) {
    guint offset = 0;

    /* Set segment on our parser state machine */
    self->state.segment = &self->segment;
    /* Now parse the line, out of segment lines will just return NULL */
    GST_LOG_OBJECT (self, "State %d. Parsing line '%s'", self->state.state,
        line + offset);
    subtitle = self->parse_line (&self->state, line + offset);
    g_free (line);

    if (subtitle) {
      guint subtitle_len;

      if (self->strip_pango_markup) {
        GError *error = NULL;
        gchar *stripped;

        if ((stripped = strip_pango_markup (subtitle, &error))) {
          g_free (subtitle);
          subtitle = stripped;
        } else {
          GST_WARNING_OBJECT (self, "Failed to strip pango markup: %s",
              error->message);
        }
      }

      subtitle_len = strlen (subtitle);

      /* +1 for terminating NUL character */
      buf = gst_buffer_new_and_alloc (subtitle_len + 1);

      /* copy terminating NUL character as well */
      gst_buffer_fill (buf, 0, subtitle, subtitle_len + 1);
      gst_buffer_set_size (buf, subtitle_len);

      GST_BUFFER_TIMESTAMP (buf) = self->state.start_time;
      GST_BUFFER_DURATION (buf) = self->state.duration;

      /* in some cases (e.g. tmplayer) we can only determine the duration
       * of a text chunk from the timestamp of the next text chunk; in those
       * cases, we probably want to limit the duration to something
       * reasonable, so we don't end up showing some text for e.g. 40 seconds
       * just because nothing else is being said during that time */
      if (self->state.max_duration > 0 && GST_BUFFER_DURATION_IS_VALID (buf)) {
        if (GST_BUFFER_DURATION (buf) > self->state.max_duration)
          GST_BUFFER_DURATION (buf) = self->state.max_duration;
      }

      self->segment.position = self->state.start_time;

      GST_DEBUG_OBJECT (self, "Sending text '%s', %" GST_TIME_FORMAT " + %"
          GST_TIME_FORMAT, subtitle, GST_TIME_ARGS (self->state.start_time),
          GST_TIME_ARGS (self->state.duration));

      g_free (self->state.vertical);
      self->state.vertical = NULL;
      g_free (self->state.alignment);
      self->state.alignment = NULL;

      ret = gst_pad_push (self->srcpad, buf);

      /* move this forward (the tmplayer parser needs this) */
      if (self->state.duration != GST_CLOCK_TIME_NONE)
        self->state.start_time += self->state.duration;

      g_free (subtitle);
      subtitle = NULL;

      if (ret != GST_FLOW_OK) {
        GST_DEBUG_OBJECT (self, "flow: %s", gst_flow_get_name (ret));
        break;
      }
    }
  }

  return ret;
}

static GstFlowReturn
gst_sub_parse_chain (GstPad * sinkpad, GstObject * parent, GstBuffer * buf)
{
  GstFlowReturn ret;
  GstSubParse *self;

  self = GST_SUBPARSE (parent);

  ret = handle_buffer (self, buf);

  return ret;
}

static gboolean
gst_sub_parse_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSubParse *self = GST_SUBPARSE (parent);
  gboolean ret = FALSE;

  GST_LOG_OBJECT (self, "%s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_GROUP_DONE:
    case GST_EVENT_EOS:{
      /* Make sure the last subrip chunk is pushed out even
       * if the file does not have an empty line at the end */
      if (self->parser_type == GST_SUB_PARSE_FORMAT_SUBRIP ||
          self->parser_type == GST_SUB_PARSE_FORMAT_TMPLAYER ||
          self->parser_type == GST_SUB_PARSE_FORMAT_MPL2 ||
          self->parser_type == GST_SUB_PARSE_FORMAT_QTTEXT ||
          self->parser_type == GST_SUB_PARSE_FORMAT_VTT) {
        gchar term_chars[] = { '\n', '\n', '\0' };
        GstBuffer *buf = gst_buffer_new_and_alloc (2 + 1);

        GST_DEBUG_OBJECT (self, "%s: force pushing of any remaining text",
            GST_EVENT_TYPE_NAME (event));

        gst_buffer_fill (buf, 0, term_chars, 3);
        gst_buffer_set_size (buf, 2);

        GST_BUFFER_OFFSET (buf) = self->offset;
        gst_sub_parse_chain (pad, parent, buf);
      }
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *s;
      gst_event_parse_segment (event, &s);
      if (s->format == GST_FORMAT_TIME)
        gst_event_copy_segment (event, &self->segment);
      GST_DEBUG_OBJECT (self, "newsegment (%s)",
          gst_format_get_name (self->segment.format));
      self->segment_seqnum = gst_event_get_seqnum (event);

      /* if not time format, we'll either start with a 0 timestamp anyway or
       * it's following a seek in which case we'll have saved the requested
       * seek segment and don't want to overwrite it (remember that on a seek
       * we always just seek back to the start in BYTES format and just throw
       * away all text that's before the requested position; if the subtitles
       * come from an upstream demuxer, it won't be able to handle our BYTES
       * seek request and instead send us a newsegment from the seek request
       * it received via its video pads instead, so all is fine then too) */
      ret = TRUE;
      gst_event_unref (event);
      /* in either case, let's not simply discard this event;
       * trigger sending of the saved requested seek segment
       * or the one taken here from upstream */
      self->need_segment = TRUE;
      break;
    }
    case GST_EVENT_GAP:
    {
      ret = check_initial_events (self);
      if (ret == GST_FLOW_OK) {
        ret = gst_pad_event_default (pad, parent, event);
      } else {
        gst_event_unref (event);
      }
      break;
    }
    case GST_EVENT_FLUSH_START:
    {
      self->flushing = TRUE;

      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      self->flushing = FALSE;

      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}


static GstStateChangeReturn
gst_sub_parse_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstSubParse *self = GST_SUBPARSE (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* format detection will init the parser state */
      self->offset = 0;
      self->parser_type = GST_SUB_PARSE_FORMAT_UNKNOWN;
      self->strip_pango_markup = FALSE;
      self->valid_utf8 = TRUE;
      self->first_buffer = TRUE;
      g_free (self->detected_encoding);
      self->detected_encoding = NULL;
      g_string_truncate (self->textbuf, 0);
      gst_adapter_clear (self->adapter);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      parser_state_dispose (self, &self->state);
      self->parser_type = GST_SUB_PARSE_FORMAT_UNKNOWN;
      break;
    default:
      break;
  }

  return ret;
}
