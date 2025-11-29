/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 * Copyright © 2023 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "nir.h"
#include "nir_builder.h"
#include "radv_nir.h"

bool
radv_nir_lower_intrinsics_early(nir_shader *nir, bool lower_view_index_to_zero)
{
   nir_function_impl *entry = nir_shader_get_entrypoint(nir);
   bool progress = false;
   nir_builder b = nir_builder_create(entry);

   nir_foreach_block (block, entry) {
      nir_foreach_instr_safe (instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
         b.cursor = nir_before_instr(&intrin->instr);

         nir_def *def = NULL;
         switch (intrin->intrinsic) {
         case nir_intrinsic_is_sparse_texels_resident:
            def = nir_ieq_imm(&b, intrin->src[0].ssa, 0);
            break;
         case nir_intrinsic_sparse_residency_code_and:
            def = nir_ior(&b, intrin->src[0].ssa, intrin->src[1].ssa);
            break;
         case nir_intrinsic_load_view_index:
            if (!lower_view_index_to_zero)
               continue;
            def = nir_imm_zero(&b, 1, 32);
            break;
         default:
            continue;
         }

         nir_def_rewrite_uses(&intrin->def, def);

         nir_instr_remove(instr);
         progress = true;
      }
   }

   if (progress)
      nir_metadata_preserve(entry, nir_metadata_block_index | nir_metadata_dominance);
   else
      nir_metadata_preserve(entry, nir_metadata_all);

   return progress;
}
