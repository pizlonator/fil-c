/*
 * Copyright 2023 Alyssa Rosenzweig
 * Copyright 2023 Valve Corporation
 * Copyright 2022 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

/*
 * Implementation of "Simple and Efficient
 * Construction of Static Single Assignment Form", also by Braun et al.
 * https://link.springer.com/content/pdf/10.1007/978-3-642-37051-9_6.pdf
 */

#include "util/hash_table.h"
#include "util/ralloc.h"
#include "util/u_dynarray.h"
#include "agx_builder.h"
#include "agx_compiler.h"
#include "agx_opcodes.h"

struct repair_block {
   /* For a loop header, whether phis operands have been added */
   bool sealed;

   /* Sparse map: variable name -> agx_index.
    *
    * Definition of a variable at the end of the block.
    */
   struct hash_table_u64 *defs;
};

struct repair_ctx {
   agx_context *shader;

   /* Number of variables */
   unsigned n;

   /* Information on blocks indexed in source order */
   struct repair_block *blocks;
};

static inline struct repair_block *
repair_block(struct repair_ctx *ctx, agx_block *block)
{
   return &ctx->blocks[block->index];
}

static void
record_write(struct repair_ctx *ctx, agx_block *block, unsigned node,
             agx_index val)
{
   assert(node < ctx->n);
   struct hash_table_u64 *defs = repair_block(ctx, block)->defs;
   _mesa_hash_table_u64_insert(defs, node,
                               ralloc_memdup(defs, &val, sizeof(val)));
}

static void add_phi_operands(struct repair_ctx *ctx, agx_block *block,
                             agx_instr *phi, agx_index node);

static agx_index
resolve_read(struct repair_ctx *ctx, agx_block *block, agx_index node)
{
   struct repair_block *rb = repair_block(ctx, block);

   /* Local value numbering */
   assert(node.type == AGX_INDEX_NORMAL);
   agx_index *local = _mesa_hash_table_u64_search(rb->defs, node.value);

   if (local) {
      assert(!agx_is_null(*local));
      return *local;
   }

   /* Global value numbering. readValueRecursive in the paper */
   unsigned nr_preds = agx_num_predecessors(block);
   agx_index val;

   assert(nr_preds > 0);

   /* Loop headers are not in the "sealedBlock" set in the paper. To
    * handle, we insert an incomplete phi to be filled in after the rest of
    * the loop is processed.
    */
   if (block->loop_header && !rb->sealed) {
      val = agx_temp_like(ctx->shader, node);
      agx_builder b = agx_init_builder(ctx->shader, agx_before_block(block));
      agx_instr *phi = agx_phi_to(&b, val, nr_preds);
      phi->shadow = true;

      /* Stash the variable in for an intrusive incompletePhis map */
      phi->imm = node.value + 1;
   } else if (nr_preds == 1) {
      /* No phi needed */
      agx_block *pred =
         *util_dynarray_element(&block->predecessors, agx_block *, 0);
      val = resolve_read(ctx, pred, node);
   } else {
      /* Insert phi first to break cycles */
      val = agx_temp_like(ctx->shader, node);
      agx_builder b = agx_init_builder(ctx->shader, agx_before_block(block));
      agx_instr *phi = agx_phi_to(&b, val, nr_preds);
      phi->shadow = true;
      record_write(ctx, block, node.value, val);
      add_phi_operands(ctx, block, phi, node);
   }

   assert(!agx_is_null(val));
   record_write(ctx, block, node.value, val);
   return val;
}

static void
add_phi_operands(struct repair_ctx *ctx, agx_block *block, agx_instr *phi,
                 agx_index node)
{
   /* Add phi operands */
   agx_foreach_predecessor(block, pred) {
      unsigned s = agx_predecessor_index(block, *pred);
      phi->src[s] = resolve_read(ctx, *pred, node);
   }
}

static void
seal_block(struct repair_ctx *ctx, agx_block *block)
{
   agx_foreach_phi_in_block(block, phi) {
      /* We use phi->imm as a sideband to pass the variable name. */
      if (phi->imm) {
         agx_index var = agx_get_vec_index(phi->imm - 1, phi->dest[0].size,
                                           agx_channels(phi->dest[0]));
         var.memory = phi->dest[0].memory;
         add_phi_operands(ctx, block, phi, var);
         phi->imm = 0;
      }
   }

   repair_block(ctx, block)->sealed = true;
}

