/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@schleef.org>
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015 Florian Langlois <florian.langlois@fr.thalesgroup.com>
 * Copyright (C) 2020 Sohonet <dev@sohonet.com>
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

#ifndef _GST_DECKLINK_H_
#define _GST_DECKLINK_H_

#include <gst/gst.h>
#include <gst/video/video.h>

#include <stdint.h>

#ifdef G_OS_WIN32
#include "win/DeckLinkAPI.h"

#include <stdio.h>

#define bool BOOL
#define COMSTR_T BSTR
#define CONVERT_COM_STRING(s) G_STMT_START { \
  BSTR _s = (BSTR)s; \
  int _length = ::WideCharToMultiByte(CP_ACP, 0, (wchar_t*)_s, -1, NULL, 0, NULL, NULL); \
  s = (char *) malloc(_length); \
  ::WideCharToMultiByte(CP_ACP, 0, (wchar_t*)_s, -1, s, _length, NULL, NULL); \
  ::SysFreeString(_s); \
} G_STMT_END
#define FREE_COM_STRING(s) free(s);
#define CONVERT_TO_COM_STRING(s) G_STMT_START { \
  char * _s = (char *)s; \
  int _length = ::MultiByteToWideChar(CP_ACP, 0, (char*)_s, -1, NULL, 0); \
  s = ::SysAllocStringLen(NULL, _length); \
  ::MultiByteToWideChar(CP_ACP, 0, (char*)_s, -1, s, _length); \
  g_free(_s); \
} G_STMT_END
#elif defined(__APPLE__)
#include "osx/DeckLinkAPI.h"

#define COMSTR_T CFStringRef
#define CONVERT_COM_STRING(s) G_STMT_START { \
  CFStringRef _s = (CFStringRef)s; \
  CFIndex _length; \
  CFStringGetBytes(_s, CFRangeMake(0, CFStringGetLength(_s)), kCFStringEncodingUTF8, 0, FALSE, NULL, 0, &_length); \
  _length += 1; \
  s = (char *) malloc(_length); \
  CFStringGetCString(_s, s, _length, kCFStringEncodingUTF8); \
  CFRelease(_s); \
} G_STMT_END
#define FREE_COM_STRING(s) free(s);
#define CONVERT_TO_COM_STRING(s) G_STMT_START { \
  char * _s = (char *)s; \
  s = CFStringCreateWithCString(kCFAllocatorDefault, _s, kCFStringEncodingUTF8); \
  g_free(_s); \
} G_STMT_END
#define WINAPI
#else /* Linux */
#include "linux/DeckLinkAPI.h"

#define COMSTR_T const char*
#define CONVERT_COM_STRING(s)
#define CONVERT_TO_COM_STRING(s)
/* While this is a const char*, the string still has to be freed */
#define FREE_COM_STRING(s) free(s);
#define WINAPI
#endif /* G_OS_WIN32 */

void decklink_element_init (GstPlugin * plugin);

