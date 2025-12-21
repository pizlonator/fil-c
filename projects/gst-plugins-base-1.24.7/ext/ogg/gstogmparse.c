/* GStreamer OGM parsing
 * Copyright (C) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
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
#include <string.h>

#include <gst/gst.h>
#include <gst/tag/tag.h>
#include <gst/riff/riff-media.h>
#include <gst/riff/riff-read.h>

#include "gstoggelements.h"

GST_DEBUG_CATEGORY_STATIC (gst_ogm_parse_debug);
#define GST_CAT_DEFAULT gst_ogm_parse_debug

#define GST_TYPE_OGM_VIDEO_PARSE (gst_ogm_video_parse_get_type())
#define GST_IS_OGM_VIDEO_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_OGM_VIDEO_PARSE))

#define GST_TYPE_OGM_AUDIO_PARSE (gst_ogm_audio_parse_get_type())
#define GST_IS_OGM_AUDIO_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_OGM_AUDIO_PARSE))

#define GST_TYPE_OGM_TEXT_PARSE (gst_ogm_text_parse_get_type())
#define GST_IS_OGM_TEXT_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_OGM_TEXT_PARSE))

#define GST_TYPE_OGM_PARSE (gst_ogm_parse_get_type())
#define GST_OGM_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_OGM_PARSE, GstOgmParse))
#define GST_OGM_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_OGM_PARSE, GstOgmParse))
#define GST_IS_OGM_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_OGM_PARSE))
#define GST_IS_OGM_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_OGM_PARSE))
#define GST_OGM_PARSE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_OGM_PARSE, GstOgmParseClass))

typedef struct _stream_header_video
{
  gint32 width;
  gint32 height;
} stream_header_video;

typedef struct _stream_header_audio
{
  gint16 channels;
  gint16 blockalign;
  gint32 avgbytespersec;
} stream_header_audio;

/* sizeof(stream_header) might differ due to structure packing and
 * alignment differences on some architectures, so not using that */
#define OGM_STREAM_HEADER_SIZE (8+4+4+8+8+4+4+4+8)

typedef struct _stream_header
{
  gchar streamtype[8];
  gchar subtype[4 + 1];

  /* size of the structure */
  gint32 size;

  /* in reference time */
  gint64 time_unit;

  gint64 samples_per_unit;

  /* in media time */
  gint32 default_len;

  gint32 buffersize;
  gint32 bits_per_sample;

  union
  {
    stream_header_video video;
    stream_header_audio audio;
    /* text has no additional data */
  } s;
} stream_header;

typedef struct _GstOgmParse
{
  GstElement element;

  /* pads */
  GstPad *srcpad, *sinkpad;
  GstPadTemplate *srcpadtempl;

  /* we need to cache events that we receive before creating the source pad */
  GList *cached_events;

  /* audio or video */
  stream_header hdr;

  /* expected next granulepos (used for timestamp guessing) */
  guint64 next_granulepos;
} GstOgmParse;

typedef struct _GstOgmParseClass
{
  GstElementClass parent_class;
} GstOgmParseClass;

static GstStaticPadTemplate sink_factory_video =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-ogm-video"));
static GstStaticPadTemplate sink_factory_audio =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-ogm-audio"));
static GstStaticPadTemplate sink_factory_text =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-ogm-text"));
static GstPadTemplate *video_src_templ, *audio_src_templ, *text_src_templ;

static GType gst_ogm_audio_parse_get_type (void);
static GType gst_ogm_video_parse_get_type (void);
static GType gst_ogm_text_parse_get_type (void);
static GType gst_ogm_parse_get_type (void);

static void gst_ogm_audio_parse_base_init (GstOgmParseClass * klass);
static void gst_ogm_video_parse_base_init (GstOgmParseClass * klass);
static void gst_ogm_text_parse_base_init (GstOgmParseClass * klass);
static void gst_ogm_parse_class_init (GstOgmParseClass * klass);
static void gst_ogm_parse_init (GstOgmParse * ogm);
static void gst_ogm_video_parse_init (GstOgmParse * ogm);
static void gst_ogm_audio_parse_init (GstOgmParse * ogm);
static void gst_ogm_text_parse_init (GstOgmParse * ogm);
static void gst_ogm_parse_element_init (GstPlugin * plugin);

