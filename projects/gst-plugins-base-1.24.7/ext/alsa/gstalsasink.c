/* GStreamer
 * Copyright (C) 2005 Wim Taymans <wim@fluendo.com>
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 *
 * gstalsasink.c:
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
 * SECTION:element-alsasink
 * @title: alsasink
 * @see_also: alsasrc
 *
 * This element renders audio samples using the ALSA audio API.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v uridecodebin uri=file:///path/to/audio.ogg ! audioconvert ! audioresample ! autoaudiosink
 * ]|
 *
 * Play an Ogg/Vorbis file and output audio via ALSA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <alsa/asoundlib.h>

#include "gstalsaelements.h"
#include "gstalsa.h"
#include "gstalsasink.h"

#include <gst/audio/gstaudioiec61937.h>
#include <gst/audio/gstdsd.h>
#include <glib/gi18n-lib.h>

#ifndef ESTRPIPE
#define ESTRPIPE EPIPE
#endif

#define DEFAULT_DEVICE		"default"
#define DEFAULT_DEVICE_NAME	""
#define DEFAULT_CARD_NAME	""
#define SPDIF_PERIOD_SIZE 1536
#define SPDIF_BUFFER_SIZE 15360

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_DEVICE_NAME,
  PROP_CARD_NAME,
  PROP_LAST
};

#define gst_alsasink_parent_class parent_class
G_DEFINE_TYPE (GstAlsaSink, gst_alsasink, GST_TYPE_AUDIO_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (alsasink, "alsasink", GST_RANK_PRIMARY,
    GST_TYPE_ALSA_SINK, alsa_element_init (plugin));

static void gst_alsasink_finalise (GObject * object);
static void gst_alsasink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_alsasink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstCaps *gst_alsasink_getcaps (GstBaseSink * bsink, GstCaps * filter);
static gboolean gst_alsasink_query (GstBaseSink * bsink, GstQuery * query);

static gboolean gst_alsasink_open (GstAudioSink * asink);
static gboolean gst_alsasink_prepare (GstAudioSink * asink,
    GstAudioRingBufferSpec * spec);
static gboolean gst_alsasink_unprepare (GstAudioSink * asink);
static gboolean gst_alsasink_close (GstAudioSink * asink);
static gint gst_alsasink_write (GstAudioSink * asink, gpointer data,
    guint length);
static guint gst_alsasink_delay (GstAudioSink * asink);
static void gst_alsasink_pause (GstAudioSink * asink);
static void gst_alsasink_resume (GstAudioSink * asink);
static void gst_alsasink_stop (GstAudioSink * asink);
static gboolean gst_alsasink_acceptcaps (GstAlsaSink * alsa, GstCaps * caps);
static GstBuffer *gst_alsasink_payload (GstAudioBaseSink * sink,
    GstBuffer * buf);

static gint output_ref;         /* 0    */
static snd_output_t *output;    /* NULL */
static GMutex output_mutex;

static GstStaticPadTemplate alsasink_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_FORMATS_ALL ", "
        "layout = (string) interleaved, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
        GST_DSD_MEDIA_TYPE ", "
        "format = (string) " GST_DSD_FORMATS_ALL ", "
        "layout = (string) interleaved, "
        "reversed-bytes = (gboolean) false, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
        PASSTHROUGH_CAPS)
    );

