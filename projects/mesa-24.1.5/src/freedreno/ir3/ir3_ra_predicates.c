/*
 * Copyright 2024 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#include "ir3.h"
#include "ir3_ra.h"
#include "ir3_shader.h"

/* Represents a def that is currently live. We keep track of both the pre-RA def
 * a register refers to and, in case of spilling and reloading, the def of the
 * reloaded instruction. This allows us to assign reloaded defs to sources and
 * prevents additional reloads.
 */
struct live_def {
   /* The pre-RA def. */
   struct ir3_register *def;

   /* The reloaded def. NULL if def was not reloaded. */
   struct ir3_register *reloaded_def;

   /* Set when used for a src marked first-kill. We cannot immediately free the
    * register because then it might get reused for another src in the same
    * instruction. Instead, we free it after an instruction's sources have been
    * processed.
    */
   bool killed;
};

/* Per-block liveness information. Stores live defs per supported register,
 * indexed by register component.
 */
struct block_liveness {
   /* Live-in defs taken from the intersections the block's predecessors
    * live-out defs.
    */
   struct live_def *live_in_defs;

   /* Currently live defs. Starts from live-in and is updated while processing
    * the instructions in a block. Contains the live-out defs after the whole
    * block has been processed.
    */
   struct live_def *live_defs;
};

struct ra_predicates_ctx {
   struct ir3 *ir;
   unsigned num_regs;
   struct ir3_liveness *liveness;
   struct block_liveness *blocks_liveness;

   /* Number of precolored defs that have not been processed yet. When this
    * drops to zero, we can stop trying to avoid allocating p0.x (the only
    * register currently used for precoloring).
    */
   unsigned outstanding_precolored_defs;
};

static bool
has_free_regs(struct ra_predicates_ctx *ctx, struct block_liveness *live)
{
   for (unsigned i = 0; i < ctx->num_regs; ++i) {
      if (live->live_defs[i].def == NULL)
         return true;
   }

   return false;
}

static bool
try_avoid_comp(struct ra_predicates_ctx *ctx, struct block_liveness *live,
               unsigned comp)
{
   /* Currently, only p0.x is ever used for a precolored register so just try to
    * avoid that one if we have any precolored defs.
    */
   return comp == 0 && ctx->outstanding_precolored_defs > 0;
}

static bool
reg_is_free(struct ra_predicates_ctx *ctx, struct block_liveness *live,
            unsigned comp)
{
   assert(comp < ctx->num_regs);

   return live->live_defs[comp].def == NULL;
}

static unsigned
alloc_reg_comp(struct ra_predicates_ctx *ctx, struct block_liveness *live)
{
   for (unsigned i = 0; i < ctx->num_regs; ++i) {
      if (live->live_defs[i].def == NULL && !try_avoid_comp(ctx, live, i))
         return i;
   }

   for (unsigned i = 0; i < ctx->num_regs; ++i) {
      if (live->live_defs[i].def == NULL)
         return i;
   }

   unreachable("Reg availability should have been checked before");
}

static struct live_def *
assign_reg(struct ra_predicates_ctx *ctx, struct block_liveness *live,
           struct ir3_register *def, struct ir3_register *reloaded_def,
           unsigned comp)
{
   assert(comp < ctx->num_regs);

   struct ir3_register *current_def =
      (reloaded_def == NULL) ? def : reloaded_def;

   current_def->num = regid(REG_P0, comp);

   struct live_def *live_def = &live->live_defs[comp];
   assert((live_def->def == NULL) && (live_def->reloaded_def == NULL));

   live_def->def = def;
   live_def->reloaded_def = reloaded_def;
   return live_def;
}

static struct live_def *
alloc_reg(struct ra_predicates_ctx *ctx, struct block_liveness *live,
          struct ir3_register *def, struct ir3_register *reloaded_def)
{
   /* Try to assign the precolored register if it's free. If not, use normal
    * allocation and reload whenever a precolored source needs it.
    * NOTE: this means we currently only support precolored sources, not dests.
    */
   if (def->num != INVALID_REG) {
      assert(ctx->outstanding_precolored_defs > 0);
      ctx->outstanding_precolored_defs--;
      unsigned comp = reg_comp(def);

      if (reg_is_free(ctx, live, comp))
         return assign_reg(ctx, live, def, reloaded_def, comp);
   }

   unsigned comp = alloc_reg_comp(ctx, live);
   return assign_reg(ctx, live, def, reloaded_def, comp);
}

