#pragma once

#include "gskgpudescriptorsprivate.h"

#include "gskvulkandeviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_DESCRIPTORS         (gsk_vulkan_descriptors_get_type ())
#define GSK_VULKAN_DESCRIPTORS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSK_TYPE_VULKAN_DESCRIPTORS, GskVulkanDescriptors))
#define GSK_VULKAN_DESCRIPTORS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GSK_TYPE_VULKAN_DESCRIPTORS, GskVulkanDescriptorsClass))
#define GSK_IS_VULKAN_DESCRIPTORS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSK_TYPE_VULKAN_DESCRIPTORS))
#define GSK_IS_VULKAN_DESCRIPTORS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSK_TYPE_VULKAN_DESCRIPTORS))
#define GSK_VULKAN_DESCRIPTORS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSK_TYPE_VULKAN_DESCRIPTORS, GskVulkanDescriptorsClass))

typedef struct _GskVulkanDescriptorsClass GskVulkanDescriptorsClass;

struct _GskVulkanDescriptors
{
  GskGpuDescriptors parent_instance;
};

struct _GskVulkanDescriptorsClass
{
  GskGpuDescriptorsClass parent_class;

  GskVulkanPipelineLayout *     (* get_pipeline_layout)                         (GskVulkanDescriptors   *self);
  void                          (* bind)                                        (GskVulkanDescriptors   *self,
                                                                                 GskVulkanDescriptors   *previous,
                                                                                 VkCommandBuffer         vk_command_buffer);
};

GType                           gsk_vulkan_descriptors_get_type                 (void) G_GNUC_CONST;

GskVulkanPipelineLayout *       gsk_vulkan_descriptors_get_pipeline_layout      (GskVulkanDescriptors   *self);

void                            gsk_vulkan_descriptors_transition               (GskVulkanDescriptors   *self,
                                                                                 GskVulkanSemaphores    *semaphores,
                                                                                 VkCommandBuffer         vk_command_buffer);
void                            gsk_vulkan_descriptors_bind                     (GskVulkanDescriptors   *self,
                                                                                 GskVulkanDescriptors   *previous,
                                                                                 VkCommandBuffer         vk_command_buffer);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskVulkanDescriptors, g_object_unref)

G_END_DECLS

