/*
 * Copyright 2023 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#include "aco_ir.h"

#include "util/bitscan.h"
#include "util/macros.h"

#include <limits>

/*
 * This pass implements a simple forward list-scheduler which works on a small
 * partial DAG of 16 nodes at any time. Only ALU instructions are scheduled
 * entirely freely. Memory load instructions must be kept in-order and any other
 * instruction must not be re-scheduled at all.
 *
 * The main goal of this scheduler is to create more memory clauses, schedule
 * memory loads early, and to improve ALU instruction level parallelism.
 */

namespace aco {
namespace {

constexpr unsigned num_nodes = 16;
using mask_t = uint16_t;
static_assert(std::numeric_limits<mask_t>::digits >= num_nodes);

struct VOPDInfo {
   VOPDInfo() : is_opy_only(0), is_dst_odd(0), src_banks(0), has_literal(0), is_commutative(0) {}
   uint16_t is_opy_only : 1;
   uint16_t is_dst_odd : 1;
   uint16_t src_banks : 10; /* 0-3: src0, 4-7: src1, 8-9: src2 */
   uint16_t has_literal : 1;
   uint16_t is_commutative : 1;
   aco_opcode op = aco_opcode::num_opcodes;
   uint32_t literal = 0;
};

struct InstrInfo {
   Instruction* instr;
   int32_t priority;
   mask_t dependency_mask;       /* bitmask of nodes which have to be scheduled before this node. */
   uint8_t next_non_reorderable; /* index of next non-reorderable instruction node after this one. */
   bool potential_clause; /* indicates that this instruction is not (yet) immediately followed by a
                             reorderable instruction. */
};

struct RegisterInfo {
   mask_t read_mask; /* bitmask of nodes which have to be scheduled before the next write. */
   int8_t latency;   /* estimated latency of last register write. */
   uint8_t direct_dependency : 4;     /* node that has to be scheduled before any other access. */
   uint8_t has_direct_dependency : 1; /* whether there is an unscheduled direct dependency. */
   uint8_t padding : 3;
};

struct SchedILPContext {
   Program* program;
   bool is_vopd = false;
   InstrInfo nodes[num_nodes];
   RegisterInfo regs[512];
   mask_t non_reorder_mask = 0; /* bitmask of instruction nodes which should not be reordered. */
   mask_t active_mask = 0;      /* bitmask of valid instruction nodes. */
   uint8_t next_non_reorderable = UINT8_MAX; /* index of next node which should not be reordered. */
   uint8_t last_non_reorderable = UINT8_MAX; /* index of last node which should not be reordered. */

   /* VOPD scheduler: */
   VOPDInfo vopd[num_nodes];
   VOPDInfo prev_vopd_info;
   InstrInfo prev_info;

