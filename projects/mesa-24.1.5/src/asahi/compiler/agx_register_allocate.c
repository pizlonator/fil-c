/*
 * Copyright 2021 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#include "util/bitset.h"
#include "util/macros.h"
#include "util/u_dynarray.h"
#include "util/u_memory.h"
#include "util/u_qsort.h"
#include "agx_builder.h"
#include "agx_compile.h"
#include "agx_compiler.h"
#include "agx_debug.h"
#include "agx_opcodes.h"
#include "shader_enums.h"

/* SSA-based register allocator */

enum ra_class {
   /* General purpose register */
   RA_GPR,

   /* Memory, used to assign stack slots */
   RA_MEM,

   /* Keep last */
   RA_CLASSES,
};

static inline enum ra_class
ra_class_for_index(agx_index idx)
{
   return idx.memory ? RA_MEM : RA_GPR;
}

struct phi_web_node {
   /* Parent index, or circular for root */
   uint32_t parent;

   /* If root, assigned register, or ~0 if no register assigned. */
   uint16_t reg;
   bool assigned;

   /* Rank, at most log2(n) so need ~5-bits */
   uint8_t rank;
};
static_assert(sizeof(struct phi_web_node) == 8, "packed");

static unsigned
phi_web_find(struct phi_web_node *web, unsigned x)
{
   if (web[x].parent == x) {
      /* Root */
      return x;
   } else {
      /* Search up the tree */
      unsigned root = x;
      while (web[root].parent != root)
         root = web[root].parent;

      /* Compress path. Second pass ensures O(1) memory usage. */
      while (web[x].parent != x) {
         unsigned temp = web[x].parent;
         web[x].parent = root;
         x = temp;
      }

      return root;
   }
}

static void
phi_web_union(struct phi_web_node *web, unsigned x, unsigned y)
{
   x = phi_web_find(web, x);
   y = phi_web_find(web, y);

   if (x == y)
      return;

   /* Union-by-rank: ensure x.rank >= y.rank */
   if (web[x].rank < web[y].rank) {
      unsigned temp = x;
      x = y;
      y = temp;
   }

   web[y].parent = x;

   /* Increment rank if necessary */
   if (web[x].rank == web[y].rank) {
      web[x].rank++;
   }
}

struct ra_ctx {
   agx_context *shader;
   agx_block *block;
   agx_instr *instr;
   uint16_t *ssa_to_reg;
   uint8_t *ncomps;
   enum agx_size *sizes;
   enum ra_class *classes;
   BITSET_WORD *visited;
   BITSET_WORD *used_regs[RA_CLASSES];

   /* Maintained while assigning registers */
   unsigned *max_reg[RA_CLASSES];

   /* For affinities */
   agx_instr **src_to_collect_phi;
   struct phi_web_node *phi_web;

   /* If bit i of used_regs is set, and register i is the first consecutive
    * register holding an SSA value, then reg_to_ssa[i] is the SSA index of the
    * value currently in register  i.
    *
    * Only for GPRs. We can add reg classes later if we have a use case.
    */
   uint32_t reg_to_ssa[AGX_NUM_REGS];

   /* Maximum number of registers that RA is allowed to use */
   unsigned bound[RA_CLASSES];
};

enum agx_size
agx_split_width(const agx_instr *I)
{
   enum agx_size width = ~0;

   agx_foreach_dest(I, d) {
      if (I->dest[d].type == AGX_INDEX_NULL)
         continue;
      else if (width != ~0)
         assert(width == I->dest[d].size);
      else
         width = I->dest[d].size;
   }

   assert(width != ~0 && "should have been DCE'd");
   return width;
}

/*
 * Calculate register demand in 16-bit registers, while gathering widths and
 * classes. Becuase we allocate in SSA, this calculation is exact in
 * linear-time. Depends on liveness information.
 */
static unsigned
agx_calc_register_demand(agx_context *ctx)
{
   uint8_t *widths = calloc(ctx->alloc, sizeof(uint8_t));
   enum ra_class *classes = calloc(ctx->alloc, sizeof(enum ra_class));

   agx_foreach_instr_global(ctx, I) {
      agx_foreach_ssa_dest(I, d) {
         unsigned v = I->dest[d].value;
         assert(widths[v] == 0 && "broken SSA");
         /* Round up vectors for easier live range splitting */
         widths[v] = util_next_power_of_two(agx_index_size_16(I->dest[d]));
         classes[v] = ra_class_for_index(I->dest[d]);
      }
   }

   /* Calculate demand at the start of each block based on live-in, then update
    * for each instruction processed. Calculate rolling maximum.
    */
   unsigned max_demand = 0;

   agx_foreach_block(ctx, block) {
      unsigned demand = 0;

      /* RA treats the nesting counter as alive throughout if control flow is
       * used anywhere. This could be optimized.
       */
      if (ctx->any_cf)
         demand++;

      /* Everything live-in */
      {
         int i;
         BITSET_FOREACH_SET(i, block->live_in, ctx->alloc) {
            if (classes[i] == RA_GPR)
               demand += widths[i];
         }
      }

      max_demand = MAX2(demand, max_demand);

      /* To handle non-power-of-two vectors, sometimes live range splitting
       * needs extra registers for 1 instruction. This counter tracks the number
       * of registers to be freed after 1 extra instruction.
       */
      unsigned late_kill_count = 0;

      agx_foreach_instr_in_block(block, I) {
         /* Phis happen in parallel and are already accounted for in the live-in
          * set, just skip them so we don't double count.
          */
         if (I->op == AGX_OPCODE_PHI)
            continue;

         if (I->op == AGX_OPCODE_PRELOAD) {
            unsigned size = agx_size_align_16(I->src[0].size);
            max_demand = MAX2(max_demand, I->src[0].value + size);
         } else if (I->op == AGX_OPCODE_EXPORT) {
            unsigned size = agx_size_align_16(I->src[0].size);
            max_demand = MAX2(max_demand, I->imm + size);
         }

         /* Handle late-kill registers from last instruction */
         demand -= late_kill_count;
         late_kill_count = 0;

         /* Kill sources the first time we see them */
         agx_foreach_src(I, s) {
            if (!I->src[s].kill)
               continue;
            assert(I->src[s].type == AGX_INDEX_NORMAL);
            if (ra_class_for_index(I->src[s]) != RA_GPR)
               continue;

            bool skip = false;

            for (unsigned backwards = 0; backwards < s; ++backwards) {
               if (agx_is_equiv(I->src[backwards], I->src[s])) {
                  skip = true;
                  break;
               }
            }

            if (!skip)
               demand -= widths[I->src[s].value];
         }

         /* Make destinations live */
         agx_foreach_ssa_dest(I, d) {
            if (ra_class_for_index(I->dest[d]) != RA_GPR)
               continue;

            /* Live range splits allocate at power-of-two granularity. Round up
             * destination sizes (temporarily) to powers-of-two.
             */
            unsigned real_width = widths[I->dest[d].value];
            unsigned pot_width = util_next_power_of_two(real_width);

            demand += pot_width;
            late_kill_count += (pot_width - real_width);
         }

         max_demand = MAX2(demand, max_demand);
      }

      demand -= late_kill_count;
   }

   free(widths);
   free(classes);
   return max_demand;
}

