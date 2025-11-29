/*
 * Copyright 2023 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#include "util/macros.h"
#include "agx_builder.h"
#include "agx_compile.h"
#include "agx_compiler.h"

/* Lower moves involving memory registers (created when spilling) to concrete
 * spills and fills.
 */

static void
spill_fill(agx_builder *b, agx_instr *I, enum agx_size size, unsigned channels,
           unsigned component_offset)
{
   enum agx_format format =
      size == AGX_SIZE_16 ? AGX_FORMAT_I16 : AGX_FORMAT_I32;

   unsigned offset_B = component_offset * agx_size_align_16(size) * 2;
   unsigned effective_chans = size == AGX_SIZE_64 ? (channels * 2) : channels;
   unsigned mask = BITFIELD_MASK(effective_chans);

   assert(effective_chans <= 4);

   /* Pick off the memory and register parts of the move */
   agx_index mem = I->dest[0].memory ? I->dest[0] : I->src[0];
   agx_index reg = I->dest[0].memory ? I->src[0] : I->dest[0];

   assert(mem.type == AGX_INDEX_REGISTER && mem.memory);
   assert(reg.type == AGX_INDEX_REGISTER && !reg.memory);

   /* Slice the register according to the part of the spill we're handling */
   if (component_offset > 0 || channels != agx_channels(reg)) {
      reg.value += component_offset * agx_size_align_16(reg.size);
      reg.channels_m1 = channels - 1;
   }

   /* Calculate stack offset in bytes. IR registers are 2-bytes each. */
   unsigned stack_offs_B = b->shader->spill_base + (mem.value * 2) + offset_B;

   /* Emit the spill/fill */
   if (I->dest[0].memory) {
      agx_stack_store(b, reg, agx_immediate(stack_offs_B), format, mask);
   } else {
      agx_stack_load_to(b, reg, agx_immediate(stack_offs_B), format, mask);
   }
}

void
agx_lower_spill(agx_context *ctx)
{
   agx_foreach_instr_global_safe(ctx, I) {
      if (I->op != AGX_OPCODE_MOV || (!I->dest[0].memory && !I->src[0].memory))
         continue;

      enum agx_size size = I->dest[0].size;
      unsigned channels = agx_channels(I->dest[0]);

      assert(size == I->src[0].size);
      assert(channels == agx_channels(I->src[0]));

      /* Texture gradient sources can be vec6, and if such a vector is spilled,
       * we need to be able to spill/fill a vec6. Since stack_store/stack_load
       * only work up to vec4, we break up into (at most) vec4 components.
       */
      agx_builder b = agx_init_builder(ctx, agx_before_instr(I));

      for (unsigned c = 0; c < channels; c += 4) {
         spill_fill(&b, I, size, MIN2(channels - c, 4), c);
      }

      agx_remove_instruction(I);
   }
}