static void
free_reg(struct ra_predicates_ctx *ctx, struct block_liveness *live,
         struct ir3_register *reg)
{
   assert((reg->flags & IR3_REG_PREDICATE) && (reg->num != INVALID_REG));

   unsigned comp = reg_comp(reg);
   assert(comp < ctx->num_regs);

   struct live_def *reg_live_def = &live->live_defs[comp];
   assert((reg_live_def->def == reg) || (reg_live_def->reloaded_def == reg));

   reg_live_def->def = NULL;
   reg_live_def->reloaded_def = NULL;
   reg_live_def->killed = false;
}

static struct ir3_instruction *
first_non_allocated_use_after(struct ir3_register *def,
                              struct ir3_instruction *after)
{
   uint32_t first_ip = UINT32_MAX;
   struct ir3_instruction *first = NULL;

   foreach_ssa_use (use, def->instr) {
      if (!ir3_block_dominates(after->block, use->block))
         continue;

      /* Do not filter-out after itself. This ensures that if after is a use of
       * def, def will not get selected to get spilled because there must be
       * another register with a further first use. We have to ensure that def
       * doesn't get spilled in this case because otherwise, we might spill a
       * register used by an earlier source of after.
       */
      if (use->ip < after->ip)
         continue;
      if (use->ip >= first_ip)
         continue;

      first_ip = use->ip;
      first = use;
   }

   return first;
}

static bool
is_predicate_use(struct ir3_instruction *instr, unsigned src_n)
{
   if (__is_false_dep(instr, src_n))
      return false;
   return ra_reg_is_predicate(instr->srcs[src_n]);
}

/* Spill a register by simply removing one from the live defs. We don't need to
 * store its value anywhere since it can be rematerialized (see reload). We
 * chose the register whose def's first use is the furthest.
 */
static void
spill(struct ra_predicates_ctx *ctx, struct block_liveness *live,
      struct ir3_instruction *spill_location)
{
   unsigned furthest_first_use = 0;
   unsigned spill_reg = ~0;

   for (unsigned i = 0; i < ctx->num_regs; ++i) {
      struct ir3_register *candidate = live->live_defs[i].def;
      assert(candidate != NULL);

      struct ir3_instruction *first_use =
         first_non_allocated_use_after(candidate, spill_location);

      if (first_use == NULL) {
         spill_reg = i;
         break;
      }

      if (first_use->ip > furthest_first_use) {
         furthest_first_use = first_use->ip;
         spill_reg = i;
      }
   }

   assert(spill_reg != ~0);

   live->live_defs[spill_reg].def = NULL;
   live->live_defs[spill_reg].reloaded_def = NULL;
}

static struct live_def *
find_live_def(struct ra_predicates_ctx *ctx, struct block_liveness *live,
              struct ir3_register *def)
{
   for (unsigned i = 0; i < ctx->num_regs; ++i) {
      struct live_def *live_def = &live->live_defs[i];
      if (live_def->def == def)
         return live_def;
   }

   return NULL;
}

static struct ir3_register *
get_def(struct live_def *live_def)
{
   return live_def->reloaded_def == NULL ? live_def->def
                                         : live_def->reloaded_def;
}

/* Reload a def into s specific register, which must be free. Reloading is
 * implemented by cloning the instruction that produced the def and moving it in
 * front of the use.
 */
static struct live_def *
reload_into(struct ra_predicates_ctx *ctx, struct block_liveness *live,
            struct ir3_register *def, struct ir3_instruction *use,
            unsigned comp)
{
   struct ir3_instruction *reloaded_instr = NULL;
   bool def_is_allocated = !(def->flags & IR3_REG_UNUSED);

   if (!def_is_allocated && use->block == def->instr->block) {
      /* If def has not been allocated a register yet, no source is currently
       * using it. If it's in the same block as the current use, just move it in
       * front of it.
       */
      reloaded_instr = def->instr;
   } else {
      /* If the def is either 1) already allocated or 2) in a different block
       * than the current use, we have to clone it. For 1) because its allocated
       * register isn't currently live (we wouldn't be reloading it otherwise).
       * For 2) because it might have other uses in blocks that aren't
       * successors of the use.
       */
      reloaded_instr = ir3_instr_clone(def->instr);
   }

   reloaded_instr->block = use->block;

   /* Keep track of the original def for validation. */
   reloaded_instr->data = def;

   ir3_instr_move_before(reloaded_instr, use);
   struct ir3_register *reloaded_def = reloaded_instr->dsts[0];
   return assign_reg(ctx, live, def, reloaded_def, comp);
}

