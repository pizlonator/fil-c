/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv and radv which are:
 * Copyright © 2015 Intel Corporation
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 */

#include "vn_queue.h"

#include "util/libsync.h"
#include "venus-protocol/vn_protocol_driver_event.h"
#include "venus-protocol/vn_protocol_driver_fence.h"
#include "venus-protocol/vn_protocol_driver_queue.h"
#include "venus-protocol/vn_protocol_driver_semaphore.h"
#include "venus-protocol/vn_protocol_driver_transport.h"

#include "vn_command_buffer.h"
#include "vn_device.h"
#include "vn_device_memory.h"
#include "vn_feedback.h"
#include "vn_instance.h"
#include "vn_physical_device.h"
#include "vn_query_pool.h"
#include "vn_renderer.h"
#include "vn_wsi.h"

/* queue commands */

struct vn_submit_info_pnext_fix {
   VkDeviceGroupSubmitInfo group;
   VkProtectedSubmitInfo protected;
   VkTimelineSemaphoreSubmitInfo timeline;
};

struct vn_queue_submission {
   VkStructureType batch_type;
   VkQueue queue_handle;
   uint32_t batch_count;
   union {
      const void *batches;
      const VkSubmitInfo *submit_batches;
      const VkSubmitInfo2 *submit2_batches;
      const VkBindSparseInfo *sparse_batches;
   };
   VkFence fence_handle;

   uint32_t cmd_count;
   uint32_t feedback_types;
   uint32_t pnext_count;
   uint32_t dev_mask_count;
   bool has_zink_sync_batch;
   const struct vn_device_memory *wsi_mem;
   struct vn_sync_payload_external external_payload;

   /* Temporary storage allocation for submission
    *
    * A single alloc for storage is performed and the offsets inside storage
    * are set as below:
    *
    * batches
    *  - non-empty submission: copy of original batches
    *  - empty submission: a single batch for fence feedback (ffb)
    * cmds
    *  - for each batch:
    *    - copy of original batch cmds
    *    - a single cmd for query feedback (qfb)
    *    - one cmd for each signal semaphore that has feedback (sfb)
    *    - if last batch, a single cmd for ffb
    */
   struct {
      void *storage;

      union {
         void *batches;
         VkSubmitInfo *submit_batches;
         VkSubmitInfo2 *submit2_batches;
      };

      union {
         void *cmds;
         VkCommandBuffer *cmd_handles;
         VkCommandBufferSubmitInfo *cmd_infos;
      };

      struct vn_submit_info_pnext_fix *pnexts;
      uint32_t *dev_masks;
   } temp;
};

static inline uint32_t
vn_get_wait_semaphore_count(struct vn_queue_submission *submit,
                            uint32_t batch_index)
{
   switch (submit->batch_type) {
   case VK_STRUCTURE_TYPE_SUBMIT_INFO:
      return submit->submit_batches[batch_index].waitSemaphoreCount;
   case VK_STRUCTURE_TYPE_SUBMIT_INFO_2:
      return submit->submit2_batches[batch_index].waitSemaphoreInfoCount;
   case VK_STRUCTURE_TYPE_BIND_SPARSE_INFO:
      return submit->sparse_batches[batch_index].waitSemaphoreCount;
   default:
      unreachable("unexpected batch type");
   }
}

static inline uint32_t
vn_get_signal_semaphore_count(struct vn_queue_submission *submit,
                              uint32_t batch_index)
{
   switch (submit->batch_type) {
   case VK_STRUCTURE_TYPE_SUBMIT_INFO:
      return submit->submit_batches[batch_index].signalSemaphoreCount;
   case VK_STRUCTURE_TYPE_SUBMIT_INFO_2:
      return submit->submit2_batches[batch_index].signalSemaphoreInfoCount;
   case VK_STRUCTURE_TYPE_BIND_SPARSE_INFO:
      return submit->sparse_batches[batch_index].signalSemaphoreCount;
   default:
      unreachable("unexpected batch type");
   }
}

static inline VkSemaphore
vn_get_wait_semaphore(struct vn_queue_submission *submit,
                      uint32_t batch_index,
                      uint32_t semaphore_index)
{
   switch (submit->batch_type) {
   case VK_STRUCTURE_TYPE_SUBMIT_INFO:
      return submit->submit_batches[batch_index]
         .pWaitSemaphores[semaphore_index];
   case VK_STRUCTURE_TYPE_SUBMIT_INFO_2:
      return submit->submit2_batches[batch_index]
         .pWaitSemaphoreInfos[semaphore_index]
         .semaphore;
   case VK_STRUCTURE_TYPE_BIND_SPARSE_INFO:
      return submit->sparse_batches[batch_index]
         .pWaitSemaphores[semaphore_index];
   default:
      unreachable("unexpected batch type");
   }
}

static inline VkSemaphore
vn_get_signal_semaphore(struct vn_queue_submission *submit,
                        uint32_t batch_index,
                        uint32_t semaphore_index)
{
   switch (submit->batch_type) {
   case VK_STRUCTURE_TYPE_SUBMIT_INFO:
      return submit->submit_batches[batch_index]
         .pSignalSemaphores[semaphore_index];
   case VK_STRUCTURE_TYPE_SUBMIT_INFO_2:
      return submit->submit2_batches[batch_index]
         .pSignalSemaphoreInfos[semaphore_index]
         .semaphore;
   case VK_STRUCTURE_TYPE_BIND_SPARSE_INFO:
      return submit->sparse_batches[batch_index]
         .pSignalSemaphores[semaphore_index];
   default:
      unreachable("unexpected batch type");
   }
}

static inline size_t
vn_get_batch_size(struct vn_queue_submission *submit)
{
   assert((submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO) ||
          (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2));
   return submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO
             ? sizeof(VkSubmitInfo)
             : sizeof(VkSubmitInfo2);
}

static inline size_t
vn_get_cmd_size(struct vn_queue_submission *submit)
{
   assert((submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO) ||
          (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2));
   return submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO
             ? sizeof(VkCommandBuffer)
             : sizeof(VkCommandBufferSubmitInfo);
}

static inline uint32_t
vn_get_cmd_count(struct vn_queue_submission *submit, uint32_t batch_index)
{
   assert((submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO) ||
          (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2));
   return submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO
             ? submit->submit_batches[batch_index].commandBufferCount
             : submit->submit2_batches[batch_index].commandBufferInfoCount;
}

static inline const void *
vn_get_cmds(struct vn_queue_submission *submit, uint32_t batch_index)
{
   assert((submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO) ||
          (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2));
   return submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO
             ? (const void *)submit->submit_batches[batch_index]
                  .pCommandBuffers
             : (const void *)submit->submit2_batches[batch_index]
                  .pCommandBufferInfos;
}

static inline struct vn_command_buffer *
vn_get_cmd(struct vn_queue_submission *submit,
           uint32_t batch_index,
           uint32_t cmd_index)
{
   assert((submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO) ||
          (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2));
   return vn_command_buffer_from_handle(
      submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO
         ? submit->submit_batches[batch_index].pCommandBuffers[cmd_index]
         : submit->submit2_batches[batch_index]
              .pCommandBufferInfos[cmd_index]
              .commandBuffer);
}

static inline void
vn_set_temp_cmd(struct vn_queue_submission *submit,
                uint32_t cmd_index,
                VkCommandBuffer cmd_handle)
{
   assert((submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO) ||
          (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2));
   if (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2) {
      submit->temp.cmd_infos[cmd_index] = (VkCommandBufferSubmitInfo){
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
         .commandBuffer = cmd_handle,
      };
   } else {
      submit->temp.cmd_handles[cmd_index] = cmd_handle;
   }
}

static uint64_t
vn_get_signal_semaphore_counter(struct vn_queue_submission *submit,
                                uint32_t batch_index,
                                uint32_t sem_index)
{
   switch (submit->batch_type) {
   case VK_STRUCTURE_TYPE_SUBMIT_INFO: {
      const struct VkTimelineSemaphoreSubmitInfo *timeline_sem_info =
         vk_find_struct_const(submit->submit_batches[batch_index].pNext,
                              TIMELINE_SEMAPHORE_SUBMIT_INFO);
      return timeline_sem_info->pSignalSemaphoreValues[sem_index];
   }
   case VK_STRUCTURE_TYPE_SUBMIT_INFO_2:
      return submit->submit2_batches[batch_index]
         .pSignalSemaphoreInfos[sem_index]
         .value;
   default:
      unreachable("unexpected batch type");
   }
}

static bool
vn_has_zink_sync_batch(struct vn_queue_submission *submit)
{
   struct vn_queue *queue = vn_queue_from_handle(submit->queue_handle);
   struct vn_device *dev = (void *)queue->base.base.base.device;
   struct vn_instance *instance = dev->instance;
   const uint32_t last_batch_index = submit->batch_count - 1;

   if (!instance->engine_is_zink)
      return false;

   if (!submit->batch_count || !last_batch_index ||
       vn_get_cmd_count(submit, last_batch_index))
      return false;

   if (vn_get_wait_semaphore_count(submit, last_batch_index))
      return false;

   const uint32_t signal_count =
      vn_get_signal_semaphore_count(submit, last_batch_index);
   for (uint32_t i = 0; i < signal_count; i++) {
      struct vn_semaphore *sem = vn_semaphore_from_handle(
         vn_get_signal_semaphore(submit, last_batch_index, i));
      if (sem->feedback.slot) {
         return true;
      }
   }
   return false;
}

static bool
vn_fix_batch_cmd_count_for_zink_sync(struct vn_queue_submission *submit,
                                     uint32_t batch_index,
                                     uint32_t new_cmd_count)
{
   /* If the last batch is a zink sync batch which is empty but contains
    * feedback, append the feedback to the previous batch instead so that
    * the last batch remains empty for perf.
    */
   if (batch_index == submit->batch_count - 1 &&
       submit->has_zink_sync_batch) {
      if (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2) {
         VkSubmitInfo2 *batch =
            &submit->temp.submit2_batches[batch_index - 1];
         assert(batch->pCommandBufferInfos);
         batch->commandBufferInfoCount += new_cmd_count;
      } else {
         VkSubmitInfo *batch = &submit->temp.submit_batches[batch_index - 1];
         assert(batch->pCommandBuffers);
         batch->commandBufferCount += new_cmd_count;
      }
      return true;
   }
   return false;
}

