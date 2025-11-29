/* -*- mesa-c++  -*-
 * Copyright 2019 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef SFN_NIR_LOWER_FS_OUT_TO_VECTOR_H
#define SFN_NIR_LOWER_FS_OUT_TO_VECTOR_H

#include "nir.h"

namespace r600 {

bool
r600_lower_fs_out_to_vector(nir_shader *sh);

}

#endif // SFN_NIR_LOWER_FS_OUT_TO_VECTOR_H
