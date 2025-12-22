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

#include <fcntl.h>
#include <unistd.h>

#include <va/va_drm.h>
#include "msdk.h"
#include "msdk_libva.h"

struct fourcc_map
{
  mfxU32 mfx_fourcc;
  guint32 va_fourcc;
};

struct rt_map
{
  mfxU32 mfx_rt_format;
  guint32 va_rt_format;
};

#define FOURCC_MFX_TO_VA(MFX, VA) \
    { MFX_FOURCC_##MFX, VA_FOURCC_##VA }

#define RT_MFX_TO_VA(MFX, VA) \
    { MFX_CHROMAFORMAT_##MFX, VA_RT_FORMAT_##VA }

static const struct fourcc_map gst_msdk_fourcc_mfx_to_va[] = {
  FOURCC_MFX_TO_VA (NV12, NV12),
  FOURCC_MFX_TO_VA (YUY2, YUY2),
  FOURCC_MFX_TO_VA (UYVY, UYVY),
  FOURCC_MFX_TO_VA (YV12, YV12),
  FOURCC_MFX_TO_VA (RGB4, ARGB),
  FOURCC_MFX_TO_VA (P8, P208),
  FOURCC_MFX_TO_VA (P010, P010),
#if (MFX_VERSION >= 1031)
  FOURCC_MFX_TO_VA (P016, P016),
  FOURCC_MFX_TO_VA (Y216, Y216),
  FOURCC_MFX_TO_VA (Y416, Y416),
#endif
#if (MFX_VERSION >= 1028)
  FOURCC_MFX_TO_VA (RGB565, RGB565),
#endif
  FOURCC_MFX_TO_VA (AYUV, AYUV),
  FOURCC_MFX_TO_VA (A2RGB10, A2R10G10B10),
#if (MFX_VERSION >= 1027)
  FOURCC_MFX_TO_VA (Y210, Y210),
  FOURCC_MFX_TO_VA (Y410, Y410),
#endif
  FOURCC_MFX_TO_VA (BGR4, ABGR),

#if (MFX_VERSION >= 2004)
  FOURCC_MFX_TO_VA (RGBP, RGBP),
  FOURCC_MFX_TO_VA (BGRP, BGRP),
#endif

  {0, 0}
};

static const struct rt_map gst_msdk_rt_mfx_to_va[] = {
  RT_MFX_TO_VA (YUV420, YUV420),
  RT_MFX_TO_VA (YUV422, YUV422),
  RT_MFX_TO_VA (YUV444, YUV444),
  {0, 0}
};

mfxStatus
gst_msdk_get_mfx_status_from_va_status (VAStatus va_res)
{
  mfxStatus mfxRes = MFX_ERR_NONE;

  switch (va_res) {
    case VA_STATUS_SUCCESS:
      mfxRes = MFX_ERR_NONE;
      break;
    case VA_STATUS_ERROR_ALLOCATION_FAILED:
      mfxRes = MFX_ERR_MEMORY_ALLOC;
      break;
    case VA_STATUS_ERROR_ATTR_NOT_SUPPORTED:
    case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
    case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
    case VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT:
    case VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE:
    case VA_STATUS_ERROR_FLAG_NOT_SUPPORTED:
    case VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED:
      mfxRes = MFX_ERR_UNSUPPORTED;
      break;
    case VA_STATUS_ERROR_INVALID_DISPLAY:
    case VA_STATUS_ERROR_INVALID_CONFIG:
    case VA_STATUS_ERROR_INVALID_CONTEXT:
    case VA_STATUS_ERROR_INVALID_SURFACE:
    case VA_STATUS_ERROR_INVALID_BUFFER:
    case VA_STATUS_ERROR_INVALID_IMAGE:
    case VA_STATUS_ERROR_INVALID_SUBPICTURE:
      mfxRes = MFX_ERR_NOT_INITIALIZED;
      break;
    case VA_STATUS_ERROR_INVALID_PARAMETER:
      mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
      break;
    default:
      mfxRes = MFX_ERR_UNKNOWN;
      break;
  }
  return mfxRes;
}

guint
gst_msdk_get_va_fourcc_from_mfx_fourcc (mfxU32 fourcc)
{
  const struct fourcc_map *m = gst_msdk_fourcc_mfx_to_va;

  for (; m->mfx_fourcc != 0; m++) {
    if (m->mfx_fourcc == fourcc)
      return m->va_fourcc;
  }

  return 0;
}

guint
gst_msdk_get_mfx_fourcc_from_va_fourcc (guint32 fourcc)
{
  const struct fourcc_map *m = gst_msdk_fourcc_mfx_to_va;

  for (; m->va_fourcc != 0; m++) {
    if (m->va_fourcc == fourcc)
      return m->mfx_fourcc;
  }

  return 0;
}

guint
gst_msdk_get_va_rt_format_from_mfx_rt_format (mfxU32 format)
{
  const struct rt_map *m = gst_msdk_rt_mfx_to_va;

  for (; m->mfx_rt_format != 0; m++) {
    if (m->mfx_rt_format == format)
      return m->va_rt_format;
  }

  return 0;
}

guint
gst_msdk_get_mfx_rt_format_from_va_rt_format (guint32 format)
{
  const struct rt_map *m = gst_msdk_rt_mfx_to_va;

  for (; m->va_rt_format != 0; m++) {
    if (m->va_rt_format == format)
      return m->mfx_rt_format;
  }

  return 0;
}
