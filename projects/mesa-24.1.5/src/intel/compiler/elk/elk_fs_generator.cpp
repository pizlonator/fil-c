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

/** @file elk_fs_generator.cpp
 *
 * This file supports generating code from the FS LIR to the actual
 * native instructions.
 */

#include "elk_eu.h"
#include "elk_disasm_info.h"
#include "elk_fs.h"
#include "elk_cfg.h"
#include "dev/intel_debug.h"
#include "util/mesa-sha1.h"
#include "util/half_float.h"

static enum elk_reg_file
elk_file_from_reg(elk_fs_reg *reg)
{
   switch (reg->file) {
   case ARF:
      return ELK_ARCHITECTURE_REGISTER_FILE;
   case FIXED_GRF:
   case VGRF:
      return ELK_GENERAL_REGISTER_FILE;
   case MRF:
      return ELK_MESSAGE_REGISTER_FILE;
   case IMM:
      return ELK_IMMEDIATE_VALUE;
   case BAD_FILE:
   case ATTR:
   case UNIFORM:
      unreachable("not reached");
   }
   return ELK_ARCHITECTURE_REGISTER_FILE;
}

static struct elk_reg
elk_reg_from_fs_reg(const struct intel_device_info *devinfo, elk_fs_inst *inst,
                    elk_fs_reg *reg, bool compressed)
{
   struct elk_reg elk_reg;

   switch (reg->file) {
   case MRF:
      assert((reg->nr & ~ELK_MRF_COMPR4) < ELK_MAX_MRF(devinfo->ver));
      FALLTHROUGH;
   case VGRF:
      if (reg->stride == 0) {
         elk_reg = elk_vec1_reg(elk_file_from_reg(reg), reg->nr, 0);
      } else {
         /* From the Haswell PRM:
          *
          *  "VertStride must be used to cross GRF register boundaries. This
          *   rule implies that elements within a 'Width' cannot cross GRF
          *   boundaries."
          *
          * The maximum width value that could satisfy this restriction is:
          */
         const unsigned reg_width = REG_SIZE / (reg->stride * type_sz(reg->type));

         /* Because the hardware can only split source regions at a whole
          * multiple of width during decompression (i.e. vertically), clamp
          * the value obtained above to the physical execution size of a
          * single decompressed chunk of the instruction:
          */
         const unsigned phys_width = compressed ? inst->exec_size / 2 :
                                     inst->exec_size;

         const unsigned max_hw_width = 16;

         /* XXX - The equation above is strictly speaking not correct on
          *       hardware that supports unbalanced GRF writes -- On Gfx9+
          *       each decompressed chunk of the instruction may have a
          *       different execution size when the number of components
          *       written to each destination GRF is not the same.
          */
         if (reg->stride > 4) {
            assert(reg != &inst->dst);
            assert(reg->stride * type_sz(reg->type) <= REG_SIZE);
            elk_reg = elk_vecn_reg(1, elk_file_from_reg(reg), reg->nr, 0);
            elk_reg = stride(elk_reg, reg->stride, 1, 0);
         } else {
            const unsigned width = MIN3(reg_width, phys_width, max_hw_width);
            elk_reg = elk_vecn_reg(width, elk_file_from_reg(reg), reg->nr, 0);
            elk_reg = stride(elk_reg, width * reg->stride, width, reg->stride);
         }

         if (devinfo->verx10 == 70) {
            /* From the IvyBridge PRM (EU Changes by Processor Generation, page 13):
             *  "Each DF (Double Float) operand uses an element size of 4 rather
             *   than 8 and all regioning parameters are twice what the values
             *   would be based on the true element size: ExecSize, Width,
             *   HorzStride, and VertStride. Each DF operand uses a pair of
             *   channels and all masking and swizzing should be adjusted
             *   appropriately."
             *
             * From the IvyBridge PRM (Special Requirements for Handling Double
             * Precision Data Types, page 71):
             *  "In Align1 mode, all regioning parameters like stride, execution
             *   size, and width must use the syntax of a pair of packed
             *   floats. The offsets for these data types must be 64-bit
             *   aligned. The execution size and regioning parameters are in terms
             *   of floats."
             *
             * Summarized: when handling DF-typed arguments, ExecSize,
             * VertStride, and Width must be doubled.
             *
             * It applies to BayTrail too.
             */
            if (type_sz(reg->type) == 8) {
               elk_reg.width++;
               if (elk_reg.vstride > 0)
                  elk_reg.vstride++;
               assert(elk_reg.hstride == ELK_HORIZONTAL_STRIDE_1);
            }

            /* When converting from DF->F, we set the destination stride to 2
             * because each d2f conversion implicitly writes 2 floats, being
             * the first one the converted value. IVB/BYT actually writes two
             * F components per SIMD channel, and every other component is
             * filled with garbage.
             */
            if (reg == &inst->dst && get_exec_type_size(inst) == 8 &&
                type_sz(inst->dst.type) < 8) {
               assert(elk_reg.hstride > ELK_HORIZONTAL_STRIDE_1);
               elk_reg.hstride--;
            }
         }
      }

      elk_reg = retype(elk_reg, reg->type);
      elk_reg = byte_offset(elk_reg, reg->offset);
      elk_reg.abs = reg->abs;
      elk_reg.negate = reg->negate;
      break;
   case ARF:
   case FIXED_GRF:
   case IMM:
      assert(reg->offset == 0);
      elk_reg = reg->as_elk_reg();
      break;
   case BAD_FILE:
      /* Probably unused. */
      elk_reg = elk_null_reg();
      break;
   case ATTR:
   case UNIFORM:
      unreachable("not reached");
   }

   /* On HSW+, scalar DF sources can be accessed using the normal <0,1,0>
    * region, but on IVB and BYT DF regions must be programmed in terms of
    * floats. A <0,2,1> region accomplishes this.
    */
   if (devinfo->verx10 == 70 &&
       type_sz(reg->type) == 8 &&
       elk_reg.vstride == ELK_VERTICAL_STRIDE_0 &&
       elk_reg.width == ELK_WIDTH_1 &&
       elk_reg.hstride == ELK_HORIZONTAL_STRIDE_0) {
      elk_reg.width = ELK_WIDTH_2;
      elk_reg.hstride = ELK_HORIZONTAL_STRIDE_1;
   }

   return elk_reg;
}

elk_fs_generator::elk_fs_generator(const struct elk_compiler *compiler,
                           const struct elk_compile_params *params,
                           struct elk_stage_prog_data *prog_data,
                           bool runtime_check_aads_emit,
                           gl_shader_stage stage)

   : compiler(compiler), params(params),
     devinfo(compiler->devinfo),
     prog_data(prog_data), dispatch_width(0),
     runtime_check_aads_emit(runtime_check_aads_emit), debug_flag(false),
     shader_name(NULL), stage(stage), mem_ctx(params->mem_ctx)
{
   p = rzalloc(mem_ctx, struct elk_codegen);
   elk_init_codegen(&compiler->isa, p, mem_ctx);

   /* In the FS code generator, we are very careful to ensure that we always
    * set the right execution size so we don't need the EU code to "help" us
    * by trying to infer it.  Sometimes, it infers the wrong thing.
    */
   p->automatic_exec_sizes = false;
}

elk_fs_generator::~elk_fs_generator()
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
elk_fs_generator::patch_halt_jumps()
{
   if (this->discard_halt_patches.is_empty())
      return false;

   int scale = elk_jump_scale(p->devinfo);

   if (devinfo->ver >= 6) {
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
      elk_inst *last_halt = elk_HALT(p);
      elk_inst_set_uip(p->devinfo, last_halt, 1 * scale);
      elk_inst_set_jip(p->devinfo, last_halt, 1 * scale);
   }

   int ip = p->nr_insn;

   foreach_in_list(ip_record, patch_ip, &discard_halt_patches) {
      elk_inst *patch = &p->store[patch_ip->ip];

      assert(elk_inst_opcode(p->isa, patch) == ELK_OPCODE_HALT);
      if (devinfo->ver >= 6) {
         /* HALT takes a half-instruction distance from the pre-incremented IP. */
         elk_inst_set_uip(p->devinfo, patch, (ip - patch_ip->ip) * scale);
      } else {
         elk_set_src1(p, patch, elk_imm_d((ip - patch_ip->ip) * scale));
      }
   }

   this->discard_halt_patches.make_empty();

   if (devinfo->ver < 6) {
      /* From the g965 PRM:
       *
       *    "As DMask is not automatically reloaded into AMask upon completion
       *    of this instruction, software has to manually restore AMask upon
       *    completion."
       *
       * DMask lives in the bottom 16 bits of sr0.1.
       */
      elk_inst *reset = elk_MOV(p, elk_mask_reg(ELK_AMASK),
                                   retype(elk_sr0_reg(1), ELK_REGISTER_TYPE_UW));
      elk_inst_set_exec_size(devinfo, reset, ELK_EXECUTE_1);
      elk_inst_set_mask_control(devinfo, reset, ELK_MASK_DISABLE);
      elk_inst_set_qtr_control(devinfo, reset, ELK_COMPRESSION_NONE);
      elk_inst_set_thread_control(devinfo, reset, ELK_THREAD_SWITCH);
   }

   if (devinfo->ver == 4 && devinfo->platform != INTEL_PLATFORM_G4X) {
      /* From the g965 PRM:
       *
       *    "[DevBW, DevCL] Erratum: The subfields in mask stack register are
       *    reset to zero during graphics reset, however, they are not
       *    initialized at thread dispatch. These subfields will retain the
       *    values from the previous thread. Software should make sure the
       *    mask stack is empty (reset to zero) before terminating the thread.
       *    In case that this is not practical, software may have to reset the
       *    mask stack at the beginning of each kernel, which will impact the
       *    performance."
       *
       * Luckily we can rely on:
       *
       *    "[DevBW, DevCL] This register access restriction is not
       *    applicable, hardware does ensure execution pipeline coherency,
       *    when a mask stack register is used as an explicit source and/or
       *    destination."
       */
      elk_push_insn_state(p);
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);
      elk_set_default_compression_control(p, ELK_COMPRESSION_NONE);

      elk_set_default_exec_size(p, ELK_EXECUTE_2);
      elk_MOV(p, vec2(elk_mask_stack_depth_reg(0)), elk_imm_uw(0));

      elk_set_default_exec_size(p, ELK_EXECUTE_16);
      /* Reset the if stack. */
      elk_MOV(p, retype(elk_mask_stack_reg(0), ELK_REGISTER_TYPE_UW),
              elk_imm_uw(0));

      elk_pop_insn_state(p);
   }

   return true;
}

