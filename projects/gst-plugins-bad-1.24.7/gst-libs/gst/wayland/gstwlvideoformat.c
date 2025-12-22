/* GStreamer Wayland Library
 *
 * Copyright (C) 2011 Intel Corporation
 * Copyright (C) 2011 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 * Copyright (C) 2012 Wim Taymans <wim.taymans@gmail.com>
 * Copyright (C) 2014 Collabora Ltd.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstwlvideoformat.h"

#include <drm_fourcc.h>

/* This can be removed once we can bump the required wl_client_dep,
 * which again is blocked by a CI image update, see
 * https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/5275
 */
#ifndef WL_SHM_FORMAT_P010
#define WL_SHM_FORMAT_P010 DRM_FORMAT_P010
#endif

#define GST_CAT_DEFAULT gst_wl_videoformat_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

void
gst_wl_videoformat_init_once (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (gst_wl_videoformat_debug, "wl_videoformat", 0,
        "wl_videoformat library");

    g_once_init_leave (&_init, 1);
  }
}

typedef struct
{
  enum wl_shm_format wl_shm_format;
  guint32 dma_format;
  GstVideoFormat gst_format;
} wl_VideoFormat;

static const wl_VideoFormat wl_formats[] = {
  {WL_SHM_FORMAT_XRGB8888, DRM_FORMAT_XRGB8888, GST_VIDEO_FORMAT_BGRx},
  {WL_SHM_FORMAT_ARGB8888, DRM_FORMAT_ARGB8888, GST_VIDEO_FORMAT_BGRA},
  {WL_SHM_FORMAT_XBGR8888, DRM_FORMAT_XBGR8888, GST_VIDEO_FORMAT_RGBx},
  {WL_SHM_FORMAT_RGBX8888, DRM_FORMAT_RGBX8888, GST_VIDEO_FORMAT_xBGR},
  {WL_SHM_FORMAT_BGRX8888, DRM_FORMAT_BGRX8888, GST_VIDEO_FORMAT_xRGB},
  {WL_SHM_FORMAT_ABGR8888, DRM_FORMAT_ABGR8888, GST_VIDEO_FORMAT_RGBA},
  {WL_SHM_FORMAT_RGBA8888, DRM_FORMAT_RGBA8888, GST_VIDEO_FORMAT_ABGR},
  {WL_SHM_FORMAT_BGRA8888, DRM_FORMAT_BGRA8888, GST_VIDEO_FORMAT_ARGB},
  {WL_SHM_FORMAT_RGB888, DRM_FORMAT_RGB888, GST_VIDEO_FORMAT_BGR},
  {WL_SHM_FORMAT_BGR888, DRM_FORMAT_BGR888, GST_VIDEO_FORMAT_RGB},
  {WL_SHM_FORMAT_RGB565, DRM_FORMAT_RGB565, GST_VIDEO_FORMAT_RGB16},
  {WL_SHM_FORMAT_BGR565, DRM_FORMAT_BGR565, GST_VIDEO_FORMAT_BGR16},

  {WL_SHM_FORMAT_YUYV, DRM_FORMAT_YUYV, GST_VIDEO_FORMAT_YUY2},
  {WL_SHM_FORMAT_YVYU, DRM_FORMAT_YVYU, GST_VIDEO_FORMAT_YVYU},
  {WL_SHM_FORMAT_UYVY, DRM_FORMAT_UYVY, GST_VIDEO_FORMAT_UYVY},
  {WL_SHM_FORMAT_AYUV, DRM_FORMAT_AYUV, GST_VIDEO_FORMAT_AYUV},
  {WL_SHM_FORMAT_NV12, DRM_FORMAT_NV12, GST_VIDEO_FORMAT_NV12},
  {WL_SHM_FORMAT_NV21, DRM_FORMAT_NV21, GST_VIDEO_FORMAT_NV21},
  {WL_SHM_FORMAT_NV16, DRM_FORMAT_NV16, GST_VIDEO_FORMAT_NV16},
  {WL_SHM_FORMAT_NV61, DRM_FORMAT_NV61, GST_VIDEO_FORMAT_NV61},
  {WL_SHM_FORMAT_P010, DRM_FORMAT_P010, GST_VIDEO_FORMAT_P010_10LE},
  {WL_SHM_FORMAT_YUV410, DRM_FORMAT_YUV410, GST_VIDEO_FORMAT_YUV9},
  {WL_SHM_FORMAT_YVU410, DRM_FORMAT_YVU410, GST_VIDEO_FORMAT_YVU9},
  {WL_SHM_FORMAT_YUV411, DRM_FORMAT_YUV411, GST_VIDEO_FORMAT_Y41B},
  {WL_SHM_FORMAT_YUV420, DRM_FORMAT_YUV420, GST_VIDEO_FORMAT_I420},
  {WL_SHM_FORMAT_YVU420, DRM_FORMAT_YVU420, GST_VIDEO_FORMAT_YV12},
  {WL_SHM_FORMAT_YUV422, DRM_FORMAT_YUV422, GST_VIDEO_FORMAT_Y42B},
  {WL_SHM_FORMAT_YUV444, DRM_FORMAT_YUV444, GST_VIDEO_FORMAT_v308},
};

enum wl_shm_format
gst_video_format_to_wl_shm_format (GstVideoFormat format)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (wl_formats); i++)
    if (wl_formats[i].gst_format == format)
      return wl_formats[i].wl_shm_format;

  GST_WARNING ("wayland shm video format not found");
  return -1;
}

guint32
gst_video_format_to_wl_dmabuf_format (GstVideoFormat format)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (wl_formats); i++)
    if (wl_formats[i].gst_format == format)
      return wl_formats[i].dma_format;

  GST_WARNING ("wayland dmabuf video format not found");
  return 0;
}

GstVideoFormat
gst_wl_shm_format_to_video_format (enum wl_shm_format wl_format)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (wl_formats); i++)
    if (wl_formats[i].wl_shm_format == wl_format)
      return wl_formats[i].gst_format;

  return GST_VIDEO_FORMAT_UNKNOWN;
}

GstVideoFormat
gst_wl_dmabuf_format_to_video_format (guint wl_format)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (wl_formats); i++)
    if (wl_formats[i].dma_format == wl_format)
      return wl_formats[i].gst_format;

  return GST_VIDEO_FORMAT_UNKNOWN;
}

const gchar *
gst_wl_shm_format_to_string (enum wl_shm_format wl_format)
{
  return gst_video_format_to_string
      (gst_wl_shm_format_to_video_format (wl_format));
}

gchar *
gst_wl_dmabuf_format_to_string (guint wl_format, guint64 modifier)
{
  GstVideoFormat gst_format = gst_wl_dmabuf_format_to_video_format (wl_format);
  const guint32 fourcc = gst_video_dma_drm_fourcc_from_format (gst_format);

  return gst_video_dma_drm_fourcc_to_string (fourcc, modifier);
}
