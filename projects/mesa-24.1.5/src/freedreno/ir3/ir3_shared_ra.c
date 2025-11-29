/*
 * Copyright (C) 2021 Valve Corporation
 * Copyright (C) 2014 Rob Clark <robclark@freedesktop.org>
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

#include "ir3_ra.h"
#include "ir3_shader.h"

#include "util/u_math.h"

/* Allocating shared registers can pose a challenge, because their live
 * intervals use the physical CFG which has extra edges inserted that are
 * pretty much always critical edges. This causes problems with phi nodes,
 * because copies for phi nodes have to happen "along the edge," and similarly
 * causes problems when reunifying values that have had their live range split.
 * Problematic phi nodes should be relatively rare, so we ban them for now.
 * The solution we choose for live-range splitting is to integrate spilling and
 * register allcoation and spill to vector registers rather than split a live
 * range, which negates some of the advantages of SSA-based RA, but it isn't as
 * bad as it seems because the conditions needed (vector shared registers, which
 * only movmsk currently produces, or fixed registers which we don't do) are
 * relatively rare. Spilling is also much cheaper than spilling vector registers
 * to private memory.
 */

struct ra_interval {
   struct ir3_reg_interval interval;

   struct rb_node physreg_node;
   physreg_t physreg_start, physreg_end;

   /* Where the shared register is spilled to. If there were no uses when it's
    * spilled it could be the original defining instruction.
    */
   struct ir3_register *spill_def;

   /* Whether this contains a source of the current instruction that can't be
    * spilled.
    */
   bool src;

   bool needs_reload;
};

struct ra_block_state {
   bool visited;

   /* For blocks whose successors are visited first (i.e. loop backedges), which
    * values should be live at the end.
    */
   BITSET_WORD *live_out;
};

struct ra_ctx {
   struct ir3_reg_ctx reg_ctx;

   BITSET_DECLARE(available, RA_MAX_FILE_SIZE);

   struct rb_tree physreg_intervals;

   struct ra_interval *intervals;

   struct ir3_liveness *live;

   struct hash_table *pcopy_src_map;

   struct ra_block_state *blocks;

   unsigned start;
};

static struct ra_interval *
ir3_reg_interval_to_ra_interval(struct ir3_reg_interval *interval)
{
   return rb_node_data(struct ra_interval, interval, interval);
}

static struct ra_interval *
rb_node_to_interval(struct rb_node *node)
{
   return rb_node_data(struct ra_interval, node, physreg_node);
}

static const struct ra_interval *
rb_node_to_interval_const(const struct rb_node *node)
{
   return rb_node_data(struct ra_interval, node, physreg_node);
}

static struct ra_interval *
ra_interval_next(struct ra_interval *interval)
{
   struct rb_node *next = rb_node_next(&interval->physreg_node);
   return next ? rb_node_to_interval(next) : NULL;
}

static struct ra_interval *
ra_interval_next_or_null(struct ra_interval *interval)
{
   return interval ? ra_interval_next(interval) : NULL;
}

static int
ra_interval_insert_cmp(const struct rb_node *_a, const struct rb_node *_b)
{
   const struct ra_interval *a = rb_node_to_interval_const(_a);
   const struct ra_interval *b = rb_node_to_interval_const(_b);
   return b->physreg_start - a->physreg_start;
}

static int
ra_interval_cmp(const struct rb_node *node, const void *data)
{
   physreg_t reg = *(const physreg_t *)data;
   const struct ra_interval *interval = rb_node_to_interval_const(node);
   if (interval->physreg_start > reg)
      return -1;
   else if (interval->physreg_end <= reg)
      return 1;
   else
      return 0;
}

static struct ra_ctx *
ir3_reg_ctx_to_ctx(struct ir3_reg_ctx *ctx)
{
   return rb_node_data(struct ra_ctx, ctx, reg_ctx);
}

static struct ra_interval *
ra_interval_search_sloppy(struct rb_tree *tree, physreg_t reg)
{
   struct rb_node *node = rb_tree_search_sloppy(tree, &reg, ra_interval_cmp);
   return node ? rb_node_to_interval(node) : NULL;
}

/* Get the interval covering the reg, or the closest to the right if it
 * doesn't exist.
 */
static struct ra_interval *
ra_interval_search_right(struct rb_tree *tree, physreg_t reg)
{
   struct ra_interval *interval = ra_interval_search_sloppy(tree, reg);
   if (!interval) {
      return NULL;
   } else if (interval->physreg_end > reg) {
      return interval;
   } else {
      /* There is no interval covering reg, and ra_file_search_sloppy()
       * returned the closest range to the left, so the next interval to the
       * right should be the closest to the right.
       */
      return ra_interval_next_or_null(interval);
   }
}

static struct ra_interval *
ra_ctx_search_right(struct ra_ctx *ctx, physreg_t reg)
{
   return ra_interval_search_right(&ctx->physreg_intervals, reg);
}

static void
interval_add(struct ir3_reg_ctx *reg_ctx, struct ir3_reg_interval *_interval)
{
   struct ra_interval *interval = ir3_reg_interval_to_ra_interval(_interval);
   struct ra_ctx *ctx = ir3_reg_ctx_to_ctx(reg_ctx);

   /* We can assume in this case that physreg_start/physreg_end is already
    * initialized.
    */
   for (physreg_t i = interval->physreg_start; i < interval->physreg_end; i++) {
      BITSET_CLEAR(ctx->available, i);
   }

   rb_tree_insert(&ctx->physreg_intervals, &interval->physreg_node,
                  ra_interval_insert_cmp);
}

static void
interval_delete(struct ir3_reg_ctx *reg_ctx, struct ir3_reg_interval *_interval)
{
   struct ra_interval *interval = ir3_reg_interval_to_ra_interval(_interval);
   struct ra_ctx *ctx = ir3_reg_ctx_to_ctx(reg_ctx);

   for (physreg_t i = interval->physreg_start; i < interval->physreg_end; i++) {
      BITSET_SET(ctx->available, i);
   }

   rb_tree_remove(&ctx->physreg_intervals, &interval->physreg_node);
}

