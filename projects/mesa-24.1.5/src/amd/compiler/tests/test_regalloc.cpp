/*
 * Copyright © 2020 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */
#include "helpers.h"

using namespace aco;

BEGIN_TEST(regalloc.subdword_alloc.reuse_16bit_operands)
   /* Registers of operands should be "recycled" for the output. But if the
    * input is smaller than the output, that's not generally possible. The
    * first v_cvt_f32_f16 instruction below uses the upper 16 bits of v0
    * while the lower 16 bits are still live, so the output must be stored in
    * a register other than v0. For the second v_cvt_f32_f16, the original
    * value stored in v0 is no longer used and hence it's safe to store the
    * result in v0.
    */

   /* TODO: is this possible to do on GFX11? */
   for (amd_gfx_level cc = GFX8; cc <= GFX10_3; cc = (amd_gfx_level)((unsigned)cc + 1)) {
      for (bool pessimistic : {false, true}) {
         const char* subvariant = pessimistic ? "/pessimistic" : "/optimistic";

         //>> v1: %_:v[#a] = p_startpgm
         if (!setup_cs("v1", (amd_gfx_level)cc, CHIP_UNKNOWN, subvariant))
            return;

         //! v2b: %_:v[#a][0:16], v2b: %res1:v[#a][16:32] = p_split_vector %_:v[#a]
         Builder::Result tmp =
            bld.pseudo(aco_opcode::p_split_vector, bld.def(v2b), bld.def(v2b), inputs[0]);

         //! v1: %_:v[#b] = v_cvt_f32_f16 %_:v[#a][16:32] dst_sel:dword src0_sel:uword1
         //! v1: %_:v[#a] = v_cvt_f32_f16 %_:v[#a][0:16]
         //; success = (b != a)
         auto result1 = bld.vop1(aco_opcode::v_cvt_f32_f16, bld.def(v1), tmp.def(1).getTemp());
         auto result2 = bld.vop1(aco_opcode::v_cvt_f32_f16, bld.def(v1), tmp.def(0).getTemp());
         writeout(0, result1);
         writeout(1, result2);

         finish_ra_test(ra_test_policy{pessimistic});
      }
   }
END_TEST

