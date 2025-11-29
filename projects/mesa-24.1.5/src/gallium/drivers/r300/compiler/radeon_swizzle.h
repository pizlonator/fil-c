/*
 * Copyright 2009 Nicolai Haehnle.
 * SPDX-License-Identifier: MIT
 */

#ifndef RADEON_SWIZZLE_H
#define RADEON_SWIZZLE_H

#include "radeon_program.h"

struct rc_swizzle_split {
	unsigned char NumPhases;
	unsigned char Phase[4];
};

/**
 * Describe the swizzling capability of target hardware.
 */
struct rc_swizzle_caps {
	/**
	 * Check whether the given swizzle, absolute and negate combination
	 * can be implemented natively by the hardware for this opcode.
	 *
	 * \return 1 if the swizzle is native for the given opcode
	 */
	int (*IsNative)(rc_opcode opcode, struct rc_src_register reg);

	/**
	 * Determine how to split access to the masked channels of the
	 * given source register to obtain ALU-native swizzles.
	 */
	void (*Split)(struct rc_src_register reg, unsigned int mask, struct rc_swizzle_split * split);
};

extern const struct rc_swizzle_caps r300_vertprog_swizzle_caps;

#endif /* RADEON_SWIZZLE_H */