static void
gst_alsasink_finalise (GObject * object)
{
  GstAlsaSink *sink = GST_ALSA_SINK (object);

  g_free (sink->device);
  g_mutex_clear (&sink->alsa_lock);
  g_mutex_clear (&sink->delay_lock);

  g_mutex_lock (&output_mutex);
  --output_ref;
  if (output_ref == 0) {
    snd_output_close (output);
    output = NULL;
  }
  g_mutex_unlock (&output_mutex);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_alsasink_class_init (GstAlsaSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstAudioBaseSinkClass *gstbaseaudiosink_class;
  GstAudioSinkClass *gstaudiosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstbaseaudiosink_class = (GstAudioBaseSinkClass *) klass;
  gstaudiosink_class = (GstAudioSinkClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_alsasink_finalise;
  gobject_class->get_property = gst_alsasink_get_property;
  gobject_class->set_property = gst_alsasink_set_property;

  gst_element_class_set_static_metadata (gstelement_class,
      "Audio sink (ALSA)", "Sink/Audio",
      "Output to a sound card via ALSA", "Wim Taymans <wim@fluendo.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &alsasink_sink_factory);

  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_alsasink_getcaps);
  gstbasesink_class->query = GST_DEBUG_FUNCPTR (gst_alsasink_query);

  gstbaseaudiosink_class->payload = GST_DEBUG_FUNCPTR (gst_alsasink_payload);

  gstaudiosink_class->open = GST_DEBUG_FUNCPTR (gst_alsasink_open);
  gstaudiosink_class->prepare = GST_DEBUG_FUNCPTR (gst_alsasink_prepare);
  gstaudiosink_class->unprepare = GST_DEBUG_FUNCPTR (gst_alsasink_unprepare);
  gstaudiosink_class->close = GST_DEBUG_FUNCPTR (gst_alsasink_close);
  gstaudiosink_class->write = GST_DEBUG_FUNCPTR (gst_alsasink_write);
  gstaudiosink_class->delay = GST_DEBUG_FUNCPTR (gst_alsasink_delay);
  gstaudiosink_class->stop = GST_DEBUG_FUNCPTR (gst_alsasink_stop);
  gstaudiosink_class->pause = GST_DEBUG_FUNCPTR (gst_alsasink_pause);
  gstaudiosink_class->resume = GST_DEBUG_FUNCPTR (gst_alsasink_resume);

  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "ALSA device, as defined in an asound configuration file",
          DEFAULT_DEVICE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device name",
          "Human-readable name of the sound device", DEFAULT_DEVICE_NAME,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CARD_NAME,
      g_param_spec_string ("card-name", "Card name",
          "Human-readable name of the sound card", DEFAULT_CARD_NAME,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_DOC_SHOW_DEFAULT));
}

static void
gst_alsasink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAlsaSink *sink;

  sink = GST_ALSA_SINK (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_free (sink->device);
      sink->device = g_value_dup_string (value);
      /* setting NULL restores the default device */
      if (sink->device == NULL) {
        sink->device = g_strdup (DEFAULT_DEVICE);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_alsasink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAlsaSink *sink;

  sink = GST_ALSA_SINK (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, sink->device);
      break;
    case PROP_DEVICE_NAME:
      g_value_take_string (value,
          gst_alsa_find_device_name (GST_OBJECT_CAST (sink),
              sink->device, sink->handle, SND_PCM_STREAM_PLAYBACK));
      break;
    case PROP_CARD_NAME:
      g_value_take_string (value,
          gst_alsa_find_card_name (GST_OBJECT_CAST (sink),
              sink->device, SND_PCM_STREAM_PLAYBACK));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_alsasink_init (GstAlsaSink * alsasink)
{
  GST_DEBUG_OBJECT (alsasink, "initializing alsasink");

  alsasink->device = g_strdup (DEFAULT_DEVICE);
  alsasink->handle = NULL;
  alsasink->cached_caps = NULL;
  alsasink->is_paused = FALSE;
  alsasink->after_paused = FALSE;
  alsasink->hw_support_pause = FALSE;
  g_mutex_init (&alsasink->alsa_lock);
  g_mutex_init (&alsasink->delay_lock);

  g_mutex_lock (&output_mutex);
  if (output_ref == 0) {
    snd_output_stdio_attach (&output, stdout, 0);
    ++output_ref;
  }
  g_mutex_unlock (&output_mutex);
}

#define CHECK(call, error) \
G_STMT_START {             \
  if ((err = call) < 0) {  \
    GST_WARNING_OBJECT (alsa, "Error %d (%s) calling " #call, err, snd_strerror (err)); \
    goto error;            \
  }                        \
} G_STMT_END;

static GstCaps *
gst_alsasink_getcaps (GstBaseSink * bsink, GstCaps * filter)
{
  GstElementClass *element_class;
  GstPadTemplate *pad_template;
  GstAlsaSink *sink = GST_ALSA_SINK (bsink);
  GstCaps *caps, *templ_caps;

  GST_OBJECT_LOCK (sink);
  if (sink->handle == NULL) {
    GST_OBJECT_UNLOCK (sink);
    GST_DEBUG_OBJECT (sink, "device not open, using template caps");
    return NULL;                /* base class will get template caps for us */
  }

  if (sink->cached_caps) {
    if (filter) {
      caps = gst_caps_intersect_full (filter, sink->cached_caps,
          GST_CAPS_INTERSECT_FIRST);
      GST_OBJECT_UNLOCK (sink);
      GST_LOG_OBJECT (sink, "Returning cached caps %" GST_PTR_FORMAT " with "
          "filter %" GST_PTR_FORMAT " applied: %" GST_PTR_FORMAT,
          sink->cached_caps, filter, caps);
      return caps;
    } else {
      caps = gst_caps_ref (sink->cached_caps);
      GST_OBJECT_UNLOCK (sink);
      GST_LOG_OBJECT (sink, "Returning cached caps %" GST_PTR_FORMAT, caps);
      return caps;
    }
  }

  element_class = GST_ELEMENT_GET_CLASS (sink);
  pad_template = gst_element_class_get_pad_template (element_class, "sink");
  if (pad_template == NULL) {
    GST_OBJECT_UNLOCK (sink);
    g_assert_not_reached ();
    return NULL;
  }

  templ_caps = gst_pad_template_get_caps (pad_template);
  caps = gst_alsa_probe_supported_formats (GST_OBJECT (sink), sink->device,
      sink->handle, templ_caps);
  gst_caps_unref (templ_caps);

  if (caps) {
    sink->cached_caps = gst_caps_ref (caps);
  }

  GST_OBJECT_UNLOCK (sink);

  GST_INFO_OBJECT (sink, "returning caps %" GST_PTR_FORMAT, caps);

  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    return intersection;
  } else {
    return caps;
  }
}

static gboolean
gst_alsasink_acceptcaps (GstAlsaSink * alsa, GstCaps * caps)
{
  GstPad *pad = GST_BASE_SINK (alsa)->sinkpad;
  GstCaps *pad_caps;
  GstStructure *st;
  gboolean ret = FALSE;
  GstAudioRingBufferSpec spec = { 0 };

  pad_caps = gst_pad_query_caps (pad, caps);
  if (!pad_caps || gst_caps_is_empty (pad_caps)) {
    if (pad_caps)
      gst_caps_unref (pad_caps);
    ret = FALSE;
    goto done;
  }
  gst_caps_unref (pad_caps);

  /* If we've not got fixed caps, creating a stream might fail, so let's just
   * return from here with default acceptcaps behaviour */
  if (!gst_caps_is_fixed (caps))
    goto done;

  /* parse helper expects this set, so avoid nasty warning
   * will be set properly later on anyway  */
  spec.latency_time = GST_SECOND;
  if (!gst_audio_ring_buffer_parse_caps (&spec, caps))
    goto done;

  /* Make sure input is framed (one frame per buffer) and can be payloaded */
  switch (spec.type) {
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_AC3:
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_EAC3:
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_DTS:
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_MPEG:
    {
      gboolean framed = FALSE, parsed = FALSE;
      st = gst_caps_get_structure (caps, 0);

      gst_structure_get_boolean (st, "framed", &framed);
      gst_structure_get_boolean (st, "parsed", &parsed);
      if ((!framed && !parsed) || gst_audio_iec61937_frame_size (&spec) <= 0)
        goto done;
    }
    default:{
    }
  }
  ret = TRUE;

done:
  gst_caps_replace (&spec.caps, NULL);
  return ret;
}

static gboolean
gst_alsasink_query (GstBaseSink * sink, GstQuery * query)
{
  GstAlsaSink *alsa = GST_ALSA_SINK (sink);
  gboolean ret;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ACCEPT_CAPS:
    {
      GstCaps *caps;

      gst_query_parse_accept_caps (query, &caps);
      ret = gst_alsasink_acceptcaps (alsa, caps);
      gst_query_set_accept_caps_result (query, ret);
      ret = TRUE;
      break;
    }
    default:
      ret = GST_BASE_SINK_CLASS (parent_class)->query (sink, query);
      break;
  }
  return ret;
}

static int
set_hwparams (GstAlsaSink * alsa)
{
  guint rrate;
  gint err = 0;
  snd_pcm_hw_params_t *params, *params_copy;

  snd_pcm_hw_params_malloc (&params);
  snd_pcm_hw_params_malloc (&params_copy);

  GST_DEBUG_OBJECT (alsa, "Negotiating to %d channels @ %d Hz (format = %s) "
      "SPDIF (%d)", alsa->channels, alsa->rate,
      snd_pcm_format_name (alsa->format), alsa->iec958);

  /* choose all parameters */
  CHECK (snd_pcm_hw_params_any (alsa->handle, params), no_config);
  /* set the interleaved read/write format */
  CHECK (snd_pcm_hw_params_set_access (alsa->handle, params, alsa->access),
      wrong_access);
  /* set the sample format */
  if (alsa->iec958) {
    /* Try to use big endian first else fallback to le and swap bytes */
    if (snd_pcm_hw_params_set_format (alsa->handle, params, alsa->format) < 0) {
      alsa->format = SND_PCM_FORMAT_S16_LE;
      alsa->need_swap = TRUE;
      GST_DEBUG_OBJECT (alsa, "falling back to little endian with swapping");
    } else {
      alsa->need_swap = FALSE;
    }
  }
  CHECK (snd_pcm_hw_params_set_format (alsa->handle, params, alsa->format),
      no_sample_format);
  /* set the count of channels */
  CHECK (snd_pcm_hw_params_set_channels (alsa->handle, params, alsa->channels),
      no_channels);
  /* set the stream rate */
  rrate = alsa->rate;
  CHECK (snd_pcm_hw_params_set_rate_near (alsa->handle, params, &rrate, NULL),
      no_rate);
#ifndef GST_DISABLE_GST_DEBUG
  /* get and dump some limits */
  {
    guint min, max;

    snd_pcm_hw_params_get_buffer_time_min (params, &min, NULL);
    snd_pcm_hw_params_get_buffer_time_max (params, &max, NULL);

    GST_DEBUG_OBJECT (alsa, "buffer time %u, min %u, max %u",
        alsa->buffer_time, min, max);

    snd_pcm_hw_params_get_period_time_min (params, &min, NULL);
    snd_pcm_hw_params_get_period_time_max (params, &max, NULL);

    GST_DEBUG_OBJECT (alsa, "period time %u, min %u, max %u",
        alsa->period_time, min, max);

    snd_pcm_hw_params_get_periods_min (params, &min, NULL);
    snd_pcm_hw_params_get_periods_max (params, &max, NULL);

    GST_DEBUG_OBJECT (alsa, "periods min %u, max %u", min, max);
  }
#endif
  /* Keep a copy of initial params struct that can be used later */
  snd_pcm_hw_params_copy (params_copy, params);
  if (!alsa->iec958) {
    /* Following pulseaudio's approach in
     * https://gitlab.freedesktop.org/pulseaudio/pulseaudio/-/commit/557c4295107dc7374c850b0bd5331dd35e8fdd0f
     * we'll try various configuration to set the period time and buffer time as some
     * driver can be picky on the order of the calls.
     */
    if (alsa->buffer_time != -1 && alsa->period_time != -1) {
      if (((err = snd_pcm_hw_params_set_period_time_near (alsa->handle,
                      params, &alsa->period_time, NULL)) >= 0)
          && ((err =
                  snd_pcm_hw_params_set_buffer_time_near (alsa->handle,
                      params, &alsa->buffer_time, NULL)) >= 0)) {
        GST_DEBUG_OBJECT (alsa, "period time %u buffer time %u set correctly",
            alsa->period_time, alsa->buffer_time);
        goto success;
      }
      /* Try the new order with previous params struct as current one might
         have partial settings from the order that was tried unsuccessfully */
      snd_pcm_hw_params_copy (params, params_copy);
      if (((err = snd_pcm_hw_params_set_buffer_time_near (alsa->handle,
                      params, &alsa->buffer_time, NULL)) >= 0)
          && ((err =
                  snd_pcm_hw_params_set_period_time_near (alsa->handle,
                      params, &alsa->period_time, NULL)) >= 0)) {
        GST_DEBUG_OBJECT (alsa, "buffer time %u period time %u set correctly",
            alsa->buffer_time, alsa->period_time);
        goto success;
      }
    }
    /* now try to configure the period time and buffer time exclusively
     * if both fail  we fall back to the defaults */
    if (alsa->period_time != -1) {
      snd_pcm_hw_params_copy (params, params_copy);
      /* set the period time */
      if ((err =
              snd_pcm_hw_params_set_period_time_near (alsa->handle, params,
                  &alsa->period_time, NULL)) < 0) {
        GST_DEBUG_OBJECT (alsa, "Unable to set period time %i for playback: %s",
            alsa->period_time, snd_strerror (err));
      } else {
        GST_DEBUG_OBJECT (alsa, "period time %u set correctly",
            alsa->period_time);
        goto success;
      }
    }
    if (alsa->buffer_time != -1) {
      snd_pcm_hw_params_copy (params, params_copy);
      /* set the buffer time */
      if ((err =
              snd_pcm_hw_params_set_buffer_time_near (alsa->handle, params,
                  &alsa->buffer_time, NULL)) < 0) {
        GST_DEBUG_OBJECT (alsa, "Unable to set buffer time %i for playback: %s",
            alsa->buffer_time, snd_strerror (err));
      } else {
        GST_DEBUG_OBJECT (alsa, "buffer time %u set correctly",
            alsa->buffer_time);
        goto success;
      }
    }
  } else {
    /* Set buffer size and period size manually for SPDIF */
    snd_pcm_uframes_t buffer_size = SPDIF_BUFFER_SIZE;
    snd_pcm_uframes_t period_size = SPDIF_PERIOD_SIZE;

    CHECK (snd_pcm_hw_params_set_buffer_size_near (alsa->handle, params,
            &buffer_size), buffer_size);
    CHECK (snd_pcm_hw_params_set_period_size_near (alsa->handle, params,
            &period_size, NULL), period_size);
    goto success;
  }
  /* Set nothing if all above failed */
  snd_pcm_hw_params_copy (params, params_copy);
  GST_DEBUG_OBJECT (alsa, "Not setting period time and buffer time");

success:
  /* write the parameters to device */
  CHECK (snd_pcm_hw_params (alsa->handle, params), set_hw_params);
  /* now get the configured values */
  CHECK (snd_pcm_hw_params_get_buffer_size (params, &alsa->buffer_size),
      buffer_size);
  CHECK (snd_pcm_hw_params_get_period_size (params, &alsa->period_size,
          NULL), period_size);

  GST_DEBUG_OBJECT (alsa, "buffer size %lu, period size %lu", alsa->buffer_size,
      alsa->period_size);

  /* Check if hardware supports pause */
  alsa->hw_support_pause = snd_pcm_hw_params_can_pause (params);
  GST_DEBUG_OBJECT (alsa, "Hw support pause: %s",
      alsa->hw_support_pause ? "yes" : "no");

  goto exit;
  /* ERRORS */
no_config:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Broken configuration for playback: no configurations available: %s",
            snd_strerror (err)));
    goto exit;
  }