BEGIN_TEST(regalloc._32bit_partial_write)
   //>> v1: %_:v[0] = p_startpgm
   if (!setup_cs("v1", GFX10))
      return;

   /* ensure high 16 bits are occupied */
   //! v2b: %_:v[0][0:16], v2b: %_:v[0][16:32] = p_split_vector %_:v[0]
   Temp hi =
      bld.pseudo(aco_opcode::p_split_vector, bld.def(v2b), bld.def(v2b), inputs[0]).def(1).getTemp();

   /* This test checks if this instruction uses SDWA. */
   //! v2b: %_:v[0][0:16] = v_not_b32 0 dst_sel:uword0 dst_preserve src0_sel:dword
   Temp lo = bld.vop1(aco_opcode::v_not_b32, bld.def(v2b), Operand::zero());

   //! v1: %_:v[0] = p_create_vector %_:v[0][0:16], %_:v[0][16:32]
   bld.pseudo(aco_opcode::p_create_vector, bld.def(v1), lo, hi);

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.precolor.swap)
   //>> s2: %op0:s[0-1] = p_startpgm
   if (!setup_cs("s2", GFX10))
      return;

   program->dev.sgpr_limit = 4;

   //! s2: %op1:s[2-3] = p_unit_test
   Temp op1 = bld.pseudo(aco_opcode::p_unit_test, bld.def(s2));

   //! s2: %op0_2:s[2-3], s2: %op1_2:s[0-1] = p_parallelcopy %op0:s[0-1], %op1:s[2-3]
   //! p_unit_test %op0_2:s[2-3], %op1_2:s[0-1]
   Operand op(inputs[0]);
   op.setFixed(PhysReg(2));
   bld.pseudo(aco_opcode::p_unit_test, op, op1);

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.precolor.blocking_vector)
   //>> s2: %tmp0:s[0-1], s1: %tmp1:s[2] = p_startpgm
   if (!setup_cs("s2 s1", GFX10))
      return;

   //! s1: %tmp1_2:s[1], s2: %tmp0_2:s[2-3] = p_parallelcopy %tmp1:s[2], %tmp0:s[0-1]
   //! p_unit_test %tmp1_2:s[1]
   Operand op(inputs[1]);
   op.setFixed(PhysReg(1));
   bld.pseudo(aco_opcode::p_unit_test, op);

   //! p_unit_test %tmp0_2:s[2-3]
   bld.pseudo(aco_opcode::p_unit_test, inputs[0]);

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.precolor.vector.test)
   //>> s2: %tmp0:s[0-1], s1: %tmp1:s[2], s1: %tmp2:s[3] = p_startpgm
   if (!setup_cs("s2 s1 s1", GFX10))
      return;

   //! s2: %tmp0_2:s[2-3], s1: %tmp2_2:s[0] = p_parallelcopy %tmp0:s[0-1], %tmp2:s[3]
   //! p_unit_test %tmp0_2:s[2-3]
   Operand op(inputs[0]);
   op.setFixed(PhysReg(2));
   bld.pseudo(aco_opcode::p_unit_test, op);

   //! p_unit_test %tmp2_2:s[0]
   bld.pseudo(aco_opcode::p_unit_test, inputs[2]);

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.precolor.vector.collect)
   //>> s2: %tmp0:s[0-1], s1: %tmp1:s[2], s1: %tmp2:s[3] = p_startpgm
   if (!setup_cs("s2 s1 s1", GFX10))
      return;

   //! s2: %tmp0_2:s[2-3], s1: %tmp1_2:s[0], s1: %tmp2_2:s[1] = p_parallelcopy %tmp0:s[0-1], %tmp1:s[2], %tmp2:s[3]
   //! p_unit_test %tmp0_2:s[2-3]
   Operand op(inputs[0]);
   op.setFixed(PhysReg(2));
   bld.pseudo(aco_opcode::p_unit_test, op);

   //! p_unit_test %tmp1_2:s[0], %tmp2_2:s[1]
   bld.pseudo(aco_opcode::p_unit_test, inputs[1], inputs[2]);

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.precolor.vgpr_move)
   //>> v1: %tmp0:v[0], v1: %tmp1:v[1] = p_startpgm
   if (!setup_cs("v1 v1", GFX10))
      return;

   //! v1: %tmp1_2:v[0], v1: %tmp0_2:v[1] = p_parallelcopy %tmp1:v[1], %tmp0:v[0]
   //! p_unit_test %tmp0_2:v[1], %tmp1_2:v[0]
   bld.pseudo(aco_opcode::p_unit_test, inputs[0], Operand(inputs[1], PhysReg(256)));

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.precolor.multiple_operands)
   //>> v1: %tmp0:v[0], v1: %tmp1:v[1], v1: %tmp2:v[2], v1: %tmp3:v[3] = p_startpgm
   if (!setup_cs("v1 v1 v1 v1", GFX10))
      return;

   //! v1: %tmp3_2:v[0], v1: %tmp0_2:v[1], v1: %tmp1_2:v[2], v1: %tmp2_2:v[3] = p_parallelcopy %tmp3:v[3], %tmp0:v[0], %tmp1:v[1], %tmp2:v[2]
   //! p_unit_test %tmp3_2:v[0], %tmp0_2:v[1], %tmp1_2:v[2], %tmp2_2:v[3]
   bld.pseudo(aco_opcode::p_unit_test, Operand(inputs[3], PhysReg(256 + 0)),
              Operand(inputs[0], PhysReg(256 + 1)), Operand(inputs[1], PhysReg(256 + 2)),
              Operand(inputs[2], PhysReg(256 + 3)));

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.precolor.different_regs)
   //>> v1: %tmp0:v[0] = p_startpgm
   if (!setup_cs("v1", GFX10))
      return;

   //! v1: %tmp1:v[1], v1: %tmp2:v[2] = p_parallelcopy %tmp0:v[0], %tmp0:v[0]
   //! p_unit_test %tmp0:v[0], %tmp1:v[1], %tmp2:v[2]
   bld.pseudo(aco_opcode::p_unit_test, Operand(inputs[0], PhysReg(256 + 0)),
              Operand(inputs[0], PhysReg(256 + 1)), Operand(inputs[0], PhysReg(256 + 2)));

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.scratch_sgpr.create_vector)
   if (!setup_cs("v1 s1", GFX7))
      return;

   Temp tmp = bld.pseudo(aco_opcode::p_extract_vector, bld.def(v1b), inputs[0], Operand::zero());

   //>> v3b: %0:v[0][0:24] = v_and_b32 0xffffff, %0:v[0][0:24]
   //! s1: %0:s[1] = s_mov_b32 0x1000001
   //! v1: %0:v[0] = v_mul_lo_u32 %0:s[1], %_:v[0][0:8]
   bld.pseudo(aco_opcode::p_create_vector, bld.def(v1), Operand(v3b), Operand(tmp));

   //! p_unit_test %_:s[0]
   //! s_endpgm
   bld.pseudo(aco_opcode::p_unit_test, inputs[1]);

   finish_ra_test(ra_test_policy(), true);
