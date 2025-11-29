/*
 * Copyright 2009 Corbin Simpson <MostAwesomeDude@gmail.com>
 * Copyright 2009 Marek Olšák <maraeo@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef R300_VS_H
#define R300_VS_H

#include "pipe/p_state.h"
#include "tgsi/tgsi_scan.h"
#include "compiler/radeon_code.h"

#include "r300_context.h"
#include "r300_shader_semantics.h"

struct r300_context;

struct r300_vertex_shader_code {
    /* Parent class */

    struct tgsi_shader_info info;
    struct r300_shader_semantics outputs;

    /* Whether the shader was replaced by a dummy one due to a shader
     * compilation failure. */
    bool dummy;

    bool wpos;

    /* Numbers of constants for each type. */
    unsigned externals_count;
    unsigned immediates_count;

    /* HWTCL-specific.  */
    /* Machine code (if translated) */
    struct r300_vertex_program_code code;

    struct r300_vertex_shader_code *next;
};

struct r300_vertex_shader {
    /* Parent class */
    struct pipe_shader_state state;

    /* Currently-bound vertex shader. */
    struct r300_vertex_shader_code *shader;

    /* List of the same shaders compiled with different states. */
    struct r300_vertex_shader_code *first;

    /* SWTCL-specific. */
    void *draw_vs;
};

struct nir_shader;

void r300_init_vs_outputs(struct r300_context *r300,
                          struct r300_vertex_shader *vs);

void r300_translate_vertex_shader(struct r300_context *r300,
                                  struct r300_vertex_shader *vs);

void r300_draw_init_vertex_shader(struct r300_context *r300,
                                  struct r300_vertex_shader *vs);

#endif /* R300_VS_H */
