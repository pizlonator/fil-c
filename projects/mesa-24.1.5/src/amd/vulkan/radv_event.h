/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_EVENT_H
#define RADV_EVENT_H

#include "radv_radeon_winsys.h"

#include "vk_object.h"

struct radv_device;

struct radv_event {
   struct vk_object_base base;
   struct radeon_winsys_bo *bo;
   uint64_t *map;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(radv_event, base, VkEvent, VK_OBJECT_TYPE_EVENT)

VkResult radv_create_event(struct radv_device *device, const VkEventCreateInfo *pCreateInfo,
                           const VkAllocationCallbacks *pAllocator, VkEvent *pEvent, bool is_internal);

#endif /* RADV_EVENT_H */
