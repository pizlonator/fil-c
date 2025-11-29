/*
 * Copyright 2014 Intel Corporation
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

/* This implements dominance and post-dominance of the SSA use graph where
 * instructions are vertices and SSA uses are edges (i.e. edges go from
 * each instruction to all its uses). CF nodes are ignored and irrelevant.
 * It's different from nir_dominance.c, but the algorithm is the same, which
 * is from "A Simple, Fast Dominance Algorithm" by Cooper, Harvey, and Kennedy.
 *
 * Definitions:
 * - Instruction A is post-dominated by instruction B if the result of
 *   instruction A and following intermediate results using the result of
 *   instruction A only affect the result of instruction B. Consequently,
 *   if instruction B was removed, instruction A would become dead including
 *   all instructions computing the intermediate results.
 *   Example: A(load) -> ... -> B(ALU)
 *   Note: This is the foundation of inter-shader code motion from later
 *   shaders to earlier shaders.
 * - Instruction B is dominated by instruction A if all use paths from
 *   all loads to instruction B must go through instruction A.
 *   Note: Unlike post-dominance, dominance is unusable as-is because
 *   the immediate dominator typically doesn't exist if there are non-unary
 *   opcodes (i.e. branches of an expression tree following source operands
 *   don't usually converge to a single instruction unless all instructions
 *   are unary). The solution is to ignore loads like load_const to allow
 *   non-unary opcodes, which would be the foundation of inter-shader code
 *   motion from earlier shaders to later shaders, such as 2 output stores
 *   having only 1 ALU instruction as their only source at the beginning,
 *   ignoring constant and uniform operands along the way.
 *
 * Interesting cases implied by this (post-)dominator tree:
 * - load_const, loads without src operands, and undef are not dominated by
 *   anything because they don't have any src operands.
 * - No instruction post-dominates store intrinsics (and all other intrinsics
 *   without a destination) and nir_if nodes (they use a value but don't
 *   produce any).
 *
 * Typical application:
 * - The immediate post-dominator query returns the solution to the problem of
 *   how much code we can move into the previous shader or preamble without
 *   increasing the number of inputs. Example of an SSA-use graph and
 *   the possible result that a user of this utility can produce:
 *
 *          input0 input1             input0 input1
 *              \   / \                  |      \
 *    constant   alu  ...    ------>     |     ...
 *           \   /
 *            alu
 * (immediate post-dominator of input0)
 *
 * Examples of possible applications:
 * - Moving load_input+ALU to the previous shader: An immediate post-dominator
 *   of load_input and all instructions between load_input and the immediate
 *   post-dominator are a candidate for being moved into the previous shader
 *   and we only need to check if the post-dominator is movable. Repeat
 *   the immediate post-dominator query on the accepted post-dominator and see
 *   if that is also movable. Repeat that until you find the farthest post-
 *   dominator that is movable.
 * - Moving load_uniform+ALU to a preamble shader or the CPU: An immediate
 *   post-dominator of load_uniform is a candidate for being moved into
 *   the preamble shader or the CPU. Repeat the immediate post-dominator query
 *   until you find the farthest post-dominator that is movable.
 * - Replacing a value used to compute 2 shader outputs by only 1 output, and
 *   moving the computation into the next shader:
 *   The Lowest Common Ancestor of 2 output stores within the dominator tree
 *   is a candidate for the new replacement output. Any loads that are
 *   trivially movable such as load_const should be ignored by this utility,
 *   otherwise the Lowest Common Ancestor wouldn't exist.
 *
 * Queries:
 * - get the immediate dominator of an instruction
 * - get the Lowest Common Ancestor of 2 instructions
 * - whether one instruction dominates another
 *
 * Implemenation details:
 * - Since some instructions are not dominated by anything, a dummy root is
 *   added into the graph that dominates such instructions, which is required
 *   by the algorithm.
 *
 * TODO: only post-dominance implemented, not dominance
 */

#include "nir.h"

struct nir_use_dom_node {
   nir_instr *instr;
   uint32_t index;

   /* The index of this node's immediate dominator in the dominator tree.
    * The dummy root points it to itself. -1 == unset.
    */
   int32_t imm_dom;
};