static gboolean gst_ogm_parse_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_ogm_parse_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_ogm_parse_sink_convert (GstPad * pad, GstFormat src_format,
    gint64 src_value, GstFormat * dest_format, gint64 * dest_value);

static GstFlowReturn gst_ogm_parse_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);

static GstStateChangeReturn gst_ogm_parse_change_state (GstElement * element,
    GstStateChange transition);

static GstElementClass *parent_class = NULL;

static GType
gst_ogm_parse_get_type (void)
{
  static GType ogm_parse_type = 0;

  if (!ogm_parse_type) {
    static const GTypeInfo ogm_parse_info = {
      sizeof (GstOgmParseClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_ogm_parse_class_init,
      NULL,
      NULL,
      sizeof (GstOgmParse),
      0,
      (GInstanceInitFunc) gst_ogm_parse_init,
    };

    ogm_parse_type =
        g_type_register_static (GST_TYPE_ELEMENT,
        "GstOgmParse", &ogm_parse_info, 0);
  }

  return ogm_parse_type;
}

static GType
gst_ogm_audio_parse_get_type (void)
{
  static GType ogm_audio_parse_type = 0;

  if (!ogm_audio_parse_type) {
    static const GTypeInfo ogm_audio_parse_info = {
      sizeof (GstOgmParseClass),
      (GBaseInitFunc) gst_ogm_audio_parse_base_init,
      NULL,
      NULL,
      NULL,
      NULL,
      sizeof (GstOgmParse),
      0,
      (GInstanceInitFunc) gst_ogm_audio_parse_init,
    };

    ogm_audio_parse_type =
        g_type_register_static (GST_TYPE_OGM_PARSE,
        "GstOgmAudioParse", &ogm_audio_parse_info, 0);
  }

  return ogm_audio_parse_type;
}

static GType
gst_ogm_video_parse_get_type (void)
{
  static GType ogm_video_parse_type = 0;

  if (!ogm_video_parse_type) {
    static const GTypeInfo ogm_video_parse_info = {
      sizeof (GstOgmParseClass),
      (GBaseInitFunc) gst_ogm_video_parse_base_init,
      NULL,
      NULL,
      NULL,
      NULL,
      sizeof (GstOgmParse),
      0,
      (GInstanceInitFunc) gst_ogm_video_parse_init,
    };

    ogm_video_parse_type =
        g_type_register_static (GST_TYPE_OGM_PARSE,
        "GstOgmVideoParse", &ogm_video_parse_info, 0);
  }

  return ogm_video_parse_type;
}

static GType
gst_ogm_text_parse_get_type (void)
{
  static GType ogm_text_parse_type = 0;

  if (!ogm_text_parse_type) {
    static const GTypeInfo ogm_text_parse_info = {
      sizeof (GstOgmParseClass),
      (GBaseInitFunc) gst_ogm_text_parse_base_init,
      NULL,
      NULL,
      NULL,
      NULL,
      sizeof (GstOgmParse),
      0,
      (GInstanceInitFunc) gst_ogm_text_parse_init,
    };

    ogm_text_parse_type =
        g_type_register_static (GST_TYPE_OGM_PARSE,
        "GstOgmTextParse", &ogm_text_parse_info, 0);
  }

  return ogm_text_parse_type;
}

GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (ogmaudioparse, "ogmaudioparse",
    GST_RANK_PRIMARY, GST_TYPE_OGM_AUDIO_PARSE,
    gst_ogm_parse_element_init (plugin));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (ogmvideoparse, "ogmvideoparse",
    GST_RANK_PRIMARY, GST_TYPE_OGM_VIDEO_PARSE,
    gst_ogm_parse_element_init (plugin));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (ogmtextparse, "ogmtextparse",
    GST_RANK_PRIMARY, GST_TYPE_OGM_TEXT_PARSE,
    gst_ogm_parse_element_init (plugin));

static void
gst_ogm_audio_parse_base_init (GstOgmParseClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstCaps *caps = gst_riff_create_audio_template_caps ();

  gst_element_class_set_static_metadata (element_class,
      "OGM audio stream parser", "Codec/Parser/Audio",
      "parse an OGM audio header and stream",
      "GStreamer maintainers <gstreamer-devel@lists.freedesktop.org>");

  gst_element_class_add_static_pad_template (element_class,
      &sink_factory_audio);
  audio_src_templ =
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_SOMETIMES, caps);
  gst_element_class_add_pad_template (element_class, audio_src_templ);
  gst_caps_unref (caps);
}

