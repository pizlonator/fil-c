/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vn_feedback.h"

#include "vn_command_buffer.h"
#include "vn_device.h"
#include "vn_physical_device.h"
#include "vn_query_pool.h"
#include "vn_queue.h"

static uint32_t
vn_get_memory_type_index(const VkPhysicalDeviceMemoryProperties *mem_props,
                         uint32_t mem_type_bits,
                         VkMemoryPropertyFlags required_mem_flags)
{
   u_foreach_bit(mem_type_index, mem_type_bits)
   {
      assert(mem_type_index < mem_props->memoryTypeCount);
      if ((mem_props->memoryTypes[mem_type_index].propertyFlags &
           required_mem_flags) == required_mem_flags)
         return mem_type_index;
   }

   return UINT32_MAX;
}

VkResult
vn_feedback_buffer_create(struct vn_device *dev,
                          uint32_t size,
                          const VkAllocationCallbacks *alloc,
                          struct vn_feedback_buffer **out_fb_buf)
{
   const bool exclusive = dev->queue_family_count == 1;
   const VkPhysicalDeviceMemoryProperties *mem_props =
      &dev->physical_device->memory_properties;
   VkDevice dev_handle = vn_device_to_handle(dev);
   VkResult result;

   struct vn_feedback_buffer *fb_buf =
      vk_zalloc(alloc, sizeof(*fb_buf), VN_DEFAULT_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!fb_buf)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   /* use concurrent to avoid explicit queue family ownership transfer for
    * device created with queues from multiple queue families
    */
   const VkBufferCreateInfo buf_create_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      /* Feedback for fences and timeline semaphores will write to this buffer
       * as a DST when signalling. Timeline semaphore feedback will also read
       * from this buffer as a SRC to retrieve the counter value to signal.
       */
      .usage =
         VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .sharingMode =
         exclusive ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
      /* below favors the current venus protocol */
      .queueFamilyIndexCount = exclusive ? 0 : dev->queue_family_count,
      .pQueueFamilyIndices = exclusive ? NULL : dev->queue_families,
   };
   result = vn_CreateBuffer(dev_handle, &buf_create_info, alloc,
                            &fb_buf->buf_handle);
   if (result != VK_SUCCESS)
      goto out_free_feedback_buffer;

   struct vn_buffer *buf = vn_buffer_from_handle(fb_buf->buf_handle);
   const VkMemoryRequirements *mem_req =
      &buf->requirements.memory.memoryRequirements;
   const uint32_t mem_type_index =
      vn_get_memory_type_index(mem_props, mem_req->memoryTypeBits,
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   if (mem_type_index >= mem_props->memoryTypeCount) {
      result = VK_ERROR_INITIALIZATION_FAILED;
      goto out_destroy_buffer;
   }

   const VkMemoryAllocateInfo mem_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = mem_req->size,
      .memoryTypeIndex = mem_type_index,
   };
   result = vn_AllocateMemory(dev_handle, &mem_alloc_info, alloc,
                              &fb_buf->mem_handle);
   if (result != VK_SUCCESS)
      goto out_destroy_buffer;

   const VkBindBufferMemoryInfo bind_info = {
      .sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
      .buffer = fb_buf->buf_handle,
      .memory = fb_buf->mem_handle,
      .memoryOffset = 0,
   };
   result = vn_BindBufferMemory2(dev_handle, 1, &bind_info);
   if (result != VK_SUCCESS)
      goto out_free_memory;

   result = vn_MapMemory(dev_handle, fb_buf->mem_handle, 0, VK_WHOLE_SIZE, 0,
                         &fb_buf->data);
   if (result != VK_SUCCESS)
      goto out_free_memory;

   *out_fb_buf = fb_buf;

   return VK_SUCCESS;

out_free_memory:
   vn_FreeMemory(dev_handle, fb_buf->mem_handle, alloc);

out_destroy_buffer:
   vn_DestroyBuffer(dev_handle, fb_buf->buf_handle, alloc);

out_free_feedback_buffer:
   vk_free(alloc, fb_buf);

   return result;
}

void
vn_feedback_buffer_destroy(struct vn_device *dev,
                           struct vn_feedback_buffer *fb_buf,
                           const VkAllocationCallbacks *alloc)
{
   VkDevice dev_handle = vn_device_to_handle(dev);

   vn_UnmapMemory(dev_handle, fb_buf->mem_handle);
   vn_FreeMemory(dev_handle, fb_buf->mem_handle, alloc);
   vn_DestroyBuffer(dev_handle, fb_buf->buf_handle, alloc);
   vk_free(alloc, fb_buf);
}

