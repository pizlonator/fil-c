/*
 * Copyright © 2012 Intel Corporation
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "util/ralloc.h"
#include "elk_disasm.h"
#include "elk_eu.h"

#include <gtest/gtest.h>

struct CompactParams {
   unsigned verx10;
   unsigned align;
};

std::string
get_compact_params_name(const testing::TestParamInfo<CompactParams> p)
{
   CompactParams params = p.param;
   std::stringstream ss;
   ss << params.verx10 << "_";
   switch (params.align) {
   case ELK_ALIGN_1:
      ss << "Align_1";
      break;
   case ELK_ALIGN_16:
      ss << "Align_16";
      break;
   default:
      unreachable("invalid align");
   }
   return ss.str();
}

static bool
test_compact_instruction(struct elk_codegen *p, elk_inst src)
{
   elk_compact_inst dst;
   memset(&dst, 0xd0, sizeof(dst));

   if (elk_try_compact_instruction(p->isa, &dst, &src)) {
      elk_inst uncompacted;

      elk_uncompact_instruction(p->isa, &uncompacted, &dst);
      if (memcmp(&uncompacted, &src, sizeof(src))) {
	 elk_debug_compact_uncompact(p->isa, &src, &uncompacted);
	 return false;
      }
   } else {
      elk_compact_inst unchanged;
      memset(&unchanged, 0xd0, sizeof(unchanged));
      /* It's not supposed to change dst unless it compacted. */
      if (memcmp(&unchanged, &dst, sizeof(dst))) {
	 fprintf(stderr, "Failed to compact, but dst changed\n");
	 fprintf(stderr, "  Instruction: ");
	 elk_disassemble_inst(stderr, p->isa, &src, false, 0, NULL);
	 return false;
      }
   }

   return true;
}

/**
 * When doing fuzz testing, pad bits won't round-trip.
 *
 * This sort of a superset of skip_bit, which is testing for changing bits that
 * aren't worth testing for fuzzing.  We also just want to clear bits that
 * become meaningless once fuzzing twiddles a related bit.
 */
static void
clear_pad_bits(const struct elk_isa_info *isa, elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   if (elk_inst_opcode(isa, inst) != ELK_OPCODE_SEND &&
       elk_inst_opcode(isa, inst) != ELK_OPCODE_SENDC &&
       elk_inst_src0_reg_file(devinfo, inst) != ELK_IMMEDIATE_VALUE &&
       elk_inst_src1_reg_file(devinfo, inst) != ELK_IMMEDIATE_VALUE) {
      elk_inst_set_bits(inst, 127, 111, 0);
   }

   if (devinfo->ver == 8 && devinfo->platform != INTEL_PLATFORM_CHV &&
       elk_is_3src(isa, elk_inst_opcode(isa, inst))) {
      elk_inst_set_bits(inst, 105, 105, 0);
      elk_inst_set_bits(inst, 84, 84, 0);
      elk_inst_set_bits(inst, 36, 35, 0);
   }
}

static bool
skip_bit(const struct elk_isa_info *isa, elk_inst *src, int bit)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   /* pad bit */
   if (bit == 7)
      return true;

   /* The compact bit -- uncompacted can't have it set. */
   if (bit == 29)
      return true;

   if (elk_is_3src(isa, elk_inst_opcode(isa, src))) {
      if (devinfo->platform == INTEL_PLATFORM_CHV) {
         if (bit == 127)
            return true;
      } else {
         if (bit >= 126 && bit <= 127)
            return true;

         if (bit == 105)
            return true;

         if (bit == 84)
            return true;

         if (bit >= 35 && bit <= 36)
            return true;
      }
   } else {
      if (bit == 47)
         return true;

      if (devinfo->ver >= 8) {
         if (bit == 11)
            return true;

         if (bit == 95)
            return true;
      } else {
         if (devinfo->ver < 7 && bit == 90)
            return true;

         if (bit >= 91 && bit <= 95)
            return true;
      }
   }

   /* sometimes these are pad bits. */
   if (elk_inst_opcode(isa, src) != ELK_OPCODE_SEND &&
       elk_inst_opcode(isa, src) != ELK_OPCODE_SENDC &&
       elk_inst_src0_reg_file(devinfo, src) != ELK_IMMEDIATE_VALUE &&
       elk_inst_src1_reg_file(devinfo, src) != ELK_IMMEDIATE_VALUE &&
       bit >= 121) {
      return true;
   }

   return false;
}

static bool
test_fuzz_compact_instruction(struct elk_codegen *p, elk_inst src)
{
   for (int bit0 = 0; bit0 < 128; bit0++) {
      if (skip_bit(p->isa, &src, bit0))
	 continue;

      for (int bit1 = 0; bit1 < 128; bit1++) {
         elk_inst instr = src;
	 uint64_t *bits = instr.data;

         if (skip_bit(p->isa, &src, bit1))
	    continue;

	 bits[bit0 / 64] ^= (1ull << (bit0 & 63));
	 bits[bit1 / 64] ^= (1ull << (bit1 & 63));

         clear_pad_bits(p->isa, &instr);

         if (!elk_validate_instruction(p->isa, &instr, 0, sizeof(elk_inst), NULL))
            continue;

	 if (!test_compact_instruction(p, instr)) {
	    printf("  twiddled bits for fuzzing %d, %d\n", bit0, bit1);
	    return false;
	 }
      }
   }

   return true;
}

