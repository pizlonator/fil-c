/*
 * Copyright © 2023 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "radv_cmd_buffer.h"
#include "radv_device.h"
#include "radv_entrypoints.h"
#include "vk_common_entrypoints.h"
#include "vk_framebuffer.h"

VKAPI_ATTR void VKAPI_CALL
rage2_CmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBegin,
                         VkSubpassContents contents)
{
   VK_FROM_HANDLE(vk_framebuffer, framebuffer, pRenderPassBegin->framebuffer);
   VK_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);

   VkRenderPassBeginInfo render_pass_begin = {
      .sType = pRenderPassBegin->sType,
      .pNext = pRenderPassBegin->pNext,
      .renderPass = pRenderPassBegin->renderPass,
      .framebuffer = pRenderPassBegin->framebuffer,
      .clearValueCount = pRenderPassBegin->clearValueCount,
      .pClearValues = pRenderPassBegin->pClearValues,
   };

   /* RAGE2 seems to incorrectly set the render area and with dynamic rendering the concept of
    * framebuffer dimensions goes away. Forcing the render area to be the framebuffer dimensions
    * restores previous logic and it fixes rendering issues.
    */
   render_pass_begin.renderArea.offset.x = 0;
   render_pass_begin.renderArea.offset.y = 0;
   render_pass_begin.renderArea.extent.width = framebuffer->width;
   render_pass_begin.renderArea.extent.height = framebuffer->height;

   device->layer_dispatch.app.CmdBeginRenderPass(commandBuffer, &render_pass_begin, contents);
}