static inline uint32_t
vn_get_feedback_buffer_alignment(struct vn_feedback_buffer *fb_buf)
{
   struct vn_buffer *buf = vn_buffer_from_handle(fb_buf->buf_handle);
   return buf->requirements.memory.memoryRequirements.alignment;
}

static VkResult
vn_feedback_pool_grow_locked(struct vn_feedback_pool *pool)
{
   VN_TRACE_FUNC();
   struct vn_feedback_buffer *fb_buf = NULL;
   VkResult result;

   result =
      vn_feedback_buffer_create(pool->dev, pool->size, pool->alloc, &fb_buf);
   if (result != VK_SUCCESS)
      return result;

   pool->used = 0;
   pool->alignment = vn_get_feedback_buffer_alignment(fb_buf);

   list_add(&fb_buf->head, &pool->fb_bufs);

   return VK_SUCCESS;
}

VkResult
vn_feedback_pool_init(struct vn_device *dev,
                      struct vn_feedback_pool *pool,
                      uint32_t size,
                      const VkAllocationCallbacks *alloc)
{
   simple_mtx_init(&pool->mutex, mtx_plain);

   pool->dev = dev;
   pool->alloc = alloc;
   pool->size = size;
   pool->used = size;
   pool->alignment = 1;
   list_inithead(&pool->fb_bufs);
   list_inithead(&pool->free_slots);

   return VK_SUCCESS;
}

void
vn_feedback_pool_fini(struct vn_feedback_pool *pool)
{
   list_for_each_entry_safe(struct vn_feedback_slot, slot, &pool->free_slots,
                            head)
      vk_free(pool->alloc, slot);

   list_for_each_entry_safe(struct vn_feedback_buffer, fb_buf, &pool->fb_bufs,
                            head)
      vn_feedback_buffer_destroy(pool->dev, fb_buf, pool->alloc);

   simple_mtx_destroy(&pool->mutex);
}

static struct vn_feedback_buffer *
vn_feedback_pool_alloc_locked(struct vn_feedback_pool *pool,
                              uint32_t size,
                              uint32_t *out_offset)
{
   /* Default values of pool->used and pool->alignment are used to trigger the
    * initial pool grow, and will be properly initialized after that.
    */
   if (unlikely(align(size, pool->alignment) > pool->size - pool->used)) {
      VkResult result = vn_feedback_pool_grow_locked(pool);
      if (result != VK_SUCCESS)
         return NULL;

      assert(align(size, pool->alignment) <= pool->size - pool->used);
   }

   *out_offset = pool->used;
   pool->used += align(size, pool->alignment);

   return list_first_entry(&pool->fb_bufs, struct vn_feedback_buffer, head);
}

struct vn_feedback_slot *
vn_feedback_pool_alloc(struct vn_feedback_pool *pool,
                       enum vn_feedback_type type)
{
   static const uint32_t slot_size = 8;
   struct vn_feedback_buffer *fb_buf;
   uint32_t offset;
   struct vn_feedback_slot *slot;

   simple_mtx_lock(&pool->mutex);
   if (!list_is_empty(&pool->free_slots)) {
      slot =
         list_first_entry(&pool->free_slots, struct vn_feedback_slot, head);
      list_del(&slot->head);
      simple_mtx_unlock(&pool->mutex);

      slot->type = type;
      return slot;
   }

   slot = vk_alloc(pool->alloc, sizeof(*slot), VN_DEFAULT_ALIGN,
                   VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!slot) {
      simple_mtx_unlock(&pool->mutex);
      return NULL;
   }

   fb_buf = vn_feedback_pool_alloc_locked(pool, slot_size, &offset);
   simple_mtx_unlock(&pool->mutex);

   if (!fb_buf) {
      vk_free(pool->alloc, slot);
      return NULL;
   }

   slot->type = type;
   slot->offset = offset;
   slot->buf_handle = fb_buf->buf_handle;
   slot->data = fb_buf->data + offset;

   return slot;
}

void
vn_feedback_pool_free(struct vn_feedback_pool *pool,
                      struct vn_feedback_slot *slot)
{
   simple_mtx_lock(&pool->mutex);
   list_add(&slot->head, &pool->free_slots);
   simple_mtx_unlock(&pool->mutex);
}

