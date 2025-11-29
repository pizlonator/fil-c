/*
 * Copyright 2010 Red Hat Inc.
 * Authors: Dave Airlie
 * SPDX-License-Identifier: MIT
 */

#ifndef R300_SCREEN_BUFFER_H
#define R300_SCREEN_BUFFER_H

#include <stdio.h>
#include "util/compiler.h"
#include "pipe/p_state.h"
#include "util/u_transfer.h"

#include "r300_screen.h"
#include "r300_context.h"

/* Functions. */

void r300_upload_index_buffer(struct r300_context *r300,
			      struct pipe_resource **index_buffer,
			      unsigned index_size, unsigned *start,
			      unsigned count, const uint8_t *ptr);

void r300_resource_destroy(struct pipe_screen *screen,
                           struct pipe_resource *buf);

struct pipe_resource *r300_buffer_create(struct pipe_screen *screen,
					 const struct pipe_resource *templ);

/* Inline functions. */

static inline struct r300_buffer *r300_buffer(struct pipe_resource *buffer)
{
    return (struct r300_buffer *)buffer;
}

void *
r300_buffer_transfer_map( struct pipe_context *context,
                          struct pipe_resource *resource,
                          unsigned level,
                          unsigned usage,
                          const struct pipe_box *box,
                          struct pipe_transfer **ptransfer );

void r300_buffer_transfer_unmap( struct pipe_context *pipe,
                                 struct pipe_transfer *transfer );

#endif
