/* -*- mesa-c++  -*-
 * Copyright 2022 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "sfn_shader.h"

namespace r600 {

Shader *
schedule(Shader *original);

}

#endif // SCHEDULER_H