static void
interval_readd(struct ir3_reg_ctx *ctx, struct ir3_reg_interval *_parent,
               struct ir3_reg_interval *_child)
{
   struct ra_interval *parent = ir3_reg_interval_to_ra_interval(_parent);
   struct ra_interval *child = ir3_reg_interval_to_ra_interval(_child);

   child->physreg_start =
      parent->physreg_start + (child->interval.reg->interval_start -
                               parent->interval.reg->interval_start);
   child->physreg_end =
      child->physreg_start +
      (child->interval.reg->interval_end - child->interval.reg->interval_start);

   interval_add(ctx, _child);
}

static void
ra_ctx_init(struct ra_ctx *ctx)
{
   ctx->reg_ctx.interval_add = interval_add;
   ctx->reg_ctx.interval_delete = interval_delete;
   ctx->reg_ctx.interval_readd = interval_readd;
}

static void
ra_ctx_reset_block(struct ra_ctx *ctx)
{
   for (unsigned i = 0; i < RA_SHARED_SIZE; i++) {
      BITSET_SET(ctx->available, i);
   }

   rb_tree_init(&ctx->reg_ctx.intervals);
   rb_tree_init(&ctx->physreg_intervals);
}

static void
ra_interval_init(struct ra_interval *interval, struct ir3_register *reg)
{
   ir3_reg_interval_init(&interval->interval, reg);
}

static physreg_t
ra_interval_get_physreg(const struct ra_interval *interval)
{
   unsigned child_start = interval->interval.reg->interval_start;

   while (interval->interval.parent) {
      interval = ir3_reg_interval_to_ra_interval(interval->interval.parent);
   }

   return interval->physreg_start +
          (child_start - interval->interval.reg->interval_start);
}

static unsigned
ra_interval_get_num(const struct ra_interval *interval)
{
   return ra_physreg_to_num(ra_interval_get_physreg(interval),
                            interval->interval.reg->flags);
}

static void
ra_interval_dump(struct log_stream *stream, struct ra_interval *interval)
{
   mesa_log_stream_printf(stream, "physreg %u ", interval->physreg_start);

   ir3_reg_interval_dump(stream, &interval->interval);
}

static void
ra_ctx_dump(struct ra_ctx *ctx)
{
   struct log_stream *stream = mesa_log_streami();

   mesa_log_stream_printf(stream, "shared:\n");
   rb_tree_foreach (struct ra_interval, interval, &ctx->physreg_intervals,
                    physreg_node) {
      ra_interval_dump(stream, interval);
   }

   unsigned start, end;
   mesa_log_stream_printf(stream, "available:\n");
   BITSET_FOREACH_RANGE (start, end, ctx->available, RA_SHARED_SIZE) {
      mesa_log_stream_printf(stream, "%u-%u ", start, end);
   }
   mesa_log_stream_printf(stream, "\n");
   mesa_log_stream_printf(stream, "start: %u\n", ctx->start);
}

static bool
get_reg_specified(struct ra_ctx *ctx, struct ir3_register *reg, physreg_t physreg)
{
   for (unsigned i = 0; i < reg_size(reg); i++) {
      if (!BITSET_TEST(ctx->available, physreg + i))
         return false;
   }

   return true;
}

static unsigned
reg_file_size(struct ir3_register *reg)
{
   return RA_SHARED_SIZE;
}

static physreg_t
find_best_gap(struct ra_ctx *ctx, struct ir3_register *dst, unsigned size,
              unsigned align)
{
   unsigned file_size = reg_file_size(dst);

   /* This can happen if we create a very large merge set. Just bail out in that
    * case.
    */
   if (size > file_size)
      return (physreg_t) ~0;

   unsigned start = ALIGN(ctx->start, align) % (file_size - size + align);
   unsigned candidate = start;
   do {
      bool is_available = true;
      for (unsigned i = 0; i < size; i++) {
         if (!BITSET_TEST(ctx->available, candidate + i)) {
            is_available = false;
            break;
         }
      }

      if (is_available) {
         ctx->start = (candidate + size) % file_size;
         return candidate;
      }

      candidate += align;
      if (candidate + size > file_size)
         candidate = 0;
   } while (candidate != start);

   return (physreg_t)~0;
}

static physreg_t
find_best_spill_reg(struct ra_ctx *ctx, struct ir3_register *reg,
                    unsigned size, unsigned align)
{
   unsigned file_size = reg_file_size(reg);
   unsigned min_cost = UINT_MAX;

   unsigned start = ALIGN(ctx->start, align) % (file_size - size + align);
   physreg_t candidate = start;
   physreg_t best_reg = (physreg_t)~0;
   do {
      unsigned cost = 0;

      /* Iterate through intervals we'd need to spill to use this reg. */
      for (struct ra_interval *interval = ra_ctx_search_right(ctx, candidate);
           interval && interval->physreg_start < candidate + size;
           interval = ra_interval_next_or_null(interval)) {
         /* We can't spill sources of the current instruction when reloading
          * sources.
          */
         if (interval->src) {
            cost = UINT_MAX;
            break;
         }

         /* We prefer spilling intervals that already have been spilled, so we
          * don't have to emit another mov.
          */
         if (!interval->spill_def)
            cost += (interval->physreg_end - interval->physreg_start);
      }

      if (cost < min_cost) {
         min_cost = cost;
         best_reg = candidate;
      }

      candidate += align;
      if (candidate + size > file_size)
         candidate = 0;
   } while (candidate != start);

   return best_reg;
}

static struct ir3_register *
split(struct ir3_register *def, unsigned offset, struct ir3_instruction *before)
{
   if (reg_elems(def) == 1) {
      assert(offset == 0);
      return def;
   }

   struct ir3_instruction *split =
      ir3_instr_create(before->block, OPC_META_SPLIT, 1, 1);
   split->split.off = offset;
   struct ir3_register *dst = __ssa_dst(split);
   struct ir3_register *src =
      ir3_src_create(split, INVALID_REG, def->flags & (IR3_REG_HALF | IR3_REG_SSA));
   src->wrmask = def->wrmask;
   src->def = def;
   ir3_instr_move_after(split, before);
   return dst;
}

static struct ir3_register *
extract(struct ir3_register *parent_def, unsigned offset, unsigned elems,
        struct ir3_instruction *before)
{
   if (offset == 0 && elems == reg_elems(parent_def))
      return parent_def;

