/*
 * Copyright 2008 Nicolai Haehnle.
 * SPDX-License-Identifier: MIT
 */

#ifndef __RADEON_PROGRAM_ALU_H_
#define __RADEON_PROGRAM_ALU_H_

#include "radeon_program.h"

int radeonTransformALU(
	struct radeon_compiler * c,
	struct rc_instruction * inst,
	void*);

int r300_transform_vertex_alu(
	struct radeon_compiler * c,
	struct rc_instruction * inst,
	void*);

int radeonStubDeriv(
	struct radeon_compiler * c,
	struct rc_instruction * inst,
	void*);

int radeonTransformDeriv(
	struct radeon_compiler * c,
	struct rc_instruction * inst,
	void*);

int rc_force_output_alpha_to_one(struct radeon_compiler *c,
				 struct rc_instruction *inst, void *data);

#endif /* __RADEON_PROGRAM_ALU_H_ */
