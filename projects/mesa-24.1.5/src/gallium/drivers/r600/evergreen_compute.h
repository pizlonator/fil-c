/*
 * Copyright 2011 Adam Rak <adam.rak@streamnovation.com>
 * Authors:
 *      Adam Rak <adam.rak@streamnovation.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef EVERGREEN_COMPUTE_H
#define EVERGREEN_COMPUTE_H

#include "r600_pipe.h"

struct r600_atom;
struct evergreen_compute_resource;
struct compute_memory_item;

struct r600_resource_global {
	struct r600_resource base;
	struct compute_memory_item *chunk;
};

void evergreen_init_atom_start_compute_cs(struct r600_context *rctx);
void evergreen_init_compute_state_functions(struct r600_context *rctx);
void evergreen_emit_cs_shader(struct r600_context *rctx, struct r600_atom * atom);

struct r600_resource* r600_compute_buffer_alloc_vram(struct r600_screen *screen, unsigned size);
struct pipe_resource *r600_compute_global_buffer_create(struct pipe_screen *screen, const struct pipe_resource *templ);
void r600_compute_global_buffer_destroy(struct pipe_screen *screen,
					struct pipe_resource *res);
void *r600_compute_global_transfer_map(struct pipe_context *ctx,
				      struct pipe_resource *resource,
				      unsigned level,
				      unsigned usage,
				      const struct pipe_box *box,
				      struct pipe_transfer **ptransfer);
void r600_compute_global_transfer_unmap(struct pipe_context *ctx,
					struct pipe_transfer *transfer);

#endif
