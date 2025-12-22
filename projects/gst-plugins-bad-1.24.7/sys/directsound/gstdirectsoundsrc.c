/*
 * GStreamer
 * Copyright 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright 2005 S�bastien Moutte <sebastien@moutte.net>
 * Copyright 2006 Joni Valtanen <joni.valtanen@movial.fi>
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
  TODO: add mixer device init for selection by device-guid
*/

/**
 * SECTION:element-directsoundsrc
 * @title: directsoundsrc
 *
 * Reads audio data using the DirectSound API.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v directsoundsrc ! audioconvert ! vorbisenc ! oggmux ! filesink location=dsound.ogg
 * ]| Record from DirectSound and encode to Ogg/Vorbis.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiobasesrc.h>

#include "gstdirectsoundsrc.h"

#include <windows.h>
#include <dsound.h>
#include <mmsystem.h>
#include <stdio.h>

GST_DEBUG_CATEGORY_STATIC (directsoundsrc_debug);
#define GST_CAT_DEFAULT directsoundsrc_debug

/* defaults here */
#define DEFAULT_DEVICE 0
#define DEFAULT_MUTE FALSE

/* properties */
enum
{
  PROP_0,
  PROP_DEVICE_NAME,
  PROP_DEVICE,
  PROP_VOLUME,
  PROP_MUTE
};

static void gst_directsound_src_finalize (GObject * object);

static void gst_directsound_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);

static void gst_directsound_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_directsound_src_open (GstAudioSrc * asrc);
static gboolean gst_directsound_src_close (GstAudioSrc * asrc);
static gboolean gst_directsound_src_prepare (GstAudioSrc * asrc,
    GstAudioRingBufferSpec * spec);
static gboolean gst_directsound_src_unprepare (GstAudioSrc * asrc);
static void gst_directsound_src_reset (GstAudioSrc * asrc);
static GstCaps *gst_directsound_src_getcaps (GstBaseSrc * bsrc,
    GstCaps * filter);

static guint gst_directsound_src_read (GstAudioSrc * asrc,
    gpointer data, guint length, GstClockTime * timestamp);

static void gst_directsound_src_dispose (GObject * object);

static guint gst_directsound_src_delay (GstAudioSrc * asrc);

static gboolean gst_directsound_src_mixer_find (GstDirectSoundSrc * dsoundsrc,
    MIXERCAPS * mixer_caps);
static void gst_directsound_src_mixer_init (GstDirectSoundSrc * dsoundsrc);

static gdouble gst_directsound_src_get_volume (GstDirectSoundSrc * dsoundsrc);
static void gst_directsound_src_set_volume (GstDirectSoundSrc * dsoundsrc,
    gdouble volume);

static gboolean gst_directsound_src_get_mute (GstDirectSoundSrc * dsoundsrc);
static void gst_directsound_src_set_mute (GstDirectSoundSrc * dsoundsrc,
    gboolean mute);

static const gchar *gst_directsound_src_get_device (GstDirectSoundSrc *
    dsoundsrc);
static void gst_directsound_src_set_device (GstDirectSoundSrc * dsoundsrc,
    const gchar * device_id);

static GstStaticPadTemplate directsound_src_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_DIRECTSOUND_SRC_CAPS));

#define gst_directsound_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDirectSoundSrc, gst_directsound_src,
    GST_TYPE_AUDIO_SRC, G_IMPLEMENT_INTERFACE (GST_TYPE_STREAM_VOLUME, NULL)
    );

