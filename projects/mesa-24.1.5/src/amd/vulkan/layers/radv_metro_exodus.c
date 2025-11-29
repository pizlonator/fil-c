/*
 * Copyright © 2021 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "radv_device.h"
#include "radv_entrypoints.h"
#include "vk_common_entrypoints.h"

VKAPI_ATTR VkResult VKAPI_CALL
metro_exodus_GetSemaphoreCounterValue(VkDevice _device, VkSemaphore _semaphore, uint64_t *pValue)
{
   /* See https://gitlab.freedesktop.org/mesa/mesa/-/issues/5119. */
   if (_semaphore == VK_NULL_HANDLE) {
      fprintf(stderr, "RADV: Ignoring vkGetSemaphoreCounterValue() with NULL semaphore (game bug)!\n");
      return VK_SUCCESS;
   }

   VK_FROM_HANDLE(radv_device, device, _device);
   return device->layer_dispatch.app.GetSemaphoreCounterValue(_device, _semaphore, pValue);
}
