/*
 * Copyright 2011 Tom Stellard <tstellar@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef RADEON_VARIABLE_H
#define RADEON_VARIABLE_H

#include "radeon_compiler.h"

struct radeon_compiler;
struct rc_list;
struct rc_reader_data;
struct rc_readers;

struct live_intervals {
	int Start;
	int End;
	int Used;
};

struct rc_variable {
	struct radeon_compiler * C;
	struct rc_dst_register Dst;

	struct rc_instruction * Inst;
	unsigned int ReaderCount;
	struct rc_reader * Readers;
	struct live_intervals Live[4];

	/* A friend is a variable that shares a reader with another variable.
	 */
	struct rc_variable * Friend;
};

void rc_variable_change_dst(
	struct rc_variable * var,
	unsigned int new_index,
	unsigned int new_writemask);

void rc_variable_compute_live_intervals(struct rc_variable * var);

void rc_variable_add_friend(
	struct rc_variable * var,
	struct rc_variable * friend);

struct rc_variable * rc_variable(
	struct radeon_compiler * c,
	unsigned int DstFile,
	unsigned int DstIndex,
	unsigned int DstWriteMask,
	struct rc_reader_data * reader_data);

struct rc_list * rc_get_variables(struct radeon_compiler * c);

unsigned int rc_variable_writemask_sum(struct rc_variable * var);

struct rc_list * rc_variable_readers_union(struct rc_variable * var);

struct rc_list * rc_variable_list_get_writers(
	struct rc_list * var_list,
	unsigned int src_type,
	void * src);

struct rc_list * rc_variable_list_get_writers_one_reader(
	struct rc_list * var_list,
	unsigned int src_type,
	void * src);

void rc_variable_print(struct rc_variable * var);

#endif /* RADEON_VARIABLE_H */
