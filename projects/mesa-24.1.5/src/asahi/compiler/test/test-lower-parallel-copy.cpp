/*
 * Copyright 2021 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "agx_builder.h"
#include "agx_compiler.h"
#include "agx_test.h"

#include <gtest/gtest.h>

#define CASE(copies, expected)                                                 \
   do {                                                                        \
      agx_builder *A = agx_test_builder(mem_ctx);                              \
      agx_builder *B = agx_test_builder(mem_ctx);                              \
                                                                               \
      A->shader->scratch_size = 2000;                                          \
      agx_emit_parallel_copies(A, copies, ARRAY_SIZE(copies));                 \
                                                                               \
      {                                                                        \
         agx_builder *b = B;                                                   \
         expected;                                                             \
      }                                                                        \
                                                                               \
      ASSERT_SHADER_EQUAL(A->shader, B->shader);                               \
   } while (0)

static inline void
extr_swap(agx_builder *b, agx_index x)
{
   x.size = AGX_SIZE_32;
   agx_extr_to(b, x, x, x, agx_immediate(16), 0);
}

static inline void
xor_swap(agx_builder *b, agx_index x, agx_index y)
{
   agx_xor_to(b, x, x, y);
   agx_xor_to(b, y, x, y);
   agx_xor_to(b, x, x, y);
}

class LowerParallelCopy : public testing::Test {
 protected:
   LowerParallelCopy()
   {
      mem_ctx = ralloc_context(NULL);
   }

   ~LowerParallelCopy()
   {
      ralloc_free(mem_ctx);
   }

   void *mem_ctx;
};

TEST_F(LowerParallelCopy, UnrelatedCopies)
{
   struct agx_copy test_1[] = {
      {.dest = 0, .src = agx_register(2, AGX_SIZE_32)},
      {.dest = 4, .src = agx_register(6, AGX_SIZE_32)},
   };

   CASE(test_1, {
      agx_mov_to(b, agx_register(0, AGX_SIZE_32), agx_register(2, AGX_SIZE_32));
      agx_mov_to(b, agx_register(4, AGX_SIZE_32), agx_register(6, AGX_SIZE_32));
   });

   struct agx_copy test_2[] = {
      {.dest = 0, .src = agx_register(1, AGX_SIZE_16)},
      {.dest = 4, .src = agx_register(5, AGX_SIZE_16)},
   };

   CASE(test_2, {
      agx_mov_to(b, agx_register(0, AGX_SIZE_16), agx_register(1, AGX_SIZE_16));
      agx_mov_to(b, agx_register(4, AGX_SIZE_16), agx_register(5, AGX_SIZE_16));
   });
}

TEST_F(LowerParallelCopy, RelatedSource)
{
   struct agx_copy test_1[] = {
      {.dest = 0, .src = agx_register(2, AGX_SIZE_32)},
      {.dest = 4, .src = agx_register(2, AGX_SIZE_32)},
   };

   CASE(test_1, {
      agx_mov_to(b, agx_register(0, AGX_SIZE_32), agx_register(2, AGX_SIZE_32));
      agx_mov_to(b, agx_register(4, AGX_SIZE_32), agx_register(2, AGX_SIZE_32));
   });

   struct agx_copy test_2[] = {
      {.dest = 0, .src = agx_register(1, AGX_SIZE_16)},
      {.dest = 4, .src = agx_register(1, AGX_SIZE_16)},
   };

   CASE(test_2, {
      agx_mov_to(b, agx_register(0, AGX_SIZE_16), agx_register(1, AGX_SIZE_16));
      agx_mov_to(b, agx_register(4, AGX_SIZE_16), agx_register(1, AGX_SIZE_16));
   });
}

TEST_F(LowerParallelCopy, DependentCopies)
{
   struct agx_copy test_1[] = {
      {.dest = 0, .src = agx_register(2, AGX_SIZE_32)},
      {.dest = 4, .src = agx_register(0, AGX_SIZE_32)},
   };

   CASE(test_1, {
      agx_mov_to(b, agx_register(4, AGX_SIZE_32), agx_register(0, AGX_SIZE_32));
      agx_mov_to(b, agx_register(0, AGX_SIZE_32), agx_register(2, AGX_SIZE_32));
   });

   struct agx_copy test_2[] = {
      {.dest = 0, .src = agx_register(1, AGX_SIZE_16)},
      {.dest = 4, .src = agx_register(0, AGX_SIZE_16)},
   };

   CASE(test_2, {
      agx_mov_to(b, agx_register(4, AGX_SIZE_16), agx_register(0, AGX_SIZE_16));
      agx_mov_to(b, agx_register(0, AGX_SIZE_16), agx_register(1, AGX_SIZE_16));
   });
}

TEST_F(LowerParallelCopy, ManyDependentCopies)
{
   struct agx_copy test_1[] = {
      {.dest = 0, .src = agx_register(2, AGX_SIZE_32)},
      {.dest = 4, .src = agx_register(0, AGX_SIZE_32)},
      {.dest = 8, .src = agx_register(6, AGX_SIZE_32)},
      {.dest = 6, .src = agx_register(4, AGX_SIZE_32)},
   };

   CASE(test_1, {
      agx_mov_to(b, agx_register(8, AGX_SIZE_32), agx_register(6, AGX_SIZE_32));
      agx_mov_to(b, agx_register(6, AGX_SIZE_32), agx_register(4, AGX_SIZE_32));
      agx_mov_to(b, agx_register(4, AGX_SIZE_32), agx_register(0, AGX_SIZE_32));
      agx_mov_to(b, agx_register(0, AGX_SIZE_32), agx_register(2, AGX_SIZE_32));
   });

   struct agx_copy test_2[] = {
      {.dest = 0, .src = agx_register(1, AGX_SIZE_16)},
      {.dest = 2, .src = agx_register(0, AGX_SIZE_16)},
      {.dest = 4, .src = agx_register(3, AGX_SIZE_16)},
      {.dest = 3, .src = agx_register(2, AGX_SIZE_16)},
   };

   CASE(test_2, {
      agx_mov_to(b, agx_register(4, AGX_SIZE_16), agx_register(3, AGX_SIZE_16));
      agx_mov_to(b, agx_register(3, AGX_SIZE_16), agx_register(2, AGX_SIZE_16));
      agx_mov_to(b, agx_register(2, AGX_SIZE_16), agx_register(0, AGX_SIZE_16));
      agx_mov_to(b, agx_register(0, AGX_SIZE_16), agx_register(1, AGX_SIZE_16));
   });
}

TEST_F(LowerParallelCopy, Swap)
{
   struct agx_copy test_1[] = {
      {.dest = 0, .src = agx_register(2, AGX_SIZE_32)},
      {.dest = 2, .src = agx_register(0, AGX_SIZE_32)},
   };

   CASE(test_1, {
      xor_swap(b, agx_register(0, AGX_SIZE_32), agx_register(2, AGX_SIZE_32));
   });

   struct agx_copy test_2[] = {
      {.dest = 0, .src = agx_register(1, AGX_SIZE_16)},
      {.dest = 1, .src = agx_register(0, AGX_SIZE_16)},
   };

   CASE(test_2, { extr_swap(b, agx_register(0, AGX_SIZE_16)); });
}

TEST_F(LowerParallelCopy, Cycle3)
{
   struct agx_copy test[] = {
      {.dest = 0, .src = agx_register(1, AGX_SIZE_16)},
      {.dest = 1, .src = agx_register(2, AGX_SIZE_16)},
      {.dest = 2, .src = agx_register(0, AGX_SIZE_16)},
   };

   CASE(test, {
      extr_swap(b, agx_register(0, AGX_SIZE_16));
      xor_swap(b, agx_register(1, AGX_SIZE_16), agx_register(2, AGX_SIZE_16));
   });
}

TEST_F(LowerParallelCopy, Immediate64)
{
   agx_index imm = agx_immediate(10);
   imm.size = AGX_SIZE_64;

   struct agx_copy test_1[] = {
      {.dest = 4, .src = imm},
   };

   CASE(test_1, {
      agx_mov_imm_to(b, agx_register(4, AGX_SIZE_32), 10);
      agx_mov_imm_to(b, agx_register(6, AGX_SIZE_32), 0);
   });
}

/* Test case from Hack et al */
TEST_F(LowerParallelCopy, TwoSwaps)
{
   struct agx_copy test[] = {
      {.dest = 4, .src = agx_register(2, AGX_SIZE_32)},
      {.dest = 6, .src = agx_register(4, AGX_SIZE_32)},
      {.dest = 2, .src = agx_register(6, AGX_SIZE_32)},
      {.dest = 8, .src = agx_register(8, AGX_SIZE_32)},
   };

   CASE(test, {
      xor_swap(b, agx_register(4, AGX_SIZE_32), agx_register(2, AGX_SIZE_32));
      xor_swap(b, agx_register(6, AGX_SIZE_32), agx_register(2, AGX_SIZE_32));
   });
}

