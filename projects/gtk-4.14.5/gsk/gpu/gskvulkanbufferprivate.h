#pragma once

#include "gskgpubufferprivate.h"

#include "gskvulkandeviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_BUFFER (gsk_vulkan_buffer_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanBuffer, gsk_vulkan_buffer, GSK, VULKAN_BUFFER, GskGpuBuffer)

GskGpuBuffer *          gsk_vulkan_buffer_new_vertex                    (GskVulkanDevice        *device,
                                                                         gsize                   size);
GskGpuBuffer *          gsk_vulkan_buffer_new_storage                   (GskVulkanDevice        *device,
                                                                         gsize                   size);
GskGpuBuffer *          gsk_vulkan_buffer_new_write                     (GskVulkanDevice        *device,
                                                                         gsize                   size);
GskGpuBuffer *          gsk_vulkan_buffer_new_read                      (GskVulkanDevice        *device,
                                                                         gsize                   size);

VkBuffer                gsk_vulkan_buffer_get_vk_buffer                 (GskVulkanBuffer        *self);

G_END_DECLS

