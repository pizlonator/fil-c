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

#include "gstvkimagememory.h"

/**
 * SECTION:vkimagememory
 * @title: GstVulkanImageMemory
 * @short_description: memory subclass for Vulkan image memory
 * @see_also: #GstVulkanDevice, #GstMemory, #GstAllocator
 *
 * GstVulkanImageMemory is a #GstMemory subclass providing support for the
 * mapping of Vulkan device memory.
 */

#define GST_CAT_DEFUALT GST_CAT_VULKAN_IMAGE_MEMORY
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFUALT);

static GstAllocator *_vulkan_image_memory_allocator;

static gboolean
_create_info_from_args (VkImageCreateInfo * info, VkFormat format, gsize width,
    gsize height, VkImageTiling tiling, VkImageUsageFlags usage)
{
  /* FIXME: validate these */

  /* *INDENT-OFF* */
  *info = (VkImageCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = (VkExtent3D) { width, height, 1 },
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = tiling,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = NULL,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  /* *INDENT-ON* */

  return TRUE;
}

gboolean
gst_vulkan_image_memory_init (GstVulkanImageMemory * mem,
    GstAllocator * allocator, GstMemory * parent, GstVulkanDevice * device,
    VkFormat format, VkImageUsageFlags usage, VkImageLayout layout,
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
  mem->barrier.parent.type = GST_VULKAN_BARRIER_TYPE_IMAGE;
  mem->barrier.parent.pipeline_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  mem->barrier.parent.access_flags = 0;
  mem->barrier.parent.semaphore = VK_NULL_HANDLE;
  mem->barrier.parent.semaphore_value = 0;
  mem->barrier.image_layout = layout;
  /* *INDENT-OFF* */
  mem->barrier.subresource_range = (VkImageSubresourceRange) {
          .aspectMask = gst_vulkan_format_get_aspect (format),
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
  };
  /* *INDENT-ON* */
  mem->usage = usage;
  mem->wrapped = FALSE;
  mem->notify = notify;
  mem->user_data = user_data;

  g_mutex_init (&mem->lock);

  mem->views = g_ptr_array_new ();
  mem->outstanding_views = g_ptr_array_new ();

  GST_CAT_DEBUG (GST_CAT_VULKAN_IMAGE_MEMORY,
      "new Vulkan Image memory:%p size:%" G_GSIZE_FORMAT, mem, maxsize);

  return TRUE;
}

static GstVulkanImageMemory *
_vk_image_mem_new_alloc_with_image_info (GstAllocator * allocator,
    GstMemory * parent, GstVulkanDevice * device,
    VkImageCreateInfo * image_info, VkMemoryPropertyFlags mem_prop_flags,
    gpointer user_data, GDestroyNotify notify)
{
  GstVulkanImageMemory *mem = NULL;
  GstAllocationParams params = { 0, };
  VkPhysicalDevice gpu;
  GError *error = NULL;
  GArray *qfi = NULL;
  guint32 type_idx;
  VkImage image;
  VkResult err;

  gpu = gst_vulkan_device_get_physical_device (device);

  if (!image_info->pQueueFamilyIndices) {
    /* XXX: overwrite the queue indices part of the structure */
    qfi = gst_vulkan_device_queue_family_indices (device);
    image_info->pQueueFamilyIndices = (uint32_t *) qfi->data;
    image_info->queueFamilyIndexCount = qfi->len;
    image_info->sharingMode = qfi->len > 1 ? VK_SHARING_MODE_CONCURRENT :
        VK_SHARING_MODE_EXCLUSIVE;
  }

  err = vkCreateImage (device->device, image_info, NULL, &image);

  if (qfi)
    g_array_unref (qfi);

  if (gst_vulkan_error_to_g_error (err, &error, "vkCreateImage") < 0)
    goto vk_error;

  mem = g_new0 (GstVulkanImageMemory, 1);

  vkGetImageMemoryRequirements (device->device, image, &mem->requirements);

  gst_vulkan_image_memory_init (mem, allocator, parent, device,
      image_info->format, image_info->usage, image_info->initialLayout, &params,
      mem->requirements.size, user_data, notify);
  mem->create_info = *image_info;
  /* XXX: to avoid handling pNext lifetime  */
  mem->create_info.pNext = NULL;
  mem->image = image;

#if defined(VK_KHR_timeline_semaphore)
  if (gst_vulkan_device_is_extension_enabled (device,
          VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)) {
    VkSemaphoreTypeCreateInfo semaphore_type_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = 0,
    };
    VkSemaphoreCreateInfo semaphore_create_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &semaphore_type_info,
    };

    err = vkCreateSemaphore (device->device, &semaphore_create_info, NULL,
        &mem->barrier.parent.semaphore);
    if (gst_vulkan_error_to_g_error (err, &error, "vkCreateSemaphore") < 0)
      goto vk_error;
  }
