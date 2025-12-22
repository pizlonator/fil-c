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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstwasapi2util.h"
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <winternl.h>

GST_DEBUG_CATEGORY_EXTERN (gst_wasapi2_debug);
#define GST_CAT_DEFAULT gst_wasapi2_debug

/* Desktop only defines */
#ifndef KSAUDIO_SPEAKER_MONO
#define KSAUDIO_SPEAKER_MONO            (SPEAKER_FRONT_CENTER)
#endif
#ifndef KSAUDIO_SPEAKER_1POINT1
#define KSAUDIO_SPEAKER_1POINT1         (SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY)
#endif
#ifndef KSAUDIO_SPEAKER_STEREO
#define KSAUDIO_SPEAKER_STEREO          (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)
#endif
#ifndef KSAUDIO_SPEAKER_2POINT1
#define KSAUDIO_SPEAKER_2POINT1         (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_LOW_FREQUENCY)
#endif
#ifndef KSAUDIO_SPEAKER_3POINT0
#define KSAUDIO_SPEAKER_3POINT0         (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER)
#endif
#ifndef KSAUDIO_SPEAKER_3POINT1
#define KSAUDIO_SPEAKER_3POINT1         (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | \
                                         SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY)
#endif
#ifndef KSAUDIO_SPEAKER_QUAD
#define KSAUDIO_SPEAKER_QUAD            (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | \
                                         SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT)
#endif
#define KSAUDIO_SPEAKER_SURROUND        (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | \
                                         SPEAKER_FRONT_CENTER | SPEAKER_BACK_CENTER)
#ifndef KSAUDIO_SPEAKER_5POINT0
#define KSAUDIO_SPEAKER_5POINT0         (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | \
                                         SPEAKER_SIDE_LEFT  | SPEAKER_SIDE_RIGHT)
#endif
#define KSAUDIO_SPEAKER_5POINT1         (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | \
                                         SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | \
                                         SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT)
#ifndef KSAUDIO_SPEAKER_7POINT0
#define KSAUDIO_SPEAKER_7POINT0         (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | \
                                         SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | \
                                         SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT)
#endif
#ifndef KSAUDIO_SPEAKER_7POINT1
#define KSAUDIO_SPEAKER_7POINT1         (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | \
                                         SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | \
                                         SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | \
                                         SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER)
#endif

/* *INDENT-OFF* */
static struct
{
  guint64 wasapi_pos;
  GstAudioChannelPosition gst_pos;
} wasapi_to_gst_pos[] = {
  {SPEAKER_FRONT_LEFT, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT},
  {SPEAKER_FRONT_RIGHT, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT},
  {SPEAKER_FRONT_CENTER, GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER},
  {SPEAKER_LOW_FREQUENCY, GST_AUDIO_CHANNEL_POSITION_LFE1},
  {SPEAKER_BACK_LEFT, GST_AUDIO_CHANNEL_POSITION_REAR_LEFT},
  {SPEAKER_BACK_RIGHT, GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT},
  {SPEAKER_FRONT_LEFT_OF_CENTER,
      GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER},
  {SPEAKER_FRONT_RIGHT_OF_CENTER,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER},
  {SPEAKER_BACK_CENTER, GST_AUDIO_CHANNEL_POSITION_REAR_CENTER},
  /* Enum values diverge from this point onwards */
  {SPEAKER_SIDE_LEFT, GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT},
  {SPEAKER_SIDE_RIGHT, GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT},
  {SPEAKER_TOP_CENTER, GST_AUDIO_CHANNEL_POSITION_TOP_CENTER},
  {SPEAKER_TOP_FRONT_LEFT, GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_LEFT},
  {SPEAKER_TOP_FRONT_CENTER, GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_CENTER},
  {SPEAKER_TOP_FRONT_RIGHT, GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_RIGHT},
  {SPEAKER_TOP_BACK_LEFT, GST_AUDIO_CHANNEL_POSITION_TOP_REAR_LEFT},
  {SPEAKER_TOP_BACK_CENTER, GST_AUDIO_CHANNEL_POSITION_TOP_REAR_CENTER},
  {SPEAKER_TOP_BACK_RIGHT, GST_AUDIO_CHANNEL_POSITION_TOP_REAR_RIGHT}
};

