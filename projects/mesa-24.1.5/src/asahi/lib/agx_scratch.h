/*
 * Copyright 2023 Asahi Lina
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "agx_device.h"
#include <agx_pack.h>

// #define SCRATCH_DEBUG

struct agx_scratch {
   struct agx_device *dev;
   struct agx_bo *buf;
   uint32_t max_core_id;
   uint32_t num_cores;

   uint32_t subgroups;
   uint32_t size_dwords;

   struct agx_helper_header *header;

#ifdef SCRATCH_DEBUG
   bool core_present[1024];
   struct agx_helper_block *blocklist;
   void *data;
   size_t core_size;
#endif
};

struct agx_bo *agx_build_helper(struct agx_device *dev);

void agx_scratch_init(struct agx_device *dev, struct agx_scratch *scratch);
void agx_scratch_fini(struct agx_scratch *scratch);
void agx_scratch_debug_pre(struct agx_scratch *scratch);
void agx_scratch_debug_post(struct agx_scratch *scratch);

uint32_t agx_scratch_get_bucket(uint32_t dwords);
void agx_scratch_alloc(struct agx_scratch *scratch, uint32_t dwords,
                       size_t subgroups);
