/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_PRIV_BO_H
#define PANVK_PRIV_BO_H

#include <vulkan/vulkan_core.h>

#include "panfrost-job.h"

struct panvk_kmod_bo;

/* Used for internal object allocation. */
struct panvk_priv_bo {
   struct panvk_device *dev;
   struct pan_kmod_bo *bo;
   struct {
      mali_ptr dev;
      void *host;
   } addr;
};

struct panvk_priv_bo *panvk_priv_bo_create(struct panvk_device *dev,
                                           size_t size, uint32_t flags,
                                           const VkAllocationCallbacks *alloc,
                                           VkSystemAllocationScope scope);

void panvk_priv_bo_destroy(struct panvk_priv_bo *bo,
                           const VkAllocationCallbacks *alloc);

#endif
