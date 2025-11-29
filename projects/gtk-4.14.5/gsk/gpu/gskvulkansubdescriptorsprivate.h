#pragma once

#include "gskvulkandescriptorsprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_SUB_DESCRIPTORS (gsk_vulkan_sub_descriptors_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanSubDescriptors, gsk_vulkan_sub_descriptors, GSK, VULKAN_SUB_DESCRIPTORS, GskVulkanDescriptors)

GskVulkanSubDescriptors *       gsk_vulkan_sub_descriptors_new          (GskVulkanDescriptors           *parent);

G_END_DECLS

