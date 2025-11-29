/*
 * Copyright 2024 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "agx_pack.h"
#include "shader_enums.h"

struct nir_shader;
struct nir_instr;

/* Texture backend flags */
#define AGX_TEXTURE_FLAG_NO_CLAMP (1 << 0)

bool agx_nir_lower_texture_early(struct nir_shader *s, bool support_lod_bias);
bool agx_nir_lower_texture(struct nir_shader *s);
bool agx_nir_lower_multisampled_image_store(struct nir_shader *s);
bool agx_nir_needs_texture_crawl(struct nir_instr *instr);
