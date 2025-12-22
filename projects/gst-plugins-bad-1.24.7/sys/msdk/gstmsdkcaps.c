/* GStreamer Intel MSDK plugin
 * Copyright (c) 2023, Intel Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gstmsdkcaps.h"

#ifndef _WIN32
#include <libdrm/drm_fourcc.h>
#include <gst/video/video-info-dma.h>
#include "gstmsdkallocator_libva.h"
#include <gst/va/gstvavideoformat.h>
#endif

#define DEFAULT_DELIMITER ", "
#define PROFILE_DELIMITER DEFAULT_DELIMITER

#define DEFAULT_VIDEO_FORMAT GST_VIDEO_FORMAT_NV12

#define ENC_IOPATTERN MFX_IOPATTERN_IN_VIDEO_MEMORY
#define DEC_IOPATTERN MFX_IOPATTERN_OUT_VIDEO_MEMORY
#define VPP_IOPATTERN \
    MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY

#ifdef _WIN32
/* fix "unreferenced local variable" for windows */
#define VAR_UNUSED(v) (void)(v)
#endif

#if (MFX_VERSION >= 2000)
static guint default_width = GST_ROUND_UP_16 (320);
static guint default_height = GST_ROUND_UP_16 (240);

static guint max_res_widths[] = { 640, 1280, 1920, 2048, 4096, 8192, 16384 };

static guint max_res_heights[] = {
  480, 720, 1080, 1920, 2048, 4096, 8192, 12288, 16384
};
#endif

typedef struct
{
  mfxU32 id;
  const gchar *names;
} Profile;

static const Profile profs_avc[] = {
  {MFX_PROFILE_AVC_MAIN, "main"},
  {MFX_PROFILE_AVC_BASELINE, "baseline"},
  {MFX_PROFILE_AVC_EXTENDED, "extended"},
  {MFX_PROFILE_AVC_HIGH, "high"},
  {MFX_PROFILE_AVC_CONSTRAINED_BASELINE, "constrained-baseline"},
  {MFX_PROFILE_AVC_CONSTRAINED_HIGH, "constrained-high"},
  {MFX_PROFILE_AVC_PROGRESSIVE_HIGH, "progressive-high"},
  {MFX_PROFILE_UNKNOWN, NULL}
};

static const Profile profs_hevc[] = {
  {MFX_PROFILE_HEVC_MAIN, "main"},
  {MFX_PROFILE_HEVC_MAIN10, "main-10, main-10-still-picture"},
  {MFX_PROFILE_HEVC_MAINSP, "main-still-picture"},
  {MFX_PROFILE_HEVC_REXT, "main-444, main-444-10, main-422-10, main-12"},
#if (MFX_VERSION >= 1032)
  {MFX_PROFILE_HEVC_SCC, "screen-extended-main, screen-extended-main-10, "
        "screen-extended-main-444, screen-extended-main-444-10"},
#endif
  {MFX_PROFILE_UNKNOWN, NULL}
};

static const Profile profs_mpeg2[] = {
  {MFX_PROFILE_MPEG2_MAIN, "main"},
  {MFX_PROFILE_MPEG2_SIMPLE, "simple"},
  {MFX_PROFILE_MPEG2_HIGH, "high"},
  {MFX_PROFILE_UNKNOWN, NULL}
};

static const Profile profs_vc1[] = {
  {MFX_PROFILE_VC1_MAIN, "main"},
  {MFX_PROFILE_VC1_SIMPLE, "simple"},
  {MFX_PROFILE_VC1_ADVANCED, "advanced"},
  {MFX_PROFILE_UNKNOWN, NULL}
};

static const Profile profs_vp8[] = {
  {MFX_PROFILE_VP8_0, "0"},
  {MFX_PROFILE_VP8_1, "1"},
  {MFX_PROFILE_VP8_2, "2"},
  {MFX_PROFILE_VP8_3, "3"},
  {MFX_PROFILE_UNKNOWN, NULL}
};

static const Profile profs_vp9[] = {
  {MFX_PROFILE_VP9_0, "0"},
  {MFX_PROFILE_VP9_1, "1"},
  {MFX_PROFILE_VP9_2, "2"},
  {MFX_PROFILE_VP9_3, "3"},
  {MFX_PROFILE_UNKNOWN, NULL}
};

static const Profile profs_av1[] = {
#if (MFX_VERSION >= 1034)
  {MFX_PROFILE_AV1_MAIN, "main"},
  {MFX_PROFILE_AV1_HIGH, "high"},
  {MFX_PROFILE_AV1_PRO, "pro"},
#endif
  {MFX_PROFILE_UNKNOWN, NULL}
};

static const Profile profs_jpeg[] = {
  {MFX_PROFILE_JPEG_BASELINE, "baseline"},
  {MFX_PROFILE_UNKNOWN, NULL}
};

static const struct CodecProfiles
{
  guint codec;
  const gchar *media_type;
  const Profile *profiles;
} codec_profs[] = {
  {MFX_CODEC_AVC, "video/x-h264", profs_avc},
  {MFX_CODEC_HEVC, "video/x-h265", profs_hevc},
  {MFX_CODEC_MPEG2, "video/mpeg", profs_mpeg2},
  {MFX_CODEC_VC1, "video/x-wmv", profs_vc1},
  {MFX_CODEC_VP8, "video/x-vp8", profs_vp8},
  {MFX_CODEC_VP9, "video/x-vp9", profs_vp9},
  {MFX_CODEC_AV1, "video/x-av1", profs_av1},
  {MFX_CODEC_JPEG, "image/jpeg", profs_jpeg}
};

typedef struct
{
  guint min_width;
  guint max_width;
  guint min_height;
  guint max_height;
} ResolutionRange;

typedef gboolean (*IsParamSupportedFunc) (mfxSession * session,
    mfxVideoParam * in, mfxVideoParam * out);

gboolean
gst_msdkcaps_has_feature (const GstCaps * caps, const gchar * feature)
{
  guint i;

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    GstCapsFeatures *const features = gst_caps_get_features (caps, i);
    /* Skip ANY features, we need an exact match for correct evaluation */
    if (gst_caps_features_is_any (features))
      continue;
    if (gst_caps_features_contains (features, feature))
      return TRUE;
  }

  return FALSE;
}

static void
_list_append_string (GValue * list, const gchar * str)
{
  GValue gval = G_VALUE_INIT;

  g_return_if_fail (list != NULL);
  g_return_if_fail (str != NULL);

  g_value_init (&gval, G_TYPE_STRING);
  g_value_set_string (&gval, str);

  gst_value_list_append_value (list, &gval);
  g_value_unset (&gval);
}

static gboolean
_strings_to_list (const gchar * strings, GValue * list)
{
  gchar **strs = NULL;

  if (!strings || !list)
    return FALSE;

  strs = g_strsplit (strings, DEFAULT_DELIMITER, 0);
  for (guint i = 0; strs[i]; i++)
    _list_append_string (list, strs[i]);
  g_strfreev (strs);

  return TRUE;
}

static const gchar *
_get_media_type (guint codec)
{
  for (int c = 0; c < G_N_ELEMENTS (codec_profs); c++) {
    if (codec_profs[c].codec == codec)
      return codec_profs[c].media_type;
  }

  return NULL;
}

#ifndef _WIN32
/* For RGB cases, the byte orders are different between vaImageFormat (LSB) and
 * GStreamer video format (MSB). This function corrects the mapping between
 * different order manners.
 */
static void
_fix_map (GstMsdkContext * context)
{
  GstVaDisplay *display = NULL;
  VAImageFormat *va_formats;
  VADisplay dpy;
  VAStatus status;
  int max, num = 0;

  display = (GstVaDisplay *) gst_msdk_context_get_va_display (context);
  dpy = gst_va_display_get_va_dpy (display);
  gst_object_unref (display);

  max = vaMaxNumImageFormats (dpy);
  if (max == 0)
    return;

  va_formats = g_new (VAImageFormat, max);
  status = vaQueryImageFormats (dpy, va_formats, &num);
  gst_va_video_format_fix_map (va_formats, num);

  if (status != VA_STATUS_SUCCESS)
    GST_WARNING ("vaQueryImageFormats: %s", vaErrorStr (status));

  g_free (va_formats);
  return;
}