static bool
find_regs_simple(struct ra_ctx *rctx, enum ra_class cls, unsigned count,
                 unsigned align, unsigned *out)
{
   for (unsigned reg = 0; reg + count <= rctx->bound[cls]; reg += align) {
      if (!BITSET_TEST_RANGE(rctx->used_regs[cls], reg, reg + count - 1)) {
         *out = reg;
         return true;
      }
   }

   return false;
}

/*
 * Search the register file for the best contiguous aligned region of the given
 * size to evict when shuffling registers. The region must not contain any
 * register marked in the passed bitset.
 *
 * As a hint, this also takes in the set of registers from killed sources passed
 * to this instruction. These should be deprioritized, since they are more
 * expensive to use (extra moves to shuffle the contents away).
 *
 * Precondition: such a region exists.
 *
 * Postcondition: at least one register in the returned region is already free.
 */
static unsigned
find_best_region_to_evict(struct ra_ctx *rctx, enum ra_class cls, unsigned size,
                          BITSET_WORD *already_evicted, BITSET_WORD *killed)
{
   assert(util_is_power_of_two_or_zero(size) && "precondition");
   assert((rctx->bound[cls] % size) == 0 &&
          "register file size must be aligned to the maximum vector size");
   assert(cls == RA_GPR);

   unsigned best_base = ~0;
   unsigned best_moves = ~0;

   for (unsigned base = 0; base + size <= rctx->bound[cls]; base += size) {
      /* r0l is unevictable, skip it. By itself, this does not pose a problem.
       * We are allocating n registers, but the region containing r0l has at
       * most n-1 free. Since there are at least n free registers total, there
       * is at least 1 free register outside this region. Thus the region
       * containing that free register contains at most n-1 occupied registers.
       * In the worst case, those n-1 occupied registers are moved to the region
       * with r0l and then the n free registers are used for the destination.
       * Thus, we do not need extra registers to handle "single point"
       * unevictability.
       */
      if (base == 0 && rctx->shader->any_cf)
         continue;

      /* Do not evict the same register multiple times. It's not necessary since
       * we're just shuffling, there are enough free registers elsewhere.
       */
      if (BITSET_TEST_RANGE(already_evicted, base, base + size - 1))
         continue;

      /* Estimate the number of moves required if we pick this region */
      unsigned moves = 0;
      bool any_free = false;

      for (unsigned reg = base; reg < base + size; ++reg) {
         /* We need a move for each blocked register (TODO: we only need a
          * single move for 32-bit pairs, could optimize to use that instead.)
          */
         if (BITSET_TEST(rctx->used_regs[cls], reg))
            moves++;
         else
            any_free = true;

         /* Each clobbered killed register requires a move or a swap. Since
          * swaps require more instructions, assign a higher cost here. In
          * practice, 3 is too high but 2 is slightly better than 1.
          */
         if (BITSET_TEST(killed, reg))
            moves += 2;
      }

      /* Pick the region requiring fewest moves as a heuristic. Regions with no
       * free registers are skipped even if the heuristic estimates a lower cost
       * (due to killed sources), since the recursive splitting algorithm
       * requires at least one free register.
       */
      if (any_free && moves < best_moves) {
         best_moves = moves;
         best_base = base;
      }
   }

   assert(best_base < rctx->bound[cls] &&
          "not enough registers (should have spilled already)");
   return best_base;
}

static void
set_ssa_to_reg(struct ra_ctx *rctx, unsigned ssa, unsigned reg)
{
   enum ra_class cls = rctx->classes[ssa];

   *(rctx->max_reg[cls]) =
      MAX2(*(rctx->max_reg[cls]), reg + rctx->ncomps[ssa] - 1);

   rctx->ssa_to_reg[ssa] = reg;
}

static unsigned
assign_regs_by_copying(struct ra_ctx *rctx, unsigned npot_count, unsigned align,
                       const agx_instr *I, struct util_dynarray *copies,
                       BITSET_WORD *clobbered, BITSET_WORD *killed,
                       enum ra_class cls)
{
   assert(cls == RA_GPR);

   /* Expand the destination to the next power-of-two size. This simplifies
    * splitting and is accounted for by the demand calculation, so is legal.
    */
   unsigned count = util_next_power_of_two(npot_count);
   assert(align <= count && "still aligned");
   align = count;

   /* There's not enough contiguous room in the register file. We need to
    * shuffle some variables around. Look for a range of the register file
    * that is partially blocked.
    */
   unsigned base =
      find_best_region_to_evict(rctx, cls, count, clobbered, killed);

   assert(count <= 16 && "max allocation size (conservative)");
   BITSET_DECLARE(evict_set, 16) = {0};

   /* Store the set of blocking registers that need to be evicted */
   for (unsigned i = 0; i < count; ++i) {
      if (BITSET_TEST(rctx->used_regs[cls], base + i)) {
         BITSET_SET(evict_set, i);
      }
   }

   /* We are going to allocate the destination to this range, so it is now fully
    * used. Mark it as such so we don't reassign here later.
    */
   BITSET_SET_RANGE(rctx->used_regs[cls], base, base + count - 1);

   /* Before overwriting the range, we need to evict blocked variables */
   for (unsigned i = 0; i < 16; ++i) {
      /* Look for subranges that needs eviction */
      if (!BITSET_TEST(evict_set, i))
         continue;

      unsigned reg = base + i;
      uint32_t ssa = rctx->reg_to_ssa[reg];
      uint32_t nr = rctx->ncomps[ssa];
      unsigned align = agx_size_align_16(rctx->sizes[ssa]);

      assert(nr >= 1 && "must be assigned");
      assert(rctx->ssa_to_reg[ssa] == reg &&
             "variable must start within the range, since vectors are limited");

      for (unsigned j = 0; j < nr; ++j) {
         assert(BITSET_TEST(evict_set, i + j) &&
                "variable is allocated contiguous and vectors are limited, "
                "so evicted in full");
      }

      /* Assign a new location for the variable. This terminates with finite
       * recursion because nr is decreasing because of the gap.
       */
      assert(nr < count && "fully contained in range that's not full");
      unsigned new_reg = assign_regs_by_copying(rctx, nr, align, I, copies,
                                                clobbered, killed, cls);

      /* Copy the variable over, register by register */
      for (unsigned i = 0; i < nr; i += align) {
         assert(cls == RA_GPR);

         struct agx_copy copy = {
            .dest = new_reg + i,
            .src = agx_register(reg + i, rctx->sizes[ssa]),
         };

         assert((copy.dest % agx_size_align_16(rctx->sizes[ssa])) == 0 &&
                "new dest must be aligned");
         assert((copy.src.value % agx_size_align_16(rctx->sizes[ssa])) == 0 &&
                "src must be aligned");
         util_dynarray_append(copies, struct agx_copy, copy);
      }

      /* Mark down the set of clobbered registers, so that killed sources may be
       * handled correctly later.
       */
      BITSET_SET_RANGE(clobbered, new_reg, new_reg + nr - 1);

      /* Update bookkeeping for this variable */
      assert(cls == rctx->classes[cls]);
      set_ssa_to_reg(rctx, ssa, new_reg);
      rctx->reg_to_ssa[new_reg] = ssa;

      /* Skip to the next variable */
      i += nr - 1;
   }