wrong_access:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Access type not available for playback: %s", snd_strerror (err)));
    goto exit;
  }
no_sample_format:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Sample format not available for playback: %s", snd_strerror (err)));
    goto exit;
  }
no_channels:
  {
    gchar *msg = NULL;

    if ((alsa->channels) == 1)
      msg = g_strdup (_("Could not open device for playback in mono mode."));
    if ((alsa->channels) == 2)
      msg = g_strdup (_("Could not open device for playback in stereo mode."));
    if ((alsa->channels) > 2)
      msg =
          g_strdup_printf (_
          ("Could not open device for playback in %d-channel mode."),
          alsa->channels);
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, ("%s", msg),
        ("%s", snd_strerror (err)));
    g_free (msg);
    goto exit;
  }
no_rate:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Rate %iHz not available for playback: %s",
            alsa->rate, snd_strerror (err)));
    goto exit;
  }
buffer_size:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to get buffer size for playback: %s", snd_strerror (err)));
    goto exit;
  }
period_size:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to get period size for playback: %s", snd_strerror (err)));
    goto exit;
  }
set_hw_params:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to set hw params for playback: %s", snd_strerror (err)));
  }
exit:
  {
    snd_pcm_hw_params_free (params);
    snd_pcm_hw_params_free (params_copy);
    return err;
  }
}

