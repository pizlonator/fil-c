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

/** @file brw_fs_generator.cpp
 *
 * This file supports generating code from the FS LIR to the actual
 * native instructions.
 */

#include "brw_eu.h"
#include "brw_disasm_info.h"
#include "brw_fs.h"
#include "brw_cfg.h"
#include "dev/intel_debug.h"
#include "util/mesa-sha1.h"
#include "util/half_float.h"

static uint32_t
brw_math_function(enum opcode op)
{
   switch (op) {
   case SHADER_OPCODE_RCP:
      return BRW_MATH_FUNCTION_INV;
   case SHADER_OPCODE_RSQ:
      return BRW_MATH_FUNCTION_RSQ;
   case SHADER_OPCODE_SQRT:
      return BRW_MATH_FUNCTION_SQRT;
   case SHADER_OPCODE_EXP2:
      return BRW_MATH_FUNCTION_EXP;
   case SHADER_OPCODE_LOG2:
      return BRW_MATH_FUNCTION_LOG;
   case SHADER_OPCODE_POW:
      return BRW_MATH_FUNCTION_POW;
   case SHADER_OPCODE_SIN:
      return BRW_MATH_FUNCTION_SIN;
   case SHADER_OPCODE_COS:
      return BRW_MATH_FUNCTION_COS;
   case SHADER_OPCODE_INT_QUOTIENT:
      return BRW_MATH_FUNCTION_INT_DIV_QUOTIENT;
   case SHADER_OPCODE_INT_REMAINDER:
      return BRW_MATH_FUNCTION_INT_DIV_REMAINDER;
   default:
      unreachable("not reached: unknown math function");
   }
}

static struct brw_reg
brw_reg_from_fs_reg(const struct intel_device_info *devinfo, fs_inst *inst,
                    fs_reg *reg)
{
   struct brw_reg brw_reg;

   switch (reg->file) {
   case ARF:
   case FIXED_GRF:
   case IMM:
      assert(reg->offset == 0);
      brw_reg = reg->as_brw_reg();
      break;
   case BAD_FILE:
      /* Probably unused. */
      brw_reg = brw_null_reg();
      break;
   case VGRF:
   case ATTR:
   case UNIFORM:
      unreachable("not reached");
   }

   return brw_reg;
}

fs_generator::fs_generator(const struct brw_compiler *compiler,
                           const struct brw_compile_params *params,
                           struct brw_stage_prog_data *prog_data,
                           gl_shader_stage stage)

   : compiler(compiler), params(params),
     devinfo(compiler->devinfo),
     prog_data(prog_data), dispatch_width(0),
     debug_flag(false),
     shader_name(NULL), stage(stage), mem_ctx(params->mem_ctx)
{
   p = rzalloc(mem_ctx, struct brw_codegen);
   brw_init_codegen(&compiler->isa, p, mem_ctx);
}

fs_generator::~fs_generator()
{
}

class ip_record : public exec_node {
public:
   DECLARE_RALLOC_CXX_OPERATORS(ip_record)

   ip_record(int ip)
   {
      this->ip = ip;
   }

   int ip;
};

bool
fs_generator::patch_halt_jumps()
{
   if (this->discard_halt_patches.is_empty())
      return false;

   int scale = brw_jump_scale(p->devinfo);

   /* There is a somewhat strange undocumented requirement of using
    * HALT, according to the simulator.  If some channel has HALTed to
    * a particular UIP, then by the end of the program, every channel
    * must have HALTed to that UIP.  Furthermore, the tracking is a
    * stack, so you can't do the final halt of a UIP after starting
    * halting to a new UIP.
    *
    * Symptoms of not emitting this instruction on actual hardware
    * included GPU hangs and sparkly rendering on the piglit discard
    * tests.
    */
   brw_inst *last_halt = brw_HALT(p);
   brw_inst_set_uip(p->devinfo, last_halt, 1 * scale);
   brw_inst_set_jip(p->devinfo, last_halt, 1 * scale);

   int ip = p->nr_insn;

   foreach_in_list(ip_record, patch_ip, &discard_halt_patches) {
      brw_inst *patch = &p->store[patch_ip->ip];

      assert(brw_inst_opcode(p->isa, patch) == BRW_OPCODE_HALT);
      /* HALT takes a half-instruction distance from the pre-incremented IP. */
      brw_inst_set_uip(p->devinfo, patch, (ip - patch_ip->ip) * scale);
   }

   this->discard_halt_patches.make_empty();

   return true;
}

void
fs_generator::generate_send(fs_inst *inst,
                            struct brw_reg dst,
                            struct brw_reg desc,
                            struct brw_reg ex_desc,
                            struct brw_reg payload,
                            struct brw_reg payload2)
{
   const unsigned rlen = inst->dst.is_null() ? 0 : inst->size_written / REG_SIZE;

   uint32_t desc_imm = inst->desc |
      brw_message_desc(devinfo, inst->mlen, rlen, inst->header_size);

   uint32_t ex_desc_imm = inst->ex_desc |
      brw_message_ex_desc(devinfo, inst->ex_mlen);

   if (ex_desc.file != BRW_IMMEDIATE_VALUE || ex_desc.ud || ex_desc_imm ||
       inst->send_ex_desc_scratch) {
      /* If we have any sort of extended descriptor, then we need SENDS.  This
       * also covers the dual-payload case because ex_mlen goes in ex_desc.
       */
      brw_send_indirect_split_message(p, inst->sfid, dst, payload, payload2,
                                      desc, desc_imm, ex_desc, ex_desc_imm,
                                      inst->send_ex_desc_scratch,
                                      inst->send_ex_bso, inst->eot);
      if (inst->check_tdr)
         brw_inst_set_opcode(p->isa, brw_last_inst,
                             devinfo->ver >= 12 ? BRW_OPCODE_SENDC : BRW_OPCODE_SENDSC);
   } else {
      brw_send_indirect_message(p, inst->sfid, dst, payload, desc, desc_imm,
                                   inst->eot);
      if (inst->check_tdr)
         brw_inst_set_opcode(p->isa, brw_last_inst, BRW_OPCODE_SENDC);
   }
}

void
fs_generator::generate_mov_indirect(fs_inst *inst,
                                    struct brw_reg dst,
                                    struct brw_reg reg,
                                    struct brw_reg indirect_byte_offset)
{
   assert(indirect_byte_offset.type == BRW_REGISTER_TYPE_UD);
   assert(indirect_byte_offset.file == BRW_GENERAL_REGISTER_FILE);
   assert(!reg.abs && !reg.negate);

   /* Gen12.5 adds the following region restriction:
    *
    *    "Vx1 and VxH indirect addressing for Float, Half-Float, Double-Float
    *    and Quad-Word data must not be used."
    *
    * We require the source and destination types to match so stomp to an
    * unsigned integer type.
    */
   assert(reg.type == dst.type);
   reg.type = dst.type = brw_reg_type_from_bit_size(type_sz(reg.type) * 8,
                                                    BRW_REGISTER_TYPE_UD);

   unsigned imm_byte_offset = reg.nr * REG_SIZE + reg.subnr;