static void
gst_directsound_src_dispose (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_directsound_src_finalize (GObject * object)
{
  GstDirectSoundSrc *dsoundsrc = GST_DIRECTSOUND_SRC (object);

  g_mutex_clear (&dsoundsrc->dsound_lock);
  gst_object_unref (dsoundsrc->system_clock);
  if (dsoundsrc->read_wait_clock_id != NULL)
    gst_clock_id_unref (dsoundsrc->read_wait_clock_id);

  g_free (dsoundsrc->device_name);

  g_free (dsoundsrc->device_id);

  g_free (dsoundsrc->device_guid);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_directsound_src_class_init (GstDirectSoundSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstAudioSrcClass *gstaudiosrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstaudiosrc_class = (GstAudioSrcClass *) klass;

  GST_DEBUG_CATEGORY_INIT (directsoundsrc_debug, "directsoundsrc", 0,
      "DirectSound Src");

  GST_DEBUG ("initializing directsoundsrc class");

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_directsound_src_finalize);
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_directsound_src_dispose);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_directsound_src_get_property);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_directsound_src_set_property);

  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_directsound_src_getcaps);

  gstaudiosrc_class->open = GST_DEBUG_FUNCPTR (gst_directsound_src_open);
  gstaudiosrc_class->close = GST_DEBUG_FUNCPTR (gst_directsound_src_close);
  gstaudiosrc_class->read = GST_DEBUG_FUNCPTR (gst_directsound_src_read);
  gstaudiosrc_class->prepare = GST_DEBUG_FUNCPTR (gst_directsound_src_prepare);
  gstaudiosrc_class->unprepare =
      GST_DEBUG_FUNCPTR (gst_directsound_src_unprepare);
  gstaudiosrc_class->delay = GST_DEBUG_FUNCPTR (gst_directsound_src_delay);
  gstaudiosrc_class->reset = GST_DEBUG_FUNCPTR (gst_directsound_src_reset);

  gst_element_class_set_static_metadata (gstelement_class,
      "DirectSound audio source", "Source/Audio",
      "Capture from a soundcard via DirectSound",
      "Joni Valtanen <joni.valtanen@movial.fi>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &directsound_src_src_factory);

  g_object_class_install_property
      (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device name",
          "Human-readable name of the sound device", NULL, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
      PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "DirectSound playback device as a GUID string (volume and mute will not work!)",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property
      (gobject_class, PROP_VOLUME,
      g_param_spec_double ("volume", "Volume",
          "Volume of this stream", 0.0, 1.0, 1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property
      (gobject_class, PROP_MUTE,
      g_param_spec_boolean ("mute", "Mute",
          "Mute state of this stream", DEFAULT_MUTE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static GstCaps *
gst_directsound_src_getcaps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstCaps *caps = NULL;
  GST_DEBUG_OBJECT (bsrc, "get caps");

  caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (bsrc));
  return caps;
}

static void
gst_directsound_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDirectSoundSrc *src = GST_DIRECTSOUND_SRC (object);
  GST_DEBUG ("set property");

  switch (prop_id) {
    case PROP_DEVICE_NAME:
      if (src->device_name) {
        g_free (src->device_name);
        src->device_name = NULL;
      }
      if (g_value_get_string (value)) {
        src->device_name = g_value_dup_string (value);
      }
      break;
    case PROP_VOLUME:
      gst_directsound_src_set_volume (src, g_value_get_double (value));
      break;
    case PROP_MUTE:
      gst_directsound_src_set_mute (src, g_value_get_boolean (value));
      break;
    case PROP_DEVICE:
      gst_directsound_src_set_device (src, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_directsound_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDirectSoundSrc *src = GST_DIRECTSOUND_SRC (object);

  GST_DEBUG ("get property");

  switch (prop_id) {
    case PROP_DEVICE_NAME:
      g_value_set_string (value, src->device_name);
      break;
    case PROP_DEVICE:
      g_value_set_string (value, gst_directsound_src_get_device (src));
      break;
    case PROP_VOLUME:
      g_value_set_double (value, gst_directsound_src_get_volume (src));
      break;
    case PROP_MUTE:
      g_value_set_boolean (value, gst_directsound_src_get_mute (src));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_directsound_src_init (GstDirectSoundSrc * src)
{
  GST_DEBUG_OBJECT (src, "initializing directsoundsrc");
  g_mutex_init (&src->dsound_lock);
  src->system_clock = gst_system_clock_obtain ();
  src->read_wait_clock_id = NULL;
  src->reset_while_sleeping = FALSE;
  src->device_guid = NULL;
  src->device_id = NULL;
  src->device_name = NULL;
  src->mixer = NULL;
  src->control_id_mute = -1;
  src->control_id_volume = -1;
  src->volume = 100;
  src->mute = FALSE;
}


/* Enumeration callback called by DirectSoundCaptureEnumerate.
 * Gets the GUID of request audio device
 */
static BOOL CALLBACK
gst_directsound_enum_callback (GUID * pGUID, TCHAR * strDesc,
    TCHAR * strDrvName, VOID * pContext)
{
  GstDirectSoundSrc *dsoundsrc = GST_DIRECTSOUND_SRC (pContext);
  gchar *driver, *description;

  description = g_locale_to_utf8 (strDesc, -1, NULL, NULL, NULL);
  if (!description) {
    GST_ERROR_OBJECT (dsoundsrc,
        "Failed to convert description from locale encoding to UTF8");
    return TRUE;
  }

  driver = g_locale_to_utf8 (strDrvName, -1, NULL, NULL, NULL);

  if (pGUID && dsoundsrc && dsoundsrc->device_name &&
      !g_strcmp0 (dsoundsrc->device_name, description)) {
    g_free (dsoundsrc->device_guid);
    dsoundsrc->device_guid = (GUID *) g_malloc0 (sizeof (GUID));
    memcpy (dsoundsrc->device_guid, pGUID, sizeof (GUID));
    GST_INFO_OBJECT (dsoundsrc, "found the requested audio device :%s",
        dsoundsrc->device_name);
    g_free (description);
    g_free (driver);
    return FALSE;
  }

  GST_INFO_OBJECT (dsoundsrc, "sound device names: %s, %s, requested device:%s",
      description, driver, dsoundsrc->device_name);

  g_free (description);
  g_free (driver);

  return TRUE;
}

static LPGUID
string_to_guid (const gchar * str)
{
  HRESULT ret;
  gunichar2 *wstr;
  LPGUID out;

  wstr = g_utf8_to_utf16 (str, -1, NULL, NULL, NULL);
  if (!wstr)
    return NULL;

  out = g_new (GUID, 1);
  ret = CLSIDFromString ((LPOLESTR) wstr, out);
  g_free (wstr);
  if (ret != NOERROR) {
    g_free (out);
    return NULL;
  }

  return out;
}

static gboolean
gst_directsound_src_open (GstAudioSrc * asrc)
{
  GstDirectSoundSrc *dsoundsrc;
  HRESULT hRes;                 /* Result for windows functions */

  GST_DEBUG_OBJECT (asrc, "opening directsoundsrc");

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);

  if (dsoundsrc->device_id) {
    GST_DEBUG_OBJECT (asrc, "device id set to: %s ", dsoundsrc->device_id);
    dsoundsrc->device_guid = string_to_guid (dsoundsrc->device_id);
    if (dsoundsrc->device_guid == NULL) {
      GST_ELEMENT_ERROR (dsoundsrc, RESOURCE, OPEN_READ,
          ("gst_directsound_src_open: device set, but guid not found: %s",
              dsoundsrc->device_id), (NULL));
      g_free (dsoundsrc->device_guid);
      return FALSE;
    }
  } else {

    hRes = DirectSoundCaptureEnumerate ((LPDSENUMCALLBACK)
        gst_directsound_enum_callback, (VOID *) dsoundsrc);

    if (FAILED (hRes)) {
      goto capture_enumerate;
    }
  }
  /* Create capture object */
  hRes =
      DirectSoundCaptureCreate (dsoundsrc->device_guid, &dsoundsrc->pDSC, NULL);


  if (FAILED (hRes)) {
    goto capture_object;
  }
  // mixer is only supported when device-id is not set
  if (!dsoundsrc->device_id) {
    gst_directsound_src_mixer_init (dsoundsrc);
  }

  return TRUE;

capture_enumerate:
  {
    GST_ELEMENT_ERROR (dsoundsrc, RESOURCE, OPEN_READ,
        ("Unable to enumerate audio capture devices"), (NULL));
    return FALSE;
  }
capture_object:
  {
    GST_ELEMENT_ERROR (dsoundsrc, RESOURCE, OPEN_READ,
        ("Unable to create capture object"), (NULL));
    return FALSE;
  }
}

static gboolean
gst_directsound_src_close (GstAudioSrc * asrc)
{
  GstDirectSoundSrc *dsoundsrc;

  GST_DEBUG_OBJECT (asrc, "closing directsoundsrc");

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);

  /* Release capture handler  */
  IDirectSoundCapture_Release (dsoundsrc->pDSC);

  if (dsoundsrc->mixer)
    mixerClose (dsoundsrc->mixer);

  return TRUE;
}

static gboolean
gst_directsound_src_prepare (GstAudioSrc * asrc, GstAudioRingBufferSpec * spec)
{
  GstDirectSoundSrc *dsoundsrc;
  WAVEFORMATEX wfx;             /* Wave format structure */
  HRESULT hRes;                 /* Result for windows functions */
  DSCBUFFERDESC descSecondary;  /* Capturebuffer description */

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);

  GST_DEBUG_OBJECT (asrc, "preparing directsoundsrc");

  /* Define buffer */
  memset (&wfx, 0, sizeof (WAVEFORMATEX));
  wfx.wFormatTag = WAVE_FORMAT_PCM;
  wfx.nChannels = GST_AUDIO_INFO_CHANNELS (&spec->info);
  wfx.nSamplesPerSec = GST_AUDIO_INFO_RATE (&spec->info);
  wfx.wBitsPerSample = GST_AUDIO_INFO_BPF (&spec->info) * 8 / wfx.nChannels;
  wfx.nBlockAlign = GST_AUDIO_INFO_BPF (&spec->info);
  wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
  /* Ignored for WAVE_FORMAT_PCM. */
  wfx.cbSize = 0;

  if (wfx.wBitsPerSample != 16 && wfx.wBitsPerSample != 8)
    goto dodgy_width;

  GST_INFO_OBJECT (asrc, "latency time: %" G_GUINT64_FORMAT " - buffer time: %"
      G_GUINT64_FORMAT, spec->latency_time, spec->buffer_time);

  /* Buffer-time should always be >= 2*latency */
  if (spec->buffer_time < spec->latency_time * 2) {
    spec->buffer_time = spec->latency_time * 2;
    GST_WARNING ("buffer-time was less than 2*latency-time, clamping");
  }

  /* Set the buffer size from our configured buffer time (in microsecs) */
  dsoundsrc->buffer_size =
      gst_util_uint64_scale_int (spec->buffer_time, wfx.nAvgBytesPerSec,
      GST_SECOND / GST_USECOND);

  GST_INFO_OBJECT (asrc, "Buffer size: %d", dsoundsrc->buffer_size);

  spec->segsize =
      gst_util_uint64_scale (spec->latency_time, wfx.nAvgBytesPerSec,
      GST_SECOND / GST_USECOND);

  /* Sanitized segsize */
  if (spec->segsize < GST_AUDIO_INFO_BPF (&spec->info))
    spec->segsize = GST_AUDIO_INFO_BPF (&spec->info);
  else if (spec->segsize % GST_AUDIO_INFO_BPF (&spec->info) != 0)
    spec->segsize =
        ((spec->segsize + GST_AUDIO_INFO_BPF (&spec->info) -
            1) / GST_AUDIO_INFO_BPF (&spec->info)) *
        GST_AUDIO_INFO_BPF (&spec->info);
  spec->segtotal = dsoundsrc->buffer_size / spec->segsize;
  /* The device usually takes time = 1-2 segments to start producing buffers */
  spec->seglatency = spec->segtotal + 2;

  /* Fetch and set the actual latency time that will be used */
  dsoundsrc->latency_time =
      gst_util_uint64_scale (spec->segsize, GST_SECOND / GST_USECOND,
      GST_AUDIO_INFO_BPF (&spec->info) * GST_AUDIO_INFO_RATE (&spec->info));

  GST_INFO_OBJECT (asrc, "actual latency time: %" G_GUINT64_FORMAT,
      spec->latency_time);

  /* Init secondary buffer description */
  memset (&descSecondary, 0, sizeof (DSCBUFFERDESC));
  descSecondary.dwSize = sizeof (DSCBUFFERDESC);
  descSecondary.dwFlags = 0;
  descSecondary.dwReserved = 0;

  /* This is not primary buffer so have to set size  */
  descSecondary.dwBufferBytes = dsoundsrc->buffer_size;
  descSecondary.lpwfxFormat = &wfx;

  /* Create buffer */
  hRes = IDirectSoundCapture_CreateCaptureBuffer (dsoundsrc->pDSC,
      &descSecondary, &dsoundsrc->pDSBSecondary, NULL);
  if (hRes != DS_OK)
    goto capture_buffer;

  dsoundsrc->bytes_per_sample = GST_AUDIO_INFO_BPF (&spec->info);

  GST_INFO_OBJECT (asrc,
      "bytes/sec: %lu, buffer size: %d, segsize: %d, segtotal: %d",
      wfx.nAvgBytesPerSec, dsoundsrc->buffer_size, spec->segsize,
      spec->segtotal);

  /* Not read anything yet */
  dsoundsrc->current_circular_offset = 0;

  GST_INFO_OBJECT (asrc, "channels: %d, rate: %d, bytes_per_sample: %d"
      " WAVEFORMATEX.nSamplesPerSec: %ld, WAVEFORMATEX.wBitsPerSample: %d,"
      " WAVEFORMATEX.nBlockAlign: %d, WAVEFORMATEX.nAvgBytesPerSec: %ld",
      GST_AUDIO_INFO_CHANNELS (&spec->info), GST_AUDIO_INFO_RATE (&spec->info),
      GST_AUDIO_INFO_BPF (&spec->info), wfx.nSamplesPerSec, wfx.wBitsPerSample,
      wfx.nBlockAlign, wfx.nAvgBytesPerSec);

  return TRUE;

capture_buffer:
  {
    GST_ELEMENT_ERROR (dsoundsrc, RESOURCE, OPEN_READ,
        ("Unable to create capturebuffer"), (NULL));
    return FALSE;
  }
dodgy_width:
  {
    GST_ELEMENT_ERROR (dsoundsrc, RESOURCE, OPEN_READ,
        ("Unexpected width %d", wfx.wBitsPerSample), (NULL));
    return FALSE;
  }
}

static gboolean
gst_directsound_src_unprepare (GstAudioSrc * asrc)
{
  GstDirectSoundSrc *dsoundsrc;

  GST_DEBUG_OBJECT (asrc, "unpreparing directsoundsrc");

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);

  GST_DSOUND_LOCK (dsoundsrc);

  /* Stop capturing */
  IDirectSoundCaptureBuffer_Stop (dsoundsrc->pDSBSecondary);

  /* Release buffer  */
  IDirectSoundCaptureBuffer_Release (dsoundsrc->pDSBSecondary);
  GST_DSOUND_UNLOCK (dsoundsrc);
  return TRUE;
}

/* 
return number of readed bytes */
static guint
gst_directsound_src_read (GstAudioSrc * asrc, gpointer data, guint length,
    GstClockTime * timestamp)
{
  GstDirectSoundSrc *dsoundsrc;
  guint64 sleep_time_ms, sleep_until;
  GstClockID clock_id;

  HRESULT hRes;                 /* Result for windows functions */
  DWORD dwCurrentCaptureCursor = 0;
  DWORD dwBufferSize = 0;

  LPVOID pLockedBuffer1 = NULL;
  LPVOID pLockedBuffer2 = NULL;
  DWORD dwSizeBuffer1 = 0;
  DWORD dwSizeBuffer2 = 0;

  DWORD dwStatus = 0;

  GST_DEBUG_OBJECT (asrc, "reading directsoundsrc");

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);

  GST_DSOUND_LOCK (dsoundsrc);

  /* Get current buffer status */
  hRes = IDirectSoundCaptureBuffer_GetStatus (dsoundsrc->pDSBSecondary,
      &dwStatus);

  if (FAILED (hRes)) {
    GST_DSOUND_UNLOCK (dsoundsrc);
    return -1;
  }

  /* Starting capturing if not already */
  if (!(dwStatus & DSCBSTATUS_CAPTURING)) {
    hRes = IDirectSoundCaptureBuffer_Start (dsoundsrc->pDSBSecondary,
        DSCBSTART_LOOPING);
    GST_INFO_OBJECT (asrc, "capture started");
  }

  /* Loop till the source has produced bytes equal to or greater than @length.
   *
   * DirectSound has a notification-based API that uses Windows CreateEvent()
   * + WaitForSingleObject(), but it is completely useless for live streams.
   *
   *  1. You must schedule all events before starting capture
   *  2. The events are all fired exactly once
   *  3. You cannot schedule new events while a capture is running
   *  4. You cannot stop/schedule/start either
   *
   * This means you cannot use the API while doing live looped capture and we
   * must resort to this.
   *
   * However, this is almost as efficient as event-based capture since it's ok
   * to consistently overwait by a fixed amount; the extra bytes will just end
   * up being used in the next call, and the extra latency will be constant. */
  while (TRUE) {
    hRes =
        IDirectSoundCaptureBuffer_GetCurrentPosition (dsoundsrc->pDSBSecondary,
        &dwCurrentCaptureCursor, NULL);

    if (FAILED (hRes)) {
      GST_DSOUND_UNLOCK (dsoundsrc);
      return -1;
    }

    /* calculate the size of the buffer that's been captured while accounting
     * for wrap-arounds */
    if (dwCurrentCaptureCursor < dsoundsrc->current_circular_offset) {
      dwBufferSize = dsoundsrc->buffer_size -
          (dsoundsrc->current_circular_offset - dwCurrentCaptureCursor);
    } else {
      dwBufferSize =
          dwCurrentCaptureCursor - dsoundsrc->current_circular_offset;
    }

    if (dwBufferSize >= length) {
      /* Yay, we got all the data we need */
      break;
    } else {
      GST_DEBUG_OBJECT (asrc, "not enough data, got %lu (want at least %u)",
          dwBufferSize, length);
      /* If we didn't get enough data, sleep for a proportionate time */
      sleep_time_ms = gst_util_uint64_scale (dsoundsrc->latency_time,
          length - dwBufferSize, length * 1000);
      /* Make sure we don't run in a tight loop unnecessarily */
      sleep_time_ms = MAX (sleep_time_ms, 10);
      /* Sleep using gst_clock_id_wait() so that we can be interrupted */
      sleep_until = gst_clock_get_time (dsoundsrc->system_clock) +
          sleep_time_ms * GST_MSECOND;
      /* Setup the clock id wait */
      if (G_UNLIKELY (dsoundsrc->read_wait_clock_id == NULL ||
              gst_clock_single_shot_id_reinit (dsoundsrc->system_clock,
                  dsoundsrc->read_wait_clock_id, sleep_until) == FALSE)) {
        if (dsoundsrc->read_wait_clock_id != NULL)
          gst_clock_id_unref (dsoundsrc->read_wait_clock_id);
        dsoundsrc->read_wait_clock_id =
            gst_clock_new_single_shot_id (dsoundsrc->system_clock, sleep_until);
      }

      clock_id = dsoundsrc->read_wait_clock_id;
      dsoundsrc->reset_while_sleeping = FALSE;

      GST_DEBUG_OBJECT (asrc, "waiting %" G_GUINT64_FORMAT "ms for more data",
          sleep_time_ms);
      GST_DSOUND_UNLOCK (dsoundsrc);

      gst_clock_id_wait (clock_id, NULL);

      GST_DSOUND_LOCK (dsoundsrc);

      if (dsoundsrc->reset_while_sleeping == TRUE) {
        GST_DEBUG_OBJECT (asrc, "reset while sleeping, cancelled read");
        GST_DSOUND_UNLOCK (dsoundsrc);
        return -1;
      }
    }
  }

  GST_DEBUG_OBJECT (asrc, "Got enough data: %lu bytes (wanted at least %u)",
      dwBufferSize, length);

  /* Lock the buffer and read only the first @length bytes. Keep the rest in
   * the capture buffer for the next read. */
  hRes = IDirectSoundCaptureBuffer_Lock (dsoundsrc->pDSBSecondary,
      dsoundsrc->current_circular_offset,
      length,
      &pLockedBuffer1, &dwSizeBuffer1, &pLockedBuffer2, &dwSizeBuffer2, 0L);

  /* NOTE: We now assume that dwSizeBuffer1 + dwSizeBuffer2 == length since the
   * API is supposed to guarantee that */

  /* Copy buffer data to another buffer */
  if (hRes == DS_OK) {
    memcpy (data, pLockedBuffer1, dwSizeBuffer1);
  }

  /* ...and if something is in another buffer */
  if (pLockedBuffer2 != NULL) {
    memcpy (((guchar *) data + dwSizeBuffer1), pLockedBuffer2, dwSizeBuffer2);
  }

  dsoundsrc->current_circular_offset += dwSizeBuffer1 + dwSizeBuffer2;
  dsoundsrc->current_circular_offset %= dsoundsrc->buffer_size;

  IDirectSoundCaptureBuffer_Unlock (dsoundsrc->pDSBSecondary,
      pLockedBuffer1, dwSizeBuffer1, pLockedBuffer2, dwSizeBuffer2);

  GST_DSOUND_UNLOCK (dsoundsrc);

  /* We always read exactly @length data */
  return length;
}

static guint
gst_directsound_src_delay (GstAudioSrc * asrc)
{
  GstDirectSoundSrc *dsoundsrc;
  HRESULT hRes;
  DWORD dwCurrentCaptureCursor;
  DWORD dwBytesInQueue = 0;
  gint nNbSamplesInQueue = 0;

  GST_INFO_OBJECT (asrc, "Delay");

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);

  /* evaluate the number of samples in queue in the circular buffer */
  hRes =
      IDirectSoundCaptureBuffer_GetCurrentPosition (dsoundsrc->pDSBSecondary,
      &dwCurrentCaptureCursor, NULL);
  /* FIXME: Check is this calculated right */
  if (hRes == S_OK) {
    if (dwCurrentCaptureCursor < dsoundsrc->current_circular_offset) {
      dwBytesInQueue =
          dsoundsrc->buffer_size - (dsoundsrc->current_circular_offset -
          dwCurrentCaptureCursor);
    } else {
      dwBytesInQueue =
          dwCurrentCaptureCursor - dsoundsrc->current_circular_offset;
    }

    nNbSamplesInQueue = dwBytesInQueue / dsoundsrc->bytes_per_sample;
  }

  GST_INFO_OBJECT (asrc, "Delay is %d samples", nNbSamplesInQueue);

  return nNbSamplesInQueue;
}

static void
gst_directsound_src_reset (GstAudioSrc * asrc)
{
  GstDirectSoundSrc *dsoundsrc;
  LPVOID pLockedBuffer = NULL;
  DWORD dwSizeBuffer = 0;

  GST_DEBUG_OBJECT (asrc, "reset directsoundsrc");

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);

  GST_DSOUND_LOCK (dsoundsrc);

  dsoundsrc->reset_while_sleeping = TRUE;
  /* Interrupt read sleep if required */
  if (dsoundsrc->read_wait_clock_id != NULL)
    gst_clock_id_unschedule (dsoundsrc->read_wait_clock_id);

  if (dsoundsrc->pDSBSecondary) {
    /*stop capturing */
    HRESULT hRes = IDirectSoundCaptureBuffer_Stop (dsoundsrc->pDSBSecondary);

    /*reset position */
    /*    hRes = IDirectSoundCaptureBuffer_SetCurrentPosition (dsoundsrc->pDSBSecondary, 0); */

    /*reset the buffer */
    hRes = IDirectSoundCaptureBuffer_Lock (dsoundsrc->pDSBSecondary,
        dsoundsrc->current_circular_offset, dsoundsrc->buffer_size,
        &pLockedBuffer, &dwSizeBuffer, NULL, NULL, 0L);

    if (SUCCEEDED (hRes)) {
      memset (pLockedBuffer, 0, dwSizeBuffer);

      hRes =
          IDirectSoundCaptureBuffer_Unlock (dsoundsrc->pDSBSecondary,
          pLockedBuffer, dwSizeBuffer, NULL, 0);
    }
    dsoundsrc->current_circular_offset = 0;

  }

  GST_DSOUND_UNLOCK (dsoundsrc);
}

