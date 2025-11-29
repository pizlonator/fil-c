/*
 * Copyright © 2010 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "brw_eu.h"
#include "brw_fs.h"
#include "brw_fs_builder.h"

using namespace brw;

void
brw_fs_optimize(fs_visitor &s)
{
   const nir_shader *nir = s.nir;

   s.debug_optimizer(nir, "start", 0, 0);

   /* Start by validating the shader we currently have. */
   brw_fs_validate(s);

   bool progress = false;
   int iteration = 0;
   int pass_num = 0;

#define OPT(pass, ...) ({                                               \
      pass_num++;                                                       \
      bool this_progress = pass(s, ##__VA_ARGS__);                      \
                                                                        \
      if (this_progress)                                                \
         s.debug_optimizer(nir, #pass, iteration, pass_num);            \
                                                                        \
      brw_fs_validate(s);                                               \
                                                                        \
      progress = progress || this_progress;                             \
      this_progress;                                                    \
   })

   s.assign_constant_locations();
   OPT(brw_fs_lower_constant_loads);

   if (s.compiler->lower_dpas)
      OPT(brw_fs_lower_dpas);

   OPT(brw_fs_opt_split_virtual_grfs);

   /* Before anything else, eliminate dead code.  The results of some NIR
    * instructions may effectively be calculated twice.  Once when the
    * instruction is encountered, and again when the user of that result is
    * encountered.  Wipe those away before algebraic optimizations and
    * especially copy propagation can mix things up.
    */
   OPT(brw_fs_opt_dead_code_eliminate);

   OPT(brw_fs_opt_remove_extra_rounding_modes);

   OPT(brw_fs_opt_eliminate_find_live_channel);

   do {
      progress = false;
      pass_num = 0;
      iteration++;

      OPT(brw_fs_opt_algebraic);
      OPT(brw_fs_opt_cse);
      OPT(brw_fs_opt_copy_propagation);
      OPT(brw_fs_opt_predicated_break);
      OPT(brw_fs_opt_cmod_propagation);
      OPT(brw_fs_opt_dead_code_eliminate);
      OPT(brw_fs_opt_peephole_sel);
      OPT(brw_fs_opt_dead_control_flow_eliminate);
      OPT(brw_fs_opt_saturate_propagation);
      OPT(brw_fs_opt_register_coalesce);

      OPT(brw_fs_opt_compact_virtual_grfs);
   } while (progress);

   progress = false;
   pass_num = 0;

   if (OPT(brw_fs_lower_pack)) {
      OPT(brw_fs_opt_register_coalesce);
      OPT(brw_fs_opt_dead_code_eliminate);
   }

   OPT(brw_fs_lower_simd_width);
   OPT(brw_fs_lower_barycentrics);
   OPT(brw_fs_lower_logical_sends);

   /* After logical SEND lowering. */

   if (OPT(brw_fs_opt_copy_propagation))
      OPT(brw_fs_opt_algebraic);

   /* Identify trailing zeros LOAD_PAYLOAD of sampler messages.
    * Do this before splitting SENDs.
    */
   if (OPT(brw_fs_opt_zero_samples) && OPT(brw_fs_opt_copy_propagation))
      OPT(brw_fs_opt_algebraic);

   OPT(brw_fs_opt_split_sends);
   OPT(brw_fs_workaround_nomask_control_flow);

   if (progress) {
      if (OPT(brw_fs_opt_copy_propagation))
         OPT(brw_fs_opt_algebraic);

      /* Run after logical send lowering to give it a chance to CSE the
       * LOAD_PAYLOAD instructions created to construct the payloads of
       * e.g. texturing messages in cases where it wasn't possible to CSE the
       * whole logical instruction.
       */
      OPT(brw_fs_opt_cse);
      OPT(brw_fs_opt_register_coalesce);
      OPT(brw_fs_opt_dead_code_eliminate);
      OPT(brw_fs_opt_peephole_sel);
   }

   OPT(brw_fs_opt_remove_redundant_halts);

   if (OPT(brw_fs_lower_load_payload)) {
      OPT(brw_fs_opt_split_virtual_grfs);

      OPT(brw_fs_opt_register_coalesce);
      OPT(brw_fs_lower_simd_width);
      OPT(brw_fs_opt_dead_code_eliminate);
   }

   OPT(brw_fs_lower_alu_restrictions);

   OPT(brw_fs_opt_combine_constants);
   if (OPT(brw_fs_lower_integer_multiplication)) {
      /* If lower_integer_multiplication made progress, it may have produced
       * some 32x32-bit MULs in the process of lowering 64-bit MULs.  Run it
       * one more time to clean those up if they exist.
       */
      OPT(brw_fs_lower_integer_multiplication);
   }
   OPT(brw_fs_lower_sub_sat);

   progress = false;
   OPT(brw_fs_lower_derivatives);
   OPT(brw_fs_lower_regioning);
   if (progress) {
      if (OPT(brw_fs_opt_copy_propagation)) {
         OPT(brw_fs_opt_algebraic);
         OPT(brw_fs_opt_combine_constants);
      }
      OPT(brw_fs_opt_dead_code_eliminate);
      OPT(brw_fs_lower_simd_width);
   }

   OPT(brw_fs_lower_sends_overlapping_payload);

   OPT(brw_fs_lower_uniform_pull_constant_loads);

   OPT(brw_fs_lower_find_live_channel);
}

