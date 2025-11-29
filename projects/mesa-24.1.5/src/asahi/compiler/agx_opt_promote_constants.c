/*
 * Copyright 2023 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include "util/bitset.h"
#include "util/hash_table.h"
#include "util/ralloc.h"
#include "agx_compiler.h"
#include "agx_opcodes.h"

/*
 * Information about a constant, indexed by its 64-bit value. This describes the
 * value, not the move that generated it. If there are multiple moves in the
 * shader with the same immediate value, they resolve to the same constant.
 */
struct constant_info {
   uint64_t value;

   /* Number of uses of the constant that could be promoted */
   unsigned nr_promotable_uses;

   /* If we push, the uniform used */
   uint16_t uniform;

   /* Alignment in 16-bit units needed for the constant */
   uint8_t align_16;

   /* True if the constant was promoted to a uniform */
   bool promoted;
};

/*
 * Choosing constants to promote is similar to the 0-1 knapsack problem. We use
 * a well-known heuristic: sort by benefit divided by size. We approximate
 * benefit by use count.
 */
static int
constant_priority(const struct constant_info *const info)
{
   int size = info->align_16;
   assert(size == 1 || size == 2 || size == 4);
   int inverse_size = (size == 1) ? 4 : (size == 2) ? 2 : 1;

   return info->nr_promotable_uses * inverse_size;
}

static int
priority_compare(const void *A_, const void *B_)
{
   const struct constant_info *const *A = A_;
   const struct constant_info *const *B = B_;

   /* This is backwards from qsort's documentation, because we want descending
    * order and qsort returns ascending.
    */
   return constant_priority(*B) - constant_priority(*A);
}

static void
record_use(void *memctx, struct hash_table_u64 *constants, uint64_t imm,
           enum agx_size size)
{
   struct constant_info *info = _mesa_hash_table_u64_search(constants, imm);

   if (!info) {
      info = rzalloc(memctx, struct constant_info);
      info->value = imm;
      _mesa_hash_table_u64_insert(constants, imm, info);
   }

   info->nr_promotable_uses++;
   info->align_16 = MAX2(info->align_16, agx_size_align_16(size));
}

static void
pass(agx_context *ctx, void *memctx)
{
   /* Map from SSA indices to struct constant_info */
   struct hash_table_u64 *constants = _mesa_hash_table_u64_create(memctx);

   /* Map from SSA indices to immediate values */
   uint64_t *values = rzalloc_array(memctx, uint64_t, ctx->alloc);

   /* Set of SSA indices that map to immediate values */
   BITSET_WORD *is_immediate =
      rzalloc_array(memctx, BITSET_WORD, BITSET_WORDS(ctx->alloc));

   /* Gather constant definitions and use */
   agx_foreach_instr_global(ctx, I) {
      if (I->op == AGX_OPCODE_MOV_IMM) {
         assert(I->dest[0].type == AGX_INDEX_NORMAL);
         BITSET_SET(is_immediate, I->dest[0].value);
         values[I->dest[0].value] = I->imm;
      } else {
         agx_foreach_ssa_src(I, s) {
            if (BITSET_TEST(is_immediate, I->src[s].value) &&
                agx_instr_accepts_uniform(I->op, s, ctx->out->push_count,
                                          I->src[s].size)) {

               record_use(memctx, constants, values[I->src[s].value],
                          I->src[s].size);
            }
         }
      }
   }

   /* Early exit if there were no constants */
   unsigned nr_nodes = _mesa_hash_table_u64_num_entries(constants);
   if (nr_nodes == 0)
      return;

   /* Collect nodes that are promotable */
   struct constant_info **flat =
      rzalloc_array(memctx, struct constant_info *, nr_nodes);

   unsigned flat_count = 0;
   hash_table_u64_foreach(constants, entry) {
      flat[flat_count++] = entry.data;
   }

   /* Select constants. Even when we can promote everything, sorting keeps hot
    * constants in lower uniforms, required by some instructions.
    */
   qsort(flat, flat_count, sizeof(*flat), priority_compare);

   ctx->out->immediate_base_uniform = ctx->out->push_count;

   /* Promote as many constants as we can */
   for (unsigned i = 0; i < flat_count; ++i) {
      struct constant_info *info = flat[i];
      assert(info->nr_promotable_uses > 0);

      /* Try to assign a uniform */
      unsigned uniform = ALIGN_POT(ctx->out->push_count, info->align_16);
      unsigned new_count = uniform + info->align_16;
      if (new_count > AGX_NUM_UNIFORMS)
         break;

      info->uniform = uniform;
      info->promoted = true;
      ctx->out->push_count = new_count;

      unsigned size_B = info->align_16 * 2;
      memcpy(&ctx->out->immediates[uniform - ctx->out->immediate_base_uniform],
             &info->value, size_B);

      ctx->out->immediate_size_16 =
         new_count - ctx->out->immediate_base_uniform;
   }

   /* Promote in the IR */
   agx_foreach_instr_global(ctx, I) {
      agx_foreach_ssa_src(I, s) {
         if (!BITSET_TEST(is_immediate, I->src[s].value))
            continue;

         struct constant_info *info =
            _mesa_hash_table_u64_search(constants, values[I->src[s].value]);

         if (info && info->promoted &&
             agx_instr_accepts_uniform(I->op, s, info->uniform,
                                       I->src[s].size)) {

            agx_replace_src(I, s, agx_uniform(info->uniform, I->src[s].size));
         }
      }
   }
}

void
agx_opt_promote_constants(agx_context *ctx)
{
   /* We do not promote constants in preambles since it's pointless and wastes
    * uniform slots.
    */
   if (ctx->is_preamble)
      return;

   void *memctx = ralloc_context(NULL);
   pass(ctx, memctx);
   ralloc_free(memctx);
}
