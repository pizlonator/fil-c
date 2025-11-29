/*
 * Copyright 2005 Ben Skeggs.
 * Authors:
 *   Ben Skeggs <darktama@iinet.net.au>
 *   Jerome Glisse <j.glisse@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef __R500_FRAGPROG_H_
#define __R500_FRAGPROG_H_

#include "radeon_compiler.h"
#include "radeon_swizzle.h"

extern void r500BuildFragmentProgramHwCode(struct radeon_compiler *c, void *user);

extern void r500FragmentProgramDump(struct radeon_compiler *c, void *user);

extern const struct rc_swizzle_caps r500_swizzle_caps;

extern void r500_transform_IF(
	struct radeon_compiler * c,
	void* data);

#endif