   if (indirect_byte_offset.file == BRW_IMMEDIATE_VALUE) {
      imm_byte_offset += indirect_byte_offset.ud;

      reg.nr = imm_byte_offset / REG_SIZE;
      reg.subnr = imm_byte_offset % REG_SIZE;
      if (type_sz(reg.type) > 4 && !devinfo->has_64bit_int) {
         brw_MOV(p, subscript(dst, BRW_REGISTER_TYPE_D, 0),
                    subscript(reg, BRW_REGISTER_TYPE_D, 0));
         brw_set_default_swsb(p, tgl_swsb_null());
         brw_MOV(p, subscript(dst, BRW_REGISTER_TYPE_D, 1),
                    subscript(reg, BRW_REGISTER_TYPE_D, 1));
      } else {
         brw_MOV(p, dst, reg);
      }
   } else {
      /* We use VxH indirect addressing, clobbering a0.0 through a0.7. */
      struct brw_reg addr = vec8(brw_address_reg(0));

      /* Whether we can use destination dependency control without running the
       * risk of a hang if an instruction gets shot down.
       */
      const bool use_dep_ctrl = !inst->predicate &&
                                inst->exec_size == dispatch_width;
      brw_inst *insn;

      /* The destination stride of an instruction (in bytes) must be greater
       * than or equal to the size of the rest of the instruction.  Since the
       * address register is of type UW, we can't use a D-type instruction.
       * In order to get around this, re retype to UW and use a stride.
       */
      indirect_byte_offset =
         retype(spread(indirect_byte_offset, 2), BRW_REGISTER_TYPE_UW);

      /* There are a number of reasons why we don't use the base offset here.
       * One reason is that the field is only 9 bits which means we can only
       * use it to access the first 16 GRFs.  Also, from the Haswell PRM
       * section "Register Region Restrictions":
       *
       *    "The lower bits of the AddressImmediate must not overflow to
       *    change the register address.  The lower 5 bits of Address
       *    Immediate when added to lower 5 bits of address register gives
       *    the sub-register offset. The upper bits of Address Immediate
       *    when added to upper bits of address register gives the register
       *    address. Any overflow from sub-register offset is dropped."
       *
       * Since the indirect may cause us to cross a register boundary, this
       * makes the base offset almost useless.  We could try and do something
       * clever where we use a actual base offset if base_offset % 32 == 0 but
       * that would mean we were generating different code depending on the
       * base offset.  Instead, for the sake of consistency, we'll just do the
       * add ourselves.  This restriction is only listed in the Haswell PRM
       * but empirical testing indicates that it applies on all older
       * generations and is lifted on Broadwell.
       *
       * In the end, while base_offset is nice to look at in the generated
       * code, using it saves us 0 instructions and would require quite a bit
       * of case-by-case work.  It's just not worth it.
       *
       * Due to a hardware bug some platforms (particularly Gfx11+) seem to
       * require the address components of all channels to be valid whether or
       * not they're active, which causes issues if we use VxH addressing
       * under non-uniform control-flow.  We can easily work around that by
       * initializing the whole address register with a pipelined NoMask MOV
       * instruction.
       */
      insn = brw_MOV(p, addr, brw_imm_uw(imm_byte_offset));
      brw_inst_set_mask_control(devinfo, insn, BRW_MASK_DISABLE);
      brw_inst_set_pred_control(devinfo, insn, BRW_PREDICATE_NONE);
      if (devinfo->ver >= 12)
         brw_set_default_swsb(p, tgl_swsb_null());
      else
         brw_inst_set_no_dd_clear(devinfo, insn, use_dep_ctrl);

      insn = brw_ADD(p, addr, indirect_byte_offset, brw_imm_uw(imm_byte_offset));
      if (devinfo->ver >= 12)
         brw_set_default_swsb(p, tgl_swsb_regdist(1));
      else
         brw_inst_set_no_dd_check(devinfo, insn, use_dep_ctrl);

      if (type_sz(reg.type) > 4 &&
          (intel_device_info_is_9lp(devinfo) || !devinfo->has_64bit_int)) {
         /* From the Cherryview PRM Vol 7. "Register Region Restrictions":
          *
          *   "When source or destination datatype is 64b or operation is
          *    integer DWord multiply, indirect addressing must not be used."
          *
          * We may also not support Q/UQ types.
          *
          * To work around both of these, we do two integer MOVs instead
          * of one 64-bit MOV.  Because no double value should ever cross
          * a register boundary, it's safe to use the immediate offset in
          * the indirect here to handle adding 4 bytes to the offset and
          * avoid the extra ADD to the register file.
          */
         brw_MOV(p, subscript(dst, BRW_REGISTER_TYPE_D, 0),
                    retype(brw_VxH_indirect(0, 0), BRW_REGISTER_TYPE_D));
         brw_set_default_swsb(p, tgl_swsb_null());
         brw_MOV(p, subscript(dst, BRW_REGISTER_TYPE_D, 1),
                    retype(brw_VxH_indirect(0, 4), BRW_REGISTER_TYPE_D));
      } else {
         struct brw_reg ind_src = brw_VxH_indirect(0, 0);

         brw_MOV(p, dst, retype(ind_src, reg.type));
      }
   }
}

void
fs_generator::generate_shuffle(fs_inst *inst,
                               struct brw_reg dst,
                               struct brw_reg src,
                               struct brw_reg idx)
{
   assert(src.file == BRW_GENERAL_REGISTER_FILE);
   assert(!src.abs && !src.negate);

   /* Ivy bridge has some strange behavior that makes this a real pain to
    * implement for 64-bit values so we just don't bother.
    */
   assert(devinfo->has_64bit_float || type_sz(src.type) <= 4);

   /* Gen12.5 adds the following region restriction:
    *
    *    "Vx1 and VxH indirect addressing for Float, Half-Float, Double-Float
    *    and Quad-Word data must not be used."
    *
    * We require the source and destination types to match so stomp to an
    * unsigned integer type.
    */
   assert(src.type == dst.type);
   src.type = dst.type = brw_reg_type_from_bit_size(type_sz(src.type) * 8,
                                                    BRW_REGISTER_TYPE_UD);

   /* Because we're using the address register, we're limited to 16-wide
    * by the address register file and 8-wide for 64-bit types.  We could try
    * and make this instruction splittable higher up in the compiler but that
    * gets weird because it reads all of the channels regardless of execution
    * size.  It's easier just to split it here.
    */
   const unsigned lower_width =
      element_sz(src) > 4 || element_sz(dst) > 4 ? 8 :
      MIN2(16, inst->exec_size);

