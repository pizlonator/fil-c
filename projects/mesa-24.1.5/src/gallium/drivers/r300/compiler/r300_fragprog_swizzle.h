/*
 * Copyright 2008 Nicolai Haehnle.
 * SPDX-License-Identifier: MIT
 */

#ifndef __R300_FRAGPROG_SWIZZLE_H_
#define __R300_FRAGPROG_SWIZZLE_H_

#include "radeon_swizzle.h"

extern const struct rc_swizzle_caps r300_swizzle_caps;

unsigned int r300FPTranslateRGBSwizzle(unsigned int src, unsigned int swizzle);
unsigned int r300FPTranslateAlphaSwizzle(unsigned int src, unsigned int swizzle);
int r300_swizzle_is_native_basic(unsigned int swizzle);

#endif /* __R300_FRAGPROG_SWIZZLE_H_ */
