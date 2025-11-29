/*
 * Copyright © 2022 Collabora Ltd. and Red Hat Inc.
 * SPDX-License-Identifier: MIT
 */
#include "nvk_queue.h"

#include "nvk_cmd_buffer.h"
#include "nvk_cmd_pool.h"
#include "nvk_device.h"
#include "nvk_buffer.h"
#include "nvk_image.h"
#include "nvk_device_memory.h"
#include "nvk_physical_device.h"

#include "nouveau_context.h"

#include "drm-uapi/nouveau_drm.h"

#include "vk_drm_syncobj.h"

#include <xf86drm.h>

#define NVK_PUSH_MAX_SYNCS 256
#define NVK_PUSH_MAX_BINDS 4096
#define NVK_PUSH_MAX_PUSH 1024

struct push_builder {
   uint32_t max_push;
   struct drm_nouveau_sync req_wait[NVK_PUSH_MAX_SYNCS];
   struct drm_nouveau_sync req_sig[NVK_PUSH_MAX_SYNCS];
   struct drm_nouveau_exec_push req_push[NVK_PUSH_MAX_PUSH];
   struct drm_nouveau_exec req;
   struct drm_nouveau_vm_bind vmbind;
   struct drm_nouveau_vm_bind_op bind_ops[NVK_PUSH_MAX_BINDS];
   bool is_vmbind;
};

static void
push_builder_init(struct nvk_queue *queue,
                  struct push_builder *pb,
                  bool is_vmbind)
{
   struct nvk_device *dev = nvk_queue_device(queue);

   pb->max_push = is_vmbind ? 0 :
      MIN2(NVK_PUSH_MAX_PUSH, dev->ws_dev->max_push);
   pb->req = (struct drm_nouveau_exec) {
      .channel = queue->drm.ws_ctx->channel,
      .push_count = 0,
      .wait_count = 0,
      .sig_count = 0,
      .push_ptr = (uintptr_t)&pb->req_push,
      .wait_ptr = (uintptr_t)&pb->req_wait,
      .sig_ptr = (uintptr_t)&pb->req_sig,
   };
   pb->vmbind = (struct drm_nouveau_vm_bind) {
      .flags = DRM_NOUVEAU_VM_BIND_RUN_ASYNC,
      .op_count = 0,
      .op_ptr = (uintptr_t)&pb->bind_ops,
      .wait_count = 0,
      .sig_count = 0,
      .wait_ptr = (uintptr_t)&pb->req_wait,
      .sig_ptr = (uintptr_t)&pb->req_sig,
   };
   pb->is_vmbind = is_vmbind;
}

static void
push_add_syncobj_wait(struct push_builder *pb,
                      uint32_t syncobj,
                      uint64_t wait_value)
{
   assert(pb->req.wait_count < NVK_PUSH_MAX_SYNCS);
   pb->req_wait[pb->req.wait_count++] = (struct drm_nouveau_sync) {
      .flags = wait_value ? DRM_NOUVEAU_SYNC_TIMELINE_SYNCOBJ :
                            DRM_NOUVEAU_SYNC_SYNCOBJ,
      .handle = syncobj,
      .timeline_value = wait_value,
   };
}

static void
push_add_sync_wait(struct push_builder *pb,
                   struct vk_sync_wait *wait)
{
   struct vk_drm_syncobj *sync = vk_sync_as_drm_syncobj(wait->sync);
   assert(sync != NULL);
   push_add_syncobj_wait(pb, sync->syncobj, wait->wait_value);
}

static void
push_add_sync_signal(struct push_builder *pb,
                     struct vk_sync_signal *sig)
{
   struct vk_drm_syncobj *sync  = vk_sync_as_drm_syncobj(sig->sync);
   assert(sync);
   assert(pb->req.sig_count < NVK_PUSH_MAX_SYNCS);
   pb->req_sig[pb->req.sig_count++] = (struct drm_nouveau_sync) {
      .flags = sig->signal_value ? DRM_NOUVEAU_SYNC_TIMELINE_SYNCOBJ :
                                   DRM_NOUVEAU_SYNC_SYNCOBJ,
      .handle = sync->syncobj,
      .timeline_value = sig->signal_value,
   };
}

