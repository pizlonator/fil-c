/* -*- mesa-c++  -*-
 * Copyright 2019 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef SFN_NIR_LOWER_ALU_H
#define SFN_NIR_LOWER_ALU_H

#include "amd_family.h"
#include "nir.h"

bool
r600_nir_lower_pack_unpack_2x16(nir_shader *shader);

bool
r600_nir_lower_trigen(nir_shader *shader, enum amd_gfx_level gfx_level);

bool
r600_nir_fix_kcache_indirect_access(nir_shader *shader);

#endif // SFN_NIR_LOWER_ALU_H
