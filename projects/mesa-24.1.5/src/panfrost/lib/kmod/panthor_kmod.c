/*
 * Copyright © 2023 Collabora, Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <xf86drm.h>

#include "util/hash_table.h"
#include "util/libsync.h"
#include "util/macros.h"
#include "util/os_time.h"
#include "util/simple_mtx.h"
#include "util/u_debug.h"
#include "util/vma.h"

#include "drm-uapi/dma-buf.h"
#include "drm-uapi/panthor_drm.h"

#include "pan_kmod_backend.h"

const struct pan_kmod_ops panthor_kmod_ops;

/* Objects used to track VAs returned through async unmaps. */
struct panthor_kmod_va_collect {
   struct list_head node;

   /* VM sync point at which the VA range should be released. */
   uint64_t sync_point;

   /* Start of the VA range to release. */
   uint64_t va;

   /* Size of the VA range to release. */
   size_t size;
};

struct panthor_kmod_vm {
   struct pan_kmod_vm base;

   /* Fields used for auto-VA management. Since the kernel doesn't do it for
    * us, we need to deal with the VA allocation ourselves.
    */
   struct {
      /* Lock protecting VA allocation/freeing. */
      simple_mtx_t lock;

      /* VA heap used to automatically assign a VA. */
      struct util_vma_heap heap;

      /* VA ranges to garbage collect. */
      struct list_head gc_list;
   } auto_va;

   /* Fields used for VM activity tracking (TRACK_ACTIVITY flag). */
   struct {
      /* VM sync handle. */
      uint32_t handle;

      /* Current VM sync point. Incremented every time a GPU job or VM
       * operation is issued.
       */
      uint64_t point;

      /* Lock protecting insertion of sync points to the timeline syncobj. */
      simple_mtx_t lock;
   } sync;
};

struct panthor_kmod_dev {
   struct pan_kmod_dev base;

   /* Userspace mapping of the LATEST_FLUSH_ID register page. */
   uint32_t *flush_id;

   /* Cached device properties. Filled at device creation time. */
   struct {
      struct drm_panthor_gpu_info gpu;
      struct drm_panthor_csif_info csif;
   } props;
};

struct panthor_kmod_bo {
   struct pan_kmod_bo base;
   struct {
      /* BO sync handle. Will point to the VM BO if the object is not shared. */
      uint32_t handle;

      /* BO read sync point. Zero when the object is shared. */
      uint64_t read_point;

      /* BO write sync point. Zero when the object is shared. */
      uint64_t write_point;
   } sync;
};

static struct pan_kmod_dev *
panthor_kmod_dev_create(int fd, uint32_t flags, drmVersionPtr version,
                        const struct pan_kmod_allocator *allocator)
{
   struct panthor_kmod_dev *panthor_dev =
      pan_kmod_alloc(allocator, sizeof(*panthor_dev));
   if (!panthor_dev) {
      mesa_loge("failed to allocate a panthor_kmod_dev object");
      return NULL;
   }

   /* Cache GPU and CSIF information. */
   struct drm_panthor_dev_query query = {
      .type = DRM_PANTHOR_DEV_QUERY_GPU_INFO,
      .size = sizeof(panthor_dev->props.gpu),
      .pointer = (uint64_t)(uintptr_t)&panthor_dev->props.gpu,
   };

   int ret = drmIoctl(fd, DRM_IOCTL_PANTHOR_DEV_QUERY, &query);
   if (ret) {
      mesa_loge("DRM_IOCTL_PANTHOR_DEV_QUERY failed (err=%d)", errno);
      goto err_free_dev;
   }

   query = (struct drm_panthor_dev_query){
      .type = DRM_PANTHOR_DEV_QUERY_CSIF_INFO,
      .size = sizeof(panthor_dev->props.csif),
      .pointer = (uint64_t)(uintptr_t)&panthor_dev->props.csif,
   };

   ret = drmIoctl(fd, DRM_IOCTL_PANTHOR_DEV_QUERY, &query);
   if (ret) {
      mesa_loge("DRM_IOCTL_PANTHOR_DEV_QUERY failed (err=%d)", errno);
      goto err_free_dev;
   }

   /* Map the LATEST_FLUSH_ID register at device creation time. */
   panthor_dev->flush_id = os_mmap(0, getpagesize(), PROT_READ, MAP_SHARED, fd,
                                   DRM_PANTHOR_USER_FLUSH_ID_MMIO_OFFSET);
   if (panthor_dev->flush_id == MAP_FAILED) {
      mesa_loge("failed to mmap the LATEST_FLUSH_ID register (err=%d)", errno);
      goto err_free_dev;
   }

   assert(!ret);
   pan_kmod_dev_init(&panthor_dev->base, fd, flags, version, &panthor_kmod_ops,
                     allocator);
   return &panthor_dev->base;

err_free_dev:
   pan_kmod_free(allocator, panthor_dev);
   return NULL;
}

static void
panthor_kmod_dev_destroy(struct pan_kmod_dev *dev)
{
   struct panthor_kmod_dev *panthor_dev =
      container_of(dev, struct panthor_kmod_dev, base);

   os_munmap(panthor_dev->flush_id, getpagesize());
   pan_kmod_dev_cleanup(dev);
   pan_kmod_free(dev->allocator, panthor_dev);
}

static void
panthor_dev_query_thread_props(const struct panthor_kmod_dev *panthor_dev,
                               struct pan_kmod_dev_props *props)
{
   props->max_threads_per_wg = panthor_dev->props.gpu.thread_max_workgroup_size;
   props->max_threads_per_core = panthor_dev->props.gpu.max_threads;
   props->num_registers_per_core =
      panthor_dev->props.gpu.thread_features & 0x3fffff;