static DWORD default_ch_masks[] = {
  0,
  KSAUDIO_SPEAKER_MONO,
  /* 2ch */
  KSAUDIO_SPEAKER_STEREO,
  /* 2.1ch */
  /* KSAUDIO_SPEAKER_3POINT0 ? */
  KSAUDIO_SPEAKER_2POINT1,
  /* 4ch */
  /* KSAUDIO_SPEAKER_3POINT1 or KSAUDIO_SPEAKER_SURROUND ? */
  KSAUDIO_SPEAKER_QUAD,
  /* 5ch */
  KSAUDIO_SPEAKER_5POINT0,
  /* 5.1ch */
  KSAUDIO_SPEAKER_5POINT1,
  /* 7ch */
  KSAUDIO_SPEAKER_7POINT0,
  /* 7.1ch */
  KSAUDIO_SPEAKER_7POINT1,
};
/* *INDENT-ON* */

static const gchar *
hresult_to_string_fallback (HRESULT hr)
{
  const gchar *s = "unknown error";

  switch (hr) {
    case AUDCLNT_E_NOT_INITIALIZED:
      s = "AUDCLNT_E_NOT_INITIALIZED";
      break;
    case AUDCLNT_E_ALREADY_INITIALIZED:
      s = "AUDCLNT_E_ALREADY_INITIALIZED";
      break;
    case AUDCLNT_E_WRONG_ENDPOINT_TYPE:
      s = "AUDCLNT_E_WRONG_ENDPOINT_TYPE";
      break;
    case AUDCLNT_E_DEVICE_INVALIDATED:
      s = "AUDCLNT_E_DEVICE_INVALIDATED";
      break;
    case AUDCLNT_E_NOT_STOPPED:
      s = "AUDCLNT_E_NOT_STOPPED";
      break;
    case AUDCLNT_E_BUFFER_TOO_LARGE:
      s = "AUDCLNT_E_BUFFER_TOO_LARGE";
      break;
    case AUDCLNT_E_OUT_OF_ORDER:
      s = "AUDCLNT_E_OUT_OF_ORDER";
      break;
    case AUDCLNT_E_UNSUPPORTED_FORMAT:
      s = "AUDCLNT_E_UNSUPPORTED_FORMAT";
      break;
    case AUDCLNT_E_INVALID_DEVICE_PERIOD:
      s = "AUDCLNT_E_INVALID_DEVICE_PERIOD";
      break;
    case AUDCLNT_E_INVALID_SIZE:
      s = "AUDCLNT_E_INVALID_SIZE";
      break;
    case AUDCLNT_E_DEVICE_IN_USE:
      s = "AUDCLNT_E_DEVICE_IN_USE";
      break;
    case AUDCLNT_E_BUFFER_OPERATION_PENDING:
      s = "AUDCLNT_E_BUFFER_OPERATION_PENDING";
      break;
    case AUDCLNT_E_BUFFER_SIZE_ERROR:
      s = "AUDCLNT_E_BUFFER_SIZE_ERROR";
      break;
    case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED:
      s = "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED";
      break;
    case AUDCLNT_E_THREAD_NOT_REGISTERED:
      s = "AUDCLNT_E_THREAD_NOT_REGISTERED";
      break;
    case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED:
      s = "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED";
      break;
    case AUDCLNT_E_ENDPOINT_CREATE_FAILED:
      s = "AUDCLNT_E_ENDPOINT_CREATE_FAILED";
      break;
    case AUDCLNT_E_SERVICE_NOT_RUNNING:
      s = "AUDCLNT_E_SERVICE_NOT_RUNNING";
      break;
    case AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED:
      s = "AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED";
      break;
    case AUDCLNT_E_EXCLUSIVE_MODE_ONLY:
      s = "AUDCLNT_E_EXCLUSIVE_MODE_ONLY";
      break;
    case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL:
      s = "AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL";
      break;
    case AUDCLNT_E_EVENTHANDLE_NOT_SET:
      s = "AUDCLNT_E_EVENTHANDLE_NOT_SET";
      break;
    case AUDCLNT_E_INCORRECT_BUFFER_SIZE:
      s = "AUDCLNT_E_INCORRECT_BUFFER_SIZE";
      break;
    case AUDCLNT_E_CPUUSAGE_EXCEEDED:
      s = "AUDCLNT_E_CPUUSAGE_EXCEEDED";
      break;
    case AUDCLNT_S_BUFFER_EMPTY:
      s = "AUDCLNT_S_BUFFER_EMPTY";
      break;
    case AUDCLNT_S_THREAD_ALREADY_REGISTERED:
      s = "AUDCLNT_S_THREAD_ALREADY_REGISTERED";
      break;
    case AUDCLNT_S_POSITION_STALLED:
      s = "AUDCLNT_S_POSITION_STALLED";
      break;
    case E_POINTER:
      s = "E_POINTER";
      break;
    case E_INVALIDARG:
      s = "E_INVALIDARG";
      break;
  }

  return s;
}

