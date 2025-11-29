/*
 * Copyright © 2022 Collabora Ltd. and Red Hat Inc.
 * SPDX-License-Identifier: MIT
 */
#include "nvk_device.h"

#include "nvk_cmd_buffer.h"
#include "nvk_entrypoints.h"
#include "nvk_instance.h"
#include "nvk_physical_device.h"
#include "nvk_shader.h"

#include "vk_pipeline_cache.h"
#include "vulkan/wsi/wsi_common.h"

#include "nouveau_context.h"

#include <fcntl.h>
#include <xf86drm.h>

#include "cl9097.h"
#include "clb097.h"
#include "clc397.h"

static void
nvk_slm_area_init(struct nvk_slm_area *area)
{
   memset(area, 0, sizeof(*area));
   simple_mtx_init(&area->mutex, mtx_plain);
}

static void
nvk_slm_area_finish(struct nvk_slm_area *area)
{
   simple_mtx_destroy(&area->mutex);
   if (area->bo)
      nouveau_ws_bo_destroy(area->bo);
}

struct nouveau_ws_bo *
nvk_slm_area_get_bo_ref(struct nvk_slm_area *area,
                        uint32_t *bytes_per_warp_out,
                        uint32_t *bytes_per_tpc_out)
{
   simple_mtx_lock(&area->mutex);
   struct nouveau_ws_bo *bo = area->bo;
   if (bo)
      nouveau_ws_bo_ref(bo);
   *bytes_per_warp_out = area->bytes_per_warp;
   *bytes_per_tpc_out = area->bytes_per_tpc;
   simple_mtx_unlock(&area->mutex);

   return bo;
}