static int
set_swparams (GstAlsaSink * alsa)
{
  int err;
  snd_pcm_sw_params_t *params;

  snd_pcm_sw_params_malloc (&params);

  /* get the current swparams */
  CHECK (snd_pcm_sw_params_current (alsa->handle, params), no_config);
  /* start the transfer when the buffer is almost full: */
  /* (buffer_size / avail_min) * avail_min */
  CHECK (snd_pcm_sw_params_set_start_threshold (alsa->handle, params,
          (alsa->buffer_size / alsa->period_size) * alsa->period_size),
      start_threshold);

  /* allow the transfer when at least period_size samples can be processed */
  CHECK (snd_pcm_sw_params_set_avail_min (alsa->handle, params,
          alsa->period_size), set_avail);

#if GST_CHECK_ALSA_VERSION(1,0,16)
  /* snd_pcm_sw_params_set_xfer_align() is deprecated, alignment is always 1 */
#else
  /* align all transfers to 1 sample */
  CHECK (snd_pcm_sw_params_set_xfer_align (alsa->handle, params, 1), set_align);
#endif

  /* write the parameters to the playback device */
  CHECK (snd_pcm_sw_params (alsa->handle, params), set_sw_params);

  snd_pcm_sw_params_free (params);
  return 0;

  /* ERRORS */
no_config:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to determine current swparams for playback: %s",
            snd_strerror (err)));
    snd_pcm_sw_params_free (params);
    return err;
  }
