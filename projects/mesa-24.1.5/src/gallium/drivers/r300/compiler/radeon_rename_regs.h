/*
 * Copyright 2010 Tom Stellard <tstellar@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef RADEON_RENAME_REGS_H
#define RADEON_RENAME_REGS_H

struct radeon_compiler;

void rc_rename_regs(struct radeon_compiler *c, void *user);

#endif /* RADEON_RENAME_REGS_H */