void
elk_fs_generator::generate_send(elk_fs_inst *inst,
                                struct elk_reg dst,
                                struct elk_reg desc,
                                struct elk_reg payload)
{
   const bool dst_is_null = dst.file == ELK_ARCHITECTURE_REGISTER_FILE &&
                            dst.nr == ELK_ARF_NULL;
   const unsigned rlen = dst_is_null ? 0 : inst->size_written / REG_SIZE;

   uint32_t desc_imm = inst->desc |
      elk_message_desc(devinfo, inst->mlen, rlen, inst->header_size);

   elk_send_indirect_message(p, inst->sfid, dst, payload, desc, desc_imm,
                             inst->eot);
   if (inst->check_tdr)
      elk_inst_set_opcode(p->isa, elk_last_inst, ELK_OPCODE_SENDC);
}

void
elk_fs_generator::fire_fb_write(elk_fs_inst *inst,
                            struct elk_reg payload,
                            struct elk_reg implied_header,
                            GLuint nr)
{
   struct elk_wm_prog_data *prog_data = elk_wm_prog_data(this->prog_data);

   if (devinfo->ver < 6) {
      elk_push_insn_state(p);
      elk_set_default_exec_size(p, ELK_EXECUTE_8);
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);
      elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
      elk_set_default_flag_reg(p, 0, 0);
      elk_set_default_compression_control(p, ELK_COMPRESSION_NONE);
      elk_MOV(p, offset(retype(payload, ELK_REGISTER_TYPE_UD), 1),
              offset(retype(implied_header, ELK_REGISTER_TYPE_UD), 1));
      elk_pop_insn_state(p);
   }

   uint32_t msg_control = elk_fb_write_msg_control(inst, prog_data);

   /* We assume render targets start at 0, because headerless FB write
    * messages set "Render Target Index" to 0.  Using a different binding
    * table index would make it impossible to use headerless messages.
    */
   const uint32_t surf_index = inst->target;

   elk_inst *insn = elk_fb_WRITE(p,
                                 payload,
                                 retype(implied_header, ELK_REGISTER_TYPE_UW),
                                 msg_control,
                                 surf_index,
                                 nr,
                                 0,
                                 inst->eot,
                                 inst->last_rt,
                                 inst->header_size != 0);

   if (devinfo->ver >= 6)
      elk_inst_set_rt_slot_group(devinfo, insn, inst->group / 16);
}

void
elk_fs_generator::generate_fb_write(elk_fs_inst *inst, struct elk_reg payload)
{
   assert(devinfo->ver < 7);

   elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
   elk_set_default_flag_reg(p, 0, 0);

   const struct elk_reg implied_header =
      devinfo->ver < 6 ? payload : elk_null_reg();

   if (inst->base_mrf >= 0)
      payload = elk_message_reg(inst->base_mrf);

   if (!runtime_check_aads_emit) {
      fire_fb_write(inst, payload, implied_header, inst->mlen);
   } else {
      /* This can only happen in gen < 6 */
      assert(devinfo->ver < 6);

      struct elk_reg v1_null_ud = vec1(retype(elk_null_reg(), ELK_REGISTER_TYPE_UD));

      /* Check runtime bit to detect if we have to send AA data or not */
      elk_push_insn_state(p);
      elk_set_default_compression_control(p, ELK_COMPRESSION_NONE);
      elk_set_default_exec_size(p, ELK_EXECUTE_1);
      elk_AND(p,
              v1_null_ud,
              retype(elk_vec1_grf(1, 6), ELK_REGISTER_TYPE_UD),
              elk_imm_ud(1<<26));
      elk_inst_set_cond_modifier(p->devinfo, elk_last_inst, ELK_CONDITIONAL_NZ);

      int jmp = elk_JMPI(p, elk_imm_ud(0), ELK_PREDICATE_NORMAL) - p->store;
      elk_pop_insn_state(p);
      {
         /* Don't send AA data */
         fire_fb_write(inst, offset(payload, 1), implied_header, inst->mlen-1);
      }
      elk_land_fwd_jump(p, jmp);
      fire_fb_write(inst, payload, implied_header, inst->mlen);
   }
}

void
elk_fs_generator::generate_mov_indirect(elk_fs_inst *inst,
                                    struct elk_reg dst,
                                    struct elk_reg reg,
                                    struct elk_reg indirect_byte_offset)
{
   assert(indirect_byte_offset.type == ELK_REGISTER_TYPE_UD);
   assert(indirect_byte_offset.file == ELK_GENERAL_REGISTER_FILE);
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
   reg.type = dst.type = elk_reg_type_from_bit_size(type_sz(reg.type) * 8,
                                                    ELK_REGISTER_TYPE_UD);

   unsigned imm_byte_offset = reg.nr * REG_SIZE + reg.subnr;

   if (indirect_byte_offset.file == ELK_IMMEDIATE_VALUE) {
      imm_byte_offset += indirect_byte_offset.ud;

      reg.nr = imm_byte_offset / REG_SIZE;
      reg.subnr = imm_byte_offset % REG_SIZE;
      if (type_sz(reg.type) > 4 && !devinfo->has_64bit_float) {
         elk_MOV(p, subscript(dst, ELK_REGISTER_TYPE_D, 0),
                    subscript(reg, ELK_REGISTER_TYPE_D, 0));
         elk_MOV(p, subscript(dst, ELK_REGISTER_TYPE_D, 1),
                    subscript(reg, ELK_REGISTER_TYPE_D, 1));
      } else {
         elk_MOV(p, dst, reg);
      }
   } else {
      /* Prior to Broadwell, there are only 8 address registers. */
      assert(inst->exec_size <= 8 || devinfo->ver >= 8);

      /* We use VxH indirect addressing, clobbering a0.0 through a0.7. */
      struct elk_reg addr = vec8(elk_address_reg(0));

      /* Whether we can use destination dependency control without running the
       * risk of a hang if an instruction gets shot down.
       */
      const bool use_dep_ctrl = !inst->predicate &&
                                inst->exec_size == dispatch_width;
      elk_inst *insn;

      /* The destination stride of an instruction (in bytes) must be greater
       * than or equal to the size of the rest of the instruction.  Since the
       * address register is of type UW, we can't use a D-type instruction.
       * In order to get around this, re retype to UW and use a stride.
       */
      indirect_byte_offset =
         retype(spread(indirect_byte_offset, 2), ELK_REGISTER_TYPE_UW);

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
      if (devinfo->ver >= 7) {
         insn = elk_MOV(p, addr, elk_imm_uw(imm_byte_offset));
         elk_inst_set_mask_control(devinfo, insn, ELK_MASK_DISABLE);
         elk_inst_set_pred_control(devinfo, insn, ELK_PREDICATE_NONE);
         elk_inst_set_no_dd_clear(devinfo, insn, use_dep_ctrl);
      }

      insn = elk_ADD(p, addr, indirect_byte_offset, elk_imm_uw(imm_byte_offset));
      if (devinfo->ver >= 7)
         elk_inst_set_no_dd_check(devinfo, insn, use_dep_ctrl);

      if (type_sz(reg.type) > 4 &&
          (devinfo->verx10 == 70 || devinfo->platform == INTEL_PLATFORM_CHV ||
           !devinfo->has_64bit_float)) {
         /* IVB has an issue (which we found empirically) where it reads two
          * address register components per channel for indirectly addressed
          * 64-bit sources.
          *
          * From the Cherryview PRM Vol 7. "Register Region Restrictions":
          *
          *    "When source or destination datatype is 64b or operation is
          *    integer DWord multiply, indirect addressing must not be used."
          *
          * To work around both of these, we do two integer MOVs insead of one
          * 64-bit MOV.  Because no double value should ever cross a register
          * boundary, it's safe to use the immediate offset in the indirect
          * here to handle adding 4 bytes to the offset and avoid the extra
          * ADD to the register file.
          */
         elk_MOV(p, subscript(dst, ELK_REGISTER_TYPE_D, 0),
                    retype(elk_VxH_indirect(0, 0), ELK_REGISTER_TYPE_D));
         elk_MOV(p, subscript(dst, ELK_REGISTER_TYPE_D, 1),
                    retype(elk_VxH_indirect(0, 4), ELK_REGISTER_TYPE_D));
      } else {
         struct elk_reg ind_src = elk_VxH_indirect(0, 0);

         elk_inst *mov = elk_MOV(p, dst, retype(ind_src, reg.type));

         if (devinfo->ver == 6 && dst.file == ELK_MESSAGE_REGISTER_FILE &&
             !inst->get_next()->is_tail_sentinel() &&
             ((elk_fs_inst *)inst->get_next())->mlen > 0) {
            /* From the Sandybridge PRM:
             *
             *    "[Errata: DevSNB(SNB)] If MRF register is updated by any
             *    instruction that “indexed/indirect” source AND is followed
             *    by a send, the instruction requires a “Switch”. This is to
             *    avoid race condition where send may dispatch before MRF is
             *    updated."
             */
            elk_inst_set_thread_control(devinfo, mov, ELK_THREAD_SWITCH);
         }
      }
   }
}

void
elk_fs_generator::generate_shuffle(elk_fs_inst *inst,
                               struct elk_reg dst,
                               struct elk_reg src,
                               struct elk_reg idx)
{
   assert(src.file == ELK_GENERAL_REGISTER_FILE);
   assert(!src.abs && !src.negate);

   /* Ivy bridge has some strange behavior that makes this a real pain to
    * implement for 64-bit values so we just don't bother.
    */
   assert((devinfo->verx10 >= 75 && devinfo->has_64bit_float) ||
          type_sz(src.type) <= 4);

   /* Gen12.5 adds the following region restriction:
    *
    *    "Vx1 and VxH indirect addressing for Float, Half-Float, Double-Float
    *    and Quad-Word data must not be used."
    *
    * We require the source and destination types to match so stomp to an
    * unsigned integer type.
    */
   assert(src.type == dst.type);
   src.type = dst.type = elk_reg_type_from_bit_size(type_sz(src.type) * 8,
                                                    ELK_REGISTER_TYPE_UD);