static void
push_bind(struct push_builder *pb, const struct drm_nouveau_vm_bind_op *bind)
{
   if (pb->vmbind.op_count > 0) {
      struct drm_nouveau_vm_bind_op *prev_bind =
         &pb->bind_ops[pb->vmbind.op_count - 1];

      /* Try to coalesce bind ops together if we can */
      if (bind->op == prev_bind->op &&
          bind->flags == prev_bind->flags &&
          bind->handle == prev_bind->handle &&
          bind->addr == prev_bind->addr + prev_bind->range &&
          bind->bo_offset == prev_bind->bo_offset + prev_bind->range) {
         prev_bind->range += bind->range;
         return;
      }
   }

   assert(pb->vmbind.op_count < NVK_PUSH_MAX_BINDS);
   pb->bind_ops[pb->vmbind.op_count++] = *bind;
}

static void
push_add_buffer_bind(struct push_builder *pb,
                     VkSparseBufferMemoryBindInfo *bind_info)
{
   VK_FROM_HANDLE(nvk_buffer, buffer, bind_info->buffer);
   for (unsigned i = 0; i < bind_info->bindCount; i++) {
      const VkSparseMemoryBind *bind = &bind_info->pBinds[i];
      VK_FROM_HANDLE(nvk_device_memory, mem, bind->memory);

      assert(bind->resourceOffset + bind->size <= buffer->vma_size_B);
      assert(!mem || bind->memoryOffset + bind->size <= mem->vk.size);

      push_bind(pb, &(struct drm_nouveau_vm_bind_op) {
         .op = mem ? DRM_NOUVEAU_VM_BIND_OP_MAP :
                     DRM_NOUVEAU_VM_BIND_OP_UNMAP,
         .handle = mem ? mem->bo->handle : 0,
         .addr = buffer->addr + bind->resourceOffset,
         .bo_offset = bind->memoryOffset,
         .range = bind->size,
      });
   }
}

static void
push_add_image_plane_bind(struct push_builder *pb,
                          const struct nvk_image_plane *plane,
                          const VkSparseImageMemoryBind *bind)
{
   VK_FROM_HANDLE(nvk_device_memory, mem, bind->memory);
   uint64_t image_bind_offset_B;

   const uint64_t mem_bind_offset_B = bind->memoryOffset;
   const uint32_t layer = bind->subresource.arrayLayer;
   const uint32_t level = bind->subresource.mipLevel;

   const struct nil_tiling plane_tiling = plane->nil.levels[level].tiling;
   const uint32_t tile_size_B = nil_tiling_size_B(&plane_tiling);

   const struct nil_Extent4D_Pixels bind_extent_px = {
      .width = bind->extent.width,
      .height = bind->extent.height,
      .depth = bind->extent.depth,
      .array_len = 1,
   };
   const struct nil_Offset4D_Pixels bind_offset_px = {
      .x = bind->offset.x,
      .y = bind->offset.y,
      .z = bind->offset.z,
      .a = layer,
   };

   const struct nil_Extent4D_Pixels level_extent_px =
      nil_image_level_extent_px(&plane->nil, level);
   const struct nil_Extent4D_Tiles level_extent_tl =
      nil_extent4d_px_to_tl(level_extent_px, &plane_tiling,
                            plane->nil.format,
                            plane->nil.sample_layout);

   /* Convert the extent and offset to tiles */
   const struct nil_Extent4D_Tiles bind_extent_tl =
      nil_extent4d_px_to_tl(bind_extent_px, &plane_tiling,
                            plane->nil.format,
                            plane->nil.sample_layout);
   const struct nil_Offset4D_Tiles bind_offset_tl =
      nil_offset4d_px_to_tl(bind_offset_px, &plane_tiling,
                            plane->nil.format,
                            plane->nil.sample_layout);


   image_bind_offset_B =
      nil_image_level_layer_offset_B(&plane->nil, level, layer);

   /* We can only bind contiguous ranges, so we'll split the image into rows
    * of tiles that are guaranteed to be contiguous, and bind in terms of
    * these rows
    */

   /* First, get the size of the bind. Since we have the extent in terms of
    * tiles already, we just need to multiply that by the tile size to get
    * the size in bytes
    */
   uint64_t row_bind_size_B = bind_extent_tl.width * tile_size_B;

   /* Second, start walking the binding region in units of tiles, starting
    * from the third dimension
    */
   for (uint32_t z_tl = 0; z_tl < bind_extent_tl.depth; z_tl++) {
      /* Start walking the rows to be bound */
      for (uint32_t y_tl = 0; y_tl < bind_extent_tl.height; y_tl++) {
         /* For the bind offset, get a memory offset to the start of the row
          * in terms of the bind extent
          */
         const uint64_t mem_row_start_tl =
            y_tl * bind_extent_tl.width +
            z_tl * bind_extent_tl.width * bind_extent_tl.height;

         const uint32_t image_x_tl = bind_offset_tl.x;
         const uint32_t image_y_tl = bind_offset_tl.y + y_tl;
         const uint32_t image_z_tl = bind_offset_tl.z + z_tl;

         /* The image offset is calculated in terms of the level extent */
         const uint64_t image_row_start_tl =
            image_x_tl +
            image_y_tl * level_extent_tl.width +
            image_z_tl * level_extent_tl.width * level_extent_tl.height;

         push_bind(pb, &(struct drm_nouveau_vm_bind_op) {
            .op = mem ? DRM_NOUVEAU_VM_BIND_OP_MAP :
                        DRM_NOUVEAU_VM_BIND_OP_UNMAP,
            .handle = mem ? mem->bo->handle : 0,
            .addr = plane->addr + image_bind_offset_B +
                    image_row_start_tl * tile_size_B,
            .bo_offset = mem_bind_offset_B +
                         mem_row_start_tl * tile_size_B,
            .range = row_bind_size_B,
            .flags = plane->nil.pte_kind,
         });
      }
   }
}