END_TEST

BEGIN_TEST(regalloc.scratch_sgpr.create_vector_sgpr_operand)
   if (!setup_cs("v2 s1", GFX7))
      return;

   Temp tmp = bld.pseudo(aco_opcode::p_extract_vector, bld.def(v1b), inputs[0], Operand::c32(4u));

   //>> v1: %0:v[0] = v_mov_b32 %_:s[0]
   //! v3b: %0:v[1][0:24] = v_and_b32 0xffffff, %0:v[1][0:24]
   //! s1: %0:s[1] = s_mov_b32 0x1000001
   //! v1: %0:v[1] = v_mul_lo_u32 %0:s[1], %_:v[1][0:8]
   bld.pseudo(aco_opcode::p_create_vector, bld.def(v2), inputs[1], Operand(v3b), Operand(tmp));

   //! p_unit_test %_:s[0]
   //! s_endpgm
   bld.pseudo(aco_opcode::p_unit_test, inputs[1]);

   finish_ra_test(ra_test_policy(), true);
END_TEST

BEGIN_TEST(regalloc.branch_def_phis_at_merge_block)
   //>> p_startpgm
   if (!setup_cs("", GFX10))
      return;

   program->blocks[0].kind &= ~block_kind_top_level;

   //! s2: %_:s[2-3] = p_branch
   bld.branch(aco_opcode::p_branch, bld.def(s2));

   //! BB1
   //! /* logical preds: / linear preds: BB0, / kind: uniform, */
   bld.reset(program->create_and_insert_block());
   program->blocks[1].linear_preds.push_back(0);

   //! s2: %tmp:s[0-1] = p_linear_phi 0
   Temp tmp = bld.pseudo(aco_opcode::p_linear_phi, bld.def(s2), Operand::c64(0u));

   //! p_unit_test %tmp:s[0-1]
   bld.pseudo(aco_opcode::p_unit_test, tmp);

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.branch_def_phis_at_branch_block)
   //>> p_startpgm
   if (!setup_cs("", GFX10))
      return;

   //! s2: %tmp:s[0-1] = p_unit_test
   Temp tmp = bld.pseudo(aco_opcode::p_unit_test, bld.def(s2));

   //! s2: %_:s[2-3] = p_cbranch_z %0:scc
   bld.branch(aco_opcode::p_cbranch_z, bld.def(s2), Operand(scc, s1));

   //! BB1
   //! /* logical preds: / linear preds: BB0, / kind: */
   bld.reset(program->create_and_insert_block());
   program->blocks[1].linear_preds.push_back(0);

   //! p_unit_test %tmp:s[0-1]
   bld.pseudo(aco_opcode::p_unit_test, tmp);
   bld.branch(aco_opcode::p_branch, bld.def(s2));

   bld.reset(program->create_and_insert_block());
   program->blocks[2].linear_preds.push_back(0);

   bld.branch(aco_opcode::p_branch, bld.def(s2));

   bld.reset(program->create_and_insert_block());
   program->blocks[3].linear_preds.push_back(1);
   program->blocks[3].linear_preds.push_back(2);
   program->blocks[3].kind |= block_kind_top_level;

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.vintrp_fp16)
   //>> v1: %in0:v[0], s1: %in1:s[0], v1: %in2:v[1] = p_startpgm
   if (!setup_cs("v1 s1 v1", GFX10))
      return;

   //! s1: %npm:m0 = p_parallelcopy %in1:s[0]
   //! v2b: %lo:v[2][0:16] = v_interp_p2_f16 %in0:v[0], %npm:m0, %in2:v[1] attr0.x
   Temp lo = bld.vintrp(aco_opcode::v_interp_p2_f16, bld.def(v2b), inputs[0], bld.m0(inputs[1]),
                        inputs[2], 0, 0, false);
   //! v2b: %hi:v[2][16:32] = v_interp_p2_hi_f16 %in0:v[0], %npm:m0, %in2:v[1] attr0.x high
   Temp hi = bld.vintrp(aco_opcode::v_interp_p2_f16, bld.def(v2b), inputs[0], bld.m0(inputs[1]),
                        inputs[2], 0, 0, true);
   //! v1: %res:v[2] = p_create_vector %lo:v[2][0:16], %hi:v[2][16:32]
   Temp res = bld.pseudo(aco_opcode::p_create_vector, bld.def(v1), lo, hi);
   //! p_unit_test %res:v[2]
   bld.pseudo(aco_opcode::p_unit_test, res);

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.vinterp_fp16)
   //>> v1: %in0:v[0], v1: %in1:v[1], v1: %in2:v[2] = p_startpgm
   if (!setup_cs("v1 v1 v1", GFX11))
      return;

   //! v2b: %lo:v[3][0:16], v2b: %hi:v[3][16:32] = p_split_vector %in0:v[0]
   Temp lo = bld.tmp(v2b);
   Temp hi = bld.tmp(v2b);
   bld.pseudo(aco_opcode::p_split_vector, Definition(lo), Definition(hi), inputs[0]);

   //! v1: %tmp0:v[1] = v_interp_p10_f16_f32_inreg %lo:v[3][0:16], %in1:v[1], hi(%hi:v[3][16:32])
   //! p_unit_test %tmp0:v[1]
   Temp tmp0 =
      bld.vinterp_inreg(aco_opcode::v_interp_p10_f16_f32_inreg, bld.def(v1), lo, inputs[1], hi);
   bld.pseudo(aco_opcode::p_unit_test, tmp0);

   //! v2b: %tmp1:v[0][16:32] = v_interp_p2_f16_f32_inreg %in0:v[0], %in2:v[2], %tmp0:v[1] opsel_hi
   //! v1: %tmp2:v[0] = p_create_vector 0, %tmp1:v[0][16:32]
   //! p_unit_test %tmp2:v[0]
   Temp tmp1 = bld.vinterp_inreg(aco_opcode::v_interp_p2_f16_f32_inreg, bld.def(v2b), inputs[0],
                                 inputs[2], tmp0);
   Temp tmp2 = bld.pseudo(aco_opcode::p_create_vector, bld.def(v1), Operand::zero(2), tmp1);
   bld.pseudo(aco_opcode::p_unit_test, tmp2);

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.writelane)
   //>> v1: %in0:v[0], s1: %in1:s[0], s1: %in2:s[1], s1: %in3:s[2] = p_startpgm
   if (!setup_cs("v1 s1 s1 s1", GFX8))
      return;

   //! s1: %tmp:m0 = p_parallelcopy %int3:s[2]
   Temp tmp = bld.copy(bld.def(s1, m0), inputs[3]);

   //! s1: %in1_2:m0,  s1: %tmp_2:s[0] = p_parallelcopy %in1:s[0], %tmp:m0
   //! v1: %tmp2:v[0] = v_writelane_b32_e64 %in1_2:m0, %in2:s[1], %in0:v[0]
   Temp tmp2 = bld.writelane(bld.def(v1), inputs[1], inputs[2], inputs[0]);

   //! p_unit_test %tmp_2:s[0], %tmp2:v[0]
   bld.pseudo(aco_opcode::p_unit_test, tmp, tmp2);

   finish_ra_test(ra_test_policy());