/* If the PROP_DEVICE_NAME is set, find the mixer related to device;
 * otherwise we get the default input mixer. */
static gboolean
gst_directsound_src_mixer_find (GstDirectSoundSrc * dsoundsrc,
    MIXERCAPS * mixer_caps)
{
  MMRESULT mmres;
  guint i, num_mixers;

  num_mixers = mixerGetNumDevs ();
  for (i = 0; i < num_mixers; i++) {
    mmres = mixerOpen (&dsoundsrc->mixer, i, 0L, 0L,
        MIXER_OBJECTF_MIXER | MIXER_OBJECTF_WAVEIN);

    if (mmres != MMSYSERR_NOERROR)
      continue;

    mmres = mixerGetDevCaps ((UINT_PTR) dsoundsrc->mixer,
        mixer_caps, sizeof (MIXERCAPS));

    if (mmres != MMSYSERR_NOERROR) {
      mixerClose (dsoundsrc->mixer);
      continue;
    }

    /* Get default mixer */
    if (dsoundsrc->device_name == NULL) {
      GST_DEBUG ("Got default input mixer: %s", mixer_caps->szPname);
      return TRUE;
    }

    if (g_strstr_len (dsoundsrc->device_name, -1, mixer_caps->szPname) != NULL) {
      GST_DEBUG ("Got requested input mixer: %s", mixer_caps->szPname);
      return TRUE;
    }

    /* Wrong mixer */
    mixerClose (dsoundsrc->mixer);
  }

  GST_DEBUG ("Can't find input mixer");
  return FALSE;
}

