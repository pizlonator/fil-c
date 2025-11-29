#include "config.h"

#include "gskvulkanmemoryprivate.h"

/* for GSK_VK_CHECK */
#include "gskvulkandeviceprivate.h"

/* {{{ direct allocator ***/

typedef struct _GskVulkanDirectAllocator GskVulkanDirectAllocator;

struct _GskVulkanDirectAllocator
{
  GskVulkanAllocator allocator_class;

  VkDevice device; /* no reference held */
  uint32_t vk_memory_type_index;
  VkMemoryType vk_memory_type;
};

static void
gsk_vulkan_direct_allocator_free_allocator (GskVulkanAllocator *allocator)
{
  g_free (allocator);
}

static void
gsk_vulkan_direct_allocator_alloc (GskVulkanAllocator  *allocator,
                                   VkDeviceSize         size,
                                   VkDeviceSize         alignment,
                                   GskVulkanAllocation *alloc)
{
  GskVulkanDirectAllocator *self = (GskVulkanDirectAllocator *) allocator;

  GSK_VK_CHECK (vkAllocateMemory, self->device,
                                  &(VkMemoryAllocateInfo) {
                                      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                      .allocationSize = size,
                                      .memoryTypeIndex = self->vk_memory_type_index
                                  },
                                  NULL,
                                  &alloc->vk_memory);

  if ((self->vk_memory_type.propertyFlags & GSK_VULKAN_MEMORY_MAPPABLE) == GSK_VULKAN_MEMORY_MAPPABLE)
    {
        GSK_VK_CHECK (vkMapMemory, self->device,
                                   alloc->vk_memory,
                                   0,
                                   size,
                                   0,
                                   (void **) &alloc->map);
    }
  else
    {
      alloc->map = NULL;
    }

  alloc->offset = 0;
  alloc->size = size;
  alloc->memory_flags = self->vk_memory_type.propertyFlags;
}

static void
gsk_vulkan_direct_allocator_free (GskVulkanAllocator  *allocator,
                                  GskVulkanAllocation *alloc)
{
  GskVulkanDirectAllocator *self = (GskVulkanDirectAllocator *) allocator;

  if (alloc->map)
    vkUnmapMemory (self->device, alloc->vk_memory);

  vkFreeMemory (self->device,
                alloc->vk_memory,
                NULL);
}

GskVulkanAllocator *
gsk_vulkan_direct_allocator_new (VkDevice            device,
                                 uint32_t            vk_type_index,
                                 const VkMemoryType *vk_type)
{
  GskVulkanDirectAllocator *self;

  self = g_new0 (GskVulkanDirectAllocator, 1);
  self->allocator_class.ref_count = 1;
  self->allocator_class.free_allocator = gsk_vulkan_direct_allocator_free_allocator;
  self->allocator_class.alloc = gsk_vulkan_direct_allocator_alloc;
  self->allocator_class.free = gsk_vulkan_direct_allocator_free;
  self->device = device;
  self->vk_memory_type_index = vk_type_index;
  self->vk_memory_type = *vk_type;

  return (GskVulkanAllocator *) self;
}

/* }}} */
/* {{{ buddy allocator ***/

#define GDK_ARRAY_NAME gsk_vulkan_allocation_list
#define GDK_ARRAY_TYPE_NAME GskVulkanAllocationList
#define GDK_ARRAY_ELEMENT_TYPE GskVulkanAllocation
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 4
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define N_SUBDIVISIONS 10

typedef struct _GskVulkanBuddyAllocator GskVulkanBuddyAllocator;

struct _GskVulkanBuddyAllocator
{
  GskVulkanAllocator allocator_class;

  GskVulkanAllocator *allocator;

  gsize block_size_slot;

  GskVulkanAllocation cache;
  GskVulkanAllocationList free_lists[N_SUBDIVISIONS];
};

static void
gsk_vulkan_buddy_allocator_free_allocator (GskVulkanAllocator *allocator)
{
  GskVulkanBuddyAllocator *self = (GskVulkanBuddyAllocator *) allocator;
  gsize i;

  if (self->cache.vk_memory)
    gsk_vulkan_free (self->allocator, &self->cache);

  for (i = 0; i < N_SUBDIVISIONS; i++)
    {
      gsk_vulkan_allocation_list_clear (&self->free_lists[i]);
    }

  gsk_vulkan_allocator_unref (self->allocator);

  g_free (self);
}

/* must not be 0:
 * gets exponent for next power of 2 that's >= num.
 * So num=1234 gets 11, because 2048 = 2^11 */
static gsize
find_slot (gsize num)
{
  return g_bit_storage (num - 1);
}

