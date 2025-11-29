/*
 * Copyright (C) 2023 Valve Corporation
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

/* The pass uses information on which branches are divergent in order to
 * determine which blocks are "reconvergence points" where parked threads may
 * become reactivated as well as to add "physical" edges where the machine may
 * fall through to the next reconvergence point. Reconvergence points need a
 * (jp) added in the assembly, and physical edges are needed to model shared
 * register liveness correctly. Reconvergence happens in the following two
 * scenarios:
 *
 * 1. When there is a divergent branch, the later of the two block destinations
 *    becomes a reconvergence point.
 * 2. When a forward edge crosses over a reconvergence point that may be
 *    outstanding at the start of the edge, we need to park the threads that
 *    take the edge and resume execution at the reconvergence point. This means
 *    that there is a physical edge from the start of the edge to the
 *    reconvergence point, and the destination of the edge becomes a new
 *    reconvergence point.
 *
 * For example, consider this simple if-else:
 *
 *    bb0:
 *    ...
 *    br p0.x, #bb1, #bb2
 *    bb1:
 *    ...
 *    jump bb3
 *    bb2:
 *    ...
 *    jump bb3
 *    bb3:
 *    ...
 *
 * The divergent branch at the end of bb0 makes bb2 a reconvergence point
 * following (1), which starts being outstanding after the branch at the end of
 * bb1. The jump to bb3 at the end of bb1 goes over bb2 while it is outstanding,
 * so there is a physical edge from bb1 to bb2 and bb3 is a reconvergence point
 * following (2).
 * 
 * Note that (2) can apply recursively. To handle this efficiently we build an
 * interval tree of forward edges that cross other blocks and whenever a block
 * becomes a RP we iterate through the edges jumping across it using the tree.
 * We also need to keep track of the range where each RP may be
 * "outstanding." A RP becomes outstanding after a branch to it parks its
 * threads there. This range may increase in size as we discover more and more
 * branches to it that may park their threads there.
 *
 * Finally, we need to compute the branchstack value, which is the maximum
 * number of outstanding reconvergence points. For the if-else, the branchstack
 * is 2, because after the jump at the end of bb2 both reconvergence points are
 * outstanding (although the first is removed immediately afterwards). Because
 * we already computed the range where each RP is outstanding, this part is
 * relatively straightforward.
 */

#include <limits.h>

#include "ir3_shader.h"

#include "util/rb_tree.h"
#include "util/u_worklist.h"
#include "util/ralloc.h"

struct logical_edge {
   struct uinterval_node node;
   struct ir3_block *start_block;
   struct ir3_block *end_block;
};

struct block_data {
   /* For a reconvergance point, the index of the first block where, upon
    * exiting, the RP may be outstanding. Normally this is a predecessor but may
    * be a loop header for loops.
    */
   unsigned first_divergent_pred;

   /* The last processed first_divergent_pred. */
   unsigned first_processed_divergent_pred;

   /* The number of blocks that have this block as a first_divergent_pred. */
   unsigned divergence_count;
};

