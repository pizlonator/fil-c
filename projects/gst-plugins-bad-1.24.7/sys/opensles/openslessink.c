/* GStreamer
 * Copyright (C) 2012 Fluendo S.A. <support@fluendo.com>
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
 * SECTION:element-openslessink
 * @title: openslessink
 * @see_also: openslessrc
 *
 * This element renders raw audio samples using the OpenSL ES API in Android OS.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v filesrc location=music.ogg ! oggdemux ! vorbisdec ! audioconvert ! audioresample ! opeslessink
 * ]| Play an Ogg/Vorbis file.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "opensles.h"
#include "openslessink.h"

GST_DEBUG_CATEGORY_STATIC (opensles_sink_debug);
#define GST_CAT_DEFAULT opensles_sink_debug

enum
{
  PROP_0,
  PROP_VOLUME,
  PROP_MUTE,
  PROP_STREAM_TYPE,
  PROP_LAST
};

#define DEFAULT_VOLUME 1.0
#define DEFAULT_MUTE   FALSE

#define DEFAULT_STREAM_TYPE GST_OPENSLES_STREAM_TYPE_NONE


/* According to Android's NDK doc the following are the supported rates */
#define RATES "8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000"

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) { " GST_AUDIO_NE (S16) ", " GST_AUDIO_NE (U8) "}, "
        "rate = (int) { " RATES "}, " "channels = (int) [1, 2], "
        "layout = (string) interleaved")
    );

#define _do_init \
  GST_DEBUG_CATEGORY_INIT (opensles_sink_debug, "openslessink", 0, \
      "OpenSLES Sink");
#define parent_class gst_opensles_sink_parent_class
G_DEFINE_TYPE_WITH_CODE (GstOpenSLESSink, gst_opensles_sink,
    GST_TYPE_AUDIO_BASE_SINK, _do_init);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (openslessink, "openslessink",
    GST_RANK_PRIMARY, GST_TYPE_OPENSLES_SINK, opensles_element_init (plugin));

static GstAudioRingBuffer *
gst_opensles_sink_create_ringbuffer (GstAudioBaseSink * base)
{
  GstOpenSLESSink *sink = GST_OPENSLES_SINK (base);
  GstAudioRingBuffer *rb;

  rb = gst_opensles_ringbuffer_new (RB_MODE_SINK_PCM);
  gst_opensles_ringbuffer_set_volume (rb, sink->volume);
  gst_opensles_ringbuffer_set_mute (rb, sink->mute);

  GST_OPENSLES_RING_BUFFER (rb)->stream_type = sink->stream_type;

  return rb;
}

#define AUDIO_OUTPUT_DESC_FORMAT                                              \
    "deviceName: %s deviceConnection: %d deviceScope: %d deviceLocation: %d " \
    "isForTelephony: %d minSampleRate: %d maxSampleRate: %d "                 \
    "isFreqRangeContinuous: %d maxChannels: %d"

#define AUDIO_OUTPUT_DESC_ARGS(aod)                                           \
    (gchar*) (aod)->pDeviceName, (gint) (aod)->deviceConnection,              \
    (gint) (aod)->deviceScope, (gint) (aod)->deviceLocation,                  \
    (gint) (aod)->isForTelephony, (gint) (aod)->minSampleRate,                \
    (gint) (aod)->maxSampleRate, (gint) (aod)->isFreqRangeContinuous,         \
    (gint) (aod)->maxChannels

