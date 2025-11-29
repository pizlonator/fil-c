/*
 * Copyright © 2022 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */
#include "helpers.h"

using namespace aco;

BEGIN_TEST(setup_reduce_temp.divergent_if_phi)
   /*
    * This must have an end_linear_vgpr after the phi (to ensure it's live during the phi copies):
    * v0 = start_linear_vgpr
    * divergent_if (...) {
    *
    * } else {
    *    use_linear_vgpr(v0)
    * }
    * ... = phi ...
    */
   // TODO: fix the RA validator to spot this
   //>> s2: %_, v1: %a = p_startpgm
   if (!setup_cs("s2 v1", GFX9))
      return;

   //>> p_logical_start
   //>> p_logical_end
   bld.pseudo(aco_opcode::p_logical_start);
   bld.pseudo(aco_opcode::p_logical_end);

   //>> lv1: %lv = p_start_linear_vgpr
   emit_divergent_if_else(
      program.get(), bld, Operand(inputs[0]),
      [&]() -> void
      {
         //>> s1: %_, s2: %_, s1: %_:scc = p_reduce %a, (latekill)%lv, lv1: undef op:umin32 cluster_size:64
         Instruction* reduce =
            bld.reduction(aco_opcode::p_reduce, bld.def(s1), bld.def(bld.lm), bld.def(s1, scc),
                          inputs[1], Operand(v1.as_linear()), Operand(v1.as_linear()), umin32);
         reduce->reduction().cluster_size = bld.lm.bytes() * 8;
      },
      [&]() -> void
      {
         /* nothing */
      });
   bld.pseudo(aco_opcode::p_phi, bld.def(v1), Operand::c32(1), Operand::zero());
   //>> /* logical preds: BB1, BB4, / linear preds: BB4, BB5, / kind: uniform, top-level, merge, */
   //! p_end_linear_vgpr (latekill)%lv

   finish_setup_reduce_temp_test();
END_TEST
