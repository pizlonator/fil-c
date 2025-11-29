/*
 * Copyright (C) 2021 Valve Corporation
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

#include "util/ralloc.h"
#include "ir3_ra.h"
#include "ir3_shader.h"

/* This file implements a validation pass for register allocation. We check
 * that the assignment of SSA values to registers is "valid", in the sense
 * that each original definition reaches all of its uses without being
 * clobbered by something else.
 *
 * The validation is a forward dataflow analysis. The state at each point
 * consists of, for each physical register, the SSA value occupying it, or a
 * few special values:
 *
 * - "unknown" is set initially, before the dataflow analysis assigns it a
 *   value. This is the lattice bottom.
 * - Values at the start get "undef", which acts like a special SSA value that
 *   indicates it is never written.
 * - "overdefined" registers are set to more than one value, depending on
 *   which path you take to get to the spot. This is the lattice top.
 *
 * Overdefined is necessary to distinguish because in some programs, like this
 * simple example, it's perfectly normal and allowed:
 *
 * if (...) {
 *    mov.u32u32 ssa_1(r1.x), ...
 *    ...
 * } else {
 *    mov.u32u32 ssa_2(r1.x), ...
 *    ...
 * }
 * // r1.x is overdefined here!
 *
 * However, if an ssa value after the if is accidentally assigned to r1.x, we
 * need to remember that it's invalid to catch the mistake. Overdef has to be
 * distinguished from undef so that the state forms a valid lattice to
 * guarantee that the analysis always terminates. We could avoid relying on
 * overdef by using liveness analysis, but not relying on liveness has the
 * benefit that we can catch bugs in liveness analysis too.
 *
 * One tricky thing we have to handle is the coalescing of splits/collects,
 * which means that multiple SSA values can occupy a register at the same
 * time. While we could use the same merge set indices that RA uses, again
 * that would rely on the merge set calculation being correct which we don't
 * want to. Instead we treat splits/collects as transfer instructions, similar
 * to the parallelcopy instructions inserted by RA, and have them copy their
 * sources to their destinations. This means that each physreg must carry the
 * SSA def assigned to it plus an offset into that definition, and when
 * validating sources we must look through splits/collects to find the
 * "original" source for each subregister.
 */

#define UNKNOWN ((struct ir3_register *)NULL)
#define UNDEF   ((struct ir3_register *)(uintptr_t)1)
#define OVERDEF ((struct ir3_register *)(uintptr_t)2)

struct reg_state {
   struct ir3_register *def;
   unsigned offset;
};

struct file_state {
   struct reg_state regs[RA_MAX_FILE_SIZE];
};

struct reaching_state {
   struct file_state half, full, shared, predicate;
};

struct ra_val_ctx {
   struct ir3_instruction *current_instr;

   /* The current state of the dataflow analysis for the instruction we're
    * processing.
    */
   struct reaching_state reaching;

   /* The state at the end of each basic block. */
   struct reaching_state *block_reaching;
   unsigned block_count;

   /* When validating shared RA, we have to take spill/reload instructions into
    * account. This saves an array of reg_state for the source of each spill
    * instruction, to be restored at the corresponding reload(s).
    */
   struct hash_table *spill_reaching;

   unsigned full_size, half_size, predicate_size;

   bool merged_regs;
   bool shared_ra;

   bool failed;
};

static void
validate_error(struct ra_val_ctx *ctx, const char *condstr)
{
   fprintf(stderr, "ra validation fail: %s\n", condstr);
   fprintf(stderr, "  -> for instruction: ");
   ir3_print_instr(ctx->current_instr);
   abort();
}

