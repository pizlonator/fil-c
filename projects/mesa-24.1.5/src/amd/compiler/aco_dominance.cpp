/*
 * Copyright © 2018 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ACO_DOMINANCE_CPP
#define ACO_DOMINANCE_CPP

#include "aco_ir.h"

/*
 * Implements the algorithms for computing the dominator tree from
 * "A Simple, Fast Dominance Algorithm" by Cooper, Harvey, and Kennedy.
 *
 * Different from the paper, our CFG allows to compute the dominator tree
 * in a single pass as it is guaranteed that the dominating predecessors
 * are processed before the current block.
 */

namespace aco {

void
dominator_tree(Program* program)
{
   for (unsigned i = 0; i < program->blocks.size(); i++) {
      Block& block = program->blocks[i];

      /* If this block has no predecessor, it dominates itself by definition */
      if (block.linear_preds.empty()) {
         block.linear_idom = block.index;
         block.logical_idom = block.index;
         continue;
      }

      int new_logical_idom = -1;
      int new_linear_idom = -1;
      for (unsigned pred_idx : block.logical_preds) {
         if ((int)program->blocks[pred_idx].logical_idom == -1)
            continue;

         if (new_logical_idom == -1) {
            new_logical_idom = pred_idx;
            continue;
         }

         while ((int)pred_idx != new_logical_idom) {
            if ((int)pred_idx > new_logical_idom)
               pred_idx = program->blocks[pred_idx].logical_idom;
            if ((int)pred_idx < new_logical_idom)
               new_logical_idom = program->blocks[new_logical_idom].logical_idom;
         }
      }

      for (unsigned pred_idx : block.linear_preds) {
         if ((int)program->blocks[pred_idx].linear_idom == -1)
            continue;

         if (new_linear_idom == -1) {
            new_linear_idom = pred_idx;
            continue;
         }

         while ((int)pred_idx != new_linear_idom) {
            if ((int)pred_idx > new_linear_idom)
               pred_idx = program->blocks[pred_idx].linear_idom;
            if ((int)pred_idx < new_linear_idom)
               new_linear_idom = program->blocks[new_linear_idom].linear_idom;
         }
      }

      block.logical_idom = new_logical_idom;
      block.linear_idom = new_linear_idom;
   }
}

} // namespace aco
#endif