static void
vn_fix_device_group_cmd_count(struct vn_queue_submission *submit,
                              uint32_t batch_index)
{
   struct vk_queue *queue_vk = vk_queue_from_handle(submit->queue_handle);
   struct vn_device *dev = (void *)queue_vk->base.device;
   const VkSubmitInfo *src_batch = &submit->submit_batches[batch_index];
   struct vn_submit_info_pnext_fix *pnext_fix = submit->temp.pnexts;
   VkBaseOutStructure *dst =
      (void *)&submit->temp.submit_batches[batch_index];
   uint32_t new_cmd_count =
      submit->temp.submit_batches[batch_index].commandBufferCount;

   vk_foreach_struct_const(src, src_batch->pNext) {
      void *pnext = NULL;
      switch (src->sType) {
      case VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO: {
         uint32_t orig_cmd_count = 0;

         memcpy(&pnext_fix->group, src, sizeof(pnext_fix->group));

         VkDeviceGroupSubmitInfo *src_device_group =
            (VkDeviceGroupSubmitInfo *)src;
         if (src_device_group->commandBufferCount) {
            orig_cmd_count = src_device_group->commandBufferCount;
            memcpy(submit->temp.dev_masks,
                   src_device_group->pCommandBufferDeviceMasks,
                   sizeof(uint32_t) * orig_cmd_count);
         }

         /* Set the group device mask. Unlike sync2, zero means skip. */
         for (uint32_t i = orig_cmd_count; i < new_cmd_count; i++) {
            submit->temp.dev_masks[i] = dev->device_mask;
         }

         pnext_fix->group.commandBufferCount = new_cmd_count;
         pnext_fix->group.pCommandBufferDeviceMasks = submit->temp.dev_masks;
         pnext = &pnext_fix->group;
         break;
      }
      case VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO:
         memcpy(&pnext_fix->protected, src, sizeof(pnext_fix->protected));
         pnext = &pnext_fix->protected;
         break;
      case VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO:
         memcpy(&pnext_fix->timeline, src, sizeof(pnext_fix->timeline));
         pnext = &pnext_fix->timeline;
         break;
      default:
         /* The following structs are not supported by venus so are not
          * handled here. VkAmigoProfilingSubmitInfoSEC,
          * VkD3D12FenceSubmitInfoKHR, VkFrameBoundaryEXT,
          * VkLatencySubmissionPresentIdNV, VkPerformanceQuerySubmitInfoKHR,
          * VkWin32KeyedMutexAcquireReleaseInfoKHR,
          * VkWin32KeyedMutexAcquireReleaseInfoNV
          */
         break;
      }

      if (pnext) {
         dst->pNext = pnext;
         dst = pnext;
      }
   }
   submit->temp.pnexts++;
   submit->temp.dev_masks += new_cmd_count;
}

static bool
vn_semaphore_wait_external(struct vn_device *dev, struct vn_semaphore *sem);

static VkResult
vn_queue_submission_fix_batch_semaphores(struct vn_queue_submission *submit,
                                         uint32_t batch_index)
{
   struct vk_queue *queue_vk = vk_queue_from_handle(submit->queue_handle);
   VkDevice dev_handle = vk_device_to_handle(queue_vk->base.device);
   struct vn_device *dev = vn_device_from_handle(dev_handle);

   const uint32_t wait_count =
      vn_get_wait_semaphore_count(submit, batch_index);
   for (uint32_t i = 0; i < wait_count; i++) {
      VkSemaphore sem_handle = vn_get_wait_semaphore(submit, batch_index, i);
      struct vn_semaphore *sem = vn_semaphore_from_handle(sem_handle);
      const struct vn_sync_payload *payload = sem->payload;

      if (payload->type != VN_SYNC_TYPE_IMPORTED_SYNC_FD)
         continue;

      if (!vn_semaphore_wait_external(dev, sem))
         return VK_ERROR_DEVICE_LOST;

      assert(dev->physical_device->renderer_sync_fd.semaphore_importable);

      const VkImportSemaphoreResourceInfoMESA res_info = {
         .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_RESOURCE_INFO_MESA,
         .semaphore = sem_handle,
         .resourceId = 0,
      };
      vn_async_vkImportSemaphoreResourceMESA(dev->primary_ring, dev_handle,
                                             &res_info);
   }

   return VK_SUCCESS;
}

static void
vn_queue_submission_count_batch_feedback(struct vn_queue_submission *submit,
                                         uint32_t batch_index)
{
   const uint32_t signal_count =
      vn_get_signal_semaphore_count(submit, batch_index);
   uint32_t extra_cmd_count = 0;
   uint32_t feedback_types = 0;

   for (uint32_t i = 0; i < signal_count; i++) {
      struct vn_semaphore *sem = vn_semaphore_from_handle(
         vn_get_signal_semaphore(submit, batch_index, i));
      if (sem->feedback.slot) {
         feedback_types |= VN_FEEDBACK_TYPE_SEMAPHORE;
         extra_cmd_count++;
      }
   }

   if (submit->batch_type != VK_STRUCTURE_TYPE_BIND_SPARSE_INFO) {
      const uint32_t cmd_count = vn_get_cmd_count(submit, batch_index);
      for (uint32_t i = 0; i < cmd_count; i++) {
         struct vn_command_buffer *cmd = vn_get_cmd(submit, batch_index, i);
         if (!list_is_empty(&cmd->builder.query_records))
            feedback_types |= VN_FEEDBACK_TYPE_QUERY;

         /* If a cmd that was submitted previously and already has a feedback
          * cmd linked, as long as
          * VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT was not set we can
          * assume it has completed execution and is no longer in the pending
          * state so its safe to recycle the old feedback command.
          */
         if (cmd->linked_qfb_cmd) {
            assert(!cmd->builder.is_simultaneous);

            vn_query_feedback_cmd_free(cmd->linked_qfb_cmd);
            cmd->linked_qfb_cmd = NULL;
         }
      }
      if (feedback_types & VN_FEEDBACK_TYPE_QUERY)
         extra_cmd_count++;

      if (submit->feedback_types & VN_FEEDBACK_TYPE_FENCE &&
          batch_index == submit->batch_count - 1) {
         feedback_types |= VN_FEEDBACK_TYPE_FENCE;
         extra_cmd_count++;
      }

      /* Space to copy the original cmds to append feedback to it.
       * If the last batch is a zink sync batch which is an empty batch with
       * sem  feedback, feedback will be appended to the second to last batch
       * so also need to copy the second to last batch's original cmds even
       * if it doesn't have feedback itself.
       */
      if (feedback_types || (batch_index == submit->batch_count - 2 &&
                             submit->has_zink_sync_batch)) {
         extra_cmd_count += cmd_count;
      }
   }

   if (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO &&
       extra_cmd_count) {
      const VkDeviceGroupSubmitInfo *device_group = vk_find_struct_const(
         submit->submit_batches[batch_index].pNext, DEVICE_GROUP_SUBMIT_INFO);
      if (device_group) {
         submit->pnext_count++;
         submit->dev_mask_count += extra_cmd_count;
      }
   }

   submit->feedback_types |= feedback_types;
   submit->cmd_count += extra_cmd_count;
}

static VkResult
vn_queue_submission_prepare(struct vn_queue_submission *submit)
{
   struct vn_queue *queue = vn_queue_from_handle(submit->queue_handle);
   struct vn_fence *fence = vn_fence_from_handle(submit->fence_handle);

   assert(!fence || !fence->is_external || !fence->feedback.slot);
   if (fence && fence->feedback.slot)
      submit->feedback_types |= VN_FEEDBACK_TYPE_FENCE;

   if (submit->batch_type != VK_STRUCTURE_TYPE_BIND_SPARSE_INFO)
      submit->has_zink_sync_batch = vn_has_zink_sync_batch(submit);

   submit->external_payload.ring_idx = queue->ring_idx;

   submit->wsi_mem = NULL;
   if (submit->batch_count == 1 &&
       submit->batch_type != VK_STRUCTURE_TYPE_BIND_SPARSE_INFO) {
      const struct wsi_memory_signal_submit_info *info = vk_find_struct_const(
         submit->submit_batches[0].pNext, WSI_MEMORY_SIGNAL_SUBMIT_INFO_MESA);
      if (info) {
         submit->wsi_mem = vn_device_memory_from_handle(info->memory);
         assert(!submit->wsi_mem->base_memory && submit->wsi_mem->base_bo);
      }
   }

   for (uint32_t i = 0; i < submit->batch_count; i++) {
      VkResult result = vn_queue_submission_fix_batch_semaphores(submit, i);
      if (result != VK_SUCCESS)
         return result;

      vn_queue_submission_count_batch_feedback(submit, i);
   }

   return VK_SUCCESS;
}

static VkResult
vn_queue_submission_alloc_storage(struct vn_queue_submission *submit)
{
   struct vn_queue *queue = vn_queue_from_handle(submit->queue_handle);

   if (!submit->feedback_types)
      return VK_SUCCESS;

   /* for original batches or a new batch to hold feedback fence cmd */
   const size_t total_batch_size =
      vn_get_batch_size(submit) * MAX2(submit->batch_count, 1);
   /* for fence, timeline semaphore and query feedback cmds */
   const size_t total_cmd_size =
      vn_get_cmd_size(submit) * MAX2(submit->cmd_count, 1);
   /* for fixing command buffer counts in device group info, if it exists */
   const size_t total_pnext_size =
      submit->pnext_count * sizeof(struct vn_submit_info_pnext_fix);
   const size_t total_dev_mask_size =
      submit->dev_mask_count * sizeof(uint32_t);
   submit->temp.storage = vn_cached_storage_get(
      &queue->storage, total_batch_size + total_cmd_size + total_pnext_size +
                          total_dev_mask_size);
   if (!submit->temp.storage)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   submit->temp.batches = submit->temp.storage;
   submit->temp.cmds = submit->temp.storage + total_batch_size;
   submit->temp.pnexts =
      submit->temp.storage + total_batch_size + total_cmd_size;
   submit->temp.dev_masks = submit->temp.storage + total_batch_size +
                            total_cmd_size + total_pnext_size;

   return VK_SUCCESS;
}

static VkResult
vn_queue_submission_get_resolved_query_records(
   struct vn_queue_submission *submit,
   uint32_t batch_index,
   struct vn_feedback_cmd_pool *fb_cmd_pool,
   struct list_head *resolved_records)
{
   struct vn_command_pool *cmd_pool =
      vn_command_pool_from_handle(fb_cmd_pool->pool_handle);
   struct list_head dropped_records;
   VkResult result = VK_SUCCESS;

   list_inithead(resolved_records);
   list_inithead(&dropped_records);
   const uint32_t cmd_count = vn_get_cmd_count(submit, batch_index);
   for (uint32_t i = 0; i < cmd_count; i++) {
      struct vn_command_buffer *cmd = vn_get_cmd(submit, batch_index, i);

      list_for_each_entry(struct vn_cmd_query_record, record,
                          &cmd->builder.query_records, head) {
         if (!record->copy) {
            list_for_each_entry_safe(struct vn_cmd_query_record, prev,
                                     resolved_records, head) {
               /* If we previously added a query feedback that is now getting
                * reset, remove it since it is now a no-op and the deferred
                * feedback copy will cause a hang waiting for the reset query
                * to become available.
                */
               if (prev->copy && prev->query_pool == record->query_pool &&
                   prev->query >= record->query &&
                   prev->query < record->query + record->query_count)
                  list_move_to(&prev->head, &dropped_records);
            }
         }

         simple_mtx_lock(&fb_cmd_pool->mutex);
         struct vn_cmd_query_record *curr = vn_cmd_pool_alloc_query_record(
            cmd_pool, record->query_pool, record->query, record->query_count,
            record->copy);
         simple_mtx_unlock(&fb_cmd_pool->mutex);

         if (!curr) {
            list_splicetail(resolved_records, &dropped_records);
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto out_free_dropped_records;
         }

         list_addtail(&curr->head, resolved_records);
      }
   }