typedef enum {
  GST_DECKLINK_MODE_AUTO,

  GST_DECKLINK_MODE_NTSC,
  GST_DECKLINK_MODE_NTSC2398,
  GST_DECKLINK_MODE_PAL,
  GST_DECKLINK_MODE_NTSC_P,
  GST_DECKLINK_MODE_PAL_P,

  GST_DECKLINK_MODE_1080p2398,
  GST_DECKLINK_MODE_1080p24,
  GST_DECKLINK_MODE_1080p25,
  GST_DECKLINK_MODE_1080p2997,
  GST_DECKLINK_MODE_1080p30,

  GST_DECKLINK_MODE_1080i50,
  GST_DECKLINK_MODE_1080i5994,
  GST_DECKLINK_MODE_1080i60,

  GST_DECKLINK_MODE_1080p50,
  GST_DECKLINK_MODE_1080p5994,
  GST_DECKLINK_MODE_1080p60,

  GST_DECKLINK_MODE_720p50,
  GST_DECKLINK_MODE_720p5994,
  GST_DECKLINK_MODE_720p60,

  GST_DECKLINK_MODE_1556p2398,
  GST_DECKLINK_MODE_1556p24,
  GST_DECKLINK_MODE_1556p25,

  GST_DECKLINK_MODE_2KDCI2398,
  GST_DECKLINK_MODE_2KDCI24,
  GST_DECKLINK_MODE_2KDCI25,
  GST_DECKLINK_MODE_2KDCI2997,
  GST_DECKLINK_MODE_2KDCI30,
  GST_DECKLINK_MODE_2KDCI50,
  GST_DECKLINK_MODE_2KDCI5994,
  GST_DECKLINK_MODE_2KDCI60,

  GST_DECKLINK_MODE_2160p2398,
  GST_DECKLINK_MODE_2160p24,
  GST_DECKLINK_MODE_2160p25,
  GST_DECKLINK_MODE_2160p2997,
  GST_DECKLINK_MODE_2160p30,
  GST_DECKLINK_MODE_2160p50,
  GST_DECKLINK_MODE_2160p5994,
  GST_DECKLINK_MODE_2160p60,

  GST_DECKLINK_MODE_NTSC_WIDESCREEN,
  GST_DECKLINK_MODE_NTSC2398_WIDESCREEN,
  GST_DECKLINK_MODE_PAL_WIDESCREEN,
  GST_DECKLINK_MODE_NTSC_P_WIDESCREEN,
  GST_DECKLINK_MODE_PAL_P_WIDESCREEN,

  /**
   * GstDecklinkModes::4kdcip2398:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4Kp2398,
  /**
   * GstDecklinkModes::4kdcip24:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4Kp24,
  /**
   * GstDecklinkModes::4kdcip25:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4Kp25,
  /**
   * GstDecklinkModes::4kdcip2997:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4Kp2997,
  /**
   * GstDecklinkModes::4kdcip30:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4Kp30,
  /**
   * GstDecklinkModes::4kdcip50:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4Kp50,
  /**
   * GstDecklinkModes::4kdcip5994:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4Kp5994,
  /**
   * GstDecklinkModes::4kdcip60:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4Kp60,

  /**
   * GstDecklinkModes::8kp2398:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4320p2398,
  /**
   * GstDecklinkModes::8kp24:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4320p24,
  /**
   * GstDecklinkModes::8kp25:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4320p25,
  /**
   * GstDecklinkModes::8kp2997:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4320p2997,
  /**
   * GstDecklinkModes::8kp30:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4320p30,
  /**
   * GstDecklinkModes::8kp50:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4320p50,
  /**
   * GstDecklinkModes::8kp5994:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4320p5994,
  /**
   * GstDecklinkModes::8kp60:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_4320p60,

  /**
   * GstDecklinkModes::8kdcip2398:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_8Kp2398,
  /**
   * GstDecklinkModes::8kdcip24:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_8Kp24,
  /**
   * GstDecklinkModes::8kdcip25:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_8Kp25,
  /**
   * GstDecklinkModes::8kdcip2997:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_8Kp2997,
  /**
   * GstDecklinkModes::8kdcip30:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_8Kp30,
  /**
   * GstDecklinkModes::8kdcip50:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_8Kp50,
  /**
   * GstDecklinkModes::8kdcip5994:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_8Kp5994,
  /**
   * GstDecklinkModes::8kdcip60:
   *
   * Since: 1.22
   */
  GST_DECKLINK_MODE_8Kp60
} GstDecklinkModeEnum;
#define GST_TYPE_DECKLINK_MODE (gst_decklink_mode_get_type ())
GType gst_decklink_mode_get_type (void);

typedef enum {
  GST_DECKLINK_CONNECTION_AUTO,
  GST_DECKLINK_CONNECTION_SDI,
  GST_DECKLINK_CONNECTION_HDMI,
  GST_DECKLINK_CONNECTION_OPTICAL_SDI,
  GST_DECKLINK_CONNECTION_COMPONENT,
  GST_DECKLINK_CONNECTION_COMPOSITE,
  GST_DECKLINK_CONNECTION_SVIDEO
} GstDecklinkConnectionEnum;
#define GST_TYPE_DECKLINK_CONNECTION (gst_decklink_connection_get_type ())
GType gst_decklink_connection_get_type (void);

