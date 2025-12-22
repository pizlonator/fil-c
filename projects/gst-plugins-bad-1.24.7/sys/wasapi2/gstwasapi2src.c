/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
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
 * SECTION:element-wasapi2src
 * @title: wasapi2src
 *
 * Provides audio capture from the Windows Audio Session API available with
 * Windows 10.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v wasapi2src ! fakesink
 * ]| Capture from the default audio device and render to fakesink.
 *
 * |[
 * gst-launch-1.0 -v wasapi2src low-latency=true ! fakesink
 * ]| Capture from the default audio device with the minimum possible latency and render to fakesink.
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstwasapi2src.h"
#include "gstwasapi2util.h"
#include "gstwasapi2ringbuffer.h"

GST_DEBUG_CATEGORY_STATIC (gst_wasapi2_src_debug);
#define GST_CAT_DEFAULT gst_wasapi2_src_debug

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_WASAPI2_STATIC_CAPS));

/**
 * GstWasapi2SrcLoopbackMode:
 *
 * Loopback capture mode
 *
 * Since: 1.22
 */
typedef enum
{
  /**
   * GstWasapi2SrcLoopbackMode::default:
   *
   * Default loopback mode
   *
   * Since: 1.22
   */
  GST_WASAPI2_SRC_LOOPBACK_DEFAULT,

  /**
   * GstWasapi2SrcLoopbackMode::include-process-tree:
   *
   * Captures only specified process and its child process
   *
   * Since: 1.22
   */
  GST_WASAPI2_SRC_LOOPBACK_INCLUDE_PROCESS_TREE,

  /**
   * GstWasapi2SrcLoopbackMode::exclude-process-tree:
   *
   * Excludes specified process and its child process
   *
   * Since: 1.22
   */
  GST_WASAPI2_SRC_LOOPBACK_EXCLUDE_PROCESS_TREE,
} GstWasapi2SrcLoopbackMode;

#define GST_TYPE_WASAPI2_SRC_LOOPBACK_MODE (gst_wasapi2_src_loopback_mode_get_type ())
static GType
gst_wasapi2_src_loopback_mode_get_type (void)
{
  static GType loopback_type = 0;
  static const GEnumValue types[] = {
    {GST_WASAPI2_SRC_LOOPBACK_DEFAULT, "Default", "default"},
    {GST_WASAPI2_SRC_LOOPBACK_INCLUDE_PROCESS_TREE,
          "Include process and its child processes",
        "include-process-tree"},
    {GST_WASAPI2_SRC_LOOPBACK_EXCLUDE_PROCESS_TREE,
          "Exclude process and its child processes",
        "exclude-process-tree"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&loopback_type)) {
    GType gtype = g_enum_register_static ("GstWasapi2SrcLoopbackMode", types);
    g_once_init_leave (&loopback_type, gtype);
  }

  return loopback_type;
}

#define DEFAULT_LOW_LATENCY   FALSE
#define DEFAULT_MUTE          FALSE
#define DEFAULT_VOLUME        1.0
#define DEFAULT_LOOPBACK      FALSE
#define DEFAULT_LOOPBACK_MODE GST_WASAPI2_SRC_LOOPBACK_DEFAULT
#define DEFAULT_LOOPBACK_SILENCE_ON_DEVICE_MUTE FALSE

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_LOW_LATENCY,
  PROP_MUTE,
  PROP_VOLUME,
  PROP_DISPATCHER,
  PROP_LOOPBACK,
  PROP_LOOPBACK_MODE,
  PROP_LOOPBACK_TARGET_PID,
  PROP_LOOPBACK_SILENCE_ON_DEVICE_MUTE,
};

struct _GstWasapi2Src
{
  GstAudioBaseSrc parent;

  /* properties */
  gchar *device_id;
  gboolean low_latency;
  gboolean mute;
  gdouble volume;
  gpointer dispatcher;
  gboolean loopback;
  GstWasapi2SrcLoopbackMode loopback_mode;
  guint loopback_pid;
  gboolean loopback_silence_on_device_mute;

  gboolean mute_changed;
  gboolean volume_changed;
};

static void gst_wasapi2_src_finalize (GObject * object);
static void gst_wasapi2_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_wasapi2_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstStateChangeReturn gst_wasapi2_src_change_state (GstElement *
    element, GstStateChange transition);

static GstCaps *gst_wasapi2_src_get_caps (GstBaseSrc * bsrc, GstCaps * filter);
static GstAudioRingBuffer *gst_wasapi2_src_create_ringbuffer (GstAudioBaseSrc *
    src);

