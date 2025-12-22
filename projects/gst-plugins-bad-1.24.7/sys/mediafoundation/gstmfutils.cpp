/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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
#include "config.h"
#endif

#include "gstmfconfig.h"

#include "gstmfutils.h"
#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

GST_DEBUG_CATEGORY_EXTERN (gst_mf_utils_debug);
#define GST_CAT_DEFAULT gst_mf_utils_debug

#define MAKE_RAW_FORMAT_CAPS(format) \
    "video/x-raw, format = (string) " format

/* No GUID is defined for "Y16 " in mfapi.h, but it's used by several devices */
DEFINE_MEDIATYPE_GUID (MFVideoFormat_Y16, FCC ('Y16 '));

static struct
{
  const GUID &mf_format;
  const gchar *caps_string;
  GstVideoFormat format;
} raw_video_format_map[] = {
  /* NOTE: when adding new format, gst_mf_update_video_info_with_stride() must
   * be updated as well */
  {MFVideoFormat_RGB32,  MAKE_RAW_FORMAT_CAPS ("BGRx"),  GST_VIDEO_FORMAT_BGRx},
  {MFVideoFormat_ARGB32, MAKE_RAW_FORMAT_CAPS ("BGRA"),  GST_VIDEO_FORMAT_BGRA},
  {MFVideoFormat_RGB565, MAKE_RAW_FORMAT_CAPS ("RGB16"), GST_VIDEO_FORMAT_RGB16},
  {MFVideoFormat_RGB555, MAKE_RAW_FORMAT_CAPS ("RGB15"), GST_VIDEO_FORMAT_RGB15},
  {MFVideoFormat_RGB24,  MAKE_RAW_FORMAT_CAPS ("BGR"),   GST_VIDEO_FORMAT_BGR},

  /* packed YUV */
  {MFVideoFormat_YUY2,   MAKE_RAW_FORMAT_CAPS ("YUY2"),  GST_VIDEO_FORMAT_YUY2},
  {MFVideoFormat_YVYU,   MAKE_RAW_FORMAT_CAPS ("YVYU"),  GST_VIDEO_FORMAT_YVYU},
  {MFVideoFormat_UYVY,   MAKE_RAW_FORMAT_CAPS ("UYVY"),  GST_VIDEO_FORMAT_UYVY},
  {MFVideoFormat_AYUV,   MAKE_RAW_FORMAT_CAPS ("VUYA"),  GST_VIDEO_FORMAT_VUYA},

  /* semi-planar */
  {MFVideoFormat_NV12,   MAKE_RAW_FORMAT_CAPS ("NV12"),  GST_VIDEO_FORMAT_NV12},
  {MFVideoFormat_P010,   MAKE_RAW_FORMAT_CAPS ("P010_10LE"),  GST_VIDEO_FORMAT_P010_10LE},
  {MFVideoFormat_P016,   MAKE_RAW_FORMAT_CAPS ("P016_LE"),  GST_VIDEO_FORMAT_P016_LE},

  /* planar */
  {MFVideoFormat_I420,   MAKE_RAW_FORMAT_CAPS ("I420"),  GST_VIDEO_FORMAT_I420},
  {MFVideoFormat_IYUV,   MAKE_RAW_FORMAT_CAPS ("I420"),  GST_VIDEO_FORMAT_I420},
  {MFVideoFormat_YV12,   MAKE_RAW_FORMAT_CAPS ("YV12"),  GST_VIDEO_FORMAT_YV12},

  /* complex format */
  {MFVideoFormat_v210,   MAKE_RAW_FORMAT_CAPS ("v210"),  GST_VIDEO_FORMAT_v210},
  {MFVideoFormat_v216,   MAKE_RAW_FORMAT_CAPS ("v216"),  GST_VIDEO_FORMAT_v216},

  /* gray */
  {MFVideoFormat_Y16,    MAKE_RAW_FORMAT_CAPS ("GRAY16_LE"),  GST_VIDEO_FORMAT_GRAY16_LE},
};