static void
push_add_image_bind(struct push_builder *pb,
                    VkSparseImageMemoryBindInfo *bind_info)
{
   VK_FROM_HANDLE(nvk_image, image, bind_info->image);
   /* Sparse residency with multiplane is currently not supported */
   assert(image->plane_count == 1);
   for (unsigned i = 0; i < bind_info->bindCount; i++) {
      push_add_image_plane_bind(pb, &image->planes[0],
                                &bind_info->pBinds[i]);
   }
}

static bool
next_opaque_bind_plane(const VkSparseMemoryBind *bind,
                       uint64_t size_B, uint32_t align_B,
                       uint64_t *plane_offset_B,
                       uint64_t *mem_offset_B,
                       uint64_t *bind_size_B,
                       uint64_t *image_plane_offset_B_iter)
{
   /* Figure out the offset to thise plane and increment _iter up-front so
    * that we're free to early return elsewhere in the function.
    */
   *image_plane_offset_B_iter = align64(*image_plane_offset_B_iter, align_B);
   const uint64_t image_plane_offset_B = *image_plane_offset_B_iter;
   *image_plane_offset_B_iter += size_B;

   /* Offset into the image or image mip tail, as appropriate */
   uint64_t bind_offset_B = bind->resourceOffset;
   if (bind_offset_B >= NVK_MIP_TAIL_START_OFFSET)
      bind_offset_B -= NVK_MIP_TAIL_START_OFFSET;

   if (bind_offset_B < image_plane_offset_B) {
      /* The offset of the plane within the bind */
      const uint64_t bind_plane_offset_B =
         image_plane_offset_B - bind_offset_B;

      /* If this plane lies above the bound range, skip this plane */
      if (bind_plane_offset_B >= bind->size)
         return false;

      *plane_offset_B = 0;
      *mem_offset_B = bind->memoryOffset + bind_plane_offset_B;
      *bind_size_B = MIN2(bind->size - bind_plane_offset_B, size_B);
   } else {
      /* The offset of the bind within the plane */
      const uint64_t plane_bind_offset_B =
         bind_offset_B - image_plane_offset_B;

      /* If this plane lies below the bound range, skip this plane */
      if (plane_bind_offset_B >= size_B)
         return false;

      *plane_offset_B = plane_bind_offset_B;
      *mem_offset_B = bind->memoryOffset;
      *bind_size_B = MIN2(bind->size, size_B - plane_bind_offset_B);
   }

   return true;
}

