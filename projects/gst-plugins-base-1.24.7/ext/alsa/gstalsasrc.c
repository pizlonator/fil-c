/* GStreamer
 * Copyright (C) 2005 Wim Taymans <wim@fluendo.com>
 *
 * gstalsasrc.c:
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
 * SECTION:element-alsasrc
 * @title: alsasrc
 * @see_also: alsasink
 *
 * This element reads data from an audio card using the ALSA API.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v alsasrc ! queue ! audioconvert ! vorbisenc ! oggmux ! filesink location=alsasrc.ogg
 * ]|
 *  Record from a sound card using ALSA and encode to Ogg/Vorbis.
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
#include "gstalsasrc.h"

#include <glib/gi18n-lib.h>

#ifndef ESTRPIPE
#define ESTRPIPE EPIPE
#endif

#define DEFAULT_PROP_DEVICE		  "default"
#define DEFAULT_PROP_DEVICE_NAME	  ""
#define DEFAULT_PROP_CARD_NAME	          ""
#define DEFAULT_PROP_USE_DRIVER_TIMESTAMP TRUE

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_DEVICE_NAME,
  PROP_CARD_NAME,
  PROP_USE_DRIVER_TIMESTAMP,
  PROP_LAST
};

#define gst_alsasrc_parent_class parent_class
G_DEFINE_TYPE (GstAlsaSrc, gst_alsasrc, GST_TYPE_AUDIO_SRC);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (alsasrc, "alsasrc", GST_RANK_PRIMARY,
    GST_TYPE_ALSA_SRC, alsa_element_init (plugin));

static void gst_alsasrc_finalize (GObject * object);
static void gst_alsasrc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_alsasrc_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_alsasrc_change_state (GstElement * element,
    GstStateChange transition);
static GstCaps *gst_alsasrc_getcaps (GstBaseSrc * bsrc, GstCaps * filter);

static gboolean gst_alsasrc_open (GstAudioSrc * asrc);
static gboolean gst_alsasrc_prepare (GstAudioSrc * asrc,
    GstAudioRingBufferSpec * spec);
static gboolean gst_alsasrc_unprepare (GstAudioSrc * asrc);
static gboolean gst_alsasrc_close (GstAudioSrc * asrc);
static guint gst_alsasrc_read
    (GstAudioSrc * asrc, gpointer data, guint length, GstClockTime * timestamp);
static guint gst_alsasrc_delay (GstAudioSrc * asrc);
static void gst_alsasrc_reset (GstAudioSrc * asrc);

/* AlsaSrc signals and args */
enum
{
  LAST_SIGNAL
};

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
# define ALSA_SRC_FACTORY_ENDIANNESS   "LITTLE_ENDIAN, BIG_ENDIAN"
#else
# define ALSA_SRC_FACTORY_ENDIANNESS   "BIG_ENDIAN, LITTLE_ENDIAN"
#endif

static GstStaticPadTemplate alsasrc_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_FORMATS_ALL ", "
        "layout = (string) interleaved, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]")
    );

