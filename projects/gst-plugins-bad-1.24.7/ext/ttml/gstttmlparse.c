/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2004 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) <2015> British Broadcasting Corporation <dash@rd.bbc.co.uk>
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

/**
 * SECTION:element-ttmlparse
 * @title: ttmlparse
 *
 * Parses timed text subtitle files described using Timed Text Markup Language
 * (TTML). Currently, only the EBU-TT-D profile of TTML, designed for
 * distribution of subtitles over IP, is supported.
 *
 * The parser outputs a #GstBuffer for each scene in the input TTML file, a
 * scene being a period of time during which a static set of subtitles should
 * be visible. The parser places each text element within a scene into its own
 * #GstMemory within the scene's buffer, and attaches metadata to the buffer
 * describing the styling and layout associated with all the contained text
 * elements. A downstream renderer element uses this information to correctly
 * render the text on top of video frames.
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 filesrc location=<media file location> ! video/quicktime ! qtdemux name=q ttmlrender name=r q. ! queue ! h264parse ! avdec_h264 ! autovideoconvert ! r.video_sink filesrc location=<subtitle file location> blocksize=16777216 ! queue ! ttmlparse ! r.text_sink r. ! ximagesink q. ! queue ! aacparse ! avdec_aac ! audioconvert ! alsasink
 * ]| Parse and render TTML subtitles contained in a single XML file over an
 * MP4 stream containing H.264 video and AAC audio.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <glib.h>

#include "gstttmlelements.h"
#include "gstttmlparse.h"
#include "ttmlparse.h"

GST_DEBUG_CATEGORY (ttmlparse_debug);
#define GST_CAT_DEFAULT ttmlparse_debug


#define DEFAULT_ENCODING   NULL

static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/ttml+xml")
    );

static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-raw(meta:GstSubtitleMeta)")
    );

static gboolean gst_ttml_parse_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_ttml_parse_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_ttml_parse_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static GstStateChangeReturn gst_ttml_parse_change_state (GstElement * element,
    GstStateChange transition);

static GstFlowReturn gst_ttml_parse_chain (GstPad * sinkpad, GstObject * parent,
    GstBuffer * buf);
static gboolean gst_element_ttmlparse_init (GstPlugin * plugin);

#define gst_ttml_parse_parent_class parent_class
G_DEFINE_TYPE (GstTtmlParse, gst_ttml_parse, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_CUSTOM (ttmlparse, gst_element_ttmlparse_init);

static void
gst_ttml_parse_dispose (GObject * object)
{
  GstTtmlParse *ttmlparse = GST_TTML_PARSE (object);

  GST_DEBUG_OBJECT (ttmlparse, "cleaning up subtitle parser");

  g_free (ttmlparse->encoding);
  ttmlparse->encoding = NULL;

  g_free (ttmlparse->detected_encoding);
  ttmlparse->detected_encoding = NULL;

  if (ttmlparse->adapter) {
    g_object_unref (ttmlparse->adapter);
    ttmlparse->adapter = NULL;
  }

  if (ttmlparse->textbuf) {
    g_string_free (ttmlparse->textbuf, TRUE);
    ttmlparse->textbuf = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_ttml_parse_class_init (GstTtmlParseClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  object_class->dispose = gst_ttml_parse_dispose;

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_templ));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_templ));
  gst_element_class_set_static_metadata (element_class,
      "TTML subtitle parser", "Codec/Decoder/Subtitle",
      "Parses TTML subtitle files",
      "GStreamer maintainers <gstreamer-devel@lists.sourceforge.net>, "
      "Chris Bass <dash@rd.bbc.co.uk>");

  element_class->change_state = gst_ttml_parse_change_state;
}

static void
gst_ttml_parse_init (GstTtmlParse * ttmlparse)
{
  ttmlparse->sinkpad = gst_pad_new_from_static_template (&sink_templ, "sink");
  gst_pad_set_chain_function (ttmlparse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ttml_parse_chain));
  gst_pad_set_event_function (ttmlparse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ttml_parse_sink_event));
  gst_element_add_pad (GST_ELEMENT (ttmlparse), ttmlparse->sinkpad);

  ttmlparse->srcpad = gst_pad_new_from_static_template (&src_templ, "src");
  gst_pad_set_event_function (ttmlparse->srcpad,
      GST_DEBUG_FUNCPTR (gst_ttml_parse_src_event));
  gst_pad_set_query_function (ttmlparse->srcpad,
      GST_DEBUG_FUNCPTR (gst_ttml_parse_src_query));
  gst_element_add_pad (GST_ELEMENT (ttmlparse), ttmlparse->srcpad);

  ttmlparse->textbuf = g_string_new (NULL);
  gst_segment_init (&ttmlparse->segment, GST_FORMAT_TIME);
  ttmlparse->need_segment = TRUE;
  ttmlparse->encoding = g_strdup (DEFAULT_ENCODING);
  ttmlparse->detected_encoding = NULL;
  ttmlparse->adapter = gst_adapter_new ();
}

/*
 * Source pad functions.
 */
