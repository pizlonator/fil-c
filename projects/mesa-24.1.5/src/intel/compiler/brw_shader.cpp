/*
 * Copyright © 2010 Intel Corporation
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

#include "brw_cfg.h"
#include "brw_eu.h"
#include "brw_fs.h"
#include "brw_nir.h"
#include "brw_private.h"
#include "dev/intel_debug.h"
#include "util/macros.h"

bool
fs_reg_saturate_immediate(fs_reg *reg)
{
   union {
      unsigned ud;
      int d;
      float f;
      double df;
   } imm, sat_imm = { 0 };

   const unsigned size = type_sz(reg->type);

   /* We want to either do a 32-bit or 64-bit data copy, the type is otherwise
    * irrelevant, so just check the size of the type and copy from/to an
    * appropriately sized field.
    */
   if (size < 8)
      imm.ud = reg->ud;
   else
      imm.df = reg->df;

   switch (reg->type) {
   case BRW_REGISTER_TYPE_UD:
   case BRW_REGISTER_TYPE_D:
   case BRW_REGISTER_TYPE_UW:
   case BRW_REGISTER_TYPE_W:
   case BRW_REGISTER_TYPE_UQ:
   case BRW_REGISTER_TYPE_Q:
      /* Nothing to do. */
      return false;
   case BRW_REGISTER_TYPE_F:
      sat_imm.f = SATURATE(imm.f);
      break;
   case BRW_REGISTER_TYPE_DF:
      sat_imm.df = SATURATE(imm.df);
      break;
   case BRW_REGISTER_TYPE_UB:
   case BRW_REGISTER_TYPE_B:
      unreachable("no UB/B immediates");
   case BRW_REGISTER_TYPE_V:
   case BRW_REGISTER_TYPE_UV:
   case BRW_REGISTER_TYPE_VF:
      unreachable("unimplemented: saturate vector immediate");
   case BRW_REGISTER_TYPE_HF:
      unreachable("unimplemented: saturate HF immediate");
   case BRW_REGISTER_TYPE_NF:
      unreachable("no NF immediates");
   }

   if (size < 8) {
      if (imm.ud != sat_imm.ud) {
         reg->ud = sat_imm.ud;
         return true;
      }
   } else {
      if (imm.df != sat_imm.df) {
         reg->df = sat_imm.df;
         return true;
      }
   }
   return false;
}

bool
fs_reg_negate_immediate(fs_reg *reg)
{
   switch (reg->type) {
   case BRW_REGISTER_TYPE_D:
   case BRW_REGISTER_TYPE_UD:
      reg->d = -reg->d;
      return true;
   case BRW_REGISTER_TYPE_W:
   case BRW_REGISTER_TYPE_UW: {
      uint16_t value = -(int16_t)reg->ud;
      reg->ud = value | (uint32_t)value << 16;
      return true;
   }
   case BRW_REGISTER_TYPE_F:
      reg->f = -reg->f;
      return true;
   case BRW_REGISTER_TYPE_VF:
      reg->ud ^= 0x80808080;
      return true;
   case BRW_REGISTER_TYPE_DF:
      reg->df = -reg->df;
      return true;
   case BRW_REGISTER_TYPE_UQ:
   case BRW_REGISTER_TYPE_Q:
      reg->d64 = -reg->d64;
      return true;
   case BRW_REGISTER_TYPE_UB:
   case BRW_REGISTER_TYPE_B:
      unreachable("no UB/B immediates");
   case BRW_REGISTER_TYPE_UV:
   case BRW_REGISTER_TYPE_V:
      assert(!"unimplemented: negate UV/V immediate");
   case BRW_REGISTER_TYPE_HF:
      reg->ud ^= 0x80008000;
      return true;
   case BRW_REGISTER_TYPE_NF:
      unreachable("no NF immediates");
   }

   return false;
}