static void
gst_alsasrc_finalize (GObject * object)
{
  GstAlsaSrc *src = GST_ALSA_SRC (object);

  g_free (src->device);
  g_mutex_clear (&src->alsa_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_alsasrc_class_init (GstAlsaSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstAudioSrcClass *gstaudiosrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstaudiosrc_class = (GstAudioSrcClass *) klass;

  gobject_class->finalize = gst_alsasrc_finalize;
  gobject_class->get_property = gst_alsasrc_get_property;
  gobject_class->set_property = gst_alsasrc_set_property;

  gst_element_class_set_static_metadata (gstelement_class,
      "Audio source (ALSA)", "Source/Audio",
      "Read from a sound card via ALSA", "Wim Taymans <wim@fluendo.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &alsasrc_src_factory);

  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_alsasrc_getcaps);

  gstaudiosrc_class->open = GST_DEBUG_FUNCPTR (gst_alsasrc_open);
  gstaudiosrc_class->prepare = GST_DEBUG_FUNCPTR (gst_alsasrc_prepare);
  gstaudiosrc_class->unprepare = GST_DEBUG_FUNCPTR (gst_alsasrc_unprepare);
  gstaudiosrc_class->close = GST_DEBUG_FUNCPTR (gst_alsasrc_close);
  gstaudiosrc_class->read = GST_DEBUG_FUNCPTR (gst_alsasrc_read);
  gstaudiosrc_class->delay = GST_DEBUG_FUNCPTR (gst_alsasrc_delay);
  gstaudiosrc_class->reset = GST_DEBUG_FUNCPTR (gst_alsasrc_reset);
  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_alsasrc_change_state);

  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "ALSA device, as defined in an asound configuration file",
          DEFAULT_PROP_DEVICE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device name",
          "Human-readable name of the sound device",
          DEFAULT_PROP_DEVICE_NAME, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CARD_NAME,
      g_param_spec_string ("card-name", "Card name",
          "Human-readable name of the sound card",
          DEFAULT_PROP_CARD_NAME, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
          | GST_PARAM_DOC_SHOW_DEFAULT));

  g_object_class_install_property (gobject_class, PROP_USE_DRIVER_TIMESTAMP,
      g_param_spec_boolean ("use-driver-timestamps", "Use driver timestamps",
          "Use driver timestamps or the pipeline clock timestamps",
          DEFAULT_PROP_USE_DRIVER_TIMESTAMP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_alsasrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAlsaSrc *src;

  src = GST_ALSA_SRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_free (src->device);
      src->device = g_value_dup_string (value);
      if (src->device == NULL) {
        src->device = g_strdup (DEFAULT_PROP_DEVICE);
      }
      break;
    case PROP_USE_DRIVER_TIMESTAMP:
      GST_OBJECT_LOCK (src);
      src->use_driver_timestamps = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (src);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_alsasrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAlsaSrc *src;

  src = GST_ALSA_SRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, src->device);
      break;
    case PROP_DEVICE_NAME:
      g_value_take_string (value,
          gst_alsa_find_device_name (GST_OBJECT_CAST (src),
              src->device, src->handle, SND_PCM_STREAM_CAPTURE));
      break;
    case PROP_CARD_NAME:
      g_value_take_string (value,
          gst_alsa_find_card_name (GST_OBJECT_CAST (src),
              src->device, SND_PCM_STREAM_CAPTURE));
      break;
    case PROP_USE_DRIVER_TIMESTAMP:
      GST_OBJECT_LOCK (src);
      g_value_set_boolean (value, src->use_driver_timestamps);
      GST_OBJECT_UNLOCK (src);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_alsasrc_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstAlsaSrc *alsa = GST_ALSA_SRC (element);
  GstClock *clk;

  switch (transition) {
      /* show the compiler that we care */
    case GST_STATE_CHANGE_NULL_TO_READY:
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    case GST_STATE_CHANGE_READY_TO_NULL:
    case GST_STATE_CHANGE_NULL_TO_NULL:
    case GST_STATE_CHANGE_READY_TO_READY:
    case GST_STATE_CHANGE_PAUSED_TO_PAUSED:
    case GST_STATE_CHANGE_PLAYING_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      alsa->driver_timestamps = FALSE;

      clk = gst_element_get_clock (element);
      if (clk != NULL) {
        if (G_OBJECT_TYPE (clk) == GST_TYPE_SYSTEM_CLOCK) {
          gint clocktype;
          g_object_get (clk, "clock-type", &clocktype, NULL);
          if (clocktype == GST_CLOCK_TYPE_MONOTONIC &&
              alsa->use_driver_timestamps) {
            GST_INFO ("Using driver timestamps !");
            alsa->driver_timestamps = TRUE;
          } else {
            GST_INFO ("Not using driver timestamps !");
            alsa->driver_timestamps = FALSE;
          }
        }

        gst_object_unref (clk);
      }
      break;
  }
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  return ret;
}

static void
gst_alsasrc_init (GstAlsaSrc * alsasrc)
{
  GST_DEBUG_OBJECT (alsasrc, "initializing");

  alsasrc->device = g_strdup (DEFAULT_PROP_DEVICE);
  alsasrc->cached_caps = NULL;
  alsasrc->driver_timestamps = FALSE;
  alsasrc->use_driver_timestamps = DEFAULT_PROP_USE_DRIVER_TIMESTAMP;

  g_mutex_init (&alsasrc->alsa_lock);
}

#define CHECK(call, error) \
G_STMT_START {                  \
if ((err = call) < 0)           \
  goto error;                   \
} G_STMT_END;


static GstCaps *
gst_alsasrc_getcaps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstElementClass *element_class;
  GstPadTemplate *pad_template;
  GstAlsaSrc *src;
  GstCaps *caps, *templ_caps;

  src = GST_ALSA_SRC (bsrc);

  if (src->handle == NULL) {
    GST_DEBUG_OBJECT (src, "device not open, using template caps");
    return GST_BASE_SRC_CLASS (parent_class)->get_caps (bsrc, filter);
  }

  if (src->cached_caps) {
    GST_LOG_OBJECT (src, "Returning cached caps");
    if (filter)
      return gst_caps_intersect_full (filter, src->cached_caps,
          GST_CAPS_INTERSECT_FIRST);
    else
      return gst_caps_ref (src->cached_caps);
  }

  element_class = GST_ELEMENT_GET_CLASS (src);
  pad_template = gst_element_class_get_pad_template (element_class, "src");
  g_return_val_if_fail (pad_template != NULL, NULL);

  templ_caps = gst_pad_template_get_caps (pad_template);
  GST_INFO_OBJECT (src, "template caps %" GST_PTR_FORMAT, templ_caps);

  caps = gst_alsa_probe_supported_formats (GST_OBJECT (src),
      src->device, src->handle, templ_caps);
  gst_caps_unref (templ_caps);

  if (caps) {
    src->cached_caps = gst_caps_ref (caps);
  }

  GST_INFO_OBJECT (src, "returning caps %" GST_PTR_FORMAT, caps);

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

static int
set_hwparams (GstAlsaSrc * alsa)
{
  guint rrate;
  gint err = 0;
  snd_pcm_hw_params_t *params, *params_copy;

  snd_pcm_hw_params_malloc (&params);
  snd_pcm_hw_params_malloc (&params_copy);

  /* choose all parameters */
  CHECK (snd_pcm_hw_params_any (alsa->handle, params), no_config);
  /* set the interleaved read/write format */
  CHECK (snd_pcm_hw_params_set_access (alsa->handle, params, alsa->access),
      wrong_access);
  /* set the sample format */
  CHECK (snd_pcm_hw_params_set_format (alsa->handle, params, alsa->format),
      no_sample_format);
  /* set the count of channels */
  CHECK (snd_pcm_hw_params_set_channels (alsa->handle, params, alsa->channels),
      no_channels);
  /* set the stream rate */
  rrate = alsa->rate;
  CHECK (snd_pcm_hw_params_set_rate_near (alsa->handle, params, &rrate, NULL),
      no_rate);
  if (rrate != alsa->rate)
    goto rate_match;

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
  /* Following pulseaudio's approach in
   * https://gitlab.freedesktop.org/pulseaudio/pulseaudio/-/commit/557c4295107dc7374c850b0bd5331dd35e8fdd0f
   * we'll try various configuration to set the buffer time and period time as some
   * driver can be picky on the order of the calls.
   */
  if (alsa->period_time != -1 && alsa->buffer_time != -1) {
    if ((snd_pcm_hw_params_set_period_time_near (alsa->handle, params,
                &alsa->period_time, NULL) >= 0)
        && (snd_pcm_hw_params_set_buffer_time_near (alsa->handle, params,
                &alsa->buffer_time, NULL) >= 0)) {
      GST_DEBUG_OBJECT (alsa, "period time %u buffer time %u set correctly",
          alsa->period_time, alsa->buffer_time);
      goto success;
    }
    /* Try the new order with previous params struct as current one might
       have partial settings from the order that was tried unsuccessfully */
    snd_pcm_hw_params_copy (params, params_copy);
    if ((snd_pcm_hw_params_set_buffer_time_near (alsa->handle, params,
                &alsa->buffer_time, NULL) >= 0)
        && (snd_pcm_hw_params_set_period_time_near (alsa->handle, params,
                &alsa->period_time, NULL) >= 0)) {
      GST_DEBUG_OBJECT (alsa, "buffer time %u period time %u set correctly",
          alsa->buffer_time, alsa->period_time);
      goto success;
    }
  }
  if (alsa->period_time != -1) {
    snd_pcm_hw_params_copy (params, params_copy);
    /* set the period time only */
    if ((snd_pcm_hw_params_set_period_time_near (alsa->handle, params,
                &alsa->period_time, NULL) >= 0)) {
      GST_DEBUG_OBJECT (alsa, "period time %u set correctly",
          alsa->period_time);
      goto success;
    }
  }
  if (alsa->buffer_time != -1) {
    snd_pcm_hw_params_copy (params, params_copy);
    /* set the buffer time only */
    if ((snd_pcm_hw_params_set_buffer_time_near (alsa->handle, params,
                &alsa->buffer_time, NULL) >= 0)) {
      GST_DEBUG_OBJECT (alsa, "buffer time %u set correctly",
          alsa->buffer_time);
      goto success;
    }
  }
  /* Set nothing if all above failed */
  snd_pcm_hw_params_copy (params, params_copy);
  GST_DEBUG_OBJECT (alsa, "Not setting period time and buffer time");

success:
  /* write the parameters to device */
  CHECK (snd_pcm_hw_params (alsa->handle, params), set_hw_params);
  CHECK (snd_pcm_hw_params_get_buffer_size (params, &alsa->buffer_size),
      buffer_size);
  GST_DEBUG_OBJECT (alsa, "buffer size : %lu", alsa->buffer_size);
  CHECK (snd_pcm_hw_params_get_period_size (params, &alsa->period_size,
          NULL), period_size);
  GST_DEBUG_OBJECT (alsa, "period size : %lu", alsa->period_size);

  goto exit;
  /* ERRORS */
no_config:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Broken configuration for recording: no configurations available: %s",
            snd_strerror (err)));
    goto exit;
  }
