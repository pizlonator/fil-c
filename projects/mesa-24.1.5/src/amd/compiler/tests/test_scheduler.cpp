/*
 * Copyright 2024 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#include "helpers.h"

using namespace aco;

BEGIN_TEST(vopd_sched.commutative)
   if (!setup_cs(NULL, GFX11, CHIP_UNKNOWN, "", 32))
      return;

   PhysReg reg_v0{256};
   PhysReg reg_v1{257};
   PhysReg reg_v2{258};
   PhysReg reg_v3{259};

   //>> p_unit_test 0
   //! v1: %0:v[1] = v_dual_add_f32 %0:v[3], %0:v[2] :: v1: %0:v[0] = v_dual_mul_f32 %0:v[2], %0:v[3]
   bld.pseudo(aco_opcode::p_unit_test, Operand::zero());
   bld.vop2(aco_opcode::v_mul_f32, Definition(reg_v0, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));
   bld.vop2(aco_opcode::v_add_f32, Definition(reg_v1, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));

   /* Neither of these opcodes are commutative. */
   bld.reset(program->create_and_insert_block());
   //>> p_unit_test 1
   //! v1: %0:v[0] = v_fmamk_f32 %0:v[2], %0:v[3], 0
   //! v1: %0:v[1] = v_lshlrev_b32 %0:v[2], %0:v[3]
   bld.pseudo(aco_opcode::p_unit_test, Operand::c32(1));
   bld.vop2(aco_opcode::v_fmamk_f32, Definition(reg_v0, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1), Operand::zero());
   bld.vop2(aco_opcode::v_lshlrev_b32, Definition(reg_v1, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));

   /* We have to change the opcode for subtractions. */
   bld.reset(program->create_and_insert_block());
   //>> p_unit_test 2
   //! v1: %0:v[1] = v_dual_subrev_f32 %0:v[3], %0:v[2] :: v1: %0:v[0] = v_dual_fmamk_f32 %0:v[2], %0:v[3], 0
   bld.pseudo(aco_opcode::p_unit_test, Operand::c32(2));
   bld.vop2(aco_opcode::v_fmamk_f32, Definition(reg_v0, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1), Operand::zero());
   bld.vop2(aco_opcode::v_sub_f32, Definition(reg_v1, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));

   bld.reset(program->create_and_insert_block());
   //>> p_unit_test 3
   //! v1: %0:v[0] = v_dual_fmamk_f32 %0:v[2], %0:v[3], 0 :: v1: %0:v[1] = v_dual_sub_f32 %0:v[3], %0:v[2]
   bld.pseudo(aco_opcode::p_unit_test, Operand::c32(3));
   bld.vop2(aco_opcode::v_subrev_f32, Definition(reg_v1, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));
   bld.vop2(aco_opcode::v_fmamk_f32, Definition(reg_v0, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1), Operand::zero());

   /* If we have to move the second instruction into OPY instead of OPX, then swapping must still be
    * correct. */
   bld.reset(program->create_and_insert_block());
   //>> p_unit_test 4
   //! v1: %0:v[0] = v_dual_mul_f32 %0:v[3], %0:v[2] :: v1: %0:v[1] = v_dual_lshlrev_b32 %0:v[2], %0:v[3]
   bld.pseudo(aco_opcode::p_unit_test, Operand::c32(4));
   bld.vop2(aco_opcode::v_mul_f32, Definition(reg_v0, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));
   bld.vop2(aco_opcode::v_lshlrev_b32, Definition(reg_v1, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));

   bld.reset(program->create_and_insert_block());
   //>> p_unit_test 5
   //! v1: %0:v[0] = v_dual_mul_f32 %0:v[3], %0:v[2] :: v1: %0:v[1] = v_dual_lshlrev_b32 %0:v[2], %0:v[3]
   bld.pseudo(aco_opcode::p_unit_test, Operand::c32(5));
   bld.vop2(aco_opcode::v_lshlrev_b32, Definition(reg_v1, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));
   bld.vop2(aco_opcode::v_mul_f32, Definition(reg_v0, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));

   finish_schedule_vopd_test();
END_TEST

