/*
 * Copyright 2023 Alyssa Rosenzweig
 * Copyright 2021 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "agx_compile.h"
#include "agx_tilebuffer.h"
#include "nir.h"
#include "nir_builder.h"
#include "nir_builder_opcodes.h"
#include "nir_intrinsics.h"

static bool
lower_to_per_sample(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   b->cursor = nir_before_instr(&intr->instr);

   switch (intr->intrinsic) {
   case nir_intrinsic_load_sample_id: {
      nir_def *mask = nir_u2u32(b, nir_load_active_samples_agx(b));
      nir_def *bit = nir_ufind_msb(b, mask);
      nir_def_rewrite_uses(&intr->def, nir_u2uN(b, bit, intr->def.bit_size));
      nir_instr_remove(&intr->instr);
      return true;
   }

   case nir_intrinsic_load_local_pixel_agx:
   case nir_intrinsic_store_local_pixel_agx:
   case nir_intrinsic_store_zs_agx:
   case nir_intrinsic_discard_agx:
   case nir_intrinsic_sample_mask_agx: {
      /* Fragment I/O inside the loop should only affect active samples. */
      unsigned mask_index =
         (intr->intrinsic == nir_intrinsic_store_local_pixel_agx) ? 1 : 0;

      nir_def *mask = intr->src[mask_index].ssa;
      nir_def *id_mask = nir_load_active_samples_agx(b);
      nir_def *converted = nir_u2uN(b, id_mask, mask->bit_size);

      nir_src_rewrite(&intr->src[mask_index], nir_iand(b, mask, converted));
      return true;
   }

   default:
      return false;
   }
}

bool
agx_nir_lower_to_per_sample(nir_shader *shader)
{
   return nir_shader_intrinsics_pass(
      shader, lower_to_per_sample,
      nir_metadata_block_index | nir_metadata_dominance, NULL);
}

static bool
lower_active_samples(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   if (intr->intrinsic != nir_intrinsic_load_active_samples_agx)
      return false;

   b->cursor = nir_instr_remove(&intr->instr);
   nir_def_rewrite_uses(&intr->def, data);
   return true;
}

/*
 * In a monolithic pixel shader, we wrap the fragment shader in a loop over
 * each sample, and then let optimizations (like loop unrolling) go to town.
 * This lowering is not compatible with fragment epilogues, which require
 * something similar at the binary level since the NIR is long gone by then.
 */
static bool
agx_nir_wrap_per_sample_loop(nir_shader *shader, uint8_t nr_samples)
{
   assert(nr_samples > 1);

   /* Get the original function */
   nir_function_impl *impl = nir_shader_get_entrypoint(shader);

   nir_cf_list list;
   nir_cf_extract(&list, nir_before_impl(impl), nir_after_impl(impl));

   /* Create a builder for the wrapped function */
   nir_builder b = nir_builder_at(nir_after_block(nir_start_block(impl)));

   nir_variable *i =
      nir_local_variable_create(impl, glsl_uintN_t_type(16), NULL);
   nir_store_var(&b, i, nir_imm_intN_t(&b, 1, 16), ~0);
   nir_def *bit = NULL;
   nir_def *end_bit = nir_imm_intN_t(&b, 1 << nr_samples, 16);

   /* Create a loop in the wrapped function */
   nir_loop *loop = nir_push_loop(&b);
   {
      bit = nir_load_var(&b, i);
      nir_push_if(&b, nir_uge(&b, bit, end_bit));
      {
         nir_jump(&b, nir_jump_break);
      }
      nir_pop_if(&b, NULL);

      b.cursor = nir_cf_reinsert(&list, b.cursor);
      nir_store_var(&b, i, nir_ishl_imm(&b, bit, 1), ~0);
   }
   nir_pop_loop(&b, loop);

   /* We've mucked about with control flow */
   nir_metadata_preserve(impl, nir_metadata_none);

   /* Use the loop variable for the active sampple mask each iteration */
   nir_shader_intrinsics_pass(shader, lower_active_samples,
                              nir_metadata_block_index | nir_metadata_dominance,
                              bit);
   return true;
}

/*
 * Lower a fragment shader into a monolithic pixel shader, with static sample
 * count, blend state, and tilebuffer formats in the shader key. For dynamic,
 * epilogs must be used, which have separate lowerings.
 */
bool
agx_nir_lower_monolithic_msaa(nir_shader *shader, uint8_t nr_samples)
{
   assert(shader->info.stage == MESA_SHADER_FRAGMENT);
   assert(nr_samples == 1 || nr_samples == 2 || nr_samples == 4);

   agx_nir_lower_sample_mask(shader);

   /* In single sampled programs, interpolateAtSample needs to return the
    * center pixel.
    */
   if (nr_samples == 1)
      nir_lower_single_sampled(shader);
   else if (shader->info.fs.uses_sample_shading) {
      agx_nir_lower_to_per_sample(shader);
      agx_nir_wrap_per_sample_loop(shader, nr_samples);
   }

   return true;
}