   /* We overallocated for non-power-of-two vectors. Free up the excess now.
    * This is modelled as late kill in demand calculation.
    */
   if (npot_count != count) {
      BITSET_CLEAR_RANGE(rctx->used_regs[cls], base + npot_count,
                         base + count - 1);
   }

   return base;
}

static int
sort_by_size(const void *a_, const void *b_, void *sizes_)
{
   const enum agx_size *sizes = sizes_;
   const unsigned *a = a_, *b = b_;

   return sizes[*b] - sizes[*a];
}

/*
 * Allocating a destination of n consecutive registers may require moving those
 * registers' contents to the locations of killed sources. For the instruction
 * to read the correct values, the killed sources themselves need to be moved to
 * the space where the destination will go.
 *
 * This is legal because there is no interference between the killed source and
 * the destination. This is always possible because, after this insertion, the
 * destination needs to contain the killed sources already overlapping with the
 * destination (size k) plus the killed sources clobbered to make room for
 * livethrough sources overlapping with the destination (at most size |dest|-k),
 * so the total size is at most k + |dest| - k = |dest| and so fits in the dest.
 * Sorting by alignment may be necessary.
 */
static void
insert_copies_for_clobbered_killed(struct ra_ctx *rctx, unsigned reg,
                                   unsigned count, const agx_instr *I,
                                   struct util_dynarray *copies,
                                   BITSET_WORD *clobbered)
{
   unsigned vars[16] = {0};
   unsigned nr_vars = 0;

   /* Precondition: the nesting counter is not overwritten. Therefore we do not
    * have to move it.  find_best_region_to_evict knows better than to try.
    */
   assert(!(reg == 0 && rctx->shader->any_cf) && "r0l is never moved");

   /* Consider the destination clobbered for the purpose of source collection.
    * This way, killed sources already in the destination will be preserved
    * (though possibly compacted).
    */
   BITSET_SET_RANGE(clobbered, reg, reg + count - 1);

   /* Collect killed clobbered sources, if any */
   agx_foreach_ssa_src(I, s) {
      unsigned reg = rctx->ssa_to_reg[I->src[s].value];

      if (I->src[s].kill && ra_class_for_index(I->src[s]) == RA_GPR &&
          BITSET_TEST(clobbered, reg)) {

         assert(nr_vars < ARRAY_SIZE(vars) &&
                "cannot clobber more than max variable size");

         vars[nr_vars++] = I->src[s].value;
      }
   }

   if (nr_vars == 0)
      return;

   assert(I->op != AGX_OPCODE_PHI && "kill bit not set for phis");

   /* Sort by descending alignment so they are packed with natural alignment */
   util_qsort_r(vars, nr_vars, sizeof(vars[0]), sort_by_size, rctx->sizes);

   /* Reassign in the destination region */
   unsigned base = reg;

   /* We align vectors to their sizes, so this assertion holds as long as no
    * instruction has a source whose scalar size is greater than the entire size
    * of the vector destination. Yet the killed source must fit within this
    * destination, so the destination must be bigger and therefore have bigger
    * alignment.
    */
   assert((base % agx_size_align_16(rctx->sizes[vars[0]])) == 0 &&
          "destination alignment >= largest killed source alignment");

   for (unsigned i = 0; i < nr_vars; ++i) {
      unsigned var = vars[i];
      unsigned var_base = rctx->ssa_to_reg[var];
      unsigned var_count = rctx->ncomps[var];
      unsigned var_align = agx_size_align_16(rctx->sizes[var]);

      assert(rctx->classes[var] == RA_GPR && "construction");
      assert((base % var_align) == 0 && "induction");
      assert((var_count % var_align) == 0 && "no partial variables");

      for (unsigned j = 0; j < var_count; j += var_align) {
         struct agx_copy copy = {
            .dest = base + j,
            .src = agx_register(var_base + j, rctx->sizes[var]),
         };

         util_dynarray_append(copies, struct agx_copy, copy);
      }

      set_ssa_to_reg(rctx, var, base);
      rctx->reg_to_ssa[base] = var;

      base += var_count;
   }

   assert(base <= reg + count && "no overflow");
}

/*
 * When shuffling registers to assign a phi destination, we can't simply insert
 * the required moves before the phi, since phis happen in parallel along the
 * edge. Instead, there are two cases:
 *
 * 1. The source of the copy is the destination of a phi. Since we are
 *    emitting shuffle code, there will be no more reads of that destination
 *    with the old register. Since the phis all happen in parallel and writes
 *    precede reads, there was no previous read of that destination either. So
 *    the old destination is dead. Just replace the phi's destination with the
 *    moves's destination instead.
 *
 * 2. Otherwise, the source of the copy is a live-in value, since it's
 *    live when assigning phis at the start of a block but it is not a phi.
 *    If we move in parallel with the phi, the phi will still read the correct
 *    old register regardless and the destinations can't alias. So, insert a phi
 *    to do the copy in parallel along the incoming edges.
 */
static void
agx_emit_move_before_phi(agx_context *ctx, agx_block *block,
                         struct agx_copy *copy)
{
   assert(!copy->dest_mem && !copy->src.memory && "no memory shuffles");

   /* Look for the phi writing the destination */
   agx_foreach_phi_in_block(block, phi) {
      if (agx_is_equiv(phi->dest[0], copy->src) && !phi->dest[0].memory) {
         phi->dest[0].value = copy->dest;
         return;
      }
   }

   /* There wasn't such a phi, so it's live-in. Insert a phi instead. */
   agx_builder b = agx_init_builder(ctx, agx_before_block(block));

   agx_instr *phi = agx_phi_to(&b, agx_register_like(copy->dest, copy->src),
                               agx_num_predecessors(block));

   agx_foreach_src(phi, s) {
      phi->src[s] = copy->src;
   }
}

