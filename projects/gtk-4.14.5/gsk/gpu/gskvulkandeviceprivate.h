#pragma once

#include "gskgpudeviceprivate.h"

#include "gskdebugprivate.h"
#include "gskgpuclipprivate.h"
#include "gskvulkanmemoryprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkvulkancontextprivate.h"

G_BEGIN_DECLS

/* also used by gskvulkanframe.c */
enum {
  GSK_VULKAN_IMAGE_SET_LAYOUT,
  GSK_VULKAN_BUFFER_SET_LAYOUT,

  GSK_VULKAN_N_DESCRIPTOR_SETS
};

#define GSK_TYPE_VULKAN_DEVICE (gsk_vulkan_device_get_type ())

G_DECLARE_FINAL_TYPE(GskVulkanDevice, gsk_vulkan_device, GSK, VULKAN_DEVICE, GskGpuDevice)

typedef struct _GskVulkanPipelineLayout GskVulkanPipelineLayout;

GskGpuDevice *          gsk_vulkan_device_get_for_display               (GdkDisplay             *display,
                                                                         GError                **error);

gsize                   gsk_vulkan_device_get_max_immutable_samplers    (GskVulkanDevice        *self);
gsize                   gsk_vulkan_device_get_max_samplers              (GskVulkanDevice        *self);
gsize                   gsk_vulkan_device_get_max_buffers               (GskVulkanDevice        *self);
gboolean                gsk_vulkan_device_has_feature                   (GskVulkanDevice        *self,
                                                                         GdkVulkanFeatures       feature) G_GNUC_PURE;

VkDevice                gsk_vulkan_device_get_vk_device                 (GskVulkanDevice        *self) G_GNUC_PURE;
VkPhysicalDevice        gsk_vulkan_device_get_vk_physical_device        (GskVulkanDevice        *self) G_GNUC_PURE;
VkQueue                 gsk_vulkan_device_get_vk_queue                  (GskVulkanDevice        *self) G_GNUC_PURE;
uint32_t                gsk_vulkan_device_get_vk_queue_family_index     (GskVulkanDevice        *self) G_GNUC_PURE;
VkCommandPool           gsk_vulkan_device_get_vk_command_pool           (GskVulkanDevice        *self) G_GNUC_PURE;
VkSampler               gsk_vulkan_device_get_vk_sampler                (GskVulkanDevice        *self,
                                                                         GskGpuSampler           sampler) G_GNUC_PURE;

GskVulkanPipelineLayout *
                        gsk_vulkan_device_acquire_pipeline_layout       (GskVulkanDevice        *self,
                                                                         VkSampler              *immutable_samplers,
                                                                         gsize                   n_immutable_samplers,
                                                                         gsize                   n_samplers,
                                                                         gsize                   n_buffers);
void                    gsk_vulkan_device_release_pipeline_layout       (GskVulkanDevice        *self,
                                                                         GskVulkanPipelineLayout*layout);
void                    gsk_vulkan_device_get_pipeline_sizes            (GskVulkanDevice        *self,
                                                                         GskVulkanPipelineLayout*layout,
                                                                         gsize                  *n_immutable_samplers,
                                                                         gsize                  *n_samplers,
                                                                         gsize                  *n_buffers);
VkDescriptorSetLayout   gsk_vulkan_device_get_vk_image_set_layout       (GskVulkanDevice        *self,
                                                                         GskVulkanPipelineLayout*layout) G_GNUC_PURE;
VkDescriptorSetLayout   gsk_vulkan_device_get_vk_buffer_set_layout      (GskVulkanDevice        *self,
                                                                         GskVulkanPipelineLayout*layout) G_GNUC_PURE;
VkPipelineLayout        gsk_vulkan_device_get_vk_pipeline_layout        (GskVulkanDevice        *self,
                                                                         GskVulkanPipelineLayout*layout) G_GNUC_PURE;

VkSamplerYcbcrConversion
                        gsk_vulkan_device_get_vk_conversion             (GskVulkanDevice        *self,
                                                                         VkFormat                vk_format,
                                                                         VkSampler              *out_sampler);
VkRenderPass            gsk_vulkan_device_get_vk_render_pass            (GskVulkanDevice        *self,
                                                                         VkFormat                format,
                                                                         VkImageLayout           from_layout,
                                                                         VkImageLayout           to_layout);
VkPipeline              gsk_vulkan_device_get_vk_pipeline               (GskVulkanDevice        *self,
                                                                         GskVulkanPipelineLayout*layout,
                                                                         const GskGpuShaderOpClass *op_class,
                                                                         guint32                 variation,
                                                                         GskGpuShaderClip        clip,
                                                                         GskGpuBlend             blend,
                                                                         VkFormat                format,
                                                                         VkRenderPass            render_pass);

GskVulkanAllocator *    gsk_vulkan_device_get_external_allocator        (GskVulkanDevice        *self);
GskVulkanAllocator *    gsk_vulkan_device_find_allocator                (GskVulkanDevice        *self,
                                                                         uint32_t                allowed_types,
                                                                         VkMemoryPropertyFlags   required_flags,
                                                                         VkMemoryPropertyFlags   desired_flags);
static inline VkResult
gsk_vulkan_handle_result (VkResult    res,
                          const char *called_function)
{
  if (res != VK_SUCCESS)
    {
      GSK_DEBUG (VULKAN, "%s(): %s (%d)", called_function, gdk_vulkan_strerror (res), res);
    }
  return res;
}

#define GSK_VK_CHECK(func, ...) gsk_vulkan_handle_result (func (__VA_ARGS__), G_STRINGIFY (func))


G_END_DECLS
