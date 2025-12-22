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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "gstdecklink.h"
#include "gstdecklinkaudiosink.h"
#include "gstdecklinkvideosink.h"
#include "gstdecklinkaudiosrc.h"
#include "gstdecklinkvideosrc.h"
#include "gstdecklinkdeviceprovider.h"

GST_DEBUG_CATEGORY_STATIC (gst_decklink_debug);
#define GST_CAT_DEFAULT gst_decklink_debug
#define DEFAULT_PERSISTENT_ID (-1)

GType
gst_decklink_mode_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue modes[] = {
    {GST_DECKLINK_MODE_AUTO, "Automatic detection", "auto"},

    {GST_DECKLINK_MODE_NTSC, "NTSC SD 60i", "ntsc"},
    {GST_DECKLINK_MODE_NTSC2398, "NTSC SD 60i (24 fps)", "ntsc2398"},
    {GST_DECKLINK_MODE_PAL, "PAL SD 50i", "pal"},
    {GST_DECKLINK_MODE_NTSC_P, "NTSC SD 60p", "ntsc-p"},
    {GST_DECKLINK_MODE_PAL_P, "PAL SD 50p", "pal-p"},

    {GST_DECKLINK_MODE_1080p2398, "HD1080 23.98p", "1080p2398"},
    {GST_DECKLINK_MODE_1080p24, "HD1080 24p", "1080p24"},
    {GST_DECKLINK_MODE_1080p25, "HD1080 25p", "1080p25"},
    {GST_DECKLINK_MODE_1080p2997, "HD1080 29.97p", "1080p2997"},
    {GST_DECKLINK_MODE_1080p30, "HD1080 30p", "1080p30"},

    {GST_DECKLINK_MODE_1080i50, "HD1080 50i", "1080i50"},
    {GST_DECKLINK_MODE_1080i5994, "HD1080 59.94i", "1080i5994"},
    {GST_DECKLINK_MODE_1080i60, "HD1080 60i", "1080i60"},

    {GST_DECKLINK_MODE_1080p50, "HD1080 50p", "1080p50"},
    {GST_DECKLINK_MODE_1080p5994, "HD1080 59.94p", "1080p5994"},
    {GST_DECKLINK_MODE_1080p60, "HD1080 60p", "1080p60"},

    {GST_DECKLINK_MODE_720p50, "HD720 50p", "720p50"},
    {GST_DECKLINK_MODE_720p5994, "HD720 59.94p", "720p5994"},
    {GST_DECKLINK_MODE_720p60, "HD720 60p", "720p60"},

    {GST_DECKLINK_MODE_1556p2398, "2k 23.98p", "1556p2398"},
    {GST_DECKLINK_MODE_1556p24, "2k 24p", "1556p24"},
    {GST_DECKLINK_MODE_1556p25, "2k 25p", "1556p25"},

    {GST_DECKLINK_MODE_2KDCI2398, "2k dci 23.98p", "2kdcip2398"},
    {GST_DECKLINK_MODE_2KDCI24, "2k dci 24p", "2kdcip24"},
    {GST_DECKLINK_MODE_2KDCI25, "2k dci 25p", "2kdcip25"},
    {GST_DECKLINK_MODE_2KDCI2997, "2k dci 29.97p", "2kdcip2997"},
    {GST_DECKLINK_MODE_2KDCI30, "2k dci 30p", "2kdcip30"},
    {GST_DECKLINK_MODE_2KDCI50, "2k dci 50p", "2kdcip50"},
    {GST_DECKLINK_MODE_2KDCI5994, "2k dci 59.94p", "2kdcip5994"},
    {GST_DECKLINK_MODE_2KDCI60, "2k dci 60p", "2kdcip60"},

    {GST_DECKLINK_MODE_2160p2398, "4k 23.98p", "2160p2398"},
    {GST_DECKLINK_MODE_2160p24, "4k 24p", "2160p24"},
    {GST_DECKLINK_MODE_2160p25, "4k 25p", "2160p25"},
    {GST_DECKLINK_MODE_2160p2997, "4k 29.97p", "2160p2997"},
    {GST_DECKLINK_MODE_2160p30, "4k 30p", "2160p30"},
    {GST_DECKLINK_MODE_2160p50, "4k 50p", "2160p50"},
    {GST_DECKLINK_MODE_2160p5994, "4k 59.94p", "2160p5994"},
    {GST_DECKLINK_MODE_2160p60, "4k 60p", "2160p60"},

    {GST_DECKLINK_MODE_NTSC_WIDESCREEN, "NTSC SD 60i Widescreen",
        "ntsc-widescreen"},
    {GST_DECKLINK_MODE_NTSC2398_WIDESCREEN, "NTSC SD 60i Widescreen (24 fps)",
        "ntsc2398-widescreen"},
    {GST_DECKLINK_MODE_PAL_WIDESCREEN, "PAL SD 50i Widescreen",
        "pal-widescreen"},
    {GST_DECKLINK_MODE_NTSC_P_WIDESCREEN, "NTSC SD 60p Widescreen",
        "ntsc-p-widescreen"},
    {GST_DECKLINK_MODE_PAL_P_WIDESCREEN, "PAL SD 50p Widescreen",
        "pal-p-widescreen"},

    {GST_DECKLINK_MODE_4Kp2398, "4k dci 23.98p", "4kdcip2398"},
    {GST_DECKLINK_MODE_4Kp24, "4k dci 24p", "4kdcip24"},
    {GST_DECKLINK_MODE_4Kp25, "4k dci 25p", "4kdcip25"},
    {GST_DECKLINK_MODE_4Kp2997, "4k dci 29.97p", "4kdcip2997"},
    {GST_DECKLINK_MODE_4Kp30, "4k dci 30p", "4kdcip30"},
    {GST_DECKLINK_MODE_4Kp50, "4k dci 50p", "4kdcip50"},
    {GST_DECKLINK_MODE_4Kp5994, "4k dci 59.94p", "4kdcip5994"},
    {GST_DECKLINK_MODE_4Kp60, "4k dci 60p", "4kdcip60"},

    {GST_DECKLINK_MODE_4320p2398, "8k 23.98p", "8kp2398"},
    {GST_DECKLINK_MODE_4320p24, "8k 24p", "8kp24"},
    {GST_DECKLINK_MODE_4320p25, "8k 25p", "8kp25"},
    {GST_DECKLINK_MODE_4320p2997, "8k 29.97p", "8kp2997"},
    {GST_DECKLINK_MODE_4320p30, "8k 30p", "8kp30"},
    {GST_DECKLINK_MODE_4320p50, "8k 50p", "8kp50"},
    {GST_DECKLINK_MODE_4320p5994, "8k 59.94p", "8kp5994"},
    {GST_DECKLINK_MODE_4320p60, "8k 60p", "8kp60"},

    {GST_DECKLINK_MODE_8Kp2398, "8k dci 23.98p", "8kdcip2398"},
    {GST_DECKLINK_MODE_8Kp24, "8k dci 24p", "8kdcip24"},
    {GST_DECKLINK_MODE_8Kp25, "8k dci 25p", "8kdcip25"},
    {GST_DECKLINK_MODE_8Kp2997, "8k dci 29.97p", "8kdcip2997"},
    {GST_DECKLINK_MODE_8Kp30, "8k dci 30p", "8kdcip30"},
    {GST_DECKLINK_MODE_8Kp50, "8k dci 50p", "8kdcip50"},
    {GST_DECKLINK_MODE_8Kp5994, "8k dci 59.94p", "8kdcip5994"},
    {GST_DECKLINK_MODE_8Kp60, "8k dci 60p", "8kdcip60"},

    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstDecklinkModes", modes);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

GType
gst_decklink_connection_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue connections[] = {
    {GST_DECKLINK_CONNECTION_AUTO, "Auto", "auto"},
    {GST_DECKLINK_CONNECTION_SDI, "SDI", "sdi"},
    {GST_DECKLINK_CONNECTION_HDMI, "HDMI", "hdmi"},
    {GST_DECKLINK_CONNECTION_OPTICAL_SDI, "Optical SDI", "optical-sdi"},
    {GST_DECKLINK_CONNECTION_COMPONENT, "Component", "component"},
    {GST_DECKLINK_CONNECTION_COMPOSITE, "Composite", "composite"},
    {GST_DECKLINK_CONNECTION_SVIDEO, "S-Video", "svideo"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstDecklinkConnection", connections);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

GType
gst_decklink_video_format_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue types[] = {
    {GST_DECKLINK_VIDEO_FORMAT_AUTO, "Auto", "auto"},
    {GST_DECKLINK_VIDEO_FORMAT_8BIT_YUV, "bmdFormat8BitYUV", "8bit-yuv"},
    {GST_DECKLINK_VIDEO_FORMAT_10BIT_YUV, "bmdFormat10BitYUV", "10bit-yuv"},
    {GST_DECKLINK_VIDEO_FORMAT_8BIT_ARGB, "bmdFormat8BitARGB", "8bit-argb"},
    {GST_DECKLINK_VIDEO_FORMAT_8BIT_BGRA, "bmdFormat8BitBGRA", "8bit-bgra"},
    {GST_DECKLINK_VIDEO_FORMAT_10BIT_RGB, "bmdFormat10BitRGB", "10bit-rgb"},
    /* Not yet supported:
       {GST_DECKLINK_VIDEO_FORMAT_12BIT_RGB, "bmdFormat12BitRGB", "12bit-rgb"},
       {GST_DECKLINK_VIDEO_FORMAT_12BIT_RGBLE, "bmdFormat12BitRGBLE", "12bit-rgble"},
       {GST_DECKLINK_VIDEO_FORMAT_10BIT_RGBXLE, "bmdFormat10BitRGBXLE", "10bit-rgbxle"},
       {GST_DECKLINK_VIDEO_FORMAT_10BIT_RGBX, "bmdFormat10BitRGBX", "10bit-rgbx"},
     */
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstDecklinkVideoFormat", types);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

/**
 * GstDecklinkProfileId:
 * @GST_DECKLINK_PROFILE_ID_DEFAULT: Don't change the profile
 * @GST_DECKLINK_PROFILE_ID_ONE_SUB_DEVICE_FULL_DUPLEX: Equivalent to bmdProfileOneSubDeviceFullDuplex
 * @GST_DECKLINK_PROFILE_ID_ONE_SUB_DEVICE_HALF_DUPLEX: Equivalent to bmdProfileOneSubDeviceHalfDuplex
 * @GST_DECKLINK_PROFILE_ID_TWO_SUB_DEVICES_FULL_DUPLEX: Equivalent to bmdProfileTwoSubDevicesFullDuplex
 * @GST_DECKLINK_PROFILE_ID_TWO_SUB_DEVICES_HALF_DUPLEX: Equivalent to bmdProfileTwoSubDevicesHalfDuplex
 * @GST_DECKLINK_PROFILE_ID_FOUR_SUB_DEVICES_HALF_DUPLEX: Equivalent to bmdProfileFourSubDevicesHalfDuplex
 *
 * Decklink Profile ID
 *
 * Since: 1.20
 */
GType
gst_decklink_profile_id_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue types[] = {
    {GST_DECKLINK_PROFILE_ID_DEFAULT, "Default, don't change profile",
        "default"},
    {GST_DECKLINK_PROFILE_ID_ONE_SUB_DEVICE_FULL_DUPLEX,
        "One sub-device, Full-Duplex", "one-sub-device-full"},
    {GST_DECKLINK_PROFILE_ID_ONE_SUB_DEVICE_HALF_DUPLEX,
        "One sub-device, Half-Duplex", "one-sub-device-half"},
    {GST_DECKLINK_PROFILE_ID_TWO_SUB_DEVICES_FULL_DUPLEX,
        "Two sub-devices, Full-Duplex", "two-sub-devices-full"},
    {GST_DECKLINK_PROFILE_ID_TWO_SUB_DEVICES_HALF_DUPLEX,
        "Two sub-devices, Half-Duplex", "two-sub-devices-half"},
    {GST_DECKLINK_PROFILE_ID_FOUR_SUB_DEVICES_HALF_DUPLEX,
        "Four sub-devices, Half-Duplex", "four-sub-devices-half"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstDecklinkProfileId", types);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

/**
 * GstDecklinkMappingFormat:
 * @GST_DECKLINK_MAPPING_FORMAT_DEFAULT: Don't change the mapping format
 * @GST_DECKLINK_MAPPING_FORMAT_LEVEL_A: Level A
 * @GST_DECKLINK_MAPPING_FORMAT_LEVEL_B: Level B
 *
 * 3G-SDI mapping format (SMPTE ST 425-1:2017)
 *
 * Since: 1.22
 */
GType
gst_decklink_mapping_format_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue mappingformats[] = {
    {GST_DECKLINK_MAPPING_FORMAT_DEFAULT, "Default, don't change mapping format",
      "default"},
    {GST_DECKLINK_MAPPING_FORMAT_LEVEL_A, "Level A", "level-a"},
    {GST_DECKLINK_MAPPING_FORMAT_LEVEL_B, "Level B", "level-b"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstDecklinkMappingFormat", mappingformats);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

GType
gst_decklink_timecode_format_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue timecodeformats[] = {
    {GST_DECKLINK_TIMECODE_FORMAT_RP188VITC1, "bmdTimecodeRP188VITC1",
        "rp188vitc1"},
    {GST_DECKLINK_TIMECODE_FORMAT_RP188VITC2, "bmdTimecodeRP188VITC2",
        "rp188vitc2"},
    {GST_DECKLINK_TIMECODE_FORMAT_RP188LTC, "bmdTimecodeRP188LTC", "rp188ltc"},
    {GST_DECKLINK_TIMECODE_FORMAT_RP188ANY, "bmdTimecodeRP188Any", "rp188any"},
    {GST_DECKLINK_TIMECODE_FORMAT_VITC, "bmdTimecodeVITC", "vitc"},
    {GST_DECKLINK_TIMECODE_FORMAT_VITCFIELD2, "bmdTimecodeVITCField2",
        "vitcfield2"},
    {GST_DECKLINK_TIMECODE_FORMAT_SERIAL, "bmdTimecodeSerial", "serial"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp =
        g_enum_register_static ("GstDecklinkTimecodeFormat", timecodeformats);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

GType
gst_decklink_keyer_mode_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue keyermodes[] = {
    {GST_DECKLINK_KEYER_MODE_OFF, "Off", "off"},
    {GST_DECKLINK_KEYER_MODE_INTERNAL, "Internal", "internal"},
    {GST_DECKLINK_KEYER_MODE_EXTERNAL, "External", "external"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstDecklinkKeyerMode", keyermodes);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

GType
gst_decklink_audio_connection_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue connections[] = {
    {GST_DECKLINK_AUDIO_CONNECTION_AUTO, "Automatic", "auto"},
    {GST_DECKLINK_AUDIO_CONNECTION_EMBEDDED, "SDI/HDMI embedded audio",
        "embedded"},
    {GST_DECKLINK_AUDIO_CONNECTION_AES_EBU, "AES/EBU input", "aes"},
    {GST_DECKLINK_AUDIO_CONNECTION_ANALOG, "Analog input", "analog"},
    {GST_DECKLINK_AUDIO_CONNECTION_ANALOG_XLR, "Analog input (XLR)",
        "analog-xlr"},
    {GST_DECKLINK_AUDIO_CONNECTION_ANALOG_RCA, "Analog input (RCA)",
        "analog-rca"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp =
        g_enum_register_static ("GstDecklinkAudioConnection", connections);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

GType
gst_decklink_audio_channels_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue connections[] = {
    {GST_DECKLINK_AUDIO_CHANNELS_2, "2 Channels", "2"},
    {GST_DECKLINK_AUDIO_CHANNELS_8, "8 Channels", "8"},
    {GST_DECKLINK_AUDIO_CHANNELS_16, "16 Channels", "16"},
    {GST_DECKLINK_AUDIO_CHANNELS_MAX, "Maximum channels supported", "max"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp =
        g_enum_register_static ("GstDecklinkAudioChannels", connections);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

#define NTSC 10, 11, false, "bt601"
#define PAL 12, 11, true, "bt601"
#define NTSC_WS 40, 33, false, "bt601"
#define PAL_WS 16, 11, true, "bt601"
#define HD 1, 1, true, "bt709"
#define UHD 1, 1, true, "bt2020"

static const GstDecklinkMode modes[] = {
  {bmdModeNTSC, 720, 486, 30000, 1001, true, NTSC},     // default is ntsc

  {bmdModeNTSC, 720, 486, 30000, 1001, true, NTSC},
  {bmdModeNTSC2398, 720, 486, 24000, 1001, true, NTSC},
  {bmdModePAL, 720, 576, 25, 1, true, PAL},
  {bmdModeNTSCp, 720, 486, 30000, 1001, false, NTSC},
  {bmdModePALp, 720, 576, 25, 1, false, PAL},

  {bmdModeHD1080p2398, 1920, 1080, 24000, 1001, false, HD},
  {bmdModeHD1080p24, 1920, 1080, 24, 1, false, HD},
  {bmdModeHD1080p25, 1920, 1080, 25, 1, false, HD},
  {bmdModeHD1080p2997, 1920, 1080, 30000, 1001, false, HD},
  {bmdModeHD1080p30, 1920, 1080, 30, 1, false, HD},

  {bmdModeHD1080i50, 1920, 1080, 25, 1, true, HD},
  {bmdModeHD1080i5994, 1920, 1080, 30000, 1001, true, HD},
  {bmdModeHD1080i6000, 1920, 1080, 30, 1, true, HD},

  {bmdModeHD1080p50, 1920, 1080, 50, 1, false, HD},
  {bmdModeHD1080p5994, 1920, 1080, 60000, 1001, false, HD},
  {bmdModeHD1080p6000, 1920, 1080, 60, 1, false, HD},

  {bmdModeHD720p50, 1280, 720, 50, 1, false, HD},
  {bmdModeHD720p5994, 1280, 720, 60000, 1001, false, HD},
  {bmdModeHD720p60, 1280, 720, 60, 1, false, HD},

  {bmdMode2k2398, 2048, 1556, 24000, 1001, false, HD},
  {bmdMode2k24, 2048, 1556, 24, 1, false, HD},
  {bmdMode2k25, 2048, 1556, 25, 1, false, HD},

  {bmdMode2kDCI2398, 2048, 1080, 24000, 1001, false, HD},
  {bmdMode2kDCI24, 2048, 1080, 24, 1, false, HD},
  {bmdMode2kDCI25, 2048, 1080, 25, 1, false, HD},
  {bmdMode2kDCI2997, 2048, 1080, 30000, 1001, false, HD},
  {bmdMode2kDCI30, 2048, 1080, 30, 1, false, HD},
  {bmdMode2kDCI50, 2048, 1080, 50, 1, false, HD},
  {bmdMode2kDCI5994, 2048, 1080, 60000, 1001, false, HD},
  {bmdMode2kDCI60, 2048, 1080, 60, 1, false, HD},

  {bmdMode4K2160p2398, 3840, 2160, 24000, 1001, false, UHD},
  {bmdMode4K2160p24, 3840, 2160, 24, 1, false, UHD},
  {bmdMode4K2160p25, 3840, 2160, 25, 1, false, UHD},
  {bmdMode4K2160p2997, 3840, 2160, 30000, 1001, false, UHD},
  {bmdMode4K2160p30, 3840, 2160, 30, 1, false, UHD},
  {bmdMode4K2160p50, 3840, 2160, 50, 1, false, UHD},
  {bmdMode4K2160p5994, 3840, 2160, 60000, 1001, false, UHD},
  {bmdMode4K2160p60, 3840, 2160, 60, 1, false, UHD},

  {bmdModeNTSC, 720, 486, 30000, 1001, true, NTSC_WS},
  {bmdModeNTSC2398, 720, 486, 24000, 1001, true, NTSC_WS},
  {bmdModePAL, 720, 576, 25, 1, true, PAL_WS},
  {bmdModeNTSCp, 720, 486, 30000, 1001, false, NTSC_WS},
  {bmdModePALp, 720, 576, 25, 1, false, PAL_WS},

  {bmdMode4kDCI2398, 4096, 2160, 24000, 1001, false, UHD},
  {bmdMode4kDCI24, 4096, 2160, 24, 1, false, UHD},
  {bmdMode4kDCI25, 4096, 2160, 25, 1, false, UHD},
  {bmdMode4kDCI2997, 4096, 2160, 30000, 1001, false, UHD},
  {bmdMode4kDCI30, 4096, 2160, 30, 1, false, UHD},
  {bmdMode4kDCI50, 4096, 2160, 50, 1, false, UHD},
  {bmdMode4kDCI5994, 4096, 2160, 60000, 1001, false, UHD},
  {bmdMode4kDCI60, 4096, 2160, 60, 1, false, UHD},

  {bmdMode8K4320p2398, 7680, 4320, 24000, 1001, false, UHD},
  {bmdMode8K4320p24, 7680, 4320, 24, 1, false, UHD},
  {bmdMode8K4320p25, 7680, 4320, 25, 1, false, UHD},
  {bmdMode8K4320p2997, 7680, 4320, 30000, 1001, false, UHD},
  {bmdMode8K4320p30, 7680, 4320, 30, 1, false, UHD},
  {bmdMode8K4320p50, 7680, 4320, 50, 1, false, UHD},
  {bmdMode8K4320p5994, 7680, 4320, 60000, 1001, false, UHD},
  {bmdMode8K4320p60, 7680, 4320, 60, 1, false, UHD},

  {bmdMode8kDCI2398, 8192, 4320, 24000, 1001, false, UHD},
  {bmdMode8kDCI24, 8192, 4320, 24, 1, false, UHD},
  {bmdMode8kDCI25, 8192, 4320, 25, 1, false, UHD},
  {bmdMode8kDCI2997, 8192, 4320, 30000, 1001, false, UHD},
  {bmdMode8kDCI30, 8192, 4320, 30, 1, false, UHD},
  {bmdMode8kDCI50, 8192, 4320, 50, 1, false, UHD},
  {bmdMode8kDCI5994, 8192, 4320, 60000, 1001, false, UHD},
  {bmdMode8kDCI60, 8192, 4320, 60, 1, false, UHD},
};

static const struct
{
  BMDPixelFormat format;
  gint bpp;
  GstVideoFormat vformat;
} formats[] = {
  /* *INDENT-OFF* */
  {bmdFormat8BitYUV, 2, GST_VIDEO_FORMAT_UYVY},  /* auto */
  {bmdFormat8BitYUV, 2, GST_VIDEO_FORMAT_UYVY},
  {bmdFormat10BitYUV, 4, GST_VIDEO_FORMAT_v210},
  {bmdFormat8BitARGB, 4, GST_VIDEO_FORMAT_ARGB},
  {bmdFormat8BitBGRA, 4, GST_VIDEO_FORMAT_BGRA},
  {bmdFormat10BitRGB, 4, GST_VIDEO_FORMAT_r210},
/* Not yet supported
  {bmdFormat12BitRGB, FIXME, FIXME},
  {bmdFormat12BitRGBLE, FIXME, FIXME},
  {bmdFormat10BitRGBXLE, FIXME, FIXME},
  {bmdFormat10BitRGBX, FIXME, FIXME} */
  /* *INDENT-ON* */
};

enum ProfileSetOperationResult
{
  PROFILE_SET_UNSUPPORTED,
  PROFILE_SET_SUCCESS,
  PROFILE_SET_FAILURE
};

enum MappingFormatSetOperationResult
{
  MAPPING_FORMAT_SET_UNSUPPORTED,
  MAPPING_FORMAT_SET_SUCCESS,
  MAPPING_FORMAT_SET_FAILURE
};

enum DuplexModeSetOperationResult
{
  DUPLEX_MODE_SET_UNSUPPORTED,
  DUPLEX_MODE_SET_SUCCESS,
  DUPLEX_MODE_SET_FAILURE
};

static const struct
{
  BMDTimecodeFormat format;
  GstDecklinkTimecodeFormat gstformat;
} tcformats[] = {
  /* *INDENT-OFF* */
  {bmdTimecodeRP188VITC1, GST_DECKLINK_TIMECODE_FORMAT_RP188VITC1},
  {bmdTimecodeRP188VITC2, GST_DECKLINK_TIMECODE_FORMAT_RP188VITC2},
  {bmdTimecodeRP188LTC, GST_DECKLINK_TIMECODE_FORMAT_RP188LTC},
  {bmdTimecodeRP188Any, GST_DECKLINK_TIMECODE_FORMAT_RP188ANY},
  {bmdTimecodeVITC, GST_DECKLINK_TIMECODE_FORMAT_VITC},
  {bmdTimecodeVITCField2, GST_DECKLINK_TIMECODE_FORMAT_VITCFIELD2},
  {bmdTimecodeSerial, GST_DECKLINK_TIMECODE_FORMAT_SERIAL}
  /* *INDENT-ON* */
};

static const struct
{
  BMDKeyerMode keymode;
  GstDecklinkKeyerMode gstkeymode;
} kmodes[] = {
  /* *INDENT-OFF* */
  {bmdKeyerModeOff, GST_DECKLINK_KEYER_MODE_OFF},
  {bmdKeyerModeInternal, GST_DECKLINK_KEYER_MODE_INTERNAL},
  {bmdKeyerModeExternal, GST_DECKLINK_KEYER_MODE_EXTERNAL}
  /* *INDENT-ON* */
};

const GstDecklinkMode *
gst_decklink_get_mode (GstDecklinkModeEnum e)
{
  if (e < GST_DECKLINK_MODE_AUTO || e > GST_DECKLINK_MODE_8Kp60)
    return NULL;
  return &modes[e];
}

const GstDecklinkModeEnum
gst_decklink_get_mode_enum_from_bmd (BMDDisplayMode mode)
{
  GstDecklinkModeEnum displayMode = GST_DECKLINK_MODE_NTSC;
  switch (mode) {
    case bmdModeNTSC:
      displayMode = GST_DECKLINK_MODE_NTSC;
      break;
    case bmdModeNTSC2398:
      displayMode = GST_DECKLINK_MODE_NTSC2398;
      break;
    case bmdModePAL:
      displayMode = GST_DECKLINK_MODE_PAL;
      break;
    case bmdModeNTSCp:
      displayMode = GST_DECKLINK_MODE_NTSC_P;
      break;
    case bmdModePALp:
      displayMode = GST_DECKLINK_MODE_PAL_P;
      break;
    case bmdModeHD1080p2398:
      displayMode = GST_DECKLINK_MODE_1080p2398;
      break;
    case bmdModeHD1080p24:
      displayMode = GST_DECKLINK_MODE_1080p24;
      break;
    case bmdModeHD1080p25:
      displayMode = GST_DECKLINK_MODE_1080p25;
      break;
    case bmdModeHD1080p2997:
      displayMode = GST_DECKLINK_MODE_1080p2997;
      break;
    case bmdModeHD1080p30:
      displayMode = GST_DECKLINK_MODE_1080p30;
      break;
    case bmdModeHD1080i50:
      displayMode = GST_DECKLINK_MODE_1080i50;
      break;
    case bmdModeHD1080i5994:
      displayMode = GST_DECKLINK_MODE_1080i5994;
      break;
    case bmdModeHD1080i6000:
      displayMode = GST_DECKLINK_MODE_1080i60;
      break;
    case bmdModeHD1080p50:
      displayMode = GST_DECKLINK_MODE_1080p50;
      break;
    case bmdModeHD1080p5994:
      displayMode = GST_DECKLINK_MODE_1080p5994;
      break;
    case bmdModeHD1080p6000:
      displayMode = GST_DECKLINK_MODE_1080p60;
      break;
    case bmdModeHD720p50:
      displayMode = GST_DECKLINK_MODE_720p50;
      break;
    case bmdModeHD720p5994:
      displayMode = GST_DECKLINK_MODE_720p5994;
      break;
    case bmdModeHD720p60:
      displayMode = GST_DECKLINK_MODE_720p60;
      break;
    case bmdMode2k2398:
      displayMode = GST_DECKLINK_MODE_1556p2398;
      break;
    case bmdMode2k24:
      displayMode = GST_DECKLINK_MODE_1556p24;
      break;
    case bmdMode2k25:
      displayMode = GST_DECKLINK_MODE_1556p25;
      break;
    case bmdMode2kDCI2398:
      displayMode = GST_DECKLINK_MODE_2KDCI2398;
      break;
    case bmdMode2kDCI24:
      displayMode = GST_DECKLINK_MODE_2KDCI24;
      break;
    case bmdMode2kDCI25:
      displayMode = GST_DECKLINK_MODE_2KDCI25;
      break;
    case bmdMode2kDCI2997:
      displayMode = GST_DECKLINK_MODE_2KDCI2997;
      break;
    case bmdMode2kDCI30:
      displayMode = GST_DECKLINK_MODE_2KDCI30;
      break;
    case bmdMode2kDCI50:
      displayMode = GST_DECKLINK_MODE_2KDCI50;
      break;
    case bmdMode2kDCI5994:
      displayMode = GST_DECKLINK_MODE_2KDCI5994;
      break;
    case bmdMode2kDCI60:
      displayMode = GST_DECKLINK_MODE_2KDCI60;
      break;
    case bmdMode4K2160p2398:
      displayMode = GST_DECKLINK_MODE_2160p2398;
      break;
    case bmdMode4K2160p24:
      displayMode = GST_DECKLINK_MODE_2160p24;
      break;
    case bmdMode4K2160p25:
      displayMode = GST_DECKLINK_MODE_2160p25;
      break;
    case bmdMode4K2160p2997:
      displayMode = GST_DECKLINK_MODE_2160p2997;
      break;
    case bmdMode4K2160p30:
      displayMode = GST_DECKLINK_MODE_2160p30;
      break;
    case bmdMode4K2160p50:
      displayMode = GST_DECKLINK_MODE_2160p50;
      break;
    case bmdMode4K2160p5994:
      displayMode = GST_DECKLINK_MODE_2160p5994;
      break;
    case bmdMode4K2160p60:
      displayMode = GST_DECKLINK_MODE_2160p60;
      break;
    case bmdMode4kDCI2398:
      displayMode = GST_DECKLINK_MODE_4Kp2398;
      break;
    case bmdMode4kDCI24:
      displayMode = GST_DECKLINK_MODE_4Kp24;
      break;
    case bmdMode4kDCI25:
      displayMode = GST_DECKLINK_MODE_4Kp25;
      break;
    case bmdMode4kDCI2997:
      displayMode = GST_DECKLINK_MODE_4Kp2997;
      break;
    case bmdMode4kDCI30:
      displayMode = GST_DECKLINK_MODE_4Kp30;
      break;
    case bmdMode4kDCI50:
      displayMode = GST_DECKLINK_MODE_4Kp50;
      break;
    case bmdMode4kDCI5994:
      displayMode = GST_DECKLINK_MODE_4Kp5994;
      break;
    case bmdMode4kDCI60:
      displayMode = GST_DECKLINK_MODE_4Kp60;
      break;
    case bmdMode8K4320p2398:
      displayMode = GST_DECKLINK_MODE_4320p2398;
      break;
    case bmdMode8K4320p24:
      displayMode = GST_DECKLINK_MODE_4320p24;
      break;
    case bmdMode8K4320p25:
      displayMode = GST_DECKLINK_MODE_4320p25;
      break;
    case bmdMode8K4320p2997:
      displayMode = GST_DECKLINK_MODE_4320p2997;
      break;
    case bmdMode8K4320p30:
      displayMode = GST_DECKLINK_MODE_4320p30;
      break;
    case bmdMode8K4320p50:
      displayMode = GST_DECKLINK_MODE_4320p50;
      break;
    case bmdMode8K4320p5994:
      displayMode = GST_DECKLINK_MODE_4320p5994;
      break;
    case bmdMode8K4320p60:
      displayMode = GST_DECKLINK_MODE_4320p60;
      break;
    case bmdMode8kDCI2398:
      displayMode = GST_DECKLINK_MODE_4Kp2398;
      break;
    case bmdMode8kDCI24:
      displayMode = GST_DECKLINK_MODE_4Kp24;
      break;
    case bmdMode8kDCI25:
      displayMode = GST_DECKLINK_MODE_4Kp25;
      break;
    case bmdMode8kDCI2997:
      displayMode = GST_DECKLINK_MODE_4Kp2997;
      break;
    case bmdMode8kDCI30:
      displayMode = GST_DECKLINK_MODE_4Kp30;
      break;
    case bmdMode8kDCI50:
      displayMode = GST_DECKLINK_MODE_4Kp50;
      break;
    case bmdMode8kDCI5994:
      displayMode = GST_DECKLINK_MODE_4Kp5994;
      break;
    case bmdMode8kDCI60:
      displayMode = GST_DECKLINK_MODE_4Kp60;
      break;
    default:
      displayMode = (GstDecklinkModeEnum) - 1;
      break;
  }
  return displayMode;
}

const BMDPixelFormat
gst_decklink_pixel_format_from_type (GstDecklinkVideoFormat t)
{
  return formats[t].format;
}

const gint
gst_decklink_bpp_from_type (GstDecklinkVideoFormat t)
{
  return formats[t].bpp;
}

const GstDecklinkVideoFormat
gst_decklink_type_from_video_format (GstVideoFormat f)
{
  guint i;

  for (i = 1; i < G_N_ELEMENTS (formats); i++) {
    if (formats[i].vformat == f)
      return (GstDecklinkVideoFormat) i;
  }
  g_assert_not_reached ();
  return GST_DECKLINK_VIDEO_FORMAT_AUTO;
}

GstVideoFormat
gst_decklink_video_format_from_type (BMDPixelFormat pf)
{
  guint i;

  for (i = 1; i < G_N_ELEMENTS (formats); i++) {
    if (formats[i].format == pf)
      return formats[i].vformat;
  }
  GST_WARNING ("Unknown pixel format 0x%x", pf);
  return GST_VIDEO_FORMAT_UNKNOWN;
}


const BMDTimecodeFormat
gst_decklink_timecode_format_from_enum (GstDecklinkTimecodeFormat f)
{
  return tcformats[f].format;
}

const GstDecklinkTimecodeFormat
gst_decklink_timecode_format_to_enum (BMDTimecodeFormat f)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (tcformats); i++) {
    if (tcformats[i].format == f)
      return (GstDecklinkTimecodeFormat) i;
  }
  g_assert_not_reached ();
  return GST_DECKLINK_TIMECODE_FORMAT_RP188ANY;
}

const BMDKeyerMode
gst_decklink_keyer_mode_from_enum (GstDecklinkKeyerMode m)
{
  return kmodes[m].keymode;
}

const GstDecklinkKeyerMode
gst_decklink_keyer_mode_to_enum (BMDKeyerMode m)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (kmodes); i++) {
    if (kmodes[i].keymode == m)
      return (GstDecklinkKeyerMode) i;
  }
  g_assert_not_reached ();
  return GST_DECKLINK_KEYER_MODE_OFF;
}

static const BMDVideoConnection connections[] = {
  (BMDVideoConnection) 0,       /* auto */
  bmdVideoConnectionSDI,
  bmdVideoConnectionHDMI,
  bmdVideoConnectionOpticalSDI,
  bmdVideoConnectionComponent,
  bmdVideoConnectionComposite,
  bmdVideoConnectionSVideo
};

const BMDVideoConnection
gst_decklink_get_connection (GstDecklinkConnectionEnum e)
{
  g_return_val_if_fail (e != GST_DECKLINK_CONNECTION_AUTO,
      bmdVideoConnectionSDI);

  if (e <= GST_DECKLINK_CONNECTION_AUTO || e > GST_DECKLINK_CONNECTION_SVIDEO)
    e = GST_DECKLINK_CONNECTION_SDI;

  return connections[e];
}

static gboolean
gst_decklink_caps_get_pixel_format (GstCaps * caps, BMDPixelFormat * format)
{
  GstVideoInfo vinfo;
  GstVideoFormat f;

  if (gst_video_info_from_caps (&vinfo, caps) == FALSE) {
    GST_ERROR ("Could not get video info from caps: %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  f = vinfo.finfo->format;
  *format = gst_decklink_pixel_format_from_type(gst_decklink_type_from_video_format (f));
  return TRUE;
}

static GstStructure *
gst_decklink_mode_get_generic_structure (GstDecklinkModeEnum e)
{
  const GstDecklinkMode *mode = &modes[e];
  GstStructure *s = gst_structure_new ("video/x-raw",
      "width", G_TYPE_INT, mode->width,
      "height", G_TYPE_INT, mode->height,
      "pixel-aspect-ratio", GST_TYPE_FRACTION, mode->par_n, mode->par_d,
      "interlace-mode", G_TYPE_STRING,
      mode->interlaced ? "interleaved" : "progressive",
      "framerate", GST_TYPE_FRACTION, mode->fps_n, mode->fps_d, NULL);

  return s;
}

static GstStructure *
gst_decklink_mode_get_structure (GstDecklinkModeEnum e, BMDPixelFormat f,
    gboolean input)
{
  const GstDecklinkMode *mode = &modes[e];
  GstStructure *s = gst_decklink_mode_get_generic_structure (e);

  if (input && mode->interlaced) {
    if (mode->tff)
      gst_structure_set (s, "field-order", G_TYPE_STRING, "top-field-first",
          NULL);
    else
      gst_structure_set (s, "field-order", G_TYPE_STRING, "bottom-field-first",
          NULL);
  }

  switch (f) {
    case bmdFormat8BitYUV:     /* '2vuy' */
      gst_structure_set (s, "format", G_TYPE_STRING, "UYVY",
          "colorimetry", G_TYPE_STRING, mode->colorimetry,
          "chroma-site", G_TYPE_STRING, "mpeg2", NULL);
      break;
    case bmdFormat10BitYUV:    /* 'v210' */
      gst_structure_set (s, "format", G_TYPE_STRING, "v210", NULL);
      break;
    case bmdFormat8BitARGB:    /* 'ARGB' */
      gst_structure_set (s, "format", G_TYPE_STRING, "ARGB", NULL);
      break;
    case bmdFormat8BitBGRA:    /* 'BGRA' */
      gst_structure_set (s, "format", G_TYPE_STRING, "BGRA", NULL);
      break;
    case bmdFormat10BitRGB:    /* 'r210' Big-endian RGB 10-bit per component with SMPTE video levels (64-960). Packed as 2:10:10:10 */
      gst_structure_set (s, "format", G_TYPE_STRING, "r210", NULL);
      break;
    case bmdFormat12BitRGB:    /* 'R12B' Big-endian RGB 12-bit per component with full range (0-4095). Packed as 12-bit per component */
    case bmdFormat12BitRGBLE:  /* 'R12L' Little-endian RGB 12-bit per component with full range (0-4095). Packed as 12-bit per component */
    case bmdFormat10BitRGBXLE: /* 'R10l' Little-endian 10-bit RGB with SMPTE video levels (64-940) */
    case bmdFormat10BitRGBX:   /* 'R10b' Big-endian 10-bit RGB with SMPTE video levels (64-940) */
    default:
      GST_WARNING ("format not supported %d", f);
      gst_structure_free (s);
      s = NULL;
      break;
  }

  return s;
}

GstCaps *
gst_decklink_mode_get_caps (GstDecklinkModeEnum e, BMDPixelFormat f,
    gboolean input)
{
  GstCaps *caps;

  caps = gst_caps_new_empty ();
  caps =
      gst_caps_merge_structure (caps, gst_decklink_mode_get_structure (e, f,
          input));

  return caps;
}

GstCaps *
gst_decklink_mode_get_caps_all_formats (GstDecklinkModeEnum e, gboolean input)
{
  GstCaps *caps;
  guint i;

  caps = gst_caps_new_empty ();
  for (i = 1; i < G_N_ELEMENTS (formats); i++)
    caps =
        gst_caps_merge_structure (caps, gst_decklink_mode_get_structure (e,
            formats[i].format, input));

  return caps;
}

GstCaps *
gst_decklink_pixel_format_get_caps (BMDPixelFormat f, gboolean input)
{
  int i;
  GstCaps *caps;
  GstStructure *s;

  caps = gst_caps_new_empty ();
  for (i = 1; i < (int) G_N_ELEMENTS (modes); i++) {
    s = gst_decklink_mode_get_structure ((GstDecklinkModeEnum) i, f, input);
    caps = gst_caps_merge_structure (caps, s);
  }

  return caps;
}

GstCaps *
gst_decklink_mode_get_template_caps (gboolean input)
{
  int i;
  GstCaps *caps;

  caps = gst_caps_new_empty ();
  for (i = 1; i < (int) G_N_ELEMENTS (modes); i++)
    caps =
        gst_caps_merge (caps,
        gst_decklink_mode_get_caps_all_formats ((GstDecklinkModeEnum) i,
            input));

  return caps;
}

const GstDecklinkMode *
gst_decklink_find_mode_and_format_for_caps (GstCaps * caps,
    BMDPixelFormat * format)
{
  int i;
  GstCaps *mode_caps;

  g_return_val_if_fail (gst_caps_is_fixed (caps), NULL);
  if (!gst_decklink_caps_get_pixel_format (caps, format))
    return NULL;

  for (i = 1; i < (int) G_N_ELEMENTS (modes); i++) {
    mode_caps =
        gst_decklink_mode_get_caps ((GstDecklinkModeEnum) i, *format, FALSE);
    if (gst_caps_can_intersect (caps, mode_caps)) {
      gst_caps_unref (mode_caps);
      return gst_decklink_get_mode ((GstDecklinkModeEnum) i);
    }
    gst_caps_unref (mode_caps);
  }

  return NULL;
}

const GstDecklinkMode *
gst_decklink_find_mode_for_caps (GstCaps * caps)
{
  BMDPixelFormat format;

  return gst_decklink_find_mode_and_format_for_caps (caps, &format);
}

#define GST_TYPE_DECKLINK_CLOCK \
  (gst_decklink_clock_get_type())
#define GST_DECKLINK_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DECKLINK_CLOCK,GstDecklinkClock))
#define GST_DECKLINK_CLOCK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DECKLINK_CLOCK,GstDecklinkClockClass))
#define GST_IS_Decklink_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DECKLINK_CLOCK))
#define GST_IS_Decklink_CLOCK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DECKLINK_CLOCK))
#define GST_DECKLINK_CLOCK_CAST(obj) \
  ((GstDecklinkClock*)(obj))

typedef struct _GstDecklinkClock GstDecklinkClock;
typedef struct _GstDecklinkClockClass GstDecklinkClockClass;

struct _GstDecklinkClock
{
  GstSystemClock clock;

  GstDecklinkOutput *output;
};

struct _GstDecklinkClockClass
{
  GstSystemClockClass parent_class;
};

GType gst_decklink_clock_get_type (void);
static GstClock *gst_decklink_clock_new (const gchar * name);

typedef struct _Device Device;
struct _Device
{
  GstDecklinkOutput output;
  GstDecklinkInput input;

  /* Audio/video output, Audio/video input */
  GstDecklinkDevice *devices[4];
};

static ProfileSetOperationResult gst_decklink_configure_profile (Device *
    device, GstDecklinkProfileId profile_id);
static MappingFormatSetOperationResult gst_decklink_configure_mapping_format (Device *
    device, GstDecklinkMappingFormat mapping_format);

static gboolean
persistent_id_is_equal_input (const Device * a, const gint64 * b)
{
  return a->input.persistent_id == *b;
}

static gboolean
persistent_id_is_equal_output (const Device * a, const gint64 * b)
{
  return a->output.persistent_id == *b;
}

class GStreamerDecklinkInputCallback:public IDeckLinkInputCallback
{
private:
  GstDecklinkInput * m_input;
  GMutex m_mutex;
  gint m_refcount;
public:
    GStreamerDecklinkInputCallback (GstDecklinkInput * input)
  : IDeckLinkInputCallback (), m_refcount (1)
  {
    m_input = input;
    g_mutex_init (&m_mutex);
  }

  virtual ~ GStreamerDecklinkInputCallback ()
  {
    g_mutex_clear (&m_mutex);
  }

  virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID, LPVOID *)
  {
    return E_NOINTERFACE;
  }

  virtual ULONG STDMETHODCALLTYPE AddRef (void)
  {
    ULONG ret;

    g_mutex_lock (&m_mutex);
    m_refcount++;
    ret = m_refcount;
    g_mutex_unlock (&m_mutex);

    return ret;
  }

  virtual ULONG STDMETHODCALLTYPE Release (void)
  {
    ULONG ret;

    g_mutex_lock (&m_mutex);
    m_refcount--;
    ret = m_refcount;
    g_mutex_unlock (&m_mutex);


    if (ret == 0) {
      delete this;
    }

    return ret;
  }

  virtual HRESULT STDMETHODCALLTYPE
      VideoInputFormatChanged (BMDVideoInputFormatChangedEvents,
      IDeckLinkDisplayMode * mode, BMDDetectedVideoInputFormatFlags formatFlags)
  {
    BMDPixelFormat pixelFormat = bmdFormatUnspecified;

    GST_INFO ("Video input format changed");

    /* Detect input format */
    if (formatFlags & bmdDetectedVideoInputRGB444) {
      if (formatFlags & bmdDetectedVideoInput10BitDepth) {
        pixelFormat = bmdFormat10BitRGB;
      } else if (formatFlags & bmdDetectedVideoInput8BitDepth) {
        /* Cannot detect ARGB vs BGRA, so assume ARGB unless user sets BGRA */
        if (m_input->format == bmdFormat8BitBGRA) {
          pixelFormat = bmdFormat8BitBGRA;
        } else {
          pixelFormat = bmdFormat8BitARGB;
        }
      } else {
        GST_ERROR ("Not implemented depth");
      }
    } else if (formatFlags & bmdDetectedVideoInputYCbCr422) {
      if (formatFlags & bmdDetectedVideoInput10BitDepth) {
        pixelFormat = bmdFormat10BitYUV;
      } else if (formatFlags & bmdDetectedVideoInput8BitDepth) {
        pixelFormat = bmdFormat8BitYUV;
      }
    }

    if (pixelFormat == bmdFormatUnspecified) {
      GST_ERROR ("Video input format is not supported");
      return E_FAIL;
    }

    if (!m_input->auto_format && (m_input->format != pixelFormat)) {
      GST_ERROR ("Video input format does not match the user-set format");
      return E_FAIL;
    }

    g_mutex_lock (&m_input->lock);
    m_input->input->PauseStreams ();
    m_input->input->EnableVideoInput (mode->GetDisplayMode (),
        pixelFormat, bmdVideoInputEnableFormatDetection);
    m_input->input->FlushStreams ();

    /* Reset any timestamp observations we might've made */
    if (m_input->videosrc) {
      GstDecklinkVideoSrc *videosrc =
          GST_DECKLINK_VIDEO_SRC (m_input->videosrc);

      g_mutex_lock (&videosrc->lock);
      videosrc->window_fill = 0;
      videosrc->window_filled = FALSE;
      videosrc->window_skip = 1;
      videosrc->window_skip_count = 0;
      videosrc->current_time_mapping.xbase = 0;
      videosrc->current_time_mapping.b = 0;
      videosrc->current_time_mapping.num = 1;
      videosrc->current_time_mapping.den = 1;
      videosrc->next_time_mapping.xbase = 0;
      videosrc->next_time_mapping.b = 0;
      videosrc->next_time_mapping.num = 1;
      videosrc->next_time_mapping.den = 1;
      g_mutex_unlock (&videosrc->lock);
    }

    m_input->input->StartStreams ();
    m_input->mode =
        gst_decklink_get_mode (gst_decklink_get_mode_enum_from_bmd
        (mode->GetDisplayMode ()));
    m_input->format = pixelFormat;
    g_mutex_unlock (&m_input->lock);

    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE
      VideoInputFrameArrived (IDeckLinkVideoInputFrame * video_frame,
      IDeckLinkAudioInputPacket * audio_packet)
  {
    GstElement *videosrc = NULL, *audiosrc = NULL;
    void (*got_video_frame) (GstElement * videosrc,
        IDeckLinkVideoInputFrame * frame, GstDecklinkModeEnum mode,
        GstClockTime capture_time, GstClockTime stream_time,
        GstClockTime stream_duration, GstClockTime hardware_time,
        GstClockTime hardware_duration, IDeckLinkTimecode * dtc,
        gboolean no_signal) = NULL;
    void (*got_audio_packet) (GstElement * videosrc,
        IDeckLinkAudioInputPacket * packet, GstClockTime capture_time,
        GstClockTime stream_time, GstClockTime stream_duration,
        GstClockTime hardware_time, GstClockTime hardware_duration,
        gboolean no_signal) = NULL;
    GstDecklinkModeEnum mode = GST_DECKLINK_MODE_AUTO;
    GstClockTime capture_time = GST_CLOCK_TIME_NONE;
    GstClockTime base_time = 0;
    gboolean no_signal = FALSE;
    GstClock *clock = NULL;
    HRESULT res;
    BMDTimeValue stream_time = GST_CLOCK_TIME_NONE;
    BMDTimeValue stream_duration = GST_CLOCK_TIME_NONE;
    BMDTimeValue hardware_time = GST_CLOCK_TIME_NONE;
    BMDTimeValue hardware_duration = GST_CLOCK_TIME_NONE;

    g_mutex_lock (&m_input->lock);
    if (m_input->videosrc) {
      videosrc = GST_ELEMENT_CAST (gst_object_ref (m_input->videosrc));
      clock = gst_element_get_clock (videosrc);
      base_time = gst_element_get_base_time (videosrc);
      got_video_frame = m_input->got_video_frame;
    }

    if (m_input->mode)
      mode = gst_decklink_get_mode_enum_from_bmd (m_input->mode->mode);

    if (m_input->audiosrc) {
      audiosrc = GST_ELEMENT_CAST (gst_object_ref (m_input->audiosrc));
      if (!clock) {
        clock = gst_element_get_clock (GST_ELEMENT_CAST (audiosrc));
        base_time = gst_element_get_base_time (audiosrc);
      }
      got_audio_packet = m_input->got_audio_packet;
    }
    g_mutex_unlock (&m_input->lock);

    if (clock) {
      capture_time = gst_clock_get_time (clock);
      if (video_frame) {
	// If we have the actual capture time for the frame, compensate the
	// capture time accordingly.
	//
	// We do this by subtracting the belay between "now" in hardware
	// reference clock and the time when the frame was finished being
	// capture based on the same hardware reference clock.
	//
	// We then subtract that difference from the "now" on the gst clock.
	//
	// *Technically* we should be compensating that difference for the
	// difference in clock rate between the "hardware reference clock" and
	// the GStreamer clock. But since the values are quite small this has
	// very little impact.
	BMDTimeValue hardware_now;
	res = m_input->input->GetHardwareReferenceClock (GST_SECOND, &hardware_now, NULL, NULL);
	if (res == S_OK) {
	  res =
	    video_frame->GetHardwareReferenceTimestamp (GST_SECOND,
							&hardware_time, &hardware_duration);
	  if (res != S_OK) {
	    GST_ERROR ("Failed to get hardware time: 0x%08lx", (unsigned long) res);
	    hardware_time = GST_CLOCK_TIME_NONE;
	    hardware_duration = GST_CLOCK_TIME_NONE;
	  } else {
	    GstClockTime hardware_diff = hardware_now - hardware_time;
	    GST_LOG ("Compensating capture time by %" GST_TIME_FORMAT,
		     GST_TIME_ARGS (hardware_diff));
	    if (capture_time > hardware_diff)
	      capture_time -= hardware_diff;
	    else
	      capture_time = 0;
	  }
	}
      }
      if (capture_time > base_time)
        capture_time -= base_time;
      else
        capture_time = 0;
    }

    if (video_frame) {
      BMDFrameFlags flags;

      flags = video_frame->GetFlags ();
      if (flags & bmdFrameHasNoInputSource) {
        no_signal = TRUE;
      }
    }

    if (got_video_frame && videosrc && video_frame) {
      IDeckLinkTimecode *dtc = 0;

      res =
          video_frame->GetStreamTime (&stream_time, &stream_duration,
          GST_SECOND);
      if (res != S_OK) {
        GST_ERROR ("Failed to get stream time: 0x%08lx", (unsigned long) res);
        stream_time = GST_CLOCK_TIME_NONE;
        stream_duration = GST_CLOCK_TIME_NONE;
      }

      res =
          video_frame->GetHardwareReferenceTimestamp (GST_SECOND,
          &hardware_time, &hardware_duration);
      if (res != S_OK) {
        GST_ERROR ("Failed to get hardware time: 0x%08lx", (unsigned long) res);
        hardware_time = GST_CLOCK_TIME_NONE;
        hardware_duration = GST_CLOCK_TIME_NONE;
      }

      if (m_input->videosrc) {
        /* FIXME: Avoid circularity between gstdecklink.cpp and
         * gstdecklinkvideosrc.cpp */
        res =
            video_frame->
            GetTimecode (GST_DECKLINK_VIDEO_SRC (videosrc)->timecode_format,
            &dtc);

        if (res != S_OK) {
          GST_DEBUG_OBJECT (videosrc, "Failed to get timecode: 0x%08lx",
              (unsigned long) res);
          dtc = NULL;
        }
      }

      /* passing dtc reference */
      got_video_frame (videosrc, video_frame, mode, capture_time,
          stream_time, stream_duration, hardware_time, hardware_duration, dtc,
          no_signal);
    }

    if (got_audio_packet && audiosrc && audio_packet) {
      m_input->got_audio_packet (audiosrc, audio_packet, capture_time,
          stream_time, stream_duration, hardware_time, hardware_duration,
          no_signal);
    } else {
      if (!audio_packet)
        GST_DEBUG ("Received no audio packet at %" GST_TIME_FORMAT,
            GST_TIME_ARGS (capture_time));
    }

    gst_object_replace ((GstObject **) & videosrc, NULL);
    gst_object_replace ((GstObject **) & audiosrc, NULL);
    gst_object_replace ((GstObject **) & clock, NULL);

    return S_OK;
  }
};

class GStreamerDecklinkMemoryAllocator:public IDeckLinkMemoryAllocator
{
private:
  GMutex m_mutex;
  uint32_t m_lastBufferSize;
  uint32_t m_nonEmptyCalls;
  GstQueueArray *m_buffers;
  gint m_refcount;

  void _clearBufferPool ()
  {
    uint8_t *buf;

    if (!m_buffers)
        return;

    while ((buf = (uint8_t *) gst_queue_array_pop_head (m_buffers))) {
      uint8_t offset = *(buf - 1);
      void *alloc_buf = buf - 128 + offset;
        g_free (alloc_buf);
    }
  }

public:
    GStreamerDecklinkMemoryAllocator ()
  : IDeckLinkMemoryAllocator (),
      m_lastBufferSize (0),
      m_nonEmptyCalls (0), m_buffers (NULL), m_refcount (1)
  {
    g_mutex_init (&m_mutex);

    m_buffers = gst_queue_array_new (60);
  }

  virtual ~ GStreamerDecklinkMemoryAllocator () {
    Decommit ();

    gst_queue_array_free (m_buffers);

    g_mutex_clear (&m_mutex);
  }

  virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID, LPVOID *)
  {
    return E_NOINTERFACE;
  }

  virtual ULONG STDMETHODCALLTYPE AddRef (void)
  {
    ULONG ret;

    g_mutex_lock (&m_mutex);
    m_refcount++;
    ret = m_refcount;
    g_mutex_unlock (&m_mutex);

    return ret;
  }

  virtual ULONG STDMETHODCALLTYPE Release (void)
  {
    ULONG ret;

    g_mutex_lock (&m_mutex);
    m_refcount--;
    ret = m_refcount;
    g_mutex_unlock (&m_mutex);


    if (ret == 0) {
      delete this;
    }

    return ret;
  }

  virtual HRESULT STDMETHODCALLTYPE
      AllocateBuffer (uint32_t bufferSize, void **allocatedBuffer)
  {
    uint8_t *buf;
    uint8_t offset = 0;

    g_mutex_lock (&m_mutex);

    /* If buffer size changed since last call, empty buffer pool */
    if (bufferSize != m_lastBufferSize) {
      _clearBufferPool ();
      m_lastBufferSize = bufferSize;
    }

    /* Look if there is a free buffer in the pool */
    if (!(buf = (uint8_t *) gst_queue_array_pop_head (m_buffers))) {
      /* If not, alloc a new one */
      buf = (uint8_t *) g_malloc (bufferSize + 128);

      /* The Decklink SDK requires 16 byte aligned memory at least but for us
       * to work nicely let's align to 64 bytes (512 bits) as this allows
       * aligned AVX2 operations for example */
      if (((guintptr) buf) % 64 != 0) {
        offset = ((guintptr) buf) % 64;
      }

      /* Write the allocation size at the very beginning. It's guaranteed by
       * malloc() to be allocated aligned enough for doing this. */
      *((uint32_t *) buf) = bufferSize;

      /* Align our buffer */
      buf += 128 - offset;

      /* And write the alignment offset right before the buffer */
      *(buf - 1) = offset;
    }
    *allocatedBuffer = (void *) buf;

    /* If there are still unused buffers in the pool
     * remove one of them every fifth call */
    if (gst_queue_array_get_length (m_buffers) > 0) {
      if (++m_nonEmptyCalls >= 5) {
        buf = (uint8_t *) gst_queue_array_pop_head (m_buffers);
        uint8_t offset = *(buf - 1);
        void *alloc_buf = buf - 128 + offset;
        g_free (alloc_buf);
        m_nonEmptyCalls = 0;
      }
    } else {
      m_nonEmptyCalls = 0;
    }

    g_mutex_unlock (&m_mutex);

    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE ReleaseBuffer (void *buffer)
  {
    g_mutex_lock (&m_mutex);

    /* Put the buffer back to the pool if size matches with current pool */
    uint8_t offset = *(((uint8_t *) buffer) - 1);
    uint8_t *alloc_buffer = ((uint8_t *) buffer) - 128 + offset;
    uint32_t size = *(uint32_t *) alloc_buffer;
    if (size == m_lastBufferSize) {
      gst_queue_array_push_tail (m_buffers, buffer);
    } else {
      g_free (alloc_buffer);
    }

    g_mutex_unlock (&m_mutex);

    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE Commit ()
  {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE Decommit ()
  {
    /* Clear all remaining pools */
    _clearBufferPool ();

    return S_OK;
  }
};

#ifdef G_OS_WIN32
/* FIXME: We currently never deinit this */

static GMutex com_init_lock;
static GMutex com_deinit_lock;
static GCond com_init_cond;
static GCond com_deinit_cond;
static GCond com_deinited_cond;
static gboolean com_initialized = FALSE;

/* COM initialization/uninitialization thread */
static gpointer
gst_decklink_com_thread (gpointer data)
{
  HRESULT res;

  g_mutex_lock (&com_init_lock);

  /* Initialize COM with a MTA for this process. This thread will
   * be the first one to enter the apartement and the last one to leave
   * it, unitializing COM properly */

  res = CoInitializeEx (0, COINIT_MULTITHREADED);
  if (res == S_FALSE)
    GST_WARNING ("COM has been already initialized in the same process");
  else if (res == RPC_E_CHANGED_MODE)
    GST_WARNING ("The concurrency model of COM has changed.");
  else
    GST_INFO ("COM initialized successfully");

  com_initialized = TRUE;

  /* Signal other threads waiting on this condition that COM was initialized */
  g_cond_signal (&com_init_cond);

  g_mutex_unlock (&com_init_lock);

  /* Wait until the uninitialize condition is met to leave the COM apartement */
  g_mutex_lock (&com_deinit_lock);
  g_cond_wait (&com_deinit_cond, &com_deinit_lock);

  CoUninitialize ();
  GST_INFO ("COM uninitialized successfully");
  com_initialized = FALSE;
  g_cond_signal (&com_deinited_cond);
  g_mutex_unlock (&com_deinit_lock);

  return NULL;
}
#endif /* G_OS_WIN32 */

static GOnce devices_once = G_ONCE_INIT;
static GPtrArray *devices;      /* array of Device */


static GstDecklinkDevice *
gst_decklink_device_new (const gchar * model_name, const gchar * display_name,
    const gchar * serial_number, gint64 persistent_id,
    gboolean supports_format_detection, GstCaps * video_caps,
    guint max_channels, gboolean video, gboolean capture, guint device_number)
{
  GstDevice *ret;
  gchar *name;
  const gchar *device_class;
  GstCaps *caps = NULL;
  GstStructure *properties;

  if (capture)
    device_class = video ? "Video/Source/Hardware" : "Audio/Source/Hardware";
  else
    device_class = video ? "Video/Sink/Hardware" : "Audio/Sink/Hardware";

  name =
      g_strdup_printf ("%s (%s %s)", display_name,
      video ? "Video" : "Audio", capture ? "Capture" : "Output");

  if (video) {
    caps = gst_caps_ref (video_caps);
  } else {
    static GstStaticCaps audio_caps =
        GST_STATIC_CAPS
        ("audio/x-raw, format={S16LE,S32LE}, channels={2, 8, 16}, rate=48000, "
        "layout=interleaved");
    GstCaps *max_channel_caps =
        gst_caps_new_simple ("audio/x-raw", "channels", GST_TYPE_INT_RANGE, 2,
        max_channels, NULL);

    caps =
        gst_caps_intersect (gst_static_caps_get (&audio_caps),
        max_channel_caps);
    gst_caps_unref (max_channel_caps);
  }
  properties = gst_structure_new_empty ("properties");

  gst_structure_set (properties,
      "device-number", G_TYPE_UINT, device_number,
      "model-name", G_TYPE_STRING, model_name,
      "display-name", G_TYPE_STRING, display_name,
      "max-channels", G_TYPE_UINT, max_channels, NULL);

  if (capture)
    gst_structure_set (properties, "supports-format-detection", G_TYPE_BOOLEAN,
        supports_format_detection, NULL);

  if (serial_number)
    gst_structure_set (properties, "serial-number", G_TYPE_STRING,
        serial_number, NULL);

  if (persistent_id)
    gst_structure_set (properties, "persistent-id", G_TYPE_INT64,
        persistent_id, NULL);

  ret = GST_DEVICE (g_object_new (GST_TYPE_DECKLINK_DEVICE,
          "display-name", name,
          "device-class", device_class, "caps", caps, "properties", properties,
          NULL));

  g_free (name);
  gst_caps_unref (caps);
  gst_structure_free (properties);

  GST_DECKLINK_DEVICE (ret)->video = video;
  GST_DECKLINK_DEVICE (ret)->capture = capture;
  GST_DECKLINK_DEVICE (ret)->persistent_id = persistent_id;

  return GST_DECKLINK_DEVICE (ret);
}

static gint
compare_persistent_id (gconstpointer a, gconstpointer b)
{
  const Device *const dev1 = *(Device **) a;
  const Device *const dev2 = *(Device **) b;
  return dev1->input.persistent_id - dev2->input.persistent_id;
}

static gpointer
init_devices (gpointer data)
{
  IDeckLinkIterator *iterator;
  IDeckLink *decklink = NULL;
  HRESULT ret;
  int i;

#ifdef G_OS_WIN32
  // Start COM thread for Windows

  g_mutex_lock (&com_init_lock);

  /* create the COM initialization thread */
  g_thread_new ("COM init thread", (GThreadFunc) gst_decklink_com_thread, NULL);

  /* wait until the COM thread signals that COM has been initialized */
  g_cond_wait (&com_init_cond, &com_init_lock);
  g_mutex_unlock (&com_init_lock);
#endif /* G_OS_WIN32 */

  iterator = CreateDeckLinkIteratorInstance ();
  if (iterator == NULL) {
    GST_DEBUG ("no driver");
    return NULL;
  }

  devices = g_ptr_array_new ();

  i = 0;
  ret = iterator->Next (&decklink);
  while (ret == S_OK) {
    Device *dev;
    gboolean capture = FALSE;
    gboolean output = FALSE;
    gchar *model_name = NULL;
    gchar *display_name = NULL;
    gchar *serial_number = NULL;
    gint64 persistent_id = 0;
    gboolean supports_format_detection = 0;
    gint64 max_channels = 2;
    GstCaps *video_input_caps = gst_caps_new_empty ();
    GstCaps *video_output_caps = gst_caps_new_empty ();

    dev = g_new0 (Device, 1);

    g_mutex_init (&dev->input.lock);
    g_mutex_init (&dev->output.lock);
    g_cond_init (&dev->output.cond);

    ret = decklink->QueryInterface (IID_IDeckLinkInput,
        (void **) &dev->input.input);
    if (ret != S_OK) {
      GST_WARNING ("selected device does not have input interface: 0x%08lx",
          (unsigned long) ret);
    } else {
      IDeckLinkDisplayModeIterator *mode_iter;

      dev->input.device = decklink;
      dev->input.input->
          SetCallback (new GStreamerDecklinkInputCallback (&dev->input));

      if ((ret = dev->input.input->GetDisplayModeIterator (&mode_iter)) == S_OK) {
        IDeckLinkDisplayMode *mode;

        GST_DEBUG ("Input %d supports:", i);
        while ((ret = mode_iter->Next (&mode)) == S_OK) {
          char *name;
          GstDecklinkModeEnum mode_enum;

          mode_enum =
              gst_decklink_get_mode_enum_from_bmd (mode->GetDisplayMode ());
          if (mode_enum != (GstDecklinkModeEnum) - 1)
            video_input_caps =
                gst_caps_merge_structure (video_input_caps,
                gst_decklink_mode_get_generic_structure (mode_enum));

          mode->GetName ((COMSTR_T *) & name);
          CONVERT_COM_STRING (name);
          GST_DEBUG ("    %s mode: 0x%08x width: %ld height: %ld"
              " fields: 0x%08x flags: 0x%08x", name,
              (int) mode->GetDisplayMode (), mode->GetWidth (),
              mode->GetHeight (), (int) mode->GetFieldDominance (),
              (int) mode->GetFlags ());
          FREE_COM_STRING (name);
          mode->Release ();
        }
        mode_iter->Release ();
      }

      capture = TRUE;

      ret = S_OK;
    }

    ret = decklink->QueryInterface (IID_IDeckLinkOutput,
        (void **) &dev->output.output);
    if (ret != S_OK) {
      GST_WARNING ("selected device does not have output interface: 0x%08lx",
          (unsigned long) ret);
    } else {
      IDeckLinkDisplayModeIterator *mode_iter;

      dev->output.device = decklink;
      dev->output.clock = gst_decklink_clock_new ("GstDecklinkOutputClock");
      GST_DECKLINK_CLOCK_CAST (dev->output.clock)->output = &dev->output;

      if ((ret =
              dev->output.output->GetDisplayModeIterator (&mode_iter)) ==
          S_OK) {
        IDeckLinkDisplayMode *mode;

        GST_DEBUG ("Output %d supports:", i);
        while ((ret = mode_iter->Next (&mode)) == S_OK) {
          char *name;
          GstDecklinkModeEnum mode_enum;

          mode_enum =
              gst_decklink_get_mode_enum_from_bmd (mode->GetDisplayMode ());
          if (mode_enum != (GstDecklinkModeEnum) - 1)
            video_output_caps =
                gst_caps_merge_structure (video_output_caps,
                gst_decklink_mode_get_generic_structure (mode_enum));

          mode->GetName ((COMSTR_T *) & name);
          CONVERT_COM_STRING (name);
          GST_DEBUG ("    %s mode: 0x%08x width: %ld height: %ld"
              " fields: 0x%08x flags: 0x%08x", name,
              (int) mode->GetDisplayMode (), mode->GetWidth (),
              mode->GetHeight (), (int) mode->GetFieldDominance (),
              (int) mode->GetFlags ());
          FREE_COM_STRING (name);
          mode->Release ();
        }
        mode_iter->Release ();
      }

      output = TRUE;

      ret = S_OK;
    }

    ret = decklink->QueryInterface (IID_IDeckLinkConfiguration,
        (void **) &dev->input.config);
    if (ret != S_OK) {
      GST_WARNING ("selected device does not have config interface: 0x%08lx",
          (unsigned long) ret);
    } else {
      ret =
          dev->input.
          config->GetString (bmdDeckLinkConfigDeviceInformationSerialNumber,
          (COMSTR_T *) & serial_number);
      if (ret == S_OK) {
        CONVERT_COM_STRING (serial_number);
        dev->output.hw_serial_number = g_strdup (serial_number);
        dev->input.hw_serial_number = g_strdup (serial_number);
        GST_DEBUG ("device %d has serial number %s", i, serial_number);
      }
    }

    ret = decklink->QueryInterface (IID_IDeckLinkProfileAttributes,
        (void **) &dev->input.attributes);
    dev->output.attributes = dev->input.attributes;
    if (ret != S_OK) {
      GST_WARNING ("selected device does not have attributes interface: "
          "0x%08lx", (unsigned long) ret);
    } else {
      bool tmp_bool = false;
      int64_t tmp_int = 2;
      int64_t tmp_int_persistent_id = 0;

      dev->input.attributes->GetInt (BMDDeckLinkMaximumAudioChannels, &tmp_int);
      dev->input.attributes->GetFlag (BMDDeckLinkSupportsInputFormatDetection,
          &tmp_bool);
      supports_format_detection = tmp_bool;
      max_channels = tmp_int;

      ret =
          dev->input.attributes->GetInt (BMDDeckLinkPersistentID,
          &tmp_int_persistent_id);
      if (ret == S_OK) {
        persistent_id = tmp_int_persistent_id;
        dev->output.persistent_id = persistent_id;
        dev->input.persistent_id = persistent_id;
        GST_DEBUG ("device %d has persistent id %" G_GINT64_FORMAT, i, persistent_id);
      } else {
        persistent_id = i;
        dev->output.persistent_id = i;
        dev->input.persistent_id = i;
        GST_DEBUG ("device %d does not have persistent id. Value set to %d", i, i);
      }
    }

    decklink->GetModelName ((COMSTR_T *) & model_name);
    if (model_name)
      CONVERT_COM_STRING (model_name);
    decklink->GetDisplayName ((COMSTR_T *) & display_name);
    if (display_name)
      CONVERT_COM_STRING (display_name);

    if (capture) {
      dev->devices[0] =
          gst_decklink_device_new (model_name, display_name, serial_number,
          persistent_id, supports_format_detection, video_input_caps,
          max_channels, TRUE, TRUE, i);
      dev->devices[1] =
          gst_decklink_device_new (model_name, display_name, serial_number,
          persistent_id, supports_format_detection, video_input_caps,
          max_channels, FALSE, TRUE, i);
    }
    if (output) {
      dev->devices[2] =
          gst_decklink_device_new (model_name, display_name, serial_number,
          persistent_id, supports_format_detection, video_output_caps,
          max_channels, TRUE, FALSE, i);
      dev->devices[3] =
          gst_decklink_device_new (model_name, display_name, serial_number,
          persistent_id, supports_format_detection, video_output_caps,
          max_channels, FALSE, FALSE, i);
    }

    if (model_name)
      FREE_COM_STRING (model_name);
    if (display_name)
      FREE_COM_STRING (display_name);
    if (serial_number)
      FREE_COM_STRING (serial_number);
    gst_caps_unref (video_input_caps);
    gst_caps_unref (video_output_caps);

    ret = decklink->QueryInterface (IID_IDeckLinkKeyer,
        (void **) &dev->output.keyer);

    g_ptr_array_add (devices, dev);

    /* We only warn of failure to obtain the keyer interface if the keyer
     * is enabled by keyer_mode
     */

    ret = iterator->Next (&decklink);
    i++;
  }

  GST_INFO ("Detected %u devices", devices->len);

  iterator->Release ();

  g_ptr_array_sort (devices, compare_persistent_id);

  return NULL;
}

GList *
gst_decklink_get_devices (void)
{
  guint i;
  GList *l = NULL;

  g_once (&devices_once, init_devices, NULL);

  if (!devices) {
    return NULL;
  }

  for (i = 0; i < devices->len; i++) {
    Device *device = (Device *) g_ptr_array_index (devices, i);

    if (device->devices[0])
      l = g_list_prepend (l, g_object_ref (device->devices[0]));

    if (device->devices[1])
      l = g_list_prepend (l, g_object_ref (device->devices[1]));

    if (device->devices[2])
      l = g_list_prepend (l, g_object_ref (device->devices[2]));

    if (device->devices[3])
      l = g_list_prepend (l, g_object_ref (device->devices[3]));
  }

  l = g_list_reverse (l);

  return l;
}

GstDecklinkOutput *
gst_decklink_acquire_nth_output (gint n, gint64 persistent_id,
    GstElement * sink, gboolean is_audio)
{
  GstDecklinkOutput *output;
  Device *device;
  guint found_index;

  g_once (&devices_once, init_devices, NULL);

  if (devices == NULL)
    return NULL;

  if (persistent_id != DEFAULT_PERSISTENT_ID) {
    if (g_ptr_array_find_with_equal_func (devices, &persistent_id,
            (GEqualFunc) persistent_id_is_equal_output, &found_index)) {
      n = found_index;
      GST_DEBUG ("Persistent ID: %" G_GINT64_FORMAT ", used", persistent_id);
    } else {
      return NULL;
    }
  }

  if (n < 0 || (guint) n >= devices->len)
    return NULL;

  device = (Device *) g_ptr_array_index (devices, n);
  output = &device->output;
  if (!output->output) {
    GST_ERROR ("Device %d has no output", n);
    return NULL;
  }

  if (!is_audio) {
    GstDecklinkVideoSink *videosink = (GstDecklinkVideoSink *) (sink);
    if (gst_decklink_configure_profile (device,
            videosink->profile_id) == PROFILE_SET_FAILURE) {
      return NULL;
    }
    if (gst_decklink_configure_mapping_format (device,
            videosink->mapping_format) == MAPPING_FORMAT_SET_FAILURE) {
      return NULL;
    }
  }

  g_mutex_lock (&output->lock);
  if (is_audio && !output->audiosink) {
    output->audiosink = GST_ELEMENT_CAST (gst_object_ref (sink));
    g_mutex_unlock (&output->lock);
    return output;
  } else if (!output->videosink) {
    output->videosink = GST_ELEMENT_CAST (gst_object_ref (sink));
    g_mutex_unlock (&output->lock);
    return output;
  }
  g_mutex_unlock (&output->lock);

  GST_ERROR ("Output device %d (audio: %d) in use already", n, is_audio);
  return NULL;
}

void
gst_decklink_release_nth_output (gint n, gint64 persistent_id,
    GstElement * sink, gboolean is_audio)
{
  GstDecklinkOutput *output;
  Device *device;
  guint found_index;

  if (devices == NULL)
    return;

  if (persistent_id != DEFAULT_PERSISTENT_ID) {
    if (g_ptr_array_find_with_equal_func (devices, &persistent_id,
            (GEqualFunc) persistent_id_is_equal_output, &found_index)) {
      n = found_index;
      GST_DEBUG ("Persistent ID: %" G_GINT64_FORMAT ", used", persistent_id);
    } else {
      return;
    }
  }

  if (n < 0 || (guint) n >= devices->len)
    return;

  device = (Device *) g_ptr_array_index (devices, n);
  output = &device->output;
  g_assert (output->output);

  g_mutex_lock (&output->lock);
  if (is_audio) {
    g_assert (output->audiosink == sink);
    gst_object_unref (sink);
    output->audiosink = NULL;
  } else {
    g_assert (output->videosink == sink);
    gst_object_unref (sink);
    output->videosink = NULL;
  }
  g_mutex_unlock (&output->lock);
}

GstDecklinkInput *
gst_decklink_acquire_nth_input (gint n, gint64 persistent_id, GstElement * src,
    gboolean is_audio)
{
  GstDecklinkInput *input;
  Device *device;
  guint found_index;

  g_once (&devices_once, init_devices, NULL);

  if (devices == NULL)
    return NULL;

  if (persistent_id != DEFAULT_PERSISTENT_ID) {
    if (g_ptr_array_find_with_equal_func (devices, &persistent_id,
            (GEqualFunc) persistent_id_is_equal_input, &found_index)) {
      n = found_index;
      GST_DEBUG ("Persistent ID: %" G_GINT64_FORMAT ", used", persistent_id);
    } else {
      return NULL;
    }
  }

  if (n < 0 || (guint) n >= devices->len)
    return NULL;

  device = (Device *) g_ptr_array_index (devices, n);
  input = &device->input;
  if (!input->input) {
    GST_ERROR ("Device %d has no input", n);
    return NULL;
  }

  if (!is_audio) {
    GstDecklinkVideoSrc *videosrc = (GstDecklinkVideoSrc *) (src);
    if (gst_decklink_configure_profile (device,
            videosrc->profile_id) == PROFILE_SET_FAILURE) {
      return NULL;
    }
  }

  g_mutex_lock (&input->lock);
  input->input->SetVideoInputFrameMemoryAllocator (new
      GStreamerDecklinkMemoryAllocator);
  if (is_audio && !input->audiosrc) {
    input->audiosrc = GST_ELEMENT_CAST (gst_object_ref (src));
    g_mutex_unlock (&input->lock);
    return input;
  } else if (!input->videosrc) {
    input->videosrc = GST_ELEMENT_CAST (gst_object_ref (src));
    g_mutex_unlock (&input->lock);
    return input;
  }

  g_mutex_unlock (&input->lock);

  GST_ERROR ("Input device %d (audio: %d) in use already", n, is_audio);
  return NULL;
}

void
gst_decklink_release_nth_input (gint n, gint64 persistent_id, GstElement * src,
    gboolean is_audio)
{
  GstDecklinkInput *input;
  Device *device;
  guint found_index;

  if (devices == NULL)
    return;

  if (persistent_id != DEFAULT_PERSISTENT_ID) {
    if (g_ptr_array_find_with_equal_func (devices, &persistent_id,
            (GEqualFunc) persistent_id_is_equal_input, &found_index)) {
      n = found_index;
      GST_DEBUG ("Persistent ID: %" G_GINT64_FORMAT ", used", persistent_id);
    } else {
      return;
    }
  }

  if (n < 0 || (guint) n >= devices->len)
    return;

  device = (Device *) g_ptr_array_index (devices, n);

  input = &device->input;
  g_assert (input->input);

  g_mutex_lock (&input->lock);
  if (is_audio) {
    g_assert (input->audiosrc == src);
    gst_object_unref (src);
    input->audiosrc = NULL;
  } else {
    g_assert (input->videosrc == src);
    gst_object_unref (src);
    input->videosrc = NULL;
  }
  g_mutex_unlock (&input->lock);
}

static ProfileSetOperationResult
gst_decklink_configure_profile (Device * device,
    GstDecklinkProfileId profile_id)
{
  HRESULT res;

  if (profile_id == GST_DECKLINK_PROFILE_ID_DEFAULT)
    return PROFILE_SET_SUCCESS;

  GstDecklinkInput *input = &device->input;
  IDeckLink *decklink = input->device;

  IDeckLinkProfileManager *manager = NULL;
  if (decklink->QueryInterface (IID_IDeckLinkProfileManager,
          (void **) &manager) == S_OK) {
    BMDProfileID bmd_profile_id;

    switch (profile_id) {
      case GST_DECKLINK_PROFILE_ID_ONE_SUB_DEVICE_FULL_DUPLEX:
        bmd_profile_id = bmdProfileOneSubDeviceFullDuplex;
        break;
      case GST_DECKLINK_PROFILE_ID_ONE_SUB_DEVICE_HALF_DUPLEX:
        bmd_profile_id = bmdProfileOneSubDeviceHalfDuplex;
        break;
      case GST_DECKLINK_PROFILE_ID_TWO_SUB_DEVICES_FULL_DUPLEX:
        bmd_profile_id = bmdProfileTwoSubDevicesFullDuplex;
        break;
      case GST_DECKLINK_PROFILE_ID_TWO_SUB_DEVICES_HALF_DUPLEX:
        bmd_profile_id = bmdProfileTwoSubDevicesHalfDuplex;
        break;
      case GST_DECKLINK_PROFILE_ID_FOUR_SUB_DEVICES_HALF_DUPLEX:
        bmd_profile_id = bmdProfileFourSubDevicesHalfDuplex;
        break;
      default:
      case GST_DECKLINK_PROFILE_ID_DEFAULT:
        g_assert_not_reached ();
        break;
    }

    IDeckLinkProfile *profile = NULL;
    res = manager->GetProfile (bmd_profile_id, &profile);

    if (res == S_OK && profile) {
      res = profile->SetActive ();
      profile->Release ();
    }

    manager->Release ();

    if (res == S_OK) {
      GST_DEBUG ("Successfully set profile");
      return PROFILE_SET_SUCCESS;
    } else {
      GST_ERROR ("Failed to set profile");
      return PROFILE_SET_FAILURE;
    }
  } else {
    GST_DEBUG ("Device has only one profile");
    return PROFILE_SET_UNSUPPORTED;
  }
}

static MappingFormatSetOperationResult
gst_decklink_configure_mapping_format (Device * device,
    GstDecklinkMappingFormat mapping_format)
{
  HRESULT res;

  bool level_a_output;
  switch (mapping_format) {
    case GST_DECKLINK_MAPPING_FORMAT_LEVEL_A:
      level_a_output = true;
      break;
    case GST_DECKLINK_MAPPING_FORMAT_LEVEL_B:
      level_a_output = false;
      break;
    default:
    case GST_DECKLINK_MAPPING_FORMAT_DEFAULT:
      return MAPPING_FORMAT_SET_SUCCESS;
  }

  // Make sure Level A is supported
  bool supports_level_a_output = false;
  res = device->output.attributes->GetFlag(BMDDeckLinkSupportsSMPTELevelAOutput,
    &supports_level_a_output);
  if (res != S_OK || !supports_level_a_output) {
    if (level_a_output) {
      GST_DEBUG ("Device does not support Level A mapping format");
      return MAPPING_FORMAT_SET_UNSUPPORTED;
    } else {
      // Level B is the only supported option
      return MAPPING_FORMAT_SET_SUCCESS;
    }
  }

  res = device->input.config->SetFlag(bmdDeckLinkConfigSMPTELevelAOutput, level_a_output);
  if (res == S_OK) {
    GST_DEBUG ("Successfully set mapping format");
    return MAPPING_FORMAT_SET_SUCCESS;
  } else {
    GST_ERROR ("Failed to set mapping format");
    return MAPPING_FORMAT_SET_FAILURE;
  }
}

G_DEFINE_TYPE (GstDecklinkClock, gst_decklink_clock, GST_TYPE_SYSTEM_CLOCK);

static GstClockTime gst_decklink_clock_get_internal_time (GstClock * clock);

static void
gst_decklink_clock_class_init (GstDecklinkClockClass * klass)
{
  GstClockClass *clock_class = (GstClockClass *) klass;

  clock_class->get_internal_time = gst_decklink_clock_get_internal_time;
}

static void
gst_decklink_clock_init (GstDecklinkClock * clock)
{
  GST_OBJECT_FLAG_SET (clock, GST_CLOCK_FLAG_CAN_SET_MASTER);
}

static GstClock *
gst_decklink_clock_new (const gchar * name)
{
  GstDecklinkClock *self =
      GST_DECKLINK_CLOCK (g_object_new (GST_TYPE_DECKLINK_CLOCK, "name", name,
          "clock-type", GST_CLOCK_TYPE_OTHER, NULL));

  gst_object_ref_sink (self);

  return GST_CLOCK_CAST (self);
}

static GstClockTime
gst_decklink_clock_get_internal_time (GstClock * clock)
{
  GstDecklinkClock *self = GST_DECKLINK_CLOCK (clock);
  GstClockTime result, start_time, last_time;
  GstClockTimeDiff offset;
  BMDTimeValue time;
  HRESULT ret;

  g_mutex_lock (&self->output->lock);
  start_time = self->output->clock_start_time;
  offset = self->output->clock_offset;
  last_time = self->output->clock_last_time;
  time = -1;
  if (!self->output->started) {
    result = last_time;
    ret = -1;
  } else {
    ret =
        self->output->output->GetHardwareReferenceClock (GST_SECOND, &time,
        NULL, NULL);
    if (ret == S_OK && time >= 0) {
      result = time;

      if (start_time == GST_CLOCK_TIME_NONE)
        start_time = self->output->clock_start_time = result;

      if (result > start_time)
        result -= start_time;
      else
        result = 0;

      if (self->output->clock_restart) {
        self->output->clock_offset = result - last_time;
        offset = self->output->clock_offset;
        self->output->clock_restart = FALSE;
      }
      result = MAX (last_time, result);
      result -= offset;
      result = MAX (last_time, result);
    } else {
      result = last_time;
    }

    self->output->clock_last_time = result;
  }
  result += self->output->clock_epoch;
  g_mutex_unlock (&self->output->lock);

  GST_LOG_OBJECT (clock,
      "result %" GST_TIME_FORMAT " time %" GST_TIME_FORMAT " last time %"
      GST_TIME_FORMAT " offset %" GST_TIME_FORMAT " start time %"
      GST_TIME_FORMAT " (ret: 0x%08lx)", GST_TIME_ARGS (result),
      GST_TIME_ARGS (time), GST_TIME_ARGS (last_time), GST_TIME_ARGS (offset),
      GST_TIME_ARGS (start_time), (unsigned long) ret);

  return result;
}

void
decklink_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;
  if (g_once_init_enter (&res)) {
    GST_DEBUG_CATEGORY_INIT (gst_decklink_debug, "decklink", 0,
        "debug category for decklink plugin");
    gst_type_mark_as_plugin_api (GST_TYPE_DECKLINK_AUDIO_CHANNELS, (GstPluginAPIFlags) 0);
    gst_type_mark_as_plugin_api (GST_TYPE_DECKLINK_AUDIO_CONNECTION, (GstPluginAPIFlags) 0);
    gst_type_mark_as_plugin_api (GST_TYPE_DECKLINK_PROFILE_ID, (GstPluginAPIFlags) 0);
    gst_type_mark_as_plugin_api (GST_TYPE_DECKLINK_KEYER_MODE, (GstPluginAPIFlags) 0);
    gst_type_mark_as_plugin_api (GST_TYPE_DECKLINK_MODE, (GstPluginAPIFlags) 0);
    gst_type_mark_as_plugin_api (GST_TYPE_DECKLINK_TIMECODE_FORMAT, (GstPluginAPIFlags) 0);
    gst_type_mark_as_plugin_api (GST_TYPE_DECKLINK_VIDEO_FORMAT, (GstPluginAPIFlags) 0);
    gst_type_mark_as_plugin_api (GST_TYPE_DECKLINK_CONNECTION, (GstPluginAPIFlags) 0);

    g_once_init_leave (&res, TRUE);
  }
}