typedef enum {
  GST_DECKLINK_AUDIO_CONNECTION_AUTO,
  GST_DECKLINK_AUDIO_CONNECTION_EMBEDDED,
  GST_DECKLINK_AUDIO_CONNECTION_AES_EBU,
  GST_DECKLINK_AUDIO_CONNECTION_ANALOG,
  GST_DECKLINK_AUDIO_CONNECTION_ANALOG_XLR,
  GST_DECKLINK_AUDIO_CONNECTION_ANALOG_RCA
} GstDecklinkAudioConnectionEnum;
#define GST_TYPE_DECKLINK_AUDIO_CONNECTION (gst_decklink_audio_connection_get_type ())
GType gst_decklink_audio_connection_get_type (void);

typedef enum {
  GST_DECKLINK_AUDIO_CHANNELS_MAX = 0,
  GST_DECKLINK_AUDIO_CHANNELS_2 = 2,
  GST_DECKLINK_AUDIO_CHANNELS_8 = 8,
  GST_DECKLINK_AUDIO_CHANNELS_16 = 16
} GstDecklinkAudioChannelsEnum;
#define GST_TYPE_DECKLINK_AUDIO_CHANNELS (gst_decklink_audio_channels_get_type ())
GType gst_decklink_audio_channels_get_type (void);

typedef enum {
  GST_DECKLINK_VIDEO_FORMAT_AUTO,
  GST_DECKLINK_VIDEO_FORMAT_8BIT_YUV, /* bmdFormat8BitYUV */
  GST_DECKLINK_VIDEO_FORMAT_10BIT_YUV, /* bmdFormat10BitYUV */
  GST_DECKLINK_VIDEO_FORMAT_8BIT_ARGB, /* bmdFormat8BitARGB */
  GST_DECKLINK_VIDEO_FORMAT_8BIT_BGRA, /* bmdFormat8BitBGRA */

  /**
   * GstDecklinkVideoFormat::10bit-rgb:
   *
   * Since: 1.22.2
   */
  GST_DECKLINK_VIDEO_FORMAT_10BIT_RGB, /* bmdFormat10BitRGB */
  /* Not yet supported */
#if 0
  GST_DECKLINK_VIDEO_FORMAT_12BIT_RGB, /* bmdFormat12BitRGB */
  GST_DECKLINK_VIDEO_FORMAT_12BIT_RGBLE, /* bmdFormat12BitRGBLE */
  GST_DECKLINK_VIDEO_FORMAT_10BIT_RGBXLE, /* bmdFormat10BitRGBXLE */
  GST_DECKLINK_VIDEO_FORMAT_10BIT_RGBX, /* bmdFormat10BitRGBX */
#endif
} GstDecklinkVideoFormat;
#define GST_TYPE_DECKLINK_VIDEO_FORMAT (gst_decklink_video_format_get_type ())
GType gst_decklink_video_format_get_type (void);

typedef enum {
  GST_DECKLINK_PROFILE_ID_DEFAULT,
  GST_DECKLINK_PROFILE_ID_ONE_SUB_DEVICE_FULL_DUPLEX, /* bmdProfileOneSubDeviceFullDuplex */
  GST_DECKLINK_PROFILE_ID_ONE_SUB_DEVICE_HALF_DUPLEX, /* bmdProfileOneSubDeviceHalfDuplex */
  GST_DECKLINK_PROFILE_ID_TWO_SUB_DEVICES_FULL_DUPLEX, /* bmdProfileTwoSubDevicesFullDuplex */
  GST_DECKLINK_PROFILE_ID_TWO_SUB_DEVICES_HALF_DUPLEX, /* bmdProfileTwoSubDevicesHalfDuplex */
  GST_DECKLINK_PROFILE_ID_FOUR_SUB_DEVICES_HALF_DUPLEX, /* bmdProfileFourSubDevicesHalfDuplex */
} GstDecklinkProfileId;
#define GST_TYPE_DECKLINK_PROFILE_ID (gst_decklink_profile_id_get_type ())
GType gst_decklink_profile_id_get_type (void);