   /* Because we're using the address register, we're limited to 8-wide
    * execution on gfx7.  On gfx8, we're limited to 16-wide by the address
    * register file and 8-wide for 64-bit types.  We could try and make this
    * instruction splittable higher up in the compiler but that gets weird
    * because it reads all of the channels regardless of execution size.  It's
    * easier just to split it here.
    */
   const unsigned lower_width =
      devinfo->ver <= 7 || element_sz(src) > 4 || element_sz(dst) > 4 ? 8 :
      MIN2(16, inst->exec_size);

   elk_set_default_exec_size(p, cvt(lower_width) - 1);
   for (unsigned group = 0; group < inst->exec_size; group += lower_width) {
      elk_set_default_group(p, group);

      if ((src.vstride == 0 && src.hstride == 0) ||
          idx.file == ELK_IMMEDIATE_VALUE) {
         /* Trivial, the source is already uniform or the index is a constant.
          * We will typically not get here if the optimizer is doing its job,
          * but asserting would be mean.
          */
         const unsigned i = idx.file == ELK_IMMEDIATE_VALUE ? idx.ud : 0;
         struct elk_reg group_src = stride(suboffset(src, i), 0, 1, 0);
         struct elk_reg group_dst = suboffset(dst, group << (dst.hstride - 1));
         elk_MOV(p, group_dst, group_src);
      } else {
         /* We use VxH indirect addressing, clobbering a0.0 through a0.7. */
         struct elk_reg addr = vec8(elk_address_reg(0));

         struct elk_reg group_idx = suboffset(idx, group);

         if (lower_width == 8 && group_idx.width == ELK_WIDTH_16) {
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
            group_idx = retype(spread(group_idx, 2), ELK_REGISTER_TYPE_W);
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
         elk_inst *insn;

         /* Due to a hardware bug some platforms (particularly Gfx11+) seem
          * to require the address components of all channels to be valid
          * whether or not they're active, which causes issues if we use VxH
          * addressing under non-uniform control-flow.  We can easily work
          * around that by initializing the whole address register with a
          * pipelined NoMask MOV instruction.
          */
         insn = elk_MOV(p, addr, elk_imm_uw(src_start_offset));
         elk_inst_set_mask_control(devinfo, insn, ELK_MASK_DISABLE);
         elk_inst_set_pred_control(devinfo, insn, ELK_PREDICATE_NONE);
         elk_inst_set_no_dd_clear(devinfo, insn, use_dep_ctrl);

         /* Take into account the component size and horizontal stride. */
         assert(src.vstride == src.hstride + src.width);
         insn = elk_SHL(p, addr, group_idx,
                        elk_imm_uw(util_logbase2(type_sz(src.type)) +
                                   src.hstride - 1));
         elk_inst_set_no_dd_check(devinfo, insn, use_dep_ctrl);

         /* Add on the register start offset */
         elk_ADD(p, addr, addr, elk_imm_uw(src_start_offset));
         elk_MOV(p, suboffset(dst, group << (dst.hstride - 1)),
                 retype(elk_VxH_indirect(0, 0), src.type));
      }
   }
}

void
elk_fs_generator::generate_quad_swizzle(const elk_fs_inst *inst,
                                    struct elk_reg dst, struct elk_reg src,
                                    unsigned swiz)
{
   /* Requires a quad. */
   assert(inst->exec_size >= 4);

   if (src.file == ELK_IMMEDIATE_VALUE ||
       has_scalar_region(src)) {
      /* The value is uniform across all channels */
      elk_MOV(p, dst, src);

   } else if (type_sz(src.type) == 4) {
      /* This only works on 8-wide 32-bit values */
      assert(inst->exec_size == 8);
      assert(src.hstride == ELK_HORIZONTAL_STRIDE_1);
      assert(src.vstride == src.width + 1);
      elk_set_default_access_mode(p, ELK_ALIGN_16);
      struct elk_reg swiz_src = stride(src, 4, 4, 1);
      swiz_src.swizzle = swiz;
      elk_MOV(p, dst, swiz_src);

   } else {
      assert(src.hstride == ELK_HORIZONTAL_STRIDE_1);
      assert(src.vstride == src.width + 1);
      const struct elk_reg src_0 = suboffset(src, ELK_GET_SWZ(swiz, 0));

      switch (swiz) {
      case ELK_SWIZZLE_XXXX:
      case ELK_SWIZZLE_YYYY:
      case ELK_SWIZZLE_ZZZZ:
      case ELK_SWIZZLE_WWWW:
         elk_MOV(p, dst, stride(src_0, 4, 4, 0));
         break;

      case ELK_SWIZZLE_XXZZ:
      case ELK_SWIZZLE_YYWW:
         elk_MOV(p, dst, stride(src_0, 2, 2, 0));
         break;

      case ELK_SWIZZLE_XYXY:
      case ELK_SWIZZLE_ZWZW:
         assert(inst->exec_size == 4);
         elk_MOV(p, dst, stride(src_0, 0, 2, 1));
         break;

      default:
         assert(inst->force_writemask_all);
         elk_set_default_exec_size(p, cvt(inst->exec_size / 4) - 1);

         for (unsigned c = 0; c < 4; c++) {
            elk_inst *insn = elk_MOV(
               p, stride(suboffset(dst, c),
                         4 * inst->dst.stride, 1, 4 * inst->dst.stride),
               stride(suboffset(src, ELK_GET_SWZ(swiz, c)), 4, 1, 0));

            elk_inst_set_no_dd_clear(devinfo, insn, c < 3);
            elk_inst_set_no_dd_check(devinfo, insn, c > 0);
         }

         break;
      }
   }
}

void
elk_fs_generator::generate_cs_terminate(elk_fs_inst *inst, struct elk_reg payload)
{
   struct elk_inst *insn;

   insn = elk_next_insn(p, ELK_OPCODE_SEND);

   elk_set_dest(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_UW));
   elk_set_src0(p, insn, retype(payload, ELK_REGISTER_TYPE_UW));
   elk_set_src1(p, insn, elk_imm_ud(0u));

   elk_inst_set_sfid(devinfo, insn, ELK_SFID_THREAD_SPAWNER);
   elk_inst_set_mlen(devinfo, insn, 1);
   elk_inst_set_rlen(devinfo, insn, 0);
   elk_inst_set_eot(devinfo, insn, inst->eot);
   elk_inst_set_header_present(devinfo, insn, false);

   elk_inst_set_ts_opcode(devinfo, insn, 0); /* Dereference resource */

   elk_inst_set_ts_request_type(devinfo, insn, 0); /* Root thread */

   /* Note that even though the thread has a URB resource associated with it,
    * we set the "do not dereference URB" bit, because the URB resource is
    * managed by the fixed-function unit, so it will free it automatically.
    */
   elk_inst_set_ts_resource_select(devinfo, insn, 1); /* Do not dereference URB */

   elk_inst_set_mask_control(devinfo, insn, ELK_MASK_DISABLE);
}

void
elk_fs_generator::generate_barrier(elk_fs_inst *, struct elk_reg src)
{
   elk_barrier(p, src);
   elk_WAIT(p);
}

bool
elk_fs_generator::generate_linterp(elk_fs_inst *inst,
                               struct elk_reg dst, struct elk_reg *src)
{
   /* PLN reads:
    *                      /   in SIMD16   \
    *    -----------------------------------
    *   | src1+0 | src1+1 | src1+2 | src1+3 |
    *   |-----------------------------------|
    *   |(x0, x1)|(y0, y1)|(x2, x3)|(y2, y3)|
    *    -----------------------------------
    *
    * but for the LINE/MAC pair, the LINE reads Xs and the MAC reads Ys:
    *
    *    -----------------------------------
    *   | src1+0 | src1+1 | src1+2 | src1+3 |
    *   |-----------------------------------|
    *   |(x0, x1)|(y0, y1)|        |        | in SIMD8
    *   |-----------------------------------|
    *   |(x0, x1)|(x2, x3)|(y0, y1)|(y2, y3)| in SIMD16
    *    -----------------------------------
    *
    * See also: emit_interpolation_setup_gfx4().
    */
   struct elk_reg delta_x = src[0];
   struct elk_reg delta_y = offset(src[0], inst->exec_size / 8);
   struct elk_reg interp = src[1];
   elk_inst *i[2];

   if (devinfo->has_pln) {
      if (devinfo->ver <= 6 && (delta_x.nr & 1) != 0) {
         /* From the Sandy Bridge PRM Vol. 4, Pt. 2, Section 8.3.53, "Plane":
          *
          *    "[DevSNB]:<src1> must be even register aligned.
          *
          * This restriction is lifted on Ivy Bridge.
          *
          * This means that we need to split PLN into LINE+MAC on-the-fly.
          * Unfortunately, the inputs are laid out for PLN and not LINE+MAC so
          * we have to split into SIMD8 pieces.  For gfx4 (!has_pln), the
          * coordinate registers are laid out differently so we leave it as a
          * SIMD16 instruction.
          */
         assert(inst->exec_size == 8 || inst->exec_size == 16);
         assert(inst->group % 16 == 0);

         elk_push_insn_state(p);
         elk_set_default_exec_size(p, ELK_EXECUTE_8);

         /* Thanks to two accumulators, we can emit all the LINEs and then all
          * the MACs.  This improves parallelism a bit.
          */
         for (unsigned g = 0; g < inst->exec_size / 8; g++) {
            elk_inst *line = elk_LINE(p, elk_null_reg(), interp,
                                      offset(delta_x, g * 2));
            elk_inst_set_group(devinfo, line, inst->group + g * 8);

            /* LINE writes the accumulator automatically on gfx4-5.  On Sandy
             * Bridge and later, we have to explicitly enable it.
             */
            if (devinfo->ver >= 6)
               elk_inst_set_acc_wr_control(p->devinfo, line, true);

            /* elk_set_default_saturate() is called before emitting
             * instructions, so the saturate bit is set in each instruction,
             * so we need to unset it on the LINE instructions.
             */
            elk_inst_set_saturate(p->devinfo, line, false);
         }

         for (unsigned g = 0; g < inst->exec_size / 8; g++) {
            elk_inst *mac = elk_MAC(p, offset(dst, g), suboffset(interp, 1),
                                    offset(delta_x, g * 2 + 1));
            elk_inst_set_group(devinfo, mac, inst->group + g * 8);
            elk_inst_set_cond_modifier(p->devinfo, mac, inst->conditional_mod);
         }

         elk_pop_insn_state(p);

         return true;
      } else {
         elk_PLN(p, dst, interp, delta_x);

         return false;
      }
   } else {
      i[0] = elk_LINE(p, elk_null_reg(), interp, delta_x);
      i[1] = elk_MAC(p, dst, suboffset(interp, 1), delta_y);

      elk_inst_set_cond_modifier(p->devinfo, i[1], inst->conditional_mod);

      /* elk_set_default_saturate() is called before emitting instructions, so
       * the saturate bit is set in each instruction, so we need to unset it on
       * the first instruction.
       */
      elk_inst_set_saturate(p->devinfo, i[0], false);

      return true;
   }
}