/* Reload a def into a register, spilling one if necessary. */
static struct live_def *
reload(struct ra_predicates_ctx *ctx, struct block_liveness *live,
       struct ir3_register *def, struct ir3_instruction *use)
{
   if (!has_free_regs(ctx, live))
      spill(ctx, live, use);

   unsigned comp = alloc_reg_comp(ctx, live);
   return reload_into(ctx, live, def, use, comp);
}

static int
ra_block(struct ra_predicates_ctx *ctx, struct ir3_block *block)
{
   struct block_liveness *live = &ctx->blocks_liveness[block->index];

   foreach_instr (instr, &block->instr_list) {
      /* Assign registers to sources based on their defs. */
      foreach_src (src, instr) {
         if (!ra_reg_is_predicate(src))
            continue;

         struct live_def *live_def = find_live_def(ctx, live, src->def);
         if (src->num != INVALID_REG &&
             (!live_def || get_def(live_def)->num != src->num)) {
            /* If src is precolored and its def is either not live or is live in
             * the wrong register, reload it into the correct one.
             */
            unsigned comp = reg_comp(src);

            if (!reg_is_free(ctx, live, comp))
               free_reg(ctx, live, get_def(&live->live_defs[comp]));
            if (live_def)
               free_reg(ctx, live, get_def(live_def));

            live_def = reload_into(ctx, live, src->def, instr, comp);
         } else if (!live_def) {
            live_def = reload(ctx, live, src->def, instr);
         }

         assert(live_def != NULL);

         struct ir3_register *def = get_def(live_def);

         assert((src->num == INVALID_REG) || (src->num == def->num));
         src->num = def->num;
         src->def = def;

         /* Mark the def as used to make sure we won't move it anymore. */
         def->flags &= ~IR3_REG_UNUSED;

         /* If this source kills the def, don't free the register right away to
          * prevent it being reused for another source of this instruction. We
          * can free it after all sources of this instruction have been
          * processed.
          */
         if (src->flags & IR3_REG_FIRST_KILL)
            live_def->killed = true;
      }

      /* After all sources of an instruction have been processed, we can  free
       * the registers that were killed by a source.
       */
      for (unsigned reg = 0; reg < ctx->num_regs; ++reg) {
         struct live_def *live_def = &live->live_defs[reg];
         if (live_def->def == NULL)
            continue;

         if (live_def->killed)
            free_reg(ctx, live, get_def(live_def));
      }

      /* Allocate registers for new defs. */
      foreach_dst (dst, instr) {
         if (!ra_reg_is_predicate(dst))
            continue;

         /* Mark it as unused until we encounter the first use. This allows us
          * to know when it is legal to move the instruction.
          */
         dst->flags |= IR3_REG_UNUSED;

         /* For validation, we keep track of which def an instruction produces.
          * Normally, this will be the instruction's dst but in case of
          * reloading, it will point to the original instruction's dst.
          */
         dst->instr->data = dst;

         /* If we don't have any free registers, ignore the def for now. If we
          * start spilling right away, we might end-up with a cascade of spills
          * when there are a lot of defs before their first uses.
          */
         if (!has_free_regs(ctx, live))
            continue;

         alloc_reg(ctx, live, dst, NULL);
      }
   }

   /* Process loop back edges. Since we ignore them while calculating a block's
    * live-in defs in init_block_liveness, we now make sure that we satisfy our
    * successor's live-in requirements by producing the correct defs in the
    * required registers.
    */
   for (unsigned i = 0; i < 2; ++i) {
      struct ir3_block *succ = block->successors[i];
      if (!succ)
         continue;

      struct live_def *succ_live_in =
         ctx->blocks_liveness[succ->index].live_in_defs;

      /* If live_in_defs has not been set yet, it's not a back edge. */
      if (!succ_live_in)
         continue;

      for (unsigned reg = 0; reg < ctx->num_regs; ++reg) {
         struct live_def *succ_def = &succ_live_in[reg];
         if (!succ_def->def)
            continue;

         struct live_def *cur_def = &live->live_defs[reg];

         /* Same def in the same register, nothing to be done. */
         if (cur_def->def == succ_def->def)
            continue;

         /* Different def in the same register, free it first. */
         if (cur_def->def)
            free_reg(ctx, live, get_def(cur_def));

         /* Reload the def in the required register right before the block's
          * terminator.
          */
         struct ir3_instruction *use = ir3_block_get_terminator(block);
         reload_into(ctx, live, succ_def->def, use, reg);
      }
   }

   return 0;
}

