/*
 * Copyright © 2022 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef ELK_ISA_ENCODING_H
#define ELK_ISA_ENCODING_H

#include "dev/intel_device_info.h"
#include "elk_eu_opcodes.h"

#ifdef __cplusplus
extern "C" {
#endif

struct elk_opcode_desc;

struct elk_isa_info {
   const struct intel_device_info *devinfo;

   /* A mapping from enum elk_opcode to the corresponding opcode_desc */
   const struct elk_opcode_desc *ir_to_descs[NUM_ELK_OPCODES];

   /** A mapping from a HW opcode encoding to the corresponding opcode_desc */
   const struct elk_opcode_desc *hw_to_descs[128];
};

void elk_init_isa_info(struct elk_isa_info *isa,
                       const struct intel_device_info *devinfo);

struct elk_opcode_desc {
   unsigned ir;
   unsigned hw;
   const char *name;
   int nsrc;
   int ndst;
   int gfx_vers;
};

const struct elk_opcode_desc *
elk_opcode_desc(const struct elk_isa_info *isa, enum elk_opcode opcode);

const struct elk_opcode_desc *
elk_opcode_desc_from_hw(const struct elk_isa_info *isa, unsigned hw);

static inline unsigned
elk_opcode_encode(const struct elk_isa_info *isa, enum elk_opcode opcode)
{
   return elk_opcode_desc(isa, opcode)->hw;
}

static inline enum elk_opcode
elk_opcode_decode(const struct elk_isa_info *isa, unsigned hw)
{
   const struct elk_opcode_desc *desc = elk_opcode_desc_from_hw(isa, hw);
   return desc ? (enum elk_opcode)desc->ir : ELK_OPCODE_ILLEGAL;
}

static inline bool
elk_is_3src(const struct elk_isa_info *isa, enum elk_opcode opcode)
{
   const struct elk_opcode_desc *desc = elk_opcode_desc(isa, opcode);
   return desc && desc->nsrc == 3;
}

#ifdef __cplusplus
}
#endif
#endif