   /* further resolve to batch sequential queries */
   struct vn_cmd_query_record *curr =
      list_first_entry(resolved_records, struct vn_cmd_query_record, head);
   list_for_each_entry_safe(struct vn_cmd_query_record, next,
                            resolved_records, head) {
      if (curr->query_pool == next->query_pool && curr->copy == next->copy) {
         if (curr->query + curr->query_count == next->query) {
            curr->query_count += next->query_count;
            list_move_to(&next->head, &dropped_records);
         } else if (curr->query == next->query + next->query_count) {
            curr->query = next->query;
            curr->query_count += next->query_count;
            list_move_to(&next->head, &dropped_records);
         } else {
            curr = next;
         }
      } else {
         curr = next;
      }
   }

out_free_dropped_records:
   simple_mtx_lock(&fb_cmd_pool->mutex);
   vn_cmd_pool_free_query_records(cmd_pool, &dropped_records);
   simple_mtx_unlock(&fb_cmd_pool->mutex);
   return result;
}

static VkResult
vn_queue_submission_add_query_feedback(struct vn_queue_submission *submit,
                                       uint32_t batch_index,
                                       uint32_t *new_cmd_count)
{
   struct vk_queue *queue_vk = vk_queue_from_handle(submit->queue_handle);
   struct vn_device *dev = (void *)queue_vk->base.device;
   VkResult result;

   struct vn_feedback_cmd_pool *fb_cmd_pool = NULL;
   for (uint32_t i = 0; i < dev->queue_family_count; i++) {
      if (dev->queue_families[i] == queue_vk->queue_family_index) {
         fb_cmd_pool = &dev->fb_cmd_pools[i];
         break;
      }
   }
   assert(fb_cmd_pool);

   struct list_head resolved_records;
   result = vn_queue_submission_get_resolved_query_records(
      submit, batch_index, fb_cmd_pool, &resolved_records);
   if (result != VK_SUCCESS)
      return result;

   /* currently the reset query is always recorded */
   assert(!list_is_empty(&resolved_records));
   struct vn_query_feedback_cmd *qfb_cmd;
   result = vn_query_feedback_cmd_alloc(vn_device_to_handle(dev), fb_cmd_pool,
                                        &resolved_records, &qfb_cmd);
   if (result == VK_SUCCESS) {
      /* link query feedback cmd lifecycle with a cmd in the original batch so
       * that the feedback cmd can be reset and recycled when that cmd gets
       * reset/freed.
       *
       * Avoid cmd buffers with VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
       * since we don't know if all its instances have completed execution.
       * Should be rare enough to just log and leak the feedback cmd.
       */
      bool found_companion_cmd = false;
      const uint32_t cmd_count = vn_get_cmd_count(submit, batch_index);
      for (uint32_t i = 0; i < cmd_count; i++) {
         struct vn_command_buffer *cmd = vn_get_cmd(submit, batch_index, i);
         if (!cmd->builder.is_simultaneous) {
            cmd->linked_qfb_cmd = qfb_cmd;
            found_companion_cmd = true;
            break;
         }
      }
      if (!found_companion_cmd)
         vn_log(dev->instance, "WARN: qfb cmd has leaked!");

      vn_set_temp_cmd(submit, (*new_cmd_count)++, qfb_cmd->cmd_handle);
   }

   simple_mtx_lock(&fb_cmd_pool->mutex);
   vn_cmd_pool_free_query_records(
      vn_command_pool_from_handle(fb_cmd_pool->pool_handle),
      &resolved_records);
   simple_mtx_unlock(&fb_cmd_pool->mutex);

   return result;
}

struct vn_semaphore_feedback_cmd *
vn_semaphore_get_feedback_cmd(struct vn_device *dev,
                              struct vn_semaphore *sem);

static VkResult
vn_queue_submission_add_semaphore_feedback(struct vn_queue_submission *submit,
                                           uint32_t batch_index,
                                           uint32_t signal_index,
                                           uint32_t *new_cmd_count)
{
   struct vn_semaphore *sem = vn_semaphore_from_handle(
      vn_get_signal_semaphore(submit, batch_index, signal_index));
   if (!sem->feedback.slot)
      return VK_SUCCESS;

   VK_FROM_HANDLE(vk_queue, queue_vk, submit->queue_handle);
   struct vn_device *dev = (void *)queue_vk->base.device;
   struct vn_semaphore_feedback_cmd *sfb_cmd =
      vn_semaphore_get_feedback_cmd(dev, sem);
   if (!sfb_cmd)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   const uint64_t counter =
      vn_get_signal_semaphore_counter(submit, batch_index, signal_index);
   vn_feedback_set_counter(sfb_cmd->src_slot, counter);

   for (uint32_t i = 0; i < dev->queue_family_count; i++) {
      if (dev->queue_families[i] == queue_vk->queue_family_index) {
         vn_set_temp_cmd(submit, (*new_cmd_count)++, sfb_cmd->cmd_handles[i]);
         return VK_SUCCESS;
      }
   }

   unreachable("bad feedback sem");
}

static void
vn_queue_submission_add_fence_feedback(struct vn_queue_submission *submit,
                                       uint32_t batch_index,
                                       uint32_t *new_cmd_count)
{
   VK_FROM_HANDLE(vk_queue, queue_vk, submit->queue_handle);
   struct vn_device *dev = (void *)queue_vk->base.device;
   struct vn_fence *fence = vn_fence_from_handle(submit->fence_handle);

   VkCommandBuffer ffb_cmd_handle = VK_NULL_HANDLE;
   for (uint32_t i = 0; i < dev->queue_family_count; i++) {
      if (dev->queue_families[i] == queue_vk->queue_family_index) {
         ffb_cmd_handle = fence->feedback.commands[i];
      }
   }
   assert(ffb_cmd_handle != VK_NULL_HANDLE);

   vn_set_temp_cmd(submit, (*new_cmd_count)++, ffb_cmd_handle);
}

static VkResult
vn_queue_submission_add_feedback_cmds(struct vn_queue_submission *submit,
                                      uint32_t batch_index,
                                      uint32_t feedback_types)
{
   VkResult result;
   uint32_t new_cmd_count = vn_get_cmd_count(submit, batch_index);

   if (feedback_types & VN_FEEDBACK_TYPE_QUERY) {
      result = vn_queue_submission_add_query_feedback(submit, batch_index,
                                                      &new_cmd_count);
      if (result != VK_SUCCESS)
         return result;
   }

   if (feedback_types & VN_FEEDBACK_TYPE_SEMAPHORE) {
      const uint32_t signal_count =
         vn_get_signal_semaphore_count(submit, batch_index);
      for (uint32_t i = 0; i < signal_count; i++) {
         result = vn_queue_submission_add_semaphore_feedback(
            submit, batch_index, i, &new_cmd_count);
         if (result != VK_SUCCESS)
            return result;
      }
      if (vn_fix_batch_cmd_count_for_zink_sync(submit, batch_index,
                                               new_cmd_count))
         return VK_SUCCESS;
   }

   if (feedback_types & VN_FEEDBACK_TYPE_FENCE) {
      vn_queue_submission_add_fence_feedback(submit, batch_index,
                                             &new_cmd_count);
   }

   if (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2) {
      VkSubmitInfo2 *batch = &submit->temp.submit2_batches[batch_index];
      batch->pCommandBufferInfos = submit->temp.cmd_infos;
      batch->commandBufferInfoCount = new_cmd_count;
   } else {
      VkSubmitInfo *batch = &submit->temp.submit_batches[batch_index];
      batch->pCommandBuffers = submit->temp.cmd_handles;
      batch->commandBufferCount = new_cmd_count;

      const VkDeviceGroupSubmitInfo *device_group = vk_find_struct_const(
         submit->submit_batches[batch_index].pNext, DEVICE_GROUP_SUBMIT_INFO);
      if (device_group)
         vn_fix_device_group_cmd_count(submit, batch_index);
   }

   return VK_SUCCESS;
}

static VkResult
vn_queue_submission_setup_batch(struct vn_queue_submission *submit,
                                uint32_t batch_index)
{
   uint32_t feedback_types = 0;
   uint32_t extra_cmd_count = 0;

   const uint32_t signal_count =
      vn_get_signal_semaphore_count(submit, batch_index);
   for (uint32_t i = 0; i < signal_count; i++) {
      struct vn_semaphore *sem = vn_semaphore_from_handle(
         vn_get_signal_semaphore(submit, batch_index, i));
      if (sem->feedback.slot) {
         feedback_types |= VN_FEEDBACK_TYPE_SEMAPHORE;
         extra_cmd_count++;
      }
   }

   const uint32_t cmd_count = vn_get_cmd_count(submit, batch_index);
   for (uint32_t i = 0; i < cmd_count; i++) {
      struct vn_command_buffer *cmd = vn_get_cmd(submit, batch_index, i);
      if (!list_is_empty(&cmd->builder.query_records)) {
         feedback_types |= VN_FEEDBACK_TYPE_QUERY;
         extra_cmd_count++;
         break;
      }
   }

   if (submit->feedback_types & VN_FEEDBACK_TYPE_FENCE &&
       batch_index == submit->batch_count - 1) {
      feedback_types |= VN_FEEDBACK_TYPE_FENCE;
      extra_cmd_count++;
   }

   /* If the batch has qfb, sfb or ffb, copy the original commands and append
    * feedback cmds.
    * If this is the second to last batch and the last batch a zink sync batch
    * which is empty but has feedback, also copy the original commands for
    * this batch so that the last batch's feedback can be appended to it.
    */
   if (feedback_types || (batch_index == submit->batch_count - 2 &&
                          submit->has_zink_sync_batch)) {
      const size_t cmd_size = vn_get_cmd_size(submit);
      const size_t total_cmd_size = cmd_count * cmd_size;
      /* copy only needed for non-empty batches */
      if (total_cmd_size) {
         memcpy(submit->temp.cmds, vn_get_cmds(submit, batch_index),
                total_cmd_size);
      }

      VkResult result = vn_queue_submission_add_feedback_cmds(
         submit, batch_index, feedback_types);
      if (result != VK_SUCCESS)
         return result;

      /* advance the temp cmds for working on next batch cmds */
      submit->temp.cmds += total_cmd_size + (extra_cmd_count * cmd_size);
   }

   return VK_SUCCESS;
}

