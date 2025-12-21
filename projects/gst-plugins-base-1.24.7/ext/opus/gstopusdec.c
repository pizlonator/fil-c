/* GStreamer
 * Copyright (C) 2004 Wim Taymans <wim@fluendo.com>
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2008 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2011-2012 Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
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

/*
 * Based on the speexdec element.
 */

/**
 * SECTION:element-opusdec
 * @title: opusdec
 * @see_also: opusenc, oggdemux
 *
 * This element decodes a OPUS stream to raw integer audio.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v filesrc location=opus.ogg ! oggdemux ! opusdec ! audioconvert ! audioresample ! alsasink
 * ]|
 * Decode an Ogg/Opus file. To create an Ogg/Opus file refer to the documentation of opusenc.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "gstopuselements.h"
#include "gstopusheader.h"
#include "gstopuscommon.h"
#include "gstopusdec.h"
#include <gst/pbutils/pbutils.h>

GST_DEBUG_CATEGORY_STATIC (opusdec_debug);
#define GST_CAT_DEFAULT opusdec_debug

static GstStaticPadTemplate opus_dec_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) { 48000, 24000, 16000, 12000, 8000 }, "
        "channels = (int) [ 1, 255 ] ")
    );

static GstStaticPadTemplate opus_dec_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-opus, "
        "channel-mapping-family = (int) 0; "
        "audio/x-opus, "
        "channel-mapping-family = (int) [1, 255], "
        "channels = (int) [1, 255], "
        "stream-count = (int) [1, 255], " "coupled-count = (int) [0, 255]")
    );

G_DEFINE_TYPE (GstOpusDec, gst_opus_dec, GST_TYPE_AUDIO_DECODER);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (opusdec, "opusdec",
    GST_RANK_PRIMARY, GST_TYPE_OPUS_DEC, opus_element_init (plugin));

#define DB_TO_LINEAR(x) pow (10., (x) / 20.)

#define DEFAULT_USE_INBAND_FEC FALSE
#define DEFAULT_APPLY_GAIN TRUE
#define DEFAULT_PHASE_INVERSION FALSE

enum
{
  PROP_0,
  PROP_USE_INBAND_FEC,
  PROP_APPLY_GAIN,
  PROP_PHASE_INVERSION,
  PROP_STATS,
};


static GstFlowReturn gst_opus_dec_parse_header (GstOpusDec * dec,
    GstBuffer * buf);
static gboolean gst_opus_dec_start (GstAudioDecoder * dec);
static gboolean gst_opus_dec_stop (GstAudioDecoder * dec);
static GstFlowReturn gst_opus_dec_handle_frame (GstAudioDecoder * dec,
    GstBuffer * buffer);
static gboolean gst_opus_dec_set_format (GstAudioDecoder * bdec,
    GstCaps * caps);
static void gst_opus_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_opus_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static GstCaps *gst_opus_dec_getcaps (GstAudioDecoder * dec, GstCaps * filter);


static void
gst_opus_dec_class_init (GstOpusDecClass * klass)
{
  GObjectClass *gobject_class;
  GstAudioDecoderClass *adclass;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  adclass = (GstAudioDecoderClass *) klass;
  element_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_opus_dec_set_property;
  gobject_class->get_property = gst_opus_dec_get_property;

  adclass->start = GST_DEBUG_FUNCPTR (gst_opus_dec_start);
  adclass->stop = GST_DEBUG_FUNCPTR (gst_opus_dec_stop);
  adclass->handle_frame = GST_DEBUG_FUNCPTR (gst_opus_dec_handle_frame);
  adclass->set_format = GST_DEBUG_FUNCPTR (gst_opus_dec_set_format);
  adclass->getcaps = GST_DEBUG_FUNCPTR (gst_opus_dec_getcaps);

  gst_element_class_add_static_pad_template (element_class,
      &opus_dec_src_factory);
  gst_element_class_add_static_pad_template (element_class,
      &opus_dec_sink_factory);
  gst_element_class_set_static_metadata (element_class, "Opus audio decoder",
      "Codec/Decoder/Audio/Converter", "decode opus streams to audio",
      "Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>");
  g_object_class_install_property (gobject_class, PROP_USE_INBAND_FEC,
      g_param_spec_boolean ("use-inband-fec", "Use in-band FEC",
          "Use forward error correction if available (needs PLC enabled)",
          DEFAULT_USE_INBAND_FEC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_APPLY_GAIN,
      g_param_spec_boolean ("apply-gain", "Apply gain",
          "Apply gain if any is specified in the header", DEFAULT_APPLY_GAIN,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

#ifdef OPUS_SET_PHASE_INVERSION_DISABLED_REQUEST
  g_object_class_install_property (gobject_class, PROP_PHASE_INVERSION,
      g_param_spec_boolean ("phase-inversion",
          "Control Phase Inversion", "Set to true to enable phase inversion, "
          "this will slightly improve stereo quality, but will have side "
          "effects when downmixed to mono.", DEFAULT_PHASE_INVERSION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

#endif

  /**
   * GstOpusDec:stats:
   *
   * Various decoder statistics. This property returns a GstStructure
   * with name application/x-opusdec-stats with the following fields:
   *
   * * #guint64 `num-pushed`: the number of packets pushed out.
   * * #guint64 `num-gap`: the number of gap packets received.
   * * #guint64 `plc-num-samples`: the number of samples generated using PLC
   * * #guint64 `plc-duration`: the total duration, in ns, of samples generated using PLC
   * * #guint32 `bandwidth`: decoder last bandpass, in kHz, or 0 if unknown
   * * #guint32 `sample-rate`: decoder sampling rate, or 0 if unknown
   * * #guint32 `gain`: decoder gain adjustement, in Q8 dB units, or 0 if unknown
   * * #guint32 `last-packet-duration`: duration, in samples, of the last packet successfully decoded or concealed, or 0 if unknown
   * * #guint `channels`: the number of channels
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class, PROP_STATS,
      g_param_spec_boxed ("stats", "Statistics",
          "Various statistics", GST_TYPE_STRUCTURE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (opusdec_debug, "opusdec", 0,
      "opus decoding element");
}

static void
gst_opus_dec_reset (GstOpusDec * dec)
{
  dec->packetno = 0;
  if (dec->state) {
    opus_multistream_decoder_destroy (dec->state);
    dec->state = NULL;
  }

  gst_buffer_replace (&dec->streamheader, NULL);
  gst_buffer_replace (&dec->vorbiscomment, NULL);
  gst_buffer_replace (&dec->last_buffer, NULL);
  dec->primed = FALSE;

  dec->pre_skip = 0;
  dec->r128_gain = 0;
  dec->sample_rate = 0;
  dec->n_channels = 0;
  dec->leftover_plc_duration = 0;
  dec->last_known_buffer_duration = GST_CLOCK_TIME_NONE;
}

static void
gst_opus_dec_init (GstOpusDec * dec)
{
  dec->use_inband_fec = FALSE;
  dec->apply_gain = DEFAULT_APPLY_GAIN;
  dec->phase_inversion = DEFAULT_PHASE_INVERSION;

  gst_audio_decoder_set_needs_format (GST_AUDIO_DECODER (dec), TRUE);
  gst_audio_decoder_set_use_default_pad_acceptcaps (GST_AUDIO_DECODER_CAST
      (dec), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_DECODER_SINK_PAD (dec));

  gst_opus_dec_reset (dec);
}

static gboolean
gst_opus_dec_start (GstAudioDecoder * dec)
{
  GstOpusDec *odec = GST_OPUS_DEC (dec);

  gst_opus_dec_reset (odec);

  /* we know about concealment */
  gst_audio_decoder_set_plc_aware (dec, TRUE);

  if (odec->use_inband_fec) {
    /* opusdec outputs samples directly from an input buffer, except if
     * FEC is on, in which case it buffers one buffer in case one buffer
     * goes missing.
     */
    gst_audio_decoder_set_latency (dec, 120 * GST_MSECOND, 120 * GST_MSECOND);
  }

  GST_OBJECT_LOCK (dec);
  odec->num_pushed = 0;
  odec->num_gap = 0;
  odec->plc_num_samples = 0;
  odec->plc_duration = 0;
  GST_OBJECT_UNLOCK (dec);

  return TRUE;
}