   if (elems == 1)
      return split(parent_def, offset, before);

   struct ir3_instruction *collect =
      ir3_instr_create(before->block, OPC_META_COLLECT, 1, elems);
   struct ir3_register *dst = __ssa_dst(collect);
   dst->flags |= parent_def->flags & IR3_REG_HALF;
   dst->wrmask = MASK(elems);

   ir3_instr_move_after(collect, before);

   for (unsigned i = 0; i < elems; i++) {
      ir3_src_create(collect, INVALID_REG,
                     parent_def->flags & (IR3_REG_HALF | IR3_REG_SSA))->def =
         split(parent_def, offset + i, before);
   }

   return dst;
}

static void
spill_interval_children(struct ra_interval *interval,
                        struct ir3_instruction *before)
{
   rb_tree_foreach (struct ra_interval, child, &interval->interval.children,
                    interval.node) {
      if (!child->spill_def) {
         child->spill_def = extract(interval->spill_def,
                                    (child->interval.reg->interval_start -
                                     interval->interval.reg->interval_start) /
                                    reg_elem_size(interval->interval.reg),
                                    reg_elems(child->interval.reg), before);
      }
      spill_interval_children(child, before);
   }
}

static void
spill_interval(struct ra_ctx *ctx, struct ra_interval *interval)
{
   struct ir3_instruction *before = interval->interval.reg->instr;

   d("spilling ssa_%u:%u", before->serialno, interval->interval.reg->name);

   if (!interval->spill_def) {
      /* If this is a phi node or input, we need to insert the demotion to a
       * regular register after the last phi or input in the block.
       */
      if (before->opc == OPC_META_PHI ||
          before->opc == OPC_META_INPUT) {
         struct ir3_block *block = before->block;
         struct ir3_instruction *last_phi_input = NULL;
         foreach_instr_from (instr, before, &block->instr_list) {
            if (instr->opc != before->opc)
               break;
            last_phi_input = instr;
         }
         before = last_phi_input;
      }

      struct ir3_instruction *mov = ir3_instr_create(before->block, OPC_MOV, 1, 1);
      mov->flags |= IR3_INSTR_SHARED_SPILL;
      struct ir3_register *dst = __ssa_dst(mov);
      dst->flags |= (interval->interval.reg->flags & IR3_REG_HALF);
      dst->wrmask = interval->interval.reg->wrmask;
      mov->repeat = reg_elems(dst) - 1;
      ir3_src_create(mov, interval->interval.reg->num,
                     IR3_REG_SHARED | (mov->repeat ? IR3_REG_R : 0) |
                     (interval->interval.reg->flags & IR3_REG_HALF))->wrmask =
                     interval->interval.reg->wrmask;
      mov->cat1.src_type = mov->cat1.dst_type =
         (interval->interval.reg->flags & IR3_REG_HALF) ? TYPE_U16 : TYPE_U32;

      ir3_instr_move_after(mov, before);
      interval->spill_def = dst;
   }

   spill_interval_children(interval, interval->spill_def->instr);

   ir3_reg_interval_remove_all(&ctx->reg_ctx, &interval->interval);
}

/* Try to demote a scalar ALU instruction to a normal ALU instruction, using the
 * spilled sources. We have to take into account restrictions on the number of
 * shared sources that only exist for normal ALU instructions.
 */
static bool
try_demote_instruction(struct ra_ctx *ctx, struct ir3_instruction *instr)
{
   /* First, check restrictions. */
   switch (opc_cat(instr->opc)) {
   case 1:
      /* MOVMSK is special and can't be demoted. It also has no sources so must
       * go before the check below.
       */
      if (instr->opc == OPC_MOVMSK)
         return false;

      assert(instr->srcs_count >= 1);
      if (!(instr->srcs[0]->flags & (IR3_REG_CONST | IR3_REG_IMMED)))
         return false;
      break;
   case 2: {
      /* We need one source to either be demotable or an immediate. */
      if (instr->srcs_count > 1) {
         struct ra_interval *src0_interval =
            (instr->srcs[0]->flags & IR3_REG_SSA) ? &ctx->intervals[instr->srcs[0]->def->name] : NULL;
         struct ra_interval *src1_interval =
            (instr->srcs[0]->flags & IR3_REG_SSA) ? &ctx->intervals[instr->srcs[0]->def->name] : NULL;
         if (!(src0_interval && src0_interval->spill_def) &&
             !(src1_interval && src1_interval->spill_def) &&
             !(instr->srcs[0]->flags & IR3_REG_IMMED) &&
             !(instr->srcs[1]->flags & IR3_REG_IMMED))
            return false;
      }
      break;
   }
   case 3: {
      struct ra_interval *src0_interval =
         (instr->srcs[0]->flags & IR3_REG_SSA) ? &ctx->intervals[instr->srcs[0]->def->name] : NULL;
      struct ra_interval *src1_interval =
         (instr->srcs[1]->flags & IR3_REG_SSA) ? &ctx->intervals[instr->srcs[1]->def->name] : NULL;

      /* src1 cannot be shared */
      if (src1_interval && !src1_interval->spill_def) {
         /* Try to swap src0 and src1, similar to what copy prop does. */
         if (!is_mad(instr->opc))
            return false;

         if ((src0_interval && src0_interval->spill_def) ||
             (instr->srcs[0]->flags & IR3_REG_IMMED)) {
            struct ir3_register *src0 = instr->srcs[0];
            instr->srcs[0] = instr->srcs[1];
            instr->srcs[1] = src0;
         } else {
            return false;
         }
      }
      break;
   }
   case 4: {
      assert(instr->srcs[0]->flags & IR3_REG_SSA);
      struct ra_interval *src_interval = &ctx->intervals[instr->srcs[0]->def->name];
      if (!src_interval->spill_def)
         return false;
      break;
   }

   default:
      return false;
   }

   d("demoting instruction");

   /* If the instruction is already not a scalar ALU instruction, we should've
    * skipped reloading and just demoted sources directly, so we should never
    * get here.
    */
   assert(instr->dsts[0]->flags & IR3_REG_SHARED);

   /* Now we actually demote the instruction */
   ra_foreach_src (src, instr) {
      assert(src->flags & IR3_REG_SHARED);
      struct ra_interval *interval = &ctx->intervals[src->def->name];
      if (interval->spill_def) {
         src->def = interval->spill_def;
         src->flags &= ~IR3_REG_SHARED;
         interval->needs_reload = false;
         if (interval->interval.inserted)
            ir3_reg_interval_remove(&ctx->reg_ctx, &interval->interval);
         while (interval->interval.parent)
            interval = ir3_reg_interval_to_ra_interval(interval->interval.parent);
         interval->src = false;
      }
   }

   struct ra_interval *dst_interval = &ctx->intervals[instr->dsts[0]->name];
   instr->dsts[0]->flags &= ~IR3_REG_SHARED;
   ra_interval_init(dst_interval, instr->dsts[0]);
   dst_interval->spill_def = instr->dsts[0];

   instr->flags |= IR3_INSTR_SHARED_SPILL;

   return true;
}