static void
push_add_image_plane_opaque_bind(struct push_builder *pb,
                                 const struct nvk_image *image,
                                 const struct nvk_image_plane *plane,
                                 const VkSparseMemoryBind *bind,
                                 uint64_t *image_plane_offset_B)
{
   uint64_t plane_size_B, plane_align_B;
   nvk_image_plane_size_align_B(image, plane, &plane_size_B, &plane_align_B);

   uint64_t plane_offset_B, mem_offset_B, bind_size_B;
   if (!next_opaque_bind_plane(bind, plane_size_B, plane_align_B,
                               &plane_offset_B, &mem_offset_B, &bind_size_B,
                               image_plane_offset_B))
      return;

   VK_FROM_HANDLE(nvk_device_memory, mem, bind->memory);

   assert(plane->vma_size_B == plane->nil.size_B);
   assert(plane_offset_B + bind_size_B <= plane->vma_size_B);
   assert(!mem || mem_offset_B + bind_size_B <= mem->vk.size);

   push_bind(pb, &(struct drm_nouveau_vm_bind_op) {
      .op = mem ? DRM_NOUVEAU_VM_BIND_OP_MAP :
                  DRM_NOUVEAU_VM_BIND_OP_UNMAP,
      .handle = mem ? mem->bo->handle : 0,
      .addr = plane->addr + plane_offset_B,
      .bo_offset = mem_offset_B,
      .range = bind_size_B,
      .flags = plane->nil.pte_kind,
   });
}

static void
push_add_image_plane_mip_tail_bind(struct push_builder *pb,
                                   const struct nvk_image *image,
                                   const struct nvk_image_plane *plane,
                                   const VkSparseMemoryBind *bind,
                                   uint64_t *image_plane_offset_B)
{
   uint64_t plane_size_B, plane_align_B;
   nvk_image_plane_size_align_B(image, plane, &plane_size_B, &plane_align_B);

   const uint64_t mip_tail_offset_B =
      nil_image_mip_tail_offset_B(&plane->nil);
   const uint64_t mip_tail_size_B =
      nil_image_mip_tail_size_B(&plane->nil);
   const uint64_t mip_tail_stride_B = plane->nil.array_stride_B;

   const uint64_t whole_mip_tail_size_B =
      mip_tail_size_B * plane->nil.extent_px.array_len;

   uint64_t plane_offset_B, mem_offset_B, bind_size_B;
   if (!next_opaque_bind_plane(bind, whole_mip_tail_size_B, plane_align_B,
                               &plane_offset_B, &mem_offset_B, &bind_size_B,
                               image_plane_offset_B))
      return;

   VK_FROM_HANDLE(nvk_device_memory, mem, bind->memory);

   /* Range within the virtual mip_tail space */
   const uint64_t mip_bind_start_B = plane_offset_B;
   const uint64_t mip_bind_end_B = mip_bind_start_B + bind_size_B;

   /* Range of array slices covered by this bind */
   const uint32_t start_a = mip_bind_start_B / mip_tail_size_B;
   const uint32_t end_a = DIV_ROUND_UP(mip_bind_end_B, mip_tail_size_B);

   for (uint32_t a = start_a; a < end_a; a++) {
      /* Range within the virtual mip_tail space of this array slice */
      const uint64_t a_mip_bind_start_B =
         MAX2(a * mip_tail_size_B, mip_bind_start_B);
      const uint64_t a_mip_bind_end_B =
         MIN2((a + 1) * mip_tail_size_B, mip_bind_end_B);

      /* Offset and range within this mip_tail slice */
      const uint64_t a_offset_B = a_mip_bind_start_B - a * mip_tail_size_B;
      const uint64_t a_range_B = a_mip_bind_end_B - a_mip_bind_start_B;

      /* Offset within the current bind operation */
      const uint64_t a_bind_offset_B =
         a_mip_bind_start_B - mip_bind_start_B;

      /* Offset within the image */
      const uint64_t a_image_offset_B =
         mip_tail_offset_B + (a * mip_tail_stride_B) + a_offset_B;

      push_bind(pb, &(struct drm_nouveau_vm_bind_op) {
         .op = mem ? DRM_NOUVEAU_VM_BIND_OP_MAP :
                     DRM_NOUVEAU_VM_BIND_OP_UNMAP,
         .handle = mem ? mem->bo->handle : 0,
         .addr = plane->addr + a_image_offset_B,
         .bo_offset = mem_offset_B + a_bind_offset_B,
         .range = a_range_B,
         .flags = plane->nil.pte_kind,
      });
   }
}

static void
push_add_image_opaque_bind(struct push_builder *pb,
                           VkSparseImageOpaqueMemoryBindInfo *bind_info)
{
   VK_FROM_HANDLE(nvk_image, image, bind_info->image);
   for (unsigned i = 0; i < bind_info->bindCount; i++) {
      const VkSparseMemoryBind *bind = &bind_info->pBinds[i];

      uint64_t image_plane_offset_B = 0;
      for (unsigned plane = 0; plane < image->plane_count; plane++) {
         if (bind->resourceOffset >= NVK_MIP_TAIL_START_OFFSET) {
            push_add_image_plane_mip_tail_bind(pb, image, &image->planes[plane],
                                               bind, &image_plane_offset_B);
         } else {
            push_add_image_plane_opaque_bind(pb, image, &image->planes[plane],
                                             bind, &image_plane_offset_B);
         }
      }
      if (image->stencil_copy_temp.nil.size_B > 0) {
         push_add_image_plane_opaque_bind(pb, image, &image->stencil_copy_temp,
                                          bind, &image_plane_offset_B);
      }
   }
}