static void
gst_directsound_src_mixer_init (GstDirectSoundSrc * dsoundsrc)
{
  gint i, k;
  gboolean found_mic;
  MMRESULT mmres;
  MIXERCAPS mixer_caps;
  MIXERLINE mixer_line;
  MIXERLINECONTROLS ml_ctrl;
  PMIXERCONTROL pamixer_ctrls;

  if (!gst_directsound_src_mixer_find (dsoundsrc, &mixer_caps))
    goto mixer_init_fail;

  /* Find the MIXERLINE related to MICROPHONE */
  found_mic = FALSE;
  for (i = 0; i < mixer_caps.cDestinations && !found_mic; i++) {
    gint j, num_connections;

    mixer_line.cbStruct = sizeof (mixer_line);
    mixer_line.dwDestination = i;
    mmres = mixerGetLineInfo ((HMIXEROBJ) dsoundsrc->mixer,
        &mixer_line, MIXER_GETLINEINFOF_DESTINATION);

    if (mmres != MMSYSERR_NOERROR)
      goto mixer_init_fail;

    num_connections = mixer_line.cConnections;
    for (j = 0; j < num_connections && !found_mic; j++) {
      mixer_line.cbStruct = sizeof (mixer_line);
      mixer_line.dwDestination = i;
      mixer_line.dwSource = j;
      mmres = mixerGetLineInfo ((HMIXEROBJ) dsoundsrc->mixer,
          &mixer_line, MIXER_GETLINEINFOF_SOURCE);

      if (mmres != MMSYSERR_NOERROR)
        goto mixer_init_fail;

      if (mixer_line.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE
          || mixer_line.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_LINE)
        found_mic = TRUE;
    }
  }

  if (found_mic == FALSE) {
    GST_DEBUG ("Can't find mixer line related to input");
    goto mixer_init_fail;
  }

  /* Get control associated with microphone audio line */
  pamixer_ctrls = g_malloc (sizeof (MIXERCONTROL) * mixer_line.cControls);
  ml_ctrl.cbStruct = sizeof (ml_ctrl);
  ml_ctrl.dwLineID = mixer_line.dwLineID;
  ml_ctrl.cControls = mixer_line.cControls;
  ml_ctrl.cbmxctrl = sizeof (MIXERCONTROL);
  ml_ctrl.pamxctrl = pamixer_ctrls;
  mmres = mixerGetLineControls ((HMIXEROBJ) dsoundsrc->mixer,
      &ml_ctrl, MIXER_GETLINECONTROLSF_ALL);

  /* Find control associated with volume and mute */
  for (k = 0; k < mixer_line.cControls; k++) {
    if (strstr (pamixer_ctrls[k].szName, "Volume") != NULL) {
      dsoundsrc->control_id_volume = pamixer_ctrls[k].dwControlID;
      dsoundsrc->dw_vol_max = pamixer_ctrls[k].Bounds.dwMaximum;
      dsoundsrc->dw_vol_min = pamixer_ctrls[k].Bounds.dwMinimum;
    } else if (strstr (pamixer_ctrls[k].szName, "Mute") != NULL) {
      dsoundsrc->control_id_mute = pamixer_ctrls[k].dwControlID;
    } else {
      GST_DEBUG ("Control not handled: %s", pamixer_ctrls[k].szName);
    }
  }
  g_free (pamixer_ctrls);

  if (dsoundsrc->control_id_volume < 0 && dsoundsrc->control_id_mute < 0)
    goto mixer_init_fail;

  /* Save cChannels information to properly changes in volume */
  dsoundsrc->mixerline_cchannels = mixer_line.cChannels;
  return;

mixer_init_fail:
  GST_WARNING ("Failed to get Volume and Mute controls");
  if (dsoundsrc->mixer != NULL) {
    mixerClose (dsoundsrc->mixer);
    dsoundsrc->mixer = NULL;
  }
}

