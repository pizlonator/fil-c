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

#ifndef H_ETNAVIV_ASM
#define H_ETNAVIV_ASM

#include <stdint.h>
#include <stdbool.h>
#include "util/u_math.h"

#include <etnaviv/isa/asm.h>

/* Size of an instruction in 32-bit words */
#define ETNA_INST_SIZE (4)

/* Compose two swizzles (computes swz1.swz2) */
static inline uint32_t inst_swiz_compose(uint32_t swz1, uint32_t swz2)
{
   return SWIZ_X((swz1 >> (((swz2 >> 0)&3)*2))&3) |
          SWIZ_Y((swz1 >> (((swz2 >> 2)&3)*2))&3) |
          SWIZ_Z((swz1 >> (((swz2 >> 4)&3)*2))&3) |
          SWIZ_W((swz1 >> (((swz2 >> 6)&3)*2))&3);
};

/* Compose two write_masks (computes wm1.wm2) */
static inline uint32_t inst_write_mask_compose(uint32_t wm1, uint32_t wm2)
{
   unsigned wm = 0;
   for (unsigned i = 0, j = 0; i < 4; i++) {
      if (wm2 & (1 << i)) {
         if (wm1 & (1 << j))
            wm |= (1 << i);
         j++;
      }
   }
   return wm;
};

/* Return whether the rgroup is one of the uniforms */
static inline int
etna_rgroup_is_uniform(enum isa_reg_group rgroup)
{
   return rgroup == ISA_REG_GROUP_UNIFORM_0 ||
          rgroup == ISA_REG_GROUP_UNIFORM_1;
}

static inline struct etna_inst_src
etna_immediate_src(unsigned type, uint32_t bits)
{
   return (struct etna_inst_src) {
      .use = 1,
      .rgroup = ISA_REG_GROUP_IMMED,
      .imm_val = bits,
      .imm_type = type
   };
}

static inline struct etna_inst_src
etna_immediate_float(float x)
{
	uint32_t bits = fui(x);
	assert((bits & 0xfff) == 0); /* 12 lsb cut off */
	return etna_immediate_src(0, bits >> 12);
}

static inline struct etna_inst_src
etna_immediate_int(int x)
{
    assert(x >= -0x80000 && x < 0x80000); /* 20-bit signed int */
	return etna_immediate_src(1, x);
}

/**
 * Build vivante instruction from structure with
 *  opcode, cond, sat, dst_use, dst_amode,
 *  dst_reg, dst_comps, tex_id, tex_amode, tex_swiz,
 *  src[0-2]_reg, use, swiz, neg, abs, amode, rgroup,
 *  imm
 *
 * Return 0 if successful, and a non-zero
 * value otherwise.
 */
int
etna_assemble(uint32_t *out, const struct etna_inst *inst, bool has_no_oneconst_limit);

#endif
