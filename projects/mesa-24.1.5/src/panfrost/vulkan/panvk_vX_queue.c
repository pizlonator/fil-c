/*
 * Copyright © 2021 Collabora Ltd.
 *
 * Derived from tu_device.c which is:
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "genxml/gen_macros.h"

#include "decode.h"

#include "panvk_cmd_buffer.h"
#include "panvk_device.h"
#include "panvk_entrypoints.h"
#include "panvk_event.h"
#include "panvk_image.h"
#include "panvk_image_view.h"
#include "panvk_instance.h"
#include "panvk_physical_device.h"
#include "panvk_priv_bo.h"
#include "panvk_queue.h"

#include "vk_drm_syncobj.h"
#include "vk_framebuffer.h"

#include "drm-uapi/panfrost_drm.h"

static void
panvk_queue_submit_batch(struct panvk_queue *queue, struct panvk_batch *batch,
                         uint32_t *bos, unsigned nr_bos, uint32_t *in_fences,
                         unsigned nr_in_fences)
{
   struct panvk_device *dev = to_panvk_device(queue->vk.base.device);
   struct panvk_physical_device *phys_dev =
      to_panvk_physical_device(dev->vk.physical);
   struct panvk_instance *instance =
      to_panvk_instance(dev->vk.physical->instance);
   unsigned debug = instance->debug_flags;
   int ret;

   /* Reset the batch if it's already been issued */
   if (batch->issued) {
      util_dynarray_foreach(&batch->jobs, void *, job)
         memset((*job), 0, 4 * 4);

      /* Reset the tiler before re-issuing the batch */
      if (batch->tiler.ctx_desc.cpu) {
         memcpy(batch->tiler.heap_desc.cpu, &batch->tiler.heap_templ,
                sizeof(batch->tiler.heap_templ));
         memcpy(batch->tiler.ctx_desc.cpu, &batch->tiler.ctx_templ,
                sizeof(batch->tiler.ctx_templ));
      }
   }

   if (batch->jc.first_job) {
      struct drm_panfrost_submit submit = {
         .bo_handles = (uintptr_t)bos,
         .bo_handle_count = nr_bos,
         .in_syncs = (uintptr_t)in_fences,
         .in_sync_count = nr_in_fences,
         .out_sync = queue->sync,
         .jc = batch->jc.first_job,
      };

      ret = drmIoctl(dev->vk.drm_fd, DRM_IOCTL_PANFROST_SUBMIT, &submit);
      assert(!ret);

      if (debug & (PANVK_DEBUG_TRACE | PANVK_DEBUG_SYNC)) {
         ret = drmSyncobjWait(dev->vk.drm_fd, &submit.out_sync, 1, INT64_MAX, 0,
                              NULL);
         assert(!ret);
      }

      if (debug & PANVK_DEBUG_TRACE) {
         pandecode_jc(dev->debug.decode_ctx, batch->jc.first_job,
                      phys_dev->kmod.props.gpu_prod_id);
      }

      if (debug & PANVK_DEBUG_SYNC)
         pandecode_abort_on_fault(dev->debug.decode_ctx, submit.jc,
                                  phys_dev->kmod.props.gpu_prod_id);

      if (debug & PANVK_DEBUG_DUMP)
         pandecode_dump_mappings(dev->debug.decode_ctx);
   }

   if (batch->fragment_job) {
      struct drm_panfrost_submit submit = {
         .bo_handles = (uintptr_t)bos,
         .bo_handle_count = nr_bos,
         .out_sync = queue->sync,
         .jc = batch->fragment_job,
         .requirements = PANFROST_JD_REQ_FS,
      };

      if (batch->jc.first_job) {
         submit.in_syncs = (uintptr_t)(&queue->sync);
         submit.in_sync_count = 1;
      } else {
         submit.in_syncs = (uintptr_t)in_fences;
         submit.in_sync_count = nr_in_fences;
      }

      ret = drmIoctl(dev->vk.drm_fd, DRM_IOCTL_PANFROST_SUBMIT, &submit);
      assert(!ret);
      if (debug & (PANVK_DEBUG_TRACE | PANVK_DEBUG_SYNC)) {
         ret = drmSyncobjWait(dev->vk.drm_fd, &submit.out_sync, 1, INT64_MAX, 0,
                              NULL);
         assert(!ret);
      }

      if (debug & PANVK_DEBUG_TRACE)
         pandecode_jc(dev->debug.decode_ctx, batch->fragment_job,
                      phys_dev->kmod.props.gpu_prod_id);

      if (debug & PANVK_DEBUG_DUMP)
         pandecode_dump_mappings(dev->debug.decode_ctx);

      if (debug & PANVK_DEBUG_SYNC)
         pandecode_abort_on_fault(dev->debug.decode_ctx, submit.jc,
                                  phys_dev->kmod.props.gpu_prod_id);
   }

   if (debug & PANVK_DEBUG_TRACE)
      pandecode_next_frame(dev->debug.decode_ctx);

   batch->issued = true;
}