static gboolean
gst_opus_dec_stop (GstAudioDecoder * dec)
{
  GstOpusDec *odec = GST_OPUS_DEC (dec);

  gst_opus_dec_reset (odec);

  return TRUE;
}

static double
gst_opus_dec_get_r128_gain (gint16 r128_gain)
{
  return r128_gain / (double) (1 << 8);
}

static double
gst_opus_dec_get_r128_volume (gint16 r128_gain)
{
  return DB_TO_LINEAR (gst_opus_dec_get_r128_gain (r128_gain));
}

static gboolean
gst_opus_dec_negotiate (GstOpusDec * dec, const GstAudioChannelPosition * pos)
{
  GstCaps *caps = gst_pad_get_allowed_caps (GST_AUDIO_DECODER_SRC_PAD (dec));
  GstStructure *s;
  GstAudioInfo info;

  if (caps) {
    gint rate = dec->sample_rate, channels = dec->n_channels;
    GstCaps *constraint, *inter;

    constraint = gst_caps_new_empty_simple ("audio/x-raw");
    if (dec->n_channels <= 2) { /* including 0 */
      gst_caps_set_simple (constraint, "channels", GST_TYPE_INT_RANGE, 1, 2,
          NULL);
    } else {
      gst_caps_set_simple (constraint, "channels", G_TYPE_INT, dec->n_channels,
          NULL);
    }

    inter = gst_caps_intersect (caps, constraint);
    gst_caps_unref (constraint);

    if (gst_caps_is_empty (inter)) {
      GST_DEBUG_OBJECT (dec, "Empty intersection, failed to negotiate");
      gst_caps_unref (inter);
      gst_caps_unref (caps);
      return FALSE;
    }

    /* If we have a channels preference (0 means we prefer 2), then check if
     * we can passthrough that. The preferred channel count might not be in
     * the first structure! */
    if (dec->n_channels <= 2) {
      GstCaps *preferred =
          gst_caps_new_simple ("audio/x-raw", "channels", G_TYPE_INT,
          dec->n_channels > 0 ? dec->n_channels : 2, NULL);
      GstCaps *tmp;

      tmp = gst_caps_intersect (inter, preferred);
      if (!gst_caps_is_empty (tmp)) {
        gst_caps_unref (inter);
        inter = tmp;
      }

      gst_caps_unref (preferred);
    }

    /* If we have a rate preference, then check if we can passthrough that.
     * The preferred rate might not be in the first structure! */
    {
      GstCaps *preferred =
          gst_caps_new_simple ("audio/x-raw", "rate", G_TYPE_INT,
          dec->sample_rate > 0 ? dec->sample_rate : 48000, NULL);
      GstCaps *tmp;

      tmp = gst_caps_intersect (inter, preferred);
      if (!gst_caps_is_empty (tmp)) {
        gst_caps_unref (inter);
        inter = tmp;
      }

      gst_caps_unref (preferred);
    }

    inter = gst_caps_truncate (inter);
    s = gst_caps_get_structure (inter, 0);
    rate = dec->sample_rate > 0 ? dec->sample_rate : 48000;
    gst_structure_fixate_field_nearest_int (s, "rate", dec->sample_rate);
    gst_structure_get_int (s, "rate", &rate);
    channels = dec->n_channels > 0 ? dec->n_channels : 2;
    gst_structure_fixate_field_nearest_int (s, "channels", channels);
    gst_structure_get_int (s, "channels", &channels);

    gst_caps_unref (inter);

    dec->sample_rate = rate;
    dec->n_channels = channels;
    gst_caps_unref (caps);
  }

  if (dec->n_channels == 0) {
    GST_DEBUG_OBJECT (dec, "Using a default of 2 channels");
    dec->n_channels = 2;
    pos = NULL;
  }

  if (dec->sample_rate == 0) {
    GST_DEBUG_OBJECT (dec, "Using a default of 48kHz sample rate");
    dec->sample_rate = 48000;
  }

  GST_INFO_OBJECT (dec, "Negotiated %d channels, %d Hz", dec->n_channels,
      dec->sample_rate);

  /* pass valid order to audio info */
  if (pos) {
    memcpy (dec->opus_pos, pos, sizeof (pos[0]) * dec->n_channels);
    gst_audio_channel_positions_to_valid_order (dec->opus_pos, dec->n_channels);
  }

  /* set up source format */
  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S16,
      dec->sample_rate, dec->n_channels, pos ? dec->opus_pos : NULL);
  gst_audio_decoder_set_output_format (GST_AUDIO_DECODER (dec), &info);

  /* but we still need the opus order for later reordering */
  if (pos) {
    memcpy (dec->opus_pos, pos, sizeof (pos[0]) * dec->n_channels);
  } else {
    dec->opus_pos[0] = GST_AUDIO_CHANNEL_POSITION_INVALID;
  }

  dec->info = info;

  return TRUE;
}