static VkResult
vn_queue_submission_setup_batches(struct vn_queue_submission *submit)
{
   assert(submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2 ||
          submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO);

   if (!submit->feedback_types)
      return VK_SUCCESS;

   /* For a submission that is:
    * - non-empty: copy batches for adding feedbacks
    * - empty: initialize a batch for fence feedback
    */
   if (submit->batch_count) {
      memcpy(submit->temp.batches, submit->batches,
             vn_get_batch_size(submit) * submit->batch_count);
   } else {
      assert(submit->feedback_types & VN_FEEDBACK_TYPE_FENCE);
      if (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2) {
         submit->temp.submit2_batches[0] = (VkSubmitInfo2){
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
         };
      } else {
         submit->temp.submit_batches[0] = (VkSubmitInfo){
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
         };
      }
      submit->batch_count = 1;
      submit->batches = submit->temp.batches;
   }

   for (uint32_t i = 0; i < submit->batch_count; i++) {
      VkResult result = vn_queue_submission_setup_batch(submit, i);
      if (result != VK_SUCCESS)
         return result;
   }

   submit->batches = submit->temp.batches;

   return VK_SUCCESS;
}

static void
vn_queue_submission_cleanup_semaphore_feedback(
   struct vn_queue_submission *submit)
{
   struct vk_queue *queue_vk = vk_queue_from_handle(submit->queue_handle);
   VkDevice dev_handle = vk_device_to_handle(queue_vk->base.device);

   for (uint32_t i = 0; i < submit->batch_count; i++) {
      const uint32_t wait_count = vn_get_wait_semaphore_count(submit, i);
      for (uint32_t j = 0; j < wait_count; j++) {
         VkSemaphore sem_handle = vn_get_wait_semaphore(submit, i, j);
         struct vn_semaphore *sem = vn_semaphore_from_handle(sem_handle);
         if (!sem->feedback.slot)
            continue;

         /* sfb pending cmds are recycled when signaled counter is updated */
         uint64_t counter = 0;
         vn_GetSemaphoreCounterValue(dev_handle, sem_handle, &counter);
      }

      const uint32_t signal_count = vn_get_signal_semaphore_count(submit, i);
      for (uint32_t j = 0; j < signal_count; j++) {
         VkSemaphore sem_handle = vn_get_signal_semaphore(submit, i, j);
         struct vn_semaphore *sem = vn_semaphore_from_handle(sem_handle);
         if (!sem->feedback.slot)
            continue;

         /* sfb pending cmds are recycled when signaled counter is updated */
         uint64_t counter = 0;
         vn_GetSemaphoreCounterValue(dev_handle, sem_handle, &counter);
      }
   }
}

static void
vn_queue_submission_cleanup(struct vn_queue_submission *submit)
{
   /* TODO clean up pending src feedbacks on failure? */
   if (submit->feedback_types & VN_FEEDBACK_TYPE_SEMAPHORE)
      vn_queue_submission_cleanup_semaphore_feedback(submit);
}

static VkResult
vn_queue_submission_prepare_submit(struct vn_queue_submission *submit)
{
   VkResult result = vn_queue_submission_prepare(submit);
   if (result != VK_SUCCESS)
      return result;

   result = vn_queue_submission_alloc_storage(submit);
   if (result != VK_SUCCESS)
      return result;

   result = vn_queue_submission_setup_batches(submit);
   if (result != VK_SUCCESS) {
      vn_queue_submission_cleanup(submit);
      return result;
   }

   return VK_SUCCESS;
}

static void
vn_queue_wsi_present(struct vn_queue_submission *submit)
{
   struct vk_queue *queue_vk = vk_queue_from_handle(submit->queue_handle);
   struct vn_device *dev = (void *)queue_vk->base.device;

   if (!submit->wsi_mem)
      return;

   if (dev->renderer->info.has_implicit_fencing) {
      struct vn_renderer_submit_batch batch = {
         .ring_idx = submit->external_payload.ring_idx,
      };

      uint32_t local_data[8];
      struct vn_cs_encoder local_enc =
         VN_CS_ENCODER_INITIALIZER_LOCAL(local_data, sizeof(local_data));
      if (submit->external_payload.ring_seqno_valid) {
         const uint64_t ring_id = vn_ring_get_id(dev->primary_ring);
         vn_encode_vkWaitRingSeqnoMESA(&local_enc, 0, ring_id,
                                       submit->external_payload.ring_seqno);
         batch.cs_data = local_data;
         batch.cs_size = vn_cs_encoder_get_len(&local_enc);
      }

      const struct vn_renderer_submit renderer_submit = {
         .bos = &submit->wsi_mem->base_bo,
         .bo_count = 1,
         .batches = &batch,
         .batch_count = 1,
      };
      vn_renderer_submit(dev->renderer, &renderer_submit);
   } else {
      if (VN_DEBUG(WSI)) {
         static uint32_t num_rate_limit_warning = 0;

         if (num_rate_limit_warning++ < 10)
            vn_log(dev->instance,
                   "forcing vkQueueWaitIdle before presenting");
      }

      vn_QueueWaitIdle(submit->queue_handle);
   }
}

static VkResult
vn_queue_submit(struct vn_queue_submission *submit)
{
   struct vn_queue *queue = vn_queue_from_handle(submit->queue_handle);
   struct vn_device *dev = (void *)queue->base.base.base.device;
   struct vn_instance *instance = dev->instance;
   VkResult result;

   /* To ensure external components waiting on the correct fence payload,
    * below sync primitives must be installed after the submission:
    * - explicit fencing: sync file export
    * - implicit fencing: dma-fence attached to the wsi bo
    *
    * We enforce above via an asynchronous vkQueueSubmit(2) via ring followed
    * by an asynchronous renderer submission to wait for the ring submission:
    * - struct wsi_memory_signal_submit_info
    * - fence is an external fence
    * - has an external signal semaphore
    */
   result = vn_queue_submission_prepare_submit(submit);
   if (result != VK_SUCCESS)
      return vn_error(instance, result);

   /* skip no-op submit */
   if (!submit->batch_count && submit->fence_handle == VK_NULL_HANDLE)
      return VK_SUCCESS;

   if (VN_PERF(NO_ASYNC_QUEUE_SUBMIT)) {
      if (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2) {
         result = vn_call_vkQueueSubmit2(
            dev->primary_ring, submit->queue_handle, submit->batch_count,
            submit->submit2_batches, submit->fence_handle);
      } else {
         result = vn_call_vkQueueSubmit(
            dev->primary_ring, submit->queue_handle, submit->batch_count,
            submit->submit_batches, submit->fence_handle);
      }

      if (result != VK_SUCCESS) {
         vn_queue_submission_cleanup(submit);
         return vn_error(instance, result);
      }
   } else {
      struct vn_ring_submit_command ring_submit;
      if (submit->batch_type == VK_STRUCTURE_TYPE_SUBMIT_INFO_2) {
         vn_submit_vkQueueSubmit2(
            dev->primary_ring, 0, submit->queue_handle, submit->batch_count,
            submit->submit2_batches, submit->fence_handle, &ring_submit);
      } else {
         vn_submit_vkQueueSubmit(dev->primary_ring, 0, submit->queue_handle,
                                 submit->batch_count, submit->submit_batches,
                                 submit->fence_handle, &ring_submit);
      }
      if (!ring_submit.ring_seqno_valid) {
         vn_queue_submission_cleanup(submit);
         return vn_error(instance, VK_ERROR_DEVICE_LOST);
      }
      submit->external_payload.ring_seqno_valid = true;
      submit->external_payload.ring_seqno = ring_submit.ring_seqno;
   }

   /* If external fence, track the submission's ring_idx to facilitate
    * sync_file export.
    *
    * Imported syncs don't need a proxy renderer sync on subsequent export,
    * because an fd is already available.
    */
   struct vn_fence *fence = vn_fence_from_handle(submit->fence_handle);
   if (fence && fence->is_external) {
      assert(fence->payload->type == VN_SYNC_TYPE_DEVICE_ONLY);
      fence->external_payload = submit->external_payload;
   }

   for (uint32_t i = 0; i < submit->batch_count; i++) {
      const uint32_t signal_count = vn_get_signal_semaphore_count(submit, i);
      for (uint32_t j = 0; j < signal_count; j++) {
         struct vn_semaphore *sem =
            vn_semaphore_from_handle(vn_get_signal_semaphore(submit, i, j));
         if (sem->is_external) {
            assert(sem->payload->type == VN_SYNC_TYPE_DEVICE_ONLY);
            sem->external_payload = submit->external_payload;
         }
      }
   }

   vn_queue_wsi_present(submit);

   vn_queue_submission_cleanup(submit);

   return VK_SUCCESS;
}

VkResult
vn_QueueSubmit(VkQueue queue,
               uint32_t submitCount,
               const VkSubmitInfo *pSubmits,
               VkFence fence)
{
   VN_TRACE_FUNC();

   struct vn_queue_submission submit = {
      .batch_type = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .queue_handle = queue,
      .batch_count = submitCount,
      .submit_batches = pSubmits,
      .fence_handle = fence,
   };

   return vn_queue_submit(&submit);
}

VkResult
vn_QueueSubmit2(VkQueue queue,
                uint32_t submitCount,
                const VkSubmitInfo2 *pSubmits,
                VkFence fence)
{
   VN_TRACE_FUNC();

   struct vn_queue_submission submit = {
      .batch_type = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .queue_handle = queue,
      .batch_count = submitCount,
      .submit2_batches = pSubmits,
      .fence_handle = fence,
   };

   return vn_queue_submit(&submit);
}

static VkResult
vn_queue_bind_sparse_submit(struct vn_queue_submission *submit)
{
   struct vn_queue *queue = vn_queue_from_handle(submit->queue_handle);
   struct vn_device *dev = (void *)queue->base.base.base.device;
   struct vn_instance *instance = dev->instance;
   VkResult result;

   if (VN_PERF(NO_ASYNC_QUEUE_SUBMIT)) {
      result = vn_call_vkQueueBindSparse(
         dev->primary_ring, submit->queue_handle, submit->batch_count,
         submit->sparse_batches, submit->fence_handle);
      if (result != VK_SUCCESS)
         return vn_error(instance, result);
   } else {
      struct vn_ring_submit_command ring_submit;
      vn_submit_vkQueueBindSparse(dev->primary_ring, 0, submit->queue_handle,
                                  submit->batch_count, submit->sparse_batches,
                                  submit->fence_handle, &ring_submit);

      if (!ring_submit.ring_seqno_valid)
         return vn_error(instance, VK_ERROR_DEVICE_LOST);
   }

   return VK_SUCCESS;
}