   brw_set_default_exec_size(p, cvt(lower_width) - 1);
   for (unsigned group = 0; group < inst->exec_size; group += lower_width) {
      brw_set_default_group(p, group);

      if ((src.vstride == 0 && src.hstride == 0) ||
          idx.file == BRW_IMMEDIATE_VALUE) {
         /* Trivial, the source is already uniform or the index is a constant.
          * We will typically not get here if the optimizer is doing its job,
          * but asserting would be mean.
          */
         const unsigned i = idx.file == BRW_IMMEDIATE_VALUE ? idx.ud : 0;
         struct brw_reg group_src = stride(suboffset(src, i), 0, 1, 0);
         struct brw_reg group_dst = suboffset(dst, group << (dst.hstride - 1));
         brw_MOV(p, group_dst, group_src);
      } else {
         /* We use VxH indirect addressing, clobbering a0.0 through a0.7. */
         struct brw_reg addr = vec8(brw_address_reg(0));

         struct brw_reg group_idx = suboffset(idx, group);

         if (lower_width == 8 && group_idx.width == BRW_WIDTH_16) {
            /* Things get grumpy if the register is too wide. */
            group_idx.width--;
            group_idx.vstride--;
         }

         assert(type_sz(group_idx.type) <= 4);
         if (type_sz(group_idx.type) == 4) {
            /* The destination stride of an instruction (in bytes) must be
             * greater than or equal to the size of the rest of the
             * instruction.  Since the address register is of type UW, we
             * can't use a D-type instruction.  In order to get around this,
             * re retype to UW and use a stride.
             */
            group_idx = retype(spread(group_idx, 2), BRW_REGISTER_TYPE_W);
         }

         uint32_t src_start_offset = src.nr * REG_SIZE + src.subnr;

         /* From the Haswell PRM:
          *
          *    "When a sequence of NoDDChk and NoDDClr are used, the last
          *    instruction that completes the scoreboard clear must have a
          *    non-zero execution mask. This means, if any kind of predication
          *    can change the execution mask or channel enable of the last
          *    instruction, the optimization must be avoided.  This is to
          *    avoid instructions being shot down the pipeline when no writes
          *    are required."
          *
          * Whenever predication is enabled or the instructions being emitted
          * aren't the full width, it's possible that it will be run with zero
          * channels enabled so we can't use dependency control without
          * running the risk of a hang if an instruction gets shot down.
          */
         const bool use_dep_ctrl = !inst->predicate &&
                                   lower_width == dispatch_width;
         brw_inst *insn;

         /* Due to a hardware bug some platforms (particularly Gfx11+) seem
          * to require the address components of all channels to be valid
          * whether or not they're active, which causes issues if we use VxH
          * addressing under non-uniform control-flow.  We can easily work
          * around that by initializing the whole address register with a
          * pipelined NoMask MOV instruction.
          */
         insn = brw_MOV(p, addr, brw_imm_uw(src_start_offset));
         brw_inst_set_mask_control(devinfo, insn, BRW_MASK_DISABLE);
         brw_inst_set_pred_control(devinfo, insn, BRW_PREDICATE_NONE);
         if (devinfo->ver >= 12)
            brw_set_default_swsb(p, tgl_swsb_null());
         else
            brw_inst_set_no_dd_clear(devinfo, insn, use_dep_ctrl);

         /* Take into account the component size and horizontal stride. */
         assert(src.vstride == src.hstride + src.width);
         insn = brw_SHL(p, addr, group_idx,
                        brw_imm_uw(util_logbase2(type_sz(src.type)) +
                                   src.hstride - 1));
         if (devinfo->ver >= 12)
            brw_set_default_swsb(p, tgl_swsb_regdist(1));
         else
            brw_inst_set_no_dd_check(devinfo, insn, use_dep_ctrl);

         /* Add on the register start offset */
         brw_ADD(p, addr, addr, brw_imm_uw(src_start_offset));
         brw_MOV(p, suboffset(dst, group << (dst.hstride - 1)),
                 retype(brw_VxH_indirect(0, 0), src.type));
      }

      brw_set_default_swsb(p, tgl_swsb_null());
   }
}

void
fs_generator::generate_quad_swizzle(const fs_inst *inst,
                                    struct brw_reg dst, struct brw_reg src,
                                    unsigned swiz)
{
   /* Requires a quad. */
   assert(inst->exec_size >= 4);

   if (src.file == BRW_IMMEDIATE_VALUE ||
       has_scalar_region(src)) {
      /* The value is uniform across all channels */
      brw_MOV(p, dst, src);

   } else if (devinfo->ver < 11 && type_sz(src.type) == 4) {
      /* This only works on 8-wide 32-bit values */
      assert(inst->exec_size == 8);
      assert(src.hstride == BRW_HORIZONTAL_STRIDE_1);
      assert(src.vstride == src.width + 1);
      brw_set_default_access_mode(p, BRW_ALIGN_16);
      struct brw_reg swiz_src = stride(src, 4, 4, 1);
      swiz_src.swizzle = swiz;
      brw_MOV(p, dst, swiz_src);

   } else {
      assert(src.hstride == BRW_HORIZONTAL_STRIDE_1);
      assert(src.vstride == src.width + 1);
      const struct brw_reg src_0 = suboffset(src, BRW_GET_SWZ(swiz, 0));

      switch (swiz) {
      case BRW_SWIZZLE_XXXX:
      case BRW_SWIZZLE_YYYY:
      case BRW_SWIZZLE_ZZZZ:
      case BRW_SWIZZLE_WWWW:
         brw_MOV(p, dst, stride(src_0, 4, 4, 0));
         break;

      case BRW_SWIZZLE_XXZZ:
      case BRW_SWIZZLE_YYWW:
         brw_MOV(p, dst, stride(src_0, 2, 2, 0));
         break;

      case BRW_SWIZZLE_XYXY:
      case BRW_SWIZZLE_ZWZW:
         assert(inst->exec_size == 4);
         brw_MOV(p, dst, stride(src_0, 0, 2, 1));
         break;

      default:
         assert(inst->force_writemask_all);
         brw_set_default_exec_size(p, cvt(inst->exec_size / 4) - 1);

         for (unsigned c = 0; c < 4; c++) {
            brw_inst *insn = brw_MOV(
               p, stride(suboffset(dst, c),
                         4 * inst->dst.stride, 1, 4 * inst->dst.stride),
               stride(suboffset(src, BRW_GET_SWZ(swiz, c)), 4, 1, 0));

            if (devinfo->ver < 12) {
               brw_inst_set_no_dd_clear(devinfo, insn, c < 3);
               brw_inst_set_no_dd_check(devinfo, insn, c > 0);
            }

            brw_set_default_swsb(p, tgl_swsb_null());
         }

         break;
      }
   }
}

void
fs_generator::generate_barrier(fs_inst *, struct brw_reg src)
{
   brw_barrier(p, src);
   if (devinfo->ver >= 12) {
      brw_set_default_swsb(p, tgl_swsb_null());
      brw_SYNC(p, TGL_SYNC_BAR);
   } else {
      brw_WAIT(p);
   }
}

/* For OPCODE_DDX and OPCODE_DDY, per channel of output we've got input
 * looking like:
 *
 * arg0: ss0.tl ss0.tr ss0.bl ss0.br ss1.tl ss1.tr ss1.bl ss1.br
 *
 * Ideally, we want to produce:
 *
 *           DDX                     DDY
 * dst: (ss0.tr - ss0.tl)     (ss0.tl - ss0.bl)
 *      (ss0.tr - ss0.tl)     (ss0.tr - ss0.br)
 *      (ss0.br - ss0.bl)     (ss0.tl - ss0.bl)
 *      (ss0.br - ss0.bl)     (ss0.tr - ss0.br)
 *      (ss1.tr - ss1.tl)     (ss1.tl - ss1.bl)
 *      (ss1.tr - ss1.tl)     (ss1.tr - ss1.br)
 *      (ss1.br - ss1.bl)     (ss1.tl - ss1.bl)
 *      (ss1.br - ss1.bl)     (ss1.tr - ss1.br)
 *
 * and add another set of two more subspans if in 16-pixel dispatch mode.
 *
 * For DDX, it ends up being easy: width = 2, horiz=0 gets us the same result
 * for each pair, and vertstride = 2 jumps us 2 elements after processing a
 * pair.  But the ideal approximation may impose a huge performance cost on
 * sample_d.  On at least Haswell, sample_d instruction does some
 * optimizations if the same LOD is used for all pixels in the subspan.
 *
 * For DDY, we need to use ALIGN16 mode since it's capable of doing the
 * appropriate swizzling.
 */
void
fs_generator::generate_ddx(const fs_inst *inst,
                           struct brw_reg dst, struct brw_reg src)
{
   unsigned vstride, width;

   if (inst->opcode == FS_OPCODE_DDX_FINE) {
      /* produce accurate derivatives */
      vstride = BRW_VERTICAL_STRIDE_2;
      width = BRW_WIDTH_2;
   } else {
      /* replicate the derivative at the top-left pixel to other pixels */
      vstride = BRW_VERTICAL_STRIDE_4;
      width = BRW_WIDTH_4;
   }

