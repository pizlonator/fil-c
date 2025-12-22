/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#ifndef __GST_VULKAN_IMAGE_MEMORY_H__
#define __GST_VULKAN_IMAGE_MEMORY_H__

#include <gst/vulkan/gstvkbarrier.h>
#include <gst/vulkan/gstvkdevice.h>

#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_IMAGE_MEMORY_ALLOCATOR (gst_vulkan_image_memory_allocator_get_type())
GST_VULKAN_API
GType gst_vulkan_image_memory_allocator_get_type(void);

#define GST_IS_VULKAN_IMAGE_MEMORY_ALLOCATOR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_VULKAN_IMAGE_MEMORY_ALLOCATOR))
#define GST_IS_VULKAN_IMAGE_MEMORY_ALLOCATOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_VULKAN_IMAGE_MEMORY_ALLOCATOR))
#define GST_VULKAN_IMAGE_MEMORY_ALLOCATOR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_VULKAN_MEMORY_ALLOCATOR, GstVulkanImageMemoryAllocatorClass))
#define GST_VULKAN_IMAGE_MEMORY_ALLOCATOR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_VULKAN_MEMORY_ALLOCATOR, GstVulkanImageMemoryAllocator))
#define GST_VULKAN_IMAGE_MEMORY_ALLOCATOR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_VULKAN_MEMORY_ALLOCATOR, GstVulkanImageMemoryAllocatorClass))
/**
 * GST_VULKAN_IMAGE_MEMORY_ALLOCATOR_CAST:
 *
 * Since: 1.18
 */
#define GST_VULKAN_IMAGE_MEMORY_ALLOCATOR_CAST(obj)            ((GstVulkanImageMemoryAllocator *)(obj))

/**
 * GST_VULKAN_IMAGE_MEMORY_ALLOCATOR_NAME:
 *
 * Since: 1.18
 */
#define GST_VULKAN_IMAGE_MEMORY_ALLOCATOR_NAME "VulkanImage"
/**
 * GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE:
 *
 * Since: 1.18
 */
#define GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE "memory:VulkanImage"

/**
 * GstVulkanBarrierImageInfo:
 * @parent: parent #GstVulkanBarrierMemoryInfo
 * @image_layout: the image layout of this barrier
 * @subresource_range: what subresource the barrier applies to
 *
 * Since: 1.18
 */
struct _GstVulkanBarrierImageInfo
{
  GstVulkanBarrierMemoryInfo parent;

  VkImageLayout image_layout;
  /* FIXME: multiple layers or mipmap levels may require multiple barriers */
  VkImageSubresourceRange subresource_range;
};

/**
 * GstVulkanImageMemory:
 * @parent: parent #GstMemory
 * @device: the #GstVulkanDevice to allocate images from
 * @image: the Vulkan image handle
 * @vk_mem: the backing #GstVulkanMemory for @image
 * @create_info: creation information for @image
 * @requirements: memory requirements for @image
 * @format_properties: format properties
 * @usage: intended usage for @image
 * @barrier: last set barrier for @image
 *
 * Since: 1.18
 */
struct _GstVulkanImageMemory
{
  GstMemory parent;

  GstVulkanDevice * device;

  VkImage image;
  GstVulkanMemory *vk_mem;

  VkImageCreateInfo create_info;
  VkMemoryRequirements requirements;
  VkImageFormatProperties format_properties;
  VkImageUsageFlags usage;

  GstVulkanBarrierImageInfo barrier;

  /* <private> */
  GMutex lock;
  gboolean wrapped;
  GDestroyNotify notify;
  gpointer user_data;

  GPtrArray *views;
  GPtrArray *outstanding_views;

  gpointer _padding[GST_PADDING];
};

/**
 * GstVulkanImageMemoryFindViewFunc:
 *
 * Function definition used to find views.  Return %TRUE if @view matches the
 * criteria.
 *
 * Since: 1.18
 */
typedef gboolean (*GstVulkanImageMemoryFindViewFunc) (GstVulkanImageView * view, gpointer user_data);

/**
 * GstVulkanImageMemoryAllocator
 * @parent: the parent #GstAllocator
 *
 * Opaque #GstVulkanImageMemoryAllocator struct
 *
 * Since: 1.18
 */
struct _GstVulkanImageMemoryAllocator
{
  GstAllocator parent;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

/**
 * GstVulkanImageMemoryAllocatorClass:
 * @parent_class: the parent #GstAllocatorClass
 *
 * The #GstVulkanImageMemoryAllocatorClass only contains private data
 *
 * Since: 1.18
 */
struct _GstVulkanImageMemoryAllocatorClass
{
  GstAllocatorClass parent_class;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVulkanImageMemoryAllocator, gst_object_unref);

GST_VULKAN_API
void            gst_vulkan_image_memory_init_once       (void);
GST_VULKAN_API
gboolean        gst_is_vulkan_image_memory              (GstMemory * mem);

GST_VULKAN_API
gboolean        gst_vulkan_image_memory_init            (GstVulkanImageMemory * mem,
                                                         GstAllocator * allocator,
                                                         GstMemory * parent,
                                                         GstVulkanDevice * device,
                                                         VkFormat format,
                                                         VkImageUsageFlags usage,
                                                         VkImageLayout initial_layout,
                                                         GstAllocationParams * params,
                                                         gsize size,
                                                         gpointer user_data,
                                                         GDestroyNotify notify);

GST_VULKAN_API
GstMemory *     gst_vulkan_image_memory_alloc_with_image_info
                                                       (GstVulkanDevice * device,
                                                        VkImageCreateInfo * image_info,
                                                        VkMemoryPropertyFlags mem_prop_flags);

GST_VULKAN_API
GstMemory *     gst_vulkan_image_memory_alloc           (GstVulkanDevice * device,
                                                         VkFormat format,
                                                         gsize width,
                                                         gsize height,
                                                         VkImageTiling tiling,
                                                         VkImageUsageFlags usage,
                                                         VkMemoryPropertyFlags mem_prop_flags);

GST_VULKAN_API
GstMemory *     gst_vulkan_image_memory_wrapped         (GstVulkanDevice * device,
                                                         VkImage image,
                                                         VkFormat format,
                                                         gsize width,
                                                         gsize height,
                                                         VkImageTiling tiling,
                                                         VkImageUsageFlags usage,
                                                         gpointer user_data,
                                                         GDestroyNotify notify);

GST_VULKAN_API
guint32         gst_vulkan_image_memory_get_width       (GstVulkanImageMemory * image);
GST_VULKAN_API
guint32         gst_vulkan_image_memory_get_height      (GstVulkanImageMemory * image);

GST_VULKAN_API
GstVulkanImageView *gst_vulkan_image_memory_find_view   (GstVulkanImageMemory * image,
                                                         GstVulkanImageMemoryFindViewFunc find_func,
                                                         gpointer user_data);
GST_VULKAN_API
void            gst_vulkan_image_memory_add_view        (GstVulkanImageMemory * image,
                                                         GstVulkanImageView * view);


G_END_DECLS

#endif /* __GST_VULKAN_IMAGE_MEMORY_H__ */
