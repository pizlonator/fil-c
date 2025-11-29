/*
 * Copyright © 2023 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <sys/mman.h>

#include "common/xe/intel_engine.h"

#include "anv_private.h"

#include "xe/anv_batch_chain.h"

#include "drm-uapi/gpu_scheduler.h"
#include "drm-uapi/xe_drm.h"

static uint32_t
xe_gem_create(struct anv_device *device,
              const struct intel_memory_class_instance **regions,
              uint16_t regions_count, uint64_t size,
              enum anv_bo_alloc_flags alloc_flags,
              uint64_t *actual_size)
{
   /* TODO: protected content */
   assert((alloc_flags & ANV_BO_ALLOC_PROTECTED) == 0);
   /* WB+0 way coherent not supported by Xe KMD */
   assert((alloc_flags & ANV_BO_ALLOC_HOST_CACHED) == 0 ||
          (alloc_flags & ANV_BO_ALLOC_HOST_CACHED_COHERENT) == ANV_BO_ALLOC_HOST_CACHED_COHERENT);

   uint32_t flags = 0;
   if (alloc_flags & ANV_BO_ALLOC_SCANOUT)
      flags |= DRM_XE_GEM_CREATE_FLAG_SCANOUT;
   if ((alloc_flags & (ANV_BO_ALLOC_MAPPED | ANV_BO_ALLOC_LOCAL_MEM_CPU_VISIBLE)) &&
       !(alloc_flags & ANV_BO_ALLOC_NO_LOCAL_MEM) &&
       device->physical->vram_non_mappable.size > 0)
      flags |= DRM_XE_GEM_CREATE_FLAG_NEEDS_VISIBLE_VRAM;

   struct drm_xe_gem_create gem_create = {
     /* From xe_drm.h: If a VM is specified, this BO must:
      * 1. Only ever be bound to that VM.
      * 2. Cannot be exported as a PRIME fd.
      */
     .vm_id = alloc_flags & ANV_BO_ALLOC_EXTERNAL ? 0 : device->vm_id,
     .size = align64(size, device->info->mem_alignment),
     .flags = flags,
   };
   for (uint16_t i = 0; i < regions_count; i++)
      gem_create.placement |= BITFIELD_BIT(regions[i]->instance);

   const struct intel_device_info_pat_entry *pat_entry =
         anv_device_get_pat_entry(device, alloc_flags);
   switch (pat_entry->mmap) {
   case INTEL_DEVICE_INFO_MMAP_MODE_WC:
      gem_create.cpu_caching = DRM_XE_GEM_CPU_CACHING_WC;
      break;
   case INTEL_DEVICE_INFO_MMAP_MODE_WB:
      gem_create.cpu_caching = DRM_XE_GEM_CPU_CACHING_WB;
      break;
   default:
      unreachable("missing");
      gem_create.cpu_caching = DRM_XE_GEM_CPU_CACHING_WC;
   }

   if (intel_ioctl(device->fd, DRM_IOCTL_XE_GEM_CREATE, &gem_create))
      return 0;

   *actual_size = gem_create.size;
   return gem_create.handle;
}

static void
xe_gem_close(struct anv_device *device, struct anv_bo *bo)
{
   if (bo->from_host_ptr)
      return;

   struct drm_gem_close close = {
      .handle = bo->gem_handle,
   };
   intel_ioctl(device->fd, DRM_IOCTL_GEM_CLOSE, &close);
}

static void *
xe_gem_mmap(struct anv_device *device, struct anv_bo *bo, uint64_t offset,
            uint64_t size, void *placed_addr)
{
   struct drm_xe_gem_mmap_offset args = {
      .handle = bo->gem_handle,
   };
   if (intel_ioctl(device->fd, DRM_IOCTL_XE_GEM_MMAP_OFFSET, &args))
      return MAP_FAILED;

   return mmap(placed_addr, size, PROT_READ | PROT_WRITE,
               (placed_addr != NULL ? MAP_FIXED : 0) | MAP_SHARED,
               device->fd, args.offset);
}

static inline uint32_t
capture_vm_in_error_dump(struct anv_device *device, struct anv_bo *bo)
{
   enum anv_bo_alloc_flags alloc_flags = bo ? bo->alloc_flags : 0;
   bool capture = INTEL_DEBUG(DEBUG_CAPTURE_ALL) ||
                  (alloc_flags & ANV_BO_ALLOC_CAPTURE);

   return capture ? DRM_XE_VM_BIND_FLAG_DUMPABLE : 0;
}