static unsigned
load_payload_sources_read_for_size(fs_inst *lp, unsigned size_read)
{
   assert(lp->opcode == SHADER_OPCODE_LOAD_PAYLOAD);
   assert(size_read >= lp->header_size * REG_SIZE);

   unsigned i;
   unsigned size = lp->header_size * REG_SIZE;
   for (i = lp->header_size; size < size_read && i < lp->sources; i++)
      size += lp->exec_size * type_sz(lp->src[i].type);

   /* Size read must cover exactly a subset of sources. */
   assert(size == size_read);
   return i;
}

/**
 * Optimize sample messages that have constant zero values for the trailing
 * parameters. We can just reduce the message length for these
 * instructions instead of reserving a register for it. Trailing parameters
 * that aren't sent default to zero anyway. This will cause the dead code
 * eliminator to remove the MOV instruction that would otherwise be emitted to
 * set up the zero value.
 */

bool
brw_fs_opt_zero_samples(fs_visitor &s)
{
   bool progress = false;

   foreach_block_and_inst(block, fs_inst, send, s.cfg) {
      if (send->opcode != SHADER_OPCODE_SEND ||
          send->sfid != BRW_SFID_SAMPLER)
         continue;

      /* Wa_14012688258:
       *
       * Don't trim zeros at the end of payload for sample operations
       * in cube and cube arrays.
       */
      if (send->keep_payload_trailing_zeros)
         continue;

      /* This pass works on SENDs before splitting. */
      if (send->ex_mlen > 0)
         continue;

      fs_inst *lp = (fs_inst *) send->prev;

      if (lp->is_head_sentinel() || lp->opcode != SHADER_OPCODE_LOAD_PAYLOAD)
         continue;

      /* How much of the payload are actually read by this SEND. */
      const unsigned params =
         load_payload_sources_read_for_size(lp, send->mlen * REG_SIZE);

      /* We don't want to remove the message header or the first parameter.
       * Removing the first parameter is not allowed, see the Haswell PRM
       * volume 7, page 149:
       *
       *     "Parameter 0 is required except for the sampleinfo message, which
       *      has no parameter 0"
       */
      const unsigned first_param_idx = lp->header_size;
      unsigned zero_size = 0;
      for (unsigned i = params - 1; i > first_param_idx; i--) {
         if (lp->src[i].file != BAD_FILE && !lp->src[i].is_zero())
            break;
         zero_size += lp->exec_size * type_sz(lp->src[i].type) * lp->dst.stride;
      }

      /* Round down to ensure to only consider full registers. */
      const unsigned zero_len = ROUND_DOWN_TO(zero_size / REG_SIZE, reg_unit(s.devinfo));
      if (zero_len > 0) {
         /* Note mlen is in REG_SIZE units. */
         send->mlen -= zero_len;
         progress = true;
      }
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTION_DETAIL);

   return progress;
}