bool
fs_reg_abs_immediate(fs_reg *reg)
{
   switch (reg->type) {
   case BRW_REGISTER_TYPE_D:
      reg->d = abs(reg->d);
      return true;
   case BRW_REGISTER_TYPE_W: {
      uint16_t value = abs((int16_t)reg->ud);
      reg->ud = value | (uint32_t)value << 16;
      return true;
   }
   case BRW_REGISTER_TYPE_F:
      reg->f = fabsf(reg->f);
      return true;
   case BRW_REGISTER_TYPE_DF:
      reg->df = fabs(reg->df);
      return true;
   case BRW_REGISTER_TYPE_VF:
      reg->ud &= ~0x80808080;
      return true;
   case BRW_REGISTER_TYPE_Q:
      reg->d64 = imaxabs(reg->d64);
      return true;
   case BRW_REGISTER_TYPE_UB:
   case BRW_REGISTER_TYPE_B:
      unreachable("no UB/B immediates");
   case BRW_REGISTER_TYPE_UQ:
   case BRW_REGISTER_TYPE_UD:
   case BRW_REGISTER_TYPE_UW:
   case BRW_REGISTER_TYPE_UV:
      /* Presumably the absolute value modifier on an unsigned source is a
       * nop, but it would be nice to confirm.
       */
      assert(!"unimplemented: abs unsigned immediate");
   case BRW_REGISTER_TYPE_V:
      assert(!"unimplemented: abs V immediate");
   case BRW_REGISTER_TYPE_HF:
      reg->ud &= ~0x80008000;
      return true;
   case BRW_REGISTER_TYPE_NF:
      unreachable("no NF immediates");
   }

   return false;
}

bool
fs_reg::is_zero() const
{
   if (file != IMM)
      return false;

   assert(type_sz(type) > 1);

   switch (type) {
   case BRW_REGISTER_TYPE_HF:
      assert((d & 0xffff) == ((d >> 16) & 0xffff));
      return (d & 0xffff) == 0 || (d & 0xffff) == 0x8000;
   case BRW_REGISTER_TYPE_F:
      return f == 0;
   case BRW_REGISTER_TYPE_DF:
      return df == 0;
   case BRW_REGISTER_TYPE_W:
   case BRW_REGISTER_TYPE_UW:
      assert((d & 0xffff) == ((d >> 16) & 0xffff));
      return (d & 0xffff) == 0;
   case BRW_REGISTER_TYPE_D:
   case BRW_REGISTER_TYPE_UD:
      return d == 0;
   case BRW_REGISTER_TYPE_UQ:
   case BRW_REGISTER_TYPE_Q:
      return u64 == 0;
   default:
      return false;
   }
}

bool
fs_reg::is_one() const
{
   if (file != IMM)
      return false;

   assert(type_sz(type) > 1);

   switch (type) {
   case BRW_REGISTER_TYPE_HF:
      assert((d & 0xffff) == ((d >> 16) & 0xffff));
      return (d & 0xffff) == 0x3c00;
   case BRW_REGISTER_TYPE_F:
      return f == 1.0f;
   case BRW_REGISTER_TYPE_DF:
      return df == 1.0;
   case BRW_REGISTER_TYPE_W:
   case BRW_REGISTER_TYPE_UW:
      assert((d & 0xffff) == ((d >> 16) & 0xffff));
      return (d & 0xffff) == 1;
   case BRW_REGISTER_TYPE_D:
   case BRW_REGISTER_TYPE_UD:
      return d == 1;
   case BRW_REGISTER_TYPE_UQ:
   case BRW_REGISTER_TYPE_Q:
      return u64 == 1;
   default:
      return false;
   }
}

bool
fs_reg::is_negative_one() const
{
   if (file != IMM)
      return false;

   assert(type_sz(type) > 1);

   switch (type) {
   case BRW_REGISTER_TYPE_HF:
      assert((d & 0xffff) == ((d >> 16) & 0xffff));
      return (d & 0xffff) == 0xbc00;
   case BRW_REGISTER_TYPE_F:
      return f == -1.0;
   case BRW_REGISTER_TYPE_DF:
      return df == -1.0;
   case BRW_REGISTER_TYPE_W:
      assert((d & 0xffff) == ((d >> 16) & 0xffff));
      return (d & 0xffff) == 0xffff;
   case BRW_REGISTER_TYPE_D:
      return d == -1;
   case BRW_REGISTER_TYPE_Q:
      return d64 == -1;
   default:
      return false;
   }
}