static void
gsk_vulkan_buddy_allocator_alloc (GskVulkanAllocator  *allocator,
                                  VkDeviceSize         size,
                                  VkDeviceSize         align,
                                  GskVulkanAllocation *alloc)
{
  GskVulkanBuddyAllocator *self = (GskVulkanBuddyAllocator *) allocator;
  gsize slot;
  int i;

  size = MAX (size, align);

  slot = find_slot (size);
  if (slot >= self->block_size_slot)
    {
      gsk_vulkan_alloc (self->allocator, size, align, alloc);
      return;
    }

  slot = MIN (self->block_size_slot - slot, N_SUBDIVISIONS) - 1;
  for (i = slot; i >= 0; i--)
    {
      if (gsk_vulkan_allocation_list_get_size (&self->free_lists[i]) > 0)
        break;
    }
  if (i < 0)
    {
      if (self->cache.vk_memory)
        {
          *alloc = self->cache;
          self->cache.vk_memory = VK_NULL_HANDLE;
        }
      else
        {
          /* We force alignment to our size, so that we can use offset
           * to find the buddy allocation.
           */
          gsk_vulkan_alloc (self->allocator, 1 << self->block_size_slot, 1 << self->block_size_slot, alloc);
        }
    }
  else
    {
      gsize n = gsk_vulkan_allocation_list_get_size (&self->free_lists[i]);
      *alloc = *gsk_vulkan_allocation_list_get (&self->free_lists[i], n - 1);
      gsk_vulkan_allocation_list_set_size (&self->free_lists[i], n - 1);
    }

  while (i != slot)
    {
      i++;
      alloc->size >>= 1;
      gsk_vulkan_allocation_list_append (&self->free_lists[i], alloc);
      alloc->offset += alloc->size;
      if (alloc->map)
        alloc->map += alloc->size;
    }

  g_assert (alloc->size >= size);
}

static void
gsk_vulkan_buddy_allocator_free (GskVulkanAllocator  *allocator,
                                 GskVulkanAllocation *alloc)
{
  GskVulkanBuddyAllocator *self = (GskVulkanBuddyAllocator *) allocator;
  gsize slot, i, n;

  slot = find_slot (alloc->size);
  if (slot >= self->block_size_slot)
    {
      gsk_vulkan_free (self->allocator, alloc);
      return;
    }

  slot = MIN (self->block_size_slot - slot, N_SUBDIVISIONS) - 1;
restart:
  n = gsk_vulkan_allocation_list_get_size (&self->free_lists[slot]);
  for (i = 0; i < n; i++)
    {
      GskVulkanAllocation *maybe_buddy = gsk_vulkan_allocation_list_index (&self->free_lists[slot], i);
      if (maybe_buddy->vk_memory == alloc->vk_memory &&
          maybe_buddy->offset == (alloc->offset ^ alloc->size))
        {
          if (i < n - 1)
            *maybe_buddy = *gsk_vulkan_allocation_list_get (&self->free_lists[slot], n - 1);
          gsk_vulkan_allocation_list_set_size (&self->free_lists[slot], n - 1);
          if (alloc->map && alloc->offset & alloc->size)
            alloc->map -= alloc->size;
          alloc->offset &= ~alloc->size;
          alloc->size <<= 1;
          if (slot == 0)
            {
              if (self->cache.vk_memory == VK_NULL_HANDLE)
                self->cache = *alloc;
              else
                gsk_vulkan_free (self->allocator, alloc);
              return;
            }
          else
            {
              slot--;
              /* no idea how to make this look good with loops */
              goto restart;
            }
        }
    }
  gsk_vulkan_allocation_list_append (&self->free_lists[slot], alloc);
}

GskVulkanAllocator *
gsk_vulkan_buddy_allocator_new (GskVulkanAllocator *allocator,
                                gsize               block_size)
{
  GskVulkanBuddyAllocator *self;
  gsize i;

  self = g_new0 (GskVulkanBuddyAllocator, 1);
  self->allocator_class.ref_count = 1;
  self->allocator_class.free_allocator = gsk_vulkan_buddy_allocator_free_allocator;
  self->allocator_class.alloc = gsk_vulkan_buddy_allocator_alloc;
  self->allocator_class.free = gsk_vulkan_buddy_allocator_free;
  self->allocator = allocator;
  self->block_size_slot = find_slot (block_size);

  for (i = 0; i < N_SUBDIVISIONS; i++)
    {
      gsk_vulkan_allocation_list_init (&self->free_lists[i]);
    }

  return (GskVulkanAllocator *) self;
}

/* }}} */
/* {{{ stats allocator ***/

typedef struct _GskVulkanStatsAllocator GskVulkanStatsAllocator;

struct _GskVulkanStatsAllocator
{
  GskVulkanAllocator allocator_class;

  GskVulkanAllocator *allocator;

  gsize n_alloc;
  gsize n_free;

  gsize n_bytes_requested;
  gsize n_bytes_allocated;
  gsize n_bytes_freed;
};

