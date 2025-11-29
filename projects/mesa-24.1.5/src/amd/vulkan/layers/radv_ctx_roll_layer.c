/*
 * Copyright © 2024 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "radv_cmd_buffer.h"
#include "radv_device.h"
#include "radv_entrypoints.h"

VKAPI_ATTR VkResult VKAPI_CALL
ctx_roll_QueuePresentKHR(VkQueue _queue, const VkPresentInfoKHR *pPresentInfo)
{
   VK_FROM_HANDLE(radv_queue, queue, _queue);
   struct radv_device *device = radv_queue_device(queue);

   simple_mtx_lock(&device->ctx_roll_mtx);

   if (device->ctx_roll_file) {
      fclose(device->ctx_roll_file);
      device->ctx_roll_file = NULL;
   }

   simple_mtx_unlock(&device->ctx_roll_mtx);

   return device->layer_dispatch.ctx_roll.QueuePresentKHR(_queue, pPresentInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL
ctx_roll_QueueSubmit2(VkQueue _queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits, VkFence _fence)
{
   VK_FROM_HANDLE(radv_queue, queue, _queue);
   struct radv_device *device = radv_queue_device(queue);

   simple_mtx_lock(&device->ctx_roll_mtx);

   if (device->ctx_roll_file) {
      for (uint32_t submit_index = 0; submit_index < submitCount; submit_index++) {
         const VkSubmitInfo2 *submit = pSubmits + submit_index;
         for (uint32_t i = 0; i < submit->commandBufferInfoCount; i++) {
            VK_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, submit->pCommandBufferInfos[i].commandBuffer);
            fprintf(device->ctx_roll_file, "\n%s:\n", vk_object_base_name(&cmd_buffer->vk.base));
            device->ws->cs_dump(cmd_buffer->cs, device->ctx_roll_file, NULL, 0, RADV_CS_DUMP_TYPE_CTX_ROLLS);
         }
      }
   }

   simple_mtx_unlock(&device->ctx_roll_mtx);

   return device->layer_dispatch.ctx_roll.QueueSubmit2(_queue, submitCount, pSubmits, _fence);
}
