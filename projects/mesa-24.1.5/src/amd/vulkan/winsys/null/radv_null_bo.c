/*
 * Copyright © 2020 Valve Corporation
 *
 * based on amdgpu winsys.
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * SPDX-License-Identifier: MIT
 */

#include "radv_null_bo.h"
#include "util/u_memory.h"

static VkResult
radv_null_winsys_bo_create(struct radeon_winsys *_ws, uint64_t size, unsigned alignment,
                           enum radeon_bo_domain initial_domain, enum radeon_bo_flag flags, unsigned priority,
                           uint64_t address, struct radeon_winsys_bo **out_bo)
{
   struct radv_null_winsys_bo *bo;

   /* Courtesy for users using NULL to check if they need to destroy the BO. */
   *out_bo = NULL;

   bo = CALLOC_STRUCT(radv_null_winsys_bo);
   if (!bo)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   bo->ptr = malloc(size);
   if (!bo->ptr)
      goto error_ptr_alloc;

   *out_bo = (struct radeon_winsys_bo *)bo;
   return VK_SUCCESS;
error_ptr_alloc:
   FREE(bo);
   return VK_ERROR_OUT_OF_HOST_MEMORY;
}

static void *
radv_null_winsys_bo_map(struct radeon_winsys *_ws, struct radeon_winsys_bo *_bo, bool use_fixed_addr, void *fixed_addr)
{
   struct radv_null_winsys_bo *bo = radv_null_winsys_bo(_bo);
   return bo->ptr;
}

static void
radv_null_winsys_bo_unmap(struct radeon_winsys *_ws, struct radeon_winsys_bo *_bo, bool replace)
{
}

static VkResult
radv_null_winsys_bo_make_resident(struct radeon_winsys *_ws, struct radeon_winsys_bo *_bo, bool resident)
{
   return VK_SUCCESS;
}

static void
radv_null_winsys_bo_destroy(struct radeon_winsys *_ws, struct radeon_winsys_bo *_bo)
{
   struct radv_null_winsys_bo *bo = radv_null_winsys_bo(_bo);
   FREE(bo->ptr);
   FREE(bo);
}

void
radv_null_bo_init_functions(struct radv_null_winsys *ws)
{
   ws->base.buffer_create = radv_null_winsys_bo_create;
   ws->base.buffer_destroy = radv_null_winsys_bo_destroy;
   ws->base.buffer_map = radv_null_winsys_bo_map;
   ws->base.buffer_unmap = radv_null_winsys_bo_unmap;
   ws->base.buffer_make_resident = radv_null_winsys_bo_make_resident;
}