static VkResult
vn_queue_bind_sparse_submit_batch(struct vn_queue_submission *submit,
                                  uint32_t batch_index)
{
   struct vn_queue *queue = vn_queue_from_handle(submit->queue_handle);
   VkDevice dev_handle = vk_device_to_handle(queue->base.base.base.device);
   const VkBindSparseInfo *sparse_info = &submit->sparse_batches[batch_index];
   const VkSemaphore *signal_sem = sparse_info->pSignalSemaphores;
   uint32_t signal_sem_count = sparse_info->signalSemaphoreCount;
   VkResult result;

   struct vn_queue_submission sparse_batch = {
      .batch_type = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
      .queue_handle = submit->queue_handle,
      .batch_count = 1,
      .fence_handle = VK_NULL_HANDLE,
   };

   /* lazily create sparse semaphore */
   if (queue->sparse_semaphore == VK_NULL_HANDLE) {
      queue->sparse_semaphore_counter = 1;
      const VkSemaphoreTypeCreateInfo sem_type_create_info = {
         .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
         .pNext = NULL,
         /* This must be timeline type to adhere to mesa's requirement
          * not to mix binary semaphores with wait-before-signal.
          */
         .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
         .initialValue = 1,
      };
      const VkSemaphoreCreateInfo create_info = {
         .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
         .pNext = &sem_type_create_info,
         .flags = 0,
      };

      result = vn_CreateSemaphore(dev_handle, &create_info, NULL,
                                  &queue->sparse_semaphore);
      if (result != VK_SUCCESS)
         return result;
   }

   /* Setup VkTimelineSemaphoreSubmitInfo's for our queue sparse semaphore
    * so that the vkQueueSubmit waits on the vkQueueBindSparse signal.
    */
   queue->sparse_semaphore_counter++;
   struct VkTimelineSemaphoreSubmitInfo wait_timeline_sem_info = { 0 };
   wait_timeline_sem_info.sType =
      VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
   wait_timeline_sem_info.signalSemaphoreValueCount = 1;
   wait_timeline_sem_info.pSignalSemaphoreValues =
      &queue->sparse_semaphore_counter;

   struct VkTimelineSemaphoreSubmitInfo signal_timeline_sem_info = { 0 };
   signal_timeline_sem_info.sType =
      VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
   signal_timeline_sem_info.waitSemaphoreValueCount = 1;
   signal_timeline_sem_info.pWaitSemaphoreValues =
      &queue->sparse_semaphore_counter;

   /* Split up the original wait and signal semaphores into its respective
    * vkTimelineSemaphoreSubmitInfo
    */
   const struct VkTimelineSemaphoreSubmitInfo *timeline_sem_info =
      vk_find_struct_const(sparse_info->pNext,
                           TIMELINE_SEMAPHORE_SUBMIT_INFO);
   if (timeline_sem_info) {
      if (timeline_sem_info->waitSemaphoreValueCount) {
         wait_timeline_sem_info.waitSemaphoreValueCount =
            timeline_sem_info->waitSemaphoreValueCount;
         wait_timeline_sem_info.pWaitSemaphoreValues =
            timeline_sem_info->pWaitSemaphoreValues;
      }

      if (timeline_sem_info->signalSemaphoreValueCount) {
         signal_timeline_sem_info.signalSemaphoreValueCount =
            timeline_sem_info->signalSemaphoreValueCount;
         signal_timeline_sem_info.pSignalSemaphoreValues =
            timeline_sem_info->pSignalSemaphoreValues;
      }
   }

   /* Attach the original VkDeviceGroupBindSparseInfo if it exists */
   struct VkDeviceGroupBindSparseInfo batch_device_group_info;
   const struct VkDeviceGroupBindSparseInfo *device_group_info =
      vk_find_struct_const(sparse_info->pNext, DEVICE_GROUP_BIND_SPARSE_INFO);
   if (device_group_info) {
      memcpy(&batch_device_group_info, device_group_info,
             sizeof(*device_group_info));
      batch_device_group_info.pNext = NULL;

      wait_timeline_sem_info.pNext = &batch_device_group_info;
   }

   /* Copy the original batch VkBindSparseInfo modified to signal
    * our sparse semaphore.
    */
   VkBindSparseInfo batch_sparse_info;
   memcpy(&batch_sparse_info, sparse_info, sizeof(*sparse_info));

   batch_sparse_info.pNext = &wait_timeline_sem_info;
   batch_sparse_info.signalSemaphoreCount = 1;
   batch_sparse_info.pSignalSemaphores = &queue->sparse_semaphore;

   /* Set up the SubmitInfo to wait on our sparse semaphore before sending
    * feedback and signaling the original semaphores/fence
    *
    * Even if this VkBindSparse batch does not have feedback semaphores,
    * we still glue all the batches together to ensure the feedback
    * fence occurs after.
    */
   VkPipelineStageFlags stage_masks = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
   VkSubmitInfo batch_submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = &signal_timeline_sem_info,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &queue->sparse_semaphore,
      .pWaitDstStageMask = &stage_masks,
      .signalSemaphoreCount = signal_sem_count,
      .pSignalSemaphores = signal_sem,
   };

   /* Set the possible fence if on the last batch */
   VkFence fence_handle = VK_NULL_HANDLE;
   if ((submit->feedback_types & VN_FEEDBACK_TYPE_FENCE) &&
       batch_index == (submit->batch_count - 1)) {
      fence_handle = submit->fence_handle;
   }

   sparse_batch.sparse_batches = &batch_sparse_info;
   result = vn_queue_bind_sparse_submit(&sparse_batch);
   if (result != VK_SUCCESS)
      return result;

   result = vn_QueueSubmit(submit->queue_handle, 1, &batch_submit_info,
                           fence_handle);
   if (result != VK_SUCCESS)
      return result;

   return VK_SUCCESS;
}

VkResult
vn_QueueBindSparse(VkQueue queue,
                   uint32_t bindInfoCount,
                   const VkBindSparseInfo *pBindInfo,
                   VkFence fence)
{
   VN_TRACE_FUNC();
   VkResult result;

   struct vn_queue_submission submit = {
      .batch_type = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
      .queue_handle = queue,
      .batch_count = bindInfoCount,
      .sparse_batches = pBindInfo,
      .fence_handle = fence,
   };

   result = vn_queue_submission_prepare(&submit);
   if (result != VK_SUCCESS)
      return result;

   if (!submit.batch_count) {
      /* skip no-op submit */
      if (submit.fence_handle == VK_NULL_HANDLE)
         return VK_SUCCESS;

      /* if empty batch, just send a vkQueueSubmit with the fence */
      result =
         vn_QueueSubmit(submit.queue_handle, 0, NULL, submit.fence_handle);
      if (result != VK_SUCCESS)
         return result;
   }

   /* if feedback isn't used in the batch, can directly submit */
   if (!submit.feedback_types)
      return vn_queue_bind_sparse_submit(&submit);

   for (uint32_t i = 0; i < submit.batch_count; i++) {
      result = vn_queue_bind_sparse_submit_batch(&submit, i);
      if (result != VK_SUCCESS)
         return result;
   }

   return VK_SUCCESS;
}

VkResult
vn_QueueWaitIdle(VkQueue _queue)
{
   VN_TRACE_FUNC();
   struct vn_queue *queue = vn_queue_from_handle(_queue);
   VkDevice dev_handle = vk_device_to_handle(queue->base.base.base.device);
   struct vn_device *dev = vn_device_from_handle(dev_handle);
   VkResult result;

   /* lazily create queue wait fence for queue idle waiting */
   if (queue->wait_fence == VK_NULL_HANDLE) {
      const VkFenceCreateInfo create_info = {
         .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
         .flags = 0,
      };
      result =
         vn_CreateFence(dev_handle, &create_info, NULL, &queue->wait_fence);
      if (result != VK_SUCCESS)
         return result;
   }

   result = vn_QueueSubmit(_queue, 0, NULL, queue->wait_fence);
   if (result != VK_SUCCESS)
      return result;

   result =
      vn_WaitForFences(dev_handle, 1, &queue->wait_fence, true, UINT64_MAX);
   vn_ResetFences(dev_handle, 1, &queue->wait_fence);

   return vn_result(dev->instance, result);
}

/* fence commands */

static void
vn_sync_payload_release(UNUSED struct vn_device *dev,
                        struct vn_sync_payload *payload)
{
   if (payload->type == VN_SYNC_TYPE_IMPORTED_SYNC_FD && payload->fd >= 0)
      close(payload->fd);

   payload->type = VN_SYNC_TYPE_INVALID;
}

static VkResult
vn_fence_init_payloads(struct vn_device *dev,
                       struct vn_fence *fence,
                       bool signaled,
                       const VkAllocationCallbacks *alloc)
{
   fence->permanent.type = VN_SYNC_TYPE_DEVICE_ONLY;
   fence->temporary.type = VN_SYNC_TYPE_INVALID;
   fence->payload = &fence->permanent;

   return VK_SUCCESS;
}

void
vn_fence_signal_wsi(struct vn_device *dev, struct vn_fence *fence)
{
   struct vn_sync_payload *temp = &fence->temporary;

   vn_sync_payload_release(dev, temp);
   temp->type = VN_SYNC_TYPE_IMPORTED_SYNC_FD;
   temp->fd = -1;
   fence->payload = temp;
}

static VkResult
vn_fence_feedback_init(struct vn_device *dev,
                       struct vn_fence *fence,
                       bool signaled,
                       const VkAllocationCallbacks *alloc)
{
   VkDevice dev_handle = vn_device_to_handle(dev);
   struct vn_feedback_slot *slot;
   VkCommandBuffer *cmd_handles;
   VkResult result;

   if (fence->is_external)
      return VK_SUCCESS;

   if (VN_PERF(NO_FENCE_FEEDBACK))
      return VK_SUCCESS;

   slot = vn_feedback_pool_alloc(&dev->feedback_pool, VN_FEEDBACK_TYPE_FENCE);
   if (!slot)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   vn_feedback_set_status(slot, signaled ? VK_SUCCESS : VK_NOT_READY);

   cmd_handles =
      vk_zalloc(alloc, sizeof(*cmd_handles) * dev->queue_family_count,
                VN_DEFAULT_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!cmd_handles) {
      vn_feedback_pool_free(&dev->feedback_pool, slot);
      return VK_ERROR_OUT_OF_HOST_MEMORY;
   }

   for (uint32_t i = 0; i < dev->queue_family_count; i++) {
      result = vn_feedback_cmd_alloc(dev_handle, &dev->fb_cmd_pools[i], slot,
                                     NULL, &cmd_handles[i]);
      if (result != VK_SUCCESS) {
         for (uint32_t j = 0; j < i; j++) {
            vn_feedback_cmd_free(dev_handle, &dev->fb_cmd_pools[j],
                                 cmd_handles[j]);
         }
         break;
      }
   }

   if (result != VK_SUCCESS) {
      vk_free(alloc, cmd_handles);
      vn_feedback_pool_free(&dev->feedback_pool, slot);
      return result;
   }

   fence->feedback.slot = slot;
   fence->feedback.commands = cmd_handles;

   return VK_SUCCESS;
}