static gboolean
_dma_fmt_to_dma_drm_fmts (GstMsdkContext * context,
    GstMsdkContextJobType job_type,
    const GValue * dma_fmts, GValue * dma_drm_fmts)
{
  const gchar *fmt_str;
  gchar *drm_fmt_str;
  guint32 drm_fourcc;
  guint64 modifier;
  GstVideoFormat fmt;
  GValue gval = G_VALUE_INIT;
  GValue mods = G_VALUE_INIT;

  g_return_val_if_fail (dma_fmts != NULL, FALSE);
  g_return_val_if_fail (context != NULL, FALSE);

  fmt_str = g_value_get_string (dma_fmts);
  fmt = gst_video_format_from_string (fmt_str);

  g_return_val_if_fail (fmt != GST_VIDEO_FORMAT_UNKNOWN, FALSE);

  _fix_map (context);

  drm_fourcc = gst_va_drm_fourcc_from_video_format (fmt);
  if (drm_fourcc == DRM_FORMAT_INVALID)
    return FALSE;

  g_value_init (&mods, GST_TYPE_LIST);
  g_value_init (&gval, G_TYPE_STRING);

  gst_msdk_get_supported_modifiers (context, job_type, fmt, &mods);

  for (gint m = 0; m < gst_value_list_get_size (&mods); m++) {
    const GValue *gmod = gst_value_list_get_value (&mods, m);
    modifier = g_value_get_uint64 (gmod);

    drm_fmt_str = gst_video_dma_drm_fourcc_to_string (drm_fourcc, modifier);
    if (!drm_fmt_str)
      continue;

    g_value_set_string (&gval, drm_fmt_str);
    gst_value_list_append_value (dma_drm_fmts, &gval);

    GST_DEBUG ("Got modifier: %s", drm_fmt_str);
    g_free (drm_fmt_str);
  }
  g_value_unset (&mods);
  g_value_unset (&gval);

  return TRUE;
}

static gboolean
_dma_fmts_to_dma_drm_fmts (GstMsdkContext * context,
    GstMsdkContextJobType job_type,
    const GValue * dma_fmts, GValue * dma_drm_fmts)
{
  gint size = gst_value_list_get_size (dma_fmts);

  for (gint f = 0; f < size; f++) {
    const GValue *dma_fmt = gst_value_list_get_value (dma_fmts, f);
    if (!dma_fmt)
      continue;

    _dma_fmt_to_dma_drm_fmts (context, job_type, dma_fmt, dma_drm_fmts);
  }

  return TRUE;
}

static GstCaps *
_create_dma_drm_caps (GstMsdkContext * context,
    GstMsdkContextJobType job_type, const GValue * dma_formats)
{
  GstCaps *dma_drm_caps = NULL;
  GValue dma_drm_fmts = G_VALUE_INIT;

  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (dma_formats != NULL, FALSE);

  g_value_init (&dma_drm_fmts, GST_TYPE_LIST);

  if (GST_VALUE_HOLDS_LIST (dma_formats))
    _dma_fmts_to_dma_drm_fmts (context, job_type, dma_formats, &dma_drm_fmts);
  else if (G_VALUE_HOLDS_STRING (dma_formats))
    _dma_fmt_to_dma_drm_fmts (context, job_type, dma_formats, &dma_drm_fmts);

  if (gst_value_list_get_size (&dma_drm_fmts) > 0) {
    dma_drm_caps = gst_caps_from_string ("video/x-raw(memory:DMABuf)");
    gst_caps_set_simple (dma_drm_caps, "format", G_TYPE_STRING, "DMA_DRM",
        NULL);
    gst_caps_set_value (dma_drm_caps, "drm-format", &dma_drm_fmts);
  }

  g_value_unset (&dma_drm_fmts);

  return dma_drm_caps;
}
#endif

#if (MFX_VERSION >= 2000)

static gboolean
_fill_mfxframeinfo (GstVideoFormat format, mfxFrameInfo * frameinfo)
{
  if (format == GST_VIDEO_FORMAT_UNKNOWN)
    return FALSE;

  frameinfo->ChromaFormat = gst_msdk_get_mfx_chroma_from_format (format);
  frameinfo->FourCC = gst_msdk_get_mfx_fourcc_from_format (format);

  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_VUYA:
      frameinfo->BitDepthLuma = 8;
      frameinfo->BitDepthChroma = 8;
      frameinfo->Shift = 0;
      break;
    case GST_VIDEO_FORMAT_BGR10A2_LE:
      frameinfo->BitDepthLuma = 10;
      frameinfo->BitDepthChroma = 10;
      break;
    case GST_VIDEO_FORMAT_P010_10LE:
      frameinfo->BitDepthLuma = 10;
      frameinfo->BitDepthChroma = 10;
      frameinfo->Shift = 1;
      break;
#if (MFX_VERSION >= 1027)
    case GST_VIDEO_FORMAT_Y210:
      frameinfo->BitDepthLuma = 10;
      frameinfo->BitDepthChroma = 10;
      frameinfo->Shift = 1;
      break;
    case GST_VIDEO_FORMAT_Y410:
      frameinfo->BitDepthLuma = 10;
      frameinfo->BitDepthChroma = 10;
      frameinfo->Shift = 0;
      break;
#endif
#if (MFX_VERSION >= 1031)
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_Y212_LE:
    case GST_VIDEO_FORMAT_Y412_LE:
      frameinfo->BitDepthLuma = 12;
      frameinfo->BitDepthChroma = 12;
      frameinfo->Shift = 1;
      break;
#endif
#if (MFX_VERSION >=2004)
    case GST_VIDEO_FORMAT_RGBP:
    case GST_VIDEO_FORMAT_BGRP:
      frameinfo->BitDepthLuma = 8;
      frameinfo->BitDepthChroma = 8;
      frameinfo->Shift = 0;
      break;
#endif
    case GST_VIDEO_FORMAT_RGB16:
      /* Ignore parameters settings, do nothing */
      break;
    default:
      GST_WARNING ("Unsupported format %s",
          gst_video_format_to_string (format));
      return FALSE;
  }

  return TRUE;
}

static const gchar *
_profile_to_string (guint codec, mfxU32 profile)
{
  if (profile == MFX_PROFILE_UNKNOWN)
    return NULL;

  for (int c = 0; c < G_N_ELEMENTS (codec_profs); c++) {
    if (codec_profs[c].codec == codec) {
      const Profile *p = codec_profs[c].profiles;
      for (; p->id != MFX_PROFILE_UNKNOWN; p++) {
        if (p->id == profile)
          return p->names;
      }
    }
  }

  return NULL;
}

static mfxU16
_get_main_codec_profile (guint codec)
{
  for (int c = 0; c < G_N_ELEMENTS (codec_profs); c++) {
    if (codec_profs[c].codec == codec) {
      return codec_profs[c].profiles->id;
    }
  }

  return MFX_PROFILE_UNKNOWN;
}

static void
_codec_init_param (mfxVideoParam * param,
    guint codec_id, mfxU16 pattern, GstVideoFormat format)
{
  g_return_if_fail (param != NULL);

  memset (param, 0, sizeof (mfxVideoParam));
  param->IOPattern = pattern;
  param->mfx.CodecId = codec_id;
  param->mfx.CodecProfile = _get_main_codec_profile (codec_id);
  param->mfx.FrameInfo.Width = default_width;
  param->mfx.FrameInfo.Height = default_height;
  param->mfx.FrameInfo.CropW = param->mfx.FrameInfo.Width;
  param->mfx.FrameInfo.CropH = param->mfx.FrameInfo.Height;
  param->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
  param->mfx.FrameInfo.FrameRateExtN = 30;
  param->mfx.FrameInfo.FrameRateExtD = 1;
  param->mfx.FrameInfo.AspectRatioW = 1;
  param->mfx.FrameInfo.AspectRatioH = 1;

  _fill_mfxframeinfo (format, &param->mfx.FrameInfo);
}

static gboolean
_get_min_width (mfxSession * session, mfxVideoParam * in,
    mfxVideoParam * out, IsParamSupportedFunc func,
    const mfxRange32U * width, guint * min_w)
{
  g_return_val_if_fail (func != NULL, FALSE);

  in->mfx.FrameInfo.Height = default_height;
  in->mfx.FrameInfo.CropH = default_height;
  out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;
  out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;

  for (guint w = width->Min; w < *min_w;) {
    in->mfx.FrameInfo.Width = w;
    in->mfx.FrameInfo.CropW = w;
    out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
    out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;

    if (func (session, in, out)) {
      *min_w = in->mfx.FrameInfo.Width;
      return TRUE;
    } else {
      if (out->mfx.FrameInfo.Width != 0) {
        if (func (session, out, out)) {
          *min_w = out->mfx.FrameInfo.Width;
          return TRUE;
        }
      }
    }

    w += width->Step;
  }

  return FALSE;
}

