/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
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
 * SECTION:element-wasapisrc
 * @title: wasapisrc
 *
 * Provides audio capture from the Windows Audio Session API available with
 * Vista and newer.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v wasapisrc ! fakesink
 * ]| Capture from the default audio device and render to fakesink.
 *
 * |[
 * gst-launch-1.0 -v wasapisrc low-latency=true ! fakesink
 * ]| Capture from the default audio device with the minimum possible latency and render to fakesink.
 *
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstwasapisrc.h"

#include <avrt.h>

GST_DEBUG_CATEGORY_STATIC (gst_wasapi_src_debug);
#define GST_CAT_DEFAULT gst_wasapi_src_debug

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_WASAPI_STATIC_CAPS));

#define DEFAULT_ROLE          GST_WASAPI_DEVICE_ROLE_CONSOLE
#define DEFAULT_LOOPBACK      FALSE
#define DEFAULT_EXCLUSIVE     FALSE
#define DEFAULT_LOW_LATENCY   FALSE
#define DEFAULT_AUDIOCLIENT3  FALSE
/* The clock provided by WASAPI is always off and causes buffers to be late
 * very quickly on the sink. Disable pending further investigation. */
#define DEFAULT_PROVIDE_CLOCK FALSE

enum
{
  PROP_0,
  PROP_ROLE,
  PROP_DEVICE,
  PROP_LOOPBACK,
  PROP_EXCLUSIVE,
  PROP_LOW_LATENCY,
  PROP_AUDIOCLIENT3
};

static void gst_wasapi_src_dispose (GObject * object);
static void gst_wasapi_src_finalize (GObject * object);
static void gst_wasapi_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_wasapi_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_wasapi_src_get_caps (GstBaseSrc * bsrc, GstCaps * filter);

static gboolean gst_wasapi_src_open (GstAudioSrc * asrc);
static gboolean gst_wasapi_src_close (GstAudioSrc * asrc);
static gboolean gst_wasapi_src_prepare (GstAudioSrc * asrc,
    GstAudioRingBufferSpec * spec);
static gboolean gst_wasapi_src_unprepare (GstAudioSrc * asrc);
static guint gst_wasapi_src_read (GstAudioSrc * asrc, gpointer data,
    guint length, GstClockTime * timestamp);
static guint gst_wasapi_src_delay (GstAudioSrc * asrc);
static void gst_wasapi_src_reset (GstAudioSrc * asrc);

#if DEFAULT_PROVIDE_CLOCK
static GstClockTime gst_wasapi_src_get_time (GstClock * clock,
    gpointer user_data);
#endif

#define gst_wasapi_src_parent_class parent_class
G_DEFINE_TYPE (GstWasapiSrc, gst_wasapi_src, GST_TYPE_AUDIO_SRC);

