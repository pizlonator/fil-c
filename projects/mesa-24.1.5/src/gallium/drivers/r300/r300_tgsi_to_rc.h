/*
 * Copyright 2009 Nicolai Hähnle <nhaehnle@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef R300_TGSI_TO_RC_H
#define R300_TGSI_TO_RC_H

#include "util/compiler.h"

struct radeon_compiler;

struct tgsi_full_declaration;
struct tgsi_shader_info;
struct tgsi_token;

struct swizzled_imms {
    unsigned index;
    unsigned swizzle;
};

struct tgsi_to_rc {
    struct radeon_compiler * compiler;
    const struct tgsi_shader_info * info;

    int immediate_offset;

    /* If an error occurred. */
    bool error;
};

void r300_tgsi_to_rc(struct tgsi_to_rc * ttr, const struct tgsi_token * tokens);

#endif /* R300_TGSI_TO_RC_H */
