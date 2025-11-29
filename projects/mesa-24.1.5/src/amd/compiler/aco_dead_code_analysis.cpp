/*
 * Copyright © 2019 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "aco_ir.h"

#include <algorithm>
#include <vector>

/*
 * Implements an analysis pass to determine the number of uses
 * for each SSA-definition.
 *
 * This pass assumes that no loop header phis are dead code.
 */

namespace aco {
namespace {

void
process_loop_header_phis(std::vector<uint16_t>& uses, Block& block)
{
   for (aco_ptr<Instruction>& instr : block.instructions) {
      if (!is_phi(instr))
         return;
      for (const Operand& op : instr->operands) {
         if (op.isTemp())
            uses[op.tempId()]++;
      }
   }
}

void
process_block(std::vector<uint16_t>& uses, Block& block)
{
   for (auto it = block.instructions.rbegin(); it != block.instructions.rend(); it++) {
      aco_ptr<Instruction>& instr = *it;
      if ((block.kind & block_kind_loop_header) && is_phi(instr))
         break;

      if (!is_dead(uses, instr.get())) {
         for (const Operand& op : instr->operands) {
            if (op.isTemp())
               uses[op.tempId()]++;
         }
      }
   }
}

} /* end namespace */

std::vector<uint16_t>
dead_code_analysis(Program* program)
{
   std::vector<uint16_t> uses(program->peekAllocationId());

   for (Block& block : program->blocks) {
      if (block.kind & block_kind_loop_header)
         process_loop_header_phis(uses, block);
   }

   for (auto it = program->blocks.rbegin(); it != program->blocks.rend(); it++)
      process_block(uses, *it);

   return uses;
}

} // namespace aco