END_TEST

static void
end_linear_vgpr(Temp tmp)
{
   Operand op(tmp);
   op.setLateKill(true);
   bld.pseudo(aco_opcode::p_end_linear_vgpr, op);
}

BEGIN_TEST(regalloc.linear_vgpr.alloc.basic)
   if (!setup_cs("", GFX8))
      return;

   //>> lv1: %ltmp0:v[31] = p_start_linear_vgpr
   //! lv1: %ltmp1:v[30] = p_start_linear_vgpr
   //! p_end_linear_vgpr (latekill)%ltmp0:v[31]
   //! lv1: %ltmp2:v[31] = p_start_linear_vgpr
   //! p_end_linear_vgpr (latekill)%ltmp1:v[30]
   //! p_end_linear_vgpr (latekill)%ltmp2:v[31]
   Temp ltmp0 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
   Temp ltmp1 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
   end_linear_vgpr(ltmp0);
   Temp ltmp2 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
   end_linear_vgpr(ltmp1);
   end_linear_vgpr(ltmp2);

   finish_ra_test(ra_test_policy());
END_TEST

BEGIN_TEST(regalloc.linear_vgpr.alloc.compact_grow)
   for (bool pessimistic : {false, true}) {
      const char* subvariant = pessimistic ? "_pessimistic" : "_optimistic";
      //>> v1: %in0:v[0] = p_startpgm
      if (!setup_cs("v1", GFX8, CHIP_UNKNOWN, subvariant))
         continue;

      //! lv1: %ltmp0:v[31] = p_start_linear_vgpr
      //! lv1: %ltmp1:v[30] = p_start_linear_vgpr
      //! p_end_linear_vgpr (latekill)%ltmp0:v[31]
      Temp ltmp0 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      Temp ltmp1 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      end_linear_vgpr(ltmp0);

      //! v1: %tmp:v[29] = p_parallelcopy %in0:v[0]
      Temp tmp = bld.pseudo(aco_opcode::p_parallelcopy, bld.def(v1, PhysReg(256 + 29)), inputs[0]);

      /* When there's not enough space in the linear VGPR area for a new one, the area is compacted
       * and the beginning is chosen. Any variables which are in the way, are moved.
       */
      //! lv1: %ltmp1_2:v[31] = p_parallelcopy %ltmp1:v[30]
      //! v1: %tmp_2:v[#_] = p_parallelcopy %tmp:v[29]
      //! lv2: %ltmp2:v[29-30] = p_start_linear_vgpr
      Temp ltmp2 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v2.as_linear()));

      //! p_end_linear_vgpr (latekill)%ltmp1_2:v[31]
      //! p_end_linear_vgpr (latekill)%ltmp2:v[29-30]
      end_linear_vgpr(ltmp1);
      end_linear_vgpr(ltmp2);

      //! p_unit_test %tmp_2:v[#_]
      bld.pseudo(aco_opcode::p_unit_test, tmp);

      finish_ra_test(ra_test_policy{pessimistic});
   }
