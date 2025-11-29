/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "vk_alloc.h"
#include "vk_log.h"

#include "radv_buffer.h"
#include "radv_device.h"
#include "radv_entrypoints.h"
#include "radv_event.h"
#include "radv_rmv.h"

static void
radv_destroy_event(struct radv_device *device, const VkAllocationCallbacks *pAllocator, struct radv_event *event)
{
   if (event->bo)
      radv_bo_destroy(device, &event->base, event->bo);

   radv_rmv_log_resource_destroy(device, (uint64_t)radv_event_to_handle(event));
   vk_object_base_finish(&event->base);
   vk_free2(&device->vk.alloc, pAllocator, event);
}

VkResult
radv_create_event(struct radv_device *device, const VkEventCreateInfo *pCreateInfo,
                  const VkAllocationCallbacks *pAllocator, VkEvent *pEvent, bool is_internal)
{
   enum radeon_bo_domain bo_domain;
   enum radeon_bo_flag bo_flags;
   struct radv_event *event;
   VkResult result;

   event = vk_alloc2(&device->vk.alloc, pAllocator, sizeof(*event), 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!event)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_object_base_init(&device->vk, &event->base, VK_OBJECT_TYPE_EVENT);

   if (pCreateInfo->flags & VK_EVENT_CREATE_DEVICE_ONLY_BIT) {
      bo_domain = RADEON_DOMAIN_VRAM;
      bo_flags = RADEON_FLAG_NO_CPU_ACCESS;
   } else {
      bo_domain = RADEON_DOMAIN_GTT;
      bo_flags = RADEON_FLAG_CPU_ACCESS;
   }

   result = radv_bo_create(device, &event->base, 8, 8, bo_domain,
                           RADEON_FLAG_VA_UNCACHED | RADEON_FLAG_NO_INTERPROCESS_SHARING | bo_flags,
                           RADV_BO_PRIORITY_FENCE, 0, is_internal, &event->bo);
   if (result != VK_SUCCESS) {
      radv_destroy_event(device, pAllocator, event);
      return vk_error(device, result);
   }

   if (!(pCreateInfo->flags & VK_EVENT_CREATE_DEVICE_ONLY_BIT)) {
      event->map = (uint64_t *)radv_buffer_map(device->ws, event->bo);
      if (!event->map) {
         radv_destroy_event(device, pAllocator, event);
         return vk_error(device, VK_ERROR_OUT_OF_DEVICE_MEMORY);
      }
   }

   *pEvent = radv_event_to_handle(event);
   radv_rmv_log_event_create(device, *pEvent, pCreateInfo->flags, is_internal);
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CreateEvent(VkDevice _device, const VkEventCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator,
                 VkEvent *pEvent)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   VkResult result = radv_create_event(device, pCreateInfo, pAllocator, pEvent, false);
   if (result != VK_SUCCESS)
      return result;

   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
radv_DestroyEvent(VkDevice _device, VkEvent _event, const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   VK_FROM_HANDLE(radv_event, event, _event);

   if (!event)
      return;

   radv_destroy_event(device, pAllocator, event);
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_GetEventStatus(VkDevice _device, VkEvent _event)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   VK_FROM_HANDLE(radv_event, event, _event);

   if (vk_device_is_lost(&device->vk))
      return VK_ERROR_DEVICE_LOST;

   if (*event->map == 1)
      return VK_EVENT_SET;
   return VK_EVENT_RESET;
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_SetEvent(VkDevice _device, VkEvent _event)
{
   VK_FROM_HANDLE(radv_event, event, _event);
   *event->map = 1;

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_ResetEvent(VkDevice _device, VkEvent _event)
{
   VK_FROM_HANDLE(radv_event, event, _event);
   *event->map = 0;

   return VK_SUCCESS;
}