static void
push_add_push(struct push_builder *pb, uint64_t addr, uint32_t range,
              bool no_prefetch)
{
   /* This is the hardware limit on all current GPUs */
   assert((addr % 4) == 0 && (range % 4) == 0);
   assert(range < (1u << 23));

   uint32_t flags = 0;
   if (no_prefetch)
      flags |= DRM_NOUVEAU_EXEC_PUSH_NO_PREFETCH;

   assert(pb->req.push_count < pb->max_push);
   pb->req_push[pb->req.push_count++] = (struct drm_nouveau_exec_push) {
      .va = addr,
      .va_len = range,
      .flags = flags,
   };
}

static VkResult
bind_submit(struct nvk_queue *queue, struct push_builder *pb, bool sync)
{
   struct nvk_device *dev = nvk_queue_device(queue);
   int err;

   pb->vmbind.wait_count = pb->req.wait_count;
   pb->vmbind.sig_count = pb->req.sig_count;
   err = drmCommandWriteRead(dev->ws_dev->fd,
                             DRM_NOUVEAU_VM_BIND,
                             &pb->vmbind, sizeof(pb->vmbind));
   if (err) {
      return vk_errorf(queue, VK_ERROR_UNKNOWN,
                       "DRM_NOUVEAU_VM_BIND failed: %m");
   }
   return VK_SUCCESS;
}

static VkResult
push_submit(struct nvk_queue *queue, struct push_builder *pb, bool sync)
{
   struct nvk_device *dev = nvk_queue_device(queue);

   int err;
   if (sync) {
      assert(pb->req.sig_count < NVK_PUSH_MAX_SYNCS);
      pb->req_sig[pb->req.sig_count++] = (struct drm_nouveau_sync) {
         .flags = DRM_NOUVEAU_SYNC_SYNCOBJ,
         .handle = queue->drm.syncobj,
         .timeline_value = 0,
      };
   }
   err = drmCommandWriteRead(dev->ws_dev->fd,
                             DRM_NOUVEAU_EXEC,
                             &pb->req, sizeof(pb->req));
   if (err) {
      VkResult result = VK_ERROR_UNKNOWN;
      if (err == -ENODEV)
         result = VK_ERROR_DEVICE_LOST;
      return vk_errorf(queue, result,
                       "DRM_NOUVEAU_EXEC failed: %m");
   }
   if (sync) {
      err = drmSyncobjWait(dev->ws_dev->fd,
                           &queue->drm.syncobj, 1, INT64_MAX,
                           DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT,
                           NULL);
      if (err) {
         return vk_errorf(queue, VK_ERROR_UNKNOWN,
                          "DRM_SYNCOBJ_WAIT failed: %m");
      }

      /* Push an empty again, just to check for errors */
      struct drm_nouveau_exec empty = {
         .channel = pb->req.channel,
      };
      err = drmCommandWriteRead(dev->ws_dev->fd,
                                DRM_NOUVEAU_EXEC,
                                &empty, sizeof(empty));
      if (err) {
         return vk_errorf(queue, VK_ERROR_DEVICE_LOST,
                          "DRM_NOUVEAU_EXEC failed: %m");
      }
   }
   return VK_SUCCESS;
}