#endif


  err = vkGetPhysicalDeviceImageFormatProperties (gpu, image_info->format,
      VK_IMAGE_TYPE_2D, image_info->tiling, image_info->usage, 0,
      &mem->format_properties);
  if (gst_vulkan_error_to_g_error (err, &error,
          "vkGetPhysicalDeviceImageFormatProperties") < 0)
    goto vk_error;

  if (!gst_vulkan_memory_find_memory_type_index_with_requirements (device,
          &mem->requirements, mem_prop_flags, &type_idx))
    goto error;

  if ((mem->requirements.alignment & (mem->requirements.alignment - 1)) != 0) {
    g_set_error_literal (&error, GST_VULKAN_ERROR, GST_VULKAN_FAILED,
        "Vulkan implementation requires unsupported non-power-of 2 memory alignment");
    goto error;
  }
  params.align = mem->requirements.alignment - 1;
  mem->vk_mem = (GstVulkanMemory *) gst_vulkan_memory_alloc (device, type_idx,
      &params, mem->requirements.size, mem_prop_flags);
  if (!mem->vk_mem)
    goto error;

  err = vkBindImageMemory (device->device, image, mem->vk_mem->mem_ptr, 0);
  if (gst_vulkan_error_to_g_error (err, &error, "vkBindImageMemory") < 0)
    goto vk_error;

  return mem;