static void
gst_wasapi_src_class_init (GstWasapiSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstAudioSrcClass *gstaudiosrc_class = GST_AUDIO_SRC_CLASS (klass);

  gobject_class->dispose = gst_wasapi_src_dispose;
  gobject_class->finalize = gst_wasapi_src_finalize;
  gobject_class->set_property = gst_wasapi_src_set_property;
  gobject_class->get_property = gst_wasapi_src_get_property;

  g_object_class_install_property (gobject_class,
      PROP_ROLE,
      g_param_spec_enum ("role", "Role",
          "Role of the device: communications, multimedia, etc",
          GST_WASAPI_DEVICE_TYPE_ROLE, DEFAULT_ROLE, G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class,
      PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "WASAPI device endpoint ID as provided by IMMDevice::GetId",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_LOOPBACK,
      g_param_spec_boolean ("loopback", "Loopback recording",
          "Open the sink device for loopback recording",
          DEFAULT_LOOPBACK, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

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
          "Whether to use the Windows 10 AudioClient3 API when available",
          DEFAULT_AUDIOCLIENT3, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
  gst_element_class_set_static_metadata (gstelement_class, "WasapiSrc",
      "Source/Audio/Hardware",
      "Stream audio from an audio capture device through WASAPI",
      "Nirbheek Chauhan <nirbheek@centricular.com>, "
      "Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>");

  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_wasapi_src_get_caps);

  gstaudiosrc_class->open = GST_DEBUG_FUNCPTR (gst_wasapi_src_open);
  gstaudiosrc_class->close = GST_DEBUG_FUNCPTR (gst_wasapi_src_close);
  gstaudiosrc_class->read = GST_DEBUG_FUNCPTR (gst_wasapi_src_read);
  gstaudiosrc_class->prepare = GST_DEBUG_FUNCPTR (gst_wasapi_src_prepare);
  gstaudiosrc_class->unprepare = GST_DEBUG_FUNCPTR (gst_wasapi_src_unprepare);
  gstaudiosrc_class->delay = GST_DEBUG_FUNCPTR (gst_wasapi_src_delay);
  gstaudiosrc_class->reset = GST_DEBUG_FUNCPTR (gst_wasapi_src_reset);

  GST_DEBUG_CATEGORY_INIT (gst_wasapi_src_debug, "wasapisrc",
      0, "Windows audio session API source");

  gst_type_mark_as_plugin_api (GST_WASAPI_DEVICE_TYPE_ROLE, 0);
}

static void
gst_wasapi_src_init (GstWasapiSrc * self)
{
#if DEFAULT_PROVIDE_CLOCK
  /* override with a custom clock */
  if (GST_AUDIO_BASE_SRC (self)->clock)
    gst_object_unref (GST_AUDIO_BASE_SRC (self)->clock);

  GST_AUDIO_BASE_SRC (self)->clock = gst_audio_clock_new ("GstWasapiSrcClock",
      gst_wasapi_src_get_time, gst_object_ref (self),
      (GDestroyNotify) gst_object_unref);
#endif

  self->role = DEFAULT_ROLE;
  self->sharemode = AUDCLNT_SHAREMODE_SHARED;
  self->loopback = DEFAULT_LOOPBACK;
  self->low_latency = DEFAULT_LOW_LATENCY;
  self->try_audioclient3 = DEFAULT_AUDIOCLIENT3;
  self->event_handle = CreateEvent (NULL, FALSE, FALSE, NULL);
  self->cancellable = CreateEvent (NULL, TRUE, FALSE, NULL);
  self->client_needs_restart = FALSE;
  self->adapter = gst_adapter_new ();

  /* Extra event handles used for loopback */
  self->loopback_event_handle = CreateEvent (NULL, FALSE, FALSE, NULL);
  self->loopback_cancellable = CreateEvent (NULL, TRUE, FALSE, NULL);

  self->enumerator = gst_mm_device_enumerator_new ();
}

static void
gst_wasapi_src_dispose (GObject * object)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (object);

  if (self->event_handle != NULL) {
    CloseHandle (self->event_handle);
    self->event_handle = NULL;
  }

  if (self->cancellable != NULL) {
    CloseHandle (self->cancellable);
    self->cancellable = NULL;
  }

  if (self->client_clock != NULL) {
    IUnknown_Release (self->client_clock);
    self->client_clock = NULL;
  }

  if (self->client != NULL) {
    IUnknown_Release (self->client);
    self->client = NULL;
  }

  if (self->capture_client != NULL) {
    IUnknown_Release (self->capture_client);
    self->capture_client = NULL;
  }

  if (self->loopback_client != NULL) {
    IUnknown_Release (self->loopback_client);
    self->loopback_client = NULL;
  }

  if (self->loopback_event_handle != NULL) {
    CloseHandle (self->loopback_event_handle);
    self->loopback_event_handle = NULL;
  }

  if (self->loopback_cancellable != NULL) {
    CloseHandle (self->loopback_cancellable);
    self->loopback_cancellable = NULL;
  }

  gst_clear_object (&self->enumerator);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_wasapi_src_finalize (GObject * object)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (object);

  CoTaskMemFree (self->mix_format);
  self->mix_format = NULL;

  g_clear_pointer (&self->cached_caps, gst_caps_unref);
  g_clear_pointer (&self->positions, g_free);
  g_clear_pointer (&self->device_strid, g_free);

  g_object_unref (self->adapter);
  self->adapter = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_wasapi_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (object);

  switch (prop_id) {
    case PROP_ROLE:
      self->role = gst_wasapi_device_role_to_erole (g_value_get_enum (value));
      break;
    case PROP_DEVICE:
    {
      const gchar *device = g_value_get_string (value);
      g_free (self->device_strid);
      self->device_strid =
          device ? g_utf8_to_utf16 (device, -1, NULL, NULL, NULL) : NULL;
      break;
    }
    case PROP_LOOPBACK:
      self->loopback = g_value_get_boolean (value);
      break;
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
gst_wasapi_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (object);

  switch (prop_id) {
    case PROP_ROLE:
      g_value_set_enum (value, gst_wasapi_erole_to_device_role (self->role));
      break;
    case PROP_DEVICE:
      g_value_take_string (value, self->device_strid ?
          g_utf16_to_utf8 (self->device_strid, -1, NULL, NULL, NULL) : NULL);
      break;
    case PROP_LOOPBACK:
      g_value_set_boolean (value, self->loopback);
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
gst_wasapi_src_can_audioclient3 (GstWasapiSrc * self)
{
  return (self->sharemode == AUDCLNT_SHAREMODE_SHARED &&
      self->try_audioclient3 && gst_wasapi_util_have_audioclient3 ());
}

static GstCaps *
gst_wasapi_src_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (bsrc);
  WAVEFORMATEX *format = NULL;
  GstCaps *caps = NULL;

  GST_DEBUG_OBJECT (self, "entering get caps");

  if (self->cached_caps) {
    caps = gst_caps_ref (self->cached_caps);
  } else {
    GstCaps *template_caps;
    gboolean ret;

    template_caps = gst_pad_get_pad_template_caps (bsrc->srcpad);

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
gst_wasapi_src_open (GstAudioSrc * asrc)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (asrc);
  gboolean res = FALSE;
  IAudioClient *client = NULL;
  IMMDevice *device = NULL;
  IMMDevice *loopback_device = NULL;

  if (self->client)
    return TRUE;

  /* FIXME: Switching the default device does not switch the stream to it,
   * even if the old device was unplugged. We need to handle this somehow.
   * For example, perhaps we should automatically switch to the new device if
   * the default device is changed and a device isn't explicitly selected. */
  if (!gst_wasapi_util_get_device (self->enumerator,
          self->loopback ? eRender : eCapture, self->role, self->device_strid,
          &device)
      || !gst_wasapi_util_get_audio_client (GST_ELEMENT (self),
          device, &client)) {
    if (!self->device_strid)
      GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, (NULL),
          ("Failed to get default device"));
    else
      GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, (NULL),
          ("Failed to open device %S", self->device_strid));
    goto beach;
  }

  /* An oddness of wasapi loopback feature is that capture client will not
   * provide any audio data if there is no outputting sound.
   * To workaround this problem, probably we can add timeout around loop
   * in this case but it's glitch prone. So, instead of timeout,
   * we will keep pusing silence data to into wasapi client so that make audio
   * client report audio data in any case
   */
  if (!gst_wasapi_util_get_device (self->enumerator,
          eRender, self->role, self->device_strid, &loopback_device)
      || !gst_wasapi_util_get_audio_client (GST_ELEMENT (self),
          loopback_device, &self->loopback_client)) {
    if (!self->device_strid)
      GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, (NULL),
          ("Failed to get default device for loopback"));
    else
      GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, (NULL),
          ("Failed to open device %S", self->device_strid));
    goto beach;

    /* no need to hold this object */
    IUnknown_Release (loopback_device);
  }

  self->client = client;
  self->device = device;
  res = TRUE;

beach:

  return res;
}