static gboolean
_get_min_height (mfxSession * session, mfxVideoParam * in,
    mfxVideoParam * out, IsParamSupportedFunc func,
    const mfxRange32U * height, guint * min_h)
{
  g_return_val_if_fail (func != NULL, FALSE);

  in->mfx.FrameInfo.Width = default_width;
  in->mfx.FrameInfo.CropW = default_width;
  out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
  out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;

  for (guint h = height->Min; h < *min_h;) {
    in->mfx.FrameInfo.Height = h;
    in->mfx.FrameInfo.CropH = h;
    out->mfx.FrameInfo.Height = h;
    out->mfx.FrameInfo.CropH = h;

    if (func (session, in, out)) {
      *min_h = in->mfx.FrameInfo.Height;
      return TRUE;
    } else {
      if (out->mfx.FrameInfo.Height != 0) {
        if (func (session, out, out)) {
          *min_h = out->mfx.FrameInfo.Height;
          return TRUE;
        }
      }
    }

    h += height->Step;
  }

  return FALSE;
}

static guint
_get_smaller_res_width (guint cur_res_width)
{
  guint i = G_N_ELEMENTS (max_res_widths) - 1;

  for (; i >= 0; i--) {
    if (max_res_widths[i] < cur_res_width)
      return max_res_widths[i];
  }

  return 0;
}

static gboolean
_get_max_width (mfxSession * session, mfxVideoParam * in,
    mfxVideoParam * out, IsParamSupportedFunc func,
    const mfxRange32U * width, guint * max_w)
{
  guint w;

  g_return_val_if_fail (func != NULL, FALSE);

  in->mfx.FrameInfo.Height = default_height;
  in->mfx.FrameInfo.CropH = default_height;
  out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;
  out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;

  w = width->Max;
  do {
    in->mfx.FrameInfo.Width = w;
    in->mfx.FrameInfo.CropW = w;
    out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
    out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;

    if (func (session, in, out)) {
      *max_w = in->mfx.FrameInfo.Width;
      return TRUE;
    } else {
      if (out->mfx.FrameInfo.Width != 0) {
        if (func (session, out, out)) {
          *max_w = out->mfx.FrameInfo.Width;
          return TRUE;
        }
      }
    }

    w = _get_smaller_res_width (w);
  } while (w);

  return FALSE;
}

static guint
_get_smaller_res_height (guint cur_res_height)
{
  guint i = G_N_ELEMENTS (max_res_heights) - 1;

  for (; i >= 0; i--) {
    if (max_res_heights[i] < cur_res_height)
      return max_res_heights[i];
  }

  return 0;
}

static gboolean
_get_max_height (mfxSession * session, mfxVideoParam * in,
    mfxVideoParam * out, IsParamSupportedFunc func,
    const mfxRange32U * height, guint * max_h)
{
  guint h;

  g_return_val_if_fail (func != NULL, FALSE);

  in->mfx.FrameInfo.Width = default_width;
  in->mfx.FrameInfo.CropW = default_width;
  out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
  out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;

  h = height->Max;
  do {
    in->mfx.FrameInfo.Height = h;
    in->mfx.FrameInfo.CropH = h;
    out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;
    out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;

    if (func (session, in, out)) {
      *max_h = in->mfx.FrameInfo.Height;
      return TRUE;
    } else {
      if (out->mfx.FrameInfo.Height != 0) {
        if (func (session, out, out)) {
          *max_h = out->mfx.FrameInfo.Height;
          return TRUE;
        }
      }
    }

    h = _get_smaller_res_height (h);
  } while (h);

  return FALSE;
}

static gboolean
_format_in_list (GstVideoFormat format, GValue * list)
{
  const GValue *gfmt;
  const gchar *fmt_str = NULL;
  guint size = gst_value_list_get_size (list);

  if (format == GST_VIDEO_FORMAT_UNKNOWN)
    return FALSE;

  for (guint i = 0; i < size; i++) {
    gfmt = gst_value_list_get_value (list, i);
    fmt_str = g_value_get_string (gfmt);

    if (format == gst_video_format_from_string (fmt_str))
      return TRUE;
  }

  return FALSE;
}

static gboolean
_fourcc_in_array (mfxU32 fourcc, mfxU32 * array, mfxU16 num)
{
  for (guint i = 0; i < num; i++) {
    if (array[i] == fourcc)
      return TRUE;
  }

  return FALSE;
}

static inline gboolean
_enc_is_param_supported (mfxSession * session,
    mfxVideoParam * in, mfxVideoParam * out)
{
  mfxStatus status = MFXVideoENCODE_Query (*session, in, out);
  if (status == MFX_ERR_NONE)
    return TRUE;

  return FALSE;
}

static inline gboolean
_enc_ensure_codec (mfxEncoderDescription * enc_desc, guint c)
{
  for (guint p = 0; p < enc_desc->Codecs[c].NumProfiles; p++) {
    if (enc_desc->Codecs[c].Profiles[p].MemDesc->NumColorFormats)
      return TRUE;
  }

  return FALSE;
}

static inline gint
_enc_get_codec_index (mfxEncoderDescription * enc_desc, guint codec_id)
{
  gint c;

  for (c = 0; c < enc_desc->NumCodecs; c++) {
    if (enc_desc->Codecs[c].CodecID == codec_id) {
      break;
    }
  }

  if (c >= enc_desc->NumCodecs)
    goto failed;

  if (!_enc_ensure_codec (enc_desc, c))
    goto failed;

  return c;

failed:
  GST_WARNING ("Unsupported codec %" GST_FOURCC_FORMAT,
      GST_FOURCC_ARGS (codec_id));
  return -1;
}

static gboolean
_enc_get_resolution_range (mfxSession * session,
    mfxEncoderDescription * enc_desc, guint codec_id,
    ResolutionRange * res_range)
{
  guint c;
  mfxVideoParam in, out;
  mfxRange32U *width, *height;
  ResolutionRange res = { default_width, default_width,
    default_height, default_height
  };

  g_return_val_if_fail (res_range != NULL, FALSE);

  c = _enc_get_codec_index (enc_desc, codec_id);
  width = &enc_desc->Codecs[c].Profiles->MemDesc->Width;
  height = &enc_desc->Codecs[c].Profiles->MemDesc->Height;

  _codec_init_param (&in, codec_id, ENC_IOPATTERN, DEFAULT_VIDEO_FORMAT);
  if (codec_id == MFX_CODEC_AV1)
    in.mfx.CodecLevel = MFX_LEVEL_AV1_41;
  out = in;

  IsParamSupportedFunc func = _enc_is_param_supported;
  if (!_get_min_width (session, &in, &out, func, width, &res.min_width) ||
      !_get_max_width (session, &in, &out, func, width, &res.max_width) ||
      !_get_min_height (session, &in, &out, func, height, &res.min_height) ||
      !_get_max_height (session, &in, &out, func, height, &res.max_height))
    return FALSE;

  GST_DEBUG ("Got %" GST_FOURCC_FORMAT
      " ENC supported resolution range width: [%d, %d], height: [%d, %d]",
      GST_FOURCC_ARGS (enc_desc->Codecs[c].CodecID),
      res.min_width, res.max_width, res.min_height, res.max_height);

  res_range->min_width = res.min_width;
  res_range->max_width = res.max_width;
  res_range->min_height = res.min_height;
  res_range->max_height = res.max_height;

  return TRUE;
}

static gboolean
_enc_is_format_supported (mfxSession * session,
    guint codec_id, GstVideoFormat format,
    mfxVideoParam * in, mfxVideoParam * out)
{
  if (!_fill_mfxframeinfo (format, &in->mfx.FrameInfo))
    return FALSE;

  in->mfx.LowPower = MFX_CODINGOPTION_UNKNOWN;
  if (!_enc_is_param_supported (session, in, out)) {
    in->mfx.LowPower = (out->mfx.LowPower == MFX_CODINGOPTION_ON) ?
        MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON;

    if (!_enc_is_param_supported (session, in, out))
      return FALSE;
  }

  return TRUE;
}