static unsigned
find_regs(struct ra_ctx *rctx, agx_instr *I, unsigned dest_idx, unsigned count,
          unsigned align)
{
   unsigned reg;
   assert(count == align);

   enum ra_class cls = ra_class_for_index(I->dest[dest_idx]);

   if (find_regs_simple(rctx, cls, count, align, &reg)) {
      return reg;
   } else {
      assert(cls == RA_GPR && "no memory live range splits");

      BITSET_DECLARE(clobbered, AGX_NUM_REGS) = {0};
      BITSET_DECLARE(killed, AGX_NUM_REGS) = {0};
      struct util_dynarray copies = {0};
      util_dynarray_init(&copies, NULL);

      /* Initialize the set of registers killed by this instructions' sources */
      agx_foreach_ssa_src(I, s) {
         unsigned v = I->src[s].value;

         if (BITSET_TEST(rctx->visited, v) && !I->src[s].memory) {
            unsigned base = rctx->ssa_to_reg[v];
            unsigned nr = rctx->ncomps[v];

            assert(base + nr <= AGX_NUM_REGS);
            BITSET_SET_RANGE(killed, base, base + nr - 1);
         }
      }

      reg = assign_regs_by_copying(rctx, count, align, I, &copies, clobbered,
                                   killed, cls);
      insert_copies_for_clobbered_killed(rctx, reg, count, I, &copies,
                                         clobbered);

      /* Insert the necessary copies. Phis need special handling since we can't
       * insert instructions before the phi.
       */
      if (I->op == AGX_OPCODE_PHI) {
         util_dynarray_foreach(&copies, struct agx_copy, copy) {
            agx_emit_move_before_phi(rctx->shader, rctx->block, copy);
         }
      } else {
         agx_builder b = agx_init_builder(rctx->shader, agx_before_instr(I));
         agx_emit_parallel_copies(
            &b, copies.data,
            util_dynarray_num_elements(&copies, struct agx_copy));
      }

      util_dynarray_fini(&copies);

      /* assign_regs asserts this is cleared, so clear to be reassigned */
      BITSET_CLEAR_RANGE(rctx->used_regs[cls], reg, reg + count - 1);
      return reg;
   }
}

static uint32_t
search_ssa_to_reg_out(struct ra_ctx *ctx, struct agx_block *blk,
                      enum ra_class cls, unsigned ssa)
{
   for (unsigned reg = 0; reg < ctx->bound[cls]; ++reg) {
      if (blk->reg_to_ssa_out[cls][reg] == ssa)
         return reg;
   }

   unreachable("variable not defined in block");
}

/*
 * Loop over live-in values at the start of the block and mark their registers
 * as in-use. We process blocks in dominance order, so this handles everything
 * but loop headers.
 *
 * For loop headers, this handles the forward edges but not the back edge.
 * However, that's okay: we don't want to reserve the registers that are
 * defined within the loop, because then we'd get a contradiction. Instead we
 * leave them available and then they become fixed points of a sort.
 */
static void
reserve_live_in(struct ra_ctx *rctx)
{
   /* If there are no predecessors, there is nothing live-in */
   unsigned nr_preds = agx_num_predecessors(rctx->block);
   if (nr_preds == 0)
      return;

   agx_builder b =
      agx_init_builder(rctx->shader, agx_before_block(rctx->block));

   int i;
   BITSET_FOREACH_SET(i, rctx->block->live_in, rctx->shader->alloc) {
      /* Skip values defined in loops when processing the loop header */
      if (!BITSET_TEST(rctx->visited, i))
         continue;

      unsigned base;
      enum ra_class cls = rctx->classes[i];

      /* If we split live ranges, the variable might be defined differently at
       * the end of each predecessor. Join them together with a phi inserted at
       * the start of the block.
       */
      if (nr_preds > 1) {
         /* We'll fill in the destination after, to coalesce one of the moves */
         agx_instr *phi = agx_phi_to(&b, agx_null(), nr_preds);
         enum agx_size size = rctx->sizes[i];

         agx_foreach_predecessor(rctx->block, pred) {
            unsigned pred_idx = agx_predecessor_index(rctx->block, *pred);

            if ((*pred)->reg_to_ssa_out[cls] == NULL) {
               /* If this is a loop header, we don't know where the register
                * will end up. So, we create a phi conservatively but don't fill
                * it in until the end of the loop. Stash in the information
                * we'll need to fill in the real register later.
                */
               assert(rctx->block->loop_header);
               phi->src[pred_idx] = agx_get_index(i, size);
               phi->src[pred_idx].memory = rctx->classes[i] == RA_MEM;
            } else {
               /* Otherwise, we can build the phi now */
               unsigned reg = search_ssa_to_reg_out(rctx, *pred, cls, i);
               phi->src[pred_idx] = cls == RA_MEM
                                       ? agx_memory_register(reg, size)
                                       : agx_register(reg, size);
            }
         }

         /* Pick the phi destination to coalesce a move. Predecessor ordering is
          * stable, so this means all live-in values get their registers from a
          * particular predecessor. That means that such a register allocation
          * is valid here, because it was valid in the predecessor.
          */
         phi->dest[0] = phi->src[0];
         base = phi->dest[0].value;
      } else {
         /* If we don't emit a phi, there is already a unique register */
         assert(nr_preds == 1);

         agx_block **pred = util_dynarray_begin(&rctx->block->predecessors);
         /* TODO: Flip logic to eliminate the search */
         base = search_ssa_to_reg_out(rctx, *pred, cls, i);
      }

      set_ssa_to_reg(rctx, i, base);

      for (unsigned j = 0; j < rctx->ncomps[i]; ++j) {
         BITSET_SET(rctx->used_regs[cls], base + j);

         if (cls == RA_GPR)
            rctx->reg_to_ssa[base + j] = i;
      }
   }
}

static void
assign_regs(struct ra_ctx *rctx, agx_index v, unsigned reg)
{
   enum ra_class cls = ra_class_for_index(v);
   assert(reg < rctx->bound[cls] && "must not overflow register file");
   assert(v.type == AGX_INDEX_NORMAL && "only SSA gets registers allocated");
   set_ssa_to_reg(rctx, v.value, reg);

   assert(!BITSET_TEST(rctx->visited, v.value) && "SSA violated");
   BITSET_SET(rctx->visited, v.value);

   assert(rctx->ncomps[v.value] >= 1);
   unsigned end = reg + rctx->ncomps[v.value] - 1;

   assert(!BITSET_TEST_RANGE(rctx->used_regs[cls], reg, end) &&
          "no interference");
   BITSET_SET_RANGE(rctx->used_regs[cls], reg, end);

   if (cls == RA_GPR)
      rctx->reg_to_ssa[reg] = v.value;

   /* Phi webs need to remember which register they're assigned to */
   struct phi_web_node *node =
      &rctx->phi_web[phi_web_find(rctx->phi_web, v.value)];

   if (!node->assigned) {
      node->reg = reg;
      node->assigned = true;
   }
}