static void
gst_ogm_video_parse_base_init (GstOgmParseClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstCaps *caps = gst_riff_create_video_template_caps ();

  gst_element_class_set_static_metadata (element_class,
      "OGM video stream parser", "Codec/Parser/Video",
      "parse an OGM video header and stream",
      "GStreamer maintainers <gstreamer-devel@lists.freedesktop.org>");

  gst_element_class_add_static_pad_template (element_class,
      &sink_factory_video);
  video_src_templ =
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_SOMETIMES, caps);
  gst_element_class_add_pad_template (element_class, video_src_templ);
  gst_caps_unref (caps);
}

static void
gst_ogm_text_parse_base_init (GstOgmParseClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstCaps *caps = gst_caps_new_simple ("text/x-raw", "format", G_TYPE_STRING,
      "utf8", NULL);

  gst_element_class_set_static_metadata (element_class,
      "OGM text stream parser", "Codec/Decoder/Subtitle",
      "parse an OGM text header and stream",
      "GStreamer maintainers <gstreamer-devel@lists.freedesktop.org>");

  gst_element_class_add_static_pad_template (element_class, &sink_factory_text);
  text_src_templ = gst_pad_template_new ("src",
      GST_PAD_SRC, GST_PAD_SOMETIMES, caps);
  gst_element_class_add_pad_template (element_class, text_src_templ);
  gst_caps_unref (caps);
}

static void
gst_ogm_parse_class_init (GstOgmParseClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ogm_parse_change_state);

  gst_type_mark_as_plugin_api (GST_TYPE_OGM_PARSE, 0);
}

static void
gst_ogm_parse_init (GstOgmParse * ogm)
{
  memset (&ogm->hdr, 0, sizeof (ogm->hdr));
  ogm->next_granulepos = 0;
  ogm->srcpad = NULL;
  ogm->cached_events = NULL;
}

static void
gst_ogm_audio_parse_init (GstOgmParse * ogm)
{
  ogm->sinkpad = gst_pad_new_from_static_template (&sink_factory_audio, "sink");
  gst_pad_set_query_function (ogm->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ogm_parse_sink_query));
  gst_pad_set_chain_function (ogm->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ogm_parse_chain));
  gst_pad_set_event_function (ogm->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ogm_parse_sink_event));
  gst_element_add_pad (GST_ELEMENT (ogm), ogm->sinkpad);

  ogm->srcpad = NULL;
  ogm->srcpadtempl = audio_src_templ;
}

static void
gst_ogm_video_parse_init (GstOgmParse * ogm)
{
  ogm->sinkpad = gst_pad_new_from_static_template (&sink_factory_video, "sink");
  gst_pad_set_query_function (ogm->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ogm_parse_sink_query));
  gst_pad_set_chain_function (ogm->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ogm_parse_chain));
  gst_pad_set_event_function (ogm->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ogm_parse_sink_event));
  gst_element_add_pad (GST_ELEMENT (ogm), ogm->sinkpad);

  ogm->srcpad = NULL;
  ogm->srcpadtempl = video_src_templ;
}