static struct
{
  const GUID &mf_format;
  const gchar *caps_string;
} encoded_video_format_map[] = {
  {MFVideoFormat_H264, "video/x-h264"},
  {MFVideoFormat_HEVC, "video/x-h265"},
  {MFVideoFormat_H265, "video/x-h265"},
  {MFVideoFormat_VP80, "video/x-vp8"},
  {MFVideoFormat_VP90, "video/x-vp9"},
  {MFVideoFormat_MJPG, "image/jpeg"},
};
/* *INDENT-ON* */

GstVideoFormat
gst_mf_video_subtype_to_video_format (const GUID * subtype)
{
  gint i;
  for (i = 0; i < G_N_ELEMENTS (raw_video_format_map); i++) {
    if (IsEqualGUID (raw_video_format_map[i].mf_format, *subtype))
      return raw_video_format_map[i].format;
  }

  return GST_VIDEO_FORMAT_UNKNOWN;
}

const GUID *
gst_mf_video_subtype_from_video_format (GstVideoFormat format)
{
  gint i;
  for (i = 0; i < G_N_ELEMENTS (raw_video_format_map); i++) {
    if (raw_video_format_map[i].format == format)
      return &raw_video_format_map[i].mf_format;
  }

  return nullptr;
}

static GstCaps *
gst_mf_media_type_to_video_caps (IMFMediaType * media_type)
{
  HRESULT hr;
  GstCaps *caps = nullptr;
  gint i;
  guint32 width = 0;
  guint32 height = 0;
  guint32 num, den;
  guint32 val;
  gchar *str;
  GUID subtype;
  GstVideoChromaSite chroma_site;
  GstVideoColorimetry colorimetry;
  gboolean raw_format = TRUE;

  hr = media_type->GetGUID (MF_MT_SUBTYPE, &subtype);
  if (FAILED (hr)) {
    GST_WARNING ("Failed to get subtype, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  for (i = 0; i < G_N_ELEMENTS (raw_video_format_map); i++) {
    if (IsEqualGUID (raw_video_format_map[i].mf_format, subtype)) {
      caps = gst_caps_from_string (raw_video_format_map[i].caps_string);
      break;
    }
  }

  if (!caps) {
    for (i = 0; i < G_N_ELEMENTS (encoded_video_format_map); i++) {
      if (IsEqualGUID (encoded_video_format_map[i].mf_format, subtype)) {
        caps = gst_caps_from_string (encoded_video_format_map[i].caps_string);
        raw_format = FALSE;
        break;
      }
    }
  }

  if (!caps) {
    GST_WARNING ("Unknown format %" GST_FOURCC_FORMAT,
        GST_FOURCC_ARGS (subtype.Data1));
    return nullptr;
  }

  hr = MFGetAttributeSize (media_type, MF_MT_FRAME_SIZE, &width, &height);
  if (FAILED (hr) || !width || !height) {
    GST_WARNING ("Couldn't get frame size, hr: 0x%x", (guint) hr);
    if (raw_format) {
      gst_caps_unref (caps);

      return nullptr;
    }
  }

  if (width > 0 && height > 0) {
    gst_caps_set_simple (caps, "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height, nullptr);
  }

  hr = MFGetAttributeRatio (media_type, MF_MT_FRAME_RATE, &num, &den);
  if (SUCCEEDED (hr) && num > 0 && den > 0)
    gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION, num, den,
        nullptr);

  hr = MFGetAttributeRatio (media_type, MF_MT_PIXEL_ASPECT_RATIO, &num, &den);
  if (SUCCEEDED (hr) && num > 0 && den > 0)
    gst_caps_set_simple (caps,
        "pixel-aspect-ratio", GST_TYPE_FRACTION, num, den, nullptr);

  colorimetry.range = GST_VIDEO_COLOR_RANGE_UNKNOWN;
  colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_UNKNOWN;
  colorimetry.transfer = GST_VIDEO_TRANSFER_UNKNOWN;
  colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_UNKNOWN;

  hr = media_type->GetUINT32 (MF_MT_VIDEO_NOMINAL_RANGE, &val);
  if (SUCCEEDED (hr)) {
    switch (val) {
      case MFNominalRange_0_255:
        colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;
        break;
      case MFNominalRange_16_235:
        colorimetry.range = GST_VIDEO_COLOR_RANGE_16_235;
        break;
      default:
        break;
    }
  }

  hr = media_type->GetUINT32 (MF_MT_VIDEO_PRIMARIES, &val);
  if (SUCCEEDED (hr)) {
    switch (val) {
      case MFVideoPrimaries_BT709:
        colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
        break;
      case MFVideoPrimaries_BT470_2_SysM:
        colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT470M;
        break;
      case MFVideoPrimaries_BT470_2_SysBG:
        colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT470BG;
        break;
      case MFVideoPrimaries_SMPTE170M:
        colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTE170M;
        break;
      case MFVideoPrimaries_SMPTE240M:
        colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTE240M;
        break;
      case MFVideoPrimaries_EBU3213:
        colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_EBU3213;
        break;
      case MFVideoPrimaries_BT2020:
        colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
        break;
      default:
        GST_FIXME ("unhandled color primaries %d", val);
        break;
    }
  }

  hr = media_type->GetUINT32 (MF_MT_YUV_MATRIX, &val);
  if (SUCCEEDED (hr)) {
    switch (val) {
      case MFVideoTransferMatrix_BT709:
        colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT709;
        break;
      case MFVideoTransferMatrix_BT601:
        colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT601;
        break;
      case MFVideoTransferMatrix_SMPTE240M:
        colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_SMPTE240M;
        break;
      case MFVideoTransferMatrix_BT2020_10:
      case MFVideoTransferMatrix_BT2020_12:
        colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT2020;
        break;
      default:
        GST_FIXME ("unhandled color matrix %d", val);
        break;
    }
  }

  hr = media_type->GetUINT32 (MF_MT_TRANSFER_FUNCTION, &val);
  if (SUCCEEDED (hr)) {
    switch (val) {
      case MFVideoTransFunc_10:
        colorimetry.transfer = GST_VIDEO_TRANSFER_GAMMA10;
        break;
      case MFVideoTransFunc_18:
        colorimetry.transfer = GST_VIDEO_TRANSFER_GAMMA18;
        break;
      case MFVideoTransFunc_20:
        colorimetry.transfer = GST_VIDEO_TRANSFER_GAMMA20;
        break;
      case MFVideoTransFunc_22:
        colorimetry.transfer = GST_VIDEO_TRANSFER_GAMMA22;
        break;
      case MFVideoTransFunc_709:
      case MFVideoTransFunc_709_sym:
        colorimetry.transfer = GST_VIDEO_TRANSFER_BT709;
        break;
      case MFVideoTransFunc_240M:
        colorimetry.transfer = GST_VIDEO_TRANSFER_SMPTE240M;
        break;
      case MFVideoTransFunc_sRGB:
        colorimetry.transfer = GST_VIDEO_TRANSFER_SRGB;
        break;
      case MFVideoTransFunc_28:
        colorimetry.transfer = GST_VIDEO_TRANSFER_GAMMA28;
        break;
      case MFVideoTransFunc_Log_100:
        colorimetry.transfer = GST_VIDEO_TRANSFER_LOG100;
        break;
      case MFVideoTransFunc_Log_316:
        colorimetry.transfer = GST_VIDEO_TRANSFER_LOG316;
        break;
      case MFVideoTransFunc_2020_const:
      case MFVideoTransFunc_2020:
        colorimetry.transfer = GST_VIDEO_TRANSFER_BT2020_10;
        break;
      case MFVideoTransFunc_2084:
        colorimetry.transfer = GST_VIDEO_TRANSFER_SMPTE2084;
        break;
      case MFVideoTransFunc_HLG:
        colorimetry.transfer = GST_VIDEO_TRANSFER_ARIB_STD_B67;
        break;
      default:
        GST_FIXME ("unhandled color transfer %d", val);
        break;
    }
  }

  str = gst_video_colorimetry_to_string (&colorimetry);
  if (str) {
    gst_caps_set_simple (caps, "colorimetry", G_TYPE_STRING, str, nullptr);
    g_free (str);
    str = nullptr;
  }

  chroma_site = GST_VIDEO_CHROMA_SITE_UNKNOWN;

  hr = media_type->GetUINT32 (MF_MT_VIDEO_CHROMA_SITING, &val);
  if (SUCCEEDED (hr)) {
    gboolean known_value = TRUE;

    if ((val & MFVideoChromaSubsampling_MPEG2) ==
        MFVideoChromaSubsampling_MPEG2) {
      chroma_site = GST_VIDEO_CHROMA_SITE_MPEG2;
    } else if ((val & MFVideoChromaSubsampling_DV_PAL) ==
        MFVideoChromaSubsampling_DV_PAL) {
      chroma_site = GST_VIDEO_CHROMA_SITE_DV;
    } else if ((val & MFVideoChromaSubsampling_Cosited) ==
        MFVideoChromaSubsampling_Cosited) {
      chroma_site = GST_VIDEO_CHROMA_SITE_COSITED;
    } else {
      known_value = FALSE;
    }

    GST_LOG ("have %s chroma site value 0x%x",
        known_value ? "known" : "unknown", val);
  }

  if (chroma_site != GST_VIDEO_CHROMA_SITE_UNKNOWN)
    gst_caps_set_simple (caps, "chroma-site", G_TYPE_STRING,
        gst_video_chroma_to_string (chroma_site), nullptr);

  return caps;
}

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
  guint64 mf_pos;
  GstAudioChannelPosition gst_pos;
} mf_to_gst_pos[] = {
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

static void
gst_mf_media_audio_channel_mask_to_position (guint channels, DWORD mask,
    GstAudioChannelPosition * position)
{
  guint i, ch;

  for (i = 0, ch = 0; i < G_N_ELEMENTS (mf_to_gst_pos) && ch < channels; i++) {
    if ((mask & mf_to_gst_pos[i].mf_pos) == 0)
      continue;

    position[ch] = mf_to_gst_pos[i].gst_pos;
    ch++;
  }
}

static GstCaps *
gst_mf_media_type_to_audio_caps (IMFMediaType * media_type)
{
  GUID subtype;
  HRESULT hr;
  UINT32 bps;
  GstAudioFormat format = GST_AUDIO_FORMAT_UNKNOWN;
  GstAudioInfo info;
  UINT32 rate, channels, mask;
  GstAudioChannelPosition position[64];

  hr = media_type->GetGUID (MF_MT_SUBTYPE, &subtype);
  if (FAILED (hr)) {
    GST_WARNING ("failed to get subtype, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  if (!IsEqualGUID (subtype, MFAudioFormat_PCM) &&
      !IsEqualGUID (subtype, MFAudioFormat_Float)) {
    GST_FIXME ("Unknown subtype");
    return nullptr;
  }

  hr = media_type->GetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, &bps);
  if (FAILED (hr)) {
    GST_WARNING ("Failed to get bps, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  if (IsEqualGUID (subtype, MFAudioFormat_PCM)) {
    format = gst_audio_format_build_integer (TRUE, G_LITTLE_ENDIAN, bps, bps);
  } else if (bps == 32) {
    format = GST_AUDIO_FORMAT_F32LE;
  } else if (bps == 64) {
    format = GST_AUDIO_FORMAT_F64LE;
  }

  if (format == GST_AUDIO_FORMAT_UNKNOWN) {
    GST_WARNING ("Unknown audio format");
    return nullptr;
  }

  hr = media_type->GetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, &channels);
  if (FAILED (hr) || channels == 0) {
    GST_WARNING ("Unknown channels");
    return nullptr;
  }

  hr = media_type->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
  if (FAILED (hr) || rate == 0) {
    GST_WARNING ("Unknown rate");
    return nullptr;
  }

  for (guint i = 0; i < G_N_ELEMENTS (position); i++)
    position[i] = GST_AUDIO_CHANNEL_POSITION_NONE;

  hr = media_type->GetUINT32 (MF_MT_AUDIO_CHANNEL_MASK, &mask);
  if (FAILED (hr)) {
    if (channels == 1) {
      position[0] = GST_AUDIO_CHANNEL_POSITION_MONO;
    } else if (channels == 2) {
      position[0] = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
      position[1] = GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
    } else if (channels <= 8) {
      GST_WARNING ("Unknown channel position, use default value");
      gst_mf_media_audio_channel_mask_to_position (channels,
          default_ch_masks[channels], position);
    } else {
      GST_WARNING ("Failed to determine channel position");
      return nullptr;
    }
  } else {
    gst_mf_media_audio_channel_mask_to_position (channels, mask, position);
  }

  gst_audio_info_set_format (&info, format, rate, channels, position);

  return gst_audio_info_to_caps (&info);
}

GstCaps *
gst_mf_media_type_to_caps (IMFMediaType * media_type)
{
  GUID major_type;
  HRESULT hr;

  g_return_val_if_fail (media_type != nullptr, nullptr);

  hr = media_type->GetMajorType (&major_type);
  if (FAILED (hr)) {
    GST_WARNING ("failed to get major type, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  if (IsEqualGUID (major_type, MFMediaType_Video)) {
    return gst_mf_media_type_to_video_caps (media_type);
  } else if (IsEqualGUID (major_type, MFMediaType_Audio)) {
    return gst_mf_media_type_to_audio_caps (media_type);
  }

  return nullptr;
}

void
gst_mf_media_type_release (IMFMediaType * media_type)
{
  if (media_type)
    media_type->Release ();
}

gboolean
gst_mf_update_video_info_with_stride (GstVideoInfo * info, gint stride)
{
  guint width, height, cr_h;

  g_return_val_if_fail (info != nullptr, FALSE);
  g_return_val_if_fail (stride > 0, FALSE);
  g_return_val_if_fail (GST_VIDEO_INFO_FORMAT (info)
      != GST_VIDEO_FORMAT_UNKNOWN, FALSE);

  if (GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_ENCODED)
    return TRUE;

  width = GST_VIDEO_INFO_WIDTH (info);
  height = GST_VIDEO_INFO_HEIGHT (info);

  /* copied from video-info */
  switch (GST_VIDEO_INFO_FORMAT (info)) {
      /* RGB */
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGB16:
    case GST_VIDEO_FORMAT_BGR15:
    case GST_VIDEO_FORMAT_BGR:
      info->stride[0] = stride;
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
      /* packed YUV */
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_YVYU:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_VUYA:
      info->stride[0] = stride;
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
      /* semi-planar */
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P016_LE:
      if (height % 2) {
        GST_ERROR ("Height must be even number");
        return FALSE;
      }

      cr_h = height / 2;

      info->stride[0] = stride;
      info->stride[1] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->size = info->offset[1] + info->stride[0] * cr_h;
      break;
      /* planar */
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
      if (stride % 2) {
        GST_ERROR ("Stride must be even number");
        return FALSE;
      }

      if (height % 2) {
        GST_ERROR ("Height must be even number");
        return FALSE;
      }

      cr_h = height / 2;

      info->stride[0] = stride;
      info->stride[1] = stride / 2;
      info->stride[2] = info->stride[1];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->offset[2] = info->offset[1] + info->stride[1] * cr_h;
      info->size = info->offset[2] + info->stride[2] * cr_h;
      break;
      /* complex */
    case GST_VIDEO_FORMAT_v210:
    case GST_VIDEO_FORMAT_v216:
      info->stride[0] = stride;
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
      /* gray */
    case GST_VIDEO_FORMAT_GRAY16_LE:
      info->stride[0] = stride;
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
    default:
      GST_ERROR ("Unhandled format %s",
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (info)));
      return FALSE;
  }

  return TRUE;
}

gboolean
_gst_mf_result (HRESULT hr, GstDebugCategory * cat, const gchar * file,
    const gchar * function, gint line)
{
#ifndef GST_DISABLE_GST_DEBUG
  gboolean ret = TRUE;

  if (FAILED (hr)) {
    gchar *error_text = nullptr;

    error_text = g_win32_error_message ((gint) hr);
    /* g_win32_error_message() doesn't cover all HERESULT return code,
     * so it could be empty string, or null if there was an error
     * in g_utf16_to_utf8() */
    gst_debug_log (cat, GST_LEVEL_WARNING, file, function, line,
        nullptr, "MediaFoundation call failed: 0x%x, %s", (guint) hr,
        GST_STR_NULL (error_text));
    g_free (error_text);

    ret = FALSE;
  }

  return ret;
#else
  return SUCCEEDED (hr);
#endif
}

/* Reference:
 * https://docs.microsoft.com/en-us/windows/win32/medfound/media-type-debugging-code */
#define GST_MF_IF_EQUAL_RETURN(guid,val) G_STMT_START { \
  if (IsEqualGUID (guid, val)) \
    return G_STRINGIFY (val); \
} G_STMT_END

