/* -*- mesa-c++  -*-
 * Copyright 2022 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef R600_SFN_SPLIT_ADDRESS_LOADS_H
#define R600_SFN_SPLIT_ADDRESS_LOADS_H

#include "sfn_shader.h"

namespace r600 {

bool split_address_loads(Shader& sh);

}

#endif // R600_SFN_SPLIT_ADDRESS_LOADS_H
