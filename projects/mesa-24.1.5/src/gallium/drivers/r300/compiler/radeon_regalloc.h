/*
 * Copyright 2009 Nicolai Haehnle.
 * Copyright 2011 Tom Stellard <tstellar@gmail.com>
 * Copyright 2012 Advanced Micro Devices, Inc.
 * Author: Tom Stellard <thomas.stellard@amd.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef RADEON_REGALLOC_H
#define RADEON_REGALLOC_H

#include "util/register_allocate.h"
#include "util/u_memory.h"
#include "util/ralloc.h"

#include "radeon_variable.h"

struct ra_regs;

enum rc_reg_class {
	RC_REG_CLASS_FP_SINGLE,
	RC_REG_CLASS_FP_DOUBLE,
	RC_REG_CLASS_FP_TRIPLE,
	RC_REG_CLASS_FP_ALPHA,
	RC_REG_CLASS_FP_SINGLE_PLUS_ALPHA,
	RC_REG_CLASS_FP_DOUBLE_PLUS_ALPHA,
	RC_REG_CLASS_FP_TRIPLE_PLUS_ALPHA,
	RC_REG_CLASS_FP_X,
	RC_REG_CLASS_FP_Y,
	RC_REG_CLASS_FP_Z,
	RC_REG_CLASS_FP_XY,
	RC_REG_CLASS_FP_YZ,
	RC_REG_CLASS_FP_XZ,
	RC_REG_CLASS_FP_XW,
	RC_REG_CLASS_FP_YW,
	RC_REG_CLASS_FP_ZW,
	RC_REG_CLASS_FP_XYW,
	RC_REG_CLASS_FP_YZW,
	RC_REG_CLASS_FP_XZW,
	RC_REG_CLASS_FP_COUNT
};

enum rc_reg_class_vp {
	RC_REG_CLASS_VP_SINGLE,
	RC_REG_CLASS_VP_DOUBLE,
	RC_REG_CLASS_VP_TRIPLE,
	RC_REG_CLASS_VP_QUADRUPLE,
	RC_REG_CLASS_VP_COUNT
};

struct rc_regalloc_state {
	struct ra_regs *regs;
	struct ra_class *classes[RC_REG_CLASS_FP_COUNT];
	const struct rc_class *class_list;
};

struct register_info {
	struct live_intervals Live[4];

	unsigned int Used:1;
	unsigned int Allocated:1;
	unsigned int File:3;
	unsigned int Index:RC_REGISTER_INDEX_BITS;
	unsigned int Writemask;
};

struct regalloc_state {
	struct radeon_compiler * C;

	struct register_info * Input;
	unsigned int NumInputs;

	struct register_info * Temporary;
	unsigned int NumTemporaries;

	unsigned int Simple;
	int LoopEnd;
};

struct rc_class {
	enum rc_reg_class ID;

	unsigned int WritemaskCount;

	/** List of writemasks that belong to this class */
	unsigned int Writemasks[6];
};

int rc_find_class(
	const struct rc_class * classes,
	unsigned int writemask,
	unsigned int max_writemask_count);

unsigned int rc_overlap_live_intervals_array(
	struct live_intervals * a,
	struct live_intervals * b);

static inline unsigned int reg_get_index(int reg)
{
	return reg / RC_MASK_XYZW;
};

static inline unsigned int reg_get_writemask(int reg)
{
	return (reg % RC_MASK_XYZW) + 1;
};

static inline int get_reg_id(unsigned int index, unsigned int writemask)
{
       assert(writemask);
       if (writemask == 0) {
               return 0;
       }
       return (index * RC_MASK_XYZW) + (writemask - 1);
}

void rc_build_interference_graph(
	struct ra_graph * graph,
	struct rc_list * variables);

void rc_init_regalloc_state(struct rc_regalloc_state *s, enum rc_program_type prog);
void rc_destroy_regalloc_state(struct rc_regalloc_state *s);

#endif /* RADEON_REGALLOC_H */