static GstFlowReturn
gst_opus_dec_parse_header (GstOpusDec * dec, GstBuffer * buf)
{
  GstAudioChannelPosition pos[64];
  const GstAudioChannelPosition *posn = NULL;
  guint8 n_channels;

  if (!gst_opus_header_is_id_header (buf)) {
    GST_ELEMENT_ERROR (dec, STREAM, FORMAT, (NULL),
        ("Header is not an Opus ID header"));
    return GST_FLOW_ERROR;
  }

  if (!gst_codec_utils_opus_parse_header (buf,
          &dec->sample_rate,
          &n_channels,
          &dec->channel_mapping_family,
          &dec->n_streams,
          &dec->n_stereo_streams,
          dec->channel_mapping, &dec->pre_skip, &dec->r128_gain)) {
    GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL),
        ("Failed to parse Opus ID header"));
    return GST_FLOW_ERROR;
  }
  dec->n_channels = n_channels;
  dec->r128_gain_volume = gst_opus_dec_get_r128_volume (dec->r128_gain);

  GST_INFO_OBJECT (dec,
      "Found pre-skip of %u samples, R128 gain %d (volume %f)",
      dec->pre_skip, dec->r128_gain, dec->r128_gain_volume);

  if (dec->channel_mapping_family == 1) {
    GST_INFO_OBJECT (dec, "Channel mapping family 1, Vorbis mapping");
    switch (dec->n_channels) {
      case 1:
      case 2:
        /* nothing */
        break;
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
        posn = gst_opus_channel_positions[dec->n_channels - 1];
        break;
      default:{
        gint i;

        GST_ELEMENT_WARNING (GST_ELEMENT (dec), STREAM, DECODE,
            (NULL), ("Using NONE channel layout for more than 8 channels"));

        for (i = 0; i < dec->n_channels; i++)
          pos[i] = GST_AUDIO_CHANNEL_POSITION_NONE;

        posn = pos;
      }
    }
  } else {
    GST_INFO_OBJECT (dec, "Channel mapping family %d",
        dec->channel_mapping_family);
  }

  if (!gst_opus_dec_negotiate (dec, posn))
    return GST_FLOW_NOT_NEGOTIATED;

  return GST_FLOW_OK;
}


static GstFlowReturn
gst_opus_dec_parse_comments (GstOpusDec * dec, GstBuffer * buf)
{
  return GST_FLOW_OK;
}

/* adapted from ext/ogg/gstoggstream.c */
static gint64
packet_duration_opus (const unsigned char *data, size_t bytes)
{
  static const guint64 durations[32] = {
    480, 960, 1920, 2880,       /* Silk NB */
    480, 960, 1920, 2880,       /* Silk MB */
    480, 960, 1920, 2880,       /* Silk WB */
    480, 960,                   /* Hybrid SWB */
    480, 960,                   /* Hybrid FB */
    120, 240, 480, 960,         /* CELT NB */
    120, 240, 480, 960,         /* CELT NB */
    120, 240, 480, 960,         /* CELT NB */
    120, 240, 480, 960,         /* CELT NB */
  };

  gint64 duration;
  gint64 frame_duration;
  gint nframes = 0;
  guint8 toc;

  if (bytes < 1)
    return 0;

  /* headers */
  if (bytes >= 8 && !memcmp (data, "Opus", 4))
    return 0;

  toc = data[0];

  frame_duration = durations[toc >> 3];
  switch (toc & 3) {
    case 0:
      nframes = 1;
      break;
    case 1:
      nframes = 2;
      break;
    case 2:
      nframes = 2;
      break;
    case 3:
      if (bytes < 2) {
        GST_WARNING ("Code 3 Opus packet has less than 2 bytes");
        return 0;
      }
      nframes = data[1] & 63;
      break;
  }

  duration = nframes * frame_duration;
  if (duration > 5760) {
    GST_WARNING ("Opus packet duration > 120 ms, invalid");
    return 0;
  }
  GST_LOG ("Opus packet: frame size %.1f ms, %d frames, duration %.1f ms",
      frame_duration / 48.f, nframes, duration / 48.f);
  return duration / 48.f * 1000000;
}

