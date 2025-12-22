/* GStreamer Intel MSDK plugin
 * Copyright (c) 2016, Oblong Industries, Inc.
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

#include "msdk.h"
#include "gstmsdkcontext.h"

GST_DEBUG_CATEGORY_EXTERN (gst_msdk_debug);
#define GST_CAT_DEFAULT gst_msdk_debug

#define INVALID_INDEX         ((guint) -1)
#define GST_MSDK_ALIGNMENT_PADDING(num,padding) ((padding) - ((num) & ((padding) - 1)))

struct map
{
  GstVideoFormat format;
  mfxU16 mfx_chroma_format;
  mfxU32 mfx_fourcc;
};

#define GST_VIDEO_INFO_TO_MFX_MAP(FORMAT, CHROMA, FOURCC) \
    { GST_VIDEO_FORMAT_##FORMAT, MFX_CHROMAFORMAT_##CHROMA, MFX_FOURCC_##FOURCC }

static const struct map gst_msdk_video_format_to_mfx_map[] = {
  GST_VIDEO_INFO_TO_MFX_MAP (NV12, YUV420, NV12),
  GST_VIDEO_INFO_TO_MFX_MAP (YV12, YUV420, YV12),
  GST_VIDEO_INFO_TO_MFX_MAP (I420, YUV420, YV12),
  GST_VIDEO_INFO_TO_MFX_MAP (P010_10LE, YUV420, P010),
  GST_VIDEO_INFO_TO_MFX_MAP (YUY2, YUV422, YUY2),
  GST_VIDEO_INFO_TO_MFX_MAP (UYVY, YUV422, UYVY),
  GST_VIDEO_INFO_TO_MFX_MAP (BGRA, YUV444, RGB4),
  GST_VIDEO_INFO_TO_MFX_MAP (BGRx, YUV444, RGB4),
#if (MFX_VERSION >= 1028)
  GST_VIDEO_INFO_TO_MFX_MAP (RGB16, YUV444, RGB565),
#endif
  GST_VIDEO_INFO_TO_MFX_MAP (VUYA, YUV444, AYUV),
  GST_VIDEO_INFO_TO_MFX_MAP (BGR10A2_LE, YUV444, A2RGB10),
#if (MFX_VERSION >= 1027)
  GST_VIDEO_INFO_TO_MFX_MAP (Y210, YUV422, Y210),
  GST_VIDEO_INFO_TO_MFX_MAP (Y410, YUV444, Y410),
#endif
#if (MFX_VERSION >= 1031)
  /* P016 is used for semi-planar 12 bits format in MSDK */
  GST_VIDEO_INFO_TO_MFX_MAP (P012_LE, YUV420, P016),
  /* Y216 is used for 12bit 4:2:2 format in MSDK */
  GST_VIDEO_INFO_TO_MFX_MAP (Y212_LE, YUV422, Y216),
  /* Y416 is used for 12bit 4:4:4:4 format in MSDK */
  GST_VIDEO_INFO_TO_MFX_MAP (Y412_LE, YUV444, Y416),
#endif
#if (MFX_VERSION >=2004)
  GST_VIDEO_INFO_TO_MFX_MAP (RGBP, YUV444, RGBP),
  GST_VIDEO_INFO_TO_MFX_MAP (BGRP, YUV444, BGRP),
#endif
  {0, 0, 0}
};

