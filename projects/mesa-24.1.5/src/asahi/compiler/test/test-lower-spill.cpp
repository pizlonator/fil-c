/*
 * Copyright 2023 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#include "agx_builder.h"
#include "agx_compile.h"
#include "agx_compiler.h"
#include "agx_test.h"

#include "util/macros.h"
#include <gtest/gtest.h>

#define CASE(instr, expected)                                                  \
   do {                                                                        \
      agx_builder *A = agx_test_builder(mem_ctx);                              \
      agx_builder *B = agx_test_builder(mem_ctx);                              \
      {                                                                        \
         agx_builder *b = A;                                                   \
         instr;                                                                \
      }                                                                        \
      {                                                                        \
         agx_builder *b = B;                                                   \
         expected;                                                             \
      }                                                                        \
      agx_lower_spill(A->shader);                                              \
      ASSERT_SHADER_EQUAL(A->shader, B->shader);                               \
   } while (0)

class LowerSpill : public testing::Test {
 protected:
   LowerSpill()
   {
      mem_ctx = ralloc_context(NULL);

      wx = agx_register(0, AGX_SIZE_32);
      hy = agx_register(2, AGX_SIZE_16);

      mw4 = agx_memory_register(0, AGX_SIZE_32);
      mh4 = agx_memory_register(0, AGX_SIZE_16);
      mw4.channels_m1 = 4 - 1;
      mh4.channels_m1 = 4 - 1;

      wx4 = wx;
      wx4.channels_m1 = 4 - 1;

      hy4 = hy;
      hy4.channels_m1 = 4 - 1;
   }

   ~LowerSpill()
   {
      ralloc_free(mem_ctx);
   }

   void *mem_ctx;
   agx_index wx, hy, wx4, hy4;
   agx_index mw4, mh4;

   unsigned scalar = BITFIELD_MASK(1);
   unsigned vec4 = BITFIELD_MASK(4);

   enum agx_format i16 = AGX_FORMAT_I16;
   enum agx_format i32 = AGX_FORMAT_I32;
};

TEST_F(LowerSpill, ScalarSpills)
{
   CASE(agx_mov_to(b, agx_memory_register(11, AGX_SIZE_16), hy),
        agx_stack_store(b, hy, agx_immediate(22), i16, scalar));

   CASE(agx_mov_to(b, agx_memory_register(18, AGX_SIZE_32), wx),
        agx_stack_store(b, wx, agx_immediate(36), i32, scalar));
}

TEST_F(LowerSpill, ScalarFills)
{
   CASE(agx_mov_to(b, hy, agx_memory_register(11, AGX_SIZE_16)),
        agx_stack_load_to(b, hy, agx_immediate(22), i16, scalar));

   CASE(agx_mov_to(b, wx, agx_memory_register(18, AGX_SIZE_32)),
        agx_stack_load_to(b, wx, agx_immediate(36), i32, scalar));
}

TEST_F(LowerSpill, VectorSpills)
{
   CASE(agx_mov_to(b, mh4, hy4),
        agx_stack_store(b, hy4, agx_immediate(0), i16, vec4));

   CASE(agx_mov_to(b, mw4, wx4),
        agx_stack_store(b, wx4, agx_immediate(0), i32, vec4));
}

TEST_F(LowerSpill, VectorFills)
{
   CASE(agx_mov_to(b, hy4, mh4),
        agx_stack_load_to(b, hy4, agx_immediate(0), i16, vec4));

   CASE(agx_mov_to(b, wx4, mw4),
        agx_stack_load_to(b, wx4, agx_immediate(0), i32, vec4));
}

TEST_F(LowerSpill, ScalarSpill64)
{
   CASE(agx_mov_to(b, agx_memory_register(16, AGX_SIZE_64),
                   agx_register(8, AGX_SIZE_64)),
        agx_stack_store(b, agx_register(8, AGX_SIZE_64), agx_immediate(32), i32,
                        BITFIELD_MASK(2)));
}

TEST_F(LowerSpill, ScalarFill64)
{
   CASE(agx_mov_to(b, agx_register(16, AGX_SIZE_64),
                   agx_memory_register(8, AGX_SIZE_64)),
        agx_stack_load_to(b, agx_register(16, AGX_SIZE_64), agx_immediate(16),
                          i32, BITFIELD_MASK(2)));
}

TEST_F(LowerSpill, Vec6Spill)
{
   CASE(
      {
         agx_index mvec6 = agx_memory_register(16, AGX_SIZE_32);
         agx_index vec6 = agx_register(8, AGX_SIZE_32);
         vec6.channels_m1 = 6 - 1;
         mvec6.channels_m1 = 6 - 1;

         agx_mov_to(b, mvec6, vec6);
      },
      {
         agx_index vec4 = agx_register(8, AGX_SIZE_32);
         agx_index vec2 = agx_register(8 + (4 * 2), AGX_SIZE_32);
         vec4.channels_m1 = 4 - 1;
         vec2.channels_m1 = 2 - 1;

         agx_stack_store(b, vec4, agx_immediate(32), i32, BITFIELD_MASK(4));
         agx_stack_store(b, vec2, agx_immediate(32 + 4 * 4), i32,
                         BITFIELD_MASK(2));
      });
}

TEST_F(LowerSpill, Vec6Fill)
{
   CASE(
      {
         agx_index mvec6 = agx_memory_register(16, AGX_SIZE_32);
         agx_index vec6 = agx_register(8, AGX_SIZE_32);
         vec6.channels_m1 = 6 - 1;
         mvec6.channels_m1 = 6 - 1;

         agx_mov_to(b, vec6, mvec6);
      },
      {
         agx_index vec4 = agx_register(8, AGX_SIZE_32);
         agx_index vec2 = agx_register(8 + (4 * 2), AGX_SIZE_32);
         vec4.channels_m1 = 4 - 1;
         vec2.channels_m1 = 2 - 1;

         agx_stack_load_to(b, vec4, agx_immediate(32), i32, BITFIELD_MASK(4));
         agx_stack_load_to(b, vec2, agx_immediate(32 + 4 * 4), i32,
                           BITFIELD_MASK(2));
      });
}
