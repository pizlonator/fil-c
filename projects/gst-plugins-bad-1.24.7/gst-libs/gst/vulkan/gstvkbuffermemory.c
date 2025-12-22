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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvkbuffermemory.h"

/**
 * SECTION:vkbuffermemory
 * @title: GstVulkanBufferMemory
 * @short_description: memory subclass for Vulkan buffer memory
 * @see_also: #GstVulkanMemory, #GstMemory, #GstAllocator
 *
 * #GstVulkanBufferMemory is a #GstMemory subclass providing support for the
 * mapping of Vulkan device memory.
 */

#define GST_CAT_DEFUALT GST_CAT_VULKAN_BUFFER_MEMORY
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFUALT);

static GstAllocator *_vulkan_buffer_memory_allocator;

static gboolean
_create_info_from_args (VkBufferCreateInfo * info, gsize size,
    VkBufferUsageFlags usage)
{
  /* FIXME: validate these */
  /* *INDENT-OFF* */
  *info = (VkBufferCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = NULL,
  };
  /* *INDENT-ON* */

  return TRUE;
}

static void
_vk_buffer_mem_init (GstVulkanBufferMemory * mem, GstAllocator * allocator,
    GstMemory * parent, GstVulkanDevice * device, VkBufferUsageFlags usage,
    GstAllocationParams * params, gsize size, gpointer user_data,
    GDestroyNotify notify)
{
  gsize align = gst_memory_alignment, offset = 0, maxsize = size;
  GstMemoryFlags flags = 0;

  if (params) {
    flags = params->flags;
    align |= params->align;
    offset = params->prefix;
    maxsize += params->prefix + params->padding + align;
  }

  gst_memory_init (GST_MEMORY_CAST (mem), flags, allocator, parent, maxsize,
      align, offset, size);

  mem->device = gst_object_ref (device);
  mem->usage = usage;
  mem->wrapped = FALSE;
  mem->notify = notify;
  mem->user_data = user_data;

  mem->barrier.parent.type = GST_VULKAN_BARRIER_TYPE_BUFFER;
  mem->barrier.parent.pipeline_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  mem->barrier.parent.access_flags = 0;

  g_mutex_init (&mem->lock);

  GST_CAT_DEBUG (GST_CAT_VULKAN_BUFFER_MEMORY,
      "new Vulkan Buffer memory:%p size:%" G_GSIZE_FORMAT, mem, maxsize);
}

static GstVulkanBufferMemory *
_vk_buffer_mem_new_alloc_with_buffer_info (GstAllocator * allocator,
    GstMemory * parent, GstVulkanDevice * device,
    VkBufferCreateInfo * buffer_info, VkMemoryPropertyFlags mem_prop_flags,
    gpointer user_data, GDestroyNotify notify)
{
  GstVulkanBufferMemory *mem = NULL;
  GstAllocationParams params = { 0, };
  GError *error = NULL;
  guint32 type_idx;
  VkBuffer buffer;
  VkResult err;

  err = vkCreateBuffer (device->device, buffer_info, NULL, &buffer);
  if (gst_vulkan_error_to_g_error (err, &error, "vkCreateBuffer") < 0)
    goto vk_error;

  mem = g_new0 (GstVulkanBufferMemory, 1);
  vkGetBufferMemoryRequirements (device->device, buffer, &mem->requirements);

  if ((mem->requirements.alignment & (mem->requirements.alignment - 1)) != 0) {
    g_set_error_literal (&error, GST_VULKAN_ERROR, GST_VULKAN_FAILED,
        "Vulkan implementation requires unsupported non-power-of 2 memory alignment");
    goto vk_error;
  }

  params.align = mem->requirements.alignment - 1;
  _vk_buffer_mem_init (mem, allocator, parent, device, buffer_info->usage,
      &params, buffer_info->size, user_data, notify);
  mem->buffer = buffer;

  if (!gst_vulkan_memory_find_memory_type_index_with_requirements (device,
          &mem->requirements, mem_prop_flags, &type_idx))
    goto error;

  mem->vk_mem = (GstVulkanMemory *) gst_vulkan_memory_alloc (device, type_idx,
      &params, mem->requirements.size, mem_prop_flags);
  if (!mem->vk_mem)
    goto error;

  err = vkBindBufferMemory (device->device, buffer, mem->vk_mem->mem_ptr, 0);
  if (gst_vulkan_error_to_g_error (err, &error, "vkBindBufferMemory") < 0)
    goto vk_error;

  return mem;

vk_error:
  {
    GST_CAT_ERROR (GST_CAT_VULKAN_BUFFER_MEMORY,
        "Failed to allocate buffer memory %s", error->message);
    g_clear_error (&error);
    goto error;
  }

error:
  {
    if (mem)
      gst_memory_unref ((GstMemory *) mem);
    return NULL;
  }
}

