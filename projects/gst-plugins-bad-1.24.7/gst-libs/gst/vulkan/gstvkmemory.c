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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstvkmemory.h"

#include "gstvkdebug-private.h"

/**
 * SECTION:vkmemory
 * @title: GstVulkanMemory
 * @short_description: memory subclass for Vulkan device memory
 * @see_also: #GstVulkanDevice, #GstMemory, #GstAllocator
 *
 * GstVulkanMemory is a #GstMemory subclass providing support for the mapping of
 * Vulkan device memory.
 */

/* WARNING: while suballocation is allowed, nothing prevents aliasing which
 * requires external synchronisation */

#define GST_CAT_DEFUALT GST_CAT_VULKAN_MEMORY
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFUALT);

static GstAllocator *_vulkan_memory_allocator;

static void
_vk_mem_init (GstVulkanMemory * mem, GstAllocator * allocator,
    GstMemory * parent, GstVulkanDevice * device, guint32 memory_type_index,
    GstAllocationParams * params, gsize size,
    VkMemoryPropertyFlags mem_prop_flags, gpointer user_data,
    GDestroyNotify notify)
{
  gsize align = gst_memory_alignment, offset = 0, maxsize = size;
  GstMemoryFlags flags = 0;
  gchar *props_str;

  if (params) {
    flags = params->flags;
    align |= params->align;
    offset = params->prefix;
    maxsize += params->prefix + params->padding;
    maxsize += align;
  }

  gst_memory_init (GST_MEMORY_CAST (mem), flags, allocator, parent, maxsize,
      align, offset, size);

  mem->device = gst_object_ref (device);
  mem->alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem->alloc_info.pNext = NULL;
  mem->alloc_info.allocationSize = (VkDeviceSize) mem->mem.maxsize;
  mem->alloc_info.memoryTypeIndex = memory_type_index;
  mem->properties = mem_prop_flags;
  mem->notify = notify;
  mem->user_data = user_data;
  mem->vk_offset = 0;
  mem->map_count = 0;
  mem->mapping = NULL;

  g_mutex_init (&mem->lock);

  props_str = gst_vulkan_memory_property_flags_to_string (mem_prop_flags);

  GST_CAT_DEBUG (GST_CAT_VULKAN_MEMORY, "new Vulkan memory:%p size:%"
      G_GSIZE_FORMAT " properties:%s", mem, maxsize, props_str);

  g_free (props_str);
}

static GstVulkanMemory *
_vk_mem_new (GstAllocator * allocator, GstMemory * parent,
    GstVulkanDevice * device, guint32 memory_type_index,
    GstAllocationParams * params, gsize size,
    VkMemoryPropertyFlags mem_props_flags, gpointer user_data,
    GDestroyNotify notify)
{
  GstVulkanMemory *mem = g_new0 (GstVulkanMemory, 1);
  GError *error = NULL;
  VkResult err;

  _vk_mem_init (mem, allocator, parent, device, memory_type_index, params,
      size, mem_props_flags, user_data, notify);

  err =
      vkAllocateMemory (device->device, &mem->alloc_info, NULL, &mem->mem_ptr);
  if (gst_vulkan_error_to_g_error (err, &error, "vkAllocMemory") < 0) {
    GST_CAT_ERROR (GST_CAT_VULKAN_MEMORY, "Failed to allocate device memory %s",
        error->message);
    gst_memory_unref ((GstMemory *) mem);
    g_clear_error (&error);
    return NULL;
  }

  return mem;
}