void
elk_fs_generator::generate_tex(elk_fs_inst *inst, struct elk_reg dst,
                           struct elk_reg surface_index,
                           struct elk_reg sampler_index)
{
   assert(devinfo->ver < 7);
   assert(inst->size_written % REG_SIZE == 0);
   int msg_type = -1;
   uint32_t simd_mode;
   uint32_t return_format;

   /* Sampler EOT message of less than the dispatch width would kill the
    * thread prematurely.
    */
   assert(!inst->eot || inst->exec_size == dispatch_width);

   switch (dst.type) {
   case ELK_REGISTER_TYPE_D:
      return_format = ELK_SAMPLER_RETURN_FORMAT_SINT32;
      break;
   case ELK_REGISTER_TYPE_UD:
      return_format = ELK_SAMPLER_RETURN_FORMAT_UINT32;
      break;
   default:
      return_format = ELK_SAMPLER_RETURN_FORMAT_FLOAT32;
      break;
   }

   /* Stomp the resinfo output type to UINT32.  On gens 4-5, the output type
    * is set as part of the message descriptor.  On gfx4, the PRM seems to
    * allow UINT32 and FLOAT32 (i965 PRM, Vol. 4 Section 4.8.1.1), but on
    * later gens UINT32 is required.  Once you hit Sandy Bridge, the bit is
    * gone from the message descriptor entirely and you just get UINT32 all
    * the time regasrdless.  Since we can really only do non-UINT32 on gfx4,
    * just stomp it to UINT32 all the time.
    */
   if (inst->opcode == ELK_SHADER_OPCODE_TXS)
      return_format = ELK_SAMPLER_RETURN_FORMAT_UINT32;

   switch (inst->exec_size) {
   case 8:
      simd_mode = ELK_SAMPLER_SIMD_MODE_SIMD8;
      break;
   case 16:
      simd_mode = ELK_SAMPLER_SIMD_MODE_SIMD16;
      break;
   default:
      unreachable("Invalid width for texture instruction");
   }

   if (devinfo->ver >= 5) {
      switch (inst->opcode) {
      case ELK_SHADER_OPCODE_TEX:
	 if (inst->shadow_compare) {
	    msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_COMPARE;
	 } else {
	    msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE;
	 }
	 break;
      case ELK_FS_OPCODE_TXB:
	 if (inst->shadow_compare) {
	    msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_BIAS_COMPARE;
	 } else {
	    msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_BIAS;
	 }
	 break;
      case ELK_SHADER_OPCODE_TXL:
	 if (inst->shadow_compare) {
	    msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_LOD_COMPARE;
	 } else {
	    msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_LOD;
	 }
	 break;
      case ELK_SHADER_OPCODE_TXS:
	 msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_RESINFO;
	 break;
      case ELK_SHADER_OPCODE_TXD:
         assert(!inst->shadow_compare);
         msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_DERIVS;
	 break;
      case ELK_SHADER_OPCODE_TXF:
	 msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_LD;
	 break;
      case ELK_SHADER_OPCODE_TXF_CMS:
         msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_LD;
         break;
      case ELK_SHADER_OPCODE_LOD:
         msg_type = GFX5_SAMPLER_MESSAGE_LOD;
         break;
      case ELK_SHADER_OPCODE_TG4:
         assert(devinfo->ver == 6);
         assert(!inst->shadow_compare);
         msg_type = GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4;
         break;
      case ELK_SHADER_OPCODE_SAMPLEINFO:
         msg_type = GFX6_SAMPLER_MESSAGE_SAMPLE_SAMPLEINFO;
         break;
      default:
	 unreachable("not reached");
      }
   } else {
      switch (inst->opcode) {
      case ELK_SHADER_OPCODE_TEX:
	 /* Note that G45 and older determines shadow compare and dispatch width
	  * from message length for most messages.
	  */
         if (inst->exec_size == 8) {
            msg_type = ELK_SAMPLER_MESSAGE_SIMD8_SAMPLE;
            if (inst->shadow_compare) {
               assert(inst->mlen == 6);
            } else {
               assert(inst->mlen <= 4);
            }
         } else {
            if (inst->shadow_compare) {
               msg_type = ELK_SAMPLER_MESSAGE_SIMD16_SAMPLE_COMPARE;
               assert(inst->mlen == 9);
            } else {
               msg_type = ELK_SAMPLER_MESSAGE_SIMD16_SAMPLE;
               assert(inst->mlen <= 7 && inst->mlen % 2 == 1);
            }
         }
	 break;
      case ELK_FS_OPCODE_TXB:
	 if (inst->shadow_compare) {
            assert(inst->exec_size == 8);
	    assert(inst->mlen == 6);
	    msg_type = ELK_SAMPLER_MESSAGE_SIMD8_SAMPLE_BIAS_COMPARE;
	 } else {
	    assert(inst->mlen == 9);
	    msg_type = ELK_SAMPLER_MESSAGE_SIMD16_SAMPLE_BIAS;
	    simd_mode = ELK_SAMPLER_SIMD_MODE_SIMD16;
	 }
	 break;
      case ELK_SHADER_OPCODE_TXL:
	 if (inst->shadow_compare) {
            assert(inst->exec_size == 8);
	    assert(inst->mlen == 6);
	    msg_type = ELK_SAMPLER_MESSAGE_SIMD8_SAMPLE_LOD_COMPARE;
	 } else {
	    assert(inst->mlen == 9);
	    msg_type = ELK_SAMPLER_MESSAGE_SIMD16_SAMPLE_LOD;
	    simd_mode = ELK_SAMPLER_SIMD_MODE_SIMD16;
	 }
	 break;
      case ELK_SHADER_OPCODE_TXD:
	 /* There is no sample_d_c message; comparisons are done manually */
         assert(inst->exec_size == 8);
	 assert(inst->mlen == 7 || inst->mlen == 10);
	 msg_type = ELK_SAMPLER_MESSAGE_SIMD8_SAMPLE_GRADIENTS;
	 break;
      case ELK_SHADER_OPCODE_TXF:
         assert(inst->mlen <= 9 && inst->mlen % 2 == 1);
	 msg_type = ELK_SAMPLER_MESSAGE_SIMD16_LD;
	 simd_mode = ELK_SAMPLER_SIMD_MODE_SIMD16;
	 break;
      case ELK_SHADER_OPCODE_TXS:
	 assert(inst->mlen == 3);
	 msg_type = ELK_SAMPLER_MESSAGE_SIMD16_RESINFO;
	 simd_mode = ELK_SAMPLER_SIMD_MODE_SIMD16;
	 break;
      default:
	 unreachable("not reached");
      }
   }
   assert(msg_type != -1);

   if (simd_mode == ELK_SAMPLER_SIMD_MODE_SIMD16) {
      dst = vec16(dst);
   }

   assert(sampler_index.type == ELK_REGISTER_TYPE_UD);

   /* Load the message header if present.  If there's a texture offset,
    * we need to set it up explicitly and load the offset bitfield.
    * Otherwise, we can use an implied move from g0 to the first message reg.
    */
   struct elk_reg src = elk_null_reg();
   if (inst->header_size != 0) {
      if (devinfo->ver < 6 && !inst->offset) {
         /* Set up an implied move from g0 to the MRF. */
         src = retype(elk_vec8_grf(0, 0), ELK_REGISTER_TYPE_UW);
      } else {
         assert(inst->base_mrf != -1);
         struct elk_reg header_reg = elk_message_reg(inst->base_mrf);

         elk_push_insn_state(p);
         elk_set_default_exec_size(p, ELK_EXECUTE_8);
         elk_set_default_mask_control(p, ELK_MASK_DISABLE);
         elk_set_default_compression_control(p, ELK_COMPRESSION_NONE);
         /* Explicitly set up the message header by copying g0 to the MRF. */
         elk_MOV(p, header_reg, elk_vec8_grf(0, 0));

         elk_set_default_exec_size(p, ELK_EXECUTE_1);
         if (inst->offset) {
            /* Set the offset bits in DWord 2. */
            elk_MOV(p, get_element_ud(header_reg, 2),
                       elk_imm_ud(inst->offset));
         }

         elk_pop_insn_state(p);
      }
   }

   assert(surface_index.file == ELK_IMMEDIATE_VALUE);
   assert(sampler_index.file == ELK_IMMEDIATE_VALUE);

   elk_SAMPLE(p,
              retype(dst, ELK_REGISTER_TYPE_UW),
              inst->base_mrf,
              src,
              surface_index.ud,
              sampler_index.ud % 16,
              msg_type,
              inst->size_written / REG_SIZE,
              inst->mlen,
              inst->header_size != 0,
              simd_mode,
              return_format);
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
elk_fs_generator::generate_ddx(const elk_fs_inst *inst,
                           struct elk_reg dst, struct elk_reg src)
{
   unsigned vstride, width;