start_threshold:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to set start threshold mode for playback: %s",
            snd_strerror (err)));
    snd_pcm_sw_params_free (params);
    return err;
  }
set_avail:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to set avail min for playback: %s", snd_strerror (err)));
    snd_pcm_sw_params_free (params);
    return err;
  }
#if !GST_CHECK_ALSA_VERSION(1,0,16)
set_align:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to set transfer align for playback: %s", snd_strerror (err)));
    snd_pcm_sw_params_free (params);
    return err;
  }
#endif
set_sw_params:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to set sw params for playback: %s", snd_strerror (err)));
    snd_pcm_sw_params_free (params);
    return err;
  }
}

static gboolean
alsasink_parse_spec (GstAlsaSink * alsa, GstAudioRingBufferSpec * spec)
{
  /* Initialize our boolean */
  alsa->iec958 = FALSE;

  switch (spec->type) {
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_RAW:
      switch (GST_AUDIO_INFO_FORMAT (&spec->info)) {
        case GST_AUDIO_FORMAT_U8:
          alsa->format = SND_PCM_FORMAT_U8;
          break;
        case GST_AUDIO_FORMAT_S8:
          alsa->format = SND_PCM_FORMAT_S8;
          break;
        case GST_AUDIO_FORMAT_S16LE:
          alsa->format = SND_PCM_FORMAT_S16_LE;
          break;
        case GST_AUDIO_FORMAT_S16BE:
          alsa->format = SND_PCM_FORMAT_S16_BE;
          break;
        case GST_AUDIO_FORMAT_U16LE:
          alsa->format = SND_PCM_FORMAT_U16_LE;
          break;
        case GST_AUDIO_FORMAT_U16BE:
          alsa->format = SND_PCM_FORMAT_U16_BE;
          break;
        case GST_AUDIO_FORMAT_S24_32LE:
          alsa->format = SND_PCM_FORMAT_S24_LE;
          break;
        case GST_AUDIO_FORMAT_S24_32BE:
          alsa->format = SND_PCM_FORMAT_S24_BE;
          break;
        case GST_AUDIO_FORMAT_U24_32LE:
          alsa->format = SND_PCM_FORMAT_U24_LE;
          break;
        case GST_AUDIO_FORMAT_U24_32BE:
          alsa->format = SND_PCM_FORMAT_U24_BE;
          break;
        case GST_AUDIO_FORMAT_S32LE:
          alsa->format = SND_PCM_FORMAT_S32_LE;
          break;
        case GST_AUDIO_FORMAT_S32BE:
          alsa->format = SND_PCM_FORMAT_S32_BE;
          break;
        case GST_AUDIO_FORMAT_U32LE:
          alsa->format = SND_PCM_FORMAT_U32_LE;
          break;
        case GST_AUDIO_FORMAT_U32BE:
          alsa->format = SND_PCM_FORMAT_U32_BE;
          break;
        case GST_AUDIO_FORMAT_S24LE:
          alsa->format = SND_PCM_FORMAT_S24_3LE;
          break;
        case GST_AUDIO_FORMAT_S24BE:
          alsa->format = SND_PCM_FORMAT_S24_3BE;
          break;
        case GST_AUDIO_FORMAT_U24LE:
          alsa->format = SND_PCM_FORMAT_U24_3LE;
          break;
        case GST_AUDIO_FORMAT_U24BE:
          alsa->format = SND_PCM_FORMAT_U24_3BE;
          break;
        case GST_AUDIO_FORMAT_S20LE:
          alsa->format = SND_PCM_FORMAT_S20_3LE;
          break;
        case GST_AUDIO_FORMAT_S20BE:
          alsa->format = SND_PCM_FORMAT_S20_3BE;
          break;
        case GST_AUDIO_FORMAT_U20LE:
          alsa->format = SND_PCM_FORMAT_U20_3LE;
          break;
        case GST_AUDIO_FORMAT_U20BE:
          alsa->format = SND_PCM_FORMAT_U20_3BE;
          break;
        case GST_AUDIO_FORMAT_S18LE:
          alsa->format = SND_PCM_FORMAT_S18_3LE;
          break;
        case GST_AUDIO_FORMAT_S18BE:
          alsa->format = SND_PCM_FORMAT_S18_3BE;
          break;
        case GST_AUDIO_FORMAT_U18LE:
          alsa->format = SND_PCM_FORMAT_U18_3LE;
          break;
        case GST_AUDIO_FORMAT_U18BE:
          alsa->format = SND_PCM_FORMAT_U18_3BE;
          break;
        case GST_AUDIO_FORMAT_F32LE:
          alsa->format = SND_PCM_FORMAT_FLOAT_LE;
          break;
        case GST_AUDIO_FORMAT_F32BE:
          alsa->format = SND_PCM_FORMAT_FLOAT_BE;
          break;
        case GST_AUDIO_FORMAT_F64LE:
          alsa->format = SND_PCM_FORMAT_FLOAT64_LE;
          break;
        case GST_AUDIO_FORMAT_F64BE:
          alsa->format = SND_PCM_FORMAT_FLOAT64_BE;
          break;
        default:
          goto error;
      }
      break;
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_DSD:
      switch (GST_AUDIO_RING_BUFFER_SPEC_DSD_FORMAT (spec)) {
        case GST_DSD_FORMAT_U8:
          alsa->format = SND_PCM_FORMAT_DSD_U8;
          break;
        case GST_DSD_FORMAT_U16LE:
          alsa->format = SND_PCM_FORMAT_DSD_U16_LE;
          break;
        case GST_DSD_FORMAT_U16BE:
          alsa->format = SND_PCM_FORMAT_DSD_U16_BE;
          break;
        case GST_DSD_FORMAT_U32LE:
          alsa->format = SND_PCM_FORMAT_DSD_U32_LE;
          break;
        case GST_DSD_FORMAT_U32BE:
          alsa->format = SND_PCM_FORMAT_DSD_U32_BE;
          break;
        default:
          goto error;
      }
      break;
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_A_LAW:
      alsa->format = SND_PCM_FORMAT_A_LAW;
      break;
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_MU_LAW:
      alsa->format = SND_PCM_FORMAT_MU_LAW;
      break;
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_AC3:
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_EAC3:
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_DTS:
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_MPEG:
      alsa->format = SND_PCM_FORMAT_S16_BE;
      alsa->iec958 = TRUE;
      break;
    default:
      goto error;

  }
  alsa->rate = GST_AUDIO_INFO_RATE (&spec->info);
  alsa->channels = GST_AUDIO_INFO_CHANNELS (&spec->info);
  alsa->buffer_time = spec->buffer_time;
  alsa->period_time = spec->latency_time;
  alsa->access = SND_PCM_ACCESS_RW_INTERLEAVED;

  if ((spec->type == GST_AUDIO_RING_BUFFER_FORMAT_TYPE_RAW ||
          spec->type == GST_AUDIO_RING_BUFFER_FORMAT_TYPE_DSD) &&
      alsa->channels < 9)
    gst_audio_ring_buffer_set_channel_positions (GST_AUDIO_BASE_SINK
        (alsa)->ringbuffer, alsa_position[alsa->channels - 1]);

  return TRUE;

  /* ERRORS */
error:
  {
    return FALSE;
  }
}

