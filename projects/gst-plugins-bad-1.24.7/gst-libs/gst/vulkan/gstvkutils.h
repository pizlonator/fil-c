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
#ifndef _VK_UTILS_H_
#define _VK_UTILS_H_

#include <gst/gst.h>
#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

GST_VULKAN_API
gboolean                gst_vulkan_ensure_element_data          (GstElement * element,
                                                                 GstVulkanDisplay ** display_ptr,
                                                                 GstVulkanInstance ** instance_ptr);
GST_VULKAN_API
gboolean                gst_vulkan_handle_set_context           (GstElement * element,
                                                                 GstContext * context,
                                                                 GstVulkanDisplay ** display,
                                                                 GstVulkanInstance ** instance);
GST_VULKAN_API
gboolean                gst_vulkan_handle_context_query         (GstElement * element,
                                                                 GstQuery * query,
                                                                 GstVulkanDisplay * display,
                                                                 GstVulkanInstance * instance,
                                                                 GstVulkanDevice * device);

GST_VULKAN_API
void                    gst_vulkan_global_context_query         (GstElement * element,
                                                                 const gchar * context_type);
GST_VULKAN_API
GstQuery *              gst_vulkan_local_context_query          (GstElement * element,
                                                                 const gchar * context_type);
GST_VULKAN_API
gboolean                gst_vulkan_run_query                    (GstElement * element,
                                                                 GstQuery * query,
                                                                 GstPadDirection direction);

GST_VULKAN_API
GstVulkanImageView *    gst_vulkan_get_or_create_image_view     (GstVulkanImageMemory * image);

GST_VULKAN_API
GstVulkanImageView *    gst_vulkan_get_or_create_image_view_with_info (GstVulkanImageMemory * image,
                                                                       VkImageViewCreateInfo * create_info);

GST_VULKAN_API
GstVulkanHandle *       gst_vulkan_create_shader                (GstVulkanDevice * device,
                                                                 const gchar * code,
                                                                 gsize size,
                                                                 GError ** error);

G_END_DECLS

#endif /*_VK_UTILS_H_ */
