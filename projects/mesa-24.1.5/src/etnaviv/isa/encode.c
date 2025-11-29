/*
 * Copyright © 2024 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#include "asm.h"
#include "isa.h"

struct encode_state {
};

static inline enum isa_opc
__instruction_case(struct encode_state *s, const struct etna_inst *instr)
{
   return instr->opcode;
}

#include "encode.h"

void isa_assemble_instruction(uint32_t *out, const struct etna_inst *instr)
{
   bitmask_t encoded = encode__instruction(NULL, NULL, instr);

   store_instruction(out, encoded);
}