static gboolean
_enc_get_supported_formats_and_profiles (mfxSession * session,
    mfxEncoderDescription * enc_desc, guint codec_id,
    GValue * supported_fmts, GValue * supported_profs)
{
  guint size;
  const GValue *gfmt;
  GValue fmts = G_VALUE_INIT;
  GstVideoFormat fmt = DEFAULT_VIDEO_FORMAT;
  mfxVideoParam in, out;
  gboolean prof_supported;
  const gchar *prof_str = NULL;
  gchar **profs = NULL;
  gint c = _enc_get_codec_index (enc_desc, codec_id);

  _codec_init_param (&in, codec_id, ENC_IOPATTERN, DEFAULT_VIDEO_FORMAT);
  if (codec_id == MFX_CODEC_AV1)
    in.mfx.CodecLevel = MFX_LEVEL_AV1_41;
  out = in;

  g_value_init (&fmts, GST_TYPE_LIST);
  gst_msdk_get_video_format_list (&fmts);
  size = gst_value_list_get_size (&fmts);

  for (guint p = 0; p < enc_desc->Codecs[c].NumProfiles; p++) {
    in.mfx.CodecProfile = enc_desc->Codecs[c].Profiles[p].Profile;
    prof_supported = FALSE;

    for (guint i = 0; i < size; i++) {
      gfmt = gst_value_list_get_value (&fmts, i);
      fmt = g_value_get_uint (gfmt);

      if (_format_in_list (fmt, supported_fmts))
        continue;

      if (!_fourcc_in_array (gst_msdk_get_mfx_fourcc_from_format (fmt),
              enc_desc->Codecs[c].Profiles[p].MemDesc->ColorFormats,
              enc_desc->Codecs[c].Profiles[p].MemDesc->NumColorFormats))
        continue;

      if (!_enc_is_format_supported (session, codec_id, fmt, &in, &out))
        continue;

      _list_append_string (supported_fmts, gst_video_format_to_string (fmt));
      prof_supported = TRUE;
    }

    if (!prof_supported) {
      for (guint f = 0;
          f < enc_desc->Codecs[c].Profiles[p].MemDesc->NumColorFormats; f++) {
        fmt =
            gst_msdk_get_video_format_from_mfx_fourcc (enc_desc->
            Codecs[c].Profiles[p].MemDesc->ColorFormats[f]);
        if (_enc_is_format_supported (session, codec_id, fmt, &in, &out)) {
          prof_supported = TRUE;
          break;
        }
      }
    }

    if (!prof_supported)
      continue;

    prof_str = _profile_to_string (codec_id, in.mfx.CodecProfile);
    if (!prof_str)
      continue;

    profs = g_strsplit (prof_str, PROFILE_DELIMITER, 0);
    for (guint i = 0; profs[i]; i++)
      _list_append_string (supported_profs, profs[i]);
    g_strfreev (profs);
  }

  g_value_unset (&fmts);

  if (gst_value_list_get_size (supported_fmts) == 0 ||
      gst_value_list_get_size (supported_profs) == 0)
    return FALSE;

  return TRUE;
}

static GstCaps *
_enc_create_sink_caps (GstMsdkContext * context, guint codec_id,
    const ResolutionRange * res, GValue * supported_formats)
{
  GstCaps *caps, *dma_caps, *raw_caps;

#ifndef _WIN32
  caps = gst_caps_from_string
      ("video/x-raw(memory:VAMemory), format=(string){ NV12 }");
  dma_caps = _create_dma_drm_caps (context, GST_MSDK_JOB_ENCODER,
      supported_formats);
  gst_caps_append (caps, dma_caps);
#else
  VAR_UNUSED (dma_caps);
  caps = gst_caps_from_string
      ("video/x-raw(memory:D3D11Memory), format=(string){ NV12 }");
#endif

  raw_caps = gst_caps_from_string ("video/x-raw");
  gst_caps_set_value (raw_caps, "format", supported_formats);
  gst_caps_append (caps, raw_caps);

  gst_caps_set_simple (caps,
      "width", GST_TYPE_INT_RANGE, res->min_width, res->max_width,
      "height", GST_TYPE_INT_RANGE, res->min_height, res->max_height,
      "interlace-mode", G_TYPE_STRING, "progressive", NULL);

  GST_DEBUG ("Create %" GST_FOURCC_FORMAT " ENC sink_caps %" GST_PTR_FORMAT,
      GST_FOURCC_ARGS (codec_id), caps);

  return caps;
}

static GstCaps *
_enc_create_src_caps (guint codec_id,
    const ResolutionRange * res, GValue * supported_profiles)
{
  GstCaps *caps;
  const gchar *media_type = NULL;

  media_type = _get_media_type (codec_id);
  if (!media_type)
    return NULL;

  caps = gst_caps_new_empty_simple (media_type);
  gst_caps_set_value (caps, "profile", supported_profiles);
  gst_caps_set_simple (caps,
      "width", GST_TYPE_INT_RANGE, res->min_width, res->max_width,
      "height", GST_TYPE_INT_RANGE, res->min_height, res->max_height, NULL);

  GST_DEBUG ("Create %" GST_FOURCC_FORMAT " ENC src_caps %" GST_PTR_FORMAT,
      GST_FOURCC_ARGS (codec_id), caps);

  return caps;
}

gboolean
gst_msdkcaps_enc_create_caps (GstMsdkContext * context,
    gpointer enc_description, guint codec_id,
    GstCaps ** sink_caps, GstCaps ** src_caps)
{
  mfxEncoderDescription *enc_desc;
  GstCaps *in_caps = NULL, *out_caps = NULL;
  ResolutionRange res_range = { 1, G_MAXUINT16, 1, G_MAXUINT16 };
  GValue supported_fmts = G_VALUE_INIT;
  GValue supported_profs = G_VALUE_INIT;
  mfxSession session;

  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (enc_description, FALSE);

  session = gst_msdk_context_get_session (context);
  enc_desc = (mfxEncoderDescription *) enc_description;

  if (_enc_get_codec_index (enc_desc, codec_id) < 0)
    goto failed;

  g_value_init (&supported_fmts, GST_TYPE_LIST);
  g_value_init (&supported_profs, GST_TYPE_LIST);
  if (!_enc_get_supported_formats_and_profiles (&session,
          enc_desc, codec_id, &supported_fmts, &supported_profs))
    goto failed;

  if (!_enc_get_resolution_range (&session, enc_desc, codec_id, &res_range))
    goto failed;

  in_caps = _enc_create_sink_caps (context,
      codec_id, &res_range, &supported_fmts);
  g_value_unset (&supported_fmts);
  if (!in_caps)
    goto failed;

  out_caps = _enc_create_src_caps (codec_id, &res_range, &supported_profs);
  g_value_unset (&supported_profs);
  if (!out_caps)
    goto failed;

  *sink_caps = in_caps;
  *src_caps = out_caps;

  return TRUE;

failed:
  GST_WARNING ("Failed to create caps for %" GST_FOURCC_FORMAT " ENC",
      GST_FOURCC_ARGS (codec_id));

  g_value_unset (&supported_fmts);
  g_value_unset (&supported_profs);
  if (in_caps)
    gst_caps_unref (in_caps);

  return FALSE;
}

static inline gboolean
_dec_is_param_supported (mfxSession * session,
    mfxVideoParam * in, mfxVideoParam * out)
{
  mfxStatus status = MFXVideoDECODE_Query (*session, in, out);
  if (status == MFX_ERR_NONE)
    return TRUE;

  return FALSE;
}

static inline gboolean
_dec_ensure_codec (mfxDecoderDescription * dec_desc, guint c)
{
  for (guint p = 0; p < dec_desc->Codecs[c].NumProfiles; p++) {
    if (dec_desc->Codecs[c].Profiles[p].MemDesc->NumColorFormats)
      return TRUE;
  }

  return FALSE;
}

static inline gint
_dec_get_codec_index (mfxDecoderDescription * dec_desc, guint codec_id)
{
  gint c;

  for (c = 0; c < dec_desc->NumCodecs; c++) {
    if (dec_desc->Codecs[c].CodecID == codec_id) {
      break;
    }
  }

  if (c >= dec_desc->NumCodecs)
    goto failed;

  if (!_dec_ensure_codec (dec_desc, c))
    goto failed;

  return c;

failed:
  GST_WARNING ("Unsupported codec %" GST_FOURCC_FORMAT,
      GST_FOURCC_ARGS (codec_id));
  return -1;
}

static void
_jpegdec_set_color_format (mfxVideoParam * param, GstVideoFormat format)
{
  param->mfx.JPEGChromaFormat = param->mfx.FrameInfo.ChromaFormat;

  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_YUY2:
      param->mfx.JPEGColorFormat = MFX_JPEG_COLORFORMAT_YCbCr;
      break;
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
      param->mfx.JPEGColorFormat = MFX_JPEG_COLORFORMAT_RGB;
      break;
    default:
      GST_WARNING ("Jpegdec unsupported format %s",
          gst_video_format_to_string (format));
      break;
  }
}

static gboolean
_dec_get_resolution_range (mfxSession * session,
    mfxDecoderDescription * dec_desc, guint codec_id,
    ResolutionRange * res_range)
{
  guint c;
  mfxVideoParam in, out;
  mfxRange32U *width, *height;
  ResolutionRange res = { default_width, default_width,
    default_height, default_height
  };

  g_return_val_if_fail (res_range != NULL, FALSE);

  c = _dec_get_codec_index (dec_desc, codec_id);
  width = &dec_desc->Codecs[c].Profiles->MemDesc->Width;
  height = &dec_desc->Codecs[c].Profiles->MemDesc->Height;

  _codec_init_param (&in, codec_id, DEC_IOPATTERN, DEFAULT_VIDEO_FORMAT);
  if (codec_id == MFX_CODEC_AV1)
    in.mfx.CodecLevel = MFX_LEVEL_AV1_41;
  if (codec_id == MFX_CODEC_JPEG)
    _jpegdec_set_color_format (&in, GST_VIDEO_FORMAT_NV12);
  out = in;

  IsParamSupportedFunc func = _dec_is_param_supported;
  if (!_get_min_width (session, &in, &out, func, width, &res.min_width) ||
      !_get_max_width (session, &in, &out, func, width, &res.max_width) ||
      !_get_min_height (session, &in, &out, func, height, &res.min_height) ||
      !_get_max_height (session, &in, &out, func, height, &res.max_height))
    return FALSE;

  GST_DEBUG ("Got %" GST_FOURCC_FORMAT
      " DEC supported resolution range width: [%d, %d], height: [%d, %d]",
      GST_FOURCC_ARGS (dec_desc->Codecs[c].CodecID),
      res.min_width, res.max_width, res.min_height, res.max_height);

  res_range->min_width = res.min_width;
  res_range->max_width = res.max_width;
  res_range->min_height = res.min_height;
  res_range->max_height = res.max_height;

  return TRUE;
}

