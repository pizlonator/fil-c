/* -*- mesa-c++  -*-
 * Copyright 2022 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "sfn_shader.h"

namespace r600 {

bool
dead_code_elimination(Shader& shader);
bool
copy_propagation_fwd(Shader& shader);
bool
copy_propagation_backward(Shader& shader);
bool
simplify_source_vectors(Shader& sh);

bool
optimize(Shader& shader);

} // namespace r600

#endif // OPTIMIZER_H
