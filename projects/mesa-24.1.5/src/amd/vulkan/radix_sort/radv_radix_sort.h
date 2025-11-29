/*
 * Copyright © 2022 Konstantin Seurer
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_RADIX_SORT_H
#define RADV_RADIX_SORT_H

#include "radix_sort_vk_devaddr.h"

radix_sort_vk_t *radv_create_radix_sort_u64(VkDevice device, VkAllocationCallbacks const *ac, VkPipelineCache pc);

#endif
