#include "config.h"

#include "gskvulkandescriptorsprivate.h"

#include "gskvulkanbufferprivate.h"
#include "gskvulkanframeprivate.h"
#include "gskvulkanimageprivate.h"

G_DEFINE_TYPE (GskVulkanDescriptors, gsk_vulkan_descriptors, GSK_TYPE_GPU_DESCRIPTORS)

static void
gsk_vulkan_descriptors_class_init (GskVulkanDescriptorsClass *klass)
{
}

static void
gsk_vulkan_descriptors_init (GskVulkanDescriptors *self)
{
}

GskVulkanPipelineLayout *
gsk_vulkan_descriptors_get_pipeline_layout (GskVulkanDescriptors *self)
{
  return GSK_VULKAN_DESCRIPTORS_GET_CLASS (self)->get_pipeline_layout (self);
}

void
gsk_vulkan_descriptors_transition (GskVulkanDescriptors *self,
                                   GskVulkanSemaphores  *semaphores,
                                   VkCommandBuffer       vk_command_buffer)
{
  GskGpuDescriptors *desc = GSK_GPU_DESCRIPTORS (self);
  gsize i;

  for (i = 0; i < gsk_gpu_descriptors_get_n_images (desc); i++)
    {
      gsk_vulkan_image_transition (GSK_VULKAN_IMAGE (gsk_gpu_descriptors_get_image (desc, i)),
                                   semaphores,
                                   vk_command_buffer,
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                   VK_ACCESS_SHADER_READ_BIT);
    }
}

void
gsk_vulkan_descriptors_bind (GskVulkanDescriptors *self,
                             GskVulkanDescriptors *previous,
                             VkCommandBuffer       vk_command_buffer)
{
  GSK_VULKAN_DESCRIPTORS_GET_CLASS (self)->bind (self, previous, vk_command_buffer);
}