static struct drm_xe_vm_bind_op
anv_vm_bind_to_drm_xe_vm_bind(struct anv_device *device,
                              struct anv_vm_bind *anv_bind)
{
   struct anv_bo *bo = anv_bind->bo;
   uint16_t pat_index = bo ?
      anv_device_get_pat_entry(device, bo->alloc_flags)->index : 0;

   struct drm_xe_vm_bind_op xe_bind = {
         .obj = 0,
         .obj_offset = anv_bind->bo_offset,
         .range = anv_bind->size,
         .addr = intel_48b_address(anv_bind->address),
         .op = DRM_XE_VM_BIND_OP_UNMAP,
         .flags = capture_vm_in_error_dump(device, bo),
         .prefetch_mem_region_instance = 0,
         .pat_index = pat_index,
   };

   if (anv_bind->op == ANV_VM_BIND) {
      if (!bo) {
         xe_bind.op = DRM_XE_VM_BIND_OP_MAP;
         xe_bind.flags |= DRM_XE_VM_BIND_FLAG_NULL;
         assert(xe_bind.obj_offset == 0);
      } else if (bo->from_host_ptr) {
         xe_bind.op = DRM_XE_VM_BIND_OP_MAP_USERPTR;
      } else {
         xe_bind.op = DRM_XE_VM_BIND_OP_MAP;
         xe_bind.obj = bo->gem_handle;
      }
   } else if (anv_bind->op == ANV_VM_UNBIND_ALL) {
      xe_bind.op = DRM_XE_VM_BIND_OP_UNMAP_ALL;
      xe_bind.obj = bo->gem_handle;
      assert(anv_bind->address == 0);
      assert(anv_bind->size == 0);
   } else {
      assert(anv_bind->op == ANV_VM_UNBIND);
   }

   /* userptr and bo_offset are an union! */
   if (bo && bo->from_host_ptr)
      xe_bind.userptr = (uintptr_t)bo->map;

   return xe_bind;
}

static inline VkResult
xe_vm_bind_op(struct anv_device *device,
              struct anv_sparse_submission *submit,
              enum anv_vm_bind_flags flags)
{
   VkResult result = VK_SUCCESS;
   const bool signal_bind_timeline =
      flags & ANV_VM_BIND_FLAG_SIGNAL_BIND_TIMELINE;

   int num_syncs = submit->wait_count + submit->signal_count +
                   signal_bind_timeline;
   STACK_ARRAY(struct drm_xe_sync, xe_syncs, num_syncs);
   if (!xe_syncs)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   int sync_idx = 0;
   for (int s = 0; s < submit->wait_count; s++) {
      xe_syncs[sync_idx++] =
         vk_sync_to_drm_xe_sync(submit->waits[s].sync,
                                submit->waits[s].wait_value,
                                false);
   }
   for (int s = 0; s < submit->signal_count; s++) {
      xe_syncs[sync_idx++] =
         vk_sync_to_drm_xe_sync(submit->signals[s].sync,
                                submit->signals[s].signal_value,
                                true);
   }
   if (signal_bind_timeline) {
      xe_syncs[sync_idx++] = (struct drm_xe_sync) {
         .type = DRM_XE_SYNC_TYPE_TIMELINE_SYNCOBJ,
         .flags = DRM_XE_SYNC_FLAG_SIGNAL,
         .handle = intel_bind_timeline_get_syncobj(&device->bind_timeline),
         /* .timeline_value will be set later. */
      };
   }
   assert(sync_idx == num_syncs);

   struct drm_xe_vm_bind args = {
      .vm_id = device->vm_id,
      .num_binds = submit->binds_len,
      .bind = {},
      .num_syncs = num_syncs,
      .syncs = (uintptr_t)xe_syncs,
   };

   STACK_ARRAY(struct drm_xe_vm_bind_op, xe_binds_stackarray,
               submit->binds_len);
   struct drm_xe_vm_bind_op *xe_binds;
   if (submit->binds_len > 1) {
      if (!xe_binds_stackarray) {
         result = vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);
         goto out_syncs;
      }

