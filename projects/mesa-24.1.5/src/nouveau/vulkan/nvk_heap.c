/*
 * Copyright © 2022 Collabora Ltd. and Red Hat Inc.
 * SPDX-License-Identifier: MIT
 */
#include "nvk_heap.h"

#include "nvk_device.h"
#include "nvk_physical_device.h"
#include "nvk_queue.h"

#include "util/macros.h"

#include "nv_push.h"
#include "nvk_cl90b5.h"

VkResult
nvk_heap_init(struct nvk_device *dev, struct nvk_heap *heap,
              enum nouveau_ws_bo_flags bo_flags,
              enum nouveau_ws_bo_map_flags map_flags,
              uint32_t overalloc, bool contiguous)
{
   memset(heap, 0, sizeof(*heap));

   heap->bo_flags = bo_flags;
   if (map_flags)
      heap->bo_flags |= NOUVEAU_WS_BO_MAP;
   heap->map_flags = map_flags;
   heap->overalloc = overalloc;

   if (contiguous) {
      heap->base_addr = nouveau_ws_alloc_vma(dev->ws_dev, 0,
                                             NVK_HEAP_MAX_SIZE,
                                             0, false /* bda */,
                                             false /* sparse */);
      if (heap->base_addr == 0) {
         return vk_errorf(dev, VK_ERROR_OUT_OF_DEVICE_MEMORY,
                          "Failed to allocate VMA for heap");
      }
   }

   simple_mtx_init(&heap->mutex, mtx_plain);
   util_vma_heap_init(&heap->heap, 0, 0);

   heap->total_size = 0;
   heap->bo_count = 0;

   return VK_SUCCESS;
}

void
nvk_heap_finish(struct nvk_device *dev, struct nvk_heap *heap)
{
   for (uint32_t bo_idx = 0; bo_idx < heap->bo_count; bo_idx++) {
      if (heap->base_addr != 0) {
         nouveau_ws_bo_unbind_vma(dev->ws_dev, heap->bos[bo_idx].addr,
                                  heap->bos[bo_idx].bo->size);
      }
      if (heap->map_flags) {
         assert(heap->bos[bo_idx].map);
         nouveau_ws_bo_unmap(heap->bos[bo_idx].bo, heap->bos[bo_idx].map);
      }
      nouveau_ws_bo_destroy(heap->bos[bo_idx].bo);
   }

   util_vma_heap_finish(&heap->heap);
   simple_mtx_destroy(&heap->mutex);

   if (heap->base_addr != 0) {
      nouveau_ws_free_vma(dev->ws_dev, heap->base_addr, NVK_HEAP_MAX_SIZE,
                          false /* bda */, false /* sparse */);
   }
}

static uint64_t
encode_vma(uint32_t bo_idx, uint64_t bo_offset)
{
   assert(bo_idx < UINT16_MAX - 1);
   assert(bo_offset < (1ull << 48));
   return ((uint64_t)(bo_idx + 1) << 48) | bo_offset;
}

static uint32_t
vma_bo_idx(uint64_t offset)
{
   offset = offset >> 48;
   assert(offset > 0);
   return offset - 1;
}

static uint64_t
vma_bo_offset(uint64_t offset)
{
   return offset & BITFIELD64_MASK(48);
}

static VkResult
nvk_heap_grow_locked(struct nvk_device *dev, struct nvk_heap *heap)
{
   if (heap->bo_count >= NVK_HEAP_MAX_BO_COUNT) {
      return vk_errorf(dev, VK_ERROR_OUT_OF_DEVICE_MEMORY,
                       "Heap has already hit its maximum size");
   }

   /* First two BOs are MIN_SIZE, double after that */
   const uint64_t new_bo_size =
      NVK_HEAP_MIN_SIZE << (MAX2(heap->bo_count, 1) - 1);

   struct nouveau_ws_bo *bo =
      nouveau_ws_bo_new(dev->ws_dev, new_bo_size, 0, heap->bo_flags);
   if (bo == NULL) {
      return vk_errorf(dev, VK_ERROR_OUT_OF_DEVICE_MEMORY,
                       "Failed to allocate a heap BO: %m");
   }

   void *map = NULL;
   if (heap->map_flags) {
      map = nouveau_ws_bo_map(bo, heap->map_flags, NULL);
      if (map == NULL) {
         nouveau_ws_bo_destroy(bo);
         return vk_errorf(dev, VK_ERROR_OUT_OF_HOST_MEMORY,
                          "Failed to map a heap BO: %m");
      }
   }

   uint64_t addr = bo->offset;
   if (heap->base_addr != 0) {
      addr = heap->base_addr + heap->total_size;
      nouveau_ws_bo_bind_vma(dev->ws_dev, bo, addr, new_bo_size, 0, 0);

      /* For contiguous heaps, we can now free the padding from the previous
       * BO because the BO we just added will provide the needed padding. For
       * non-contiguous heaps, we have to leave each BO padded individually.
       */
      if (heap->bo_count > 0) {
         struct nouveau_ws_bo *prev_bo = heap->bos[heap->bo_count - 1].bo;
         assert(heap->overalloc < prev_bo->size);
         const uint64_t pad_vma =
            encode_vma(heap->bo_count - 1, prev_bo->size - heap->overalloc);
         util_vma_heap_free(&heap->heap, pad_vma, heap->overalloc);
      }
   }

   uint64_t vma = encode_vma(heap->bo_count, 0);
   assert(heap->overalloc < new_bo_size);
   util_vma_heap_free(&heap->heap, vma, new_bo_size - heap->overalloc);

   heap->bos[heap->bo_count++] = (struct nvk_heap_bo) {
      .bo = bo,
      .map = map,
      .addr = addr,
   };
   heap->total_size += new_bo_size;

   return VK_SUCCESS;
}

