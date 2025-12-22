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

#ifndef __GST_VULKAN_BUFFER_MEMORY_H__
#define __GST_VULKAN_BUFFER_MEMORY_H__

#include <gst/gst.h>
#include <gst/gstallocator.h>
#include <gst/gstmemory.h>

#include <gst/vulkan/gstvkbarrier.h>
#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_BUFFER_MEMORY_ALLOCATOR (gst_vulkan_buffer_memory_allocator_get_type())
GST_VULKAN_API
GType gst_vulkan_buffer_memory_allocator_get_type(void);

#define GST_IS_VULKAN_BUFFER_MEMORY_ALLOCATOR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_VULKAN_BUFFER_MEMORY_ALLOCATOR))
#define GST_IS_VULKAN_BUFFER_MEMORY_ALLOCATOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_VULKAN_BUFFER_MEMORY_ALLOCATOR))
#define GST_VULKAN_BUFFER_MEMORY_ALLOCATOR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_VULKAN_MEMORY_ALLOCATOR, GstVulkanBufferMemoryAllocatorClass))
#define GST_VULKAN_BUFFER_MEMORY_ALLOCATOR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_VULKAN_MEMORY_ALLOCATOR, GstVulkanBufferMemoryAllocator))
#define GST_VULKAN_BUFFER_MEMORY_ALLOCATOR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_VULKAN_MEMORY_ALLOCATOR, GstVulkanBufferMemoryAllocatorClass))
/**
 * GST_VULKAN_BUFFER_MEMORY_ALLOCATOR_CAST:
 *
 * Since: 1.18
 */
#define GST_VULKAN_BUFFER_MEMORY_ALLOCATOR_CAST(obj)            ((GstVulkanBufferMemoryAllocator *)(obj))

/**
 * GST_VULKAN_BUFFER_MEMORY_ALLOCATOR_NAME:
 *
 * Since: 1.18
 */
#define GST_VULKAN_BUFFER_MEMORY_ALLOCATOR_NAME "VulkanBuffer"
/**
 * GST_CAPS_FEATURE_MEMORY_VULKAN_BUFFER:
 *
 * Since: 1.18
 */
#define GST_CAPS_FEATURE_MEMORY_VULKAN_BUFFER "memory:VulkanBuffer"

/**
 * GstVulkanBarrierBufferInfo:
 * @parent: parent #GstVulkanBarrierMemoryInfo
 * @offset: offset into the vulkan buffer to execute the barrier with
 * @size: size of memory to execute barrier over
 *
 * Since: 1.18
 */
struct _GstVulkanBarrierBufferInfo
{
  GstVulkanBarrierMemoryInfo parent;

  VkDeviceSize offset;
  VkDeviceSize size;
};
/**
 * GstVulkanBufferMemory:
 * @parent: parent #GstMemory
 * @device: the #GstVulkanDevice this vulkan buffer is allocated from
 * @buffer: Vulkan buffer object
 * @vk_mem: backing #GstVulkanMemory for @buffer
 * @requirements: allocation requirements for @buffer
 * @usage: intended usage for @buffer
 * @barrier: the last set barrier information
 *
 * Since: 1.18
 */
struct _GstVulkanBufferMemory
{
  GstMemory parent;

  GstVulkanDevice * device;

  VkBuffer buffer;
  GstVulkanMemory *vk_mem;

  VkMemoryRequirements requirements;
  VkBufferUsageFlags usage;

  GstVulkanBarrierBufferInfo barrier;

  /* <private> */
  GMutex lock;
  gboolean wrapped;
  GDestroyNotify notify;
  gpointer user_data;
};

/**
 * GstVulkanBufferMemoryAllocator
 * @parent: the parent #GstAllocator
 *
 * Opaque #GstVulkanBufferMemoryAllocator struct
 *
 * Since: 1.18
 */
struct _GstVulkanBufferMemoryAllocator
{
  GstAllocator parent;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

/**
 * GstVulkanBufferMemoryAllocatorClass:
 * @parent_class: the parent #GstAllocatorClass
 *
 * The #GstVulkanBufferMemoryAllocatorClass only contains private data
 *
 * Since: 1.18
 */
struct _GstVulkanBufferMemoryAllocatorClass
{
  GstAllocatorClass parent_class;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVulkanBufferMemoryAllocator, gst_object_unref);

GST_VULKAN_API
void            gst_vulkan_buffer_memory_init_once       (void);
GST_VULKAN_API
gboolean        gst_is_vulkan_buffer_memory              (GstMemory * mem);

GST_VULKAN_API
GstMemory *     gst_vulkan_buffer_memory_alloc           (GstVulkanDevice * device,
                                                          gsize size,
                                                          VkBufferUsageFlags usage,
                                                          VkMemoryPropertyFlags mem_prop_flags);

GST_VULKAN_API
GstMemory *    gst_vulkan_buffer_memory_alloc_with_buffer_info
                                                         (GstVulkanDevice * device,
                                                          VkBufferCreateInfo * buffer_info,
                                                          VkMemoryPropertyFlags mem_prop_flags);

GST_VULKAN_API
GstMemory *     gst_vulkan_buffer_memory_wrapped         (GstVulkanDevice * device,
                                                          VkBuffer buffer,
                                                          VkBufferUsageFlags usage,
                                                          gpointer user_data,
                                                          GDestroyNotify notify);

G_END_DECLS

#endif /* __GST_VULKAN_BUFFER_MEMORY_H__ */