   mask_t vopd_odd_mask = 0;
   mask_t vopd_even_mask = 0;
};

/**
 * Returns true for side-effect free SALU and VALU instructions.
 */
bool
can_reorder(const Instruction* const instr)
{
   if (instr->isVALU())
      return true;
   if (!instr->isSALU() || instr->isSOPP())
      return false;

   switch (instr->opcode) {
   /* SOP2 */
   case aco_opcode::s_cbranch_g_fork:
   case aco_opcode::s_rfe_restore_b64:
   /* SOP1 */
   case aco_opcode::s_setpc_b64:
   case aco_opcode::s_swappc_b64:
   case aco_opcode::s_rfe_b64:
   case aco_opcode::s_cbranch_join:
   case aco_opcode::s_set_gpr_idx_idx:
   case aco_opcode::s_sendmsg_rtn_b32:
   case aco_opcode::s_sendmsg_rtn_b64:
   /* SOPK */
   case aco_opcode::s_cbranch_i_fork:
   case aco_opcode::s_getreg_b32:
   case aco_opcode::s_setreg_b32:
   case aco_opcode::s_setreg_imm32_b32:
   case aco_opcode::s_call_b64:
   case aco_opcode::s_waitcnt_vscnt:
   case aco_opcode::s_waitcnt_vmcnt:
   case aco_opcode::s_waitcnt_expcnt:
   case aco_opcode::s_waitcnt_lgkmcnt:
   case aco_opcode::s_subvector_loop_begin:
   case aco_opcode::s_subvector_loop_end:
   /* SOPC */
   case aco_opcode::s_setvskip:
   case aco_opcode::s_set_gpr_idx_on: return false;
   default: break;
   }

   return true;
}

VOPDInfo
get_vopd_info(const Instruction* instr)
{
   if (instr->format != Format::VOP1 && instr->format != Format::VOP2)
      return VOPDInfo();

   VOPDInfo info;
   info.is_commutative = true;
   switch (instr->opcode) {
   case aco_opcode::v_fmac_f32: info.op = aco_opcode::v_dual_fmac_f32; break;
   case aco_opcode::v_fmaak_f32: info.op = aco_opcode::v_dual_fmaak_f32; break;
   case aco_opcode::v_fmamk_f32:
      info.op = aco_opcode::v_dual_fmamk_f32;
      info.is_commutative = false;
      break;
   case aco_opcode::v_mul_f32: info.op = aco_opcode::v_dual_mul_f32; break;
   case aco_opcode::v_add_f32: info.op = aco_opcode::v_dual_add_f32; break;
   case aco_opcode::v_sub_f32: info.op = aco_opcode::v_dual_sub_f32; break;
   case aco_opcode::v_subrev_f32: info.op = aco_opcode::v_dual_subrev_f32; break;
   case aco_opcode::v_mul_legacy_f32: info.op = aco_opcode::v_dual_mul_dx9_zero_f32; break;
   case aco_opcode::v_mov_b32: info.op = aco_opcode::v_dual_mov_b32; break;
   case aco_opcode::v_cndmask_b32:
      info.op = aco_opcode::v_dual_cndmask_b32;
      info.is_commutative = false;
      break;
   case aco_opcode::v_max_f32: info.op = aco_opcode::v_dual_max_f32; break;
   case aco_opcode::v_min_f32: info.op = aco_opcode::v_dual_min_f32; break;
   case aco_opcode::v_dot2c_f32_f16: info.op = aco_opcode::v_dual_dot2acc_f32_f16; break;
   case aco_opcode::v_add_u32:
      info.op = aco_opcode::v_dual_add_nc_u32;
      info.is_opy_only = true;
      break;
   case aco_opcode::v_lshlrev_b32:
      info.op = aco_opcode::v_dual_lshlrev_b32;
      info.is_opy_only = true;
      info.is_commutative = false;
      break;
   case aco_opcode::v_and_b32:
      info.op = aco_opcode::v_dual_and_b32;
      info.is_opy_only = true;
      break;
   default: return VOPDInfo();
   }

   /* Each instruction may use at most one SGPR. */
   if (instr->opcode == aco_opcode::v_cndmask_b32 && instr->operands[0].isOfType(RegType::sgpr))
      return VOPDInfo();

   info.is_dst_odd = instr->definitions[0].physReg().reg() & 0x1;

   static const unsigned bank_mask[3] = {0x3, 0x3, 0x1};
   bool has_sgpr = false;
   for (unsigned i = 0; i < instr->operands.size(); i++) {
      unsigned port = (instr->opcode == aco_opcode::v_fmamk_f32 && i == 1) ? 2 : i;
      if (instr->operands[i].isOfType(RegType::vgpr))
         info.src_banks |= 1 << (port * 4 + (instr->operands[i].physReg().reg() & bank_mask[port]));

      /* Check all operands because of fmaak/fmamk. */
      if (instr->operands[i].isLiteral()) {
         assert(!info.has_literal || info.literal == instr->operands[i].constantValue());
         info.has_literal = true;
         info.literal = instr->operands[i].constantValue();
      }

      /* Check all operands because of cndmask. */
      has_sgpr |= !instr->operands[i].isConstant() && instr->operands[i].isOfType(RegType::sgpr);
   }

   /* An instruction can't use both a literal and an SGPR. */
   if (has_sgpr && info.has_literal)
      return VOPDInfo();

   info.is_commutative &= instr->operands[0].isOfType(RegType::vgpr);

   return info;
}

bool
is_vopd_compatible(const VOPDInfo& a, const VOPDInfo& b)
{
   if ((a.is_opy_only && b.is_opy_only) || (a.is_dst_odd == b.is_dst_odd))
      return false;

   /* Both can use a literal, but it must be the same literal. */
   if (a.has_literal && b.has_literal && a.literal != b.literal)
      return false;

   /* The rest is checking src VGPR bank compatibility. */
   if ((a.src_banks & b.src_banks) == 0)
      return true;

   if (!a.is_commutative && !b.is_commutative)
      return false;

   uint16_t src0 = a.src_banks & 0xf;
   uint16_t src1 = a.src_banks & 0xf0;
   uint16_t src2 = a.src_banks & 0x300;
   uint16_t a_src_banks = (src0 << 4) | (src1 >> 4) | src2;
   if ((a_src_banks & b.src_banks) != 0)
      return false;

   /* If we have to turn v_mov_b32 into v_add_u32 but there is already an OPY-only instruction,
    * we can't do it.
    */
   if (a.op == aco_opcode::v_dual_mov_b32 && !b.is_commutative && b.is_opy_only)
      return false;
   if (b.op == aco_opcode::v_dual_mov_b32 && !a.is_commutative && a.is_opy_only)
      return false;

   return true;
}

bool
can_use_vopd(const SchedILPContext& ctx, unsigned idx)
{
   VOPDInfo cur_vopd = ctx.vopd[idx];
   Instruction* first = ctx.nodes[idx].instr;
   Instruction* second = ctx.prev_info.instr;

   if (!second)
      return false;

   if (ctx.prev_vopd_info.op == aco_opcode::num_opcodes || cur_vopd.op == aco_opcode::num_opcodes)
      return false;

   if (!is_vopd_compatible(ctx.prev_vopd_info, cur_vopd))
      return false;

   assert(first->definitions.size() == 1);
   assert(first->definitions[0].size() == 1);
   assert(second->definitions.size() == 1);
   assert(second->definitions[0].size() == 1);

   /* Check for WaW dependency. */
   if (first->definitions[0].physReg() == second->definitions[0].physReg())
      return false;

   /* Check for RaW dependency. */
   for (Operand op : second->operands) {
      assert(op.size() == 1);
      if (first->definitions[0].physReg() == op.physReg())
         return false;
   }

   /* WaR dependencies are not a concern. */
   return true;
}

unsigned
get_latency(const Instruction* const instr)
{
   /* Note, that these are not accurate latency estimations. */
   if (instr->isVALU())
      return 5;
   if (instr->isSALU())
      return 2;
   if (instr->isVMEM() || instr->isFlatLike())
      return 32;
   if (instr->isSMEM())
      return 5;
   if (instr->accessesLDS())
      return 2;

   return 0;
}

bool
is_memory_instr(const Instruction* const instr)
{
   /* For memory instructions, we allow to reorder them with ALU if it helps
    * to form larger clauses or to increase def-use distances.
    */
   return instr->isVMEM() || instr->isFlatLike() || instr->isSMEM() || instr->accessesLDS();
}

constexpr unsigned max_sgpr = 128;
constexpr unsigned min_vgpr = 256;

void
add_entry(SchedILPContext& ctx, Instruction* const instr, const uint32_t idx)
{
   InstrInfo& entry = ctx.nodes[idx];
   entry.instr = instr;
   entry.priority = 0;
   const mask_t mask = BITFIELD_BIT(idx);
   bool reorder = can_reorder(instr);
   ctx.active_mask |= mask;

   if (ctx.is_vopd) {
      VOPDInfo vopd = get_vopd_info(entry.instr);

      ctx.vopd[idx] = vopd;
      ctx.vopd_odd_mask &= ~mask;
      ctx.vopd_odd_mask |= vopd.is_dst_odd ? mask : 0;
      ctx.vopd_even_mask &= ~mask;
      ctx.vopd_even_mask |= vopd.is_dst_odd || vopd.op == aco_opcode::num_opcodes ? 0 : mask;
   }

   for (const Operand& op : instr->operands) {
      assert(op.isFixed());
      unsigned reg = op.physReg();
      if (reg >= max_sgpr && reg != scc && reg < min_vgpr) {
         reorder &= reg != pops_exiting_wave_id;
         continue;
      }

      for (unsigned i = 0; i < op.size(); i++) {
         RegisterInfo& reg_info = ctx.regs[reg + i];

         /* Add register reads. */
         reg_info.read_mask |= mask;

         int cycles_since_reg_write = num_nodes;
         if (reg_info.has_direct_dependency) {
            /* A previous dependency is still part of the DAG. */
            entry.dependency_mask |= BITFIELD_BIT(reg_info.direct_dependency);
            cycles_since_reg_write = ctx.nodes[reg_info.direct_dependency].priority;
         }

         if (reg_info.latency) {
            /* Ignore and reset register latencies for memory loads and other non-reorderable
             * instructions. We schedule these as early as possible anyways.
             */
            if (reorder && reg_info.latency > cycles_since_reg_write) {
               entry.priority = MIN2(entry.priority, cycles_since_reg_write - reg_info.latency);

               /* If a previous register write created some latency, ensure that this
                * is the first read of the register by making this instruction a direct
                * dependency of all following register reads.
                */
               reg_info.has_direct_dependency = 1;
               reg_info.direct_dependency = idx;
            }
            reg_info.latency = 0;
         }
      }
   }

   /* Check if this instructions reads implicit registers. */
   if (needs_exec_mask(instr)) {
      for (unsigned reg = exec_lo; reg <= exec_hi; reg++) {
         if (ctx.regs[reg].has_direct_dependency)
            entry.dependency_mask |= BITFIELD_BIT(ctx.regs[reg].direct_dependency);
         ctx.regs[reg].read_mask |= mask;
      }
   }
   if (ctx.program->gfx_level < GFX10 && instr->isScratch()) {
      for (unsigned reg = flat_scr_lo; reg <= flat_scr_hi; reg++) {
         if (ctx.regs[reg].has_direct_dependency)
            entry.dependency_mask |= BITFIELD_BIT(ctx.regs[reg].direct_dependency);
         ctx.regs[reg].read_mask |= mask;
      }
   }

   for (const Definition& def : instr->definitions) {
      for (unsigned i = 0; i < def.size(); i++) {
         RegisterInfo& reg_info = ctx.regs[def.physReg().reg() + i];

         /* Add all previous register reads and writes to the dependencies. */
         entry.dependency_mask |= reg_info.read_mask;
         reg_info.read_mask = mask;

         /* This register write is a direct dependency for all following reads. */
         reg_info.has_direct_dependency = 1;
         reg_info.direct_dependency = idx;

         if (!ctx.is_vopd) {
            /* Add latency information for the next register read. */
            reg_info.latency = get_latency(instr);
         }
      }
   }

   if (!reorder) {
      ctx.non_reorder_mask |= mask;

      /* Set this node as last non-reorderable instruction */
      if (ctx.next_non_reorderable == UINT8_MAX) {
         ctx.next_non_reorderable = idx;
      } else {
         ctx.nodes[ctx.last_non_reorderable].next_non_reorderable = idx;
      }
      ctx.last_non_reorderable = idx;
      entry.next_non_reorderable = UINT8_MAX;

      /* Just don't reorder these at all. */
      if (!is_memory_instr(instr) || instr->definitions.empty() ||
          get_sync_info(instr).semantics & semantic_volatile || ctx.is_vopd) {
         /* Add all previous instructions as dependencies. */
         entry.dependency_mask = ctx.active_mask;
      }

      /* Remove non-reorderable instructions from dependencies, since WaR dependencies can interfere
       * with clause formation. This should be fine, since these are always scheduled in-order and
       * any cases that are actually a concern for clause formation are added as transitive
       * dependencies. */
      entry.dependency_mask &= ~ctx.non_reorder_mask;
      entry.potential_clause = true;
   } else if (ctx.last_non_reorderable != UINT8_MAX) {
      ctx.nodes[ctx.last_non_reorderable].potential_clause = false;
   }

   entry.dependency_mask &= ~mask;

   for (unsigned i = 0; i < num_nodes; i++) {
      if (!ctx.nodes[i].instr || i == idx)
         continue;

      /* Add transitive dependencies. */
      if (entry.dependency_mask & BITFIELD_BIT(i))
         entry.dependency_mask |= ctx.nodes[i].dependency_mask;

      /* increment base priority */
      ctx.nodes[i].priority++;
   }
}

void
remove_entry(SchedILPContext& ctx, const Instruction* const instr, const uint32_t idx)
{
   const mask_t mask = ~BITFIELD_BIT(idx);
   ctx.active_mask &= mask;

   for (const Operand& op : instr->operands) {
      const unsigned reg = op.physReg();
      if (reg >= max_sgpr && reg != scc && reg < min_vgpr)
         continue;

      for (unsigned i = 0; i < op.size(); i++) {
         RegisterInfo& reg_info = ctx.regs[reg + i];
         reg_info.read_mask &= mask;
         reg_info.has_direct_dependency &= reg_info.direct_dependency != idx;
      }
   }
   if (needs_exec_mask(instr)) {
      ctx.regs[exec_lo].read_mask &= mask;
      ctx.regs[exec_hi].read_mask &= mask;
   }
   if (ctx.program->gfx_level < GFX10 && instr->isScratch()) {
      ctx.regs[flat_scr_lo].read_mask &= mask;
      ctx.regs[flat_scr_hi].read_mask &= mask;
   }
   for (const Definition& def : instr->definitions) {
      for (unsigned i = 0; i < def.size(); i++) {
         unsigned reg = def.physReg().reg() + i;
         ctx.regs[reg].read_mask &= mask;
         ctx.regs[reg].has_direct_dependency &= ctx.regs[reg].direct_dependency != idx;
      }
   }

   for (unsigned i = 0; i < num_nodes; i++)
      ctx.nodes[i].dependency_mask &= mask;

   if (ctx.next_non_reorderable == idx) {
      ctx.non_reorder_mask &= mask;
      ctx.next_non_reorderable = ctx.nodes[idx].next_non_reorderable;
      if (ctx.last_non_reorderable == idx)
         ctx.last_non_reorderable = UINT8_MAX;
   }
}

/**
 * Returns a bitfield of nodes which have to be scheduled before the
 * next non-reorderable instruction.
 * If the next non-reorderable instruction can form a clause, returns the
 * dependencies of the entire clause.
 */
mask_t
collect_clause_dependencies(const SchedILPContext& ctx, const uint8_t next, mask_t clause_mask)
{
   const InstrInfo& entry = ctx.nodes[next];
   mask_t dependencies = entry.dependency_mask;
   clause_mask |= (entry.potential_clause << next);

   if (!is_memory_instr(entry.instr))
      return dependencies;

   /* If this is potentially an "open" clause, meaning that the clause might
    * consist of instruction not yet added to the DAG, consider all previous
    * instructions as dependencies. This prevents splitting of larger, already
    * formed clauses.
    */
   if (next == ctx.last_non_reorderable && entry.potential_clause)
      return (~clause_mask & ctx.active_mask) | dependencies;

   if (entry.next_non_reorderable == UINT8_MAX)
      return dependencies;

   /* Check if this can form a clause with the following non-reorderable instruction */
   if (should_form_clause(entry.instr, ctx.nodes[entry.next_non_reorderable].instr)) {
      mask_t clause_deps =
         collect_clause_dependencies(ctx, entry.next_non_reorderable, clause_mask);

      /* if the following clause is independent from us, add their dependencies */
      if (!(clause_deps & BITFIELD_BIT(next)))
         dependencies |= clause_deps;
   }

   return dependencies;
}

/**
 * Returns the index of the next instruction to be selected.
 */
unsigned
select_instruction_ilp(const SchedILPContext& ctx)
{
   mask_t mask = ctx.active_mask;

   /* First, collect all dependencies of the next non-reorderable instruction(s).
    * These make up the list of possible candidates.
    */
   if (ctx.next_non_reorderable != UINT8_MAX)
      mask = collect_clause_dependencies(ctx, ctx.next_non_reorderable, 0);

   /* If the next non-reorderable instruction has no dependencies, select it */
   if (mask == 0)
      return ctx.next_non_reorderable;

   /* Otherwise, select the instruction with highest priority of all candidates. */
   unsigned idx = -1u;
   int32_t priority = INT32_MIN;
   u_foreach_bit (i, mask) {
      const InstrInfo& candidate = ctx.nodes[i];

      /* Check if the candidate has pending dependencies. */
      if (candidate.dependency_mask)
         continue;

      if (idx == -1u || candidate.priority > priority) {
         idx = i;
         priority = candidate.priority;
      }
   }

   assert(idx != -1u);
   return idx;
}

bool
compare_nodes_vopd(const SchedILPContext& ctx, int num_vopd_odd_minus_even, bool* use_vopd,
                   unsigned current, unsigned candidate)
{
   if (can_use_vopd(ctx, candidate)) {
      /* If we can form a VOPD instruction, always prefer to do so. */
      if (!*use_vopd) {
         *use_vopd = true;
         return true;
      }
   } else {
      if (*use_vopd)
         return false;

      /* Neither current nor candidate can form a VOPD instruction with the previously scheduled
       * instruction. */
      VOPDInfo current_vopd = ctx.vopd[current];
      VOPDInfo candidate_vopd = ctx.vopd[candidate];

      /* Delay scheduling VOPD-capable instructions in case an opportunity appears later. */
      bool current_vopd_capable = current_vopd.op != aco_opcode::num_opcodes;
      bool candidate_vopd_capable = candidate_vopd.op != aco_opcode::num_opcodes;
      if (current_vopd_capable != candidate_vopd_capable)
         return !candidate_vopd_capable;

      /* If we have to select from VOPD-capable instructions, prefer maintaining a balance of
       * odd/even instructions, in case selecting this instruction fails to make a pair.
       */
      if (current_vopd_capable && num_vopd_odd_minus_even != 0) {
         assert(candidate_vopd_capable);
         bool prefer_vopd_dst_odd = num_vopd_odd_minus_even > 0;
         if (current_vopd.is_dst_odd != candidate_vopd.is_dst_odd)
            return prefer_vopd_dst_odd ? candidate_vopd.is_dst_odd : !candidate_vopd.is_dst_odd;
      }
   }

   return ctx.nodes[candidate].priority > ctx.nodes[current].priority;
}

unsigned
select_instruction_vopd(const SchedILPContext& ctx, bool* use_vopd)
{
   *use_vopd = false;

   mask_t mask = ctx.active_mask;
   if (ctx.next_non_reorderable != UINT8_MAX)
      mask = ctx.nodes[ctx.next_non_reorderable].dependency_mask;

   if (mask == 0)
      return ctx.next_non_reorderable;

   int num_vopd_odd_minus_even =
      (int)util_bitcount(ctx.vopd_odd_mask & mask) - (int)util_bitcount(ctx.vopd_even_mask & mask);

   unsigned cur = -1u;
   u_foreach_bit (i, mask) {
      const InstrInfo& candidate = ctx.nodes[i];

      /* Check if the candidate has pending dependencies. */
      if (candidate.dependency_mask)
         continue;

      if (cur == -1u) {
         cur = i;
         *use_vopd = can_use_vopd(ctx, i);
      } else if (compare_nodes_vopd(ctx, num_vopd_odd_minus_even, use_vopd, cur, i)) {
         cur = i;
      }
   }

   assert(cur != -1u);
   return cur;
}

void
get_vopd_opcode_operands(Instruction* instr, const VOPDInfo& info, bool swap, aco_opcode* op,
                         unsigned* num_operands, Operand* operands)
{
   *op = info.op;
   *num_operands += instr->operands.size();
   std::copy(instr->operands.begin(), instr->operands.end(), operands);

   if (swap && info.op == aco_opcode::v_dual_mov_b32) {
      *op = aco_opcode::v_dual_add_nc_u32;
      (*num_operands)++;
      operands[0] = Operand::zero();
      operands[1] = instr->operands[0];
   } else if (swap) {
      if (info.op == aco_opcode::v_dual_sub_f32)
         *op = aco_opcode::v_dual_subrev_f32;
      else if (info.op == aco_opcode::v_dual_subrev_f32)
         *op = aco_opcode::v_dual_sub_f32;
      std::swap(operands[0], operands[1]);
   }
}

Instruction*
create_vopd_instruction(const SchedILPContext& ctx, unsigned idx)
{
   Instruction* x = ctx.prev_info.instr;
   Instruction* y = ctx.nodes[idx].instr;
   VOPDInfo x_info = ctx.prev_vopd_info;
   VOPDInfo y_info = ctx.vopd[idx];

   bool swap_x = false, swap_y = false;
   if (x_info.src_banks & y_info.src_banks) {
      assert(x_info.is_commutative || y_info.is_commutative);
      /* Avoid swapping v_mov_b32 because it will become an OPY-only opcode. */
      if (x_info.op == aco_opcode::v_dual_mov_b32 && !y_info.is_commutative) {
         swap_x = true;
         x_info.is_opy_only = true;
      } else {
         swap_x = x_info.is_commutative && x_info.op != aco_opcode::v_dual_mov_b32;
         swap_y = y_info.is_commutative && !swap_x;
      }
   }

   if (x_info.is_opy_only) {
      std::swap(x, y);
      std::swap(x_info, y_info);
      std::swap(swap_x, swap_y);
   }

   aco_opcode x_op, y_op;
   unsigned num_operands = 0;
   Operand operands[6];
   get_vopd_opcode_operands(x, x_info, swap_x, &x_op, &num_operands, operands);
   get_vopd_opcode_operands(y, y_info, swap_y, &y_op, &num_operands, operands + num_operands);

   Instruction* instr = create_instruction(x_op, Format::VOPD, num_operands, 2);
   instr->vopd().opy = y_op;
   instr->definitions[0] = x->definitions[0];
   instr->definitions[1] = y->definitions[0];
   std::copy(operands, operands + num_operands, instr->operands.begin());

   return instr;
}

template <typename It>
void
do_schedule(SchedILPContext& ctx, It& insert_it, It& remove_it, It instructions_begin,
            It instructions_end)
{
   for (unsigned i = 0; i < num_nodes; i++) {
      if (remove_it == instructions_end)
         break;

      add_entry(ctx, (remove_it++)->get(), i);
   }

   ctx.prev_info.instr = NULL;
   bool use_vopd = false;

   while (ctx.active_mask) {
      unsigned next_idx =
         ctx.is_vopd ? select_instruction_vopd(ctx, &use_vopd) : select_instruction_ilp(ctx);
      Instruction* next_instr = ctx.nodes[next_idx].instr;

      if (use_vopd) {
         std::prev(insert_it)->reset(create_vopd_instruction(ctx, next_idx));
         ctx.prev_info.instr = NULL;
      } else {
         (insert_it++)->reset(next_instr);
         ctx.prev_info = ctx.nodes[next_idx];
         ctx.prev_vopd_info = ctx.vopd[next_idx];
      }

      remove_entry(ctx, next_instr, next_idx);
      ctx.nodes[next_idx].instr = NULL;

      if (remove_it != instructions_end) {
         add_entry(ctx, (remove_it++)->get(), next_idx);
      } else if (ctx.last_non_reorderable != UINT8_MAX) {
         ctx.nodes[ctx.last_non_reorderable].potential_clause = false;
         ctx.last_non_reorderable = UINT8_MAX;
      }
   }
}

} // namespace

void
schedule_ilp(Program* program)
{
   SchedILPContext ctx = {program};

   for (Block& block : program->blocks) {
      auto it = block.instructions.begin();
      auto insert_it = block.instructions.begin();
      do_schedule(ctx, insert_it, it, block.instructions.begin(), block.instructions.end());
      block.instructions.resize(insert_it - block.instructions.begin());
   }
}

void
schedule_vopd(Program* program)
{
   if (program->gfx_level < GFX11 || program->wave_size != 32)
      return;

   SchedILPContext ctx = {program};
   ctx.is_vopd = true;

   for (Block& block : program->blocks) {
      auto it = block.instructions.rbegin();
      auto insert_it = block.instructions.rbegin();
      do_schedule(ctx, insert_it, it, block.instructions.rbegin(), block.instructions.rend());
      block.instructions.erase(block.instructions.begin(), insert_it.base());
   }
}

} // namespace aco
