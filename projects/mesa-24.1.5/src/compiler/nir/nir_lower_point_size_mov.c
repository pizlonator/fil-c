/*
 * Copyright © 2019 Collabora Ltd
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

#include "nir.h"
#include "nir_builder.h"

/** nir_lower_point_size_mov.c
 *
 * This pass lowers glPointSize into gl_PointSize, by adding a uniform
 * and a move from that uniform to VARYING_SLOT_PSIZ. This is useful for
 * OpenGL ES level hardware that lack constant point-size hardware state.
 */

static void
lower_point_size_mov_after(nir_builder *b, nir_variable *in)
{
   nir_def *load = nir_load_var(b, in);
   load = nir_fclamp(b, nir_channel(b, load, 0), nir_channel(b, load, 1), nir_channel(b, load, 2));
   if (b->shader->info.io_lowered) {
      nir_store_output(b, load, nir_imm_int(b, 0),
                       .io_semantics.location = VARYING_SLOT_PSIZ,
                       .io_semantics.num_slots = 1,
                       .src_type = nir_type_float32
      );
   } else {
      nir_variable *out = NULL;
      /* the existing output can't be removed in order to avoid breaking xfb.
       * drivers must check var->data.explicit_location to find the original output
       * and only emit that one for xfb
       */
      nir_foreach_shader_out_variable(var, b->shader) {
         if (var->data.location == VARYING_SLOT_PSIZ && !var->data.explicit_location) {
            out = var;
            break;
         }
      }
      if (!out) {
         out = nir_create_variable_with_location(b->shader, nir_var_shader_out,
                                                 VARYING_SLOT_PSIZ, glsl_float_type());
      }
      nir_store_var(b, out, load, 0x1);
   }
}

static bool
lower_point_size_mov(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   nir_variable *var = NULL;
   switch (intr->intrinsic) {
   case nir_intrinsic_store_output:
   case nir_intrinsic_store_per_vertex_output:
   case nir_intrinsic_store_per_primitive_output: {
      nir_io_semantics sem = nir_intrinsic_io_semantics(intr);
      if (sem.location != VARYING_SLOT_PSIZ)
         return false;
      break;
   }
   case nir_intrinsic_store_deref:
      var = nir_intrinsic_get_var(intr, 0);
      if (var->data.location != VARYING_SLOT_PSIZ)
         return false;
      break;
   default:
      return false;
   }
   b->cursor = nir_after_instr(&intr->instr);
   lower_point_size_mov_after(b, data);
   if (var && !var->data.explicit_location)
      nir_instr_remove(&intr->instr);
   return true;
}

bool
nir_lower_point_size_mov(nir_shader *shader,
                         const gl_state_index16 *pointsize_state_tokens)
{
   assert(shader->info.stage != MESA_SHADER_FRAGMENT &&
          shader->info.stage != MESA_SHADER_COMPUTE);


   bool progress = false;
   nir_metadata preserved = nir_metadata_dominance | nir_metadata_block_index;
   nir_variable *in = nir_state_variable_create(shader, glsl_vec4_type(),
                                                "gl_PointSizeClampedMESA",
                                                pointsize_state_tokens);
   if (shader->info.outputs_written & VARYING_BIT_PSIZ) {
      progress = nir_shader_intrinsics_pass(shader, lower_point_size_mov, preserved, in);
   } else {
      nir_function_impl *impl = nir_shader_get_entrypoint(shader);
      nir_builder b = nir_builder_at(nir_before_impl(impl));

      lower_point_size_mov_after(&b, in);
      shader->info.outputs_written |= VARYING_BIT_PSIZ;
      progress = true;
      nir_metadata_preserve(impl, preserved);
   }
   return progress;
}