static GstFlowReturn
opus_dec_chain_parse_data (GstOpusDec * dec, GstBuffer * buffer)
{
  GstFlowReturn res = GST_FLOW_OK;
  gsize size;
  guint8 *data;
  GstBuffer *outbuf, *bufd;
  gint16 *out_data;
  int n, err;
  int samples;
  unsigned int packet_size;
  GstBuffer *buf;
  GstMapInfo map, omap;
  GstAudioClippingMeta *cmeta = NULL;

  if (dec->state == NULL) {
    /* If we did not get any headers, default to 2 channels */
    if (dec->n_channels == 0) {
      GST_INFO_OBJECT (dec, "No header, assuming single stream");
      dec->n_channels = 2;
      dec->sample_rate = 48000;
      /* default stereo mapping */
      dec->channel_mapping_family = 0;
      dec->channel_mapping[0] = 0;
      dec->channel_mapping[1] = 1;
      dec->n_streams = 1;
      dec->n_stereo_streams = 1;

      if (!gst_opus_dec_negotiate (dec, NULL))
        return GST_FLOW_NOT_NEGOTIATED;
    }

    if (dec->n_channels == 2 && dec->n_streams == 1
        && dec->n_stereo_streams == 0) {
      /* if we are automatically decoding 2 channels, but only have
         a single encoded one, direct both channels to it */
      dec->channel_mapping[1] = 0;
    }

    GST_DEBUG_OBJECT (dec, "Creating decoder with %d channels, %d Hz",
        dec->n_channels, dec->sample_rate);
#ifndef GST_DISABLE_GST_DEBUG
    gst_opus_common_log_channel_mapping_table (GST_ELEMENT (dec), opusdec_debug,
        "Mapping table", dec->n_channels, dec->channel_mapping);
#endif

    GST_DEBUG_OBJECT (dec, "%d streams, %d stereo", dec->n_streams,
        dec->n_stereo_streams);
    dec->state =
        opus_multistream_decoder_create (dec->sample_rate, dec->n_channels,
        dec->n_streams, dec->n_stereo_streams, dec->channel_mapping, &err);
    if (!dec->state || err != OPUS_OK)
      goto creation_failed;

#ifdef OPUS_SET_PHASE_INVERSION_DISABLED_REQUEST
    {
      int err;
      err = opus_multistream_decoder_ctl (dec->state,
          OPUS_SET_PHASE_INVERSION_DISABLED (!dec->phase_inversion));
      if (err != OPUS_OK)
        GST_WARNING_OBJECT (dec, "Could not configure phase inversion: %s",
            opus_strerror (err));
    }
#else
    GST_WARNING_OBJECT (dec, "Phase inversion request is not support by this "
        "version of the Opus Library");
#endif
  }

  if (buffer) {
    GST_DEBUG_OBJECT (dec, "Received buffer of size %" G_GSIZE_FORMAT,
        gst_buffer_get_size (buffer));
  } else {
    GST_DEBUG_OBJECT (dec, "Received missing buffer");
  }

  /* if using in-band FEC, we introdude one extra frame's delay as we need
     to potentially wait for next buffer to decode a missing buffer */
  if (dec->use_inband_fec && !dec->primed) {
    GST_DEBUG_OBJECT (dec, "First buffer received in FEC mode, early out");
    gst_buffer_replace (&dec->last_buffer, buffer);
    dec->primed = TRUE;
    goto done;
  }

  /* That's the buffer we'll be sending to the opus decoder. */
  buf = (dec->use_inband_fec
      && gst_buffer_get_size (dec->last_buffer) >
      0) ? dec->last_buffer : buffer;

  /* That's the buffer we get duration from */
  bufd = dec->use_inband_fec ? dec->last_buffer : buffer;

  if (buf && gst_buffer_get_size (buf) > 0) {
    gst_buffer_map (buf, &map, GST_MAP_READ);
    data = map.data;
    size = map.size;
    GST_DEBUG_OBJECT (dec, "Using buffer of size %" G_GSIZE_FORMAT, size);
  } else {
    /* concealment data, pass NULL as the bits parameters */
    GST_DEBUG_OBJECT (dec, "Using NULL buffer");
    data = NULL;
    size = 0;
  }

  if (gst_buffer_get_size (bufd) == 0) {
    GstClockTime const opus_plc_alignment = 2500 * GST_USECOND;
    GstClockTime aligned_missing_duration;
    GstClockTime missing_duration = GST_BUFFER_DURATION (bufd);

    if (!GST_CLOCK_TIME_IS_VALID (missing_duration) || missing_duration == 0) {
      if (GST_CLOCK_TIME_IS_VALID (dec->last_known_buffer_duration)) {
        missing_duration = dec->last_known_buffer_duration;
        GST_WARNING_OBJECT (dec,
            "Missing duration, using last duration %" GST_TIME_FORMAT,
            GST_TIME_ARGS (missing_duration));
      } else {
        GST_WARNING_OBJECT (dec,
            "Missing buffer, but unknown duration, and no previously known duration, assuming 20 ms");
        missing_duration = 20 * GST_MSECOND;
      }
    }

    GST_DEBUG_OBJECT (dec,
        "missing buffer, doing PLC duration %" GST_TIME_FORMAT
        " plus leftover %" GST_TIME_FORMAT, GST_TIME_ARGS (missing_duration),
        GST_TIME_ARGS (dec->leftover_plc_duration));

    GST_OBJECT_LOCK (dec);
    dec->num_gap++;
    GST_OBJECT_UNLOCK (dec);

    /* add the leftover PLC duration to that of the buffer */
    missing_duration += dec->leftover_plc_duration;

    /* align the combined buffer and leftover PLC duration to multiples
     * of 2.5ms, rounding to nearest, and store excess duration for later */
    aligned_missing_duration =
        ((missing_duration +
            opus_plc_alignment / 2) / opus_plc_alignment) * opus_plc_alignment;
    dec->leftover_plc_duration = missing_duration - aligned_missing_duration;

    /* Opus' PLC cannot operate with less than 2.5ms; skip PLC
     * and accumulate the missing duration in the leftover_plc_duration
     * for the next PLC attempt */
    if (aligned_missing_duration < opus_plc_alignment) {
      GST_DEBUG_OBJECT (dec,
          "current duration %" GST_TIME_FORMAT
          " of missing data not enough for PLC (minimum needed: %"
          GST_TIME_FORMAT ") - skipping", GST_TIME_ARGS (missing_duration),
          GST_TIME_ARGS (opus_plc_alignment));
      goto done;
    }

    /* convert the duration (in nanoseconds) to sample count */
    samples =
        gst_util_uint64_scale_int (aligned_missing_duration, dec->sample_rate,
        GST_SECOND);

    GST_DEBUG_OBJECT (dec,
        "calculated PLC frame length: %" GST_TIME_FORMAT
        " num frame samples: %d new leftover: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (aligned_missing_duration), samples,
        GST_TIME_ARGS (dec->leftover_plc_duration));

    GST_OBJECT_LOCK (dec);
    dec->plc_num_samples += samples;
    dec->plc_duration += aligned_missing_duration;
    GST_OBJECT_UNLOCK (dec);
  } else {
    /* use maximum size (120 ms) as the number of returned samples is
       not constant over the stream. */
    samples = 120 * dec->sample_rate / 1000;
  }
  packet_size = samples * dec->n_channels * 2;

  outbuf =
      gst_audio_decoder_allocate_output_buffer (GST_AUDIO_DECODER (dec),
      packet_size);
  if (!outbuf) {
    goto buffer_failed;
  }

  if (size > 0)
    dec->last_known_buffer_duration = packet_duration_opus (data, size);

  gst_buffer_map (outbuf, &omap, GST_MAP_WRITE);
  out_data = (gint16 *) omap.data;

  do {
    if (dec->use_inband_fec) {
      if (gst_buffer_get_size (dec->last_buffer) > 0) {
        /* normal delayed decode */
        GST_LOG_OBJECT (dec, "FEC enabled, decoding last delayed buffer");
        n = opus_multistream_decode (dec->state, data, size, out_data, samples,
            0);
      } else {
        /* FEC reconstruction decode */
        GST_LOG_OBJECT (dec, "FEC enabled, reconstructing last buffer");
        n = opus_multistream_decode (dec->state, data, size, out_data, samples,
            1);
      }
    } else {
      /* normal decode */
      GST_LOG_OBJECT (dec, "FEC disabled, decoding buffer");
      n = opus_multistream_decode (dec->state, data, size, out_data, samples,
          0);
    }
    if (n == OPUS_BUFFER_TOO_SMALL) {
      /* if too small, add 2.5 milliseconds and try again, up to the
       * Opus max size of 120 milliseconds */
      if (samples >= 120 * dec->sample_rate / 1000)
        break;
      samples += 25 * dec->sample_rate / 10000;
      packet_size = samples * dec->n_channels * 2;
      gst_buffer_unmap (outbuf, &omap);
      gst_buffer_unref (outbuf);
      outbuf =
          gst_audio_decoder_allocate_output_buffer (GST_AUDIO_DECODER (dec),
          packet_size);
      if (!outbuf) {
        goto buffer_failed;
      }
      gst_buffer_map (outbuf, &omap, GST_MAP_WRITE);
      out_data = (gint16 *) omap.data;
    }
  } while (n == OPUS_BUFFER_TOO_SMALL);
  gst_buffer_unmap (outbuf, &omap);
  if (data != NULL)
    gst_buffer_unmap (buf, &map);

  if (n < 0) {
    GstFlowReturn ret = GST_FLOW_ERROR;

    gst_buffer_unref (outbuf);
    GST_AUDIO_DECODER_ERROR (dec, 1, STREAM, DECODE, (NULL),
        ("Decoding error (%d): %s", n, opus_strerror (n)), ret);
    return ret;
  }
  GST_DEBUG_OBJECT (dec, "decoded %d samples", n);
  gst_buffer_set_size (outbuf, n * 2 * dec->n_channels);
  GST_BUFFER_DURATION (outbuf) = samples * GST_SECOND / dec->sample_rate;
  samples = n;

  cmeta = gst_buffer_get_audio_clipping_meta (buf);

  g_assert (!cmeta || cmeta->format == GST_FORMAT_DEFAULT);

  /* Skip any samples that need skipping */
  if (cmeta && cmeta->start) {
    guint pre_skip = cmeta->start;
    guint scaled_pre_skip = pre_skip * dec->sample_rate / 48000;
    guint skip = scaled_pre_skip > n ? n : scaled_pre_skip;
    guint scaled_skip = skip * 48000 / dec->sample_rate;

    gst_buffer_resize (outbuf, skip * 2 * dec->n_channels, -1);

    GST_INFO_OBJECT (dec,
        "Skipping %u samples at the beginning (%u at 48000 Hz)",
        skip, scaled_skip);
  }

  if (cmeta && cmeta->end) {
    guint post_skip = cmeta->end;
    guint scaled_post_skip = post_skip * dec->sample_rate / 48000;
    guint skip = scaled_post_skip > n ? n : scaled_post_skip;
    guint scaled_skip = skip * 48000 / dec->sample_rate;
    guint outsize = gst_buffer_get_size (outbuf);
    guint skip_bytes = skip * 2 * dec->n_channels;

    if (outsize > skip_bytes)
      outsize -= skip_bytes;
    else
      outsize = 0;

    gst_buffer_resize (outbuf, 0, outsize);

    GST_INFO_OBJECT (dec,
        "Skipping %u samples at the end (%u at 48000 Hz)", skip, scaled_skip);
  }

  if (gst_buffer_get_size (outbuf) == 0) {
    gst_buffer_unref (outbuf);
    outbuf = NULL;
  } else if (dec->opus_pos[0] != GST_AUDIO_CHANNEL_POSITION_INVALID) {
    gst_audio_buffer_reorder_channels (outbuf, GST_AUDIO_FORMAT_S16,
        dec->n_channels, dec->opus_pos, dec->info.position);
  }

  /* Apply gain */
  /* Would be better off leaving this to a volume element, as this is
     a naive conversion that does too many int/float conversions.
     However, we don't have control over the pipeline...
     So make it optional if the user program wants to use a volume,
     but do it by default so the correct volume goes out by default */
  if (dec->apply_gain && outbuf && dec->r128_gain) {
    gsize rsize;
    unsigned int i, nsamples;
    double volume = dec->r128_gain_volume;
    gint16 *samples;

    gst_buffer_map (outbuf, &omap, GST_MAP_READWRITE);
    samples = (gint16 *) omap.data;
    rsize = omap.size;
    GST_DEBUG_OBJECT (dec, "Applying gain: volume %f", volume);
    nsamples = rsize / 2;
    for (i = 0; i < nsamples; ++i) {
      int sample = (int) (samples[i] * volume + 0.5);
      samples[i] = sample < -32768 ? -32768 : sample > 32767 ? 32767 : sample;
    }
    gst_buffer_unmap (outbuf, &omap);
  }

  if (dec->use_inband_fec) {
    gst_buffer_replace (&dec->last_buffer, buffer);
  }

  GST_OBJECT_LOCK (dec);
  dec->num_pushed++;
  GST_OBJECT_UNLOCK (dec);

  res = gst_audio_decoder_finish_frame (GST_AUDIO_DECODER (dec), outbuf, 1);

  if (res != GST_FLOW_OK)
    GST_DEBUG_OBJECT (dec, "flow: %s", gst_flow_get_name (res));

done:
  return res;

creation_failed:
  GST_ELEMENT_ERROR (dec, LIBRARY, INIT, ("Failed to create Opus decoder"),
      ("Failed to create Opus decoder (%d): %s", err, opus_strerror (err)));
  return GST_FLOW_ERROR;

buffer_failed:
  GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL),
      ("Failed to create %u byte buffer", packet_size));
  return GST_FLOW_ERROR;
}