const gchar *
msdk_status_to_string (mfxStatus status)
{
  switch (status) {
      /* no error */
    case MFX_ERR_NONE:
      return "no error";
      /* reserved for unexpected errors */
    case MFX_ERR_UNKNOWN:
      return "unknown error";
      /* error codes <0 */
    case MFX_ERR_NULL_PTR:
      return "null pointer";
    case MFX_ERR_UNSUPPORTED:
      return "undeveloped feature";
    case MFX_ERR_MEMORY_ALLOC:
      return "failed to allocate memory";
    case MFX_ERR_NOT_ENOUGH_BUFFER:
      return "insufficient buffer at input/output";
    case MFX_ERR_INVALID_HANDLE:
      return "invalid handle";
    case MFX_ERR_LOCK_MEMORY:
      return "failed to lock the memory block";
    case MFX_ERR_NOT_INITIALIZED:
      return "member function called before initialization";
    case MFX_ERR_NOT_FOUND:
      return "the specified object is not found";
    case MFX_ERR_MORE_DATA:
      return "expect more data at input";
    case MFX_ERR_MORE_SURFACE:
      return "expect more surface at output";
    case MFX_ERR_ABORTED:
      return "operation aborted";
    case MFX_ERR_DEVICE_LOST:
      return "lose the HW acceleration device";
    case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
      return "incompatible video parameters";
    case MFX_ERR_INVALID_VIDEO_PARAM:
      return "invalid video parameters";
    case MFX_ERR_UNDEFINED_BEHAVIOR:
      return "undefined behavior";
    case MFX_ERR_DEVICE_FAILED:
      return "device operation failure";
    case MFX_ERR_MORE_BITSTREAM:
      return "expect more bitstream buffers at output";
#if (MFX_VERSION < 2000)
    case MFX_ERR_INCOMPATIBLE_AUDIO_PARAM:
      return "incompatible audio parameters";
    case MFX_ERR_INVALID_AUDIO_PARAM:
      return "invalid audio parameters";
#endif
      /* warnings >0 */
    case MFX_WRN_IN_EXECUTION:
      return "the previous asynchronous operation is in execution";
    case MFX_WRN_DEVICE_BUSY:
      return "the HW acceleration device is busy";
    case MFX_WRN_VIDEO_PARAM_CHANGED:
      return "the video parameters are changed during decoding";
    case MFX_WRN_PARTIAL_ACCELERATION:
      return "SW is used";
    case MFX_WRN_INCOMPATIBLE_VIDEO_PARAM:
      return "incompatible video parameters";
    case MFX_WRN_VALUE_NOT_CHANGED:
      return "the value is saturated based on its valid range";
    case MFX_WRN_OUT_OF_RANGE:
      return "the value is out of valid range";
    case MFX_WRN_FILTER_SKIPPED:
      return "one of requested filters has been skipped";
#if (MFX_VERSION < 2000)
    case MFX_WRN_INCOMPATIBLE_AUDIO_PARAM:
      return "incompatible audio parameters";
#endif
    default:
      break;
  }
  return "undefined error";
}

mfxU16
msdk_get_platform_codename (mfxSession session)
{
  mfxU16 codename = MFX_PLATFORM_UNKNOWN;

#if (MFX_VERSION >= 1019)
  {
    mfxStatus status;
    mfxPlatform platform = { 0 };
    status = MFXVideoCORE_QueryPlatform (session, &platform);
    if (MFX_ERR_NONE == status)
      codename = platform.CodeName;
  }
#endif

  return codename;
}

#if (MFX_VERSION >= 2000)

gpointer
msdk_get_impl_description (const mfxLoader * loader, mfxU32 impl_idx)
{
  mfxImplDescription *desc = NULL;
  mfxStatus status = MFX_ERR_NONE;

  g_return_val_if_fail (loader != NULL, NULL);

  status = MFXEnumImplementations (*loader, impl_idx,
      MFX_IMPLCAPS_IMPLDESCSTRUCTURE, (mfxHDL *) & desc);
  if (status != MFX_ERR_NONE) {
    GST_ERROR ("Failed to get implementation description, %s",
        msdk_status_to_string (status));
    return NULL;
  }

  return desc;
}

gboolean
msdk_release_impl_description (const mfxLoader * loader, gpointer impl_desc)
{
  mfxStatus status = MFX_ERR_NONE;
  mfxImplDescription *desc = (mfxImplDescription *) impl_desc;

  g_return_val_if_fail (loader != NULL, FALSE);

  status = MFXDispReleaseImplDescription (*loader, desc);
  if (status != MFX_ERR_NONE)
    return FALSE;

  return TRUE;
}