/* Free up [start, start + size) by spilling live intervals.
 */
static void
free_space(struct ra_ctx *ctx, physreg_t start, unsigned size)
{
   struct ra_interval *interval = ra_ctx_search_right(ctx, start);
   while (interval && interval->physreg_start < start + size) {
      struct ra_interval *next = ra_interval_next_or_null(interval);
      spill_interval(ctx, interval);
      interval = next;
   }
}

static physreg_t
get_reg(struct ra_ctx *ctx, struct ir3_register *reg, bool src)
{
   if (reg->merge_set && reg->merge_set->preferred_reg != (physreg_t)~0) {
      physreg_t preferred_reg =
         reg->merge_set->preferred_reg + reg->merge_set_offset;
      if (preferred_reg < reg_file_size(reg) &&
          preferred_reg % reg_elem_size(reg) == 0 &&
          get_reg_specified(ctx, reg, preferred_reg))
         return preferred_reg;
   }

   /* If this register is a subset of a merge set which we have not picked a
    * register for, first try to allocate enough space for the entire merge
    * set.
    */
   unsigned size = reg_size(reg);
   if (reg->merge_set && reg->merge_set->preferred_reg == (physreg_t)~0 &&
       size < reg->merge_set->size) {
      physreg_t best_reg = find_best_gap(ctx, reg, reg->merge_set->size,
                                         reg->merge_set->alignment);
      if (best_reg != (physreg_t)~0u) {
         best_reg += reg->merge_set_offset;
         return best_reg;
      }
   }

   /* For ALU and SFU instructions, if the src reg is avail to pick, use it.
    * Because this doesn't introduce unnecessary dependencies, and it
    * potentially avoids needing (ss) syncs for write after read hazards for
    * SFU instructions:
    */
   if (!src && (is_sfu(reg->instr) || is_alu(reg->instr))) {
      for (unsigned i = 0; i < reg->instr->srcs_count; i++) {
         struct ir3_register *src = reg->instr->srcs[i];
         if (!ra_reg_is_src(src))
            continue;
         if ((src->flags & IR3_REG_SHARED) && reg_size(src) >= size) {
            struct ra_interval *src_interval = &ctx->intervals[src->def->name];
            physreg_t src_physreg = ra_interval_get_physreg(src_interval);
            if (src_physreg % reg_elem_size(reg) == 0 &&
                src_physreg + size <= reg_file_size(reg) &&
                get_reg_specified(ctx, reg, src_physreg))
               return src_physreg;
         }
      }
   }

   return find_best_gap(ctx, reg, size, reg_elem_size(reg));
}

/* The reload process is split in two, first we allocate a register to reload to
 * for all sources that need a reload and then we actually execute the reload.
 * This is to allow us to demote shared ALU instructions to non-shared whenever
 * we would otherwise need to spill to reload, without leaving dangling unused
 * reload mov's from previously processed sources. So, for example, we could
 * need to reload both sources of an add, but after reloading the first source
 * we realize that we would need to spill to reload the second source and we
 * should demote the add instead, which means cancelling the first reload.
 */
static void
reload_src(struct ra_ctx *ctx, struct ir3_instruction *instr,
           struct ir3_register *src)
{
   struct ir3_register *reg = src->def;
   struct ra_interval *interval = &ctx->intervals[reg->name];
   unsigned size = reg_size(reg);

   physreg_t best_reg = get_reg(ctx, reg, true);

   if (best_reg == (physreg_t)~0u) {
      if (try_demote_instruction(ctx, instr))
         return;

      best_reg = find_best_spill_reg(ctx, reg, size, reg_elem_size(reg));
      assert(best_reg != (physreg_t)~0u);

      free_space(ctx, best_reg, size);
   }

   d("reload src %u physreg %u", reg->name, best_reg);
   interval->physreg_start = best_reg;
   interval->physreg_end = best_reg + size;
   interval->needs_reload = true;
   ir3_reg_interval_insert(&ctx->reg_ctx, &interval->interval);
   interval->src = true;
}

static void
reload_interval(struct ra_ctx *ctx, struct ir3_instruction *instr,
                struct ir3_block *block, struct ra_interval *interval)
{
   struct ir3_register *def = interval->interval.reg;
   struct ir3_instruction *mov = ir3_instr_create(block, OPC_MOV, 1, 1);
   mov->flags |= IR3_INSTR_SHARED_SPILL;
   unsigned flags = IR3_REG_SHARED | (def->flags & IR3_REG_HALF);
   ir3_dst_create(mov, ra_physreg_to_num(interval->physreg_start, flags),
                  flags)->wrmask = def->wrmask;
   mov->repeat = reg_elems(def) - 1;
   struct ir3_register *mov_src =
      ir3_src_create(mov, INVALID_REG, IR3_REG_SSA | (def->flags & IR3_REG_HALF) |
                     (mov->repeat ? IR3_REG_R : 0));
   assert(interval->spill_def);
   mov_src->def = interval->spill_def;
   mov_src->wrmask = def->wrmask;
   mov->cat1.src_type = mov->cat1.dst_type =
      (def->flags & IR3_REG_HALF) ? TYPE_U16 : TYPE_U32;

   if (instr)
      ir3_instr_move_before(mov, instr);
}

