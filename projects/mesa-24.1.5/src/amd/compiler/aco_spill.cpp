/*
 * Copyright © 2018 Valve Corporation
 * Copyright © 2018 Google
 *
 * SPDX-License-Identifier: MIT
 */

#include "aco_builder.h"
#include "aco_ir.h"
#include "aco_util.h"

#include "common/sid.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace std {
template <> struct hash<aco::Temp> {
   size_t operator()(aco::Temp temp) const noexcept
   {
      uint32_t v;
      std::memcpy(&v, &temp, sizeof(temp));
      return std::hash<uint32_t>{}(v);
   }
};
} // namespace std

/*
 * Implements the spilling algorithm on SSA-form based on
 * "Register Spilling and Live-Range Splitting for SSA-Form Programs"
 * by Matthias Braun and Sebastian Hack.
 *
 * Key difference between this algorithm and the min-algorithm from the paper
 * is the use of average use distances rather than next-use distances per
 * instruction.
 * As we decrement the number of remaining uses, the average use distances
 * give an approximation of the next-use distances while being computationally
 * and memory-wise less expensive.
 */

namespace aco {

namespace {

struct remat_info {
   Instruction* instr;
};

struct loop_info {
   uint32_t index;
   aco::unordered_map<Temp, uint32_t> spills;
   IDSet live_in;
};

struct use_info {
   uint32_t num_uses = 0;
   uint32_t last_use = 0;
   float score() { return last_use / num_uses; }
};

struct spill_ctx {
   RegisterDemand target_pressure;
   Program* program;
   aco::monotonic_buffer_resource memory;

   live& live_vars;
   std::vector<aco::map<Temp, Temp>> renames;
   std::vector<aco::unordered_map<Temp, uint32_t>> spills_entry;
   std::vector<aco::unordered_map<Temp, uint32_t>> spills_exit;

   std::vector<bool> processed;
   std::vector<loop_info> loop;

   std::vector<use_info> ssa_infos;
   std::vector<std::pair<RegClass, std::unordered_set<uint32_t>>> interferences;
   std::vector<std::vector<uint32_t>> affinities;
   std::vector<bool> is_reloaded;
   aco::unordered_map<Temp, remat_info> remat;
   std::set<Instruction*> unused_remats;
   unsigned wave_size;

   unsigned sgpr_spill_slots;
   unsigned vgpr_spill_slots;
   Temp scratch_rsrc;

   spill_ctx(const RegisterDemand target_pressure_, Program* program_, live& live_vars_)
       : target_pressure(target_pressure_), program(program_), memory(), live_vars(live_vars_),
         renames(program->blocks.size(), aco::map<Temp, Temp>(memory)),
         spills_entry(program->blocks.size(), aco::unordered_map<Temp, uint32_t>(memory)),
         spills_exit(program->blocks.size(), aco::unordered_map<Temp, uint32_t>(memory)),
         processed(program->blocks.size(), false), ssa_infos(program->peekAllocationId()),
         remat(memory), wave_size(program->wave_size), sgpr_spill_slots(0), vgpr_spill_slots(0)
   {}

   void add_affinity(uint32_t first, uint32_t second)
   {
      unsigned found_first = affinities.size();
      unsigned found_second = affinities.size();
      for (unsigned i = 0; i < affinities.size(); i++) {
         std::vector<uint32_t>& vec = affinities[i];
         for (uint32_t entry : vec) {
            if (entry == first)
               found_first = i;
            else if (entry == second)
               found_second = i;
         }
      }
      if (found_first == affinities.size() && found_second == affinities.size()) {
         affinities.emplace_back(std::vector<uint32_t>({first, second}));
      } else if (found_first < affinities.size() && found_second == affinities.size()) {
         affinities[found_first].push_back(second);
      } else if (found_second < affinities.size() && found_first == affinities.size()) {
         affinities[found_second].push_back(first);
      } else if (found_first != found_second) {
         /* merge second into first */
         affinities[found_first].insert(affinities[found_first].end(),
                                        affinities[found_second].begin(),
                                        affinities[found_second].end());
         affinities.erase(std::next(affinities.begin(), found_second));
      } else {
         assert(found_first == found_second);
      }
   }

   uint32_t add_to_spills(Temp to_spill, aco::unordered_map<Temp, uint32_t>& spills)
   {
      const uint32_t spill_id = allocate_spill_id(to_spill.regClass());
      for (auto pair : spills)
         add_interference(spill_id, pair.second);
      if (!loop.empty()) {
         for (auto pair : loop.back().spills)
            add_interference(spill_id, pair.second);
      }

      spills[to_spill] = spill_id;
      return spill_id;
   }

   void add_interference(uint32_t first, uint32_t second)
   {
      if (interferences[first].first.type() != interferences[second].first.type())
         return;

      bool inserted = interferences[first].second.insert(second).second;
      if (inserted)
         interferences[second].second.insert(first);
   }

   uint32_t allocate_spill_id(RegClass rc)
   {
      interferences.emplace_back(rc, std::unordered_set<uint32_t>());
      is_reloaded.push_back(false);
      return next_spill_id++;
   }