static void
gsk_vulkan_stats_allocator_dump_stats (GskVulkanStatsAllocator *self,
                                       const char              *reason)
{
  g_printerr ("%s\n", reason);
  g_printerr ("  %zu bytes requested in %zu allocations\n", self->n_bytes_requested, self->n_alloc);
  g_printerr ("  %zu bytes allocated (%.2f%% overhead)\n", self->n_bytes_allocated,
              (self->n_bytes_allocated - self->n_bytes_requested) * 100. / self->n_bytes_requested);
  g_printerr ("  %zu bytes freed in %zu frees\n", self->n_bytes_freed , self->n_free);
  g_printerr ("  %zu bytes remaining in %zu allocations\n",
              self->n_bytes_allocated - self->n_bytes_freed, self->n_alloc - self->n_free);
}

static void
gsk_vulkan_stats_allocator_free_allocator (GskVulkanAllocator *allocator)
{
  GskVulkanStatsAllocator *self = (GskVulkanStatsAllocator *) allocator;

  g_assert (self->n_alloc == self->n_free);
  g_assert (self->n_bytes_allocated == self->n_bytes_freed);

  gsk_vulkan_allocator_unref (self->allocator);

  g_free (self);
}

static void
gsk_vulkan_stats_allocator_alloc (GskVulkanAllocator  *allocator,
                                  VkDeviceSize         size,
                                  VkDeviceSize         align,
                                  GskVulkanAllocation *alloc)
{
  GskVulkanStatsAllocator *self = (GskVulkanStatsAllocator *) allocator;

  gsk_vulkan_alloc (self->allocator, size, align, alloc);

  self->n_alloc++;
  self->n_bytes_requested += size;
  self->n_bytes_allocated += alloc->size;

  gsk_vulkan_stats_allocator_dump_stats (self, "alloc()");
}

static void
gsk_vulkan_stats_allocator_free (GskVulkanAllocator  *allocator,
                                 GskVulkanAllocation *alloc)
{
  GskVulkanStatsAllocator *self = (GskVulkanStatsAllocator *) allocator;

  self->n_free++;
  self->n_bytes_freed += alloc->size;

  gsk_vulkan_free (self->allocator, alloc);

  gsk_vulkan_stats_allocator_dump_stats (self, "free()");
}

GskVulkanAllocator *
gsk_vulkan_stats_allocator_new (GskVulkanAllocator *allocator)
{
  GskVulkanStatsAllocator *self;

  self = g_new0 (GskVulkanStatsAllocator, 1);
  self->allocator_class.ref_count = 1;
  self->allocator_class.free_allocator = gsk_vulkan_stats_allocator_free_allocator;
  self->allocator_class.alloc = gsk_vulkan_stats_allocator_alloc;
  self->allocator_class.free = gsk_vulkan_stats_allocator_free;
  self->allocator = allocator;

  return (GskVulkanAllocator *) self;
}

/* }}} */
/* {{{ external allocator ***/

typedef struct _GskVulkanExteralAllocator GskVulkanExteralAllocator;

struct _GskVulkanExteralAllocator
{
  GskVulkanAllocator allocator_class;

  VkDevice device; /* no reference held */
};

static void
gsk_vulkan_external_allocator_free_allocator (GskVulkanAllocator *allocator)
{
  g_free (allocator);
}

static void
gsk_vulkan_external_allocator_alloc (GskVulkanAllocator  *allocator,
                                     VkDeviceSize         size,
                                     VkDeviceSize         alignment,
                                     GskVulkanAllocation *alloc)
{
  alloc->vk_memory = VK_NULL_HANDLE;
  alloc->map = NULL;
  alloc->offset = 0;
  alloc->size = size;
  alloc->memory_flags = 0;
}

static void
gsk_vulkan_external_allocator_free (GskVulkanAllocator  *allocator,
                                    GskVulkanAllocation *alloc)
{
  GskVulkanExteralAllocator *self = (GskVulkanExteralAllocator *) allocator;

  g_assert (alloc->map == NULL);

  if (alloc->vk_memory)
    vkFreeMemory (self->device,
                  alloc->vk_memory,
                  NULL);
}

/* The external allocator assumes you call alloc() and then set alloc->vk_memory
 * manually.
 * You can even usnet it before calling free() if you set it back to VK_NULL_HANDLE.
 */
GskVulkanAllocator *
gsk_vulkan_external_allocator_new (VkDevice device)
{
  GskVulkanExteralAllocator *self;

  self = g_new0 (GskVulkanExteralAllocator, 1);
  self->allocator_class.ref_count = 1;
  self->allocator_class.free_allocator = gsk_vulkan_external_allocator_free_allocator;
  self->allocator_class.alloc = gsk_vulkan_external_allocator_alloc;
  self->allocator_class.free = gsk_vulkan_external_allocator_free;
  self->device = device;

  return (GskVulkanAllocator *) self;
}

/* }}} */