static gboolean
gst_ttml_parse_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstTtmlParse *self = GST_TTML_PARSE (parent);
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
gst_ttml_parse_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstTtmlParse *self = GST_TTML_PARSE (parent);
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

        self->need_segment = TRUE;
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

static gchar *
gst_convert_to_utf8 (const gchar * str, gsize len, const gchar * encoding,
    gsize * consumed, GError ** err)
{
  gchar *ret = NULL;

  *consumed = 0;
  /* The char cast is necessary in glib < 2.24 */
  ret =
      g_convert_with_fallback (str, len, "UTF-8", encoding, (char *) "*",
      consumed, NULL, err);
  if (ret == NULL)
    return ret;

  /* + 3 to skip UTF-8 BOM if it was added */
  len = strlen (ret);
  if (len >= 3 && (guint8) ret[0] == 0xEF && (guint8) ret[1] == 0xBB
      && (guint8) ret[2] == 0xBF)
    memmove (ret, ret + 3, len + 1 - 3);

  return ret;
}

static gchar *
detect_encoding (const gchar * str, gsize len)
{
  if (len >= 3 && (guint8) str[0] == 0xEF && (guint8) str[1] == 0xBB
      && (guint8) str[2] == 0xBF)
    return g_strdup ("UTF-8");

  if (len >= 2 && (guint8) str[0] == 0xFE && (guint8) str[1] == 0xFF)
    return g_strdup ("UTF-16BE");

  if (len >= 2 && (guint8) str[0] == 0xFF && (guint8) str[1] == 0xFE)
    return g_strdup ("UTF-16LE");

  if (len >= 4 && (guint8) str[0] == 0x00 && (guint8) str[1] == 0x00
      && (guint8) str[2] == 0xFE && (guint8) str[3] == 0xFF)
    return g_strdup ("UTF-32BE");

  if (len >= 4 && (guint8) str[0] == 0xFF && (guint8) str[1] == 0xFE
      && (guint8) str[2] == 0x00 && (guint8) str[3] == 0x00)
    return g_strdup ("UTF-32LE");

  return NULL;
}

static gchar *
convert_encoding (GstTtmlParse * self, const gchar * str, gsize len,
    gsize * consumed)
{
  const gchar *encoding;
  GError *err = NULL;
  gchar *ret = NULL;

  *consumed = 0;

  /* First try any detected encoding */
  if (self->detected_encoding) {
    ret =
        gst_convert_to_utf8 (str, len, self->detected_encoding, consumed, &err);

    if (!err)
      return ret;

    GST_WARNING_OBJECT (self, "could not convert string from '%s' to UTF-8: %s",
        self->detected_encoding, err->message);
    g_free (self->detected_encoding);
    self->detected_encoding = NULL;
    g_error_free (err);
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
    /* if local encoding is UTF-8 and no encoding specified
     * via the environment variable, assume ISO-8859-15 */
    if (g_get_charset (&encoding)) {
      encoding = "ISO-8859-15";
    }
  }

  ret = gst_convert_to_utf8 (str, len, encoding, consumed, &err);

  if (err) {
    GST_WARNING_OBJECT (self, "could not convert string from '%s' to UTF-8: %s",
        encoding, err->message);
    g_error_free (err);

    /* invalid input encoding, fall back to ISO-8859-15 (always succeeds) */
    ret = gst_convert_to_utf8 (str, len, "ISO-8859-15", consumed, NULL);
  }

  GST_LOG_OBJECT (self,
      "successfully converted %" G_GSIZE_FORMAT " characters from %s to UTF-8"
      "%s", len, encoding, (err) ? " , using ISO-8859-15 as fallback" : "");

  return ret;
}

static GstCaps *
gst_ttml_parse_get_src_caps (GstTtmlParse * self)
{
  GstCaps *caps;
  GstCapsFeatures *features = gst_caps_features_new ("meta:GstSubtitleMeta",
      NULL);

  caps = gst_caps_new_empty_simple ("text/x-raw");
  gst_caps_set_features (caps, 0, features);
  return caps;
}

