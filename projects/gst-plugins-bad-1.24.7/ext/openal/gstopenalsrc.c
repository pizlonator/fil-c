/*
 * GStreamer
 *
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Victor Lin <bornstub@gmail.com>
 * Copyright (C) 2013 Juan Manuel Borges Caño <juanmabcmail@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * Copyright (C) 2013 Juan Manuel Borges Caño <juanmabcmail@gmail.com>
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
 * SECTION:element-openalsrc
 * @title: openalsrc
 * @see_also: openalsink
 * @short_description: capture raw audio samples through OpenAL
 *
 * This element captures raw audio samples through OpenAL.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v openalsrc ! audioconvert ! wavenc ! filesink location=stream.wav
 * ]| * will capture sound through OpenAL and encode it to a wav file.
 * |[
 * gst-launch-1.0 openalsrc ! "audio/x-raw,format=S16LE,rate=44100" ! audioconvert ! volume volume=0.25 ! openalsink
 * ]| will capture and play audio through OpenAL.
 *
 */

/*
 * DEV:
 * To get better timing/delay information you may also be interested in this:
 *  http://kcat.strangesoft.net/openal-extensions/SOFT_source_latency.txt
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include <gst/gsterror.h>

GST_DEBUG_CATEGORY_EXTERN (openal_debug);
#define GST_CAT_DEFAULT openal_debug

#include "gstopenalelements.h"
#include "gstopenalsrc.h"

static void gst_openal_src_dispose (GObject * object);
static void gst_openal_src_finalize (GObject * object);
static void gst_openal_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_openal_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstCaps *gst_openal_src_getcaps (GstBaseSrc * basesrc, GstCaps * filter);
static gboolean gst_openal_src_open (GstAudioSrc * audiosrc);
static gboolean gst_openal_src_prepare (GstAudioSrc * audiosrc,
    GstAudioRingBufferSpec * spec);
static gboolean gst_openal_src_unprepare (GstAudioSrc * audiosrc);
static gboolean gst_openal_src_close (GstAudioSrc * audiosrc);
static guint gst_openal_src_read (GstAudioSrc * audiosrc, gpointer data,
    guint length, GstClockTime * timestamp);
static guint gst_openal_src_delay (GstAudioSrc * audiosrc);
static void gst_openal_src_reset (GstAudioSrc * audiosrc);

#define OPENAL_DEFAULT_DEVICE_NAME NULL
#define OPENAL_DEFAULT_DEVICE NULL

#define OPENAL_MIN_RATE 8000
#define OPENAL_MAX_RATE 192000

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_DEVICE_NAME
};

static GstStaticPadTemplate openalsrc_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        /* These caps do not work on my card */
        // "audio/x-adpcm, " "layout = (string) ima, "
        // "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ]; "
        // "audio/x-alaw, " "rate = (int) [ 1, MAX ], "
        // "channels = (int) 1; "
        // "audio/x-mulaw, " "rate = (int) [ 1, MAX ], "
        // "channels = (int) 1; "
        // "audio/x-raw, " "format = (string) " GST_AUDIO_NE (F64) ", "
        // "rate = (int) [ 1, MAX ], " "channels = (int) 1; "
        // "audio/x-raw, " "format = (string) " GST_AUDIO_NE (F32) ", "
        // "rate = (int) [ 1, MAX ], " "channels = (int) 1; "
        "audio/x-raw, " "format = (string) " GST_AUDIO_NE (S16) ", "
        "rate = (int) [ 1, MAX ], " "channels = (int) 1; "
        /* These caps work wrongly on my card */
        // "audio/x-raw, " "format = (string) " GST_AUDIO_NE (U16) ", "
        // "rate = (int) [ 1, MAX ], " "channels = (int) 1; "
        // "audio/x-raw, " "format = (string) " G_STRINGIFY (S8) ", "
        // "rate = (int) [ 1, MAX ], " "channels = (int) 1"));
        "audio/x-raw, " "format = (string) " G_STRINGIFY (U8) ", "
        "rate = (int) [ 1, MAX ], " "channels = (int) 1")
    );

G_DEFINE_TYPE (GstOpenalSrc, gst_openal_src, GST_TYPE_AUDIO_SRC);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (openalsrc, "openalsrc",
    GST_RANK_SECONDARY, GST_TYPE_OPENAL_SRC, openal_element_init (plugin));