static gboolean
gst_opus_dec_set_format (GstAudioDecoder * bdec, GstCaps * caps)
{
  GstOpusDec *dec = GST_OPUS_DEC (bdec);
  gboolean ret = TRUE;
  GstStructure *s;
  const GValue *streamheader;
  GstCaps *old_caps;

  GST_DEBUG_OBJECT (dec, "set_format: %" GST_PTR_FORMAT, caps);

  if ((old_caps = gst_pad_get_current_caps (GST_AUDIO_DECODER_SINK_PAD (bdec)))) {
    if (gst_caps_is_equal (caps, old_caps)) {
      gst_caps_unref (old_caps);
      GST_DEBUG_OBJECT (dec, "caps didn't change");
      goto done;
    }

    GST_DEBUG_OBJECT (dec, "caps have changed, resetting decoder");
    gst_opus_dec_reset (dec);
    gst_caps_unref (old_caps);
  }

  s = gst_caps_get_structure (caps, 0);
  if ((streamheader = gst_structure_get_value (s, "streamheader")) &&
      G_VALUE_HOLDS (streamheader, GST_TYPE_ARRAY) &&
      gst_value_array_get_size (streamheader) >= 2) {
    const GValue *header, *vorbiscomment;
    GstBuffer *buf;
    GstFlowReturn res = GST_FLOW_OK;

    header = gst_value_array_get_value (streamheader, 0);
    if (header && G_VALUE_HOLDS (header, GST_TYPE_BUFFER)) {
      buf = gst_value_get_buffer (header);
      res = gst_opus_dec_parse_header (dec, buf);
      if (res != GST_FLOW_OK) {
        ret = FALSE;
        goto done;
      }
      gst_buffer_replace (&dec->streamheader, buf);
    }

    vorbiscomment = gst_value_array_get_value (streamheader, 1);
    if (vorbiscomment && G_VALUE_HOLDS (vorbiscomment, GST_TYPE_BUFFER)) {
      buf = gst_value_get_buffer (vorbiscomment);
      res = gst_opus_dec_parse_comments (dec, buf);
      if (res != GST_FLOW_OK) {
        ret = FALSE;
        goto done;
      }
      gst_buffer_replace (&dec->vorbiscomment, buf);
    }
  } else {
    const GstAudioChannelPosition *posn = NULL;
    guint8 n_channels;

    if (!gst_codec_utils_opus_parse_caps (caps, &dec->sample_rate,
            &n_channels, &dec->channel_mapping_family,
            &dec->n_streams, &dec->n_stereo_streams, dec->channel_mapping)) {
      ret = FALSE;
      goto done;
    }
    dec->n_channels = n_channels;

    if (dec->channel_mapping_family == 1 && dec->n_channels <= 8)
      posn = gst_opus_channel_positions[dec->n_channels - 1];

    if (!gst_opus_dec_negotiate (dec, posn))
      return FALSE;
  }

done:
  return ret;
}