bool
fs_reg::is_null() const
{
   return file == ARF && nr == BRW_ARF_NULL;
}


bool
fs_reg::is_accumulator() const
{
   return file == ARF && (nr & 0xF0) == BRW_ARF_ACCUMULATOR;
}

bool
fs_inst::is_commutative() const
{
   switch (opcode) {
   case BRW_OPCODE_AND:
   case BRW_OPCODE_OR:
   case BRW_OPCODE_XOR:
   case BRW_OPCODE_ADD:
   case BRW_OPCODE_ADD3:
   case SHADER_OPCODE_MULH:
      return true;

   case BRW_OPCODE_MUL:
      /* Integer multiplication of dword and word sources is not actually
       * commutative. The DW source must be first.
       */
      return !brw_reg_type_is_integer(src[0].type) ||
             type_sz(src[0].type) == type_sz(src[1].type);

   case BRW_OPCODE_SEL:
      /* MIN and MAX are commutative. */
      if (conditional_mod == BRW_CONDITIONAL_GE ||
          conditional_mod == BRW_CONDITIONAL_L) {
         return true;
      }
      FALLTHROUGH;
   default:
      return false;
   }
}

bool
fs_inst::is_3src(const struct brw_compiler *compiler) const
{
   return ::is_3src(&compiler->isa, opcode);
}

bool
fs_inst::is_math() const
{
   return (opcode == SHADER_OPCODE_RCP ||
           opcode == SHADER_OPCODE_RSQ ||
           opcode == SHADER_OPCODE_SQRT ||
           opcode == SHADER_OPCODE_EXP2 ||
           opcode == SHADER_OPCODE_LOG2 ||
           opcode == SHADER_OPCODE_SIN ||
           opcode == SHADER_OPCODE_COS ||
           opcode == SHADER_OPCODE_INT_QUOTIENT ||
           opcode == SHADER_OPCODE_INT_REMAINDER ||
           opcode == SHADER_OPCODE_POW);
}

bool
fs_inst::is_control_flow_begin() const
{
   switch (opcode) {
   case BRW_OPCODE_DO:
   case BRW_OPCODE_IF:
   case BRW_OPCODE_ELSE:
      return true;
   default:
      return false;
   }
}

bool
fs_inst::is_control_flow_end() const
{
   switch (opcode) {
   case BRW_OPCODE_ELSE:
   case BRW_OPCODE_WHILE:
   case BRW_OPCODE_ENDIF:
      return true;
   default:
      return false;
   }
}

bool
fs_inst::is_control_flow() const
{
   switch (opcode) {
   case BRW_OPCODE_DO:
   case BRW_OPCODE_WHILE:
   case BRW_OPCODE_IF:
   case BRW_OPCODE_ELSE:
   case BRW_OPCODE_ENDIF:
   case BRW_OPCODE_BREAK:
   case BRW_OPCODE_CONTINUE:
      return true;
   default:
      return false;
   }
}

bool
fs_inst::uses_indirect_addressing() const
{
   switch (opcode) {
   case SHADER_OPCODE_BROADCAST:
   case SHADER_OPCODE_CLUSTER_BROADCAST:
   case SHADER_OPCODE_MOV_INDIRECT:
      return true;
   default:
      return false;
   }
}

