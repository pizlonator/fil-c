/*
 * Copyright 2010 Corbin Simpson
 * SPDX-License-Identifier: MIT
 */

#ifndef __RADEON_PROGRAM_TEX_H_
#define __RADEON_PROGRAM_TEX_H_

#include "radeon_compiler.h"
#include "radeon_program.h"

int radeonTransformTEX(
	struct radeon_compiler * c,
	struct rc_instruction * inst,
	void* data);

#endif /* __RADEON_PROGRAM_TEX_H_ */
