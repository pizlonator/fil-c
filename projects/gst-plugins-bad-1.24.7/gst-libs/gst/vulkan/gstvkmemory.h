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

#ifndef __GST_VULKAN_BASE_BUFFER_H__
#define __GST_VULKAN_BASE_BUFFER_H__

#include <gst/gst.h>
#include <gst/gstallocator.h>
#include <gst/gstmemory.h>

#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_MEMORY_ALLOCATOR (gst_vulkan_memory_allocator_get_type())
GST_VULKAN_API
GType gst_vulkan_memory_allocator_get_type(void);

#define GST_IS_VULKAN_MEMORY_ALLOCATOR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_VULKAN_MEMORY_ALLOCATOR))
#define GST_IS_VULKAN_MEMORY_ALLOCATOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_VULKAN_MEMORY_ALLOCATOR))
#define GST_VULKAN_MEMORY_ALLOCATOR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_VULKAN_MEMORY_ALLOCATOR, GstVulkanMemoryAllocatorClass))
#define GST_VULKAN_MEMORY_ALLOCATOR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_VULKAN_MEMORY_ALLOCATOR, GstVulkanMemoryAllocator))
#define GST_VULKAN_MEMORY_ALLOCATOR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_VULKAN_MEMORY_ALLOCATOR, GstVulkanMemoryAllocatorClass))
/**
 * GST_VULKAN_MEMORY_ALLOCATOR_CAST:
 *
 * Since: 1.18
 */
#define GST_VULKAN_MEMORY_ALLOCATOR_CAST(obj)            ((GstVulkanMemoryAllocator *)(obj))

/**
 * GST_VULKAN_MEMORY_ALLOCATOR_NAME:
 *
 * Since: 1.18
 */
#define GST_VULKAN_MEMORY_ALLOCATOR_NAME "Vulkan"

/**
 * GstVulkanMemory:
 * @mem: the parent #GstMemory
 * @device: the #GstVulkanDevice this memory is allocated from
 * @mem_ptr: the vulkan memory handle
 * @lock: lock for accessing/changing memory informat
 * @map_count: number of times this memory is mapped
 * @mapping: internal pointer to already mapped memory
 *
 * Since: 1.18
 */
struct _GstVulkanMemory
{
  GstMemory                 mem;

  GstVulkanDevice          *device;

  VkDeviceMemory            mem_ptr;

  /* <protected> */
  GMutex                    lock;
  guint                     map_count;
  /**
   * GstVulkanMemory.mapping:
   *
   * internal pointer to already mapped memory
   *
   * Since: 1.24
   */
  gpointer                  mapping;

  /* <private> */
  GDestroyNotify            notify;
  gpointer                  user_data;

  VkMemoryAllocateInfo      alloc_info;
  VkMemoryPropertyFlags     properties;

  /* we need our own offset because GstMemory's is used to offset into the
   * mapped pointer which when suballocating, we need to avoid.  This in
   * relation to the root memory */
  guint64                   vk_offset;
  gboolean                  wrapped;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

/**
 * GstVulkanMemoryAllocator
 * @parent: the parent #GstAllocator
 *
 * Opaque #GstVulkanMemoryAllocator struct
 *
 * Since: 1.18
 */
struct _GstVulkanMemoryAllocator
{
  GstAllocator parent;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

/**
 * GstVulkanMemoryAllocatorClass:
 * @parent_class: the parent #GstAllocatorClass
 *
 * The #GstVulkanMemoryAllocatorClass only contains private data
 *
 * Since: 1.18
 */
struct _GstVulkanMemoryAllocatorClass
{
  GstAllocatorClass parent_class;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVulkanMemoryAllocator, gst_object_unref);

GST_VULKAN_API
void            gst_vulkan_memory_init_once     (void);
GST_VULKAN_API
gboolean        gst_is_vulkan_memory            (GstMemory * mem);

GST_VULKAN_API
GstMemory *     gst_vulkan_memory_alloc         (GstVulkanDevice * device,
                                                 guint32 memory_type_index,
                                                 GstAllocationParams * params,
                                                 gsize size,
                                                 VkMemoryPropertyFlags mem_prop_flags);

GST_VULKAN_API
gboolean       gst_vulkan_memory_find_memory_type_index_with_requirements       (GstVulkanDevice * device,
                                                                                 VkMemoryRequirements * req,
                                                                                 VkMemoryPropertyFlags properties,
                                                                                 guint32 * type_index);


G_END_DECLS

#endif /* _GST_VULKAN_BASE_BUFFER_H_ */