static gboolean
gst_wasapi_src_close (GstAudioSrc * asrc)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (asrc);

  if (self->device != NULL) {
    IUnknown_Release (self->device);
    self->device = NULL;
  }

  if (self->client != NULL) {
    IUnknown_Release (self->client);
    self->client = NULL;
  }

  if (self->loopback_client != NULL) {
    IUnknown_Release (self->loopback_client);
    self->loopback_client = NULL;
  }

  return TRUE;
}

static gpointer
gst_wasapi_src_loopback_silence_feeding_thread (GstWasapiSrc * self)
{
  HRESULT hr;
  UINT32 buffer_frames;
  gboolean res G_GNUC_UNUSED = FALSE;
  BYTE *data;
  DWORD dwWaitResult;
  HANDLE event_handle[2];
  UINT32 padding;
  UINT32 n_frames;

  /* NOTE: if this task cause glitch, we need to consider thread priority
   * adjusing. See gstaudioutilsprivate.c (e.g., AvSetMmThreadCharacteristics)
   * for this context */

  GST_INFO_OBJECT (self, "Run loopback silence feeding thread");

  event_handle[0] = self->loopback_event_handle;
  event_handle[1] = self->loopback_cancellable;

  hr = IAudioClient_GetBufferSize (self->loopback_client, &buffer_frames);
  HR_FAILED_GOTO (hr, IAudioClient::GetBufferSize, beach);

  hr = IAudioClient_SetEventHandle (self->loopback_client,
      self->loopback_event_handle);
  HR_FAILED_GOTO (hr, IAudioClient::SetEventHandle, beach);

  /* To avoid start-up glitches, before starting the streaming, we fill the
   * buffer with silence as recommended by the documentation:
   * https://msdn.microsoft.com/en-us/library/windows/desktop/dd370879%28v=vs.85%29.aspx */
  hr = IAudioRenderClient_GetBuffer (self->loopback_render_client,
      buffer_frames, &data);
  HR_FAILED_GOTO (hr, IAudioRenderClient::GetBuffer, beach);

  hr = IAudioRenderClient_ReleaseBuffer (self->loopback_render_client,
      buffer_frames, AUDCLNT_BUFFERFLAGS_SILENT);
  HR_FAILED_GOTO (hr, IAudioRenderClient::ReleaseBuffer, beach);

  hr = IAudioClient_Start (self->loopback_client);
  HR_FAILED_GOTO (hr, IAudioClock::Start, beach);

  /* There is an OS bug prior to Windows 10, that is loopback capture client
   * will not receive event (in case of event-driven mode).
   * A guide for workaround this case is that signal it whenever render client
   * writes data.
   * See https://docs.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-initialize
   */

  /* Signal for read thread to wakeup */
  SetEvent (self->event_handle);

  /* Ok, now we are ready for running for feeding silence data */
  while (1) {
    dwWaitResult = WaitForMultipleObjects (2, event_handle, FALSE, INFINITE);
    if (dwWaitResult != WAIT_OBJECT_0 && dwWaitResult != WAIT_OBJECT_0 + 1) {
      GST_ERROR_OBJECT (self, "Error waiting for event handle: %x",
          (guint) dwWaitResult);
      goto stop;
    }

    /* Stopping was requested from unprepare() */
    if (dwWaitResult == WAIT_OBJECT_0 + 1) {
      GST_DEBUG_OBJECT (self, "operation was cancelled");
      goto stop;
    }

    hr = IAudioClient_GetCurrentPadding (self->loopback_client, &padding);
    HR_FAILED_GOTO (hr, IAudioClock::Start, stop);

    if (buffer_frames < padding) {
      GST_WARNING_OBJECT (self,
          "Current padding %d is too large (buffer size %d)",
          padding, buffer_frames);
      n_frames = 0;
    } else {
      n_frames = buffer_frames - padding;
    }

    hr = IAudioRenderClient_GetBuffer (self->loopback_render_client, n_frames,
        &data);
    HR_FAILED_GOTO (hr, IAudioRenderClient::GetBuffer, stop);

    hr = IAudioRenderClient_ReleaseBuffer (self->loopback_render_client,
        n_frames, AUDCLNT_BUFFERFLAGS_SILENT);
    HR_FAILED_GOTO (hr, IAudioRenderClient::ReleaseBuffer, stop);

    /* Signal for read thread to wakeup */
    SetEvent (self->event_handle);
  }

stop:
  IAudioClient_Stop (self->loopback_client);

beach:
  GST_INFO_OBJECT (self, "Terminate loopback silence feeding thread");

  return NULL;
}

