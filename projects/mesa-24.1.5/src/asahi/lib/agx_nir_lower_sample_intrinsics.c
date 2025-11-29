/*
 * Copyright 2023 Valve Corporation
 * Copyright 2023 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#include "agx_tilebuffer.h"
#include "nir.h"
#include "nir_builder.h"
#include "nir_builder_opcodes.h"
#include "nir_intrinsics.h"

static nir_def *
select_if_msaa_else_0(nir_builder *b, nir_def *x)
{
   /* Sample count > 1 <==> log2(Sample count) > 0 */
   nir_def *msaa = nir_ugt_imm(b, nir_load_samples_log2_agx(b), 0);

   return nir_bcsel(b, msaa, x, nir_imm_intN_t(b, 0, x->bit_size));
}

static bool
lower(nir_builder *b, nir_intrinsic_instr *intr, void *_)
{
   b->cursor = nir_before_instr(&intr->instr);

   switch (intr->intrinsic) {
   case nir_intrinsic_load_sample_pos: {
      /* Lower sample positions to decode the packed fixed-point register:
       *
       *    uint32_t packed = load_sample_positions();
       *    uint32_t shifted = packed >> (sample_id * 8);
       *
       *    for (i = 0; i < 2; ++i) {
       *       uint8_t nibble = (shifted >> (i * 4)) & 0xF;
       *       xy[component] = ((float)nibble) / 16.0;
       *    }
       */
      nir_def *packed = nir_load_sample_positions_agx(b);

      /* The n'th sample is the in the n'th byte of the register */
      nir_def *shifted = nir_ushr(
         b, packed, nir_u2u32(b, nir_imul_imm(b, nir_load_sample_id(b), 8)));

      nir_def *xy[2];
      for (unsigned i = 0; i < 2; ++i) {
         /* Get the appropriate nibble */
         nir_def *nibble =
            nir_iand_imm(b, nir_ushr_imm(b, shifted, i * 4), 0xF);

         /* Convert it from fixed point to float */
         xy[i] = nir_fmul_imm(b, nir_u2f16(b, nibble), 1.0 / 16.0);

         /* Upconvert if necessary */
         xy[i] = nir_f2fN(b, xy[i], intr->def.bit_size);
      }

      /* Collect and rewrite */
      nir_def_rewrite_uses(&intr->def, nir_vec2(b, xy[0], xy[1]));
      nir_instr_remove(&intr->instr);
      return true;
   }

   case nir_intrinsic_load_sample_mask_in: {
      /* Apply API sample mask to sample mask inputs, lowering:
       *
       *     sample_mask_in --> sample_mask_in & api_sample_mask
       *
       * Furthermore in OpenGL, gl_SampleMaskIn is only supposed to have the
       * single bit set of the sample currently being shaded when sample shading
       * is used. Mask by the sample ID to make that happen.
       */
      b->cursor = nir_after_instr(&intr->instr);
      nir_def *old = &intr->def;
      nir_def *lowered = nir_iand(
         b, old, nir_u2uN(b, nir_load_api_sample_mask_agx(b), old->bit_size));

      if (b->shader->info.fs.uses_sample_shading) {
         nir_def *bit = nir_load_active_samples_agx(b);
         lowered = nir_iand(b, lowered, nir_u2uN(b, bit, old->bit_size));
      }

      nir_def_rewrite_uses_after(old, lowered, lowered->parent_instr);
      return true;
   }

   case nir_intrinsic_load_barycentric_sample: {
      /* Lower fragment varyings with "sample" interpolation to
       * interpolateAtSample() with the sample ID. If multisampling is disabled,
       * the sample ID is 0, so we don't need to mask unlike for
       * load_barycentric_at_sample.
       */
      b->cursor = nir_after_instr(&intr->instr);
      nir_def *old = &intr->def;

      nir_def *lowered = nir_load_barycentric_at_sample(
         b, intr->def.bit_size, nir_load_sample_id(b),
         .interp_mode = nir_intrinsic_interp_mode(intr));

      nir_def_rewrite_uses_after(old, lowered, lowered->parent_instr);
      return true;
   }

   case nir_intrinsic_load_barycentric_at_sample: {
      /*
       * In OpenGL, interpolateAtSample interpolates at the centre when
       * multisampling is disabled. Furthermore, results are undefined when
       * multisampling is enabled but the sample ID is out-of-bounds.
       *
       * To handle the former case, we force the sample ID to 0 when
       * multisampling is disabled. To optimize the latter case, we force the
       * sample ID to 0 when the requested sample is definitively out-of-bounds.
       */
      b->cursor = nir_before_instr(&intr->instr);

      nir_src *src = &intr->src[0];
      nir_def *sample = src->ssa;

      if (nir_src_is_const(*src) && nir_src_as_uint(*src) >= 4) {
         sample = nir_imm_int(b, 0);
      } else {
         sample = select_if_msaa_else_0(b, sample);
      }

      nir_src_rewrite(src, sample);
      return true;
   }

   case nir_intrinsic_store_output: {
      /*
       * Sample mask writes are ignored unless multisampling is used. If it is
       * used, the Vulkan spec says:
       *
       *    If sample shading is enabled, bits written to SampleMask
       *    corresponding to samples that are not being shaded by the fragment
       *    shader invocation are ignored.
       *
       * That will be satisfied by outputting gl_SampleMask for the whole pixel
       * and then lowering sample shading after (splitting up discard targets).
       */
      nir_io_semantics sem = nir_intrinsic_io_semantics(intr);
      if (sem.location != FRAG_RESULT_SAMPLE_MASK)
         return false;

      nir_def *mask = nir_inot(b, nir_u2u16(b, intr->src[0].ssa));

      nir_discard_agx(b, select_if_msaa_else_0(b, mask));
      nir_instr_remove(&intr->instr);

      b->shader->info.fs.uses_discard = true;
      return true;
   }

   default:
      return false;
   }
}

/*
 * In a fragment shader using sample shading, lower intrinsics like
 * load_sample_position to variants in terms of load_sample_id. Except for a
 * possible API bit to force sample shading in shaders that don't otherwise need
 * it, this pass does not depend on the shader key. In particular, it does not
 * depend on the sample count. So it runs on fragment shaders at compile-time.
 * The load_sample_id intrinsics themselves are lowered later, with different
 * lowerings for monolithic vs epilogs.
 *
 * Note that fragment I/O (like store_local_pixel_agx and discard_agx) does not
 * get lowered here, because that lowering is different for monolithic vs FS
 * epilogs even though there's no dependency on sample count.
 */
bool
agx_nir_lower_sample_intrinsics(nir_shader *shader)
{
   return nir_shader_intrinsics_pass(
      shader, lower, nir_metadata_block_index | nir_metadata_dominance, NULL);
}
