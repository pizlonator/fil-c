#include "config.h"

#include "gskvulkanrealdescriptorsprivate.h"

#include "gskvulkanbufferprivate.h"
#include "gskvulkanframeprivate.h"
#include "gskvulkanimageprivate.h"

#define GDK_ARRAY_NAME gsk_descriptor_image_infos
#define GDK_ARRAY_TYPE_NAME GskDescriptorImageInfos
#define GDK_ARRAY_ELEMENT_TYPE VkDescriptorImageInfo
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 128
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define GDK_ARRAY_NAME gsk_descriptor_buffer_infos
#define GDK_ARRAY_TYPE_NAME GskDescriptorBufferInfos
#define GDK_ARRAY_ELEMENT_TYPE VkDescriptorBufferInfo
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 32
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define GDK_ARRAY_NAME gsk_samplers
#define GDK_ARRAY_TYPE_NAME GskSamplers
#define GDK_ARRAY_ELEMENT_TYPE VkSampler
#define GDK_ARRAY_PREALLOC 32
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

struct _GskVulkanRealDescriptors
{
  GskVulkanDescriptors parent_instance;

  GskVulkanFrame *frame; /* no reference, the frame owns us */

  GskVulkanPipelineLayout *pipeline_layout;

  GskSamplers immutable_samplers;
  GskDescriptorImageInfos descriptor_immutable_images;
  GskDescriptorImageInfos descriptor_images;
  GskDescriptorBufferInfos descriptor_buffers;

  VkDescriptorSet descriptor_sets[GSK_VULKAN_N_DESCRIPTOR_SETS];
};

G_DEFINE_TYPE (GskVulkanRealDescriptors, gsk_vulkan_real_descriptors, GSK_TYPE_VULKAN_DESCRIPTORS)

static GskVulkanPipelineLayout *
gsk_vulkan_real_descriptors_get_pipeline_layout (GskVulkanDescriptors *desc)
{
  GskVulkanRealDescriptors *self = GSK_VULKAN_REAL_DESCRIPTORS (desc);

  return self->pipeline_layout;
}

static void
gsk_vulkan_real_descriptors_bind (GskVulkanDescriptors *desc,
                                  GskVulkanDescriptors *previous,
                                  VkCommandBuffer       vk_command_buffer)
{
  GskVulkanRealDescriptors *self = GSK_VULKAN_REAL_DESCRIPTORS (desc);

  if (desc == previous)
    return;

  vkCmdBindDescriptorSets (vk_command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           gsk_vulkan_device_get_vk_pipeline_layout (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self->frame))),
                                                                     self->pipeline_layout),
                           0,
                           G_N_ELEMENTS (self->descriptor_sets),
                           self->descriptor_sets,
                           0,
                           NULL);
}

static gboolean
gsk_vulkan_real_descriptors_add_image (GskGpuDescriptors *desc,
                                       GskGpuImage       *image,
                                       GskGpuSampler      sampler,
                                       guint32            *out_descriptor)
{
  GskVulkanRealDescriptors *self = GSK_VULKAN_REAL_DESCRIPTORS (desc);
  GskVulkanImage *vulkan_image = GSK_VULKAN_IMAGE (image);
  GskVulkanDevice *device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self->frame)));
  VkSampler vk_sampler;
  guint32 result;

  vk_sampler = gsk_vulkan_image_get_vk_sampler (vulkan_image);

  if (vk_sampler)
    {
      if (gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) >=
          gsk_vulkan_device_get_max_immutable_samplers (device))
        return FALSE;
      if ((1 + gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images)) * 3 +
          gsk_descriptor_image_infos_get_size (&self->descriptor_images) >
          gsk_vulkan_device_get_max_samplers (device))
        return FALSE;

      result = gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) << 1 | 1;

      gsk_samplers_append (&self->immutable_samplers, vk_sampler);
      gsk_descriptor_image_infos_append (&self->descriptor_immutable_images,
                                         &(VkDescriptorImageInfo) {
                                           .imageView = gsk_vulkan_image_get_vk_image_view (vulkan_image),
                                           .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                         });
    }
  else
    {
      if (MAX (1, gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) * 3) +
          gsk_descriptor_image_infos_get_size (&self->descriptor_images) >=
          gsk_vulkan_device_get_max_samplers (device))
        return FALSE;

      result = gsk_descriptor_image_infos_get_size (&self->descriptor_images) << 1;

      gsk_descriptor_image_infos_append (&self->descriptor_images,
                                         &(VkDescriptorImageInfo) {
                                           .sampler = gsk_vulkan_device_get_vk_sampler (device, sampler),
                                           .imageView = gsk_vulkan_image_get_vk_image_view (vulkan_image),
                                           .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                         });
    }

  *out_descriptor = result;
  return TRUE;
}

