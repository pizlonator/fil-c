/*
 * Copyright 2023 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#include "compiler/glsl/list.h"
#include "compiler/nir/nir_builder.h"
#include "agx_compiler.h"
#include "nir.h"
#include "nir_builder_opcodes.h"
#include "nir_intrinsics.h"

/*
 * sample_mask takes two bitmasks as arguments, TARGET and LIVE. Each bit refers
 * to an indexed sample. Roughly, the instruction does:
 *
 *    foreach sample in TARGET {
 *       if sample in LIVE {
 *          run depth/stencil/occlusion test/update
 *       } else {
 *          kill sample
 *       }
 *    }
 *
 * As a special case, TARGET may be set to all-1s (~0) to refer to all samples
 * regardless of the framebuffer sample count.
 *
 * For example, to discard an entire pixel unconditionally, we could run:
 *
 *    sample_mask ~0, 0
 *
 * sample_mask must follow these rules:
 *
 * 1. All sample_mask instructions affecting a sample must execute before a
 *    local_store_pixel instruction targeting that sample. This ensures that
 *    nothing is written for discarded samples (whether discarded in shader or
 *    due to a failed depth/stencil test).
 *
 * 2. If sample_mask is used anywhere in a shader, then on every execution path,
 *    every sample must be killed or else run depth/stencil tests exactly ONCE.
 *
 * 3. If a sample is killed, future sample_mask instructions have
 *    no effect on that sample. The following code sequence correctly implements
 *    a conditional discard (if there are no other sample_mask instructions in
 *    the shader):
 *
 *       sample_mask discarded, 0
 *       sample_mask ~0, ~0
 *
 *    but this sequence is incorrect:
 *
 *       sample_mask ~0, ~discarded
 *       sample_mask ~0, ~0         <-- incorrect: depth/stencil tests run twice
 *
 * 4. zs_emit may be used in the shader exactly once to trigger tests.
 * sample_mask with 0 may be used to discard early.
 *
 * This pass lowers discard_agx to sample_mask instructions satisfying these
 * rules. Other passes should not generate sample_mask instructions, as there
 * are too many footguns.
 */

#define ALL_SAMPLES (0xFF)
#define BASE_Z      1
#define BASE_S      2

static bool
lower_discard_to_sample_mask_0(nir_builder *b, nir_intrinsic_instr *intr,
                               UNUSED void *data)
{
   if (intr->intrinsic != nir_intrinsic_discard_agx)
      return false;

   b->cursor = nir_before_instr(&intr->instr);
   nir_sample_mask_agx(b, intr->src[0].ssa, nir_imm_intN_t(b, 0, 16));
   nir_instr_remove(&intr->instr);
   return true;
}

static nir_intrinsic_instr *
last_discard_in_block(nir_block *block)
{
   nir_foreach_instr_reverse(instr, block) {
      if (instr->type != nir_instr_type_intrinsic)
         continue;

      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
      if (intr->intrinsic == nir_intrinsic_discard_agx)
         return intr;
   }

   return NULL;
}

static bool
cf_node_contains_discard(nir_cf_node *node)
{
   nir_foreach_block_in_cf_node(block, node) {
      if (last_discard_in_block(block))
         return true;
   }

   return false;
}

/*
 * We want to run depth/stencil tests as early as possible, but we have to
 * wait until after the last discard. We find the last discard and
 * execute depth/stencil tests in the first unconditional block after (if
 * in conditional control flow), or fuse depth/stencil tests into the
 * sample instruction (if in unconditional control flow).
 *
 * To do so, we walk the root control flow list backwards, looking for the
 * earliest unconditionally executed instruction after all discard.
 */
static void
run_tests_after_last_discard(nir_builder *b)
{
   foreach_list_typed_reverse(nir_cf_node, node, node, &b->impl->body) {
      if (node->type == nir_cf_node_block) {
         /* Unconditionally executed block */
         nir_block *block = nir_cf_node_as_block(node);
         nir_intrinsic_instr *intr = last_discard_in_block(block);

         if (intr) {
            /* Last discard is executed unconditionally, so fuse tests:
             *
             *    sample_mask (testing | killed), ~killed
             *
             * When testing, this is `sample_mask ~0, ~killed` which kills the
             * kill set and triggers tests on the rest.
             *
             * When not testing, this is `sample_mask killed, ~killed` which is
             * equivalent to `sample_mask killed, 0`, killing without testing.
             */
            b->cursor = nir_before_instr(&intr->instr);

            nir_def *all_samples = nir_imm_intN_t(b, ALL_SAMPLES, 16);
            nir_def *killed = intr->src[0].ssa;
            nir_def *live = nir_ixor(b, killed, all_samples);

            nir_def *testing = nir_load_shader_part_tests_zs_agx(b);
            nir_def *affected = nir_ior(b, testing, killed);

            nir_sample_mask_agx(b, affected, live);
            nir_instr_remove(&intr->instr);
            return;
         } else {
            /* Set cursor for insertion due to a preceding conditionally
             * executed discard.
             */
            b->cursor = nir_before_block_after_phis(block);
         }
      } else if (cf_node_contains_discard(node)) {
         /* Conditionally executed block contains the last discard. Test
          * depth/stencil for remaining samples in unconditional code after.
          *
          * If we're not testing, this turns into sample_mask(0, ~0) which is a
          * no-op.
          */
         nir_sample_mask_agx(b, nir_load_shader_part_tests_zs_agx(b),
                             nir_imm_intN_t(b, ALL_SAMPLES, 16));
         return;
      }
   }
}

static void
run_tests_at_start(nir_shader *shader)
{
   nir_function_impl *impl = nir_shader_get_entrypoint(shader);
   nir_builder b = nir_builder_at(nir_before_impl(impl));

   nir_sample_mask_agx(&b, nir_imm_intN_t(&b, ALL_SAMPLES, 16),
                       nir_imm_intN_t(&b, ALL_SAMPLES, 16));
}

bool
agx_nir_lower_sample_mask(nir_shader *shader)
{
   nir_function_impl *impl = nir_shader_get_entrypoint(shader);

   bool writes_zs =
      shader->info.outputs_written &
      (BITFIELD64_BIT(FRAG_RESULT_STENCIL) | BITFIELD64_BIT(FRAG_RESULT_DEPTH));

   if (shader->info.fs.early_fragment_tests) {
      /* run tests early, if we need testing */
      if (shader->info.fs.uses_discard || writes_zs ||
          shader->info.writes_memory) {

         run_tests_at_start(shader);
      }
   } else if (shader->info.fs.uses_discard) {
      /* If we have zs_emit, the tests will be triggered by zs_emit, otherwise
       * we need to trigger tests explicitly. Allow sample_mask with zs_emit.
       */
      if (!writes_zs) {
         nir_builder b = nir_builder_create(impl);

         /* run tests late */
         run_tests_after_last_discard(&b);
      }
   } else {
      /* regular shaders that don't use discard have nothing to lower */
      nir_metadata_preserve(impl, nir_metadata_all);
      return false;
   }

   nir_metadata_preserve(impl,
                         nir_metadata_block_index | nir_metadata_dominance);

   nir_shader_intrinsics_pass(shader, lower_discard_to_sample_mask_0,
                              nir_metadata_block_index | nir_metadata_dominance,
                              NULL);

   return true;
}