static void
feed_textbuf (GstTtmlParse * self, GstBuffer * buf)
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
    g_string_truncate (self->textbuf, 0);
    gst_adapter_clear (self->adapter);
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
    if (!self->textbuf)
      self->textbuf = g_string_new (input);
    else
      self->textbuf = g_string_append (self->textbuf, input);

    gst_adapter_unmap (self->adapter);
    gst_adapter_flush (self->adapter, consumed);
  } else {
    gst_adapter_unmap (self->adapter);
  }

  g_free (input);
}

static GstFlowReturn
handle_buffer (GstTtmlParse * self, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstCaps *caps = NULL;
  GList *subtitle_list = NULL;
  GList *iter;
  GstClockTime begin = GST_BUFFER_PTS (buf);
  GstClockTime duration = GST_BUFFER_DURATION (buf);
  guint consumed;

  if (self->first_buffer) {
    GstMapInfo map;

    gst_buffer_map (buf, &map, GST_MAP_READ);
    self->detected_encoding = detect_encoding ((gchar *) map.data, map.size);
    gst_buffer_unmap (buf, &map);
    self->first_buffer = FALSE;
  }

  feed_textbuf (self, buf);

  if (!(caps = gst_ttml_parse_get_src_caps (self)))
    return GST_FLOW_EOS;
  gst_caps_unref (caps);

  /* Push newsegment if needed */
  if (self->need_segment) {
    GST_LOG_OBJECT (self, "pushing newsegment event with %" GST_SEGMENT_FORMAT,
        &self->segment);

    gst_pad_push_event (self->srcpad, gst_event_new_segment (&self->segment));
    self->need_segment = FALSE;
  }

  do {
    consumed = ttml_parse (self->textbuf->str, begin, duration, &subtitle_list);

    if (!consumed) {
      GST_DEBUG_OBJECT (self, "need more data");
      return ret;
    }

    self->textbuf = g_string_erase (self->textbuf, 0, consumed);

    for (iter = subtitle_list; iter; iter = g_list_next (iter)) {
      GstBuffer *op_buffer = GST_BUFFER (iter->data);
      self->segment.position = GST_BUFFER_PTS (op_buffer);

      ret = gst_pad_push (self->srcpad, op_buffer);

      if (ret != GST_FLOW_OK) {
        GST_DEBUG_OBJECT (self, "flow: %s", gst_flow_get_name (ret));
        break;
      }
    }

    g_list_free (subtitle_list);
  } while (TRUE);

  return ret;
}

static GstFlowReturn
gst_ttml_parse_chain (GstPad * sinkpad, GstObject * parent, GstBuffer * buf)
{
  GstTtmlParse *self = GST_TTML_PARSE (parent);
  return handle_buffer (self, buf);
}

static gboolean
gst_ttml_parse_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstTtmlParse *self = GST_TTML_PARSE (parent);
  gboolean ret = FALSE;

  GST_DEBUG ("Handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *s;
      gst_event_parse_segment (event, &s);
      if (s->format == GST_FORMAT_TIME)
        gst_event_copy_segment (event, &self->segment);
      GST_DEBUG_OBJECT (self, "newsegment (%s)",
          gst_format_get_name (self->segment.format));

      /* if not time format, we'll either start with a 0 timestamp anyway or
       * it's following a seek in which case we'll have saved the requested
       * seek segment and don't want to overwrite it (remember that on a seek
       * we always just seek back to the start in BYTES format and just throw
       * away all text that's before the requested position; if the subtitles
       * come from an upstream demuxer, it won't be able to handle our BYTES
       * seek request and instead send us a newsegment from the seek request
       * it received via its video pads instead, so all is fine then too) */
      ret = TRUE;
      self->need_segment = TRUE;
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      gst_event_unref (event);

      caps = gst_ttml_parse_get_src_caps (self);
      event = gst_event_new_caps (caps);
      gst_caps_unref (caps);

      ret = gst_pad_push_event (self->srcpad, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static GstStateChangeReturn
gst_ttml_parse_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstTtmlParse *self = GST_TTML_PARSE (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* format detection will init the parser state */
      self->offset = 0;
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
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_element_ttmlparse_init (GstPlugin * plugin)
{
  guint rank = GST_RANK_NONE;

  ttml_element_init (plugin);

  GST_DEBUG_CATEGORY_INIT (ttmlparse_debug, "ttmlparse", 0, "TTML parser");

  /* We don't want this autoplugged by default yet for now */
  if (g_getenv ("GST_TTML_AUTOPLUG")) {
    GST_INFO_OBJECT (plugin, "Registering ttml elements with primary rank.");
    rank = GST_RANK_PRIMARY;
  }

  return gst_element_register (plugin, "ttmlparse", rank, GST_TYPE_TTML_PARSE);
}