void
ir3_calc_reconvergence(struct ir3_shader_variant *so)
{
   void *mem_ctx = ralloc_context(NULL);

   /* It's important that the index we use corresponds to the final order blocks
    * are emitted in!
    */
   unsigned index = 0;
   foreach_block (block, &so->ir->block_list) {
      block->index = index++;
   }

   /* Setup the tree of edges */
   unsigned edge_count = 0;
   foreach_block (block, &so->ir->block_list) {
      if (block->successors[0])
         edge_count++;
      if (block->successors[1])
         edge_count++;
   }

   struct rb_tree forward_edges, backward_edges;
   rb_tree_init(&forward_edges);
   rb_tree_init(&backward_edges);

   unsigned edge = 0;
   struct logical_edge *edges =
      ralloc_array(mem_ctx, struct logical_edge, edge_count);
   struct block_data *blocks =
      ralloc_array(mem_ctx, struct block_data, index);
   foreach_block (block, &so->ir->block_list) {
      blocks[block->index].divergence_count = 0;
      blocks[block->index].first_divergent_pred = UINT_MAX;
      blocks[block->index].first_processed_divergent_pred = UINT_MAX;
      for (unsigned i = 0; i < ARRAY_SIZE(block->successors); i++) {
         if (block->successors[i]) {
            ir3_block_link_physical(block, block->successors[i]);

            if (block->successors[i]->index > block->index + 1) {
               edges[edge] = (struct logical_edge) {
                  .node = {
                     .interval = {
                        block->index + 1,
                        block->successors[i]->index - 1
                     },
                  },
                  .start_block = block,
                  .end_block = block->successors[i],
               };

               uinterval_tree_insert(&forward_edges, &edges[edge++].node);
            } else if (block->successors[i]->index < block->index - 1) {
               edges[edge] = (struct logical_edge) {
                  .node = {
                     .interval = {
                        block->successors[i]->index - 1,
                        block->index + 1
                     },
                  },
                  .start_block = block->successors[i],
                  .end_block = block,
               };

               uinterval_tree_insert(&backward_edges, &edges[edge++].node);
            }
         }
      }
   }

   assert(edge <= edge_count);

   u_worklist worklist;
   u_worklist_init(&worklist, index, mem_ctx);

   /* First, find and mark divergent branches. The later destination will be the
    * reconvergence point.
    */
   foreach_block (block, &so->ir->block_list) {
      struct ir3_instruction *terminator = ir3_block_get_terminator(block);
      if (!terminator)
         continue;
      if (terminator->opc == OPC_PREDT || terminator->opc == OPC_PREDF)
         continue;
      if (block->successors[0] && block->successors[1] &&
          block->divergent_condition) {
         unsigned idx = block->successors[0]->index >
            block->successors[1]->index ? 0 : 1;
         block->successors[idx]->reconvergence_point = true;
         blocks[block->successors[idx]->index].first_divergent_pred =
            block->index;
         u_worklist_push_tail(&worklist, block->successors[idx], index);
      }
   }

   while (!u_worklist_is_empty(&worklist)) {
      struct ir3_block *block =
         u_worklist_pop_head(&worklist, struct ir3_block, index);
      assert(block->reconvergence_point);

      /* Iterate over all edges stepping over the block. */
      struct uinterval interval = { block->index, block->index };
      struct logical_edge *prev = NULL;
      uinterval_tree_foreach (struct logical_edge, edge, interval, &forward_edges,
                              node) {
         /* If "block" definitely isn't outstanding when the branch
          * corresponding to "edge" is taken, then we don't need to park
          * "edge->end_block" and we can ignore this.
          *
          * TODO: add uinterval_tree_foreach_from() and use that instead.
          */
         if (edge->start_block->index <= blocks[block->index].first_divergent_pred)
            continue;

         /* If we've already processed this edge + RP pair, don't process it
          * again. Because edges are ordered by start point, we must have
          * processed every edge after this too.
          */
         if (edge->start_block->index >
             blocks[block->index].first_processed_divergent_pred)
            break;

         edge->end_block->reconvergence_point = true;
         if (blocks[edge->end_block->index].first_divergent_pred >
             edge->start_block->index) {
            blocks[edge->end_block->index].first_divergent_pred =
               edge->start_block->index;
            u_worklist_push_tail(&worklist, edge->end_block, index);
         }

         /* Backwards branches extend the range of divergence. For example, a
          * divergent break creates a reconvergence point after the loop that
          * stays outstanding throughout subsequent iterations, even at points
          * before the break. This takes that into account.
          *
          * More precisely, a backwards edge that originates between the start
          * and end of "edge" extends the divergence range to the beginning of
          * its destination if it is taken, or alternatively to the end of the
          * block before its destination.
          *
          * TODO: in case we ever start accepting weird non-structured control
          * flow, we may also need to handle this above if a divergent branch
          * crosses over a backwards edge.
          */
         struct uinterval interval2 = { edge->start_block->index, edge->start_block->index };
         uinterval_tree_foreach (struct logical_edge, back_edge, interval2, &backward_edges,
                                 node) {
            if (back_edge->end_block->index < edge->end_block->index) {
               if (blocks[edge->end_block->index].first_divergent_pred >
                   back_edge->start_block->index - 1) {
                  blocks[edge->end_block->index].first_divergent_pred =
                     back_edge->start_block->index - 1;
                  u_worklist_push_tail(&worklist, edge->end_block, index);
               }
            }
         }

         if (!prev || prev->start_block != edge->start_block) {
            /* We should only process this edge + block combination once, and
             * we use the fact that edges are sorted by start point to avoid
             * adding redundant physical edges in case multiple edges have the
             * same start point by comparing with the previous edge. Therefore
             * we should only add the physical edge once.
             */
            for (unsigned i = 0; i < block->physical_predecessors_count; i++)
               assert(block->physical_predecessors[i] != edge->start_block);
            ir3_block_link_physical(edge->start_block, block);
         }
         prev = edge;
      }

      blocks[block->index].first_processed_divergent_pred =
         blocks[block->index].first_divergent_pred;
   }

   /* For each reconvergent point p we have an open range
    * (p->first_divergent_pred, p) where p may be outstanding. We need to keep
    * track of the number of outstanding RPs and calculate the maximum.
    */
   foreach_block (block, &so->ir->block_list) {
      if (block->reconvergence_point) {
         blocks[blocks[block->index].first_divergent_pred].divergence_count++;
      }
   }

   unsigned rc_level = 0;
   so->branchstack = 0;
   foreach_block (block, &so->ir->block_list) {
      if (block->reconvergence_point)
         rc_level--;

      /* Account for lowerings that produce divergent control flow. */
      foreach_instr (instr, &block->instr_list) {
         switch (instr->opc) {
         case OPC_SCAN_MACRO:
            so->branchstack = MAX2(so->branchstack, rc_level + 2);
            break;
         case OPC_BALLOT_MACRO:
         case OPC_READ_COND_MACRO:
         case OPC_ELECT_MACRO:
         case OPC_READ_FIRST_MACRO:
         case OPC_SWZ_SHARED_MACRO:
            so->branchstack = MAX2(so->branchstack, rc_level + 1);
            break;
         default:
            break;
         }
      }

      rc_level += blocks[block->index].divergence_count;

      so->branchstack = MAX2(so->branchstack, rc_level); 
   }
   assert(rc_level == 0);

   ralloc_free(mem_ctx);
}