/**
 * Opportunistically split SEND message payloads.
 *
 * Gfx9+ supports "split" SEND messages, which take two payloads that are
 * implicitly concatenated.  If we find a SEND message with a single payload,
 * we can split that payload in two.  This results in smaller contiguous
 * register blocks for us to allocate.  But it can help beyond that, too.
 *
 * We try and split a LOAD_PAYLOAD between sources which change registers.
 * For example, a sampler message often contains a x/y/z coordinate that may
 * already be in a contiguous VGRF, combined with an LOD, shadow comparitor,
 * or array index, which comes from elsewhere.  In this case, the first few
 * sources will be different offsets of the same VGRF, then a later source
 * will be a different VGRF.  So we split there, possibly eliminating the
 * payload concatenation altogether.
 */
bool
brw_fs_opt_split_sends(fs_visitor &s)
{
   bool progress = false;

   foreach_block_and_inst(block, fs_inst, send, s.cfg) {
      if (send->opcode != SHADER_OPCODE_SEND ||
          send->mlen <= reg_unit(s.devinfo) || send->ex_mlen > 0 ||
          send->src[2].file != VGRF)
         continue;

      /* Currently don't split sends that reuse a previously used payload. */
      fs_inst *lp = (fs_inst *) send->prev;

      if (lp->is_head_sentinel() || lp->opcode != SHADER_OPCODE_LOAD_PAYLOAD)
         continue;

      if (lp->dst.file != send->src[2].file || lp->dst.nr != send->src[2].nr)
         continue;

      /* Split either after the header (if present), or when consecutive
       * sources switch from one VGRF to a different one.
       */
      unsigned mid = lp->header_size;
      if (mid == 0) {
         for (mid = 1; mid < lp->sources; mid++) {
            if (lp->src[mid].file == BAD_FILE)
               continue;

            if (lp->src[0].file != lp->src[mid].file ||
                lp->src[0].nr != lp->src[mid].nr)
               break;
         }
      }

      /* SEND mlen might be smaller than what LOAD_PAYLOAD provides, so
       * find out how many sources from the payload does it really need.
       */
      const unsigned end =
         load_payload_sources_read_for_size(lp, send->mlen * REG_SIZE);

      /* Nothing to split. */
      if (end <= mid)
         continue;

      const fs_builder ibld(&s, block, lp);
      fs_inst *lp1 = ibld.LOAD_PAYLOAD(lp->dst, &lp->src[0], mid, lp->header_size);
      fs_inst *lp2 = ibld.LOAD_PAYLOAD(lp->dst, &lp->src[mid], end - mid, 0);

      assert(lp1->size_written % REG_SIZE == 0);
      assert(lp2->size_written % REG_SIZE == 0);
      assert((lp1->size_written + lp2->size_written) / REG_SIZE == send->mlen);

      lp1->dst = fs_reg(VGRF, s.alloc.allocate(lp1->size_written / REG_SIZE), lp1->dst.type);
      lp2->dst = fs_reg(VGRF, s.alloc.allocate(lp2->size_written / REG_SIZE), lp2->dst.type);

      send->resize_sources(4);
      send->src[2] = lp1->dst;
      send->src[3] = lp2->dst;
      send->ex_mlen = lp2->size_written / REG_SIZE;
      send->mlen -= send->ex_mlen;

      progress = true;
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}

/**
 * Remove redundant or useless halts.
 *
 * For example, we can eliminate halts in the following sequence:
 *
 * halt        (redundant with the next halt)
 * halt        (useless; jumps to the next instruction)
 * halt-target
 */
bool
brw_fs_opt_remove_redundant_halts(fs_visitor &s)
{
   bool progress = false;

   unsigned halt_count = 0;
   fs_inst *halt_target = NULL;
   bblock_t *halt_target_block = NULL;
   foreach_block_and_inst(block, fs_inst, inst, s.cfg) {
      if (inst->opcode == BRW_OPCODE_HALT)
         halt_count++;

      if (inst->opcode == SHADER_OPCODE_HALT_TARGET) {
         halt_target = inst;
         halt_target_block = block;
         break;
      }
   }

   if (!halt_target) {
      assert(halt_count == 0);
      return false;
   }

   /* Delete any HALTs immediately before the halt target. */
   for (fs_inst *prev = (fs_inst *) halt_target->prev;
        !prev->is_head_sentinel() && prev->opcode == BRW_OPCODE_HALT;
        prev = (fs_inst *) halt_target->prev) {
      prev->remove(halt_target_block);
      halt_count--;
      progress = true;
   }

   if (halt_count == 0) {
      halt_target->remove(halt_target_block);
      progress = true;
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS);

   return progress;
}

/**
 * Eliminate FIND_LIVE_CHANNEL instructions occurring outside any control
 * flow.  We could probably do better here with some form of divergence
 * analysis.
 */
bool
brw_fs_opt_eliminate_find_live_channel(fs_visitor &s)
{
   bool progress = false;
   unsigned depth = 0;

   if (!brw_stage_has_packed_dispatch(s.devinfo, s.stage, s.max_polygons,
                                      s.prog_data)) {
      /* The optimization below assumes that channel zero is live on thread
       * dispatch, which may not be the case if the fixed function dispatches
       * threads sparsely.
       */
      return false;
   }

   foreach_block_and_inst_safe(block, fs_inst, inst, s.cfg) {
      switch (inst->opcode) {
      case BRW_OPCODE_IF:
      case BRW_OPCODE_DO:
         depth++;
         break;

      case BRW_OPCODE_ENDIF:
      case BRW_OPCODE_WHILE:
         depth--;
         break;

      case BRW_OPCODE_HALT:
         /* This can potentially make control flow non-uniform until the end
          * of the program.
          */
         goto out;

      case SHADER_OPCODE_FIND_LIVE_CHANNEL:
         if (depth == 0) {
            inst->opcode = BRW_OPCODE_MOV;
            inst->src[0] = brw_imm_ud(0u);
            inst->sources = 1;
            inst->force_writemask_all = true;
            progress = true;

            /* emit_uniformize() frequently emits FIND_LIVE_CHANNEL paired
             * with a BROADCAST.  Save some work for opt_copy_propagation
             * and opt_algebraic by trivially cleaning up both together.
             */
            assert(!inst->next->is_tail_sentinel());
            fs_inst *bcast = (fs_inst *) inst->next;

            /* Ignore stride when comparing */
            if (bcast->opcode == SHADER_OPCODE_BROADCAST &&
                inst->dst.file == VGRF &&
                inst->dst.file == bcast->src[1].file &&
                inst->dst.nr == bcast->src[1].nr &&
                inst->dst.offset == bcast->src[1].offset) {
               bcast->opcode = BRW_OPCODE_MOV;
               if (!is_uniform(bcast->src[0]))
                  bcast->src[0] = component(bcast->src[0], 0);
               bcast->sources = 1;
               bcast->force_writemask_all = true;
            }
         }
         break;

      default:
         break;
      }
   }

out:
   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTION_DETAIL);

   return progress;
}

