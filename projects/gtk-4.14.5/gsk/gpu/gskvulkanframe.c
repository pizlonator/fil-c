#include "config.h"

#include "gskvulkanframeprivate.h"

#include "gskgpuopprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkandescriptorsprivate.h"
#include "gskvulkandeviceprivate.h"
#include "gskvulkanimageprivate.h"
#include "gskvulkanrealdescriptorsprivate.h"
#include "gskvulkansubdescriptorsprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkdmabuftextureprivate.h"

#define GDK_ARRAY_NAME gsk_descriptors
#define GDK_ARRAY_TYPE_NAME GskDescriptors
#define GDK_ARRAY_ELEMENT_TYPE GskVulkanRealDescriptors *
#define GDK_ARRAY_FREE_FUNC g_object_unref
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define GDK_ARRAY_NAME gsk_semaphores
#define GDK_ARRAY_TYPE_NAME GskSemaphores
#define GDK_ARRAY_ELEMENT_TYPE VkSemaphore
#define GDK_ARRAY_PREALLOC 16
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define GDK_ARRAY_NAME gsk_pipeline_stages
#define GDK_ARRAY_TYPE_NAME GskPipelineStages
#define GDK_ARRAY_ELEMENT_TYPE VkPipelineStageFlags
#define GDK_ARRAY_PREALLOC 16
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

struct _GskVulkanSemaphores
{
  GskSemaphores wait_semaphores;
  GskPipelineStages wait_stages;
  GskSemaphores signal_semaphores;
};

struct _GskVulkanFrame
{
  GskGpuFrame parent_instance;

  VkFence vk_fence;
  VkCommandBuffer vk_command_buffer;
  VkDescriptorPool vk_descriptor_pool;

  GskDescriptors descriptors;

  gsize pool_n_sets;
  gsize pool_n_images;
  gsize pool_n_buffers;
};

struct _GskVulkanFrameClass
{
  GskGpuFrameClass parent_class;
};

G_DEFINE_TYPE (GskVulkanFrame, gsk_vulkan_frame, GSK_TYPE_GPU_FRAME)

static gboolean
gsk_vulkan_frame_is_busy (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  VkDevice device;

  device = gsk_vulkan_device_get_vk_device (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)));

  return vkGetFenceStatus (device, self->vk_fence) == VK_NOT_READY;
}

static void
gsk_vulkan_frame_wait (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  VkDevice vk_device;

  vk_device = gsk_vulkan_device_get_vk_device (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)));

  GSK_VK_CHECK (vkWaitForFences, vk_device,
                                 1,
                                 &self->vk_fence,
                                 VK_FALSE,
                                 INT64_MAX);
}

static void
gsk_vulkan_frame_setup (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  GskVulkanDevice *device;
  VkDevice vk_device;
  VkCommandPool vk_command_pool;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame));
  vk_device = gsk_vulkan_device_get_vk_device (device);
  vk_command_pool = gsk_vulkan_device_get_vk_command_pool (device);

  GSK_VK_CHECK (vkAllocateCommandBuffers, vk_device,
                                          &(VkCommandBufferAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                              .commandPool = vk_command_pool,
                                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                              .commandBufferCount = 1,
                                          },
                                          &self->vk_command_buffer);

  GSK_VK_CHECK (vkCreateFence, vk_device,
                               &(VkFenceCreateInfo) {
                                   .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                   .flags = VK_FENCE_CREATE_SIGNALED_BIT
                               },
                               NULL,
                               &self->vk_fence);
}

static void
gsk_vulkan_frame_cleanup (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  GskVulkanDevice *device;
  VkDevice vk_device;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame));
  vk_device = gsk_vulkan_device_get_vk_device (device);

  GSK_VK_CHECK (vkWaitForFences, vk_device,
                                 1,
                                 &self->vk_fence,
                                 VK_TRUE,
                                 INT64_MAX);

  GSK_VK_CHECK (vkResetFences, vk_device,
                               1,
                               &self->vk_fence);

  GSK_VK_CHECK (vkResetCommandBuffer, self->vk_command_buffer,
                                      0);

  if (self->vk_descriptor_pool != VK_NULL_HANDLE)
    {
      GSK_VK_CHECK (vkResetDescriptorPool, vk_device,
                                           self->vk_descriptor_pool,
                                           0);
    }

  gsk_descriptors_set_size (&self->descriptors, 0);

  GSK_GPU_FRAME_CLASS (gsk_vulkan_frame_parent_class)->cleanup (frame);
}

static GskGpuImage *
gsk_vulkan_frame_upload_texture (GskGpuFrame  *frame,
                                 gboolean      with_mipmap,
                                 GdkTexture   *texture)
{
#ifdef HAVE_DMABUF
  if (GDK_IS_DMABUF_TEXTURE (texture))
    {
      GskGpuImage *image = gsk_vulkan_image_new_for_dmabuf (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                            texture);
      if (image)
        return image;
    }
#endif

  return GSK_GPU_FRAME_CLASS (gsk_vulkan_frame_parent_class)->upload_texture (frame, with_mipmap, texture);
}