static void
agx_set_sources(struct ra_ctx *rctx, agx_instr *I)
{
   assert(I->op != AGX_OPCODE_PHI);

   agx_foreach_ssa_src(I, s) {
      assert(BITSET_TEST(rctx->visited, I->src[s].value) && "no phis");

      unsigned v = rctx->ssa_to_reg[I->src[s].value];
      agx_replace_src(I, s, agx_register_like(v, I->src[s]));
   }
}

static void
agx_set_dests(struct ra_ctx *rctx, agx_instr *I)
{
   agx_foreach_ssa_dest(I, s) {
      unsigned v = rctx->ssa_to_reg[I->dest[s].value];
      I->dest[s] =
         agx_replace_index(I->dest[s], agx_register_like(v, I->dest[s]));
   }
}

static unsigned
affinity_base_of_collect(struct ra_ctx *rctx, agx_instr *collect, unsigned src)
{
   unsigned src_reg = rctx->ssa_to_reg[collect->src[src].value];
   unsigned src_offset = src * agx_size_align_16(collect->src[src].size);

   if (src_reg >= src_offset)
      return src_reg - src_offset;
   else
      return ~0;
}

static bool
try_coalesce_with(struct ra_ctx *rctx, agx_index ssa, unsigned count,
                  bool may_be_unvisited, unsigned *out)
{
   assert(ssa.type == AGX_INDEX_NORMAL);
   if (!BITSET_TEST(rctx->visited, ssa.value)) {
      assert(may_be_unvisited);
      return false;
   }

   unsigned base = rctx->ssa_to_reg[ssa.value];
   enum ra_class cls = ra_class_for_index(ssa);

   if (BITSET_TEST_RANGE(rctx->used_regs[cls], base, base + count - 1))
      return false;

   assert(base + count <= rctx->bound[cls] && "invariant");
   *out = base;
   return true;
}

static unsigned
pick_regs(struct ra_ctx *rctx, agx_instr *I, unsigned d)
{
   agx_index idx = I->dest[d];
   enum ra_class cls = ra_class_for_index(idx);
   assert(idx.type == AGX_INDEX_NORMAL);

   unsigned count = rctx->ncomps[idx.value];
   assert(count >= 1);

   unsigned align = count;

   /* Try to allocate entire phi webs compatibly */
   unsigned phi_idx = phi_web_find(rctx->phi_web, idx.value);
   if (rctx->phi_web[phi_idx].assigned) {
      unsigned reg = rctx->phi_web[phi_idx].reg;
      if ((reg % align) == 0 && reg + align < rctx->bound[cls] &&
          !BITSET_TEST_RANGE(rctx->used_regs[cls], reg, reg + align - 1))
         return reg;
   }

   /* Try to allocate moves compatibly with their sources */
   if (I->op == AGX_OPCODE_MOV && I->src[0].type == AGX_INDEX_NORMAL &&
       I->src[0].memory == I->dest[0].memory &&
       I->src[0].size == I->dest[0].size) {

      unsigned out;
      if (try_coalesce_with(rctx, I->src[0], count, false, &out))
         return out;
   }

   /* Try to allocate phis compatibly with their sources */
   if (I->op == AGX_OPCODE_PHI) {
      agx_foreach_ssa_src(I, s) {
         /* Loop headers have phis with a source preceding the definition */
         bool may_be_unvisited = rctx->block->loop_header;

         unsigned out;
         if (try_coalesce_with(rctx, I->src[s], count, may_be_unvisited, &out))
            return out;
      }
   }

   /* Try to allocate collects compatibly with their sources */
   if (I->op == AGX_OPCODE_COLLECT) {
      agx_foreach_ssa_src(I, s) {
         assert(BITSET_TEST(rctx->visited, I->src[s].value) &&
                "registers assigned in an order compatible with dominance "
                "and this is not a phi node, so we have assigned a register");

         unsigned base = affinity_base_of_collect(rctx, I, s);
         if (base >= rctx->bound[cls] || (base + count) > rctx->bound[cls])
            continue;

         /* Unaligned destinations can happen when dest size > src size */
         if (base % align)
            continue;

         if (!BITSET_TEST_RANGE(rctx->used_regs[cls], base, base + count - 1))
            return base;
      }
   }

   /* Try to coalesce scalar exports */
   agx_instr *collect_phi = rctx->src_to_collect_phi[idx.value];
   if (collect_phi && collect_phi->op == AGX_OPCODE_EXPORT) {
      unsigned reg = collect_phi->imm;

      if (!BITSET_TEST_RANGE(rctx->used_regs[cls], reg, reg + align - 1) &&
          (reg % align) == 0)
         return reg;
   }

   /* Try to coalesce vector exports */
   if (collect_phi && collect_phi->op == AGX_OPCODE_SPLIT) {
      if (collect_phi->dest[0].type == AGX_INDEX_NORMAL) {
         agx_instr *exp = rctx->src_to_collect_phi[collect_phi->dest[0].value];
         if (exp && exp->op == AGX_OPCODE_EXPORT) {
            unsigned reg = exp->imm;

            if (!BITSET_TEST_RANGE(rctx->used_regs[cls], reg,
                                   reg + align - 1) &&
                (reg % align) == 0)
               return reg;
         }
      }
   }

   /* Try to allocate sources of collects contiguously */
   if (collect_phi && collect_phi->op == AGX_OPCODE_COLLECT) {
      agx_instr *collect = collect_phi;

      assert(count == align && "collect sources are scalar");

      /* Find our offset in the collect. If our source is repeated in the
       * collect, this may not be unique. We arbitrarily choose the first.
       */
      unsigned our_source = ~0;
      agx_foreach_ssa_src(collect, s) {
         if (agx_is_equiv(collect->src[s], idx)) {
            our_source = s;
            break;
         }
      }

      assert(our_source < collect->nr_srcs && "source must be in the collect");

      /* See if we can allocate compatibly with any source of the collect */
      agx_foreach_ssa_src(collect, s) {
         if (!BITSET_TEST(rctx->visited, collect->src[s].value))
            continue;

         /* Determine where the collect should start relative to the source */
         unsigned base = affinity_base_of_collect(rctx, collect, s);
         if (base >= rctx->bound[cls])
            continue;

         unsigned our_reg = base + (our_source * align);

         /* Don't allocate past the end of the register file */
         if ((our_reg + align) > rctx->bound[cls])
            continue;

         /* If those registers are free, then choose them */
         if (!BITSET_TEST_RANGE(rctx->used_regs[cls], our_reg,
                                our_reg + align - 1))
            return our_reg;
      }

      unsigned collect_align = rctx->ncomps[collect->dest[0].value];
      unsigned offset = our_source * align;

      /* Prefer ranges of the register file that leave room for all sources of
       * the collect contiguously.
       */
      for (unsigned base = 0;
           base + (collect->nr_srcs * align) <= rctx->bound[cls];
           base += collect_align) {
         if (!BITSET_TEST_RANGE(rctx->used_regs[cls], base,
                                base + (collect->nr_srcs * align) - 1))
            return base + offset;
      }

      /* Try to respect the alignment requirement of the collect destination,
       * which may be greater than the sources (e.g. pack_64_2x32_split). Look
       * for a register for the source such that the collect base is aligned.
       */
      if (collect_align > align) {
         for (unsigned reg = offset; reg + collect_align <= rctx->bound[cls];
              reg += collect_align) {
            if (!BITSET_TEST_RANGE(rctx->used_regs[cls], reg, reg + count - 1))
               return reg;
         }
      }
   }

   /* Try to allocate phi sources compatibly with their phis */
   if (collect_phi && collect_phi->op == AGX_OPCODE_PHI) {
      agx_instr *phi = collect_phi;
      unsigned out;

      agx_foreach_ssa_src(phi, s) {
         if (try_coalesce_with(rctx, phi->src[s], count, true, &out))
            return out;
      }

      /* If we're in a loop, we may have already allocated the phi. Try that. */
      if (phi->dest[0].type == AGX_INDEX_REGISTER) {
         unsigned base = phi->dest[0].value;

         if (base + count <= rctx->bound[cls] &&
             !BITSET_TEST_RANGE(rctx->used_regs[cls], base, base + count - 1))
            return base;
      }
   }

   /* Default to any contiguous sequence of registers */
   return find_regs(rctx, I, d, count, align);
}