static void
reload_src_finalize(struct ra_ctx *ctx, struct ir3_instruction *instr,
                    struct ir3_register *src)
{
   struct ir3_register *reg = src->def;
   struct ra_interval *interval = &ctx->intervals[reg->name];

   if (!interval->needs_reload)
      return;

   reload_interval(ctx, instr, instr->block, interval);

   interval->needs_reload = false;
}

static bool
can_demote_src(struct ir3_instruction *instr)
{
   switch (instr->opc) {
   case OPC_SCAN_MACRO:
   case OPC_META_COLLECT:
      return false;
   case OPC_MOV:
      /* non-shared -> shared floating-point conversions don't work */
      return (!(instr->dsts[0]->flags & IR3_REG_SHARED) ||
          (full_type(instr->cat1.src_type) != TYPE_F32 &&
           full_type(instr->cat1.dst_type) != TYPE_F32));
   default:
      return (!is_alu(instr) && !is_sfu(instr)) ||
         !(instr->dsts[0]->flags & IR3_REG_SHARED);
   }
}

/* Ensure that this source is never spilled while reloading other sources.
 */
static void
mark_src(struct ra_ctx *ctx, struct ir3_register *src)
{
   if (!(src->flags & IR3_REG_SHARED))
      return;

   struct ra_interval *interval = &ctx->intervals[src->def->name];

   if (interval->interval.inserted) {
      while (interval->interval.parent)
         interval = ir3_reg_interval_to_ra_interval(interval->interval.parent);

      interval->src = true;
   }
}

static void
ensure_src_live(struct ra_ctx *ctx, struct ir3_instruction *instr,
                struct ir3_register *src)
{
   if (!(src->flags & IR3_REG_SHARED))
      return;

   struct ra_interval *interval = &ctx->intervals[src->def->name];

   if (!interval->interval.inserted) {
      /* In some cases we cannot demote shared reg sources to non-shared regs,
       * then we have to reload it.
       */
      assert(interval->spill_def);
      if (!can_demote_src(instr)) {
         reload_src(ctx, instr, src);
      } else {
         if (instr->opc == OPC_META_PARALLEL_COPY) {
            /* Stash away the original def to use later in case we actually have
             * to insert a reload.
             */
            _mesa_hash_table_insert(ctx->pcopy_src_map, src, src->def);
         }
         src->def = interval->spill_def;
         src->flags &= ~IR3_REG_SHARED;
      }
   }
}

static void
assign_src(struct ra_ctx *ctx, struct ir3_register *src)
{
   if (!(src->flags & IR3_REG_SHARED))
      return;

   struct ra_interval *interval = &ctx->intervals[src->def->name];
   assert(interval->interval.inserted);
   src->num = ra_physreg_to_num(ra_interval_get_physreg(interval), src->flags);

   if ((src->flags & IR3_REG_FIRST_KILL) &&
       !interval->interval.parent &&
       rb_tree_is_empty(&interval->interval.children))
      ir3_reg_interval_remove(&ctx->reg_ctx, &interval->interval);

   while (interval->interval.parent)
      interval = ir3_reg_interval_to_ra_interval(interval->interval.parent);

   interval->src = false;
}

static void
handle_dst(struct ra_ctx *ctx, struct ir3_instruction *instr,
           struct ir3_register *dst)
{
   if (!(dst->flags & IR3_REG_SHARED))
      return;

   struct ra_interval *interval = &ctx->intervals[dst->name];
   ra_interval_init(interval, dst);
   interval->spill_def = NULL;

   if (dst->tied) {
      struct ir3_register *tied_def = dst->tied->def;
      struct ra_interval *tied_interval = &ctx->intervals[tied_def->name];
      if ((dst->tied->flags & IR3_REG_KILL) &&
          !tied_interval->interval.parent &&
          rb_tree_is_empty(&tied_interval->interval.children)) {
         dst->num = dst->tied->num;
         interval->physreg_start = tied_interval->physreg_start;
         interval->physreg_end = tied_interval->physreg_end;
         ir3_reg_interval_insert(&ctx->reg_ctx, &interval->interval);
         return;
      }
   }

   physreg_t physreg = get_reg(ctx, dst, false);
   if (physreg == (physreg_t) ~0u) {
      if (try_demote_instruction(ctx, instr))
         return;

      unsigned size = reg_size(dst);
      physreg = find_best_spill_reg(ctx, dst, size, reg_elem_size(dst));
      assert(physreg != (physreg_t)~0u);
      free_space(ctx, physreg, size);
   }

   interval->physreg_start = physreg;
   interval->physreg_end = physreg + reg_size(dst);
   dst->num = ra_physreg_to_num(physreg, dst->flags);
   ir3_reg_interval_insert(&ctx->reg_ctx, &interval->interval);
   d("insert dst %u physreg %u", dst->name, physreg);

   if (dst->tied) {
      struct ir3_instruction *mov = ir3_instr_create(instr->block, OPC_META_PARALLEL_COPY, 1, 1);
      unsigned flags = IR3_REG_SHARED | (dst->flags & IR3_REG_HALF);
      ir3_dst_create(mov, dst->num, flags)->wrmask = dst->wrmask;
      ir3_src_create(mov, dst->tied->num, flags)->wrmask = dst->wrmask;
      mov->cat1.src_type = mov->cat1.dst_type =
         (dst->flags & IR3_REG_HALF) ? TYPE_U16 : TYPE_U32;;
      ir3_instr_move_before(mov, instr);
      dst->tied->num = dst->num;
   }
}

static void
handle_src_late(struct ra_ctx *ctx, struct ir3_instruction *instr,
                struct ir3_register *src)
{
   if (!(src->flags & IR3_REG_SHARED))
      return;

   struct ra_interval *interval = &ctx->intervals[src->def->name];
   reload_src_finalize(ctx, instr, src);

   /* Remove killed sources that have to be killed late due to being merged with
    * other defs.
    */
   if (!(src->flags & IR3_REG_KILL))
      return;

   if (interval->interval.inserted)
      ir3_reg_interval_remove(&ctx->reg_ctx, &interval->interval);
}