static const gchar *
gst_mf_guid_to_static_string (const GUID & guid)
{
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_MAJOR_TYPE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_MAJOR_TYPE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_SUBTYPE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_ALL_SAMPLES_INDEPENDENT);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_FIXED_SIZE_SAMPLES);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_COMPRESSED);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_SAMPLE_SIZE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_WRAPPED_TYPE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_NUM_CHANNELS);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_SAMPLES_PER_SECOND);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_AVG_BYTES_PER_SECOND);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_BLOCK_ALIGNMENT);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_BITS_PER_SAMPLE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_VALID_BITS_PER_SAMPLE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_SAMPLES_PER_BLOCK);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_CHANNEL_MASK);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_FOLDDOWN_MATRIX);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_WMADRC_PEAKREF);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_WMADRC_PEAKTARGET);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_WMADRC_AVGREF);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_WMADRC_AVGTARGET);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AUDIO_PREFER_WAVEFORMATEX);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AAC_PAYLOAD_TYPE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_FRAME_SIZE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_FRAME_RATE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_FRAME_RATE_RANGE_MAX);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_FRAME_RATE_RANGE_MIN);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_PIXEL_ASPECT_RATIO);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_DRM_FLAGS);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_PAD_CONTROL_FLAGS);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_SOURCE_CONTENT_HINT);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_VIDEO_CHROMA_SITING);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_INTERLACE_MODE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_TRANSFER_FUNCTION);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_VIDEO_PRIMARIES);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_YUV_MATRIX);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_VIDEO_LIGHTING);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_VIDEO_NOMINAL_RANGE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_GEOMETRIC_APERTURE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_MINIMUM_DISPLAY_APERTURE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_PAN_SCAN_APERTURE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_PAN_SCAN_ENABLED);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AVG_BITRATE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AVG_BIT_ERROR_RATE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_MAX_KEYFRAME_SPACING);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_DEFAULT_STRIDE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_PALETTE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_USER_DATA);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_MPEG_START_TIME_CODE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_MPEG2_PROFILE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_MPEG2_LEVEL);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_MPEG2_FLAGS);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_MPEG_SEQUENCE_HEADER);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_DV_AAUX_SRC_PACK_0);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_DV_AAUX_CTRL_PACK_0);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_DV_AAUX_SRC_PACK_1);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_DV_AAUX_CTRL_PACK_1);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_DV_VAUX_SRC_PACK);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_DV_VAUX_CTRL_PACK);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_IMAGE_LOSS_TOLERANT);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_MPEG4_SAMPLE_DESCRIPTION);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_MPEG4_CURRENT_SAMPLE_ENTRY);

  GST_MF_IF_EQUAL_RETURN (guid, MFMediaType_Audio);
  GST_MF_IF_EQUAL_RETURN (guid, MFMediaType_Video);
  GST_MF_IF_EQUAL_RETURN (guid, MFMediaType_Protected);
  GST_MF_IF_EQUAL_RETURN (guid, MFMediaType_SAMI);
  GST_MF_IF_EQUAL_RETURN (guid, MFMediaType_Script);
  GST_MF_IF_EQUAL_RETURN (guid, MFMediaType_Image);
  GST_MF_IF_EQUAL_RETURN (guid, MFMediaType_HTML);
  GST_MF_IF_EQUAL_RETURN (guid, MFMediaType_Binary);
  GST_MF_IF_EQUAL_RETURN (guid, MFMediaType_FileTransfer);

  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_AI44);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_ARGB32);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_AYUV);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_DV25);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_DV50);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_DVH1);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_DVSD);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_DVSL);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_H264);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_H265);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_HEVC);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_HEVC_ES);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_I420);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_IYUV);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_M4S2);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_MJPG);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_MP43);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_MP4S);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_MP4V);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_MPG1);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_MSS1);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_MSS2);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_NV11);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_NV12);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_P010);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_P016);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_P210);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_P216);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_RGB24);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_RGB32);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_RGB555);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_RGB565);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_RGB8);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_UYVY);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_v210);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_v410);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_VP80);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_VP90);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_WMV1);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_WMV2);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_WMV3);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_WVC1);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_Y210);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_Y216);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_Y410);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_Y416);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_Y41P);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_Y41T);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_YUY2);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_YV12);
  GST_MF_IF_EQUAL_RETURN (guid, MFVideoFormat_YVYU);

  /* WAVE_FORMAT_PCM */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_PCM);
  /* WAVE_FORMAT_IEEE_FLOAT */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_Float);
  /* WAVE_FORMAT_DTS */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_DTS);
  /* WAVE_FORMAT_DOLBY_AC3_SPDIF */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_Dolby_AC3_SPDIF);
  /* WAVE_FORMAT_DRM */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_DRM);
  /* WAVE_FORMAT_WMAUDIO2 */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_WMAudioV8);
  /* WAVE_FORMAT_WMAUDIO3 */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_WMAudioV9);
  /* WAVE_FORMAT_WMAUDIO_LOSSLESS */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_WMAudio_Lossless);
  /* WAVE_FORMAT_WMASPDIF */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_WMASPDIF);
  /* WAVE_FORMAT_WMAVOICE9 */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_MSP1);
  /* WAVE_FORMAT_MPEGLAYER3 */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_MP3);
  /* WAVE_FORMAT_MPEG */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_MPEG);
  /* WAVE_FORMAT_MPEG_HEAAC */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_AAC);
  /* WAVE_FORMAT_MPEG_ADTS_AAC */
  GST_MF_IF_EQUAL_RETURN (guid, MFAudioFormat_ADTS);