static gboolean
gsk_vulkan_real_descriptors_add_buffer (GskGpuDescriptors *desc,
                                        GskGpuBuffer      *buffer,
                                        guint32           *out_descriptor)
{
  GskVulkanRealDescriptors *self = GSK_VULKAN_REAL_DESCRIPTORS (desc);
  GskVulkanDevice *device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self->frame)));

  if (gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers) >=
      gsk_vulkan_device_get_max_buffers (device))
    return FALSE;

  *out_descriptor = gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers);
  gsk_descriptor_buffer_infos_append (&self->descriptor_buffers,
                                      &(VkDescriptorBufferInfo) {
                                        .buffer = gsk_vulkan_buffer_get_vk_buffer (GSK_VULKAN_BUFFER (buffer)),
                                        .offset = 0,
                                        .range = VK_WHOLE_SIZE
                                      });

  return TRUE;
}

static void
gsk_vulkan_real_descriptors_finalize (GObject *object)
{
  GskVulkanRealDescriptors *self = GSK_VULKAN_REAL_DESCRIPTORS (object);

  gsk_samplers_clear (&self->immutable_samplers);
  gsk_descriptor_image_infos_clear (&self->descriptor_immutable_images);
  gsk_descriptor_image_infos_clear (&self->descriptor_images);
  gsk_descriptor_buffer_infos_clear (&self->descriptor_buffers);

  gsk_vulkan_device_release_pipeline_layout (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self->frame))),
                                             self->pipeline_layout);

  G_OBJECT_CLASS (gsk_vulkan_real_descriptors_parent_class)->finalize (object);
}

static void
gsk_vulkan_real_descriptors_class_init (GskVulkanRealDescriptorsClass *klass)
{
  GskVulkanDescriptorsClass *vulkan_descriptors_class = GSK_VULKAN_DESCRIPTORS_CLASS (klass);
  GskGpuDescriptorsClass *descriptors_class = GSK_GPU_DESCRIPTORS_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gsk_vulkan_real_descriptors_finalize;

  descriptors_class->add_image = gsk_vulkan_real_descriptors_add_image;
  descriptors_class->add_buffer = gsk_vulkan_real_descriptors_add_buffer;

  vulkan_descriptors_class->get_pipeline_layout = gsk_vulkan_real_descriptors_get_pipeline_layout;
  vulkan_descriptors_class->bind = gsk_vulkan_real_descriptors_bind;
}

static void
gsk_vulkan_real_descriptors_init (GskVulkanRealDescriptors *self)
{
  gsk_samplers_init (&self->immutable_samplers);
  gsk_descriptor_image_infos_init (&self->descriptor_immutable_images);
  gsk_descriptor_image_infos_init (&self->descriptor_images);
  gsk_descriptor_buffer_infos_init (&self->descriptor_buffers);
}

GskVulkanRealDescriptors *
gsk_vulkan_real_descriptors_new (GskVulkanFrame *frame)
{
  GskVulkanRealDescriptors *self;

  self = g_object_new (GSK_TYPE_VULKAN_REAL_DESCRIPTORS, NULL);

  self->frame = frame;

  return self;
}

gboolean
gsk_vulkan_real_descriptors_is_full (GskVulkanRealDescriptors *self)
{
  GskVulkanDevice *device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self->frame)));

  return gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) >= gsk_vulkan_device_get_max_immutable_samplers (device) ||
         gsk_descriptor_image_infos_get_size (&self->descriptor_images) +
         MAX (1, gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) * 3) >=
         gsk_vulkan_device_get_max_samplers (device) ||
         gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers) >= gsk_vulkan_device_get_max_buffers (device);
}

static void
gsk_vulkan_real_descriptors_fill_sets (GskVulkanRealDescriptors *self)
{
  gsize n_immutable_samplers, n_samplers, n_buffers;
  GskVulkanDevice *device;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self->frame)));

  if (gsk_vulkan_device_has_feature (device, GDK_VULKAN_FEATURE_DESCRIPTOR_INDEXING))
    return;

  /* If descriptor indexing isn't supported, all descriptors in the shaders
   * must be properly setup. And that means we need to have
   * descriptors for all of them.
   */
  gsk_vulkan_device_get_pipeline_sizes (device,
                                        self->pipeline_layout,
                                        &n_immutable_samplers,
                                        &n_samplers,
                                        &n_buffers);

  if (gsk_descriptor_image_infos_get_size (&self->descriptor_images) == 0)
    {
      /* We have no image, find any random image and attach it */
      guint32 ignored;

      if (!gsk_gpu_descriptors_add_image (GSK_GPU_DESCRIPTORS (self),
                                          gsk_gpu_device_get_atlas_image (GSK_GPU_DEVICE (device)),
                                          GSK_GPU_SAMPLER_DEFAULT,
                                          &ignored))
        {
          g_assert_not_reached ();
        }
    }
  while (MAX (1, n_immutable_samplers) > gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images))
    {
      gsk_descriptor_image_infos_append (&self->descriptor_immutable_images, gsk_descriptor_image_infos_get (&self->descriptor_images, 0));
    }
  while (n_samplers - MAX (1, 3 * n_immutable_samplers) > gsk_descriptor_image_infos_get_size (&self->descriptor_images))
    {
      gsk_descriptor_image_infos_append (&self->descriptor_images, gsk_descriptor_image_infos_get (&self->descriptor_images, 0));
    }
  if (gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers) == 0)
    {
      /* If there's no storage buffer yet, just make one */
      GskGpuBuffer *buffer;
      gsize ignored_offset;
      guint32 ignored;

      buffer = gsk_gpu_frame_write_storage_buffer (GSK_GPU_FRAME (self->frame), NULL, 0, &ignored_offset);
      if (!gsk_gpu_descriptors_add_buffer (GSK_GPU_DESCRIPTORS (self),
                                           buffer,
                                           &ignored))
        {
          g_assert_not_reached ();
        }
    }
  while (n_buffers > gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers))
    {
      gsk_descriptor_buffer_infos_append (&self->descriptor_buffers, gsk_descriptor_buffer_infos_get (&self->descriptor_buffers, 0));
    }
}