class CompactTestFixture : public testing::TestWithParam<CompactParams> {
protected:
   virtual void SetUp() {
      CompactParams params = GetParam();
      mem_ctx = ralloc_context(NULL);
      devinfo = rzalloc(mem_ctx, intel_device_info);
      p = rzalloc(mem_ctx, elk_codegen);

      devinfo->verx10 = params.verx10;
      devinfo->ver = devinfo->verx10 / 10;

      elk_init_isa_info(&isa, devinfo);
      elk_init_codegen(&isa, p, p);
      elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
      elk_set_default_access_mode(p, params.align);
   };

   virtual void TearDown() {
      EXPECT_EQ(p->nr_insn, 1);
      EXPECT_TRUE(test_compact_instruction(p, p->store[0]));
      EXPECT_TRUE(test_fuzz_compact_instruction(p, p->store[0]));

      ralloc_free(mem_ctx);
   };

   void *mem_ctx;
   struct elk_isa_info isa;
   intel_device_info *devinfo;
   elk_codegen *p;
};

class Instructions : public CompactTestFixture {};

INSTANTIATE_TEST_SUITE_P(
   CompactTest,
   Instructions,
   testing::Values(
      CompactParams{ 50,  ELK_ALIGN_1 }, CompactParams{ 50, ELK_ALIGN_16 },
      CompactParams{ 60,  ELK_ALIGN_1 }, CompactParams{ 60, ELK_ALIGN_16 },
      CompactParams{ 70,  ELK_ALIGN_1 }, CompactParams{ 70, ELK_ALIGN_16 },
      CompactParams{ 75,  ELK_ALIGN_1 }, CompactParams{ 75, ELK_ALIGN_16 },
      CompactParams{ 80,  ELK_ALIGN_1 }, CompactParams{ 80, ELK_ALIGN_16 }
   ),
   get_compact_params_name);

class InstructionsBeforeIvyBridge : public CompactTestFixture {};

INSTANTIATE_TEST_SUITE_P(
   CompactTest,
   InstructionsBeforeIvyBridge,
   testing::Values(
      CompactParams{ 50,  ELK_ALIGN_1 }, CompactParams{ 50, ELK_ALIGN_16 },
      CompactParams{ 60,  ELK_ALIGN_1 }, CompactParams{ 60, ELK_ALIGN_16 }
   ),
   get_compact_params_name);


TEST_P(Instructions, ADD_GRF_GRF_GRF)
{
   struct elk_reg g0 = elk_vec8_grf(0, 0);
   struct elk_reg g2 = elk_vec8_grf(2, 0);
   struct elk_reg g4 = elk_vec8_grf(4, 0);

   elk_ADD(p, g0, g2, g4);
}

TEST_P(Instructions, ADD_GRF_GRF_IMM)
{
   struct elk_reg g0 = elk_vec8_grf(0, 0);
   struct elk_reg g2 = elk_vec8_grf(2, 0);

   elk_ADD(p, g0, g2, elk_imm_f(1.0));
}

TEST_P(Instructions, ADD_GRF_GRF_IMM_d)
{
   struct elk_reg g0 = retype(elk_vec8_grf(0, 0), ELK_REGISTER_TYPE_D);
   struct elk_reg g2 = retype(elk_vec8_grf(2, 0), ELK_REGISTER_TYPE_D);

   elk_ADD(p, g0, g2, elk_imm_d(1));
}

TEST_P(Instructions, MOV_GRF_GRF)
{
   struct elk_reg g0 = elk_vec8_grf(0, 0);
   struct elk_reg g2 = elk_vec8_grf(2, 0);

   elk_MOV(p, g0, g2);
}

TEST_P(InstructionsBeforeIvyBridge, ADD_MRF_GRF_GRF)
{
   struct elk_reg m6 = elk_vec8_reg(ELK_MESSAGE_REGISTER_FILE, 6, 0);
   struct elk_reg g2 = elk_vec8_grf(2, 0);
   struct elk_reg g4 = elk_vec8_grf(4, 0);

   elk_ADD(p, m6, g2, g4);
}

TEST_P(Instructions, ADD_vec1_GRF_GRF_GRF)
{
   struct elk_reg g0 = elk_vec1_grf(0, 0);
   struct elk_reg g2 = elk_vec1_grf(2, 0);
   struct elk_reg g4 = elk_vec1_grf(4, 0);

   elk_ADD(p, g0, g2, g4);
}

TEST_P(InstructionsBeforeIvyBridge, PLN_MRF_GRF_GRF)
{
   struct elk_reg m6 = elk_vec8_reg(ELK_MESSAGE_REGISTER_FILE, 6, 0);
   struct elk_reg interp = elk_vec1_grf(2, 0);
   struct elk_reg g4 = elk_vec8_grf(4, 0);

   elk_PLN(p, m6, interp, g4);
}

TEST_P(Instructions, f0_0_MOV_GRF_GRF)
{
   struct elk_reg g0 = elk_vec8_grf(0, 0);
   struct elk_reg g2 = elk_vec8_grf(2, 0);

   elk_push_insn_state(p);
   elk_set_default_predicate_control(p, ELK_PREDICATE_NORMAL);
   elk_MOV(p, g0, g2);
   elk_pop_insn_state(p);
}

/* The handling of f0.1 vs f0.0 changes between gfx6 and gfx7.  Explicitly test
 * it, so that we run the fuzzing can run over all the other bits that might
 * interact with it.
 */
TEST_P(Instructions, f0_1_MOV_GRF_GRF)
{
   struct elk_reg g0 = elk_vec8_grf(0, 0);
   struct elk_reg g2 = elk_vec8_grf(2, 0);

   elk_push_insn_state(p);
   elk_set_default_predicate_control(p, ELK_PREDICATE_NORMAL);
   elk_inst *mov = elk_MOV(p, g0, g2);
   elk_inst_set_flag_subreg_nr(p->devinfo, mov, 1);
   elk_pop_insn_state(p);
}