/**
 * Rounding modes for conversion instructions are included for each
 * conversion, but right now it is a state. So once it is set,
 * we don't need to call it again for subsequent calls.
 *
 * This is useful for vector/matrices conversions, as setting the
 * mode once is enough for the full vector/matrix
 */
bool
brw_fs_opt_remove_extra_rounding_modes(fs_visitor &s)
{
   bool progress = false;
   unsigned execution_mode = s.nir->info.float_controls_execution_mode;

   brw_rnd_mode base_mode = BRW_RND_MODE_UNSPECIFIED;
   if ((FLOAT_CONTROLS_ROUNDING_MODE_RTE_FP16 |
        FLOAT_CONTROLS_ROUNDING_MODE_RTE_FP32 |
        FLOAT_CONTROLS_ROUNDING_MODE_RTE_FP64) &
       execution_mode)
      base_mode = BRW_RND_MODE_RTNE;
   if ((FLOAT_CONTROLS_ROUNDING_MODE_RTZ_FP16 |
        FLOAT_CONTROLS_ROUNDING_MODE_RTZ_FP32 |
        FLOAT_CONTROLS_ROUNDING_MODE_RTZ_FP64) &
       execution_mode)
      base_mode = BRW_RND_MODE_RTZ;

   foreach_block (block, s.cfg) {
      brw_rnd_mode prev_mode = base_mode;

      foreach_inst_in_block_safe (fs_inst, inst, block) {
         if (inst->opcode == SHADER_OPCODE_RND_MODE) {
            assert(inst->src[0].file == BRW_IMMEDIATE_VALUE);
            const brw_rnd_mode mode = (brw_rnd_mode) inst->src[0].d;
            if (mode == prev_mode) {
               inst->remove(block);
               progress = true;
            } else {
               prev_mode = mode;
            }
         }
      }
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS);

   return progress;
}