wrong_access:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Access type not available for recording: %s", snd_strerror (err)));
    goto exit;
  }
no_sample_format:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Sample format not available for recording: %s", snd_strerror (err)));
    goto exit;
  }
no_channels:
  {
    gchar *msg = NULL;

    if ((alsa->channels) == 1)
      msg = g_strdup (_("Could not open device for recording in mono mode."));
    if ((alsa->channels) == 2)
      msg = g_strdup (_("Could not open device for recording in stereo mode."));
    if ((alsa->channels) > 2)
      msg =
          g_strdup_printf (_
          ("Could not open device for recording in %d-channel mode"),
          alsa->channels);
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, ("%s", msg),
        ("%s", snd_strerror (err)));
    g_free (msg);
    goto exit;
  }
no_rate:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Rate %iHz not available for recording: %s",
            alsa->rate, snd_strerror (err)));
    goto exit;
  }
rate_match:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Rate doesn't match (requested %iHz, get %iHz)", alsa->rate, err));
    err = -EINVAL;
    goto exit;
  }
buffer_size:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to get buffer size for recording: %s", snd_strerror (err)));
    goto exit;
  }
period_size:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to get period size for recording: %s", snd_strerror (err)));
    goto exit;
  }
set_hw_params:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to set hw params for recording: %s", snd_strerror (err)));
  }
