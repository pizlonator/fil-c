/*
 * Copyright © 2023 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "nir.h"
#include "nir_builder.h"
#include "radv_nir.h"
#include "radv_pipeline_graphics.h"

static bool
radv_should_lower_poly_line_smooth(nir_shader *nir, const struct radv_graphics_state_key *gfx_state)
{
   nir_function_impl *impl = nir_shader_get_entrypoint(nir);

   if (!gfx_state->rs.line_smooth_enabled && !gfx_state->dynamic_line_rast_mode)
      return false;

   nir_foreach_block (block, impl) {
      nir_foreach_instr (instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
         if (intr->intrinsic != nir_intrinsic_store_output)
            continue;

         /* Line smooth lowering is only valid for vec4. */
         if (intr->num_components != 4)
            return false;
      }
   }

   return true;
}

void
radv_nir_lower_poly_line_smooth(nir_shader *nir, const struct radv_graphics_state_key *gfx_state)
{
   bool progress = false;

   if (!radv_should_lower_poly_line_smooth(nir, gfx_state))
      return;

   NIR_PASS(progress, nir, nir_lower_poly_line_smooth, RADV_NUM_SMOOTH_AA_SAMPLES);
   if (progress)
      nir_shader_gather_info(nir, nir_shader_get_entrypoint(nir));
}
