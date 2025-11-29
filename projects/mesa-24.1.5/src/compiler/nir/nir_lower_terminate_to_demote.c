/*
 * Copyright © 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "nir.h"
#include "nir_builder.h"

static bool
nir_lower_terminate_cf_list(nir_builder *b, struct exec_list *cf_list)
{
   bool progress = false;

   foreach_list_typed_safe(nir_cf_node, node, node, cf_list) {
      switch (node->type) {
      case nir_cf_node_block: {
         nir_block *block = nir_cf_node_as_block(node);

         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
            switch (intrin->intrinsic) {
            case nir_intrinsic_terminate: {
               /* Everything after the terminate is dead */
               nir_cf_list dead_cf;
               nir_cf_extract(&dead_cf, nir_after_instr(&intrin->instr),
                                        nir_after_cf_list(cf_list));
               nir_cf_delete(&dead_cf);

               intrin->intrinsic = nir_intrinsic_demote;
               b->cursor = nir_after_instr(&intrin->instr);
               nir_jump(b, nir_jump_halt);

               /* We just removed the remainder of this list of CF nodes.
                * It's not safe to continue iterating.
                */
               return true;
            }

            case nir_intrinsic_terminate_if:
               b->cursor = nir_before_instr(&intrin->instr);
               nir_push_if(b, intrin->src[0].ssa);
               {
                  nir_demote(b);
                  nir_jump(b, nir_jump_halt);
               }
               nir_instr_remove(&intrin->instr);
               progress = true;
               break;

            default:
               break;
            }
         }
         break;
      }

      case nir_cf_node_if: {
         nir_if *nif = nir_cf_node_as_if(node);
         progress |= nir_lower_terminate_cf_list(b, &nif->then_list);
         progress |= nir_lower_terminate_cf_list(b, &nif->else_list);
         break;
      }

      case nir_cf_node_loop: {
         nir_loop *loop = nir_cf_node_as_loop(node);
         progress |= nir_lower_terminate_cf_list(b, &loop->body);
         progress |= nir_lower_terminate_cf_list(b, &loop->continue_list);
         break;
      }

      default:
         unreachable("Unknown CF node type");
      }
   }

   return progress;
}

static bool
nir_lower_terminate_impl(nir_function_impl *impl)
{
   nir_builder b = nir_builder_create(impl);
   bool progress = nir_lower_terminate_cf_list(&b, &impl->body);

   if (progress) {
      nir_metadata_preserve(impl, nir_metadata_none);
   } else {
      nir_metadata_preserve(impl, nir_metadata_all);
   }

   return progress;
}

/** Lowers nir_intrinsic_terminate to demote + halt
 *
 * The semantics of nir_intrinsic_terminate require that threads immediately
 * exit.  In SPIR-V, terminate is branch instruction even though it's only an
 * intrinsic in NIR.  This pass lowers terminate to demote + halt.  Since halt
 * is a jump instruction in NIR, this restores those semantics and NIR can
 * reason about dead threads after a halt.  It allows lets back-ends to only
 * implement nir_intrinsic_demote as long as they also implement nir_jump_halt.
 */
bool
nir_lower_terminate_to_demote(nir_shader *nir)
{
   bool progress = false;

   nir_foreach_function_impl(impl, nir) {
      if (nir_lower_terminate_impl(impl))
         progress = true;
   }

   return progress;
}
