/*
 * GStreamer
 * Copyright (C) 2016 Matthew Waters <matthew@centricular.com>
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

#ifndef __GST_VULKAN_BUFFER_POOL_H__
#define __GST_VULKAN_BUFFER_POOL_H__

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>

#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

GST_VULKAN_API
GType gst_vulkan_buffer_pool_get_type (void);
#define GST_TYPE_VULKAN_BUFFER_POOL      (gst_vulkan_buffer_pool_get_type())
#define GST_IS_VULKAN_BUFFER_POOL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_VULKAN_BUFFER_POOL))
#define GST_VULKAN_BUFFER_POOL(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_VULKAN_BUFFER_POOL, GstVulkanBufferPool))
/**
 * GST_VULKAN_BUFFER_POOL_CAST:
 *
 * Since: 1.18
 */
#define GST_VULKAN_BUFFER_POOL_CAST(obj) ((GstVulkanBufferPool*)(obj))

/**
 * GstVulkanBufferPool:
 * @bufferpool: the parent #GstBufferPool
 * @device: the #GstVulkanDevice to allocate Vulkan buffers from
 *
 * Opaque #GstVulkanBufferPool struct
 *
 * Since: 1.18
 */
struct _GstVulkanBufferPool
{
  GstBufferPool bufferpool;

  GstVulkanDevice *device;

  /* <private> */
  gpointer _padding[GST_PADDING];
};

/**
 * GstVulkanBufferPoolClass:
 * @parent_class: the parent #GstBufferPoolClass
 *
 * The #GstVulkanBufferPoolClass structure contains only private data
 *
 * Since: 1.18
 */
struct _GstVulkanBufferPoolClass
{
  GstBufferPoolClass parent_class;

  /* <private> */
  gpointer _padding[GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVulkanBufferPool, gst_object_unref);

GST_VULKAN_API
GstBufferPool *gst_vulkan_buffer_pool_new               (GstVulkanDevice * device);

GST_VULKAN_API
void            gst_vulkan_buffer_pool_config_set_allocation_params
                                                        (GstStructure * config,
                                                         VkBufferUsageFlags usage,
                                                         VkMemoryPropertyFlags mem_properties);

G_END_DECLS

#endif /* __GST_VULKAN_BUFFER_POOL_H__ */
