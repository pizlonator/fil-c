#include "config.h"

#include "gskvulkanbufferprivate.h"

#include "gskvulkandeviceprivate.h"
#include "gskvulkanmemoryprivate.h"

struct _GskVulkanBuffer
{
  GskGpuBuffer parent_instance;

  GskVulkanDevice *device;

  VkBuffer vk_buffer;

  GskVulkanAllocator *allocator;
  GskVulkanAllocation allocation;
};

G_DEFINE_TYPE (GskVulkanBuffer, gsk_vulkan_buffer, GSK_TYPE_GPU_BUFFER)

static void
gsk_vulkan_buffer_finalize (GObject *object)
{
  GskVulkanBuffer *self = GSK_VULKAN_BUFFER (object);

  vkDestroyBuffer (gsk_vulkan_device_get_vk_device (self->device),
                   self->vk_buffer,
                   NULL);

  gsk_vulkan_free (self->allocator, &self->allocation);
  gsk_vulkan_allocator_unref (self->allocator);

  g_object_unref (self->device);

  G_OBJECT_CLASS (gsk_vulkan_buffer_parent_class)->finalize (object);
}

static guchar *
gsk_vulkan_buffer_map (GskGpuBuffer *buffer)
{
  GskVulkanBuffer *self = GSK_VULKAN_BUFFER (buffer);

  return self->allocation.map;
}

static void
gsk_vulkan_buffer_unmap (GskGpuBuffer *buffer,
                         gsize         size)
{
}

static void
gsk_vulkan_buffer_class_init (GskVulkanBufferClass *klass)
{
  GskGpuBufferClass *buffer_class = GSK_GPU_BUFFER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  buffer_class->map = gsk_vulkan_buffer_map;
  buffer_class->unmap = gsk_vulkan_buffer_unmap;

  gobject_class->finalize = gsk_vulkan_buffer_finalize;
}

static void
gsk_vulkan_buffer_init (GskVulkanBuffer *self)
{
}

static GskGpuBuffer *
gsk_vulkan_buffer_new_internal (GskVulkanDevice   *device,
                                gsize              size,
                                VkBufferUsageFlags usage)
{
  VkMemoryRequirements requirements;
  GskVulkanBuffer *self;

  self = g_object_new (GSK_TYPE_VULKAN_BUFFER, NULL);

  self->device = g_object_ref (device);

  GSK_VK_CHECK (vkCreateBuffer, gsk_vulkan_device_get_vk_device (device),
                                &(VkBufferCreateInfo) {
                                    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                    .size = size,
                                    .flags = 0,
                                    .usage = usage,
                                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
                                },
                                NULL,
                                &self->vk_buffer);

  vkGetBufferMemoryRequirements (gsk_vulkan_device_get_vk_device (device),
                                 self->vk_buffer,
                                 &requirements);
  
  self->allocator = gsk_vulkan_device_find_allocator (device,
                                                      requirements.memoryTypeBits,
                                                      GSK_VULKAN_MEMORY_MAPPABLE,
                                                      GSK_VULKAN_MEMORY_MAPPABLE |
                                                      VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
  gsk_vulkan_alloc (self->allocator,
                    requirements.size,
                    requirements.alignment,
                    &self->allocation);

  gsk_gpu_buffer_setup (GSK_GPU_BUFFER (self), self->allocation.size);

  GSK_VK_CHECK (vkBindBufferMemory, gsk_vulkan_device_get_vk_device (device),
                                    self->vk_buffer,
                                    self->allocation.vk_memory,
                                    self->allocation.offset);

  return GSK_GPU_BUFFER (self);
}

GskGpuBuffer *
gsk_vulkan_buffer_new_vertex (GskVulkanDevice *device,
                              gsize            size)
{
  return gsk_vulkan_buffer_new_internal (device, size,
                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

GskGpuBuffer *
gsk_vulkan_buffer_new_storage (GskVulkanDevice *device,
                               gsize            size)
{
  return gsk_vulkan_buffer_new_internal (device, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

GskGpuBuffer *
gsk_vulkan_buffer_new_write (GskVulkanDevice *device,
                             gsize            size)
{
  return gsk_vulkan_buffer_new_internal (device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
}

GskGpuBuffer *
gsk_vulkan_buffer_new_read (GskVulkanDevice *device,
                            gsize            size)
{
  return gsk_vulkan_buffer_new_internal (device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

VkBuffer
gsk_vulkan_buffer_get_vk_buffer (GskVulkanBuffer *self)
{
  return self->vk_buffer;
}