static VkResult
nvk_slm_area_ensure(struct nvk_device *dev,
                    struct nvk_slm_area *area,
                    uint32_t bytes_per_thread)
{
   struct nvk_physical_device *pdev = nvk_device_physical(dev);
   assert(bytes_per_thread < (1 << 24));

   /* TODO: Volta+doesn't use CRC */
   const uint32_t crs_size = 0;

   uint64_t bytes_per_warp = bytes_per_thread * 32 + crs_size;

   /* The hardware seems to require this alignment for
    * NV9097_SET_SHADER_LOCAL_MEMORY_E_DEFAULT_SIZE_PER_WARP
    */
   bytes_per_warp = align64(bytes_per_warp, 0x200);

   uint64_t bytes_per_mp = bytes_per_warp * pdev->info.max_warps_per_mp;
   uint64_t bytes_per_tpc = bytes_per_mp * pdev->info.mp_per_tpc;

   /* The hardware seems to require this alignment for
    * NVA0C0_SET_SHADER_LOCAL_MEMORY_NON_THROTTLED_A_SIZE_LOWER.
    */
   bytes_per_tpc = align64(bytes_per_tpc, 0x8000);

   /* nvk_slm_area::bytes_per_mp only ever increases so we can check this
    * outside the lock and exit early in the common case.  We only need to
    * take the lock if we're actually going to resize.
    *
    * Also, we only care about bytes_per_mp and not bytes_per_warp because
    * they are integer multiples of each other.
    */
   if (likely(bytes_per_tpc <= area->bytes_per_tpc))
      return VK_SUCCESS;

   uint64_t size = bytes_per_tpc * pdev->info.tpc_count;

   /* The hardware seems to require this alignment for
    * NV9097_SET_SHADER_LOCAL_MEMORY_D_SIZE_LOWER.
    */
   size = align64(size, 0x20000);

   struct nouveau_ws_bo *bo =
      nouveau_ws_bo_new(dev->ws_dev, size, 0,
                        NOUVEAU_WS_BO_LOCAL | NOUVEAU_WS_BO_NO_SHARE);
   if (bo == NULL)
      return vk_error(dev, VK_ERROR_OUT_OF_DEVICE_MEMORY);

   struct nouveau_ws_bo *unref_bo;
   simple_mtx_lock(&area->mutex);
   if (bytes_per_tpc <= area->bytes_per_tpc) {
      /* We lost the race, throw away our BO */
      assert(area->bytes_per_warp == bytes_per_warp);
      unref_bo = bo;
   } else {
      unref_bo = area->bo;
      area->bo = bo;
      area->bytes_per_warp = bytes_per_warp;
      area->bytes_per_tpc = bytes_per_tpc;
   }
   simple_mtx_unlock(&area->mutex);

   if (unref_bo)
      nouveau_ws_bo_destroy(unref_bo);

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
nvk_CreateDevice(VkPhysicalDevice physicalDevice,
                 const VkDeviceCreateInfo *pCreateInfo,
                 const VkAllocationCallbacks *pAllocator,
                 VkDevice *pDevice)
{
   VK_FROM_HANDLE(nvk_physical_device, pdev, physicalDevice);
   VkResult result = VK_ERROR_OUT_OF_HOST_MEMORY;
   struct nvk_device *dev;

   dev = vk_zalloc2(&pdev->vk.instance->alloc, pAllocator,
                    sizeof(*dev), 8, VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   if (!dev)
      return vk_error(pdev, VK_ERROR_OUT_OF_HOST_MEMORY);

   struct vk_device_dispatch_table dispatch_table;
   vk_device_dispatch_table_from_entrypoints(&dispatch_table,
                                             &nvk_device_entrypoints, true);
   vk_device_dispatch_table_from_entrypoints(&dispatch_table,
                                             &wsi_device_entrypoints, false);

   result = vk_device_init(&dev->vk, &pdev->vk, &dispatch_table,
                           pCreateInfo, pAllocator);
   if (result != VK_SUCCESS)
      goto fail_alloc;

   dev->vk.shader_ops = &nvk_device_shader_ops;

   drmDevicePtr drm_device = NULL;
   int ret = drmGetDeviceFromDevId(pdev->render_dev, 0, &drm_device);
   if (ret != 0) {
      result = vk_errorf(dev, VK_ERROR_INITIALIZATION_FAILED,
                         "Failed to get DRM device: %m");
      goto fail_init;
   }

   dev->ws_dev = nouveau_ws_device_new(drm_device);
   drmFreeDevice(&drm_device);
   if (dev->ws_dev == NULL) {
      result = vk_errorf(dev, VK_ERROR_INITIALIZATION_FAILED,
                         "Failed to get DRM device: %m");
      goto fail_init;
   }

   vk_device_set_drm_fd(&dev->vk, dev->ws_dev->fd);
   dev->vk.command_buffer_ops = &nvk_cmd_buffer_ops;

   result = nvk_upload_queue_init(dev, &dev->upload);
   if (result != VK_SUCCESS)
      goto fail_ws_dev;

   result = nvk_descriptor_table_init(dev, &dev->images,
                                      8 * 4 /* tic entry size */,
                                      1024, 1024 * 1024);
   if (result != VK_SUCCESS)
      goto fail_upload;

   /* Reserve the descriptor at offset 0 to be the null descriptor */
   uint32_t null_image[8] = { 0, };
   ASSERTED uint32_t null_image_index;
   result = nvk_descriptor_table_add(dev, &dev->images,
                                     null_image, sizeof(null_image),
                                     &null_image_index);
   assert(result == VK_SUCCESS);
   assert(null_image_index == 0);

   result = nvk_descriptor_table_init(dev, &dev->samplers,
                                      8 * 4 /* tsc entry size */,
                                      4096, 4096);
   if (result != VK_SUCCESS)
      goto fail_images;

   /* If we have a full BAR, go ahead and do shader uploads on the CPU.
    * Otherwise, we fall back to doing shader uploads via the upload queue.
    *
    * Also, the I-cache pre-fetches and we don't really know by how much.
    * Over-allocating shader BOs by 4K ensures we don't run past.
    */
   enum nouveau_ws_bo_map_flags shader_map_flags = 0;
   if (pdev->info.bar_size_B >= pdev->info.vram_size_B)
      shader_map_flags = NOUVEAU_WS_BO_WR;
   result = nvk_heap_init(dev, &dev->shader_heap,
                          NOUVEAU_WS_BO_LOCAL | NOUVEAU_WS_BO_NO_SHARE,
                          shader_map_flags,
                          4096 /* overalloc */,
                          pdev->info.cls_eng3d < VOLTA_A);
   if (result != VK_SUCCESS)
      goto fail_samplers;

   result = nvk_heap_init(dev, &dev->event_heap,
                          NOUVEAU_WS_BO_LOCAL | NOUVEAU_WS_BO_NO_SHARE,
                          NOUVEAU_WS_BO_WR,
                          0 /* overalloc */, false /* contiguous */);
   if (result != VK_SUCCESS)
      goto fail_shader_heap;

   nvk_slm_area_init(&dev->slm);

   void *zero_map;
   dev->zero_page = nouveau_ws_bo_new_mapped(dev->ws_dev, 0x1000, 0,
                                             NOUVEAU_WS_BO_LOCAL |
                                             NOUVEAU_WS_BO_NO_SHARE,
                                             NOUVEAU_WS_BO_WR, &zero_map);
   if (dev->zero_page == NULL)
      goto fail_slm;

   memset(zero_map, 0, 0x1000);
   nouveau_ws_bo_unmap(dev->zero_page, zero_map);

   if (pdev->info.cls_eng3d >= FERMI_A &&
       pdev->info.cls_eng3d < MAXWELL_A) {
      /* max size is 256k */
      dev->vab_memory = nouveau_ws_bo_new(dev->ws_dev, 1 << 17, 1 << 20,
                                          NOUVEAU_WS_BO_LOCAL |
                                          NOUVEAU_WS_BO_NO_SHARE);
      if (dev->vab_memory == NULL)
         goto fail_zero_page;
   }

   result = nvk_queue_init(dev, &dev->queue,
                           &pCreateInfo->pQueueCreateInfos[0], 0);
   if (result != VK_SUCCESS)
      goto fail_vab_memory;

   struct vk_pipeline_cache_create_info cache_info = {
      .weak_ref = true,
   };
   dev->vk.mem_cache = vk_pipeline_cache_create(&dev->vk, &cache_info, NULL);
   if (dev->vk.mem_cache == NULL) {
      result = VK_ERROR_OUT_OF_HOST_MEMORY;
      goto fail_queue;
   }

   result = nvk_device_init_meta(dev);
   if (result != VK_SUCCESS)
      goto fail_mem_cache;

   *pDevice = nvk_device_to_handle(dev);

   return VK_SUCCESS;

fail_mem_cache:
   vk_pipeline_cache_destroy(dev->vk.mem_cache, NULL);
fail_queue:
   nvk_queue_finish(dev, &dev->queue);
fail_vab_memory:
   if (dev->vab_memory)
      nouveau_ws_bo_destroy(dev->vab_memory);
fail_zero_page:
   nouveau_ws_bo_destroy(dev->zero_page);
fail_slm:
   nvk_slm_area_finish(&dev->slm);
   nvk_heap_finish(dev, &dev->event_heap);
fail_shader_heap:
   nvk_heap_finish(dev, &dev->shader_heap);
fail_samplers:
   nvk_descriptor_table_finish(dev, &dev->samplers);
fail_images:
   nvk_descriptor_table_finish(dev, &dev->images);
fail_upload:
   nvk_upload_queue_finish(dev, &dev->upload);
fail_ws_dev:
   nouveau_ws_device_destroy(dev->ws_dev);
fail_init:
   vk_device_finish(&dev->vk);
fail_alloc:
   vk_free(&dev->vk.alloc, dev);
   return result;
}

VKAPI_ATTR void VKAPI_CALL
nvk_DestroyDevice(VkDevice _device, const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(nvk_device, dev, _device);

   if (!dev)
      return;

   nvk_device_finish_meta(dev);

   vk_pipeline_cache_destroy(dev->vk.mem_cache, NULL);
   nvk_queue_finish(dev, &dev->queue);
   if (dev->vab_memory)
      nouveau_ws_bo_destroy(dev->vab_memory);
   nouveau_ws_bo_destroy(dev->zero_page);
   vk_device_finish(&dev->vk);

   /* Idle the upload queue before we tear down heaps */
   nvk_upload_queue_sync(dev, &dev->upload);

   nvk_slm_area_finish(&dev->slm);
   nvk_heap_finish(dev, &dev->event_heap);
   nvk_heap_finish(dev, &dev->shader_heap);
   nvk_descriptor_table_finish(dev, &dev->samplers);
   nvk_descriptor_table_finish(dev, &dev->images);
   nvk_upload_queue_finish(dev, &dev->upload);
   nouveau_ws_device_destroy(dev->ws_dev);
   vk_free(&dev->vk.alloc, dev);
}

VKAPI_ATTR VkResult VKAPI_CALL
nvk_GetCalibratedTimestampsKHR(VkDevice _device,
                               uint32_t timestampCount,
                               const VkCalibratedTimestampInfoKHR *pTimestampInfos,
                               uint64_t *pTimestamps,
                               uint64_t *pMaxDeviation)
{
   VK_FROM_HANDLE(nvk_device, dev, _device);
   struct nvk_physical_device *pdev = nvk_device_physical(dev);
   uint64_t max_clock_period = 0;
   uint64_t begin, end;
   int d;

#ifdef CLOCK_MONOTONIC_RAW
   begin = vk_clock_gettime(CLOCK_MONOTONIC_RAW);
#else
   begin = vk_clock_gettime(CLOCK_MONOTONIC);
#endif

   for (d = 0; d < timestampCount; d++) {
      switch (pTimestampInfos[d].timeDomain) {
      case VK_TIME_DOMAIN_DEVICE_KHR:
         pTimestamps[d] = nouveau_ws_device_timestamp(pdev->ws_dev);
         max_clock_period = MAX2(max_clock_period, 1); /* FIXME: Is timestamp period actually 1? */
         break;
      case VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR:
         pTimestamps[d] = vk_clock_gettime(CLOCK_MONOTONIC);
         max_clock_period = MAX2(max_clock_period, 1);
         break;

#ifdef CLOCK_MONOTONIC_RAW
      case VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR:
         pTimestamps[d] = begin;
         break;
#endif
      default:
         pTimestamps[d] = 0;
         break;
      }
   }

#ifdef CLOCK_MONOTONIC_RAW
   end = vk_clock_gettime(CLOCK_MONOTONIC_RAW);
#else
   end = vk_clock_gettime(CLOCK_MONOTONIC);
#endif

   *pMaxDeviation = vk_time_max_deviation(begin, end, max_clock_period);

   return VK_SUCCESS;
}

VkResult
nvk_device_ensure_slm(struct nvk_device *dev,
                      uint32_t bytes_per_thread)
{
   return nvk_slm_area_ensure(dev, &dev->slm, bytes_per_thread);
}