static void
gst_ogm_text_parse_init (GstOgmParse * ogm)
{
  ogm->sinkpad = gst_pad_new_from_static_template (&sink_factory_text, "sink");
  gst_pad_set_query_function (ogm->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ogm_parse_sink_query));
  gst_pad_set_chain_function (ogm->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ogm_parse_chain));
  gst_pad_set_event_function (ogm->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ogm_parse_sink_event));
  gst_element_add_pad (GST_ELEMENT (ogm), ogm->sinkpad);

  ogm->srcpad = NULL;
  ogm->srcpadtempl = text_src_templ;
}

static gboolean
gst_ogm_parse_sink_convert (GstPad * pad,
    GstFormat src_format, gint64 src_value,
    GstFormat * dest_format, gint64 * dest_value)
{
  gboolean res = FALSE;
  GstOgmParse *ogm = GST_OGM_PARSE (gst_pad_get_parent (pad));

  switch (src_format) {
    case GST_FORMAT_DEFAULT:
      switch (*dest_format) {
        case GST_FORMAT_TIME:
          switch (ogm->hdr.streamtype[0]) {
            case 'a':
              *dest_value = GST_SECOND * src_value / ogm->hdr.samples_per_unit;
              res = TRUE;
              break;
            case 'v':
            case 't':
              *dest_value = (GST_SECOND / 10000000) *
                  ogm->hdr.time_unit * src_value;
              res = TRUE;
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_DEFAULT:
          switch (ogm->hdr.streamtype[0]) {
            case 'a':
              *dest_value = ogm->hdr.samples_per_unit * src_value / GST_SECOND;
              res = TRUE;
              break;
            case 'v':
            case 't':
              *dest_value = src_value /
                  ((GST_SECOND / 10000000) * ogm->hdr.time_unit);
              res = TRUE;
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }

  gst_object_unref (ogm);
  return res;
}

static gboolean
gst_ogm_parse_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstOgmParse *ogm = GST_OGM_PARSE (parent);
  GstFormat format;
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      gint64 val;

      gst_query_parse_position (query, &format, NULL);

      if (format != GST_FORMAT_DEFAULT && format != GST_FORMAT_TIME)
        break;

      if ((res = gst_ogm_parse_sink_convert (pad,
                  GST_FORMAT_DEFAULT, ogm->next_granulepos, &format, &val))) {
        /* don't know the total length here.. */
        gst_query_set_position (query, format, val);
      }
      break;
    }
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      /* peel off input */
      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      if ((res = gst_ogm_parse_sink_convert (pad, src_fmt, src_val,
                  &dest_fmt, &dest_val))) {
        gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      }
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }

  return res;
}

