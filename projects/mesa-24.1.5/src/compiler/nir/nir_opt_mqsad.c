/*
 * Copyright 2023 Valve Corporation
 * SPDX-License-Identifier: MIT
 */
#include "nir.h"
#include "nir_builder.h"
#include "nir_worklist.h"

/*
 * This pass recognizes certain patterns of nir_op_shfr and nir_op_msad_4x8 and replaces it
 * with a single nir_op_mqsad_4x8 instruction.
 */

struct mqsad {
   nir_scalar ref;
   nir_scalar src[2];

   nir_scalar accum[4];
   nir_alu_instr *msad[4];
   unsigned first_msad_index;
   uint8_t mask;
};

static bool
is_mqsad_compatible(struct mqsad *mqsad, nir_scalar ref, nir_scalar src0, nir_scalar src1,
                    unsigned idx, nir_alu_instr *msad)
{
   if (!nir_scalar_equal(ref, mqsad->ref) || !nir_scalar_equal(src0, mqsad->src[0]))
      return false;
   if ((mqsad->mask & 0b1110) && idx && !nir_scalar_equal(src1, mqsad->src[1]))
      return false;

   /* Ensure that this MSAD doesn't depend on any previous MSAD. */
   nir_instr_worklist *wl = nir_instr_worklist_create();
   nir_instr_worklist_add_ssa_srcs(wl, &msad->instr);
   nir_foreach_instr_in_worklist(instr, wl) {
      if (instr->block != msad->instr.block || instr->index < mqsad->first_msad_index)
         continue;

      u_foreach_bit(i, mqsad->mask) {
         if (instr == &mqsad->msad[i]->instr) {
            nir_instr_worklist_destroy(wl);
            return false;
         }
      }

      nir_instr_worklist_add_ssa_srcs(wl, instr);
   }
   nir_instr_worklist_destroy(wl);

   return true;
}

static void
parse_msad(nir_alu_instr *msad, struct mqsad *mqsad)
{
   if (msad->def.num_components != 1)
      return;

   nir_scalar msad_s = nir_get_scalar(&msad->def, 0);
   nir_scalar ref = nir_scalar_chase_alu_src(msad_s, 0);
   nir_scalar accum = nir_scalar_chase_alu_src(msad_s, 2);

   unsigned idx = 0;
   nir_scalar src0 = nir_scalar_chase_alu_src(msad_s, 1);
   nir_scalar src1;
   if (nir_scalar_is_alu(src0) && nir_scalar_alu_op(src0) == nir_op_shfr) {
      nir_scalar amount_s = nir_scalar_chase_alu_src(src0, 2);
      uint32_t amount = nir_scalar_is_const(amount_s) ? nir_scalar_as_uint(amount_s) : 0;
      if (amount == 8 || amount == 16 || amount == 24) {
         idx = amount / 8;
         src1 = nir_scalar_chase_alu_src(src0, 0);
         src0 = nir_scalar_chase_alu_src(src0, 1);
      }
   }

   if (mqsad->mask && !is_mqsad_compatible(mqsad, ref, src0, src1, idx, msad))
      memset(mqsad, 0, sizeof(*mqsad));

   /* Add this instruction to the in-progress MQSAD. */
   mqsad->ref = ref;
   mqsad->src[0] = src0;
   if (idx)
      mqsad->src[1] = src1;

   mqsad->accum[idx] = accum;
   mqsad->msad[idx] = msad;
   if (!mqsad->mask)
      mqsad->first_msad_index = msad->instr.index;
   mqsad->mask |= 1 << idx;
}

static void
create_msad(nir_builder *b, struct mqsad *mqsad)
{
   nir_def *mqsad_def = nir_mqsad_4x8(b, nir_channel(b, mqsad->ref.def, mqsad->ref.comp),
                                      nir_vec_scalars(b, mqsad->src, 2),
                                      nir_vec_scalars(b, mqsad->accum, 4));

   for (unsigned i = 0; i < 4; i++)
      nir_def_rewrite_uses(&mqsad->msad[i]->def, nir_channel(b, mqsad_def, i));

   memset(mqsad, 0, sizeof(*mqsad));
}

bool
nir_opt_mqsad(nir_shader *shader)
{
   bool progress = false;
   nir_foreach_function_impl(impl, shader) {
      bool progress_impl = false;

      nir_metadata_require(impl, nir_metadata_instr_index);

      nir_foreach_block(block, impl) {
         struct mqsad mqsad;
         memset(&mqsad, 0, sizeof(mqsad));

         nir_foreach_instr(instr, block) {
            if (instr->type != nir_instr_type_alu)
               continue;

            nir_alu_instr *alu = nir_instr_as_alu(instr);
            if (alu->op != nir_op_msad_4x8)
               continue;

            parse_msad(alu, &mqsad);

            if (mqsad.mask == 0xf) {
               nir_builder b = nir_builder_at(nir_before_instr(instr));
               create_msad(&b, &mqsad);
               progress_impl = true;
            }
         }
      }

      if (progress_impl) {
         nir_metadata_preserve(impl, nir_metadata_block_index | nir_metadata_dominance);
         progress = true;
      } else {
         nir_metadata_preserve(impl, nir_metadata_block_index);
      }
   }

   return progress;
}
