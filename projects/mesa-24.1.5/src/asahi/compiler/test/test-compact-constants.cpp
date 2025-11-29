/*
 * Copyright 2021 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "agx_builder.h"
#include "agx_compiler.h"
#include "agx_test.h"

#include <gtest/gtest.h>

#define CASE(instr, expected, size)                                            \
   INSTRUCTION_CASE(                                                           \
      {                                                                        \
         UNUSED agx_index out = agx_temp(b->shader, AGX_SIZE_##size);          \
         instr;                                                                \
         agx_unit_test(b, out);                                                \
      },                                                                       \
      {                                                                        \
         UNUSED agx_index out = agx_temp(b->shader, AGX_SIZE_##size);          \
         expected;                                                             \
         agx_unit_test(b, out);                                                \
      },                                                                       \
      agx_opt_compact_constants)

#define NEGCASE(instr, size)    CASE(instr, instr, size)
#define CASE32(instr, expected) CASE(instr, expected, 32)
#define NEGCASE32(instr)        NEGCASE(instr, 32)

class CompactConstants : public testing::Test {
 protected:
   CompactConstants()
   {
      mem_ctx = ralloc_context(NULL);

      wx = agx_register(0, AGX_SIZE_32);
   }

   ~CompactConstants()
   {
      ralloc_free(mem_ctx);
   }

   void *mem_ctx;
   agx_index wx;
};

TEST_F(CompactConstants, FP32)
{
   CASE32(agx_fadd_to(b, out, wx, agx_mov_imm(b, 32, fui(32768.0))),
          agx_fadd_to(b, out, wx, agx_mov_imm(b, 16, 0x7800)));
}

TEST_F(CompactConstants, InexactFP32)
{
   NEGCASE32(agx_fadd_to(b, out, wx, agx_mov_imm(b, 32, fui(32769.0))));
}
