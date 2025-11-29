/*
 * Copyright 2023 Valve Corporation
 * SPDX-License-Identifier: MIT
 */
#include "agx_state.h"
#include "nir.h"
#include "nir_builder.h"
#include "nir_builder_opcodes.h"

/*
 * gl_PointSize lowering. This runs late on a vertex shader. By this time, I/O
 * has been lowered, and transform feedback has been written. Point size will
 * thus only get consumed by the rasterizer, so we can clamp/replace. We do
 * this instead of the mesa/st lowerings to avoid the variant. I wouldn't mind
 * ripping this out some day...
 */

static bool
pass(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   b->cursor = nir_before_instr(&intr->instr);

   if ((intr->intrinsic != nir_intrinsic_store_output) ||
       (nir_intrinsic_io_semantics(intr).location != VARYING_SLOT_PSIZ))
      return false;

   /* The size we write must be clamped */
   nir_def *size = nir_fmax(b, intr->src[0].ssa, nir_imm_float(b, 1.0f));

   /* Override it if the API requires */
   nir_def *fixed_size = nir_load_fixed_point_size_agx(b);
   size = nir_bcsel(b, nir_fgt_imm(b, fixed_size, 0.0), fixed_size, size);

   nir_src_rewrite(&intr->src[0], size);
   return true;
}

bool
agx_nir_lower_point_size(nir_shader *nir, bool insert_write)
{
   /* Lower existing point size write */
   if (nir_shader_intrinsics_pass(
          nir, pass, nir_metadata_block_index | nir_metadata_dominance, NULL))
      return true;

   if (!insert_write)
      return false;

   /* If there's no existing point size write, insert one. This assumes there
    * was a fixed point size set in the API. If not, GL allows undefined
    * behaviour, which we implement by writing garbage.
    */
   nir_builder b =
      nir_builder_at(nir_after_impl(nir_shader_get_entrypoint(nir)));

   nir_store_output(&b, nir_load_fixed_point_size_agx(&b), nir_imm_int(&b, 0),
                    .io_semantics.location = VARYING_SLOT_PSIZ,
                    .io_semantics.num_slots = 1,
                    .write_mask = nir_component_mask(1));

   nir->info.outputs_written |= VARYING_BIT_PSIZ;
   nir_metadata_preserve(b.impl,
                         nir_metadata_dominance | nir_metadata_block_index);

   return true;
}