VkResult
nvk_queue_init_drm_nouveau(struct nvk_device *dev,
                           struct nvk_queue *queue,
                           VkQueueFlags queue_flags)
{
   VkResult result;
   int err;

   enum nouveau_ws_engines engines = 0;
   if (queue_flags & VK_QUEUE_GRAPHICS_BIT)
      engines |= NOUVEAU_WS_ENGINE_3D;
   if (queue_flags & VK_QUEUE_COMPUTE_BIT)
      engines |= NOUVEAU_WS_ENGINE_COMPUTE;
   if (queue_flags & VK_QUEUE_TRANSFER_BIT)
      engines |= NOUVEAU_WS_ENGINE_COPY;

   err = nouveau_ws_context_create(dev->ws_dev, engines, &queue->drm.ws_ctx);
   if (err != 0) {
      if (err == -ENOSPC)
         return vk_error(dev, VK_ERROR_TOO_MANY_OBJECTS);
      else
         return vk_error(dev, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   err = drmSyncobjCreate(dev->ws_dev->fd, 0, &queue->drm.syncobj);
   if (err < 0) {
      result = vk_error(dev, VK_ERROR_OUT_OF_HOST_MEMORY);
      goto fail_context;
   }

   return VK_SUCCESS;

fail_context:
   nouveau_ws_context_destroy(queue->drm.ws_ctx);

   return result;
}

void
nvk_queue_finish_drm_nouveau(struct nvk_device *dev,
                             struct nvk_queue *queue)
{
   ASSERTED int err = drmSyncobjDestroy(dev->ws_dev->fd, queue->drm.syncobj);
   assert(err == 0);
   nouveau_ws_context_destroy(queue->drm.ws_ctx);
}

VkResult
nvk_queue_submit_simple_drm_nouveau(struct nvk_queue *queue,
                                    uint32_t push_dw_count,
                                    struct nouveau_ws_bo *push_bo,
                                    uint32_t extra_bo_count,
                                    struct nouveau_ws_bo **extra_bos)
{
   struct push_builder pb;
   push_builder_init(queue, &pb, false);

   push_add_push(&pb, push_bo->offset, push_dw_count * 4, false);

   return push_submit(queue, &pb, true);
}

static void
push_add_queue_state(struct push_builder *pb, struct nvk_queue_state *qs)
{
   if (qs->push.bo)
      push_add_push(pb, qs->push.bo->offset, qs->push.dw_count * 4, false);
}

VkResult
nvk_queue_submit_drm_nouveau(struct nvk_queue *queue,
                             struct vk_queue_submit *submit,
                             bool sync)
{
   struct nvk_device *dev = nvk_queue_device(queue);
   struct push_builder pb;
   VkResult result;

   uint64_t upload_time_point;
   result = nvk_upload_queue_flush(dev, &dev->upload, &upload_time_point);
   if (result != VK_SUCCESS)
      return result;

   const bool is_vmbind = submit->buffer_bind_count > 0 ||
                          submit->image_bind_count > 0  ||
                          submit->image_opaque_bind_count > 0;
   push_builder_init(queue, &pb, is_vmbind);

   if (!is_vmbind && upload_time_point > 0)
      push_add_syncobj_wait(&pb, dev->upload.drm.syncobj, upload_time_point);

   for (uint32_t i = 0; i < submit->wait_count; i++)
      push_add_sync_wait(&pb, &submit->waits[i]);

   if (is_vmbind) {
      assert(submit->command_buffer_count == 0);

      for (uint32_t i = 0; i < submit->buffer_bind_count; i++)
         push_add_buffer_bind(&pb, &submit->buffer_binds[i]);

      for (uint32_t i = 0; i < submit->image_bind_count; i++)
         push_add_image_bind(&pb, &submit->image_binds[i]);

      for (uint32_t i = 0; i < submit->image_opaque_bind_count; i++)
         push_add_image_opaque_bind(&pb, &submit->image_opaque_binds[i]);
   } else if (submit->command_buffer_count > 0) {
      assert(submit->buffer_bind_count == 0);
      assert(submit->image_bind_count == 0);
      assert(submit->image_opaque_bind_count == 0);

      push_add_queue_state(&pb, &queue->state);

      for (unsigned i = 0; i < submit->command_buffer_count; i++) {
         struct nvk_cmd_buffer *cmd =
            container_of(submit->command_buffers[i], struct nvk_cmd_buffer, vk);

         util_dynarray_foreach(&cmd->pushes, struct nvk_cmd_push, push) {
            if (push->range == 0)
               continue;

            if (pb.req.push_count >= pb.max_push) {
               result = push_submit(queue, &pb, sync);
               if (result != VK_SUCCESS)
                  return result;

               push_builder_init(queue, &pb, is_vmbind);
            }

            push_add_push(&pb, push->addr, push->range, push->no_prefetch);
         }
      }
   }

   for (uint32_t i = 0; i < submit->signal_count; i++)
      push_add_sync_signal(&pb, &submit->signals[i]);

   if (is_vmbind)
      return bind_submit(queue, &pb, sync);
   else
      return push_submit(queue, &pb, sync);
}