static void gst_wasapi2_src_set_mute (GstWasapi2Src * self, gboolean mute);
static gboolean gst_wasapi2_src_get_mute (GstWasapi2Src * self);
static void gst_wasapi2_src_set_volume (GstWasapi2Src * self, gdouble volume);
static gdouble gst_wasapi2_src_get_volume (GstWasapi2Src * self);
static void gst_wasapi2_src_set_silence_on_mute (GstWasapi2Src * self,
    gboolean value);

#define gst_wasapi2_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWasapi2Src, gst_wasapi2_src,
    GST_TYPE_AUDIO_BASE_SRC,
    G_IMPLEMENT_INTERFACE (GST_TYPE_STREAM_VOLUME, NULL));

static void
gst_wasapi2_src_class_init (GstWasapi2SrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);
  GstAudioBaseSrcClass *audiobasesrc_class = GST_AUDIO_BASE_SRC_CLASS (klass);

  gobject_class->finalize = gst_wasapi2_src_finalize;
  gobject_class->set_property = gst_wasapi2_src_set_property;
  gobject_class->get_property = gst_wasapi2_src_get_property;

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
   * GstWasapi2Src:dispatcher:
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

  /**
   * GstWasapi2Src:loopback:
   *
   * Open render device for loopback recording
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_LOOPBACK,
      g_param_spec_boolean ("loopback", "Loopback recording",
          "Open render device for loopback recording", DEFAULT_LOOPBACK,
          GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  if (gst_wasapi2_can_process_loopback ()) {
    /**
     * GstWasapi2Src:loopback-mode:
     *
     * Loopback mode. "target-process-id" must be specified in case of
     * process loopback modes.
     *
     * This feature requires "Windows 10 build 20348"
     *
     * Since: 1.22
     */
    g_object_class_install_property (gobject_class, PROP_LOOPBACK_MODE,
        g_param_spec_enum ("loopback-mode", "Loopback Mode",
            "Loopback mode to use", GST_TYPE_WASAPI2_SRC_LOOPBACK_MODE,
            DEFAULT_LOOPBACK_MODE,
            GST_PARAM_CONDITIONALLY_AVAILABLE | GST_PARAM_MUTABLE_READY |
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
     * GstWasapi2Src:loopback-target-pid:
     *
     * Target process id to be recorded or excluded depending on loopback mode
     *
     * This feature requires "Windows 10 build 20348"
     *
     * Since: 1.22
     */
    g_object_class_install_property (gobject_class, PROP_LOOPBACK_TARGET_PID,
        g_param_spec_uint ("loopback-target-pid", "Loopback Target PID",
            "Process ID to be recorded or excluded for process loopback mode",
            0, G_MAXUINT32, 0,
            GST_PARAM_CONDITIONALLY_AVAILABLE | GST_PARAM_MUTABLE_READY |
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }

  /**
   * GstWasapi2Src:loopback-silence-on-device-mute:
   *
   * When loopback recording, if the device is muted, inject silence in the pipeline
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class,
      PROP_LOOPBACK_SILENCE_ON_DEVICE_MUTE,
      g_param_spec_boolean ("loopback-silence-on-device-mute",
          "Loopback Silence On Device Mute",
          "When loopback recording, if the device is muted, inject silence in the pipeline",
          DEFAULT_LOOPBACK_SILENCE_ON_DEVICE_MUTE,
          GST_PARAM_MUTABLE_PLAYING | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (element_class, &src_template);
  gst_element_class_set_static_metadata (element_class, "Wasapi2Src",
      "Source/Audio/Hardware",
      "Stream audio from an audio capture device through WASAPI",
      "Nirbheek Chauhan <nirbheek@centricular.com>, "
      "Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>, "
      "Seungha Yang <seungha@centricular.com>");

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_wasapi2_src_change_state);

  basesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_wasapi2_src_get_caps);

  audiobasesrc_class->create_ringbuffer =
      GST_DEBUG_FUNCPTR (gst_wasapi2_src_create_ringbuffer);

  GST_DEBUG_CATEGORY_INIT (gst_wasapi2_src_debug, "wasapi2src",
      0, "Windows audio session API source");

  if (gst_wasapi2_can_process_loopback ())
    gst_type_mark_as_plugin_api (GST_TYPE_WASAPI2_SRC_LOOPBACK_MODE, 0);
}

static void
gst_wasapi2_src_init (GstWasapi2Src * self)
{
  self->mute = DEFAULT_MUTE;
  self->volume = DEFAULT_VOLUME;
  self->low_latency = DEFAULT_LOW_LATENCY;
  self->loopback = DEFAULT_LOOPBACK;
  self->loopback_silence_on_device_mute =
      DEFAULT_LOOPBACK_SILENCE_ON_DEVICE_MUTE;
}

static void
gst_wasapi2_src_finalize (GObject * object)
{
  GstWasapi2Src *self = GST_WASAPI2_SRC (object);

  g_free (self->device_id);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_wasapi2_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWasapi2Src *self = GST_WASAPI2_SRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_free (self->device_id);
      self->device_id = g_value_dup_string (value);
      break;
    case PROP_LOW_LATENCY:
      self->low_latency = g_value_get_boolean (value);
      break;
    case PROP_MUTE:
      gst_wasapi2_src_set_mute (self, g_value_get_boolean (value));
      break;
    case PROP_VOLUME:
      gst_wasapi2_src_set_volume (self, g_value_get_double (value));
      break;
    case PROP_DISPATCHER:
      self->dispatcher = g_value_get_pointer (value);
      break;
    case PROP_LOOPBACK:
      self->loopback = g_value_get_boolean (value);
      break;
    case PROP_LOOPBACK_MODE:
      self->loopback_mode = g_value_get_enum (value);
      break;
    case PROP_LOOPBACK_TARGET_PID:
      self->loopback_pid = g_value_get_uint (value);
      break;
    case PROP_LOOPBACK_SILENCE_ON_DEVICE_MUTE:
      gst_wasapi2_src_set_silence_on_mute (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wasapi2_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWasapi2Src *self = GST_WASAPI2_SRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, self->device_id);
      break;
    case PROP_LOW_LATENCY:
      g_value_set_boolean (value, self->low_latency);
      break;
    case PROP_MUTE:
      g_value_set_boolean (value, gst_wasapi2_src_get_mute (self));
      break;
    case PROP_VOLUME:
      g_value_set_double (value, gst_wasapi2_src_get_volume (self));
      break;
    case PROP_LOOPBACK:
      g_value_set_boolean (value, self->loopback);
      break;
    case PROP_LOOPBACK_MODE:
      g_value_set_enum (value, self->loopback_mode);
      break;
    case PROP_LOOPBACK_TARGET_PID:
      g_value_set_uint (value, self->loopback_pid);
      break;
    case PROP_LOOPBACK_SILENCE_ON_DEVICE_MUTE:
      g_value_set_boolean (value, self->loopback_silence_on_device_mute);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_wasapi2_src_change_state (GstElement * element, GstStateChange transition)
{
  GstWasapi2Src *self = GST_WASAPI2_SRC (element);
  GstAudioBaseSrc *asrc = GST_AUDIO_BASE_SRC_CAST (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* If we have pending volume/mute values to set, do here */
      GST_OBJECT_LOCK (self);
      if (asrc->ringbuffer) {
        GstWasapi2RingBuffer *ringbuffer =
            GST_WASAPI2_RING_BUFFER (asrc->ringbuffer);

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
gst_wasapi2_src_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstAudioBaseSrc *asrc = GST_AUDIO_BASE_SRC_CAST (bsrc);
  GstCaps *caps = NULL;

  GST_OBJECT_LOCK (bsrc);
  if (asrc->ringbuffer) {
    GstWasapi2RingBuffer *ringbuffer =
        GST_WASAPI2_RING_BUFFER (asrc->ringbuffer);

    gst_object_ref (ringbuffer);
    GST_OBJECT_UNLOCK (bsrc);

    /* Get caps might be able to block if device is not activated yet */
    caps = gst_wasapi2_ring_buffer_get_caps (ringbuffer);
    gst_object_unref (ringbuffer);
  } else {
    GST_OBJECT_UNLOCK (bsrc);
  }

  if (!caps)
    caps = gst_pad_get_pad_template_caps (bsrc->srcpad);

  if (filter) {
    GstCaps *filtered =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = filtered;
  }

  GST_DEBUG_OBJECT (bsrc, "returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstAudioRingBuffer *
gst_wasapi2_src_create_ringbuffer (GstAudioBaseSrc * src)
{
  GstWasapi2Src *self = GST_WASAPI2_SRC (src);
  GstAudioRingBuffer *ringbuffer;
  gchar *name;
  GstWasapi2ClientDeviceClass device_class =
      GST_WASAPI2_CLIENT_DEVICE_CLASS_CAPTURE;

  if (self->loopback_pid) {
    if (self->loopback_mode == GST_WASAPI2_SRC_LOOPBACK_INCLUDE_PROCESS_TREE) {
      device_class =
          GST_WASAPI2_CLIENT_DEVICE_CLASS_INCLUDE_PROCESS_LOOPBACK_CAPTURE;
    } else if (self->loopback_mode ==
        GST_WASAPI2_SRC_LOOPBACK_EXCLUDE_PROCESS_TREE) {
      device_class =
          GST_WASAPI2_CLIENT_DEVICE_CLASS_EXCLUDE_PROCESS_LOOPBACK_CAPTURE;
    }
  } else if (self->loopback) {
    device_class = GST_WASAPI2_CLIENT_DEVICE_CLASS_LOOPBACK_CAPTURE;
  }

  GST_DEBUG_OBJECT (self, "Device class %d", device_class);

  name = g_strdup_printf ("%s-ringbuffer", GST_OBJECT_NAME (src));

  ringbuffer =
      gst_wasapi2_ring_buffer_new (device_class,
      self->low_latency, self->device_id, self->dispatcher, name,
      self->loopback_pid);
  g_free (name);

  if (self->loopback) {
    gst_wasapi2_ring_buffer_set_device_mute_monitoring (GST_WASAPI2_RING_BUFFER
        (ringbuffer), self->loopback_silence_on_device_mute);
  }

  return ringbuffer;
}

static void
gst_wasapi2_src_set_mute (GstWasapi2Src * self, gboolean mute)
{
  GstAudioBaseSrc *bsrc = GST_AUDIO_BASE_SRC_CAST (self);
  HRESULT hr;

  GST_OBJECT_LOCK (self);

  self->mute = mute;
  self->mute_changed = TRUE;

  if (bsrc->ringbuffer) {
    GstWasapi2RingBuffer *ringbuffer =
        GST_WASAPI2_RING_BUFFER (bsrc->ringbuffer);

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
gst_wasapi2_src_get_mute (GstWasapi2Src * self)
{
  GstAudioBaseSrc *bsrc = GST_AUDIO_BASE_SRC_CAST (self);
  gboolean mute;
  HRESULT hr;

  GST_OBJECT_LOCK (self);

  mute = self->mute;

  if (bsrc->ringbuffer) {
    GstWasapi2RingBuffer *ringbuffer =
        GST_WASAPI2_RING_BUFFER (bsrc->ringbuffer);

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
gst_wasapi2_src_set_volume (GstWasapi2Src * self, gdouble volume)
{
  GstAudioBaseSrc *bsrc = GST_AUDIO_BASE_SRC_CAST (self);
  HRESULT hr;

  GST_OBJECT_LOCK (self);

  self->volume = volume;
  /* clip volume value */
  self->volume = MAX (0.0, self->volume);
  self->volume = MIN (1.0, self->volume);
  self->volume_changed = TRUE;

  if (bsrc->ringbuffer) {
    GstWasapi2RingBuffer *ringbuffer =
        GST_WASAPI2_RING_BUFFER (bsrc->ringbuffer);

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
gst_wasapi2_src_get_volume (GstWasapi2Src * self)
{
  GstAudioBaseSrc *bsrc = GST_AUDIO_BASE_SRC_CAST (self);
  gfloat volume;
  HRESULT hr;

  GST_OBJECT_LOCK (self);

  volume = (gfloat) self->volume;

  if (bsrc->ringbuffer) {
    GstWasapi2RingBuffer *ringbuffer =
        GST_WASAPI2_RING_BUFFER (bsrc->ringbuffer);

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

static void
gst_wasapi2_src_set_silence_on_mute (GstWasapi2Src * self, gboolean value)
{
  GstAudioBaseSrc *bsrc = GST_AUDIO_BASE_SRC_CAST (self);

  GST_OBJECT_LOCK (self);

  self->loopback_silence_on_device_mute = value;

  if (self->loopback && bsrc->ringbuffer) {
    GstWasapi2RingBuffer *ringbuffer =
        GST_WASAPI2_RING_BUFFER (bsrc->ringbuffer);

    gst_wasapi2_ring_buffer_set_device_mute_monitoring (ringbuffer, value);
  }

  GST_OBJECT_UNLOCK (self);
}