static inline bool
mask_is_32bit(uint64_t x)
{
   return (x & 0xffffffff00000000) == 0;
}

static void
vn_build_buffer_memory_barrier(const VkDependencyInfo *dep_info,
                               VkBufferMemoryBarrier *barrier1,
                               VkPipelineStageFlags *src_stage_mask,
                               VkPipelineStageFlags *dst_stage_mask)
{

   assert(dep_info->pNext == NULL);
   assert(dep_info->memoryBarrierCount == 0);
   assert(dep_info->bufferMemoryBarrierCount == 1);
   assert(dep_info->imageMemoryBarrierCount == 0);

   const VkBufferMemoryBarrier2 *barrier2 =
      &dep_info->pBufferMemoryBarriers[0];
   assert(barrier2->pNext == NULL);
   assert(mask_is_32bit(barrier2->srcStageMask));
   assert(mask_is_32bit(barrier2->srcAccessMask));
   assert(mask_is_32bit(barrier2->dstStageMask));
   assert(mask_is_32bit(barrier2->dstAccessMask));

   *barrier1 = (VkBufferMemoryBarrier){
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .pNext = NULL,
      .srcAccessMask = barrier2->srcAccessMask,
      .dstAccessMask = barrier2->dstAccessMask,
      .srcQueueFamilyIndex = barrier2->srcQueueFamilyIndex,
      .dstQueueFamilyIndex = barrier2->dstQueueFamilyIndex,
      .buffer = barrier2->buffer,
      .offset = barrier2->offset,
      .size = barrier2->size,
   };

   *src_stage_mask = barrier2->srcStageMask;
   *dst_stage_mask = barrier2->dstStageMask;
}

static void
vn_cmd_buffer_memory_barrier(VkCommandBuffer cmd_handle,
                             const VkDependencyInfo *dep_info,
                             bool sync2)
{
   if (sync2)
      vn_CmdPipelineBarrier2(cmd_handle, dep_info);
   else {
      VkBufferMemoryBarrier barrier1;
      VkPipelineStageFlags src_stage_mask;
      VkPipelineStageFlags dst_stage_mask;

      vn_build_buffer_memory_barrier(dep_info, &barrier1, &src_stage_mask,
                                     &dst_stage_mask);
      vn_CmdPipelineBarrier(cmd_handle, src_stage_mask, dst_stage_mask,
                            dep_info->dependencyFlags, 0, NULL, 1, &barrier1,
                            0, NULL);
   }
}

void
vn_event_feedback_cmd_record(VkCommandBuffer cmd_handle,
                             VkEvent ev_handle,
                             VkPipelineStageFlags2 src_stage_mask,
                             VkResult status,
                             bool sync2)
{
   /* For vkCmdSetEvent and vkCmdResetEvent feedback interception.
    *
    * The injection point is after the event call to avoid introducing
    * unexpected src stage waiting for VK_PIPELINE_STAGE_HOST_BIT and
    * VK_PIPELINE_STAGE_TRANSFER_BIT if they are not already being waited by
    * vkCmdSetEvent or vkCmdResetEvent. On the other hand, the delay in the
    * feedback signal is acceptable for the nature of VkEvent, and the event
    * feedback cmds lifecycle is guarded by the intercepted command buffer.
    */
   struct vn_event *ev = vn_event_from_handle(ev_handle);
   struct vn_feedback_slot *slot = ev->feedback_slot;

   if (!slot)
      return;

   STATIC_ASSERT(sizeof(*slot->status) == 4);

   const VkDependencyInfo dep_before = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .dependencyFlags = 0,
      .bufferMemoryBarrierCount = 1,
      .pBufferMemoryBarriers =
         (VkBufferMemoryBarrier2[]){
            {
               .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
               .srcStageMask = src_stage_mask | VK_PIPELINE_STAGE_HOST_BIT |
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
               .srcAccessMask =
                  VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
               .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
               .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
               .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
               .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
               .buffer = slot->buf_handle,
               .offset = slot->offset,
               .size = 4,
            },
         },
   };
   vn_cmd_buffer_memory_barrier(cmd_handle, &dep_before, sync2);

   vn_CmdFillBuffer(cmd_handle, slot->buf_handle, slot->offset, 4, status);

   const VkDependencyInfo dep_after = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .dependencyFlags = 0,
      .bufferMemoryBarrierCount = 1,
      .pBufferMemoryBarriers =
         (VkBufferMemoryBarrier2[]){
            {
               .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
               .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
               .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
               .dstStageMask = VK_PIPELINE_STAGE_HOST_BIT,
               .dstAccessMask =
                  VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT,
               .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
               .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
               .buffer = slot->buf_handle,
               .offset = slot->offset,
               .size = 4,
            },
         },
   };
   vn_cmd_buffer_memory_barrier(cmd_handle, &dep_after, sync2);
}