static gboolean
gst_alsasink_open (GstAudioSink * asink)
{
  GstAlsaSink *alsa;
  gint err;

  alsa = GST_ALSA_SINK (asink);

  /* open in non-blocking mode, we'll use snd_pcm_wait() for space to become
   * available. */
  CHECK (snd_pcm_open (&alsa->handle, alsa->device, SND_PCM_STREAM_PLAYBACK,
          SND_PCM_NONBLOCK), open_error);
  GST_LOG_OBJECT (alsa, "Opened device %s", alsa->device);

  return TRUE;

  /* ERRORS */
open_error:
  {
    if (err == -EBUSY) {
      GST_ELEMENT_ERROR (alsa, RESOURCE, BUSY,
          (_("Could not open audio device for playback. "
                  "Device is being used by another application.")),
          ("Device '%s' is busy", alsa->device));
    } else {
      GST_ELEMENT_ERROR (alsa, RESOURCE, OPEN_WRITE,
          (_("Could not open audio device for playback.")),
          ("Playback open error on device '%s': %s", alsa->device,
              snd_strerror (err)));
    }
    return FALSE;
  }
}

static gboolean
gst_alsasink_prepare (GstAudioSink * asink, GstAudioRingBufferSpec * spec)
{
  GstAlsaSink *alsa;
  gint err;

  alsa = GST_ALSA_SINK (asink);

  if (alsa->iec958) {
    snd_pcm_close (alsa->handle);
    alsa->handle = gst_alsa_open_iec958_pcm (GST_OBJECT (alsa), alsa->device);
    if (G_UNLIKELY (!alsa->handle)) {
      goto no_iec958;
    }
  }

  if (!alsasink_parse_spec (alsa, spec))
    goto spec_parse;

  CHECK (set_hwparams (alsa), hw_params_failed);
  CHECK (set_swparams (alsa), sw_params_failed);

  alsa->bpf = GST_AUDIO_INFO_BPF (&spec->info);
  spec->segsize = alsa->period_size * alsa->bpf;
  spec->segtotal = alsa->buffer_size / alsa->period_size;

  {
    snd_output_t *out_buf = NULL;
    char *msg = NULL;

    snd_output_buffer_open (&out_buf);
    snd_pcm_dump_hw_setup (alsa->handle, out_buf);
    snd_output_buffer_string (out_buf, &msg);
    GST_DEBUG_OBJECT (alsa, "Hardware setup: \n%s", msg);
    snd_output_close (out_buf);
    snd_output_buffer_open (&out_buf);
    snd_pcm_dump_sw_setup (alsa->handle, out_buf);
    snd_output_buffer_string (out_buf, &msg);
    GST_DEBUG_OBJECT (alsa, "Software setup: \n%s", msg);
    snd_output_close (out_buf);
  }

#ifdef SND_CHMAP_API_VERSION
  alsa_detect_channels_mapping (GST_OBJECT (alsa), alsa->handle, spec,
      alsa->channels, GST_AUDIO_BASE_SINK (alsa)->ringbuffer);
#endif /* SND_CHMAP_API_VERSION */

  return TRUE;

  /* ERRORS */
no_iec958:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, OPEN_WRITE, (NULL),
        ("Could not open IEC958 (SPDIF) device for playback"));
    return FALSE;
  }
