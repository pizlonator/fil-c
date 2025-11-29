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

#ifndef RADV_AMDGPU_BO_H
#define RADV_AMDGPU_BO_H

#include "radv_amdgpu_winsys.h"

struct radv_amdgpu_winsys_bo_log {
   struct list_head list;
   uint64_t va;
   uint64_t size;
   uint64_t timestamp; /* CPU timestamp */
   uint8_t is_virtual : 1;
   uint8_t destroyed : 1;
};

struct radv_amdgpu_map_range {
   uint64_t offset;
   uint64_t size;
   struct radv_amdgpu_winsys_bo *bo;
   uint64_t bo_offset;
};

struct radv_amdgpu_winsys_bo {
   struct radeon_winsys_bo base;
   amdgpu_va_handle va_handle;
   bool is_virtual;
   uint8_t priority;

   union {
      /* physical bo */
      struct {
         amdgpu_bo_handle bo;
         uint32_t bo_handle;

         void *cpu_map;
      };
      /* virtual bo */
      struct {
         struct u_rwlock lock;

         struct radv_amdgpu_map_range *ranges;
         uint32_t range_count;
         uint32_t range_capacity;

         struct radv_amdgpu_winsys_bo **bos;
         uint32_t bo_count;
         uint32_t bo_capacity;
      };
   };
};

static inline struct radv_amdgpu_winsys_bo *
radv_amdgpu_winsys_bo(struct radeon_winsys_bo *bo)
{
   return (struct radv_amdgpu_winsys_bo *)bo;
}

void radv_amdgpu_bo_init_functions(struct radv_amdgpu_winsys *ws);

#endif /* RADV_AMDGPU_BO_H */