static gdouble
gst_directsound_src_get_volume (GstDirectSoundSrc * dsoundsrc)
{
  return (gdouble) dsoundsrc->volume / 100;
}

static gboolean
gst_directsound_src_get_mute (GstDirectSoundSrc * dsoundsrc)
{
  return dsoundsrc->mute;
}

static void
gst_directsound_src_set_volume (GstDirectSoundSrc * dsoundsrc, gdouble volume)
{
  MMRESULT mmres;
  MIXERCONTROLDETAILS details;
  MIXERCONTROLDETAILS_UNSIGNED details_unsigned;
  glong dwvolume;

  if (dsoundsrc->mixer == NULL || dsoundsrc->control_id_volume < 0) {
    GST_WARNING ("mixer not initialized");
    return;
  }

  dwvolume = volume * dsoundsrc->dw_vol_max;
  dwvolume = CLAMP (dwvolume, dsoundsrc->dw_vol_min, dsoundsrc->dw_vol_max);

  GST_DEBUG ("max volume %ld | min volume %ld",
      dsoundsrc->dw_vol_max, dsoundsrc->dw_vol_min);
  GST_DEBUG ("set volume to %f (%ld)", volume, dwvolume);

  details.cbStruct = sizeof (details);
  details.dwControlID = dsoundsrc->control_id_volume;
  details.cChannels = dsoundsrc->mixerline_cchannels;
  details.cMultipleItems = 0;

  details_unsigned.dwValue = dwvolume;
  details.cbDetails = sizeof (MIXERCONTROLDETAILS_UNSIGNED);
  details.paDetails = &details_unsigned;

  mmres = mixerSetControlDetails ((HMIXEROBJ) dsoundsrc->mixer,
      &details, MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE);

  if (mmres != MMSYSERR_NOERROR)
    GST_WARNING ("Failed to set volume");
  else
    dsoundsrc->volume = volume * 100;
}