struct nir_use_dominance_state {
   nir_function_impl *impl;
   struct nir_use_dom_node *dom_nodes;
   unsigned num_dom_nodes;
};

static struct nir_use_dom_node *
get_node(struct nir_use_dominance_state *state, nir_instr *instr)
{
   return &state->dom_nodes[instr->index];
}

static struct nir_use_dom_node *
get_imm_dom(struct nir_use_dominance_state *state,
            struct nir_use_dom_node *node)
{
   assert(node->imm_dom != -1);
   return &state->dom_nodes[node->imm_dom];
}

static bool
init_instr(struct nir_use_dominance_state *state, nir_instr *instr,
           unsigned *index)
{
   assert(*index < state->num_dom_nodes);
   struct nir_use_dom_node *node = &state->dom_nodes[*index];

   if (*index == 0) {
      /* dummy root */
      node->imm_dom = 0;
   } else {
      node->imm_dom = -1;
      node->instr = instr;
      instr->index = node->index = *index;
   }
   (*index)++;

   return true;
}

static struct nir_use_dom_node *
intersect(struct nir_use_dominance_state *state, struct nir_use_dom_node *i1,
          struct nir_use_dom_node *i2)
{
   while (i1 != i2) {
      /* Note, the comparisons here are the opposite of what the paper says
       * because we index instrs from beginning -> end (i.e. reverse
       * post-order) instead of post-order like they assume.
       */
      while (i1->index > i2->index)
         i1 = get_imm_dom(state, i1);
      while (i2->index > i1->index)
         i2 = get_imm_dom(state, i2);
   }

   return i1;
}

static void
update_imm_dom(struct nir_use_dominance_state *state,
               struct nir_use_dom_node *pred,
               struct nir_use_dom_node **new_idom)
{
   if (pred->imm_dom != -1) {
      if (*new_idom)
         *new_idom = intersect(state, pred, *new_idom);
      else
         *new_idom = pred;
   }
}

static bool
calc_dominance(struct nir_use_dominance_state *state,
               struct nir_use_dom_node *node, bool post_dominance)
{
   struct nir_use_dom_node *new_idom = NULL;

   if (post_dominance) {
      nir_def *def = nir_instr_def(node->instr);
      bool has_use = false;

      /* Intrinsics that can't be reordered will get the root node as
       * the post-dominator.
       */
      if (def &&
          (node->instr->type != nir_instr_type_intrinsic ||
           nir_intrinsic_can_reorder(nir_instr_as_intrinsic(node->instr)))) {
         nir_foreach_use_including_if(src, def) {
            has_use = true;

            /* Ifs are treated like stores because they don't produce
             * a value. dom_nodes[0] is the dummy root.
             */
            if (nir_src_is_if(src)) {
               update_imm_dom(state, &state->dom_nodes[0], &new_idom);
               /* Short-cut because we can't come back from the root node. */
               break;
            } else {
               update_imm_dom(state,
                              get_node(state, nir_src_parent_instr(src)),
                              &new_idom);
            }
         }
      }

      /* No destination (e.g. stores, atomics with an unused result, discard,
       * dead instructions). dom_nodes[0] is the dummy root.
       */
      if (!has_use)
         update_imm_dom(state, &state->dom_nodes[0], &new_idom);
   } else {
      unreachable("TODO: only post-dominance implemented, not dominance");
   }

   if (new_idom && node->imm_dom != new_idom->index) {
      node->imm_dom = new_idom->index;
      return true;
   }

   return false;
}

/**
 * Calculate dominance or post-dominance of the SSA use graph.
 * The returned state must not be freed while dominance queries are being used.
 * ralloc_free() frees the state.
 *
 * It clobbers nir_instr::index, which can't be changed while dominance queries
 * are being used.
 *
 * \param impl             NIR function
 * \param post_dominance   Whether to compute post-dominance or dominance.
 */
