/* GStreamer FAAC (Free AAC Encoder) plugin
 * Copyright (C) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2009 Mark Nauwelaerts <mnauw@users.sourceforge.net>
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
 * SECTION:element-faac
 * @title: faac
 * @see_also: faad
 *
 * faac encodes raw audio to AAC (MPEG-4 part 3) streams.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 audiotestsrc wave=sine num-buffers=100 ! audioconvert ! faac ! matroskamux ! filesink location=sine.mkv
 * ]| Encode a sine beep as aac and write to matroska container.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>

#include <gst/audio/audio.h>
#include <gst/pbutils/codec-utils.h>

#include "gstfaac.h"

#define SAMPLE_RATES " 8000, " \
                    "11025, " \
                    "12000, " \
                    "16000, " \
                    "22050, " \
                    "24000, " \
                    "32000, " \
                    "44100, " \
                    "48000, " \
                    "64000, " \
                    "88200, " \
                    "96000"

/* these don't seem to work? */
#if 0
"audio/x-raw-int, "
    "endianness = (int) BYTE_ORDER, "
    "signed = (boolean) true, "
    "width = (int) 32, "
    "depth = (int) { 24, 32 }, "
    "rate = (int) [ 8000, 96000], "
    "channels = (int) [ 1, 6]; "
    "audio/x-raw-float, "
    "endianness = (int) BYTE_ORDER, "
    "width = (int) 32, "
    "rate = (int) [ 8000, 96000], " "channels = (int) [ 1, 6]"
#endif
#define SRC_CAPS \
    "audio/mpeg, "                     \
    "mpegversion = (int) 4, "   \
    "channels = (int) [ 1, 6 ], "      \
    "rate = (int) {" SAMPLE_RATES "}, "   \
    "stream-format = (string) { adts, raw }, " \
    "base-profile = (string) { main, lc, ssr, ltp }, " \
    "framed = (boolean) true; " \
    "audio/mpeg, "                     \
    "mpegversion = (int) 2, "   \
    "channels = (int) [ 1, 6 ], "      \
    "rate = (int) {" SAMPLE_RATES "}, "   \
    "stream-format = (string) { adts, raw }, " \
    "profile = (string) { main, lc }," \
    "framed = (boolean) true; "
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SRC_CAPS));

enum
{
  PROP_0,
  PROP_QUALITY,
  PROP_BITRATE,
  PROP_RATE_CONTROL,
  PROP_PROFILE,
  PROP_TNS,
  PROP_MIDSIDE,
  PROP_SHORTCTL
};

enum
{
  VBR = 1,
  ABR
};

static void gst_faac_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_faac_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstCaps *gst_faac_enc_generate_sink_caps (void);
static gboolean gst_faac_configure_source_pad (GstFaac * faac,
    GstAudioInfo * info);

static gboolean gst_faac_stop (GstAudioEncoder * enc);
static gboolean gst_faac_set_format (GstAudioEncoder * enc,
    GstAudioInfo * info);
static GstFlowReturn gst_faac_handle_frame (GstAudioEncoder * enc,
    GstBuffer * in_buf);

GST_DEBUG_CATEGORY_STATIC (faac_debug);
#define GST_CAT_DEFAULT faac_debug

#define FAAC_DEFAULT_QUALITY      100
#define FAAC_DEFAULT_BITRATE      128 * 1000
#define FAAC_DEFAULT_RATE_CONTROL VBR
#define FAAC_DEFAULT_TNS          FALSE
#define FAAC_DEFAULT_MIDSIDE      TRUE
#define FAAC_DEFAULT_SHORTCTL     SHORTCTL_NORMAL

#define gst_faac_parent_class parent_class
G_DEFINE_TYPE (GstFaac, gst_faac, GST_TYPE_AUDIO_ENCODER);
GST_ELEMENT_REGISTER_DEFINE (faac, "faac", GST_RANK_SECONDARY, GST_TYPE_FAAC);

