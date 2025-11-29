/*
 * Copyright © 2014 Broadcom
 * SPDX-License-Identifier: MIT
 */

#ifndef NIR_TO_RC_H
#define NIR_TO_RC_H

#include <stdbool.h>
#include "pipe/p_defines.h"

struct nir_shader;
struct pipe_screen;

const void *nir_to_rc(struct nir_shader *s,
                      struct pipe_screen *screen);

#endif /* NIR_TO_RC_H */