static gboolean
gst_wasapi_src_prepare (GstAudioSrc * asrc, GstAudioRingBufferSpec * spec)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (asrc);
  gboolean res = FALSE;
  REFERENCE_TIME latency_rt;
  guint bpf, rate, devicep_frames, buffer_frames;
  HRESULT hr;

  if (gst_wasapi_src_can_audioclient3 (self)) {
    if (!gst_wasapi_util_initialize_audioclient3 (GST_ELEMENT (self), spec,
            (IAudioClient3 *) self->client, self->mix_format, self->low_latency,
            self->loopback, &devicep_frames))
      goto beach;
  } else {
    if (!gst_wasapi_util_initialize_audioclient (GST_ELEMENT (self), spec,
            self->client, self->mix_format, self->sharemode, self->low_latency,
            self->loopback, &devicep_frames))
      goto beach;
  }

  bpf = GST_AUDIO_INFO_BPF (&spec->info);
  rate = GST_AUDIO_INFO_RATE (&spec->info);

  /* Total size in frames of the allocated buffer that we will read from */
  hr = IAudioClient_GetBufferSize (self->client, &buffer_frames);
  HR_FAILED_GOTO (hr, IAudioClient::GetBufferSize, beach);

  GST_INFO_OBJECT (self, "buffer size is %i frames, device period is %i "
      "frames, bpf is %i bytes, rate is %i Hz", buffer_frames,
      devicep_frames, bpf, rate);

  /* Actual latency-time/buffer-time will be different now */
  spec->segsize = devicep_frames * bpf;

  /* We need a minimum of 2 segments to ensure glitch-free playback */
  spec->segtotal = MAX (buffer_frames * bpf / spec->segsize, 2);

  GST_INFO_OBJECT (self, "segsize is %i, segtotal is %i", spec->segsize,
      spec->segtotal);

  /* Get WASAPI latency for logging */
  hr = IAudioClient_GetStreamLatency (self->client, &latency_rt);
  HR_FAILED_GOTO (hr, IAudioClient::GetStreamLatency, beach);

  GST_INFO_OBJECT (self, "wasapi stream latency: %" G_GINT64_FORMAT " (%"
      G_GINT64_FORMAT " ms)", latency_rt, latency_rt / 10000);

  /* Set the event handler which will trigger reads */
  hr = IAudioClient_SetEventHandle (self->client, self->event_handle);
  HR_FAILED_GOTO (hr, IAudioClient::SetEventHandle, beach);

  /* Get the clock and the clock freq */
  if (!gst_wasapi_util_get_clock (GST_ELEMENT (self), self->client,
          &self->client_clock))
    goto beach;

  hr = IAudioClock_GetFrequency (self->client_clock, &self->client_clock_freq);
  HR_FAILED_GOTO (hr, IAudioClock::GetFrequency, beach);

  GST_INFO_OBJECT (self, "wasapi clock freq is %" G_GUINT64_FORMAT,
      self->client_clock_freq);

  /* Get capture source client and start it up */
  if (!gst_wasapi_util_get_capture_client (GST_ELEMENT (self), self->client,
          &self->capture_client)) {
    goto beach;
  }

  /* In case loopback, spawn another dedicated thread for feeding silence data
   * into wasapi render client */
  if (self->loopback) {
    /* don't need to be audioclient3 or low-latency since we will keep pushing
     * silence data which is not varying over entire playback */
    if (!gst_wasapi_util_initialize_audioclient (GST_ELEMENT (self), spec,
            self->loopback_client, self->mix_format, self->sharemode,
            FALSE, FALSE, &devicep_frames))
      goto beach;

    if (!gst_wasapi_util_get_render_client (GST_ELEMENT (self),
            self->loopback_client, &self->loopback_render_client)) {
      goto beach;
    }

    self->loopback_thread = g_thread_new ("wasapi-loopback",
        (GThreadFunc) gst_wasapi_src_loopback_silence_feeding_thread, self);
  }

  hr = IAudioClient_Start (self->client);
  HR_FAILED_GOTO (hr, IAudioClock::Start, beach);
  self->client_needs_restart = FALSE;

  gst_audio_ring_buffer_set_channel_positions (GST_AUDIO_BASE_SRC
      (self)->ringbuffer, self->positions);

  res = TRUE;

  /* reset cancellable event handle */
  ResetEvent (self->cancellable);