   uint32_t next_spill_id = 0;
};

/**
 * Gathers information about the number of uses and point of last use
 * per SSA value.
 *
 * Live-out variables are converted to live-in.
 */
void
gather_ssa_use_info(spill_ctx& ctx)
{
   unsigned instruction_idx = 0;
   for (Block& block : ctx.program->blocks) {
      IDSet& live_set = ctx.live_vars.live_out[block.index];

      for (int i = block.instructions.size() - 1; i >= 0; i--) {
         aco_ptr<Instruction>& instr = block.instructions[i];
         const bool phi = is_phi(instr);

         for (const Definition& def : instr->definitions) {
            if (!phi && def.isTemp() && !def.isKill())
               live_set.erase(def.tempId());
         }
         for (const Operand& op : instr->operands) {
            if (op.isTemp()) {
               use_info& info = ctx.ssa_infos[op.tempId()];
               info.num_uses++;
               info.last_use = std::max(info.last_use, instruction_idx + i);
               if (!phi && op.isFirstKill())
                  live_set.insert(op.tempId());
            }
         }
      }

      /* All live-in variables at loop headers get an additional artificial use.
       * As we decrement the number of uses while processing the blocks, this
       * ensures that the number of uses won't becomes zero before the loop
       * (and the variables' live-ranges) end.
       */
      if (block.kind & block_kind_loop_header) {
         for (unsigned t : live_set)
            ctx.ssa_infos[t].num_uses++;
      }

      instruction_idx += block.instructions.size();
   }
}

bool
should_rematerialize(aco_ptr<Instruction>& instr)
{
   /* TODO: rematerialization is only supported for VOP1, SOP1 and PSEUDO */
   if (instr->format != Format::VOP1 && instr->format != Format::SOP1 &&
       instr->format != Format::PSEUDO && instr->format != Format::SOPK)
      return false;
   /* TODO: pseudo-instruction rematerialization is only supported for
    * p_create_vector/p_parallelcopy */
   if (instr->isPseudo() && instr->opcode != aco_opcode::p_create_vector &&
       instr->opcode != aco_opcode::p_parallelcopy)
      return false;
   if (instr->isSOPK() && instr->opcode != aco_opcode::s_movk_i32)
      return false;

   for (const Operand& op : instr->operands) {
      /* TODO: rematerialization using temporaries isn't yet supported */
      if (!op.isConstant())
         return false;
   }

   /* TODO: rematerialization with multiple definitions isn't yet supported */
   if (instr->definitions.size() > 1)
      return false;

   return true;
}

aco_ptr<Instruction>
do_reload(spill_ctx& ctx, Temp tmp, Temp new_name, uint32_t spill_id)
{
   std::unordered_map<Temp, remat_info>::iterator remat = ctx.remat.find(tmp);
   if (remat != ctx.remat.end()) {
      Instruction* instr = remat->second.instr;
      assert((instr->isVOP1() || instr->isSOP1() || instr->isPseudo() || instr->isSOPK()) &&
             "unsupported");
      assert((instr->format != Format::PSEUDO || instr->opcode == aco_opcode::p_create_vector ||
              instr->opcode == aco_opcode::p_parallelcopy) &&
             "unsupported");
      assert(instr->definitions.size() == 1 && "unsupported");

      aco_ptr<Instruction> res;
      res.reset(create_instruction(instr->opcode, instr->format, instr->operands.size(),
                                   instr->definitions.size()));
      if (instr->isSOPK())
         res->salu().imm = instr->salu().imm;

      for (unsigned i = 0; i < instr->operands.size(); i++) {
         res->operands[i] = instr->operands[i];
         if (instr->operands[i].isTemp()) {
            assert(false && "unsupported");
            if (ctx.remat.count(instr->operands[i].getTemp()))
               ctx.unused_remats.erase(ctx.remat[instr->operands[i].getTemp()].instr);
         }
      }
      res->definitions[0] = Definition(new_name);
      return res;
   } else {
      aco_ptr<Instruction> reload{create_instruction(aco_opcode::p_reload, Format::PSEUDO, 1, 1)};
      reload->operands[0] = Operand::c32(spill_id);
      reload->definitions[0] = Definition(new_name);
      ctx.is_reloaded[spill_id] = true;
      return reload;
   }
}

void
get_rematerialize_info(spill_ctx& ctx)
{
   for (Block& block : ctx.program->blocks) {
      bool logical = false;
      for (aco_ptr<Instruction>& instr : block.instructions) {
         if (instr->opcode == aco_opcode::p_logical_start)
            logical = true;
         else if (instr->opcode == aco_opcode::p_logical_end)
            logical = false;
         if (logical && should_rematerialize(instr)) {
            for (const Definition& def : instr->definitions) {
               if (def.isTemp()) {
                  ctx.remat[def.getTemp()] = remat_info{instr.get()};
                  ctx.unused_remats.insert(instr.get());
               }
            }
         }
      }
   }
}

RegisterDemand
get_demand_before(spill_ctx& ctx, unsigned block_idx, unsigned idx)
{
   if (idx == 0) {
      RegisterDemand demand = ctx.live_vars.register_demand[block_idx][idx];
      aco_ptr<Instruction>& instr = ctx.program->blocks[block_idx].instructions[idx];
      aco_ptr<Instruction> instr_before(nullptr);
      return get_demand_before(demand, instr, instr_before);
   } else {
      return ctx.live_vars.register_demand[block_idx][idx - 1];
   }
}

RegisterDemand
get_live_in_demand(spill_ctx& ctx, unsigned block_idx)
{
   unsigned idx = 0;
   RegisterDemand reg_pressure = RegisterDemand();
   Block& block = ctx.program->blocks[block_idx];
   for (aco_ptr<Instruction>& phi : block.instructions) {
      if (!is_phi(phi))
         break;
      idx++;

      /* Killed phi definitions increase pressure in the predecessor but not
       * the block they're in. Since the loops below are both to control
       * pressure of the start of this block and the ends of it's
       * predecessors, we need to count killed unspilled phi definitions here. */
      if (phi->definitions[0].isTemp() && phi->definitions[0].isKill() &&
          !ctx.spills_entry[block_idx].count(phi->definitions[0].getTemp()))
         reg_pressure += phi->definitions[0].getTemp();
   }

   reg_pressure += get_demand_before(ctx, block_idx, idx);

   /* In order to create long jumps, we might need an empty SGPR pair. */
   reg_pressure.sgpr += 2;

   return reg_pressure;
}

RegisterDemand
init_live_in_vars(spill_ctx& ctx, Block* block, unsigned block_idx)
{
   RegisterDemand spilled_registers;

   /* first block, nothing was spilled before */
   if (block->linear_preds.empty())
      return {0, 0};

   /* live-in variables at the beginning of the current block */
   const IDSet& live_in = ctx.live_vars.live_out[block_idx];

   /* loop header block */
   if (block->kind & block_kind_loop_header) {
      assert(block->linear_preds[0] == block_idx - 1);
      assert(block->logical_preds[0] == block_idx - 1);

      /* check how many live-through variables should be spilled */
      RegisterDemand reg_pressure = get_live_in_demand(ctx, block_idx);
      RegisterDemand loop_demand = reg_pressure;
      unsigned i = block_idx;
      while (ctx.program->blocks[i].loop_nest_depth >= block->loop_nest_depth)
         loop_demand.update(ctx.program->blocks[i++].register_demand);

      for (auto spilled : ctx.spills_exit[block_idx - 1]) {
         /* variable is not live at loop entry: probably a phi operand */
         if (!live_in.count(spilled.first.id()))
            continue;

         /* keep live-through variables spilled */
         ctx.spills_entry[block_idx][spilled.first] = spilled.second;
         spilled_registers += spilled.first;
         loop_demand -= spilled.first;
      }
      if (!ctx.loop.empty()) {
         /* If this is a nested loop, keep variables from the outer loop spilled. */
         for (auto spilled : ctx.loop.back().spills) {
            /* If the inner loop comes after the last continue statement of the outer loop,
             * the loop-carried variables might not be live-in for the inner loop.
             */
            if (live_in.count(spilled.first.id()) &&
                ctx.spills_entry[block_idx].insert(spilled).second) {
               spilled_registers += spilled.first;
               loop_demand -= spilled.first;
            }
         }
      }

      /* select more live-through variables and constants */
      RegType type = RegType::vgpr;
      while (loop_demand.exceeds(ctx.target_pressure)) {
         /* if VGPR demand is low enough, select SGPRs */
         if (type == RegType::vgpr && loop_demand.vgpr <= ctx.target_pressure.vgpr)
            type = RegType::sgpr;
         /* if SGPR demand is low enough, break */
         if (type == RegType::sgpr && loop_demand.sgpr <= ctx.target_pressure.sgpr)
            break;

         float score = 0.0;
         unsigned remat = 0;
         Temp to_spill;
         for (unsigned t : live_in) {
            Temp var = Temp(t, ctx.program->temp_rc[t]);
            if (var.type() != type || ctx.spills_entry[block_idx].count(var) ||
                !ctx.live_vars.live_out[block_idx - 1].count(t) || var.regClass().is_linear_vgpr())
               continue;

            unsigned can_remat = ctx.remat.count(var);
            if (can_remat > remat || (can_remat == remat && ctx.ssa_infos[t].score() > score)) {
               to_spill = var;
               score = ctx.ssa_infos[t].score();
               remat = can_remat;
            }
         }

         /* select SGPRs or break */
         if (score == 0.0) {
            if (type == RegType::sgpr)
               break;
            type = RegType::sgpr;
            continue;
         }

         ctx.add_to_spills(to_spill, ctx.spills_entry[block_idx]);
         spilled_registers += to_spill;
         loop_demand -= to_spill;
      }

      /* create new loop_info */
      loop_info info = {block_idx, ctx.spills_entry[block_idx], live_in};
      ctx.loop.emplace_back(std::move(info));

      /* shortcut */
      if (!loop_demand.exceeds(ctx.target_pressure))
         return spilled_registers;

      /* if reg pressure is too high at beginning of loop, add variables with furthest use */
      reg_pressure -= spilled_registers;

      while (reg_pressure.exceeds(ctx.target_pressure)) {
         float score = 0;
         Temp to_spill;
         type = reg_pressure.vgpr > ctx.target_pressure.vgpr ? RegType::vgpr : RegType::sgpr;
         for (unsigned t : live_in) {
            Temp var = Temp(t, ctx.program->temp_rc[t]);
            if (var.type() == type && !ctx.spills_entry[block_idx].count(var) &&
                ctx.ssa_infos[t].score() > score) {
               to_spill = var;
               score = ctx.ssa_infos[t].score();
            }
         }
         assert(score != 0.0);
         ctx.add_to_spills(to_spill, ctx.spills_entry[block_idx]);
         spilled_registers += to_spill;
         reg_pressure -= to_spill;
      }

      return spilled_registers;
   }

   /* branch block */
   if (block->linear_preds.size() == 1 && !(block->kind & block_kind_loop_exit)) {
      /* keep variables spilled */
      unsigned pred_idx = block->linear_preds[0];
      for (std::pair<Temp, uint32_t> pair : ctx.spills_exit[pred_idx]) {
         if (pair.first.type() != RegType::sgpr)
            continue;

         if (live_in.count(pair.first.id())) {
            spilled_registers += pair.first;
            ctx.spills_entry[block_idx].emplace(pair);
         }
      }

      if (block->logical_preds.empty())
         return spilled_registers;

      pred_idx = block->logical_preds[0];
      for (std::pair<Temp, uint32_t> pair : ctx.spills_exit[pred_idx]) {
         if (pair.first.type() != RegType::vgpr)
            continue;

         if (live_in.count(pair.first.id())) {
            spilled_registers += pair.first;
            ctx.spills_entry[block_idx].emplace(pair);
         }
      }

      return spilled_registers;
   }

   /* else: merge block */
   std::map<Temp, bool> partial_spills;

   /* keep variables spilled on all incoming paths */
   for (unsigned t : live_in) {
      const RegClass rc = ctx.program->temp_rc[t];
      Temp var = Temp(t, rc);
      Block::edge_vec& preds = rc.is_linear() ? block->linear_preds : block->logical_preds;

      /* If it can be rematerialized, keep the variable spilled if all predecessors do not reload
       * it. Otherwise, if any predecessor reloads it, ensure it's reloaded on all other
       * predecessors. The idea is that it's better in practice to rematerialize redundantly than to
       * create lots of phis. */
      const bool remat = ctx.remat.count(var);
      /* If the variable is spilled at the current loop-header, spilling is essentially for free
       * while reloading is not. Thus, keep them spilled if they are at least partially spilled.
       */
      const bool avoid_respill = block->loop_nest_depth && ctx.loop.back().spills.count(var);
      bool spill = true;
      bool partial_spill = false;
      uint32_t spill_id = 0;
      for (unsigned pred_idx : preds) {
         /* variable is not even live at the predecessor: probably from a phi */
         if (!ctx.live_vars.live_out[pred_idx].count(t)) {
            spill = false;
            break;
         }

         if (!ctx.spills_exit[pred_idx].count(var)) {
            spill = false;
         } else {
            partial_spill = true;
            /* it might be that on one incoming path, the variable has a different spill_id, but
             * add_couple_code() will take care of that. */
            spill_id = ctx.spills_exit[pred_idx][var];
         }
      }
      spill |= (remat && partial_spill);
      spill |= (avoid_respill && partial_spill);
      if (spill) {
         ctx.spills_entry[block_idx][var] = spill_id;
         partial_spills.erase(var);
         spilled_registers += var;
      } else {
         partial_spills[var] = partial_spill;
      }
   }

   /* same for phis */
   for (aco_ptr<Instruction>& phi : block->instructions) {
      if (!is_phi(phi))
         break;
      if (!phi->definitions[0].isTemp())
         continue;

      Block::edge_vec& preds =
         phi->opcode == aco_opcode::p_phi ? block->logical_preds : block->linear_preds;
      bool is_all_undef = true;
      bool is_all_spilled = true;
      bool is_partial_spill = false;
      for (unsigned i = 0; i < phi->operands.size(); i++) {
         if (phi->operands[i].isUndefined())
            continue;
         bool spilled = phi->operands[i].isTemp() &&
                        ctx.spills_exit[preds[i]].count(phi->operands[i].getTemp());
         is_all_spilled &= spilled;
         is_partial_spill |= spilled;
         is_all_undef = false;
      }

      if (is_all_spilled && !is_all_undef) {
         /* The phi is spilled at all predecessors. Keep it spilled. */
         ctx.add_to_spills(phi->definitions[0].getTemp(), ctx.spills_entry[block_idx]);
         spilled_registers += phi->definitions[0].getTemp();
         partial_spills.erase(phi->definitions[0].getTemp());
      } else {
         /* Phis might increase the register pressure. */
         partial_spills[phi->definitions[0].getTemp()] = is_partial_spill;
      }
   }

   /* if reg pressure at first instruction is still too high, add partially spilled variables */
   RegisterDemand reg_pressure = get_live_in_demand(ctx, block_idx);
   reg_pressure -= spilled_registers;

   while (reg_pressure.exceeds(ctx.target_pressure)) {
      assert(!partial_spills.empty());
      std::map<Temp, bool>::iterator it = partial_spills.begin();
      Temp to_spill = Temp();
      bool is_partial_spill = false;
      float score = 0.0;
      RegType type = reg_pressure.vgpr > ctx.target_pressure.vgpr ? RegType::vgpr : RegType::sgpr;

      while (it != partial_spills.end()) {
         assert(!ctx.spills_entry[block_idx].count(it->first));

         if (it->first.type() == type && !it->first.regClass().is_linear_vgpr() &&
             ((it->second && !is_partial_spill) ||
              (it->second == is_partial_spill && ctx.ssa_infos[it->first.id()].score() > score))) {
            score = ctx.ssa_infos[it->first.id()].score();
            to_spill = it->first;
            is_partial_spill = it->second;
         }
         ++it;
      }
      assert(score != 0.0);
      ctx.add_to_spills(to_spill, ctx.spills_entry[block_idx]);
      partial_spills.erase(to_spill);
      spilled_registers += to_spill;
      reg_pressure -= to_spill;
   }

   return spilled_registers;
}

void
add_coupling_code(spill_ctx& ctx, Block* block, IDSet& live_in)
{
   const unsigned block_idx = block->index;
   /* No coupling code necessary */
   if (block->linear_preds.size() == 0)
      return;

   /* Branch block: update renames */
   if (block->linear_preds.size() == 1 &&
       !(block->kind & (block_kind_loop_exit | block_kind_loop_header))) {
      assert(ctx.processed[block->linear_preds[0]]);
      assert(ctx.live_vars.register_demand[block_idx].size() == block->instructions.size());

      ctx.renames[block_idx] = ctx.renames[block->linear_preds[0]];
      if (!block->logical_preds.empty() && block->logical_preds[0] != block->linear_preds[0]) {
         for (auto it : ctx.renames[block->logical_preds[0]]) {
            if (it.first.type() == RegType::vgpr)
               ctx.renames[block_idx].insert_or_assign(it.first, it.second);
         }
      }
      return;
   }

   std::vector<aco_ptr<Instruction>> instructions;

   /* loop header and merge blocks: check if all (linear) predecessors have been processed */
   for (ASSERTED unsigned pred : block->linear_preds)
      assert(ctx.processed[pred]);

   /* iterate the phi nodes for which operands to spill at the predecessor */
   for (aco_ptr<Instruction>& phi : block->instructions) {
      if (!is_phi(phi))
         break;

      for (const Operand& op : phi->operands) {
         if (op.isTemp())
            ctx.ssa_infos[op.tempId()].num_uses--;
      }

      /* if the phi is not spilled, add to instructions */
      if (!phi->definitions[0].isTemp() ||
          !ctx.spills_entry[block_idx].count(phi->definitions[0].getTemp())) {
         instructions.emplace_back(std::move(phi));
         continue;
      }

      Block::edge_vec& preds =
         phi->opcode == aco_opcode::p_phi ? block->logical_preds : block->linear_preds;
      uint32_t def_spill_id = ctx.spills_entry[block_idx][phi->definitions[0].getTemp()];

      for (unsigned i = 0; i < phi->operands.size(); i++) {
         if (phi->operands[i].isUndefined())
            continue;

         unsigned pred_idx = preds[i];
         Operand spill_op = phi->operands[i];

         if (spill_op.isTemp()) {
            assert(phi->operands[i].isKill());
            Temp var = phi->operands[i].getTemp();

            std::map<Temp, Temp>::iterator rename_it = ctx.renames[pred_idx].find(var);
            /* prevent the defining instruction from being DCE'd if it could be rematerialized */
            if (rename_it == ctx.renames[preds[i]].end() && ctx.remat.count(var))
               ctx.unused_remats.erase(ctx.remat[var].instr);

            /* check if variable is already spilled at predecessor */
            auto spilled = ctx.spills_exit[pred_idx].find(var);
            if (spilled != ctx.spills_exit[pred_idx].end()) {
               if (spilled->second != def_spill_id)
                  ctx.add_affinity(def_spill_id, spilled->second);
               continue;
            }

            /* rename if necessary */
            if (rename_it != ctx.renames[pred_idx].end()) {
               spill_op.setTemp(rename_it->second);
               ctx.renames[pred_idx].erase(rename_it);
            }
         }

         /* add interferences */
         for (std::pair<Temp, uint32_t> pair : ctx.spills_exit[pred_idx])
            ctx.add_interference(def_spill_id, pair.second);

         aco_ptr<Instruction> spill{create_instruction(aco_opcode::p_spill, Format::PSEUDO, 2, 0)};
         spill->operands[0] = spill_op;
         spill->operands[1] = Operand::c32(def_spill_id);
         Block& pred = ctx.program->blocks[pred_idx];
         unsigned idx = pred.instructions.size();
         do {
            assert(idx != 0);
            idx--;
         } while (phi->opcode == aco_opcode::p_phi &&
                  pred.instructions[idx]->opcode != aco_opcode::p_logical_end);
         std::vector<aco_ptr<Instruction>>::iterator it = std::next(pred.instructions.begin(), idx);
         pred.instructions.insert(it, std::move(spill));

         /* If the phi operand has the same name as the definition,
          * add to predecessor's spilled variables, so that it gets
          * skipped in the loop below.
          */
         if (spill_op.isTemp() && phi->operands[i].getTemp() == phi->definitions[0].getTemp())
            ctx.spills_exit[pred_idx][phi->operands[i].getTemp()] = def_spill_id;
      }

      /* remove phi from instructions */
      phi.reset();
   }

   /* iterate all (other) spilled variables for which to spill at the predecessor */
   // TODO: would be better to have them sorted: first vgprs and first with longest distance
   for (std::pair<Temp, uint32_t> pair : ctx.spills_entry[block_idx]) {
      Block::edge_vec& preds = pair.first.is_linear() ? block->linear_preds : block->logical_preds;

      for (unsigned pred_idx : preds) {
         /* variable is dead at predecessor, it must be from a phi: this works because of CSSA form */
         if (!ctx.live_vars.live_out[pred_idx].count(pair.first.id()))
            continue;

         /* variable is already spilled at predecessor */
         auto spilled = ctx.spills_exit[pred_idx].find(pair.first);
         if (spilled != ctx.spills_exit[pred_idx].end()) {
            if (spilled->second != pair.second)
               ctx.add_affinity(pair.second, spilled->second);
            continue;
         }

         /* If this variable is spilled through the entire loop, no need to re-spill.
          * It can be reloaded from the same spill-slot it got at the loop-preheader.
          * No need to add interferences since every spilled variable in the loop already
          * interferes with the spilled loop-variables. Make sure that the spill_ids match.
          */
         const uint32_t loop_nest_depth = std::min(ctx.program->blocks[pred_idx].loop_nest_depth,
                                                   ctx.program->blocks[block_idx].loop_nest_depth);
         if (loop_nest_depth) {
            auto spill = ctx.loop[loop_nest_depth - 1].spills.find(pair.first);
            if (spill != ctx.loop[loop_nest_depth - 1].spills.end() && spill->second == pair.second)
               continue;
         }

         /* add interferences between spilled variable and predecessors exit spills */
         for (std::pair<Temp, uint32_t> exit_spill : ctx.spills_exit[pred_idx])
            ctx.add_interference(exit_spill.second, pair.second);

         /* variable is in register at predecessor and has to be spilled */
         /* rename if necessary */
         Temp var = pair.first;
         std::map<Temp, Temp>::iterator rename_it = ctx.renames[pred_idx].find(var);
         if (rename_it != ctx.renames[pred_idx].end()) {
            var = rename_it->second;
            ctx.renames[pred_idx].erase(rename_it);
         }

         aco_ptr<Instruction> spill{create_instruction(aco_opcode::p_spill, Format::PSEUDO, 2, 0)};
         spill->operands[0] = Operand(var);
         spill->operands[1] = Operand::c32(pair.second);
         Block& pred = ctx.program->blocks[pred_idx];
         unsigned idx = pred.instructions.size();
         do {
            assert(idx != 0);
            idx--;
         } while (pair.first.type() == RegType::vgpr &&
                  pred.instructions[idx]->opcode != aco_opcode::p_logical_end);
         std::vector<aco_ptr<Instruction>>::iterator it = std::next(pred.instructions.begin(), idx);
         pred.instructions.insert(it, std::move(spill));
      }
   }

   /* iterate phis for which operands to reload */
   for (aco_ptr<Instruction>& phi : instructions) {
      assert(phi->opcode == aco_opcode::p_phi || phi->opcode == aco_opcode::p_linear_phi);
      assert(!phi->definitions[0].isTemp() ||
             !ctx.spills_entry[block_idx].count(phi->definitions[0].getTemp()));

      Block::edge_vec& preds =
         phi->opcode == aco_opcode::p_phi ? block->logical_preds : block->linear_preds;
      for (unsigned i = 0; i < phi->operands.size(); i++) {
         if (!phi->operands[i].isTemp())
            continue;
         unsigned pred_idx = preds[i];

         /* if the operand was reloaded, rename */
         if (!ctx.spills_exit[pred_idx].count(phi->operands[i].getTemp())) {
            std::map<Temp, Temp>::iterator it =
               ctx.renames[pred_idx].find(phi->operands[i].getTemp());
            if (it != ctx.renames[pred_idx].end()) {
               phi->operands[i].setTemp(it->second);
               /* prevent the defining instruction from being DCE'd if it could be rematerialized */
            } else {
               auto remat_it = ctx.remat.find(phi->operands[i].getTemp());
               if (remat_it != ctx.remat.end()) {
                  ctx.unused_remats.erase(remat_it->second.instr);
               }
            }
            continue;
         }

         Temp tmp = phi->operands[i].getTemp();

         /* reload phi operand at end of predecessor block */
         Temp new_name = ctx.program->allocateTmp(tmp.regClass());
         Block& pred = ctx.program->blocks[pred_idx];
         unsigned idx = pred.instructions.size();
         do {
            assert(idx != 0);
            idx--;
         } while (phi->opcode == aco_opcode::p_phi &&
                  pred.instructions[idx]->opcode != aco_opcode::p_logical_end);
         std::vector<aco_ptr<Instruction>>::iterator it = std::next(pred.instructions.begin(), idx);
         aco_ptr<Instruction> reload =
            do_reload(ctx, tmp, new_name, ctx.spills_exit[pred_idx][tmp]);

         /* reload spilled exec mask directly to exec */
         if (!phi->definitions[0].isTemp()) {
            assert(phi->definitions[0].isFixed() && phi->definitions[0].physReg() == exec);
            reload->definitions[0] = phi->definitions[0];
            phi->operands[i] = Operand(exec, ctx.program->lane_mask);
         } else {
            ctx.spills_exit[pred_idx].erase(tmp);
            ctx.renames[pred_idx][tmp] = new_name;
            phi->operands[i].setTemp(new_name);
         }

         pred.instructions.insert(it, std::move(reload));
      }
   }

   /* iterate live variables for which to reload */
   for (unsigned t : live_in) {
      const RegClass rc = ctx.program->temp_rc[t];
      Temp var = Temp(t, rc);

      /* skip spilled variables */
      if (ctx.spills_entry[block_idx].count(var))
         continue;

      Block::edge_vec& preds = rc.is_linear() ? block->linear_preds : block->logical_preds;
      /* if a variable is dead at any predecessor, it must be from a phi */
      const bool is_dead =
         std::any_of(preds.begin(), preds.end(),
                     [&](unsigned pred) { return !ctx.live_vars.live_out[pred].count(var.id()); });
      if (is_dead)
         continue;

      for (unsigned pred_idx : preds) {
         /* skip if the variable is not spilled at the predecessor */
         if (!ctx.spills_exit[pred_idx].count(var))
            continue;

         /* variable is spilled at predecessor and has to be reloaded */
         Temp new_name = ctx.program->allocateTmp(rc);
         Block& pred = ctx.program->blocks[pred_idx];
         unsigned idx = pred.instructions.size();
         do {
            assert(idx != 0);
            idx--;
         } while (rc.type() == RegType::vgpr &&
                  pred.instructions[idx]->opcode != aco_opcode::p_logical_end);
         std::vector<aco_ptr<Instruction>>::iterator it = std::next(pred.instructions.begin(), idx);

         aco_ptr<Instruction> reload =
            do_reload(ctx, var, new_name, ctx.spills_exit[pred.index][var]);
         pred.instructions.insert(it, std::move(reload));

         ctx.spills_exit[pred.index].erase(var);
         ctx.renames[pred.index][var] = new_name;
      }

      /* check if we have to create a new phi for this variable */
      Temp rename = Temp();
      bool is_same = true;
      for (unsigned pred_idx : preds) {
         if (!ctx.renames[pred_idx].count(var)) {
            if (rename == Temp())
               rename = var;
            else
               is_same = rename == var;
         } else {
            if (rename == Temp())
               rename = ctx.renames[pred_idx][var];
            else
               is_same = rename == ctx.renames[pred_idx][var];
         }

         if (!is_same)
            break;
      }

      if (!is_same) {
         /* the variable was renamed differently in the predecessors: we have to create a phi */
         aco_opcode opcode = rc.is_linear() ? aco_opcode::p_linear_phi : aco_opcode::p_phi;
         aco_ptr<Instruction> phi{create_instruction(opcode, Format::PSEUDO, preds.size(), 1)};
         rename = ctx.program->allocateTmp(rc);
         for (unsigned i = 0; i < phi->operands.size(); i++) {
            Temp tmp;
            if (ctx.renames[preds[i]].count(var)) {
               tmp = ctx.renames[preds[i]][var];
            } else if (preds[i] >= block_idx) {
               tmp = rename;
            } else {
               tmp = var;
               /* prevent the defining instruction from being DCE'd if it could be rematerialized */
               if (ctx.remat.count(tmp))
                  ctx.unused_remats.erase(ctx.remat[tmp].instr);
            }
            phi->operands[i] = Operand(tmp);
         }
         phi->definitions[0] = Definition(rename);
         instructions.emplace_back(std::move(phi));
      }

      /* the variable was renamed: add new name to renames */
      if (!(rename == Temp() || rename == var))
         ctx.renames[block_idx][var] = rename;
   }

   /* combine phis with instructions */
   unsigned idx = 0;
   while (!block->instructions[idx]) {
      idx++;
   }

   if (!ctx.processed[block_idx]) {
      assert(!(block->kind & block_kind_loop_header));
      RegisterDemand demand_before = get_demand_before(ctx, block_idx, idx);
      ctx.live_vars.register_demand[block->index].erase(
         ctx.live_vars.register_demand[block->index].begin(),
         ctx.live_vars.register_demand[block->index].begin() + idx);
      ctx.live_vars.register_demand[block->index].insert(
         ctx.live_vars.register_demand[block->index].begin(), instructions.size(), demand_before);
   }

   std::vector<aco_ptr<Instruction>>::iterator start = std::next(block->instructions.begin(), idx);
   instructions.insert(
      instructions.end(), std::move_iterator<std::vector<aco_ptr<Instruction>>::iterator>(start),
      std::move_iterator<std::vector<aco_ptr<Instruction>>::iterator>(block->instructions.end()));
   block->instructions = std::move(instructions);
}

void
process_block(spill_ctx& ctx, unsigned block_idx, Block* block, RegisterDemand spilled_registers)
{
   assert(!ctx.processed[block_idx]);

   std::vector<aco_ptr<Instruction>> instructions;
   unsigned idx = 0;

   /* phis are handled separately */
   while (block->instructions[idx]->opcode == aco_opcode::p_phi ||
          block->instructions[idx]->opcode == aco_opcode::p_linear_phi) {
      instructions.emplace_back(std::move(block->instructions[idx++]));
   }

   auto& current_spills = ctx.spills_exit[block_idx];

   while (idx < block->instructions.size()) {
      aco_ptr<Instruction>& instr = block->instructions[idx];

      /* Spilling is handled as part of phis (they should always have the same or higher register
       * demand). If we try to spill here, we might not be able to reduce the register demand enough
       * because there is no path to spill constant/undef phi operands. */
      if (instr->opcode == aco_opcode::p_branch) {
         instructions.emplace_back(std::move(instr));
         idx++;
         continue;
      }

      std::map<Temp, std::pair<Temp, uint32_t>> reloads;

      /* rename and reload operands */
      for (Operand& op : instr->operands) {
         if (!op.isTemp())
            continue;

         if (op.isFirstKill())
            ctx.live_vars.live_out[block_idx].erase(op.tempId());
         ctx.ssa_infos[op.tempId()].num_uses--;

         if (!current_spills.count(op.getTemp()))
            continue;

         /* the Operand is spilled: add it to reloads */
         Temp new_tmp = ctx.program->allocateTmp(op.regClass());
         ctx.renames[block_idx][op.getTemp()] = new_tmp;
         reloads[new_tmp] = std::make_pair(op.getTemp(), current_spills[op.getTemp()]);
         current_spills.erase(op.getTemp());
         spilled_registers -= new_tmp;
      }

      /* check if register demand is low enough before and after the current instruction */
      if (block->register_demand.exceeds(ctx.target_pressure)) {

         RegisterDemand new_demand = ctx.live_vars.register_demand[block_idx][idx];
         new_demand.update(get_demand_before(ctx, block_idx, idx));

         /* if reg pressure is too high, spill variable with furthest next use */
         while ((new_demand - spilled_registers).exceeds(ctx.target_pressure)) {
            float score = 0.0;
            Temp to_spill;
            unsigned do_rematerialize = 0;
            unsigned avoid_respill = 0;
            RegType type = RegType::sgpr;
            if (new_demand.vgpr - spilled_registers.vgpr > ctx.target_pressure.vgpr)
               type = RegType::vgpr;

            for (unsigned t : ctx.live_vars.live_out[block_idx]) {
               RegClass rc = ctx.program->temp_rc[t];
               Temp var = Temp(t, rc);
               if (rc.type() != type || current_spills.count(var) || rc.is_linear_vgpr())
                  continue;

               unsigned can_rematerialize = ctx.remat.count(var);
               unsigned loop_variable = block->loop_nest_depth && ctx.loop.back().spills.count(var);
               if (avoid_respill > loop_variable || do_rematerialize > can_rematerialize)
                  continue;

               if (can_rematerialize > do_rematerialize || loop_variable > avoid_respill ||
                   ctx.ssa_infos[t].score() > score) {
                  /* Don't spill operands */
                  if (std::any_of(instr->operands.begin(), instr->operands.end(),
                                  [&](Operand& op) { return op.isTemp() && op.getTemp() == var; }))
                     continue;

                  to_spill = var;
                  score = ctx.ssa_infos[t].score();
                  do_rematerialize = can_rematerialize;
                  avoid_respill = loop_variable;
               }
            }
            assert(score != 0.0);

            if (avoid_respill) {
               /* This variable is spilled at the loop-header of the current loop.
                * Re-use the spill-slot in order to avoid an extra store.
                */
               current_spills[to_spill] = ctx.loop.back().spills[to_spill];
               spilled_registers += to_spill;
               continue;
            }

            uint32_t spill_id = ctx.add_to_spills(to_spill, current_spills);
            /* add interferences with reloads */
            for (std::pair<const Temp, std::pair<Temp, uint32_t>>& pair : reloads)
               ctx.add_interference(spill_id, pair.second.second);

            spilled_registers += to_spill;

            /* rename if necessary */
            if (ctx.renames[block_idx].count(to_spill)) {
               to_spill = ctx.renames[block_idx][to_spill];
            }

            /* add spill to new instructions */
            aco_ptr<Instruction> spill{
               create_instruction(aco_opcode::p_spill, Format::PSEUDO, 2, 0)};
            spill->operands[0] = Operand(to_spill);
            spill->operands[1] = Operand::c32(spill_id);
            instructions.emplace_back(std::move(spill));
         }
      }

      for (const Definition& def : instr->definitions) {
         if (def.isTemp() && !def.isKill())
            ctx.live_vars.live_out[block_idx].insert(def.tempId());
      }
      /* rename operands */
      for (Operand& op : instr->operands) {
         if (op.isTemp()) {
            auto rename_it = ctx.renames[block_idx].find(op.getTemp());
            if (rename_it != ctx.renames[block_idx].end()) {
               op.setTemp(rename_it->second);
            } else {
               /* prevent its defining instruction from being DCE'd if it could be rematerialized */
               auto remat_it = ctx.remat.find(op.getTemp());
               if (remat_it != ctx.remat.end()) {
                  ctx.unused_remats.erase(remat_it->second.instr);
               }
            }
         }
      }

      /* add reloads and instruction to new instructions */
      for (std::pair<const Temp, std::pair<Temp, uint32_t>>& pair : reloads) {
         aco_ptr<Instruction> reload =
            do_reload(ctx, pair.second.first, pair.first, pair.second.second);
         instructions.emplace_back(std::move(reload));
      }
      instructions.emplace_back(std::move(instr));
      idx++;
   }

   block->instructions = std::move(instructions);
}

void
spill_block(spill_ctx& ctx, unsigned block_idx)
{
   Block* block = &ctx.program->blocks[block_idx];

   /* determine set of variables which are spilled at the beginning of the block */
   RegisterDemand spilled_registers = init_live_in_vars(ctx, block, block_idx);

   if (!(block->kind & block_kind_loop_header)) {
      /* add spill/reload code on incoming control flow edges */
      add_coupling_code(ctx, block, ctx.live_vars.live_out[block_idx]);
   }

   assert(ctx.spills_exit[block_idx].empty());
   ctx.spills_exit[block_idx] = ctx.spills_entry[block_idx];
   process_block(ctx, block_idx, block, spilled_registers);

   ctx.processed[block_idx] = true;

   /* check if the next block leaves the current loop */
   if (block->loop_nest_depth == 0 ||
       ctx.program->blocks[block_idx + 1].loop_nest_depth >= block->loop_nest_depth)
      return;

   uint32_t loop_header_idx = ctx.loop.back().index;

   /* preserve original renames at end of loop header block */
   aco::map<Temp, Temp> renames = std::move(ctx.renames[loop_header_idx]);

   /* add coupling code to all loop header predecessors */
   for (unsigned t : ctx.loop.back().live_in)
      ctx.ssa_infos[t].num_uses--;
   add_coupling_code(ctx, &ctx.program->blocks[loop_header_idx], ctx.loop.back().live_in);
   renames.swap(ctx.renames[loop_header_idx]);

   /* remove loop header info from stack */
   ctx.loop.pop_back();
   if (renames.empty())
      return;

   /* Add the new renames to each block */
   for (std::pair<Temp, Temp> rename : renames) {
      /* If there is already a rename, don't overwrite it. */
      for (unsigned idx = loop_header_idx; idx <= block_idx; idx++)
         ctx.renames[idx].insert(rename);
   }

   /* propagate new renames through loop: i.e. repair the SSA */
   for (unsigned idx = loop_header_idx; idx <= block_idx; idx++) {
      Block& current = ctx.program->blocks[idx];
      /* rename all uses in this block */
      for (aco_ptr<Instruction>& instr : current.instructions) {
         /* no need to rename the loop header phis once again. */
         if (idx == loop_header_idx && is_phi(instr))
            continue;

         for (Operand& op : instr->operands) {
            if (!op.isTemp())
               continue;

            auto rename = renames.find(op.getTemp());
            if (rename != renames.end())
               op.setTemp(rename->second);
         }
      }
   }
}

Temp
load_scratch_resource(spill_ctx& ctx, Builder& bld, bool apply_scratch_offset)
{
   Temp private_segment_buffer = ctx.program->private_segment_buffer;
   if (!private_segment_buffer.bytes()) {
      Temp addr_lo =
         bld.sop1(aco_opcode::p_load_symbol, bld.def(s1), Operand::c32(aco_symbol_scratch_addr_lo));
      Temp addr_hi =
         bld.sop1(aco_opcode::p_load_symbol, bld.def(s1), Operand::c32(aco_symbol_scratch_addr_hi));
      private_segment_buffer =
         bld.pseudo(aco_opcode::p_create_vector, bld.def(s2), addr_lo, addr_hi);
   } else if (ctx.program->stage.hw != AC_HW_COMPUTE_SHADER) {
      private_segment_buffer =
         bld.smem(aco_opcode::s_load_dwordx2, bld.def(s2), private_segment_buffer, Operand::zero());
   }

   if (apply_scratch_offset) {
      Temp addr_lo = bld.tmp(s1);
      Temp addr_hi = bld.tmp(s1);
      bld.pseudo(aco_opcode::p_split_vector, Definition(addr_lo), Definition(addr_hi),
                 private_segment_buffer);

      Temp carry = bld.tmp(s1);
      addr_lo = bld.sop2(aco_opcode::s_add_u32, bld.def(s1), bld.scc(Definition(carry)), addr_lo,
                         ctx.program->scratch_offset);
      addr_hi = bld.sop2(aco_opcode::s_addc_u32, bld.def(s1), bld.def(s1, scc), addr_hi,
                         Operand::c32(0), bld.scc(carry));

      private_segment_buffer =
         bld.pseudo(aco_opcode::p_create_vector, bld.def(s2), addr_lo, addr_hi);
   }

   uint32_t rsrc_conf =
      S_008F0C_ADD_TID_ENABLE(1) | S_008F0C_INDEX_STRIDE(ctx.program->wave_size == 64 ? 3 : 2);

   if (ctx.program->gfx_level >= GFX10) {
      rsrc_conf |= S_008F0C_FORMAT(V_008F0C_GFX10_FORMAT_32_FLOAT) |
                   S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_RAW) |
                   S_008F0C_RESOURCE_LEVEL(ctx.program->gfx_level < GFX11);
   } else if (ctx.program->gfx_level <= GFX7) {
      /* dfmt modifies stride on GFX8/GFX9 when ADD_TID_EN=1 */
      rsrc_conf |= S_008F0C_NUM_FORMAT(V_008F0C_BUF_NUM_FORMAT_FLOAT) |
                   S_008F0C_DATA_FORMAT(V_008F0C_BUF_DATA_FORMAT_32);
   }
   /* older generations need element size = 4 bytes. element size removed in GFX9 */
   if (ctx.program->gfx_level <= GFX8)
      rsrc_conf |= S_008F0C_ELEMENT_SIZE(1);