vk_error:
  {
    GST_CAT_ERROR (GST_CAT_VULKAN_IMAGE_MEMORY,
        "Failed to allocate image memory %s", error->message);
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

static GstVulkanImageMemory *
_vk_image_mem_new_alloc (GstAllocator * allocator, GstMemory * parent,
    GstVulkanDevice * device, VkFormat format, gsize width, gsize height,
    VkImageTiling tiling, VkImageUsageFlags usage,
    VkMemoryPropertyFlags mem_prop_flags, gpointer user_data,
    GDestroyNotify notify)
{
  VkImageCreateInfo image_info;

  if (!_create_info_from_args (&image_info, format, width, height, tiling,
          usage)) {
    GST_CAT_ERROR (GST_CAT_VULKAN_IMAGE_MEMORY, "Incorrect image parameters");
    return NULL;
  }

  return _vk_image_mem_new_alloc_with_image_info (allocator, parent, device,
      &image_info, mem_prop_flags, user_data, notify);
}

static GstVulkanImageMemory *
_vk_image_mem_new_wrapped (GstAllocator * allocator, GstMemory * parent,
    GstVulkanDevice * device, VkImage image, VkFormat format, gsize width,
    gsize height, VkImageTiling tiling, VkImageUsageFlags usage,
    gpointer user_data, GDestroyNotify notify)
{
  GstVulkanImageMemory *mem = g_new0 (GstVulkanImageMemory, 1);
  GstAllocationParams params = { 0, };
  VkPhysicalDevice gpu;
  GError *error = NULL;
  VkResult err;

  gpu = gst_vulkan_device_get_physical_device (device);
  mem->image = image;

  vkGetImageMemoryRequirements (device->device, mem->image, &mem->requirements);

  /* XXX: assumes alignment is a power of 2 */
  params.align = mem->requirements.alignment - 1;
  params.flags = GST_MEMORY_FLAG_NOT_MAPPABLE;
  gst_vulkan_image_memory_init (mem, allocator, parent, device, format, usage,
      VK_IMAGE_LAYOUT_UNDEFINED, &params, mem->requirements.size, user_data,
      notify);
  mem->wrapped = TRUE;

  if (!_create_info_from_args (&mem->create_info, format, width, height, tiling,
          usage)) {
    GST_CAT_ERROR (GST_CAT_VULKAN_IMAGE_MEMORY, "Incorrect image parameters");
    goto error;
  }

  err = vkGetPhysicalDeviceImageFormatProperties (gpu, format, VK_IMAGE_TYPE_2D,
      tiling, usage, 0, &mem->format_properties);
  if (gst_vulkan_error_to_g_error (err, &error,
          "vkGetPhysicalDeviceImageFormatProperties") < 0)
    goto vk_error;

  return mem;

vk_error:
  {
    GST_CAT_ERROR (GST_CAT_VULKAN_IMAGE_MEMORY,
        "Failed to allocate image memory %s", error->message);
    g_clear_error (&error);
    goto error;
  }

error:
  {
    gst_memory_unref ((GstMemory *) mem);
    return NULL;
  }
}

static gpointer
_vk_image_mem_map_full (GstVulkanImageMemory * mem, GstMapInfo * info,
    gsize size)
{
  GstMapInfo *vk_map_info;

  /* FIXME: possible layout transformation needed */
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
_vk_image_mem_unmap_full (GstVulkanImageMemory * mem, GstMapInfo * info)
{
  g_mutex_lock (&mem->lock);
  gst_memory_unmap ((GstMemory *) mem->vk_mem, info->user_data[0]);
  g_mutex_unlock (&mem->lock);

  g_free (info->user_data[0]);
}

static GstMemory *
_vk_image_mem_copy (GstVulkanImageMemory * src, gssize offset, gssize size)
{
  return NULL;
}

static GstMemory *
_vk_image_mem_share (GstVulkanImageMemory * mem, gssize offset, gssize size)
{
  return NULL;
}

static gboolean
_vk_image_mem_is_span (GstVulkanImageMemory * mem1, GstVulkanImageMemory * mem2,
    gsize * offset)
{
  return FALSE;
}

static GstMemory *
_vk_image_mem_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  g_critical ("Subclass should override GstAllocatorClass::alloc() function");

  return NULL;
}

static void
_free_view (GstVulkanImageView * view, gpointer unused)
{
  gst_vulkan_image_view_unref (view);
}

static void
_vk_image_mem_free (GstAllocator * allocator, GstMemory * memory)
{
  GstVulkanImageMemory *mem = (GstVulkanImageMemory *) memory;

  GST_CAT_TRACE (GST_CAT_VULKAN_IMAGE_MEMORY, "freeing image memory:%p "
      "id:%" G_GUINT64_FORMAT, mem, (guint64) mem->image);

  g_warn_if_fail (mem->outstanding_views->len == 0);
  g_ptr_array_unref (mem->outstanding_views);

  g_ptr_array_foreach (mem->views, (GFunc) _free_view, NULL);
  g_ptr_array_unref (mem->views);

  if (mem->image && !mem->wrapped)
    vkDestroyImage (mem->device->device, mem->image, NULL);

  gst_clear_object (&mem->barrier.parent.queue);

  if (mem->vk_mem)
    gst_memory_unref ((GstMemory *) mem->vk_mem);

  if (mem->barrier.parent.semaphore) {
    vkDestroySemaphore (mem->device->device, mem->barrier.parent.semaphore,
        NULL);
  }

  if (mem->notify)
    mem->notify (mem->user_data);

  gst_object_unref (mem->device);

  g_free (mem);
}

/**
 * gst_vulkan_image_memory_alloc_with_image_info:
 * @device: a #GstVulkanDevice
 * @image_info: VkImageCreateInfo structure
 * @mem_prop_flags: VkMemoryPropertyFlags flags
 *
 * Returns: a #GstMemory object backed by a vulkan device memory
 *
 * Since: 1.24
 */
GstMemory *
gst_vulkan_image_memory_alloc_with_image_info (GstVulkanDevice * device,
    VkImageCreateInfo * image_info, VkMemoryPropertyFlags mem_prop_flags)
{
  GstVulkanImageMemory *mem;

  g_return_val_if_fail (image_info
      && image_info->sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL);

  mem = _vk_image_mem_new_alloc_with_image_info (_vulkan_image_memory_allocator,
      NULL, device, image_info, mem_prop_flags, NULL, NULL);

  return (GstMemory *) mem;
}

/**
 * gst_vulkan_image_memory_alloc:
 * @device: a #GstVulkanDevice
 * @format: the VkFormat for the new image
 * @width: width for the new image
 * @height: height for the new image
 * @tiling: tiling for the new image
 * @usage: usage flags for the new image
 * @mem_prop_flags: VkDeviceMemory property flags for the new image
 *
 * Allocated a new #GstVulkanImageMemory.
 *
 * Returns: a #GstMemory object backed by a vulkan device memory
 *
 * Since: 1.18
 */
GstMemory *
gst_vulkan_image_memory_alloc (GstVulkanDevice * device, VkFormat format,
    gsize width, gsize height, VkImageTiling tiling, VkImageUsageFlags usage,
    VkMemoryPropertyFlags mem_prop_flags)
{
  GstVulkanImageMemory *mem;

  mem = _vk_image_mem_new_alloc (_vulkan_image_memory_allocator, NULL, device,
      format, width, height, tiling, usage, mem_prop_flags, NULL, NULL);

  return (GstMemory *) mem;
}

/**
 * gst_vulkan_image_memory_wrapped:
 * @device: a #GstVulkanDevice
 * @image: a VkImage
 * @format: the VkFormat for @image
 * @width: width of @image
 * @height: height of @image
 * @tiling: tiling of @image
 * @usage: usage flags of @image
 * @user_data: (nullable): user data for @notify
 * @notify: a #GDestroyNotify when @image is no longer needed
 *
 * Return: a new #GstVulkanImageMemory wrapping @image
 *
 * Since: 1.18
 */
GstMemory *
gst_vulkan_image_memory_wrapped (GstVulkanDevice * device, VkImage image,
    VkFormat format, gsize width, gsize height, VkImageTiling tiling,
    VkImageUsageFlags usage, gpointer user_data, GDestroyNotify notify)
{
  GstVulkanImageMemory *mem;

  mem = _vk_image_mem_new_wrapped (_vulkan_image_memory_allocator, NULL, device,
      image, format, width, height, tiling, usage, user_data, notify);

  return (GstMemory *) mem;
}

/**
 * gst_vulkan_image_memory_get_width:
 * @image: a #GstVulkanImageMemory
 *
 * Return: the width of @image
 *
 * Since: 1.18
 */
guint32
gst_vulkan_image_memory_get_width (GstVulkanImageMemory * image)
{
  g_return_val_if_fail (gst_is_vulkan_image_memory (GST_MEMORY_CAST (image)),
      0);

  return image->create_info.extent.width;
}

/**
 * gst_vulkan_image_memory_get_height:
 * @image: a #GstVulkanImageMemory
 *
 * Return: the height of @image
 *
 * Since: 1.18
 */
guint32
gst_vulkan_image_memory_get_height (GstVulkanImageMemory * image)
{
  g_return_val_if_fail (gst_is_vulkan_image_memory (GST_MEMORY_CAST (image)),
      0);

  return image->create_info.extent.height;
}

static gint
find_view_index_unlocked (GstVulkanImageMemory * image,
    GstVulkanImageView * view)
{
  guint index;

  if (!g_ptr_array_find (image->views, view, &index))
    return -1;

  return (gint) index;
}

extern void
gst_vulkan_image_memory_release_view (GstVulkanImageMemory * image,
    GstVulkanImageView * view);

void
gst_vulkan_image_memory_release_view (GstVulkanImageMemory * image,
    GstVulkanImageView * view)
{
  guint index;

  g_return_if_fail (gst_is_vulkan_image_memory (GST_MEMORY_CAST (image)));
  g_return_if_fail (image == view->image);

  g_mutex_lock (&image->lock);
  GST_CAT_TRACE (GST_CAT_VULKAN_IMAGE_MEMORY, "image %p removing view %p",
      image, view);
  if (g_ptr_array_find (image->outstanding_views, view, &index)) {
    g_ptr_array_steal_index_fast (image->outstanding_views, index);
    g_ptr_array_add (image->views, view);
  } else {
    g_warning ("GstVulkanImageMemory:%p attempt to remove a view %p "
        "that we do not own", image, view);
  }
  view->image = NULL;
  g_mutex_unlock (&image->lock);
  gst_memory_unref ((GstMemory *) image);
}

/**
 * gst_vulkan_image_memory_add_view:
 * @image: a #GstVulkanImageMemory
 * @view: a #GstVulkanImageView
 *
 * Since: 1.18
 */
void
gst_vulkan_image_memory_add_view (GstVulkanImageMemory * image,
    GstVulkanImageView * view)
{
  g_return_if_fail (gst_is_vulkan_image_memory (GST_MEMORY_CAST (image)));
  g_return_if_fail (view != NULL);
  g_return_if_fail (image == view->image);

  g_mutex_lock (&image->lock);
  if (find_view_index_unlocked (image, view) != -1) {
    g_warn_if_reached ();
    g_mutex_unlock (&image->lock);
    return;
  }
  g_ptr_array_add (image->outstanding_views, view);
  GST_CAT_TRACE (GST_CAT_VULKAN_IMAGE_MEMORY, "Image %p adding view %p",
      image, view);

  g_mutex_unlock (&image->lock);
}

struct view_data
{
  GstVulkanImageMemory *img;
  GstVulkanImageMemoryFindViewFunc find_func;
  gpointer find_data;
};

static gboolean
find_view_func (GstVulkanImageView * view, gpointer user_data)
{
  struct view_data *data = user_data;
  GstVulkanImageMemory *previous;
  gboolean ret;

  previous = view->image;
  view->image = data->img;

  ret = data->find_func (view, data->find_data);

  view->image = previous;

  return ret;
}

/**
 * gst_vulkan_image_memory_find_view:
 * @image: a #GstVulkanImageMemory
 * @find_func: (scope call): #GstVulkanImageMemoryFindViewFunc to search with
 * @user_data: user data to call @finc_func with
 *
 * Return: (transfer full) (nullable): the first #GstVulkanImageView that
 *                                     @find_func returns %TRUE for, or %NULL
 *
 * Since: 1.18
 */
GstVulkanImageView *
gst_vulkan_image_memory_find_view (GstVulkanImageMemory * image,
    GstVulkanImageMemoryFindViewFunc find_func, gpointer user_data)
{
  GstVulkanImageView *ret = NULL;
  struct view_data view;
  guint index;

  g_return_val_if_fail (gst_is_vulkan_image_memory (GST_MEMORY_CAST (image)),
      NULL);
  g_return_val_if_fail (find_func != NULL, NULL);

  g_mutex_lock (&image->lock);
  view.img = image;
  view.find_func = find_func;
  view.find_data = user_data;

  if (g_ptr_array_find_with_equal_func (image->outstanding_views, &view,
          (GEqualFunc) find_view_func, &index)) {
    ret =
        gst_vulkan_image_view_ref (g_ptr_array_index (image->outstanding_views,
            index));
  } else if (g_ptr_array_find_with_equal_func (image->views, &view,
          (GEqualFunc) find_view_func, &index)) {
    ret = g_ptr_array_steal_index_fast (image->views, index);
    g_ptr_array_add (image->outstanding_views, ret);
    ret->image = (GstVulkanImageMemory *) gst_memory_ref ((GstMemory *) image);
  }

  GST_CAT_TRACE (GST_CAT_VULKAN_IMAGE_MEMORY, "Image %p found view %p",
      image, ret);
  g_mutex_unlock (&image->lock);

  return ret;
}

G_DEFINE_TYPE (GstVulkanImageMemoryAllocator, gst_vulkan_image_memory_allocator,
    GST_TYPE_ALLOCATOR);

static void
gst_vulkan_image_memory_allocator_class_init (GstVulkanImageMemoryAllocatorClass
    * klass)
{
  GstAllocatorClass *allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = _vk_image_mem_alloc;
  allocator_class->free = _vk_image_mem_free;
}

static void
gst_vulkan_image_memory_allocator_init (GstVulkanImageMemoryAllocator *
    allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_VULKAN_IMAGE_MEMORY_ALLOCATOR_NAME;
  alloc->mem_map_full = (GstMemoryMapFullFunction) _vk_image_mem_map_full;
  alloc->mem_unmap_full = (GstMemoryUnmapFullFunction) _vk_image_mem_unmap_full;
  alloc->mem_copy = (GstMemoryCopyFunction) _vk_image_mem_copy;
  alloc->mem_share = (GstMemoryShareFunction) _vk_image_mem_share;
  alloc->mem_is_span = (GstMemoryIsSpanFunction) _vk_image_mem_is_span;
}

/**
 * gst_vulkan_image_memory_init_once:
 *
 * Initializes the Vulkan image memory allocator. It is safe to call this function
 * multiple times.  This must be called before any other #GstVulkanImageMemory operation.
 *
 * Since: 1.18
 */
void
gst_vulkan_image_memory_init_once (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_VULKAN_IMAGE_MEMORY, "vulkanimagememory",
        0, "Vulkan Image Memory");

    _vulkan_image_memory_allocator =
        g_object_new (gst_vulkan_image_memory_allocator_get_type (), NULL);
    gst_object_ref_sink (_vulkan_image_memory_allocator);

    gst_allocator_register (GST_VULKAN_IMAGE_MEMORY_ALLOCATOR_NAME,
        gst_object_ref (_vulkan_image_memory_allocator));
    g_once_init_leave (&_init, 1);
  }
}

/**
 * gst_is_vulkan_image_memory:
 * @mem: a #GstMemory
 *
 * Returns: whether the memory at @mem is a #GstVulkanImageMemory
 *
 * Since: 1.18
 */
gboolean
gst_is_vulkan_image_memory (GstMemory * mem)
{
  return mem != NULL && mem->allocator != NULL &&
      g_type_is_a (G_OBJECT_TYPE (mem->allocator),
      GST_TYPE_VULKAN_IMAGE_MEMORY_ALLOCATOR);
}