static inline void
vn_feedback_cmd_record_flush_barrier(VkCommandBuffer cmd_handle,
                                     VkBuffer buffer,
                                     VkDeviceSize offset,
                                     VkDeviceSize size)
{
   const VkBufferMemoryBarrier buf_flush_barrier = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .pNext = NULL,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = buffer,
      .offset = offset,
      .size = size,
   };
   vn_CmdPipelineBarrier(cmd_handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1,
                         &buf_flush_barrier, 0, NULL);
}

static VkResult
vn_feedback_cmd_record(VkCommandBuffer cmd_handle,
                       struct vn_feedback_slot *dst_slot,
                       struct vn_feedback_slot *src_slot)
{
   STATIC_ASSERT(sizeof(*dst_slot->status) == 4);
   STATIC_ASSERT(sizeof(*dst_slot->counter) == 8);
   STATIC_ASSERT(sizeof(*src_slot->counter) == 8);

   /* slot size is 8 bytes for timeline semaphore and 4 bytes fence.
    * src slot is non-null for timeline semaphore.
    */
   const VkDeviceSize buf_size = src_slot ? 8 : 4;

   static const VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = NULL,
      .flags = 0,
      .pInheritanceInfo = NULL,
   };
   VkResult result = vn_BeginCommandBuffer(cmd_handle, &begin_info);
   if (result != VK_SUCCESS)
      return result;

   static const VkMemoryBarrier mem_barrier_before = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      .pNext = NULL,
      /* make pending writes available to stay close to signal op */
      .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
      /* no need to make all memory visible for feedback update */
      .dstAccessMask = 0,
   };

   const VkBufferMemoryBarrier buf_barrier_before = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .pNext = NULL,
      /* slot memory has been made available via mem_barrier_before */
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = dst_slot->buf_handle,
      .offset = dst_slot->offset,
      .size = buf_size,
   };

   /* host writes for src_slots should implicitly be made visible upon
    * QueueSubmit call */
   vn_CmdPipelineBarrier(cmd_handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1,
                         &mem_barrier_before, 1, &buf_barrier_before, 0,
                         NULL);

   /* If passed a src_slot, timeline semaphore feedback records a
    * cmd to copy the counter value from the src slot to the dst slot.
    * If src_slot is NULL, then fence feedback records a cmd to fill
    * the dst slot with VK_SUCCESS.
    */
   if (src_slot) {
      assert(src_slot->type == VN_FEEDBACK_TYPE_SEMAPHORE);
      assert(dst_slot->type == VN_FEEDBACK_TYPE_SEMAPHORE);

      const VkBufferCopy buffer_copy = {
         .srcOffset = src_slot->offset,
         .dstOffset = dst_slot->offset,
         .size = buf_size,
      };
      vn_CmdCopyBuffer(cmd_handle, src_slot->buf_handle, dst_slot->buf_handle,
                       1, &buffer_copy);
   } else {
      assert(dst_slot->type == VN_FEEDBACK_TYPE_FENCE);

      vn_CmdFillBuffer(cmd_handle, dst_slot->buf_handle, dst_slot->offset,
                       buf_size, VK_SUCCESS);
   }

   vn_feedback_cmd_record_flush_barrier(cmd_handle, dst_slot->buf_handle,
                                        dst_slot->offset, buf_size);

   return vn_EndCommandBuffer(cmd_handle);
}

struct vn_semaphore_feedback_cmd *
vn_semaphore_feedback_cmd_alloc(struct vn_device *dev,
                                struct vn_feedback_slot *dst_slot)
{
   const VkAllocationCallbacks *alloc = &dev->base.base.alloc;
   struct vn_semaphore_feedback_cmd *sfb_cmd;
   VkCommandBuffer *cmd_handles;