typedef enum {
  GST_DECKLINK_MAPPING_FORMAT_DEFAULT,
  GST_DECKLINK_MAPPING_FORMAT_LEVEL_A, /* bmdDeckLinkConfigSMPTELevelAOutput = true */
  GST_DECKLINK_MAPPING_FORMAT_LEVEL_B, /* bmdDeckLinkConfigSMPTELevelAOutput = false */
} GstDecklinkMappingFormat;
#define GST_TYPE_DECKLINK_MAPPING_FORMAT (gst_decklink_mapping_format_get_type ())
GType gst_decklink_mapping_format_get_type (void);

typedef enum {
  GST_DECKLINK_TIMECODE_FORMAT_RP188VITC1, /*bmdTimecodeRP188VITC1 */
  GST_DECKLINK_TIMECODE_FORMAT_RP188VITC2, /*bmdTimecodeRP188VITC2 */
  GST_DECKLINK_TIMECODE_FORMAT_RP188LTC, /*bmdTimecodeRP188LTC */
  GST_DECKLINK_TIMECODE_FORMAT_RP188ANY, /*bmdTimecodeRP188Any */
  GST_DECKLINK_TIMECODE_FORMAT_VITC, /*bmdTimecodeVITC */
  GST_DECKLINK_TIMECODE_FORMAT_VITCFIELD2, /*bmdTimecodeVITCField2 */
  GST_DECKLINK_TIMECODE_FORMAT_SERIAL /* bmdTimecodeSerial */
} GstDecklinkTimecodeFormat;
#define GST_TYPE_DECKLINK_TIMECODE_FORMAT (gst_decklink_timecode_format_get_type ())
GType gst_decklink_timecode_format_get_type (void);

typedef enum
{
  GST_DECKLINK_KEYER_MODE_OFF,
  GST_DECKLINK_KEYER_MODE_INTERNAL,
  GST_DECKLINK_KEYER_MODE_EXTERNAL
} GstDecklinkKeyerMode;
#define GST_TYPE_DECKLINK_KEYER_MODE (gst_decklink_keyer_mode_get_type ())
GType gst_decklink_keyer_mode_get_type (void);

/* Enum BMDKeyerMode options of off, internal and external @@@ DJ @@@ */

typedef uint32_t BMDKeyerMode;
enum _BMDKeyerMode
{
  bmdKeyerModeOff = /* 'off' */ 0,
  bmdKeyerModeInternal = /* 'int' */ 1,
  bmdKeyerModeExternal = /* 'ext' */ 2
};

const BMDPixelFormat gst_decklink_pixel_format_from_type (GstDecklinkVideoFormat t);
const gint gst_decklink_bpp_from_type (GstDecklinkVideoFormat t);
const GstDecklinkVideoFormat gst_decklink_type_from_video_format (GstVideoFormat f);
GstVideoFormat gst_decklink_video_format_from_type (BMDPixelFormat pf);
const BMDTimecodeFormat gst_decklink_timecode_format_from_enum (GstDecklinkTimecodeFormat f);
const GstDecklinkTimecodeFormat gst_decklink_timecode_format_to_enum (BMDTimecodeFormat f);
const BMDProfileID gst_decklink_profile_id_from_enum (GstDecklinkProfileId p);
const GstDecklinkProfileId gst_decklink_profile_id_to_enum (BMDProfileID p);
const BMDKeyerMode gst_decklink_keyer_mode_from_enum (GstDecklinkKeyerMode m);
const GstDecklinkKeyerMode gst_decklink_keyer_mode_to_enum (BMDKeyerMode m);

typedef struct _GstDecklinkMode GstDecklinkMode;
struct _GstDecklinkMode {
  BMDDisplayMode mode;
  int width;
  int height;
  int fps_n;
  int fps_d;
  gboolean interlaced;
  int par_n;
  int par_d;
  gboolean tff;
  const gchar *colorimetry;
};

const GstDecklinkMode * gst_decklink_get_mode (GstDecklinkModeEnum e);
const GstDecklinkModeEnum gst_decklink_get_mode_enum_from_bmd (BMDDisplayMode mode);
const BMDVideoConnection gst_decklink_get_connection (GstDecklinkConnectionEnum e);
GstCaps * gst_decklink_mode_get_caps (GstDecklinkModeEnum e, BMDPixelFormat f, gboolean input);
GstCaps * gst_decklink_mode_get_template_caps (gboolean input);