static GstVulkanBufferMemory *
_vk_buffer_mem_new_alloc (GstAllocator * allocator, GstMemory * parent,
    GstVulkanDevice * device, gsize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags mem_prop_flags, gpointer user_data,
    GDestroyNotify notify)
{
  VkBufferCreateInfo buffer_info;

  if (!_create_info_from_args (&buffer_info, size, usage)) {
    GST_CAT_ERROR (GST_CAT_VULKAN_BUFFER_MEMORY, "Incorrect buffer parameters");
    return NULL;
  }

  return _vk_buffer_mem_new_alloc_with_buffer_info (allocator, parent, device,
      &buffer_info, mem_prop_flags, user_data, notify);
}

static GstVulkanBufferMemory *
_vk_buffer_mem_new_wrapped (GstAllocator * allocator, GstMemory * parent,
    GstVulkanDevice * device, VkBuffer buffer, VkBufferUsageFlags usage,
    gpointer user_data, GDestroyNotify notify)
{
  GstVulkanBufferMemory *mem = g_new0 (GstVulkanBufferMemory, 1);
  GstAllocationParams params = { 0, };

  mem->buffer = buffer;

  vkGetBufferMemoryRequirements (device->device, mem->buffer,
      &mem->requirements);

  params.align = mem->requirements.alignment - 1;
  params.flags = GST_MEMORY_FLAG_NOT_MAPPABLE;
  _vk_buffer_mem_init (mem, allocator, parent, device, usage, &params,
      mem->requirements.size, user_data, notify);
  mem->wrapped = TRUE;

  return mem;
}

static gpointer
_vk_buffer_mem_map_full (GstVulkanBufferMemory * mem, GstMapInfo * info,
    gsize size)
{
  GstMapInfo *vk_map_info;

  /* FIXME: possible barrier needed */
  g_mutex_lock (&mem->lock);

  if (!mem->vk_mem) {
    g_mutex_unlock (&mem->lock);
    return NULL;
  }

  vk_map_info = g_new0 (GstMapInfo, 1);
  info->user_data[0] = vk_map_info;
  if (!gst_memory_map ((GstMemory *) mem->vk_mem, vk_map_info, info->flags)) {
    g_free (vk_map_info);
    g_mutex_unlock (&mem->lock);
    return NULL;
  }
  g_mutex_unlock (&mem->lock);

  return vk_map_info->data;
}

static void
_vk_buffer_mem_unmap_full (GstVulkanBufferMemory * mem, GstMapInfo * info)
{
  g_mutex_lock (&mem->lock);
  gst_memory_unmap ((GstMemory *) mem->vk_mem, info->user_data[0]);
  g_mutex_unlock (&mem->lock);

  g_free (info->user_data[0]);
}

static GstMemory *
_vk_buffer_mem_copy (GstVulkanBufferMemory * src, gssize offset, gssize size)
{
  return NULL;
}

static GstMemory *
_vk_buffer_mem_share (GstVulkanBufferMemory * mem, gssize offset, gssize size)
{
  return NULL;
}

static gboolean
_vk_buffer_mem_is_span (GstVulkanBufferMemory * mem1,
    GstVulkanBufferMemory * mem2, gsize * offset)
{
  return FALSE;
}

static GstMemory *
_vk_buffer_mem_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  g_critical ("Subclass should override GstAllocatorClass::alloc() function");

  return NULL;
}

static void
_vk_buffer_mem_free (GstAllocator * allocator, GstMemory * memory)
{
  GstVulkanBufferMemory *mem = (GstVulkanBufferMemory *) memory;

  GST_CAT_TRACE (GST_CAT_VULKAN_BUFFER_MEMORY, "freeing buffer memory:%p "
      "id:%" G_GUINT64_FORMAT, mem, (guint64) mem->buffer);

  if (mem->buffer && !mem->wrapped)
    vkDestroyBuffer (mem->device->device, mem->buffer, NULL);

  gst_clear_object (&mem->barrier.parent.queue);

  if (mem->vk_mem)
    gst_memory_unref ((GstMemory *) mem->vk_mem);

  if (mem->notify)
    mem->notify (mem->user_data);

  gst_object_unref (mem->device);

  g_free (mem);
}

/**
 * gst_vulkan_buffer_memory_alloc_with_buffer_info:
 * @device: a #GstVulkanDevice
 * @buffer_info: the VkBufferCreateInfo structure
 * @mem_prop_flags: memory properties flags for the backing memory
 *
 * Allocate a new #GstVulkanBufferMemory.
 *
 * Returns: (transfer full): a #GstMemory object backed by a vulkan buffer
 *          backed by vulkan device memory
 *
 * Since: 1.24
 */