/** Assign registers to SSA values in a block. */

static void
agx_ra_assign_local(struct ra_ctx *rctx)
{
   BITSET_DECLARE(used_regs_gpr, AGX_NUM_REGS) = {0};
   BITSET_DECLARE(used_regs_mem, AGX_NUM_MODELED_REGS) = {0};
   uint16_t *ssa_to_reg = calloc(rctx->shader->alloc, sizeof(uint16_t));

   agx_block *block = rctx->block;
   uint8_t *ncomps = rctx->ncomps;
   rctx->used_regs[RA_GPR] = used_regs_gpr;
   rctx->used_regs[RA_MEM] = used_regs_mem;
   rctx->ssa_to_reg = ssa_to_reg;

   reserve_live_in(rctx);

   /* Force the nesting counter r0l live throughout shaders using control flow.
    * This could be optimized (sync with agx_calc_register_demand).
    */
   if (rctx->shader->any_cf)
      BITSET_SET(used_regs_gpr, 0);

   agx_foreach_instr_in_block(block, I) {
      rctx->instr = I;

      /* Optimization: if a split contains the last use of a vector, the split
       * can be removed by assigning the destinations overlapping the source.
       */
      if (I->op == AGX_OPCODE_SPLIT && I->src[0].kill) {
         assert(ra_class_for_index(I->src[0]) == RA_GPR);
         unsigned reg = ssa_to_reg[I->src[0].value];
         unsigned width = agx_size_align_16(agx_split_width(I));

         agx_foreach_dest(I, d) {
            assert(ra_class_for_index(I->dest[0]) == RA_GPR);

            /* Free up the source */
            unsigned offset_reg = reg + (d * width);
            BITSET_CLEAR_RANGE(used_regs_gpr, offset_reg,
                               offset_reg + width - 1);

            /* Assign the destination where the source was */
            if (!agx_is_null(I->dest[d]))
               assign_regs(rctx, I->dest[d], offset_reg);
         }

         unsigned excess =
            rctx->ncomps[I->src[0].value] - (I->nr_dests * width);
         if (excess) {
            BITSET_CLEAR_RANGE(used_regs_gpr, reg + (I->nr_dests * width),
                               reg + rctx->ncomps[I->src[0].value] - 1);
         }

         agx_set_sources(rctx, I);
         agx_set_dests(rctx, I);
         continue;
      } else if (I->op == AGX_OPCODE_PRELOAD) {
         /* We must coalesce all preload moves */
         assert(I->dest[0].size == I->src[0].size);
         assert(I->src[0].type == AGX_INDEX_REGISTER);

         assign_regs(rctx, I->dest[0], I->src[0].value);
         agx_set_dests(rctx, I);
         continue;
      }

      /* First, free killed sources */
      agx_foreach_ssa_src(I, s) {
         if (I->src[s].kill) {
            assert(I->op != AGX_OPCODE_PHI && "phis don't use .kill");

            enum ra_class cls = ra_class_for_index(I->src[s]);
            unsigned reg = ssa_to_reg[I->src[s].value];
            unsigned count = ncomps[I->src[s].value];

            assert(count >= 1);
            BITSET_CLEAR_RANGE(rctx->used_regs[cls], reg, reg + count - 1);
         }
      }

      /* Next, assign destinations one at a time. This is always legal
       * because of the SSA form.
       */
      agx_foreach_ssa_dest(I, d) {
         assign_regs(rctx, I->dest[d], pick_regs(rctx, I, d));
      }

      /* Phi sources are special. Set in the corresponding predecessors */
      if (I->op != AGX_OPCODE_PHI)
         agx_set_sources(rctx, I);

      agx_set_dests(rctx, I);
   }

   for (unsigned i = 0; i < RA_CLASSES; ++i) {
      block->reg_to_ssa_out[i] =
         malloc(rctx->bound[i] * sizeof(*block->reg_to_ssa_out[i]));

      /* Initialize with sentinel so we don't have unused regs mapping to r0 */
      memset(block->reg_to_ssa_out[i], 0xFF,
             rctx->bound[i] * sizeof(*block->reg_to_ssa_out[i]));
   }

   int i;
   BITSET_FOREACH_SET(i, block->live_out, rctx->shader->alloc) {
      block->reg_to_ssa_out[rctx->classes[i]][rctx->ssa_to_reg[i]] = i;
   }

   /* Also set the sources for the phis in our successors, since that logically
    * happens now (given the possibility of live range splits, etc)
    */
   agx_foreach_successor(block, succ) {
      unsigned pred_idx = agx_predecessor_index(succ, block);

      agx_foreach_phi_in_block(succ, phi) {
         if (phi->src[pred_idx].type == AGX_INDEX_NORMAL) {
            /* This source needs a fixup */
            unsigned value = phi->src[pred_idx].value;

            agx_replace_src(
               phi, pred_idx,
               agx_register_like(rctx->ssa_to_reg[value], phi->src[pred_idx]));
         }
      }
   }

   free(rctx->ssa_to_reg);
}

/*
 * Lower phis to parallel copies at the logical end of a given block. If a block
 * needs parallel copies inserted, a successor of the block has a phi node. To
 * have a (nontrivial) phi node, a block must have multiple predecessors. So the
 * edge from the block to the successor (with phi) is not the only edge entering
 * the successor. Because the control flow graph has no critical edges, this
 * edge must therefore be the only edge leaving the block, so the block must
 * have only a single successor.
 */