   if (devinfo->ver >= 8) {
      if (inst->opcode == ELK_FS_OPCODE_DDX_FINE) {
         /* produce accurate derivatives */
         vstride = ELK_VERTICAL_STRIDE_2;
         width = ELK_WIDTH_2;
      } else {
         /* replicate the derivative at the top-left pixel to other pixels */
         vstride = ELK_VERTICAL_STRIDE_4;
         width = ELK_WIDTH_4;
      }

      struct elk_reg src0 = byte_offset(src, type_sz(src.type));;
      struct elk_reg src1 = src;

      src0.vstride = vstride;
      src0.width   = width;
      src0.hstride = ELK_HORIZONTAL_STRIDE_0;
      src1.vstride = vstride;
      src1.width   = width;
      src1.hstride = ELK_HORIZONTAL_STRIDE_0;

      elk_ADD(p, dst, src0, negate(src1));
   } else {
      /* On Haswell and earlier, the region used above appears to not work
       * correctly for compressed instructions.  At least on Haswell and
       * Iron Lake, compressed ALIGN16 instructions do work.  Since we
       * would have to split to SIMD8 no matter which method we choose, we
       * may as well use ALIGN16 on all platforms gfx7 and earlier.
       */
      struct elk_reg src0 = stride(src, 4, 4, 1);
      struct elk_reg src1 = stride(src, 4, 4, 1);
      if (inst->opcode == ELK_FS_OPCODE_DDX_FINE) {
         src0.swizzle = ELK_SWIZZLE_XXZZ;
         src1.swizzle = ELK_SWIZZLE_YYWW;
      } else {
         src0.swizzle = ELK_SWIZZLE_XXXX;
         src1.swizzle = ELK_SWIZZLE_YYYY;
      }

      elk_push_insn_state(p);
      elk_set_default_access_mode(p, ELK_ALIGN_16);
      elk_ADD(p, dst, negate(src0), src1);
      elk_pop_insn_state(p);
   }
}

/* The negate_value boolean is used to negate the derivative computation for
 * FBOs, since they place the origin at the upper left instead of the lower
 * left.
 */
void
elk_fs_generator::generate_ddy(const elk_fs_inst *inst,
                           struct elk_reg dst, struct elk_reg src)
{
   const uint32_t type_size = type_sz(src.type);

