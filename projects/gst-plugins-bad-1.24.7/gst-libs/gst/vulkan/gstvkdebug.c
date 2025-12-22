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

#include <glib/gprintf.h>

#include "gstvkerror.h"
#include "gstvkdebug.h"
#include "gstvkdebug-private.h"

/**
 * SECTION:vkdebug
 * @title: GstVulkanDebug
 * @short_description: Vulkan debugging utilities
 * @see_also: #GstVulkanDevice
 */

#define FLAGS_TO_STRING(under_name, VkType)                                     \
gchar * G_PASTE(G_PASTE(gst_vulkan_,under_name),_flags_to_string) (VkType flag_bits) \
{                                                                               \
  GString *s = g_string_new (NULL);                                             \
  gboolean first = TRUE;                                                        \
  int i;                                                                        \
  for (i = 0; i < G_N_ELEMENTS (G_PASTE(G_PASTE(vk_,under_name),_flags_map)); i++) { \
    if (flag_bits & G_PASTE(G_PASTE(vk_,under_name),_flags_map)[i].flag_bit) {  \
      if (!first) {                                                             \
        g_string_append (s, "|");                                               \
      }                                                                         \
      g_string_append (s, G_PASTE(G_PASTE(vk_,under_name),_flags_map)[i].str); \
      first = FALSE;                                                            \
    }                                                                           \
  }                                                                             \
  return g_string_free (s, FALSE);                                              \
}

/* *INDENT-OFF* */
static const struct 
{
  VkMemoryPropertyFlagBits flag_bit;
  const char *str;
} vk_memory_property_flags_map[] = {
  {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "device-local"},
  {VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, "host-visible"},
  {VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "host-coherent"},
  {VK_MEMORY_PROPERTY_HOST_CACHED_BIT, "host-cached"},
  {VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, "lazily-allocated"},
#if VK_HEADER_VERSION >= 70
  {VK_MEMORY_PROPERTY_PROTECTED_BIT, "protected"},
#endif
#if VK_HEADER_VERSION >= 121
  {VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD, "device-coherent"},
  {VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD, "device-uncached"},
#endif
};
/**
 * gst_vulkan_memory_property_flags_to_string:
 *
 * Since: 1.18
 */
FLAGS_TO_STRING(memory_property, VkMemoryPropertyFlags);

static const struct 
{
  VkMemoryHeapFlagBits flag_bit;
  const char *str;
} vk_memory_heap_flags_map[] = {
  {VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, "device-local"},
#if VK_HEADER_VERSION >= 70
  {VK_MEMORY_HEAP_MULTI_INSTANCE_BIT, "multi-instance"},
#endif
};
/**
 * gst_vulkan_memory_heap_flags_to_string:
 *
 * Since: 1.18
 */
FLAGS_TO_STRING(memory_heap, VkMemoryHeapFlags);

static const struct 
{
  VkQueueFlagBits flag_bit;
  const char *str;
} vk_queue_flags_map[] = {
  {VK_QUEUE_GRAPHICS_BIT, "graphics"},
  {VK_QUEUE_COMPUTE_BIT, "compute"},
  {VK_QUEUE_TRANSFER_BIT, "transfer"},
  {VK_QUEUE_SPARSE_BINDING_BIT, "sparse-binding"},
#if VK_HEADER_VERSION >= 70
  {VK_QUEUE_PROTECTED_BIT, "protected"},
#endif
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
  {VK_QUEUE_VIDEO_DECODE_BIT_KHR, "decode"},
#ifdef VK_ENABLE_BETA_EXTENSIONS
  {VK_QUEUE_VIDEO_ENCODE_BIT_KHR, "encode"}
#endif
#endif
};
/**
 * gst_vulkan_queue_flags_to_string:
 *
 * Since: 1.18
 */
FLAGS_TO_STRING(queue, VkQueueFlags);

static const struct 
{
  VkSampleCountFlagBits flag_bit;
  const char *str;
} vk_sample_count_flags_map[] = {
  {VK_SAMPLE_COUNT_1_BIT, "1"},
  {VK_SAMPLE_COUNT_2_BIT, "2"},
  {VK_SAMPLE_COUNT_4_BIT, "4"},
  {VK_SAMPLE_COUNT_8_BIT, "8"},
  {VK_SAMPLE_COUNT_16_BIT, "16"},
  {VK_SAMPLE_COUNT_32_BIT, "32"},
  {VK_SAMPLE_COUNT_64_BIT, "64"},
};
/**
 * gst_vulkan_sample_count_flags_to_string:
 *
 * Since: 1.18
 */
FLAGS_TO_STRING(sample_count, VkSampleCountFlags);
/* *INDENT-ON* */

/**
 * gst_vulkan_physical_device_type_to_string:
 * @type: a `VkPhysicalDeviceType
 *
 * Returns: name of @type
 *
 * Since: 1.18
 */
const gchar *
gst_vulkan_physical_device_type_to_string (VkPhysicalDeviceType type)
{
  switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
      return "other";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      return "CPU";
    default:
      return "unknown";
  }
}

/**
 * gst_vulkan_present_mode_to_string:
 * @present_mode: a `VkPresentModeKHR`
 *
 * Returns: name of @present_mode
 *
 * Since: 1.20
 */
const gchar *
gst_vulkan_present_mode_to_string (VkPresentModeKHR present_mode)
{
  switch (present_mode) {
    case VK_PRESENT_MODE_FIFO_KHR:
      return "FIFO";
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
      return "immediate";
    case VK_PRESENT_MODE_MAILBOX_KHR:
      return "mailbox";
      /* XXX: add other values as necessary */
    default:
      return "unknown";
  }
}
