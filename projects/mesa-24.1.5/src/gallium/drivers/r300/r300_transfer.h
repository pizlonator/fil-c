/*
 * Copyright 2008 Corbin Simpson <MostAwesomeDude@gmail.com>
 * Copyright 2010 Marek Olšák <maraeo@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef R300_TRANSFER
#define R300_TRANSFER

#include "pipe/p_context.h"

struct r300_context;

void *
r300_texture_transfer_map(struct pipe_context *ctx,
                          struct pipe_resource *texture,
                          unsigned level,
                          unsigned usage,
                          const struct pipe_box *box,
                          struct pipe_transfer **transfer);

void
r300_texture_transfer_unmap(struct pipe_context *ctx,
                            struct pipe_transfer *transfer);


#endif
