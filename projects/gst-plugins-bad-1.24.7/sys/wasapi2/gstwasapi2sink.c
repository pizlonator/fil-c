/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
 * Copyright (C) 2013 Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2018 Centricular Ltd.
 *   Author: Nirbheek Chauhan <nirbheek@centricular.com>
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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
 * SECTION:element-wasapi2sink
 * @title: wasapi2sink
 *
 * Provides audio playback using the Windows Audio Session API available with
 * Windows 10.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v audiotestsrc ! wasapi2sink
 * ]| Generate audio test buffers and render to the default audio device.
 *
 * |[
 * gst-launch-1.0 -v audiotestsink samplesperbuffer=160 ! wasapi2sink low-latency=true
 * ]| Same as above, but with the minimum possible latency
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstwasapi2sink.h"
#include "gstwasapi2util.h"
#include "gstwasapi2ringbuffer.h"

GST_DEBUG_CATEGORY_STATIC (gst_wasapi2_sink_debug);
#define GST_CAT_DEFAULT gst_wasapi2_sink_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_WASAPI2_STATIC_CAPS));

#define DEFAULT_LOW_LATENCY   FALSE
#define DEFAULT_MUTE          FALSE
#define DEFAULT_VOLUME        1.0

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_LOW_LATENCY,
  PROP_MUTE,
  PROP_VOLUME,
  PROP_DISPATCHER,
};

struct _GstWasapi2Sink
{
  GstAudioBaseSink parent;

  /* properties */
  gchar *device_id;
  gboolean low_latency;
  gboolean mute;
  gdouble volume;
  gpointer dispatcher;

  gboolean mute_changed;
  gboolean volume_changed;
};

static void gst_wasapi2_sink_finalize (GObject * object);
static void gst_wasapi2_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_wasapi2_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstStateChangeReturn gst_wasapi2_sink_change_state (GstElement *
    element, GstStateChange transition);

static GstCaps *gst_wasapi2_sink_get_caps (GstBaseSink * bsink,
    GstCaps * filter);
static GstAudioRingBuffer *gst_wasapi2_sink_create_ringbuffer (GstAudioBaseSink
    * sink);

static void gst_wasapi2_sink_set_mute (GstWasapi2Sink * self, gboolean mute);
static gboolean gst_wasapi2_sink_get_mute (GstWasapi2Sink * self);
static void gst_wasapi2_sink_set_volume (GstWasapi2Sink * self, gdouble volume);
static gdouble gst_wasapi2_sink_get_volume (GstWasapi2Sink * self);

#define gst_wasapi2_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWasapi2Sink, gst_wasapi2_sink,
    GST_TYPE_AUDIO_BASE_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_STREAM_VOLUME, NULL));