bool
fs_inst::can_do_saturate() const
{
   switch (opcode) {
   case BRW_OPCODE_ADD:
   case BRW_OPCODE_ADD3:
   case BRW_OPCODE_ASR:
   case BRW_OPCODE_AVG:
   case BRW_OPCODE_CSEL:
   case BRW_OPCODE_DP2:
   case BRW_OPCODE_DP3:
   case BRW_OPCODE_DP4:
   case BRW_OPCODE_DPH:
   case BRW_OPCODE_DP4A:
   case BRW_OPCODE_LINE:
   case BRW_OPCODE_LRP:
   case BRW_OPCODE_MAC:
   case BRW_OPCODE_MAD:
   case BRW_OPCODE_MATH:
   case BRW_OPCODE_MOV:
   case BRW_OPCODE_MUL:
   case SHADER_OPCODE_MULH:
   case BRW_OPCODE_PLN:
   case BRW_OPCODE_RNDD:
   case BRW_OPCODE_RNDE:
   case BRW_OPCODE_RNDU:
   case BRW_OPCODE_RNDZ:
   case BRW_OPCODE_SEL:
   case BRW_OPCODE_SHL:
   case BRW_OPCODE_SHR:
   case SHADER_OPCODE_COS:
   case SHADER_OPCODE_EXP2:
   case SHADER_OPCODE_LOG2:
   case SHADER_OPCODE_POW:
   case SHADER_OPCODE_RCP:
   case SHADER_OPCODE_RSQ:
   case SHADER_OPCODE_SIN:
   case SHADER_OPCODE_SQRT:
      return true;
   default:
      return false;
   }
}

bool
fs_inst::reads_accumulator_implicitly() const
{
   switch (opcode) {
   case BRW_OPCODE_MAC:
   case BRW_OPCODE_MACH:
   case BRW_OPCODE_SADA2:
      return true;
   default:
      return false;
   }
}

bool
fs_inst::writes_accumulator_implicitly(const struct intel_device_info *devinfo) const
{
   return writes_accumulator ||
          (eot && intel_needs_workaround(devinfo, 14010017096));
}

bool
fs_inst::has_side_effects() const
{
   switch (opcode) {
   case SHADER_OPCODE_SEND:
      return send_has_side_effects;

   case BRW_OPCODE_SYNC:
   case SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL:
   case SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL:
   case SHADER_OPCODE_A64_UNTYPED_WRITE_LOGICAL:
   case SHADER_OPCODE_A64_BYTE_SCATTERED_WRITE_LOGICAL:
   case SHADER_OPCODE_A64_UNTYPED_ATOMIC_LOGICAL:
   case SHADER_OPCODE_BYTE_SCATTERED_WRITE_LOGICAL:
   case SHADER_OPCODE_DWORD_SCATTERED_WRITE_LOGICAL:
   case SHADER_OPCODE_TYPED_ATOMIC_LOGICAL:
   case SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL:
   case SHADER_OPCODE_MEMORY_FENCE:
   case SHADER_OPCODE_INTERLOCK:
   case SHADER_OPCODE_URB_WRITE_LOGICAL:
   case FS_OPCODE_FB_WRITE_LOGICAL:
   case SHADER_OPCODE_BARRIER:
   case SHADER_OPCODE_RND_MODE:
   case SHADER_OPCODE_FLOAT_CONTROL_MODE:
   case FS_OPCODE_SCHEDULING_FENCE:
   case SHADER_OPCODE_OWORD_BLOCK_WRITE_LOGICAL:
   case SHADER_OPCODE_A64_OWORD_BLOCK_WRITE_LOGICAL:
   case SHADER_OPCODE_BTD_SPAWN_LOGICAL:
   case SHADER_OPCODE_BTD_RETIRE_LOGICAL:
   case RT_OPCODE_TRACE_RAY_LOGICAL:
      return true;
   default:
      return eot;
   }
}

bool
fs_inst::is_volatile() const
{
   switch (opcode) {
   case SHADER_OPCODE_SEND:
      return send_is_volatile;

   case SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL:
   case SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL:
   case SHADER_OPCODE_BYTE_SCATTERED_READ_LOGICAL:
   case SHADER_OPCODE_DWORD_SCATTERED_READ_LOGICAL:
   case SHADER_OPCODE_A64_UNTYPED_READ_LOGICAL:
   case SHADER_OPCODE_A64_BYTE_SCATTERED_READ_LOGICAL:
      return true;
   default:
      return false;
   }
}