static void
vn_fence_feedback_fini(struct vn_device *dev,
                       struct vn_fence *fence,
                       const VkAllocationCallbacks *alloc)
{
   VkDevice dev_handle = vn_device_to_handle(dev);

   if (!fence->feedback.slot)
      return;

   for (uint32_t i = 0; i < dev->queue_family_count; i++) {
      vn_feedback_cmd_free(dev_handle, &dev->fb_cmd_pools[i],
                           fence->feedback.commands[i]);
   }

   vn_feedback_pool_free(&dev->feedback_pool, fence->feedback.slot);

   vk_free(alloc, fence->feedback.commands);
}

VkResult
vn_CreateFence(VkDevice device,
               const VkFenceCreateInfo *pCreateInfo,
               const VkAllocationCallbacks *pAllocator,
               VkFence *pFence)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.base.alloc;
   const bool signaled = pCreateInfo->flags & VK_FENCE_CREATE_SIGNALED_BIT;
   VkResult result;

   struct vn_fence *fence = vk_zalloc(alloc, sizeof(*fence), VN_DEFAULT_ALIGN,
                                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!fence)
      return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vn_object_base_init(&fence->base, VK_OBJECT_TYPE_FENCE, &dev->base);

   const struct VkExportFenceCreateInfo *export_info =
      vk_find_struct_const(pCreateInfo->pNext, EXPORT_FENCE_CREATE_INFO);
   fence->is_external = export_info && export_info->handleTypes;

   result = vn_fence_init_payloads(dev, fence, signaled, alloc);
   if (result != VK_SUCCESS)
      goto out_object_base_fini;

   result = vn_fence_feedback_init(dev, fence, signaled, alloc);
   if (result != VK_SUCCESS)
      goto out_payloads_fini;

   *pFence = vn_fence_to_handle(fence);
   vn_async_vkCreateFence(dev->primary_ring, device, pCreateInfo, NULL,
                          pFence);

   return VK_SUCCESS;

out_payloads_fini:
   vn_sync_payload_release(dev, &fence->permanent);
   vn_sync_payload_release(dev, &fence->temporary);

out_object_base_fini:
   vn_object_base_fini(&fence->base);
   vk_free(alloc, fence);
   return vn_error(dev->instance, result);
}

void
vn_DestroyFence(VkDevice device,
                VkFence _fence,
                const VkAllocationCallbacks *pAllocator)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_fence *fence = vn_fence_from_handle(_fence);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.base.alloc;

   if (!fence)
      return;

   vn_async_vkDestroyFence(dev->primary_ring, device, _fence, NULL);

   vn_fence_feedback_fini(dev, fence, alloc);

   vn_sync_payload_release(dev, &fence->permanent);
   vn_sync_payload_release(dev, &fence->temporary);

   vn_object_base_fini(&fence->base);
   vk_free(alloc, fence);
}

VkResult
vn_ResetFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);

   vn_async_vkResetFences(dev->primary_ring, device, fenceCount, pFences);

   for (uint32_t i = 0; i < fenceCount; i++) {
      struct vn_fence *fence = vn_fence_from_handle(pFences[i]);
      struct vn_sync_payload *perm = &fence->permanent;

      vn_sync_payload_release(dev, &fence->temporary);

      assert(perm->type == VN_SYNC_TYPE_DEVICE_ONLY);
      fence->payload = perm;

      if (fence->feedback.slot)
         vn_feedback_reset_status(fence->feedback.slot);
   }

   return VK_SUCCESS;
}

VkResult
vn_GetFenceStatus(VkDevice device, VkFence _fence)
{
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_fence *fence = vn_fence_from_handle(_fence);
   struct vn_sync_payload *payload = fence->payload;

   VkResult result;
   switch (payload->type) {
   case VN_SYNC_TYPE_DEVICE_ONLY:
      if (fence->feedback.slot) {
         result = vn_feedback_get_status(fence->feedback.slot);
         if (result == VK_SUCCESS) {
            /* When fence feedback slot gets signaled, the real fence
             * signal operation follows after but the signaling isr can be
             * deferred or preempted. To avoid racing, we let the
             * renderer wait for the fence. This also helps resolve
             * synchronization validation errors, because the layer no
             * longer sees any fence status checks and falsely believes the
             * caller does not sync.
             */
            vn_async_vkWaitForFences(dev->primary_ring, device, 1, &_fence,
                                     VK_TRUE, UINT64_MAX);
         }
      } else {
         result = vn_call_vkGetFenceStatus(dev->primary_ring, device, _fence);
      }
      break;
   case VN_SYNC_TYPE_IMPORTED_SYNC_FD:
      if (payload->fd < 0 || sync_wait(payload->fd, 0) == 0)
         result = VK_SUCCESS;
      else
         result = errno == ETIME ? VK_NOT_READY : VK_ERROR_DEVICE_LOST;
      break;
   default:
      unreachable("unexpected fence payload type");
      break;
   }

   return vn_result(dev->instance, result);
}

static VkResult
vn_find_first_signaled_fence(VkDevice device,
                             const VkFence *fences,
                             uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) {
      VkResult result = vn_GetFenceStatus(device, fences[i]);
      if (result == VK_SUCCESS || result < 0)
         return result;
   }
   return VK_NOT_READY;
}

static VkResult
vn_remove_signaled_fences(VkDevice device, VkFence *fences, uint32_t *count)
{
   uint32_t cur = 0;
   for (uint32_t i = 0; i < *count; i++) {
      VkResult result = vn_GetFenceStatus(device, fences[i]);
      if (result != VK_SUCCESS) {
         if (result < 0)
            return result;
         fences[cur++] = fences[i];
      }
   }

   *count = cur;
   return cur ? VK_NOT_READY : VK_SUCCESS;
}

static VkResult
vn_update_sync_result(struct vn_device *dev,
                      VkResult result,
                      int64_t abs_timeout,
                      struct vn_relax_state *relax_state)
{
   switch (result) {
   case VK_NOT_READY:
      if (abs_timeout != OS_TIMEOUT_INFINITE &&
          os_time_get_nano() >= abs_timeout)
         result = VK_TIMEOUT;
      else
         vn_relax(relax_state);
      break;
   default:
      assert(result == VK_SUCCESS || result < 0);
      break;
   }

   return result;
}

VkResult
vn_WaitForFences(VkDevice device,
                 uint32_t fenceCount,
                 const VkFence *pFences,
                 VkBool32 waitAll,
                 uint64_t timeout)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);

   const int64_t abs_timeout = os_time_get_absolute_timeout(timeout);
   VkResult result = VK_NOT_READY;
   if (fenceCount > 1 && waitAll) {
      STACK_ARRAY(VkFence, fences, fenceCount);
      typed_memcpy(fences, pFences, fenceCount);

      struct vn_relax_state relax_state =
         vn_relax_init(dev->instance, VN_RELAX_REASON_FENCE);
      while (result == VK_NOT_READY) {
         result = vn_remove_signaled_fences(device, fences, &fenceCount);
         result =
            vn_update_sync_result(dev, result, abs_timeout, &relax_state);
      }
      vn_relax_fini(&relax_state);

      STACK_ARRAY_FINISH(fences);
   } else {
      struct vn_relax_state relax_state =
         vn_relax_init(dev->instance, VN_RELAX_REASON_FENCE);
      while (result == VK_NOT_READY) {
         result = vn_find_first_signaled_fence(device, pFences, fenceCount);
         result =
            vn_update_sync_result(dev, result, abs_timeout, &relax_state);
      }
      vn_relax_fini(&relax_state);
   }

   return vn_result(dev->instance, result);
}

static VkResult
vn_create_sync_file(struct vn_device *dev,
                    struct vn_sync_payload_external *external_payload,
                    int *out_fd)
{
   struct vn_renderer_sync *sync;
   VkResult result = vn_renderer_sync_create(dev->renderer, 0,
                                             VN_RENDERER_SYNC_BINARY, &sync);
   if (result != VK_SUCCESS)
      return vn_error(dev->instance, result);

   struct vn_renderer_submit_batch batch = {
      .syncs = &sync,
      .sync_values = &(const uint64_t){ 1 },
      .sync_count = 1,
      .ring_idx = external_payload->ring_idx,
   };

   uint32_t local_data[8];
   struct vn_cs_encoder local_enc =
      VN_CS_ENCODER_INITIALIZER_LOCAL(local_data, sizeof(local_data));
   if (external_payload->ring_seqno_valid) {
      const uint64_t ring_id = vn_ring_get_id(dev->primary_ring);
      vn_encode_vkWaitRingSeqnoMESA(&local_enc, 0, ring_id,
                                    external_payload->ring_seqno);
      batch.cs_data = local_data;
      batch.cs_size = vn_cs_encoder_get_len(&local_enc);
   }

   const struct vn_renderer_submit submit = {
      .batches = &batch,
      .batch_count = 1,
   };
   result = vn_renderer_submit(dev->renderer, &submit);
   if (result != VK_SUCCESS) {
      vn_renderer_sync_destroy(dev->renderer, sync);
      return vn_error(dev->instance, result);
   }

   *out_fd = vn_renderer_sync_export_syncobj(dev->renderer, sync, true);
   vn_renderer_sync_destroy(dev->renderer, sync);

   return *out_fd >= 0 ? VK_SUCCESS : VK_ERROR_TOO_MANY_OBJECTS;
}

static inline bool
vn_sync_valid_fd(int fd)
{
   /* the special value -1 for fd is treated like a valid sync file descriptor
    * referring to an object that has already signaled
    */
   return (fd >= 0 && sync_valid_fd(fd)) || fd == -1;
}

VkResult
vn_ImportFenceFdKHR(VkDevice device,
                    const VkImportFenceFdInfoKHR *pImportFenceFdInfo)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_fence *fence = vn_fence_from_handle(pImportFenceFdInfo->fence);
   ASSERTED const bool sync_file = pImportFenceFdInfo->handleType ==
                                   VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;
   const int fd = pImportFenceFdInfo->fd;

   assert(sync_file);

   if (!vn_sync_valid_fd(fd))
      return vn_error(dev->instance, VK_ERROR_INVALID_EXTERNAL_HANDLE);

   struct vn_sync_payload *temp = &fence->temporary;
   vn_sync_payload_release(dev, temp);
   temp->type = VN_SYNC_TYPE_IMPORTED_SYNC_FD;
   temp->fd = fd;
   fence->payload = temp;

   return VK_SUCCESS;
}

