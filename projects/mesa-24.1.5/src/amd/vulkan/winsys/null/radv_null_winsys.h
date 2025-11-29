/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 * based on amdgpu winsys.
 * Copyright © 2011 Marek Olšák <maraeo@gmail.com>
 * Copyright © 2015 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_NULL_WINSYS_H
#define RADV_NULL_WINSYS_H

#include "util/list.h"
#include "ac_gpu_info.h"
#include "radv_radeon_winsys.h"

struct vk_sync_type;

struct radv_null_winsys {
   struct radeon_winsys base;
   const struct vk_sync_type *sync_types[2];
};

static inline struct radv_null_winsys *
radv_null_winsys(struct radeon_winsys *base)
{
   return (struct radv_null_winsys *)base;
}

#endif /* RADV_NULL_WINSYS_H */