END_TEST

BEGIN_TEST(regalloc.linear_vgpr.alloc.compact_shrink)
   for (bool pessimistic : {false, true}) {
      const char* subvariant = pessimistic ? "_pessimistic" : "_optimistic";
      //>> v1: %in0:v[0] = p_startpgm
      if (!setup_cs("v1", GFX8, CHIP_UNKNOWN, subvariant))
         continue;

      //! lv1: %ltmp0:v[31] = p_start_linear_vgpr
      //! lv1: %ltmp1:v[30] = p_start_linear_vgpr
      //! lv1: %ltmp2:v[29] = p_start_linear_vgpr
      //! lv1: %ltmp3:v[28] = p_start_linear_vgpr
      //! lv1: %ltmp4:v[27] = p_start_linear_vgpr
      //! p_end_linear_vgpr (latekill)%ltmp0:v[31]
      //! p_end_linear_vgpr (latekill)%ltmp2:v[29]
      //! p_end_linear_vgpr (latekill)%ltmp4:v[27]
      Temp ltmp0 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      Temp ltmp1 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      Temp ltmp2 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      Temp ltmp3 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      Temp ltmp4 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      end_linear_vgpr(ltmp0);
      end_linear_vgpr(ltmp2);
      end_linear_vgpr(ltmp4);

      /* Unlike regalloc.linear_vgpr.alloc.compact_grow, this shrinks the linear VGPR area. */
      //! lv1: %ltmp3_2:v[30], lv1: %ltmp1_2:v[31] = p_parallelcopy %ltmp3:v[28], %ltmp1:v[30]
      //! lv2: %ltmp5:v[28-29] = p_start_linear_vgpr
      Temp ltmp5 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v2.as_linear()));

      /* There should be enough space for 28 normal VGPRs. */
      //! v28: %_:v[0-27] = p_unit_test
      bld.pseudo(aco_opcode::p_unit_test, bld.def(RegClass::get(RegType::vgpr, 28 * 4)));

      //! p_end_linear_vgpr (latekill)%ltmp1_2:v[31]
      //! p_end_linear_vgpr (latekill)%ltmp3_2:v[30]
      //! p_end_linear_vgpr (latekill)%ltmp5:v[28-29]
      end_linear_vgpr(ltmp1);
      end_linear_vgpr(ltmp3);
      end_linear_vgpr(ltmp5);

      finish_ra_test(ra_test_policy{pessimistic});
   }