exit:
  {
    snd_pcm_hw_params_free (params);
    snd_pcm_hw_params_free (params_copy);
    return err;
  }
}

static int
set_swparams (GstAlsaSrc * alsa)
{
  int err;
  snd_pcm_sw_params_t *params;

  snd_pcm_sw_params_malloc (&params);

  /* get the current swparams */
  CHECK (snd_pcm_sw_params_current (alsa->handle, params), no_config);
  /* allow the transfer when at least period_size samples can be processed */
  CHECK (snd_pcm_sw_params_set_avail_min (alsa->handle, params,
          alsa->period_size), set_avail);
  /* start the transfer on first read */
  CHECK (snd_pcm_sw_params_set_start_threshold (alsa->handle, params,
          0), start_threshold);
  /* use monotonic timestamping */
  CHECK (snd_pcm_sw_params_set_tstamp_mode (alsa->handle, params,
          SND_PCM_TSTAMP_MMAP), tstamp_mode);

#if GST_CHECK_ALSA_VERSION(1,0,16)
  /* snd_pcm_sw_params_set_xfer_align() is deprecated, alignment is always 1 */
#else
  /* align all transfers to 1 sample */
  CHECK (snd_pcm_sw_params_set_xfer_align (alsa->handle, params, 1), set_align);
#endif

  /* write the parameters to the recording device */
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
tstamp_mode:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to set tstamp mode for playback: %s", snd_strerror (err)));
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
alsasrc_parse_spec (GstAlsaSrc * alsa, GstAudioRingBufferSpec * spec)
{
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
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_A_LAW:
      alsa->format = SND_PCM_FORMAT_A_LAW;
      break;
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_MU_LAW:
      alsa->format = SND_PCM_FORMAT_MU_LAW;
      break;
    default:
      goto error;

  }
  alsa->rate = GST_AUDIO_INFO_RATE (&spec->info);
  alsa->channels = GST_AUDIO_INFO_CHANNELS (&spec->info);
  alsa->buffer_time = spec->buffer_time;
  alsa->period_time = spec->latency_time;
  alsa->access = SND_PCM_ACCESS_RW_INTERLEAVED;

  if (spec->type == GST_AUDIO_RING_BUFFER_FORMAT_TYPE_RAW && alsa->channels < 9)
    gst_audio_ring_buffer_set_channel_positions (GST_AUDIO_BASE_SRC
        (alsa)->ringbuffer, alsa_position[alsa->channels - 1]);

  return TRUE;

  /* ERRORS */
error:
  {
    return FALSE;
  }
}