static void
gst_directsound_src_set_mute (GstDirectSoundSrc * dsoundsrc, gboolean mute)
{
  MMRESULT mmres;
  MIXERCONTROLDETAILS details;
  MIXERCONTROLDETAILS_BOOLEAN details_boolean;

  if (dsoundsrc->mixer == NULL || dsoundsrc->control_id_mute < 0) {
    GST_WARNING ("mixer not initialized");
    return;
  }

  details.cbStruct = sizeof (details);
  details.dwControlID = dsoundsrc->control_id_mute;
  details.cChannels = dsoundsrc->mixerline_cchannels;
  details.cMultipleItems = 0;

  details_boolean.fValue = mute;
  details.cbDetails = sizeof (MIXERCONTROLDETAILS_BOOLEAN);
  details.paDetails = &details_boolean;

  mmres = mixerSetControlDetails ((HMIXEROBJ) dsoundsrc->mixer,
      &details, MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE);

  if (mmres != MMSYSERR_NOERROR)
    GST_WARNING ("Failed to set mute");
  else
    dsoundsrc->mute = mute;
}

static const gchar *
gst_directsound_src_get_device (GstDirectSoundSrc * dsoundsrc)
{
  return dsoundsrc->device_id;
}

static void
gst_directsound_src_set_device (GstDirectSoundSrc * dsoundsrc,
    const gchar * device_id)
{
  g_free (dsoundsrc->device_id);
  dsoundsrc->device_id = g_strdup (device_id);
}