#ifndef NDEBUG
static bool
inst_is_in_block(const bblock_t *block, const fs_inst *inst)
{
   const exec_node *n = inst;

   /* Find the tail sentinel. If the tail sentinel is the sentinel from the
    * list header in the bblock_t, then this instruction is in that basic
    * block.
    */
   while (!n->is_tail_sentinel())
      n = n->get_next();

   return n == &block->instructions.tail_sentinel;
}
#endif

static void
adjust_later_block_ips(bblock_t *start_block, int ip_adjustment)
{
   for (bblock_t *block_iter = start_block->next();
        block_iter;
        block_iter = block_iter->next()) {
      block_iter->start_ip += ip_adjustment;
      block_iter->end_ip += ip_adjustment;
   }
}

void
fs_inst::insert_after(bblock_t *block, fs_inst *inst)
{
   assert(this != inst);
   assert(block->end_ip_delta == 0);

   if (!this->is_head_sentinel())
      assert(inst_is_in_block(block, this) || !"Instruction not in block");

   block->end_ip++;

   adjust_later_block_ips(block, 1);

   exec_node::insert_after(inst);
}

void
fs_inst::insert_before(bblock_t *block, fs_inst *inst)
{
   assert(this != inst);
   assert(block->end_ip_delta == 0);

   if (!this->is_tail_sentinel())
      assert(inst_is_in_block(block, this) || !"Instruction not in block");

   block->end_ip++;

   adjust_later_block_ips(block, 1);

   exec_node::insert_before(inst);
}

void
fs_inst::remove(bblock_t *block, bool defer_later_block_ip_updates)
{
   assert(inst_is_in_block(block, this) || !"Instruction not in block");

   if (defer_later_block_ip_updates) {
      block->end_ip_delta--;
   } else {
      assert(block->end_ip_delta == 0);
      adjust_later_block_ips(block, -1);
   }

   if (block->start_ip == block->end_ip) {
      if (block->end_ip_delta != 0) {
         adjust_later_block_ips(block, block->end_ip_delta);
         block->end_ip_delta = 0;
      }

      block->cfg->remove_block(block);
   } else {
      block->end_ip--;
   }

   exec_node::remove();
}