   struct brw_reg src0 = byte_offset(src, type_sz(src.type));;
   struct brw_reg src1 = src;

   src0.vstride = vstride;
   src0.width   = width;
   src0.hstride = BRW_HORIZONTAL_STRIDE_0;
   src1.vstride = vstride;
   src1.width   = width;
   src1.hstride = BRW_HORIZONTAL_STRIDE_0;

   brw_ADD(p, dst, src0, negate(src1));
}

/* The negate_value boolean is used to negate the derivative computation for
 * FBOs, since they place the origin at the upper left instead of the lower
 * left.
 */
void
fs_generator::generate_ddy(const fs_inst *inst,
                           struct brw_reg dst, struct brw_reg src)
{
   const uint32_t type_size = type_sz(src.type);

   if (inst->opcode == FS_OPCODE_DDY_FINE) {
      /* produce accurate derivatives.
       *
       * From the Broadwell PRM, Volume 7 (3D-Media-GPGPU)
       * "Register Region Restrictions", Section "1. Special Restrictions":
       *
       *    "In Align16 mode, the channel selects and channel enables apply to
       *     a pair of half-floats, because these parameters are defined for
       *     DWord elements ONLY. This is applicable when both source and
       *     destination are half-floats."
       *
       * So for half-float operations we use the Gfx11+ Align1 path. CHV
       * inherits its FP16 hardware from SKL, so it is not affected.
       */
      if (devinfo->ver >= 11) {
         src = stride(src, 0, 2, 1);

         brw_push_insn_state(p);
         brw_set_default_exec_size(p, BRW_EXECUTE_4);
         for (uint32_t g = 0; g < inst->exec_size; g += 4) {
            brw_set_default_group(p, inst->group + g);
            brw_ADD(p, byte_offset(dst, g * type_size),
                       negate(byte_offset(src,  g * type_size)),
                       byte_offset(src, (g + 2) * type_size));
            brw_set_default_swsb(p, tgl_swsb_null());
         }
         brw_pop_insn_state(p);
      } else {
         struct brw_reg src0 = stride(src, 4, 4, 1);
         struct brw_reg src1 = stride(src, 4, 4, 1);
         src0.swizzle = BRW_SWIZZLE_XYXY;
         src1.swizzle = BRW_SWIZZLE_ZWZW;

         brw_push_insn_state(p);
         brw_set_default_access_mode(p, BRW_ALIGN_16);
         brw_ADD(p, dst, negate(src0), src1);
         brw_pop_insn_state(p);
      }
   } else {
      /* replicate the derivative at the top-left pixel to other pixels */
      struct brw_reg src0 = byte_offset(stride(src, 4, 4, 0), 0 * type_size);
      struct brw_reg src1 = byte_offset(stride(src, 4, 4, 0), 2 * type_size);

      brw_ADD(p, dst, negate(src0), src1);
   }
}

void
fs_generator::generate_halt(fs_inst *)
{
   /* This HALT will be patched up at FB write time to point UIP at the end of
    * the program, and at brw_uip_jip() JIP will be set to the end of the
    * current block (or the program).
    */
   this->discard_halt_patches.push_tail(new(mem_ctx) ip_record(p->nr_insn));
   brw_HALT(p);
}

/* The A32 messages take a buffer base address in header.5:[31:0] (See
 * MH1_A32_PSM for typed messages or MH_A32_GO for byte/dword scattered
 * and OWord block messages in the SKL PRM Vol. 2d for more details.)
 * Unfortunately, there are a number of subtle differences:
 *
 * For the block read/write messages:
 *
 *   - We always stomp header.2 to fill in the actual scratch address (in
 *     units of OWORDs) so we don't care what's in there.
 *
 *   - They rely on per-thread scratch space value in header.3[3:0] to do
 *     bounds checking so that needs to be valid.  The upper bits of
 *     header.3 are ignored, though, so we can copy all of g0.3.
 *
 *   - They ignore header.5[9:0] and assumes the address is 1KB aligned.
 *
 *
 * For the byte/dword scattered read/write messages:
 *
 *   - We want header.2 to be zero because that gets added to the per-channel
 *     offset in the non-header portion of the message.
 *
 *   - Contrary to what the docs claim, they don't do any bounds checking so
 *     the value of header.3[3:0] doesn't matter.
 *
 *   - They consider all of header.5 for the base address and header.5[9:0]
 *     are not ignored.  This means that we can't copy g0.5 verbatim because
 *     g0.5[9:0] contains the FFTID on most platforms.  Instead, we have to
 *     use an AND to mask off the bottom 10 bits.
 *
 *
 * For block messages, just copying g0 gives a valid header because all the
 * garbage gets ignored except for header.2 which we stomp as part of message
 * setup.  For byte/dword scattered messages, we can just zero out the header
 * and copy over the bits we need from g0.5.  This opcode, however, tries to
 * satisfy the requirements of both by starting with 0 and filling out the
 * information required by either set of opcodes.
 */
void
fs_generator::generate_scratch_header(fs_inst *inst, struct brw_reg dst)
{
   assert(inst->exec_size == 8 && inst->force_writemask_all);
   assert(dst.file == BRW_GENERAL_REGISTER_FILE);

   dst.type = BRW_REGISTER_TYPE_UD;

   brw_inst *insn = brw_MOV(p, dst, brw_imm_ud(0));
   if (devinfo->ver >= 12)
      brw_set_default_swsb(p, tgl_swsb_null());
   else
      brw_inst_set_no_dd_clear(p->devinfo, insn, true);

   /* Copy the per-thread scratch space size from g0.3[3:0] */
   brw_set_default_exec_size(p, BRW_EXECUTE_1);
   insn = brw_AND(p, suboffset(dst, 3),
                     retype(brw_vec1_grf(0, 3), BRW_REGISTER_TYPE_UD),
                     brw_imm_ud(INTEL_MASK(3, 0)));
   if (devinfo->ver < 12) {
      brw_inst_set_no_dd_clear(p->devinfo, insn, true);
      brw_inst_set_no_dd_check(p->devinfo, insn, true);
   }

   /* Copy the scratch base address from g0.5[31:10] */
   insn = brw_AND(p, suboffset(dst, 5),
                     retype(brw_vec1_grf(0, 5), BRW_REGISTER_TYPE_UD),
                     brw_imm_ud(INTEL_MASK(31, 10)));
   if (devinfo->ver < 12)
      brw_inst_set_no_dd_check(p->devinfo, insn, true);
}

void
fs_generator::enable_debug(const char *shader_name)
{
   debug_flag = true;
   this->shader_name = shader_name;
}

static gfx12_systolic_depth
translate_systolic_depth(unsigned d)
{
   /* Could also return (ffs(d) - 1) & 3. */
   switch (d) {
   case 2:  return BRW_SYSTOLIC_DEPTH_2;
   case 4:  return BRW_SYSTOLIC_DEPTH_4;
   case 8:  return BRW_SYSTOLIC_DEPTH_8;
   case 16: return BRW_SYSTOLIC_DEPTH_16;
   default: unreachable("Invalid systolic depth.");
   }
}