static mfxStatus
_set_pci_id (mfxHDL handle, mfxConfig cfg)
{
#ifndef _WIN32
  VAStatus va_status;
  mfxStatus mfx_status;
  VADisplay dpy = handle;
  mfxVariant impl_value = {
    .Type = MFX_VARIANT_TYPE_U16,
  };
  VADisplayAttribute attr = {
    .type = VADisplayPCIID,
  };

  va_status = vaGetDisplayAttributes (dpy, &attr, 1);
  if (va_status != VA_STATUS_SUCCESS ||
      attr.flags == VA_DISPLAY_ATTRIB_NOT_SUPPORTED)
    return MFX_ERR_UNSUPPORTED;

  impl_value.Data.U16 = (attr.value & 0xFFFF);
  mfx_status = MFXSetConfigFilterProperty (cfg,
      (const mfxU8 *) "mfxImplDescription.mfxDeviceDescription.DeviceID",
      impl_value);

  if (mfx_status != MFX_ERR_NONE) {
    GST_ERROR ("Failed to add an additional MFX configuration (%s)",
        msdk_status_to_string (mfx_status));
  }

  return mfx_status;
#else
  return MFX_ERR_NONE;
#endif
}

mfxStatus
msdk_init_msdk_session (mfxHDL handle, mfxIMPL impl, mfxVersion * pver,
    MsdkSession * msdk_session)
{
  mfxStatus sts = MFX_ERR_NONE;
  mfxLoader loader = NULL;
  mfxSession session = NULL;
  mfxU32 impl_idx = 0;
  mfxConfig cfg;
  mfxVariant impl_value;

  loader = msdk_session->loader;

  if (!loader) {
    loader = MFXLoad ();

    GST_INFO ("Use the Intel oneVPL SDK to create MFX session");

    if (!loader) {
      GST_WARNING ("Failed to create a MFX loader");
      return MFX_ERR_UNKNOWN;
    }

    /* Create configurations for implementation */
    cfg = MFXCreateConfig (loader);

    if (!cfg) {
      GST_ERROR ("Failed to create a MFX configuration");
      MFXUnload (loader);
      return MFX_ERR_UNKNOWN;
    }

    impl_value.Type = MFX_VARIANT_TYPE_U32;
    impl_value.Data.U32 =
        (impl ==
        MFX_IMPL_SOFTWARE) ? MFX_IMPL_TYPE_SOFTWARE : MFX_IMPL_TYPE_HARDWARE;
    sts =
        MFXSetConfigFilterProperty (cfg,
        (const mfxU8 *) "mfxImplDescription.Impl", impl_value);

    if (sts != MFX_ERR_NONE) {
      GST_ERROR ("Failed to add an additional MFX configuration (%s)",
          msdk_status_to_string (sts));
      MFXUnload (loader);
      return sts;
    }

    impl_value.Type = MFX_VARIANT_TYPE_U32;
    impl_value.Data.U32 = pver->Version;
    sts =
        MFXSetConfigFilterProperty (cfg,
        (const mfxU8 *) "mfxImplDescription.ApiVersion.Version", impl_value);

    if (sts != MFX_ERR_NONE) {
      GST_ERROR ("Failed to add an additional MFX configuration (%s)",
          msdk_status_to_string (sts));
      MFXUnload (loader);
      return sts;
    }

    sts = _set_pci_id (handle, cfg);

    if (sts != MFX_ERR_NONE) {
      MFXUnload (loader);
      return sts;
    }
  }

  while (1) {
    /* Enumerate all implementations */
    mfxImplDescription *impl_desc;

    sts = MFXEnumImplementations (loader, impl_idx,
        MFX_IMPLCAPS_IMPLDESCSTRUCTURE, (mfxHDL *) & impl_desc);

    /* Failed to find an available implementation */
    if (sts == MFX_ERR_NOT_FOUND)
      break;
    else if (sts != MFX_ERR_NONE) {
      impl_idx++;
      continue;
    }

    sts = MFXCreateSession (loader, impl_idx, &session);
    MFXDispReleaseImplDescription (loader, impl_desc);

    if (sts == MFX_ERR_NONE) {
      msdk_session->impl_idx = impl_idx;
      break;
    }

    impl_idx++;
  }

  if (sts != MFX_ERR_NONE) {
    GST_ERROR ("Failed to create a MFX session (%s)",
        msdk_status_to_string (sts));

    if (!msdk_session->loader)
      MFXUnload (loader);

    return sts;
  }

  msdk_session->session = session;
  msdk_session->loader = loader;

  return MFX_ERR_NONE;
}

