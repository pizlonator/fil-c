/*
 * GStreamer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "iosurfacevulkanmemory.h"
#include "metal-helpers.h"

GST_DEBUG_CATEGORY_STATIC (GST_CAT_IO_SURFACE_VULKAN_MEMORY);
#define GST_CAT_DEFAULT GST_CAT_IO_SURFACE_VULKAN_MEMORY

G_DEFINE_TYPE (GstIOSurfaceVulkanMemoryAllocator,
    gst_io_surface_vulkan_memory_allocator,
    GST_TYPE_VULKAN_IMAGE_MEMORY_ALLOCATOR);

typedef struct
{
  GstIOSurfaceVulkanMemory *memory;
  IOSurfaceRef surface;
} ContextThreadData;

static GstAllocator *_io_surface_vulkan_memory_allocator;

static void
_mem_free (GstAllocator * allocator, GstMemory * mem)
{
  gst_io_surface_vulkan_memory_set_surface ((GstIOSurfaceVulkanMemory *) mem,
      NULL);

  GST_ALLOCATOR_CLASS
      (gst_io_surface_vulkan_memory_allocator_parent_class)->free (allocator,
      mem);
}

static gpointer
_io_surface_vulkan_memory_allocator_map (GstMemory * bmem,
    GstMapInfo * info, gsize size)
{
  GstIOSurfaceVulkanMemory *mem = (GstIOSurfaceVulkanMemory *) bmem;

  GST_LOG ("mapping surface %p flags %d", mem->surface, info->flags);

  if (!(info->flags & GST_MAP_WRITE)) {
    IOSurfaceLock (mem->surface, kIOSurfaceLockReadOnly, NULL);
    return IOSurfaceGetBaseAddressOfPlane (mem->surface, mem->plane);
  } else {
    GST_ERROR ("couldn't map IOSurface %p flags %d", mem->surface, info->flags);
    return NULL;
  }
}

static void
_io_surface_vulkan_memory_allocator_unmap (GstMemory * bmem, GstMapInfo * info)
{
  GstIOSurfaceVulkanMemory *mem = (GstIOSurfaceVulkanMemory *) bmem;

  GST_LOG ("unmapping surface %p flags %d", mem->surface, info->flags);

  IOSurfaceUnlock (mem->surface, kIOSurfaceLockReadOnly, NULL);
}

static GstMemory *
_mem_alloc (GstAllocator * allocator, gsize size, GstAllocationParams * params)
{
  g_warning
      ("use gst_io_surface_vulkan_memory_wrapped () to allocate from this "
      "IOSurface allocator");

  return NULL;
}

static void
    gst_io_surface_vulkan_memory_allocator_class_init
    (GstIOSurfaceVulkanMemoryAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = _mem_alloc;
  allocator_class->free = _mem_free;
}

static void
gst_io_surface_vulkan_memory_allocator_init (GstIOSurfaceVulkanMemoryAllocator *
    allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_IO_SURFACE_VULKAN_MEMORY_ALLOCATOR_NAME;
  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);

  alloc->mem_map_full = _io_surface_vulkan_memory_allocator_map;
  alloc->mem_unmap_full = _io_surface_vulkan_memory_allocator_unmap;
}

void
gst_io_surface_vulkan_memory_init (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_IO_SURFACE_VULKAN_MEMORY,
        "iosurfacevulkan", 0, "IOSurface Vulkan Buffer");

    _io_surface_vulkan_memory_allocator =
        g_object_new (GST_TYPE_IO_SURFACE_VULKAN_MEMORY_ALLOCATOR, NULL);
    gst_object_ref_sink (_io_surface_vulkan_memory_allocator);

    gst_allocator_register (GST_IO_SURFACE_VULKAN_MEMORY_ALLOCATOR_NAME,
        gst_object_ref (_io_surface_vulkan_memory_allocator));
    g_once_init_leave (&_init, 1);
  }
}

gboolean
gst_is_io_surface_vulkan_memory (GstMemory * mem)
{
  return mem != NULL && mem->allocator != NULL &&
      g_type_is_a (G_OBJECT_TYPE (mem->allocator),
      GST_TYPE_IO_SURFACE_VULKAN_MEMORY_ALLOCATOR);
}

static GstIOSurfaceVulkanMemory *
_io_surface_vulkan_memory_new (GstVulkanDevice * device, IOSurfaceRef surface,
    unsigned int /* MTLPixelFormat */ fmt, GstVideoInfo * info, guint plane,
    gpointer user_data, GDestroyNotify notify)
{
  GstIOSurfaceVulkanMemory *mem;
  GstAllocationParams params = { 0, };
  VkImageCreateInfo image_info;
  VkPhysicalDevice gpu;
  VkImageUsageFlags usage;
  VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
  VkFormat vk_format;
  VkImage image;
  GError *error = NULL;
  VkResult err;

  mem = g_new0 (GstIOSurfaceVulkanMemory, 1);

  vk_format = metal_format_to_vulkan (fmt);
  /* FIXME: choose from outside */
  usage =
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
      VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

  /* *INDENT-OFF* */
  image_info = (VkImageCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = vk_format,
      /* MoltenVK double checks these against the IOSurface in vkUseIOSurface()
       * and will fail if they do not match */
      .extent = (VkExtent3D) { GST_VIDEO_INFO_COMP_WIDTH (info, plane), GST_VIDEO_INFO_COMP_HEIGHT (info, plane), 1 },
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

  gpu = gst_vulkan_device_get_physical_device (device);

  err = vkCreateImage (device->device, &image_info, NULL, &image);
  if (gst_vulkan_error_to_g_error (err, &error, "vkCreateImage") < 0)
    goto vk_error;

  vkGetImageMemoryRequirements (device->device, image,
      &mem->vulkan_mem.requirements);

  gst_vulkan_image_memory_init (&mem->vulkan_mem,
      _io_surface_vulkan_memory_allocator, NULL, device, vk_format, usage,
      VK_IMAGE_LAYOUT_UNDEFINED, &params, mem->vulkan_mem.requirements.size,
      user_data, notify);
  mem->vulkan_mem.create_info = image_info;
  mem->vulkan_mem.image = image;
  mem->vulkan_mem.barrier.image_layout = VK_IMAGE_LAYOUT_GENERAL;

  err =
      vkGetPhysicalDeviceImageFormatProperties (gpu, vk_format,
      VK_IMAGE_TYPE_2D, tiling, usage, 0, &mem->vulkan_mem.format_properties);
  if (gst_vulkan_error_to_g_error (err, &error,
          "vkGetPhysicalDeviceImageFormatProperties") < 0)
    goto vk_error;

  GST_MINI_OBJECT_FLAG_SET (mem, GST_MEMORY_FLAG_READONLY);

  mem->surface = NULL;
  mem->plane = plane;
  gst_io_surface_vulkan_memory_set_surface (mem, surface);

  return mem;

vk_error:
  {
    GST_CAT_ERROR (GST_CAT_IO_SURFACE_VULKAN_MEMORY,
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

GstIOSurfaceVulkanMemory *
gst_io_surface_vulkan_memory_wrapped (GstVulkanDevice * device,
    IOSurfaceRef surface, unsigned int /* MTLPixelFormat */ fmt,
    GstVideoInfo * info, guint plane, gpointer user_data, GDestroyNotify notify)
{
  return _io_surface_vulkan_memory_new (device, surface, fmt, info, plane,
      user_data, notify);
}