   /* We assume that all thread properties are populated. If we ever have a GPU
    * that have one of the THREAD_xxx register that's zero, we can always add a
    * quirk here.
    */
   assert(props->max_threads_per_wg && props->max_threads_per_core &&
          props->num_registers_per_core);

   /* There is no THREAD_TLS_ALLOC register on v10+, and the maximum number
    * of TLS instance per core is assumed to be the maximum number of threads
    * per core.
    */
   props->max_tls_instance_per_core = props->max_threads_per_core;
}

static void
panthor_dev_query_props(const struct pan_kmod_dev *dev,
                        struct pan_kmod_dev_props *props)
{
   struct panthor_kmod_dev *panthor_dev =
      container_of(dev, struct panthor_kmod_dev, base);

   *props = (struct pan_kmod_dev_props){
      .gpu_prod_id = panthor_dev->props.gpu.gpu_id >> 16,
      .gpu_revision = panthor_dev->props.gpu.gpu_id & 0xffff,
      .gpu_variant = panthor_dev->props.gpu.core_features & 0xff,
      .shader_present = panthor_dev->props.gpu.shader_present,
      .tiler_features = panthor_dev->props.gpu.tiler_features,
      .mem_features = panthor_dev->props.gpu.mem_features,
      .mmu_features = panthor_dev->props.gpu.mmu_features,

      /* This register does not exist because AFBC is no longer optional. */
      .afbc_features = 0,
   };

   static_assert(sizeof(props->texture_features) ==
                    sizeof(panthor_dev->props.gpu.texture_features),
                 "Mismatch in texture_features array size");

   memcpy(props->texture_features, panthor_dev->props.gpu.texture_features,
          sizeof(props->texture_features));

   panthor_dev_query_thread_props(panthor_dev, props);
}

static struct pan_kmod_va_range
panthor_kmod_dev_query_user_va_range(const struct pan_kmod_dev *dev)
{
   struct panthor_kmod_dev *panthor_dev =
      container_of(dev, struct panthor_kmod_dev, base);
   uint8_t va_bits = MMU_FEATURES_VA_BITS(panthor_dev->props.gpu.mmu_features);

   /* If we have less than 32-bit VA space it starts to be tricky, so let's
    * assume we always have at least that.
    */
   assert(va_bits >= 32);

   return (struct pan_kmod_va_range){
      .start = 0,

      /* 3G/1G user/kernel VA split for 32-bit VA space. Otherwise, we reserve
       * half of the VA space for kernel objects.
       */
      .size =
         va_bits == 32 ? (1ull << (va_bits - 2)) * 3 : 1ull << (va_bits - 1),
   };
}

static uint32_t
to_panthor_bo_flags(uint32_t flags)
{
   uint32_t panthor_flags = 0;

   if (flags & PAN_KMOD_BO_FLAG_NO_MMAP)
      panthor_flags |= DRM_PANTHOR_BO_NO_MMAP;

   return panthor_flags;
}

static struct pan_kmod_bo *
panthor_kmod_bo_alloc(struct pan_kmod_dev *dev,
                      struct pan_kmod_vm *exclusive_vm, size_t size,
                      uint32_t flags)
{
   /* We don't support allocating on-fault. */
   if (flags & PAN_KMOD_BO_FLAG_ALLOC_ON_FAULT) {
      mesa_loge("panthor_kmod doesn't support PAN_KMOD_BO_FLAG_ALLOC_ON_FAULT");
      return NULL;
   }

   struct panthor_kmod_vm *panthor_vm =
      exclusive_vm ? container_of(exclusive_vm, struct panthor_kmod_vm, base)
                   : NULL;
   struct panthor_kmod_bo *bo = pan_kmod_dev_alloc(dev, sizeof(*bo));
   if (!bo) {
      mesa_loge("failed to allocate a panthor_kmod_bo object");
      return NULL;
   }

   struct drm_panthor_bo_create req = {
      .size = size,
      .flags = to_panthor_bo_flags(flags),
      .exclusive_vm_id = panthor_vm ? panthor_vm->base.handle : 0,
   };

   int ret = drmIoctl(dev->fd, DRM_IOCTL_PANTHOR_BO_CREATE, &req);
   if (ret) {
      mesa_loge("DRM_IOCTL_PANTHOR_BO_CREATE failed (err=%d)", errno);
      goto err_free_bo;
   }

   if (!exclusive_vm) {
      /* For buffers we know will be shared, create our own syncobj. */
      int ret = drmSyncobjCreate(dev->fd, DRM_SYNCOBJ_CREATE_SIGNALED,
                                 &bo->sync.handle);
      if (ret) {
         mesa_loge("drmSyncobjCreate() failed (err=%d)", errno);
         goto err_destroy_bo;
      }
   } else {
      /* If the buffer is private to the VM, we just use the VM syncobj. */
      bo->sync.handle = panthor_vm->sync.handle;
   }

   bo->sync.read_point = bo->sync.write_point = 0;

   pan_kmod_bo_init(&bo->base, dev, exclusive_vm, req.size, flags, req.handle);
   return &bo->base;

err_destroy_bo:
   drmCloseBufferHandle(dev->fd, bo->base.handle);
err_free_bo:
   pan_kmod_dev_free(dev, bo);
   return NULL;
}

