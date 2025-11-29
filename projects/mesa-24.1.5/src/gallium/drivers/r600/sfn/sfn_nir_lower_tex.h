/* -*- mesa-c++  -*-
 * Copyright 2022 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef SFN_NIR_LOWER_TEX_H
#define SFN_NIR_LOWER_TEX_H

struct nir_shader;

bool
r600_nir_lower_int_tg4(nir_shader *nir);
bool
r600_nir_lower_txl_txf_array_or_cube(nir_shader *shader);
bool
r600_nir_lower_cube_to_2darray(nir_shader *shader);

#endif // LALA_H