#else

gpointer
msdk_get_impl_description (const mfxLoader * loader, mfxU32 impl_idx)
{
  return NULL;
}

gboolean
msdk_release_impl_description (const mfxLoader * loader, gpointer impl_desc)
{
  return TRUE;
}

/* handle is not used here */
mfxStatus
msdk_init_msdk_session (mfxHDL handle, mfxIMPL impl, mfxVersion * pver,
    MsdkSession * msdk_session)
{
  mfxStatus status;
  mfxSession session = NULL;
  mfxInitParam init_par = { impl, *pver };

  GST_INFO ("Use the " MFX_API_SDK " to create MFX session");

#if (MFX_VERSION >= 1025)
  init_par.GPUCopy = 1;
#endif

  status = MFXInitEx (init_par, &session);

  if (status != MFX_ERR_NONE) {
    GST_WARNING ("Failed to initialize a MFX session (%s)",
        msdk_status_to_string (status));
    return status;
  }

  msdk_session->session = session;
  msdk_session->loader = NULL;
  msdk_session->impl_idx = 0;

  return MFX_ERR_NONE;
}

void
GstMFXUnload (mfxLoader loader)
{
  g_assert (loader == NULL);
}

#endif

void
msdk_close_mfx_session (mfxSession session)
{
  mfxStatus status;

  if (!session)
    return;

  status = MFXClose (session);
  if (status != MFX_ERR_NONE)
    GST_ERROR ("Close failed (%s)", msdk_status_to_string (status));
}

void
msdk_close_session (MsdkSession * msdk_session)
{
  msdk_close_mfx_session (msdk_session->session);
  MFXUnload (msdk_session->loader);
}

MsdkSession
msdk_open_session (mfxHDL handle, mfxIMPL impl)
{
  mfxSession session = NULL;
  mfxVersion version = { {1, 1}
  };
  mfxIMPL implementation;
  mfxStatus status;
  MsdkSession msdk_session;

  static const gchar *implementation_names[] = {
    "AUTO", "SOFTWARE", "HARDWARE", "AUTO_ANY", "HARDWARE_ANY", "HARDWARE2",
    "HARDWARE3", "HARDWARE4", "RUNTIME"
  };

  msdk_session.session = NULL;
  msdk_session.loader = NULL;
  msdk_session.impl_idx = 0;
  status = msdk_init_msdk_session (handle, impl, &version, &msdk_session);

  if (status != MFX_ERR_NONE)
    return msdk_session;
  else
    session = msdk_session.session;

  status = MFXQueryIMPL (session, &implementation);
  if (status != MFX_ERR_NONE) {
    GST_ERROR ("Query implementation failed (%s)",
        msdk_status_to_string (status));
    goto failed;
  }

  status = MFXQueryVersion (session, &version);
  if (status != MFX_ERR_NONE) {
    GST_ERROR ("Query version failed (%s)", msdk_status_to_string (status));
    goto failed;
  }

  GST_INFO ("MFX implementation: 0x%04x (%s)", implementation,
      implementation_names[MFX_IMPL_BASETYPE (implementation)]);
  GST_INFO ("MFX version: %d.%d", version.Major, version.Minor);

  return msdk_session;

failed:
  msdk_close_session (&msdk_session);
  msdk_session.session = NULL;
  msdk_session.loader = NULL;
  msdk_session.impl_idx = 0;
  return msdk_session;
}

void
gst_msdk_set_video_alignment (GstVideoInfo * info, guint alloc_w, guint alloc_h,
    GstVideoAlignment * alignment)
{
  guint i, width, height;
  guint stride_align = 127;     /* 128-byte alignment */

  width = GST_VIDEO_INFO_WIDTH (info);
  height = GST_VIDEO_INFO_HEIGHT (info);

  g_assert (alloc_w == 0 || alloc_w >= width);
  g_assert (alloc_h == 0 || alloc_h >= height);

  if (alloc_w == 0)
    alloc_w = width;

  if (alloc_h == 0)
    alloc_h = height;

  /* PitchAlignment is set to 64 bytes in the media driver for the following formats */
  if (GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_BGRA ||
      GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_BGRx ||
      GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_BGR10A2_LE ||
      GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_RGB16)
    stride_align = 63;          /* 64-byte alignment */

  gst_video_alignment_reset (alignment);
  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (info); i++)
    alignment->stride_align[i] = stride_align;

  alignment->padding_right = GST_ROUND_UP_16 (alloc_w) - width;
  alignment->padding_bottom = GST_ROUND_UP_32 (alloc_h) - height;
}

