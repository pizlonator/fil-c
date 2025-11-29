#pragma once

#include "gskgpuframeprivate.h"

#include "gskvulkandeviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_FRAME (gsk_vulkan_frame_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanFrame, gsk_vulkan_frame, GSK, VULKAN_FRAME, GskGpuFrame)

VkFence                 gsk_vulkan_frame_get_vk_fence                   (GskVulkanFrame         *self) G_GNUC_PURE;

void                    gsk_vulkan_semaphores_add_wait                  (GskVulkanSemaphores    *self,
                                                                         VkSemaphore             semaphore,
                                                                         VkPipelineStageFlags    stage);
void                    gsk_vulkan_semaphores_add_signal                (GskVulkanSemaphores    *self,
                                                                         VkSemaphore             semaphore);

G_END_DECLS
