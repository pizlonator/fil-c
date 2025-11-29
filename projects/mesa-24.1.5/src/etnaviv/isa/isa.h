/*
 * Copyright © 2023 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#ifndef _ISA_H_
#define _ISA_H_

#include "compiler/isaspec/isaspec.h"

struct etna_inst;

void isa_assemble_instruction(uint32_t *out, const struct etna_inst *instr);

#endif /* _ISA_H_ */