static void
panthor_kmod_bo_free(struct pan_kmod_bo *bo)
{
   struct panthor_kmod_bo *panthor_bo =
      container_of(bo, struct panthor_kmod_bo, base);

   if (!bo->exclusive_vm)
      drmSyncobjDestroy(bo->dev->fd, panthor_bo->sync.handle);

   drmCloseBufferHandle(bo->dev->fd, bo->handle);
   pan_kmod_dev_free(bo->dev, bo);
}

static struct pan_kmod_bo *
panthor_kmod_bo_import(struct pan_kmod_dev *dev, uint32_t handle, size_t size,
                       uint32_t flags)
{
   struct panthor_kmod_bo *panthor_bo =
      pan_kmod_dev_alloc(dev, sizeof(*panthor_bo));
   if (!panthor_bo) {
      mesa_loge("failed to allocate a panthor_kmod_bo object");
      return NULL;
   }

   /* Create a unsignalled syncobj on import. Will serve as a
    * temporary container for the exported dmabuf sync file.
    */
   int ret = drmSyncobjCreate(dev->fd, 0, &panthor_bo->sync.handle);
   if (ret) {
      mesa_loge("drmSyncobjCreate() failed (err=%d)", errno);
      goto err_free_bo;
   }

   pan_kmod_bo_init(&panthor_bo->base, dev, NULL, size,
                    flags | PAN_KMOD_BO_FLAG_IMPORTED, handle);
   return &panthor_bo->base;

err_free_bo:
   pan_kmod_dev_free(dev, panthor_bo);
   return NULL;
}

static int
panthor_kmod_bo_export(struct pan_kmod_bo *bo, int dmabuf_fd)
{
   struct panthor_kmod_bo *panthor_bo =
      container_of(bo, struct panthor_kmod_bo, base);

   bool shared =
      bo->flags & (PAN_KMOD_BO_FLAG_EXPORTED | PAN_KMOD_BO_FLAG_IMPORTED);

   /* If the BO wasn't already shared, we migrate our internal sync points to
    * the dmabuf itself, so implicit sync can work correctly after this point.
    */
   if (!shared) {
      if (panthor_bo->sync.read_point || panthor_bo->sync.write_point) {
         struct dma_buf_import_sync_file isync = {
            .flags = DMA_BUF_SYNC_RW,
         };
         int ret = drmSyncobjExportSyncFile(bo->dev->fd,
                                            panthor_bo->sync.handle, &isync.fd);
         if (ret) {
            mesa_loge("drmSyncobjExportSyncFile() failed (err=%d)", errno);
            return -1;
         }

         ret = drmIoctl(dmabuf_fd, DMA_BUF_IOCTL_IMPORT_SYNC_FILE, &isync);
         close(isync.fd);
         if (ret) {
            mesa_loge("DMA_BUF_IOCTL_IMPORT_SYNC_FILE failed (err=%d)", errno);
            return -1;
         }
      }

      /* Make sure we reset the syncobj on export. We will use it as a
       * temporary binary syncobj to import sync_file FD from now on.
       */
      int ret = drmSyncobjReset(bo->dev->fd, &panthor_bo->sync.handle, 1);
      if (ret) {
         mesa_loge("drmSyncobjReset() failed (err=%d)", errno);
         return -1;
      }

      panthor_bo->sync.read_point = 0;
      panthor_bo->sync.write_point = 0;
   }

   bo->flags |= PAN_KMOD_BO_FLAG_EXPORTED;
   return 0;
}

static off_t
panthor_kmod_bo_get_mmap_offset(struct pan_kmod_bo *bo)
{
   struct drm_panthor_bo_mmap_offset req = {.handle = bo->handle};
   int ret = drmIoctl(bo->dev->fd, DRM_IOCTL_PANTHOR_BO_MMAP_OFFSET, &req);

   if (ret) {
      mesa_loge("DRM_IOCTL_PANTHOR_BO_MMAP_OFFSET failed (err=%d)", errno);
      return -1;
   }

   return req.offset;
}

static bool
panthor_kmod_bo_wait(struct pan_kmod_bo *bo, int64_t timeout_ns,
                     bool for_read_only_access)
{
   struct panthor_kmod_bo *panthor_bo =
      container_of(bo, struct panthor_kmod_bo, base);
   bool shared =
      bo->flags & (PAN_KMOD_BO_FLAG_EXPORTED | PAN_KMOD_BO_FLAG_IMPORTED);

   if (shared) {
      /* If the object is shared, we have to do this export sync-file dance
       * to reconcile with the implicit sync model. This implies exporting
       * our GEM object as a dma-buf and closing it right after the
       * EXPORT_SYNC_FILE, unfortunately.
       */
      int dmabuf_fd;
      int ret =
         drmPrimeHandleToFD(bo->dev->fd, bo->handle, DRM_CLOEXEC, &dmabuf_fd);

      if (ret) {
         mesa_loge("drmPrimeHandleToFD() failed (err=%d)", errno);
         return false;
      }

      struct dma_buf_export_sync_file esync = {
         .flags = for_read_only_access ? DMA_BUF_SYNC_READ : DMA_BUF_SYNC_RW,
      };

      ret = drmIoctl(dmabuf_fd, DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &esync);
      close(dmabuf_fd);

      if (ret) {
         mesa_loge("DMA_BUF_IOCTL_EXPORT_SYNC_FILE failed (err=%d)", errno);
         return false;
      }

      ret = sync_wait(esync.fd, timeout_ns / 1000000);
      close(esync.fd);
      return ret == 0;
   } else {
      /* Waiting on non-shared object is much simpler. We just pick the
       * right sync point based on for_read_only_access and call
       * drmSyncobjTimelineWait().
       */
      uint64_t sync_point =
         for_read_only_access
            ? panthor_bo->sync.write_point
            : MAX2(panthor_bo->sync.write_point, panthor_bo->sync.read_point);

      if (!sync_point)
         return true;

      int64_t abs_timeout_ns = timeout_ns < INT64_MAX - os_time_get_nano()
                                  ? timeout_ns + os_time_get_nano()
                                  : INT64_MAX;
      int ret = drmSyncobjTimelineWait(bo->dev->fd, &panthor_bo->sync.handle,
                                       &sync_point, 1, abs_timeout_ns,
                                       DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL, NULL);
      if (ret >= 0)
         return true;

      if (ret != -ETIME)
         mesa_loge("DMA_BUF_IOCTL_EXPORT_SYNC_FILE failed (err=%d)", ret);

      return false;
   }
}

