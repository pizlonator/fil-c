/*
 * Copyright 2023 Asahi Lina
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBAGX_HELPER_H
#define LIBAGX_HELPER_H

#include "agx_pack.h"
#include "libagx.h"

// Enable this to debug core mappings.
// #define SCRATCH_DEBUG_CORES 512

#define AGX_SPILL_SIZE_BUCKETS 16

#define AGX_MAX_CORES_PER_CLUSTER 16
#define AGX_MAX_CLUSTERS          8

#ifdef SCRATCH_DEBUG_CORES
#define AGX_MAX_CORE_ID SCRATCH_DEBUG_CORES
#else
#define AGX_MAX_CORE_ID (AGX_MAX_CLUSTERS * AGX_MAX_CORES_PER_CLUSTER)
#endif

struct agx_helper_block {
   uint32_t blocks[4];
} PACKED;
AGX_STATIC_ASSERT(sizeof(struct agx_helper_block) == 16);

struct agx_helper_core {
   GLOBAL(struct agx_helper_block) blocklist;
   uint32_t alloc_cur;
   uint32_t alloc_max;
   uint32_t alloc_failed;
   uint32_t _pad;
   uint32_t alloc_count[AGX_SPILL_SIZE_BUCKETS];
} PACKED;
AGX_STATIC_ASSERT(sizeof(struct agx_helper_core) ==
                  (8 + 3 * 4 + AGX_SPILL_SIZE_BUCKETS * 4 + 4));

struct agx_helper_header {
   uint32_t subgroups;
   uint32_t _pad;
   struct agx_helper_core cores[AGX_MAX_CORE_ID];
} PACKED;
AGX_STATIC_ASSERT(sizeof(struct agx_helper_header) ==
                  (4 + 4 + AGX_MAX_CORE_ID * sizeof(struct agx_helper_core)));

#endif