extern "C" const unsigned *
brw_compile_tes(const struct brw_compiler *compiler,
                brw_compile_tes_params *params)
{
   const struct intel_device_info *devinfo = compiler->devinfo;
   nir_shader *nir = params->base.nir;
   const struct brw_tes_prog_key *key = params->key;
   const struct intel_vue_map *input_vue_map = params->input_vue_map;
   struct brw_tes_prog_data *prog_data = params->prog_data;

   const bool debug_enabled = brw_should_print_shader(nir, DEBUG_TES);

   prog_data->base.base.stage = MESA_SHADER_TESS_EVAL;
   prog_data->base.base.ray_queries = nir->info.ray_queries;

   nir->info.inputs_read = key->inputs_read;
   nir->info.patch_inputs_read = key->patch_inputs_read;

   brw_nir_apply_key(nir, compiler, &key->base, 8);
   brw_nir_lower_tes_inputs(nir, input_vue_map);
   brw_nir_lower_vue_outputs(nir);
   brw_postprocess_nir(nir, compiler, debug_enabled,
                       key->base.robust_flags);

   brw_compute_vue_map(devinfo, &prog_data->base.vue_map,
                       nir->info.outputs_written,
                       nir->info.separate_shader, 1);

   unsigned output_size_bytes = prog_data->base.vue_map.num_slots * 4 * 4;

   assert(output_size_bytes >= 1);
   if (output_size_bytes > GFX7_MAX_DS_URB_ENTRY_SIZE_BYTES) {
      params->base.error_str = ralloc_strdup(params->base.mem_ctx,
                                             "DS outputs exceed maximum size");
      return NULL;
   }

   prog_data->base.clip_distance_mask =
      ((1 << nir->info.clip_distance_array_size) - 1);
   prog_data->base.cull_distance_mask =
      ((1 << nir->info.cull_distance_array_size) - 1) <<
      nir->info.clip_distance_array_size;

   prog_data->include_primitive_id =
      BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_PRIMITIVE_ID);

   /* URB entry sizes are stored as a multiple of 64 bytes. */
   prog_data->base.urb_entry_size = ALIGN(output_size_bytes, 64) / 64;

   prog_data->base.urb_read_length = 0;

   STATIC_ASSERT(INTEL_TESS_PARTITIONING_INTEGER == TESS_SPACING_EQUAL - 1);
   STATIC_ASSERT(INTEL_TESS_PARTITIONING_ODD_FRACTIONAL ==
                 TESS_SPACING_FRACTIONAL_ODD - 1);
   STATIC_ASSERT(INTEL_TESS_PARTITIONING_EVEN_FRACTIONAL ==
                 TESS_SPACING_FRACTIONAL_EVEN - 1);

   prog_data->partitioning =
      (enum intel_tess_partitioning) (nir->info.tess.spacing - 1);

   switch (nir->info.tess._primitive_mode) {
   case TESS_PRIMITIVE_QUADS:
      prog_data->domain = INTEL_TESS_DOMAIN_QUAD;
      break;
   case TESS_PRIMITIVE_TRIANGLES:
      prog_data->domain = INTEL_TESS_DOMAIN_TRI;
      break;
   case TESS_PRIMITIVE_ISOLINES:
      prog_data->domain = INTEL_TESS_DOMAIN_ISOLINE;
      break;
   default:
      unreachable("invalid domain shader primitive mode");
   }

   if (nir->info.tess.point_mode) {
      prog_data->output_topology = INTEL_TESS_OUTPUT_TOPOLOGY_POINT;
   } else if (nir->info.tess._primitive_mode == TESS_PRIMITIVE_ISOLINES) {
      prog_data->output_topology = INTEL_TESS_OUTPUT_TOPOLOGY_LINE;
   } else {
      /* Hardware winding order is backwards from OpenGL */
      prog_data->output_topology =
         nir->info.tess.ccw ? INTEL_TESS_OUTPUT_TOPOLOGY_TRI_CW
                             : INTEL_TESS_OUTPUT_TOPOLOGY_TRI_CCW;
   }

   if (unlikely(debug_enabled)) {
      fprintf(stderr, "TES Input ");
      brw_print_vue_map(stderr, input_vue_map, MESA_SHADER_TESS_EVAL);
      fprintf(stderr, "TES Output ");
      brw_print_vue_map(stderr, &prog_data->base.vue_map,
                        MESA_SHADER_TESS_EVAL);
   }

   const unsigned dispatch_width = devinfo->ver >= 20 ? 16 : 8;
   fs_visitor v(compiler, &params->base, &key->base,
                &prog_data->base.base, nir, dispatch_width,
                params->base.stats != NULL, debug_enabled);
   if (!v.run_tes()) {
      params->base.error_str =
         ralloc_strdup(params->base.mem_ctx, v.fail_msg);
      return NULL;
   }

   assert(v.payload().num_regs % reg_unit(devinfo) == 0);
   prog_data->base.base.dispatch_grf_start_reg = v.payload().num_regs / reg_unit(devinfo);

   prog_data->base.dispatch_mode = INTEL_DISPATCH_MODE_SIMD8;

   fs_generator g(compiler, &params->base,
                  &prog_data->base.base, MESA_SHADER_TESS_EVAL);
   if (unlikely(debug_enabled)) {
      g.enable_debug(ralloc_asprintf(params->base.mem_ctx,
                                     "%s tessellation evaluation shader %s",
                                     nir->info.label ? nir->info.label
                                                     : "unnamed",
                                     nir->info.name));
   }

   g.generate_code(v.cfg, dispatch_width, v.shader_stats,
                   v.performance_analysis.require(), params->base.stats);

   g.add_const_data(nir->constant_data, nir->constant_data_size);

   return g.get_assembly();
}