static void
gst_openal_src_dispose (GObject * object)
{
  GstOpenalSrc *openalsrc = GST_OPENAL_SRC (object);

  if (openalsrc->probed_caps)
    gst_caps_unref (openalsrc->probed_caps);
  openalsrc->probed_caps = NULL;

  G_OBJECT_CLASS (gst_openal_src_parent_class)->dispose (object);
}

static void
gst_openal_src_class_init (GstOpenalSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;
  GstAudioSrcClass *gstaudiosrc_class = (GstAudioSrcClass *) (klass);

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_openal_src_dispose);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_openal_src_finalize);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_openal_src_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_openal_src_get_property);

  gst_openal_src_parent_class = g_type_class_peek_parent (klass);

  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_openal_src_getcaps);

  gstaudiosrc_class->open = GST_DEBUG_FUNCPTR (gst_openal_src_open);
  gstaudiosrc_class->prepare = GST_DEBUG_FUNCPTR (gst_openal_src_prepare);
  gstaudiosrc_class->unprepare = GST_DEBUG_FUNCPTR (gst_openal_src_unprepare);
  gstaudiosrc_class->close = GST_DEBUG_FUNCPTR (gst_openal_src_close);
  gstaudiosrc_class->read = GST_DEBUG_FUNCPTR (gst_openal_src_read);
  gstaudiosrc_class->delay = GST_DEBUG_FUNCPTR (gst_openal_src_delay);
  gstaudiosrc_class->reset = GST_DEBUG_FUNCPTR (gst_openal_src_reset);

  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "ALCdevice",
          "User device, default device if NULL", OPENAL_DEFAULT_DEVICE,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device name",
          "Human-readable name of the device", OPENAL_DEFAULT_DEVICE_NAME,
          G_PARAM_READABLE));

  gst_element_class_set_static_metadata (gstelement_class,
      "OpenAL Audio Source", "Source/Audio", "Input audio through OpenAL",
      "Juan Manuel Borges Caño <juanmabcmail@gmail.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &openalsrc_factory);
}

static void
gst_openal_src_init (GstOpenalSrc * openalsrc)
{
  GST_DEBUG_OBJECT (openalsrc, "initializing");

  openalsrc->default_device_name = g_strdup (OPENAL_DEFAULT_DEVICE_NAME);
  openalsrc->default_device = OPENAL_DEFAULT_DEVICE;
  openalsrc->device = NULL;

  openalsrc->buffer_length = 0;

  openalsrc->probed_caps = NULL;
}

static void
gst_openal_src_finalize (GObject * object)
{
  GstOpenalSrc *openalsrc = GST_OPENAL_SRC (object);

  g_free (openalsrc->default_device_name);
  g_free (openalsrc->default_device);

  G_OBJECT_CLASS (gst_openal_src_parent_class)->finalize (object);
}