   VK_MULTIALLOC(ma);
   vk_multialloc_add(&ma, &sfb_cmd, __typeof__(*sfb_cmd), 1);
   vk_multialloc_add(&ma, &cmd_handles, __typeof__(*cmd_handles),
                     dev->queue_family_count);
   if (!vk_multialloc_zalloc(&ma, alloc, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
      return NULL;

   struct vn_feedback_slot *src_slot =
      vn_feedback_pool_alloc(&dev->feedback_pool, VN_FEEDBACK_TYPE_SEMAPHORE);
   if (!src_slot) {
      vk_free(alloc, sfb_cmd);
      return NULL;
   }

   for (uint32_t i = 0; i < dev->queue_family_count; i++) {
      VkDevice dev_handle = vn_device_to_handle(dev);
      VkResult result =
         vn_feedback_cmd_alloc(dev_handle, &dev->fb_cmd_pools[i], dst_slot,
                               src_slot, &cmd_handles[i]);
      if (result != VK_SUCCESS) {
         for (uint32_t j = 0; j < i; j++) {
            vn_feedback_cmd_free(dev_handle, &dev->fb_cmd_pools[j],
                                 cmd_handles[j]);
         }

         vn_feedback_pool_free(&dev->feedback_pool, src_slot);
         vk_free(alloc, sfb_cmd);
         return NULL;
      }
   }

   sfb_cmd->cmd_handles = cmd_handles;
   sfb_cmd->src_slot = src_slot;
   return sfb_cmd;
}

void
vn_semaphore_feedback_cmd_free(struct vn_device *dev,
                               struct vn_semaphore_feedback_cmd *sfb_cmd)
{
   const VkAllocationCallbacks *alloc = &dev->base.base.alloc;

   for (uint32_t i = 0; i < dev->queue_family_count; i++) {
      vn_feedback_cmd_free(vn_device_to_handle(dev), &dev->fb_cmd_pools[i],
                           sfb_cmd->cmd_handles[i]);
   }

   vn_feedback_pool_free(&dev->feedback_pool, sfb_cmd->src_slot);
   vk_free(alloc, sfb_cmd);
}

static void
vn_query_feedback_cmd_record_internal(VkCommandBuffer cmd_handle,
                                      VkQueryPool pool_handle,
                                      uint32_t query,
                                      uint32_t count,
                                      bool copy)
{
   struct vn_query_pool *pool = vn_query_pool_from_handle(pool_handle);
   if (!pool->fb_buf)
      return;

   /* Results are always 64 bit and include availability bit (also 64 bit) */
   const VkDeviceSize slot_size = (pool->result_array_size * 8) + 8;
   const VkDeviceSize offset = slot_size * query;
   const VkDeviceSize buf_size = slot_size * count;

   /* The first synchronization scope of vkCmdCopyQueryPoolResults does not
    * include the query feedback buffer. Insert a barrier to ensure ordering
    * against feedback buffer fill cmd injected in vkCmdResetQueryPool.
    *
    * The second synchronization scope of vkCmdResetQueryPool does not include
    * the query feedback buffer. Insert a barrer to ensure ordering against
    * prior cmds referencing the queries.
    *
    * For srcAccessMask, VK_ACCESS_TRANSFER_WRITE_BIT is sufficient since the
    * gpu cache invalidation for feedback buffer fill in vkResetQueryPool is
    * done implicitly via queue submission.
    */
   const VkPipelineStageFlags src_stage_mask =
      copy ? VK_PIPELINE_STAGE_TRANSFER_BIT
           : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

   const VkBufferMemoryBarrier buf_barrier_before = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .pNext = NULL,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = pool->fb_buf->buf_handle,
      .offset = offset,
      .size = buf_size,
   };
   vn_CmdPipelineBarrier(cmd_handle, src_stage_mask,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1,
                         &buf_barrier_before, 0, NULL);

   if (copy) {
      /* Per spec: "The first synchronization scope includes all commands
       * which reference the queries in queryPool indicated by query that
       * occur earlier in submission order. If flags does not include
       * VK_QUERY_RESULT_WAIT_BIT, vkCmdEndQueryIndexedEXT,
       * vkCmdWriteTimestamp2, vkCmdEndQuery, and vkCmdWriteTimestamp are
       * excluded from this scope."
       *
       * Set VK_QUERY_RESULT_WAIT_BIT to ensure ordering after
       * vkCmdEndQuery or vkCmdWriteTimestamp makes the query available.
       *
       * Set VK_QUERY_RESULT_64_BIT as we can convert it to 32 bit if app
       * requested that.
       *
       * Per spec: "vkCmdCopyQueryPoolResults is considered to be a transfer
       * operation, and its writes to buffer memory must be synchronized using
       * VK_PIPELINE_STAGE_TRANSFER_BIT and VK_ACCESS_TRANSFER_WRITE_BIT
       * before using the results."
       *
       * So we can reuse the flush barrier after this copy cmd.
       */
      vn_CmdCopyQueryPoolResults(cmd_handle, pool_handle, query, count,
                                 pool->fb_buf->buf_handle, offset, slot_size,
                                 VK_QUERY_RESULT_WITH_AVAILABILITY_BIT |
                                    VK_QUERY_RESULT_64_BIT |
                                    VK_QUERY_RESULT_WAIT_BIT);
   } else {
      vn_CmdFillBuffer(cmd_handle, pool->fb_buf->buf_handle, offset, buf_size,
                       0);
   }

   vn_feedback_cmd_record_flush_barrier(cmd_handle, pool->fb_buf->buf_handle,
                                        offset, buf_size);
}

static VkResult
vn_query_feedback_cmd_record(VkDevice dev_handle,
                             struct list_head *query_records,
                             struct vn_query_feedback_cmd *qfb_cmd)
{
   assert(!list_is_empty(query_records));