static gboolean
memcmp_buffers (GstBuffer * buf1, GstBuffer * buf2)
{
  gsize size1, size2;
  gboolean res;
  GstMapInfo map;

  size1 = gst_buffer_get_size (buf1);
  size2 = gst_buffer_get_size (buf2);

  if (size1 != size2)
    return FALSE;

  gst_buffer_map (buf1, &map, GST_MAP_READ);
  res = gst_buffer_memcmp (buf2, 0, map.data, map.size) == 0;
  gst_buffer_unmap (buf1, &map);

  return res;
}

static GstFlowReturn
gst_opus_dec_handle_frame (GstAudioDecoder * adec, GstBuffer * buf)
{
  GstFlowReturn res;
  GstOpusDec *dec;

  /* no fancy draining */
  if (G_UNLIKELY (!buf))
    return GST_FLOW_OK;

  dec = GST_OPUS_DEC (adec);
  GST_LOG_OBJECT (dec,
      "Got buffer ts %" GST_TIME_FORMAT ", duration %" GST_TIME_FORMAT,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buf)));

  /* If we have the streamheader and vorbiscomment from the caps already
   * ignore them here */
  if (dec->streamheader && dec->vorbiscomment) {
    if (memcmp_buffers (dec->streamheader, buf)) {
      GST_DEBUG_OBJECT (dec, "found streamheader");
      gst_audio_decoder_finish_frame (adec, NULL, 1);
      res = GST_FLOW_OK;
    } else if (memcmp_buffers (dec->vorbiscomment, buf)) {
      GST_DEBUG_OBJECT (dec, "found vorbiscomments");
      gst_audio_decoder_finish_frame (adec, NULL, 1);
      res = GST_FLOW_OK;
    } else {
      res = opus_dec_chain_parse_data (dec, buf);
    }
  } else {
    /* Otherwise fall back to packet counting and assume that the
     * first two packets might be the headers, checking magic. */
    switch (dec->packetno) {
      case 0:
        if (gst_opus_header_is_header (buf, "OpusHead", 8)) {
          GST_DEBUG_OBJECT (dec, "found streamheader");
          res = gst_opus_dec_parse_header (dec, buf);
          gst_audio_decoder_finish_frame (adec, NULL, 1);
        } else {
          res = opus_dec_chain_parse_data (dec, buf);
        }
        break;
      case 1:
        if (gst_opus_header_is_header (buf, "OpusTags", 8)) {
          GST_DEBUG_OBJECT (dec, "counted vorbiscomments");
          res = gst_opus_dec_parse_comments (dec, buf);
          gst_audio_decoder_finish_frame (adec, NULL, 1);
        } else {
          res = opus_dec_chain_parse_data (dec, buf);
        }
        break;
      default:
      {
        res = opus_dec_chain_parse_data (dec, buf);
        break;
      }
    }
  }

  dec->packetno++;

  return res;
}