static void
handle_normal_instr(struct ra_ctx *ctx, struct ir3_instruction *instr)
{
   ra_foreach_src (src, instr)
      mark_src(ctx, src);

   ra_foreach_src (src, instr)
      ensure_src_live(ctx, instr, src);

   ra_foreach_src_rev (src, instr)
      assign_src(ctx, src);

   ra_foreach_dst (dst, instr)
      handle_dst(ctx, instr, dst);

   ra_foreach_src (src, instr)
      handle_src_late(ctx, instr, src);
}

static void
handle_split(struct ra_ctx *ctx, struct ir3_instruction *split)
{
   struct ir3_register *src = split->srcs[0];
   struct ir3_register *dst = split->dsts[0];

   if (!(dst->flags & IR3_REG_SHARED))
      return;

   if (dst->merge_set == NULL || src->def->merge_set != dst->merge_set) {
      handle_normal_instr(ctx, split);
      return;
   }

   struct ra_interval *src_interval = &ctx->intervals[src->def->name];
   struct ra_interval *dst_interval = &ctx->intervals[dst->name];

   ra_interval_init(dst_interval, dst);
   dst_interval->spill_def = NULL;

   if (src_interval->spill_def) {
      struct ir3_instruction *spill_split =
         ir3_instr_create(split->block, OPC_META_SPLIT, 1, 1);
      struct ir3_register *dst = __ssa_dst(spill_split);
      ir3_src_create(spill_split, INVALID_REG, IR3_REG_SSA)->def =
         src_interval->spill_def;
      spill_split->split.off = split->split.off;
      ir3_instr_move_after(spill_split, split);
      dst_interval->spill_def = dst;
      return;
   }

   dst_interval->physreg_start =
      src_interval->physreg_start + dst->merge_set_offset -
      src->def->merge_set_offset;
   dst_interval->physreg_end = dst_interval->physreg_start + reg_size(dst);
   ir3_reg_interval_insert(&ctx->reg_ctx, &dst_interval->interval);
   src->num = ra_interval_get_num(src_interval);
   dst->num = ra_interval_get_num(dst_interval);
   d("insert dst %u physreg %u", dst->name, dst_interval->physreg_start);

   if (src->flags & IR3_REG_KILL)
      ir3_reg_interval_remove(&ctx->reg_ctx, &src_interval->interval);
}

static void
handle_phi(struct ra_ctx *ctx, struct ir3_instruction *phi)
{
   struct ir3_register *dst = phi->dsts[0];
   
   if (!(dst->flags & IR3_REG_SHARED))
      return;

   struct ra_interval *dst_interval = &ctx->intervals[dst->name];
   ra_interval_init(dst_interval, dst);

   /* In some rare cases, it's possible to have a phi node with a physical-only
    * source. Here's a contrived example:
    *
    * loop {
    *    if non-uniform {
    *       if uniform {
    *          x_1 = ...;
    *          continue;
    *       }
    *       x_2 = ...;
    *    } else {
    *       break;
    *    }
    *    // continue block
    *    x_3 = phi(x_1, x_2)
    * }
    *
    * Assuming x_1 and x_2 are uniform, x_3 will also be uniform, because all
    * threads that stay in the loop take the same branch to the continue block,
    * however execution may fall through from the assignment to x_2 to the
    * break statement because the outer if is non-uniform, and then it will fall
    * through again to the continue block, so if x_3 is to be in a shared reg
    * then the phi needs an extra source pointing to the break statement, which
    * itself needs a phi node:
    *
    * loop {
    *    if non-uniform {
    *       if uniform {
    *          x_1 = ...;
    *          continue;
    *       }
    *       x_2 = ...;
    *    } else {
    *       x_4 = phi(undef, x_2)
    *       break;
    *    }
    *    // continue block
    *    x_3 = phi(x_1, x_2, x_4)
    * }
    */

   /* phi nodes are special because we cannot spill them normally, instead we
    * have to spill the parallel copies that their sources point to and make the
    * entire phi not shared anymore.
    */

   physreg_t physreg = get_reg(ctx, dst, false);
   if (physreg == (physreg_t) ~0u) {
      d("spilling phi destination");
      dst->flags &= ~IR3_REG_SHARED;
      dst_interval->spill_def = dst;
      phi->flags |= IR3_INSTR_SHARED_SPILL;

      foreach_src (src, phi) {
         src->flags &= ~IR3_REG_SHARED;
         if (src->def)
            src->def->flags &= ~IR3_REG_SHARED;
      }

      return;
   }

   dst->num = ra_physreg_to_num(physreg, dst->flags);
   dst_interval->spill_def = NULL;
   dst_interval->physreg_start = physreg;
   dst_interval->physreg_end = physreg + reg_size(dst);
   ir3_reg_interval_insert(&ctx->reg_ctx, &dst_interval->interval);

   ra_foreach_src_n (src, i, phi) {
      /* We assume that any phis with non-logical sources aren't promoted. */
      assert(i < phi->block->predecessors_count);
      src->num = dst->num;
      src->def->num = dst->num;
   }
}

static void
handle_pcopy(struct ra_ctx *ctx, struct ir3_instruction *pcopy)
{
   /* For parallel copies, we only handle the source. The destination is handled
    * later when processing phi nodes.
    */

   ra_foreach_src (src, pcopy)
      mark_src(ctx, src);

   ra_foreach_src (src, pcopy)
      ensure_src_live(ctx, pcopy, src);

   ra_foreach_src_rev (src, pcopy)
      assign_src(ctx, src);

   ra_foreach_src (src, pcopy)
      handle_src_late(ctx, pcopy, src);
}

static void
handle_instr(struct ra_ctx *ctx, struct ir3_instruction *instr)
{
   instr->flags &= ~IR3_INSTR_SHARED_SPILL;

   switch (instr->opc) {
   case OPC_META_SPLIT:
      handle_split(ctx, instr);
      break;
   case OPC_META_PHI:
      handle_phi(ctx, instr);
      break;
   case OPC_META_PARALLEL_COPY:
      handle_pcopy(ctx, instr);
      break;
   default:
      handle_normal_instr(ctx, instr);
   }
}

/* In case we define a value outside a loop, use it inside the loop, then spill
 * it afterwards inside the same loop, we could lose the value so we have to
 * reload it. We have to reload it after any parallel copy instruction, when the
 * live shared registers equal the live-in of the backedge. lower_pcopy() will
 * then move any non-shared parallel copies down past the reload.
 */