static const struct map *
_map_lookup_format (GstVideoFormat format)
{
  const struct map *m = gst_msdk_video_format_to_mfx_map;

  for (; m->format != 0; m++) {
    if (m->format == format)
      return m;
  }
  return NULL;
}

gint
gst_msdk_get_mfx_chroma_from_format (GstVideoFormat format)
{
  const struct map *const m = _map_lookup_format (format);

  return m ? m->mfx_chroma_format : -1;
}

gint
gst_msdk_get_mfx_fourcc_from_format (GstVideoFormat format)
{
  const struct map *const m = _map_lookup_format (format);

  return m ? m->mfx_fourcc : -1;
}

void
gst_msdk_set_mfx_frame_info_from_video_info (mfxFrameInfo * mfx_info,
    const GstVideoInfo * info)
{
  g_return_if_fail (info && mfx_info);

  /* Use the first component in info to calculate mfx width / height */
  mfx_info->Width =
      GST_ROUND_UP_16 (GST_VIDEO_INFO_COMP_STRIDE (info,
          0) / GST_VIDEO_INFO_COMP_PSTRIDE (info, 0));

  if (GST_VIDEO_INFO_N_PLANES (info) > 1)
    mfx_info->Height =
        GST_ROUND_UP_32 (GST_VIDEO_INFO_COMP_OFFSET (info,
            1) / GST_VIDEO_INFO_COMP_STRIDE (info, 0));
  else
    mfx_info->Height =
        GST_ROUND_UP_32 (GST_VIDEO_INFO_SIZE (info) /
        GST_VIDEO_INFO_COMP_STRIDE (info, 0));

  mfx_info->CropW = GST_VIDEO_INFO_WIDTH (info);
  mfx_info->CropH = GST_VIDEO_INFO_HEIGHT (info);
  mfx_info->FrameRateExtN = GST_VIDEO_INFO_FPS_N (info);
  mfx_info->FrameRateExtD = GST_VIDEO_INFO_FPS_D (info);
  mfx_info->AspectRatioW = GST_VIDEO_INFO_PAR_N (info);
  mfx_info->AspectRatioH = GST_VIDEO_INFO_PAR_D (info);
  mfx_info->PicStruct =
      !GST_VIDEO_INFO_IS_INTERLACED (info) ? MFX_PICSTRUCT_PROGRESSIVE :
      MFX_PICSTRUCT_UNKNOWN;
  mfx_info->FourCC =
      gst_msdk_get_mfx_fourcc_from_format (GST_VIDEO_INFO_FORMAT (info));
  mfx_info->ChromaFormat =
      gst_msdk_get_mfx_chroma_from_format (GST_VIDEO_INFO_FORMAT (info));

  switch (mfx_info->FourCC) {
    case MFX_FOURCC_P010:
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y210:
#endif
      mfx_info->BitDepthLuma = 10;
      mfx_info->BitDepthChroma = 10;
      mfx_info->Shift = 1;

      break;

#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y410:
      mfx_info->BitDepthLuma = 10;
      mfx_info->BitDepthChroma = 10;
      mfx_info->Shift = 0;

      break;
#endif

#if (MFX_VERSION >= 1031)
    case MFX_FOURCC_P016:
    case MFX_FOURCC_Y216:
    case MFX_FOURCC_Y416:
      mfx_info->BitDepthLuma = 12;
      mfx_info->BitDepthChroma = 12;
      mfx_info->Shift = 1;

      break;
#endif

    default:
      break;
  }

  return;
}

gboolean
gst_msdk_is_va_mem (GstMemory * mem)
{
  GstAllocator *allocator;

  allocator = mem->allocator;
  if (!allocator)
    return FALSE;

  return g_str_equal (allocator->mem_type, "VAMemory");
}