static void
gst_openal_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOpenalSrc *openalsrc = GST_OPENAL_SRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      openalsrc->default_device = g_value_dup_string (value);
      break;
    case PROP_DEVICE_NAME:
      openalsrc->default_device_name = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_openal_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOpenalSrc *openalsrc = GST_OPENAL_SRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, openalsrc->default_device);
      break;
    case PROP_DEVICE_NAME:
      g_value_set_string (value, openalsrc->default_device_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_openal_helper_probe_caps (ALCcontext * context)
{
  GstStructure *structure;
  GstCaps *caps;
//  ALCcontext *old;

//  old = pushContext(context);

  caps = gst_caps_new_empty ();

  if (alIsExtensionPresent ("AL_EXT_DOUBLE")) {
    structure =
        gst_structure_new ("audio/x-raw", "format", G_TYPE_STRING,
        GST_AUDIO_NE (F64), "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE,
        OPENAL_MAX_RATE, "channels", G_TYPE_INT, 1, NULL);
    gst_caps_append_structure (caps, structure);
  }

  if (alIsExtensionPresent ("AL_EXT_FLOAT32")) {
    structure =
        gst_structure_new ("audio/x-raw", "format", G_TYPE_STRING,
        GST_AUDIO_NE (F32), "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE,
        OPENAL_MAX_RATE, "channels", G_TYPE_INT, 1, NULL);
    gst_caps_append_structure (caps, structure);
  }

  structure =
      gst_structure_new ("audio/x-raw", "format", G_TYPE_STRING,
      GST_AUDIO_NE (S16), "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE,
      OPENAL_MAX_RATE, "channels", G_TYPE_INT, 1, NULL);
  gst_caps_append_structure (caps, structure);

  structure =
      gst_structure_new ("audio/x-raw", "format", G_TYPE_STRING,
      G_STRINGIFY (U8), "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE,
      OPENAL_MAX_RATE, "channels", G_TYPE_INT, 1, NULL);
  gst_caps_append_structure (caps, structure);

  if (alIsExtensionPresent ("AL_EXT_IMA4")) {
    structure =
        gst_structure_new ("audio/x-adpcm", "layout", G_TYPE_STRING, "ima",
        "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE, OPENAL_MAX_RATE,
        "channels", G_TYPE_INT, 1, NULL);
    gst_caps_append_structure (caps, structure);
  }

  if (alIsExtensionPresent ("AL_EXT_ALAW")) {
    structure =
        gst_structure_new ("audio/x-alaw", "rate", GST_TYPE_INT_RANGE,
        OPENAL_MIN_RATE, OPENAL_MAX_RATE, "channels", G_TYPE_INT, 1, NULL);
    gst_caps_append_structure (caps, structure);
  }

  if (alIsExtensionPresent ("AL_EXT_MULAW")) {
    structure =
        gst_structure_new ("audio/x-mulaw", "rate", GST_TYPE_INT_RANGE,
        OPENAL_MIN_RATE, OPENAL_MAX_RATE, "channels", G_TYPE_INT, 1, NULL);
    gst_caps_append_structure (caps, structure);
  }
//  popContext(old, context);

  return caps;
}

static GstCaps *
gst_openal_src_getcaps (GstBaseSrc * basesrc, GstCaps * filter)
{
  GstOpenalSrc *openalsrc = GST_OPENAL_SRC (basesrc);
  GstCaps *caps;
  ALCdevice *device;

  device = alcOpenDevice (NULL);

  if (device == NULL) {
    GstPad *pad = GST_BASE_SRC_PAD (basesrc);
    GstCaps *tcaps = gst_pad_get_pad_template_caps (pad);

    GST_ELEMENT_WARNING (openalsrc, RESOURCE, OPEN_WRITE,
        ("Could not open temporary device."), GST_ALC_ERROR (device));
    caps = gst_caps_copy (tcaps);
    gst_caps_unref (tcaps);
  } else if (openalsrc->probed_caps)
    caps = gst_caps_copy (openalsrc->probed_caps);
  else {
    ALCcontext *context = alcCreateContext (device, NULL);
    if (context) {
      caps = gst_openal_helper_probe_caps (context);
      alcDestroyContext (context);
    } else {
      GST_ELEMENT_WARNING (openalsrc, RESOURCE, FAILED,
          ("Could not create temporary context."), GST_ALC_ERROR (device));
      caps = NULL;
    }

    if (caps && !gst_caps_is_empty (caps))
      openalsrc->probed_caps = gst_caps_copy (caps);
  }

  if (device != NULL) {
    if (alcCloseDevice (device) == ALC_FALSE) {
      GST_ELEMENT_WARNING (openalsrc, RESOURCE, CLOSE,
          ("Could not close temporary device."), GST_ALC_ERROR (device));
    }
  }

  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    return intersection;
  } else {
    return caps;
  }
}


static gboolean
gst_openal_src_open (GstAudioSrc * audiosrc)
{
  return TRUE;
}

