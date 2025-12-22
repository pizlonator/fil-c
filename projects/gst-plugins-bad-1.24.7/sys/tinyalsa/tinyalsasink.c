/* GStreamer
 * Copyright (C) 2016 Centricular Ltd.
 * Author: Arun Raghavan <arun@centricular.com>
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
 * SECTION:element-tinyalsasink
 * @title: tinyalsasink
 * @see_also: alsasink
 *
 * This element renders raw audio samples using the ALSA audio API via the
 * tinyalsa library.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v uridecodebin uri=file:///path/to/audio.ogg ! audioconvert ! audioresample ! tinyalsasink
 * ]| Play an Ogg/Vorbis file and output audio via ALSA using the tinyalsa
 * library.
 *
 */

#include <gst/audio/gstaudiobasesink.h>

#include <tinyalsa/asoundlib.h>

#include "tinyalsasink.h"

/* Hardcoding these bitmask values rather than including a kernel header */
#define SNDRV_PCM_FORMAT_S8 0
#define SNDRV_PCM_FORMAT_S16_LE 2
#define SNDRV_PCM_FORMAT_S24_LE 6
#define SNDRV_PCM_FORMAT_S32_LE 10
#define SNDRV_PCM_FORMAT_ANY            \
  ((1 << SNDRV_PCM_FORMAT_S8)     |     \
   (1 << SNDRV_PCM_FORMAT_S16_LE) |     \
   (1 << SNDRV_PCM_FORMAT_S24_LE) |     \
   (1 << SNDRV_PCM_FORMAT_S32_LE))

GST_DEBUG_CATEGORY_STATIC (tinyalsa_sink_debug);
#define GST_CAT_DEFAULT tinyalsa_sink_debug

#define parent_class gst_tinyalsa_sink_parent_class
G_DEFINE_TYPE (GstTinyalsaSink, gst_tinyalsa_sink, GST_TYPE_AUDIO_SINK);

enum
{
  PROP_0,
  PROP_CARD,
  PROP_DEVICE,
  PROP_LAST
};

#define DEFAULT_CARD 0
#define DEFAULT_DEVICE 0

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) { S16LE, S32LE, S24_32LE, S8 }, "
        "channels = (int) [ 1, MAX ], "
        "rate = (int) [ 1, MAX ], " "layout = (string) interleaved"));