static void
agx_insert_parallel_copies(agx_context *ctx, agx_block *block)
{
   bool any_succ = false;
   unsigned nr_phi = 0;

   /* Phi nodes logically happen on the control flow edge, so parallel copies
    * are added at the end of the predecessor */
   agx_builder b = agx_init_builder(ctx, agx_after_block_logical(block));

   agx_foreach_successor(block, succ) {
      assert(nr_phi == 0 && "control flow graph has a critical edge");

      agx_foreach_phi_in_block(succ, phi) {
         assert(!any_succ && "control flow graph has a critical edge");
         nr_phi += agx_channels(phi->dest[0]);
      }

      any_succ = true;

      /* Nothing to do if there are no phi nodes */
      if (nr_phi == 0)
         continue;

      unsigned pred_index = agx_predecessor_index(succ, block);

      /* Create a parallel copy lowering all the phi nodes */
      struct agx_copy *copies = calloc(sizeof(*copies), nr_phi);

      unsigned i = 0;

      agx_foreach_phi_in_block(succ, phi) {
         agx_index dest = phi->dest[0];
         agx_index src = phi->src[pred_index];

         if (src.type == AGX_INDEX_IMMEDIATE)
            src.size = dest.size;

         assert(dest.type == AGX_INDEX_REGISTER);
         assert(dest.size == src.size);

         /* Scalarize the phi, since the parallel copy lowering doesn't handle
          * vector phis. While we scalarize phis in NIR, we can generate vector
          * phis from spilling so must take care.
          */
         for (unsigned c = 0; c < agx_channels(phi->dest[0]); ++c) {
            agx_index src_ = src;
            unsigned offs = c * agx_size_align_16(src.size);

            if (src.type != AGX_INDEX_IMMEDIATE) {
               assert(src.type == AGX_INDEX_UNIFORM ||
                      src.type == AGX_INDEX_REGISTER);
               src_.value += offs;
               src_.channels_m1 = 1 - 1;
            }

            assert(i < nr_phi);
            copies[i++] = (struct agx_copy){
               .dest = dest.value + offs,
               .dest_mem = dest.memory,
               .src = src_,
            };
         }
      }

      agx_emit_parallel_copies(&b, copies, nr_phi);

      free(copies);
   }
}

static void
lower_exports(agx_context *ctx)
{
   struct agx_copy copies[AGX_NUM_REGS];
   unsigned nr = 0;
   agx_block *block = agx_exit_block(ctx);

   agx_foreach_instr_in_block_safe(block, I) {
      if (I->op != AGX_OPCODE_EXPORT)
         continue;

      assert(agx_channels(I->src[0]) == 1 && "scalarized in frontend");
      assert(nr < ARRAY_SIZE(copies));

      copies[nr++] = (struct agx_copy){
         .dest = I->imm,
         .src = I->src[0],
      };

      /* We cannot use fewer registers than we export */
      ctx->max_reg =
         MAX2(ctx->max_reg, I->imm + agx_size_align_16(I->src[0].size));
   }

   agx_builder b = agx_init_builder(ctx, agx_after_block_logical(block));
   agx_emit_parallel_copies(&b, copies, nr);
}

