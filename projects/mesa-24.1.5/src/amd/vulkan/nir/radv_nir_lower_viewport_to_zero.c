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
radv_nir_lower_viewport_to_zero(nir_shader *nir)
{
   nir_function_impl *impl = nir_shader_get_entrypoint(nir);
   bool progress = false;

   nir_builder b = nir_builder_create(impl);

   /* There should be only one deref load for VIEWPORT after lower_io_to_temporaries. */
   nir_foreach_block (block, impl) {
      nir_foreach_instr (instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
         if (intr->intrinsic != nir_intrinsic_load_deref)
            continue;

         nir_variable *var = nir_intrinsic_get_var(intr, 0);
         if (var->data.mode != nir_var_shader_in || var->data.location != VARYING_SLOT_VIEWPORT)
            continue;

         b.cursor = nir_before_instr(instr);

         nir_def_rewrite_uses(&intr->def, nir_imm_zero(&b, 1, 32));
         progress = true;
         break;
      }
      if (progress)
         break;
   }

   if (progress)
      nir_metadata_preserve(impl, nir_metadata_block_index | nir_metadata_dominance);
   else
      nir_metadata_preserve(impl, nir_metadata_all);

   return progress;
}