   if (inst->opcode == ELK_FS_OPCODE_DDY_FINE) {
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
      if (devinfo->platform == INTEL_PLATFORM_BDW && src.type == ELK_REGISTER_TYPE_HF) {
         src = stride(src, 0, 2, 1);

         elk_push_insn_state(p);
         elk_set_default_exec_size(p, ELK_EXECUTE_4);
         for (uint32_t g = 0; g < inst->exec_size; g += 4) {
            elk_set_default_group(p, inst->group + g);
            elk_ADD(p, byte_offset(dst, g * type_size),
                       negate(byte_offset(src,  g * type_size)),
                       byte_offset(src, (g + 2) * type_size));
         }
         elk_pop_insn_state(p);
      } else {
         struct elk_reg src0 = stride(src, 4, 4, 1);
         struct elk_reg src1 = stride(src, 4, 4, 1);
         src0.swizzle = ELK_SWIZZLE_XYXY;
         src1.swizzle = ELK_SWIZZLE_ZWZW;

         elk_push_insn_state(p);
         elk_set_default_access_mode(p, ELK_ALIGN_16);
         elk_ADD(p, dst, negate(src0), src1);
         elk_pop_insn_state(p);
      }
   } else {
      /* replicate the derivative at the top-left pixel to other pixels */
      if (devinfo->ver >= 8) {
         struct elk_reg src0 = byte_offset(stride(src, 4, 4, 0), 0 * type_size);
         struct elk_reg src1 = byte_offset(stride(src, 4, 4, 0), 2 * type_size);

         elk_ADD(p, dst, negate(src0), src1);
      } else {
         /* On Haswell and earlier, the region used above appears to not work
          * correctly for compressed instructions.  At least on Haswell and
          * Iron Lake, compressed ALIGN16 instructions do work.  Since we
          * would have to split to SIMD8 no matter which method we choose, we
          * may as well use ALIGN16 on all platforms gfx7 and earlier.
          */
         struct elk_reg src0 = stride(src, 4, 4, 1);
         struct elk_reg src1 = stride(src, 4, 4, 1);
         src0.swizzle = ELK_SWIZZLE_XXXX;
         src1.swizzle = ELK_SWIZZLE_ZZZZ;

         elk_push_insn_state(p);
         elk_set_default_access_mode(p, ELK_ALIGN_16);
         elk_ADD(p, dst, negate(src0), src1);
         elk_pop_insn_state(p);
      }
   }
}

void
elk_fs_generator::generate_halt(elk_fs_inst *)
{
   /* This HALT will be patched up at FB write time to point UIP at the end of
    * the program, and at elk_uip_jip() JIP will be set to the end of the
    * current block (or the program).
    */
   this->discard_halt_patches.push_tail(new(mem_ctx) ip_record(p->nr_insn));
   elk_HALT(p);
}

void
elk_fs_generator::generate_scratch_write(elk_fs_inst *inst, struct elk_reg src)
{
   /* The 32-wide messages only respect the first 16-wide half of the channel
    * enable signals which are replicated identically for the second group of
    * 16 channels, so we cannot use them unless the write is marked
    * force_writemask_all.
    */
   const unsigned lower_size = inst->force_writemask_all ? inst->exec_size :
                               MIN2(16, inst->exec_size);
   const unsigned block_size = 4 * lower_size / REG_SIZE;
   assert(inst->mlen != 0);

   elk_push_insn_state(p);
   elk_set_default_exec_size(p, cvt(lower_size) - 1);
   elk_set_default_compression(p, lower_size > 8);

   for (unsigned i = 0; i < inst->exec_size / lower_size; i++) {
      elk_set_default_group(p, inst->group + lower_size * i);

      elk_MOV(p, elk_uvec_mrf(lower_size, inst->base_mrf + 1, 0),
              retype(offset(src, block_size * i), ELK_REGISTER_TYPE_UD));

      elk_oword_block_write_scratch(p, elk_message_reg(inst->base_mrf),
                                    block_size,
                                    inst->offset + block_size * REG_SIZE * i);
   }

   elk_pop_insn_state(p);
}

void
elk_fs_generator::generate_scratch_read(elk_fs_inst *inst, struct elk_reg dst)
{
   assert(inst->exec_size <= 16 || inst->force_writemask_all);
   assert(inst->mlen != 0);

   elk_oword_block_read_scratch(p, dst, elk_message_reg(inst->base_mrf),
                                inst->exec_size / 8, inst->offset);
}

void
elk_fs_generator::generate_scratch_read_gfx7(elk_fs_inst *inst, struct elk_reg dst)
{
   assert(inst->exec_size <= 16 || inst->force_writemask_all);

   elk_gfx7_block_read_scratch(p, dst, inst->exec_size / 8, inst->offset);
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
elk_fs_generator::generate_scratch_header(elk_fs_inst *inst, struct elk_reg dst)
{
   assert(inst->exec_size == 8 && inst->force_writemask_all);
   assert(dst.file == ELK_GENERAL_REGISTER_FILE);

   dst.type = ELK_REGISTER_TYPE_UD;

   elk_inst *insn = elk_MOV(p, dst, elk_imm_ud(0));
   elk_inst_set_no_dd_clear(p->devinfo, insn, true);

   /* Copy the per-thread scratch space size from g0.3[3:0] */
   elk_set_default_exec_size(p, ELK_EXECUTE_1);
   insn = elk_AND(p, suboffset(dst, 3),
                     retype(elk_vec1_grf(0, 3), ELK_REGISTER_TYPE_UD),
                     elk_imm_ud(INTEL_MASK(3, 0)));
   elk_inst_set_no_dd_clear(p->devinfo, insn, true);
   elk_inst_set_no_dd_check(p->devinfo, insn, true);

   /* Copy the scratch base address from g0.5[31:10] */
   insn = elk_AND(p, suboffset(dst, 5),
                     retype(elk_vec1_grf(0, 5), ELK_REGISTER_TYPE_UD),
                     elk_imm_ud(INTEL_MASK(31, 10)));
   elk_inst_set_no_dd_check(p->devinfo, insn, true);
}

void
elk_fs_generator::generate_uniform_pull_constant_load(elk_fs_inst *inst,
                                                  struct elk_reg dst,
                                                  struct elk_reg index,
                                                  struct elk_reg offset)
{
   assert(type_sz(dst.type) == 4);
   assert(inst->mlen != 0);

   assert(index.file == ELK_IMMEDIATE_VALUE &&
	  index.type == ELK_REGISTER_TYPE_UD);
   uint32_t surf_index = index.ud;

   assert(offset.file == ELK_IMMEDIATE_VALUE &&
	  offset.type == ELK_REGISTER_TYPE_UD);
   uint32_t read_offset = offset.ud;

   elk_oword_block_read(p, dst, elk_message_reg(inst->base_mrf),
			read_offset, surf_index);
}

void
elk_fs_generator::generate_varying_pull_constant_load_gfx4(elk_fs_inst *inst,
                                                       struct elk_reg dst,
                                                       struct elk_reg index)
{
   assert(devinfo->ver < 7); /* Should use the gfx7 variant. */
   assert(inst->header_size != 0);
   assert(inst->mlen);

   assert(index.file == ELK_IMMEDIATE_VALUE &&
	  index.type == ELK_REGISTER_TYPE_UD);
   uint32_t surf_index = index.ud;

   uint32_t simd_mode, rlen, msg_type;
   if (inst->exec_size == 16) {
      simd_mode = ELK_SAMPLER_SIMD_MODE_SIMD16;
      rlen = 8;
   } else {
      assert(inst->exec_size == 8);
      simd_mode = ELK_SAMPLER_SIMD_MODE_SIMD8;
      rlen = 4;
   }

   if (devinfo->ver >= 5)
      msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_LD;
   else {
      /* We always use the SIMD16 message so that we only have to load U, and
       * not V or R.
       */
      msg_type = ELK_SAMPLER_MESSAGE_SIMD16_LD;
      assert(inst->mlen == 3);
      assert(inst->size_written == 8 * REG_SIZE);
      rlen = 8;
      simd_mode = ELK_SAMPLER_SIMD_MODE_SIMD16;
   }

   struct elk_reg header = elk_vec8_grf(0, 0);
   elk_gfx6_resolve_implied_move(p, &header, inst->base_mrf);

   elk_inst *send = elk_next_insn(p, ELK_OPCODE_SEND);
   elk_inst_set_compression(devinfo, send, false);
   elk_inst_set_sfid(devinfo, send, ELK_SFID_SAMPLER);
   elk_set_dest(p, send, retype(dst, ELK_REGISTER_TYPE_UW));
   elk_set_src0(p, send, header);
   if (devinfo->ver < 6)
      elk_inst_set_base_mrf(p->devinfo, send, inst->base_mrf);

   /* Our surface is set up as floats, regardless of what actual data is
    * stored in it.
    */
   uint32_t return_format = ELK_SAMPLER_RETURN_FORMAT_FLOAT32;
   elk_set_desc(p, send,
                elk_message_desc(devinfo, inst->mlen, rlen, inst->header_size) |
                elk_sampler_desc(devinfo, surf_index,
                                 0, /* sampler (unused) */
                                 msg_type, simd_mode, return_format));
}

/* Sets vstride=1, width=4, hstride=0 of register src1 during
 * the ADD instruction.
 */
void
elk_fs_generator::generate_set_sample_id(elk_fs_inst *inst,
                                     struct elk_reg dst,
                                     struct elk_reg src0,
                                     struct elk_reg src1)
{
   assert(dst.type == ELK_REGISTER_TYPE_D ||
          dst.type == ELK_REGISTER_TYPE_UD);
   assert(src0.type == ELK_REGISTER_TYPE_D ||
          src0.type == ELK_REGISTER_TYPE_UD);

   const struct elk_reg reg = stride(src1, 1, 4, 0);
   const unsigned lower_size = MIN2(inst->exec_size,
                                    devinfo->ver >= 8 ? 16 : 8);

   for (unsigned i = 0; i < inst->exec_size / lower_size; i++) {
      elk_inst *insn = elk_ADD(p, offset(dst, i * lower_size / 8),
                               offset(src0, (src0.vstride == 0 ? 0 : (1 << (src0.vstride - 1)) *
                                             (i * lower_size / (1 << src0.width))) *
                                            type_sz(src0.type) / REG_SIZE),
                               suboffset(reg, i * lower_size / 4));
      elk_inst_set_exec_size(devinfo, insn, cvt(lower_size) - 1);
      elk_inst_set_group(devinfo, insn, inst->group + lower_size * i);
      elk_inst_set_compression(devinfo, insn, lower_size > 8);
   }
}

void
elk_fs_generator::enable_debug(const char *shader_name)
{
   debug_flag = true;
   this->shader_name = shader_name;
}

int
elk_fs_generator::generate_code(const elk_cfg_t *cfg, int dispatch_width,
                            struct shader_stats shader_stats,
                            const elk::performance &perf,
                            struct elk_compile_stats *stats)
{
   /* align to 64 byte boundary. */
   elk_realign(p, 64);

   this->dispatch_width = dispatch_width;

   int start_offset = p->next_insn_offset;

   int loop_count = 0, send_count = 0, nop_count = 0, sync_nop_count = 0;
   bool is_accum_used = false;

   struct elk_disasm_info *elk_disasm_info = elk_disasm_initialize(p->isa, cfg);

   foreach_block_and_inst (block, elk_fs_inst, inst, cfg) {
      if (inst->opcode == ELK_SHADER_OPCODE_UNDEF)
         continue;

      struct elk_reg src[4], dst;
      unsigned int last_insn_offset = p->next_insn_offset;
      bool multiple_instructions_emitted = false;

      /* From the Broadwell PRM, Volume 7, "3D-Media-GPGPU", in the
       * "Register Region Restrictions" section: for BDW, SKL:
       *
       *    "A POW/FDIV operation must not be followed by an instruction
       *     that requires two destination registers."
       *
       * The documentation is often lacking annotations for Atom parts,
       * and empirically this affects CHV as well.
       */
      if (devinfo->ver >= 8 &&
          p->nr_insn > 1 &&
          elk_inst_opcode(p->isa, elk_last_inst) == ELK_OPCODE_MATH &&
          elk_inst_math_function(devinfo, elk_last_inst) == ELK_MATH_FUNCTION_POW &&
          inst->dst.component_size(inst->exec_size) > REG_SIZE) {
         elk_NOP(p);
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
         elk_set_default_exec_size(p, ELK_EXECUTE_16);
         elk_set_default_group(p, 0);
         elk_set_default_mask_control(p, ELK_MASK_DISABLE);
         elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
         elk_set_default_flag_reg(p, 0, 0);
         elk_MOV(p, elk_acc_reg(8), elk_imm_f(0.0f));
         last_insn_offset = p->next_insn_offset;
      }

      if (!is_accum_used && !inst->eot) {
         is_accum_used = inst->writes_accumulator_implicitly(devinfo) ||
                         inst->dst.is_accumulator();
      }

      if (unlikely(debug_flag))
         elk_disasm_annotate(elk_disasm_info, inst, p->next_insn_offset);

      /* If the instruction writes to more than one register, it needs to be
       * explicitly marked as compressed on Gen <= 5.  On Gen >= 6 the
       * hardware figures out by itself what the right compression mode is,
       * but we still need to know whether the instruction is compressed to
       * set up the source register regions appropriately.
       *
       * XXX - This is wrong for instructions that write a single register but
       *       read more than one which should strictly speaking be treated as
       *       compressed.  For instructions that don't write any registers it
       *       relies on the destination being a null register of the correct
       *       type and regioning so the instruction is considered compressed
       *       or not accordingly.
       */
      const bool compressed =
           inst->dst.component_size(inst->exec_size) > REG_SIZE;
      elk_set_default_compression(p, compressed);

      if (devinfo->ver < 7 && inst->group % 8 != 0) {
         assert(inst->force_writemask_all);
         assert(!inst->predicate && !inst->conditional_mod);
         assert(!inst->writes_accumulator_implicitly(devinfo) &&
                !inst->reads_accumulator_implicitly());
         assert(inst->opcode != ELK_SHADER_OPCODE_SEL_EXEC);
         elk_set_default_group(p, 0);
      } else {
         elk_set_default_group(p, inst->group);
      }

      for (unsigned int i = 0; i < inst->sources; i++) {
         src[i] = elk_reg_from_fs_reg(devinfo, inst,
                                      &inst->src[i], compressed);
	 /* The accumulator result appears to get used for the
	  * conditional modifier generation.  When negating a UD
	  * value, there is a 33rd bit generated for the sign in the
	  * accumulator value, so now you can't check, for example,
	  * equality with a 32-bit value.  See piglit fs-op-neg-uvec4.
	  */
	 assert(!inst->conditional_mod ||
		inst->src[i].type != ELK_REGISTER_TYPE_UD ||
		!inst->src[i].negate);
      }
      dst = elk_reg_from_fs_reg(devinfo, inst,
                                &inst->dst, compressed);

      elk_set_default_access_mode(p, ELK_ALIGN_1);
      elk_set_default_predicate_control(p, inst->predicate);
      elk_set_default_predicate_inverse(p, inst->predicate_inverse);
      /* On gfx7 and above, hardware automatically adds the group onto the
       * flag subregister number.  On Sandy Bridge and older, we have to do it
       * ourselves.
       */
      const unsigned flag_subreg = inst->flag_subreg +
         (devinfo->ver >= 7 ? 0 : inst->group / 16);
      elk_set_default_flag_reg(p, flag_subreg / 2, flag_subreg % 2);
      elk_set_default_saturate(p, inst->saturate);
      elk_set_default_mask_control(p, inst->force_writemask_all);
      elk_set_default_acc_write_control(p, inst->writes_accumulator);

      unsigned exec_size = inst->exec_size;
      if (devinfo->verx10 == 70 &&
          (get_exec_type_size(inst) == 8 || type_sz(inst->dst.type) == 8)) {
         exec_size *= 2;
      }

      elk_set_default_exec_size(p, cvt(exec_size) - 1);

      assert(inst->force_writemask_all || inst->exec_size >= 4);
      assert(inst->force_writemask_all || inst->group % inst->exec_size == 0);
      assert(inst->base_mrf + inst->mlen <= ELK_MAX_MRF(devinfo->ver));
      assert(inst->mlen <= ELK_MAX_MSG_LENGTH * reg_unit(devinfo));

      switch (inst->opcode) {
      case ELK_OPCODE_MOV:
	 elk_MOV(p, dst, src[0]);
	 break;
      case ELK_OPCODE_ADD:
	 elk_ADD(p, dst, src[0], src[1]);
	 break;
      case ELK_OPCODE_MUL:
	 elk_MUL(p, dst, src[0], src[1]);
	 break;
      case ELK_OPCODE_AVG:
	 elk_AVG(p, dst, src[0], src[1]);
	 break;
      case ELK_OPCODE_MACH:
	 elk_MACH(p, dst, src[0], src[1]);
	 break;

      case ELK_OPCODE_LINE:
         elk_LINE(p, dst, src[0], src[1]);
         break;

      case ELK_OPCODE_MAD:
         assert(devinfo->ver >= 6);
         elk_set_default_access_mode(p, ELK_ALIGN_16);
         elk_MAD(p, dst, src[0], src[1], src[2]);
	 break;

      case ELK_OPCODE_LRP:
         assert(devinfo->ver >= 6);
         elk_set_default_access_mode(p, ELK_ALIGN_16);
         elk_LRP(p, dst, src[0], src[1], src[2]);
	 break;

      case ELK_OPCODE_FRC:
	 elk_FRC(p, dst, src[0]);
	 break;
      case ELK_OPCODE_RNDD:
	 elk_RNDD(p, dst, src[0]);
	 break;
      case ELK_OPCODE_RNDE:
	 elk_RNDE(p, dst, src[0]);
	 break;
      case ELK_OPCODE_RNDZ:
	 elk_RNDZ(p, dst, src[0]);
	 break;

      case ELK_OPCODE_AND:
	 elk_AND(p, dst, src[0], src[1]);
	 break;
      case ELK_OPCODE_OR:
	 elk_OR(p, dst, src[0], src[1]);
	 break;
      case ELK_OPCODE_XOR:
	 elk_XOR(p, dst, src[0], src[1]);
	 break;
      case ELK_OPCODE_NOT:
	 elk_NOT(p, dst, src[0]);
	 break;
      case ELK_OPCODE_ASR:
	 elk_ASR(p, dst, src[0], src[1]);
	 break;
      case ELK_OPCODE_SHR:
	 elk_SHR(p, dst, src[0], src[1]);
	 break;
      case ELK_OPCODE_SHL:
	 elk_SHL(p, dst, src[0], src[1]);
	 break;
      case ELK_OPCODE_F32TO16:
         elk_F32TO16(p, dst, src[0]);
         break;
      case ELK_OPCODE_F16TO32:
         elk_F16TO32(p, dst, src[0]);
         break;
      case ELK_OPCODE_CMP:
         if (inst->exec_size >= 16 && devinfo->verx10 == 70 &&
             dst.file == ELK_ARCHITECTURE_REGISTER_FILE) {
            /* For unknown reasons the WaCMPInstFlagDepClearedEarly workaround
             * implemented in the compiler is not sufficient. Overriding the
             * type when the destination is the null register is necessary but
             * not sufficient by itself.
             */
            dst.type = ELK_REGISTER_TYPE_D;
         }
         elk_CMP(p, dst, inst->conditional_mod, src[0], src[1]);
	 break;
      case ELK_OPCODE_CMPN:
         if (inst->exec_size >= 16 && devinfo->verx10 == 70 &&
             dst.file == ELK_ARCHITECTURE_REGISTER_FILE) {
            /* For unknown reasons the WaCMPInstFlagDepClearedEarly workaround
             * implemented in the compiler is not sufficient. Overriding the
             * type when the destination is the null register is necessary but
             * not sufficient by itself.
             */
            dst.type = ELK_REGISTER_TYPE_D;
         }
         elk_CMPN(p, dst, inst->conditional_mod, src[0], src[1]);
         break;
      case ELK_OPCODE_SEL:
	 elk_SEL(p, dst, src[0], src[1]);
	 break;
      case ELK_OPCODE_CSEL:
         assert(devinfo->ver >= 8);
         elk_set_default_access_mode(p, ELK_ALIGN_16);
         elk_CSEL(p, dst, src[0], src[1], src[2]);
         break;
      case ELK_OPCODE_BFREV:
         assert(devinfo->ver >= 7);
         elk_BFREV(p, retype(dst, ELK_REGISTER_TYPE_UD),
                   retype(src[0], ELK_REGISTER_TYPE_UD));
         break;
      case ELK_OPCODE_FBH:
         assert(devinfo->ver >= 7);
         elk_FBH(p, retype(dst, src[0].type), src[0]);
         break;
      case ELK_OPCODE_FBL:
         assert(devinfo->ver >= 7);
         elk_FBL(p, retype(dst, ELK_REGISTER_TYPE_UD),
                 retype(src[0], ELK_REGISTER_TYPE_UD));
         break;
      case ELK_OPCODE_LZD:
         elk_LZD(p, dst, src[0]);
         break;
      case ELK_OPCODE_CBIT:
         assert(devinfo->ver >= 7);
         elk_CBIT(p, retype(dst, ELK_REGISTER_TYPE_UD),
                  retype(src[0], ELK_REGISTER_TYPE_UD));
         break;
      case ELK_OPCODE_ADDC:
         assert(devinfo->ver >= 7);
         elk_ADDC(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_SUBB:
         assert(devinfo->ver >= 7);
         elk_SUBB(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_MAC:
         elk_MAC(p, dst, src[0], src[1]);
         break;

      case ELK_OPCODE_BFE:
         assert(devinfo->ver >= 7);
         elk_set_default_access_mode(p, ELK_ALIGN_16);
         elk_BFE(p, dst, src[0], src[1], src[2]);
         break;

      case ELK_OPCODE_BFI1:
         assert(devinfo->ver >= 7);
         elk_BFI1(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_BFI2:
         assert(devinfo->ver >= 7);
         elk_set_default_access_mode(p, ELK_ALIGN_16);
         elk_BFI2(p, dst, src[0], src[1], src[2]);
         break;

      case ELK_OPCODE_IF:
	 if (inst->src[0].file != BAD_FILE) {
	    /* The instruction has an embedded compare (only allowed on gfx6) */
	    assert(devinfo->ver == 6);
	    elk_gfx6_IF(p, inst->conditional_mod, src[0], src[1]);
	 } else {
	    elk_IF(p, elk_get_default_exec_size(p));
	 }
	 break;

      case ELK_OPCODE_ELSE:
	 elk_ELSE(p);
	 break;
      case ELK_OPCODE_ENDIF:
	 elk_ENDIF(p);
	 break;

      case ELK_OPCODE_DO:
	 elk_DO(p, elk_get_default_exec_size(p));
	 break;

      case ELK_OPCODE_BREAK:
	 elk_BREAK(p);
	 break;
      case ELK_OPCODE_CONTINUE:
         elk_CONT(p);
	 break;

      case ELK_OPCODE_WHILE:
	 elk_WHILE(p);
         loop_count++;
	 break;

      case ELK_SHADER_OPCODE_RCP:
      case ELK_SHADER_OPCODE_RSQ:
      case ELK_SHADER_OPCODE_SQRT:
      case ELK_SHADER_OPCODE_EXP2:
      case ELK_SHADER_OPCODE_LOG2:
      case ELK_SHADER_OPCODE_SIN:
      case ELK_SHADER_OPCODE_COS:
         assert(inst->conditional_mod == ELK_CONDITIONAL_NONE);
	 if (devinfo->ver >= 6) {
            assert(inst->mlen == 0);
            assert(devinfo->ver >= 7 || inst->exec_size == 8);
            elk_gfx6_math(p, dst, elk_math_function(inst->opcode),
                      src[0], elk_null_reg());
	 } else {
            assert(inst->mlen >= 1);
            assert(devinfo->ver == 5 || devinfo->platform == INTEL_PLATFORM_G4X || inst->exec_size == 8);
            elk_gfx4_math(p, dst,
                      elk_math_function(inst->opcode),
                      inst->base_mrf, src[0],
                      ELK_MATH_PRECISION_FULL);
            send_count++;
	 }
	 break;
      case ELK_SHADER_OPCODE_INT_QUOTIENT:
      case ELK_SHADER_OPCODE_INT_REMAINDER:
      case ELK_SHADER_OPCODE_POW:
         assert(inst->conditional_mod == ELK_CONDITIONAL_NONE);
         if (devinfo->ver >= 6) {
            assert(inst->mlen == 0);
            assert((devinfo->ver >= 7 && inst->opcode == ELK_SHADER_OPCODE_POW) ||
                   inst->exec_size == 8);
            elk_gfx6_math(p, dst, elk_math_function(inst->opcode), src[0], src[1]);
         } else {
            assert(inst->mlen >= 1);
            assert(inst->exec_size == 8);
            elk_gfx4_math(p, dst, elk_math_function(inst->opcode),
                      inst->base_mrf, src[0],
                      ELK_MATH_PRECISION_FULL);
            send_count++;
	 }
	 break;
      case ELK_FS_OPCODE_LINTERP:
	 multiple_instructions_emitted = generate_linterp(inst, dst, src);
	 break;
      case ELK_FS_OPCODE_PIXEL_X:
         assert(src[0].type == ELK_REGISTER_TYPE_UW);
         assert(src[1].type == ELK_REGISTER_TYPE_UW);
         src[0].subnr = 0 * type_sz(src[0].type);
         if (src[1].file == ELK_IMMEDIATE_VALUE) {
            assert(src[1].ud == 0);
            elk_MOV(p, dst, stride(src[0], 8, 4, 1));
         } else {
            /* Coarse pixel case */
            elk_ADD(p, dst, stride(src[0], 8, 4, 1), src[1]);
         }
         break;
      case ELK_FS_OPCODE_PIXEL_Y:
         assert(src[0].type == ELK_REGISTER_TYPE_UW);
         assert(src[1].type == ELK_REGISTER_TYPE_UW);
         src[0].subnr = 4 * type_sz(src[0].type);
         if (src[1].file == ELK_IMMEDIATE_VALUE) {
            assert(src[1].ud == 0);
            elk_MOV(p, dst, stride(src[0], 8, 4, 1));
         } else {
            /* Coarse pixel case */
            elk_ADD(p, dst, stride(src[0], 8, 4, 1), src[1]);
         }
         break;

      case ELK_SHADER_OPCODE_SEND:
         generate_send(inst, dst, src[0], src[1]);
         send_count++;
         break;

      case ELK_SHADER_OPCODE_TEX:
      case ELK_FS_OPCODE_TXB:
      case ELK_SHADER_OPCODE_TXD:
      case ELK_SHADER_OPCODE_TXF:
      case ELK_SHADER_OPCODE_TXF_CMS:
      case ELK_SHADER_OPCODE_TXL:
      case ELK_SHADER_OPCODE_TXS:
      case ELK_SHADER_OPCODE_LOD:
      case ELK_SHADER_OPCODE_TG4:
      case ELK_SHADER_OPCODE_SAMPLEINFO:
         assert(inst->src[0].file == BAD_FILE);
         generate_tex(inst, dst, src[1], src[2]);
         send_count++;
         break;

      case ELK_FS_OPCODE_DDX_COARSE:
      case ELK_FS_OPCODE_DDX_FINE:
         generate_ddx(inst, dst, src[0]);
         break;
      case ELK_FS_OPCODE_DDY_COARSE:
      case ELK_FS_OPCODE_DDY_FINE:
         generate_ddy(inst, dst, src[0]);
	 break;

      case ELK_SHADER_OPCODE_GFX4_SCRATCH_WRITE:
	 generate_scratch_write(inst, src[0]);
         send_count++;
	 break;

      case ELK_SHADER_OPCODE_GFX4_SCRATCH_READ:
	 generate_scratch_read(inst, dst);
         send_count++;
	 break;

      case ELK_SHADER_OPCODE_GFX7_SCRATCH_READ:
	 generate_scratch_read_gfx7(inst, dst);
         send_count++;
	 break;

      case ELK_SHADER_OPCODE_SCRATCH_HEADER:
         generate_scratch_header(inst, dst);
         break;

      case ELK_SHADER_OPCODE_MOV_INDIRECT:
         generate_mov_indirect(inst, dst, src[0], src[1]);
         break;

      case ELK_SHADER_OPCODE_MOV_RELOC_IMM:
         assert(src[0].file == ELK_IMMEDIATE_VALUE);
         elk_MOV_reloc_imm(p, dst, dst.type, src[0].ud);
         break;

      case ELK_FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD:
         assert(inst->force_writemask_all);
	 generate_uniform_pull_constant_load(inst, dst,
                                             src[PULL_UNIFORM_CONSTANT_SRC_SURFACE],
                                             src[PULL_UNIFORM_CONSTANT_SRC_OFFSET]);
         send_count++;
	 break;

      case ELK_FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_GFX4:
	 generate_varying_pull_constant_load_gfx4(inst, dst, src[0]);
         send_count++;
	 break;

      case ELK_FS_OPCODE_REP_FB_WRITE:
      case ELK_FS_OPCODE_FB_WRITE:
	 generate_fb_write(inst, src[0]);
         send_count++;
	 break;

      case ELK_OPCODE_HALT:
         generate_halt(inst);
         break;

      case ELK_SHADER_OPCODE_INTERLOCK:
      case ELK_SHADER_OPCODE_MEMORY_FENCE: {
         assert(src[1].file == ELK_IMMEDIATE_VALUE);
         assert(src[2].file == ELK_IMMEDIATE_VALUE);

         const enum elk_opcode send_op = inst->opcode == ELK_SHADER_OPCODE_INTERLOCK ?
            ELK_OPCODE_SENDC : ELK_OPCODE_SEND;

         elk_memory_fence(p, dst, src[0], send_op,
                          elk_message_target(inst->sfid),
                          inst->desc,
                          /* commit_enable */ src[1].ud,
                          /* bti */ src[2].ud);
         send_count++;
         break;
      }

      case ELK_FS_OPCODE_SCHEDULING_FENCE:
         if (inst->sources == 0) {
            if (unlikely(debug_flag))
               elk_disasm_info->use_tail = true;
            break;
         }

         for (unsigned i = 0; i < inst->sources; i++) {
            /* Emit a MOV to force a stall until the instruction producing the
             * registers finishes.
             */
            elk_MOV(p, retype(elk_null_reg(), ELK_REGISTER_TYPE_UW),
                    retype(src[i], ELK_REGISTER_TYPE_UW));
         }

         if (inst->sources > 1)
            multiple_instructions_emitted = true;

         break;

      case ELK_SHADER_OPCODE_FIND_LIVE_CHANNEL:
         elk_find_live_channel(p, dst, false);
         break;
      case ELK_SHADER_OPCODE_FIND_LAST_LIVE_CHANNEL:
         elk_find_live_channel(p, dst, true);
         break;

      case ELK_FS_OPCODE_LOAD_LIVE_CHANNELS: {
         assert(devinfo->ver >= 8);
         assert(inst->force_writemask_all && inst->group == 0);
         assert(inst->dst.file == BAD_FILE);
         elk_set_default_exec_size(p, ELK_EXECUTE_1);
         elk_MOV(p, retype(elk_flag_subreg(inst->flag_subreg),
                           ELK_REGISTER_TYPE_UD),
                 retype(elk_mask_reg(0), ELK_REGISTER_TYPE_UD));
         break;
      }
      case ELK_SHADER_OPCODE_BROADCAST:
         assert(inst->force_writemask_all);
         elk_broadcast(p, dst, src[0], src[1]);
         break;

      case ELK_SHADER_OPCODE_SHUFFLE:
         generate_shuffle(inst, dst, src[0], src[1]);
         break;

      case ELK_SHADER_OPCODE_SEL_EXEC:
         assert(inst->force_writemask_all);
         assert(devinfo->has_64bit_float || type_sz(dst.type) <= 4);
         elk_set_default_mask_control(p, ELK_MASK_DISABLE);
         elk_MOV(p, dst, src[1]);
         elk_set_default_mask_control(p, ELK_MASK_ENABLE);
         elk_MOV(p, dst, src[0]);
         break;

      case ELK_SHADER_OPCODE_QUAD_SWIZZLE:
         assert(src[1].file == ELK_IMMEDIATE_VALUE);
         assert(src[1].type == ELK_REGISTER_TYPE_UD);
         generate_quad_swizzle(inst, dst, src[0], src[1].ud);
         break;

      case ELK_SHADER_OPCODE_CLUSTER_BROADCAST: {
         assert((devinfo->platform != INTEL_PLATFORM_CHV &&
                 devinfo->has_64bit_float) || type_sz(src[0].type) <= 4);
         assert(!src[0].negate && !src[0].abs);
         assert(src[1].file == ELK_IMMEDIATE_VALUE);
         assert(src[1].type == ELK_REGISTER_TYPE_UD);
         assert(src[2].file == ELK_IMMEDIATE_VALUE);
         assert(src[2].type == ELK_REGISTER_TYPE_UD);
         const unsigned component = src[1].ud;
         const unsigned cluster_size = src[2].ud;
         assert(inst->src[0].file != ARF && inst->src[0].file != FIXED_GRF);
         const unsigned s = inst->src[0].stride;
         unsigned vstride = cluster_size * s;
         unsigned width = cluster_size;

         /* The maximum exec_size is 32, but the maximum width is only 16. */
         if (inst->exec_size == width) {
            vstride = 0;
            width = 1;
         }

         struct elk_reg strided = stride(suboffset(src[0], component * s),
                                         vstride, width, 0);
         elk_MOV(p, dst, strided);
         break;
      }

      case ELK_FS_OPCODE_SET_SAMPLE_ID:
         generate_set_sample_id(inst, dst, src[0], src[1]);
         break;

      case ELK_SHADER_OPCODE_HALT_TARGET:
         /* This is the place where the final HALT needs to be inserted if
          * we've emitted any discards.  If not, this will emit no code.
          */
         if (!patch_halt_jumps()) {
            if (unlikely(debug_flag)) {
               elk_disasm_info->use_tail = true;
            }
         }
         break;

      case ELK_CS_OPCODE_CS_TERMINATE:
         generate_cs_terminate(inst, src[0]);
         send_count++;
         break;

      case ELK_SHADER_OPCODE_BARRIER:
	 generate_barrier(inst, src[0]);
         send_count++;
	 break;

      case ELK_OPCODE_DIM:
         assert(devinfo->platform == INTEL_PLATFORM_HSW);
         assert(src[0].type == ELK_REGISTER_TYPE_DF);
         assert(dst.type == ELK_REGISTER_TYPE_DF);
         elk_DIM(p, dst, retype(src[0], ELK_REGISTER_TYPE_F));
         break;

      case ELK_SHADER_OPCODE_RND_MODE: {
         assert(src[0].file == ELK_IMMEDIATE_VALUE);
         /*
          * Changes the floating point rounding mode updating the control
          * register field defined at cr0.0[5-6] bits.
          */
         enum elk_rnd_mode mode =
            (enum elk_rnd_mode) (src[0].d << ELK_CR0_RND_MODE_SHIFT);
         elk_float_controls_mode(p, mode, ELK_CR0_RND_MODE_MASK);
      }
         break;

      case ELK_SHADER_OPCODE_FLOAT_CONTROL_MODE:
         assert(src[0].file == ELK_IMMEDIATE_VALUE);
         assert(src[1].file == ELK_IMMEDIATE_VALUE);
         elk_float_controls_mode(p, src[0].d, src[1].d);
         break;

      case ELK_SHADER_OPCODE_READ_SR_REG:
         elk_MOV(p, dst, elk_sr0_reg(src[0].ud));
         break;

      default:
         unreachable("Unsupported opcode");

      case ELK_SHADER_OPCODE_LOAD_PAYLOAD:
         unreachable("Should be lowered by lower_load_payload()");
      }

      if (multiple_instructions_emitted)
         continue;

      if (inst->no_dd_clear || inst->no_dd_check || inst->conditional_mod) {
         assert(p->next_insn_offset == last_insn_offset + 16 ||
                !"conditional_mod, no_dd_check, or no_dd_clear set for IR "
                 "emitting more than 1 instruction");

         elk_inst *last = &p->store[last_insn_offset / 16];

         if (inst->conditional_mod)
            elk_inst_set_cond_modifier(p->devinfo, last, inst->conditional_mod);
         elk_inst_set_no_dd_clear(p->devinfo, last, inst->no_dd_clear);
         elk_inst_set_no_dd_check(p->devinfo, last, inst->no_dd_check);
      }
   }

   elk_set_uip_jip(p, start_offset);

   /* end of program sentinel */
   elk_disasm_new_inst_group(elk_disasm_info, p->next_insn_offset);

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
      elk_validate_instructions(&compiler->isa, p->store,
                                start_offset,
                                p->next_insn_offset,
                                elk_disasm_info);

   int before_size = p->next_insn_offset - start_offset;
   elk_compact_instructions(p, start_offset, elk_disasm_info);
   int after_size = p->next_insn_offset - start_offset;

   bool dump_shader_bin = elk_should_dump_shader_bin();
   unsigned char sha1[21];
   char sha1buf[41];

   if (unlikely(debug_flag || dump_shader_bin)) {
      _mesa_sha1_compute(p->store + start_offset / sizeof(elk_inst),
                         after_size, sha1);
      _mesa_sha1_format(sha1buf, sha1);
   }

   if (unlikely(dump_shader_bin))
      elk_dump_shader_bin(p->store, start_offset, p->next_insn_offset,
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

      /* overriding the shader makes elk_disasm_info invalid */
      if (!elk_try_override_assembly(p, start_offset, sha1buf)) {
         elk_dump_assembly(p->store, start_offset, p->next_insn_offset,
                       elk_disasm_info, perf.block_latency);
      } else {
         fprintf(stderr, "Successfully overrode shader with sha1 %s\n\n", sha1buf);
      }
   }
   ralloc_free(elk_disasm_info);
#ifndef NDEBUG
   if (!validated && !debug_flag) {
      fprintf(stderr,
            "Validation failed. Rerun with INTEL_DEBUG=shaders to get more information.\n");
   }
#endif
   assert(validated);

   elk_shader_debug_log(compiler, params->log_data,
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
elk_fs_generator::add_const_data(void *data, unsigned size)
{
   assert(prog_data->const_data_size == 0);
   if (size > 0) {
      prog_data->const_data_size = size;
      prog_data->const_data_offset = elk_append_data(p, data, size, 32);
   }
}

const unsigned *
elk_fs_generator::get_assembly()
{
   prog_data->relocs = elk_get_shader_relocs(p, &prog_data->num_relocs);

   return elk_get_program(p, &prog_data->program_size);
}
