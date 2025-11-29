#pragma once

#include <gdk/gdk.h>

#include <vulkan/vulkan.h>

G_BEGIN_DECLS

#define GSK_VULKAN_MEMORY_MAPPABLE (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | \
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)

typedef struct _GskVulkanAllocator GskVulkanAllocator;
typedef struct _GskVulkanAllocation GskVulkanAllocation;
typedef struct _GskVulkanMemory GskVulkanMemory;

struct _GskVulkanAllocation
{
  VkDeviceMemory vk_memory;
  guchar *map;
  VkDeviceSize offset;
  VkDeviceSize size;
  VkMemoryPropertyFlags memory_flags;
};

struct _GskVulkanAllocator
{
  int ref_count;

  void                  (* free_allocator)                              (GskVulkanAllocator             *allocator);

  void                  (* alloc)                                       (GskVulkanAllocator             *allocator,
                                                                         VkDeviceSize                    size,
                                                                         VkDeviceSize                    alignment,
                                                                         GskVulkanAllocation            *out_alloc);
  void                  (* free)                                        (GskVulkanAllocator             *allocator,
                                                                         GskVulkanAllocation            *alloc);
};

static inline void      gsk_vulkan_alloc                                (GskVulkanAllocator             *allocator,
                                                                         VkDeviceSize                    size,
                                                                         VkDeviceSize                    alignment,
                                                                         GskVulkanAllocation            *out_alloc);
static inline void      gsk_vulkan_free                                 (GskVulkanAllocator             *allocator,
                                                                         GskVulkanAllocation            *alloc);

static inline GskVulkanAllocator *
                        gsk_vulkan_allocator_ref                        (GskVulkanAllocator             *allocator);
static inline void      gsk_vulkan_allocator_unref                      (GskVulkanAllocator             *allocator);

GskVulkanAllocator *    gsk_vulkan_direct_allocator_new                 (VkDevice                        device,
                                                                         uint32_t                        vk_type_index,
                                                                         const VkMemoryType             *vk_type);
GskVulkanAllocator *    gsk_vulkan_buddy_allocator_new                  (GskVulkanAllocator             *allocator,
                                                                         gsize                           block_size);
GskVulkanAllocator *    gsk_vulkan_stats_allocator_new                  (GskVulkanAllocator             *allocator);
GskVulkanAllocator *    gsk_vulkan_external_allocator_new               (VkDevice                        device);

static inline void
gsk_vulkan_alloc (GskVulkanAllocator  *allocator,
                  VkDeviceSize         size,
                  VkDeviceSize         alignment,
                  GskVulkanAllocation *out_alloc)
{
  allocator->alloc (allocator, size, alignment, out_alloc);
}

static inline void
gsk_vulkan_free (GskVulkanAllocator  *allocator,
                 GskVulkanAllocation *alloc)
{
  allocator->free (allocator, alloc);
}

static inline GskVulkanAllocator *
gsk_vulkan_allocator_ref (GskVulkanAllocator *self)
{
  self->ref_count++;
  return self;
}

static inline void
gsk_vulkan_allocator_unref (GskVulkanAllocator *self)
{
  self->ref_count--;
  if (self->ref_count > 0)
    return;

  self->free_allocator (self);
}

G_END_DECLS