/* Attach a sync to a buffer object. */
int
panthor_kmod_bo_attach_sync_point(struct pan_kmod_bo *bo, uint32_t sync_handle,
                                  uint64_t sync_point, bool written)
{
   struct panthor_kmod_bo *panthor_bo =
      container_of(bo, struct panthor_kmod_bo, base);
   struct panthor_kmod_vm *panthor_vm =
      bo->exclusive_vm
         ? container_of(bo->exclusive_vm, struct panthor_kmod_vm, base)
         : NULL;
   bool shared =
      bo->flags & (PAN_KMOD_BO_FLAG_EXPORTED | PAN_KMOD_BO_FLAG_IMPORTED);

   if (shared) {
      /* Reconciling explicit/implicit sync again: we need to import the
       * new sync point in the dma-buf, so other parties can rely on
       * implicit deps.
       */
      struct dma_buf_import_sync_file isync = {
         .flags = written ? DMA_BUF_SYNC_RW : DMA_BUF_SYNC_READ,
      };
      int dmabuf_fd;
      int ret = drmSyncobjExportSyncFile(bo->dev->fd, sync_handle, &isync.fd);
      if (ret) {
         mesa_loge("drmSyncobjExportSyncFile() failed (err=%d)", errno);
         return -1;
      }

      ret =
         drmPrimeHandleToFD(bo->dev->fd, bo->handle, DRM_CLOEXEC, &dmabuf_fd);
      if (ret) {
         mesa_loge("drmPrimeHandleToFD() failed (err=%d)", errno);
         close(isync.fd);
         return -1;
      }

      ret = drmIoctl(dmabuf_fd, DMA_BUF_IOCTL_IMPORT_SYNC_FILE, &isync);
      close(dmabuf_fd);
      close(isync.fd);
      if (ret) {
         mesa_loge("DMA_BUF_IOCTL_IMPORT_SYNC_FILE failed (err=%d)", errno);
         return -1;
      }
   } else if (panthor_vm) {
      /* Private BOs should be passed the VM syncobj. */
      assert(sync_handle == panthor_vm->sync.handle);

      panthor_bo->sync.read_point =
         MAX2(sync_point, panthor_bo->sync.read_point);
      if (written) {
         panthor_bo->sync.write_point =
            MAX2(sync_point, panthor_bo->sync.write_point);
      }
   } else {
      /* For non-private BOs that are not shared yet, we add a new sync point
       * to our timeline syncobj, and push the sync there.
       */
      uint32_t new_sync_point =
         MAX2(panthor_bo->sync.write_point, panthor_bo->sync.read_point) + 1;

      int ret = drmSyncobjTransfer(bo->dev->fd, panthor_bo->sync.handle,
                                   new_sync_point, sync_handle, sync_point, 0);
      if (ret) {
         mesa_loge("drmSyncobjTransfer() failed (err=%d)", errno);
         return -1;
      }

      panthor_bo->sync.read_point = new_sync_point;
      if (written)
         panthor_bo->sync.write_point = new_sync_point;
   }

   return 0;
}

/* Get the sync point for a read or write operation on a buffer object. */
int
panthor_kmod_bo_get_sync_point(struct pan_kmod_bo *bo, uint32_t *sync_handle,
                               uint64_t *sync_point, bool for_read_only_access)
{
   struct panthor_kmod_bo *panthor_bo =
      container_of(bo, struct panthor_kmod_bo, base);
   bool shared =
      bo->flags & (PAN_KMOD_BO_FLAG_EXPORTED | PAN_KMOD_BO_FLAG_IMPORTED);

   if (shared) {
      /* Explicit/implicit sync reconciliation point. We need to export
       * a sync-file from the dmabuf and make it a syncobj.
       */
      int dmabuf_fd;
      int ret =
         drmPrimeHandleToFD(bo->dev->fd, bo->handle, DRM_CLOEXEC, &dmabuf_fd);
      if (ret) {
         mesa_loge("drmPrimeHandleToFD() failed (err=%d)\n", errno);
         return -1;
      }

      struct dma_buf_export_sync_file esync = {
         .flags = for_read_only_access ? DMA_BUF_SYNC_READ : DMA_BUF_SYNC_RW,
      };

      ret = drmIoctl(dmabuf_fd, DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &esync);
      close(dmabuf_fd);
      if (ret) {
         mesa_loge("DMA_BUF_IOCTL_EXPORT_SYNC_FILE failed (err=%d)", errno);
         return -1;
      }

      /* We store the resulting sync in our BO syncobj, which will be assigned
       * a new sync next time we enter this function.
       */
      ret = drmSyncobjImportSyncFile(bo->dev->fd, panthor_bo->sync.handle,
                                     esync.fd);
      close(esync.fd);
      if (ret) {
         mesa_loge("drmSyncobjImportSyncFile() failed (err=%d)", errno);
         return -1;
      }

      /* The syncobj is a binary syncobj in that case. */
      *sync_handle = panthor_bo->sync.handle;
      *sync_point = 0;
   } else {
      /* Fortunately, the non-shared path is much simpler, we just return
       * the read/write sync point depending on the access type. The syncobj
       * is a timeline syncobj in that case.
       */
      *sync_handle = panthor_bo->sync.handle;
      *sync_point = for_read_only_access ? panthor_bo->sync.write_point
                                         : MAX2(panthor_bo->sync.read_point,
                                                panthor_bo->sync.write_point);
   }
   return 0;
}

