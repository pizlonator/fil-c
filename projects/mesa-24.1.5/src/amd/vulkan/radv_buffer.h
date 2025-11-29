/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_BUFFER_H
#define RADV_BUFFER_H

#include "radv_radeon_winsys.h"

#include "vk_buffer.h"

struct radv_device;

struct radv_buffer {
   struct vk_buffer vk;

   /* Set when bound */
   struct radeon_winsys_bo *bo;
   VkDeviceSize offset;
   uint64_t bo_va;
   uint64_t range;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(radv_buffer, vk.base, VkBuffer, VK_OBJECT_TYPE_BUFFER)

void radv_buffer_init(struct radv_buffer *buffer, struct radv_device *device, struct radeon_winsys_bo *bo,
                      uint64_t size, uint64_t offset);
void radv_buffer_finish(struct radv_buffer *buffer);

VkResult radv_create_buffer(struct radv_device *device, const VkBufferCreateInfo *pCreateInfo,
                            const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer, bool is_internal);

VkResult radv_bo_create(struct radv_device *device, struct vk_object_base *object, uint64_t size, unsigned alignment,
                        enum radeon_bo_domain domain, enum radeon_bo_flag flags, unsigned priority, uint64_t address,
                        bool is_internal, struct radeon_winsys_bo **out_bo);

VkResult radv_bo_virtual_bind(struct radv_device *device, struct vk_object_base *object,
                              struct radeon_winsys_bo *parent, uint64_t offset, uint64_t size,
                              struct radeon_winsys_bo *bo, uint64_t bo_offset);

void radv_bo_destroy(struct radv_device *device, struct vk_object_base *object, struct radeon_winsys_bo *bo);

#endif /* RADV_BUFFER_H */