static gpointer
_vk_mem_map_full (GstVulkanMemory * mem, GstMapInfo * info, gsize size)
{
  gpointer data;
  VkResult err;
  GError *error = NULL;

  g_mutex_lock (&mem->lock);

  if (mem->map_count == 0) {
    if ((mem->properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0) {
      GST_CAT_ERROR (GST_CAT_VULKAN_MEMORY, "Cannot map host-invisible memory");
      g_mutex_unlock (&mem->lock);
      return NULL;
    }

    err = vkMapMemory (mem->device->device, mem->mem_ptr, mem->vk_offset,
        size, 0, &mem->mapping);
    if (gst_vulkan_error_to_g_error (err, &error, "vkMapMemory") < 0) {
      GST_CAT_ERROR (GST_CAT_VULKAN_MEMORY, "Failed to map device memory %s",
          error->message);
      g_clear_error (&error);
      g_mutex_unlock (&mem->lock);
      return NULL;
    }
  }

  mem->map_count++;
  data = mem->mapping;
  g_mutex_unlock (&mem->lock);

  return data;
}

static void
_vk_mem_unmap_full (GstVulkanMemory * mem, GstMapInfo * info)
{
  if ((info->flags & GST_MAP_WRITE)
      && !(mem->properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
    GError *error = NULL;
    VkResult err;
    VkMappedMemoryRange range = {
      .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
      /* .pNext = */
      .memory = mem->mem_ptr,
      .offset = mem->vk_offset,
      .size = VK_WHOLE_SIZE,
    };

    err = vkFlushMappedMemoryRanges (mem->device->device, 1u, &range);
    if (gst_vulkan_error_to_g_error (err, &error,
            "vkFlushMappedMemoryRanges") < 0) {
      GST_CAT_WARNING (GST_CAT_VULKAN_MEMORY, "Failed to flush memory: %s",
          error->message);
      g_clear_error (&error);
    }
  }

  g_mutex_lock (&mem->lock);

  g_assert (mem->map_count != 0);
  mem->map_count--;
  if (mem->map_count == 0) {
    vkUnmapMemory (mem->device->device, mem->mem_ptr);
    mem->mapping = NULL;
  }

  g_mutex_unlock (&mem->lock);
}

static GstMemory *
_vk_mem_copy (GstVulkanMemory * src, gssize offset, gssize size)
{
  return NULL;
}

static GstMemory *
_vk_mem_share (GstVulkanMemory * mem, gssize offset, gsize size)
{
  GstVulkanMemory *shared = g_new0 (GstVulkanMemory, 1);
  GstVulkanMemory *parent = mem;
  GstAllocationParams params = { 0, };

  if (size == -1)
    size = mem->mem.size - offset;

  g_return_val_if_fail (size > 0, NULL);

  while ((parent = (GstVulkanMemory *) (GST_MEMORY_CAST (parent)->parent)));

  params.flags = GST_MEMORY_FLAGS (mem);
  params.align = GST_MEMORY_CAST (parent)->align;

  _vk_mem_init (shared, _vulkan_memory_allocator, GST_MEMORY_CAST (mem),
      parent->device, parent->alloc_info.memoryTypeIndex, &params, size,
      parent->properties, NULL, NULL);
  shared->mem_ptr = parent->mem_ptr;
  shared->wrapped = TRUE;
  shared->vk_offset = offset + mem->vk_offset;

  return GST_MEMORY_CAST (shared);
}

static gboolean
_vk_mem_is_span (GstVulkanMemory * mem1, GstVulkanMemory * mem2, gsize * offset)
{
  return FALSE;
}

static GstMemory *
_vk_mem_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  g_critical ("Subclass should override GstAllocatorClass::alloc() function");

  return NULL;
}

static void
_vk_mem_free (GstAllocator * allocator, GstMemory * memory)
{
  GstVulkanMemory *mem = (GstVulkanMemory *) memory;

  GST_CAT_TRACE (GST_CAT_VULKAN_MEMORY, "freeing buffer memory:%p "
      "id:%" G_GUINT64_FORMAT, mem, (guint64) mem->mem_ptr);

  g_mutex_clear (&mem->lock);

  if (mem->notify)
    mem->notify (mem->user_data);

  if (mem->mem_ptr && !mem->wrapped)
    vkFreeMemory (mem->device->device, mem->mem_ptr, NULL);

  gst_object_unref (mem->device);

  g_free (mem);
}

/**
 * gst_vulkan_memory_find_memory_type_index_with_requirements:
 * @device: a #GstVulkanDevice
 * @req:  memory requirements to look for
 * @properties: memory properties to search for
 * @type_index: (out): resulting index of the memory type
 *
 * Returns: whether a valid memory type could be found
 *
 * Since: 1.24
 */
gboolean
gst_vulkan_memory_find_memory_type_index_with_requirements (GstVulkanDevice *
    device, VkMemoryRequirements * req, VkMemoryPropertyFlags properties,
    guint32 * type_index)
{
  guint32 i;
  VkPhysicalDeviceMemoryProperties *props;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), FALSE);

  props = &device->physical_device->memory_properties;

  /* Search memtypes to find first index with those properties */
  for (i = 0; i < props->memoryTypeCount; i++) {
    if (!(req->memoryTypeBits & (1 << i)))
      continue;
    if ((properties != G_MAXUINT32)
        && (props->memoryTypes[i].propertyFlags & properties) != properties)
      continue;
    if (req->size > props->memoryHeaps[props->memoryTypes[i].heapIndex].size)
      continue;

    *type_index = i;
    return TRUE;
  }

  return FALSE;
}