static struct pan_kmod_vm *
panthor_kmod_vm_create(struct pan_kmod_dev *dev, uint32_t flags,
                       uint64_t user_va_start, uint64_t user_va_range)
{
   struct pan_kmod_dev_props props;

   panthor_dev_query_props(dev, &props);

   struct panthor_kmod_vm *panthor_vm =
      pan_kmod_dev_alloc(dev, sizeof(*panthor_vm));
   if (!panthor_vm) {
      mesa_loge("failed to allocate a panthor_kmod_vm object");
      return NULL;
   }

   if (flags & PAN_KMOD_VM_FLAG_AUTO_VA) {
      simple_mtx_init(&panthor_vm->auto_va.lock, mtx_plain);
      list_inithead(&panthor_vm->auto_va.gc_list);
      util_vma_heap_init(&panthor_vm->auto_va.heap, user_va_start,
                         user_va_range);
   }

   if (flags & PAN_KMOD_VM_FLAG_TRACK_ACTIVITY) {
      simple_mtx_init(&panthor_vm->sync.lock, mtx_plain);
      panthor_vm->sync.point = 0;
      if (drmSyncobjCreate(dev->fd, DRM_SYNCOBJ_CREATE_SIGNALED,
                           &panthor_vm->sync.handle)) {
         mesa_loge("drmSyncobjCreate() failed (err=%d)", errno);
         goto err_free_vm;
      }
   }

   struct drm_panthor_vm_create req = {
      .user_va_range = user_va_start + user_va_range,
   };

   if (drmIoctl(dev->fd, DRM_IOCTL_PANTHOR_VM_CREATE, &req)) {
      mesa_loge("DRM_IOCTL_PANTHOR_VM_CREATE failed (err=%d)", errno);
      goto err_destroy_sync;
   }

   pan_kmod_vm_init(&panthor_vm->base, dev, req.id, flags);
   return &panthor_vm->base;

err_destroy_sync:
   if (flags & PAN_KMOD_VM_FLAG_TRACK_ACTIVITY) {
      drmSyncobjDestroy(dev->fd, panthor_vm->sync.handle);
      simple_mtx_destroy(&panthor_vm->sync.lock);
   }

err_free_vm:
   if (flags & PAN_KMOD_VM_FLAG_AUTO_VA) {
      util_vma_heap_finish(&panthor_vm->auto_va.heap);
      simple_mtx_destroy(&panthor_vm->auto_va.lock);
   }

   pan_kmod_dev_free(dev, panthor_vm);
   return NULL;
}

static void
panthor_kmod_vm_collect_freed_vas(struct panthor_kmod_vm *vm)
{
   if (!(vm->base.flags & PAN_KMOD_VM_FLAG_AUTO_VA))
      return;

   bool done = false;

   simple_mtx_assert_locked(&vm->auto_va.lock);
   list_for_each_entry_safe_rev(struct panthor_kmod_va_collect, req,
                                &vm->auto_va.gc_list, node)
   {
      /* Unmaps are queued in order of execution */
      if (!done) {
         int ret = drmSyncobjTimelineWait(
            vm->base.dev->fd, &vm->sync.handle, &req->sync_point, 1, 0,
            DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL, NULL);
         if (ret >= 0)
            done = true;
         else
            continue;
      }

      list_del(&req->node);
      util_vma_heap_free(&vm->auto_va.heap, req->va, req->size);
      pan_kmod_dev_free(vm->base.dev, req);
   }
}

static void
panthor_kmod_vm_destroy(struct pan_kmod_vm *vm)
{
   struct panthor_kmod_vm *panthor_vm =
      container_of(vm, struct panthor_kmod_vm, base);
   struct drm_panthor_vm_destroy req = {.id = vm->handle};
   int ret = drmIoctl(vm->dev->fd, DRM_IOCTL_PANTHOR_VM_DESTROY, &req);
   if (ret)
      mesa_loge("DRM_IOCTL_PANTHOR_VM_DESTROY failed (err=%d)", errno);

   assert(!ret);

   if (panthor_vm->base.flags & PAN_KMOD_VM_FLAG_TRACK_ACTIVITY) {
      drmSyncobjDestroy(vm->dev->fd, panthor_vm->sync.handle);
      simple_mtx_destroy(&panthor_vm->sync.lock);
   }

   if (panthor_vm->base.flags & PAN_KMOD_VM_FLAG_AUTO_VA) {
      simple_mtx_lock(&panthor_vm->auto_va.lock);
      list_for_each_entry_safe(struct panthor_kmod_va_collect, req,
                               &panthor_vm->auto_va.gc_list, node) {
         list_del(&req->node);
         util_vma_heap_free(&panthor_vm->auto_va.heap, req->va, req->size);
         pan_kmod_dev_free(vm->dev, req);
      }
      util_vma_heap_finish(&panthor_vm->auto_va.heap);
      simple_mtx_unlock(&panthor_vm->auto_va.lock);
      simple_mtx_destroy(&panthor_vm->auto_va.lock);
   }

   pan_kmod_dev_free(vm->dev, panthor_vm);
}