typedef struct _GstDecklinkOutput GstDecklinkOutput;
struct _GstDecklinkOutput {
  IDeckLink *device;
  IDeckLinkOutput *output;
  IDeckLinkProfileAttributes *attributes;
  IDeckLinkKeyer *keyer;

  gchar *hw_serial_number;
  gint64 persistent_id;

  GstClock *clock;
  GstClockTime clock_start_time, clock_last_time, clock_epoch;
  GstClockTimeDiff clock_offset;
  gboolean started;
  gboolean clock_restart;

  /* Everything below protected by mutex */
  GMutex lock;
  GCond cond;

  /* Set by the video source */
  /* Configured mode or NULL */
  const GstDecklinkMode *mode;

  GstElement *audiosink;
  gboolean audio_enabled;
  GstElement *videosink;
  gboolean video_enabled;
  void (*start_scheduled_playback) (GstElement *videosink);
};

typedef struct _GstDecklinkInput GstDecklinkInput;
struct _GstDecklinkInput {
  IDeckLink *device;
  IDeckLinkInput *input;
  IDeckLinkConfiguration *config;
  IDeckLinkProfileAttributes *attributes;

  gchar *hw_serial_number;
  gint64 persistent_id;

  /* Everything below protected by mutex */
  GMutex lock;

  /* Set by the video source */
  void (*got_video_frame) (GstElement *videosrc, IDeckLinkVideoInputFrame * frame, GstDecklinkModeEnum mode, GstClockTime capture_time, GstClockTime stream_time, GstClockTime stream_duration, GstClockTime hardware_time, GstClockTime hardware_duration, IDeckLinkTimecode *dtc, gboolean no_signal);
  /* Configured mode or NULL */
  const GstDecklinkMode *mode;
  BMDPixelFormat format;
  gboolean auto_format;

  /* Set by the audio source */
  void (*got_audio_packet) (GstElement *videosrc, IDeckLinkAudioInputPacket * packet, GstClockTime capture_time, GstClockTime stream_time, GstClockTime stream_duration, GstClockTime hardware_time, GstClockTime hardware_duration, gboolean no_signal);

  GstElement *audiosrc;
  gboolean audio_enabled;
  GstElement *videosrc;
  gboolean video_enabled;
  void (*start_streams) (GstElement *videosrc);
};

GstDecklinkOutput * gst_decklink_acquire_nth_output (gint n, gint64 persistent_id, GstElement * sink, gboolean is_audio);
void                gst_decklink_release_nth_output (gint n, gint64 persistent_id, GstElement * sink, gboolean is_audio);

GstDecklinkInput *  gst_decklink_acquire_nth_input (gint n, gint64 persistent_id, GstElement * src, gboolean is_audio);
void                gst_decklink_release_nth_input (gint n, gint64 persistent_id, GstElement * src, gboolean is_audio);

const GstDecklinkMode * gst_decklink_find_mode_for_caps (GstCaps * caps);
const GstDecklinkMode * gst_decklink_find_mode_and_format_for_caps (GstCaps * caps, BMDPixelFormat * format);
GstCaps * gst_decklink_mode_get_caps_all_formats (GstDecklinkModeEnum e, gboolean input);
GstCaps * gst_decklink_pixel_format_get_caps (BMDPixelFormat f, gboolean input);

#define GST_TYPE_DECKLINK_DEVICE gst_decklink_device_get_type()
#define GST_DECKLINK_DEVICE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DECKLINK_DEVICE,GstDecklinkDevice))

typedef struct _GstDecklinkDevice GstDecklinkDevice;
typedef struct _GstDecklinkDeviceClass GstDecklinkDeviceClass;

struct _GstDecklinkDeviceClass
{
  GstDeviceClass parent_class;
};

struct _GstDecklinkDevice
{
  GstDevice parent;
  gboolean video;
  gboolean capture;
  gint64 persistent_id;
};

GType gst_decklink_device_get_type (void);

GList * gst_decklink_get_devices (void);

#endif