void
gsk_vulkan_real_descriptors_prepare (GskVulkanRealDescriptors *self,
                                     gsize                    *n_images,
                                     gsize                    *n_buffers)
{
  self->pipeline_layout = gsk_vulkan_device_acquire_pipeline_layout (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self->frame))),
                                                                     gsk_samplers_get_data (&self->immutable_samplers),
                                                                     gsk_samplers_get_size (&self->immutable_samplers),
                                                                     gsk_descriptor_image_infos_get_size (&self->descriptor_images),
                                                                     gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers));
  gsk_vulkan_real_descriptors_fill_sets (self);

  *n_images = MAX (1, gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images)) +
              gsk_descriptor_image_infos_get_size (&self->descriptor_images);
  *n_buffers = gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers);
}

void
gsk_vulkan_real_descriptors_update_sets (GskVulkanRealDescriptors *self,
                                         VkDescriptorPool          vk_descriptor_pool)
{
  VkWriteDescriptorSet write_descriptor_sets[GSK_VULKAN_N_DESCRIPTOR_SETS + 1];
  gsize n_descriptor_sets;
  VkDevice vk_device;
  gboolean descriptor_indexing;
  GskVulkanDevice *device;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self->frame)));
  descriptor_indexing = gsk_vulkan_device_has_feature (device, GDK_VULKAN_FEATURE_DESCRIPTOR_INDEXING);
  vk_device = gsk_vulkan_device_get_vk_device (device);

  GSK_VK_CHECK (vkAllocateDescriptorSets, vk_device,
                                          &(VkDescriptorSetAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .descriptorPool = vk_descriptor_pool,
                                              .descriptorSetCount = GSK_VULKAN_N_DESCRIPTOR_SETS,
                                              .pSetLayouts = (VkDescriptorSetLayout[GSK_VULKAN_N_DESCRIPTOR_SETS]) {
                                                gsk_vulkan_device_get_vk_image_set_layout (device, self->pipeline_layout),
                                                gsk_vulkan_device_get_vk_buffer_set_layout (device, self->pipeline_layout),
                                              },
                                              .pNext = !descriptor_indexing ? NULL : &(VkDescriptorSetVariableDescriptorCountAllocateInfo) {
                                                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
                                                .descriptorSetCount = GSK_VULKAN_N_DESCRIPTOR_SETS,
                                                .pDescriptorCounts = (uint32_t[GSK_VULKAN_N_DESCRIPTOR_SETS]) {
                                                  gsk_descriptor_image_infos_get_size (&self->descriptor_images),
                                                  gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers)
                                                }
                                              }
                                          },
                                          self->descriptor_sets);

  n_descriptor_sets = 0;
  if (gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) > 0)
    {
      write_descriptor_sets[n_descriptor_sets++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[GSK_VULKAN_IMAGE_SET_LAYOUT],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images),
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = gsk_descriptor_image_infos_get_data (&self->descriptor_immutable_images)
      };
    }
  if (gsk_descriptor_image_infos_get_size (&self->descriptor_images) > 0)
    {
      write_descriptor_sets[n_descriptor_sets++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[GSK_VULKAN_IMAGE_SET_LAYOUT],
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = gsk_descriptor_image_infos_get_size (&self->descriptor_images),
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = gsk_descriptor_image_infos_get_data (&self->descriptor_images)
      };
    }
  if (gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers) > 0)
    {
      write_descriptor_sets[n_descriptor_sets++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[GSK_VULKAN_BUFFER_SET_LAYOUT],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers),
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .pBufferInfo = gsk_descriptor_buffer_infos_get_data (&self->descriptor_buffers)
      };
    }

  vkUpdateDescriptorSets (vk_device,
                          n_descriptor_sets,
                          write_descriptor_sets,
                          0, NULL);
}