static void
panvk_queue_transfer_sync(struct panvk_queue *queue, uint32_t syncobj)
{
   struct panvk_device *dev = to_panvk_device(queue->vk.base.device);
   int ret;

   struct drm_syncobj_handle handle = {
      .handle = queue->sync,
      .flags = DRM_SYNCOBJ_HANDLE_TO_FD_FLAGS_EXPORT_SYNC_FILE,
      .fd = -1,
   };

   ret = drmIoctl(dev->vk.drm_fd, DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD, &handle);
   assert(!ret);
   assert(handle.fd >= 0);

   handle.handle = syncobj;
   ret = drmIoctl(dev->vk.drm_fd, DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE, &handle);
   assert(!ret);

   close(handle.fd);
}

static void
panvk_add_wait_event_syncobjs(struct panvk_batch *batch, uint32_t *in_fences,
                              unsigned *nr_in_fences)
{
   util_dynarray_foreach(&batch->event_ops, struct panvk_cmd_event_op, op) {
      switch (op->type) {
      case PANVK_EVENT_OP_SET:
         /* Nothing to do yet */
         break;
      case PANVK_EVENT_OP_RESET:
         /* Nothing to do yet */
         break;
      case PANVK_EVENT_OP_WAIT:
         in_fences[(*nr_in_fences)++] = op->event->syncobj;
         break;
      default:
         unreachable("bad panvk_cmd_event_op type\n");
      }
   }
}

static void
panvk_signal_event_syncobjs(struct panvk_queue *queue,
                            struct panvk_batch *batch)
{
   struct panvk_device *dev = to_panvk_device(queue->vk.base.device);

   util_dynarray_foreach(&batch->event_ops, struct panvk_cmd_event_op, op) {
      switch (op->type) {
      case PANVK_EVENT_OP_SET: {
         panvk_queue_transfer_sync(queue, op->event->syncobj);
         break;
      }
      case PANVK_EVENT_OP_RESET: {
         struct panvk_event *event = op->event;

         struct drm_syncobj_array objs = {
            .handles = (uint64_t)(uintptr_t)&event->syncobj,
            .count_handles = 1};

         int ret = drmIoctl(dev->vk.drm_fd, DRM_IOCTL_SYNCOBJ_RESET, &objs);
         assert(!ret);
         break;
      }
      case PANVK_EVENT_OP_WAIT:
         /* Nothing left to do */
         break;
      default:
         unreachable("bad panvk_cmd_event_op type\n");
      }
   }
}