static void
gst_openal_src_parse_spec (GstOpenalSrc * openalsrc,
    const GstAudioRingBufferSpec * spec)
{
  ALuint format = AL_NONE;

  GST_DEBUG_OBJECT (openalsrc,
      "looking up format for type %d, gst-format %d, and %d channels",
      spec->type, GST_AUDIO_INFO_FORMAT (&spec->info),
      GST_AUDIO_INFO_CHANNELS (&spec->info));

  switch (spec->type) {
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_RAW:
      switch (GST_AUDIO_INFO_FORMAT (&spec->info)) {
        case GST_AUDIO_FORMAT_U8:
          switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
            case 1:
              format = AL_FORMAT_MONO8;
              break;
            default:
              break;
          }
          break;

        case GST_AUDIO_FORMAT_U16:
        case GST_AUDIO_FORMAT_S16:
          switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
            case 1:
              format = AL_FORMAT_MONO16;
              break;
            default:
              break;
          }
          break;

        case GST_AUDIO_FORMAT_F32:
          switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
            case 1:
              format = AL_FORMAT_MONO_FLOAT32;
              break;
            default:
              break;
          }
          break;

        case GST_AUDIO_FORMAT_F64:
          switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
            case 1:
              format = AL_FORMAT_MONO_DOUBLE_EXT;
              break;
            default:
              break;
          }
          break;

        default:
          break;
      }
      break;

    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_IMA_ADPCM:
      switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
        case 1:
          format = AL_FORMAT_MONO_IMA4;
          break;
        default:
          break;
      }
      break;

    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_A_LAW:
      switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
        case 1:
          format = AL_FORMAT_MONO_ALAW_EXT;
          break;
        default:
          break;
      }
      break;

    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_MU_LAW:
      switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
        case 1:
          format = AL_FORMAT_MONO_MULAW;
          break;
        default:
          break;
      }
      break;

    default:
      break;
  }

  openalsrc->bytes_per_sample = GST_AUDIO_INFO_BPS (&spec->info);
  openalsrc->rate = GST_AUDIO_INFO_RATE (&spec->info);
  openalsrc->buffer_length = spec->segsize;
  openalsrc->format = format;
}

static gboolean
gst_openal_src_prepare (GstAudioSrc * audiosrc, GstAudioRingBufferSpec * spec)
{
  GstOpenalSrc *openalsrc = GST_OPENAL_SRC (audiosrc);

  gst_openal_src_parse_spec (openalsrc, spec);
  if (openalsrc->format == AL_NONE) {
    GST_ELEMENT_ERROR (openalsrc, RESOURCE, SETTINGS, (NULL),
        ("Unable to get type %d, format %d, and %d channels", spec->type,
            GST_AUDIO_INFO_FORMAT (&spec->info),
            GST_AUDIO_INFO_CHANNELS (&spec->info)));
    return FALSE;
  }

  openalsrc->device =
      alcCaptureOpenDevice (openalsrc->default_device, openalsrc->rate,
      openalsrc->format, openalsrc->buffer_length);

  if (!openalsrc->device) {
    GST_ELEMENT_ERROR (openalsrc, RESOURCE, OPEN_READ,
        ("Could not open device."), GST_ALC_ERROR (openalsrc->device));
    return FALSE;
  }

  openalsrc->default_device_name =
      g_strdup (alcGetString (openalsrc->device, ALC_DEVICE_SPECIFIER));

  alcCaptureStart (openalsrc->device);

  return TRUE;
}

static gboolean
gst_openal_src_unprepare (GstAudioSrc * audiosrc)
{
  GstOpenalSrc *openalsrc = GST_OPENAL_SRC (audiosrc);

  if (openalsrc->device) {
    alcCaptureStop (openalsrc->device);

    if (alcCaptureCloseDevice (openalsrc->device) == ALC_FALSE) {
      GST_ELEMENT_ERROR (openalsrc, RESOURCE, CLOSE,
          ("Could not close device."), GST_ALC_ERROR (openalsrc->device));
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
gst_openal_src_close (GstAudioSrc * audiosrc)
{
  return TRUE;
}

static guint
gst_openal_src_read (GstAudioSrc * audiosrc, gpointer data, guint length,
    GstClockTime * timestamp)
{
  GstOpenalSrc *openalsrc = GST_OPENAL_SRC (audiosrc);
  gint samples;

  alcGetIntegerv (openalsrc->device, ALC_CAPTURE_SAMPLES, sizeof (samples),
      &samples);

  if (samples * openalsrc->bytes_per_sample > length) {
    samples = length / openalsrc->bytes_per_sample;
  }

  if (samples) {
    GST_DEBUG_OBJECT (openalsrc, "read samples : %d", samples);
    alcCaptureSamples (openalsrc->device, data, samples);
  }

  return samples * openalsrc->bytes_per_sample;
}

static guint
gst_openal_src_delay (GstAudioSrc * audiosrc)
{
  GstOpenalSrc *openalsrc = GST_OPENAL_SRC (audiosrc);
  ALint samples;

  alcGetIntegerv (openalsrc->device, ALC_CAPTURE_SAMPLES, sizeof (samples),
      &samples);

  if (G_UNLIKELY (samples < 0)) {
    /* make sure we never return a negative delay */
    GST_WARNING_OBJECT (openal_debug, "negative delay");
    samples = 0;
  }

  return samples;
}

static void
gst_openal_src_reset (GstAudioSrc * audiosrc)
{
}