static void
gst_tinyalsa_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTinyalsaSink *sink = GST_TINYALSA_SINK (object);

  switch (prop_id) {
    case PROP_CARD:
      g_value_set_uint (value, sink->card);
      break;

    case PROP_DEVICE:
      g_value_set_uint (value, sink->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_tinyalsa_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTinyalsaSink *sink = GST_TINYALSA_SINK (object);

  switch (prop_id) {
    case PROP_CARD:
      sink->card = g_value_get_uint (value);
      break;

    case PROP_DEVICE:
      sink->device = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_tinyalsa_sink_getcaps (GstBaseSink * bsink, GstCaps * filter)
{
  GstTinyalsaSink *sink = GST_TINYALSA_SINK (bsink);
  GstCaps *caps = NULL;
  GValue formats = { 0, };
  GValue format = { 0, };
  struct pcm_params *params = NULL;
  struct pcm_mask *mask;
  int rate_min, rate_max, channels_min, channels_max;
  guint16 m;

  GST_DEBUG_OBJECT (sink, "Querying caps");

  GST_OBJECT_LOCK (sink);

  if (sink->cached_caps) {
    GST_DEBUG_OBJECT (sink, "Returning cached caps");
    caps = gst_caps_ref (sink->cached_caps);
    goto done;
  }

  if (sink->pcm) {
    /* We can't query the device while it's open, so return current caps */
    caps = gst_pad_get_current_caps (GST_BASE_SINK_PAD (bsink));
    goto done;
  }

  params = pcm_params_get (sink->card, sink->device, PCM_OUT);
  if (!params) {
    GST_ERROR_OBJECT (sink, "Could not get PCM params");
    goto done;
  }

  mask = pcm_params_get_mask (params, PCM_PARAM_FORMAT);
  m = (mask->bits[1] << 8) | mask->bits[0];

  if (!(m & SNDRV_PCM_FORMAT_ANY)) {
    GST_ERROR_OBJECT (sink, "Could not find any supported format");
    goto done;
  }

  caps = gst_caps_new_empty_simple ("audio/x-raw");

  g_value_init (&formats, GST_TYPE_LIST);
  g_value_init (&format, G_TYPE_STRING);

  if (m & (1 << SNDRV_PCM_FORMAT_S8)) {
    g_value_set_static_string (&format, "S8");
    gst_value_list_prepend_value (&formats, &format);
  }
  if (m & (1 << SNDRV_PCM_FORMAT_S16_LE)) {
    g_value_set_static_string (&format, "S16LE");
    gst_value_list_prepend_value (&formats, &format);
  }
  if (m & (1 << SNDRV_PCM_FORMAT_S24_LE)) {
    g_value_set_static_string (&format, "S24_32LE");
    gst_value_list_prepend_value (&formats, &format);
  }
  if (m & (1 << SNDRV_PCM_FORMAT_S32_LE)) {
    g_value_set_static_string (&format, "S32LE");
    gst_value_list_prepend_value (&formats, &format);
  }

  gst_caps_set_value (caps, "format", &formats);

  g_value_unset (&format);
  g_value_unset (&formats);

  /* This is a bit of a lie, since the device likely only supports some
   * standard rates in this range. We should probably filter the range to
   * those, standard audio rates but even that isn't guaranteed to be accurate.
   */
  rate_min = pcm_params_get_min (params, PCM_PARAM_RATE);
  rate_max = pcm_params_get_max (params, PCM_PARAM_RATE);

  if (rate_min == rate_max)
    gst_caps_set_simple (caps, "rate", G_TYPE_INT, rate_min, NULL);
  else
    gst_caps_set_simple (caps, "rate", GST_TYPE_INT_RANGE, rate_min, rate_max,
        NULL);

  channels_min = pcm_params_get_min (params, PCM_PARAM_CHANNELS);
  channels_max = pcm_params_get_max (params, PCM_PARAM_CHANNELS);

  if (channels_min == channels_max)
    gst_caps_set_simple (caps, "channels", G_TYPE_INT, channels_min, NULL);
  else
    gst_caps_set_simple (caps, "channels", GST_TYPE_INT_RANGE, channels_min,
        channels_max, NULL);

  gst_caps_replace (&sink->cached_caps, caps);

done:
  GST_OBJECT_UNLOCK (sink);

  GST_DEBUG_OBJECT (sink, "Got caps %" GST_PTR_FORMAT, caps);

  if (caps && filter) {
    GstCaps *intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);

    gst_caps_unref (caps);
    caps = intersection;
  }

  if (params)
    pcm_params_free (params);

  return caps;
}

static gboolean
gst_tinyalsa_sink_open (GstAudioSink * asink)
{
  /* Nothing to do here, we can't call pcm_open() till we have stream
   * parameters available */
  return TRUE;
}

static enum pcm_format
pcm_format_from_gst (GstAudioFormat format)
{
  switch (format) {
    case GST_AUDIO_FORMAT_S8:
      return PCM_FORMAT_S8;

    case GST_AUDIO_FORMAT_S16LE:
      return PCM_FORMAT_S16_LE;

    case GST_AUDIO_FORMAT_S24_32LE:
      return PCM_FORMAT_S24_LE;

    case GST_AUDIO_FORMAT_S32LE:
      return PCM_FORMAT_S32_LE;

    default:
      g_assert_not_reached ();
  }
}

static void
pcm_config_from_spec (struct pcm_config *config,
    const GstAudioRingBufferSpec * spec)
{
  gint64 frames;

  config->format = pcm_format_from_gst (GST_AUDIO_INFO_FORMAT (&spec->info));
  config->channels = GST_AUDIO_INFO_CHANNELS (&spec->info);
  config->rate = GST_AUDIO_INFO_RATE (&spec->info);

  gst_audio_info_convert (&spec->info,
      GST_FORMAT_TIME, spec->latency_time * GST_USECOND,
      GST_FORMAT_DEFAULT /* frames */ , &frames);

  config->period_size = frames;
  config->period_count = spec->buffer_time / spec->latency_time;
}

static gboolean
gst_tinyalsa_sink_prepare (GstAudioSink * asink, GstAudioRingBufferSpec * spec)
{
  GstTinyalsaSink *sink = GST_TINYALSA_SINK (asink);
  struct pcm_config config = { 0, };
  struct pcm_params *params = NULL;
  int period_size_min, period_size_max;
  int periods_min, periods_max;

  pcm_config_from_spec (&config, spec);

  GST_DEBUG_OBJECT (sink, "Requesting %u periods of %u frames",
      config.period_count, config.period_size);

  params = pcm_params_get (sink->card, sink->device, PCM_OUT);
  if (!params)
    GST_ERROR_OBJECT (sink, "Could not get PCM params");

  period_size_min = pcm_params_get_min (params, PCM_PARAM_PERIOD_SIZE);
  period_size_max = pcm_params_get_max (params, PCM_PARAM_PERIOD_SIZE);
  periods_min = pcm_params_get_min (params, PCM_PARAM_PERIODS);
  periods_max = pcm_params_get_max (params, PCM_PARAM_PERIODS);

  pcm_params_free (params);

  /* Snap period size/count to the permitted range */
  config.period_size =
      CLAMP (config.period_size, period_size_min, period_size_max);
  config.period_count = CLAMP (config.period_count, periods_min, periods_max);

  /* mutex with getcaps */
  GST_OBJECT_LOCK (sink);

  sink->pcm = pcm_open (sink->card, sink->device, PCM_OUT | PCM_NORESTART,
      &config);

  GST_OBJECT_UNLOCK (sink);

  if (!sink->pcm || !pcm_is_ready (sink->pcm)) {
    GST_ERROR_OBJECT (sink, "Could not open device: %s",
        pcm_get_error (sink->pcm));
    goto fail;
  }

  if (pcm_prepare (sink->pcm) < 0) {
    GST_ERROR_OBJECT (sink, "Could not prepare device: %s",
        pcm_get_error (sink->pcm));
    goto fail;
  }

  spec->segsize = pcm_frames_to_bytes (sink->pcm, config.period_size);
  spec->segtotal = config.period_count;

  GST_DEBUG_OBJECT (sink, "Configured for %u periods of %u frames",
      config.period_count, config.period_size);

  return TRUE;

fail:
  if (sink->pcm)
    pcm_close (sink->pcm);

  return FALSE;
}

static gboolean
gst_tinyalsa_sink_unprepare (GstAudioSink * asink)
{
  GstTinyalsaSink *sink = GST_TINYALSA_SINK (asink);

  if (pcm_stop (sink->pcm) < 0) {
    GST_ERROR_OBJECT (sink, "Could not stop device: %s",
        pcm_get_error (sink->pcm));
  }

  /* mutex with getcaps */
  GST_OBJECT_LOCK (sink);

  if (pcm_close (sink->pcm)) {
    GST_ERROR_OBJECT (sink, "Could not close device: %s",
        pcm_get_error (sink->pcm));
    return FALSE;
  }

  sink->pcm = NULL;

  gst_caps_replace (&sink->cached_caps, NULL);

  GST_OBJECT_UNLOCK (sink);

  GST_DEBUG_OBJECT (sink, "Device unprepared");

  return TRUE;
}

static gboolean
gst_tinyalsa_sink_close (GstAudioSink * asink)
{
  /* Nothing to do here, see gst_tinyalsa_sink_open() */
  return TRUE;
}

static gint
gst_tinyalsa_sink_write (GstAudioSink * asink, gpointer data, guint length)
{
  GstTinyalsaSink *sink = GST_TINYALSA_SINK (asink);
  int ret;

again:
  GST_DEBUG_OBJECT (sink, "Starting write");

  ret = pcm_write (sink->pcm, data, length);
  if (ret == -EPIPE) {
    GST_WARNING_OBJECT (sink, "Got an underrun");

    if (pcm_prepare (sink->pcm) < 0) {
      GST_ERROR_OBJECT (sink, "Could not prepare device: %s",
          pcm_get_error (sink->pcm));
      return -1;
    }

    goto again;

  } else if (ret < 0) {
    GST_ERROR_OBJECT (sink, "Could not write data to device: %s",
        pcm_get_error (sink->pcm));
    return -1;
  }

  GST_DEBUG_OBJECT (sink, "Wrote %u bytes", length);

  return length;
}

static void
gst_tinyalsa_sink_reset (GstAudioSink * asink)
{
  GstTinyalsaSink *sink = GST_TINYALSA_SINK (asink);

  if (pcm_stop (sink->pcm) < 0) {
    GST_ERROR_OBJECT (sink, "Could not stop device: %s",
        pcm_get_error (sink->pcm));
  }

  if (pcm_prepare (sink->pcm) < 0) {
    GST_ERROR_OBJECT (sink, "Could not prepare device: %s",
        pcm_get_error (sink->pcm));
  }
}

static guint
gst_tinyalsa_sink_delay (GstAudioSink * asink)
{
  GstTinyalsaSink *sink = GST_TINYALSA_SINK (asink);
  int delay;

  delay = pcm_get_delay (sink->pcm);

  if (delay < 0) {
    /* This might happen before the stream has started */
    GST_DEBUG_OBJECT (sink, "Got negative delay");
    delay = 0;
  } else
    GST_DEBUG_OBJECT (sink, "Got delay of %u", delay);

  return delay;
}

static void
gst_tinyalsa_sink_class_init (GstTinyalsaSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS (klass);
  GstAudioSinkClass *audiosink_class = GST_AUDIO_SINK_CLASS (klass);

  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_tinyalsa_sink_get_property);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_tinyalsa_sink_set_property);

  basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_tinyalsa_sink_getcaps);

  audiosink_class->open = GST_DEBUG_FUNCPTR (gst_tinyalsa_sink_open);
  audiosink_class->prepare = GST_DEBUG_FUNCPTR (gst_tinyalsa_sink_prepare);
  audiosink_class->unprepare = GST_DEBUG_FUNCPTR (gst_tinyalsa_sink_unprepare);
  audiosink_class->close = GST_DEBUG_FUNCPTR (gst_tinyalsa_sink_close);
  audiosink_class->write = GST_DEBUG_FUNCPTR (gst_tinyalsa_sink_write);
  audiosink_class->reset = GST_DEBUG_FUNCPTR (gst_tinyalsa_sink_reset);
  audiosink_class->delay = GST_DEBUG_FUNCPTR (gst_tinyalsa_sink_delay);

  gst_element_class_set_static_metadata (element_class,
      "tinyalsa Audio Sink",
      "Sink/Audio", "Plays audio to an ALSA device",
      "Arun Raghavan <arun@centricular.com>");

  gst_element_class_add_static_pad_template (element_class, &sink_template);

  g_object_class_install_property (gobject_class,
      PROP_CARD,
      g_param_spec_uint ("card", "Card", "The ALSA card to use",
          0, G_MAXUINT, DEFAULT_CARD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_DEVICE,
      g_param_spec_uint ("device", "Device", "The ALSA device to use",
          0, G_MAXUINT, DEFAULT_CARD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (tinyalsa_sink_debug, "tinyalsasink", 0,
      "tinyalsa Sink");
}

static void
gst_tinyalsa_sink_init (GstTinyalsaSink * sink)
{
  sink->card = DEFAULT_CARD;
  sink->device = DEFAULT_DEVICE;

  sink->cached_caps = NULL;
}
