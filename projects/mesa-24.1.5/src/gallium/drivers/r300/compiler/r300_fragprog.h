/*
 * Copyright 2005 Ben Skeggs.
 * Authors:
 *   Ben Skeggs <darktama@iinet.net.au>
 *   Jerome Glisse <j.glisse@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef __R300_FRAGPROG_H_
#define __R300_FRAGPROG_H_

#include "radeon_compiler.h"
#include "radeon_program.h"


extern void r300BuildFragmentProgramHwCode(struct radeon_compiler *c, void *user);

extern void r300FragmentProgramDump(struct radeon_compiler *c, void *user);

#endif
