/*
 * Copyright © 2015 Red Hat
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nir.h"
#include "nir_builder.h"

static bool
check_location(int location)
{
   return location == VARYING_SLOT_COL0 ||
          location == VARYING_SLOT_COL1 ||
          location == VARYING_SLOT_BFC0 ||
          location == VARYING_SLOT_BFC1;
}

static bool
lower_input(nir_shader *shader, nir_variable *var)
{
   if (var->data.interpolation == INTERP_MODE_NONE &&
       check_location(var->data.location))
      var->data.interpolation = INTERP_MODE_FLAT;
   return true;
}

static bool
lower_input_io(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   if (intr->intrinsic != nir_intrinsic_load_interpolated_input)
      return false;;
   nir_io_semantics sem = nir_intrinsic_io_semantics(intr);
   if (!check_location(sem.location))
      return false;
   nir_def *interp = intr->src[0].ssa;
   nir_intrinsic_instr *interp_intr = nir_instr_as_intrinsic(interp->parent_instr);
   if (nir_intrinsic_interp_mode(interp_intr) != INTERP_MODE_NONE)
      return false;
   b->cursor = nir_before_instr(&intr->instr);
   nir_def *load = nir_load_input(b, intr->num_components,
                                  intr->def.bit_size, intr->src[1].ssa);
   nir_intrinsic_instr *new_intr = nir_instr_as_intrinsic(load->parent_instr);
   nir_intrinsic_copy_const_indices(new_intr, intr);
   nir_def_rewrite_uses(&intr->def, load);
   nir_instr_remove(&intr->instr);
   return true;
}
bool
nir_lower_flatshade(nir_shader *shader)
{
   bool progress = false;

   if (shader->info.io_lowered) {
      progress = nir_shader_intrinsics_pass(shader, lower_input_io,
                                            nir_metadata_all, NULL);
   } else {
      nir_foreach_shader_in_variable(var, shader) {
         progress |= lower_input(shader, var);
      }
   }

   /* Interpolation doesn't affect any metadata */
   nir_shader_preserve_all_metadata(shader);
   return progress;
}