      xe_binds = xe_binds_stackarray;
      args.vector_of_binds = (uintptr_t)xe_binds;
   } else {
      xe_binds = &args.bind;
   }

   for (int i = 0; i < submit->binds_len; i++)
      xe_binds[i] = anv_vm_bind_to_drm_xe_vm_bind(device, &submit->binds[i]);

   if (signal_bind_timeline) {
      xe_syncs[num_syncs - 1].timeline_value =
         intel_bind_timeline_bind_begin(&device->bind_timeline);
   }
   int ret = intel_ioctl(device->fd, DRM_IOCTL_XE_VM_BIND, &args);
   int errno_ = errno;
   if (signal_bind_timeline)
      intel_bind_timeline_bind_end(&device->bind_timeline);

   if (ret) {
      assert(errno_ != EINVAL);
      if (errno_ == ENOMEM)
         result = vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);
      else
         result = vk_device_set_lost(&device->vk,
                                     "vm_bind failed with errno %d", errno_);
      goto out_stackarray;
   }

   ANV_RMV(vm_binds, device, submit->binds, submit->binds_len);

out_stackarray:
   STACK_ARRAY_FINISH(xe_binds_stackarray);
out_syncs:
   STACK_ARRAY_FINISH(xe_syncs);

   return result;
}

static VkResult
xe_vm_bind(struct anv_device *device, struct anv_sparse_submission *submit,
           enum anv_vm_bind_flags flags)
{
   return xe_vm_bind_op(device, submit, flags);
}

static VkResult
xe_vm_bind_bo(struct anv_device *device, struct anv_bo *bo)
{
   struct anv_vm_bind bind = {
      .bo = bo,
      .address = bo->offset,
      .bo_offset = 0,
      .size = bo->actual_size,
      .op = ANV_VM_BIND,
   };
   struct anv_sparse_submission submit = {
      .queue = NULL,
      .binds = &bind,
      .binds_len = 1,
      .binds_capacity = 1,
      .wait_count = 0,
      .signal_count = 0,
   };
   return xe_vm_bind_op(device, &submit,
                        ANV_VM_BIND_FLAG_SIGNAL_BIND_TIMELINE);
}

static VkResult
xe_vm_unbind_bo(struct anv_device *device, struct anv_bo *bo)
{
   struct anv_vm_bind bind = {
      .bo = bo,
      .address = 0,
      .bo_offset = 0,
      .size = 0,
      .op = ANV_VM_UNBIND_ALL,
   };
   struct anv_sparse_submission submit = {
      .queue = NULL,
      .binds = &bind,
      .binds_len = 1,
      .binds_capacity = 1,
      .wait_count = 0,
      .signal_count = 0,
   };
   if (bo->from_host_ptr) {
      bind.address = bo->offset;
      bind.size = bo->actual_size;
      bind.op = ANV_VM_UNBIND;
   }
   return xe_vm_bind_op(device, &submit,
                        ANV_VM_BIND_FLAG_SIGNAL_BIND_TIMELINE);
}

static uint32_t
xe_gem_create_userptr(struct anv_device *device, void *mem, uint64_t size)
{
   /* We return the workaround BO gem_handle here, because Xe doesn't
    * create handles for userptrs. But we still need to make it look
    * to the rest of Anv that the operation succeeded.
    */
   return device->workaround_bo->gem_handle;
}

static uint32_t
xe_bo_alloc_flags_to_bo_flags(struct anv_device *device,
                              enum anv_bo_alloc_flags alloc_flags)
{
   return 0;
}

const struct anv_kmd_backend *
anv_xe_kmd_backend_get(void)
{
   static const struct anv_kmd_backend xe_backend = {
      .gem_create = xe_gem_create,
      .gem_create_userptr = xe_gem_create_userptr,
      .gem_close = xe_gem_close,
      .gem_mmap = xe_gem_mmap,
      .vm_bind = xe_vm_bind,
      .vm_bind_bo = xe_vm_bind_bo,
      .vm_unbind_bo = xe_vm_unbind_bo,
      .execute_simple_batch = xe_execute_simple_batch,
      .execute_trtt_batch = xe_execute_trtt_batch,
      .queue_exec_locked = xe_queue_exec_locked,
      .queue_exec_trace = xe_queue_exec_utrace_locked,
      .bo_alloc_flags_to_bo_flags = xe_bo_alloc_flags_to_bo_flags,
   };
   return &xe_backend;
}
