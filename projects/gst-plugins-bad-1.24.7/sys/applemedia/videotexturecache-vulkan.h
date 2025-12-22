/*
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
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

#ifndef __GST_CORE_VIDEO_TEXTURE_CACHE_VULKAN_H__
#define __GST_CORE_VIDEO_TEXTURE_CACHE_VULKAN_H__

#include <gst/vulkan/vulkan.h>
#include "videotexturecache.h"

G_BEGIN_DECLS

#define GST_TYPE_VIDEO_TEXTURE_CACHE_VULKAN         (gst_video_texture_cache_vulkan_get_type())
#define GST_VIDEO_TEXTURE_CACHE_VULKAN(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VIDEO_TEXTURE_CACHE_VULKAN, GstVideoTextureCacheVulkan))
#define GST_VIDEO_TEXTURE_CACHE_VULKAN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GST_TYPE_VIDEO_TEXTURE_CACHE_VULKAN, GstVideoTextureCacheVulkanClass))
#define GST_IS_VIDEO_TEXTURE_CACHE_VULKAN(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VIDEO_TEXTURE_CACHE_VULKAN))
#define GST_IS_VIDEO_TEXTURE_CACHE_VULKAN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VIDEO_TEXTURE_CACHE_VULKAN))
#define GST_VIDEO_TEXTURE_CACHE_VULKAN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VIDEO_TEXTURE_CACHE_VULKAN, GstVideoTextureCacheVulkanClass))
GType gst_video_texture_cache_vulkan_get_type     (void);

typedef struct _GstVideoTextureCacheVulkan
{
  GstVideoTextureCache parent;

  GstVulkanDevice *device;
} GstVideoTextureCacheVulkan;

typedef struct _GstVideoTextureCacheVulkanClass
{
  GstVideoTextureCacheClass parent_class;
} GstVideoTextureCacheVulkanClass;

GstVideoTextureCache *  gst_video_texture_cache_vulkan_new             (GstVulkanDevice * device);

G_END_DECLS

#endif /* __GST_CORE_VIDEO_TEXTURE_CACHE_H__ */
