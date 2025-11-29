/*
 * Copyright 2024 Valve Corporation
 * Copyright 2019 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "nir.h"
#include "nir_builder.h"

/*
 * Lowers SSBOs to global memory. SSBO base addresses are passed via
 * load_ssbo_address. Run nir_lower_robust_access first for bounds checks.
 */

static nir_def *
calc_address(nir_builder *b, nir_intrinsic_instr *intr)
{
   unsigned index_src = intr->intrinsic == nir_intrinsic_store_ssbo ? 1 : 0;
   nir_def *base = nir_load_ssbo_address(b, 1, 64, intr->src[index_src].ssa);

   return nir_iadd(b, base, nir_u2u64(b, nir_get_io_offset_src(intr)->ssa));
}

static bool
pass(nir_builder *b, nir_intrinsic_instr *intr, UNUSED void *data)
{
   b->cursor = nir_before_instr(&intr->instr);

   nir_def *def = NULL;
   switch (intr->intrinsic) {
   case nir_intrinsic_load_ssbo:
      def = nir_build_load_global(b, intr->def.num_components,
                                  intr->def.bit_size, calc_address(b, intr),
                                  .align_mul = nir_intrinsic_align_mul(intr),
                                  .align_offset = nir_intrinsic_align_offset(intr));
      break;

   case nir_intrinsic_store_ssbo:
      nir_build_store_global(b, intr->src[0].ssa, calc_address(b, intr),
                             .align_mul = nir_intrinsic_align_mul(intr),
                             .align_offset = nir_intrinsic_align_offset(intr),
                             .write_mask = nir_intrinsic_write_mask(intr));
      break;

   case nir_intrinsic_ssbo_atomic:
      def = nir_global_atomic(b, intr->def.bit_size, calc_address(b, intr),
                              intr->src[2].ssa,
                              .atomic_op = nir_intrinsic_atomic_op(intr));
      break;

   case nir_intrinsic_ssbo_atomic_swap:
      def = nir_global_atomic_swap(b, intr->def.bit_size, calc_address(b, intr),
                                   intr->src[2].ssa, intr->src[3].ssa,
                                   .atomic_op = nir_intrinsic_atomic_op(intr));
      break;

   default:
      return false;
   }

   if (def)
      nir_def_rewrite_uses(&intr->def, def);

   nir_instr_remove(&intr->instr);
   return true;
}

bool
nir_lower_ssbo(nir_shader *shader)
{
   return nir_shader_intrinsics_pass(shader, pass,
                                     nir_metadata_dominance |
                                        nir_metadata_block_index,
                                     NULL);
}