int
fs_generator::generate_code(const cfg_t *cfg, int dispatch_width,
                            struct shader_stats shader_stats,
                            const brw::performance &perf,
                            struct brw_compile_stats *stats,
                            unsigned max_polygons)
{
   /* align to 64 byte boundary. */
   brw_realign(p, 64);

   this->dispatch_width = dispatch_width;

   int start_offset = p->next_insn_offset;

   int loop_count = 0, send_count = 0, nop_count = 0, sync_nop_count = 0;
   bool is_accum_used = false;

   struct disasm_info *disasm_info = disasm_initialize(p->isa, cfg);

   foreach_block_and_inst (block, fs_inst, inst, cfg) {
      if (inst->opcode == SHADER_OPCODE_UNDEF)
         continue;

      struct brw_reg src[4], dst;
      unsigned int last_insn_offset = p->next_insn_offset;
      bool multiple_instructions_emitted = false;
      tgl_swsb swsb = inst->sched;

      /* From the Broadwell PRM, Volume 7, "3D-Media-GPGPU", in the
       * "Register Region Restrictions" section: for BDW, SKL:
       *
       *    "A POW/FDIV operation must not be followed by an instruction
       *     that requires two destination registers."
       *
       * The documentation is often lacking annotations for Atom parts,
       * and empirically this affects CHV as well.
       */
      if (devinfo->ver <= 9 &&
          p->nr_insn > 1 &&
          brw_inst_opcode(p->isa, brw_last_inst) == BRW_OPCODE_MATH &&
          brw_inst_math_function(devinfo, brw_last_inst) == BRW_MATH_FUNCTION_POW &&
          inst->dst.component_size(inst->exec_size) > REG_SIZE) {
         brw_NOP(p);
         last_insn_offset = p->next_insn_offset;

         /* In order to avoid spurious instruction count differences when the
          * instruction schedule changes, keep track of the number of inserted
          * NOPs.
          */
         nop_count++;
      }

      /* Wa_14010017096:
       *
       * Clear accumulator register before end of thread.
       */
      if (inst->eot && is_accum_used &&
          intel_needs_workaround(devinfo, 14010017096)) {
         brw_set_default_exec_size(p, BRW_EXECUTE_16);
         brw_set_default_group(p, 0);
         brw_set_default_mask_control(p, BRW_MASK_DISABLE);
         brw_set_default_predicate_control(p, BRW_PREDICATE_NONE);
         brw_set_default_flag_reg(p, 0, 0);
         brw_set_default_swsb(p, tgl_swsb_src_dep(swsb));
         brw_MOV(p, brw_acc_reg(8), brw_imm_f(0.0f));
         last_insn_offset = p->next_insn_offset;
         swsb = tgl_swsb_dst_dep(swsb, 1);
      }

      if (!is_accum_used && !inst->eot) {
         is_accum_used = inst->writes_accumulator_implicitly(devinfo) ||
                         inst->dst.is_accumulator();
      }

      /* Wa_14013672992:
       *
       * Always use @1 SWSB for EOT.
       */
      if (inst->eot && intel_needs_workaround(devinfo, 14013672992)) {
         if (tgl_swsb_src_dep(swsb).mode) {
            brw_set_default_exec_size(p, BRW_EXECUTE_1);
            brw_set_default_mask_control(p, BRW_MASK_DISABLE);
            brw_set_default_predicate_control(p, BRW_PREDICATE_NONE);
            brw_set_default_flag_reg(p, 0, 0);
            brw_set_default_swsb(p, tgl_swsb_src_dep(swsb));
            brw_SYNC(p, TGL_SYNC_NOP);
            last_insn_offset = p->next_insn_offset;
         }

         swsb = tgl_swsb_dst_dep(swsb, 1);
      }

      if (unlikely(debug_flag))
         disasm_annotate(disasm_info, inst, p->next_insn_offset);

      if (devinfo->ver >= 20 && inst->group % 8 != 0) {
         assert(inst->force_writemask_all);
         assert(!inst->predicate && !inst->conditional_mod);
         assert(!inst->writes_accumulator_implicitly(devinfo) &&
                !inst->reads_accumulator_implicitly());
         assert(inst->opcode != SHADER_OPCODE_SEL_EXEC);
         brw_set_default_group(p, 0);
      } else {
         brw_set_default_group(p, inst->group);
      }

      for (unsigned int i = 0; i < inst->sources; i++) {
         src[i] = brw_reg_from_fs_reg(devinfo, inst, &inst->src[i]);
	 /* The accumulator result appears to get used for the
	  * conditional modifier generation.  When negating a UD
	  * value, there is a 33rd bit generated for the sign in the
	  * accumulator value, so now you can't check, for example,
	  * equality with a 32-bit value.  See piglit fs-op-neg-uvec4.
	  */
	 assert(!inst->conditional_mod ||
		inst->src[i].type != BRW_REGISTER_TYPE_UD ||
		!inst->src[i].negate);
      }
      dst = brw_reg_from_fs_reg(devinfo, inst, &inst->dst);

      brw_set_default_access_mode(p, BRW_ALIGN_1);
      brw_set_default_predicate_control(p, inst->predicate);
      brw_set_default_predicate_inverse(p, inst->predicate_inverse);
      /* On gfx7 and above, hardware automatically adds the group onto the
       * flag subregister number.
       */
      const unsigned flag_subreg = inst->flag_subreg;
      brw_set_default_flag_reg(p, flag_subreg / 2, flag_subreg % 2);
      brw_set_default_saturate(p, inst->saturate);
      brw_set_default_mask_control(p, inst->force_writemask_all);
      if (devinfo->ver >= 20 && inst->writes_accumulator) {
         assert(inst->dst.is_accumulator() ||
                inst->opcode == BRW_OPCODE_ADDC ||
                inst->opcode == BRW_OPCODE_MACH ||
                inst->opcode == BRW_OPCODE_SUBB);
      } else {
         brw_set_default_acc_write_control(p, inst->writes_accumulator);
      }
      brw_set_default_swsb(p, swsb);

      unsigned exec_size = inst->exec_size;

      brw_set_default_exec_size(p, cvt(exec_size) - 1);

      assert(inst->force_writemask_all || inst->exec_size >= 4);
      assert(inst->force_writemask_all || inst->group % inst->exec_size == 0);
      assert(inst->mlen <= BRW_MAX_MSG_LENGTH * reg_unit(devinfo));

      switch (inst->opcode) {
      case BRW_OPCODE_SYNC:
         assert(src[0].file == BRW_IMMEDIATE_VALUE);
         brw_SYNC(p, tgl_sync_function(src[0].ud));

         if (tgl_sync_function(src[0].ud) == TGL_SYNC_NOP)
            ++sync_nop_count;

         break;
      case BRW_OPCODE_MOV:
	 brw_MOV(p, dst, src[0]);
	 break;
      case BRW_OPCODE_ADD:
	 brw_ADD(p, dst, src[0], src[1]);
	 break;
      case BRW_OPCODE_MUL:
	 brw_MUL(p, dst, src[0], src[1]);
	 break;
      case BRW_OPCODE_AVG:
	 brw_AVG(p, dst, src[0], src[1]);
	 break;
      case BRW_OPCODE_MACH:
	 brw_MACH(p, dst, src[0], src[1]);
	 break;

      case BRW_OPCODE_DP4A:
         assert(devinfo->ver >= 12);
         brw_DP4A(p, dst, src[0], src[1], src[2]);
         break;

      case BRW_OPCODE_LINE:
         brw_LINE(p, dst, src[0], src[1]);
         break;

      case BRW_OPCODE_DPAS:
         assert(devinfo->verx10 >= 125);
         brw_DPAS(p, translate_systolic_depth(inst->sdepth), inst->rcount,
                  dst, src[0], src[1], src[2]);
         break;

      case BRW_OPCODE_MAD:
         if (devinfo->ver < 10)
            brw_set_default_access_mode(p, BRW_ALIGN_16);
         brw_MAD(p, dst, src[0], src[1], src[2]);
	 break;

      case BRW_OPCODE_LRP:
         assert(devinfo->ver <= 10);
         if (devinfo->ver < 10)
            brw_set_default_access_mode(p, BRW_ALIGN_16);
         brw_LRP(p, dst, src[0], src[1], src[2]);
	 break;

      case BRW_OPCODE_ADD3:
         assert(devinfo->verx10 >= 125);
         brw_ADD3(p, dst, src[0], src[1], src[2]);
         break;

      case BRW_OPCODE_FRC:
	 brw_FRC(p, dst, src[0]);
	 break;
      case BRW_OPCODE_RNDD:
	 brw_RNDD(p, dst, src[0]);
	 break;
      case BRW_OPCODE_RNDE:
	 brw_RNDE(p, dst, src[0]);
	 break;
      case BRW_OPCODE_RNDZ:
	 brw_RNDZ(p, dst, src[0]);
	 break;

      case BRW_OPCODE_AND:
	 brw_AND(p, dst, src[0], src[1]);
	 break;
      case BRW_OPCODE_OR:
	 brw_OR(p, dst, src[0], src[1]);
	 break;
      case BRW_OPCODE_XOR:
	 brw_XOR(p, dst, src[0], src[1]);
	 break;
      case BRW_OPCODE_NOT:
	 brw_NOT(p, dst, src[0]);
	 break;
      case BRW_OPCODE_ASR:
	 brw_ASR(p, dst, src[0], src[1]);
	 break;
      case BRW_OPCODE_SHR:
	 brw_SHR(p, dst, src[0], src[1]);
	 break;
      case BRW_OPCODE_SHL:
	 brw_SHL(p, dst, src[0], src[1]);
	 break;
      case BRW_OPCODE_ROL:
	 assert(devinfo->ver >= 11);
	 assert(src[0].type == dst.type);
	 brw_ROL(p, dst, src[0], src[1]);
	 break;
      case BRW_OPCODE_ROR:
	 assert(devinfo->ver >= 11);
	 assert(src[0].type == dst.type);
	 brw_ROR(p, dst, src[0], src[1]);
	 break;
      case BRW_OPCODE_CMP:
         brw_CMP(p, dst, inst->conditional_mod, src[0], src[1]);
	 break;
      case BRW_OPCODE_CMPN:
         brw_CMPN(p, dst, inst->conditional_mod, src[0], src[1]);
         break;
      case BRW_OPCODE_SEL:
	 brw_SEL(p, dst, src[0], src[1]);
	 break;
      case BRW_OPCODE_CSEL:
         if (devinfo->ver < 10)
            brw_set_default_access_mode(p, BRW_ALIGN_16);
         brw_CSEL(p, dst, src[0], src[1], src[2]);
         break;
      case BRW_OPCODE_BFREV:
         brw_BFREV(p, retype(dst, BRW_REGISTER_TYPE_UD),
                   retype(src[0], BRW_REGISTER_TYPE_UD));
         break;
      case BRW_OPCODE_FBH:
         brw_FBH(p, retype(dst, src[0].type), src[0]);
         break;
      case BRW_OPCODE_FBL:
         brw_FBL(p, retype(dst, BRW_REGISTER_TYPE_UD),
                 retype(src[0], BRW_REGISTER_TYPE_UD));
         break;
      case BRW_OPCODE_LZD:
         brw_LZD(p, dst, src[0]);
         break;
      case BRW_OPCODE_CBIT:
         brw_CBIT(p, retype(dst, BRW_REGISTER_TYPE_UD),
                  retype(src[0], BRW_REGISTER_TYPE_UD));
         break;
      case BRW_OPCODE_ADDC:
         brw_ADDC(p, dst, src[0], src[1]);
         break;
      case BRW_OPCODE_SUBB:
         brw_SUBB(p, dst, src[0], src[1]);
         break;
      case BRW_OPCODE_MAC:
         brw_MAC(p, dst, src[0], src[1]);
         break;

      case BRW_OPCODE_BFE:
         if (devinfo->ver < 10)
            brw_set_default_access_mode(p, BRW_ALIGN_16);
         brw_BFE(p, dst, src[0], src[1], src[2]);
         break;

      case BRW_OPCODE_BFI1:
         brw_BFI1(p, dst, src[0], src[1]);
         break;
      case BRW_OPCODE_BFI2:
         if (devinfo->ver < 10)
            brw_set_default_access_mode(p, BRW_ALIGN_16);
         brw_BFI2(p, dst, src[0], src[1], src[2]);
         break;

      case BRW_OPCODE_IF:
         brw_IF(p, brw_get_default_exec_size(p));
	 break;

      case BRW_OPCODE_ELSE:
	 brw_ELSE(p);
	 break;
      case BRW_OPCODE_ENDIF:
	 brw_ENDIF(p);
	 break;

      case BRW_OPCODE_DO:
	 brw_DO(p, brw_get_default_exec_size(p));
	 break;

      case BRW_OPCODE_BREAK:
	 brw_BREAK(p);
	 break;
      case BRW_OPCODE_CONTINUE:
         brw_CONT(p);
	 break;

      case BRW_OPCODE_WHILE:
	 brw_WHILE(p);
         loop_count++;
	 break;

      case SHADER_OPCODE_RCP:
      case SHADER_OPCODE_RSQ:
      case SHADER_OPCODE_SQRT:
      case SHADER_OPCODE_EXP2:
      case SHADER_OPCODE_LOG2:
      case SHADER_OPCODE_SIN:
      case SHADER_OPCODE_COS:
         assert(inst->conditional_mod == BRW_CONDITIONAL_NONE);
         assert(inst->mlen == 0);
         gfx6_math(p, dst, brw_math_function(inst->opcode),
                   src[0], brw_null_reg());
	 break;
      case SHADER_OPCODE_INT_QUOTIENT:
      case SHADER_OPCODE_INT_REMAINDER:
      case SHADER_OPCODE_POW:
         assert(devinfo->verx10 < 125);
         assert(inst->conditional_mod == BRW_CONDITIONAL_NONE);
         assert(inst->mlen == 0);
         assert(inst->opcode == SHADER_OPCODE_POW || inst->exec_size == 8);
         gfx6_math(p, dst, brw_math_function(inst->opcode), src[0], src[1]);
	 break;
      case BRW_OPCODE_PLN:
         /* PLN reads:
          *                      /   in SIMD16   \
          *    -----------------------------------
          *   | src1+0 | src1+1 | src1+2 | src1+3 |
          *   |-----------------------------------|
          *   |(x0, x1)|(y0, y1)|(x2, x3)|(y2, y3)|
          *    -----------------------------------
          */
         brw_PLN(p, dst, src[0], src[1]);
	 break;
      case FS_OPCODE_PIXEL_X:
         assert(src[0].type == BRW_REGISTER_TYPE_UW);
         assert(src[1].type == BRW_REGISTER_TYPE_UW);
         src[0].subnr = 0 * type_sz(src[0].type);
         if (src[1].file == BRW_IMMEDIATE_VALUE) {
            assert(src[1].ud == 0);
            brw_MOV(p, dst, stride(src[0], 8, 4, 1));
         } else {
            /* Coarse pixel case */
            brw_ADD(p, dst, stride(src[0], 8, 4, 1), src[1]);
         }
         break;
      case FS_OPCODE_PIXEL_Y:
         assert(src[0].type == BRW_REGISTER_TYPE_UW);
         assert(src[1].type == BRW_REGISTER_TYPE_UW);
         src[0].subnr = 4 * type_sz(src[0].type);
         if (src[1].file == BRW_IMMEDIATE_VALUE) {
            assert(src[1].ud == 0);
            brw_MOV(p, dst, stride(src[0], 8, 4, 1));
         } else {
            /* Coarse pixel case */
            brw_ADD(p, dst, stride(src[0], 8, 4, 1), src[1]);
         }
         break;

      case SHADER_OPCODE_SEND:
         generate_send(inst, dst, src[0], src[1], src[2],
                       inst->ex_mlen > 0 ? src[3] : brw_null_reg());
         send_count++;
         break;

      case FS_OPCODE_DDX_COARSE:
      case FS_OPCODE_DDX_FINE:
         generate_ddx(inst, dst, src[0]);
         break;
      case FS_OPCODE_DDY_COARSE:
      case FS_OPCODE_DDY_FINE:
         generate_ddy(inst, dst, src[0]);
	 break;

      case SHADER_OPCODE_SCRATCH_HEADER:
         generate_scratch_header(inst, dst);
         break;

      case SHADER_OPCODE_MOV_INDIRECT:
         generate_mov_indirect(inst, dst, src[0], src[1]);
         break;

      case SHADER_OPCODE_MOV_RELOC_IMM:
         assert(src[0].file == BRW_IMMEDIATE_VALUE);
         brw_MOV_reloc_imm(p, dst, dst.type, src[0].ud);
         break;

      case BRW_OPCODE_HALT:
         generate_halt(inst);
         break;

      case SHADER_OPCODE_INTERLOCK:
      case SHADER_OPCODE_MEMORY_FENCE: {
         assert(src[1].file == BRW_IMMEDIATE_VALUE);
         assert(src[2].file == BRW_IMMEDIATE_VALUE);

         const enum opcode send_op = inst->opcode == SHADER_OPCODE_INTERLOCK ?
            BRW_OPCODE_SENDC : BRW_OPCODE_SEND;

         brw_memory_fence(p, dst, src[0], send_op,
                          brw_message_target(inst->sfid),
                          inst->desc,
                          /* commit_enable */ src[1].ud,
                          /* bti */ src[2].ud);
         send_count++;
         break;
      }

      case FS_OPCODE_SCHEDULING_FENCE:
         if (inst->sources == 0 && swsb.regdist == 0 &&
                                   swsb.mode == TGL_SBID_NULL) {
            if (unlikely(debug_flag))
               disasm_info->use_tail = true;
            break;
         }

         if (devinfo->ver >= 12) {
            /* Use the available SWSB information to stall.  A single SYNC is
             * sufficient since if there were multiple dependencies, the
             * scoreboard algorithm already injected other SYNCs before this
             * instruction.
             */
            brw_SYNC(p, TGL_SYNC_NOP);
         } else {
            for (unsigned i = 0; i < inst->sources; i++) {
               /* Emit a MOV to force a stall until the instruction producing the
                * registers finishes.
                */
               brw_MOV(p, retype(brw_null_reg(), BRW_REGISTER_TYPE_UW),
                       retype(src[i], BRW_REGISTER_TYPE_UW));
            }

            if (inst->sources > 1)
               multiple_instructions_emitted = true;
         }

         break;

      case SHADER_OPCODE_FIND_LIVE_CHANNEL:
      case SHADER_OPCODE_FIND_LAST_LIVE_CHANNEL:
      case SHADER_OPCODE_LOAD_LIVE_CHANNELS:
         unreachable("Should be lowered by lower_find_live_channel()");
         break;

      case FS_OPCODE_LOAD_LIVE_CHANNELS: {
         assert(inst->force_writemask_all && inst->group == 0);
         assert(inst->dst.file == BAD_FILE);
         brw_set_default_exec_size(p, BRW_EXECUTE_1);
         brw_MOV(p, retype(brw_flag_subreg(inst->flag_subreg),
                           BRW_REGISTER_TYPE_UD),
                 retype(brw_mask_reg(0), BRW_REGISTER_TYPE_UD));
         break;
      }
      case SHADER_OPCODE_BROADCAST:
         assert(inst->force_writemask_all);
         brw_broadcast(p, dst, src[0], src[1]);
         break;

      case SHADER_OPCODE_SHUFFLE:
         generate_shuffle(inst, dst, src[0], src[1]);
         break;

      case SHADER_OPCODE_SEL_EXEC:
         assert(inst->force_writemask_all);
         assert(devinfo->has_64bit_float || type_sz(dst.type) <= 4);
         brw_set_default_mask_control(p, BRW_MASK_DISABLE);
         brw_MOV(p, dst, src[1]);
         brw_set_default_mask_control(p, BRW_MASK_ENABLE);
         brw_set_default_swsb(p, tgl_swsb_null());
         brw_MOV(p, dst, src[0]);
         break;

      case SHADER_OPCODE_QUAD_SWIZZLE:
         assert(src[1].file == BRW_IMMEDIATE_VALUE);
         assert(src[1].type == BRW_REGISTER_TYPE_UD);
         generate_quad_swizzle(inst, dst, src[0], src[1].ud);
         break;

      case SHADER_OPCODE_CLUSTER_BROADCAST: {
         assert((!intel_device_info_is_9lp(devinfo) &&
                 devinfo->has_64bit_float) || type_sz(src[0].type) <= 4);
         assert(!src[0].negate && !src[0].abs);
         assert(src[1].file == BRW_IMMEDIATE_VALUE);
         assert(src[1].type == BRW_REGISTER_TYPE_UD);
         assert(src[2].file == BRW_IMMEDIATE_VALUE);
         assert(src[2].type == BRW_REGISTER_TYPE_UD);
         const unsigned component = src[1].ud;
         const unsigned cluster_size = src[2].ud;
         assert(inst->src[0].file != ARF);

         unsigned s;
         if (inst->src[0].file == FIXED_GRF) {
            s = inst->src[0].hstride ? 1 << (inst->src[0].hstride - 1) : 0;
         } else {
            s = inst->src[0].stride;
         }
         unsigned vstride = cluster_size * s;
         unsigned width = cluster_size;

         /* The maximum exec_size is 32, but the maximum width is only 16. */
         if (inst->exec_size == width) {
            vstride = 0;
            width = 1;
         }

         struct brw_reg strided = stride(suboffset(src[0], component * s),
                                         vstride, width, 0);
         brw_MOV(p, dst, strided);
         break;
      }

      case SHADER_OPCODE_HALT_TARGET:
         /* This is the place where the final HALT needs to be inserted if
          * we've emitted any discards.  If not, this will emit no code.
          */
         if (!patch_halt_jumps()) {
            if (unlikely(debug_flag)) {
               disasm_info->use_tail = true;
            }
         }
         break;

      case SHADER_OPCODE_BARRIER:
	 generate_barrier(inst, src[0]);
         send_count++;
	 break;

      case SHADER_OPCODE_RND_MODE: {
         assert(src[0].file == BRW_IMMEDIATE_VALUE);
         /*
          * Changes the floating point rounding mode updating the control
          * register field defined at cr0.0[5-6] bits.
          */
         enum brw_rnd_mode mode =
            (enum brw_rnd_mode) (src[0].d << BRW_CR0_RND_MODE_SHIFT);
         brw_float_controls_mode(p, mode, BRW_CR0_RND_MODE_MASK);
      }
         break;

      case SHADER_OPCODE_FLOAT_CONTROL_MODE:
         assert(src[0].file == BRW_IMMEDIATE_VALUE);
         assert(src[1].file == BRW_IMMEDIATE_VALUE);
         brw_float_controls_mode(p, src[0].d, src[1].d);
         break;

      case SHADER_OPCODE_READ_ARCH_REG:
         if (devinfo->ver >= 12) {
            /* There is a SWSB restriction that requires that any time sr0 is
             * accessed both the instruction doing the access and the next one
             * have SWSB set to RegDist(1).
             */
            if (brw_get_default_swsb(p).mode != TGL_SBID_NULL)
               brw_SYNC(p, TGL_SYNC_NOP);
            brw_set_default_swsb(p, tgl_swsb_regdist(1));
            brw_MOV(p, dst, src[0]);
            brw_set_default_swsb(p, tgl_swsb_regdist(1));
            brw_AND(p, dst, dst, brw_imm_ud(0xffffffff));
         } else {
            brw_MOV(p, dst, src[0]);
         }
         break;

      default:
         unreachable("Unsupported opcode");

      case SHADER_OPCODE_LOAD_PAYLOAD:
         unreachable("Should be lowered by lower_load_payload()");
      }

      if (multiple_instructions_emitted)
         continue;

      if (inst->no_dd_clear || inst->no_dd_check || inst->conditional_mod) {
         assert(p->next_insn_offset == last_insn_offset + 16 ||
                !"conditional_mod, no_dd_check, or no_dd_clear set for IR "
                 "emitting more than 1 instruction");

         brw_inst *last = &p->store[last_insn_offset / 16];

         if (inst->conditional_mod)
            brw_inst_set_cond_modifier(p->devinfo, last, inst->conditional_mod);
         if (devinfo->ver < 12) {
            brw_inst_set_no_dd_clear(p->devinfo, last, inst->no_dd_clear);
            brw_inst_set_no_dd_check(p->devinfo, last, inst->no_dd_check);
         }
      }

      /* When enabled, insert sync NOP after every instruction and make sure
       * that current instruction depends on the previous instruction.
       */
      if (INTEL_DEBUG(DEBUG_SWSB_STALL) && devinfo->ver >= 12) {
         brw_set_default_swsb(p, tgl_swsb_regdist(1));
         brw_SYNC(p, TGL_SYNC_NOP);
      }
   }

   brw_set_uip_jip(p, start_offset);

   /* end of program sentinel */
   disasm_new_inst_group(disasm_info, p->next_insn_offset);

   /* `send_count` explicitly does not include spills or fills, as we'd
    * like to use it as a metric for intentional memory access or other
    * shared function use.  Otherwise, subtle changes to scheduling or
    * register allocation could cause it to fluctuate wildly - and that
    * effect is already counted in spill/fill counts.
    */
   send_count -= shader_stats.spill_count;
   send_count -= shader_stats.fill_count;

#ifndef NDEBUG
   bool validated =
#else
   if (unlikely(debug_flag))
#endif
      brw_validate_instructions(&compiler->isa, p->store,
                                start_offset,
                                p->next_insn_offset,
                                disasm_info);

   int before_size = p->next_insn_offset - start_offset;
   brw_compact_instructions(p, start_offset, disasm_info);
   int after_size = p->next_insn_offset - start_offset;

   bool dump_shader_bin = brw_should_dump_shader_bin();
   unsigned char sha1[21];
   char sha1buf[41];

   if (unlikely(debug_flag || dump_shader_bin)) {
      _mesa_sha1_compute(p->store + start_offset / sizeof(brw_inst),
                         after_size, sha1);
      _mesa_sha1_format(sha1buf, sha1);
   }

   if (unlikely(dump_shader_bin))
      brw_dump_shader_bin(p->store, start_offset, p->next_insn_offset,
                          sha1buf);

   if (unlikely(debug_flag)) {
      fprintf(stderr, "Native code for %s (src_hash 0x%08x) (sha1 %s)\n"
              "SIMD%d shader: %d instructions. %d loops. %u cycles. "
              "%d:%d spills:fills, %u sends, "
              "scheduled with mode %s. "
              "Promoted %u constants. "
              "Compacted %d to %d bytes (%.0f%%)\n",
              shader_name, params->source_hash, sha1buf,
              dispatch_width, before_size / 16,
              loop_count, perf.latency,
              shader_stats.spill_count,
              shader_stats.fill_count,
              send_count,
              shader_stats.scheduler_mode,
              shader_stats.promoted_constants,
              before_size, after_size,
              100.0f * (before_size - after_size) / before_size);

      /* overriding the shader makes disasm_info invalid */
      if (!brw_try_override_assembly(p, start_offset, sha1buf)) {
         dump_assembly(p->store, start_offset, p->next_insn_offset,
                       disasm_info, perf.block_latency);
      } else {
         fprintf(stderr, "Successfully overrode shader with sha1 %s\n\n", sha1buf);
      }
   }
   ralloc_free(disasm_info);
#ifndef NDEBUG
   if (!validated && !debug_flag) {
      fprintf(stderr,
            "Validation failed. Rerun with INTEL_DEBUG=shaders to get more information.\n");
   }
#endif
   assert(validated);

   brw_shader_debug_log(compiler, params->log_data,
                        "%s SIMD%d shader: %d inst, %d loops, %u cycles, "
                        "%d:%d spills:fills, %u sends, "
                        "scheduled with mode %s, "
                        "Promoted %u constants, "
                        "compacted %d to %d bytes.\n",
                        _mesa_shader_stage_to_abbrev(stage),
                        dispatch_width,
                        before_size / 16 - nop_count - sync_nop_count,
                        loop_count, perf.latency,
                        shader_stats.spill_count,
                        shader_stats.fill_count,
                        send_count,
                        shader_stats.scheduler_mode,
                        shader_stats.promoted_constants,
                        before_size, after_size);
   if (stats) {
      stats->dispatch_width = dispatch_width;
      stats->max_polygons = max_polygons;
      stats->max_dispatch_width = dispatch_width;
      stats->instructions = before_size / 16 - nop_count - sync_nop_count;
      stats->sends = send_count;
      stats->loops = loop_count;
      stats->cycles = perf.latency;
      stats->spills = shader_stats.spill_count;
      stats->fills = shader_stats.fill_count;
      stats->max_live_registers = shader_stats.max_register_pressure;
   }

   return start_offset;
}

