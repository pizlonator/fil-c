/*
 * Copyright 2008 Nicolai Haehnle.
 * SPDX-License-Identifier: MIT
 */

#ifndef __RADEON_PROGRAM_PAIR_H_
#define __RADEON_PROGRAM_PAIR_H_

#include "radeon_code.h"
#include "radeon_opcodes.h"
#include "radeon_program_constants.h"

struct radeon_compiler;


/**
 * \file
 * Represents a paired ALU instruction, as found in R300 and R500
 * fragment programs.
 *
 * Note that this representation is taking some liberties as far
 * as register files are concerned, to allow separate register
 * allocation.
 *
 * Also note that there are some subtleties in that the semantics
 * of certain opcodes are implicitly changed in this representation;
 * see \ref rc_pair_translate
 */

/* For rgb and alpha instructions when arg[n].Source = RC_PAIR_PRESUB_SRC, then
 * the presubtract value will be used, and
 * {RGB,Alpha}.Src[RC_PAIR_PRESUB_SRC].File will be set to RC_FILE_PRESUB.
 */
#define RC_PAIR_PRESUB_SRC 3

struct rc_pair_instruction_source {
	unsigned int Used:1;
	unsigned int File:4;
	unsigned int Index:RC_REGISTER_INDEX_BITS;
};

struct rc_pair_instruction_arg {
	unsigned int Source:2;
	unsigned int Swizzle:12;
	unsigned int Abs:1;
	unsigned int Negate:1;
};

struct rc_pair_sub_instruction {
	unsigned int Opcode:8;
	unsigned int DestIndex:RC_REGISTER_INDEX_BITS;
	unsigned int WriteMask:4;
	unsigned int Target:2;
	unsigned int OutputWriteMask:3;
	unsigned int DepthWriteMask:1;
	unsigned int Saturate:1;
	unsigned int Omod:3;

	struct rc_pair_instruction_source Src[4];
	struct rc_pair_instruction_arg Arg[3];
};

struct rc_pair_instruction {
	struct rc_pair_sub_instruction RGB;
	struct rc_pair_sub_instruction Alpha;

	unsigned int WriteALUResult:2;
	unsigned int ALUResultCompare:3;
	unsigned int Nop:1;
	unsigned int SemWait:1;
};

typedef void (*rc_pair_foreach_src_fn)
			(void *, struct rc_pair_instruction_source *);

/**
 * General helper functions for dealing with the paired instruction format.
 */
/*@{*/
int rc_pair_alloc_source(struct rc_pair_instruction *pair,
	unsigned int rgb, unsigned int alpha,
	rc_register_file file, unsigned int index);

void rc_pair_foreach_source_that_alpha_reads(
	struct rc_pair_instruction * pair,
	void * data,
	rc_pair_foreach_src_fn cb);

void rc_pair_foreach_source_that_rgb_reads(
	struct rc_pair_instruction * pair,
	void * data,
	rc_pair_foreach_src_fn cb);

struct rc_pair_instruction_source * rc_pair_get_src(
	struct rc_pair_instruction * pair_inst,
	struct rc_pair_instruction_arg * arg);

int rc_pair_get_src_index(
	struct rc_pair_instruction * pair_inst,
	struct rc_pair_instruction_source * src);
/*@}*/


/**
 * Compiler passes that operate with the paired format.
 */
/*@{*/
struct radeon_pair_handler;

void rc_pair_translate(struct radeon_compiler *cc, void *user);
void rc_pair_schedule(struct radeon_compiler *cc, void *user);
void rc_pair_regalloc(struct radeon_compiler *cc, void *user);
void rc_pair_remove_dead_sources(struct radeon_compiler *c, void *user);
/*@}*/

#endif /* __RADEON_PROGRAM_PAIR_H_ */
