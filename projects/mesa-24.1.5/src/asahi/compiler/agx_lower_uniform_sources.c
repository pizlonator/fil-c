/*
 * Copyright 2023 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */
#include "agx_builder.h"
#include "agx_compiler.h"

/*
 * Not all instructions can take uniforms. Memory instructions can take
 * uniforms, but only for their base (first) source and only in the
 * low-half of the uniform file.
 *
 * This pass lowers invalid uniforms.
 */
bool
agx_instr_accepts_uniform(enum agx_opcode op, unsigned src_index,
                          unsigned value, enum agx_size size)
{
   /* Some instructions only seem able to access uniforms in the low half */
   bool high = value >= 256;

   /* ALU cannot access 64-bit uniforms */
   bool is_64 = size == AGX_SIZE_64;

   switch (op) {
   case AGX_OPCODE_IMAGE_LOAD:
   case AGX_OPCODE_TEXTURE_LOAD:
   case AGX_OPCODE_TEXTURE_SAMPLE:
      /* Unknown if this works, but the driver will never hit this. */
      assert(!(src_index == 2 && high) && "texture heap always low");
      return !high && (src_index == 1 || src_index == 2);

   case AGX_OPCODE_DEVICE_LOAD:
      return src_index == 0 && !high;
   case AGX_OPCODE_DEVICE_STORE:
   case AGX_OPCODE_ATOMIC:
      return src_index == 1 && !high;
   case AGX_OPCODE_LOCAL_LOAD:
      return src_index == 0;
   case AGX_OPCODE_LOCAL_STORE:
      return src_index == 1;
   case AGX_OPCODE_IMAGE_WRITE:
      return src_index == 3;
   case AGX_OPCODE_ZS_EMIT:
   case AGX_OPCODE_ST_TILE:
   case AGX_OPCODE_LD_TILE:
   case AGX_OPCODE_BLOCK_IMAGE_STORE:
   case AGX_OPCODE_UNIFORM_STORE:
   case AGX_OPCODE_ST_VARY:
   case AGX_OPCODE_LOCAL_ATOMIC:
   case AGX_OPCODE_SAMPLE_MASK:
   case AGX_OPCODE_ITER:
   case AGX_OPCODE_ITERPROJ:
   case AGX_OPCODE_STACK_LOAD:
   case AGX_OPCODE_STACK_STORE:
      return false;
   default:
      return !is_64;
   }
}

void
agx_lower_uniform_sources(agx_context *ctx)
{
   agx_foreach_instr_global_safe(ctx, I) {
      agx_builder b = agx_init_builder(ctx, agx_before_instr(I));

      agx_foreach_src(I, s) {
         if (I->src[s].type == AGX_INDEX_UNIFORM &&
             !agx_instr_accepts_uniform(I->op, s, I->src[s].value,
                                        I->src[s].size)) {

            agx_index idx = I->src[s];
            idx.abs = idx.neg = false;
            agx_replace_src(I, s, agx_mov(&b, idx));
         }
      }
   }
}