#define GST_TYPE_FAAC_RATE_CONTROL (gst_faac_brtype_get_type ())
static GType
gst_faac_brtype_get_type (void)
{
  static GType gst_faac_brtype_type = 0;

  if (!gst_faac_brtype_type) {
    static const GEnumValue gst_faac_brtype[] = {
      {VBR, "VBR", "VBR encoding"},
      {ABR, "ABR", "ABR encoding"},
      {0, NULL, NULL},
    };

    gst_faac_brtype_type = g_enum_register_static ("GstFaacBrtype",
        gst_faac_brtype);
  }

  return gst_faac_brtype_type;
}

#define GST_TYPE_FAAC_SHORTCTL (gst_faac_shortctl_get_type ())
static GType
gst_faac_shortctl_get_type (void)
{
  static GType gst_faac_shortctl_type = 0;

  if (!gst_faac_shortctl_type) {
    static const GEnumValue gst_faac_shortctl[] = {
      {SHORTCTL_NORMAL, "SHORTCTL_NORMAL", "Normal block type"},
      {SHORTCTL_NOSHORT, "SHORTCTL_NOSHORT", "No short blocks"},
      {SHORTCTL_NOLONG, "SHORTCTL_NOLONG", "No long blocks"},
      {0, NULL, NULL},
    };

    gst_faac_shortctl_type = g_enum_register_static ("GstFaacShortCtl",
        gst_faac_shortctl);
  }

  return gst_faac_shortctl_type;
}