static void
reload_live_outs(struct ra_ctx *ctx, struct ir3_block *block)
{
   struct ra_block_state *state = &ctx->blocks[block->index];
   unsigned name;
   BITSET_FOREACH_SET (name, state->live_out, ctx->live->definitions_count) {
      struct ir3_register *reg = ctx->live->definitions[name];

      struct ra_interval *interval = &ctx->intervals[name];
      if (!interval->interval.inserted) {
         d("reloading %d at end of backedge", reg->name);
         reload_interval(ctx, NULL, block, interval);
      }
   }
}

static void
record_pred_live_out(struct ra_ctx *ctx,
                     struct ra_interval *interval,
                     struct ir3_block *pred)
{
   struct ra_block_state *state = &ctx->blocks[pred->index];

   struct ir3_register *def = interval->interval.reg;
   BITSET_SET(state->live_out, def->name);

   rb_tree_foreach (struct ra_interval, child,
                    &interval->interval.children, interval.node) {
      record_pred_live_out(ctx, child, pred);
   }
}

static void
record_pred_live_outs(struct ra_ctx *ctx, struct ir3_block *block)
{
   for (unsigned i = 0; i < block->predecessors_count; i++) {
      struct ir3_block *pred = block->predecessors[i];
      struct ra_block_state *state = &ctx->blocks[pred->index];
      if (state->visited)
         continue;

      state->live_out = rzalloc_array(NULL, BITSET_WORD,
                                      BITSET_WORDS(ctx->live->definitions_count));


      rb_tree_foreach (struct ra_interval, interval,
                       &ctx->reg_ctx.intervals, interval.node) {
         record_pred_live_out(ctx, interval, pred);
      }
   }
}

static void
handle_block(struct ra_ctx *ctx, struct ir3_block *block)
{
   ra_ctx_reset_block(ctx);

   unsigned name;
   BITSET_FOREACH_SET (name, ctx->live->live_in[block->index],
                       ctx->live->definitions_count) {
      struct ir3_register *def = ctx->live->definitions[name];
      struct ra_interval *interval = &ctx->intervals[name];

      /* Non-shared definitions may still be definitions we spilled by demoting
       * them, so we still need to initialize the interval. But we shouldn't
       * make these intervals live.
       */
      ra_interval_init(interval, def);

      if ((def->flags & IR3_REG_SHARED) && !interval->spill_def) {
         ir3_reg_interval_insert(&ctx->reg_ctx, &interval->interval);
      }
   }

   if (RA_DEBUG) {
      d("after live-in block %u:\n", block->index);
      ra_ctx_dump(ctx);
   }

   if (block->predecessors_count > 1)
      record_pred_live_outs(ctx, block);

   foreach_instr (instr, &block->instr_list) {
      di(instr, "processing");

      handle_instr(ctx, instr);

      if (RA_DEBUG)
         ra_ctx_dump(ctx);
   }

   if (block->successors[0]) {
      struct ra_block_state *state = &ctx->blocks[block->successors[0]->index];

      if (state->visited) {
         assert(!block->successors[1]);

         reload_live_outs(ctx, block);
      }
   }

   ctx->blocks[block->index].visited = true;
}

