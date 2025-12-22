/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
 * Copyright (C) 2013 Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2018 Centricular Ltd.
 *   Author: Nirbheek Chauhan <nirbheek@centricular.com>
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
 * SECTION:element-wasapisink
 * @title: wasapisink
 *
 * Provides audio playback using the Windows Audio Session API available with
 * Vista and newer.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v audiotestsrc samplesperbuffer=160 ! wasapisink
 * ]| Generate 20 ms buffers and render to the default audio device.
 *
 * |[
 * gst-launch-1.0 -v audiotestsrc samplesperbuffer=160 ! wasapisink low-latency=true
 * ]| Same as above, but with the minimum possible latency
 *
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstwasapisink.h"

#include <avrt.h>

GST_DEBUG_CATEGORY_STATIC (gst_wasapi_sink_debug);
#define GST_CAT_DEFAULT gst_wasapi_sink_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_WASAPI_STATIC_CAPS));

#define DEFAULT_ROLE          GST_WASAPI_DEVICE_ROLE_CONSOLE
#define DEFAULT_MUTE          FALSE
#define DEFAULT_EXCLUSIVE     FALSE
#define DEFAULT_LOW_LATENCY   FALSE
#define DEFAULT_AUDIOCLIENT3  TRUE

enum
{
  PROP_0,
  PROP_ROLE,
  PROP_MUTE,
  PROP_DEVICE,
  PROP_EXCLUSIVE,
  PROP_LOW_LATENCY,
  PROP_AUDIOCLIENT3
};

static void gst_wasapi_sink_dispose (GObject * object);
static void gst_wasapi_sink_finalize (GObject * object);
static void gst_wasapi_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_wasapi_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_wasapi_sink_get_caps (GstBaseSink * bsink,
    GstCaps * filter);

static gboolean gst_wasapi_sink_prepare (GstAudioSink * asink,
    GstAudioRingBufferSpec * spec);
static gboolean gst_wasapi_sink_unprepare (GstAudioSink * asink);
static gboolean gst_wasapi_sink_open (GstAudioSink * asink);
static gboolean gst_wasapi_sink_close (GstAudioSink * asink);
static gint gst_wasapi_sink_write (GstAudioSink * asink,
    gpointer data, guint length);
static guint gst_wasapi_sink_delay (GstAudioSink * asink);
static void gst_wasapi_sink_reset (GstAudioSink * asink);

#define gst_wasapi_sink_parent_class parent_class
G_DEFINE_TYPE (GstWasapiSink, gst_wasapi_sink, GST_TYPE_AUDIO_SINK);