static gboolean
_dec_is_format_supported (mfxSession * session,
    guint codec_id, GstVideoFormat format,
    mfxVideoParam * in, mfxVideoParam * out)
{
  if (!_fill_mfxframeinfo (format, &in->mfx.FrameInfo))
    return FALSE;

  if (codec_id == MFX_CODEC_JPEG)
    _jpegdec_set_color_format (in, format);

  in->mfx.LowPower = MFX_CODINGOPTION_UNKNOWN;
  if (!_dec_is_param_supported (session, in, out)) {
    in->mfx.LowPower = (out->mfx.LowPower == MFX_CODINGOPTION_ON) ?
        MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON;

    if (!_dec_is_param_supported (session, in, out))
      return FALSE;
  }

  return TRUE;
}

static gboolean
_dec_get_supported_formats (mfxSession * session,
    mfxDecoderDescription * dec_desc, guint codec_id, GValue * supported_fmts)
{
  guint size;
  const GValue *gfmt;
  GValue fmts = G_VALUE_INIT;
  GstVideoFormat fmt = DEFAULT_VIDEO_FORMAT;
  mfxVideoParam in, out;
  gint c;

  _codec_init_param (&in, codec_id, DEC_IOPATTERN, DEFAULT_VIDEO_FORMAT);
  if (codec_id == MFX_CODEC_AV1)
    in.mfx.CodecLevel = MFX_LEVEL_AV1_41;
  out = in;

  g_value_init (&fmts, GST_TYPE_LIST);
  gst_msdk_get_video_format_list (&fmts);
  size = gst_value_list_get_size (&fmts);

  c = _dec_get_codec_index (dec_desc, codec_id);

  for (guint p = 0; p < dec_desc->Codecs[c].NumProfiles; p++) {
    in.mfx.CodecProfile = dec_desc->Codecs[c].Profiles[p].Profile;

    for (guint i = 0; i < size; i++) {
      gfmt = gst_value_list_get_value (&fmts, i);
      fmt = g_value_get_uint (gfmt);

      if (_format_in_list (fmt, supported_fmts))
        continue;

      if (!_fourcc_in_array (gst_msdk_get_mfx_fourcc_from_format (fmt),
              dec_desc->Codecs[c].Profiles[p].MemDesc->ColorFormats,
              dec_desc->Codecs[c].Profiles[p].MemDesc->NumColorFormats))
        continue;

      if (!_dec_is_format_supported (session, codec_id, fmt, &in, &out))
        continue;

      _list_append_string (supported_fmts, gst_video_format_to_string (fmt));
    }
  }

  g_value_unset (&fmts);

  return (gst_value_list_get_size (supported_fmts) == 0) ? FALSE : TRUE;
}

static GstCaps *
_dec_create_sink_caps (guint codec_id)
{
  GstCaps *caps;
  const gchar *media_type = NULL;

  media_type = _get_media_type (codec_id);
  if (!media_type)
    return NULL;

  caps = gst_caps_new_empty_simple (media_type);

  GST_DEBUG ("Create %" GST_FOURCC_FORMAT " DEC sink_caps %" GST_PTR_FORMAT,
      GST_FOURCC_ARGS (codec_id), caps);

  return caps;
}

static GstCaps *
_dec_create_src_caps (GstMsdkContext * context,
    mfxSession * session, guint codec_id,
    mfxDecoderDescription * dec_desc, GValue * supported_formats)
{
  GstCaps *caps, *dma_caps, *raw_caps;
  ResolutionRange res = { 1, G_MAXUINT16, 1, G_MAXUINT16 };

  if (!_dec_get_resolution_range (session, dec_desc, codec_id, &res))
    return NULL;

#ifndef _WIN32
  caps = gst_caps_from_string
      ("video/x-raw(memory:VAMemory), format=(string){ NV12 }");
  dma_caps = _create_dma_drm_caps (context, GST_MSDK_JOB_DECODER,
      supported_formats);
  gst_caps_append (caps, dma_caps);
#else
  VAR_UNUSED (dma_caps);
  caps = gst_caps_from_string
      ("video/x-raw(memory:D3D11Memory), format=(string){ NV12 }");
#endif

  raw_caps = gst_caps_from_string ("video/x-raw");
  gst_caps_set_value (raw_caps, "format", supported_formats);
  gst_caps_append (caps, raw_caps);

  gst_caps_set_simple (caps,
      "width", GST_TYPE_INT_RANGE, res.min_width, res.max_width,
      "height", GST_TYPE_INT_RANGE, res.min_height, res.max_height,
      "interlace-mode", G_TYPE_STRING, "progressive", NULL);

  GST_DEBUG ("Create %" GST_FOURCC_FORMAT " DEC src_caps %" GST_PTR_FORMAT,
      GST_FOURCC_ARGS (codec_id), caps);

  return caps;
}

gboolean
gst_msdkcaps_dec_create_caps (GstMsdkContext * context,
    gpointer dec_description, guint codec_id,
    GstCaps ** sink_caps, GstCaps ** src_caps)
{
  mfxDecoderDescription *dec_desc;
  GstCaps *in_caps = NULL, *out_caps = NULL;
  GValue supported_fmts = G_VALUE_INIT;
  mfxSession session;

  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (dec_description, FALSE);

  session = gst_msdk_context_get_session (context);
  dec_desc = (mfxDecoderDescription *) dec_description;

  if (_dec_get_codec_index (dec_desc, codec_id) < 0)
    goto failed;

  g_value_init (&supported_fmts, GST_TYPE_LIST);
  if (!_dec_get_supported_formats (&session,
          dec_desc, codec_id, &supported_fmts))
    goto failed;

  in_caps = _dec_create_sink_caps (codec_id);
  if (!in_caps)
    goto failed;

  out_caps = _dec_create_src_caps (context,
      &session, codec_id, dec_desc, &supported_fmts);
  g_value_unset (&supported_fmts);
  if (!out_caps)
    goto failed;

  *sink_caps = in_caps;
  *src_caps = out_caps;

  return TRUE;

failed:
  GST_WARNING ("Failed to create caps for %" GST_FOURCC_FORMAT " DEC",
      GST_FOURCC_ARGS (codec_id));

  g_value_unset (&supported_fmts);
  if (in_caps)
    gst_caps_unref (in_caps);

  return FALSE;
}

static void
_vpp_init_param (mfxVideoParam * param,
    GstVideoFormat infmt, GstVideoFormat outfmt)
{
  g_return_if_fail (param != NULL);

  memset (param, 0, sizeof (mfxVideoParam));
  param->IOPattern = VPP_IOPATTERN;
  param->vpp.In.Width = default_width;
  param->vpp.In.Height = default_height;
  param->vpp.In.CropW = param->mfx.FrameInfo.Width;
  param->vpp.In.CropH = param->mfx.FrameInfo.Height;
  param->vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
  param->vpp.In.FrameRateExtN = 30;
  param->vpp.In.FrameRateExtD = 1;
  param->vpp.In.AspectRatioW = 1;
  param->vpp.In.AspectRatioH = 1;

  param->vpp.Out = param->vpp.In;

  _fill_mfxframeinfo (infmt, &param->vpp.In);
  _fill_mfxframeinfo (outfmt, &param->vpp.Out);
}

static inline gboolean
_vpp_is_param_supported (mfxSession * session,
    mfxVideoParam * in, mfxVideoParam * out)
{
  mfxStatus status = MFXVideoVPP_Query (*session, in, out);
  if (status == MFX_ERR_NONE)
    return TRUE;

  return FALSE;
}

static gboolean
_vpp_are_formats_supported (mfxSession * session,
    GstVideoFormat infmt, GstVideoFormat outfmt,
    mfxVideoParam * in, mfxVideoParam * out)
{
  if (!_fill_mfxframeinfo (infmt, &in->vpp.In))
    return FALSE;

  if (!_fill_mfxframeinfo (outfmt, &in->vpp.Out))
    return FALSE;

  if (!_vpp_is_param_supported (session, in, out))
    return FALSE;

  return TRUE;
}

