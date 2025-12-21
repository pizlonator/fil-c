/* GStreamer
 * Copyright (C) 2022 Intel Corporation
 *     Author: He Junyan <junyan.he@intel.com>
 *     Author: Liu Yinhang <yinhang.liu@intel.com>
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

#pragma once

#include <gst/video/video-info.h>

G_BEGIN_DECLS

/**
 * GST_VIDEO_DMA_DRM_CAPS_MAKE:
 *
 * Generic caps string for video wit DMABuf(GST_CAPS_FEATURE_MEMORY_DMABUF)
 * feature, for use in pad templates. As drm-format is supposed to be defined
 * at run-time it's not predefined here.
 *
 * Since: 1.24
 */
#define GST_VIDEO_DMA_DRM_CAPS_MAKE                                     \
    "video/x-raw(memory:DMABuf), "                                      \
    "format = (string) DMA_DRM, "                                       \
    "width = " GST_VIDEO_SIZE_RANGE ", "                                \
    "height = " GST_VIDEO_SIZE_RANGE ", "                               \
    "framerate = " GST_VIDEO_FPS_RANGE

typedef struct _GstVideoInfoDmaDrm GstVideoInfoDmaDrm;

/**
 * GstVideoInfoDmaDrm:
 * @vinfo: the associated #GstVideoInfo
 * @drm_fourcc: the fourcc defined by drm
 * @drm_modifier: the drm modifier
 *
 * Information describing a DMABuf image properties. It wraps #GstVideoInfo and
 * adds DRM information such as drm-fourcc and drm-modifier, required for
 * negotiation and mapping.
 *
 * Since: 1.24
 */
struct _GstVideoInfoDmaDrm
{
  GstVideoInfo vinfo;
  guint32 drm_fourcc;
  guint64 drm_modifier;

  /*< private >*/
  guint32 _gst_reserved[GST_PADDING_LARGE];
};

#define GST_TYPE_VIDEO_INFO_DMA_DRM  (gst_video_info_dma_drm_get_type ())

GST_VIDEO_API
GType                gst_video_info_dma_drm_get_type      (void);

GST_VIDEO_API
void                 gst_video_info_dma_drm_free          (GstVideoInfoDmaDrm * drm_info);

GST_VIDEO_API
void                 gst_video_info_dma_drm_init          (GstVideoInfoDmaDrm * drm_info);

GST_VIDEO_API
GstVideoInfoDmaDrm * gst_video_info_dma_drm_new           (void);

GST_VIDEO_API
GstCaps *            gst_video_info_dma_drm_to_caps       (const GstVideoInfoDmaDrm * drm_info);

GST_VIDEO_API
gboolean             gst_video_info_dma_drm_from_caps     (GstVideoInfoDmaDrm * drm_info,
                                                           const GstCaps * caps);

GST_VIDEO_API
gboolean             gst_video_info_dma_drm_from_video_info
                                                          (GstVideoInfoDmaDrm * drm_info,
                                                           const GstVideoInfo * info,
                                                           guint64 modifier);

GST_VIDEO_API
gboolean             gst_video_info_dma_drm_to_video_info (const GstVideoInfoDmaDrm * drm_info,
                                                           GstVideoInfo * info);

GST_VIDEO_API
GstVideoInfoDmaDrm * gst_video_info_dma_drm_new_from_caps (const GstCaps * caps);

GST_VIDEO_API
gboolean             gst_video_is_dma_drm_caps            (const GstCaps * caps);

GST_VIDEO_API
guint32              gst_video_dma_drm_fourcc_from_string (const gchar * format_str,
                                                           guint64 * modifier);

GST_VIDEO_API
gchar *              gst_video_dma_drm_fourcc_to_string   (guint32 fourcc,
                                                           guint64 modifier);

GST_VIDEO_API
guint32              gst_video_dma_drm_fourcc_from_format (GstVideoFormat format);

GST_VIDEO_API
GstVideoFormat       gst_video_dma_drm_fourcc_to_format   (guint32 fourcc);

G_END_DECLS
