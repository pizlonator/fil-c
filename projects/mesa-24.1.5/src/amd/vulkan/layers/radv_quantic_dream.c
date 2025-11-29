/*
 * Copyright © 2024 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "radv_entrypoints.h"

VKAPI_ATTR VkResult VKAPI_CALL
quantic_dream_UnmapMemory2KHR(VkDevice _device, const VkMemoryUnmapInfoKHR *pMemoryUnmapInfo)
{
   /* Detroit: Become Human repeatedly calls vkMapMemory and vkUnmapMemory on the same buffer.
    * This creates high overhead in the kernel due to mapping operation and page fault costs.
    *
    * Simply skip the unmap call to workaround it. Mapping an already-mapped region is UB in Vulkan,
    * but will correctly return the mapped pointer on RADV.
    */
   return VK_SUCCESS;
}
