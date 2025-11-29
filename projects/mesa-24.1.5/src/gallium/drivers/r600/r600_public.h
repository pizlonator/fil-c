/*
 * Copyright 2010 Jerome Glisse <glisse@freedesktop.org>
 * SPDX-License-Identifier: MIT
 */

#ifndef R600_PUBLIC_H
#define R600_PUBLIC_H

struct radeon_winsys;
struct pipe_screen_config;

struct pipe_screen *r600_screen_create(struct radeon_winsys *ws,
				       const struct pipe_screen_config *config);
#endif