#if GST_MF_WINAPI_DESKTOP
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_CUSTOM_VIDEO_PRIMARIES);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_AM_FORMAT_TYPE);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_ARBITRARY_HEADER);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_ARBITRARY_FORMAT);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_ORIGINAL_4CC);
  GST_MF_IF_EQUAL_RETURN (guid, MF_MT_ORIGINAL_WAVE_FORMAT_TAG);
#endif

  return nullptr;
}

static gchar *
gst_mf_guid_to_string (const GUID & guid)
{
  const gchar *str = nullptr;
  HRESULT hr;
  WCHAR *name = nullptr;
  gchar *ret = nullptr;

  str = gst_mf_guid_to_static_string (guid);
  if (str)
    return g_strdup (str);

  hr = StringFromCLSID (guid, &name);
  if (gst_mf_result (hr) && name) {
    ret =
        g_utf16_to_utf8 ((const gunichar2 *) name, -1, nullptr, nullptr,
        nullptr);
    CoTaskMemFree (name);

    if (ret)
      return ret;
  }

  ret = g_strdup_printf
      ("%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
      (guint) guid.Data1, (guint) guid.Data2, (guint) guid.Data3,
      guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
      guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

  return ret;
}

static gchar *
gst_mf_attribute_value_to_string (const GUID & guid, const PROPVARIANT & var)
{
  if (IsEqualGUID (guid, MF_MT_FRAME_RATE) ||
      IsEqualGUID (guid, MF_MT_FRAME_RATE_RANGE_MAX) ||
      IsEqualGUID (guid, MF_MT_FRAME_RATE_RANGE_MIN) ||
      IsEqualGUID (guid, MF_MT_FRAME_SIZE) ||
      IsEqualGUID (guid, MF_MT_PIXEL_ASPECT_RATIO)) {
    UINT32 high = 0, low = 0;

    Unpack2UINT32AsUINT64 (var.uhVal.QuadPart, &high, &low);
    return g_strdup_printf ("%dx%d", high, low);
  }

  if (IsEqualGUID (guid, MF_MT_GEOMETRIC_APERTURE) ||
      IsEqualGUID (guid, MF_MT_MINIMUM_DISPLAY_APERTURE) ||
      IsEqualGUID (guid, MF_MT_PAN_SCAN_APERTURE)) {
    /* FIXME: Not our usecase for now */
    return g_strdup ("Not parsed");
  }

  switch (var.vt) {
    case VT_UI4:
      return g_strdup_printf ("%d", var.ulVal);
    case VT_UI8:
      return g_strdup_printf ("%" G_GUINT64_FORMAT, var.uhVal);
    case VT_R8:
      return g_strdup_printf ("%f", var.dblVal);
    case VT_CLSID:
      return gst_mf_guid_to_string (*var.puuid);
    case VT_LPWSTR:
      return g_utf16_to_utf8 ((const gunichar2 *) var.pwszVal,
          -1, nullptr, nullptr, nullptr);
    case VT_UNKNOWN:
      return g_strdup ("IUnknown");
    default:
      return g_strdup_printf ("Unhandled type (vt = %d)", var.vt);
  }

  return nullptr;
}

static void
gst_mf_dump_attribute_value_by_index (IMFAttributes * attr, const gchar * msg,
    guint index, GstDebugLevel level, GstDebugCategory * cat,
    const gchar * file, const gchar * function, gint line)
{
  gchar *guid_name = nullptr;
  gchar *value = nullptr;
  GUID guid = GUID_NULL;
  HRESULT hr;

  PROPVARIANT var;
  PropVariantInit (&var);

  hr = attr->GetItemByIndex (index, &guid, &var);
  if (!gst_mf_result (hr))
    goto done;

  guid_name = gst_mf_guid_to_string (guid);
  if (!guid_name)
    goto done;

  value = gst_mf_attribute_value_to_string (guid, var);
  if (!value)
    goto done;

  gst_debug_log (cat, level, file, function, line,
      nullptr, "%s attribute %d, %s: %s", msg ? msg : "", index, guid_name,
      value);

done:
  PropVariantClear (&var);
  g_free (guid_name);
  g_free (value);
}

void
_gst_mf_dump_attributes (IMFAttributes * attr, const gchar * msg,
    GstDebugLevel level, GstDebugCategory * cat, const gchar * file,
    const gchar * function, gint line)
{
#ifndef GST_DISABLE_GST_DEBUG
  HRESULT hr;
  UINT32 count = 0, i;

  if (!attr)
    return;

  hr = attr->GetCount (&count);
  if (!gst_mf_result (hr) || count == 0)
    return;

  for (i = 0; i < count; i++) {
    gst_mf_dump_attribute_value_by_index (attr,
        msg, i, level, cat, file, function, line);
  }
#endif
}
