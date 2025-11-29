/*
 * Copyright © 2010 Intel Corporation
 * Copyright © 2024 Valve Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * Implements the GLSL 1.30 revision 9 rule for fragment shader
 * discard handling:
 *
 *     "Control flow exits the shader, and subsequent implicit or
 *      explicit derivatives are undefined when this control flow is
 *      non-uniform (meaning different fragments within the primitive
 *      take different control paths)."
 *
 * There seem to be two conflicting things here.  "Control flow exits
 * the shader" sounds like the discarded fragments should effectively
 * jump to the end of the shader, but that breaks derivatives in the
 * case of uniform control flow and causes rendering failure in the
 * bushes in Unigine Tropics.
 *
 * The question, then, is whether the intent was "loops stop at the
 * point that the only active channels left are discarded pixels" or
 * "discarded pixels become inactive at the point that control flow
 * returns to the top of a loop".  This implements the second
 * interpretation.
 */

#include "compiler/glsl_types.h"
#include "nir.h"
#include "nir_builder.h"
#include "gl_nir.h"

static void
set_discard_global(nir_builder *b, nir_variable *discarded,
                   nir_intrinsic_instr *intrin)
{
   nir_deref_instr *lhs = nir_build_deref_var(b, discarded);
   nir_def *rhs;
   if (intrin->intrinsic == nir_intrinsic_discard_if) {
      /* discarded <- condition, use  discarded as the condition */
      rhs = intrin->src[0].ssa;
      nir_src_rewrite(&intrin->src[0], &lhs->def);
   } else {
      rhs = nir_imm_bool(b, true);
   }

   nir_store_deref(b, lhs, rhs, ~0);
}

static void
generate_discard_break(nir_builder *b, nir_variable *discarded)
{
   nir_deref_instr *condition = nir_build_deref_var(b, discarded);
   nir_if *nif = nir_push_if(b, nir_load_deref(b, condition));
   nir_jump(b, nir_jump_break);
   nir_pop_if(b, nif);
}

static void
lower_discard_flow(nir_builder *b, nir_cf_node *cf_node,
                   nir_variable *discarded)
{
   switch (cf_node->type) {
   case nir_cf_node_block: {
      nir_block *block = nir_cf_node_as_block(cf_node);
      nir_foreach_instr(instr, block) {
         if (instr->type == nir_instr_type_jump) {
            nir_jump_instr *jump_instr = nir_instr_as_jump(instr);
            if (jump_instr->type == nir_jump_continue) {
               b->cursor = nir_before_instr(instr);
               generate_discard_break(b, discarded);
            }
         } else if (instr->type == nir_instr_type_intrinsic) {
            nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
            if (intrin->intrinsic == nir_intrinsic_discard_if ||
                intrin->intrinsic == nir_intrinsic_discard) {
               b->cursor = nir_before_instr(instr);
               set_discard_global(b, discarded, intrin);
            }
         }
      }
      return;
   }
   case nir_cf_node_if: {
      nir_if *if_stmt = nir_cf_node_as_if(cf_node);
      foreach_list_typed(nir_cf_node, nested_node, node, &if_stmt->then_list)
         lower_discard_flow(b, nested_node, discarded);
      foreach_list_typed(nir_cf_node, nested_node, node, &if_stmt->else_list)
         lower_discard_flow(b, nested_node, discarded);
      return;
   }
   case nir_cf_node_loop: {
      nir_loop *loop = nir_cf_node_as_loop(cf_node);
      assert(!nir_loop_has_continue_construct(loop));

      /* Insert discard break at the end of the loop body */
      nir_block *last_block = nir_loop_last_block(loop);
      nir_instr *last_instr = nir_block_last_instr(last_block);
      if (last_instr == NULL || last_instr->type != nir_instr_type_jump) {
         b->cursor = nir_after_block(last_block);
         generate_discard_break(b, discarded);
      }

      foreach_list_typed(nir_cf_node, nested_node, node, &loop->body)
         lower_discard_flow(b, nested_node, discarded);
      return;
   }
   default:
      unreachable("unknown cf node type");
   }
}

void
gl_nir_lower_discard_flow(nir_shader *shader)
{
   nir_function_impl *main = nir_shader_get_entrypoint(shader);

   nir_variable *discarded = rzalloc(shader, nir_variable);
   discarded->name = ralloc_strdup(discarded, "discarded");
   discarded->type = glsl_bool_type();
   discarded->data.mode = nir_var_shader_temp;

   nir_shader_add_variable(shader, discarded);

   nir_foreach_function_impl(impl, shader) {
      nir_builder b = nir_builder_at(nir_before_impl(impl));

      if (impl == main) {
         nir_deref_instr *deref = nir_build_deref_var(&b, discarded);
         nir_store_deref(&b, deref, nir_imm_bool(&b, false), ~0);
      }

      foreach_list_typed(nir_cf_node, cf_node, node, &impl->body) {
         lower_discard_flow(&b, cf_node, discarded);
      }
   }
}