   return bld.pseudo(aco_opcode::p_create_vector, bld.def(s4), private_segment_buffer,
                     Operand::c32(-1u), Operand::c32(rsrc_conf));
}

void
setup_vgpr_spill_reload(spill_ctx& ctx, Block& block,
                        std::vector<aco_ptr<Instruction>>& instructions, uint32_t spill_slot,
                        Temp& scratch_offset, unsigned* offset)
{
   uint32_t scratch_size = ctx.program->config->scratch_bytes_per_wave / ctx.program->wave_size;

   uint32_t offset_range;
   if (ctx.program->gfx_level >= GFX9) {
      offset_range =
         ctx.program->dev.scratch_global_offset_max - ctx.program->dev.scratch_global_offset_min;
   } else {
      if (scratch_size < 4095)
         offset_range = 4095 - scratch_size;
      else
         offset_range = 0;
   }

   bool overflow = (ctx.vgpr_spill_slots - 1) * 4 > offset_range;

   Builder rsrc_bld(ctx.program);
   if (block.kind & block_kind_top_level) {
      rsrc_bld.reset(&instructions);
   } else if (ctx.scratch_rsrc == Temp() && (!overflow || ctx.program->gfx_level < GFX9)) {
      Block* tl_block = &block;
      while (!(tl_block->kind & block_kind_top_level))
         tl_block = &ctx.program->blocks[tl_block->linear_idom];

      /* find p_logical_end */
      std::vector<aco_ptr<Instruction>>& prev_instructions = tl_block->instructions;
      unsigned idx = prev_instructions.size() - 1;
      while (prev_instructions[idx]->opcode != aco_opcode::p_logical_end)
         idx--;
      rsrc_bld.reset(&prev_instructions, std::next(prev_instructions.begin(), idx));
   }

   /* If spilling overflows the constant offset range at any point, we need to emit the soffset
    * before every spill/reload to avoid increasing register demand.
    */
   Builder offset_bld = rsrc_bld;
   if (overflow)
      offset_bld.reset(&instructions);

   *offset = spill_slot * 4;
   if (ctx.program->gfx_level >= GFX9) {
      *offset += ctx.program->dev.scratch_global_offset_min;

      if (ctx.scratch_rsrc == Temp() || overflow) {
         int32_t saddr = scratch_size - ctx.program->dev.scratch_global_offset_min;
         if ((int32_t)*offset > (int32_t)ctx.program->dev.scratch_global_offset_max) {
            saddr += (int32_t)*offset;
            *offset = 0;
         }

         /* GFX9+ uses scratch_* instructions, which don't use a resource. */
         ctx.scratch_rsrc = offset_bld.copy(offset_bld.def(s1), Operand::c32(saddr));
      }
   } else {
      if (ctx.scratch_rsrc == Temp())
         ctx.scratch_rsrc = load_scratch_resource(ctx, rsrc_bld, overflow);

      if (overflow) {
         uint32_t soffset =
            ctx.program->config->scratch_bytes_per_wave + *offset * ctx.program->wave_size;
         *offset = 0;

         scratch_offset = offset_bld.copy(offset_bld.def(s1), Operand::c32(soffset));
      } else {
         *offset += scratch_size;
      }
   }
}

void
spill_vgpr(spill_ctx& ctx, Block& block, std::vector<aco_ptr<Instruction>>& instructions,
           aco_ptr<Instruction>& spill, std::vector<uint32_t>& slots)
{
   ctx.program->config->spilled_vgprs += spill->operands[0].size();

   uint32_t spill_id = spill->operands[1].constantValue();
   uint32_t spill_slot = slots[spill_id];

   Temp scratch_offset = ctx.program->scratch_offset;
   unsigned offset;
   setup_vgpr_spill_reload(ctx, block, instructions, spill_slot, scratch_offset, &offset);

   assert(spill->operands[0].isTemp());
   Temp temp = spill->operands[0].getTemp();
   assert(temp.type() == RegType::vgpr && !temp.is_linear());

   Builder bld(ctx.program, &instructions);
   if (temp.size() > 1) {
      Instruction* split{
         create_instruction(aco_opcode::p_split_vector, Format::PSEUDO, 1, temp.size())};
      split->operands[0] = Operand(temp);
      for (unsigned i = 0; i < temp.size(); i++)
         split->definitions[i] = bld.def(v1);
      bld.insert(split);
      for (unsigned i = 0; i < temp.size(); i++, offset += 4) {
         Temp elem = split->definitions[i].getTemp();
         if (ctx.program->gfx_level >= GFX9) {
            bld.scratch(aco_opcode::scratch_store_dword, Operand(v1), ctx.scratch_rsrc, elem,
                        offset, memory_sync_info(storage_vgpr_spill, semantic_private));
         } else {
            Instruction* instr = bld.mubuf(aco_opcode::buffer_store_dword, ctx.scratch_rsrc,
                                           Operand(v1), scratch_offset, elem, offset, false, true);
            instr->mubuf().sync = memory_sync_info(storage_vgpr_spill, semantic_private);
         }
      }
   } else if (ctx.program->gfx_level >= GFX9) {
      bld.scratch(aco_opcode::scratch_store_dword, Operand(v1), ctx.scratch_rsrc, temp, offset,
                  memory_sync_info(storage_vgpr_spill, semantic_private));
   } else {
      Instruction* instr = bld.mubuf(aco_opcode::buffer_store_dword, ctx.scratch_rsrc, Operand(v1),
                                     scratch_offset, temp, offset, false, true);
      instr->mubuf().sync = memory_sync_info(storage_vgpr_spill, semantic_private);
   }
}

void
reload_vgpr(spill_ctx& ctx, Block& block, std::vector<aco_ptr<Instruction>>& instructions,
            aco_ptr<Instruction>& reload, std::vector<uint32_t>& slots)
{
   uint32_t spill_id = reload->operands[0].constantValue();
   uint32_t spill_slot = slots[spill_id];

   Temp scratch_offset = ctx.program->scratch_offset;
   unsigned offset;
   setup_vgpr_spill_reload(ctx, block, instructions, spill_slot, scratch_offset, &offset);

   Definition def = reload->definitions[0];

   Builder bld(ctx.program, &instructions);
   if (def.size() > 1) {
      Instruction* vec{
         create_instruction(aco_opcode::p_create_vector, Format::PSEUDO, def.size(), 1)};
      vec->definitions[0] = def;
      for (unsigned i = 0; i < def.size(); i++, offset += 4) {
         Temp tmp = bld.tmp(v1);
         vec->operands[i] = Operand(tmp);
         if (ctx.program->gfx_level >= GFX9) {
            bld.scratch(aco_opcode::scratch_load_dword, Definition(tmp), Operand(v1),
                        ctx.scratch_rsrc, offset,
                        memory_sync_info(storage_vgpr_spill, semantic_private));
         } else {
            Instruction* instr =
               bld.mubuf(aco_opcode::buffer_load_dword, Definition(tmp), ctx.scratch_rsrc,
                         Operand(v1), scratch_offset, offset, false, true);
            instr->mubuf().sync = memory_sync_info(storage_vgpr_spill, semantic_private);
         }
      }
      bld.insert(vec);
   } else if (ctx.program->gfx_level >= GFX9) {
      bld.scratch(aco_opcode::scratch_load_dword, def, Operand(v1), ctx.scratch_rsrc, offset,
                  memory_sync_info(storage_vgpr_spill, semantic_private));
   } else {
      Instruction* instr = bld.mubuf(aco_opcode::buffer_load_dword, def, ctx.scratch_rsrc,
                                     Operand(v1), scratch_offset, offset, false, true);
      instr->mubuf().sync = memory_sync_info(storage_vgpr_spill, semantic_private);
   }
}

void
add_interferences(spill_ctx& ctx, std::vector<bool>& is_assigned, std::vector<uint32_t>& slots,
                  std::vector<bool>& slots_used, unsigned id)
{
   for (unsigned other : ctx.interferences[id].second) {
      if (!is_assigned[other])
         continue;

      RegClass other_rc = ctx.interferences[other].first;
      unsigned slot = slots[other];
      std::fill(slots_used.begin() + slot, slots_used.begin() + slot + other_rc.size(), true);
   }
}

unsigned
find_available_slot(std::vector<bool>& used, unsigned wave_size, unsigned size, bool is_sgpr)
{
   unsigned wave_size_minus_one = wave_size - 1;
   unsigned slot = 0;

   while (true) {
      bool available = true;
      for (unsigned i = 0; i < size; i++) {
         if (slot + i < used.size() && used[slot + i]) {
            available = false;
            break;
         }
      }
      if (!available) {
         slot++;
         continue;
      }

      if (is_sgpr && ((slot & wave_size_minus_one) > wave_size - size)) {
         slot = align(slot, wave_size);
         continue;
      }

      std::fill(used.begin(), used.end(), false);

      if (slot + size > used.size())
         used.resize(slot + size);

      return slot;
   }
}

void
assign_spill_slots_helper(spill_ctx& ctx, RegType type, std::vector<bool>& is_assigned,
                          std::vector<uint32_t>& slots, unsigned* num_slots)
{
   std::vector<bool> slots_used;

   /* assign slots for ids with affinities first */
   for (std::vector<uint32_t>& vec : ctx.affinities) {
      if (ctx.interferences[vec[0]].first.type() != type)
         continue;

      for (unsigned id : vec) {
         if (!ctx.is_reloaded[id])
            continue;

         add_interferences(ctx, is_assigned, slots, slots_used, id);
      }

      unsigned slot = find_available_slot(
         slots_used, ctx.wave_size, ctx.interferences[vec[0]].first.size(), type == RegType::sgpr);

      for (unsigned id : vec) {
         assert(!is_assigned[id]);

         if (ctx.is_reloaded[id]) {
            slots[id] = slot;
            is_assigned[id] = true;
         }
      }
   }

   /* assign slots for ids without affinities */
   for (unsigned id = 0; id < ctx.interferences.size(); id++) {
      if (is_assigned[id] || !ctx.is_reloaded[id] || ctx.interferences[id].first.type() != type)
         continue;

      add_interferences(ctx, is_assigned, slots, slots_used, id);

      unsigned slot = find_available_slot(
         slots_used, ctx.wave_size, ctx.interferences[id].first.size(), type == RegType::sgpr);

      slots[id] = slot;
      is_assigned[id] = true;
   }

   *num_slots = slots_used.size();
}

void
end_unused_spill_vgprs(spill_ctx& ctx, Block& block, std::vector<Temp>& vgpr_spill_temps,
                       const std::vector<uint32_t>& slots,
                       const aco::unordered_map<Temp, uint32_t>& spills)
{
   std::vector<bool> is_used(vgpr_spill_temps.size());
   for (std::pair<Temp, uint32_t> pair : spills) {
      if (pair.first.type() == RegType::sgpr && ctx.is_reloaded[pair.second])
         is_used[slots[pair.second] / ctx.wave_size] = true;
   }

   std::vector<Temp> temps;
   for (unsigned i = 0; i < vgpr_spill_temps.size(); i++) {
      if (vgpr_spill_temps[i].id() && !is_used[i]) {
         temps.push_back(vgpr_spill_temps[i]);
         vgpr_spill_temps[i] = Temp();
      }
   }
   if (temps.empty() || block.linear_preds.empty())
      return;

   aco_ptr<Instruction> destr{
      create_instruction(aco_opcode::p_end_linear_vgpr, Format::PSEUDO, temps.size(), 0)};
   for (unsigned i = 0; i < temps.size(); i++) {
      destr->operands[i] = Operand(temps[i]);
      destr->operands[i].setLateKill(true);
   }

   std::vector<aco_ptr<Instruction>>::iterator it = block.instructions.begin();
   while (is_phi(*it))
      ++it;
   block.instructions.insert(it, std::move(destr));
}

void
assign_spill_slots(spill_ctx& ctx, unsigned spills_to_vgpr)
{
   std::vector<uint32_t> slots(ctx.interferences.size());
   std::vector<bool> is_assigned(ctx.interferences.size());

   /* first, handle affinities: just merge all interferences into both spill ids */
   for (std::vector<uint32_t>& vec : ctx.affinities) {
      for (unsigned i = 0; i < vec.size(); i++) {
         for (unsigned j = i + 1; j < vec.size(); j++) {
            assert(vec[i] != vec[j]);
            bool reloaded = ctx.is_reloaded[vec[i]] || ctx.is_reloaded[vec[j]];
            ctx.is_reloaded[vec[i]] = reloaded;
            ctx.is_reloaded[vec[j]] = reloaded;
         }
      }
   }
   for (ASSERTED uint32_t i = 0; i < ctx.interferences.size(); i++)
      for (ASSERTED uint32_t id : ctx.interferences[i].second)
         assert(i != id);

   /* for each spill slot, assign as many spill ids as possible */
   assign_spill_slots_helper(ctx, RegType::sgpr, is_assigned, slots, &ctx.sgpr_spill_slots);
   assign_spill_slots_helper(ctx, RegType::vgpr, is_assigned, slots, &ctx.vgpr_spill_slots);

   for (unsigned id = 0; id < is_assigned.size(); id++)
      assert(is_assigned[id] || !ctx.is_reloaded[id]);

   for (std::vector<uint32_t>& vec : ctx.affinities) {
      for (unsigned i = 0; i < vec.size(); i++) {
         for (unsigned j = i + 1; j < vec.size(); j++) {
            assert(is_assigned[vec[i]] == is_assigned[vec[j]]);
            if (!is_assigned[vec[i]])
               continue;
            assert(ctx.is_reloaded[vec[i]] == ctx.is_reloaded[vec[j]]);
            assert(ctx.interferences[vec[i]].first.type() ==
                   ctx.interferences[vec[j]].first.type());
            assert(slots[vec[i]] == slots[vec[j]]);
         }
      }
   }

   /* hope, we didn't mess up */
   std::vector<Temp> vgpr_spill_temps((ctx.sgpr_spill_slots + ctx.wave_size - 1) / ctx.wave_size);
   assert(vgpr_spill_temps.size() <= spills_to_vgpr);

   /* replace pseudo instructions with actual hardware instructions */
   unsigned last_top_level_block_idx = 0;
   for (Block& block : ctx.program->blocks) {

      if (block.kind & block_kind_top_level) {
         last_top_level_block_idx = block.index;

         end_unused_spill_vgprs(ctx, block, vgpr_spill_temps, slots, ctx.spills_entry[block.index]);

         /* If the block has no predecessors (for example in RT resume shaders),
          * we cannot reuse the current scratch_rsrc temp because its definition is unreachable */
         if (block.linear_preds.empty())
            ctx.scratch_rsrc = Temp();
      }

      std::vector<aco_ptr<Instruction>>::iterator it;
      std::vector<aco_ptr<Instruction>> instructions;
      instructions.reserve(block.instructions.size());
      Builder bld(ctx.program, &instructions);
      for (it = block.instructions.begin(); it != block.instructions.end(); ++it) {

         if ((*it)->opcode == aco_opcode::p_spill) {
            uint32_t spill_id = (*it)->operands[1].constantValue();

            if (!ctx.is_reloaded[spill_id]) {
               /* never reloaded, so don't spill */
            } else if (!is_assigned[spill_id]) {
               unreachable("No spill slot assigned for spill id");
            } else if (ctx.interferences[spill_id].first.type() == RegType::vgpr) {
               spill_vgpr(ctx, block, instructions, *it, slots);
            } else {
               ctx.program->config->spilled_sgprs += (*it)->operands[0].size();

               uint32_t spill_slot = slots[spill_id];

               /* check if the linear vgpr already exists */
               if (vgpr_spill_temps[spill_slot / ctx.wave_size] == Temp()) {
                  Temp linear_vgpr = ctx.program->allocateTmp(v1.as_linear());
                  vgpr_spill_temps[spill_slot / ctx.wave_size] = linear_vgpr;
                  aco_ptr<Instruction> create{
                     create_instruction(aco_opcode::p_start_linear_vgpr, Format::PSEUDO, 0, 1)};
                  create->definitions[0] = Definition(linear_vgpr);
                  /* find the right place to insert this definition */
                  if (last_top_level_block_idx == block.index) {
                     /* insert right before the current instruction */
                     instructions.emplace_back(std::move(create));
                  } else {
                     assert(last_top_level_block_idx < block.index);
                     /* insert after p_logical_end of the last top-level block */
                     std::vector<aco_ptr<Instruction>>& block_instrs =
                        ctx.program->blocks[last_top_level_block_idx].instructions;
                     auto insert_point =
                        std::find_if(block_instrs.rbegin(), block_instrs.rend(),
                                     [](const auto& iter) {
                                        return iter->opcode == aco_opcode::p_logical_end;
                                     })
                           .base();
                     block_instrs.insert(insert_point, std::move(create));
                  }
               }

               /* spill sgpr: just add the vgpr temp to operands */
               Instruction* spill = create_instruction(aco_opcode::p_spill, Format::PSEUDO, 3, 0);
               spill->operands[0] = Operand(vgpr_spill_temps[spill_slot / ctx.wave_size]);
               spill->operands[0].setLateKill(true);
               spill->operands[1] = Operand::c32(spill_slot % ctx.wave_size);
               spill->operands[2] = (*it)->operands[0];
               instructions.emplace_back(aco_ptr<Instruction>(spill));
            }

         } else if ((*it)->opcode == aco_opcode::p_reload) {
            uint32_t spill_id = (*it)->operands[0].constantValue();
            assert(ctx.is_reloaded[spill_id]);

            if (!is_assigned[spill_id]) {
               unreachable("No spill slot assigned for spill id");
            } else if (ctx.interferences[spill_id].first.type() == RegType::vgpr) {
               reload_vgpr(ctx, block, instructions, *it, slots);
            } else {
               uint32_t spill_slot = slots[spill_id];

               /* check if the linear vgpr already exists */
               if (vgpr_spill_temps[spill_slot / ctx.wave_size] == Temp()) {
                  Temp linear_vgpr = ctx.program->allocateTmp(v1.as_linear());
                  vgpr_spill_temps[spill_slot / ctx.wave_size] = linear_vgpr;
                  aco_ptr<Instruction> create{
                     create_instruction(aco_opcode::p_start_linear_vgpr, Format::PSEUDO, 0, 1)};
                  create->definitions[0] = Definition(linear_vgpr);
                  /* find the right place to insert this definition */
                  if (last_top_level_block_idx == block.index) {
                     /* insert right before the current instruction */
                     instructions.emplace_back(std::move(create));
                  } else {
                     assert(last_top_level_block_idx < block.index);
                     /* insert after p_logical_end of the last top-level block */
                     std::vector<aco_ptr<Instruction>>& block_instrs =
                        ctx.program->blocks[last_top_level_block_idx].instructions;
                     auto insert_point =
                        std::find_if(block_instrs.rbegin(), block_instrs.rend(),
                                     [](const auto& iter) {
                                        return iter->opcode == aco_opcode::p_logical_end;
                                     })
                           .base();
                     block_instrs.insert(insert_point, std::move(create));
                  }
               }

               /* reload sgpr: just add the vgpr temp to operands */
               Instruction* reload = create_instruction(aco_opcode::p_reload, Format::PSEUDO, 2, 1);
               reload->operands[0] = Operand(vgpr_spill_temps[spill_slot / ctx.wave_size]);
               reload->operands[0].setLateKill(true);
               reload->operands[1] = Operand::c32(spill_slot % ctx.wave_size);
               reload->definitions[0] = (*it)->definitions[0];
               instructions.emplace_back(aco_ptr<Instruction>(reload));
            }
         } else if (!ctx.unused_remats.count(it->get())) {
            instructions.emplace_back(std::move(*it));
         }
      }
      block.instructions = std::move(instructions);
   }

   /* update required scratch memory */
   ctx.program->config->scratch_bytes_per_wave += ctx.vgpr_spill_slots * 4 * ctx.program->wave_size;
}

} /* end namespace */

