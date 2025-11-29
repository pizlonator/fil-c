#include "config.h"

#include "gskvulkansubdescriptorsprivate.h"

struct _GskVulkanSubDescriptors
{
  GskVulkanDescriptors parent_instance;

  GskVulkanDescriptors *parent;
};

G_DEFINE_TYPE (GskVulkanSubDescriptors, gsk_vulkan_sub_descriptors, GSK_TYPE_VULKAN_DESCRIPTORS)

static GskVulkanPipelineLayout *
gsk_vulkan_sub_descriptors_get_pipeline_layout (GskVulkanDescriptors *desc)
{
  GskVulkanSubDescriptors *self = GSK_VULKAN_SUB_DESCRIPTORS (desc);

  return gsk_vulkan_descriptors_get_pipeline_layout (self->parent);
}

static void
gsk_vulkan_sub_descriptors_bind (GskVulkanDescriptors *desc,
                                 GskVulkanDescriptors *previous,
                                 VkCommandBuffer       vk_command_buffer)
{
  GskVulkanSubDescriptors *self = GSK_VULKAN_SUB_DESCRIPTORS (desc);

  if (GSK_IS_VULKAN_SUB_DESCRIPTORS (previous))
    previous = GSK_VULKAN_SUB_DESCRIPTORS (previous)->parent;

  if (self->parent == previous)
    return;

  gsk_vulkan_descriptors_bind (self->parent, previous, vk_command_buffer);
}

static gboolean
gsk_vulkan_sub_descriptors_add_image (GskGpuDescriptors *desc,
                                      GskGpuImage       *image,
                                      GskGpuSampler      sampler,
                                      guint32           *out_descriptor)
{
  GskVulkanSubDescriptors *self = GSK_VULKAN_SUB_DESCRIPTORS (desc);

  return gsk_gpu_descriptors_add_image (GSK_GPU_DESCRIPTORS (self->parent),
                                        image,
                                        sampler,
                                        out_descriptor);
}

static gboolean
gsk_vulkan_sub_descriptors_add_buffer (GskGpuDescriptors *desc,
                                       GskGpuBuffer      *buffer,
                                       guint32           *out_descriptor)
{
  GskVulkanSubDescriptors *self = GSK_VULKAN_SUB_DESCRIPTORS (desc);

  return gsk_gpu_descriptors_add_buffer (GSK_GPU_DESCRIPTORS (self->parent),
                                         buffer,
                                         out_descriptor);
}

static void
gsk_vulkan_sub_descriptors_finalize (GObject *object)
{
  GskVulkanSubDescriptors *self = GSK_VULKAN_SUB_DESCRIPTORS (object);

  g_object_unref (self->parent);

  G_OBJECT_CLASS (gsk_vulkan_sub_descriptors_parent_class)->finalize (object);
}

static void
gsk_vulkan_sub_descriptors_class_init (GskVulkanSubDescriptorsClass *klass)
{
  GskVulkanDescriptorsClass *vulkan_descriptors_class = GSK_VULKAN_DESCRIPTORS_CLASS (klass);
  GskGpuDescriptorsClass *descriptors_class = GSK_GPU_DESCRIPTORS_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gsk_vulkan_sub_descriptors_finalize;

  descriptors_class->add_image = gsk_vulkan_sub_descriptors_add_image;
  descriptors_class->add_buffer = gsk_vulkan_sub_descriptors_add_buffer;

  vulkan_descriptors_class->get_pipeline_layout = gsk_vulkan_sub_descriptors_get_pipeline_layout;
  vulkan_descriptors_class->bind = gsk_vulkan_sub_descriptors_bind;
}

static void
gsk_vulkan_sub_descriptors_init (GskVulkanSubDescriptors *self)
{
}

GskVulkanSubDescriptors *
gsk_vulkan_sub_descriptors_new (GskVulkanDescriptors *parent)
{
  GskVulkanSubDescriptors *self;

  self = g_object_new (GSK_TYPE_VULKAN_SUB_DESCRIPTORS, NULL);

  self->parent = g_object_ref (parent);

  return self;
}