/* Called with object lock hold */
static guint32
get_bandwidth (GstOpusDec * self)
{
  gint err;
  gint32 bw;

  if (!self->state)
    return 0;

  err = opus_multistream_decoder_ctl (self->state, OPUS_GET_BANDWIDTH (&bw));
  if (err != OPUS_OK) {
    GST_WARNING_OBJECT (self, "Could not retrieve bandwith: %s",
        opus_strerror (err));
    return 0;
  }

  switch (bw) {
    case OPUS_BANDWIDTH_NARROWBAND:
      return 4;
    case OPUS_BANDWIDTH_MEDIUMBAND:
      return 6;
    case OPUS_BANDWIDTH_WIDEBAND:
      return 8;
    case OPUS_BANDWIDTH_SUPERWIDEBAND:
      return 12;
    case OPUS_BANDWIDTH_FULLBAND:
      return 20;
    default:
      GST_WARNING_OBJECT (self, "Unknown bandwith enum: %d", bw);
      return 0;
  }
}

/* Called with object lock hold */
static guint32
get_sample_rate (GstOpusDec * self)
{
  gint err;
  gint32 rate;

  if (!self->state)
    return 0;

  err =
      opus_multistream_decoder_ctl (self->state, OPUS_GET_SAMPLE_RATE (&rate));
  if (err != OPUS_OK) {
    GST_WARNING_OBJECT (self, "Could not retrieve sample rate: %s",
        opus_strerror (err));
    return 0;
  }

  return rate;
}