GstMemory *
gst_vulkan_buffer_memory_alloc_with_buffer_info (GstVulkanDevice * device,
    VkBufferCreateInfo * buffer_info, VkMemoryPropertyFlags mem_prop_flags)
{
  GstVulkanBufferMemory *mem;

  g_return_val_if_fail (buffer_info
      && buffer_info->sType == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL);

  mem = _vk_buffer_mem_new_alloc_with_buffer_info
      (_vulkan_buffer_memory_allocator, NULL, device, buffer_info,
      mem_prop_flags, NULL, NULL);

  return (GstMemory *) mem;
}

/**
 * gst_vulkan_buffer_memory_alloc:
 * @device: a #GstVulkanDevice
 * @size: size of the new buffer
 * @usage: buffer usage flags
 * @mem_prop_flags: memory properties flags for the backing memory
 *
 * Allocate a new #GstVulkanBufferMemory.
 *
 * Returns: (transfer full): a #GstMemory object backed by a vulkan buffer
 *          backed by vulkan device memory
 *
 * Since: 1.18
 */
GstMemory *
gst_vulkan_buffer_memory_alloc (GstVulkanDevice * device, gsize size,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_prop_flags)
{
  GstVulkanBufferMemory *mem;

  mem = _vk_buffer_mem_new_alloc (_vulkan_buffer_memory_allocator, NULL, device,
      size, usage, mem_prop_flags, NULL, NULL);

  return (GstMemory *) mem;
}

/**
 * gst_vulkan_buffer_memory_wrapped:
 * @device: a #GstVulkanDevice
 * @buffer: a `VkBuffer`
 * @usage: usage flags of @buffer
 * @user_data: (allow-none): user data to call @notify with
 * @notify: (allow-none): a #GDestroyNotify called when @buffer is no longer in use
 *
 * Allocated a new wrapped #GstVulkanBufferMemory with @buffer.
 *
 * Returns: (transfer full): a #GstMemory object backed by a vulkan device memory
 *
 * Since: 1.18
 */
GstMemory *
gst_vulkan_buffer_memory_wrapped (GstVulkanDevice * device, VkBuffer buffer,
    VkBufferUsageFlags usage, gpointer user_data, GDestroyNotify notify)
{
  GstVulkanBufferMemory *mem;

  mem =
      _vk_buffer_mem_new_wrapped (_vulkan_buffer_memory_allocator, NULL, device,
      buffer, usage, user_data, notify);

  return (GstMemory *) mem;
}

G_DEFINE_TYPE (GstVulkanBufferMemoryAllocator,
    gst_vulkan_buffer_memory_allocator, GST_TYPE_ALLOCATOR);

static void
    gst_vulkan_buffer_memory_allocator_class_init
    (GstVulkanBufferMemoryAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = _vk_buffer_mem_alloc;
  allocator_class->free = _vk_buffer_mem_free;
}

static void
gst_vulkan_buffer_memory_allocator_init (GstVulkanBufferMemoryAllocator *
    allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_VULKAN_BUFFER_MEMORY_ALLOCATOR_NAME;
  alloc->mem_map_full = (GstMemoryMapFullFunction) _vk_buffer_mem_map_full;
  alloc->mem_unmap_full =
      (GstMemoryUnmapFullFunction) _vk_buffer_mem_unmap_full;
  alloc->mem_copy = (GstMemoryCopyFunction) _vk_buffer_mem_copy;
  alloc->mem_share = (GstMemoryShareFunction) _vk_buffer_mem_share;
  alloc->mem_is_span = (GstMemoryIsSpanFunction) _vk_buffer_mem_is_span;
}

/**
 * gst_vulkan_buffer_memory_init_once:
 *
 * Initializes the Vulkan buffer memory allocator. It is safe to call this function
 * multiple times.  This must be called before any other #GstVulkanBufferMemory operation.
 *
 * Since: 1.18
 */
void
gst_vulkan_buffer_memory_init_once (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_VULKAN_BUFFER_MEMORY, "vulkanbuffermemory",
        0, "Vulkan Buffer Memory");

    _vulkan_buffer_memory_allocator =
        g_object_new (gst_vulkan_buffer_memory_allocator_get_type (), NULL);
    gst_object_ref_sink (_vulkan_buffer_memory_allocator);

    gst_allocator_register (GST_VULKAN_BUFFER_MEMORY_ALLOCATOR_NAME,
        gst_object_ref (_vulkan_buffer_memory_allocator));
    g_once_init_leave (&_init, 1);
  }
}

/**
 * gst_is_vulkan_buffer_memory:
 * @mem:a #GstMemory
 *
 * Returns: whether the memory at @mem is a #GstVulkanBufferMemory
 *
 * Since: 1.18
 */
gboolean
gst_is_vulkan_buffer_memory (GstMemory * mem)
{
  return mem != NULL && mem->allocator != NULL &&
      g_type_is_a (G_OBJECT_TYPE (mem->allocator),
      GST_TYPE_VULKAN_BUFFER_MEMORY_ALLOCATOR);
}
