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

#ifndef __GST_VULKAN_ERROR_H__
#define __GST_VULKAN_ERROR_H__

#include <gst/gst.h>
#include <vulkan/vulkan.h>
#include <gst/vulkan/vulkan-prelude.h>

G_BEGIN_DECLS

/**
 * GST_VULKAN_ERROR:
 *
 * Since: 1.18
 */
#define GST_VULKAN_ERROR (gst_vulkan_error_quark ())

/**
 * gst_vulkan_error_quark:
 *
 * Since: 1.18
 */
GST_VULKAN_API
GQuark gst_vulkan_error_quark (void);

GST_VULKAN_API
const char * gst_vulkan_result_to_string (VkResult result);

/**
 * GstVulkanError:
 * @GST_VULKAN_FAILED: undetermined error
 *
 * Since: 1.18
 */
/* custom error values */
typedef enum
{
  GST_VULKAN_FAILED = 0,
} GstVulkanError;

/* only fills error iff error != NULL and result < 0 */
GST_VULKAN_API
VkResult gst_vulkan_error_to_g_error (VkResult result, GError ** error, const char * format, ...) G_GNUC_PRINTF (3, 4);

G_END_DECLS

#endif /* __GST_VULKAN_ERROR_H__ */
