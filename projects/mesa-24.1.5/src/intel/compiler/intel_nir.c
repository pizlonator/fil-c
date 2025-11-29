/*
 * Copyright (c) 2014-2023 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "intel_nir.h"

bool
intel_nir_pulls_at_sample(nir_shader *shader)
{
   nir_foreach_function_impl(impl, shader) {
      nir_foreach_block(block, impl) {
         nir_foreach_instr(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

            if (intrin->intrinsic == nir_intrinsic_load_barycentric_at_sample)
               return true;
         }
      }
   }

   return false;
}