void
fs_generator::add_const_data(void *data, unsigned size)
{
   assert(prog_data->const_data_size == 0);
   if (size > 0) {
      prog_data->const_data_size = size;
      prog_data->const_data_offset = brw_append_data(p, data, size, 32);
   }
}

void
fs_generator::add_resume_sbt(unsigned num_resume_shaders, uint64_t *sbt)
{
   assert(brw_shader_stage_is_bindless(stage));
   struct brw_bs_prog_data *bs_prog_data = brw_bs_prog_data(prog_data);
   if (num_resume_shaders > 0) {
      bs_prog_data->resume_sbt_offset =
         brw_append_data(p, sbt, num_resume_shaders * sizeof(uint64_t), 32);
      for (unsigned i = 0; i < num_resume_shaders; i++) {
         size_t offset = bs_prog_data->resume_sbt_offset + i * sizeof(*sbt);
         assert(offset <= UINT32_MAX);
         brw_add_reloc(p, BRW_SHADER_RELOC_SHADER_START_OFFSET,
                       BRW_SHADER_RELOC_TYPE_U32,
                       (uint32_t)offset, (uint32_t)sbt[i]);
      }
   }
}

const unsigned *
fs_generator::get_assembly()
{
   prog_data->relocs = brw_get_shader_relocs(p, &prog_data->num_relocs);

   return brw_get_program(p, &prog_data->program_size);
}