static void
gsk_vulkan_frame_prepare_descriptors (GskVulkanFrame *self)
{
  GskVulkanDevice *device;
  VkDevice vk_device;
  gsize i, n_images, n_buffers, n_sets;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self)));
  vk_device = gsk_vulkan_device_get_vk_device (device);

  n_images = 0;
  n_buffers = 0;
  n_sets = 2 * gsk_descriptors_get_size (&self->descriptors);
  for (i = 0; i < gsk_descriptors_get_size (&self->descriptors); i++)
    {
      gsize n_desc_images, n_desc_buffers;
      GskVulkanRealDescriptors *desc = gsk_descriptors_get (&self->descriptors, i);
      gsk_vulkan_real_descriptors_prepare (desc, &n_desc_images, &n_desc_buffers);
      n_images += n_desc_images;
      n_buffers += n_desc_buffers;
    }

  if (n_sets > self->pool_n_sets ||
      n_images > self->pool_n_images ||
      n_buffers > self->pool_n_buffers)
    {
      if (self->vk_descriptor_pool != VK_NULL_HANDLE)
        {
          vkDestroyDescriptorPool (vk_device,
                                   self->vk_descriptor_pool,
                                   NULL);
          self->vk_descriptor_pool = VK_NULL_HANDLE;
        }
      if (n_sets > self->pool_n_sets)
        self->pool_n_sets = 4 << g_bit_nth_msf (n_sets - 1, -1);
      if (n_images > self->pool_n_images)
        self->pool_n_images = 2 << g_bit_nth_msf (n_images - 1, -1);
      if (n_buffers > self->pool_n_buffers)
        self->pool_n_buffers = 4 << g_bit_nth_msf (n_buffers - 1, -1);
    }

  if (self->vk_descriptor_pool == VK_NULL_HANDLE)
    {
      GSK_VK_CHECK (vkCreateDescriptorPool, vk_device,
                                            &(VkDescriptorPoolCreateInfo) {
                                                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                .flags = 0,
                                                .maxSets = self->pool_n_sets,
                                                .poolSizeCount = 2,
                                                .pPoolSizes = (VkDescriptorPoolSize[2]) {
                                                    {
                                                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                        .descriptorCount = self->pool_n_images,
                                                    },
                                                    {
                                                        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                        .descriptorCount = self->pool_n_buffers,
                                                    }
                                                }
                                            },
                                            NULL,
                                            &self->vk_descriptor_pool);
    }

  for (i = 0; i < gsk_descriptors_get_size (&self->descriptors); i++)
    {
      GskVulkanRealDescriptors *desc = gsk_descriptors_get (&self->descriptors, i);

      gsk_vulkan_real_descriptors_update_sets (desc, self->vk_descriptor_pool);
    }
}

static GskGpuDescriptors *
gsk_vulkan_frame_create_descriptors (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);

  if (gsk_vulkan_device_has_feature (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)), GDK_VULKAN_FEATURE_DESCRIPTOR_INDEXING))
    {
      GskVulkanRealDescriptors *parent;

      if (gsk_descriptors_get_size (&self->descriptors) > 0)
        {
          parent = gsk_descriptors_get (&self->descriptors, gsk_descriptors_get_size (&self->descriptors) - 1);
          if (gsk_vulkan_real_descriptors_is_full (parent))
            parent = NULL;
        }
      else
        parent = NULL;

      if (parent == NULL)
        {
          parent = gsk_vulkan_real_descriptors_new (self);
          gsk_descriptors_append (&self->descriptors, parent);
        }

      return GSK_GPU_DESCRIPTORS (gsk_vulkan_sub_descriptors_new (GSK_VULKAN_DESCRIPTORS (parent)));
    }
  else
    {
      GskVulkanRealDescriptors *desc;

      desc = gsk_vulkan_real_descriptors_new (self);
      gsk_descriptors_append (&self->descriptors, desc);

      return GSK_GPU_DESCRIPTORS (g_object_ref (desc));
    }
}

static GskGpuBuffer *
gsk_vulkan_frame_create_vertex_buffer (GskGpuFrame *frame,
                                       gsize        size)
{
  return gsk_vulkan_buffer_new_vertex (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)), size);
}

static GskGpuBuffer *
gsk_vulkan_frame_create_storage_buffer (GskGpuFrame *frame,
                                        gsize        size)
{
  return gsk_vulkan_buffer_new_storage (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)), size);
}