static VkResult
nvk_heap_alloc_locked(struct nvk_device *dev, struct nvk_heap *heap,
                      uint64_t size, uint32_t alignment,
                      uint64_t *addr_out, void **map_out)
{
   while (1) {
      uint64_t vma = util_vma_heap_alloc(&heap->heap, size, alignment);
      if (vma != 0) {
         uint32_t bo_idx = vma_bo_idx(vma);
         uint64_t bo_offset = vma_bo_offset(vma);

         assert(bo_idx < heap->bo_count);
         assert(heap->bos[bo_idx].bo != NULL);
         assert(bo_offset + size <= heap->bos[bo_idx].bo->size);

         *addr_out = heap->bos[bo_idx].addr + bo_offset;
         if (map_out != NULL) {
            if (heap->bos[bo_idx].map != NULL)
               *map_out = (char *)heap->bos[bo_idx].map + bo_offset;
            else
               *map_out = NULL;
         }

         return VK_SUCCESS;
      }

      VkResult result = nvk_heap_grow_locked(dev, heap);
      if (result != VK_SUCCESS)
         return result;
   }
}

static void
nvk_heap_free_locked(struct nvk_device *dev, struct nvk_heap *heap,
                     uint64_t addr, uint64_t size)
{
   assert(addr + size > addr);

   for (uint32_t bo_idx = 0; bo_idx < heap->bo_count; bo_idx++) {
      if (addr < heap->bos[bo_idx].addr)
         continue;

      uint64_t bo_offset = addr - heap->bos[bo_idx].addr;
      if (bo_offset >= heap->bos[bo_idx].bo->size)
         continue;

      assert(bo_offset + size <= heap->bos[bo_idx].bo->size);
      uint64_t vma = encode_vma(bo_idx, bo_offset);

      util_vma_heap_free(&heap->heap, vma, size);
      return;
   }
   assert(!"Failed to find heap BO");
}

VkResult
nvk_heap_alloc(struct nvk_device *dev, struct nvk_heap *heap,
               uint64_t size, uint32_t alignment,
               uint64_t *addr_out, void **map_out)
{
   simple_mtx_lock(&heap->mutex);
   VkResult result = nvk_heap_alloc_locked(dev, heap, size, alignment,
                                           addr_out, map_out);
   simple_mtx_unlock(&heap->mutex);

   return result;
}

VkResult
nvk_heap_upload(struct nvk_device *dev, struct nvk_heap *heap,
                const void *data, size_t size, uint32_t alignment,
                uint64_t *addr_out)
{
   simple_mtx_lock(&heap->mutex);
   void *map = NULL;
   VkResult result = nvk_heap_alloc_locked(dev, heap, size, alignment,
                                           addr_out, &map);
   simple_mtx_unlock(&heap->mutex);

   if (result != VK_SUCCESS)
      return result;

   if (map != NULL && (heap->map_flags & NOUVEAU_WS_BO_WR)) {
      /* If we have a map, copy directly with memcpy */
      memcpy(map, data, size);
   } else {
      /* Otherwise, kick off an upload with the upload queue.
       *
       * This is a queued operation that the driver ensures happens before any
       * more client work via semaphores.  Because this is asynchronous and
       * heap allocations are synchronous we have to be a bit careful here.
       * The heap only ever tracks the current known CPU state of everything
       * while the upload queue makes that state valid at some point in the
       * future.
       *
       * This can be especially tricky for very fast upload/free cycles such
       * as if the client compiles a shader, throws it away without using it,
       * and then compiles another shader that ends up at the same address.
       * What makes this all correct is the fact that the everything on the
       * upload queue happens in a well-defined device-wide order.  In this
       * case the first shader will get uploaded and then the second will get
       * uploaded over top of it.  As long as we don't free the memory out
       * from under the upload queue, everything will end up in the correct
       * state by the time the client's shaders actually execute.
       */
      result = nvk_upload_queue_upload(dev, &dev->upload, *addr_out, data, size);
      if (result != VK_SUCCESS) {
         nvk_heap_free(dev, heap, *addr_out, size);
         return result;
      }
   }

   return VK_SUCCESS;
}

void
nvk_heap_free(struct nvk_device *dev, struct nvk_heap *heap,
              uint64_t addr, uint64_t size)
{
   simple_mtx_lock(&heap->mutex);
   nvk_heap_free_locked(dev, heap, addr, size);
   simple_mtx_unlock(&heap->mutex);
}