static VkResult
panvk_queue_submit(struct vk_queue *vk_queue, struct vk_queue_submit *submit)
{
   struct panvk_queue *queue = container_of(vk_queue, struct panvk_queue, vk);
   struct panvk_device *dev = to_panvk_device(queue->vk.base.device);

   unsigned nr_semaphores = submit->wait_count + 1;
   uint32_t semaphores[nr_semaphores];

   semaphores[0] = queue->sync;
   for (unsigned i = 0; i < submit->wait_count; i++) {
      assert(vk_sync_type_is_drm_syncobj(submit->waits[i].sync->type));
      struct vk_drm_syncobj *syncobj =
         vk_sync_as_drm_syncobj(submit->waits[i].sync);

      semaphores[i + 1] = syncobj->syncobj;
   }

   for (uint32_t j = 0; j < submit->command_buffer_count; ++j) {
      struct panvk_cmd_buffer *cmdbuf =
         container_of(submit->command_buffers[j], struct panvk_cmd_buffer, vk);

      list_for_each_entry(struct panvk_batch, batch, &cmdbuf->batches, node) {
         /* FIXME: should be done at the batch level */
         unsigned nr_bos = panvk_pool_num_bos(&cmdbuf->desc_pool) +
                           panvk_pool_num_bos(&cmdbuf->varying_pool) +
                           panvk_pool_num_bos(&cmdbuf->tls_pool) +
                           batch->fb.bo_count + (batch->blit.src ? 1 : 0) +
                           (batch->blit.dst ? 1 : 0) +
                           (batch->jc.first_tiler ? 1 : 0) + 1;
         unsigned bo_idx = 0;
         uint32_t bos[nr_bos];

         panvk_pool_get_bo_handles(&cmdbuf->desc_pool, &bos[bo_idx]);
         bo_idx += panvk_pool_num_bos(&cmdbuf->desc_pool);

         panvk_pool_get_bo_handles(&cmdbuf->varying_pool, &bos[bo_idx]);
         bo_idx += panvk_pool_num_bos(&cmdbuf->varying_pool);

         panvk_pool_get_bo_handles(&cmdbuf->tls_pool, &bos[bo_idx]);
         bo_idx += panvk_pool_num_bos(&cmdbuf->tls_pool);

         for (unsigned i = 0; i < batch->fb.bo_count; i++)
            bos[bo_idx++] = pan_kmod_bo_handle(batch->fb.bos[i]);

         if (batch->blit.src)
            bos[bo_idx++] = pan_kmod_bo_handle(batch->blit.src);

         if (batch->blit.dst)
            bos[bo_idx++] = pan_kmod_bo_handle(batch->blit.dst);

         if (batch->jc.first_tiler)
            bos[bo_idx++] = pan_kmod_bo_handle(dev->tiler_heap->bo);

         bos[bo_idx++] = pan_kmod_bo_handle(dev->sample_positions->bo);
         assert(bo_idx == nr_bos);

         /* Merge identical BO entries. */
         for (unsigned x = 0; x < nr_bos; x++) {
            for (unsigned y = x + 1; y < nr_bos;) {
               if (bos[x] == bos[y])
                  bos[y] = bos[--nr_bos];
               else
                  y++;
            }
         }

         unsigned nr_in_fences = 0;
         unsigned max_wait_event_syncobjs = util_dynarray_num_elements(
            &batch->event_ops, struct panvk_cmd_event_op);
         uint32_t in_fences[nr_semaphores + max_wait_event_syncobjs];
         memcpy(in_fences, semaphores, nr_semaphores * sizeof(*in_fences));
         nr_in_fences += nr_semaphores;

         panvk_add_wait_event_syncobjs(batch, in_fences, &nr_in_fences);

         panvk_queue_submit_batch(queue, batch, bos, nr_bos, in_fences,
                                  nr_in_fences);

         panvk_signal_event_syncobjs(queue, batch);
      }
   }

   /* Transfer the out fence to signal semaphores */
   for (unsigned i = 0; i < submit->signal_count; i++) {
      assert(vk_sync_type_is_drm_syncobj(submit->signals[i].sync->type));
      struct vk_drm_syncobj *syncobj =
         vk_sync_as_drm_syncobj(submit->signals[i].sync);

      panvk_queue_transfer_sync(queue, syncobj->syncobj);
   }

   return VK_SUCCESS;
}

VkResult
panvk_per_arch(queue_init)(struct panvk_device *device,
                           struct panvk_queue *queue, int idx,
                           const VkDeviceQueueCreateInfo *create_info)
{
   VkResult result = vk_queue_init(&queue->vk, &device->vk, create_info, idx);
   if (result != VK_SUCCESS)
      return result;

   int ret = drmSyncobjCreate(device->vk.drm_fd, DRM_SYNCOBJ_CREATE_SIGNALED,
                              &queue->sync);
   if (ret) {
      vk_queue_finish(&queue->vk);
      return VK_ERROR_OUT_OF_HOST_MEMORY;
   }

   queue->vk.driver_submit = panvk_queue_submit;
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_per_arch(QueueWaitIdle)(VkQueue _queue)
{
   VK_FROM_HANDLE(panvk_queue, queue, _queue);
   struct panvk_device *dev = panvk_queue_get_device(queue);

   if (vk_device_is_lost(&dev->vk))
      return VK_ERROR_DEVICE_LOST;

   int ret = drmSyncobjWait(queue->vk.base.device->drm_fd, &queue->sync, 1,
                            INT64_MAX, DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL, NULL);
   assert(!ret);

   return VK_SUCCESS;
}
