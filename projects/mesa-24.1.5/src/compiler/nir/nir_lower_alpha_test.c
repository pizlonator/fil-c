/*
 * Copyright © 2017 Broadcom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file
 *
 * Implements GL alpha testing by comparing the output color's alpha to the
 * alpha_ref intrinsic and emitting a discard based on it.
 *
 * The alpha_to_one value overrides the source alpha to 1.0 to implement
 * GL_SAMPLE_ALPHA_TO_ONE, which applies before the alpha test (and would be
 * rather silly to use with alpha test, but the spec permits).
 */

#include "nir/nir.h"
#include "nir/nir_builder.h"

struct alpha_test_state {
   bool alpha_to_one;
   enum compare_func func;
   const gl_state_index16 *alpha_ref_state_tokens;
};

static bool
is_color_output(int location)
{
   return location == FRAG_RESULT_COLOR || location == FRAG_RESULT_DATA0;
}

static bool
lower(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   struct alpha_test_state *state = data;
   nir_variable *out = NULL;

   switch (intr->intrinsic) {
   case nir_intrinsic_store_deref:
      out = nir_intrinsic_get_var(intr, 0);
      if (out->data.mode != nir_var_shader_out)
         return false;

      if (!is_color_output(out->data.location))
         return false;
      break;
   case nir_intrinsic_store_output:
      if (!is_color_output(nir_intrinsic_io_semantics(intr).location))
         return false;
      break;
   default:
      return false;
   }

   b->cursor = nir_before_instr(&intr->instr);

   nir_def *alpha;
   if (state->alpha_to_one)
      alpha = nir_imm_float(b, 1.0);
   else if (intr->intrinsic == nir_intrinsic_store_deref)
      alpha = nir_channel(b, intr->src[1].ssa, 3);
   else
      alpha = nir_channel(b, intr->src[0].ssa, 3);

   nir_variable *var = nir_state_variable_create(b->shader, glsl_float_type(),
                                                 "gl_AlphaRefMESA",
                                                 state->alpha_ref_state_tokens);
   nir_def *alpha_ref = nir_load_var(b, var);
   nir_def *condition = nir_compare_func(b, state->func, alpha, alpha_ref);

   nir_discard_if(b, nir_inot(b, condition));
   b->shader->info.fs.uses_discard = true;
   return true;
}

bool
nir_lower_alpha_test(nir_shader *shader, enum compare_func func,
                     bool alpha_to_one,
                     const gl_state_index16 *alpha_ref_state_tokens)
{
   struct alpha_test_state state = {
      .alpha_ref_state_tokens = alpha_ref_state_tokens,
      .alpha_to_one = alpha_to_one,
      .func = func,
   };

   return nir_shader_intrinsics_pass(shader, lower,
                                     nir_metadata_block_index |
                                        nir_metadata_dominance,
                                     &state);
}