spec_parse:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Error parsing spec"));
    return FALSE;
  }
hw_params_failed:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Setting of hwparams failed: %s", snd_strerror (err)));
    return FALSE;
  }
sw_params_failed:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Setting of swparams failed: %s", snd_strerror (err)));
    return FALSE;
  }
}

static gboolean
gst_alsasink_unprepare (GstAudioSink * asink)
{
  GstAlsaSink *alsa;

  alsa = GST_ALSA_SINK (asink);

  snd_pcm_drop (alsa->handle);
  snd_pcm_hw_free (alsa->handle);

  return TRUE;
}

static gboolean
gst_alsasink_close (GstAudioSink * asink)
{
  GstAlsaSink *alsa = GST_ALSA_SINK (asink);

  GST_OBJECT_LOCK (asink);
  if (alsa->handle) {
    snd_pcm_close (alsa->handle);
    alsa->handle = NULL;
  }
  gst_caps_replace (&alsa->cached_caps, NULL);
  GST_OBJECT_UNLOCK (asink);

  return TRUE;
}


/*
 *   Underrun and suspend recovery
 */
static gint
xrun_recovery (GstAlsaSink * alsa, snd_pcm_t * handle, gint err)
{
  GST_WARNING_OBJECT (alsa, "xrun recovery %d: %s", err, g_strerror (-err));

  if (err == -EPIPE) {          /* under-run */
    err = snd_pcm_prepare (handle);
    if (err < 0)
      GST_WARNING_OBJECT (alsa,
          "Can't recover from underrun, prepare failed: %s",
          snd_strerror (err));
    gst_audio_base_sink_report_device_failure (GST_AUDIO_BASE_SINK (alsa));
    return 0;
  } else if (err == -ESTRPIPE) {
    while ((err = snd_pcm_resume (handle)) == -EAGAIN)
      g_usleep (100);           /* wait until the suspend flag is released */

    if (err < 0) {
      err = snd_pcm_prepare (handle);
      if (err < 0)
        GST_WARNING_OBJECT (alsa,
            "Can't recover from suspend, prepare failed: %s",
            snd_strerror (err));
    }
    if (err == 0)
      gst_audio_base_sink_report_device_failure (GST_AUDIO_BASE_SINK (alsa));
    return 0;
  }
  return err;
}

static gint
gst_alsasink_write (GstAudioSink * asink, gpointer data, guint length)
{
  GstAlsaSink *alsa;
  gint err;
  gint cptr;
  guint8 *ptr = data;

  alsa = GST_ALSA_SINK (asink);

  if (alsa->iec958 && alsa->need_swap) {
    guint i;
    guint16 *ptr_tmp = (guint16 *) ptr;

    GST_DEBUG_OBJECT (asink, "swapping bytes");
    for (i = 0; i < length / 2; i++) {
      ptr_tmp[i] = GUINT16_SWAP_LE_BE (ptr_tmp[i]);
    }
  }

  GST_LOG_OBJECT (asink, "received audio samples buffer of %u bytes", length);

  cptr = length / alsa->bpf;

  GST_ALSA_SINK_LOCK (asink);
  while (cptr > 0) {
    /* start by doing a blocking wait for free space. Set the timeout
     * to 4 times the period time */
    err = snd_pcm_wait (alsa->handle, (4 * alsa->period_time / 1000));
    if (err < 0) {
      GST_DEBUG_OBJECT (asink, "wait error, %d", err);
    } else {
      GST_DELAY_SINK_LOCK (asink);
      err = snd_pcm_writei (alsa->handle, ptr, cptr);
      GST_DELAY_SINK_UNLOCK (asink);
    }

    if (err < 0) {
      GST_DEBUG_OBJECT (asink, "Write error: %s (%d)", snd_strerror (err), err);
      if (err == -EAGAIN) {
        /* will continue out of the if/else group */
      } else if (err == -ENODEV) {
        goto device_disappeared;
      } else if (xrun_recovery (alsa, alsa->handle, err) < 0) {
        goto write_error;
      }

      /* Unlock so that _reset() can run and break an otherwise infinit loop
       * here */
      GST_ALSA_SINK_UNLOCK (asink);
      g_thread_yield ();
      GST_ALSA_SINK_LOCK (asink);
      continue;
    } else if (err == 0 && alsa->hw_support_pause) {
      /* We might be already paused, if so, just bail */
      if (snd_pcm_state (alsa->handle) == SND_PCM_STATE_PAUSED)
        break;
    }

    GST_DEBUG_OBJECT (asink, "written %d frames out of %d", err, cptr);
    ptr += snd_pcm_frames_to_bytes (alsa->handle, err);
    cptr -= err;
  }
  GST_ALSA_SINK_UNLOCK (asink);

  return length - (cptr * alsa->bpf);

write_error:
  {
    GST_ALSA_SINK_UNLOCK (asink);
    return length;              /* skip one period */
  }
device_disappeared:
  {
    GST_ELEMENT_ERROR (asink, RESOURCE, WRITE,
        (_("Error outputting to audio device. "
                "The device has been disconnected.")), (NULL));
    goto write_error;
  }
}