void
spill(Program* program, live& live_vars)
{
   program->config->spilled_vgprs = 0;
   program->config->spilled_sgprs = 0;

   program->progress = CompilationProgress::after_spilling;

   /* no spilling when register pressure is low enough */
   if (program->num_waves > 0)
      return;

   /* lower to CSSA before spilling to ensure correctness w.r.t. phis */
   lower_to_cssa(program, live_vars);

   /* calculate target register demand */
   const RegisterDemand demand = program->max_reg_demand; /* current max */
   const uint16_t sgpr_limit = get_addr_sgpr_from_waves(program, program->min_waves);
   const uint16_t vgpr_limit = get_addr_vgpr_from_waves(program, program->min_waves);
   uint16_t extra_vgprs = 0;
   uint16_t extra_sgprs = 0;

   /* calculate extra VGPRs required for spilling SGPRs */
   if (demand.sgpr > sgpr_limit) {
      unsigned sgpr_spills = demand.sgpr - sgpr_limit;
      extra_vgprs = DIV_ROUND_UP(sgpr_spills * 2, program->wave_size) + 1;
   }
   /* add extra SGPRs required for spilling VGPRs */
   if (demand.vgpr + extra_vgprs > vgpr_limit) {
      if (program->gfx_level >= GFX9)
         extra_sgprs = 1; /* SADDR */
      else
         extra_sgprs = 5; /* scratch_resource (s4) + scratch_offset (s1) */
      if (demand.sgpr + extra_sgprs > sgpr_limit) {
         /* re-calculate in case something has changed */
         unsigned sgpr_spills = demand.sgpr + extra_sgprs - sgpr_limit;
         extra_vgprs = DIV_ROUND_UP(sgpr_spills * 2, program->wave_size) + 1;
      }
   }
   /* the spiller has to target the following register demand */
   const RegisterDemand target(vgpr_limit - extra_vgprs, sgpr_limit - extra_sgprs);

   /* initialize ctx */
   spill_ctx ctx(target, program, live_vars);
   gather_ssa_use_info(ctx);
   get_rematerialize_info(ctx);

   /* create spills and reloads */
   for (unsigned i = 0; i < program->blocks.size(); i++)
      spill_block(ctx, i);

   /* assign spill slots and DCE rematerialized code */
   assign_spill_slots(ctx, extra_vgprs);

   /* update live variable information */
   live_vars = live_var_analysis(program);

   assert(program->num_waves > 0);
}

} // namespace aco