END_TEST

BEGIN_TEST(regalloc.linear_vgpr.alloc.compact_for_normal)
   for (bool pessimistic : {false, true}) {
      const char* subvariant = pessimistic ? "_pessimistic" : "_optimistic";
      //>> v1: %in0:v[0] = p_startpgm
      if (!setup_cs("v1", GFX8, CHIP_UNKNOWN, subvariant))
         continue;

      //! lv1: %ltmp0:v[31] = p_start_linear_vgpr
      //! lv1: %ltmp1:v[30] = p_start_linear_vgpr
      //! p_end_linear_vgpr (latekill)%ltmp0:v[31]
      Temp ltmp0 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      Temp ltmp1 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      end_linear_vgpr(ltmp0);

      //! lv1: %ltmp1_2:v[31] = p_parallelcopy %ltmp1:v[30]
      //! v31: %_:v[0-30] = p_unit_test
      bld.pseudo(aco_opcode::p_unit_test, bld.def(RegClass::get(RegType::vgpr, 31 * 4)));

      //! p_end_linear_vgpr (latekill)%ltmp1_2:v[31]
      end_linear_vgpr(ltmp1);

      finish_ra_test(ra_test_policy{pessimistic});
   }
END_TEST

BEGIN_TEST(regalloc.linear_vgpr.alloc.compact_for_vec)
   for (bool pessimistic : {false, true}) {
      const char* subvariant = pessimistic ? "_pessimistic" : "_optimistic";
      //>> v1: %in0:v[0] = p_startpgm
      if (!setup_cs("v1", GFX8, CHIP_UNKNOWN, subvariant))
         continue;

      //! lv1: %ltmp0:v[31] = p_start_linear_vgpr
      //! lv1: %ltmp1:v[30] = p_start_linear_vgpr
      //! p_end_linear_vgpr (latekill)%ltmp0:v[31]
      Temp ltmp0 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      Temp ltmp1 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      end_linear_vgpr(ltmp0);

      //! lv1: %ltmp1_2:v[31] = p_parallelcopy %ltmp1:v[30]
      //! v31: %_:v[0-30] = p_create_vector v31: undef
      RegClass v31 = RegClass::get(RegType::vgpr, 31 * 4);
      bld.pseudo(aco_opcode::p_create_vector, bld.def(v31), Operand(v31));

      //! p_end_linear_vgpr (latekill)%ltmp1_2:v[31]
      end_linear_vgpr(ltmp1);

      finish_ra_test(ra_test_policy{pessimistic});
   }
END_TEST

BEGIN_TEST(regalloc.linear_vgpr.alloc.killed_op)
   for (bool pessimistic : {false, true}) {
      const char* subvariant = pessimistic ? "_pessimistic" : "_optimistic";
      if (!setup_cs("", GFX8, CHIP_UNKNOWN, subvariant))
         continue;

      //>> v31: %tmp0:v[0-30] = p_unit_test
      //! v1: %tmp1:v[31] = p_unit_test
      Temp tmp0 =
         bld.pseudo(aco_opcode::p_unit_test, bld.def(RegClass::get(RegType::vgpr, 31 * 4)));
      Temp tmp1 = bld.pseudo(aco_opcode::p_unit_test, bld.def(v1));

      //! lv1: %ltmp0:v[31] = p_start_linear_vgpr %tmp1:v[31]
      //! p_end_linear_vgpr (latekill)%ltmp0:v[31]
      Temp ltmp0 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()), tmp1);
      end_linear_vgpr(ltmp0);

      bld.pseudo(aco_opcode::p_unit_test, tmp0);

      finish_ra_test(ra_test_policy{pessimistic});
   }