VkResult
vn_GetFenceFdKHR(VkDevice device,
                 const VkFenceGetFdInfoKHR *pGetFdInfo,
                 int *pFd)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_fence *fence = vn_fence_from_handle(pGetFdInfo->fence);
   const bool sync_file =
      pGetFdInfo->handleType == VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;
   struct vn_sync_payload *payload = fence->payload;
   VkResult result;

   assert(sync_file);
   assert(dev->physical_device->renderer_sync_fd.fence_exportable);

   int fd = -1;
   if (payload->type == VN_SYNC_TYPE_DEVICE_ONLY) {
      result = vn_create_sync_file(dev, &fence->external_payload, &fd);
      if (result != VK_SUCCESS)
         return vn_error(dev->instance, result);

      vn_async_vkResetFenceResourceMESA(dev->primary_ring, device,
                                        pGetFdInfo->fence);

      vn_sync_payload_release(dev, &fence->temporary);
      fence->payload = &fence->permanent;

#ifdef VN_USE_WSI_PLATFORM
      if (!dev->renderer->info.has_implicit_fencing)
         sync_wait(fd, -1);
#endif
   } else {
      assert(payload->type == VN_SYNC_TYPE_IMPORTED_SYNC_FD);

      /* transfer ownership of imported sync fd to save a dup */
      fd = payload->fd;
      payload->fd = -1;

      /* reset host fence in case in signaled state before import */
      result = vn_ResetFences(device, 1, &pGetFdInfo->fence);
      if (result != VK_SUCCESS) {
         /* transfer sync fd ownership back on error */
         payload->fd = fd;
         return result;
      }
   }

   *pFd = fd;
   return VK_SUCCESS;
}

/* semaphore commands */

static VkResult
vn_semaphore_init_payloads(struct vn_device *dev,
                           struct vn_semaphore *sem,
                           uint64_t initial_val,
                           const VkAllocationCallbacks *alloc)
{
   sem->permanent.type = VN_SYNC_TYPE_DEVICE_ONLY;
   sem->temporary.type = VN_SYNC_TYPE_INVALID;
   sem->payload = &sem->permanent;

   return VK_SUCCESS;
}

static bool
vn_semaphore_wait_external(struct vn_device *dev, struct vn_semaphore *sem)
{
   struct vn_sync_payload *temp = &sem->temporary;

   assert(temp->type == VN_SYNC_TYPE_IMPORTED_SYNC_FD);

   if (temp->fd >= 0) {
      if (sync_wait(temp->fd, -1))
         return false;
   }

   vn_sync_payload_release(dev, &sem->temporary);
   sem->payload = &sem->permanent;

   return true;
}

void
vn_semaphore_signal_wsi(struct vn_device *dev, struct vn_semaphore *sem)
{
   struct vn_sync_payload *temp = &sem->temporary;

   vn_sync_payload_release(dev, temp);
   temp->type = VN_SYNC_TYPE_IMPORTED_SYNC_FD;
   temp->fd = -1;
   sem->payload = temp;
}

struct vn_semaphore_feedback_cmd *
vn_semaphore_get_feedback_cmd(struct vn_device *dev, struct vn_semaphore *sem)
{
   struct vn_semaphore_feedback_cmd *sfb_cmd = NULL;

   simple_mtx_lock(&sem->feedback.cmd_mtx);
   if (!list_is_empty(&sem->feedback.free_cmds)) {
      sfb_cmd = list_first_entry(&sem->feedback.free_cmds,
                                 struct vn_semaphore_feedback_cmd, head);
      list_move_to(&sfb_cmd->head, &sem->feedback.pending_cmds);
   }
   simple_mtx_unlock(&sem->feedback.cmd_mtx);

   if (!sfb_cmd) {
      sfb_cmd = vn_semaphore_feedback_cmd_alloc(dev, sem->feedback.slot);

      simple_mtx_lock(&sem->feedback.cmd_mtx);
      list_add(&sfb_cmd->head, &sem->feedback.pending_cmds);
      simple_mtx_unlock(&sem->feedback.cmd_mtx);
   }

   return sfb_cmd;
}

static VkResult
vn_semaphore_feedback_init(struct vn_device *dev,
                           struct vn_semaphore *sem,
                           uint64_t initial_value,
                           const VkAllocationCallbacks *alloc)
{
   struct vn_feedback_slot *slot;

   assert(sem->type == VK_SEMAPHORE_TYPE_TIMELINE);

   if (sem->is_external)
      return VK_SUCCESS;

   if (VN_PERF(NO_SEMAPHORE_FEEDBACK))
      return VK_SUCCESS;

   slot =
      vn_feedback_pool_alloc(&dev->feedback_pool, VN_FEEDBACK_TYPE_SEMAPHORE);
   if (!slot)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   list_inithead(&sem->feedback.pending_cmds);
   list_inithead(&sem->feedback.free_cmds);

   vn_feedback_set_counter(slot, initial_value);

   simple_mtx_init(&sem->feedback.cmd_mtx, mtx_plain);
   simple_mtx_init(&sem->feedback.async_wait_mtx, mtx_plain);

   sem->feedback.signaled_counter = initial_value;
   sem->feedback.slot = slot;

   return VK_SUCCESS;
}

static void
vn_semaphore_feedback_fini(struct vn_device *dev, struct vn_semaphore *sem)
{
   if (!sem->feedback.slot)
      return;

   list_for_each_entry_safe(struct vn_semaphore_feedback_cmd, sfb_cmd,
                            &sem->feedback.free_cmds, head)
      vn_semaphore_feedback_cmd_free(dev, sfb_cmd);

   list_for_each_entry_safe(struct vn_semaphore_feedback_cmd, sfb_cmd,
                            &sem->feedback.pending_cmds, head)
      vn_semaphore_feedback_cmd_free(dev, sfb_cmd);

   simple_mtx_destroy(&sem->feedback.cmd_mtx);
   simple_mtx_destroy(&sem->feedback.async_wait_mtx);

   vn_feedback_pool_free(&dev->feedback_pool, sem->feedback.slot);
}

VkResult
vn_CreateSemaphore(VkDevice device,
                   const VkSemaphoreCreateInfo *pCreateInfo,
                   const VkAllocationCallbacks *pAllocator,
                   VkSemaphore *pSemaphore)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.base.alloc;

   struct vn_semaphore *sem = vk_zalloc(alloc, sizeof(*sem), VN_DEFAULT_ALIGN,
                                        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!sem)
      return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vn_object_base_init(&sem->base, VK_OBJECT_TYPE_SEMAPHORE, &dev->base);

   const VkSemaphoreTypeCreateInfo *type_info =
      vk_find_struct_const(pCreateInfo->pNext, SEMAPHORE_TYPE_CREATE_INFO);
   uint64_t initial_val = 0;
   if (type_info && type_info->semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE) {
      sem->type = VK_SEMAPHORE_TYPE_TIMELINE;
      initial_val = type_info->initialValue;
   } else {
      sem->type = VK_SEMAPHORE_TYPE_BINARY;
   }

   const struct VkExportSemaphoreCreateInfo *export_info =
      vk_find_struct_const(pCreateInfo->pNext, EXPORT_SEMAPHORE_CREATE_INFO);
   sem->is_external = export_info && export_info->handleTypes;

   VkResult result = vn_semaphore_init_payloads(dev, sem, initial_val, alloc);
   if (result != VK_SUCCESS)
      goto out_object_base_fini;

   if (sem->type == VK_SEMAPHORE_TYPE_TIMELINE) {
      result = vn_semaphore_feedback_init(dev, sem, initial_val, alloc);
      if (result != VK_SUCCESS)
         goto out_payloads_fini;
   }

   VkSemaphore sem_handle = vn_semaphore_to_handle(sem);
   vn_async_vkCreateSemaphore(dev->primary_ring, device, pCreateInfo, NULL,
                              &sem_handle);

   *pSemaphore = sem_handle;

   return VK_SUCCESS;

out_payloads_fini:
   vn_sync_payload_release(dev, &sem->permanent);
   vn_sync_payload_release(dev, &sem->temporary);

out_object_base_fini:
   vn_object_base_fini(&sem->base);
   vk_free(alloc, sem);
   return vn_error(dev->instance, result);
}

void
vn_DestroySemaphore(VkDevice device,
                    VkSemaphore semaphore,
                    const VkAllocationCallbacks *pAllocator)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_semaphore *sem = vn_semaphore_from_handle(semaphore);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.base.alloc;

   if (!sem)
      return;

   vn_async_vkDestroySemaphore(dev->primary_ring, device, semaphore, NULL);

   if (sem->type == VK_SEMAPHORE_TYPE_TIMELINE)
      vn_semaphore_feedback_fini(dev, sem);

   vn_sync_payload_release(dev, &sem->permanent);
   vn_sync_payload_release(dev, &sem->temporary);

   vn_object_base_fini(&sem->base);
   vk_free(alloc, sem);
}

VkResult
vn_GetSemaphoreCounterValue(VkDevice device,
                            VkSemaphore semaphore,
                            uint64_t *pValue)
{
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_semaphore *sem = vn_semaphore_from_handle(semaphore);
   ASSERTED struct vn_sync_payload *payload = sem->payload;

   assert(payload->type == VN_SYNC_TYPE_DEVICE_ONLY);

   if (sem->feedback.slot) {
      simple_mtx_lock(&sem->feedback.async_wait_mtx);
      const uint64_t counter = vn_feedback_get_counter(sem->feedback.slot);
      if (sem->feedback.signaled_counter < counter) {
         /* When the timeline semaphore feedback slot gets signaled, the real
          * semaphore signal operation follows after but the signaling isr can
          * be deferred or preempted. To avoid racing, we let the renderer
          * wait for the semaphore by sending an asynchronous wait call for
          * the feedback value.
          * We also cache the counter value to only send the async call once
          * per counter value to prevent spamming redundant async wait calls.
          * The cached counter value requires a lock to ensure multiple
          * threads querying for the same value are guaranteed to encode after
          * the async wait call.
          *
          * This also helps resolve synchronization validation errors, because
          * the layer no longer sees any semaphore status checks and falsely
          * believes the caller does not sync.
          */
         VkSemaphoreWaitInfo wait_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .pNext = NULL,
            .flags = 0,
            .semaphoreCount = 1,
            .pSemaphores = &semaphore,
            .pValues = &counter,
         };

         vn_async_vkWaitSemaphores(dev->primary_ring, device, &wait_info,
                                   UINT64_MAX);

         /* search pending cmds for already signaled values */
         simple_mtx_lock(&sem->feedback.cmd_mtx);
         list_for_each_entry_safe(struct vn_semaphore_feedback_cmd, sfb_cmd,
                                  &sem->feedback.pending_cmds, head) {
            if (counter >= vn_feedback_get_counter(sfb_cmd->src_slot))
               list_move_to(&sfb_cmd->head, &sem->feedback.free_cmds);
         }
         simple_mtx_unlock(&sem->feedback.cmd_mtx);

         sem->feedback.signaled_counter = counter;
      }
      simple_mtx_unlock(&sem->feedback.async_wait_mtx);

      *pValue = counter;
      return VK_SUCCESS;
   } else {
      return vn_call_vkGetSemaphoreCounterValue(dev->primary_ring, device,
                                                semaphore, pValue);
   }
}

