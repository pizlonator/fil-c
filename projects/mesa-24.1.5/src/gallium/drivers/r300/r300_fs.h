/*
 * Copyright 2008 Corbin Simpson <MostAwesomeDude@gmail.com>
 *                Joakim Sindholt <opensource@zhasha.com>
 * Copyright 2009 Marek Olšák <maraeo@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef R300_FS_H
#define R300_FS_H

#include "pipe/p_state.h"
#include "tgsi/tgsi_scan.h"
#include "compiler/radeon_code.h"
#include "r300_shader_semantics.h"

struct r300_fragment_shader_code {
    struct rX00_fragment_program_code code;
    struct tgsi_shader_info info;
    struct r300_shader_semantics inputs;

    /* Whether the shader was replaced by a dummy one due to a shader
     * compilation failure. */
    bool dummy;

    /* Numbers of constants for each type. */
    unsigned externals_count;
    unsigned immediates_count;
    unsigned rc_state_count;

    /* Registers for fragment depth output setup. */
    uint32_t fg_depth_src;      /* R300_FG_DEPTH_SRC: 0x4bd8 */
    uint32_t us_out_w;          /* R300_US_W_FMT:     0x46b4 */

    struct r300_fragment_program_external_state compare_state;

    unsigned cb_code_size;
    uint32_t *cb_code;

    struct r300_fragment_shader_code* next;

    bool write_all;

};

struct r300_fragment_shader {
    /* Parent class */
    struct pipe_shader_state state;

    /* Currently-bound fragment shader. */
    struct r300_fragment_shader_code* shader;

    /* List of the same shaders compiled with different texture-compare
     * states. */
    struct r300_fragment_shader_code* first;
};

void r300_shader_read_fs_inputs(struct tgsi_shader_info* info,
                                struct r300_shader_semantics* fs_inputs);

/* Return TRUE if the shader was switched and should be re-emitted. */
bool r300_pick_fragment_shader(struct r300_context *r300,
                               struct r300_fragment_shader* fs,
                               struct r300_fragment_program_external_state *state);
void r300_fragment_program_get_external_state(struct r300_context *r300,
                                              struct r300_fragment_program_external_state *state);

static inline bool r300_fragment_shader_writes_depth(struct r300_fragment_shader *fs)
{
    if (!fs)
        return false;
    return (fs->shader->code.writes_depth) ? true : false;
}

static inline bool r300_fragment_shader_writes_all(struct r300_fragment_shader *fs)
{
    if (!fs)
        return false;
    return (fs->shader->write_all) ? true : false;
}
#endif /* R300_FS_H */
