/*
 * Copyright © 2020 Valve Corporation
 *
 * based on amdgpu winsys.
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_NULL_BO_H
#define RADV_NULL_BO_H

#include "radv_null_winsys.h"

struct radv_null_winsys_bo {
   struct radeon_winsys_bo base;
   struct radv_null_winsys *ws;
   void *ptr;
};

static inline struct radv_null_winsys_bo *
radv_null_winsys_bo(struct radeon_winsys_bo *bo)
{
   return (struct radv_null_winsys_bo *)bo;
}

void radv_null_bo_init_functions(struct radv_null_winsys *ws);

#endif /* RADV_NULL_BO_H */