VkResult
vn_SignalSemaphore(VkDevice device, const VkSemaphoreSignalInfo *pSignalInfo)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_semaphore *sem =
      vn_semaphore_from_handle(pSignalInfo->semaphore);

   vn_async_vkSignalSemaphore(dev->primary_ring, device, pSignalInfo);

   if (sem->feedback.slot) {
      simple_mtx_lock(&sem->feedback.async_wait_mtx);

      vn_feedback_set_counter(sem->feedback.slot, pSignalInfo->value);
      /* Update async counters. Since we're signaling, we're aligned with
       * the renderer.
       */
      sem->feedback.signaled_counter = pSignalInfo->value;

      simple_mtx_unlock(&sem->feedback.async_wait_mtx);
   }

   return VK_SUCCESS;
}

static VkResult
vn_find_first_signaled_semaphore(VkDevice device,
                                 const VkSemaphore *semaphores,
                                 const uint64_t *values,
                                 uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) {
      uint64_t val = 0;
      VkResult result =
         vn_GetSemaphoreCounterValue(device, semaphores[i], &val);
      if (result != VK_SUCCESS || val >= values[i])
         return result;
   }
   return VK_NOT_READY;
}

static VkResult
vn_remove_signaled_semaphores(VkDevice device,
                              VkSemaphore *semaphores,
                              uint64_t *values,
                              uint32_t *count)
{
   uint32_t cur = 0;
   for (uint32_t i = 0; i < *count; i++) {
      uint64_t val = 0;
      VkResult result =
         vn_GetSemaphoreCounterValue(device, semaphores[i], &val);
      if (result != VK_SUCCESS)
         return result;
      if (val < values[i])
         semaphores[cur++] = semaphores[i];
   }

   *count = cur;
   return cur ? VK_NOT_READY : VK_SUCCESS;
}

VkResult
vn_WaitSemaphores(VkDevice device,
                  const VkSemaphoreWaitInfo *pWaitInfo,
                  uint64_t timeout)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);

   const int64_t abs_timeout = os_time_get_absolute_timeout(timeout);
   VkResult result = VK_NOT_READY;
   if (pWaitInfo->semaphoreCount > 1 &&
       !(pWaitInfo->flags & VK_SEMAPHORE_WAIT_ANY_BIT)) {
      uint32_t semaphore_count = pWaitInfo->semaphoreCount;
      STACK_ARRAY(VkSemaphore, semaphores, semaphore_count);
      STACK_ARRAY(uint64_t, values, semaphore_count);
      typed_memcpy(semaphores, pWaitInfo->pSemaphores, semaphore_count);
      typed_memcpy(values, pWaitInfo->pValues, semaphore_count);

      struct vn_relax_state relax_state =
         vn_relax_init(dev->instance, VN_RELAX_REASON_SEMAPHORE);
      while (result == VK_NOT_READY) {
         result = vn_remove_signaled_semaphores(device, semaphores, values,
                                                &semaphore_count);
         result =
            vn_update_sync_result(dev, result, abs_timeout, &relax_state);
      }
      vn_relax_fini(&relax_state);

      STACK_ARRAY_FINISH(semaphores);
      STACK_ARRAY_FINISH(values);
   } else {
      struct vn_relax_state relax_state =
         vn_relax_init(dev->instance, VN_RELAX_REASON_SEMAPHORE);
      while (result == VK_NOT_READY) {
         result = vn_find_first_signaled_semaphore(
            device, pWaitInfo->pSemaphores, pWaitInfo->pValues,
            pWaitInfo->semaphoreCount);
         result =
            vn_update_sync_result(dev, result, abs_timeout, &relax_state);
      }
      vn_relax_fini(&relax_state);
   }

   return vn_result(dev->instance, result);
}

VkResult
vn_ImportSemaphoreFdKHR(
   VkDevice device, const VkImportSemaphoreFdInfoKHR *pImportSemaphoreFdInfo)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_semaphore *sem =
      vn_semaphore_from_handle(pImportSemaphoreFdInfo->semaphore);
   ASSERTED const bool sync_file =
      pImportSemaphoreFdInfo->handleType ==
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
   const int fd = pImportSemaphoreFdInfo->fd;

   assert(sync_file);

   if (!vn_sync_valid_fd(fd))
      return vn_error(dev->instance, VK_ERROR_INVALID_EXTERNAL_HANDLE);

   struct vn_sync_payload *temp = &sem->temporary;
   vn_sync_payload_release(dev, temp);
   temp->type = VN_SYNC_TYPE_IMPORTED_SYNC_FD;
   temp->fd = fd;
   sem->payload = temp;

   return VK_SUCCESS;
}

VkResult
vn_GetSemaphoreFdKHR(VkDevice device,
                     const VkSemaphoreGetFdInfoKHR *pGetFdInfo,
                     int *pFd)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_semaphore *sem = vn_semaphore_from_handle(pGetFdInfo->semaphore);
   const bool sync_file =
      pGetFdInfo->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
   struct vn_sync_payload *payload = sem->payload;

   assert(sync_file);
   assert(dev->physical_device->renderer_sync_fd.semaphore_exportable);
   assert(dev->physical_device->renderer_sync_fd.semaphore_importable);

   int fd = -1;
   if (payload->type == VN_SYNC_TYPE_DEVICE_ONLY) {
      VkResult result = vn_create_sync_file(dev, &sem->external_payload, &fd);
      if (result != VK_SUCCESS)
         return vn_error(dev->instance, result);

#ifdef VN_USE_WSI_PLATFORM
      if (!dev->renderer->info.has_implicit_fencing)
         sync_wait(fd, -1);
#endif
   } else {
      assert(payload->type == VN_SYNC_TYPE_IMPORTED_SYNC_FD);

      /* transfer ownership of imported sync fd to save a dup */
      fd = payload->fd;
      payload->fd = -1;
   }

   /* When payload->type is VN_SYNC_TYPE_IMPORTED_SYNC_FD, the current
    * payload is from a prior temporary sync_fd import. The permanent
    * payload of the sempahore might be in signaled state. So we do an
    * import here to ensure later wait operation is legit. With resourceId
    * 0, renderer does a signaled sync_fd -1 payload import on the host
    * semaphore.
    */
   if (payload->type == VN_SYNC_TYPE_IMPORTED_SYNC_FD) {
      const VkImportSemaphoreResourceInfoMESA res_info = {
         .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_RESOURCE_INFO_MESA,
         .semaphore = pGetFdInfo->semaphore,
         .resourceId = 0,
      };
      vn_async_vkImportSemaphoreResourceMESA(dev->primary_ring, device,
                                             &res_info);
   }

   /* perform wait operation on the host semaphore */
   vn_async_vkWaitSemaphoreResourceMESA(dev->primary_ring, device,
                                        pGetFdInfo->semaphore);

   vn_sync_payload_release(dev, &sem->temporary);
   sem->payload = &sem->permanent;

   *pFd = fd;
   return VK_SUCCESS;
}

/* event commands */

static VkResult
vn_event_feedback_init(struct vn_device *dev, struct vn_event *ev)
{
   struct vn_feedback_slot *slot;

   if (VN_PERF(NO_EVENT_FEEDBACK))
      return VK_SUCCESS;

   slot = vn_feedback_pool_alloc(&dev->feedback_pool, VN_FEEDBACK_TYPE_EVENT);
   if (!slot)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   /* newly created event object is in the unsignaled state */
   vn_feedback_set_status(slot, VK_EVENT_RESET);

   ev->feedback_slot = slot;

   return VK_SUCCESS;
}

static inline void
vn_event_feedback_fini(struct vn_device *dev, struct vn_event *ev)
{
   if (ev->feedback_slot)
      vn_feedback_pool_free(&dev->feedback_pool, ev->feedback_slot);
}

VkResult
vn_CreateEvent(VkDevice device,
               const VkEventCreateInfo *pCreateInfo,
               const VkAllocationCallbacks *pAllocator,
               VkEvent *pEvent)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.base.alloc;

   struct vn_event *ev = vk_zalloc(alloc, sizeof(*ev), VN_DEFAULT_ALIGN,
                                   VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!ev)
      return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vn_object_base_init(&ev->base, VK_OBJECT_TYPE_EVENT, &dev->base);

   /* feedback is only needed to speed up host operations */
   if (!(pCreateInfo->flags & VK_EVENT_CREATE_DEVICE_ONLY_BIT)) {
      VkResult result = vn_event_feedback_init(dev, ev);
      if (result != VK_SUCCESS)
         return vn_error(dev->instance, result);
   }

   VkEvent ev_handle = vn_event_to_handle(ev);
   vn_async_vkCreateEvent(dev->primary_ring, device, pCreateInfo, NULL,
                          &ev_handle);

   *pEvent = ev_handle;

   return VK_SUCCESS;
}

void
vn_DestroyEvent(VkDevice device,
                VkEvent event,
                const VkAllocationCallbacks *pAllocator)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_event *ev = vn_event_from_handle(event);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.base.alloc;

   if (!ev)
      return;

   vn_async_vkDestroyEvent(dev->primary_ring, device, event, NULL);

   vn_event_feedback_fini(dev, ev);

   vn_object_base_fini(&ev->base);
   vk_free(alloc, ev);
}

VkResult
vn_GetEventStatus(VkDevice device, VkEvent event)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_event *ev = vn_event_from_handle(event);
   VkResult result;

   if (ev->feedback_slot)
      result = vn_feedback_get_status(ev->feedback_slot);
   else
      result = vn_call_vkGetEventStatus(dev->primary_ring, device, event);

   return vn_result(dev->instance, result);
}

VkResult
vn_SetEvent(VkDevice device, VkEvent event)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_event *ev = vn_event_from_handle(event);

   if (ev->feedback_slot) {
      vn_feedback_set_status(ev->feedback_slot, VK_EVENT_SET);
      vn_async_vkSetEvent(dev->primary_ring, device, event);
   } else {
      VkResult result = vn_call_vkSetEvent(dev->primary_ring, device, event);
      if (result != VK_SUCCESS)
         return vn_error(dev->instance, result);
   }

   return VK_SUCCESS;
}

VkResult
vn_ResetEvent(VkDevice device, VkEvent event)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_event *ev = vn_event_from_handle(event);

   if (ev->feedback_slot) {
      vn_feedback_reset_status(ev->feedback_slot);
      vn_async_vkResetEvent(dev->primary_ring, device, event);
   } else {
      VkResult result =
         vn_call_vkResetEvent(dev->primary_ring, device, event);
      if (result != VK_SUCCESS)
         return vn_error(dev->instance, result);
   }

   return VK_SUCCESS;
}
