/*
 * Copyright (c) 2012-2015 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Wladimir J. van der Laan <laanwj@gmail.com>
 */

#include "etnaviv_asm.h"
#include "etnaviv_debug.h"
#include "etnaviv_util.h"

#include "etnaviv/isa/isa.h"

/* An instruction can only read from one distinct uniform.
 * This function verifies this property and returns true if the instruction
 * is deemed correct and false otherwise.
 */
static bool
check_uniforms(const struct etna_inst *inst)
{
   unsigned uni_rgroup = -1;
   unsigned uni_reg = -1;
   bool conflict = false;

   for (unsigned i = 0; i < ETNA_NUM_SRC; i++) {
      const struct etna_inst_src *src = &inst->src[i];

      if (!etna_rgroup_is_uniform(src->rgroup))
         continue;

      if (uni_reg == -1) { /* first uniform used */
         uni_rgroup = src->rgroup;
         uni_reg = src->reg;
      } else { /* second or later; check that it is a re-use */
         if (uni_rgroup != src->rgroup || uni_reg != src->reg) {
            conflict = true;
         }
      }
   }

   return !conflict;
}

int
etna_assemble(uint32_t *out, const struct etna_inst *inst, bool has_no_oneconst_limit)
{
   /* cannot have both src2 and imm */
   if (inst->imm && inst->src[2].use)
      return 1;

   if (!has_no_oneconst_limit && !check_uniforms(inst))
      BUG("error: generating instruction that accesses two different uniforms");

   assert(!(inst->opcode&~0x7f));

   isa_assemble_instruction(out, inst);

   return 0;
}
