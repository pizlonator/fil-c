/*
 * Copyright (C) 2015 Alessandro Decina <twi@centricular.com>
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

#ifndef __GST_CORE_VIDEO_TEXTURE_CACHE_H__
#define __GST_CORE_VIDEO_TEXTURE_CACHE_H__

#include <gst/video/gstvideometa.h>
#include "corevideomemory.h"

G_BEGIN_DECLS

#define GST_TYPE_VIDEO_TEXTURE_CACHE         (gst_video_texture_cache_get_type())
#define GST_VIDEO_TEXTURE_CACHE(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VIDEO_TEXTURE_CACHE, GstVideoTextureCache))
#define GST_VIDEO_TEXTURE_CACHE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GST_TYPE_VIDEO_TEXTURE_CACHE, GstVideoTextureCacheClass))
#define GST_IS_VIDEO_TEXTURE_CACHE(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VIDEO_TEXTURE_CACHE))
#define GST_IS_VIDEO_TEXTURE_CACHE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VIDEO_TEXTURE_CACHE))
#define GST_VIDEO_TEXTURE_CACHE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VIDEO_TEXTURE_CACHE, GstVideoTextureCacheClass))
GType gst_video_texture_cache_get_type     (void);

typedef struct _GstVideoTextureCache
{
  GObject parent;

  GstVideoInfo input_info;
  GstVideoInfo output_info;

  gboolean configured;
  GstCaps *in_caps;
  GstCaps *out_caps;
} GstVideoTextureCache;

typedef struct _GstVideoTextureCacheClass
{
  GObjectClass parent_class;

  void              (*set_format)           (GstVideoTextureCache * cache,
                                             GstVideoFormat in_format,
                                             GstCaps * out_caps);

  GstMemory *       (*create_memory)        (GstVideoTextureCache * cache,
                                             GstAppleCoreVideoPixelBuffer *gpixbuf,
                                             guint plane,
                                             gsize size);
} GstVideoTextureCacheClass;

void                    gst_video_texture_cache_set_format      (GstVideoTextureCache * cache,
                                                                 GstVideoFormat in_format,
                                                                 GstCaps * out_caps);
GstMemory *             gst_video_texture_cache_create_memory   (GstVideoTextureCache * cache,
                                                                 GstAppleCoreVideoPixelBuffer *gpixbuf,
                                                                 guint plane,
                                                                 gsize size);

G_END_DECLS

#endif /* __GST_CORE_VIDEO_TEXTURE_CACHE_H__ */
