/*
 * Copyright 2008 Corbin Simpson <MostAwesomeDude@gmail.com>
 * Copyright 2010 Marek Olšák <maraeo@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef R300_TEXTURE_DESC_H
#define R300_TEXTURE_DESC_H

#include "util/format/u_formats.h"
#include "r300_context.h"

struct pipe_resource;
struct r300_screen;
struct r300_texture_desc;
struct r300_resource;

enum r300_dim {
    DIM_WIDTH  = 0,
    DIM_HEIGHT = 1
};

unsigned r300_get_pixel_alignment(enum pipe_format format,
                                  unsigned num_samples,
                                  enum radeon_bo_layout microtile,
                                  enum radeon_bo_layout macrotile,
                                  enum r300_dim dim, bool is_rs690,
                                  bool scanout);

void r300_texture_desc_init(struct r300_screen *rscreen,
                            struct r300_resource *tex,
                            const struct pipe_resource *base);

unsigned r300_texture_get_offset(struct r300_resource *tex,
                                 unsigned level, unsigned layer);

unsigned r300_stride_to_width(enum pipe_format format,
                              unsigned stride_in_bytes);

#endif
