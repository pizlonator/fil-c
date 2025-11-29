/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_WSI_H
#define RADV_WSI_H

#include "radv_physical_device.h"

#if defined(VK_USE_PLATFORM_WAYLAND_KHR) || defined(VK_USE_PLATFORM_XCB_KHR) || defined(VK_USE_PLATFORM_XLIB_KHR) ||   \
   defined(VK_USE_PLATFORM_DISPLAY_KHR)
#define RADV_USE_WSI_PLATFORM
#endif

VkResult radv_init_wsi(struct radv_physical_device *pdev);

void radv_finish_wsi(struct radv_physical_device *pdev);

#endif /* RADV_WSI_H */
