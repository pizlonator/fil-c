/*
 * Copyright © 2024 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#include "ir3.h"
#include "ir3_shader.h"

/* This pass tries to optimize away cmps.s.ne instructions created by
 * ir3_get_predicate in order to write predicates. It does two things:
 *  - Look through chains of multiple cmps.s.ne instructions and remove all but
 *    the first.
 *  - If the source of the cmps.s.ne can write directly to predicates (true for
 *    bitops on a6xx+), remove the cmps.s.ne.
 *
 * In both cases, no instructions are actually removed but clones are made and
 * we rely on DCE to remove anything that became unused. Note that it's fine to
 * always make a clone since even in the case that the original instruction is
 * also used for non-predicate sources (so it won't be DCE'd), we replaced a
 * cmps.ne.s with another instruction so this pass should never increase
 * instruction count.
 */

struct opt_predicates_ctx {
   struct ir3 *ir;

   /* Map from instructions to their clones with a predicate destination. Used
    * to prevent instructions being cloned multiple times.
    */
   struct hash_table *predicate_clones;
};

static struct ir3_instruction *
clone_with_predicate_dst(struct opt_predicates_ctx *ctx,
                         struct ir3_instruction *instr)
{
   struct hash_entry *entry =
      _mesa_hash_table_search(ctx->predicate_clones, instr);
   if (entry)
      return entry->data;

   assert(instr->dsts_count == 1);
   assert(!(instr->dsts[0]->flags & IR3_REG_SHARED));

   struct ir3_instruction *clone = ir3_instr_clone(instr);
   ir3_instr_move_after(clone, instr);
   clone->dsts[0]->flags |= IR3_REG_PREDICATE;
   clone->dsts[0]->flags &= ~IR3_REG_HALF;
   _mesa_hash_table_insert(ctx->predicate_clones, instr, clone);
   return clone;
}

static bool
can_write_predicate(struct opt_predicates_ctx *ctx,
                    struct ir3_instruction *instr)
{
   switch (instr->opc) {
   case OPC_CMPS_S:
   case OPC_CMPS_U:
   case OPC_CMPS_F:
      return true;
   case OPC_AND_B:
   case OPC_OR_B:
   case OPC_NOT_B:
   case OPC_XOR_B:
   case OPC_GETBIT_B:
      return ctx->ir->compiler->bitops_can_write_predicates;
   default:
      return false;
   }
}

/* Detects the pattern used by ir3_get_predicate to write a predicate register:
 * cmps.s.ne pssa_x, ssa_y, 0
 */
static bool
is_gpr_to_predicate_mov(struct ir3_instruction *instr)
{
   return (instr->opc == OPC_CMPS_S) &&
          (instr->cat2.condition == IR3_COND_NE) &&
          (instr->srcs[0]->flags & IR3_REG_SSA) &&
          (instr->srcs[1]->flags & IR3_REG_IMMED) &&
          (instr->srcs[1]->iim_val == 0);
}

/* Look through a chain of cmps.s.ne 0 instructions to find the initial source.
 * Return it if it can write to predicates. Otherwise, return the first
 * cmps.s.ne in the chain.
 */
static struct ir3_register *
resolve_predicate_def(struct opt_predicates_ctx *ctx, struct ir3_register *src)
{
   struct ir3_register *def = src->def;

   while (is_gpr_to_predicate_mov(def->instr)) {
      struct ir3_register *next_def = def->instr->srcs[0]->def;

      if (!can_write_predicate(ctx, next_def->instr))
         return def;

      def = next_def;
   }

   return def;
}

/* Find all predicate sources and try to replace their defs with instructions
 * that can directly write to predicates.
 */
static bool
opt_instr(struct opt_predicates_ctx *ctx, struct ir3_instruction *instr)
{
   bool progress = false;

   foreach_src (src, instr) {
      if (!(src->flags & IR3_REG_PREDICATE))
         continue;

      struct ir3_register *def = resolve_predicate_def(ctx, src);

      if (src->def == def)
         continue;

      assert(can_write_predicate(ctx, def->instr) &&
             !(def->flags & IR3_REG_PREDICATE));

      struct ir3_instruction *predicate =
         clone_with_predicate_dst(ctx, def->instr);
      assert(predicate->dsts_count == 1);

      src->def = predicate->dsts[0];
      progress = true;
   }

   return progress;
}

static bool
opt_blocks(struct opt_predicates_ctx *ctx)
{
   bool progress = false;

   foreach_block (block, &ctx->ir->block_list) {
      foreach_instr (instr, &block->instr_list) {
         progress |= opt_instr(ctx, instr);
      }
   }

   return progress;
}

bool
ir3_opt_predicates(struct ir3 *ir, struct ir3_shader_variant *v)
{
   struct opt_predicates_ctx *ctx = rzalloc(NULL, struct opt_predicates_ctx);
   ctx->ir = ir;
   ctx->predicate_clones = _mesa_pointer_hash_table_create(ctx);

   bool progress = opt_blocks(ctx);

   ralloc_free(ctx);
   return progress;
}