gchar *
gst_wasapi2_util_get_error_message (HRESULT hr)
{
  gchar *error_text = NULL;

  error_text = g_win32_error_message ((gint) hr);
  if (!error_text || strlen (error_text) == 0) {
    g_free (error_text);
    error_text = g_strdup (hresult_to_string_fallback (hr));
  }

  return error_text;
}

gboolean
_gst_wasapi2_result (HRESULT hr, GstDebugCategory * cat, const gchar * file,
    const gchar * function, gint line)
{
#ifndef GST_DISABLE_GST_DEBUG
  gboolean ret = TRUE;

  if (FAILED (hr)) {
    gchar *error_text = NULL;
    gboolean free_string = TRUE;

    error_text = g_win32_error_message ((gint) hr);
    /* g_win32_error_message() seems to be returning empty string for
     * AUDCLNT_* cases */
    if (!error_text || strlen (error_text) == 0) {
      g_free (error_text);
      error_text = (gchar *) hresult_to_string_fallback (hr);

      free_string = FALSE;
    }

    gst_debug_log (cat, GST_LEVEL_WARNING, file, function, line,
        NULL, "WASAPI call failed: 0x%x, %s", (guint) hr, error_text);

    if (free_string)
      g_free (error_text);

    ret = FALSE;
  }

  return ret;
#else
  return SUCCEEDED (hr);
#endif
}

static void
gst_wasapi_util_channel_position_all_none (guint channels,
    GstAudioChannelPosition * position)
{
  guint i;

  for (i = 0; i < channels; i++)
    position[i] = GST_AUDIO_CHANNEL_POSITION_NONE;
}

guint64
gst_wasapi2_util_waveformatex_to_channel_mask (WAVEFORMATEX * format,
    GstAudioChannelPosition ** out_position)
{
  guint i, ch;
  guint64 mask = 0;
  GstAudioChannelPosition *pos = NULL;
  WORD nChannels = 0;
  DWORD dwChannelMask = 0;

  nChannels = format->nChannels;
  if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    WAVEFORMATEXTENSIBLE *extensible = (WAVEFORMATEXTENSIBLE *) format;
    dwChannelMask = extensible->dwChannelMask;
  }

  if (out_position)
    *out_position = NULL;

  if (nChannels > 2 && !dwChannelMask) {
    GST_WARNING ("Unknown channel mask value for %d channel stream", nChannels);

    if (nChannels >= G_N_ELEMENTS (default_ch_masks)) {
      GST_ERROR ("To may channels %d", nChannels);
      return 0;
    }

    dwChannelMask = default_ch_masks[nChannels];
  }

  pos = g_new (GstAudioChannelPosition, nChannels);
  gst_wasapi_util_channel_position_all_none (nChannels, pos);

  /* Too many channels, have to assume that they are all non-positional */
  if (nChannels > G_N_ELEMENTS (wasapi_to_gst_pos)) {
    GST_INFO ("Got too many (%i) channels, assuming non-positional", nChannels);
    goto out;
  }

  /* Too many bits in the channel mask, and the bits don't match nChannels */
  if (dwChannelMask >> (G_N_ELEMENTS (wasapi_to_gst_pos) + 1) != 0) {
    GST_WARNING ("Too many bits in channel mask (%lu), assuming "
        "non-positional", dwChannelMask);
    goto out;
  }

  /* Map WASAPI's channel mask to Gstreamer's channel mask and positions.
   * If the no. of bits in the mask > nChannels, we will ignore the extra. */
  for (i = 0, ch = 0; i < G_N_ELEMENTS (wasapi_to_gst_pos) && ch < nChannels;
      i++) {
    if (!(dwChannelMask & wasapi_to_gst_pos[i].wasapi_pos))
      /* no match, try next */
      continue;
    mask |= G_GUINT64_CONSTANT (1) << wasapi_to_gst_pos[i].gst_pos;
    pos[ch++] = wasapi_to_gst_pos[i].gst_pos;
  }

  /* XXX: Warn if some channel masks couldn't be mapped? */

  GST_DEBUG ("Converted WASAPI mask 0x%" G_GINT64_MODIFIER "x -> 0x%"
      G_GINT64_MODIFIER "x", (guint64) dwChannelMask, (guint64) mask);