   static const VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
   };
   VkResult result = vn_BeginCommandBuffer(qfb_cmd->cmd_handle, &begin_info);
   if (result != VK_SUCCESS)
      return result;

   list_for_each_entry_safe(struct vn_cmd_query_record, record, query_records,
                            head) {
      vn_query_feedback_cmd_record_internal(
         qfb_cmd->cmd_handle, vn_query_pool_to_handle(record->query_pool),
         record->query, record->query_count, record->copy);
   }

   return vn_EndCommandBuffer(qfb_cmd->cmd_handle);
}

VkResult
vn_query_feedback_cmd_alloc(VkDevice dev_handle,
                            struct vn_feedback_cmd_pool *fb_cmd_pool,
                            struct list_head *query_records,
                            struct vn_query_feedback_cmd **out_qfb_cmd)
{
   struct vn_query_feedback_cmd *qfb_cmd;
   VkResult result;

   simple_mtx_lock(&fb_cmd_pool->mutex);

   if (list_is_empty(&fb_cmd_pool->free_qfb_cmds)) {
      struct vn_command_pool *cmd_pool =
         vn_command_pool_from_handle(fb_cmd_pool->pool_handle);

      qfb_cmd = vk_alloc(&cmd_pool->allocator, sizeof(*qfb_cmd),
                         VN_DEFAULT_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      if (!qfb_cmd) {
         result = VK_ERROR_OUT_OF_HOST_MEMORY;
         goto out_unlock;
      }

      const VkCommandBufferAllocateInfo info = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
         .commandPool = fb_cmd_pool->pool_handle,
         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
         .commandBufferCount = 1,
      };
      VkCommandBuffer qfb_cmd_handle;
      result = vn_AllocateCommandBuffers(dev_handle, &info, &qfb_cmd_handle);
      if (result != VK_SUCCESS) {
         vk_free(&cmd_pool->allocator, qfb_cmd);
         goto out_unlock;
      }

      qfb_cmd->fb_cmd_pool = fb_cmd_pool;
      qfb_cmd->cmd_handle = qfb_cmd_handle;
   } else {
      qfb_cmd = list_first_entry(&fb_cmd_pool->free_qfb_cmds,
                                 struct vn_query_feedback_cmd, head);
      list_del(&qfb_cmd->head);
      vn_ResetCommandBuffer(qfb_cmd->cmd_handle, 0);
   }

   result = vn_query_feedback_cmd_record(dev_handle, query_records, qfb_cmd);
   if (result != VK_SUCCESS) {
      list_add(&qfb_cmd->head, &fb_cmd_pool->free_qfb_cmds);
      goto out_unlock;
   }

   *out_qfb_cmd = qfb_cmd;

out_unlock:
   simple_mtx_unlock(&fb_cmd_pool->mutex);

   return result;
}

void
vn_query_feedback_cmd_free(struct vn_query_feedback_cmd *qfb_cmd)
{
   simple_mtx_lock(&qfb_cmd->fb_cmd_pool->mutex);
   list_add(&qfb_cmd->head, &qfb_cmd->fb_cmd_pool->free_qfb_cmds);
   simple_mtx_unlock(&qfb_cmd->fb_cmd_pool->mutex);
}