GstVideoFormat
gst_msdk_get_video_format_from_mfx_fourcc (mfxU32 fourcc)
{
  const struct map *m = gst_msdk_video_format_to_mfx_map;

  for (; m->mfx_fourcc != 0; m++) {
    if (m->mfx_fourcc == fourcc)
      return m->format;
  }

  return GST_VIDEO_FORMAT_UNKNOWN;
}

void
gst_msdk_get_video_format_list (GValue * formats)
{
  GValue gfmt = G_VALUE_INIT;
  const struct map *m = gst_msdk_video_format_to_mfx_map;

  g_value_init (&gfmt, G_TYPE_UINT);

  for (; m->format != 0; m++) {
    g_value_set_uint (&gfmt, m->format);
    gst_value_list_append_value (formats, &gfmt);
  }

  g_value_unset (&gfmt);
}

void
gst_msdk_update_mfx_frame_info_from_mfx_video_param (mfxFrameInfo * mfx_info,
    mfxVideoParam * param)
{
  mfx_info->BitDepthLuma = param->mfx.FrameInfo.BitDepthLuma;
  mfx_info->BitDepthChroma = param->mfx.FrameInfo.BitDepthChroma;
  mfx_info->Shift = param->mfx.FrameInfo.Shift;
}

void
gst_msdk_get_mfx_video_orientation_from_video_direction (guint value,
    guint * mfx_mirror, guint * mfx_rotation)
{
  *mfx_mirror = MFX_MIRRORING_DISABLED;
  *mfx_rotation = MFX_ANGLE_0;

  switch (value) {
    case GST_VIDEO_ORIENTATION_IDENTITY:
      *mfx_mirror = MFX_MIRRORING_DISABLED;
      *mfx_rotation = MFX_ANGLE_0;
      break;
    case GST_VIDEO_ORIENTATION_HORIZ:
      *mfx_mirror = MFX_MIRRORING_HORIZONTAL;
      *mfx_rotation = MFX_ANGLE_0;
      break;
    case GST_VIDEO_ORIENTATION_VERT:
      *mfx_mirror = MFX_MIRRORING_VERTICAL;
      *mfx_rotation = MFX_ANGLE_0;
      break;
    case GST_VIDEO_ORIENTATION_90R:
      *mfx_mirror = MFX_MIRRORING_DISABLED;
      *mfx_rotation = MFX_ANGLE_90;
      break;
    case GST_VIDEO_ORIENTATION_180:
      *mfx_mirror = MFX_MIRRORING_DISABLED;
      *mfx_rotation = MFX_ANGLE_180;
      break;
    case GST_VIDEO_ORIENTATION_90L:
      *mfx_mirror = MFX_MIRRORING_DISABLED;
      *mfx_rotation = MFX_ANGLE_270;
      break;
    case GST_VIDEO_ORIENTATION_UL_LR:
      *mfx_mirror = MFX_MIRRORING_HORIZONTAL;
      *mfx_rotation = MFX_ANGLE_90;
      break;
    case GST_VIDEO_ORIENTATION_UR_LL:
      *mfx_mirror = MFX_MIRRORING_VERTICAL;
      *mfx_rotation = MFX_ANGLE_90;
      break;
    default:
      break;
  }
}

gboolean
gst_msdk_load_plugin (mfxSession session, const mfxPluginUID * uid,
    mfxU32 version, const gchar * plugin)
{
#if (MFX_VERSION < 2000)
  mfxStatus status;

  status = MFXVideoUSER_Load (session, uid, version);

  if (status == MFX_ERR_UNDEFINED_BEHAVIOR) {
    GST_WARNING ("Media SDK Plugin for %s has been loaded", plugin);
  } else if (status < MFX_ERR_NONE) {
    GST_ERROR ("Media SDK Plugin for %s load failed (%s)", plugin,
        msdk_status_to_string (status));
    return FALSE;
  } else if (status > MFX_ERR_NONE) {
    GST_WARNING ("Media SDK Plugin for %s load warning: %s", plugin,
        msdk_status_to_string (status));
  }
#endif

  return TRUE;
}
