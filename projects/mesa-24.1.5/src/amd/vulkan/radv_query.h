/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_QUERY_H
#define RADV_QUERY_H

#include "amd_family.h"

#include "vk_query_pool.h"

struct radv_cmd_buffer;

struct radv_query_pool {
   struct vk_query_pool vk;
   struct radeon_winsys_bo *bo;
   uint32_t stride;
   uint32_t availability_offset;
   uint64_t size;
   char *ptr;
   bool uses_gds; /* For NGG GS on GFX10+ */
   bool uses_ace; /* For task shader invocations on GFX10.3+ */
};

VK_DEFINE_NONDISP_HANDLE_CASTS(radv_query_pool, vk.base, VkQueryPool, VK_OBJECT_TYPE_QUERY_POOL)

static inline uint64_t
radv_get_tdr_timeout_for_ip(enum amd_ip_type ip_type)
{
   const uint64_t compute_tdr_duration_ns = 60000000000ull; /* 1 minute (default in kernel) */
   const uint64_t other_tdr_duration_ns = 10000000000ull;   /* 10 seconds (default in kernel) */

   return ip_type == AMD_IP_COMPUTE ? compute_tdr_duration_ns : other_tdr_duration_ns;
}

void radv_write_timestamp(struct radv_cmd_buffer *cmd_buffer, uint64_t va, VkPipelineStageFlags2 stage);

#endif /* RADV_QUERY_H */
