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

#include <glib/gprintf.h>

#include "gstvkerror.h"

/**
 * SECTION:vkerror
 * @title: GstVulkanError
 * @short_description: Vulkan errors
 * @see_also: #GstVulkanInstance, #GstVulkanDevice
 */

/* *INDENT-OFF* */
static const struct
{
  VkResult result;
  const char *str;
} vk_result_string_map[] = {
  {VK_ERROR_OUT_OF_HOST_MEMORY, "Out of host memory"},
  {VK_ERROR_OUT_OF_DEVICE_MEMORY, "Out of device memory"},
  {VK_ERROR_INITIALIZATION_FAILED, "Initialization failed"},
  {VK_ERROR_DEVICE_LOST, "Device lost"},
  {VK_ERROR_MEMORY_MAP_FAILED, "Map failed"},
  {VK_ERROR_LAYER_NOT_PRESENT, "Layer not present"},
  {VK_ERROR_EXTENSION_NOT_PRESENT, "Extension not present"},
  {VK_ERROR_FEATURE_NOT_PRESENT, "Feature not present"},
  {VK_ERROR_INCOMPATIBLE_DRIVER, "Incompatible driver"},
  {VK_ERROR_TOO_MANY_OBJECTS, "Too many objects"},
  {VK_ERROR_FORMAT_NOT_SUPPORTED, "Format not supported"},
  {VK_ERROR_SURFACE_LOST_KHR, "Surface lost"},
  {VK_ERROR_OUT_OF_DATE_KHR, "Out of date"},
  {VK_ERROR_INCOMPATIBLE_DISPLAY_KHR, "Incompatible display"},
  {VK_ERROR_NATIVE_WINDOW_IN_USE_KHR, "Native window in use"},
};
/* *INDENT-ON* */

GQuark
gst_vulkan_error_quark (void)
{
  return g_quark_from_static_string ("gst-vulkan-error");
}

/**
 * gst_vulkan_result_to_string: (skip)
 * @result: a VkResult
 *
 * Returns: a message that corresponds to @result
 *
 * Since: 1.22
 */
const char *
gst_vulkan_result_to_string (VkResult result)
{
  int i;

  if (result >= 0)
    return NULL;

  for (i = 0; i < G_N_ELEMENTS (vk_result_string_map); i++) {
    if (result == vk_result_string_map[i].result)
      return vk_result_string_map[i].str;
  }

  return "Unknown Error";
}

/**
 * gst_vulkan_error_to_g_error: (skip)
 * @result: a VkResult
 * @error: (out) (optional) (transfer full): a #GError to fill
 * @format: the printf-like format to write into the #GError
 * @...: arguments for @format
 *
 * if @result indicates an error condition, fills out #GError with details of
 * the error
 *
 * Returns: @result for easy chaining
 *
 * Since: 1.18
 */
VkResult
gst_vulkan_error_to_g_error (VkResult result, GError ** error,
    const char *format, ...)
{
  const char *result_str;
  gchar *string;
  va_list args;

  if (error == NULL)
    /* we don't have an error to set */
    return result;

  result_str = gst_vulkan_result_to_string (result);
  if (result_str == NULL) {
    if (result < 0) {
      result_str = "Unknown";
    } else {
      return result;
    }
  }

  va_start (args, format);
  g_vasprintf (&string, format, args);
  va_end (args);

  g_set_error (error, GST_VULKAN_ERROR, result, "%s (0x%x, %i): %s", result_str,
      result, result, string);

  return result;
}