#define validate_assert(ctx, cond)                                             \
   do {                                                                        \
      if (!(cond)) {                                                           \
         validate_error(ctx, #cond);                                           \
      }                                                                        \
   } while (0)

static unsigned
get_file_size(struct ra_val_ctx *ctx, struct ir3_register *reg)
{
   if (reg->flags & IR3_REG_SHARED)
      return RA_SHARED_SIZE;
   else if (reg->flags & IR3_REG_PREDICATE)
      return ctx->predicate_size;
   else if (ctx->merged_regs || !(reg->flags & IR3_REG_HALF))
      return ctx->full_size;
   else
      return ctx->half_size;
}

static struct reg_state *
get_spill_state(struct ra_val_ctx *ctx, struct ir3_register *dst)
{
   struct hash_entry *entry = _mesa_hash_table_search(ctx->spill_reaching, dst);
   if (entry)
      return entry->data;
   else
      return NULL;
}

static struct reg_state *
get_or_create_spill_state(struct ra_val_ctx *ctx, struct ir3_register *dst)
{
   struct reg_state *state = get_spill_state(ctx, dst);
   if (state)
      return state;

   state = rzalloc_array(ctx, struct reg_state, reg_size(dst));
   _mesa_hash_table_insert(ctx->spill_reaching, dst, state);
   return state;
}

static bool
validate_reg_is_src(const struct ir3_register *reg)
{
   return ra_reg_is_src(reg) || ra_reg_is_predicate(reg);
}

static bool
validate_reg_is_dst(const struct ir3_register *reg)
{
   return ra_reg_is_dst(reg) || ra_reg_is_predicate(reg);
}

/* Validate simple things, like the registers being in-bounds. This way we
 * don't have to worry about out-of-bounds accesses later.
 */

static void
validate_simple(struct ra_val_ctx *ctx, struct ir3_instruction *instr)
{
   ctx->current_instr = instr;
   foreach_dst_if (dst, instr, validate_reg_is_dst) {
      if (ctx->shared_ra && !(dst->flags & IR3_REG_SHARED))
         continue;
      validate_assert(ctx, dst->num != INVALID_REG);
      unsigned dst_max = ra_reg_get_physreg(dst) + reg_size(dst);
      validate_assert(ctx, dst_max <= get_file_size(ctx, dst));
      if (dst->tied)
         validate_assert(ctx, ra_reg_get_num(dst) == ra_reg_get_num(dst->tied));
   }

   foreach_src_if (src, instr, validate_reg_is_src) {
      if (ctx->shared_ra && !(src->flags & IR3_REG_SHARED))
         continue;
      validate_assert(ctx, src->num != INVALID_REG);
      unsigned src_max = ra_reg_get_physreg(src) + reg_size(src);
      validate_assert(ctx, src_max <= get_file_size(ctx, src));
   }
}

/* This is the lattice operator. */
static bool
merge_reg(struct reg_state *dst, const struct reg_state *src)
{
   if (dst->def == UNKNOWN) {
      *dst = *src;
      return src->def != UNKNOWN;
   } else if (dst->def == OVERDEF) {
      return false;
   } else {
      if (src->def == UNKNOWN)
         return false;
      else if (src->def == OVERDEF) {
         *dst = *src;
         return true;
      } else {
         if (dst->def != src->def || dst->offset != src->offset) {
            dst->def = OVERDEF;
            dst->offset = 0;
            return true;
         } else {
            return false;
         }
      }
   }
}

static bool
merge_file(struct file_state *dst, const struct file_state *src, unsigned size)
{
   bool progress = false;
   for (unsigned i = 0; i < size; i++)
      progress |= merge_reg(&dst->regs[i], &src->regs[i]);
   return progress;
}

static bool
merge_state(struct ra_val_ctx *ctx, struct reaching_state *dst,
            const struct reaching_state *src)
{
   bool progress = false;
   progress |= merge_file(&dst->full, &src->full, ctx->full_size);
   progress |= merge_file(&dst->half, &src->half, ctx->half_size);
   progress |=
      merge_file(&dst->predicate, &src->predicate, ctx->predicate_size);
   return progress;
}

static bool
merge_state_physical(struct ra_val_ctx *ctx, struct reaching_state *dst,
                     const struct reaching_state *src)
{
   return merge_file(&dst->shared, &src->shared, RA_SHARED_SIZE);
}

static struct file_state *
ra_val_get_file(struct ra_val_ctx *ctx, struct ir3_register *reg)
{
   if (reg->flags & IR3_REG_SHARED)
      return &ctx->reaching.shared;
   else if (reg->flags & IR3_REG_PREDICATE)
      return &ctx->reaching.predicate;
   else if (ctx->merged_regs || !(reg->flags & IR3_REG_HALF))
      return &ctx->reaching.full;
   else
      return &ctx->reaching.half;
}

/* Predicate RA implements spilling by cloning the instruction that produces a
 * def. In that case, we might end up two different defs legitimately reaching a
 * source. To support validation, the RA will store the original def in the
 * instruction's data field.
 */
static struct ir3_register *
get_original_def(struct ir3_register *def)
{
   if (def == UNKNOWN || def == UNDEF || def == OVERDEF)
      return def;
   if (def->flags & IR3_REG_PREDICATE)
      return def->instr->data;
   return def;
}

static void
propagate_normal_instr(struct ra_val_ctx *ctx, struct ir3_instruction *instr)
{
   foreach_dst_if (dst, instr, validate_reg_is_dst) {
      /* Process destinations from scalar ALU instructions that were demoted to
       * normal ALU instructions. For these we must treat the instruction as a
       * spill of itself and set the propagate state to itself. See
       * try_demote_instructions().
       */
      if (ctx->shared_ra && !(dst->flags & IR3_REG_SHARED)) {
         if (instr->flags & IR3_INSTR_SHARED_SPILL) {
            struct reg_state *state = get_or_create_spill_state(ctx, dst);
            for (unsigned i = 0; i < reg_size(dst); i++) {
               state[i] = (struct reg_state){
                  .def = dst,
                  .offset = i,
               };
            }
         }
         continue;
      }

      struct file_state *file = ra_val_get_file(ctx, dst);
      physreg_t physreg = ra_reg_get_physreg(dst);

      for (unsigned i = 0; i < reg_size(dst); i++) {
         file->regs[physreg + i] = (struct reg_state){
            .def = get_original_def(dst),
            .offset = i,
         };
      }
   }
}

static void
propagate_split(struct ra_val_ctx *ctx, struct ir3_instruction *split)
{
   struct ir3_register *dst = split->dsts[0];
   struct ir3_register *src = split->srcs[0];
   physreg_t dst_physreg = ra_reg_get_physreg(dst);
   physreg_t src_physreg = ra_reg_get_physreg(src);
   struct file_state *file = ra_val_get_file(ctx, dst);

   if (ctx->shared_ra && !(dst->flags & IR3_REG_SHARED)) {
      struct reg_state *src_state = get_spill_state(ctx, src->def);
      if (src_state) {
         struct reg_state *dst_state = get_or_create_spill_state(ctx, dst);
         memcpy(dst_state, &src_state[split->split.off * reg_elem_size(src)],
                reg_size(dst) * sizeof(struct reg_state));
      }
      return;
   }

   unsigned offset = split->split.off * reg_elem_size(src);
   for (unsigned i = 0; i < reg_elem_size(src); i++) {
      file->regs[dst_physreg + i] = file->regs[src_physreg + offset + i];
   }
}

static void
propagate_collect(struct ra_val_ctx *ctx, struct ir3_instruction *collect)
{
   struct ir3_register *dst = collect->dsts[0];
   unsigned size = reg_size(dst);

   if (ctx->shared_ra && !(dst->flags & IR3_REG_SHARED)) {
      struct reg_state *dst_state = NULL;

      for (unsigned i = 0; i < collect->srcs_count; i++) {
         struct ir3_register *src = collect->srcs[i];
         unsigned dst_offset = i * reg_elem_size(dst);

         if (ra_reg_is_src(src)) {
            struct reg_state *src_state = get_spill_state(ctx, src->def);
            if (src_state) {
               if (!dst_state)
                  dst_state = get_or_create_spill_state(ctx, dst);
               memcpy(&dst_state[dst_offset], src_state,
                      reg_size(src) * sizeof(struct reg_state));
            }
         }
      }
   } else {
      struct file_state *file = ra_val_get_file(ctx, dst);
      physreg_t dst_physreg = ra_reg_get_physreg(dst);
      struct reg_state srcs[size];

      for (unsigned i = 0; i < collect->srcs_count; i++) {
         struct ir3_register *src = collect->srcs[i];
         unsigned dst_offset = i * reg_elem_size(dst);

         for (unsigned j = 0; j < reg_elem_size(dst); j++) {
            if (!ra_reg_is_src(src)) {
               srcs[dst_offset + j] = (struct reg_state){
                  .def = dst,
                  .offset = dst_offset + j,
               };
            } else {
               physreg_t src_physreg = ra_reg_get_physreg(src);
               srcs[dst_offset + j] = file->regs[src_physreg + j];
            }
         }
      }

      for (unsigned i = 0; i < size; i++)
         file->regs[dst_physreg + i] = srcs[i];
   }
}

static void
propagate_parallelcopy(struct ra_val_ctx *ctx, struct ir3_instruction *pcopy)
{
   unsigned size = 0;
   for (unsigned i = 0; i < pcopy->dsts_count; i++) {
      size += reg_size(pcopy->srcs[i]);
   }

   struct reg_state srcs[size];

   unsigned offset = 0;
   for (unsigned i = 0; i < pcopy->srcs_count; i++) {
      struct ir3_register *dst = pcopy->dsts[i];
      struct ir3_register *src = pcopy->srcs[i];
      struct file_state *file = ra_val_get_file(ctx, dst);

      if (ctx->shared_ra && !(dst->flags & IR3_REG_SHARED)) {
         if (ra_reg_is_src(src)) {
            struct reg_state *src_state = get_spill_state(ctx, src->def);
            if (src_state) {
               struct reg_state *dst_state = get_or_create_spill_state(ctx, dst);
               memcpy(dst_state, src_state, reg_size(dst) * sizeof(struct reg_state));
            }
         }
      } else {
         for (unsigned j = 0; j < reg_size(dst); j++) {
            if (src->flags & (IR3_REG_IMMED | IR3_REG_CONST)) {
               srcs[offset + j] = (struct reg_state){
                  .def = dst,
                  .offset = j,
               };
            } else {
               physreg_t src_physreg = ra_reg_get_physreg(src);
               srcs[offset + j] = file->regs[src_physreg + j];
            }
         }
      }

      offset += reg_size(dst);
   }
   assert(offset == size);

   offset = 0;
   for (unsigned i = 0; i < pcopy->dsts_count; i++) {
      struct ir3_register *dst = pcopy->dsts[i];

      if (ctx->shared_ra && !(dst->flags & IR3_REG_SHARED)) {
         offset += reg_size(dst);
         continue;
      }

      physreg_t dst_physreg = ra_reg_get_physreg(dst);
      struct file_state *file = ra_val_get_file(ctx, dst);

      for (unsigned j = 0; j < reg_size(dst); j++)
         file->regs[dst_physreg + j] = srcs[offset + j];

      offset += reg_size(dst);
   }
   assert(offset == size);
}

static void
propagate_spill(struct ra_val_ctx *ctx, struct ir3_instruction *instr)
{
   if (instr->srcs[0]->flags & IR3_REG_SHARED) { /* spill */
      struct reg_state *state = get_or_create_spill_state(ctx, instr->dsts[0]);
      physreg_t src_physreg = ra_reg_get_physreg(instr->srcs[0]);
      memcpy(state, &ctx->reaching.shared.regs[src_physreg],
             reg_size(instr->srcs[0]) * sizeof(struct reg_state));
   } else { /* reload */
      struct reg_state *state = get_spill_state(ctx, instr->srcs[0]->def);
      assert(state);
      physreg_t dst_physreg = ra_reg_get_physreg(instr->dsts[0]);
      memcpy(&ctx->reaching.shared.regs[dst_physreg], state,
             reg_size(instr->dsts[0]) * sizeof(struct reg_state));
   }
}

static void
propagate_instr(struct ra_val_ctx *ctx, struct ir3_instruction *instr)
{
   if (instr->opc == OPC_META_SPLIT)
      propagate_split(ctx, instr);
   else if (instr->opc == OPC_META_COLLECT)
      propagate_collect(ctx, instr);
   else if (instr->opc == OPC_META_PARALLEL_COPY)
      propagate_parallelcopy(ctx, instr);
   else if (ctx->shared_ra && instr->opc == OPC_MOV &&
            /* Moves from immed/const with IR3_INSTR_SHARED_SPILL were demoted
             * from scalar ALU, see try_demote_instruction().
             */
            !(instr->srcs[0]->flags & (IR3_REG_IMMED | IR3_REG_CONST)) &&
            (instr->flags & IR3_INSTR_SHARED_SPILL))
      propagate_spill(ctx, instr);
   else
      propagate_normal_instr(ctx, instr);
}

static bool
propagate_block(struct ra_val_ctx *ctx, struct ir3_block *block)
{
   ctx->reaching = ctx->block_reaching[block->index];

   foreach_instr (instr, &block->instr_list) {
      propagate_instr(ctx, instr);
   }

   bool progress = false;
   for (unsigned i = 0; i < 2; i++) {
      struct ir3_block *succ = block->successors[i];
      if (!succ)
         continue;
      progress |=
         merge_state(ctx, &ctx->block_reaching[succ->index], &ctx->reaching);
   }
   for (unsigned i = 0; i < block->physical_successors_count; i++) {
      struct ir3_block *succ = block->physical_successors[i];
      progress |= merge_state_physical(ctx, &ctx->block_reaching[succ->index],
                                       &ctx->reaching);
   }
   return progress;
}

static void
chase_definition(struct reg_state *state)
{
   while (true) {
      struct ir3_instruction *instr = state->def->instr;
      switch (instr->opc) {
      case OPC_META_SPLIT: {
         struct ir3_register *new_def = instr->srcs[0]->def;
         unsigned offset = instr->split.off * reg_elem_size(new_def);
         *state = (struct reg_state){
            .def = new_def,
            .offset = state->offset + offset,
         };
         break;
      }
      case OPC_META_COLLECT: {
         unsigned src_idx = state->offset / reg_elem_size(state->def);
         unsigned src_offset = state->offset % reg_elem_size(state->def);
         struct ir3_register *new_def = instr->srcs[src_idx]->def;
         if (new_def) {
            *state = (struct reg_state){
               .def = new_def,
               .offset = src_offset,
            };
         } else {
            /* Bail on immed/const */
            return;
         }
         break;
      }
      case OPC_META_PARALLEL_COPY: {
         unsigned dst_idx = ~0;
         for (unsigned i = 0; i < instr->dsts_count; i++) {
            if (instr->dsts[i] == state->def) {
               dst_idx = i;
               break;
            }
         }
         assert(dst_idx != ~0);

         struct ir3_register *new_def = instr->srcs[dst_idx]->def;
         if (new_def) {
            state->def = new_def;
         } else {
            /* Bail on immed/const */
            return;
         }
         break;
      }
      default:
         return;
      }
   }
}

static void
dump_reg_state(struct reg_state *state)
{
   if (state->def == UNDEF) {
      fprintf(stderr, "no reaching definition");
   } else if (state->def == OVERDEF) {
      fprintf(stderr,
              "more than one reaching definition or partial definition");
   } else {
      /* The analysis should always remove UNKNOWN eventually. */
      assert(state->def != UNKNOWN);

      const char *prefix = "r";
      unsigned num = state->def->num / 4;
      if (state->def->flags & IR3_REG_PREDICATE) {
         prefix = "p";
         num = 0;
      }

      fprintf(stderr, "ssa_%u:%u(%s%s%u.%c) + %u", state->def->instr->serialno,
              state->def->name, (state->def->flags & IR3_REG_HALF) ? "h" : "",
              prefix, num, "xyzw"[state->def->num % 4], state -> offset);
   }
}

static void
check_reaching_src(struct ra_val_ctx *ctx, struct ir3_instruction *instr,
                   struct ir3_register *src)
{
   if (ctx->shared_ra && !(src->flags & IR3_REG_SHARED))
      return;
   struct file_state *file = ra_val_get_file(ctx, src);
   physreg_t physreg = ra_reg_get_physreg(src);
   for (unsigned i = 0; i < reg_size(src); i++) {
      struct reg_state expected = (struct reg_state){
         .def = get_original_def(src->def),
         .offset = i,
      };
      chase_definition(&expected);

      struct reg_state actual = file->regs[physreg + i];

      if (expected.def != actual.def || expected.offset != actual.offset) {
         fprintf(
            stderr,
            "ra validation fail: wrong definition reaches source ssa_%u:%u + %u\n",
            src->def->instr->serialno, src->def->name, i);
         fprintf(stderr, "expected: ");
         dump_reg_state(&expected);
         fprintf(stderr, "\n");
         fprintf(stderr, "actual: ");
         dump_reg_state(&actual);
         fprintf(stderr, "\n");
         fprintf(stderr, "-> for instruction: ");
         ir3_print_instr(instr);
         ctx->failed = true;
      }
   }
}

static void
check_reaching_instr(struct ra_val_ctx *ctx, struct ir3_instruction *instr)
{
   if (instr->opc == OPC_META_SPLIT || instr->opc == OPC_META_COLLECT ||
       instr->opc == OPC_META_PARALLEL_COPY || instr->opc == OPC_META_PHI) {
      return;
   }

   foreach_src_if (src, instr, validate_reg_is_src) {
      check_reaching_src(ctx, instr, src);
   }
}

static void
check_reaching_block(struct ra_val_ctx *ctx, struct ir3_block *block)
{
   ctx->reaching = ctx->block_reaching[block->index];

   foreach_instr (instr, &block->instr_list) {
      check_reaching_instr(ctx, instr);
      propagate_instr(ctx, instr);
   }

   for (unsigned i = 0; i < 2; i++) {
      struct ir3_block *succ = block->successors[i];
      if (!succ)
         continue;

      unsigned pred_idx = ir3_block_get_pred_index(succ, block);
      foreach_instr (instr, &succ->instr_list) {
         if (instr->opc != OPC_META_PHI)
            break;
         if (instr->srcs[pred_idx]->def)
            check_reaching_src(ctx, instr, instr->srcs[pred_idx]);
      }
   }
}

static void
check_reaching_defs(struct ra_val_ctx *ctx, struct ir3 *ir)
{
   ctx->block_reaching =
      rzalloc_array(ctx, struct reaching_state, ctx->block_count);

   struct reaching_state *start = &ctx->block_reaching[0];
   for (unsigned i = 0; i < ctx->full_size; i++)
      start->full.regs[i].def = UNDEF;
   for (unsigned i = 0; i < ctx->half_size; i++)
      start->half.regs[i].def = UNDEF;
   for (unsigned i = 0; i < RA_SHARED_SIZE; i++)
      start->shared.regs[i].def = UNDEF;
   for (unsigned i = 0; i < ctx->predicate_size; i++)
      start->predicate.regs[i].def = UNDEF;

   bool progress;
   do {
      progress = false;
      foreach_block (block, &ir->block_list) {
         progress |= propagate_block(ctx, block);
      }
   } while (progress);

   foreach_block (block, &ir->block_list) {
      check_reaching_block(ctx, block);
   }

   if (ctx->failed) {
      fprintf(stderr, "failing shader:\n");
      ir3_print(ir);
      abort();
   }
}

void
ir3_ra_validate(struct ir3_shader_variant *v, unsigned full_size,
                unsigned half_size, unsigned block_count, bool shared_ra)
{
#ifdef NDEBUG
#define VALIDATE 0
#else
#define VALIDATE 1
#endif

   if (!VALIDATE)
      return;

   struct ra_val_ctx *ctx = rzalloc(NULL, struct ra_val_ctx);
   ctx->merged_regs = v->mergedregs;
   ctx->full_size = full_size;
   ctx->half_size = half_size;
   ctx->predicate_size = v->compiler->num_predicates * 2;
   ctx->block_count = block_count;
   ctx->shared_ra = shared_ra;
   if (ctx->shared_ra)
      ctx->spill_reaching = _mesa_pointer_hash_table_create(ctx);

   foreach_block (block, &v->ir->block_list) {
      foreach_instr (instr, &block->instr_list) {
         validate_simple(ctx, instr);
      }
   }

   check_reaching_defs(ctx, v->ir);

   ralloc_free(ctx);
}
