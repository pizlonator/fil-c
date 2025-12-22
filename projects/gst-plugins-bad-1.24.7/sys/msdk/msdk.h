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

#ifndef __MSDK_H__
#define __MSDK_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/allocators/allocators.h>

#include <mfxvideo.h>

#if (MFX_VERSION < 2000)
#include <mfxplugin.h>
#else
#include <mfxdispatcher.h>

#define mfxPluginUID char
static const char MFX_PLUGINID_HEVCD_SW;
static const char MFX_PLUGINID_HEVCD_HW;
static const char MFX_PLUGINID_HEVCE_SW;
static const char MFX_PLUGINID_HEVCE_HW;
static const char MFX_PLUGINID_VP8D_HW;
static const char MFX_PLUGINID_VP9E_HW;
static const char MFX_PLUGINID_VP9D_HW;
#endif

#if (MFX_VERSION >= 2000)
#define MFX_API_SDK  "Intel(R) oneVPL"
#else
#define MFX_API_SDK  "Intel(R) Media SDK"
#endif


#define MFX_RUNTIME_VERSION_ATLEAST(MFX_VERSION, MAJOR, MINOR) \
    ((MFX_VERSION.Major > (MAJOR)) ||                           \
    (MFX_VERSION.Major == (MAJOR) && MFX_VERSION.Minor >= (MINOR)))

G_BEGIN_DECLS

#define GST_MSDK_CAPS_MAKE(format) \
  GST_VIDEO_CAPS_MAKE (format) ", " \
  "interlace-mode = (string) progressive"

#ifndef _WIN32
#define GST_MSDK_CAPS_MAKE_WITH_DMABUF_FEATURE(dmaformat) \
  GST_VIDEO_CAPS_MAKE_WITH_FEATURES(GST_CAPS_FEATURE_MEMORY_DMABUF, dmaformat) ", " \
  "interlace-mode = (string) progressive"
#define GST_MSDK_CAPS_MAKE_WITH_VA_FEATURE(vaformat) \
  GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:VAMemory", vaformat) ", " \
  "interlace-mode = (string) progressive"

#define GST_MSDK_CAPS_STR(format,dmaformat) \
  GST_MSDK_CAPS_MAKE (format) "; " \
  GST_MSDK_CAPS_MAKE_WITH_DMABUF_FEATURE (dmaformat)
#else
#define GST_MSDK_CAPS_MAKE_WITH_D3D11_FEATURE(d3d11format) \
  GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:D3D11Memory", d3d11format) ", " \
  "interlace-mode = (string) progressive"

#define GST_MSDK_CAPS_STR(format,dmaformat) \
  GST_MSDK_CAPS_MAKE (format)
#endif

#if (MFX_VERSION < 2000)
typedef void * mfxLoader;

void GstMFXUnload (mfxLoader loader);

/* To avoid MFXUnload symbol re-define build issue in case of static build.
 * MFXUnload symbol may exists if other plugin built its own libmfx dispatcher
 */
#define MFXUnload GstMFXUnload
#endif

typedef struct _MsdkSession MsdkSession;

struct _MsdkSession
{
  mfxU32 impl_idx;
  mfxSession session;
  mfxLoader loader;
};

MsdkSession msdk_open_session (mfxHDL handle, mfxIMPL impl);
void msdk_close_mfx_session (mfxSession session);
void msdk_close_session (MsdkSession * session);

mfxFrameSurface1 *msdk_get_free_surface (mfxFrameSurface1 * surfaces,
    guint size);
void msdk_frame_to_surface (GstVideoFrame * frame, mfxFrameSurface1 * surface);

const gchar *msdk_status_to_string (mfxStatus status);

void gst_msdk_set_video_alignment (GstVideoInfo * info, guint alloc_w, guint alloc_h,
    GstVideoAlignment * alignment);

/* Conversion from Gstreamer to libmfx */
gint gst_msdk_get_mfx_chroma_from_format (GstVideoFormat format);
gint gst_msdk_get_mfx_fourcc_from_format (GstVideoFormat format);
void gst_msdk_set_mfx_frame_info_from_video_info (mfxFrameInfo * mfx_info,
    const GstVideoInfo * info);

gboolean
gst_msdk_is_va_mem (GstMemory * mem);

GstVideoFormat
gst_msdk_get_video_format_from_mfx_fourcc (mfxU32 fourcc);

void
gst_msdk_get_video_format_list (GValue * formats);

void
gst_msdk_update_mfx_frame_info_from_mfx_video_param (mfxFrameInfo * mfx_info,
    mfxVideoParam * param);

void
gst_msdk_get_mfx_video_orientation_from_video_direction (guint value,
    guint * mfx_mirror, guint * mfx_rotation);

gboolean
gst_msdk_load_plugin (mfxSession session, const mfxPluginUID * uid,
    mfxU32 version, const gchar * plugin);

mfxU16
msdk_get_platform_codename (mfxSession session);

mfxStatus
msdk_init_msdk_session (mfxHDL handle, mfxIMPL impl, mfxVersion * pver,
    MsdkSession * msdk_session);

gpointer
msdk_get_impl_description (const mfxLoader * loader, mfxU32 impl_idx);
gboolean
msdk_release_impl_description (const mfxLoader * loader, gpointer impl_desc);

G_END_DECLS

#endif /* __MSDK_H__ */
