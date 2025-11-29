/*
 * Copyright 2010 Marek Olšák <maraeo@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef RADEON_REMOVE_CONSTANTS_H
#define RADEON_REMOVE_CONSTANTS_H

#include "radeon_compiler.h"

void rc_remove_unused_constants(struct radeon_compiler *c, void *user);

#endif