static uint64_t
panthor_kmod_vm_alloc_va(struct panthor_kmod_vm *panthor_vm, size_t size)
{
   uint64_t va;

   assert(panthor_vm->base.flags & PAN_KMOD_VM_FLAG_AUTO_VA);

   simple_mtx_lock(&panthor_vm->auto_va.lock);
   panthor_kmod_vm_collect_freed_vas(panthor_vm);
   va = util_vma_heap_alloc(&panthor_vm->auto_va.heap, size,
                            size > 0x200000 ? 0x200000 : 0x1000);
   simple_mtx_unlock(&panthor_vm->auto_va.lock);

   return va;
}

static void
panthor_kmod_vm_free_va(struct panthor_kmod_vm *panthor_vm, uint64_t va,
                        size_t size)
{
   assert(panthor_vm->base.flags & PAN_KMOD_VM_FLAG_AUTO_VA);

   simple_mtx_lock(&panthor_vm->auto_va.lock);
   util_vma_heap_free(&panthor_vm->auto_va.heap, va, size);
   simple_mtx_unlock(&panthor_vm->auto_va.lock);
}

static int
panthor_kmod_vm_bind(struct pan_kmod_vm *vm, enum pan_kmod_vm_op_mode mode,
                     struct pan_kmod_vm_op *ops, uint32_t op_count)
{
   struct panthor_kmod_vm *panthor_vm =
      container_of(vm, struct panthor_kmod_vm, base);
   struct drm_panthor_vm_bind_op *bind_ops = NULL;
   struct drm_panthor_sync_op *sync_ops = NULL;
   uint32_t syncop_cnt = 0, syncop_ptr = 0;
   bool async = mode == PAN_KMOD_VM_OP_MODE_ASYNC ||
                mode == PAN_KMOD_VM_OP_MODE_DEFER_TO_NEXT_IDLE_POINT;
   bool auto_va = vm->flags & PAN_KMOD_VM_FLAG_AUTO_VA;
   bool track_activity = vm->flags & PAN_KMOD_VM_FLAG_TRACK_ACTIVITY;
   struct panthor_kmod_va_collect *cur_va_collect = NULL;
   struct list_head va_collect_list;
   uint32_t va_collect_cnt = 0;
   int ret = -1;

   /* For any asynchronous VM bind, we assume the user is managing the VM
    * address space, so we don't have to collect VMAs in that case.
    */
   if (mode == PAN_KMOD_VM_OP_MODE_ASYNC && auto_va) {
      mesa_loge(
         "auto-VA allocation is incompatible with PAN_KMOD_VM_OP_MODE_ASYNC");
      return -1;
   }

   if (mode == PAN_KMOD_VM_OP_MODE_DEFER_TO_NEXT_IDLE_POINT &&
       !track_activity) {
      mesa_loge(
         "PAN_KMOD_VM_OP_MODE_DEFER_TO_NEXT_IDLE_POINT requires PAN_KMOD_VM_FLAG_TRACK_ACTIVITY");
      return -1;
   }

   if (op_count == 0)
      return 0;

   /* If this is an async operation and VM activity tracking is enabled, we
    * reserve one syncop per VM operation for the signaling of our VM timeline
    * slot.
    */
   if (async && track_activity)
      syncop_cnt += op_count;

   /* With PAN_KMOD_VM_OP_MODE_DEFER_TO_NEXT_IDLE_POINT, we need to push our
    * wait VM syncobj in all of the submissions, hence the extra syncop per
    * operation.
    */
   if (mode == PAN_KMOD_VM_OP_MODE_DEFER_TO_NEXT_IDLE_POINT)
      syncop_cnt += op_count;

   for (uint32_t i = 0; i < op_count; i++) {
      if (pan_kmod_vm_op_check(vm, mode, &ops[i]))
         return -1;

      /* If auto-VA is used, for any asynchronous unmap operation, we need
       * to register a VA collection node and add it to the GC list.
       */
      if (auto_va && async && ops[i].type == PAN_KMOD_VM_OP_TYPE_UNMAP &&
          ops[i].va.size)
         va_collect_cnt++;

      syncop_cnt += ops[i].syncs.count;
   }

   /* Pre-allocate the VA collection nodes. */
   list_inithead(&va_collect_list);
   for (uint32_t i = 0; i < va_collect_cnt; i++) {
      struct panthor_kmod_va_collect *va_collect =
         pan_kmod_dev_alloc(vm->dev, sizeof(*va_collect));
      if (!va_collect) {
         mesa_loge("panthor_kmod_va_collect allocation failed");
         goto out_free_va_collect;
      }

      if (!i)
         cur_va_collect = va_collect;

      list_addtail(&va_collect->node, &va_collect_list);
   }

   if (syncop_cnt) {
      sync_ops =
         pan_kmod_dev_alloc_transient(vm->dev, sizeof(*sync_ops) * syncop_cnt);
      if (!sync_ops) {
         mesa_loge("drm_panthor_sync_op[%d] array allocation failed",
                   syncop_cnt);
         goto out_free_va_collect;
      }
   }

   bind_ops =
      pan_kmod_dev_alloc_transient(vm->dev, sizeof(*bind_ops) * op_count);
   if (!bind_ops) {
      mesa_loge("drm_panthor_vm_bind_op[%d] array allocation failed",
                MIN2(op_count, 1));
      goto out_free_sync_ops;
   }