static void
lower_pcopy(struct ir3 *ir, struct ra_ctx *ctx)
{
   foreach_block (block, &ir->block_list) {
      foreach_instr_safe (instr, &block->instr_list) {
         /* At this point due to spilling there may be parallel copies from
          * shared to non-shared registers and vice versa. Lowering these after
          * RA may produce cycles involving shared and non-shared registers,
          * which would need to be resolved by swapping a shared and non-shared
          * register which is something we can't handle. However by lowering
          * these to moves now, we can make sure that cycles only involve
          * non-shared registers. To avoid illegally moving a shared register
          * read or write across the parallel copy, which may have other
          * conflicting reads/writes if there's a cycle, we need to move copies
          * from non-shared to shared below the shared copies, and we need to
          * move copies from shared to non-shared above them. So, we have the
          * following order:
          *
          * 1. shared->non-shared copies (spills)
          * 2. shared->shared copies (one parallel copy as there may be cycles)
          * 3. non-shared->shared copies (reloads)
          * 4. non-shared->non-shared copies
          *
          * We split out the non-shared->non-shared copies as a separate step.
          */
         if (instr->opc == OPC_META_PARALLEL_COPY) {
            for (unsigned i = 0; i < instr->srcs_count; i++) {
               if ((instr->srcs[i]->flags & IR3_REG_SHARED) &&
                   !(instr->dsts[i]->flags & IR3_REG_SHARED)) {
                  /* shared->non-shared. Create a spill move and rewrite the
                   * source to be the destination of the move (so that the
                   * original shared->non-shared copy becomes a
                   * non-shared->non-shared copy).
                   */
                  struct ir3_instruction *mov =
                     ir3_instr_create(block, OPC_MOV, 1, 1);
                  mov->flags |= IR3_INSTR_SHARED_SPILL;
                  struct ir3_register *dst =
                     ir3_dst_create(mov, INVALID_REG, instr->dsts[i]->flags);
                  dst->wrmask = instr->dsts[i]->wrmask;
                  dst->instr = mov;
                  mov->repeat = reg_elems(mov->dsts[0]) - 1;
                  struct ir3_register *src =
                     ir3_src_create(mov, instr->srcs[i]->num,
                                    instr->srcs[i]->flags |
                                    (mov->repeat ? IR3_REG_R : 0));
                  src->wrmask = instr->srcs[i]->wrmask;
                  mov->cat1.dst_type = mov->cat1.src_type =
                     (mov->dsts[0]->flags & IR3_REG_HALF) ? TYPE_U16 : TYPE_U32;
                  instr->srcs[i]->flags = mov->dsts[0]->flags;
                  instr->srcs[i]->def = mov->dsts[0];
                  ir3_instr_move_before(mov, instr);
               }
            }

            for (unsigned i = 0; i < instr->dsts_count;) {
               if ((instr->dsts[i]->flags & IR3_REG_SHARED) &&
                   (instr->srcs[i]->flags & IR3_REG_SSA) &&
                   !(instr->srcs[i]->flags & IR3_REG_SHARED)) {
                  /* non-shared->shared. Create a reload move.
                   */
                  struct ir3_instruction *mov =
                     ir3_instr_create(block, OPC_MOV, 1, 1);
                  mov->flags |= IR3_INSTR_SHARED_SPILL;
                  struct ir3_register *dst =
                     ir3_dst_create(mov, instr->dsts[i]->num,
                                    instr->dsts[i]->flags);
                  dst->instr = mov;
                  dst->wrmask = instr->dsts[i]->wrmask;
                  mov->repeat = reg_elems(mov->dsts[0]) - 1;
                  struct ir3_register *src = 
                     ir3_src_create(mov, INVALID_REG, instr->srcs[i]->flags |
                                    (mov->repeat ? IR3_REG_R : 0));
                  src->def = instr->srcs[i]->def;
                  src->wrmask = instr->srcs[i]->wrmask;
                  mov->cat1.dst_type = mov->cat1.src_type =
                     (mov->dsts[0]->flags & IR3_REG_HALF) ? TYPE_U16 : TYPE_U32;

                  /* When we spill a parallel copy source, we lose the
                   * information of where it originally points to since we make
                   * it point to the spill def. If we later decide not to also
                   * spill the phi associated with it, we have to restore it
                   * here using the stashed original source so that RA
                   * validation can check that we did the correct thing.
                   *
                   * Because SSA-ness goes away after validation, this is really
                   * just about validation.
                   */
                  struct ir3_block *succ = block->successors[0];
                  unsigned pred_idx = ir3_block_get_pred_index(succ, block);
                  foreach_instr (phi, &succ->instr_list) {
                     if (phi->opc != OPC_META_PHI)
                        break;

                     if (phi->srcs[pred_idx]->def == instr->dsts[i]) {
                        struct ir3_register *def =
                           _mesa_hash_table_search(ctx->pcopy_src_map,
                                                   instr->srcs[i])->data;
                        phi->srcs[pred_idx]->def = def;
                        break;
                     }
                  }

                  instr->srcs[i] = instr->srcs[instr->srcs_count - 1];
                  instr->dsts[i] = instr->dsts[instr->dsts_count - 1];
                  instr->srcs_count--;
                  instr->dsts_count--;
                  ir3_instr_move_after(mov, instr);
                  continue;
               }

               i++;
            }

            /* Move any non-shared copies to a separate parallel copy
             * instruction right at the end of the block, after any reloads. At
             * this point all copies should be {shared,immediate}->shared or
             * {non-shared,immediate}->non-shared. 
             */
            unsigned non_shared_copies = 0;
            for (unsigned i = 0; i < instr->dsts_count; i++) {
               if (!(instr->dsts[i]->flags & IR3_REG_SHARED))
                  non_shared_copies++;
            }

            if (non_shared_copies != 0) {
               struct ir3_instruction *pcopy =
                  ir3_instr_create(block, OPC_META_PARALLEL_COPY,
                                   non_shared_copies, non_shared_copies);

               unsigned j = 0;
               for (unsigned i = 0; i < instr->dsts_count;) {
                  if (!(instr->dsts[i]->flags & IR3_REG_SHARED)) {
                     pcopy->dsts[j] = instr->dsts[i];
                     pcopy->srcs[j] = instr->srcs[i];
                     pcopy->dsts[j]->instr = pcopy;
                     instr->srcs[i] = instr->srcs[instr->srcs_count - 1];
                     instr->dsts[i] = instr->dsts[instr->dsts_count - 1];
                     instr->srcs_count--;
                     instr->dsts_count--;
                     j++;
                     continue;
                  }
                  i++;
               }

               pcopy->srcs_count = pcopy->dsts_count = j;
               if (instr->dsts_count == 0)
                  list_del(&instr->node);
            }
         }
      }
   }
}

static void
finalize(struct ir3 *ir)
{
   foreach_block (block, &ir->block_list) {
      foreach_instr (instr, &block->instr_list) {
         for (unsigned i = 0; i < instr->dsts_count; i++) {
            if (instr->dsts[i]->flags & IR3_REG_SHARED) {
               instr->dsts[i]->flags &= ~IR3_REG_SSA;
            }
         }

         for (unsigned i = 0; i < instr->srcs_count; i++) {
            if (instr->srcs[i]->flags & IR3_REG_SHARED) {
               instr->srcs[i]->flags &= ~IR3_REG_SSA;
               instr->srcs[i]->def = NULL;
            }
         }
      }
   }
}

void
ir3_ra_shared(struct ir3_shader_variant *v, struct ir3_liveness **live_ptr)
{
   struct ra_ctx ctx;
   struct ir3_liveness *live = *live_ptr;

   ra_ctx_init(&ctx);
   ctx.intervals = rzalloc_array(NULL, struct ra_interval,
                                 live->definitions_count);
   ctx.blocks = rzalloc_array(NULL, struct ra_block_state,
                              live->block_count);
   ctx.start = 0;
   ctx.live = live;
   ctx.pcopy_src_map = _mesa_pointer_hash_table_create(NULL);

   foreach_block (block, &v->ir->block_list) {
      handle_block(&ctx, block);
   }

   lower_pcopy(v->ir, &ctx);

   for (unsigned i = 0; i < live->block_count; i++) {
      if (ctx.blocks[i].live_out)
         ralloc_free(ctx.blocks[i].live_out);
   }

   ralloc_free(ctx.intervals);
   ralloc_free(ctx.pcopy_src_map);
   ralloc_free(ctx.blocks);

   ir3_ra_validate(v, RA_FULL_SIZE, RA_HALF_SIZE, live->block_count, true);
   finalize(v->ir);

   /* Recalculate liveness and register pressure now that additional values have
    * been added.
    * TODO we should only do this if any values have been spilled/reloaded.
    * Note: since we don't have to recreate merge sets, we have to manually copy
    * interval_offset to the new liveness struct.
    */
   unsigned interval_offset = live->interval_offset;
   void *live_mem_ctx = ralloc_parent(live);
   ralloc_free(live);
   *live_ptr = ir3_calc_liveness(live_mem_ctx, v->ir);
   (*live_ptr)->interval_offset = interval_offset;
}

