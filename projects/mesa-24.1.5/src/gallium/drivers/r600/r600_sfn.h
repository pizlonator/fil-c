/* -*- mesa-c++  -*-
 * Copyright 2019 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef R600_SFN_H
#define R600_SFN_H

#include "r600_pipe.h"
#include "r600_shader.h"
#include "pipe/p_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

char *
r600_finalize_nir(struct pipe_screen *screen, void *shader);

int
r600_shader_from_nir(struct r600_context *rctx,
                     struct r600_pipe_shader *pipeshader,
                     union r600_shader_key *key);

#ifdef __cplusplus
}
#endif

#endif // R600_SFN_H