beach:

  /* unprepare() is not called if prepare() fails, but we want it to be, so call
   * it manually when needed */
  if (!res)
    gst_wasapi_src_unprepare (asrc);

  return res;
}

static gboolean
gst_wasapi_src_unprepare (GstAudioSrc * asrc)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (asrc);

  if (self->client != NULL) {
    IAudioClient_Stop (self->client);
  }

  if (self->capture_client != NULL) {
    IUnknown_Release (self->capture_client);
    self->capture_client = NULL;
  }

  if (self->client_clock != NULL) {
    IUnknown_Release (self->client_clock);
    self->client_clock = NULL;
  }

  if (self->loopback_thread) {
    GST_DEBUG_OBJECT (self, "loopback task thread is stopping");

    SetEvent (self->loopback_cancellable);

    g_thread_join (self->loopback_thread);
    self->loopback_thread = NULL;
    ResetEvent (self->loopback_cancellable);
    GST_DEBUG_OBJECT (self, "loopback task thread has been stopped");
  }

  if (self->loopback_render_client != NULL) {
    IUnknown_Release (self->loopback_render_client);
    self->loopback_render_client = NULL;
  }

  self->client_clock_freq = 0;

  return TRUE;
}

static guint
gst_wasapi_src_read (GstAudioSrc * asrc, gpointer data, guint length,
    GstClockTime * timestamp)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (asrc);
  HRESULT hr;
  gint16 *from = NULL;
  guint wanted = length;
  guint bpf;
  DWORD flags;

  GST_OBJECT_LOCK (self);
  if (self->client_needs_restart) {
    hr = IAudioClient_Start (self->client);
    HR_FAILED_ELEMENT_ERROR_AND (hr, IAudioClient::Start, self,
        GST_OBJECT_UNLOCK (self); goto err);
    self->client_needs_restart = FALSE;
    ResetEvent (self->cancellable);
    gst_adapter_clear (self->adapter);
  }

  bpf = self->mix_format->nBlockAlign;
  GST_OBJECT_UNLOCK (self);

  /* If we've accumulated enough data, return it immediately */
  if (gst_adapter_available (self->adapter) >= wanted) {
    memcpy (data, gst_adapter_map (self->adapter, wanted), wanted);
    gst_adapter_flush (self->adapter, wanted);
    GST_DEBUG_OBJECT (self, "Adapter has enough data, returning %i", wanted);
    goto out;
  }

  while (wanted > 0) {
    DWORD dwWaitResult;
    guint got_frames, avail_frames, n_frames, want_frames, read_len;
    HANDLE event_handle[2];

    event_handle[0] = self->event_handle;
    event_handle[1] = self->cancellable;

    /* Wait for data to become available */
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

    hr = IAudioCaptureClient_GetBuffer (self->capture_client,
        (BYTE **) & from, &got_frames, &flags, NULL, NULL);
    if (hr != S_OK) {
      if (hr == AUDCLNT_S_BUFFER_EMPTY) {
        gchar *msg = gst_wasapi_util_hresult_to_string (hr);
        GST_WARNING_OBJECT (self, "IAudioCaptureClient::GetBuffer failed: %s"
            ", retrying", msg);
        g_free (msg);
        length = 0;
        goto out;
      }
      HR_FAILED_ELEMENT_ERROR_AND (hr, IAudioCaptureClient::GetBuffer, self,
          goto err);
    }

    if (G_UNLIKELY (flags != 0)) {
      /* https://docs.microsoft.com/en-us/windows/win32/api/audioclient/ne-audioclient-_audclnt_bufferflags */
      if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
        GST_DEBUG_OBJECT (self, "WASAPI reported discontinuity (glitch?)");
      if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
        GST_DEBUG_OBJECT (self, "WASAPI reported a timestamp error");
    }

    /* Copy all the frames we got into the adapter, and then extract at most
     * @wanted size of frames from it. This helps when ::GetBuffer returns more
     * data than we can handle right now. */
    {
      GstBuffer *tmp = gst_buffer_new_allocate (NULL, got_frames * bpf, NULL);
      /* If flags has AUDCLNT_BUFFERFLAGS_SILENT, we will ignore the actual
       * data and write out silence, see:
       * https://docs.microsoft.com/en-us/windows/win32/api/audioclient/ne-audioclient-_audclnt_bufferflags */
      if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
        memset (from, 0, got_frames * bpf);
      gst_buffer_fill (tmp, 0, from, got_frames * bpf);
      gst_adapter_push (self->adapter, tmp);
    }

    /* Release all captured buffers; we copied them above */
    hr = IAudioCaptureClient_ReleaseBuffer (self->capture_client, got_frames);
    from = NULL;
    HR_FAILED_ELEMENT_ERROR_AND (hr, IAudioCaptureClient::ReleaseBuffer, self,
        goto err);

    want_frames = wanted / bpf;
    avail_frames = gst_adapter_available (self->adapter) / bpf;

    /* Only copy data that will fit into the allocated buffer of size @length */
    n_frames = MIN (avail_frames, want_frames);
    read_len = n_frames * bpf;

    GST_DEBUG_OBJECT (self, "frames captured: %i (%i bytes), "
        "can read: %i (%i bytes), will read: %i (%i bytes), "
        "adapter has: %i (%i bytes)", got_frames, got_frames * bpf, want_frames,
        wanted, n_frames, read_len, avail_frames, avail_frames * bpf);

    memcpy (data, gst_adapter_map (self->adapter, read_len), read_len);
    gst_adapter_flush (self->adapter, read_len);
    wanted -= read_len;
  }