END_TEST

BEGIN_TEST(regalloc.linear_vgpr.alloc.move_killed_op)
   for (bool pessimistic : {false, true}) {
      const char* subvariant = pessimistic ? "_pessimistic" : "_optimistic";
      if (!setup_cs("", GFX8, CHIP_UNKNOWN, subvariant))
         continue;

      //>> v30: %tmp0:v[0-29] = p_unit_test
      //! v1: %tmp1:v[30] = p_unit_test
      //! v1: %tmp2:v[31] = p_unit_test
      Temp tmp0 =
         bld.pseudo(aco_opcode::p_unit_test, bld.def(RegClass::get(RegType::vgpr, 30 * 4)));
      Temp tmp1 = bld.pseudo(aco_opcode::p_unit_test, bld.def(v1));
      Temp tmp2 = bld.pseudo(aco_opcode::p_unit_test, bld.def(v1));

      //~gfx8_optimistic! v1: %tmp1_2:v[31], v1: %tmp2_2:v[30] = p_parallelcopy %tmp1:v[30], %tmp2:v[31]
      //~gfx8_pessimistic! v1: %tmp2_2:v[30], v1: %tmp1_2:v[31] = p_parallelcopy %tmp2:v[31], %tmp1:v[30]
      //! lv1: %ltmp0:v[31] = p_start_linear_vgpr %tmp1_2:v[31]
      //! p_end_linear_vgpr (latekill)%ltmp0:v[31]
      Temp ltmp0 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()), tmp1);
      end_linear_vgpr(ltmp0);

      //! p_unit_test %tmp0:v[0-29], %tmp2_2:v[30]
      bld.pseudo(aco_opcode::p_unit_test, tmp0, tmp2);

      finish_ra_test(ra_test_policy{pessimistic});
   }
END_TEST

BEGIN_TEST(regalloc.linear_vgpr.compact_for_future_def)
   for (bool cbr : {false, true}) {
      const char* subvariant = cbr ? "_cbranch" : "_branch";
      if (!setup_cs("", GFX8, CHIP_UNKNOWN, subvariant))
         continue;

      //>> lv2: %ltmp0:v[30-31] = p_start_linear_vgpr
      //! lv1: %ltmp1:v[29] = p_start_linear_vgpr
      //! lv1: %ltmp2:v[28] = p_start_linear_vgpr
      //! p_end_linear_vgpr (latekill)%ltmp1:v[29]
      Temp ltmp0 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v2.as_linear()));
      Temp ltmp1 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      Temp ltmp2 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      end_linear_vgpr(ltmp1);

      //! s1: %scc_tmp:scc = p_unit_test
      Temp scc_tmp = bld.pseudo(aco_opcode::p_unit_test, bld.def(s1, scc));

      //! lv1: %ltmp2_2:v[29] = p_parallelcopy %ltmp2:v[28]
      //~gfx8_cbranch! s2: %_:s[0-1] = p_cbranch_z %scc_tmp:scc
      //~gfx8_branch! s2: %_:s[0-1] = p_branch
      if (cbr)
         bld.branch(aco_opcode::p_cbranch_z, bld.def(s2), bld.scc(scc_tmp));
      else
         bld.branch(aco_opcode::p_branch, bld.def(s2));

      //! BB1
      //! /* logical preds: BB0, / linear preds: BB0, / kind: */
      bld.reset(program->create_and_insert_block());
      program->blocks[1].linear_preds.push_back(0);
      program->blocks[1].logical_preds.push_back(0);

      //! v29: %_:v[0-28] = p_unit_test
      //! s2: %_:s[0-1] = p_branch
      bld.pseudo(aco_opcode::p_unit_test, bld.def(RegClass::get(RegType::vgpr, 29 * 4)));
      bld.branch(aco_opcode::p_branch, bld.def(s2));

      //! BB2
      //! /* logical preds: BB1, / linear preds: BB1, / kind: uniform, top-level, */
      bld.reset(program->create_and_insert_block());
      program->blocks[2].linear_preds.push_back(1);
      program->blocks[2].logical_preds.push_back(1);
      program->blocks[2].kind |= block_kind_top_level;

      //! p_end_linear_vgpr (latekill)%ltmp0_2:v[30-31]
      //! p_end_linear_vgpr (latekill)%ltmp2_2:v[29]
      end_linear_vgpr(ltmp0);
      end_linear_vgpr(ltmp2);

      finish_ra_test(ra_test_policy());

      //~gfx8_cbranch>> lv1: %ltmp2_2:v[29] = p_parallelcopy %ltmp2:v[28] scc:1 scratch:s1
      //~gfx8_branch>> lv1: %ltmp2_2:v[29] = p_parallelcopy %ltmp2:v[28] scc:0 scratch:s0
      aco_ptr<Instruction>& parallelcopy = program->blocks[0].instructions[6];
      aco_print_instr(program->gfx_level, parallelcopy.get(), output);
      if (parallelcopy->isPseudo()) {
         fprintf(output, " scc:%u scratch:s%u\n", parallelcopy->pseudo().tmp_in_scc,
                 parallelcopy->pseudo().scratch_sgpr.reg());
      } else {
         fprintf(output, "\n");
      }
   }
