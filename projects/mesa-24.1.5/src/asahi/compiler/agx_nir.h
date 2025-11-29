/*
 * Copyright 2023 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>

struct nir_shader;

bool agx_nir_lower_interpolation(struct nir_shader *s);
bool agx_nir_lower_algebraic_late(struct nir_shader *shader);
bool agx_nir_fuse_selects(struct nir_shader *shader);
bool agx_nir_fuse_algebraic_late(struct nir_shader *shader);
bool agx_nir_fence_images(struct nir_shader *shader);
bool agx_nir_lower_layer(struct nir_shader *s);
bool agx_nir_lower_clip_distance(struct nir_shader *s);
bool agx_nir_lower_cull_distance_vs(struct nir_shader *s);
bool agx_nir_lower_subgroups(struct nir_shader *s);
