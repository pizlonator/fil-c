/*
 * Copyright 2023 Rose Hudson
 * Copyright 2018 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "util/disk_cache.h"
#include "agx_state.h"

void agx_disk_cache_store(struct disk_cache *cache,
                          const struct agx_uncompiled_shader *uncompiled,
                          const union asahi_shader_key *key,
                          const struct agx_compiled_shader *binary);

struct agx_compiled_shader *
agx_disk_cache_retrieve(struct agx_screen *screen,
                        const struct agx_uncompiled_shader *uncompiled,
                        const union asahi_shader_key *key);

void agx_disk_cache_init(struct agx_screen *screen);