out:
  return length;

err:
  length = -1;
  goto out;
}

static guint
gst_wasapi_src_delay (GstAudioSrc * asrc)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (asrc);
  guint delay = 0;
  HRESULT hr;

  hr = IAudioClient_GetCurrentPadding (self->client, &delay);
  HR_FAILED_RET (hr, IAudioClock::GetCurrentPadding, 0);

  return delay;
}

static void
gst_wasapi_src_reset (GstAudioSrc * asrc)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (asrc);
  HRESULT hr;

  if (!self->client)
    return;

  SetEvent (self->cancellable);

  GST_OBJECT_LOCK (self);
  hr = IAudioClient_Stop (self->client);
  HR_FAILED_AND (hr, IAudioClock::Stop, goto err);

  hr = IAudioClient_Reset (self->client);
  HR_FAILED_AND (hr, IAudioClock::Reset, goto err);

err:
  self->client_needs_restart = TRUE;
  GST_OBJECT_UNLOCK (self);
}

#if DEFAULT_PROVIDE_CLOCK
static GstClockTime
gst_wasapi_src_get_time (GstClock * clock, gpointer user_data)
{
  GstWasapiSrc *self = GST_WASAPI_SRC (user_data);
  HRESULT hr;
  guint64 devpos;
  GstClockTime result;

  if (G_UNLIKELY (self->client_clock == NULL))
    return GST_CLOCK_TIME_NONE;

  hr = IAudioClock_GetPosition (self->client_clock, &devpos, NULL);
  HR_FAILED_RET (hr, IAudioClock::GetPosition, GST_CLOCK_TIME_NONE);

  result = gst_util_uint64_scale_int (devpos, GST_SECOND,
      self->client_clock_freq);

  /*
     GST_DEBUG_OBJECT (self, "devpos = %" G_GUINT64_FORMAT
     " frequency = %" G_GUINT64_FORMAT
     " result = %" G_GUINT64_FORMAT " ms",
     devpos, self->client_clock_freq, GST_TIME_AS_MSECONDS (result));
   */

  return result;
}
#endif