   struct drm_panthor_vm_bind req = {
      .vm_id = vm->handle,
      .flags =
         mode != PAN_KMOD_VM_OP_MODE_IMMEDIATE ? DRM_PANTHOR_VM_BIND_ASYNC : 0,
      .ops = DRM_PANTHOR_OBJ_ARRAY(MIN2(op_count, 1), bind_ops),
   };

   uint64_t vm_orig_sync_point = 0, vm_new_sync_point = 0;

   if (track_activity)
      vm_orig_sync_point = vm_new_sync_point = panthor_kmod_vm_sync_lock(vm);

   for (uint32_t i = 0; i < op_count; i++) {
      uint32_t op_sync_cnt = ops[i].syncs.count;
      uint64_t signal_vm_point = 0;

      if (async && track_activity) {
         signal_vm_point = ++vm_new_sync_point;
         op_sync_cnt++;
         sync_ops[syncop_ptr++] = (struct drm_panthor_sync_op){
            .flags = DRM_PANTHOR_SYNC_OP_SIGNAL |
                     DRM_PANTHOR_SYNC_OP_HANDLE_TYPE_TIMELINE_SYNCOBJ,
            .handle = panthor_vm->sync.handle,
            .timeline_value = signal_vm_point,
         };
      }

      if (mode == PAN_KMOD_VM_OP_MODE_DEFER_TO_NEXT_IDLE_POINT) {
         op_sync_cnt++;
         sync_ops[syncop_ptr++] = (struct drm_panthor_sync_op){
            .flags = DRM_PANTHOR_SYNC_OP_WAIT |
                     DRM_PANTHOR_SYNC_OP_HANDLE_TYPE_TIMELINE_SYNCOBJ,
            .handle = panthor_vm->sync.handle,
            .timeline_value = vm_orig_sync_point,
         };

         if (auto_va && ops[i].type == PAN_KMOD_VM_OP_TYPE_UNMAP &&
             ops[i].va.size) {
            struct panthor_kmod_va_collect *va_collect = cur_va_collect;

            assert(&va_collect->node != &va_collect_list);
            assert(signal_vm_point);
            va_collect->sync_point = signal_vm_point;
            va_collect->va = ops[i].va.start;
            va_collect->size = ops[i].va.size;

            cur_va_collect = list_entry(cur_va_collect->node.next,
                                        struct panthor_kmod_va_collect, node);
         }
      }

      for (uint32_t j = 0; j < ops[i].syncs.count; j++) {
         sync_ops[syncop_ptr++] = (struct drm_panthor_sync_op){
            .flags = (ops[i].syncs.array[j].type == PAN_KMOD_SYNC_TYPE_WAIT
                         ? DRM_PANTHOR_SYNC_OP_WAIT
                         : DRM_PANTHOR_SYNC_OP_SIGNAL) |
                     DRM_PANTHOR_SYNC_OP_HANDLE_TYPE_TIMELINE_SYNCOBJ,
            .handle = ops[i].syncs.array[j].handle,
            .timeline_value = ops[i].syncs.array[j].point,
         };
      }
      op_sync_cnt += ops[i].syncs.count;

      bind_ops[i].syncs = (struct drm_panthor_obj_array)DRM_PANTHOR_OBJ_ARRAY(
         op_sync_cnt, op_sync_cnt ? &sync_ops[syncop_ptr - op_sync_cnt] : NULL);

      if (ops[i].type == PAN_KMOD_VM_OP_TYPE_MAP) {
         bind_ops[i].flags = DRM_PANTHOR_VM_BIND_OP_TYPE_MAP;
         bind_ops[i].size = ops[i].va.size;
         bind_ops[i].bo_handle = ops[i].map.bo->handle;
         bind_ops[i].bo_offset = ops[i].map.bo_offset;

         if (ops[i].va.start == PAN_KMOD_VM_MAP_AUTO_VA) {
            bind_ops[i].va =
               panthor_kmod_vm_alloc_va(panthor_vm, bind_ops[i].size);
            if (!bind_ops[i].va) {
               mesa_loge("VA allocation failed");
               ret = -1;
               goto out_update_vas;
            }
         } else {
            bind_ops[i].va = ops[i].va.start;
         }

         if (ops[i].map.bo->flags & PAN_KMOD_BO_FLAG_EXECUTABLE)
            bind_ops[i].flags |= DRM_PANTHOR_VM_BIND_OP_MAP_READONLY;
         else
            bind_ops[i].flags |= DRM_PANTHOR_VM_BIND_OP_MAP_NOEXEC;

         if (ops[i].map.bo->flags & PAN_KMOD_BO_FLAG_GPU_UNCACHED)
            bind_ops[i].flags |= DRM_PANTHOR_VM_BIND_OP_MAP_UNCACHED;

      } else if (ops[i].type == PAN_KMOD_VM_OP_TYPE_UNMAP) {
         bind_ops[i].flags = DRM_PANTHOR_VM_BIND_OP_TYPE_UNMAP;
         bind_ops[i].va = ops[i].va.start;
         bind_ops[i].size = ops[i].va.size;
      } else {
         assert(ops[i].type == PAN_KMOD_VM_OP_TYPE_SYNC_ONLY);
         bind_ops[i].flags = DRM_PANTHOR_VM_BIND_OP_TYPE_SYNC_ONLY;
      }
   }

