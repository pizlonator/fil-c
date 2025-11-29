/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_AMDGPU_SURFACE_H
#define RADV_AMDGPU_SURFACE_H

#include <amdgpu.h>

struct radv_amdgpu_winsys;

void radv_amdgpu_surface_init_functions(struct radv_amdgpu_winsys *ws);

#endif /* RADV_AMDGPU_SURFACE_H */