static void
gst_wasapi_sink_class_init (GstWasapiSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);
  GstAudioSinkClass *gstaudiosink_class = GST_AUDIO_SINK_CLASS (klass);

  gobject_class->dispose = gst_wasapi_sink_dispose;
  gobject_class->finalize = gst_wasapi_sink_finalize;
  gobject_class->set_property = gst_wasapi_sink_set_property;
  gobject_class->get_property = gst_wasapi_sink_get_property;

  g_object_class_install_property (gobject_class,
      PROP_ROLE,
      g_param_spec_enum ("role", "Role",
          "Role of the device: communications, multimedia, etc",
          GST_WASAPI_DEVICE_TYPE_ROLE, DEFAULT_ROLE, G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class,
      PROP_MUTE,
      g_param_spec_boolean ("mute", "Mute", "Mute state of this stream",
          DEFAULT_MUTE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  g_object_class_install_property (gobject_class,
      PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "WASAPI device endpoint ID as provided by IMMDevice::GetId",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_EXCLUSIVE,
      g_param_spec_boolean ("exclusive", "Exclusive mode",
          "Open the device in exclusive mode",
          DEFAULT_EXCLUSIVE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_LOW_LATENCY,
      g_param_spec_boolean ("low-latency", "Low latency",
          "Optimize all settings for lowest latency. Always safe to enable.",
          DEFAULT_LOW_LATENCY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_AUDIOCLIENT3,
      g_param_spec_boolean ("use-audioclient3", "Use the AudioClient3 API",
          "Use the Windows 10 AudioClient3 API when available and if the "
          "low-latency property is set to TRUE",
          DEFAULT_AUDIOCLIENT3, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
  gst_element_class_set_static_metadata (gstelement_class, "WasapiSrc",
      "Sink/Audio/Hardware",
      "Stream audio to an audio capture device through WASAPI",
      "Nirbheek Chauhan <nirbheek@centricular.com>, "
      "Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>");

  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_wasapi_sink_get_caps);

  gstaudiosink_class->prepare = GST_DEBUG_FUNCPTR (gst_wasapi_sink_prepare);
  gstaudiosink_class->unprepare = GST_DEBUG_FUNCPTR (gst_wasapi_sink_unprepare);
  gstaudiosink_class->open = GST_DEBUG_FUNCPTR (gst_wasapi_sink_open);
  gstaudiosink_class->close = GST_DEBUG_FUNCPTR (gst_wasapi_sink_close);
  gstaudiosink_class->write = GST_DEBUG_FUNCPTR (gst_wasapi_sink_write);
  gstaudiosink_class->delay = GST_DEBUG_FUNCPTR (gst_wasapi_sink_delay);
  gstaudiosink_class->reset = GST_DEBUG_FUNCPTR (gst_wasapi_sink_reset);

  GST_DEBUG_CATEGORY_INIT (gst_wasapi_sink_debug, "wasapisink",
      0, "Windows audio session API sink");

  gst_type_mark_as_plugin_api (GST_WASAPI_DEVICE_TYPE_ROLE, 0);
}

static void
gst_wasapi_sink_init (GstWasapiSink * self)
{
  self->role = DEFAULT_ROLE;
  self->mute = DEFAULT_MUTE;
  self->sharemode = AUDCLNT_SHAREMODE_SHARED;
  self->low_latency = DEFAULT_LOW_LATENCY;
  self->try_audioclient3 = DEFAULT_AUDIOCLIENT3;
  self->event_handle = CreateEvent (NULL, FALSE, FALSE, NULL);
  self->cancellable = CreateEvent (NULL, TRUE, FALSE, NULL);
  self->client_needs_restart = FALSE;

  self->enumerator = gst_mm_device_enumerator_new ();
}

static void
gst_wasapi_sink_dispose (GObject * object)
{
  GstWasapiSink *self = GST_WASAPI_SINK (object);

  if (self->event_handle != NULL) {
    CloseHandle (self->event_handle);
    self->event_handle = NULL;
  }

  if (self->cancellable != NULL) {
    CloseHandle (self->cancellable);
    self->cancellable = NULL;
  }

  if (self->client != NULL) {
    IUnknown_Release (self->client);
    self->client = NULL;
  }

  if (self->render_client != NULL) {
    IUnknown_Release (self->render_client);
    self->render_client = NULL;
  }

  gst_clear_object (&self->enumerator);

  G_OBJECT_CLASS (gst_wasapi_sink_parent_class)->dispose (object);
}

static void
gst_wasapi_sink_finalize (GObject * object)
{
  GstWasapiSink *self = GST_WASAPI_SINK (object);

  CoTaskMemFree (self->mix_format);
  self->mix_format = NULL;

  if (self->cached_caps != NULL) {
    gst_caps_unref (self->cached_caps);
    self->cached_caps = NULL;
  }

  g_clear_pointer (&self->positions, g_free);
  g_clear_pointer (&self->device_strid, g_free);
  self->mute = FALSE;

  G_OBJECT_CLASS (gst_wasapi_sink_parent_class)->finalize (object);
}

static void
gst_wasapi_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWasapiSink *self = GST_WASAPI_SINK (object);

  switch (prop_id) {
    case PROP_ROLE:
      self->role = gst_wasapi_device_role_to_erole (g_value_get_enum (value));
      break;
    case PROP_MUTE:
      self->mute = g_value_get_boolean (value);
      break;
    case PROP_DEVICE:
    {
      const gchar *device = g_value_get_string (value);
      g_free (self->device_strid);
      self->device_strid =
          device ? g_utf8_to_utf16 (device, -1, NULL, NULL, NULL) : NULL;
      break;
    }
    case PROP_EXCLUSIVE:
      self->sharemode = g_value_get_boolean (value)
          ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
      break;
    case PROP_LOW_LATENCY:
      self->low_latency = g_value_get_boolean (value);
      break;
    case PROP_AUDIOCLIENT3:
      self->try_audioclient3 = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wasapi_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWasapiSink *self = GST_WASAPI_SINK (object);

  switch (prop_id) {
    case PROP_ROLE:
      g_value_set_enum (value, gst_wasapi_erole_to_device_role (self->role));
      break;
    case PROP_MUTE:
      g_value_set_boolean (value, self->mute);
      break;
    case PROP_DEVICE:
      g_value_take_string (value, self->device_strid ?
          g_utf16_to_utf8 (self->device_strid, -1, NULL, NULL, NULL) : NULL);
      break;
    case PROP_EXCLUSIVE:
      g_value_set_boolean (value,
          self->sharemode == AUDCLNT_SHAREMODE_EXCLUSIVE);
      break;
    case PROP_LOW_LATENCY:
      g_value_set_boolean (value, self->low_latency);
      break;
    case PROP_AUDIOCLIENT3:
      g_value_set_boolean (value, self->try_audioclient3);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_wasapi_sink_can_audioclient3 (GstWasapiSink * self)
{
  /* AudioClient3 API only makes sense in shared mode */
  if (self->sharemode != AUDCLNT_SHAREMODE_SHARED)
    return FALSE;

  if (!self->try_audioclient3) {
    GST_INFO_OBJECT (self, "AudioClient3 disabled by user");
    return FALSE;
  }

  if (!gst_wasapi_util_have_audioclient3 ()) {
    GST_INFO_OBJECT (self, "AudioClient3 not available on this OS");
    return FALSE;
  }

  /* Only use audioclient3 when low-latency is requested because otherwise
   * very slow machines and VMs with 1 CPU allocated will get glitches:
   * https://bugzilla.gnome.org/show_bug.cgi?id=794497 */
  if (!self->low_latency) {
    GST_INFO_OBJECT (self, "AudioClient3 disabled because low-latency mode "
        "was not requested");
    return FALSE;
  }

  return TRUE;
}

static GstCaps *
gst_wasapi_sink_get_caps (GstBaseSink * bsink, GstCaps * filter)
{
  GstWasapiSink *self = GST_WASAPI_SINK (bsink);
  WAVEFORMATEX *format = NULL;
  GstCaps *caps = NULL;

  GST_DEBUG_OBJECT (self, "entering get caps");

  if (self->cached_caps) {
    caps = gst_caps_ref (self->cached_caps);
  } else {
    GstCaps *template_caps;
    gboolean ret;

    template_caps = gst_pad_get_pad_template_caps (bsink->sinkpad);

    if (!self->client) {
      caps = template_caps;
      goto out;
    }

    ret = gst_wasapi_util_get_device_format (GST_ELEMENT (self),
        self->sharemode, self->device, self->client, &format);
    if (!ret) {
      GST_ELEMENT_ERROR (self, STREAM, FORMAT, (NULL),
          ("failed to detect format"));
      gst_caps_unref (template_caps);
      return NULL;
    }

    gst_wasapi_util_parse_waveformatex ((WAVEFORMATEXTENSIBLE *) format,
        template_caps, &caps, &self->positions);
    if (caps == NULL) {
      GST_ELEMENT_ERROR (self, STREAM, FORMAT, (NULL), ("unknown format"));
      gst_caps_unref (template_caps);
      return NULL;
    }

    {
      gchar *pos_str = gst_audio_channel_positions_to_string (self->positions,
          format->nChannels);
      GST_INFO_OBJECT (self, "positions are: %s", pos_str);
      g_free (pos_str);
    }

    self->mix_format = format;
    gst_caps_replace (&self->cached_caps, caps);
    gst_caps_unref (template_caps);
  }

  if (filter) {
    GstCaps *filtered =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = filtered;
  }

out:
  GST_DEBUG_OBJECT (self, "returning caps %" GST_PTR_FORMAT, caps);
  return caps;
}

static gboolean
gst_wasapi_sink_open (GstAudioSink * asink)
{
  GstWasapiSink *self = GST_WASAPI_SINK (asink);
  gboolean res = FALSE;
  IMMDevice *device = NULL;
  IAudioClient *client = NULL;

  GST_DEBUG_OBJECT (self, "opening device");

  if (self->client)
    return TRUE;

  /* FIXME: Switching the default device does not switch the stream to it,
   * even if the old device was unplugged. We need to handle this somehow.
   * For example, perhaps we should automatically switch to the new device if
   * the default device is changed and a device isn't explicitly selected. */
  if (!gst_wasapi_util_get_device (self->enumerator, eRender,
          self->role, self->device_strid, &device)
      || !gst_wasapi_util_get_audio_client (GST_ELEMENT (self),
          device, &client)) {
    if (!self->device_strid)
      GST_ELEMENT_ERROR (self, RESOURCE, OPEN_WRITE, (NULL),
          ("Failed to get default device"));
    else
      GST_ELEMENT_ERROR (self, RESOURCE, OPEN_WRITE, (NULL),
          ("Failed to open device %S", self->device_strid));
    goto beach;
  }

  self->client = client;
  self->device = device;
  res = TRUE;

beach:

  return res;
}

static gboolean
gst_wasapi_sink_close (GstAudioSink * asink)
{
  GstWasapiSink *self = GST_WASAPI_SINK (asink);

  if (self->device != NULL) {
    IUnknown_Release (self->device);
    self->device = NULL;
  }

  if (self->client != NULL) {
    IUnknown_Release (self->client);
    self->client = NULL;
  }

  return TRUE;
}

/* Get the empty space in the buffer that we have to write to */
static gint
gst_wasapi_sink_get_can_frames (GstWasapiSink * self)
{
  HRESULT hr;
  guint n_frames_padding;

  /* There is no padding in exclusive mode since there is no ringbuffer */
  if (self->sharemode == AUDCLNT_SHAREMODE_EXCLUSIVE) {
    GST_DEBUG_OBJECT (self, "exclusive mode, can write: %i",
        self->buffer_frame_count);
    return self->buffer_frame_count;
  }

  /* Frames the card hasn't rendered yet */
  hr = IAudioClient_GetCurrentPadding (self->client, &n_frames_padding);
  HR_FAILED_ELEMENT_ERROR_RET (hr, IAudioClient::GetCurrentPadding, self, -1);

  GST_DEBUG_OBJECT (self, "%i unread frames (padding)", n_frames_padding);

  /* We can write out these many frames */
  return self->buffer_frame_count - n_frames_padding;
}

static gboolean
gst_wasapi_sink_prepare (GstAudioSink * asink, GstAudioRingBufferSpec * spec)
{
  GstWasapiSink *self = GST_WASAPI_SINK (asink);
  gboolean res = FALSE;
  REFERENCE_TIME latency_rt;
  guint bpf, rate, devicep_frames;
  HRESULT hr;

  if (!self->client) {
    GST_DEBUG_OBJECT (self, "no IAudioClient, creating a new one");
    if (!gst_wasapi_util_get_audio_client (GST_ELEMENT (self),
            self->device, &self->client))
      goto beach;
  }

  if (gst_wasapi_sink_can_audioclient3 (self)) {
    if (!gst_wasapi_util_initialize_audioclient3 (GST_ELEMENT (self), spec,
            (IAudioClient3 *) self->client, self->mix_format, self->low_latency,
            FALSE, &devicep_frames))
      goto beach;
  } else {
    if (!gst_wasapi_util_initialize_audioclient (GST_ELEMENT (self), spec,
            self->client, self->mix_format, self->sharemode, self->low_latency,
            FALSE, &devicep_frames))
      goto beach;
  }

  bpf = GST_AUDIO_INFO_BPF (&spec->info);
  rate = GST_AUDIO_INFO_RATE (&spec->info);

  /* Total size of the allocated buffer that we will write to */
  hr = IAudioClient_GetBufferSize (self->client, &self->buffer_frame_count);
  HR_FAILED_GOTO (hr, IAudioClient::GetBufferSize, beach);

  GST_INFO_OBJECT (self, "buffer size is %i frames, device period is %i "
      "frames, bpf is %i bytes, rate is %i Hz", self->buffer_frame_count,
      devicep_frames, bpf, rate);

  /* Actual latency-time/buffer-time will be different now */
  spec->segsize = devicep_frames * bpf;

  /* We need a minimum of 2 segments to ensure glitch-free playback */
  spec->segtotal = MAX (self->buffer_frame_count * bpf / spec->segsize, 2);

  GST_INFO_OBJECT (self, "segsize is %i, segtotal is %i", spec->segsize,
      spec->segtotal);

  /* Get latency for logging */
  hr = IAudioClient_GetStreamLatency (self->client, &latency_rt);
  HR_FAILED_GOTO (hr, IAudioClient::GetStreamLatency, beach);

  GST_INFO_OBJECT (self, "wasapi stream latency: %" G_GINT64_FORMAT " (%"
      G_GINT64_FORMAT "ms)", latency_rt, latency_rt / 10000);

  /* Set the event handler which will trigger writes */
  hr = IAudioClient_SetEventHandle (self->client, self->event_handle);
  HR_FAILED_GOTO (hr, IAudioClient::SetEventHandle, beach);

  /* Get render sink client and start it up */
  if (!gst_wasapi_util_get_render_client (GST_ELEMENT (self), self->client,
          &self->render_client)) {
    goto beach;
  }

  GST_INFO_OBJECT (self, "got render client");

  /* To avoid start-up glitches, before starting the streaming, we fill the
   * buffer with silence as recommended by the documentation:
   * https://msdn.microsoft.com/en-us/library/windows/desktop/dd370879%28v=vs.85%29.aspx */
  {
    gint n_frames, len;
    gint16 *dst = NULL;

    n_frames = gst_wasapi_sink_get_can_frames (self);
    if (n_frames < 1) {
      GST_ELEMENT_ERROR (self, RESOURCE, WRITE, (NULL),
          ("should have more than %i frames to write", n_frames));
      goto beach;
    }

    len = n_frames * self->mix_format->nBlockAlign;

    hr = IAudioRenderClient_GetBuffer (self->render_client, n_frames,
        (BYTE **) & dst);
    HR_FAILED_GOTO (hr, IAudioRenderClient::GetBuffer, beach);

    GST_DEBUG_OBJECT (self, "pre-wrote %i bytes of silence", len);

    hr = IAudioRenderClient_ReleaseBuffer (self->render_client, n_frames,
        AUDCLNT_BUFFERFLAGS_SILENT);
    HR_FAILED_GOTO (hr, IAudioRenderClient::ReleaseBuffer, beach);
  }

  hr = IAudioClient_Start (self->client);
  HR_FAILED_GOTO (hr, IAudioClient::Start, beach);
  self->client_needs_restart = FALSE;

  gst_audio_ring_buffer_set_channel_positions (GST_AUDIO_BASE_SINK
      (self)->ringbuffer, self->positions);

  res = TRUE;

  /* reset cancellable event handle */
  ResetEvent (self->cancellable);

beach:
  /* unprepare() is not called if prepare() fails, but we want it to be, so call
   * it manually when needed */
  if (!res)
    gst_wasapi_sink_unprepare (asink);

  return res;
}

static gboolean
gst_wasapi_sink_unprepare (GstAudioSink * asink)
{
  GstWasapiSink *self = GST_WASAPI_SINK (asink);

  if (self->client != NULL) {
    IUnknown_Release (self->client);
    self->client = NULL;
  }

  if (self->render_client != NULL) {
    IUnknown_Release (self->render_client);
    self->render_client = NULL;
  }

  return TRUE;
}

static gint
gst_wasapi_sink_write (GstAudioSink * asink, gpointer data, guint length)
{
  GstWasapiSink *self = GST_WASAPI_SINK (asink);
  HRESULT hr;
  gint16 *dst = NULL;
  DWORD dwWaitResult;
  guint can_frames, have_frames, n_frames, write_len, written_len = 0;
  HANDLE event_handle[2];

  event_handle[0] = self->event_handle;
  event_handle[1] = self->cancellable;

  GST_OBJECT_LOCK (self);
  if (self->client_needs_restart) {
    hr = IAudioClient_Start (self->client);
    HR_FAILED_ELEMENT_ERROR_AND (hr, IAudioClient::Start, self,
        GST_OBJECT_UNLOCK (self); goto err);
    self->client_needs_restart = FALSE;
    ResetEvent (self->cancellable);
  }
  GST_OBJECT_UNLOCK (self);

  /* We have N frames to be written out */
  have_frames = length / (self->mix_format->nBlockAlign);

  if (self->sharemode == AUDCLNT_SHAREMODE_EXCLUSIVE) {
    /* In exclusive mode we have to wait always */
    dwWaitResult = WaitForMultipleObjects (2, event_handle, FALSE, INFINITE);
    if (dwWaitResult != WAIT_OBJECT_0 && dwWaitResult != WAIT_OBJECT_0 + 1) {
      GST_ERROR_OBJECT (self, "Error waiting for event handle: %x",
          (guint) dwWaitResult);
      goto err;
    }

    /* ::reset was requested */
    if (dwWaitResult == WAIT_OBJECT_0 + 1) {
      GST_DEBUG_OBJECT (self, "operation was cancelled");
      return -1;
    }

    can_frames = gst_wasapi_sink_get_can_frames (self);
    if (can_frames < 0) {
      GST_ERROR_OBJECT (self, "Error getting frames to write to");
      goto err;
    }
    /* In exclusive mode we need to fill the whole buffer in one go or
     * GetBuffer will error out */
    if (can_frames != have_frames) {
      GST_ERROR_OBJECT (self,
          "Need at %i frames to write for exclusive mode, but got %i",
          can_frames, have_frames);
      goto err;
    }
  } else {
    /* In shared mode we can write parts of the buffer, so only wait
     * in case we can't write anything */
    can_frames = gst_wasapi_sink_get_can_frames (self);
    if (can_frames < 0) {
      GST_ERROR_OBJECT (self, "Error getting frames to write to");
      goto err;
    }

    if (can_frames == 0) {
      dwWaitResult = WaitForMultipleObjects (2, event_handle, FALSE, INFINITE);
      if (dwWaitResult != WAIT_OBJECT_0 && dwWaitResult != WAIT_OBJECT_0 + 1) {
        GST_ERROR_OBJECT (self, "Error waiting for event handle: %x",
            (guint) dwWaitResult);
        goto err;
      }

      /* ::reset was requested */
      if (dwWaitResult == WAIT_OBJECT_0 + 1) {
        GST_DEBUG_OBJECT (self, "operation was cancelled");
        return -1;
      }

      can_frames = gst_wasapi_sink_get_can_frames (self);
      if (can_frames < 0) {
        GST_ERROR_OBJECT (self, "Error getting frames to write to");
        goto err;
      }
    }
  }

  /* We will write out these many frames, and this much length */
  n_frames = MIN (can_frames, have_frames);
  write_len = n_frames * self->mix_format->nBlockAlign;

  GST_DEBUG_OBJECT (self, "total: %i, have_frames: %i (%i bytes), "
      "can_frames: %i, will write: %i (%i bytes)", self->buffer_frame_count,
      have_frames, length, can_frames, n_frames, write_len);

  hr = IAudioRenderClient_GetBuffer (self->render_client, n_frames,
      (BYTE **) & dst);
  HR_FAILED_ELEMENT_ERROR_AND (hr, IAudioRenderClient::GetBuffer, self,
      goto err);

  memcpy (dst, data, write_len);

  hr = IAudioRenderClient_ReleaseBuffer (self->render_client, n_frames,
      self->mute ? AUDCLNT_BUFFERFLAGS_SILENT : 0);
  HR_FAILED_ELEMENT_ERROR_AND (hr, IAudioRenderClient::ReleaseBuffer, self,
      goto err);

  written_len = write_len;

out:
  return written_len;

err:
  written_len = -1;
  goto out;
}

static guint
gst_wasapi_sink_delay (GstAudioSink * asink)
{
  GstWasapiSink *self = GST_WASAPI_SINK (asink);
  guint delay = 0;
  HRESULT hr;

  hr = IAudioClient_GetCurrentPadding (self->client, &delay);
  HR_FAILED_RET (hr, IAudioClient::GetCurrentPadding, 0);

  return delay;
}

static void
gst_wasapi_sink_reset (GstAudioSink * asink)
{
  GstWasapiSink *self = GST_WASAPI_SINK (asink);
  HRESULT hr;

  GST_INFO_OBJECT (self, "reset called");

  if (!self->client)
    return;

  SetEvent (self->cancellable);

  GST_OBJECT_LOCK (self);
  hr = IAudioClient_Stop (self->client);
  HR_FAILED_AND (hr, IAudioClient::Stop, goto err);

  hr = IAudioClient_Reset (self->client);
  HR_FAILED_AND (hr, IAudioClient::Reset, goto err);

err:
  self->client_needs_restart = TRUE;
  GST_OBJECT_UNLOCK (self);
}