/* Propagate live-out defs of a block's predecessors to the block's live-in
 * defs. This takes the intersection of all predecessors live-out defs. That is,
 * a def will be live-in if it's live-out in the same register in all
 * predecessors.
 */
static void
init_block_liveness(struct ra_predicates_ctx *ctx, struct ir3_block *block)
{
   struct block_liveness *live = &ctx->blocks_liveness[block->index];
   live->live_defs = rzalloc_array(ctx, struct live_def, ctx->num_regs);
   BITSET_WORD *live_in = ctx->liveness->live_in[block->index];

   for (unsigned i = 0; i < block->predecessors_count; ++i) {
      struct ir3_block *pred = block->predecessors[i];
      assert(pred != NULL);

      struct block_liveness *pred_live = &ctx->blocks_liveness[pred->index];

      /* If the predecessor has not been processed yet it means it's the back
       * edge of a loop. We ignore it now, take the live-out defs of the block's
       * other predecessors, and make sure the live-out defs of the back edge
       * match this block's live-in defs after processing the back edge.
       */
      if (pred_live->live_defs == NULL)
         continue;

      for (unsigned reg = 0; reg < ctx->num_regs; ++reg) {
         struct live_def *cur_def = &live->live_defs[reg];
         struct live_def *pred_def = &pred_live->live_defs[reg];

         if (i == 0 && pred_def->def != NULL) {
            /* If the first predecessor has a def in reg, use it if it's live-in
             * in this block.
             */
            if (BITSET_TEST(live_in, pred_def->def->name))
               *cur_def = *pred_def;
         } else if (cur_def->def != pred_def->def) {
            /* Different predecessors have different live-out defs in reg so we
             * cannot use it as live-in.
             */
            cur_def->def = NULL;
            cur_def->reloaded_def = NULL;
         }
      }
   }

   live->live_in_defs = rzalloc_array(ctx, struct live_def, ctx->num_regs);
   memcpy(live->live_in_defs, live->live_defs,
          sizeof(struct live_def) * ctx->num_regs);
}

static void
precolor_def(struct ra_predicates_ctx *ctx, struct ir3_register *def)
{
   foreach_ssa_use (use, def->instr) {
      foreach_src (src, use) {
         if (src->def != def)
            continue;
         if (src->num == INVALID_REG)
            continue;

         def->num = src->num;
         ctx->outstanding_precolored_defs++;

         /* We can only precolor a def once. */
         return;
      }
   }
}

/* Precolor the defs of precolored sources so that we can try to assign the
 * correct register immediately.
 */
static void
precolor_defs(struct ra_predicates_ctx *ctx)
{
   for (unsigned i = 1; i < ctx->liveness->definitions_count; ++i) {
      struct ir3_register *def = ctx->liveness->definitions[i];
      precolor_def(ctx, def);
   }
}

void
ir3_ra_predicates(struct ir3_shader_variant *v)
{
   struct ra_predicates_ctx *ctx = rzalloc(NULL, struct ra_predicates_ctx);
   ctx->ir = v->ir;
   ctx->num_regs = v->compiler->num_predicates;
   ctx->liveness = ir3_calc_liveness_for(ctx, v->ir, ra_reg_is_predicate,
                                         ra_reg_is_predicate);
   ctx->blocks_liveness =
      rzalloc_array(ctx, struct block_liveness, ctx->liveness->block_count);
   ir3_count_instructions_ra(ctx->ir);
   ir3_find_ssa_uses_for(ctx->ir, ctx, is_predicate_use);
   precolor_defs(ctx);

   foreach_block (block, &v->ir->block_list) {
      init_block_liveness(ctx, block);
      ra_block(ctx, block);
   }

   /* Remove instructions that became unused. This happens when a def was never
    * used directly but only through its reloaded clones.
    * Note that index 0 in the liveness definitions is always NULL.
    */
   for (unsigned i = 1; i < ctx->liveness->definitions_count; ++i) {
      struct ir3_register *def = ctx->liveness->definitions[i];

      if (def->flags & IR3_REG_UNUSED)
         list_delinit(&def->instr->node);
   }

   ralloc_free(ctx);
}