static gboolean
gst_alsasrc_open (GstAudioSrc * asrc)
{
  GstAlsaSrc *alsa;
  gint err;

  alsa = GST_ALSA_SRC (asrc);

  CHECK (snd_pcm_open (&alsa->handle, alsa->device, SND_PCM_STREAM_CAPTURE,
          (alsa->driver_timestamps) ? 0 : SND_PCM_NONBLOCK), open_error);

  return TRUE;

  /* ERRORS */
open_error:
  {
    if (err == -EBUSY) {
      GST_ELEMENT_ERROR (alsa, RESOURCE, BUSY,
          (_("Could not open audio device for recording. "
                  "Device is being used by another application.")),
          ("Device '%s' is busy", alsa->device));
    } else {
      GST_ELEMENT_ERROR (alsa, RESOURCE, OPEN_READ,
          (_("Could not open audio device for recording.")),
          ("Recording open error on device '%s': %s", alsa->device,
              snd_strerror (err)));
    }
    return FALSE;
  }
}

static gboolean
gst_alsasrc_prepare (GstAudioSrc * asrc, GstAudioRingBufferSpec * spec)
{
  GstAlsaSrc *alsa;
  gint err;

  alsa = GST_ALSA_SRC (asrc);

  if (!alsasrc_parse_spec (alsa, spec))
    goto spec_parse;

  CHECK (snd_pcm_nonblock (alsa->handle, 0), non_block);

  CHECK (set_hwparams (alsa), hw_params_failed);
  CHECK (set_swparams (alsa), sw_params_failed);
  CHECK (snd_pcm_prepare (alsa->handle), prepare_failed);

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
      alsa->channels, GST_AUDIO_BASE_SRC (alsa)->ringbuffer);
