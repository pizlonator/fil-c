/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 */

#ifndef TU_WSI_H
#define TU_WSI_H

#include "tu_common.h"

#if defined(VK_USE_PLATFORM_WAYLAND_KHR) || \
    defined(VK_USE_PLATFORM_XCB_KHR) || \
    defined(VK_USE_PLATFORM_XLIB_KHR) || \
    defined(VK_USE_PLATFORM_DISPLAY_KHR)
#define TU_USE_WSI_PLATFORM
#endif

VkResult
tu_wsi_init(struct tu_physical_device *physical_device);

void
tu_wsi_finish(struct tu_physical_device *physical_device);

#endif /* TU_WSI_H */