static void
seal_loop_headers(struct repair_ctx *ctx, struct agx_block *exit)
{
   agx_foreach_successor(exit, succ) {
      /* Only loop headers need to be sealed late */
      if (!succ->loop_header)
         continue;

      /* Check if all predecessors have been processed */
      bool any_unprocessed = false;

      agx_foreach_predecessor(succ, P) {
         if ((*P)->index > exit->index) {
            any_unprocessed = true;
            break;
         }
      }

      /* Seal once all predecessors are processed */
      if (!any_unprocessed)
         seal_block(ctx, succ);
   }
}

static void
agx_opt_trivial_phi(agx_context *ctx)
{
   agx_index *remap = calloc(ctx->alloc, sizeof(*remap));
   for (;;) {
      bool progress = false;
      memset(remap, 0, ctx->alloc * sizeof(*remap));

      agx_foreach_block(ctx, block) {
         agx_foreach_phi_in_block_safe(block, phi) {
            agx_index same = agx_null();
            bool all_same = true;

            agx_foreach_src(phi, s) {
               /* TODO: Handle cycles faster */
               if (!agx_is_null(remap[phi->src[s].value])) {
                  all_same = false;
                  break;
               }

               /* Same value or self-reference */
               if (agx_is_equiv(phi->src[s], same) ||
                   agx_is_equiv(phi->src[s], phi->dest[0]))
                  continue;

               if (!agx_is_null(same)) {
                  all_same = false;
                  break;
               }

               same = phi->src[s];
            }

            if (all_same) {
               remap[phi->dest[0].value] = same;
               agx_remove_instruction(phi);
               progress = true;
            }
         }
      }

      if (!progress)
         break;

      agx_foreach_instr_global(ctx, I) {
         agx_foreach_ssa_src(I, s) {
            if (!agx_is_null(remap[I->src[s].value])) {
               agx_replace_src(I, s, remap[I->src[s].value]);
            }
         }
      }
   }

   free(remap);
}

void
agx_repair_ssa(agx_context *ctx)
{
   struct repair_block *blocks =
      rzalloc_array(NULL, struct repair_block, ctx->num_blocks);

   agx_foreach_block(ctx, block) {
      struct repair_block *rb = &blocks[block->index];
      rb->defs = _mesa_hash_table_u64_create(blocks);
   }

   unsigned n = ctx->alloc;

   agx_foreach_block(ctx, block) {
      struct repair_ctx rctx = {
         .shader = ctx,
         .n = n,
         .blocks = blocks,
      };

      agx_foreach_instr_in_block(block, I) {
         /* Repair SSA for the instruction */
         if (I->op != AGX_OPCODE_PHI) {
            agx_foreach_ssa_src(I, s) {
               assert(I->src[s].value < n);
               agx_replace_src(I, s, resolve_read(&rctx, block, I->src[s]));
            }
         }

         agx_foreach_ssa_dest(I, d) {
            unsigned handle = I->dest[d].value;

            /* Skip phis that we just created when processing loops */
            if (handle >= n) {
               assert(I->op == AGX_OPCODE_PHI);
               continue;
            }

            I->dest[d] =
               agx_replace_index(I->dest[d], agx_temp_like(ctx, I->dest[d]));

            record_write(&rctx, block, handle, I->dest[d]);
         }
      }

      seal_loop_headers(&rctx, block);
   }

   agx_foreach_block(ctx, block) {
      agx_foreach_phi_in_block(block, phi) {
         /* The kill bit is invalid (and meaningless) for phis. Liveness
          * analysis does not produce it. However, we're ingesting broken SSA
          * where we can have random kill bits set on phis. Strip them as part
          * of the SSA repair.
          *
          * The register allocator depends on this for correctness.
          */
         phi->dest[0].kill = false;

         agx_foreach_src(phi, s) {
            phi->src[s].kill = false;
         }

         /* Skip the phis that we just created */
         if (phi->shadow) {
            phi->shadow = false;
            continue;
         }

         agx_foreach_ssa_src(phi, s) {
            /* Phis (uniquely) read their sources in their corresponding
             * predecessors, so chain through for that.
             */
            agx_block *read_block =
               *util_dynarray_element(&block->predecessors, agx_block *, s);

            assert(phi->src[s].value < n);

            struct repair_ctx rctx = {
               .shader = ctx,
               .n = n,
               .blocks = blocks,
            };

            agx_replace_src(phi, s,
                            resolve_read(&rctx, read_block, phi->src[s]));
         }
      }
   }

   ralloc_free(blocks);

   agx_opt_trivial_phi(ctx);
   agx_reindex_ssa(ctx);
}
