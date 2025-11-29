#pragma once

#include "gskvulkandescriptorsprivate.h"
#include "gskvulkanframeprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_REAL_DESCRIPTORS (gsk_vulkan_real_descriptors_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanRealDescriptors, gsk_vulkan_real_descriptors, GSK, VULKAN_REAL_DESCRIPTORS, GskVulkanDescriptors)

GskVulkanRealDescriptors *      gsk_vulkan_real_descriptors_new                 (GskVulkanFrame                 *frame);

gboolean                        gsk_vulkan_real_descriptors_is_full             (GskVulkanRealDescriptors       *self);

void                            gsk_vulkan_real_descriptors_prepare             (GskVulkanRealDescriptors       *self,
                                                                                 gsize                          *n_images,
                                                                                 gsize                          *n_buffers);
void                            gsk_vulkan_real_descriptors_update_sets         (GskVulkanRealDescriptors       *self,
                                                                                 VkDescriptorPool                vk_descriptor_pool);

G_END_DECLS