struct nir_use_dominance_state *
nir_calc_use_dominance_impl(nir_function_impl *impl, bool post_dominance)
{
   struct nir_use_dominance_state *state =
      rzalloc(NULL, struct nir_use_dominance_state);
   if (!state)
      return NULL;

   unsigned num_dom_nodes = 1; /* including the dummy root */
   nir_foreach_block(block, impl) {
      num_dom_nodes += exec_list_length(&block->instr_list);
   }

   state->impl = impl;
   state->num_dom_nodes = num_dom_nodes;
   state->dom_nodes = rzalloc_array(state, struct nir_use_dom_node,
                                    num_dom_nodes);
   if (!state->dom_nodes) {
      ralloc_free(state);
      return NULL;
   }

   unsigned index = 0;

   /* We need a dummy root node because there are instructions such as
    * load_const that aren't dominated by anything. If we are calculating
    * post-dominance, intrinsics without a destination aren't post-dominated
    * by anything. However, the algorithm requires a common (post-)dominator.
    */
   init_instr(state, NULL, &index);

   /* Post-dominance is identical to dominance, but instructions are added
    * in the opposite order.
    */
   if (post_dominance) {
      nir_foreach_block_reverse(block, impl) {
         nir_foreach_instr_reverse(instr, block) {
            init_instr(state, instr, &index);
         }
      }
   } else {
      nir_foreach_block(block, impl) {
         nir_foreach_instr(instr, block) {
            init_instr(state, instr, &index);
         }
      }
   }

   bool progress = true;
   while (progress) {
      progress = false;

      /* Skip the dummy root (iterate from 1). */
      for (unsigned i = 1; i < num_dom_nodes; i++) {
         progress |= calc_dominance(state, &state->dom_nodes[i],
                                    post_dominance);
      }
   }

   return state;
}

nir_instr *
nir_get_immediate_use_dominator(struct nir_use_dominance_state *state,
                                nir_instr *instr)
{
   struct nir_use_dom_node *node = get_node(state, instr);

   return get_imm_dom(state, node)->instr;
}

/**
 * Computes the least common ancestor of two instructions.
 */
nir_instr *
nir_use_dominance_lca(struct nir_use_dominance_state *state,
                      nir_instr *i1, nir_instr *i2)
{
   assert(i1 && i2);
   struct nir_use_dom_node *lca = intersect(state, get_node(state, i1),
                                            get_node(state, i2));
   assert(lca);
   /* Might be NULL in case of the dummy root. */
   return lca->instr;
}

/**
 * Returns true if the parent dominates the child in the SSA use graph
 * described at the beginning.
 */
bool
nir_instr_dominates_use(struct nir_use_dominance_state *state,
                        nir_instr *parent_instr, nir_instr *child_instr)
{
   struct nir_use_dom_node *parent = get_node(state, parent_instr);
   struct nir_use_dom_node *child = get_node(state, child_instr);

   while (parent->index < child->index)
      child = get_imm_dom(state, child);

   return parent == child;
}

static void
print_instr(struct nir_use_dom_node *node)
{
   if (!node)
      printf("NULL - bug");
   else if (node->index == 0)
      printf("dummy_root");
   else
      nir_print_instr(node->instr, stdout);
}

void
nir_print_use_dominators(struct nir_use_dominance_state *state,
                         nir_instr **instructions, unsigned num_instructions)
{
   for (unsigned i = 0; i < num_instructions; i++) {
      printf("Input idom(\"");
      nir_print_instr(instructions[i], stdout);
      printf("\") = \"");
      print_instr(get_imm_dom(state, get_node(state, instructions[i])));
      printf("\"\n");
   }
   puts("");

   nir_foreach_block(block, state->impl) {
      nir_foreach_instr(instr, block) {
         printf("idom(\"");
         nir_print_instr(instr, stdout);
         printf("\") = \"");
         print_instr(get_imm_dom(state, get_node(state, instr)));
         printf("\"\n");
      }
   }
   puts("");

   for (unsigned i = 0; i < num_instructions; i++) {
      for (unsigned j = i + 1; j < num_instructions; j++) {
         printf("LCA input 1: ");
         nir_print_instr(instructions[i], stdout);
         printf("\nLCA input 2: ");
         nir_print_instr(instructions[j], stdout);
         puts("");
         nir_instr *lca =
            nir_use_dominance_lca(state, instructions[i], instructions[j]);

         if (lca) {
            printf("2 inputs have a common post-dominator: ");
            nir_print_instr(lca, stdout);
            printf("\n");
         }
         puts("");
      }
   }
}