out:
  if (out_position) {
    *out_position = pos;
  } else {
    g_free (pos);
  }

  return mask;
}

const gchar *
gst_wasapi2_util_waveformatex_to_audio_format (WAVEFORMATEX * format)
{
  const gchar *fmt_str = NULL;
  GstAudioFormat fmt = GST_AUDIO_FORMAT_UNKNOWN;

  switch (format->wFormatTag) {
    case WAVE_FORMAT_PCM:
      fmt = gst_audio_format_build_integer (TRUE, G_LITTLE_ENDIAN,
          format->wBitsPerSample, format->wBitsPerSample);
      break;
    case WAVE_FORMAT_IEEE_FLOAT:
      if (format->wBitsPerSample == 32)
        fmt = GST_AUDIO_FORMAT_F32LE;
      else if (format->wBitsPerSample == 64)
        fmt = GST_AUDIO_FORMAT_F64LE;
      break;
    case WAVE_FORMAT_EXTENSIBLE:
    {
      WAVEFORMATEXTENSIBLE *ex = (WAVEFORMATEXTENSIBLE *) format;
      if (IsEqualGUID (&ex->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM)) {
        fmt = gst_audio_format_build_integer (TRUE, G_LITTLE_ENDIAN,
            format->wBitsPerSample, ex->Samples.wValidBitsPerSample);
      } else if (IsEqualGUID (&ex->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
        if (format->wBitsPerSample == 32
            && ex->Samples.wValidBitsPerSample == 32)
          fmt = GST_AUDIO_FORMAT_F32LE;
        else if (format->wBitsPerSample == 64 &&
            ex->Samples.wValidBitsPerSample == 64)
          fmt = GST_AUDIO_FORMAT_F64LE;
      }
      break;
    }
    default:
      break;
  }

  if (fmt != GST_AUDIO_FORMAT_UNKNOWN)
    fmt_str = gst_audio_format_to_string (fmt);

  return fmt_str;
}

gboolean
gst_wasapi2_util_parse_waveformatex (WAVEFORMATEX * format,
    GstCaps * template_caps, GstCaps ** out_caps,
    GstAudioChannelPosition ** out_positions)
{
  const gchar *afmt;
  guint64 channel_mask;

  *out_caps = NULL;

  /* TODO: handle SPDIF and other encoded formats */

  /* 1 or 2 channels <= 16 bits sample size OR
   * 1 or 2 channels > 16 bits sample size or >2 channels */
  if (format->wFormatTag != WAVE_FORMAT_PCM &&
      format->wFormatTag != WAVE_FORMAT_IEEE_FLOAT &&
      format->wFormatTag != WAVE_FORMAT_EXTENSIBLE)
    /* Unhandled format tag */
    return FALSE;

  /* WASAPI can only tell us one canonical mix format that it will accept. The
   * alternative is calling IsFormatSupported on all combinations of formats.
   * Instead, it's simpler and faster to require conversion inside gstreamer */
  afmt = gst_wasapi2_util_waveformatex_to_audio_format (format);
  if (afmt == NULL)
    return FALSE;

  *out_caps = gst_caps_copy (template_caps);

  channel_mask = gst_wasapi2_util_waveformatex_to_channel_mask (format,
      out_positions);

  gst_caps_set_simple (*out_caps,
      "format", G_TYPE_STRING, afmt,
      "channels", G_TYPE_INT, format->nChannels,
      "rate", G_TYPE_INT, format->nSamplesPerSec, NULL);

  if (channel_mask) {
    gst_caps_set_simple (*out_caps,
        "channel-mask", GST_TYPE_BITMASK, channel_mask, NULL);
  }

  return TRUE;
}

gboolean
gst_wasapi2_can_automatic_stream_routing (void)
{
#ifdef GST_WASAPI2_WINAPI_ONLY_APP
  /* Assume we are on very recent OS */
  return TRUE;
#else
  static gboolean ret = FALSE;
  static gsize version_once = 0;

  if (g_once_init_enter (&version_once)) {
    OSVERSIONINFOEXW osverinfo;
    typedef NTSTATUS (WINAPI fRtlGetVersion) (PRTL_OSVERSIONINFOEXW);
    fRtlGetVersion *RtlGetVersion = NULL;
    HMODULE hmodule = NULL;

    memset (&osverinfo, 0, sizeof (OSVERSIONINFOEXW));
    osverinfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEXW);

    hmodule = LoadLibraryW (L"ntdll.dll");
    if (hmodule)
      RtlGetVersion =
          (fRtlGetVersion *) GetProcAddress (hmodule, "RtlGetVersion");

    if (RtlGetVersion) {
      RtlGetVersion (&osverinfo);

      /* automatic stream routing requires Windows 10
       * Anniversary Update (version 1607, build number 14393.0) */
      if (osverinfo.dwMajorVersion > 10 ||
          (osverinfo.dwMajorVersion == 10 && osverinfo.dwBuildNumber >= 14393))
        ret = TRUE;
    }

    if (hmodule)
      FreeLibrary (hmodule);

    g_once_init_leave (&version_once, 1);
  }

  GST_INFO ("Automatic stream routing support: %d", ret);

  return ret;
#endif
}