void
agx_ra(agx_context *ctx)
{
   bool force_spilling =
      (agx_compiler_debug & AGX_DBG_SPILL) && ctx->key->has_scratch;

   /* Determine maximum possible registers. We won't exceed this! */
   unsigned max_possible_regs = AGX_NUM_REGS;

   /* Compute shaders need to have their entire workgroup together, so our
    * register usage is bounded by the workgroup size.
    */
   if (gl_shader_stage_is_compute(ctx->stage)) {
      unsigned threads_per_workgroup;

      /* If we don't know the workgroup size, worst case it. TODO: Optimize
       * this, since it'll decimate opencl perf.
       */
      if (ctx->nir->info.workgroup_size_variable) {
         threads_per_workgroup = 1024;
      } else {
         threads_per_workgroup = ctx->nir->info.workgroup_size[0] *
                                 ctx->nir->info.workgroup_size[1] *
                                 ctx->nir->info.workgroup_size[2];
      }

      max_possible_regs =
         agx_max_registers_for_occupancy(threads_per_workgroup);
   }

   /* The helper program is unspillable and has a limited register file */
   if (force_spilling)
      max_possible_regs = 32;
   else if (ctx->key->is_helper)
      max_possible_regs = 32;

   /* Calculate the demand. We'll use it to determine if we need to spill and to
    * bound register assignment.
    */
   agx_compute_liveness(ctx);
   unsigned effective_demand = agx_calc_register_demand(ctx);
   bool spilling = (effective_demand > max_possible_regs);

   if (spilling) {
      assert(ctx->key->has_scratch && "internal shaders are unspillable");
      agx_spill(ctx, max_possible_regs);

      /* After spilling, recalculate liveness and demand */
      agx_compute_liveness(ctx);
      effective_demand = agx_calc_register_demand(ctx);

      /* The resulting program can now be assigned registers */
      assert(effective_demand <= max_possible_regs && "spiller post-condition");
   }

   /* Record all phi webs. First initialize the union-find data structure with
    * all SSA defs in their own singletons, then union together anything related
    * by a phi. The resulting union-find structure will be the webs.
    */
   struct phi_web_node *phi_web = calloc(ctx->alloc, sizeof(*phi_web));
   for (unsigned i = 0; i < ctx->alloc; ++i) {
      phi_web[i].parent = i;
   }

   agx_foreach_block(ctx, block) {
      agx_foreach_phi_in_block(block, phi) {
         agx_foreach_ssa_src(phi, s) {
            phi_web_union(phi_web, phi->dest[0].value, phi->src[s].value);
         }
      }
   }

   uint8_t *ncomps = calloc(ctx->alloc, sizeof(uint8_t));
   enum ra_class *classes = calloc(ctx->alloc, sizeof(enum ra_class));
   agx_instr **src_to_collect_phi = calloc(ctx->alloc, sizeof(agx_instr *));
   enum agx_size *sizes = calloc(ctx->alloc, sizeof(enum agx_size));
   BITSET_WORD *visited = calloc(BITSET_WORDS(ctx->alloc), sizeof(BITSET_WORD));
   unsigned max_ncomps = 1;

   agx_foreach_instr_global(ctx, I) {
      /* Record collects/phis so we can coalesce when assigning */
      if (I->op == AGX_OPCODE_COLLECT || I->op == AGX_OPCODE_PHI ||
          I->op == AGX_OPCODE_EXPORT || I->op == AGX_OPCODE_SPLIT) {
         agx_foreach_ssa_src(I, s) {
            src_to_collect_phi[I->src[s].value] = I;
         }
      }

      agx_foreach_ssa_dest(I, d) {
         unsigned v = I->dest[d].value;
         assert(ncomps[v] == 0 && "broken SSA");
         /* Round up vectors for easier live range splitting */
         ncomps[v] = util_next_power_of_two(agx_index_size_16(I->dest[d]));
         sizes[v] = I->dest[d].size;
         classes[v] = ra_class_for_index(I->dest[d]);

         max_ncomps = MAX2(max_ncomps, ncomps[v]);
      }
   }

   /* For live range splitting to work properly, ensure the register file is
    * aligned to the larger vector size. Most of the time, this is a no-op since
    * the largest vector size is usually 128-bit and the register file is
    * naturally 128-bit aligned. However, this is required for correctness with
    * 3D textureGrad, which can have a source vector of length 6x32-bit,
    * rounding up to 256-bit and requiring special accounting here.
    */
   unsigned reg_file_alignment = MAX2(max_ncomps, 8);
   assert(util_is_power_of_two_nonzero(reg_file_alignment));

   unsigned demand = ALIGN_POT(effective_demand, reg_file_alignment);
   assert(demand <= max_possible_regs && "Invariant");

   /* Round up the demand to the maximum number of registers we can use without
    * affecting occupancy. This reduces live range splitting.
    */
   unsigned max_regs = agx_occupancy_for_register_count(demand).max_registers;
   if (ctx->key->is_helper || force_spilling)
      max_regs = max_possible_regs;

   max_regs = ROUND_DOWN_TO(max_regs, reg_file_alignment);

   /* Or, we can bound tightly for debugging */
   if (agx_compiler_debug & AGX_DBG_DEMAND)
      max_regs = ALIGN_POT(MAX2(demand, 12), reg_file_alignment);

   /* ...but not too tightly */
   assert((max_regs % reg_file_alignment) == 0 && "occupancy limits aligned");
   assert(max_regs >= (6 * 2) && "space for vertex shader preloading");
   assert(max_regs <= max_possible_regs);

   unsigned max_mem_slot = 0;

   /* Assign registers in dominance-order. This coincides with source-order due
    * to a NIR invariant, so we do not need special handling for this.
    */
   agx_foreach_block(ctx, block) {
      agx_ra_assign_local(&(struct ra_ctx){
         .shader = ctx,
         .block = block,
         .src_to_collect_phi = src_to_collect_phi,
         .phi_web = phi_web,
         .ncomps = ncomps,
         .sizes = sizes,
         .classes = classes,
         .visited = visited,
         .bound[RA_GPR] = max_regs,
         .bound[RA_MEM] = AGX_NUM_MODELED_REGS,
         .max_reg[RA_GPR] = &ctx->max_reg,
         .max_reg[RA_MEM] = &max_mem_slot,
      });
   }

   if (spilling) {
      ctx->spill_base = ctx->scratch_size;
      ctx->scratch_size += (max_mem_slot + 1) * 2;
   }

   /* Vertex shaders preload the vertex/instance IDs (r5, r6) even if the shader
    * don't use them. Account for that so the preload doesn't clobber GPRs.
    */
   if (ctx->nir->info.stage == MESA_SHADER_VERTEX)
      ctx->max_reg = MAX2(ctx->max_reg, 6 * 2);

   assert(ctx->max_reg <= max_regs);

   agx_foreach_instr_global_safe(ctx, ins) {
      /* Lower away RA pseudo-instructions */
      agx_builder b = agx_init_builder(ctx, agx_after_instr(ins));

      if (ins->op == AGX_OPCODE_COLLECT) {
         assert(ins->dest[0].type == AGX_INDEX_REGISTER);
         assert(!ins->dest[0].memory);

         unsigned base = ins->dest[0].value;
         unsigned width = agx_size_align_16(ins->src[0].size);

         struct agx_copy *copies = alloca(sizeof(copies[0]) * ins->nr_srcs);
         unsigned n = 0;

         /* Move the sources */
         agx_foreach_src(ins, i) {
            if (agx_is_null(ins->src[i]) || ins->src[i].type == AGX_INDEX_UNDEF)
               continue;
            assert(ins->src[i].size == ins->src[0].size);

            assert(n < ins->nr_srcs);
            copies[n++] = (struct agx_copy){
               .dest = base + (i * width),
               .src = ins->src[i],
            };
         }

         agx_emit_parallel_copies(&b, copies, n);
         agx_remove_instruction(ins);
         continue;
      } else if (ins->op == AGX_OPCODE_SPLIT) {
         assert(ins->src[0].type == AGX_INDEX_REGISTER ||
                ins->src[0].type == AGX_INDEX_UNIFORM);

         struct agx_copy copies[4];
         assert(ins->nr_dests <= ARRAY_SIZE(copies));

         unsigned n = 0;
         unsigned width = agx_size_align_16(agx_split_width(ins));

         /* Move the sources */
         agx_foreach_dest(ins, i) {
            if (ins->dest[i].type != AGX_INDEX_REGISTER)
               continue;

            assert(!ins->dest[i].memory);

            agx_index src = ins->src[0];
            src.size = ins->dest[i].size;
            src.channels_m1 = 0;
            src.value += (i * width);

            assert(n < ARRAY_SIZE(copies));
            copies[n++] = (struct agx_copy){
               .dest = ins->dest[i].value,
               .src = src,
            };
         }

         /* Lower away */
         agx_builder b = agx_init_builder(ctx, agx_after_instr(ins));
         agx_emit_parallel_copies(&b, copies, n);
         agx_remove_instruction(ins);
         continue;
      }
   }

   /* Insert parallel copies lowering phi nodes and exports */
   agx_foreach_block(ctx, block) {
      agx_insert_parallel_copies(ctx, block);
   }

   lower_exports(ctx);

   agx_foreach_instr_global_safe(ctx, I) {
      switch (I->op) {
      /* Pseudoinstructions for RA must be removed now */
      case AGX_OPCODE_PHI:
      case AGX_OPCODE_PRELOAD:
         agx_remove_instruction(I);
         break;

      /* Coalesced moves can be removed */
      case AGX_OPCODE_MOV:
         if (I->src[0].type == AGX_INDEX_REGISTER &&
             I->dest[0].size == I->src[0].size &&
             I->src[0].value == I->dest[0].value &&
             I->src[0].memory == I->dest[0].memory) {

            assert(I->dest[0].type == AGX_INDEX_REGISTER);
            agx_remove_instruction(I);
         }
         break;

      default:
         break;
      }
   }

   if (spilling)
      agx_lower_spill(ctx);

   agx_foreach_block(ctx, block) {
      for (unsigned i = 0; i < ARRAY_SIZE(block->reg_to_ssa_out); ++i) {
         free(block->reg_to_ssa_out[i]);
         block->reg_to_ssa_out[i] = NULL;
      }
   }

   free(phi_web);
   free(src_to_collect_phi);
   free(ncomps);
   free(sizes);
   free(classes);
   free(visited);
}