TEST_F(LowerParallelCopy, VectorizeAlignedHalfRegs)
{
   struct agx_copy test[] = {
      {.dest = 0, .src = agx_register(10, AGX_SIZE_16)},
      {.dest = 1, .src = agx_register(11, AGX_SIZE_16)},
      {.dest = 2, .src = agx_uniform(8, AGX_SIZE_16)},
      {.dest = 3, .src = agx_uniform(9, AGX_SIZE_16)},
   };

   CASE(test, {
      agx_mov_to(b, agx_register(0, AGX_SIZE_32),
                 agx_register(10, AGX_SIZE_32));
      agx_mov_to(b, agx_register(2, AGX_SIZE_32), agx_uniform(8, AGX_SIZE_32));
   });
}

TEST_F(LowerParallelCopy, StackCopies)
{
   struct agx_copy test[] = {
      {.dest = 21, .dest_mem = true, .src = agx_register(20, AGX_SIZE_16)},
      {.dest = 22, .dest_mem = true, .src = agx_register(22, AGX_SIZE_32)},
      {.dest = 0, .src = agx_memory_register(10, AGX_SIZE_16)},
      {.dest = 1, .src = agx_memory_register(11, AGX_SIZE_16)},
      {.dest = 0, .dest_mem = true, .src = agx_memory_register(12, AGX_SIZE_16)},
      {.dest = 1, .dest_mem = true, .src = agx_memory_register(13, AGX_SIZE_16)},
      {.dest = 2,
       .dest_mem = true,
       .src = agx_memory_register(804, AGX_SIZE_32)},
      {.dest = 804,
       .dest_mem = true,
       .src = agx_memory_register(2, AGX_SIZE_32)},
      {.dest = 807,
       .dest_mem = true,
       .src = agx_memory_register(808, AGX_SIZE_16)},
      {.dest = 808,
       .dest_mem = true,
       .src = agx_memory_register(807, AGX_SIZE_16)},
   };

   CASE(test, {
      /* Vectorized fill */
      agx_mov_to(b, agx_register(0, AGX_SIZE_32),
                 agx_memory_register(10, AGX_SIZE_32));

      /* Regular spills */
      agx_mov_to(b, agx_memory_register(21, AGX_SIZE_16),
                 agx_register(20, AGX_SIZE_16));
      agx_mov_to(b, agx_memory_register(22, AGX_SIZE_32),
                 agx_register(22, AGX_SIZE_32));

      /* Vectorized stack->stack copy */
      agx_mov_to(b, agx_memory_register(1000, AGX_SIZE_32),
                 agx_register(0, AGX_SIZE_32));

      agx_mov_to(b, agx_register(0, AGX_SIZE_32),
                 agx_memory_register(12, AGX_SIZE_32));

      agx_mov_to(b, agx_memory_register(0, AGX_SIZE_32),
                 agx_register(0, AGX_SIZE_32));

      agx_mov_to(b, agx_register(0, AGX_SIZE_32),
                 agx_memory_register(1000, AGX_SIZE_32));

      /* Stack swap: 32-bit */
      agx_index temp1 = agx_register(0, AGX_SIZE_32);
      agx_index temp2 = agx_register(2, AGX_SIZE_32);
      agx_index spilled_gpr_vec2 = agx_register(0, AGX_SIZE_32);
      agx_index scratch_vec2 = agx_memory_register(1000, AGX_SIZE_32);
      spilled_gpr_vec2.channels_m1++;
      scratch_vec2.channels_m1++;

      agx_mov_to(b, scratch_vec2, spilled_gpr_vec2);
      agx_mov_to(b, temp1, agx_memory_register(2, AGX_SIZE_32));
      agx_mov_to(b, temp2, agx_memory_register(804, AGX_SIZE_32));
      agx_mov_to(b, agx_memory_register(804, AGX_SIZE_32), temp1);
      agx_mov_to(b, agx_memory_register(2, AGX_SIZE_32), temp2);
      agx_mov_to(b, spilled_gpr_vec2, scratch_vec2);

      /* Stack swap: 16-bit */
      spilled_gpr_vec2.size = AGX_SIZE_16;
      scratch_vec2.size = AGX_SIZE_16;
      temp1.size = AGX_SIZE_16;
      temp2.size = AGX_SIZE_16;
      temp2.value = 1;

      agx_mov_to(b, scratch_vec2, spilled_gpr_vec2);
      agx_mov_to(b, temp1, agx_memory_register(807, AGX_SIZE_16));
      agx_mov_to(b, temp2, agx_memory_register(808, AGX_SIZE_16));
      agx_mov_to(b, agx_memory_register(808, AGX_SIZE_16), temp1);
      agx_mov_to(b, agx_memory_register(807, AGX_SIZE_16), temp2);
      agx_mov_to(b, spilled_gpr_vec2, scratch_vec2);
   });
}

#if 0
TEST_F(LowerParallelCopy, LooksLikeASwap) {
   struct agx_copy test[] = {
        { .dest = 0, .src = agx_register(2, AGX_SIZE_32) },
        { .dest = 2, .src = agx_register(0, AGX_SIZE_32) },
        { .dest = 4, .src = agx_register(2, AGX_SIZE_32) },
   };

   CASE(test, {
         agx_mov_to(b, agx_register(4, AGX_SIZE_32), agx_register(2, AGX_SIZE_32));
         agx_mov_to(b, agx_register(2, AGX_SIZE_32), agx_register(0, AGX_SIZE_32));
         agx_mov_to(b, agx_register(0, AGX_SIZE_32), agx_register(4, AGX_SIZE_32));
   });
}
#endif