static gboolean
_vpp_get_supported_formats (mfxSession * session,
    GValue * supported_in_fmts, GValue * supported_out_fmts)
{
  guint size;
  const GValue *gfmt;
  GValue fmts = G_VALUE_INIT;
  GstVideoFormat infmt = DEFAULT_VIDEO_FORMAT;
  GstVideoFormat outfmt = DEFAULT_VIDEO_FORMAT;
  gboolean infmt_in_list, outfmt_in_list;
  mfxVideoParam in, out;

  _vpp_init_param (&in, infmt, outfmt);
  out = in;

  g_value_init (&fmts, GST_TYPE_LIST);
  gst_msdk_get_video_format_list (&fmts);

  size = gst_value_list_get_size (&fmts);
  for (guint i = 0; i < size; i++) {
    gfmt = gst_value_list_get_value (&fmts, i);
    infmt = g_value_get_uint (gfmt);

    for (guint j = 0; j < size; j++) {
      gfmt = gst_value_list_get_value (&fmts, j);
      outfmt = g_value_get_uint (gfmt);

      infmt_in_list = _format_in_list (infmt, supported_in_fmts);
      outfmt_in_list = _format_in_list (outfmt, supported_out_fmts);
      if (infmt_in_list && outfmt_in_list)
        continue;

      if (!_vpp_are_formats_supported (session, infmt, outfmt, &in, &out))
        continue;

      if (!infmt_in_list)
        _list_append_string (supported_in_fmts,
            gst_video_format_to_string (infmt));

      if (!outfmt_in_list)
        _list_append_string (supported_out_fmts,
            gst_video_format_to_string (outfmt));
    }
  }

  g_value_unset (&fmts);

  if (gst_value_list_get_size (supported_in_fmts) == 0 ||
      gst_value_list_get_size (supported_out_fmts) == 0)
    return FALSE;

  return TRUE;
}

