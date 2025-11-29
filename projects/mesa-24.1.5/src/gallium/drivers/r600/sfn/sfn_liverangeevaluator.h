/* -*- mesa-c++  -*-
 * Copyright 2022 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LIFERANGEEVALUATOR_H
#define LIFERANGEEVALUATOR_H

#include "sfn_valuefactory.h"

#include <cassert>
#include <map>

namespace r600 {

class Shader;

class LiveRangeEvaluator {
public:
   LiveRangeEvaluator();

   LiveRangeMap run(Shader& sh);
};

} // namespace r600

#endif // LIFERANGEEVALUATOR_H
