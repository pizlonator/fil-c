/*
 * Copyright © 2020 Valve Corporation
 *
 * based on amdgpu winsys.
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_NULL_CS_H
#define RADV_NULL_CS_H

#include "radv_null_winsys.h"
#include "radv_radeon_winsys.h"

struct radv_null_ctx {
   struct radv_null_winsys *ws;
};

static inline struct radv_null_ctx *
radv_null_ctx(struct radeon_winsys_ctx *base)
{
   return (struct radv_null_ctx *)base;
}

void radv_null_cs_init_functions(struct radv_null_winsys *ws);

#endif /* RADV_NULL_CS_H */