#endif /* SND_CHMAP_API_VERSION */

  return TRUE;

  /* ERRORS */
spec_parse:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Error parsing spec"));
    return FALSE;
  }
non_block:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Could not set device to blocking: %s", snd_strerror (err)));
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
prepare_failed:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Prepare failed: %s", snd_strerror (err)));
    return FALSE;
  }
}

static gboolean
gst_alsasrc_unprepare (GstAudioSrc * asrc)
{
  GstAlsaSrc *alsa;

  alsa = GST_ALSA_SRC (asrc);

  snd_pcm_drop (alsa->handle);
  snd_pcm_hw_free (alsa->handle);
  snd_pcm_nonblock (alsa->handle, 1);

  return TRUE;
}

static gboolean
gst_alsasrc_close (GstAudioSrc * asrc)
{
  GstAlsaSrc *alsa = GST_ALSA_SRC (asrc);

  snd_pcm_close (alsa->handle);
  alsa->handle = NULL;

  gst_caps_replace (&alsa->cached_caps, NULL);

  return TRUE;
}

/*
 *   Underrun and suspend recovery
 */
static gint
xrun_recovery (GstAlsaSrc * alsa, snd_pcm_t * handle, gint err)
{
  GST_WARNING_OBJECT (alsa, "xrun recovery %d: %s", err, g_strerror (-err));

  if (err == -EPIPE) {          /* under-run */
    err = snd_pcm_prepare (handle);
    if (err < 0)
      GST_WARNING_OBJECT (alsa,
          "Can't recover from underrun, prepare failed: %s",
          snd_strerror (err));
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
    return 0;
  }
  return err;
}

static GstClockTime
gst_alsasrc_get_timestamp (GstAlsaSrc * asrc)
{
  snd_pcm_status_t *status;
  snd_htimestamp_t tstamp;
  GstClockTime timestamp;
  snd_pcm_uframes_t avail;
  gint err = -EPIPE;

  if (G_UNLIKELY (!asrc)) {
    GST_ERROR_OBJECT (asrc, "No alsa handle created yet !");
    return GST_CLOCK_TIME_NONE;
  }

  if (G_UNLIKELY (snd_pcm_status_malloc (&status) != 0)) {
    GST_ERROR_OBJECT (asrc, "snd_pcm_status_malloc failed");
    return GST_CLOCK_TIME_NONE;
  }

  if (G_UNLIKELY (snd_pcm_status (asrc->handle, status) != 0)) {
    GST_ERROR_OBJECT (asrc, "snd_pcm_status failed");
    return GST_CLOCK_TIME_NONE;
  }

  /* in case an xrun condition has occurred we need to handle this */
  if (snd_pcm_status_get_state (status) != SND_PCM_STATE_RUNNING) {
    if (xrun_recovery (asrc, asrc->handle, err) < 0) {
      GST_WARNING_OBJECT (asrc, "Could not recover from xrun condition !");
    }
    /* reload the status alsa status object, since recovery made it invalid */
    if (G_UNLIKELY (snd_pcm_status (asrc->handle, status) != 0)) {
      GST_ERROR_OBJECT (asrc, "snd_pcm_status failed");
    }
  }

  /* get high resolution time stamp from driver */
  snd_pcm_status_get_htstamp (status, &tstamp);

  if (tstamp.tv_sec == 0 && tstamp.tv_nsec == 0)
    return GST_CLOCK_TIME_NONE;

  timestamp = GST_TIMESPEC_TO_TIME (tstamp);

  /* max available frames sets the depth of the buffer */
  avail = snd_pcm_status_get_avail (status);

  /* calculate the timestamp of the next sample to be read */
  timestamp -= gst_util_uint64_scale_int (avail, GST_SECOND, asrc->rate);

  /* compensate for the fact that we really need the timestamp of the
   * previously read data segment */
  timestamp -= asrc->period_time * 1000;

  snd_pcm_status_free (status);

  GST_LOG_OBJECT (asrc, "ALSA timestamp : %" GST_TIME_FORMAT
      ", delay %lu", GST_TIME_ARGS (timestamp), avail);

  return timestamp;
}