/**
 * gst_vulkan_memory_alloc:
 * @device:a #GstVulkanDevice
 * @memory_type_index: the Vulkan memory type index
 * @params: a #GstAllocationParams
 * @size: the size to allocate
 *
 * Allocated a new #GstVulkanMemory.
 *
 * Returns: a #GstMemory object backed by a vulkan device memory
 *
 * Since: 1.18
 */
GstMemory *
gst_vulkan_memory_alloc (GstVulkanDevice * device, guint32 memory_type_index,
    GstAllocationParams * params, gsize size, VkMemoryPropertyFlags mem_flags)
{
  GstVulkanMemory *mem;

  mem = _vk_mem_new (_vulkan_memory_allocator, NULL, device, memory_type_index,
      params, size, mem_flags, NULL, NULL);

  return (GstMemory *) mem;
}

G_DEFINE_TYPE (GstVulkanMemoryAllocator, gst_vulkan_memory_allocator,
    GST_TYPE_ALLOCATOR);

static void
gst_vulkan_memory_allocator_class_init (GstVulkanMemoryAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = _vk_mem_alloc;
  allocator_class->free = _vk_mem_free;
}

static void
gst_vulkan_memory_allocator_init (GstVulkanMemoryAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_VULKAN_MEMORY_ALLOCATOR_NAME;
  alloc->mem_map_full = (GstMemoryMapFullFunction) _vk_mem_map_full;
  alloc->mem_unmap_full = (GstMemoryUnmapFullFunction) _vk_mem_unmap_full;
  alloc->mem_copy = (GstMemoryCopyFunction) _vk_mem_copy;
  alloc->mem_share = (GstMemoryShareFunction) _vk_mem_share;
  alloc->mem_is_span = (GstMemoryIsSpanFunction) _vk_mem_is_span;
}

/**
 * gst_vulkan_memory_init_once:
 *
 * Initializes the Vulkan memory allocator. It is safe to call this function
 * multiple times.  This must be called before any other #GstVulkanMemory operation.
 *
 * Since: 1.18
 */
void
gst_vulkan_memory_init_once (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_VULKAN_MEMORY, "vulkanmemory", 0,
        "Vulkan Memory");

    _vulkan_memory_allocator =
        g_object_new (gst_vulkan_memory_allocator_get_type (), NULL);
    gst_object_ref_sink (_vulkan_memory_allocator);

    gst_allocator_register (GST_VULKAN_MEMORY_ALLOCATOR_NAME,
        gst_object_ref (_vulkan_memory_allocator));
    g_once_init_leave (&_init, 1);
  }
}

/**
 * gst_is_vulkan_memory:
 * @mem:a #GstMemory
 *
 * Returns: whether the memory at @mem is a #GstVulkanMemory
 *
 * Since: 1.18
 */
gboolean
gst_is_vulkan_memory (GstMemory * mem)
{
  return mem != NULL && mem->allocator != NULL &&
      g_type_is_a (G_OBJECT_TYPE (mem->allocator),
      GST_TYPE_VULKAN_MEMORY_ALLOCATOR);
}
