/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based on amdgpu winsys.
 * Copyright © 2011 Marek Olšák <maraeo@gmail.com>
 * Copyright © 2015 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_AMDGPU_WINSYS_PUBLIC_H
#define RADV_AMDGPU_WINSYS_PUBLIC_H

struct radeon_winsys *radv_amdgpu_winsys_create(int fd, uint64_t debug_flags, uint64_t perftest_flags,
                                                bool reserve_vmid);

struct radeon_winsys *radv_dummy_winsys_create(void);

#endif /* RADV_AMDGPU_WINSYS_PUBLIC_H */