static void
gst_wasapi2_sink_class_init (GstWasapi2SinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS (klass);
  GstAudioBaseSinkClass *audiobasesink_class =
      GST_AUDIO_BASE_SINK_CLASS (klass);

  gobject_class->finalize = gst_wasapi2_sink_finalize;
  gobject_class->set_property = gst_wasapi2_sink_set_property;
  gobject_class->get_property = gst_wasapi2_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "Audio device ID as provided by "
          "Windows.Devices.Enumeration.DeviceInformation.Id",
          NULL, GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LOW_LATENCY,
      g_param_spec_boolean ("low-latency", "Low latency",
          "Optimize all settings for lowest latency. Always safe to enable.",
          DEFAULT_LOW_LATENCY, GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MUTE,
      g_param_spec_boolean ("mute", "Mute", "Mute state of this stream",
          DEFAULT_MUTE, GST_PARAM_MUTABLE_PLAYING | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VOLUME,
      g_param_spec_double ("volume", "Volume", "Volume of this stream",
          0.0, 1.0, DEFAULT_VOLUME,
          GST_PARAM_MUTABLE_PLAYING | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  /**
   * GstWasapi2Sink:dispatcher:
   *
   * ICoreDispatcher COM object used for activating device from UI thread.
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class, PROP_DISPATCHER,
      g_param_spec_pointer ("dispatcher", "Dispatcher",
          "ICoreDispatcher COM object to use. In order for application to ask "
          "permission of audio device, device activation should be running "
          "on UI thread via ICoreDispatcher. This element will increase "
          "the reference count of given ICoreDispatcher and release it after "
          "use. Therefore, caller does not need to consider additional "
          "reference count management",
          GST_PARAM_MUTABLE_READY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_set_static_metadata (element_class, "Wasapi2Sink",
      "Sink/Audio/Hardware",
      "Stream audio to an audio capture device through WASAPI",
      "Nirbheek Chauhan <nirbheek@centricular.com>, "
      "Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>, "
      "Seungha Yang <seungha@centricular.com>");

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_wasapi2_sink_change_state);

  basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_wasapi2_sink_get_caps);

  audiobasesink_class->create_ringbuffer =
      GST_DEBUG_FUNCPTR (gst_wasapi2_sink_create_ringbuffer);

  GST_DEBUG_CATEGORY_INIT (gst_wasapi2_sink_debug, "wasapi2sink",
      0, "Windows audio session API sink");
}

static void
gst_wasapi2_sink_init (GstWasapi2Sink * self)
{
  self->low_latency = DEFAULT_LOW_LATENCY;
  self->mute = DEFAULT_MUTE;
  self->volume = DEFAULT_VOLUME;
}

static void
gst_wasapi2_sink_finalize (GObject * object)
{
  GstWasapi2Sink *self = GST_WASAPI2_SINK (object);

  g_free (self->device_id);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_wasapi2_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWasapi2Sink *self = GST_WASAPI2_SINK (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_free (self->device_id);
      self->device_id = g_value_dup_string (value);
      break;
    case PROP_LOW_LATENCY:
      self->low_latency = g_value_get_boolean (value);
      break;
    case PROP_MUTE:
      gst_wasapi2_sink_set_mute (self, g_value_get_boolean (value));
      break;
    case PROP_VOLUME:
      gst_wasapi2_sink_set_volume (self, g_value_get_double (value));
      break;
    case PROP_DISPATCHER:
      self->dispatcher = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wasapi2_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWasapi2Sink *self = GST_WASAPI2_SINK (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, self->device_id);
      break;
    case PROP_LOW_LATENCY:
      g_value_set_boolean (value, self->low_latency);
      break;
    case PROP_MUTE:
      g_value_set_boolean (value, gst_wasapi2_sink_get_mute (self));
      break;
    case PROP_VOLUME:
      g_value_set_double (value, gst_wasapi2_sink_get_volume (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_wasapi2_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstWasapi2Sink *self = GST_WASAPI2_SINK (element);
  GstAudioBaseSink *asink = GST_AUDIO_BASE_SINK_CAST (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* If we have pending volume/mute values to set, do here */
      GST_OBJECT_LOCK (self);
      if (asink->ringbuffer) {
        GstWasapi2RingBuffer *ringbuffer =
            GST_WASAPI2_RING_BUFFER (asink->ringbuffer);

        if (self->volume_changed) {
          gst_wasapi2_ring_buffer_set_volume (ringbuffer, self->volume);
          self->volume_changed = FALSE;
        }

        if (self->mute_changed) {
          gst_wasapi2_ring_buffer_set_mute (ringbuffer, self->mute);
          self->mute_changed = FALSE;
        }
      }
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static GstCaps *
gst_wasapi2_sink_get_caps (GstBaseSink * bsink, GstCaps * filter)
{
  GstAudioBaseSink *asink = GST_AUDIO_BASE_SINK_CAST (bsink);
  GstCaps *caps = NULL;

  GST_OBJECT_LOCK (bsink);
  if (asink->ringbuffer) {
    GstWasapi2RingBuffer *ringbuffer =
        GST_WASAPI2_RING_BUFFER (asink->ringbuffer);

    gst_object_ref (ringbuffer);
    GST_OBJECT_UNLOCK (bsink);

    /* Get caps might be able to block if device is not activated yet */
    caps = gst_wasapi2_ring_buffer_get_caps (ringbuffer);
    gst_object_unref (ringbuffer);
  } else {
    GST_OBJECT_UNLOCK (bsink);
  }

  if (!caps)
    caps = gst_pad_get_pad_template_caps (bsink->sinkpad);

  if (filter) {
    GstCaps *filtered =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = filtered;
  }

  GST_DEBUG_OBJECT (bsink, "returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstAudioRingBuffer *
gst_wasapi2_sink_create_ringbuffer (GstAudioBaseSink * sink)
{
  GstWasapi2Sink *self = GST_WASAPI2_SINK (sink);
  GstAudioRingBuffer *ringbuffer;
  gchar *name;

  name = g_strdup_printf ("%s-ringbuffer", GST_OBJECT_NAME (sink));

  ringbuffer =
      gst_wasapi2_ring_buffer_new (GST_WASAPI2_CLIENT_DEVICE_CLASS_RENDER,
      self->low_latency, self->device_id, self->dispatcher, name, 0);

  g_free (name);

  return ringbuffer;
}

static void
gst_wasapi2_sink_set_mute (GstWasapi2Sink * self, gboolean mute)
{
  GstAudioBaseSink *bsink = GST_AUDIO_BASE_SINK_CAST (self);
  HRESULT hr;

  GST_OBJECT_LOCK (self);

  self->mute = mute;
  self->mute_changed = TRUE;

  if (bsink->ringbuffer) {
    GstWasapi2RingBuffer *ringbuffer =
        GST_WASAPI2_RING_BUFFER (bsink->ringbuffer);

    hr = gst_wasapi2_ring_buffer_set_mute (ringbuffer, mute);

    if (FAILED (hr)) {
      GST_INFO_OBJECT (self, "Couldn't set mute");
    } else {
      self->mute_changed = FALSE;
    }
  }

  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_wasapi2_sink_get_mute (GstWasapi2Sink * self)
{
  GstAudioBaseSink *bsink = GST_AUDIO_BASE_SINK_CAST (self);
  gboolean mute;
  HRESULT hr;

  GST_OBJECT_LOCK (self);

  mute = self->mute;

  if (bsink->ringbuffer) {
    GstWasapi2RingBuffer *ringbuffer =
        GST_WASAPI2_RING_BUFFER (bsink->ringbuffer);

    hr = gst_wasapi2_ring_buffer_get_mute (ringbuffer, &mute);

    if (FAILED (hr)) {
      GST_INFO_OBJECT (self, "Couldn't get mute");
    } else {
      self->mute = mute;
    }
  }

  GST_OBJECT_UNLOCK (self);

  return mute;
}

static void
gst_wasapi2_sink_set_volume (GstWasapi2Sink * self, gdouble volume)
{
  GstAudioBaseSink *bsink = GST_AUDIO_BASE_SINK_CAST (self);
  HRESULT hr;

  GST_OBJECT_LOCK (self);

  self->volume = volume;
  /* clip volume value */
  self->volume = MAX (0.0, self->volume);
  self->volume = MIN (1.0, self->volume);
  self->volume_changed = TRUE;

  if (bsink->ringbuffer) {
    GstWasapi2RingBuffer *ringbuffer =
        GST_WASAPI2_RING_BUFFER (bsink->ringbuffer);

    hr = gst_wasapi2_ring_buffer_set_volume (ringbuffer, (gfloat) self->volume);

    if (FAILED (hr)) {
      GST_INFO_OBJECT (self, "Couldn't set volume");
    } else {
      self->volume_changed = FALSE;
    }
  }

  GST_OBJECT_UNLOCK (self);
}

static gdouble
gst_wasapi2_sink_get_volume (GstWasapi2Sink * self)
{
  GstAudioBaseSink *bsink = GST_AUDIO_BASE_SINK_CAST (self);
  gfloat volume;
  HRESULT hr;

  GST_OBJECT_LOCK (self);

  volume = (gfloat) self->volume;

  if (bsink->ringbuffer) {
    GstWasapi2RingBuffer *ringbuffer =
        GST_WASAPI2_RING_BUFFER (bsink->ringbuffer);

    hr = gst_wasapi2_ring_buffer_get_volume (ringbuffer, &volume);

    if (FAILED (hr)) {
      GST_INFO_OBJECT (self, "Couldn't set volume");
    } else {
      self->volume = volume;
    }
  }

  GST_OBJECT_UNLOCK (self);

  volume = MAX (0.0, volume);
  volume = MIN (1.0, volume);

  return volume;
}