static gboolean
_vpp_get_desc_image_range (mfxVPPDescription * vpp_desc,
    GstVideoFormat format, mfxRange32U * width, mfxRange32U * height)
{
  mfxU32 infmt = gst_msdk_get_mfx_fourcc_from_format (format);

  for (guint f = 0; f < vpp_desc->NumFilters; f++) {
    for (guint i = 0; i < vpp_desc->NumFilters; i++) {
      if (vpp_desc->Filters[f].MemDesc->Formats[i].InFormat != infmt)
        continue;

      *width = vpp_desc->Filters[f].MemDesc->Width;
      *height = vpp_desc->Filters[f].MemDesc->Height;
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
_vpp_get_resolution_range (mfxSession * session,
    mfxVPPDescription * vpp_desc, ResolutionRange * res_range)
{
  mfxVideoParam in, out;
  mfxRange32U width, height;
  ResolutionRange res = { default_width, default_width,
    default_height, default_height
  };

  g_return_val_if_fail (res_range != NULL, FALSE);

  if (!_vpp_get_desc_image_range (vpp_desc,
          DEFAULT_VIDEO_FORMAT, &width, &height))
    return FALSE;

  _vpp_init_param (&in, DEFAULT_VIDEO_FORMAT, DEFAULT_VIDEO_FORMAT);
  out = in;

  IsParamSupportedFunc func = _vpp_is_param_supported;
  if (!_get_min_width (session, &in, &out, func, &width, &res.min_width) ||
      !_get_max_width (session, &in, &out, func, &width, &res.max_width) ||
      !_get_min_height (session, &in, &out, func, &height, &res.min_height) ||
      !_get_max_height (session, &in, &out, func, &height, &res.max_height))
    return FALSE;

  GST_DEBUG ("Got VPP supported resolution range "
      "width: [%d, %d], height: [%d, %d]",
      res.min_width, res.max_width, res.min_height, res.max_height);

  res_range->min_width = res.min_width;
  res_range->max_width = res.max_width;
  res_range->min_height = res.min_height;
  res_range->max_height = res.max_height;

  return TRUE;
}

static GstCaps *
_vpp_create_caps (GstMsdkContext * context,
    GValue * supported_fmts, ResolutionRange * res)
{
  GstCaps *caps, *dma_caps, *raw_caps;

#ifndef _WIN32
  caps = gst_caps_from_string ("video/x-raw(memory:VAMemory), "
      "format=(string){ NV12, VUYA, P010_10LE }");
  dma_caps = _create_dma_drm_caps (context, GST_MSDK_JOB_VPP, supported_fmts);
  gst_caps_append (caps, dma_caps);
#else
  VAR_UNUSED (dma_caps);
  caps = gst_caps_from_string ("video/x-raw(memory:D3D11Memory), "
      "format=(string){ NV12, VUYA, P010_10LE }");
#endif
  raw_caps = gst_caps_from_string ("video/x-raw");
  gst_caps_set_value (raw_caps, "format", supported_fmts);
  gst_caps_append (caps, raw_caps);

  gst_caps_set_simple (caps,
      "width", GST_TYPE_INT_RANGE, res->min_width, res->max_width,
      "height", GST_TYPE_INT_RANGE, res->min_height, res->max_height, NULL);

  gst_msdkcaps_set_strings (caps, "memory:SystemMemory",
      "interlace-mode", "progressive, interleaved, mixed");

  GST_DEBUG ("Create VPP caps %" GST_PTR_FORMAT, caps);

  return caps;
}

gboolean
gst_msdkcaps_vpp_create_caps (GstMsdkContext * context,
    gpointer vpp_description, GstCaps ** sink_caps, GstCaps ** src_caps)
{
  mfxVPPDescription *vpp_desc;
  GstCaps *in_caps = NULL, *out_caps = NULL;
  GValue supported_in_fmts = G_VALUE_INIT;
  GValue supported_out_fmts = G_VALUE_INIT;
  ResolutionRange res_range = { 1, G_MAXUINT16, 1, G_MAXUINT16 };
  mfxSession session;

  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (vpp_description, FALSE);

  vpp_desc = (mfxVPPDescription *) vpp_description;
  if (vpp_desc->NumFilters == 0)
    return FALSE;

  session = gst_msdk_context_get_session (context);

  g_value_init (&supported_in_fmts, GST_TYPE_LIST);
  g_value_init (&supported_out_fmts, GST_TYPE_LIST);

  if (!_vpp_get_supported_formats (&session,
          &supported_in_fmts, &supported_out_fmts))
    goto failed;

  if (!_vpp_get_resolution_range (&session, vpp_desc, &res_range))
    goto failed;

  in_caps = _vpp_create_caps (context, &supported_in_fmts, &res_range);
  g_value_unset (&supported_in_fmts);
  if (!in_caps)
    goto failed;

  out_caps = _vpp_create_caps (context, &supported_out_fmts, &res_range);
  g_value_unset (&supported_out_fmts);
  if (!out_caps)
    goto failed;

  *sink_caps = in_caps;
  *src_caps = out_caps;

  return TRUE;

failed:
  GST_WARNING ("Failed to create caps for VPP");

  g_value_unset (&supported_in_fmts);
  g_value_unset (&supported_out_fmts);
  if (in_caps)
    gst_caps_unref (in_caps);

  return FALSE;
}

#else

gboolean
gst_msdkcaps_enc_create_caps (GstMsdkContext * context,
    gpointer enc_description, guint codec_id,
    GstCaps ** sink_caps, GstCaps ** src_caps)
{
  return FALSE;
}

gboolean
gst_msdkcaps_dec_create_caps (GstMsdkContext * context,
    gpointer dec_description, guint codec_id,
    GstCaps ** sink_caps, GstCaps ** src_caps)
{
  return FALSE;
}

gboolean
gst_msdkcaps_vpp_create_caps (GstMsdkContext * context,
    gpointer vpp_description, GstCaps ** sink_caps, GstCaps ** src_caps)
{
  return FALSE;
}

#endif

static gboolean
_get_profiles (guint codec_id, GValue * supported_profs)
{
  const Profile *profiles = NULL;
  gchar **profs = NULL;

  g_return_val_if_fail (supported_profs != NULL, FALSE);

  for (guint c = 0; c < G_N_ELEMENTS (codec_profs); c++) {
    if (codec_profs[c].codec == codec_id) {
      profiles = codec_profs[c].profiles;
      break;
    }
  }
  if (!profiles)
    return FALSE;

  for (; profiles->id != MFX_PROFILE_UNKNOWN; profiles++) {
    profs = g_strsplit (profiles->names, PROFILE_DELIMITER, 0);
    for (guint p = 0; profs[p]; p++)
      _list_append_string (supported_profs, profs[p]);
    g_strfreev (profs);
  }

  return TRUE;
}

static const char *
_enc_get_static_raw_formats (guint codec_id)
{
  switch (codec_id) {
    case MFX_CODEC_AVC:
      return "NV12, YUY2, UYVY, BGRA";
    case MFX_CODEC_HEVC:
      return "NV12, YUY2, BGRA, BGR10A2_LE, P010_10LE, VUYA, Y410, Y210, "
          "P012_LE";
    case MFX_CODEC_MPEG2:
      return "NV12";
    case MFX_CODEC_VP9:
      return "NV12, P010_10LE, VUYA, Y410";
    case MFX_CODEC_AV1:
      return "NV12, P010_10LE";
    case MFX_CODEC_JPEG:
      return "NV12, YUY2, UYVY, BGRA";
    default:
      GST_WARNING ("Unsupported codec %" GST_FOURCC_FORMAT,
          GST_FOURCC_ARGS (codec_id));
      break;
  }

  return NULL;
}

#ifndef _WIN32
static const char *
_enc_get_static_dma_formats (guint codec_id)
{
  switch (codec_id) {
    case MFX_CODEC_AVC:
      return "NV12, BGRx";
    case MFX_CODEC_HEVC:
      return "NV12, P010_10LE";
    case MFX_CODEC_MPEG2:
      return "NV12";
    case MFX_CODEC_VP9:
      return "NV12, P010_10LE";
    case MFX_CODEC_AV1:
      return "NV12, P010_10LE";
    case MFX_CODEC_JPEG:
      return "NV12, BGRx";
    default:
      GST_WARNING ("Unsupported codec %" GST_FOURCC_FORMAT,
          GST_FOURCC_ARGS (codec_id));
      break;
  }

  return NULL;
}
#endif

gboolean
gst_msdkcaps_enc_create_static_caps (GstMsdkContext * context,
    guint codec_id, GstCaps ** sink_caps, GstCaps ** src_caps)
{
  GstCaps *in_caps = NULL, *out_caps = NULL;
  GstCaps *dma_caps = NULL;
  gchar *raw_caps_str;
  const gchar *media_type = NULL;
  const char *raw_fmts = NULL;
  const char *dma_fmts_str = NULL;
  GValue dma_fmts = G_VALUE_INIT;
  GValue supported_profs = G_VALUE_INIT;

  raw_fmts = _enc_get_static_raw_formats (codec_id);
  if (!raw_fmts)
    goto failed;
  raw_caps_str = g_strdup_printf ("video/x-raw, format=(string){ %s }",
      raw_fmts);
  in_caps = gst_caps_from_string (raw_caps_str);
  g_free (raw_caps_str);

#ifndef _WIN32
  dma_fmts_str = _enc_get_static_dma_formats (codec_id);
  if (!dma_fmts_str)
    goto failed;

  g_value_init (&dma_fmts, GST_TYPE_LIST);
  _strings_to_list (dma_fmts_str, &dma_fmts);

  dma_caps = _create_dma_drm_caps (context, GST_MSDK_JOB_ENCODER, &dma_fmts);
  g_value_unset (&dma_fmts);
  gst_caps_append (in_caps, dma_caps);

  gst_caps_append (in_caps,
      gst_caps_from_string
      ("video/x-raw(memory:VAMemory), format=(string){ NV12 }"));
#else
  VAR_UNUSED (dma_caps);
  VAR_UNUSED (dma_fmts_str);
  VAR_UNUSED (dma_fmts);
  gst_caps_append (in_caps,
      gst_caps_from_string
      ("video/x-raw(memory:D3D11Memory), format=(string){ NV12 }"));
#endif

  gst_caps_set_simple (in_caps,
      "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
      "interlace-mode", G_TYPE_STRING, "progressive", NULL);

  media_type = _get_media_type (codec_id);
  if (!media_type)
    goto failed;

  out_caps = gst_caps_new_empty_simple (media_type);

  g_value_init (&supported_profs, GST_TYPE_LIST);
  if (!_get_profiles (codec_id, &supported_profs))
    goto failed;

  gst_caps_set_value (out_caps, "profile", &supported_profs);
  g_value_unset (&supported_profs);

  gst_caps_set_simple (out_caps,
      "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

  *sink_caps = in_caps;
  *src_caps = out_caps;

  return TRUE;

failed:
  GST_WARNING ("Failed to create caps for %" GST_FOURCC_FORMAT " ENC",
      GST_FOURCC_ARGS (codec_id));

  g_value_unset (&supported_profs);
  if (in_caps)
    gst_caps_unref (in_caps);
  if (out_caps)
    gst_caps_unref (out_caps);
  if (dma_caps)
    gst_caps_unref (dma_caps);

  return FALSE;
}

static const char *
_dec_get_static_raw_formats (guint codec_id)
{
  switch (codec_id) {
    case MFX_CODEC_AVC:
      return "NV12, BGRA, BGRx";
    case MFX_CODEC_HEVC:
      return "NV12, P010_10LE, YUY2, Y210,  VUYA, Y410, P012_LE, "
          "Y212_LE, Y412_LE, BGRA, BGRx";
    case MFX_CODEC_MPEG2:
      return "NV12";
    case MFX_CODEC_VP9:
      return "NV12, P010_10LE, VUYA, Y410, P012_LE, Y412_LE";
    case MFX_CODEC_AV1:
      return "NV12, P010_10LE, VUYA, Y410";
    case MFX_CODEC_JPEG:
      return "NV12, YUY2";
    case MFX_CODEC_VP8:
      return "NV12";
    case MFX_CODEC_VC1:
      return "NV12";
    default:
      GST_WARNING ("Unsupported codec %" GST_FOURCC_FORMAT,
          GST_FOURCC_ARGS (codec_id));
      break;
  }

  return NULL;
}

#ifndef _WIN32
static const char *
_dec_get_static_dma_formats (guint codec_id)
{
  switch (codec_id) {
    case MFX_CODEC_AVC:
      return "NV12, BGRA, BGRx";
    case MFX_CODEC_HEVC:
      return "NV12, P010_10LE, YUY2, Y210,  VUYA, Y410, P012_LE, "
          "Y212_LE, Y412_LE, BGRA, BGRx";
    case MFX_CODEC_MPEG2:
      return "NV12";
    case MFX_CODEC_VP9:
      return "NV12, P010_10LE, VUYA, Y410, P012_LE, Y412_LE";
    case MFX_CODEC_AV1:
      return "NV12, P010_10LE, VUYA, Y410";
    case MFX_CODEC_JPEG:
      return "NV12, YUY2";
    case MFX_CODEC_VP8:
      return "NV12";
    case MFX_CODEC_VC1:
      return "NV12";
    default:
      GST_WARNING ("Unsupported codec %" GST_FOURCC_FORMAT,
          GST_FOURCC_ARGS (codec_id));
      break;
  }

  return NULL;
}
#endif

gboolean
gst_msdkcaps_dec_create_static_caps (GstMsdkContext * context,
    guint codec_id, GstCaps ** sink_caps, GstCaps ** src_caps)
{
  GstCaps *in_caps = NULL, *out_caps = NULL;
  GstCaps *dma_caps = NULL;
  gchar *raw_caps_str;
  const gchar *media_type = NULL;
  const char *raw_fmts = NULL;
  const char *dma_fmts_str = NULL;
  GValue dma_fmts = G_VALUE_INIT;

  media_type = _get_media_type (codec_id);
  if (!media_type)
    goto failed;

  in_caps = gst_caps_new_empty_simple (media_type);

  raw_fmts = _dec_get_static_raw_formats (codec_id);
  if (!raw_fmts)
    goto failed;
  raw_caps_str = g_strdup_printf ("video/x-raw, format=(string){ %s }",
      raw_fmts);
  out_caps = gst_caps_from_string (raw_caps_str);
  g_free (raw_caps_str);

#ifndef _WIN32
  dma_fmts_str = _dec_get_static_dma_formats (codec_id);
  if (!dma_fmts_str)
    goto failed;

  g_value_init (&dma_fmts, GST_TYPE_LIST);
  _strings_to_list (dma_fmts_str, &dma_fmts);

  dma_caps = _create_dma_drm_caps (context, GST_MSDK_JOB_DECODER, &dma_fmts);
  g_value_unset (&dma_fmts);
  gst_caps_append (out_caps, dma_caps);

  gst_caps_append (out_caps,
      gst_caps_from_string
      ("video/x-raw(memory:VAMemory), format=(string){ NV12 }"));
#else
  VAR_UNUSED (dma_caps);
  VAR_UNUSED (dma_fmts_str);
  VAR_UNUSED (dma_fmts);
  gst_caps_append (out_caps,
      gst_caps_from_string
      ("video/x-raw(memory:D3D11Memory), format=(string){ NV12 }"));
#endif

  gst_caps_set_simple (out_caps,
      "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
      "interlace-mode", G_TYPE_STRING, "progressive", NULL);

  *sink_caps = in_caps;
  *src_caps = out_caps;

  return TRUE;

failed:
  GST_WARNING ("Failed to create caps for %" GST_FOURCC_FORMAT " DEC",
      GST_FOURCC_ARGS (codec_id));

  if (in_caps)
    gst_caps_unref (in_caps);
  if (out_caps)
    gst_caps_unref (out_caps);

  return FALSE;
}

static const char *
_vpp_get_static_raw_formats (GstPadDirection direction)
{
  switch (direction) {
    case GST_PAD_SINK:
      return "NV12, YV12, I420, YUY2, UYVY, VUYA, BGRA, BGRx, P010_10LE, "
          "RGB16, Y410, Y210, P012_LE, Y212_LE, Y412_LE";
    case GST_PAD_SRC:
      return "NV12, BGRA, YUY2, UYVY, VUYA, BGRx, P010_10LE, BGR10A2_LE, "
          "YV12, Y410, Y210, RGBP, BGRP, P012_LE, Y212_LE, Y412_LE";
    default:
      GST_WARNING ("Unsupported VPP direction");
      break;
  }

  return NULL;
}

#ifndef _WIN32
static const char *
_vpp_get_static_dma_formats (GstPadDirection direction)
{
  switch (direction) {
    case GST_PAD_SINK:
      return "NV12, BGRA, YUY2, UYVY, VUYA, P010_10LE, RGB16, Y410, Y210, "
          "P012_LE, Y212_LE, Y412_LE";
    case GST_PAD_SRC:
      return "NV12, BGRA, YUY2, UYVY, VUYA, BGRx, P010_10LE, BGR10A2_LE, "
          "YV12, Y410, Y210, RGBP, BGRP, P012_LE, Y212_LE, Y412_LE";
    default:
      GST_WARNING ("Unsupported VPP direction");
      break;
  }

  return NULL;
}
#endif

static GstCaps *
_vpp_create_static_caps (GstMsdkContext * context, GstPadDirection direction)
{
  GstCaps *caps = NULL, *dma_caps = NULL;
  gchar *caps_str;
  GValue dma_fmts = G_VALUE_INIT;

  caps_str = g_strdup_printf ("video/x-raw, format=(string){ %s }",
      _vpp_get_static_raw_formats (direction));
  caps = gst_caps_from_string (caps_str);
  g_free (caps_str);

#ifndef _WIN32
  g_value_init (&dma_fmts, GST_TYPE_LIST);
  _strings_to_list (_vpp_get_static_dma_formats (direction), &dma_fmts);

  dma_caps = _create_dma_drm_caps (context, GST_MSDK_JOB_VPP, &dma_fmts);
  g_value_unset (&dma_fmts);
  gst_caps_append (caps, dma_caps);

  gst_caps_append (caps, gst_caps_from_string ("video/x-raw(memory:VAMemory), "
          "format=(string){ NV12, VUYA, P010_10LE }"));
#else
  VAR_UNUSED (dma_caps);
  VAR_UNUSED (dma_fmts);

  gst_caps_append (caps,
      gst_caps_from_string ("video/x-raw(memory:D3D11Memory), "
          "format=(string){ NV12, VUYA, P010_10LE }"));
#endif

  gst_caps_set_simple (caps,
      "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

  gst_msdkcaps_set_strings (caps, "memory:SystemMemory",
      "interlace-mode", "progressive, interleaved, mixed");

  return caps;
}

gboolean
gst_msdkcaps_vpp_create_static_caps (GstMsdkContext * context,
    GstCaps ** sink_caps, GstCaps ** src_caps)
{
  *sink_caps = _vpp_create_static_caps (context, GST_PAD_SINK);
  *src_caps = _vpp_create_static_caps (context, GST_PAD_SRC);

  return TRUE;
}

static void
_pad_template_init (GstElementClass * klass,
    const gchar * name_template, GstPadDirection direction,
    GstCaps * caps, const gchar * doc_caps_str)
{
  GstPadTemplate *pad_templ;

  if (!caps)
    return;

  pad_templ = gst_pad_template_new (name_template,
      direction, GST_PAD_ALWAYS, caps);
  if (doc_caps_str) {
    GstCaps *doc_caps = gst_caps_from_string (doc_caps_str);
    gst_pad_template_set_documentation_caps (pad_templ, doc_caps);
    gst_caps_unref (doc_caps);
  }

  gst_element_class_add_pad_template (klass, pad_templ);
}

void
gst_msdkcaps_pad_template_init (GstElementClass * klass,
    GstCaps * sink_caps, GstCaps * src_caps,
    const gchar * doc_sink_caps_str, const gchar * doc_src_caps_str)
{
  _pad_template_init (klass,
      "sink", GST_PAD_SINK, sink_caps, doc_sink_caps_str);

  _pad_template_init (klass, "src", GST_PAD_SRC, src_caps, doc_src_caps_str);
}

gboolean
gst_msdkcaps_set_strings (GstCaps * caps,
    const gchar * features, const char *field, const gchar * strings)
{
  GValue list = G_VALUE_INIT;
  guint size = gst_caps_get_size (caps);

  g_return_val_if_fail (GST_IS_CAPS (caps), FALSE);
  g_return_val_if_fail (field != NULL, FALSE);
  g_return_val_if_fail (strings != NULL, FALSE);

  g_value_init (&list, GST_TYPE_LIST);
  _strings_to_list (strings, &list);

  if (features) {
    GstStructure *s = NULL;
    GstCapsFeatures *f = gst_caps_features_from_string (features);

    for (guint i = 0; i < size; i++) {
      if (gst_caps_features_is_equal (f, gst_caps_get_features (caps, i))) {
        s = gst_caps_get_structure (caps, i);
        break;
      }
    }

    gst_caps_features_free (f);

    if (!s)
      return FALSE;

    /* When we use this function to get a fixated caps, we should set
     * a single value instead of a list in the corresponding field.
     */
    if (gst_value_list_get_size (&list) == 1)
      gst_structure_set_value (s, field, gst_value_list_get_value (&list, 0));
    else
      gst_structure_set_value (s, field, &list);
  } else {
    gst_caps_set_value (caps, field, &list);
  }

  g_value_unset (&list);

  return TRUE;
}

gboolean
gst_msdkcaps_video_info_from_caps (const GstCaps * caps,
    GstVideoInfo * info, guint64 * modifier)
{
  g_return_val_if_fail (caps != NULL, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

#ifndef _WIN32
  if (gst_video_is_dma_drm_caps (caps)) {
    GstVideoInfoDmaDrm *drm_info = gst_video_info_dma_drm_new_from_caps (caps);
    if (!drm_info)
      goto failed;

    if (!gst_video_info_dma_drm_to_video_info (drm_info, info)) {
      gst_video_info_dma_drm_free (drm_info);
      goto failed;
    }
    if (modifier)
      *modifier = drm_info->drm_modifier;

    gst_video_info_dma_drm_free (drm_info);
  } else
#endif

  if (!gst_video_info_from_caps (info, caps))
    goto failed;

  return TRUE;

failed:
  GST_ERROR_OBJECT (caps, "Failed to get video info fom caps");
  return FALSE;
}

#ifndef _WIN32
GstCaps *
gst_msdkcaps_video_info_to_drm_caps (GstVideoInfo * info, guint64 modifier)
{
  GstVideoInfoDmaDrm drm_info;

  gst_video_info_dma_drm_init (&drm_info);
  drm_info.vinfo = *info;
  drm_info.drm_fourcc =
      gst_va_drm_fourcc_from_video_format (GST_VIDEO_INFO_FORMAT (info));
  drm_info.drm_modifier = modifier;

  return gst_video_info_dma_drm_to_caps (&drm_info);
}

guint64
get_msdkcaps_get_modifier (const GstCaps * caps)
{
  guint64 modifier = DRM_FORMAT_MOD_INVALID;
  guint size = gst_caps_get_size (caps);

  for (guint i = 0; i < size; i++) {
    GstCapsFeatures *f = gst_caps_get_features (caps, i);

    if (gst_caps_features_contains (f, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
      GstStructure *s = gst_caps_get_structure (caps, i);
      const GValue *drm_fmts = gst_structure_get_value (s, "drm-format");
      const gchar *drm_str = NULL;

      if (!drm_fmts)
        continue;

      if (G_VALUE_HOLDS_STRING (drm_fmts))
        drm_str = g_value_get_string (drm_fmts);
      else if (GST_VALUE_HOLDS_LIST (drm_fmts)) {
        const GValue *val = gst_value_list_get_value (drm_fmts, 0);
        drm_str = g_value_get_string (val);
      }

      gst_video_dma_drm_fourcc_from_string (drm_str, &modifier);
    }
  }

  GST_DEBUG ("got modifier: 0x%016lx", modifier);

  return modifier;
}
#endif
