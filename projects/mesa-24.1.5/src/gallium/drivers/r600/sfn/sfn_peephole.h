/* -*- mesa-c++  -*-
 * Copyright 2022 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef PEEPHOLE_H
#define PEEPHOLE_H

#include "sfn_shader.h"

namespace r600 {

bool
peephole(Shader& sh);

}

#endif // PEEPHOLE_H