static void
gst_faac_class_init (GstFaacClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstAudioEncoderClass *base_class = GST_AUDIO_ENCODER_CLASS (klass);
  GstCaps *sink_caps;
  GstPadTemplate *sink_templ;

  gobject_class->set_property = gst_faac_set_property;
  gobject_class->get_property = gst_faac_get_property;

  GST_DEBUG_CATEGORY_INIT (faac_debug, "faac", 0, "AAC encoding");

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);

  sink_caps = gst_faac_enc_generate_sink_caps ();
  sink_templ = gst_pad_template_new ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS, sink_caps);
  gst_element_class_add_pad_template (gstelement_class, sink_templ);
  gst_caps_unref (sink_caps);

  gst_element_class_set_static_metadata (gstelement_class, "AAC audio encoder",
      "Codec/Encoder/Audio",
      "Free MPEG-2/4 AAC encoder",
      "Ronald Bultje <rbultje@ronald.bitfreak.net>");

  gst_type_mark_as_plugin_api (GST_TYPE_FAAC_RATE_CONTROL, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_FAAC_SHORTCTL, 0);

  base_class->stop = GST_DEBUG_FUNCPTR (gst_faac_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_faac_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_faac_handle_frame);

  /* properties */
  g_object_class_install_property (gobject_class, PROP_QUALITY,
      g_param_spec_int ("quality", "Quality (%)",
          "Variable bitrate (VBR) quantizer quality in %", 1, 1000,
          FAAC_DEFAULT_QUALITY,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_int ("bitrate", "Bitrate (bps)",
          "Average Bitrate (ABR) in bits/sec", 8 * 1000, 320 * 1000,
          FAAC_DEFAULT_BITRATE,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_RATE_CONTROL,
      g_param_spec_enum ("rate-control", "Rate Control (ABR/VBR)",
          "Encoding bitrate type (VBR/ABR)", GST_TYPE_FAAC_RATE_CONTROL,
          FAAC_DEFAULT_RATE_CONTROL,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_TNS,
      g_param_spec_boolean ("tns", "TNS", "Use temporal noise shaping",
          FAAC_DEFAULT_TNS,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MIDSIDE,
      g_param_spec_boolean ("midside", "Midside", "Allow mid/side encoding",
          FAAC_DEFAULT_MIDSIDE,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SHORTCTL,
      g_param_spec_enum ("shortctl", "Block type",
          "Block type encorcing",
          GST_TYPE_FAAC_SHORTCTL, FAAC_DEFAULT_SHORTCTL,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_faac_init (GstFaac * faac)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_ENCODER_SINK_PAD (faac));
}

static void
gst_faac_close_encoder (GstFaac * faac)
{
  if (faac->handle)
    faacEncClose (faac->handle);
  faac->handle = NULL;
}

static gboolean
gst_faac_stop (GstAudioEncoder * enc)
{
  GstFaac *faac = GST_FAAC (enc);

  GST_DEBUG_OBJECT (faac, "stop");
  gst_faac_close_encoder (faac);
  return TRUE;
}

static const GstAudioChannelPosition aac_channel_positions[][8] = {
  {GST_AUDIO_CHANNEL_POSITION_MONO},
  {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT},
  {
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
      },
  {
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
      GST_AUDIO_CHANNEL_POSITION_REAR_CENTER},
  {
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
      GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT},
  {
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
        GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
      GST_AUDIO_CHANNEL_POSITION_LFE1}
};

static GstCaps *
gst_faac_enc_generate_sink_caps (void)
{
  GstCaps *caps = gst_caps_new_empty ();
  GstStructure *s, *t;
  gint i, c;
  static const int rates[] = {
    8000, 11025, 12000, 16000, 22050, 24000,
    32000, 44100, 48000, 64000, 88200, 96000
  };
  GValue rates_arr = { 0, };
  GValue tmp_v = { 0, };

  g_value_init (&rates_arr, GST_TYPE_LIST);
  g_value_init (&tmp_v, G_TYPE_INT);
  for (i = 0; i < G_N_ELEMENTS (rates); i++) {
    g_value_set_int (&tmp_v, rates[i]);
    gst_value_list_append_value (&rates_arr, &tmp_v);
  }
  g_value_unset (&tmp_v);

  s = gst_structure_new ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (S16),
      "layout", G_TYPE_STRING, "interleaved", NULL);
  gst_structure_set_value (s, "rate", &rates_arr);

  t = gst_structure_copy (s);
  gst_structure_set (t, "channels", G_TYPE_INT, 1, NULL);
  gst_caps_append_structure (caps, t);

  for (i = 2; i <= 6; i++) {
    guint64 channel_mask = 0;
    t = gst_structure_copy (s);

    gst_structure_set (t, "channels", G_TYPE_INT, i, NULL);
    for (c = 0; c < i; c++)
      channel_mask |= G_GUINT64_CONSTANT (1) << aac_channel_positions[i - 1][c];

    gst_structure_set (t, "channel-mask", GST_TYPE_BITMASK, channel_mask, NULL);
    gst_caps_append_structure (caps, t);
  }
  gst_structure_free (s);
  g_value_unset (&rates_arr);

  GST_DEBUG ("Generated sinkcaps: %" GST_PTR_FORMAT, caps);
  return caps;
}

static void
gst_faac_set_tags (GstFaac * faac)
{
  GstTagList *taglist;

  /* create a taglist and add a bitrate tag to it */
  taglist = gst_tag_list_new_empty ();
  gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE,
      GST_TAG_BITRATE, faac->bitrate, NULL);

  gst_audio_encoder_merge_tags (GST_AUDIO_ENCODER (faac), taglist,
      GST_TAG_MERGE_REPLACE);

  gst_tag_list_unref (taglist);
}

static gboolean
gst_faac_set_format (GstAudioEncoder * enc, GstAudioInfo * info)
{
  GstFaac *faac = GST_FAAC (enc);
  gint width;
  gulong fmt = 0;
  gboolean result = FALSE;

  /* base class takes care */
  width = GST_AUDIO_INFO_WIDTH (info);

  if (GST_AUDIO_INFO_IS_INTEGER (info)) {
    switch (width) {
      case 16:
        fmt = FAAC_INPUT_16BIT;
        break;
      case 24:
      case 32:
        fmt = FAAC_INPUT_32BIT;
        break;
      default:
        g_return_val_if_reached (FALSE);
    }
  } else {
    fmt = FAAC_INPUT_FLOAT;
  }

  faac->format = fmt;

  /* finish up */
  result = gst_faac_configure_source_pad (faac, info);
  if (!result)
    goto done;

  gst_faac_set_tags (faac);

  /* report needs to base class */
  gst_audio_encoder_set_frame_samples_min (enc, faac->samples);
  gst_audio_encoder_set_frame_samples_max (enc, faac->samples);
  gst_audio_encoder_set_frame_max (enc, 1);

done:
  return result;
}

/* check downstream caps to configure format */
static void
gst_faac_negotiate (GstFaac * faac)
{
  GstCaps *caps;

  /* default setup */
  faac->profile = LOW;
  faac->mpegversion = 4;
  faac->outputformat = 0;

  caps = gst_pad_get_allowed_caps (GST_AUDIO_ENCODER_SRC_PAD (faac));

  GST_DEBUG_OBJECT (faac, "allowed caps: %" GST_PTR_FORMAT, caps);

  if (caps && gst_caps_get_size (caps) > 0) {
    GstStructure *s = gst_caps_get_structure (caps, 0);
    const gchar *str = NULL;
    gint i = 4;

    if ((str = gst_structure_get_string (s, "stream-format"))) {
      if (strcmp (str, "adts") == 0) {
        GST_DEBUG_OBJECT (faac, "use ADTS format for output");
        faac->outputformat = 1;
      } else if (strcmp (str, "raw") == 0) {
        GST_DEBUG_OBJECT (faac, "use RAW format for output");
        faac->outputformat = 0;
      } else {
        GST_DEBUG_OBJECT (faac, "unknown stream-format: %s", str);
        faac->outputformat = 0;
      }
    }

    if ((str = gst_structure_get_string (s, "profile"))) {
      if (strcmp (str, "main") == 0) {
        faac->profile = MAIN;
      } else if (strcmp (str, "lc") == 0) {
        faac->profile = LOW;
      } else if (strcmp (str, "ssr") == 0) {
        faac->profile = SSR;
      } else if (strcmp (str, "ltp") == 0) {
        faac->profile = LTP;
      } else {
        faac->profile = LOW;
      }
    }

    if (!gst_structure_get_int (s, "mpegversion", &i) || i == 4) {
      faac->mpegversion = 4;
    } else {
      faac->mpegversion = 2;
    }
  }

  if (caps)
    gst_caps_unref (caps);
}

static gboolean
gst_faac_open_encoder (GstFaac * faac, GstAudioInfo * info)
{
  faacEncHandle *handle;
  faacEncConfiguration *conf;
  guint maxbitrate;
  gulong samples, bytes;

  g_return_val_if_fail (info->rate != 0 && info->channels != 0, FALSE);

  /* clean up in case of re-configure */
  gst_faac_close_encoder (faac);

  if (!(handle = faacEncOpen (info->rate, info->channels, &samples, &bytes)))
    goto setup_failed;

  /* mind channel count */
  samples /= info->channels;

  /* record */
  faac->handle = handle;
  faac->samples = samples;
  faac->bytes = bytes;

  GST_DEBUG_OBJECT (faac, "faac needs samples %d, output size %d",
      faac->samples, faac->bytes);

  /* we negotiated caps update current configuration */
  conf = faacEncGetCurrentConfiguration (faac->handle);
  conf->mpegVersion = (faac->mpegversion == 4) ? MPEG4 : MPEG2;
  conf->aacObjectType = faac->profile;
  conf->allowMidside = faac->midside;
  conf->useLfe = 0;
  conf->useTns = faac->tns;

  if (faac->brtype == VBR) {
    conf->quantqual = faac->quality;
  } else if (faac->brtype == ABR) {
    conf->bitRate = faac->bitrate / info->channels;
  }

  conf->inputFormat = faac->format;
  conf->outputFormat = faac->outputformat;
  conf->shortctl = faac->shortctl;

  /* check, warn and correct if the max bitrate for the given samplerate is
   * exceeded. Maximum of 6144 bit for a channel */
  maxbitrate =
      (unsigned int) (6144.0 * (double) info->rate / (double) 1024.0 + .5);
  if (conf->bitRate > maxbitrate) {
    GST_ELEMENT_WARNING (faac, RESOURCE, SETTINGS, (NULL),
        ("bitrate %lu exceeds maximum allowed bitrate of %u for samplerate %d. "
            "Setting bitrate to %u", conf->bitRate, maxbitrate,
            info->rate, maxbitrate));
    conf->bitRate = maxbitrate;
  }

  /* default 0 to start with, libfaac chooses based on bitrate */
  conf->bandWidth = 0;

  if (!faacEncSetConfiguration (faac->handle, conf))
    goto setup_failed;

  /* let's see what really happened,
   * note that this may not really match desired rate */
  GST_DEBUG_OBJECT (faac, "average bitrate: %lu kbps",
      (conf->bitRate + 500) / 1000 * info->channels);
  GST_DEBUG_OBJECT (faac, "quantization quality: %ld", conf->quantqual);
  GST_DEBUG_OBJECT (faac, "bandwidth: %d Hz", conf->bandWidth);

  return TRUE;

  /* ERRORS */
setup_failed:
  {
    GST_ELEMENT_ERROR (faac, LIBRARY, SETTINGS, (NULL), (NULL));
    return FALSE;
  }
}

static gboolean
gst_faac_configure_source_pad (GstFaac * faac, GstAudioInfo * info)
{
  GstCaps *srccaps;
  gboolean ret;

  /* negotiate stream format */
  gst_faac_negotiate (faac);

  if (!gst_faac_open_encoder (faac, info))
    goto set_failed;

  /* now create a caps for it all */
  srccaps = gst_caps_new_simple ("audio/mpeg",
      "mpegversion", G_TYPE_INT, faac->mpegversion,
      "channels", G_TYPE_INT, info->channels,
      "rate", G_TYPE_INT, info->rate,
      "stream-format", G_TYPE_STRING, (faac->outputformat ? "adts" : "raw"),
      "framed", G_TYPE_BOOLEAN, TRUE, NULL);

  /* DecoderSpecificInfo is only available for mpegversion=4 */
  if (faac->mpegversion == 4) {
    guint8 *config = NULL;
    gulong config_len = 0;

    /* get the config string */
    GST_DEBUG_OBJECT (faac, "retrieving decoder info");
    faacEncGetDecoderSpecificInfo (faac->handle, &config, &config_len);

    if (!gst_codec_utils_aac_caps_set_level_and_profile (srccaps, config,
            config_len)) {
      free (config);
      gst_caps_unref (srccaps);
      goto invalid_codec_data;
    }

    if (!faac->outputformat) {
      GstBuffer *codec_data;

      /* copy it into a buffer */
      codec_data = gst_buffer_new_and_alloc (config_len);
      gst_buffer_fill (codec_data, 0, config, config_len);

      /* add to caps */
      gst_caps_set_simple (srccaps,
          "codec_data", GST_TYPE_BUFFER, codec_data, NULL);

      gst_buffer_unref (codec_data);
    }

    free (config);
  } else {
    const gchar *profile;

    /* Add least add the profile to the caps */
    switch (faac->profile) {
      case MAIN:
        profile = "main";
        break;
      case LTP:
        profile = "ltp";
        break;
      case SSR:
        profile = "ssr";
        break;
      case LOW:
      default:
        profile = "lc";
        break;
    }
    gst_caps_set_simple (srccaps, "profile", G_TYPE_STRING, profile, NULL);
    /* FIXME: How to get the profile for mpegversion==2? */
  }

  GST_DEBUG_OBJECT (faac, "src pad caps: %" GST_PTR_FORMAT, srccaps);

  ret = gst_audio_encoder_set_output_format (GST_AUDIO_ENCODER (faac), srccaps);
  gst_caps_unref (srccaps);

  return ret;

  /* ERROR */
set_failed:
  {
    GST_WARNING_OBJECT (faac, "Faac doesn't support the current configuration");
    return FALSE;
  }
invalid_codec_data:
  {
    GST_ERROR_OBJECT (faac, "Invalid codec data");
    return FALSE;
  }
}

static GstFlowReturn
gst_faac_handle_frame (GstAudioEncoder * enc, GstBuffer * in_buf)
{
  GstFaac *faac = GST_FAAC (enc);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *out_buf;
  gsize size, ret_size;
  int enc_ret;
  GstMapInfo map, omap;
  guint8 *data;
  GstAudioInfo *info =
      gst_audio_encoder_get_audio_info (GST_AUDIO_ENCODER (faac));

  out_buf = gst_buffer_new_and_alloc (faac->bytes);
  gst_buffer_map (out_buf, &omap, GST_MAP_WRITE);

  if (G_LIKELY (in_buf)) {
    if (memcmp (info->position, aac_channel_positions[info->channels - 1],
            sizeof (GstAudioChannelPosition) * info->channels) != 0) {
      in_buf = gst_buffer_make_writable (in_buf);
      gst_audio_buffer_reorder_channels (in_buf, info->finfo->format,
          info->channels, info->position,
          aac_channel_positions[info->channels - 1]);
    }
    gst_buffer_map (in_buf, &map, GST_MAP_READ);
    data = map.data;
    size = map.size;
  } else {
    data = NULL;
    size = 0;
  }

  if (G_UNLIKELY ((enc_ret = faacEncEncode (faac->handle, (gint32 *) data,
                  size / (info->finfo->width / 8), omap.data, omap.size)) < 0))
    goto encode_failed;
  ret_size = enc_ret;

  if (in_buf)
    gst_buffer_unmap (in_buf, &map);

  GST_LOG_OBJECT (faac, "encoder return: %" G_GSIZE_FORMAT, ret_size);

  if (ret_size > 0) {
    gst_buffer_unmap (out_buf, &omap);
    gst_buffer_resize (out_buf, 0, ret_size);
    ret = gst_audio_encoder_finish_frame (enc, out_buf, faac->samples);
  } else {
    gst_buffer_unmap (out_buf, &omap);
    gst_buffer_unref (out_buf);
    /* re-create encoder after final flush */
    if (!in_buf) {
      GST_DEBUG_OBJECT (faac, "flushed; recreating encoder");
      gst_faac_close_encoder (faac);
      if (!gst_faac_open_encoder (faac, gst_audio_encoder_get_audio_info (enc)))
        ret = GST_FLOW_ERROR;
    }
  }

  return ret;

  /* ERRORS */
encode_failed:
  {
    GST_ELEMENT_ERROR (faac, LIBRARY, ENCODE, (NULL), (NULL));
    if (in_buf)
      gst_buffer_unmap (in_buf, &map);
    gst_buffer_unmap (out_buf, &omap);
    gst_buffer_unref (out_buf);
    return GST_FLOW_ERROR;
  }
}

static void
gst_faac_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstFaac *faac = GST_FAAC (object);

  GST_OBJECT_LOCK (faac);

  switch (prop_id) {
    case PROP_QUALITY:
      faac->quality = g_value_get_int (value);
      break;
    case PROP_BITRATE:
      faac->bitrate = g_value_get_int (value);
      break;
    case PROP_RATE_CONTROL:
      faac->brtype = g_value_get_enum (value);
      break;
    case PROP_TNS:
      faac->tns = g_value_get_boolean (value);
      break;
    case PROP_MIDSIDE:
      faac->midside = g_value_get_boolean (value);
      break;
    case PROP_SHORTCTL:
      faac->shortctl = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (faac);
}

static void
gst_faac_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstFaac *faac = GST_FAAC (object);

  GST_OBJECT_LOCK (faac);

  switch (prop_id) {
    case PROP_QUALITY:
      g_value_set_int (value, faac->quality);
      break;
    case PROP_BITRATE:
      g_value_set_int (value, faac->bitrate);
      break;
    case PROP_RATE_CONTROL:
      g_value_set_enum (value, faac->brtype);
      break;
    case PROP_TNS:
      g_value_set_boolean (value, faac->tns);
      break;
    case PROP_MIDSIDE:
      g_value_set_boolean (value, faac->midside);
      break;
    case PROP_SHORTCTL:
      g_value_set_enum (value, faac->shortctl);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (faac);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (faac, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    faac,
    "Free AAC Encoder (FAAC)",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
