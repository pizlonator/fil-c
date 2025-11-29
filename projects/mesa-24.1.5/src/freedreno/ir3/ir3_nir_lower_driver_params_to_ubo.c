/*
 * Copyright © 2023 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#include "compiler/nir/nir.h"
#include "compiler/nir/nir_builder.h"
#include "util/u_math.h"
#include "ir3_compiler.h"
#include "ir3_nir.h"

static bool
lower_driver_param_to_ubo(nir_builder *b, nir_intrinsic_instr *intr, void *in)
{
   struct ir3_const_state *const_state = in;

   if (b->shader->info.stage == MESA_SHADER_VERTEX)
      return false;

   unsigned components = nir_intrinsic_dest_components(intr);

   b->cursor = nir_before_instr(&intr->instr);

   nir_def *result;
   switch (intr->intrinsic) {
   case nir_intrinsic_load_primitive_location_ir3:
      result = ir3_load_driver_ubo(b, components,
                                   &const_state->primitive_map_ubo,
                                   nir_intrinsic_driver_location(intr));
      break;
   case nir_intrinsic_load_vs_primitive_stride_ir3:
      result = ir3_load_driver_ubo(b, components,
                                   &const_state->primitive_param_ubo, 0);
      break;
   case nir_intrinsic_load_vs_vertex_stride_ir3:
      result = ir3_load_driver_ubo(b, components,
                                   &const_state->primitive_param_ubo, 1);
      break;
   case nir_intrinsic_load_hs_patch_stride_ir3:
      result = ir3_load_driver_ubo(b, components,
                                   &const_state->primitive_param_ubo, 2);
      break;
   case nir_intrinsic_load_patch_vertices_in:
      result = ir3_load_driver_ubo(b, components,
                                   &const_state->primitive_param_ubo, 3);
      break;
   case nir_intrinsic_load_tess_param_base_ir3:
      result = ir3_load_driver_ubo(b, components,
                                   &const_state->primitive_param_ubo, 4);
      break;
   case nir_intrinsic_load_tess_factor_base_ir3:
      result = ir3_load_driver_ubo(b, components,
                                   &const_state->primitive_param_ubo, 6);
      break;
   default: {
      struct driver_param_info param_info;
      if (!ir3_get_driver_param_info(b->shader, intr, &param_info))
         return false;

      result = ir3_load_driver_ubo(b, components,
                                   &const_state->driver_params_ubo,
                                   param_info.offset);
   }
   }

   nir_instr_remove(&intr->instr);
   nir_def_rewrite_uses(&intr->def, result);

   return true;
}

bool
ir3_nir_lower_driver_params_to_ubo(nir_shader *nir,
                                   struct ir3_shader_variant *v)
{
   bool result = nir_shader_intrinsics_pass(
      nir, lower_driver_param_to_ubo,
      nir_metadata_block_index | nir_metadata_dominance, ir3_const_state(v));

   return result;
}