static void
gsk_vulkan_frame_submit (GskGpuFrame  *frame,
                         GskGpuBuffer *vertex_buffer,
                         GskGpuOp     *op)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  GskVulkanSemaphores semaphores;
  GskVulkanCommandState state;

  if (gsk_descriptors_get_size (&self->descriptors) == 0)
    gsk_descriptors_append (&self->descriptors, gsk_vulkan_real_descriptors_new (self));

  gsk_vulkan_frame_prepare_descriptors (self);

  GSK_VK_CHECK (vkBeginCommandBuffer, self->vk_command_buffer,
                                      &(VkCommandBufferBeginInfo) {
                                          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                      });

  if (vertex_buffer)
    vkCmdBindVertexBuffers (self->vk_command_buffer,
                            0,
                            1,
                            (VkBuffer[1]) {
                                gsk_vulkan_buffer_get_vk_buffer (GSK_VULKAN_BUFFER (vertex_buffer))
                            },
                            (VkDeviceSize[1]) { 0 });

  gsk_semaphores_init (&semaphores.wait_semaphores);
  gsk_pipeline_stages_init (&semaphores.wait_stages);
  gsk_semaphores_init (&semaphores.signal_semaphores);

  state.vk_command_buffer = self->vk_command_buffer;
  state.vk_render_pass = VK_NULL_HANDLE;
  state.vk_format = VK_FORMAT_UNDEFINED;
  state.blend = GSK_GPU_BLEND_OVER; /* should we have a BLEND_NONE? */
  state.desc = GSK_VULKAN_DESCRIPTORS (gsk_descriptors_get (&self->descriptors, 0));
  state.semaphores = &semaphores;

  gsk_vulkan_descriptors_bind (GSK_VULKAN_DESCRIPTORS (gsk_descriptors_get (&self->descriptors, 0)),
                               NULL,
                               state.vk_command_buffer);

  while (op)
    {
      op = gsk_gpu_op_vk_command (op, frame, &state);
    }

  GSK_VK_CHECK (vkEndCommandBuffer, self->vk_command_buffer);

  GSK_VK_CHECK (vkQueueSubmit, gsk_vulkan_device_get_vk_queue (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame))),
                               1,
                               &(VkSubmitInfo) {
                                  .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &self->vk_command_buffer,
                                  .pWaitSemaphores = gsk_semaphores_get_data (&semaphores.wait_semaphores),
                                  .pWaitDstStageMask = gsk_pipeline_stages_get_data (&semaphores.wait_stages),
                                  .waitSemaphoreCount = gsk_semaphores_get_size (&semaphores.wait_semaphores),
                                  .pSignalSemaphores = gsk_semaphores_get_data (&semaphores.signal_semaphores),
                                  .signalSemaphoreCount = gsk_semaphores_get_size (&semaphores.signal_semaphores),
                               },
                               self->vk_fence);

  gsk_semaphores_clear (&semaphores.wait_semaphores);
  gsk_pipeline_stages_clear (&semaphores.wait_stages);
  gsk_semaphores_clear (&semaphores.signal_semaphores);
}

static void
gsk_vulkan_frame_finalize (GObject *object)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (object);
  GskVulkanDevice *device;
  VkDevice vk_device;
  VkCommandPool vk_command_pool;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self)));
  vk_device = gsk_vulkan_device_get_vk_device (device);
  vk_command_pool = gsk_vulkan_device_get_vk_command_pool (device);

  if (self->vk_descriptor_pool != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool (vk_device,
                               self->vk_descriptor_pool,
                               NULL);
    }
  gsk_descriptors_clear (&self->descriptors);

  vkFreeCommandBuffers (vk_device,
                        vk_command_pool,
                        1, &self->vk_command_buffer);
  vkDestroyFence (vk_device,
                  self->vk_fence,
                  NULL);

  G_OBJECT_CLASS (gsk_vulkan_frame_parent_class)->finalize (object);
}

static void
gsk_vulkan_frame_class_init (GskVulkanFrameClass *klass)
{
  GskGpuFrameClass *gpu_frame_class = GSK_GPU_FRAME_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_frame_class->is_busy = gsk_vulkan_frame_is_busy;
  gpu_frame_class->wait = gsk_vulkan_frame_wait;
  gpu_frame_class->setup = gsk_vulkan_frame_setup;
  gpu_frame_class->cleanup = gsk_vulkan_frame_cleanup;
  gpu_frame_class->upload_texture = gsk_vulkan_frame_upload_texture;
  gpu_frame_class->create_descriptors = gsk_vulkan_frame_create_descriptors;
  gpu_frame_class->create_vertex_buffer = gsk_vulkan_frame_create_vertex_buffer;
  gpu_frame_class->create_storage_buffer = gsk_vulkan_frame_create_storage_buffer;
  gpu_frame_class->submit = gsk_vulkan_frame_submit;

  object_class->finalize = gsk_vulkan_frame_finalize;
}

static void
gsk_vulkan_frame_init (GskVulkanFrame *self)
{
  gsk_descriptors_init (&self->descriptors);

  self->pool_n_sets = 4;
  self->pool_n_images = 8;
  self->pool_n_buffers = 8;
}

VkFence
gsk_vulkan_frame_get_vk_fence (GskVulkanFrame *self)
{
  return self->vk_fence;
}

void
gsk_vulkan_semaphores_add_wait (GskVulkanSemaphores  *self,
                                VkSemaphore           semaphore,
                                VkPipelineStageFlags  stage)
{
  gsk_semaphores_append (&self->wait_semaphores, semaphore);
  gsk_pipeline_stages_append (&self->wait_stages, stage);
}

void
gsk_vulkan_semaphores_add_signal (GskVulkanSemaphores  *self,
                                  VkSemaphore           semaphore)
{
  gsk_semaphores_append (&self->signal_semaphores, semaphore);
}

