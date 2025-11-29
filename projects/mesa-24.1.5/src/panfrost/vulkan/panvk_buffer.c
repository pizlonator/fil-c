/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "panvk_buffer.h"
#include "panvk_device.h"
#include "panvk_device_memory.h"
#include "panvk_entrypoints.h"

#include "vk_log.h"

#define PANVK_MAX_BUFFER_SIZE (1 << 30)

VKAPI_ATTR VkDeviceAddress VKAPI_CALL
panvk_GetBufferDeviceAddress(VkDevice _device,
                             const VkBufferDeviceAddressInfo *pInfo)
{
   VK_FROM_HANDLE(panvk_buffer, buffer, pInfo->buffer);

   return buffer->dev_addr;
}

VKAPI_ATTR void VKAPI_CALL
panvk_GetBufferMemoryRequirements2(VkDevice device,
                                   const VkBufferMemoryRequirementsInfo2 *pInfo,
                                   VkMemoryRequirements2 *pMemoryRequirements)
{
   VK_FROM_HANDLE(panvk_buffer, buffer, pInfo->buffer);

   const uint64_t align = 64;
   const uint64_t size = align64(buffer->vk.size, align);

   pMemoryRequirements->memoryRequirements.memoryTypeBits = 1;
   pMemoryRequirements->memoryRequirements.alignment = align;
   pMemoryRequirements->memoryRequirements.size = size;
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_BindBufferMemory2(VkDevice device, uint32_t bindInfoCount,
                        const VkBindBufferMemoryInfo *pBindInfos)
{
   for (uint32_t i = 0; i < bindInfoCount; ++i) {
      VK_FROM_HANDLE(panvk_device_memory, mem, pBindInfos[i].memory);
      VK_FROM_HANDLE(panvk_buffer, buffer, pBindInfos[i].buffer);
      struct pan_kmod_bo *old_bo = buffer->bo;

      assert(mem != NULL);

      buffer->bo = pan_kmod_bo_get(mem->bo);
      buffer->dev_addr = mem->addr.dev + pBindInfos[i].memoryOffset;

      /* FIXME: Only host map for index buffers so we can do the min/max
       * index retrieval on the CPU. This is all broken anyway and the
       * min/max search should be done with a compute shader that also
       * patches the job descriptor accordingly (basically an indirect draw).
       *
       * Make sure this goes away as soon as we fixed indirect draws.
       */
      if (buffer->vk.usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) {
         VkDeviceSize offset = pBindInfos[i].memoryOffset;
         VkDeviceSize pgsize = getpagesize();
         off_t map_start = offset & ~(pgsize - 1);
         off_t map_end = offset + buffer->vk.size;
         void *map_addr =
            pan_kmod_bo_mmap(mem->bo, map_start, map_end - map_start,
                             PROT_WRITE, MAP_SHARED, NULL);

         assert(map_addr != MAP_FAILED);
         buffer->host_ptr = map_addr + (offset & pgsize);
      }

      pan_kmod_bo_put(old_bo);
   }
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_CreateBuffer(VkDevice _device, const VkBufferCreateInfo *pCreateInfo,
                   const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   struct panvk_buffer *buffer;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);

   if (pCreateInfo->size > PANVK_MAX_BUFFER_SIZE)
      return vk_error(device, VK_ERROR_OUT_OF_DEVICE_MEMORY);

   buffer =
      vk_buffer_create(&device->vk, pCreateInfo, pAllocator, sizeof(*buffer));
   if (buffer == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   *pBuffer = panvk_buffer_to_handle(buffer);

   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
panvk_DestroyBuffer(VkDevice _device, VkBuffer _buffer,
                    const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_buffer, buffer, _buffer);

   if (!buffer)
      return;

   if (buffer->host_ptr) {
      VkDeviceSize pgsize = getpagesize();
      uintptr_t map_start = (uintptr_t)buffer->host_ptr & ~(pgsize - 1);
      uintptr_t map_end =
         ALIGN_POT((uintptr_t)buffer->host_ptr + buffer->vk.size, pgsize);
      ASSERTED int ret = os_munmap((void *)map_start, map_end - map_start);

      assert(!ret);
      buffer->host_ptr = NULL;
   }

   pan_kmod_bo_put(buffer->bo);
   vk_buffer_destroy(&device->vk, pAllocator, &buffer->vk);
}