BEGIN_TEST(vopd_sched.mov_to_add)
   if (!setup_cs(NULL, GFX11, CHIP_UNKNOWN, "", 32))
      return;

   PhysReg reg_v0{256};
   PhysReg reg_v1{257};
   PhysReg reg_v2{258};
   PhysReg reg_v3{259};

   //>> p_unit_test 0
   //! v1: %0:v[1] = v_dual_mov_b32 %0:v[2] :: v1: %0:v[0] = v_dual_add_nc_u32 0, %0:v[2]
   bld.pseudo(aco_opcode::p_unit_test, Operand::zero());
   bld.vop1(aco_opcode::v_mov_b32, Definition(reg_v0, v1), Operand(reg_v2, v1));
   bld.vop1(aco_opcode::v_mov_b32, Definition(reg_v1, v1), Operand(reg_v2, v1));

   /* We can't turn the v_mov_b32 into a v_add_u32 because then both instructions would be OPY-only.
    */
   bld.reset(program->create_and_insert_block());
   //>> p_unit_test 1
   //! v1: %0:v[0] = v_mov_b32 %0:v[2]
   //! v1: %0:v[1] = v_lshlrev_b32 %0:v[2], %0:v[3]
   bld.pseudo(aco_opcode::p_unit_test, Operand::c32(1));
   bld.vop1(aco_opcode::v_mov_b32, Definition(reg_v0, v1), Operand(reg_v2, v1));
   bld.vop2(aco_opcode::v_lshlrev_b32, Definition(reg_v1, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));

   bld.reset(program->create_and_insert_block());
   //>> p_unit_test 2
   //! v1: %0:v[1] = v_lshlrev_b32 %0:v[2], %0:v[3]
   //! v1: %0:v[0] = v_mov_b32 %0:v[2]
   bld.pseudo(aco_opcode::p_unit_test, Operand::c32(2));
   bld.vop2(aco_opcode::v_lshlrev_b32, Definition(reg_v1, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));
   bld.vop1(aco_opcode::v_mov_b32, Definition(reg_v0, v1), Operand(reg_v2, v1));

   bld.reset(program->create_and_insert_block());
   //>> p_unit_test 3
   //! v1: %0:v[0] = v_dual_mov_b32 %0:v[2] :: v1: %0:v[1] = v_dual_and_b32 %0:v[3], %0:v[2]
   bld.pseudo(aco_opcode::p_unit_test, Operand::c32(3));
   bld.vop1(aco_opcode::v_mov_b32, Definition(reg_v0, v1), Operand(reg_v2, v1));
   bld.vop2(aco_opcode::v_and_b32, Definition(reg_v1, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));

   bld.reset(program->create_and_insert_block());
   //>> p_unit_test 4
   //! v1: %0:v[0] = v_dual_mov_b32 %0:v[2] :: v1: %0:v[1] = v_dual_and_b32 %0:v[3], %0:v[2]
   bld.pseudo(aco_opcode::p_unit_test, Operand::c32(4));
   bld.vop2(aco_opcode::v_and_b32, Definition(reg_v1, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1));
   bld.vop1(aco_opcode::v_mov_b32, Definition(reg_v0, v1), Operand(reg_v2, v1));

   /* The v_add_u32 should be OPY, not OPX. */
   bld.reset(program->create_and_insert_block());
   //>> p_unit_test 5
   //! v1: %0:v[1] = v_dual_fmamk_f32 %0:v[2], %0:v[3], 0 :: v1: %0:v[0] = v_dual_add_nc_u32 0, %0:v[2]
   bld.pseudo(aco_opcode::p_unit_test, Operand::c32(5));
   bld.vop2(aco_opcode::v_fmamk_f32, Definition(reg_v1, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1), Operand::zero());
   bld.vop1(aco_opcode::v_mov_b32, Definition(reg_v0, v1), Operand(reg_v2, v1));

   bld.reset(program->create_and_insert_block());
   //>> p_unit_test 6
   //! v1: %0:v[1] = v_dual_fmamk_f32 %0:v[2], %0:v[3], 0 :: v1: %0:v[0] = v_dual_add_nc_u32 0, %0:v[2]
   bld.pseudo(aco_opcode::p_unit_test, Operand::c32(6));
   bld.vop1(aco_opcode::v_mov_b32, Definition(reg_v0, v1), Operand(reg_v2, v1));
   bld.vop2(aco_opcode::v_fmamk_f32, Definition(reg_v1, v1), Operand(reg_v2, v1),
            Operand(reg_v3, v1), Operand::zero());

   finish_schedule_vopd_test();
END_TEST
