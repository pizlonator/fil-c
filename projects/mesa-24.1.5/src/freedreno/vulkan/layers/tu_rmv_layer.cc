/*
 * Copyright © 2024 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#include "tu_device.h"
#include "tu_entrypoints.h"
#include "tu_rmv.h"
#include "vk_common_entrypoints.h"
#include "wsi_common_entrypoints.h"

VKAPI_ATTR VkResult VKAPI_CALL
tu_rmv_QueuePresentKHR(VkQueue _queue, const VkPresentInfoKHR *pPresentInfo)
{
   VK_FROM_HANDLE(tu_queue, queue, _queue);
   struct tu_device *device = queue->device;

   VkResult result = wsi_QueuePresentKHR(_queue, pPresentInfo);
   if (!(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
       || !device->vk.memory_trace_data.is_enabled)
      return result;

   vk_rmv_log_misc_token(&device->vk, VK_RMV_MISC_EVENT_TYPE_PRESENT);
   return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
tu_rmv_FlushMappedMemoryRanges(VkDevice _device, uint32_t memoryRangeCount,
                               const VkMappedMemoryRange *pMemoryRanges)
{
   VK_FROM_HANDLE(tu_device, device, _device);

   VkResult result = tu_FlushMappedMemoryRanges(_device, memoryRangeCount,
                                                pMemoryRanges);
   if (result != VK_SUCCESS || !device->vk.memory_trace_data.is_enabled)
      return result;

   vk_rmv_log_misc_token(&device->vk, VK_RMV_MISC_EVENT_TYPE_FLUSH_MAPPED_RANGE);
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
tu_rmv_InvalidateMappedMemoryRanges(VkDevice _device, uint32_t memoryRangeCount,
                                    const VkMappedMemoryRange *pMemoryRanges)
{
   VK_FROM_HANDLE(tu_device, device, _device);

   VkResult result = tu_InvalidateMappedMemoryRanges(_device, memoryRangeCount,
                                                     pMemoryRanges);
   if (result != VK_SUCCESS || !device->vk.memory_trace_data.is_enabled)
      return result;

   vk_rmv_log_misc_token(&device->vk, VK_RMV_MISC_EVENT_TYPE_INVALIDATE_RANGES);
   return VK_SUCCESS;
}

VkResult tu_rmv_SetDebugUtilsObjectNameEXT(VkDevice _device,
                                           const VkDebugUtilsObjectNameInfoEXT* pNameInfo)
{
   assert(pNameInfo->sType == VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
   VK_FROM_HANDLE(tu_device, device, _device);

   VkResult result = vk_common_SetDebugUtilsObjectNameEXT(_device, pNameInfo);
   if (result != VK_SUCCESS || !device->vk.memory_trace_data.is_enabled)
      return result;

   switch (pNameInfo->objectType) {
   case VK_OBJECT_TYPE_BUFFER:
   case VK_OBJECT_TYPE_DEVICE_MEMORY:
   case VK_OBJECT_TYPE_IMAGE:
   case VK_OBJECT_TYPE_EVENT:
   case VK_OBJECT_TYPE_QUERY_POOL:
   case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
   case VK_OBJECT_TYPE_PIPELINE:
      break;
   default:
      return VK_SUCCESS;
   }

   tu_rmv_log_resource_name(device, (const void *) pNameInfo->objectHandle,
                            pNameInfo->pObjectName);
   return VK_SUCCESS;
}