static guint
gst_alsasink_delay (GstAudioSink * asink)
{
  GstAlsaSink *alsa;
  snd_pcm_sframes_t delay;
  int res = 0;

  alsa = GST_ALSA_SINK (asink);

  GST_DELAY_SINK_LOCK (asink);
  if (alsa->is_paused == TRUE) {
    delay = alsa->pos_in_buffer;
    alsa->is_paused = FALSE;
    alsa->after_paused = TRUE;
  } else {
    if (alsa->after_paused == TRUE) {
      delay = alsa->pos_in_buffer;
      alsa->after_paused = FALSE;
    } else {
      res = snd_pcm_delay (alsa->handle, &delay);
    }
  }
  GST_DELAY_SINK_UNLOCK (asink);
  if (G_UNLIKELY (res < 0)) {
    /* on errors, report 0 delay */
    GST_DEBUG_OBJECT (alsa, "snd_pcm_delay returned %d", res);
    delay = 0;
  }
  if (G_UNLIKELY (delay < 0)) {
    /* make sure we never return a negative delay */
    GST_WARNING_OBJECT (alsa, "snd_pcm_delay returned negative delay");
    delay = 0;
  }

  return delay;
}

static void
gst_alsasink_pause (GstAudioSink * asink)
{
  GstAlsaSink *alsa;
  gint err;
  snd_pcm_sframes_t delay;

  alsa = GST_ALSA_SINK (asink);

  if (alsa->hw_support_pause == TRUE) {
    GST_ALSA_SINK_LOCK (asink);
    snd_pcm_delay (alsa->handle, &delay);
    alsa->pos_in_buffer = delay;
    CHECK (snd_pcm_pause (alsa->handle, 1), pause_error);
    GST_DEBUG_OBJECT (alsa, "pause done");
    alsa->is_paused = TRUE;
    GST_ALSA_SINK_UNLOCK (asink);
  } else {
    gst_alsasink_stop (asink);
  }

  return;

pause_error:
  {
    GST_ERROR_OBJECT (alsa, "alsa-pause: pcm pause error: %s",
        snd_strerror (err));
    GST_ALSA_SINK_UNLOCK (asink);
    gst_alsasink_stop (asink);
    return;
  }
}

static void
gst_alsasink_resume (GstAudioSink * asink)
{
  GstAlsaSink *alsa;
  gint err;

  alsa = GST_ALSA_SINK (asink);

  if (alsa->hw_support_pause == TRUE) {
    GST_ALSA_SINK_LOCK (asink);
    CHECK (snd_pcm_pause (alsa->handle, 0), resume_error);
    GST_DEBUG_OBJECT (alsa, "resume done");
    GST_ALSA_SINK_UNLOCK (asink);
  }

  return;

resume_error:
  {
    GST_ERROR_OBJECT (alsa, "alsa-resume: pcm resume error: %s",
        snd_strerror (err));
    GST_ALSA_SINK_UNLOCK (asink);
    return;
  }
}

static void
gst_alsasink_stop (GstAudioSink * asink)
{
  GstAlsaSink *alsa;
  gint err;

  alsa = GST_ALSA_SINK (asink);

  GST_ALSA_SINK_LOCK (asink);
  GST_DEBUG_OBJECT (alsa, "drop");
  CHECK (snd_pcm_drop (alsa->handle), drop_error);
  GST_DEBUG_OBJECT (alsa, "prepare");
  CHECK (snd_pcm_prepare (alsa->handle), prepare_error);
  GST_DEBUG_OBJECT (alsa, "stop done");
  GST_ALSA_SINK_UNLOCK (asink);

  return;

  /* ERRORS */
drop_error:
  {
    GST_ERROR_OBJECT (alsa, "alsa-stop: pcm drop error: %s",
        snd_strerror (err));
    GST_ALSA_SINK_UNLOCK (asink);
    return;
  }
prepare_error:
  {
    GST_ERROR_OBJECT (alsa, "alsa-stop: pcm prepare error: %s",
        snd_strerror (err));
    GST_ALSA_SINK_UNLOCK (asink);
    return;
  }
}

static GstBuffer *
gst_alsasink_payload (GstAudioBaseSink * sink, GstBuffer * buf)
{
  GstAlsaSink *alsa;

  alsa = GST_ALSA_SINK (sink);

  if (alsa->iec958) {
    GstBuffer *out;
    gint framesize;
    GstMapInfo iinfo, oinfo;

    framesize = gst_audio_iec61937_frame_size (&sink->ringbuffer->spec);
    if (framesize <= 0)
      return NULL;

    out = gst_buffer_new_and_alloc (framesize);

    gst_buffer_map (buf, &iinfo, GST_MAP_READ);
    gst_buffer_map (out, &oinfo, GST_MAP_WRITE);

    if (!gst_audio_iec61937_payload (iinfo.data, iinfo.size,
            oinfo.data, oinfo.size, &sink->ringbuffer->spec, G_BIG_ENDIAN)) {
      gst_buffer_unmap (buf, &iinfo);
      gst_buffer_unmap (out, &oinfo);
      gst_buffer_unref (out);
      return NULL;
    }

    gst_buffer_unmap (buf, &iinfo);
    gst_buffer_unmap (out, &oinfo);

    gst_buffer_copy_into (out, buf, GST_BUFFER_COPY_METADATA, 0, -1);
    return out;
  }

  return gst_buffer_ref (buf);
}
