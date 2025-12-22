/* GStreamer
 * Copyright (C) 2007 Sebastian Dröge <slomo@circular-chaos.org>
 *
 * gstaudioparse.c:
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
 * SECTION:element-audioparse
 * @title: audioparse
 *
 * Converts a byte stream into audio frames.
 *
 * This element is deprecated. Use #GstRawAudioParse instead.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* FIXME 0.11: suppress warnings for deprecated API such as g_value_array stuff
 * for now with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include "gstaudioparse.h"

#include <string.h>

typedef enum _GstRawAudioParseFormat GstRawAudioParseFormat;
enum _GstRawAudioParseFormat
{
  GST_RAW_AUDIO_PARSE_FORMAT_PCM,
  GST_RAW_AUDIO_PARSE_FORMAT_MULAW,
  GST_RAW_AUDIO_PARSE_FORMAT_ALAW
};

static GstStaticPadTemplate static_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define GST_UNALIGNED_RAW_AUDIO_CAPS \
  "audio/x-unaligned-raw" \
  ", format = (string) " GST_AUDIO_FORMATS_ALL \
  ", rate = (int) [ 1, MAX ]" \
  ", channels = (int) [ 1, MAX ]" \
  ", layout = (string) { interleaved, non-interleaved }"

static GstStaticPadTemplate static_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL)
        ", layout = (string) { interleaved, non-interleaved }; "
        GST_UNALIGNED_RAW_AUDIO_CAPS "; "
        "audio/x-alaw, rate=(int)[1,MAX], channels=(int)[1,MAX]; "
        "audio/x-mulaw, rate=(int)[1,MAX], channels=(int)[1,MAX]")
    );

typedef enum
{
  GST_AUDIO_PARSE_FORMAT_RAW,
  GST_AUDIO_PARSE_FORMAT_MULAW,
  GST_AUDIO_PARSE_FORMAT_ALAW
} GstAudioParseFormat;

static void gst_audio_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_audio_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

GST_DEBUG_CATEGORY_STATIC (gst_audio_parse_debug);
#define GST_CAT_DEFAULT gst_audio_parse_debug

enum
{
  PROP_0,
  PROP_FORMAT,
  PROP_RAW_FORMAT,
  PROP_RATE,
  PROP_CHANNELS,
  PROP_INTERLEAVED,
  PROP_CHANNEL_POSITIONS,
  PROP_USE_SINK_CAPS
};

#define GST_AUDIO_PARSE_FORMAT (gst_audio_parse_format_get_type ())
static GType
gst_audio_parse_format_get_type (void)
{
  static GType audio_parse_format_type = 0;
  static const GEnumValue format_types[] = {
    {GST_AUDIO_PARSE_FORMAT_RAW, "Raw", "raw"},
    {GST_AUDIO_PARSE_FORMAT_ALAW, "A-Law", "alaw"},
    {GST_AUDIO_PARSE_FORMAT_MULAW, "\302\265-Law", "mulaw"},
    {0, NULL, NULL}
  };

  if (!audio_parse_format_type) {
    audio_parse_format_type =
        g_enum_register_static ("GstAudioParseFormat", format_types);
  }

  return audio_parse_format_type;
}

#define gst_audio_parse_parent_class parent_class
G_DEFINE_TYPE (GstAudioParse, gst_audio_parse, GST_TYPE_BIN);
GST_ELEMENT_REGISTER_DEFINE (audioparse, "audioparse", GST_RANK_NONE,
    gst_audio_parse_get_type ());

static void
gst_audio_parse_class_init (GstAudioParseClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_audio_parse_set_property;
  gobject_class->get_property = gst_audio_parse_get_property;

  g_object_class_install_property (gobject_class, PROP_FORMAT,
      g_param_spec_enum ("format", "Format",
          "Format of audio samples in raw stream", GST_AUDIO_PARSE_FORMAT,
          GST_AUDIO_PARSE_FORMAT_RAW,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RAW_FORMAT,
      g_param_spec_enum ("raw-format", "Raw Format",
          "Format of audio samples in raw stream", GST_TYPE_AUDIO_FORMAT,
          GST_AUDIO_FORMAT_S16, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RATE,
      g_param_spec_int ("rate", "Rate", "Rate of audio samples in raw stream",
          1, INT_MAX, 44100, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CHANNELS,
      g_param_spec_int ("channels", "Channels",
          "Number of channels in raw stream", 1, 64, 2,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTERLEAVED,
      g_param_spec_boolean ("interleaved", "Interleaved Layout",
          "True if audio has interleaved layout", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CHANNEL_POSITIONS,
      g_param_spec_value_array ("channel-positions", "Channel positions",
          "Channel positions used on the output",
          g_param_spec_enum ("channel-position", "Channel position",
              "Channel position of the n-th input",
              GST_TYPE_AUDIO_CHANNEL_POSITION,
              GST_AUDIO_CHANNEL_POSITION_NONE,
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_USE_SINK_CAPS,
      g_param_spec_boolean ("use-sink-caps", "Use sink caps",
          "Use the sink caps for the format, only performing timestamping",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class, "Audio Parse",
      "Filter/Audio",
      "Converts stream into audio frames (deprecated: use rawaudioparse instead)",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&static_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&static_src_template));

  GST_DEBUG_CATEGORY_INIT (gst_audio_parse_debug, "audioparse", 0,
      "audioparse element");

  gst_type_mark_as_plugin_api (GST_AUDIO_PARSE_FORMAT, 0);
}

static void
gst_audio_parse_init (GstAudioParse * ap)
{
  GstPad *inner_pad;
  GstPad *ghostpad;

  ap->rawaudioparse =
      gst_element_factory_make ("rawaudioparse", "inner_rawaudioparse");
  g_assert (ap->rawaudioparse != NULL);

  gst_bin_add (GST_BIN (ap), ap->rawaudioparse);

  inner_pad = gst_element_get_static_pad (ap->rawaudioparse, "sink");
  ghostpad =
      gst_ghost_pad_new_from_template ("sink", inner_pad,
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (ap), "sink"));
  gst_element_add_pad (GST_ELEMENT (ap), ghostpad);
  gst_object_unref (GST_OBJECT (inner_pad));

  inner_pad = gst_element_get_static_pad (ap->rawaudioparse, "src");
  ghostpad =
      gst_ghost_pad_new_from_template ("src", inner_pad,
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (ap), "src"));
  gst_element_add_pad (GST_ELEMENT (ap), ghostpad);
  gst_object_unref (GST_OBJECT (inner_pad));
}

static void
gst_audio_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAudioParse *ap = GST_AUDIO_PARSE (object);

  switch (prop_id) {
    case PROP_FORMAT:{
      GstRawAudioParseFormat raw_parse_format;

      switch (g_value_get_enum (value)) {
        case GST_AUDIO_PARSE_FORMAT_RAW:
          raw_parse_format = GST_RAW_AUDIO_PARSE_FORMAT_PCM;
          break;

        case GST_AUDIO_PARSE_FORMAT_MULAW:
          raw_parse_format = GST_RAW_AUDIO_PARSE_FORMAT_MULAW;
          break;

        case GST_AUDIO_PARSE_FORMAT_ALAW:
          raw_parse_format = GST_RAW_AUDIO_PARSE_FORMAT_ALAW;
          break;

        default:
          g_assert_not_reached ();
          break;
      }

      g_object_set (G_OBJECT (ap->rawaudioparse), "format", raw_parse_format,
          NULL);

      break;
    }

    case PROP_RAW_FORMAT:
      g_object_set (G_OBJECT (ap->rawaudioparse), "pcm-format",
          g_value_get_enum (value), NULL);
      break;

    case PROP_RATE:
      g_object_set (G_OBJECT (ap->rawaudioparse), "sample-rate",
          g_value_get_int (value), NULL);
      break;

    case PROP_CHANNELS:
      g_object_set (G_OBJECT (ap->rawaudioparse), "num-channels",
          g_value_get_int (value), NULL);
      break;

    case PROP_INTERLEAVED:
      g_object_set (G_OBJECT (ap->rawaudioparse), "interleaved",
          g_value_get_boolean (value), NULL);
      break;

    case PROP_CHANNEL_POSITIONS:
      g_object_set (G_OBJECT (ap->rawaudioparse), "channel-positions",
          g_value_get_boxed (value), NULL);
      break;

    case PROP_USE_SINK_CAPS:
      g_object_set (G_OBJECT (ap->rawaudioparse), "use-sink-caps",
          g_value_get_boolean (value), NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_audio_parse_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstAudioParse *ap = GST_AUDIO_PARSE (object);

  switch (prop_id) {
    case PROP_FORMAT:{
      GstRawAudioParseFormat raw_parse_format;
      GstAudioParseFormat format;

      g_object_get (G_OBJECT (ap->rawaudioparse), "format", &raw_parse_format,
          NULL);

      switch (raw_parse_format) {
        case GST_RAW_AUDIO_PARSE_FORMAT_PCM:
          format = GST_AUDIO_PARSE_FORMAT_RAW;
          break;

        case GST_RAW_AUDIO_PARSE_FORMAT_MULAW:
          format = GST_AUDIO_PARSE_FORMAT_MULAW;
          break;

        case GST_RAW_AUDIO_PARSE_FORMAT_ALAW:
          format = GST_AUDIO_PARSE_FORMAT_ALAW;
          break;

        default:
          g_assert_not_reached ();
          break;
      }

      g_value_set_enum (value, format);

      break;
    }

    case PROP_RAW_FORMAT:{
      GstAudioFormat format;
      g_object_get (G_OBJECT (ap->rawaudioparse), "pcm-format", &format, NULL);
      g_value_set_enum (value, format);
      break;
    }

    case PROP_RATE:{
      gint sample_rate;
      g_object_get (G_OBJECT (ap->rawaudioparse), "sample-rate", &sample_rate,
          NULL);
      g_value_set_int (value, sample_rate);
      break;
    }

    case PROP_CHANNELS:{
      gint num_channels;
      g_object_get (G_OBJECT (ap->rawaudioparse), "num-channels", &num_channels,
          NULL);
      g_value_set_int (value, num_channels);
      break;
    }

    case PROP_INTERLEAVED:{
      gboolean interleaved;
      g_object_get (G_OBJECT (ap->rawaudioparse), "interleaved", &interleaved,
          NULL);
      g_value_set_boolean (value, interleaved);
      break;
    }

    case PROP_CHANNEL_POSITIONS:{
      gpointer channel_positions;
      g_object_get (G_OBJECT (ap->rawaudioparse), "channel-positions",
          &channel_positions, NULL);
      g_value_set_boxed (value, channel_positions);
      break;
    }

    case PROP_USE_SINK_CAPS:{
      gboolean use_sink_caps;
      g_object_get (G_OBJECT (ap->rawaudioparse), "use-sink-caps",
          &use_sink_caps, NULL);
      g_value_set_boolean (value, use_sink_caps);
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