END_TEST

BEGIN_TEST(regalloc.linear_vgpr.compact_for_future_phis)
   for (bool cbr : {false, true}) {
      const char* subvariant = cbr ? "_cbranch" : "_branch";
      if (!setup_cs("", GFX8, CHIP_UNKNOWN, subvariant))
         continue;

      //>> lv1: %ltmp0:v[31] = p_start_linear_vgpr
      //! lv1: %ltmp1:v[30] = p_start_linear_vgpr
      //! lv1: %ltmp2:v[29] = p_start_linear_vgpr
      //! p_end_linear_vgpr (latekill)%ltmp1:v[30]
      Temp ltmp0 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      Temp ltmp1 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      Temp ltmp2 = bld.pseudo(aco_opcode::p_start_linear_vgpr, bld.def(v1.as_linear()));
      end_linear_vgpr(ltmp1);

      //! lv1: %ltmp2_2:v[30] = p_parallelcopy %ltmp2:v[29]
      //~gfx8_cbranch! s2: %_:s[0-1] = p_cbranch_z %_:scc
      //~gfx8_branch! s2: %_:s[0-1] = p_branch
      if (cbr)
         bld.branch(aco_opcode::p_cbranch_z, bld.def(s2), Operand(scc, s1));
      else
         bld.branch(aco_opcode::p_branch, bld.def(s2));

      //! BB1
      //! /* logical preds: BB0, / linear preds: BB0, / kind: */
      bld.reset(program->create_and_insert_block());
      program->blocks[1].linear_preds.push_back(0);
      program->blocks[1].logical_preds.push_back(0);

      //! s2: %_:s[0-1] = p_branch
      bld.branch(aco_opcode::p_branch, bld.def(s2));

      //! BB2
      //! /* logical preds: BB1, / linear preds: BB1, / kind: uniform, top-level, */
      bld.reset(program->create_and_insert_block());
      program->blocks[2].linear_preds.push_back(1);
      program->blocks[2].logical_preds.push_back(1);
      program->blocks[2].kind |= block_kind_top_level;

      RegClass v30 = RegClass::get(RegType::vgpr, 30 * 4);
      //! v30: %tmp:v[0-29] = p_phi v30: undef
      //! p_unit_test %tmp:v[0-29]
      Temp tmp = bld.pseudo(aco_opcode::p_phi, bld.def(v30), Operand(v30));
      bld.pseudo(aco_opcode::p_unit_test, tmp);

      //! p_end_linear_vgpr (latekill)%ltmp0_2:v[31]
      //! p_end_linear_vgpr (latekill)%ltmp2_2:v[30]
      end_linear_vgpr(ltmp0);
      end_linear_vgpr(ltmp2);

      finish_ra_test(ra_test_policy());
   }
END_TEST