   ret = drmIoctl(vm->dev->fd, DRM_IOCTL_PANTHOR_VM_BIND, &req);
   if (ret)
      mesa_loge("DRM_IOCTL_PANTHOR_VM_BIND failed (err=%d)", errno);

   if (!ret && va_collect_cnt) {
      assert(&cur_va_collect->node == &va_collect_list);
      simple_mtx_lock(&panthor_vm->auto_va.lock);
      list_splicetail(&va_collect_list, &panthor_vm->auto_va.gc_list);
      list_inithead(&va_collect_list);
      simple_mtx_unlock(&panthor_vm->auto_va.lock);
   }

out_update_vas:
   if (track_activity) {
      panthor_kmod_vm_sync_unlock(vm,
                                  ret ? vm_orig_sync_point : vm_new_sync_point);
   }

   for (uint32_t i = 0; i < op_count; i++) {
      if (ops[i].type == PAN_KMOD_VM_OP_TYPE_MAP &&
          ops[i].va.start == PAN_KMOD_VM_MAP_AUTO_VA) {
         if (!ret) {
            ops[i].va.start = bind_ops[i].va;
         } else if (bind_ops[i].va != 0) {
            panthor_kmod_vm_free_va(panthor_vm, bind_ops[i].va,
                                    bind_ops[i].size);
         }
      }

      if (ops[i].type == PAN_KMOD_VM_OP_TYPE_UNMAP && auto_va && !async &&
          !ret) {
         panthor_kmod_vm_free_va(panthor_vm, bind_ops[i].va, bind_ops[i].size);
      }
   }

   pan_kmod_dev_free(vm->dev, bind_ops);

out_free_sync_ops:
   pan_kmod_dev_free(vm->dev, sync_ops);

out_free_va_collect:
   list_for_each_entry_safe(struct panthor_kmod_va_collect, va_collect,
                            &va_collect_list, node) {
      list_del(&va_collect->node);
      pan_kmod_dev_free(vm->dev, va_collect);
   }

   return ret;
}

static enum pan_kmod_vm_state
panthor_kmod_vm_query_state(struct pan_kmod_vm *vm)
{
   struct drm_panthor_vm_get_state query = {.vm_id = vm->handle};
   int ret = drmIoctl(vm->dev->fd, DRM_IOCTL_PANTHOR_VM_GET_STATE, &query);

   if (ret || query.state == DRM_PANTHOR_VM_STATE_UNUSABLE)
      return PAN_KMOD_VM_FAULTY;

   return PAN_KMOD_VM_USABLE;
}

uint32_t
panthor_kmod_vm_sync_handle(struct pan_kmod_vm *vm)
{
   struct panthor_kmod_vm *panthor_vm =
      container_of(vm, struct panthor_kmod_vm, base);

   assert(vm->flags & PAN_KMOD_VM_FLAG_TRACK_ACTIVITY);
   return panthor_vm->sync.handle;
}

uint64_t
panthor_kmod_vm_sync_lock(struct pan_kmod_vm *vm)
{
   struct panthor_kmod_vm *panthor_vm =
      container_of(vm, struct panthor_kmod_vm, base);

   assert(vm->flags & PAN_KMOD_VM_FLAG_TRACK_ACTIVITY);

   simple_mtx_lock(&panthor_vm->sync.lock);
   return panthor_vm->sync.point;
}

void
panthor_kmod_vm_sync_unlock(struct pan_kmod_vm *vm, uint64_t new_sync_point)
{
   struct panthor_kmod_vm *panthor_vm =
      container_of(vm, struct panthor_kmod_vm, base);

   assert(vm->flags & PAN_KMOD_VM_FLAG_TRACK_ACTIVITY);
   assert(new_sync_point >= panthor_vm->sync.point);

   /* Check that the new syncpoint has a fence attached to it. */
   assert(new_sync_point == panthor_vm->sync.point ||
          drmSyncobjTimelineWait(
             vm->dev->fd, &panthor_vm->sync.handle, &new_sync_point, 1, 0,
             DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE, NULL) >= 0);

   panthor_vm->sync.point = new_sync_point;
   simple_mtx_unlock(&panthor_vm->sync.lock);
}

uint32_t
panthor_kmod_get_flush_id(const struct pan_kmod_dev *dev)
{
   struct panthor_kmod_dev *panthor_dev =
      container_of(dev, struct panthor_kmod_dev, base);

   return *(panthor_dev->flush_id);
}

const struct drm_panthor_csif_info *
panthor_kmod_get_csif_props(const struct pan_kmod_dev *dev)
{
   struct panthor_kmod_dev *panthor_dev =
      container_of(dev, struct panthor_kmod_dev, base);

   return &panthor_dev->props.csif;
}

const struct pan_kmod_ops panthor_kmod_ops = {
   .dev_create = panthor_kmod_dev_create,
   .dev_destroy = panthor_kmod_dev_destroy,
   .dev_query_props = panthor_dev_query_props,
   .dev_query_user_va_range = panthor_kmod_dev_query_user_va_range,
   .bo_alloc = panthor_kmod_bo_alloc,
   .bo_free = panthor_kmod_bo_free,
   .bo_import = panthor_kmod_bo_import,
   .bo_export = panthor_kmod_bo_export,
   .bo_get_mmap_offset = panthor_kmod_bo_get_mmap_offset,
   .bo_wait = panthor_kmod_bo_wait,
   .vm_create = panthor_kmod_vm_create,
   .vm_destroy = panthor_kmod_vm_destroy,
   .vm_bind = panthor_kmod_vm_bind,
   .vm_query_state = panthor_kmod_vm_query_state,
};
