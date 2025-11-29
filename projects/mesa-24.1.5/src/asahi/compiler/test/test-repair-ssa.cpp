/*
 * Copyright 2024 Alyssa Rosenzweig
 * Copyright 2022 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "agx_builder.h"
#include "agx_compiler.h"
#include "agx_test.h"

#include <gtest/gtest.h>

static void
pass(agx_context *ctx)
{
   agx_repair_ssa(ctx);
   agx_reindex_ssa(ctx);
}

#define CASE(instr)                                                            \
   INSTRUCTION_CASE(                                                           \
      {                                                                        \
         bool repaired = false;                                                \
         instr                                                                 \
      },                                                                       \
      {                                                                        \
         bool repaired = true;                                                 \
         instr                                                                 \
      },                                                                       \
      pass)

class RepairSSA : public testing::Test {
 protected:
   RepairSSA()
   {
      mem_ctx = ralloc_context(NULL);
   }

   ~RepairSSA()
   {
      ralloc_free(mem_ctx);
   }

   void *mem_ctx;
};

static agx_index
agx_phi_2(agx_builder *b, agx_index x, agx_index y)
{
   agx_index idx = agx_temp(b->shader, x.size);
   agx_instr *phi = agx_phi_to(b, idx, 2);
   phi->src[0] = x;
   phi->src[1] = y;
   return idx;
}

TEST_F(RepairSSA, Local)
{
   CASE({
      agx_index x = agx_mov_imm(b, AGX_SIZE_32, 0xcafe);
      agx_index y = agx_mov_imm(b, AGX_SIZE_32, 0xefac);

      if (repaired) {
         agx_unit_test(b, agx_fadd(b, y, x));
      } else {
         agx_fadd_to(b, x, y, x);
         agx_unit_test(b, x);
      }
   });
}

/*      A
 *     / \
 *    B   C
 *     \ /
 *      D
 */
TEST_F(RepairSSA, IfElse)
{
   CASE({
      agx_block *A = agx_start_block(b->shader);
      agx_block *B = agx_test_block(b->shader);
      agx_block *C = agx_test_block(b->shader);
      agx_block *D = agx_test_block(b->shader);

      agx_block_add_successor(A, B);
      agx_block_add_successor(A, C);

      agx_block_add_successor(B, D);
      agx_block_add_successor(C, D);

      b->cursor = agx_after_block(B);
      agx_index x = agx_mov_imm(b, 32, 0xcafe);
      agx_index y = agx_mov_imm(b, 32, 0xbade);

      b->cursor = agx_after_block(C);
      agx_index x2 = repaired ? agx_temp(b->shader, AGX_SIZE_32) : x;
      agx_mov_imm_to(b, x2, 0xefac);
      agx_index y2 = agx_mov_imm(b, 32, 0xbade);

      b->cursor = agx_after_block(D);
      if (repaired)
         x = agx_phi_2(b, x, x2);

      agx_index y3 = agx_phi_2(b, y, y2);
      agx_unit_test(b, agx_fadd(b, x, y3));
   });
}