static gboolean
_opensles_query_capabilities (GstOpenSLESSink * sink)
{
  gboolean res = FALSE;
  SLresult result;
  SLObjectItf engineObject = NULL;
  SLAudioIODeviceCapabilitiesItf audioIODeviceCapabilities;
  SLint32 i, j, numOutputs = MAX_NUMBER_OUTPUT_DEVICES;
  SLuint32 outputDeviceIDs[MAX_NUMBER_OUTPUT_DEVICES];
  SLAudioOutputDescriptor audioOutputDescriptor;

  /* Create and realize engine */
  engineObject = gst_opensles_get_engine ();
  if (!engineObject) {
    GST_ERROR_OBJECT (sink, "Getting engine failed");
    goto beach;
  }

  /* Get the engine interface, which is needed in order to create other objects */
  result = (*engineObject)->GetInterface (engineObject,
      SL_IID_AUDIOIODEVICECAPABILITIES, &audioIODeviceCapabilities);
  if (result == SL_RESULT_FEATURE_UNSUPPORTED) {
    GST_LOG_OBJECT (sink,
        "engine.GetInterface(IODeviceCapabilities) unsupported(0x%08x)",
        (guint32) result);
    goto beach;
  } else if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (sink,
        "engine.GetInterface(IODeviceCapabilities) failed(0x%08x)",
        (guint32) result);
    goto beach;
  }

  /* Query the list of available audio outputs */
  result = (*audioIODeviceCapabilities)->GetAvailableAudioOutputs
      (audioIODeviceCapabilities, &numOutputs, outputDeviceIDs);
  if (result == SL_RESULT_FEATURE_UNSUPPORTED) {
    GST_LOG_OBJECT (sink,
        "IODeviceCapabilities.GetAvailableAudioOutputs unsupported(0x%08x)",
        (guint32) result);
    goto beach;
  } else if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (sink,
        "IODeviceCapabilities.GetAvailableAudioOutputs failed(0x%08x)",
        (guint32) result);
    goto beach;
  }

  GST_DEBUG_OBJECT (sink, "Found %d output devices", (gint32) numOutputs);

  for (i = 0; i < numOutputs; i++) {
    result = (*audioIODeviceCapabilities)->QueryAudioOutputCapabilities
        (audioIODeviceCapabilities, outputDeviceIDs[i], &audioOutputDescriptor);

    if (result == SL_RESULT_FEATURE_UNSUPPORTED) {
      GST_LOG_OBJECT (sink,
          "IODeviceCapabilities.QueryAudioOutputCapabilities unsupported(0x%08x)",
          (guint32) result);
      continue;
    } else if (result != SL_RESULT_SUCCESS) {
      GST_ERROR_OBJECT (sink,
          "IODeviceCapabilities.QueryAudioOutputCapabilities failed(0x%08x)",
          (guint32) result);
      continue;
    }

    GST_DEBUG_OBJECT (sink, "  ID: %08x " AUDIO_OUTPUT_DESC_FORMAT,
        (guint) outputDeviceIDs[i],
        AUDIO_OUTPUT_DESC_ARGS (&audioOutputDescriptor));
    GST_DEBUG_OBJECT (sink, "  Found %d supported sample rated",
        audioOutputDescriptor.numOfSamplingRatesSupported);

    for (j = 0; j < audioOutputDescriptor.numOfSamplingRatesSupported; j++) {
      GST_DEBUG_OBJECT (sink, "    %d Hz",
          (gint) audioOutputDescriptor.samplingRatesSupported[j]);
    }
  }

  res = TRUE;
beach:
  /* Destroy the engine object */
  if (engineObject) {
    gst_opensles_release_engine (engineObject);
  }

  return res;
}

static void
gst_opensles_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOpenSLESSink *sink = GST_OPENSLES_SINK (object);
  GstAudioRingBuffer *rb = GST_AUDIO_BASE_SINK (sink)->ringbuffer;

  switch (prop_id) {
    case PROP_VOLUME:
      sink->volume = g_value_get_double (value);
      if (rb && GST_IS_OPENSLES_RING_BUFFER (rb)) {
        gst_opensles_ringbuffer_set_volume (rb, sink->volume);
      }
      break;
    case PROP_MUTE:
      sink->mute = g_value_get_boolean (value);
      if (rb && GST_IS_OPENSLES_RING_BUFFER (rb)) {
        gst_opensles_ringbuffer_set_mute (rb, sink->mute);
      }
      break;
    case PROP_STREAM_TYPE:
      sink->stream_type = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_opensles_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOpenSLESSink *sink = GST_OPENSLES_SINK (object);
  switch (prop_id) {
    case PROP_VOLUME:
      g_value_set_double (value, sink->volume);
      break;
    case PROP_MUTE:
      g_value_set_boolean (value, sink->mute);
      break;
    case PROP_STREAM_TYPE:
      g_value_set_enum (value, sink->stream_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_opensles_sink_class_init (GstOpenSLESSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstAudioBaseSinkClass *gstbaseaudiosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbaseaudiosink_class = (GstAudioBaseSinkClass *) klass;

  gobject_class->set_property = gst_opensles_sink_set_property;
  gobject_class->get_property = gst_opensles_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_VOLUME,
      g_param_spec_double ("volume", "Volume", "Volume of this stream",
          0, 1.0, 1.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MUTE,
      g_param_spec_boolean ("mute", "Mute", "Mute state of this stream",
          DEFAULT_MUTE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_STREAM_TYPE,
      g_param_spec_enum ("stream-type", "Stream type",
          "Stream type that this stream should be tagged with",
          GST_TYPE_OPENSLES_STREAM_TYPE, DEFAULT_STREAM_TYPE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gst_element_class_set_static_metadata (gstelement_class, "OpenSL ES Sink",
      "Sink/Audio",
      "Output sound using the OpenSL ES APIs",
      "Josep Torra <support@fluendo.com>");

  gstbaseaudiosink_class->create_ringbuffer =
      GST_DEBUG_FUNCPTR (gst_opensles_sink_create_ringbuffer);
}

static void
gst_opensles_sink_init (GstOpenSLESSink * sink)
{
  sink->stream_type = DEFAULT_STREAM_TYPE;
  sink->volume = DEFAULT_VOLUME;
  sink->mute = DEFAULT_MUTE;

  _opensles_query_capabilities (sink);

  gst_audio_base_sink_set_provide_clock (GST_AUDIO_BASE_SINK (sink), TRUE);
}
