/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_DEVICE_MEMORY_H
#define PANVK_DEVICE_MEMORY_H

#include <stdint.h>

#include "vk_device_memory.h"

struct panvk_priv_bo;

struct panvk_device_memory {
   struct vk_device_memory vk;
   struct pan_kmod_bo *bo;
   struct {
      uint64_t dev;
      void *host;
   } addr;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(panvk_device_memory, vk.base, VkDeviceMemory,
                               VK_OBJECT_TYPE_DEVICE_MEMORY)

#endif