static GstFlowReturn
gst_ogm_parse_stream_header (GstOgmParse * ogm, const guint8 * data, guint size)
{
  GstCaps *caps = NULL;

  /* stream header */
  if (size < OGM_STREAM_HEADER_SIZE)
    goto buffer_too_small;

  if (!memcmp (data, "video\000\000\000", 8)) {
    ogm->hdr.s.video.width = GST_READ_UINT32_LE (&data[44]);
    ogm->hdr.s.video.height = GST_READ_UINT32_LE (&data[48]);
  } else if (!memcmp (data, "audio\000\000\000", 8)) {
    ogm->hdr.s.audio.channels = GST_READ_UINT32_LE (&data[44]);
    ogm->hdr.s.audio.blockalign = GST_READ_UINT32_LE (&data[46]);
    ogm->hdr.s.audio.avgbytespersec = GST_READ_UINT32_LE (&data[48]);
  } else if (!memcmp (data, "text\000\000\000\000", 8)) {
    /* nothing here */
  } else {
    goto cannot_decode;
  }
  memcpy (ogm->hdr.streamtype, &data[0], 8);
  memcpy (ogm->hdr.subtype, &data[8], 4);
  ogm->hdr.subtype[4] = '\0';
  ogm->hdr.size = GST_READ_UINT32_LE (&data[12]);
  ogm->hdr.time_unit = GST_READ_UINT64_LE (&data[16]);
  ogm->hdr.samples_per_unit = GST_READ_UINT64_LE (&data[24]);
  ogm->hdr.default_len = GST_READ_UINT32_LE (&data[32]);
  ogm->hdr.buffersize = GST_READ_UINT32_LE (&data[36]);
  ogm->hdr.bits_per_sample = GST_READ_UINT32_LE (&data[40]);

  switch (ogm->hdr.streamtype[0]) {
    case 'a':{
      guint codec_id = 0;

      if (sscanf (ogm->hdr.subtype, "%04x", &codec_id) != 1) {
        GST_WARNING_OBJECT (ogm, "cannot parse subtype %s", ogm->hdr.subtype);
      }

      /* FIXME: Need to do something with the reorder map */
      caps =
          gst_riff_create_audio_caps (codec_id, NULL, NULL, NULL, NULL, NULL,
          NULL);

      if (caps == NULL) {
        GST_WARNING_OBJECT (ogm, "no audio caps for codec %u found", codec_id);
        caps = gst_caps_new_simple ("audio/x-ogm-unknown", "codec_id",
            G_TYPE_INT, (gint) codec_id, NULL);
      }

      gst_caps_set_simple (caps,
          "channels", G_TYPE_INT, ogm->hdr.s.audio.channels,
          "rate", G_TYPE_INT, ogm->hdr.samples_per_unit, NULL);

      GST_LOG_OBJECT (ogm, "Type: %s, subtype: 0x%04x, channels: %d, "
          "samplerate: %d, blockalign: %d, bps: %d, caps = %" GST_PTR_FORMAT,
          ogm->hdr.streamtype, codec_id, ogm->hdr.s.audio.channels,
          (gint) ogm->hdr.samples_per_unit, ogm->hdr.s.audio.blockalign,
          ogm->hdr.s.audio.avgbytespersec, caps);
      break;
    }
    case 'v':{
      guint32 fourcc;
      gint time_unit;

      fourcc = GST_MAKE_FOURCC (ogm->hdr.subtype[0],
          ogm->hdr.subtype[1], ogm->hdr.subtype[2], ogm->hdr.subtype[3]);

      caps = gst_riff_create_video_caps (fourcc, NULL, NULL, NULL, NULL, NULL);

      if (caps == NULL) {
        gchar *fstr =
            g_strdup_printf ("%" GST_FOURCC_FORMAT, GST_FOURCC_ARGS (fourcc));
        GST_WARNING_OBJECT (ogm, "could not find video caps for fourcc '%s'",
            fstr);
        caps =
            gst_caps_new_simple ("video/x-ogm-unknown", "fourcc", G_TYPE_STRING,
            fstr, NULL);
        g_free (fstr);
        break;
      }

      GST_LOG_OBJECT (ogm, "Type: %s, subtype: %" GST_FOURCC_FORMAT
          ", size: %dx%d, timeunit: %" G_GINT64_FORMAT
          " (fps: %lf), s/u: %" G_GINT64_FORMAT ", "
          "def.len: %d, bufsize: %d, bps: %d, caps = %" GST_PTR_FORMAT,
          ogm->hdr.streamtype, GST_FOURCC_ARGS (fourcc),
          ogm->hdr.s.video.width, ogm->hdr.s.video.height,
          ogm->hdr.time_unit, 10000000. / ogm->hdr.time_unit,
          ogm->hdr.samples_per_unit, ogm->hdr.default_len,
          ogm->hdr.buffersize, ogm->hdr.bits_per_sample, caps);

      /* GST_TYPE_FRACTION contains gint */
      if (ogm->hdr.time_unit > G_MAXINT || ogm->hdr.time_unit < 1)
        GST_WARNING_OBJECT (ogm, "timeunit is out of range");

      time_unit = (gint) CLAMP (ogm->hdr.time_unit, 1, G_MAXINT);
      gst_caps_set_simple (caps,
          "width", G_TYPE_INT, ogm->hdr.s.video.width,
          "height", G_TYPE_INT, ogm->hdr.s.video.height,
          "framerate", GST_TYPE_FRACTION, 10000000, time_unit, NULL);
      break;
    }
    case 't':{
      GST_LOG_OBJECT (ogm, "Type: %s, s/u: %" G_GINT64_FORMAT
          ", timeunit=%" G_GINT64_FORMAT,
          ogm->hdr.streamtype, ogm->hdr.samples_per_unit, ogm->hdr.time_unit);
      caps = gst_caps_new_simple ("text/x-raw", "format", G_TYPE_STRING,
          "utf8", NULL);
      break;
    }
    default:
      g_assert_not_reached ();
  }

  if (caps == NULL)
    goto cannot_decode;

  if (!gst_caps_is_fixed (caps))
    goto non_fixed_caps;

  if (ogm->srcpad) {
    GstCaps *current_caps = gst_pad_get_current_caps (ogm->srcpad);

    if (current_caps) {
      if (caps && !gst_caps_is_equal (current_caps, caps)) {
        GST_WARNING_OBJECT (ogm, "Already an existing pad %s:%s",
            GST_DEBUG_PAD_NAME (ogm->srcpad));
        gst_pad_set_active (ogm->srcpad, FALSE);
        gst_element_remove_pad (GST_ELEMENT (ogm), ogm->srcpad);
        ogm->srcpad = NULL;
      } else {
        GST_DEBUG_OBJECT (ogm, "Existing pad has the same caps, do nothing");
      }
      gst_caps_unref (current_caps);
    }
  }

  if (ogm->srcpad == NULL) {
    GList *l, *cached_events;

    ogm->srcpad = gst_pad_new_from_template (ogm->srcpadtempl, "src");
    gst_pad_use_fixed_caps (ogm->srcpad);
    gst_pad_set_active (ogm->srcpad, TRUE);
    gst_element_add_pad (GST_ELEMENT (ogm), ogm->srcpad);
    GST_INFO_OBJECT (ogm, "Added pad %s:%s with caps %" GST_PTR_FORMAT,
        GST_DEBUG_PAD_NAME (ogm->srcpad), caps);

    GST_OBJECT_LOCK (ogm);
    cached_events = ogm->cached_events;
    ogm->cached_events = NULL;
    GST_OBJECT_UNLOCK (ogm);

    for (l = cached_events; l; l = l->next) {
      GstEvent *event = GST_EVENT_CAST (l->data);

      GST_DEBUG_OBJECT (ogm, "Pushing cached event %" GST_PTR_FORMAT, event);
      gst_pad_push_event (ogm->srcpad, event);
    }
    g_list_free (cached_events);

    gst_pad_set_caps (ogm->srcpad, caps);
    {
      GstTagList *tags;

      tags = gst_tag_list_new (GST_TAG_SUBTITLE_CODEC, "Ogm", NULL);
      gst_pad_push_event (ogm->srcpad, gst_event_new_tag (tags));
    }
  }

  gst_caps_unref (caps);

  return GST_FLOW_OK;

/* ERRORS */
buffer_too_small:
  {
    GST_ELEMENT_ERROR (ogm, STREAM, WRONG_TYPE, ("Buffer too small"), (NULL));
    return GST_FLOW_ERROR;
  }
cannot_decode:
  {
    GST_ELEMENT_ERROR (ogm, STREAM, DECODE, (NULL), ("unknown ogm format"));
    return GST_FLOW_ERROR;
  }
non_fixed_caps:
  {
    gst_caps_unref (caps);
    GST_ELEMENT_ERROR (ogm, STREAM, DECODE, (NULL), ("broken ogm format"));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_ogm_parse_comment_packet (GstOgmParse * ogm, GstBuffer * buf)
{
  GstFlowReturn ret;

  if (ogm->srcpad == NULL) {
    GST_DEBUG ("no source pad");
    return GST_FLOW_FLUSHING;
  }

  /* if this is not a subtitle stream, push the vorbiscomment packet
   * on downstream, the respective decoder will handle it; if it is
   * a subtitle stream, we will have to handle the comment ourself */
  if (ogm->hdr.streamtype[0] == 't') {
    GstTagList *tags;

    tags = gst_tag_list_from_vorbiscomment_buffer (buf,
        (guint8 *) "\003vorbis", 7, NULL);

    if (tags) {
      GST_DEBUG_OBJECT (ogm, "tags = %" GST_PTR_FORMAT, tags);
      gst_pad_push_event (ogm->srcpad, gst_event_new_tag (tags));
    } else {
      GST_DEBUG_OBJECT (ogm, "failed to extract tags from vorbis comment");
    }
    /* do not push packet downstream, just let parent unref it */
    ret = GST_FLOW_OK;
  } else {
    ret = gst_pad_push (ogm->srcpad, buf);
  }

  return ret;
}

static void
gst_ogm_text_parse_strip_trailing_zeroes (GstOgmParse * ogm, GstBuffer * buf)
{
  GstMapInfo map;
  gsize size;

  g_assert (gst_buffer_is_writable (buf));

  /* zeroes are not valid UTF-8 characters, so strip them from output */
  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  size = map.size;
  while (size > 0 && map.data[size - 1] == '\0') {
    --size;
  }
  gst_buffer_unmap (buf, &map);
}

static GstFlowReturn
gst_ogm_parse_data_packet (GstOgmParse * ogm, GstBuffer * buf,
    const guint8 * data, gsize size)
{
  GstFlowReturn ret;
  GstBuffer *sbuf;
  gboolean keyframe;
  guint len, n, xsize = 0;

  if ((data[0] & 0x01) != 0)
    goto invalid_startcode;

  /* data - push on */
  len = ((data[0] & 0xc0) >> 6) | ((data[0] & 0x02) << 1);
  keyframe = (((data[0] & 0x08) >> 3) != 0);

  if ((1 + len) > size)
    goto buffer_too_small;

  for (n = len; n > 0; n--) {
    xsize = (xsize << 8) | data[n];
  }

  GST_LOG_OBJECT (ogm, "[0x%02x] samples: %d, hdrbytes: %d, datasize: %"
      G_GSIZE_FORMAT, data[0], xsize, len, size - len - 1);

  sbuf =
      gst_buffer_copy_region (buf, GST_BUFFER_COPY_ALL, len + 1,
      size - len - 1);

  if (GST_BUFFER_OFFSET_END_IS_VALID (buf))
    ogm->next_granulepos = GST_BUFFER_OFFSET_END (buf);

  switch (ogm->hdr.streamtype[0]) {
    case 't':
    case 'v':{
      GstClockTime ts, next_ts;
      guint samples;

      samples = (ogm->hdr.streamtype[0] == 'v') ? 1 : xsize;

      if (!keyframe) {
        GST_BUFFER_FLAG_SET (sbuf, GST_BUFFER_FLAG_DELTA_UNIT);
      }

      /* shouldn't this be granulepos - samples? (tpm) */
      ts = gst_util_uint64_scale (ogm->next_granulepos,
          ogm->hdr.time_unit * GST_SECOND, 10000000);
      next_ts = gst_util_uint64_scale (ogm->next_granulepos + samples,
          ogm->hdr.time_unit * GST_SECOND, 10000000);

      GST_BUFFER_TIMESTAMP (sbuf) = ts;
      GST_BUFFER_DURATION (sbuf) = next_ts - ts;

      ogm->next_granulepos += samples;

      if (ogm->hdr.streamtype[0] == 't') {
        gst_ogm_text_parse_strip_trailing_zeroes (ogm, sbuf);
      }
      break;
    }
    case 'a':{
      GstClockTime ts, next_ts;

      /* shouldn't this be granulepos - samples? (tpm) */
      ts = gst_util_uint64_scale_int (ogm->next_granulepos,
          GST_SECOND, ogm->hdr.samples_per_unit);
      next_ts = gst_util_uint64_scale_int (ogm->next_granulepos + xsize,
          GST_SECOND, ogm->hdr.samples_per_unit);

      GST_BUFFER_TIMESTAMP (sbuf) = ts;
      GST_BUFFER_DURATION (sbuf) = next_ts - ts;

      ogm->next_granulepos += xsize;
      break;
    }
    default:
      g_assert_not_reached ();
      break;
  }

  if (ogm->srcpad) {
    GST_LOG_OBJECT (ogm, "Pushing buffer with ts=%" GST_TIME_FORMAT,
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (sbuf)));
    ret = gst_pad_push (ogm->srcpad, sbuf);
    if (ret != GST_FLOW_OK) {
      GST_DEBUG_OBJECT (ogm, "Flow on %s:%s = %s",
          GST_DEBUG_PAD_NAME (ogm->srcpad), gst_flow_get_name (ret));
    }
  } else {
    ret = GST_FLOW_FLUSHING;
  }

  return ret;

/* ERRORS */
invalid_startcode:
  {
    GST_ELEMENT_ERROR (ogm, STREAM, DECODE, (NULL),
        ("unexpected packet startcode 0x%02x", data[0]));
    return GST_FLOW_ERROR;
  }
buffer_too_small:
  {
    GST_ELEMENT_ERROR (ogm, STREAM, DECODE, (NULL),
        ("buffer too small, len+1=%u, size=%" G_GSIZE_FORMAT, len + 1, size));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_ogm_parse_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstOgmParse *ogm = GST_OGM_PARSE (parent);
  GstMapInfo map;

  gst_buffer_map (buf, &map, GST_MAP_READ);
  if (map.size < 1)
    goto buffer_too_small;

  GST_LOG_OBJECT (ogm, "Packet with start code 0x%02x", map.data[0]);

  switch (map.data[0]) {
    case 0x01:{
      ret = gst_ogm_parse_stream_header (ogm, map.data + 1, map.size - 1);
      break;
    }
    case 0x03:{
      ret = gst_ogm_parse_comment_packet (ogm, buf);
      break;
    }
    default:{
      ret = gst_ogm_parse_data_packet (ogm, buf, map.data, map.size);
      break;
    }
  }

  gst_buffer_unmap (buf, &map);
  gst_buffer_unref (buf);

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (ogm, "Flow: %s", gst_flow_get_name (ret));
  }

  return ret;

/* ERRORS */
buffer_too_small:
  {
    GST_ELEMENT_ERROR (ogm, STREAM, DECODE, (NULL), ("buffer too small"));
    gst_buffer_unmap (buf, &map);
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_ogm_parse_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstOgmParse *ogm = GST_OGM_PARSE (parent);
  gboolean res;

  GST_LOG_OBJECT (ogm, "processing %s event", GST_EVENT_TYPE_NAME (event));

  GST_OBJECT_LOCK (ogm);
  if (ogm->srcpad == NULL) {
    ogm->cached_events = g_list_append (ogm->cached_events, event);
    GST_OBJECT_UNLOCK (ogm);
    res = TRUE;
  } else {
    GST_OBJECT_UNLOCK (ogm);
    res = gst_pad_event_default (pad, parent, event);
  }

  return res;
}

static GstStateChangeReturn
gst_ogm_parse_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstOgmParse *ogm = GST_OGM_PARSE (element);

  ret = parent_class->change_state (element, transition);
  if (ret != GST_STATE_CHANGE_SUCCESS)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (ogm->srcpad) {
        gst_pad_set_active (ogm->srcpad, FALSE);
        gst_element_remove_pad (element, ogm->srcpad);
        ogm->srcpad = NULL;
      }
      memset (&ogm->hdr, 0, sizeof (ogm->hdr));
      ogm->next_granulepos = 0;
      g_list_foreach (ogm->cached_events, (GFunc) gst_mini_object_unref, NULL);
      g_list_free (ogm->cached_events);
      ogm->cached_events = NULL;
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_ogm_parse_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;
  if (g_once_init_enter (&res)) {
    gst_riff_init ();
    GST_DEBUG_CATEGORY_INIT (gst_ogm_parse_debug, "ogmparse", 0, "ogm parser");
    g_once_init_leave (&res, TRUE);
  }
}
