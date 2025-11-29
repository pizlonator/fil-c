/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_WSI_H
#define PANVK_WSI_H

#include <vulkan/vulkan_core.h>

struct panvk_physical_device;

VkResult panvk_wsi_init(struct panvk_physical_device *physical_device);
void panvk_wsi_finish(struct panvk_physical_device *physical_device);

#endif