/* Called with object lock hold */
static guint32
get_gain (GstOpusDec * self)
{
  gint err;
  gint32 gain;

  if (!self->state)
    return 0;

  err = opus_multistream_decoder_ctl (self->state, OPUS_GET_GAIN (&gain));
  if (err != OPUS_OK) {
    GST_WARNING_OBJECT (self, "Could not retrieve gain: %s",
        opus_strerror (err));
    return 0;
  }

  return gain;
}

/* Called with object lock hold */
static guint32
get_last_packet_duration (GstOpusDec * self)
{
  gint err;
  gint32 duration;

  if (!self->state)
    return 0;

  err =
      opus_multistream_decoder_ctl (self->state,
      OPUS_GET_LAST_PACKET_DURATION (&duration));
  if (err != OPUS_OK) {
    GST_WARNING_OBJECT (self, "Could not retrieve last packet duration: %s",
        opus_strerror (err));
    return 0;
  }

  return duration;
}

static GstStructure *
gst_opus_dec_create_stats (GstOpusDec * self)
{
  GstStructure *s;

  GST_OBJECT_LOCK (self);

  s = gst_structure_new ("application/x-opusdec-stats",
      "num-pushed", G_TYPE_UINT64, self->num_pushed,
      "num-gap", G_TYPE_UINT64, self->num_gap,
      "plc-num-samples", G_TYPE_UINT64, self->plc_num_samples,
      "plc-duration", G_TYPE_UINT64, self->plc_duration,
      "bandwidth", G_TYPE_UINT, get_bandwidth (self),
      "sample-rate", G_TYPE_UINT, get_sample_rate (self),
      "gain", G_TYPE_UINT, get_gain (self),
      "last-packet-duration", G_TYPE_UINT, get_last_packet_duration (self),
      "channels", G_TYPE_UINT, self->n_channels, NULL);

  GST_OBJECT_UNLOCK (self);

  return s;
}

static void
gst_opus_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOpusDec *dec = GST_OPUS_DEC (object);

  switch (prop_id) {
    case PROP_USE_INBAND_FEC:
      g_value_set_boolean (value, dec->use_inband_fec);
      break;
    case PROP_APPLY_GAIN:
      g_value_set_boolean (value, dec->apply_gain);
      break;
    case PROP_PHASE_INVERSION:
      g_value_set_boolean (value, dec->phase_inversion);
      break;
    case PROP_STATS:
      g_value_take_boxed (value, gst_opus_dec_create_stats (dec));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_opus_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOpusDec *dec = GST_OPUS_DEC (object);

  switch (prop_id) {
    case PROP_USE_INBAND_FEC:
      dec->use_inband_fec = g_value_get_boolean (value);
      break;
    case PROP_APPLY_GAIN:
      dec->apply_gain = g_value_get_boolean (value);
      break;
    case PROP_PHASE_INVERSION:
      dec->phase_inversion = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* caps must be writable */
static void
gst_opus_dec_caps_extend_channels_options (GstCaps * caps)
{
  unsigned n;
  int channels;

  for (n = 0; n < gst_caps_get_size (caps); ++n) {
    GstStructure *s = gst_caps_get_structure (caps, n);
    if (gst_structure_get_int (s, "channels", &channels)) {
      if (channels == 1 || channels == 2) {
        GValue v = { 0 };
        g_value_init (&v, GST_TYPE_INT_RANGE);
        gst_value_set_int_range (&v, 1, 2);
        gst_structure_set_value (s, "channels", &v);
        g_value_unset (&v);
      }
    }
  }
}

static void
gst_opus_dec_value_list_append_int (GValue * list, gint i)
{
  GValue v = { 0 };

  g_value_init (&v, G_TYPE_INT);
  g_value_set_int (&v, i);
  gst_value_list_append_value (list, &v);
  g_value_unset (&v);
}

static void
gst_opus_dec_caps_extend_rate_options (GstCaps * caps)
{
  unsigned n;
  GValue v = { 0 };

  g_value_init (&v, GST_TYPE_LIST);
  gst_opus_dec_value_list_append_int (&v, 48000);
  gst_opus_dec_value_list_append_int (&v, 24000);
  gst_opus_dec_value_list_append_int (&v, 16000);
  gst_opus_dec_value_list_append_int (&v, 12000);
  gst_opus_dec_value_list_append_int (&v, 8000);

  for (n = 0; n < gst_caps_get_size (caps); ++n) {
    GstStructure *s = gst_caps_get_structure (caps, n);

    gst_structure_set_value (s, "rate", &v);
  }
  g_value_unset (&v);
}

GstCaps *
gst_opus_dec_getcaps (GstAudioDecoder * dec, GstCaps * filter)
{
  GstCaps *caps, *proxy_filter = NULL, *ret;

  if (filter) {
    proxy_filter = gst_caps_copy (filter);
    gst_opus_dec_caps_extend_channels_options (proxy_filter);
    gst_opus_dec_caps_extend_rate_options (proxy_filter);
  }
  caps = gst_audio_decoder_proxy_getcaps (dec, NULL, proxy_filter);
  if (proxy_filter)
    gst_caps_unref (proxy_filter);
  if (caps) {
    caps = gst_caps_make_writable (caps);
    gst_opus_dec_caps_extend_channels_options (caps);
    gst_opus_dec_caps_extend_rate_options (caps);
  }

  if (filter) {
    ret = gst_caps_intersect (caps, filter);
    gst_caps_unref (caps);
  } else {
    ret = caps;
  }
  return ret;
}