gboolean
gst_wasapi2_can_process_loopback (void)
{
#ifdef GST_WASAPI2_WINAPI_ONLY_APP
  /* FIXME: Needs WinRT (Windows.System.Profile) API call
   * for OS version check */
  return FALSE;
#else
  static gboolean ret = FALSE;
  static gsize version_once = 0;

  if (g_once_init_enter (&version_once)) {
    OSVERSIONINFOEXW osverinfo;
    typedef NTSTATUS (WINAPI fRtlGetVersion) (PRTL_OSVERSIONINFOEXW);
    fRtlGetVersion *RtlGetVersion = NULL;
    HMODULE hmodule = NULL;

    memset (&osverinfo, 0, sizeof (OSVERSIONINFOEXW));
    osverinfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEXW);

    hmodule = LoadLibraryW (L"ntdll.dll");
    if (hmodule)
      RtlGetVersion =
          (fRtlGetVersion *) GetProcAddress (hmodule, "RtlGetVersion");

    if (RtlGetVersion) {
      RtlGetVersion (&osverinfo);

      /* Process loopback requires Windows 10 build 20348
       * https://learn.microsoft.com/en-us/windows/win32/api/audioclientactivationparams/ns-audioclientactivationparams-audioclient_process_loopback_params
       *
       * Note: "Windows 10 build 20348" would mean "Windows server 2022" or
       * "Windows 11", since build number of "Windows 10 version 21H2" is
       * still 19044.XXX
       */

      /* But other software enables this for build number 19041 or higher... */
      if (osverinfo.dwMajorVersion > 10 ||
          (osverinfo.dwMajorVersion == 10 && osverinfo.dwBuildNumber >= 19041))
        ret = TRUE;
    }

    if (hmodule)
      FreeLibrary (hmodule);

    g_once_init_leave (&version_once, 1);
  }

  GST_INFO ("Process loopback support: %d", ret);

  return ret;
#endif
}

WAVEFORMATEX *
gst_wasapi2_get_default_mix_format (void)
{
  WAVEFORMATEX *format;

  /* virtual loopback device might not provide mix format. Create our default
   * mix format */
  format = CoTaskMemAlloc (sizeof (WAVEFORMATEX));
  format->wFormatTag = WAVE_FORMAT_PCM;
  format->nChannels = 2;
  format->nSamplesPerSec = 44100;
  format->wBitsPerSample = 16;
  format->nBlockAlign = format->nChannels * format->wBitsPerSample / 8;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;

  return format;
}