static guint
gst_alsasrc_read (GstAudioSrc * asrc, gpointer data, guint length,
    GstClockTime * timestamp)
{
  GstAlsaSrc *alsa;
  gint err;
  gint cptr;
  guint8 *ptr = data;

  alsa = GST_ALSA_SRC (asrc);

  cptr = length / alsa->bpf;

  GST_ALSA_SRC_LOCK (asrc);
  while (cptr > 0) {
    if ((err = snd_pcm_readi (alsa->handle, ptr, cptr)) < 0) {
      if (err == -EAGAIN) {
        GST_DEBUG_OBJECT (asrc, "Read error: %s", snd_strerror (err));
        continue;
      } else if (err == -ENODEV) {
        goto device_disappeared;
      } else if (xrun_recovery (alsa, alsa->handle, err) < 0) {
        goto read_error;
      }
      continue;
    }

    ptr += snd_pcm_frames_to_bytes (alsa->handle, err);
    cptr -= err;
  }
  GST_ALSA_SRC_UNLOCK (asrc);

  /* if driver timestamps are enabled we need to return this here */
  if (alsa->driver_timestamps && timestamp)
    *timestamp = gst_alsasrc_get_timestamp (alsa);

  return length - (cptr * alsa->bpf);

read_error:
  {
    GST_ALSA_SRC_UNLOCK (asrc);
    return length;              /* skip one period */
  }
device_disappeared:
  {
    GST_ELEMENT_ERROR (asrc, RESOURCE, READ,
        (_("Error recording from audio device. "
                "The device has been disconnected.")), (NULL));
    GST_ALSA_SRC_UNLOCK (asrc);
    return (guint) - 1;
  }
}

static guint
gst_alsasrc_delay (GstAudioSrc * asrc)
{
  GstAlsaSrc *alsa;
  snd_pcm_sframes_t delay;
  int res;

  alsa = GST_ALSA_SRC (asrc);

  res = snd_pcm_delay (alsa->handle, &delay);
  if (G_UNLIKELY (res < 0)) {
    GST_DEBUG_OBJECT (alsa, "snd_pcm_delay returned %d", res);
    delay = 0;
  }

  return CLAMP (delay, 0, alsa->buffer_size);
}

static void
gst_alsasrc_reset (GstAudioSrc * asrc)
{
  GstAlsaSrc *alsa;
  gint err;

  alsa = GST_ALSA_SRC (asrc);

  GST_ALSA_SRC_LOCK (asrc);
  GST_DEBUG_OBJECT (alsa, "drop");
  CHECK (snd_pcm_drop (alsa->handle), drop_error);
  GST_DEBUG_OBJECT (alsa, "prepare");
  CHECK (snd_pcm_prepare (alsa->handle), prepare_error);
  GST_DEBUG_OBJECT (alsa, "reset done");
  GST_ALSA_SRC_UNLOCK (asrc);

  return;

  /* ERRORS */
drop_error:
  {
    GST_ERROR_OBJECT (alsa, "alsa-reset: pcm drop error: %s",
        snd_strerror (err));
    GST_ALSA_SRC_UNLOCK (asrc);
    return;
  }
prepare_error:
  {
    GST_ERROR_OBJECT (alsa, "alsa-reset: pcm prepare error: %s",
        snd_strerror (err));
    GST_ALSA_SRC_UNLOCK (asrc);
    return;
  }
}
