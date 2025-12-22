/*
 * GStreamer Intel MSDK plugin
 * Copyright (c) 2022 Intel Corporation. All rights reserved.
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

#include "gstmsdkallocator.h"

static gboolean
map_data (GstBuffer * buffer, mfxFrameSurface1 * mfx_surface,
    const GstVideoInfo * info)
{
  guint stride;
  GstVideoFrame frame;

  if (!gst_video_frame_map (&frame, info, buffer, GST_MAP_READWRITE))
    return FALSE;

  stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, 0);

  switch (GST_VIDEO_INFO_FORMAT (info)) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P012_LE:
      mfx_surface->Data.Y = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
      mfx_surface->Data.UV = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 1);
      mfx_surface->Data.Pitch = (mfxU16) stride;
      break;
    case GST_VIDEO_FORMAT_YV12:
      mfx_surface->Data.Y = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
      mfx_surface->Data.U = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 2);
      mfx_surface->Data.V = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 1);
      mfx_surface->Data.Pitch = (mfxU16) stride;
      break;
    case GST_VIDEO_FORMAT_I420:
      mfx_surface->Data.Y = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
      mfx_surface->Data.U = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 1);
      mfx_surface->Data.V = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 2);
      mfx_surface->Data.Pitch = (mfxU16) stride;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      mfx_surface->Data.Y = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
      mfx_surface->Data.U = mfx_surface->Data.Y + 1;
      mfx_surface->Data.V = mfx_surface->Data.Y + 3;
      mfx_surface->Data.Pitch = (mfxU16) stride;
      break;
    case GST_VIDEO_FORMAT_UYVY:
      mfx_surface->Data.Y = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
      mfx_surface->Data.U = mfx_surface->Data.Y;
      mfx_surface->Data.V = mfx_surface->Data.U + 2;
      mfx_surface->Data.Pitch = (mfxU16) stride;
      break;
    case GST_VIDEO_FORMAT_VUYA:
      mfx_surface->Data.V = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
      mfx_surface->Data.U = mfx_surface->Data.V + 1;
      mfx_surface->Data.Y = mfx_surface->Data.V + 2;
      mfx_surface->Data.A = mfx_surface->Data.V + 3;
      mfx_surface->Data.PitchHigh = (mfxU16) (stride / (1 << 16));
      mfx_surface->Data.PitchLow = (mfxU16) (stride % (1 << 16));
      break;
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
      mfx_surface->Data.B = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
      mfx_surface->Data.G = mfx_surface->Data.B + 1;
      mfx_surface->Data.R = mfx_surface->Data.B + 2;
      mfx_surface->Data.A = mfx_surface->Data.B + 3;
      mfx_surface->Data.Pitch = (mfxU16) stride;
      break;
    case GST_VIDEO_FORMAT_Y210:
    case GST_VIDEO_FORMAT_Y212_LE:
      mfx_surface->Data.Y = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
      mfx_surface->Data.U = mfx_surface->Data.Y + 2;
      mfx_surface->Data.V = mfx_surface->Data.Y + 6;
      mfx_surface->Data.Pitch = (mfxU16) stride;
      break;
    case GST_VIDEO_FORMAT_Y410:
      mfx_surface->Data.Y410 =
          (mfxY410 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
      mfx_surface->Data.Pitch = stride;
      break;
    case GST_VIDEO_FORMAT_Y412_LE:
      mfx_surface->Data.U = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
      mfx_surface->Data.Y = mfx_surface->Data.Y + 2;
      mfx_surface->Data.V = mfx_surface->Data.Y + 4;
      mfx_surface->Data.A = mfx_surface->Data.Y + 6;
      mfx_surface->Data.Pitch = (mfxU16) stride;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  gst_video_frame_unmap (&frame);
  return TRUE;
}

GstMsdkSurface *
gst_msdk_import_sys_mem_to_msdk_surface (GstBuffer * buf,
    const GstVideoInfo * info)
{
  GstMsdkSurface *msdk_surface = NULL;
  GstMapInfo map_info;
  mfxFrameInfo frame_info = { 0, };
  mfxFrameSurface1 *mfx_surface = NULL;

  if (!gst_buffer_map (buf, &map_info, GST_MAP_READ)) {
    GST_ERROR ("Failed to map buffer");
    return msdk_surface;
  }

  mfx_surface = g_slice_new0 (mfxFrameSurface1);
  mfx_surface->Data.MemId = (mfxMemId) map_info.data;

  if (!map_data (buf, mfx_surface, info)) {
    g_slice_free (mfxFrameSurface1, mfx_surface);
    return msdk_surface;
  }

  gst_buffer_unmap (buf, &map_info);

  gst_msdk_set_mfx_frame_info_from_video_info (&frame_info, info);
  mfx_surface->Info = frame_info;

  msdk_surface = g_slice_new0 (GstMsdkSurface);
  msdk_surface->surface = mfx_surface;

  return msdk_surface;
}

GQuark
gst_msdk_frame_surface_quark_get (void)
{
  static gsize g_quark;

  if (g_once_init_enter (&g_quark)) {
    gsize quark = (gsize) g_quark_from_static_string ("GstMsdkFrameSurface");
    g_once_init_leave (&g_quark, quark);
  }
  return g_quark;
}