VkResult
vn_feedback_cmd_alloc(VkDevice dev_handle,
                      struct vn_feedback_cmd_pool *fb_cmd_pool,
                      struct vn_feedback_slot *dst_slot,
                      struct vn_feedback_slot *src_slot,
                      VkCommandBuffer *out_cmd_handle)
{
   VkCommandPool cmd_pool_handle = fb_cmd_pool->pool_handle;
   const VkCommandBufferAllocateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = NULL,
      .commandPool = cmd_pool_handle,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
   };
   VkCommandBuffer cmd_handle;
   VkResult result;

   simple_mtx_lock(&fb_cmd_pool->mutex);
   result = vn_AllocateCommandBuffers(dev_handle, &info, &cmd_handle);
   if (result != VK_SUCCESS)
      goto out_unlock;

   result = vn_feedback_cmd_record(cmd_handle, dst_slot, src_slot);
   if (result != VK_SUCCESS) {
      vn_FreeCommandBuffers(dev_handle, cmd_pool_handle, 1, &cmd_handle);
      goto out_unlock;
   }

   *out_cmd_handle = cmd_handle;

out_unlock:
   simple_mtx_unlock(&fb_cmd_pool->mutex);

   return result;
}

void
vn_feedback_cmd_free(VkDevice dev_handle,
                     struct vn_feedback_cmd_pool *fb_cmd_pool,
                     VkCommandBuffer cmd_handle)
{
   simple_mtx_lock(&fb_cmd_pool->mutex);
   vn_FreeCommandBuffers(dev_handle, fb_cmd_pool->pool_handle, 1,
                         &cmd_handle);
   simple_mtx_unlock(&fb_cmd_pool->mutex);
}

VkResult
vn_feedback_cmd_pools_init(struct vn_device *dev)
{
   const VkAllocationCallbacks *alloc = &dev->base.base.alloc;
   VkDevice dev_handle = vn_device_to_handle(dev);
   struct vn_feedback_cmd_pool *fb_cmd_pools;
   VkCommandPoolCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
   };

   if (VN_PERF(NO_FENCE_FEEDBACK) && VN_PERF(NO_SEMAPHORE_FEEDBACK) &&
       VN_PERF(NO_QUERY_FEEDBACK))
      return VK_SUCCESS;

   assert(dev->queue_family_count);

   fb_cmd_pools =
      vk_zalloc(alloc, sizeof(*fb_cmd_pools) * dev->queue_family_count,
                VN_DEFAULT_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   if (!fb_cmd_pools)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   for (uint32_t i = 0; i < dev->queue_family_count; i++) {
      VkResult result;

      info.queueFamilyIndex = dev->queue_families[i];
      result = vn_CreateCommandPool(dev_handle, &info, alloc,
                                    &fb_cmd_pools[i].pool_handle);
      if (result != VK_SUCCESS) {
         for (uint32_t j = 0; j < i; j++) {
            vn_DestroyCommandPool(dev_handle, fb_cmd_pools[j].pool_handle,
                                  alloc);
            simple_mtx_destroy(&fb_cmd_pools[j].mutex);
         }

         vk_free(alloc, fb_cmd_pools);
         return result;
      }

      simple_mtx_init(&fb_cmd_pools[i].mutex, mtx_plain);
      list_inithead(&fb_cmd_pools[i].free_qfb_cmds);
   }

   dev->fb_cmd_pools = fb_cmd_pools;

   return VK_SUCCESS;
}

void
vn_feedback_cmd_pools_fini(struct vn_device *dev)
{
   const VkAllocationCallbacks *alloc = &dev->base.base.alloc;
   VkDevice dev_handle = vn_device_to_handle(dev);

   if (!dev->fb_cmd_pools)
      return;

   for (uint32_t i = 0; i < dev->queue_family_count; i++) {
      list_for_each_entry_safe(struct vn_query_feedback_cmd, feedback_cmd,
                               &dev->fb_cmd_pools[i].free_qfb_cmds, head)
         vk_free(alloc, feedback_cmd);

      vn_DestroyCommandPool(dev_handle, dev->fb_cmd_pools[i].pool_handle,
                            alloc);
      simple_mtx_destroy(&dev->fb_cmd_pools[i].mutex);
   }

   vk_free(alloc, dev->fb_cmd_pools);
}
