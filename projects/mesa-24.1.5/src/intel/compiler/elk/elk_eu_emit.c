/*
 Copyright (C) Intel Corp.  2006.  All Rights Reserved.
 Intel funded Tungsten Graphics to
 develop this 3D driver.

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice (including the
 next paragraph) shall be included in all copies or substantial
 portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 **********************************************************************/
 /*
  * Authors:
  *   Keith Whitwell <keithw@vmware.com>
  */


#include "elk_eu_defines.h"
#include "elk_eu.h"

#include "util/ralloc.h"

/**
 * Prior to Sandybridge, the SEND instruction accepted non-MRF source
 * registers, implicitly moving the operand to a message register.
 *
 * On Sandybridge, this is no longer the case.  This function performs the
 * explicit move; it should be called before emitting a SEND instruction.
 */
void
elk_gfx6_resolve_implied_move(struct elk_codegen *p,
			  struct elk_reg *src,
			  unsigned msg_reg_nr)
{
   const struct intel_device_info *devinfo = p->devinfo;
   if (devinfo->ver < 6)
      return;

   if (src->file == ELK_MESSAGE_REGISTER_FILE)
      return;

   if (src->file != ELK_ARCHITECTURE_REGISTER_FILE || src->nr != ELK_ARF_NULL) {
      elk_push_insn_state(p);
      elk_set_default_exec_size(p, ELK_EXECUTE_8);
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);
      elk_set_default_compression_control(p, ELK_COMPRESSION_NONE);
      elk_MOV(p, retype(elk_message_reg(msg_reg_nr), ELK_REGISTER_TYPE_UD),
	      retype(*src, ELK_REGISTER_TYPE_UD));
      elk_pop_insn_state(p);
   }
   *src = elk_message_reg(msg_reg_nr);
}

static void
gfx7_convert_mrf_to_grf(struct elk_codegen *p, struct elk_reg *reg)
{
   /* From the Ivybridge PRM, Volume 4 Part 3, page 218 ("send"):
    * "The send with EOT should use register space R112-R127 for <src>. This is
    *  to enable loading of a new thread into the same slot while the message
    *  with EOT for current thread is pending dispatch."
    *
    * Since we're pretending to have 16 MRFs anyway, we may as well use the
    * registers required for messages with EOT.
    */
   const struct intel_device_info *devinfo = p->devinfo;
   if (devinfo->ver >= 7 && reg->file == ELK_MESSAGE_REGISTER_FILE) {
      reg->file = ELK_GENERAL_REGISTER_FILE;
      reg->nr += GFX7_MRF_HACK_START;
   }
}

void
elk_set_dest(struct elk_codegen *p, elk_inst *inst, struct elk_reg dest)
{
   const struct intel_device_info *devinfo = p->devinfo;

   if (dest.file == ELK_MESSAGE_REGISTER_FILE)
      assert((dest.nr & ~ELK_MRF_COMPR4) < ELK_MAX_MRF(devinfo->ver));
   else if (dest.file == ELK_GENERAL_REGISTER_FILE)
      assert(dest.nr < XE2_MAX_GRF);

   /* The hardware has a restriction where a destination of size Byte with
    * a stride of 1 is only allowed for a packed byte MOV. For any other
    * instruction, the stride must be at least 2, even when the destination
    * is the NULL register.
    */
   if (dest.file == ELK_ARCHITECTURE_REGISTER_FILE &&
       dest.nr == ELK_ARF_NULL &&
       type_sz(dest.type) == 1 &&
       dest.hstride == ELK_HORIZONTAL_STRIDE_1) {
      dest.hstride = ELK_HORIZONTAL_STRIDE_2;
   }

   gfx7_convert_mrf_to_grf(p, &dest);

   elk_inst_set_dst_file_type(devinfo, inst, dest.file, dest.type);
   elk_inst_set_dst_address_mode(devinfo, inst, dest.address_mode);

   if (dest.address_mode == ELK_ADDRESS_DIRECT) {
      elk_inst_set_dst_da_reg_nr(devinfo, inst, phys_nr(devinfo, dest));

      if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
         elk_inst_set_dst_da1_subreg_nr(devinfo, inst, phys_subnr(devinfo, dest));
         if (dest.hstride == ELK_HORIZONTAL_STRIDE_0)
            dest.hstride = ELK_HORIZONTAL_STRIDE_1;
         elk_inst_set_dst_hstride(devinfo, inst, dest.hstride);
      } else {
         elk_inst_set_dst_da16_subreg_nr(devinfo, inst, dest.subnr / 16);
         elk_inst_set_da16_writemask(devinfo, inst, dest.writemask);
         if (dest.file == ELK_GENERAL_REGISTER_FILE ||
             dest.file == ELK_MESSAGE_REGISTER_FILE) {
            assert(dest.writemask != 0);
         }
         /* From the Ivybridge PRM, Vol 4, Part 3, Section 5.2.4.1:
          *    Although Dst.HorzStride is a don't care for Align16, HW needs
          *    this to be programmed as "01".
          */
         elk_inst_set_dst_hstride(devinfo, inst, 1);
      }
   } else {
      elk_inst_set_dst_ia_subreg_nr(devinfo, inst, phys_subnr(devinfo, dest));

      /* These are different sizes in align1 vs align16:
       */
      if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
         elk_inst_set_dst_ia1_addr_imm(devinfo, inst,
                                       dest.indirect_offset);
         if (dest.hstride == ELK_HORIZONTAL_STRIDE_0)
            dest.hstride = ELK_HORIZONTAL_STRIDE_1;
         elk_inst_set_dst_hstride(devinfo, inst, dest.hstride);
      } else {
         elk_inst_set_dst_ia16_addr_imm(devinfo, inst,
                                        dest.indirect_offset);
         /* even ignored in da16, still need to set as '01' */
         elk_inst_set_dst_hstride(devinfo, inst, 1);
      }
   }

   /* Generators should set a default exec_size of either 8 (SIMD4x2 or SIMD8)
    * or 16 (SIMD16), as that's normally correct.  However, when dealing with
    * small registers, it can be useful for us to automatically reduce it to
    * match the register size.
    */
   if (p->automatic_exec_sizes) {
      /*
       * In platforms that support fp64 we can emit instructions with a width
       * of 4 that need two SIMD8 registers and an exec_size of 8 or 16. In
       * these cases we need to make sure that these instructions have their
       * exec sizes set properly when they are emitted and we can't rely on
       * this code to fix it.
       */
      bool fix_exec_size;
      if (devinfo->ver >= 6)
         fix_exec_size = dest.width < ELK_EXECUTE_4;
      else
         fix_exec_size = dest.width < ELK_EXECUTE_8;

      if (fix_exec_size)
         elk_inst_set_exec_size(devinfo, inst, dest.width);
   }
}

void
elk_set_src0(struct elk_codegen *p, elk_inst *inst, struct elk_reg reg)
{
   const struct intel_device_info *devinfo = p->devinfo;

   if (reg.file == ELK_MESSAGE_REGISTER_FILE)
      assert((reg.nr & ~ELK_MRF_COMPR4) < ELK_MAX_MRF(devinfo->ver));
   else if (reg.file == ELK_GENERAL_REGISTER_FILE)
      assert(reg.nr < XE2_MAX_GRF);

   gfx7_convert_mrf_to_grf(p, &reg);

   if (devinfo->ver >= 6 &&
       (elk_inst_opcode(p->isa, inst) == ELK_OPCODE_SEND ||
        elk_inst_opcode(p->isa, inst) == ELK_OPCODE_SENDC)) {
      /* Any source modifiers or regions will be ignored, since this just
       * identifies the MRF/GRF to start reading the message contents from.
       * Check for some likely failures.
       */
      assert(!reg.negate);
      assert(!reg.abs);
      assert(reg.address_mode == ELK_ADDRESS_DIRECT);
   }

   elk_inst_set_src0_file_type(devinfo, inst, reg.file, reg.type);
   elk_inst_set_src0_abs(devinfo, inst, reg.abs);
   elk_inst_set_src0_negate(devinfo, inst, reg.negate);
   elk_inst_set_src0_address_mode(devinfo, inst, reg.address_mode);

   if (reg.file == ELK_IMMEDIATE_VALUE) {
      if (reg.type == ELK_REGISTER_TYPE_DF ||
          elk_inst_opcode(p->isa, inst) == ELK_OPCODE_DIM)
         elk_inst_set_imm_df(devinfo, inst, reg.df);
      else if (reg.type == ELK_REGISTER_TYPE_UQ ||
               reg.type == ELK_REGISTER_TYPE_Q)
         elk_inst_set_imm_uq(devinfo, inst, reg.u64);
      else
         elk_inst_set_imm_ud(devinfo, inst, reg.ud);

      if (type_sz(reg.type) < 8) {
         elk_inst_set_src1_reg_file(devinfo, inst,
                                    ELK_ARCHITECTURE_REGISTER_FILE);
         elk_inst_set_src1_reg_hw_type(devinfo, inst,
                                       elk_inst_src0_reg_hw_type(devinfo, inst));
      }
   } else {
      if (reg.address_mode == ELK_ADDRESS_DIRECT) {
         elk_inst_set_src0_da_reg_nr(devinfo, inst, phys_nr(devinfo, reg));
         if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
            elk_inst_set_src0_da1_subreg_nr(devinfo, inst, phys_subnr(devinfo, reg));
         } else {
            elk_inst_set_src0_da16_subreg_nr(devinfo, inst, reg.subnr / 16);
         }
      } else {
         elk_inst_set_src0_ia_subreg_nr(devinfo, inst, phys_subnr(devinfo, reg));

         if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
            elk_inst_set_src0_ia1_addr_imm(devinfo, inst, reg.indirect_offset);
         } else {
            elk_inst_set_src0_ia16_addr_imm(devinfo, inst, reg.indirect_offset);
         }
      }

      if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
         if (reg.width == ELK_WIDTH_1 &&
             elk_inst_exec_size(devinfo, inst) == ELK_EXECUTE_1) {
            elk_inst_set_src0_hstride(devinfo, inst, ELK_HORIZONTAL_STRIDE_0);
            elk_inst_set_src0_width(devinfo, inst, ELK_WIDTH_1);
            elk_inst_set_src0_vstride(devinfo, inst, ELK_VERTICAL_STRIDE_0);
         } else {
            elk_inst_set_src0_hstride(devinfo, inst, reg.hstride);
            elk_inst_set_src0_width(devinfo, inst, reg.width);
            elk_inst_set_src0_vstride(devinfo, inst, reg.vstride);
         }
      } else {
         elk_inst_set_src0_da16_swiz_x(devinfo, inst,
            ELK_GET_SWZ(reg.swizzle, ELK_CHANNEL_X));
         elk_inst_set_src0_da16_swiz_y(devinfo, inst,
            ELK_GET_SWZ(reg.swizzle, ELK_CHANNEL_Y));
         elk_inst_set_src0_da16_swiz_z(devinfo, inst,
            ELK_GET_SWZ(reg.swizzle, ELK_CHANNEL_Z));
         elk_inst_set_src0_da16_swiz_w(devinfo, inst,
            ELK_GET_SWZ(reg.swizzle, ELK_CHANNEL_W));

         if (reg.vstride == ELK_VERTICAL_STRIDE_8) {
            /* This is an oddity of the fact we're using the same
             * descriptions for registers in align_16 as align_1:
             */
            elk_inst_set_src0_vstride(devinfo, inst, ELK_VERTICAL_STRIDE_4);
         } else if (devinfo->verx10 == 70 &&
                    reg.type == ELK_REGISTER_TYPE_DF &&
                    reg.vstride == ELK_VERTICAL_STRIDE_2) {
            /* From SNB PRM:
             *
             * "For Align16 access mode, only encodings of 0000 and 0011
             *  are allowed. Other codes are reserved."
             *
             * Presumably the DevSNB behavior applies to IVB as well.
             */
            elk_inst_set_src0_vstride(devinfo, inst, ELK_VERTICAL_STRIDE_4);
         } else {
            elk_inst_set_src0_vstride(devinfo, inst, reg.vstride);
         }
      }
   }
}


void
elk_set_src1(struct elk_codegen *p, elk_inst *inst, struct elk_reg reg)
{
   const struct intel_device_info *devinfo = p->devinfo;

   if (reg.file == ELK_GENERAL_REGISTER_FILE)
      assert(reg.nr < XE2_MAX_GRF);

   {
      /* From the IVB PRM Vol. 4, Pt. 3, Section 3.3.3.5:
       *
       *    "Accumulator registers may be accessed explicitly as src0
       *    operands only."
       */
      assert(reg.file != ELK_ARCHITECTURE_REGISTER_FILE ||
             reg.nr != ELK_ARF_ACCUMULATOR);

      gfx7_convert_mrf_to_grf(p, &reg);
      assert(reg.file != ELK_MESSAGE_REGISTER_FILE);

      elk_inst_set_src1_file_type(devinfo, inst, reg.file, reg.type);
      elk_inst_set_src1_abs(devinfo, inst, reg.abs);
      elk_inst_set_src1_negate(devinfo, inst, reg.negate);

      /* Only src1 can be immediate in two-argument instructions.
       */
      assert(elk_inst_src0_reg_file(devinfo, inst) != ELK_IMMEDIATE_VALUE);

      if (reg.file == ELK_IMMEDIATE_VALUE) {
         /* two-argument instructions can only use 32-bit immediates */
         assert(type_sz(reg.type) < 8);
         elk_inst_set_imm_ud(devinfo, inst, reg.ud);
      } else {
         /* This is a hardware restriction, which may or may not be lifted
          * in the future:
          */
         assert (reg.address_mode == ELK_ADDRESS_DIRECT);
         /* assert (reg.file == ELK_GENERAL_REGISTER_FILE); */

         elk_inst_set_src1_da_reg_nr(devinfo, inst, phys_nr(devinfo, reg));
         if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
            elk_inst_set_src1_da1_subreg_nr(devinfo, inst, phys_subnr(devinfo, reg));
         } else {
            elk_inst_set_src1_da16_subreg_nr(devinfo, inst, reg.subnr / 16);
         }

         if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
            if (reg.width == ELK_WIDTH_1 &&
                elk_inst_exec_size(devinfo, inst) == ELK_EXECUTE_1) {
               elk_inst_set_src1_hstride(devinfo, inst, ELK_HORIZONTAL_STRIDE_0);
               elk_inst_set_src1_width(devinfo, inst, ELK_WIDTH_1);
               elk_inst_set_src1_vstride(devinfo, inst, ELK_VERTICAL_STRIDE_0);
            } else {
               elk_inst_set_src1_hstride(devinfo, inst, reg.hstride);
               elk_inst_set_src1_width(devinfo, inst, reg.width);
               elk_inst_set_src1_vstride(devinfo, inst, reg.vstride);
            }
         } else {
            elk_inst_set_src1_da16_swiz_x(devinfo, inst,
               ELK_GET_SWZ(reg.swizzle, ELK_CHANNEL_X));
            elk_inst_set_src1_da16_swiz_y(devinfo, inst,
               ELK_GET_SWZ(reg.swizzle, ELK_CHANNEL_Y));
            elk_inst_set_src1_da16_swiz_z(devinfo, inst,
               ELK_GET_SWZ(reg.swizzle, ELK_CHANNEL_Z));
            elk_inst_set_src1_da16_swiz_w(devinfo, inst,
               ELK_GET_SWZ(reg.swizzle, ELK_CHANNEL_W));

            if (reg.vstride == ELK_VERTICAL_STRIDE_8) {
               /* This is an oddity of the fact we're using the same
                * descriptions for registers in align_16 as align_1:
                */
               elk_inst_set_src1_vstride(devinfo, inst, ELK_VERTICAL_STRIDE_4);
            } else if (devinfo->verx10 == 70 &&
                       reg.type == ELK_REGISTER_TYPE_DF &&
                       reg.vstride == ELK_VERTICAL_STRIDE_2) {
               /* From SNB PRM:
                *
                * "For Align16 access mode, only encodings of 0000 and 0011
                *  are allowed. Other codes are reserved."
                *
                * Presumably the DevSNB behavior applies to IVB as well.
                */
               elk_inst_set_src1_vstride(devinfo, inst, ELK_VERTICAL_STRIDE_4);
            } else {
               elk_inst_set_src1_vstride(devinfo, inst, reg.vstride);
            }
         }
      }
   }
}

/**
 * Specify the descriptor and extended descriptor immediate for a SEND(C)
 * message instruction.
 */
void
elk_set_desc_ex(struct elk_codegen *p, elk_inst *inst,
                unsigned desc, unsigned ex_desc)
{
   const struct intel_device_info *devinfo = p->devinfo;
   assert(elk_inst_opcode(p->isa, inst) == ELK_OPCODE_SEND ||
          elk_inst_opcode(p->isa, inst) == ELK_OPCODE_SENDC);
   elk_inst_set_src1_file_type(devinfo, inst,
                               ELK_IMMEDIATE_VALUE, ELK_REGISTER_TYPE_UD);
   elk_inst_set_send_desc(devinfo, inst, desc);
}

static void elk_set_math_message( struct elk_codegen *p,
				  elk_inst *inst,
				  unsigned function,
				  unsigned integer_type,
				  bool low_precision,
				  unsigned dataType )
{
   const struct intel_device_info *devinfo = p->devinfo;
   unsigned msg_length;
   unsigned response_length;

   /* Infer message length from the function */
   switch (function) {
   case ELK_MATH_FUNCTION_POW:
   case ELK_MATH_FUNCTION_INT_DIV_QUOTIENT:
   case ELK_MATH_FUNCTION_INT_DIV_REMAINDER:
   case ELK_MATH_FUNCTION_INT_DIV_QUOTIENT_AND_REMAINDER:
      msg_length = 2;
      break;
   default:
      msg_length = 1;
      break;
   }

   /* Infer response length from the function */
   switch (function) {
   case ELK_MATH_FUNCTION_SINCOS:
   case ELK_MATH_FUNCTION_INT_DIV_QUOTIENT_AND_REMAINDER:
      response_length = 2;
      break;
   default:
      response_length = 1;
      break;
   }

   elk_set_desc(p, inst, elk_message_desc(
                   devinfo, msg_length, response_length, false));

   elk_inst_set_sfid(devinfo, inst, ELK_SFID_MATH);
   elk_inst_set_math_msg_function(devinfo, inst, function);
   elk_inst_set_math_msg_signed_int(devinfo, inst, integer_type);
   elk_inst_set_math_msg_precision(devinfo, inst, low_precision);
   elk_inst_set_math_msg_saturate(devinfo, inst, elk_inst_saturate(devinfo, inst));
   elk_inst_set_math_msg_data_type(devinfo, inst, dataType);
   elk_inst_set_saturate(devinfo, inst, 0);
}


static void elk_set_ff_sync_message(struct elk_codegen *p,
				    elk_inst *insn,
				    bool allocate,
				    unsigned response_length,
				    bool end_of_thread)
{
   const struct intel_device_info *devinfo = p->devinfo;

   elk_set_desc(p, insn, elk_message_desc(
                   devinfo, 1, response_length, true));

   elk_inst_set_sfid(devinfo, insn, ELK_SFID_URB);
   elk_inst_set_eot(devinfo, insn, end_of_thread);
   elk_inst_set_urb_opcode(devinfo, insn, 1); /* FF_SYNC */
   elk_inst_set_urb_allocate(devinfo, insn, allocate);
   /* The following fields are not used by FF_SYNC: */
   elk_inst_set_urb_global_offset(devinfo, insn, 0);
   elk_inst_set_urb_swizzle_control(devinfo, insn, 0);
   elk_inst_set_urb_used(devinfo, insn, 0);
   elk_inst_set_urb_complete(devinfo, insn, 0);
}

static void elk_set_urb_message( struct elk_codegen *p,
				 elk_inst *insn,
                                 enum elk_urb_write_flags flags,
				 unsigned msg_length,
				 unsigned response_length,
				 unsigned offset,
				 unsigned swizzle_control )
{
   const struct intel_device_info *devinfo = p->devinfo;

   assert(devinfo->ver < 7 || swizzle_control != ELK_URB_SWIZZLE_TRANSPOSE);
   assert(devinfo->ver < 7 || !(flags & ELK_URB_WRITE_ALLOCATE));
   assert(devinfo->ver >= 7 || !(flags & ELK_URB_WRITE_PER_SLOT_OFFSET));

   elk_set_desc(p, insn, elk_message_desc(
                   devinfo, msg_length, response_length, true));

   elk_inst_set_sfid(devinfo, insn, ELK_SFID_URB);
   elk_inst_set_eot(devinfo, insn, !!(flags & ELK_URB_WRITE_EOT));

   if (flags & ELK_URB_WRITE_OWORD) {
      assert(msg_length == 2); /* header + one OWORD of data */
      elk_inst_set_urb_opcode(devinfo, insn, ELK_URB_OPCODE_WRITE_OWORD);
   } else {
      elk_inst_set_urb_opcode(devinfo, insn, ELK_URB_OPCODE_WRITE_HWORD);
   }

   elk_inst_set_urb_global_offset(devinfo, insn, offset);
   elk_inst_set_urb_swizzle_control(devinfo, insn, swizzle_control);

   if (devinfo->ver < 8) {
      elk_inst_set_urb_complete(devinfo, insn, !!(flags & ELK_URB_WRITE_COMPLETE));
   }

   if (devinfo->ver < 7) {
      elk_inst_set_urb_allocate(devinfo, insn, !!(flags & ELK_URB_WRITE_ALLOCATE));
      elk_inst_set_urb_used(devinfo, insn, !(flags & ELK_URB_WRITE_UNUSED));
   } else {
      elk_inst_set_urb_per_slot_offset(devinfo, insn,
         !!(flags & ELK_URB_WRITE_PER_SLOT_OFFSET));
   }
}

static void
gfx7_set_dp_scratch_message(struct elk_codegen *p,
                            elk_inst *inst,
                            bool write,
                            bool dword,
                            bool invalidate_after_read,
                            unsigned num_regs,
                            unsigned addr_offset,
                            unsigned mlen,
                            unsigned rlen,
                            bool header_present)
{
   const struct intel_device_info *devinfo = p->devinfo;
   assert(num_regs == 1 || num_regs == 2 || num_regs == 4 ||
          (devinfo->ver >= 8 && num_regs == 8));
   const unsigned block_size = (devinfo->ver >= 8 ? util_logbase2(num_regs) :
                                num_regs - 1);

   elk_set_desc(p, inst, elk_message_desc(
                   devinfo, mlen, rlen, header_present));

   elk_inst_set_sfid(devinfo, inst, GFX7_SFID_DATAPORT_DATA_CACHE);
   elk_inst_set_dp_category(devinfo, inst, 1); /* Scratch Block Read/Write msgs */
   elk_inst_set_scratch_read_write(devinfo, inst, write);
   elk_inst_set_scratch_type(devinfo, inst, dword);
   elk_inst_set_scratch_invalidate_after_read(devinfo, inst, invalidate_after_read);
   elk_inst_set_scratch_block_size(devinfo, inst, block_size);
   elk_inst_set_scratch_addr_offset(devinfo, inst, addr_offset);
}

static void
elk_inst_set_state(const struct elk_isa_info *isa,
                   elk_inst *insn,
                   const struct elk_insn_state *state)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   elk_inst_set_exec_size(devinfo, insn, state->exec_size);
   elk_inst_set_group(devinfo, insn, state->group);
   elk_inst_set_compression(devinfo, insn, state->compressed);
   elk_inst_set_access_mode(devinfo, insn, state->access_mode);
   elk_inst_set_mask_control(devinfo, insn, state->mask_control);
   elk_inst_set_saturate(devinfo, insn, state->saturate);
   elk_inst_set_pred_control(devinfo, insn, state->predicate);
   elk_inst_set_pred_inv(devinfo, insn, state->pred_inv);

   if (elk_is_3src(isa, elk_inst_opcode(isa, insn)) &&
       state->access_mode == ELK_ALIGN_16) {
      elk_inst_set_3src_a16_flag_subreg_nr(devinfo, insn, state->flag_subreg % 2);
      if (devinfo->ver >= 7)
         elk_inst_set_3src_a16_flag_reg_nr(devinfo, insn, state->flag_subreg / 2);
   } else {
      elk_inst_set_flag_subreg_nr(devinfo, insn, state->flag_subreg % 2);
      if (devinfo->ver >= 7)
         elk_inst_set_flag_reg_nr(devinfo, insn, state->flag_subreg / 2);
   }

   if (devinfo->ver >= 6)
      elk_inst_set_acc_wr_control(devinfo, insn, state->acc_wr_control);
}

static elk_inst *
elk_append_insns(struct elk_codegen *p, unsigned nr_insn, unsigned alignment)
{
   assert(util_is_power_of_two_or_zero(sizeof(elk_inst)));
   assert(util_is_power_of_two_or_zero(alignment));
   const unsigned align_insn = MAX2(alignment / sizeof(elk_inst), 1);
   const unsigned start_insn = ALIGN(p->nr_insn, align_insn);
   const unsigned new_nr_insn = start_insn + nr_insn;

   if (p->store_size < new_nr_insn) {
      p->store_size = util_next_power_of_two(new_nr_insn * sizeof(elk_inst));
      p->store = reralloc(p->mem_ctx, p->store, elk_inst, p->store_size);
   }

   /* Memset any padding due to alignment to 0.  We don't want to be hashing
    * or caching a bunch of random bits we got from a memory allocation.
    */
   if (p->nr_insn < start_insn) {
      memset(&p->store[p->nr_insn], 0,
             (start_insn - p->nr_insn) * sizeof(elk_inst));
   }

   assert(p->next_insn_offset == p->nr_insn * sizeof(elk_inst));
   p->nr_insn = new_nr_insn;
   p->next_insn_offset = new_nr_insn * sizeof(elk_inst);

   return &p->store[start_insn];
}

void
elk_realign(struct elk_codegen *p, unsigned alignment)
{
   elk_append_insns(p, 0, alignment);
}

int
elk_append_data(struct elk_codegen *p, void *data,
                unsigned size, unsigned alignment)
{
   unsigned nr_insn = DIV_ROUND_UP(size, sizeof(elk_inst));
   void *dst = elk_append_insns(p, nr_insn, alignment);
   memcpy(dst, data, size);

   /* If it's not a whole number of instructions, memset the end */
   if (size < nr_insn * sizeof(elk_inst))
      memset(dst + size, 0, nr_insn * sizeof(elk_inst) - size);

   return dst - (void *)p->store;
}

#define next_insn elk_next_insn
elk_inst *
elk_next_insn(struct elk_codegen *p, unsigned opcode)
{
   elk_inst *insn = elk_append_insns(p, 1, sizeof(elk_inst));

   memset(insn, 0, sizeof(*insn));
   elk_inst_set_opcode(p->isa, insn, opcode);

   /* Apply the default instruction state */
   elk_inst_set_state(p->isa, insn, p->current);

   return insn;
}

void
elk_add_reloc(struct elk_codegen *p, uint32_t id,
              enum elk_shader_reloc_type type,
              uint32_t offset, uint32_t delta)
{
   if (p->num_relocs + 1 > p->reloc_array_size) {
      p->reloc_array_size = MAX2(16, p->reloc_array_size * 2);
      p->relocs = reralloc(p->mem_ctx, p->relocs,
                           struct elk_shader_reloc, p->reloc_array_size);
   }

   p->relocs[p->num_relocs++] = (struct elk_shader_reloc) {
      .id = id,
      .type = type,
      .offset = offset,
      .delta = delta,
   };
}

static elk_inst *
elk_alu1(struct elk_codegen *p, unsigned opcode,
         struct elk_reg dest, struct elk_reg src)
{
   elk_inst *insn = next_insn(p, opcode);
   elk_set_dest(p, insn, dest);
   elk_set_src0(p, insn, src);
   return insn;
}

static elk_inst *
elk_alu2(struct elk_codegen *p, unsigned opcode,
         struct elk_reg dest, struct elk_reg src0, struct elk_reg src1)
{
   /* 64-bit immediates are only supported on 1-src instructions */
   assert(src0.file != ELK_IMMEDIATE_VALUE || type_sz(src0.type) <= 4);
   assert(src1.file != ELK_IMMEDIATE_VALUE || type_sz(src1.type) <= 4);

   elk_inst *insn = next_insn(p, opcode);
   elk_set_dest(p, insn, dest);
   elk_set_src0(p, insn, src0);
   elk_set_src1(p, insn, src1);
   return insn;
}

static int
get_3src_subreg_nr(struct elk_reg reg)
{
   /* Normally, SubRegNum is in bytes (0..31).  However, 3-src instructions
    * use 32-bit units (components 0..7).  Since they only support F/D/UD
    * types, this doesn't lose any flexibility, but uses fewer bits.
    */
   return reg.subnr / 4;
}

static enum gfx10_align1_3src_vertical_stride
to_3src_align1_vstride(const struct intel_device_info *devinfo,
                       enum elk_vertical_stride vstride)
{
   switch (vstride) {
   case ELK_VERTICAL_STRIDE_0:
      return ELK_ALIGN1_3SRC_VERTICAL_STRIDE_0;
   case ELK_VERTICAL_STRIDE_2:
      return ELK_ALIGN1_3SRC_VERTICAL_STRIDE_2;
   case ELK_VERTICAL_STRIDE_4:
      return ELK_ALIGN1_3SRC_VERTICAL_STRIDE_4;
   case ELK_VERTICAL_STRIDE_8:
   case ELK_VERTICAL_STRIDE_16:
      return ELK_ALIGN1_3SRC_VERTICAL_STRIDE_8;
   default:
      unreachable("invalid vstride");
   }
}


static enum gfx10_align1_3src_src_horizontal_stride
to_3src_align1_hstride(enum elk_horizontal_stride hstride)
{
   switch (hstride) {
   case ELK_HORIZONTAL_STRIDE_0:
      return ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_0;
   case ELK_HORIZONTAL_STRIDE_1:
      return ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_1;
   case ELK_HORIZONTAL_STRIDE_2:
      return ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_2;
   case ELK_HORIZONTAL_STRIDE_4:
      return ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_4;
   default:
      unreachable("invalid hstride");
   }
}

static elk_inst *
elk_alu3(struct elk_codegen *p, unsigned opcode, struct elk_reg dest,
         struct elk_reg src0, struct elk_reg src1, struct elk_reg src2)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *inst = next_insn(p, opcode);

   gfx7_convert_mrf_to_grf(p, &dest);

   assert(dest.nr < XE2_MAX_GRF);

   assert(src0.file == ELK_IMMEDIATE_VALUE || src0.nr < XE2_MAX_GRF);
   assert(src1.file != ELK_IMMEDIATE_VALUE && src1.nr < XE2_MAX_GRF);
   assert(src2.file == ELK_IMMEDIATE_VALUE || src2.nr < XE2_MAX_GRF);
   assert(dest.address_mode == ELK_ADDRESS_DIRECT);
   assert(src0.address_mode == ELK_ADDRESS_DIRECT);
   assert(src1.address_mode == ELK_ADDRESS_DIRECT);
   assert(src2.address_mode == ELK_ADDRESS_DIRECT);

   assert(dest.file == ELK_GENERAL_REGISTER_FILE ||
          dest.file == ELK_MESSAGE_REGISTER_FILE);
   assert(dest.type == ELK_REGISTER_TYPE_F  ||
          dest.type == ELK_REGISTER_TYPE_DF ||
          dest.type == ELK_REGISTER_TYPE_D  ||
          dest.type == ELK_REGISTER_TYPE_UD ||
          (dest.type == ELK_REGISTER_TYPE_HF && devinfo->ver >= 8));
   if (devinfo->ver == 6) {
      elk_inst_set_3src_a16_dst_reg_file(devinfo, inst,
                                         dest.file == ELK_MESSAGE_REGISTER_FILE);
   }
   elk_inst_set_3src_dst_reg_nr(devinfo, inst, dest.nr);
   elk_inst_set_3src_a16_dst_subreg_nr(devinfo, inst, dest.subnr / 4);
   elk_inst_set_3src_a16_dst_writemask(devinfo, inst, dest.writemask);

   assert(src0.file == ELK_GENERAL_REGISTER_FILE);
   elk_inst_set_3src_a16_src0_swizzle(devinfo, inst, src0.swizzle);
   elk_inst_set_3src_a16_src0_subreg_nr(devinfo, inst, get_3src_subreg_nr(src0));
   elk_inst_set_3src_src0_reg_nr(devinfo, inst, src0.nr);
   elk_inst_set_3src_src0_abs(devinfo, inst, src0.abs);
   elk_inst_set_3src_src0_negate(devinfo, inst, src0.negate);
   elk_inst_set_3src_a16_src0_rep_ctrl(devinfo, inst,
                                       src0.vstride == ELK_VERTICAL_STRIDE_0);

   assert(src1.file == ELK_GENERAL_REGISTER_FILE);
   elk_inst_set_3src_a16_src1_swizzle(devinfo, inst, src1.swizzle);
   elk_inst_set_3src_a16_src1_subreg_nr(devinfo, inst, get_3src_subreg_nr(src1));
   elk_inst_set_3src_src1_reg_nr(devinfo, inst, src1.nr);
   elk_inst_set_3src_src1_abs(devinfo, inst, src1.abs);
   elk_inst_set_3src_src1_negate(devinfo, inst, src1.negate);
   elk_inst_set_3src_a16_src1_rep_ctrl(devinfo, inst,
                                       src1.vstride == ELK_VERTICAL_STRIDE_0);

   assert(src2.file == ELK_GENERAL_REGISTER_FILE);
   elk_inst_set_3src_a16_src2_swizzle(devinfo, inst, src2.swizzle);
   elk_inst_set_3src_a16_src2_subreg_nr(devinfo, inst, get_3src_subreg_nr(src2));
   elk_inst_set_3src_src2_reg_nr(devinfo, inst, src2.nr);
   elk_inst_set_3src_src2_abs(devinfo, inst, src2.abs);
   elk_inst_set_3src_src2_negate(devinfo, inst, src2.negate);
   elk_inst_set_3src_a16_src2_rep_ctrl(devinfo, inst,
                                       src2.vstride == ELK_VERTICAL_STRIDE_0);

   if (devinfo->ver >= 7) {
      /* Set both the source and destination types based on dest.type,
       * ignoring the source register types.  The MAD and LRP emitters ensure
       * that all four types are float.  The BFE and BFI2 emitters, however,
       * may send us mixed D and UD types and want us to ignore that and use
       * the destination type.
       */
      elk_inst_set_3src_a16_src_type(devinfo, inst, dest.type);
      elk_inst_set_3src_a16_dst_type(devinfo, inst, dest.type);

      /* From the Bspec, 3D Media GPGPU, Instruction fields, srcType:
       *
       *    "Three source instructions can use operands with mixed-mode
       *     precision. When SrcType field is set to :f or :hf it defines
       *     precision for source 0 only, and fields Src1Type and Src2Type
       *     define precision for other source operands:
       *
       *     0b = :f. Single precision Float (32-bit).
       *     1b = :hf. Half precision Float (16-bit)."
       */
      if (src1.type == ELK_REGISTER_TYPE_HF)
         elk_inst_set_3src_a16_src1_type(devinfo, inst, 1);

      if (src2.type == ELK_REGISTER_TYPE_HF)
         elk_inst_set_3src_a16_src2_type(devinfo, inst, 1);
   }

   return inst;
}

/***********************************************************************
 * Convenience routines.
 */
#define ALU1(OP)					\
elk_inst *elk_##OP(struct elk_codegen *p,		\
	      struct elk_reg dest,			\
	      struct elk_reg src0)   			\
{							\
   return elk_alu1(p, ELK_OPCODE_##OP, dest, src0);    	\
}

#define ALU2(OP)					\
elk_inst *elk_##OP(struct elk_codegen *p,		\
	      struct elk_reg dest,			\
	      struct elk_reg src0,			\
	      struct elk_reg src1)   			\
{							\
   return elk_alu2(p, ELK_OPCODE_##OP, dest, src0, src1);	\
}

#define ALU3(OP)					\
elk_inst *elk_##OP(struct elk_codegen *p,		\
	      struct elk_reg dest,			\
	      struct elk_reg src0,			\
	      struct elk_reg src1,			\
	      struct elk_reg src2)   			\
{                                                       \
   if (p->current->access_mode == ELK_ALIGN_16) {       \
      if (src0.vstride == ELK_VERTICAL_STRIDE_0)        \
         src0.swizzle = ELK_SWIZZLE_XXXX;               \
      if (src1.vstride == ELK_VERTICAL_STRIDE_0)        \
         src1.swizzle = ELK_SWIZZLE_XXXX;               \
      if (src2.vstride == ELK_VERTICAL_STRIDE_0)        \
         src2.swizzle = ELK_SWIZZLE_XXXX;               \
   }                                                    \
   return elk_alu3(p, ELK_OPCODE_##OP, dest, src0, src1, src2);	\
}

#define ALU3F(OP)                                               \
elk_inst *elk_##OP(struct elk_codegen *p,         \
                                 struct elk_reg dest,           \
                                 struct elk_reg src0,           \
                                 struct elk_reg src1,           \
                                 struct elk_reg src2)           \
{                                                               \
   assert(dest.type == ELK_REGISTER_TYPE_F ||                   \
          dest.type == ELK_REGISTER_TYPE_DF);                   \
   if (dest.type == ELK_REGISTER_TYPE_F) {                      \
      assert(src0.type == ELK_REGISTER_TYPE_F);                 \
      assert(src1.type == ELK_REGISTER_TYPE_F);                 \
      assert(src2.type == ELK_REGISTER_TYPE_F);                 \
   } else if (dest.type == ELK_REGISTER_TYPE_DF) {              \
      assert(src0.type == ELK_REGISTER_TYPE_DF);                \
      assert(src1.type == ELK_REGISTER_TYPE_DF);                \
      assert(src2.type == ELK_REGISTER_TYPE_DF);                \
   }                                                            \
                                                                \
   if (p->current->access_mode == ELK_ALIGN_16) {               \
      if (src0.vstride == ELK_VERTICAL_STRIDE_0)                \
         src0.swizzle = ELK_SWIZZLE_XXXX;                       \
      if (src1.vstride == ELK_VERTICAL_STRIDE_0)                \
         src1.swizzle = ELK_SWIZZLE_XXXX;                       \
      if (src2.vstride == ELK_VERTICAL_STRIDE_0)                \
         src2.swizzle = ELK_SWIZZLE_XXXX;                       \
   }                                                            \
   return elk_alu3(p, ELK_OPCODE_##OP, dest, src0, src1, src2); \
}

ALU2(SEL)
ALU1(NOT)
ALU2(AND)
ALU2(OR)
ALU2(XOR)
ALU2(SHR)
ALU2(SHL)
ALU1(DIM)
ALU2(ASR)
ALU3(CSEL)
ALU1(FRC)
ALU1(RNDD)
ALU1(RNDE)
ALU1(RNDU)
ALU1(RNDZ)
ALU2(MAC)
ALU2(MACH)
ALU1(LZD)
ALU2(DP4)
ALU2(DPH)
ALU2(DP3)
ALU2(DP2)
ALU3(MAD)
ALU3F(LRP)
ALU1(BFREV)
ALU3(BFE)
ALU2(BFI1)
ALU3(BFI2)
ALU1(FBH)
ALU1(FBL)
ALU1(CBIT)
ALU2(ADDC)
ALU2(SUBB)

elk_inst *
elk_MOV(struct elk_codegen *p, struct elk_reg dest, struct elk_reg src0)
{
   const struct intel_device_info *devinfo = p->devinfo;

   /* When converting F->DF on IVB/BYT, every odd source channel is ignored.
    * To avoid the problems that causes, we use an <X,2,0> source region to
    * read each element twice.
    */
   if (devinfo->verx10 == 70 &&
       elk_get_default_access_mode(p) == ELK_ALIGN_1 &&
       dest.type == ELK_REGISTER_TYPE_DF &&
       (src0.type == ELK_REGISTER_TYPE_F ||
        src0.type == ELK_REGISTER_TYPE_D ||
        src0.type == ELK_REGISTER_TYPE_UD) &&
       !has_scalar_region(src0)) {
      assert(src0.vstride == src0.width + src0.hstride);
      src0.vstride = src0.hstride;
      src0.width = ELK_WIDTH_2;
      src0.hstride = ELK_HORIZONTAL_STRIDE_0;
   }

   return elk_alu1(p, ELK_OPCODE_MOV, dest, src0);
}

elk_inst *
elk_ADD(struct elk_codegen *p, struct elk_reg dest,
        struct elk_reg src0, struct elk_reg src1)
{
   /* 6.2.2: add */
   if (src0.type == ELK_REGISTER_TYPE_F ||
       (src0.file == ELK_IMMEDIATE_VALUE &&
	src0.type == ELK_REGISTER_TYPE_VF)) {
      assert(src1.type != ELK_REGISTER_TYPE_UD);
      assert(src1.type != ELK_REGISTER_TYPE_D);
   }

   if (src1.type == ELK_REGISTER_TYPE_F ||
       (src1.file == ELK_IMMEDIATE_VALUE &&
	src1.type == ELK_REGISTER_TYPE_VF)) {
      assert(src0.type != ELK_REGISTER_TYPE_UD);
      assert(src0.type != ELK_REGISTER_TYPE_D);
   }

   return elk_alu2(p, ELK_OPCODE_ADD, dest, src0, src1);
}

elk_inst *
elk_AVG(struct elk_codegen *p, struct elk_reg dest,
        struct elk_reg src0, struct elk_reg src1)
{
   assert(dest.type == src0.type);
   assert(src0.type == src1.type);
   switch (src0.type) {
   case ELK_REGISTER_TYPE_B:
   case ELK_REGISTER_TYPE_UB:
   case ELK_REGISTER_TYPE_W:
   case ELK_REGISTER_TYPE_UW:
   case ELK_REGISTER_TYPE_D:
   case ELK_REGISTER_TYPE_UD:
      break;
   default:
      unreachable("Bad type for elk_AVG");
   }

   return elk_alu2(p, ELK_OPCODE_AVG, dest, src0, src1);
}

elk_inst *
elk_MUL(struct elk_codegen *p, struct elk_reg dest,
        struct elk_reg src0, struct elk_reg src1)
{
   /* 6.32.38: mul */
   if (src0.type == ELK_REGISTER_TYPE_D ||
       src0.type == ELK_REGISTER_TYPE_UD ||
       src1.type == ELK_REGISTER_TYPE_D ||
       src1.type == ELK_REGISTER_TYPE_UD) {
      assert(dest.type != ELK_REGISTER_TYPE_F);
   }

   if (src0.type == ELK_REGISTER_TYPE_F ||
       (src0.file == ELK_IMMEDIATE_VALUE &&
	src0.type == ELK_REGISTER_TYPE_VF)) {
      assert(src1.type != ELK_REGISTER_TYPE_UD);
      assert(src1.type != ELK_REGISTER_TYPE_D);
   }

   if (src1.type == ELK_REGISTER_TYPE_F ||
       (src1.file == ELK_IMMEDIATE_VALUE &&
	src1.type == ELK_REGISTER_TYPE_VF)) {
      assert(src0.type != ELK_REGISTER_TYPE_UD);
      assert(src0.type != ELK_REGISTER_TYPE_D);
   }

   assert(src0.file != ELK_ARCHITECTURE_REGISTER_FILE ||
	  src0.nr != ELK_ARF_ACCUMULATOR);
   assert(src1.file != ELK_ARCHITECTURE_REGISTER_FILE ||
	  src1.nr != ELK_ARF_ACCUMULATOR);

   return elk_alu2(p, ELK_OPCODE_MUL, dest, src0, src1);
}

elk_inst *
elk_LINE(struct elk_codegen *p, struct elk_reg dest,
         struct elk_reg src0, struct elk_reg src1)
{
   src0.vstride = ELK_VERTICAL_STRIDE_0;
   src0.width = ELK_WIDTH_1;
   src0.hstride = ELK_HORIZONTAL_STRIDE_0;
   return elk_alu2(p, ELK_OPCODE_LINE, dest, src0, src1);
}

elk_inst *
elk_PLN(struct elk_codegen *p, struct elk_reg dest,
        struct elk_reg src0, struct elk_reg src1)
{
   src0.vstride = ELK_VERTICAL_STRIDE_0;
   src0.width = ELK_WIDTH_1;
   src0.hstride = ELK_HORIZONTAL_STRIDE_0;
   src1.vstride = ELK_VERTICAL_STRIDE_8;
   src1.width = ELK_WIDTH_8;
   src1.hstride = ELK_HORIZONTAL_STRIDE_1;
   return elk_alu2(p, ELK_OPCODE_PLN, dest, src0, src1);
}

elk_inst *
elk_F32TO16(struct elk_codegen *p, struct elk_reg dst, struct elk_reg src)
{
   assert(p->devinfo->ver == 7);

   /* The F32TO16 instruction doesn't support 32-bit destination types in
    * Align1 mode.  Gfx7 (only) does zero out the high 16 bits in Align16
    * mode as an undocumented feature.
    */
   if (ELK_ALIGN_16 == elk_get_default_access_mode(p)) {
      assert(dst.type == ELK_REGISTER_TYPE_UD);
   } else {
      assert(dst.type == ELK_REGISTER_TYPE_W ||
             dst.type == ELK_REGISTER_TYPE_UW);
   }

   return elk_alu1(p, ELK_OPCODE_F32TO16, dst, src);
}

elk_inst *
elk_F16TO32(struct elk_codegen *p, struct elk_reg dst, struct elk_reg src)
{
   assert(p->devinfo->ver == 7);

   if (ELK_ALIGN_16 == elk_get_default_access_mode(p)) {
      assert(src.type == ELK_REGISTER_TYPE_UD);
   } else {
      /* From the Ivybridge PRM, Vol4, Part3, Section 6.26 f16to32:
       *
       *   Because this instruction does not have a 16-bit floating-point
       *   type, the source data type must be Word (W). The destination type
       *   must be F (Float).
       */
      assert(src.type == ELK_REGISTER_TYPE_W ||
             src.type == ELK_REGISTER_TYPE_UW);
   }

   return elk_alu1(p, ELK_OPCODE_F16TO32, dst, src);
}


void elk_NOP(struct elk_codegen *p)
{
   elk_inst *insn = next_insn(p, ELK_OPCODE_NOP);
   memset(insn, 0, sizeof(*insn));
   elk_inst_set_opcode(p->isa, insn, ELK_OPCODE_NOP);
}

/***********************************************************************
 * Comparisons, if/else/endif
 */

elk_inst *
elk_JMPI(struct elk_codegen *p, struct elk_reg index,
         unsigned predicate_control)
{
   const struct intel_device_info *devinfo = p->devinfo;
   struct elk_reg ip = elk_ip_reg();
   elk_inst *inst = elk_alu2(p, ELK_OPCODE_JMPI, ip, ip, index);

   elk_inst_set_exec_size(devinfo, inst, ELK_EXECUTE_1);
   elk_inst_set_qtr_control(devinfo, inst, ELK_COMPRESSION_NONE);
   elk_inst_set_mask_control(devinfo, inst, ELK_MASK_DISABLE);
   elk_inst_set_pred_control(devinfo, inst, predicate_control);

   return inst;
}

static void
push_if_stack(struct elk_codegen *p, elk_inst *inst)
{
   p->if_stack[p->if_stack_depth] = inst - p->store;

   p->if_stack_depth++;
   if (p->if_stack_array_size <= p->if_stack_depth) {
      p->if_stack_array_size *= 2;
      p->if_stack = reralloc(p->mem_ctx, p->if_stack, int,
			     p->if_stack_array_size);
   }
}

static elk_inst *
pop_if_stack(struct elk_codegen *p)
{
   p->if_stack_depth--;
   return &p->store[p->if_stack[p->if_stack_depth]];
}

static void
push_loop_stack(struct elk_codegen *p, elk_inst *inst)
{
   if (p->loop_stack_array_size <= (p->loop_stack_depth + 1)) {
      p->loop_stack_array_size *= 2;
      p->loop_stack = reralloc(p->mem_ctx, p->loop_stack, int,
			       p->loop_stack_array_size);
      p->if_depth_in_loop = reralloc(p->mem_ctx, p->if_depth_in_loop, int,
				     p->loop_stack_array_size);
   }

   p->loop_stack[p->loop_stack_depth] = inst - p->store;
   p->loop_stack_depth++;
   p->if_depth_in_loop[p->loop_stack_depth] = 0;
}

static elk_inst *
get_inner_do_insn(struct elk_codegen *p)
{
   return &p->store[p->loop_stack[p->loop_stack_depth - 1]];
}

/* EU takes the value from the flag register and pushes it onto some
 * sort of a stack (presumably merging with any flag value already on
 * the stack).  Within an if block, the flags at the top of the stack
 * control execution on each channel of the unit, eg. on each of the
 * 16 pixel values in our wm programs.
 *
 * When the matching 'else' instruction is reached (presumably by
 * countdown of the instruction count patched in by our ELSE/ENDIF
 * functions), the relevant flags are inverted.
 *
 * When the matching 'endif' instruction is reached, the flags are
 * popped off.  If the stack is now empty, normal execution resumes.
 */
elk_inst *
elk_IF(struct elk_codegen *p, unsigned execute_size)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn;

   insn = next_insn(p, ELK_OPCODE_IF);

   /* Override the defaults for this instruction:
    */
   if (devinfo->ver < 6) {
      elk_set_dest(p, insn, elk_ip_reg());
      elk_set_src0(p, insn, elk_ip_reg());
      elk_set_src1(p, insn, elk_imm_d(0x0));
   } else if (devinfo->ver == 6) {
      elk_set_dest(p, insn, elk_imm_w(0));
      elk_inst_set_gfx6_jump_count(devinfo, insn, 0);
      elk_set_src0(p, insn, vec1(retype(elk_null_reg(), ELK_REGISTER_TYPE_D)));
      elk_set_src1(p, insn, vec1(retype(elk_null_reg(), ELK_REGISTER_TYPE_D)));
   } else if (devinfo->ver == 7) {
      elk_set_dest(p, insn, vec1(retype(elk_null_reg(), ELK_REGISTER_TYPE_D)));
      elk_set_src0(p, insn, vec1(retype(elk_null_reg(), ELK_REGISTER_TYPE_D)));
      elk_set_src1(p, insn, elk_imm_w(0));
      elk_inst_set_jip(devinfo, insn, 0);
      elk_inst_set_uip(devinfo, insn, 0);
   } else {
      elk_set_dest(p, insn, vec1(retype(elk_null_reg(), ELK_REGISTER_TYPE_D)));
      elk_set_src0(p, insn, elk_imm_d(0));
      elk_inst_set_jip(devinfo, insn, 0);
      elk_inst_set_uip(devinfo, insn, 0);
   }

   elk_inst_set_exec_size(devinfo, insn, execute_size);
   elk_inst_set_qtr_control(devinfo, insn, ELK_COMPRESSION_NONE);
   elk_inst_set_pred_control(devinfo, insn, ELK_PREDICATE_NORMAL);
   elk_inst_set_mask_control(devinfo, insn, ELK_MASK_ENABLE);
   if (!p->single_program_flow && devinfo->ver < 6)
      elk_inst_set_thread_control(devinfo, insn, ELK_THREAD_SWITCH);

   push_if_stack(p, insn);
   p->if_depth_in_loop[p->loop_stack_depth]++;
   return insn;
}

/* This function is only used for gfx6-style IF instructions with an
 * embedded comparison (conditional modifier).  It is not used on gfx7.
 */
elk_inst *
elk_gfx6_IF(struct elk_codegen *p, enum elk_conditional_mod conditional,
	struct elk_reg src0, struct elk_reg src1)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn;

   insn = next_insn(p, ELK_OPCODE_IF);

   elk_set_dest(p, insn, elk_imm_w(0));
   elk_inst_set_exec_size(devinfo, insn, elk_get_default_exec_size(p));
   elk_inst_set_gfx6_jump_count(devinfo, insn, 0);
   elk_set_src0(p, insn, src0);
   elk_set_src1(p, insn, src1);

   assert(elk_inst_qtr_control(devinfo, insn) == ELK_COMPRESSION_NONE);
   assert(elk_inst_pred_control(devinfo, insn) == ELK_PREDICATE_NONE);
   elk_inst_set_cond_modifier(devinfo, insn, conditional);

   push_if_stack(p, insn);
   return insn;
}

/**
 * In single-program-flow (SPF) mode, convert IF and ELSE into ADDs.
 */
static void
convert_IF_ELSE_to_ADD(struct elk_codegen *p,
                       elk_inst *if_inst, elk_inst *else_inst)
{
   const struct intel_device_info *devinfo = p->devinfo;

   /* The next instruction (where the ENDIF would be, if it existed) */
   elk_inst *next_inst = &p->store[p->nr_insn];

   assert(p->single_program_flow);
   assert(if_inst != NULL && elk_inst_opcode(p->isa, if_inst) == ELK_OPCODE_IF);
   assert(else_inst == NULL || elk_inst_opcode(p->isa, else_inst) == ELK_OPCODE_ELSE);
   assert(elk_inst_exec_size(devinfo, if_inst) == ELK_EXECUTE_1);

   /* Convert IF to an ADD instruction that moves the instruction pointer
    * to the first instruction of the ELSE block.  If there is no ELSE
    * block, point to where ENDIF would be.  Reverse the predicate.
    *
    * There's no need to execute an ENDIF since we don't need to do any
    * stack operations, and if we're currently executing, we just want to
    * continue normally.
    */
   elk_inst_set_opcode(p->isa, if_inst, ELK_OPCODE_ADD);
   elk_inst_set_pred_inv(devinfo, if_inst, true);

   if (else_inst != NULL) {
      /* Convert ELSE to an ADD instruction that points where the ENDIF
       * would be.
       */
      elk_inst_set_opcode(p->isa, else_inst, ELK_OPCODE_ADD);

      elk_inst_set_imm_ud(devinfo, if_inst, (else_inst - if_inst + 1) * 16);
      elk_inst_set_imm_ud(devinfo, else_inst, (next_inst - else_inst) * 16);
   } else {
      elk_inst_set_imm_ud(devinfo, if_inst, (next_inst - if_inst) * 16);
   }
}

/**
 * Patch IF and ELSE instructions with appropriate jump targets.
 */
static void
patch_IF_ELSE(struct elk_codegen *p,
              elk_inst *if_inst, elk_inst *else_inst, elk_inst *endif_inst)
{
   const struct intel_device_info *devinfo = p->devinfo;

   /* We shouldn't be patching IF and ELSE instructions in single program flow
    * mode when gen < 6, because in single program flow mode on those
    * platforms, we convert flow control instructions to conditional ADDs that
    * operate on IP (see elk_ENDIF).
    *
    * However, on Gfx6, writing to IP doesn't work in single program flow mode
    * (see the SandyBridge PRM, Volume 4 part 2, p79: "When SPF is ON, IP may
    * not be updated by non-flow control instructions.").  And on later
    * platforms, there is no significant benefit to converting control flow
    * instructions to conditional ADDs.  So we do patch IF and ELSE
    * instructions in single program flow mode on those platforms.
    */
   if (devinfo->ver < 6)
      assert(!p->single_program_flow);

   assert(if_inst != NULL && elk_inst_opcode(p->isa, if_inst) == ELK_OPCODE_IF);
   assert(endif_inst != NULL);
   assert(else_inst == NULL || elk_inst_opcode(p->isa, else_inst) == ELK_OPCODE_ELSE);

   unsigned br = elk_jump_scale(devinfo);

   assert(elk_inst_opcode(p->isa, endif_inst) == ELK_OPCODE_ENDIF);
   elk_inst_set_exec_size(devinfo, endif_inst, elk_inst_exec_size(devinfo, if_inst));

   if (else_inst == NULL) {
      /* Patch IF -> ENDIF */
      if (devinfo->ver < 6) {
	 /* Turn it into an IFF, which means no mask stack operations for
	  * all-false and jumping past the ENDIF.
	  */
         elk_inst_set_opcode(p->isa, if_inst, ELK_OPCODE_IFF);
         elk_inst_set_gfx4_jump_count(devinfo, if_inst,
                                      br * (endif_inst - if_inst + 1));
         elk_inst_set_gfx4_pop_count(devinfo, if_inst, 0);
      } else if (devinfo->ver == 6) {
	 /* As of gfx6, there is no IFF and IF must point to the ENDIF. */
         elk_inst_set_gfx6_jump_count(devinfo, if_inst, br*(endif_inst - if_inst));
      } else {
         elk_inst_set_uip(devinfo, if_inst, br * (endif_inst - if_inst));
         elk_inst_set_jip(devinfo, if_inst, br * (endif_inst - if_inst));
      }
   } else {
      elk_inst_set_exec_size(devinfo, else_inst, elk_inst_exec_size(devinfo, if_inst));

      /* Patch IF -> ELSE */
      if (devinfo->ver < 6) {
         elk_inst_set_gfx4_jump_count(devinfo, if_inst,
                                      br * (else_inst - if_inst));
         elk_inst_set_gfx4_pop_count(devinfo, if_inst, 0);
      } else if (devinfo->ver == 6) {
         elk_inst_set_gfx6_jump_count(devinfo, if_inst,
                                      br * (else_inst - if_inst + 1));
      }

      /* Patch ELSE -> ENDIF */
      if (devinfo->ver < 6) {
	 /* ELK_OPCODE_ELSE pre-gfx6 should point just past the
	  * matching ENDIF.
	  */
         elk_inst_set_gfx4_jump_count(devinfo, else_inst,
                                      br * (endif_inst - else_inst + 1));
         elk_inst_set_gfx4_pop_count(devinfo, else_inst, 1);
      } else if (devinfo->ver == 6) {
	 /* ELK_OPCODE_ELSE on gfx6 should point to the matching ENDIF. */
         elk_inst_set_gfx6_jump_count(devinfo, else_inst,
                                      br * (endif_inst - else_inst));
      } else {
	 /* The IF instruction's JIP should point just past the ELSE */
         elk_inst_set_jip(devinfo, if_inst, br * (else_inst - if_inst + 1));
	 /* The IF instruction's UIP and ELSE's JIP should point to ENDIF */
         elk_inst_set_uip(devinfo, if_inst, br * (endif_inst - if_inst));

         if (devinfo->ver >= 8) {
            /* Set the ELSE instruction to use branch_ctrl with a join
             * jump target pointing at the NOP inserted right before
             * the ENDIF instruction in order to make sure it is
             * executed in all cases, since attempting to do the same
             * as on other generations could cause the EU to jump at
             * the instruction immediately after the ENDIF due to
             * Wa_220160235, which could cause the program to continue
             * running with all channels disabled.
             */
            elk_inst_set_jip(devinfo, else_inst, br * (endif_inst - else_inst - 1));
            elk_inst_set_branch_control(devinfo, else_inst, true);
         } else {
            elk_inst_set_jip(devinfo, else_inst, br * (endif_inst - else_inst));
         }

         if (devinfo->ver >= 8) {
            /* Since we don't set branch_ctrl on Gfx11+, the ELSE's
             * JIP and UIP both should point to ENDIF on those
             * platforms.
             */
            elk_inst_set_uip(devinfo, else_inst, br * (endif_inst - else_inst));
         }
      }
   }
}

void
elk_ELSE(struct elk_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn;

   insn = next_insn(p, ELK_OPCODE_ELSE);

   if (devinfo->ver < 6) {
      elk_set_dest(p, insn, elk_ip_reg());
      elk_set_src0(p, insn, elk_ip_reg());
      elk_set_src1(p, insn, elk_imm_d(0x0));
   } else if (devinfo->ver == 6) {
      elk_set_dest(p, insn, elk_imm_w(0));
      elk_inst_set_gfx6_jump_count(devinfo, insn, 0);
      elk_set_src0(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src1(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
   } else if (devinfo->ver == 7) {
      elk_set_dest(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src0(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src1(p, insn, elk_imm_w(0));
      elk_inst_set_jip(devinfo, insn, 0);
      elk_inst_set_uip(devinfo, insn, 0);
   } else {
      elk_set_dest(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src0(p, insn, elk_imm_d(0));
      elk_inst_set_jip(devinfo, insn, 0);
      elk_inst_set_uip(devinfo, insn, 0);
   }

   elk_inst_set_qtr_control(devinfo, insn, ELK_COMPRESSION_NONE);
   elk_inst_set_mask_control(devinfo, insn, ELK_MASK_ENABLE);
   if (!p->single_program_flow && devinfo->ver < 6)
      elk_inst_set_thread_control(devinfo, insn, ELK_THREAD_SWITCH);

   push_if_stack(p, insn);
}

void
elk_ENDIF(struct elk_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn = NULL;
   elk_inst *else_inst = NULL;
   elk_inst *if_inst = NULL;
   elk_inst *tmp;
   bool emit_endif = true;

   assert(p->if_stack_depth > 0);

   if (devinfo->ver >= 8 &&
       elk_inst_opcode(p->isa, &p->store[p->if_stack[
                             p->if_stack_depth - 1]]) == ELK_OPCODE_ELSE) {
      /* Insert a NOP to be specified as join instruction within the
       * ELSE block, which is valid for an ELSE instruction with
       * branch_ctrl on.  The ELSE instruction will be set to jump
       * here instead of to the ENDIF instruction, since attempting to
       * do the latter would prevent the ENDIF from being executed in
       * some cases due to Wa_220160235, which could cause the program
       * to continue running with all channels disabled.
       */
      elk_NOP(p);
   }

   /* In single program flow mode, we can express IF and ELSE instructions
    * equivalently as ADD instructions that operate on IP.  On platforms prior
    * to Gfx6, flow control instructions cause an implied thread switch, so
    * this is a significant savings.
    *
    * However, on Gfx6, writing to IP doesn't work in single program flow mode
    * (see the SandyBridge PRM, Volume 4 part 2, p79: "When SPF is ON, IP may
    * not be updated by non-flow control instructions.").  And on later
    * platforms, there is no significant benefit to converting control flow
    * instructions to conditional ADDs.  So we only do this trick on Gfx4 and
    * Gfx5.
    */
   if (devinfo->ver < 6 && p->single_program_flow)
      emit_endif = false;

   /*
    * A single next_insn() may change the base address of instruction store
    * memory(p->store), so call it first before referencing the instruction
    * store pointer from an index
    */
   if (emit_endif)
      insn = next_insn(p, ELK_OPCODE_ENDIF);

   /* Pop the IF and (optional) ELSE instructions from the stack */
   p->if_depth_in_loop[p->loop_stack_depth]--;
   tmp = pop_if_stack(p);
   if (elk_inst_opcode(p->isa, tmp) == ELK_OPCODE_ELSE) {
      else_inst = tmp;
      tmp = pop_if_stack(p);
   }
   if_inst = tmp;

   if (!emit_endif) {
      /* ENDIF is useless; don't bother emitting it. */
      convert_IF_ELSE_to_ADD(p, if_inst, else_inst);
      return;
   }

   if (devinfo->ver < 6) {
      elk_set_dest(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src0(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src1(p, insn, elk_imm_d(0x0));
   } else if (devinfo->ver == 6) {
      elk_set_dest(p, insn, elk_imm_w(0));
      elk_set_src0(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src1(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
   } else if (devinfo->ver == 7) {
      elk_set_dest(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src0(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src1(p, insn, elk_imm_w(0));
   } else {
      elk_set_src0(p, insn, elk_imm_d(0));
   }

   elk_inst_set_qtr_control(devinfo, insn, ELK_COMPRESSION_NONE);
   elk_inst_set_mask_control(devinfo, insn, ELK_MASK_ENABLE);
   if (devinfo->ver < 6)
      elk_inst_set_thread_control(devinfo, insn, ELK_THREAD_SWITCH);

   /* Also pop item off the stack in the endif instruction: */
   if (devinfo->ver < 6) {
      elk_inst_set_gfx4_jump_count(devinfo, insn, 0);
      elk_inst_set_gfx4_pop_count(devinfo, insn, 1);
   } else if (devinfo->ver == 6) {
      elk_inst_set_gfx6_jump_count(devinfo, insn, 2);
   } else {
      elk_inst_set_jip(devinfo, insn, 2);
   }
   patch_IF_ELSE(p, if_inst, else_inst, insn);
}

elk_inst *
elk_BREAK(struct elk_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn;

   insn = next_insn(p, ELK_OPCODE_BREAK);
   if (devinfo->ver >= 8) {
      elk_set_dest(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src0(p, insn, elk_imm_d(0x0));
   } else if (devinfo->ver >= 6) {
      elk_set_dest(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src0(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src1(p, insn, elk_imm_d(0x0));
   } else {
      elk_set_dest(p, insn, elk_ip_reg());
      elk_set_src0(p, insn, elk_ip_reg());
      elk_set_src1(p, insn, elk_imm_d(0x0));
      elk_inst_set_gfx4_pop_count(devinfo, insn,
                                  p->if_depth_in_loop[p->loop_stack_depth]);
   }
   elk_inst_set_qtr_control(devinfo, insn, ELK_COMPRESSION_NONE);
   elk_inst_set_exec_size(devinfo, insn, elk_get_default_exec_size(p));

   return insn;
}

elk_inst *
elk_CONT(struct elk_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn;

   insn = next_insn(p, ELK_OPCODE_CONTINUE);
   elk_set_dest(p, insn, elk_ip_reg());
   if (devinfo->ver >= 8) {
      elk_set_src0(p, insn, elk_imm_d(0x0));
   } else {
      elk_set_src0(p, insn, elk_ip_reg());
      elk_set_src1(p, insn, elk_imm_d(0x0));
   }

   if (devinfo->ver < 6) {
      elk_inst_set_gfx4_pop_count(devinfo, insn,
                                  p->if_depth_in_loop[p->loop_stack_depth]);
   }
   elk_inst_set_qtr_control(devinfo, insn, ELK_COMPRESSION_NONE);
   elk_inst_set_exec_size(devinfo, insn, elk_get_default_exec_size(p));
   return insn;
}

elk_inst *
elk_HALT(struct elk_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn;

   insn = next_insn(p, ELK_OPCODE_HALT);
   elk_set_dest(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
   if (devinfo->ver < 6) {
      /* From the Gfx4 PRM:
       *
       *    "IP register must be put (for example, by the assembler) at <dst>
       *    and <src0> locations.
       */
      elk_set_dest(p, insn, elk_ip_reg());
      elk_set_src0(p, insn, elk_ip_reg());
      elk_set_src1(p, insn, elk_imm_d(0x0)); /* exitcode updated later. */
   } else if (devinfo->ver < 8) {
      elk_set_src0(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      elk_set_src1(p, insn, elk_imm_d(0x0)); /* UIP and JIP, updated later. */
   } else {
      assert(devinfo->ver == 8);
      elk_set_src0(p, insn, elk_imm_d(0x0));
   }

   elk_inst_set_qtr_control(devinfo, insn, ELK_COMPRESSION_NONE);
   elk_inst_set_exec_size(devinfo, insn, elk_get_default_exec_size(p));
   return insn;
}

/* DO/WHILE loop:
 *
 * The DO/WHILE is just an unterminated loop -- break or continue are
 * used for control within the loop.  We have a few ways they can be
 * done.
 *
 * For uniform control flow, the WHILE is just a jump, so ADD ip, ip,
 * jip and no DO instruction.
 *
 * For non-uniform control flow pre-gfx6, there's a DO instruction to
 * push the mask, and a WHILE to jump back, and BREAK to get out and
 * pop the mask.
 *
 * For gfx6, there's no more mask stack, so no need for DO.  WHILE
 * just points back to the first instruction of the loop.
 */
elk_inst *
elk_DO(struct elk_codegen *p, unsigned execute_size)
{
   const struct intel_device_info *devinfo = p->devinfo;

   if (devinfo->ver >= 6 || p->single_program_flow) {
      push_loop_stack(p, &p->store[p->nr_insn]);
      return &p->store[p->nr_insn];
   } else {
      elk_inst *insn = next_insn(p, ELK_OPCODE_DO);

      push_loop_stack(p, insn);

      /* Override the defaults for this instruction:
       */
      elk_set_dest(p, insn, elk_null_reg());
      elk_set_src0(p, insn, elk_null_reg());
      elk_set_src1(p, insn, elk_null_reg());

      elk_inst_set_qtr_control(devinfo, insn, ELK_COMPRESSION_NONE);
      elk_inst_set_exec_size(devinfo, insn, execute_size);
      elk_inst_set_pred_control(devinfo, insn, ELK_PREDICATE_NONE);

      return insn;
   }
}

/**
 * For pre-gfx6, we patch BREAK/CONT instructions to point at the WHILE
 * instruction here.
 *
 * For gfx6+, see elk_set_uip_jip(), which doesn't care so much about the loop
 * nesting, since it can always just point to the end of the block/current loop.
 */
static void
elk_patch_break_cont(struct elk_codegen *p, elk_inst *while_inst)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *do_inst = get_inner_do_insn(p);
   elk_inst *inst;
   unsigned br = elk_jump_scale(devinfo);

   assert(devinfo->ver < 6);

   for (inst = while_inst - 1; inst != do_inst; inst--) {
      /* If the jump count is != 0, that means that this instruction has already
       * been patched because it's part of a loop inside of the one we're
       * patching.
       */
      if (elk_inst_opcode(p->isa, inst) == ELK_OPCODE_BREAK &&
          elk_inst_gfx4_jump_count(devinfo, inst) == 0) {
         elk_inst_set_gfx4_jump_count(devinfo, inst, br*((while_inst - inst) + 1));
      } else if (elk_inst_opcode(p->isa, inst) == ELK_OPCODE_CONTINUE &&
                 elk_inst_gfx4_jump_count(devinfo, inst) == 0) {
         elk_inst_set_gfx4_jump_count(devinfo, inst, br * (while_inst - inst));
      }
   }
}

elk_inst *
elk_WHILE(struct elk_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn, *do_insn;
   unsigned br = elk_jump_scale(devinfo);

   if (devinfo->ver >= 6) {
      insn = next_insn(p, ELK_OPCODE_WHILE);
      do_insn = get_inner_do_insn(p);

      if (devinfo->ver >= 8) {
         elk_set_dest(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
         elk_set_src0(p, insn, elk_imm_d(0));
         elk_inst_set_jip(devinfo, insn, br * (do_insn - insn));
      } else if (devinfo->ver == 7) {
         elk_set_dest(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
         elk_set_src0(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
         elk_set_src1(p, insn, elk_imm_w(0));
         elk_inst_set_jip(devinfo, insn, br * (do_insn - insn));
      } else {
         elk_set_dest(p, insn, elk_imm_w(0));
         elk_inst_set_gfx6_jump_count(devinfo, insn, br * (do_insn - insn));
         elk_set_src0(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
         elk_set_src1(p, insn, retype(elk_null_reg(), ELK_REGISTER_TYPE_D));
      }

      elk_inst_set_exec_size(devinfo, insn, elk_get_default_exec_size(p));

   } else {
      if (p->single_program_flow) {
	 insn = next_insn(p, ELK_OPCODE_ADD);
         do_insn = get_inner_do_insn(p);

	 elk_set_dest(p, insn, elk_ip_reg());
	 elk_set_src0(p, insn, elk_ip_reg());
	 elk_set_src1(p, insn, elk_imm_d((do_insn - insn) * 16));
         elk_inst_set_exec_size(devinfo, insn, ELK_EXECUTE_1);
      } else {
	 insn = next_insn(p, ELK_OPCODE_WHILE);
         do_insn = get_inner_do_insn(p);

         assert(elk_inst_opcode(p->isa, do_insn) == ELK_OPCODE_DO);

	 elk_set_dest(p, insn, elk_ip_reg());
	 elk_set_src0(p, insn, elk_ip_reg());
	 elk_set_src1(p, insn, elk_imm_d(0));

         elk_inst_set_exec_size(devinfo, insn, elk_inst_exec_size(devinfo, do_insn));
         elk_inst_set_gfx4_jump_count(devinfo, insn, br * (do_insn - insn + 1));
         elk_inst_set_gfx4_pop_count(devinfo, insn, 0);

	 elk_patch_break_cont(p, insn);
      }
   }
   elk_inst_set_qtr_control(devinfo, insn, ELK_COMPRESSION_NONE);

   p->loop_stack_depth--;

   return insn;
}

/* FORWARD JUMPS:
 */
void elk_land_fwd_jump(struct elk_codegen *p, int jmp_insn_idx)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *jmp_insn = &p->store[jmp_insn_idx];
   unsigned jmpi = 1;

   if (devinfo->ver >= 5)
      jmpi = 2;

   assert(elk_inst_opcode(p->isa, jmp_insn) == ELK_OPCODE_JMPI);
   assert(elk_inst_src1_reg_file(devinfo, jmp_insn) == ELK_IMMEDIATE_VALUE);

   elk_inst_set_gfx4_jump_count(devinfo, jmp_insn,
                                jmpi * (p->nr_insn - jmp_insn_idx - 1));
}

/* To integrate with the above, it makes sense that the comparison
 * instruction should populate the flag register.  It might be simpler
 * just to use the flag reg for most WM tasks?
 */
void elk_CMP(struct elk_codegen *p,
	     struct elk_reg dest,
	     unsigned conditional,
	     struct elk_reg src0,
	     struct elk_reg src1)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn = next_insn(p, ELK_OPCODE_CMP);

   elk_inst_set_cond_modifier(devinfo, insn, conditional);
   elk_set_dest(p, insn, dest);
   elk_set_src0(p, insn, src0);
   elk_set_src1(p, insn, src1);

   /* Item WaCMPInstNullDstForcesThreadSwitch in the Haswell Bspec workarounds
    * page says:
    *    "Any CMP instruction with a null destination must use a {switch}."
    *
    * It also applies to other Gfx7 platforms (IVB, BYT) even though it isn't
    * mentioned on their work-arounds pages.
    */
   if (devinfo->ver == 7) {
      if (dest.file == ELK_ARCHITECTURE_REGISTER_FILE &&
          dest.nr == ELK_ARF_NULL) {
         elk_inst_set_thread_control(devinfo, insn, ELK_THREAD_SWITCH);
      }
   }
}

void elk_CMPN(struct elk_codegen *p,
              struct elk_reg dest,
              unsigned conditional,
              struct elk_reg src0,
              struct elk_reg src1)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn = next_insn(p, ELK_OPCODE_CMPN);

   elk_inst_set_cond_modifier(devinfo, insn, conditional);
   elk_set_dest(p, insn, dest);
   elk_set_src0(p, insn, src0);
   elk_set_src1(p, insn, src1);

   /* Page 166 of the Ivy Bridge PRM Volume 4 part 3 (Execution Unit ISA)
    * says:
    *
    *    If the destination is the null register, the {Switch} instruction
    *    option must be used.
    *
    * Page 77 of the Haswell PRM Volume 2b contains the same text.
    */
   if (devinfo->ver == 7) {
      if (dest.file == ELK_ARCHITECTURE_REGISTER_FILE &&
          dest.nr == ELK_ARF_NULL) {
         elk_inst_set_thread_control(devinfo, insn, ELK_THREAD_SWITCH);
      }
   }
}

/***********************************************************************
 * Helpers for the various SEND message types:
 */

/** Extended math function, float[8].
 */
void elk_gfx4_math(struct elk_codegen *p,
	       struct elk_reg dest,
	       unsigned function,
	       unsigned msg_reg_nr,
	       struct elk_reg src,
	       unsigned precision )
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn = next_insn(p, ELK_OPCODE_SEND);
   unsigned data_type;
   if (has_scalar_region(src)) {
      data_type = ELK_MATH_DATA_SCALAR;
   } else {
      data_type = ELK_MATH_DATA_VECTOR;
   }

   assert(devinfo->ver < 6);

   /* Example code doesn't set predicate_control for send
    * instructions.
    */
   elk_inst_set_pred_control(devinfo, insn, 0);
   elk_inst_set_base_mrf(devinfo, insn, msg_reg_nr);

   elk_set_dest(p, insn, dest);
   elk_set_src0(p, insn, src);
   elk_set_math_message(p,
                        insn,
                        function,
                        src.type == ELK_REGISTER_TYPE_D,
                        precision,
                        data_type);
}

void elk_gfx6_math(struct elk_codegen *p,
	       struct elk_reg dest,
	       unsigned function,
	       struct elk_reg src0,
	       struct elk_reg src1)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn = next_insn(p, ELK_OPCODE_MATH);

   assert(devinfo->ver >= 6);

   assert(dest.file == ELK_GENERAL_REGISTER_FILE ||
          (devinfo->ver >= 7 && dest.file == ELK_MESSAGE_REGISTER_FILE));

   assert(dest.hstride == ELK_HORIZONTAL_STRIDE_1);
   if (devinfo->ver == 6) {
      assert(src0.hstride == ELK_HORIZONTAL_STRIDE_1);
      assert(src1.hstride == ELK_HORIZONTAL_STRIDE_1);
   }

   if (function == ELK_MATH_FUNCTION_INT_DIV_QUOTIENT ||
       function == ELK_MATH_FUNCTION_INT_DIV_REMAINDER ||
       function == ELK_MATH_FUNCTION_INT_DIV_QUOTIENT_AND_REMAINDER) {
      assert(src0.type != ELK_REGISTER_TYPE_F);
      assert(src1.type != ELK_REGISTER_TYPE_F);
      assert(src1.file == ELK_GENERAL_REGISTER_FILE ||
             (devinfo->ver >= 8 && src1.file == ELK_IMMEDIATE_VALUE));
      /* From BSpec 6647/47428 "[Instruction] Extended Math Function":
       *     INT DIV function does not support source modifiers.
       */
      assert(!src0.negate);
      assert(!src0.abs);
      assert(!src1.negate);
      assert(!src1.abs);
   } else {
      assert(src0.type == ELK_REGISTER_TYPE_F);
      assert(src1.type == ELK_REGISTER_TYPE_F);
   }

   /* Source modifiers are ignored for extended math instructions on Gfx6. */
   if (devinfo->ver == 6) {
      assert(!src0.negate);
      assert(!src0.abs);
      assert(!src1.negate);
      assert(!src1.abs);
   }

   elk_inst_set_math_function(devinfo, insn, function);

   elk_set_dest(p, insn, dest);
   elk_set_src0(p, insn, src0);
   elk_set_src1(p, insn, src1);
}

/**
 * Return the right surface index to access the thread scratch space using
 * stateless dataport messages.
 */
unsigned
elk_scratch_surface_idx(const struct elk_codegen *p)
{
   /* The scratch space is thread-local so IA coherency is unnecessary. */
   if (p->devinfo->ver >= 8)
      return GFX8_BTI_STATELESS_NON_COHERENT;
   else
      return ELK_BTI_STATELESS;
}

/**
 * Write a block of OWORDs (half a GRF each) from the scratch buffer,
 * using a constant offset per channel.
 *
 * The offset must be aligned to oword size (16 bytes).  Used for
 * register spilling.
 */
void elk_oword_block_write_scratch(struct elk_codegen *p,
				   struct elk_reg mrf,
				   int num_regs,
				   unsigned offset)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const unsigned target_cache =
      (devinfo->ver >= 7 ? GFX7_SFID_DATAPORT_DATA_CACHE :
       devinfo->ver >= 6 ? GFX6_SFID_DATAPORT_RENDER_CACHE :
       ELK_SFID_DATAPORT_WRITE);
   uint32_t msg_type;

   if (devinfo->ver >= 6)
      offset /= 16;

   mrf = retype(mrf, ELK_REGISTER_TYPE_UD);

   const unsigned mlen = 1 + num_regs;

   /* Set up the message header.  This is g0, with g0.2 filled with
    * the offset.  We don't want to leave our offset around in g0 or
    * it'll screw up texture samples, so set it up inside the message
    * reg.
    */
   {
      elk_push_insn_state(p);
      elk_set_default_exec_size(p, ELK_EXECUTE_8);
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);
      elk_set_default_compression_control(p, ELK_COMPRESSION_NONE);

      elk_MOV(p, mrf, retype(elk_vec8_grf(0, 0), ELK_REGISTER_TYPE_UD));

      /* set message header global offset field (reg 0, element 2) */
      elk_set_default_exec_size(p, ELK_EXECUTE_1);
      elk_MOV(p,
	      retype(elk_vec1_reg(ELK_MESSAGE_REGISTER_FILE,
				  mrf.nr,
				  2), ELK_REGISTER_TYPE_UD),
	      elk_imm_ud(offset));

      elk_pop_insn_state(p);
   }

   {
      struct elk_reg dest;
      elk_inst *insn = next_insn(p, ELK_OPCODE_SEND);
      int send_commit_msg;
      struct elk_reg src_header = retype(elk_vec8_grf(0, 0),
					 ELK_REGISTER_TYPE_UW);

      elk_inst_set_sfid(devinfo, insn, target_cache);
      elk_inst_set_compression(devinfo, insn, false);

      if (elk_inst_exec_size(devinfo, insn) >= 16)
	 src_header = vec16(src_header);

      assert(elk_inst_pred_control(devinfo, insn) == ELK_PREDICATE_NONE);
      if (devinfo->ver < 6)
         elk_inst_set_base_mrf(devinfo, insn, mrf.nr);

      /* Until gfx6, writes followed by reads from the same location
       * are not guaranteed to be ordered unless write_commit is set.
       * If set, then a no-op write is issued to the destination
       * register to set a dependency, and a read from the destination
       * can be used to ensure the ordering.
       *
       * For gfx6, only writes between different threads need ordering
       * protection.  Our use of DP writes is all about register
       * spilling within a thread.
       */
      if (devinfo->ver >= 6) {
	 dest = retype(vec16(elk_null_reg()), ELK_REGISTER_TYPE_UW);
	 send_commit_msg = 0;
      } else {
	 dest = src_header;
	 send_commit_msg = 1;
      }

      elk_set_dest(p, insn, dest);
      if (devinfo->ver >= 6) {
	 elk_set_src0(p, insn, mrf);
      } else {
	 elk_set_src0(p, insn, elk_null_reg());
      }

      if (devinfo->ver >= 6)
	 msg_type = GFX6_DATAPORT_WRITE_MESSAGE_OWORD_BLOCK_WRITE;
      else
	 msg_type = ELK_DATAPORT_WRITE_MESSAGE_OWORD_BLOCK_WRITE;

      elk_set_desc(p, insn,
                   elk_message_desc(devinfo, mlen, send_commit_msg, true) |
                   elk_dp_write_desc(devinfo, elk_scratch_surface_idx(p),
                                     ELK_DATAPORT_OWORD_BLOCK_DWORDS(num_regs * 8),
                                     msg_type, send_commit_msg));
   }
}


/**
 * Read a block of owords (half a GRF each) from the scratch buffer
 * using a constant index per channel.
 *
 * Offset must be aligned to oword size (16 bytes).  Used for register
 * spilling.
 */
void
elk_oword_block_read_scratch(struct elk_codegen *p,
			     struct elk_reg dest,
			     struct elk_reg mrf,
			     int num_regs,
			     unsigned offset)
{
   const struct intel_device_info *devinfo = p->devinfo;

   if (devinfo->ver >= 6)
      offset /= 16;

   if (p->devinfo->ver >= 7) {
      /* On gen 7 and above, we no longer have message registers and we can
       * send from any register we want.  By using the destination register
       * for the message, we guarantee that the implied message write won't
       * accidentally overwrite anything.  This has been a problem because
       * the MRF registers and source for the final FB write are both fixed
       * and may overlap.
       */
      mrf = retype(dest, ELK_REGISTER_TYPE_UD);
   } else {
      mrf = retype(mrf, ELK_REGISTER_TYPE_UD);
   }
   dest = retype(dest, ELK_REGISTER_TYPE_UW);

   const unsigned rlen = num_regs;
   const unsigned target_cache =
      (devinfo->ver >= 7 ? GFX7_SFID_DATAPORT_DATA_CACHE :
       devinfo->ver >= 6 ? GFX6_SFID_DATAPORT_RENDER_CACHE :
       ELK_SFID_DATAPORT_READ);

   {
      elk_push_insn_state(p);
      elk_set_default_exec_size(p, ELK_EXECUTE_8);
      elk_set_default_compression_control(p, ELK_COMPRESSION_NONE);
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);

      elk_MOV(p, mrf, retype(elk_vec8_grf(0, 0), ELK_REGISTER_TYPE_UD));

      /* set message header global offset field (reg 0, element 2) */
      elk_set_default_exec_size(p, ELK_EXECUTE_1);
      elk_MOV(p, get_element_ud(mrf, 2), elk_imm_ud(offset));

      elk_pop_insn_state(p);
   }

   {
      elk_inst *insn = next_insn(p, ELK_OPCODE_SEND);

      elk_inst_set_sfid(devinfo, insn, target_cache);
      assert(elk_inst_pred_control(devinfo, insn) == 0);
      elk_inst_set_compression(devinfo, insn, false);

      elk_set_dest(p, insn, dest);	/* UW? */
      if (devinfo->ver >= 6) {
	 elk_set_src0(p, insn, mrf);
      } else {
	 elk_set_src0(p, insn, elk_null_reg());
         elk_inst_set_base_mrf(devinfo, insn, mrf.nr);
      }

      elk_set_desc(p, insn,
                   elk_message_desc(devinfo, 1, rlen, true) |
                   elk_dp_read_desc(devinfo, elk_scratch_surface_idx(p),
                                    ELK_DATAPORT_OWORD_BLOCK_DWORDS(num_regs * 8),
                                    ELK_DATAPORT_READ_MESSAGE_OWORD_BLOCK_READ,
                                    ELK_DATAPORT_READ_TARGET_RENDER_CACHE));
   }
}

void
elk_gfx7_block_read_scratch(struct elk_codegen *p,
                        struct elk_reg dest,
                        int num_regs,
                        unsigned offset)
{
   elk_inst *insn = next_insn(p, ELK_OPCODE_SEND);
   assert(elk_inst_pred_control(p->devinfo, insn) == ELK_PREDICATE_NONE);

   elk_set_dest(p, insn, retype(dest, ELK_REGISTER_TYPE_UW));

   /* The HW requires that the header is present; this is to get the g0.5
    * scratch offset.
    */
   elk_set_src0(p, insn, elk_vec8_grf(0, 0));

   /* According to the docs, offset is "A 12-bit HWord offset into the memory
    * Immediate Memory buffer as specified by binding table 0xFF."  An HWORD
    * is 32 bytes, which happens to be the size of a register.
    */
   offset /= REG_SIZE;
   assert(offset < (1 << 12));

   gfx7_set_dp_scratch_message(p, insn,
                               false, /* scratch read */
                               false, /* OWords */
                               false, /* invalidate after read */
                               num_regs,
                               offset,
                               1,        /* mlen: just g0 */
                               num_regs, /* rlen */
                               true);    /* header present */
}

/**
 * Read float[4] vectors from the data port constant cache.
 * Location (in buffer) should be a multiple of 16.
 * Used for fetching shader constants.
 */
void elk_oword_block_read(struct elk_codegen *p,
			  struct elk_reg dest,
			  struct elk_reg mrf,
			  uint32_t offset,
			  uint32_t bind_table_index)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const unsigned target_cache =
      (devinfo->ver >= 6 ? GFX6_SFID_DATAPORT_CONSTANT_CACHE :
       ELK_SFID_DATAPORT_READ);
   const unsigned exec_size = 1 << elk_get_default_exec_size(p);

   /* On newer hardware, offset is in units of owords. */
   if (devinfo->ver >= 6)
      offset /= 16;

   mrf = retype(mrf, ELK_REGISTER_TYPE_UD);

   elk_push_insn_state(p);
   elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
   elk_set_default_flag_reg(p, 0, 0);
   elk_set_default_compression_control(p, ELK_COMPRESSION_NONE);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);

   elk_push_insn_state(p);
   elk_set_default_exec_size(p, ELK_EXECUTE_8);
   elk_MOV(p, mrf, retype(elk_vec8_grf(0, 0), ELK_REGISTER_TYPE_UD));

   /* set message header global offset field (reg 0, element 2) */
   elk_set_default_exec_size(p, ELK_EXECUTE_1);
   elk_MOV(p,
	   retype(elk_vec1_reg(ELK_MESSAGE_REGISTER_FILE,
			       mrf.nr,
			       2), ELK_REGISTER_TYPE_UD),
	   elk_imm_ud(offset));
   elk_pop_insn_state(p);

   elk_inst *insn = next_insn(p, ELK_OPCODE_SEND);

   elk_inst_set_sfid(devinfo, insn, target_cache);

   /* cast dest to a uword[8] vector */
   dest = retype(vec8(dest), ELK_REGISTER_TYPE_UW);

   elk_set_dest(p, insn, dest);
   if (devinfo->ver >= 6) {
      elk_set_src0(p, insn, mrf);
   } else {
      elk_set_src0(p, insn, elk_null_reg());
      elk_inst_set_base_mrf(devinfo, insn, mrf.nr);
   }

   elk_set_desc(p, insn,
                elk_message_desc(devinfo, 1, DIV_ROUND_UP(exec_size, 8), true) |
                elk_dp_read_desc(devinfo, bind_table_index,
                                 ELK_DATAPORT_OWORD_BLOCK_DWORDS(exec_size),
                                 ELK_DATAPORT_READ_MESSAGE_OWORD_BLOCK_READ,
                                 ELK_DATAPORT_READ_TARGET_DATA_CACHE));

   elk_pop_insn_state(p);
}

elk_inst *
elk_fb_WRITE(struct elk_codegen *p,
             struct elk_reg payload,
             struct elk_reg implied_header,
             unsigned msg_control,
             unsigned binding_table_index,
             unsigned msg_length,
             unsigned response_length,
             bool eot,
             bool last_render_target,
             bool header_present)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const unsigned target_cache =
      (devinfo->ver >= 6 ? GFX6_SFID_DATAPORT_RENDER_CACHE :
       ELK_SFID_DATAPORT_WRITE);
   elk_inst *insn;
   struct elk_reg dest, src0;

   if (elk_get_default_exec_size(p) >= ELK_EXECUTE_16)
      dest = retype(vec16(elk_null_reg()), ELK_REGISTER_TYPE_UW);
   else
      dest = retype(vec8(elk_null_reg()), ELK_REGISTER_TYPE_UW);

   if (devinfo->ver >= 6) {
      insn = next_insn(p, ELK_OPCODE_SENDC);
   } else {
      insn = next_insn(p, ELK_OPCODE_SEND);
   }
   elk_inst_set_sfid(devinfo, insn, target_cache);
   elk_inst_set_compression(devinfo, insn, false);

   if (devinfo->ver >= 6) {
      /* headerless version, just submit color payload */
      src0 = payload;
   } else {
      assert(payload.file == ELK_MESSAGE_REGISTER_FILE);
      elk_inst_set_base_mrf(devinfo, insn, payload.nr);
      src0 = implied_header;
   }

   elk_set_dest(p, insn, dest);
   elk_set_src0(p, insn, src0);
   elk_set_desc(p, insn,
                elk_message_desc(devinfo, msg_length, response_length,
                                 header_present) |
                elk_fb_write_desc(devinfo, binding_table_index, msg_control,
                                  last_render_target,
                                  false /* coarse_write */));
   elk_inst_set_eot(devinfo, insn, eot);

   return insn;
}

/**
 * Texture sample instruction.
 * Note: the msg_type plus msg_length values determine exactly what kind
 * of sampling operation is performed.  See volume 4, page 161 of docs.
 */
void elk_SAMPLE(struct elk_codegen *p,
		struct elk_reg dest,
		unsigned msg_reg_nr,
		struct elk_reg src0,
		unsigned binding_table_index,
		unsigned sampler,
		unsigned msg_type,
		unsigned response_length,
		unsigned msg_length,
		unsigned header_present,
		unsigned simd_mode,
		unsigned return_format)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn;

   if (msg_reg_nr != -1)
      elk_gfx6_resolve_implied_move(p, &src0, msg_reg_nr);

   insn = next_insn(p, ELK_OPCODE_SEND);
   elk_inst_set_sfid(devinfo, insn, ELK_SFID_SAMPLER);
   elk_inst_set_pred_control(devinfo, insn, ELK_PREDICATE_NONE); /* XXX */

   /* From the 965 PRM (volume 4, part 1, section 14.2.41):
    *
    *    "Instruction compression is not allowed for this instruction (that
    *     is, send). The hardware behavior is undefined if this instruction is
    *     set as compressed. However, compress control can be set to "SecHalf"
    *     to affect the EMask generation."
    *
    * No similar wording is found in later PRMs, but there are examples
    * utilizing send with SecHalf.  More importantly, SIMD8 sampler messages
    * are allowed in SIMD16 mode and they could not work without SecHalf.  For
    * these reasons, we allow ELK_COMPRESSION_2NDHALF here.
    */
   elk_inst_set_compression(devinfo, insn, false);

   if (devinfo->ver < 6)
      elk_inst_set_base_mrf(devinfo, insn, msg_reg_nr);

   elk_set_dest(p, insn, dest);
   elk_set_src0(p, insn, src0);
   elk_set_desc(p, insn,
                elk_message_desc(devinfo, msg_length, response_length,
                                 header_present) |
                elk_sampler_desc(devinfo, binding_table_index, sampler,
                                 msg_type, simd_mode, return_format));
}

/* Adjust the message header's sampler state pointer to
 * select the correct group of 16 samplers.
 */
void elk_adjust_sampler_state_pointer(struct elk_codegen *p,
                                      struct elk_reg header,
                                      struct elk_reg sampler_index)
{
   /* The "Sampler Index" field can only store values between 0 and 15.
    * However, we can add an offset to the "Sampler State Pointer"
    * field, effectively selecting a different set of 16 samplers.
    *
    * The "Sampler State Pointer" needs to be aligned to a 32-byte
    * offset, and each sampler state is only 16-bytes, so we can't
    * exclusively use the offset - we have to use both.
    */

   const struct intel_device_info *devinfo = p->devinfo;

   if (sampler_index.file == ELK_IMMEDIATE_VALUE) {
      const int sampler_state_size = 16; /* 16 bytes */
      uint32_t sampler = sampler_index.ud;

      if (sampler >= 16) {
         assert(devinfo->verx10 >= 75);
         elk_ADD(p,
                 get_element_ud(header, 3),
                 get_element_ud(elk_vec8_grf(0, 0), 3),
                 elk_imm_ud(16 * (sampler / 16) * sampler_state_size));
      }
   } else {
      /* Non-const sampler array indexing case */
      if (devinfo->verx10 <= 70) {
         return;
      }

      struct elk_reg temp = get_element_ud(header, 3);

      elk_push_insn_state(p);
      elk_AND(p, temp, get_element_ud(sampler_index, 0), elk_imm_ud(0x0f0));
      elk_SHL(p, temp, temp, elk_imm_ud(4));
      elk_ADD(p,
              get_element_ud(header, 3),
              get_element_ud(elk_vec8_grf(0, 0), 3),
              temp);
      elk_pop_insn_state(p);
   }
}

/* All these variables are pretty confusing - we might be better off
 * using bitmasks and macros for this, in the old style.  Or perhaps
 * just having the caller instantiate the fields in dword3 itself.
 */
void elk_urb_WRITE(struct elk_codegen *p,
		   struct elk_reg dest,
		   unsigned msg_reg_nr,
		   struct elk_reg src0,
                   enum elk_urb_write_flags flags,
		   unsigned msg_length,
		   unsigned response_length,
		   unsigned offset,
		   unsigned swizzle)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn;

   elk_gfx6_resolve_implied_move(p, &src0, msg_reg_nr);

   if (devinfo->ver >= 7 && !(flags & ELK_URB_WRITE_USE_CHANNEL_MASKS)) {
      /* Enable Channel Masks in the URB_WRITE_HWORD message header */
      elk_push_insn_state(p);
      elk_set_default_access_mode(p, ELK_ALIGN_1);
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);
      elk_set_default_exec_size(p, ELK_EXECUTE_1);
      elk_OR(p, retype(elk_vec1_reg(ELK_MESSAGE_REGISTER_FILE, msg_reg_nr, 5),
		       ELK_REGISTER_TYPE_UD),
	        retype(elk_vec1_grf(0, 5), ELK_REGISTER_TYPE_UD),
		elk_imm_ud(0xff00));
      elk_pop_insn_state(p);
   }

   insn = next_insn(p, ELK_OPCODE_SEND);

   assert(msg_length < ELK_MAX_MRF(devinfo->ver));

   elk_set_dest(p, insn, dest);
   elk_set_src0(p, insn, src0);
   elk_set_src1(p, insn, elk_imm_d(0));

   if (devinfo->ver < 6)
      elk_inst_set_base_mrf(devinfo, insn, msg_reg_nr);

   elk_set_urb_message(p,
		       insn,
		       flags,
		       msg_length,
		       response_length,
		       offset,
		       swizzle);
}

void
elk_send_indirect_message(struct elk_codegen *p,
                          unsigned sfid,
                          struct elk_reg dst,
                          struct elk_reg payload,
                          struct elk_reg desc,
                          unsigned desc_imm,
                          bool eot)
{
   const struct intel_device_info *devinfo = p->devinfo;
   struct elk_inst *send;

   dst = retype(dst, ELK_REGISTER_TYPE_UW);

   assert(desc.type == ELK_REGISTER_TYPE_UD);

   if (desc.file == ELK_IMMEDIATE_VALUE) {
      send = next_insn(p, ELK_OPCODE_SEND);
      elk_set_src0(p, send, retype(payload, ELK_REGISTER_TYPE_UD));
      elk_set_desc(p, send, desc.ud | desc_imm);
   } else {
      struct elk_reg addr = retype(elk_address_reg(0), ELK_REGISTER_TYPE_UD);

      elk_push_insn_state(p);
      elk_set_default_access_mode(p, ELK_ALIGN_1);
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);
      elk_set_default_exec_size(p, ELK_EXECUTE_1);
      elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
      elk_set_default_flag_reg(p, 0, 0);

      /* Load the indirect descriptor to an address register using OR so the
       * caller can specify additional descriptor bits with the desc_imm
       * immediate.
       */
      elk_OR(p, addr, desc, elk_imm_ud(desc_imm));

      elk_pop_insn_state(p);

      send = next_insn(p, ELK_OPCODE_SEND);
      elk_set_src0(p, send, retype(payload, ELK_REGISTER_TYPE_UD));
      elk_set_src1(p, send, addr);
   }

   elk_set_dest(p, send, dst);
   elk_inst_set_sfid(devinfo, send, sfid);
   elk_inst_set_eot(devinfo, send, eot);
}

static void
elk_send_indirect_surface_message(struct elk_codegen *p,
                                  unsigned sfid,
                                  struct elk_reg dst,
                                  struct elk_reg payload,
                                  struct elk_reg surface,
                                  unsigned desc_imm)
{
   if (surface.file != ELK_IMMEDIATE_VALUE) {
      struct elk_reg addr = retype(elk_address_reg(0), ELK_REGISTER_TYPE_UD);

      elk_push_insn_state(p);
      elk_set_default_access_mode(p, ELK_ALIGN_1);
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);
      elk_set_default_exec_size(p, ELK_EXECUTE_1);
      elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
      elk_set_default_flag_reg(p, 0, 0);

      /* Mask out invalid bits from the surface index to avoid hangs e.g. when
       * some surface array is accessed out of bounds.
       */
      elk_AND(p, addr,
              suboffset(vec1(retype(surface, ELK_REGISTER_TYPE_UD)),
                        ELK_GET_SWZ(surface.swizzle, 0)),
              elk_imm_ud(0xff));

      elk_pop_insn_state(p);

      surface = addr;
   }

   elk_send_indirect_message(p, sfid, dst, payload, surface, desc_imm, false);
}

static bool
while_jumps_before_offset(const struct intel_device_info *devinfo,
                          elk_inst *insn, int while_offset, int start_offset)
{
   int scale = 16 / elk_jump_scale(devinfo);
   int jip = devinfo->ver == 6 ? elk_inst_gfx6_jump_count(devinfo, insn)
                               : elk_inst_jip(devinfo, insn);
   assert(jip < 0);
   return while_offset + jip * scale <= start_offset;
}


static int
elk_find_next_block_end(struct elk_codegen *p, int start_offset)
{
   int offset;
   void *store = p->store;
   const struct intel_device_info *devinfo = p->devinfo;

   int depth = 0;

   for (offset = next_offset(devinfo, store, start_offset);
        offset < p->next_insn_offset;
        offset = next_offset(devinfo, store, offset)) {
      elk_inst *insn = store + offset;

      switch (elk_inst_opcode(p->isa, insn)) {
      case ELK_OPCODE_IF:
         depth++;
         break;
      case ELK_OPCODE_ENDIF:
         if (depth == 0)
            return offset;
         depth--;
         break;
      case ELK_OPCODE_WHILE:
         /* If the while doesn't jump before our instruction, it's the end
          * of a sibling do...while loop.  Ignore it.
          */
         if (!while_jumps_before_offset(devinfo, insn, offset, start_offset))
            continue;
         FALLTHROUGH;
      case ELK_OPCODE_ELSE:
      case ELK_OPCODE_HALT:
         if (depth == 0)
            return offset;
         break;
      default:
         break;
      }
   }

   return 0;
}

/* There is no DO instruction on gfx6, so to find the end of the loop
 * we have to see if the loop is jumping back before our start
 * instruction.
 */
static int
elk_find_loop_end(struct elk_codegen *p, int start_offset)
{
   const struct intel_device_info *devinfo = p->devinfo;
   int offset;
   void *store = p->store;

   assert(devinfo->ver >= 6);

   /* Always start after the instruction (such as a WHILE) we're trying to fix
    * up.
    */
   for (offset = next_offset(devinfo, store, start_offset);
        offset < p->next_insn_offset;
        offset = next_offset(devinfo, store, offset)) {
      elk_inst *insn = store + offset;

      if (elk_inst_opcode(p->isa, insn) == ELK_OPCODE_WHILE) {
	 if (while_jumps_before_offset(devinfo, insn, offset, start_offset))
	    return offset;
      }
   }
   assert(!"not reached");
   return start_offset;
}

/* After program generation, go back and update the UIP and JIP of
 * BREAK, CONT, and HALT instructions to their correct locations.
 */
void
elk_set_uip_jip(struct elk_codegen *p, int start_offset)
{
   const struct intel_device_info *devinfo = p->devinfo;
   int offset;
   int br = elk_jump_scale(devinfo);
   int scale = 16 / br;
   void *store = p->store;

   if (devinfo->ver < 6)
      return;

   for (offset = start_offset; offset < p->next_insn_offset; offset += 16) {
      elk_inst *insn = store + offset;
      assert(elk_inst_cmpt_control(devinfo, insn) == 0);

      switch (elk_inst_opcode(p->isa, insn)) {
      case ELK_OPCODE_BREAK: {
         int block_end_offset = elk_find_next_block_end(p, offset);
         assert(block_end_offset != 0);
         elk_inst_set_jip(devinfo, insn, (block_end_offset - offset) / scale);
	 /* Gfx7 UIP points to WHILE; Gfx6 points just after it */
         elk_inst_set_uip(devinfo, insn,
	    (elk_find_loop_end(p, offset) - offset +
             (devinfo->ver == 6 ? 16 : 0)) / scale);
	 break;
      }

      case ELK_OPCODE_CONTINUE: {
         int block_end_offset = elk_find_next_block_end(p, offset);
         assert(block_end_offset != 0);
         elk_inst_set_jip(devinfo, insn, (block_end_offset - offset) / scale);
         elk_inst_set_uip(devinfo, insn,
            (elk_find_loop_end(p, offset) - offset) / scale);

         assert(elk_inst_uip(devinfo, insn) != 0);
         assert(elk_inst_jip(devinfo, insn) != 0);
	 break;
      }

      case ELK_OPCODE_ENDIF: {
         int block_end_offset = elk_find_next_block_end(p, offset);
         int32_t jump = (block_end_offset == 0) ?
                        1 * br : (block_end_offset - offset) / scale;
         if (devinfo->ver >= 7)
            elk_inst_set_jip(devinfo, insn, jump);
         else
            elk_inst_set_gfx6_jump_count(devinfo, insn, jump);
	 break;
      }

      case ELK_OPCODE_HALT: {
	 /* From the Sandy Bridge PRM (volume 4, part 2, section 8.3.19):
	  *
	  *    "In case of the halt instruction not inside any conditional
	  *     code block, the value of <JIP> and <UIP> should be the
	  *     same. In case of the halt instruction inside conditional code
	  *     block, the <UIP> should be the end of the program, and the
	  *     <JIP> should be end of the most inner conditional code block."
	  *
	  * The uip will have already been set by whoever set up the
	  * instruction.
	  */
         int block_end_offset = elk_find_next_block_end(p, offset);
	 if (block_end_offset == 0) {
            elk_inst_set_jip(devinfo, insn, elk_inst_uip(devinfo, insn));
	 } else {
            elk_inst_set_jip(devinfo, insn, (block_end_offset - offset) / scale);
	 }
         assert(elk_inst_uip(devinfo, insn) != 0);
         assert(elk_inst_jip(devinfo, insn) != 0);
	 break;
      }

      default:
         break;
      }
   }
}

void elk_ff_sync(struct elk_codegen *p,
		   struct elk_reg dest,
		   unsigned msg_reg_nr,
		   struct elk_reg src0,
		   bool allocate,
		   unsigned response_length,
		   bool eot)
{
   const struct intel_device_info *devinfo = p->devinfo;
   elk_inst *insn;

   elk_gfx6_resolve_implied_move(p, &src0, msg_reg_nr);

   insn = next_insn(p, ELK_OPCODE_SEND);
   elk_set_dest(p, insn, dest);
   elk_set_src0(p, insn, src0);
   elk_set_src1(p, insn, elk_imm_d(0));

   if (devinfo->ver < 6)
      elk_inst_set_base_mrf(devinfo, insn, msg_reg_nr);

   elk_set_ff_sync_message(p,
			   insn,
			   allocate,
			   response_length,
			   eot);
}

/**
 * Emit the SEND instruction necessary to generate stream output data on Gfx6
 * (for transform feedback).
 *
 * If send_commit_msg is true, this is the last piece of stream output data
 * from this thread, so send the data as a committed write.  According to the
 * Sandy Bridge PRM (volume 2 part 1, section 4.5.1):
 *
 *   "Prior to End of Thread with a URB_WRITE, the kernel must ensure all
 *   writes are complete by sending the final write as a committed write."
 */
void
elk_svb_write(struct elk_codegen *p,
              struct elk_reg dest,
              unsigned msg_reg_nr,
              struct elk_reg src0,
              unsigned binding_table_index,
              bool   send_commit_msg)
{
   const struct intel_device_info *devinfo = p->devinfo;
   assert(devinfo->ver == 6);
   const unsigned target_cache = GFX6_SFID_DATAPORT_RENDER_CACHE;
   elk_inst *insn;

   elk_gfx6_resolve_implied_move(p, &src0, msg_reg_nr);

   insn = next_insn(p, ELK_OPCODE_SEND);
   elk_inst_set_sfid(devinfo, insn, target_cache);
   elk_set_dest(p, insn, dest);
   elk_set_src0(p, insn, src0);
   elk_set_desc(p, insn,
                elk_message_desc(devinfo, 1, send_commit_msg, true) |
                elk_dp_write_desc(devinfo, binding_table_index,
                                  0, /* msg_control: ignored */
                                  GFX6_DATAPORT_WRITE_MESSAGE_STREAMED_VB_WRITE,
                                  send_commit_msg)); /* send_commit_msg */
}

static unsigned
elk_surface_payload_size(unsigned num_channels,
                         unsigned exec_size /**< 0 for SIMD4x2 */)
{
   if (exec_size == 0)
      return 1; /* SIMD4x2 */
   else if (exec_size <= 8)
      return num_channels;
   else
      return 2 * num_channels;
}

void
elk_untyped_atomic(struct elk_codegen *p,
                   struct elk_reg dst,
                   struct elk_reg payload,
                   struct elk_reg surface,
                   unsigned atomic_op,
                   unsigned msg_length,
                   bool response_expected,
                   bool header_present)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const unsigned sfid = (devinfo->verx10 >= 75 ?
                          HSW_SFID_DATAPORT_DATA_CACHE_1 :
                          GFX7_SFID_DATAPORT_DATA_CACHE);
   const bool align1 = elk_get_default_access_mode(p) == ELK_ALIGN_1;
   /* SIMD4x2 untyped atomic instructions only exist on HSW+ */
   const bool has_simd4x2 = devinfo->verx10 >= 75;
   const unsigned exec_size = align1 ? 1 << elk_get_default_exec_size(p) :
                              has_simd4x2 ? 0 : 8;
   const unsigned response_length =
      elk_surface_payload_size(response_expected, exec_size);
   const unsigned desc =
      elk_message_desc(devinfo, msg_length, response_length, header_present) |
      elk_dp_untyped_atomic_desc(devinfo, exec_size, atomic_op,
                                 response_expected);
   /* Mask out unused components -- This is especially important in Align16
    * mode on generations that don't have native support for SIMD4x2 atomics,
    * because unused but enabled components will cause the dataport to perform
    * additional atomic operations on the addresses that happen to be in the
    * uninitialized Y, Z and W coordinates of the payload.
    */
   const unsigned mask = align1 ? WRITEMASK_XYZW : WRITEMASK_X;

   elk_send_indirect_surface_message(p, sfid, elk_writemask(dst, mask),
                                     payload, surface, desc);
}

void
elk_untyped_surface_read(struct elk_codegen *p,
                         struct elk_reg dst,
                         struct elk_reg payload,
                         struct elk_reg surface,
                         unsigned msg_length,
                         unsigned num_channels)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const unsigned sfid = (devinfo->verx10 >= 75 ?
                          HSW_SFID_DATAPORT_DATA_CACHE_1 :
                          GFX7_SFID_DATAPORT_DATA_CACHE);
   const bool align1 = elk_get_default_access_mode(p) == ELK_ALIGN_1;
   const unsigned exec_size = align1 ? 1 << elk_get_default_exec_size(p) : 0;
   const unsigned response_length =
      elk_surface_payload_size(num_channels, exec_size);
   const unsigned desc =
      elk_message_desc(devinfo, msg_length, response_length, false) |
      elk_dp_untyped_surface_rw_desc(devinfo, exec_size, num_channels, false);

   elk_send_indirect_surface_message(p, sfid, dst, payload, surface, desc);
}

void
elk_untyped_surface_write(struct elk_codegen *p,
                          struct elk_reg payload,
                          struct elk_reg surface,
                          unsigned msg_length,
                          unsigned num_channels,
                          bool header_present)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const unsigned sfid = (devinfo->verx10 >= 75 ?
                          HSW_SFID_DATAPORT_DATA_CACHE_1 :
                          GFX7_SFID_DATAPORT_DATA_CACHE);
   const bool align1 = elk_get_default_access_mode(p) == ELK_ALIGN_1;
   /* SIMD4x2 untyped surface write instructions only exist on HSW+ */
   const bool has_simd4x2 = devinfo->verx10 >= 75;
   const unsigned exec_size = align1 ? 1 << elk_get_default_exec_size(p) :
                              has_simd4x2 ? 0 : 8;
   const unsigned desc =
      elk_message_desc(devinfo, msg_length, 0, header_present) |
      elk_dp_untyped_surface_rw_desc(devinfo, exec_size, num_channels, true);
   /* Mask out unused components -- See comment in elk_untyped_atomic(). */
   const unsigned mask = !has_simd4x2 && !align1 ? WRITEMASK_X : WRITEMASK_XYZW;

   elk_send_indirect_surface_message(p, sfid, elk_writemask(elk_null_reg(), mask),
                                     payload, surface, desc);
}

static void
elk_set_memory_fence_message(struct elk_codegen *p,
                             struct elk_inst *insn,
                             enum elk_message_target sfid,
                             bool commit_enable,
                             unsigned bti)
{
   const struct intel_device_info *devinfo = p->devinfo;

   elk_set_desc(p, insn, elk_message_desc(
                   devinfo, 1, (commit_enable ? 1 : 0), true));

   elk_inst_set_sfid(devinfo, insn, sfid);

   switch (sfid) {
   case GFX6_SFID_DATAPORT_RENDER_CACHE:
      elk_inst_set_dp_msg_type(devinfo, insn, GFX7_DATAPORT_RC_MEMORY_FENCE);
      break;
   case GFX7_SFID_DATAPORT_DATA_CACHE:
      elk_inst_set_dp_msg_type(devinfo, insn, GFX7_DATAPORT_DC_MEMORY_FENCE);
      break;
   default:
      unreachable("Not reached");
   }

   if (commit_enable)
      elk_inst_set_dp_msg_control(devinfo, insn, 1 << 5);

   assert(bti == 0);
   elk_inst_set_binding_table_index(devinfo, insn, bti);
}

void
elk_memory_fence(struct elk_codegen *p,
                 struct elk_reg dst,
                 struct elk_reg src,
                 enum elk_opcode send_op,
                 enum elk_message_target sfid,
                 uint32_t desc,
                 bool commit_enable,
                 unsigned bti)
{
   const struct intel_device_info *devinfo = p->devinfo;

   dst = retype(vec1(dst), ELK_REGISTER_TYPE_UW);
   src = retype(vec1(src), ELK_REGISTER_TYPE_UD);

   /* Set dst as destination for dependency tracking, the MEMORY_FENCE
    * message doesn't write anything back.
    */
   struct elk_inst *insn = next_insn(p, send_op);
   elk_inst_set_mask_control(devinfo, insn, ELK_MASK_DISABLE);
   elk_inst_set_exec_size(devinfo, insn, ELK_EXECUTE_1);
   elk_set_dest(p, insn, dst);
   elk_set_src0(p, insn, src);

   elk_set_memory_fence_message(p, insn, sfid, commit_enable, bti);
}

void
elk_find_live_channel(struct elk_codegen *p, struct elk_reg dst, bool last)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const unsigned exec_size = 1 << elk_get_default_exec_size(p);
   const unsigned qtr_control = elk_get_default_group(p) / 8;
   elk_inst *inst;

   assert(devinfo->ver == 7);

   elk_push_insn_state(p);

   /* The flag register is only used on Gfx7 in align1 mode, so avoid setting
    * unnecessary bits in the instruction words, get the information we need
    * and reset the default flag register. This allows more instructions to be
    * compacted.
    */
   const unsigned flag_subreg = p->current->flag_subreg;
   elk_set_default_flag_reg(p, 0, 0);

   if (elk_get_default_access_mode(p) == ELK_ALIGN_1) {
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);

      const struct elk_reg flag = elk_flag_subreg(flag_subreg);

      elk_set_default_exec_size(p, ELK_EXECUTE_1);
      elk_MOV(p, retype(flag, ELK_REGISTER_TYPE_UD), elk_imm_ud(0));

      /* Run enough instructions returning zero with execution masking and
       * a conditional modifier enabled in order to get the full execution
       * mask in f1.0.  We could use a single 32-wide move here if it
       * weren't because of the hardware bug that causes channel enables to
       * be applied incorrectly to the second half of 32-wide instructions
       * on Gfx7.
       */
      const unsigned lower_size = MIN2(16, exec_size);
      for (unsigned i = 0; i < exec_size / lower_size; i++) {
         inst = elk_MOV(p, retype(elk_null_reg(), ELK_REGISTER_TYPE_UW),
                        elk_imm_uw(0));
         elk_inst_set_mask_control(devinfo, inst, ELK_MASK_ENABLE);
         elk_inst_set_group(devinfo, inst, lower_size * i + 8 * qtr_control);
         elk_inst_set_cond_modifier(devinfo, inst, ELK_CONDITIONAL_Z);
         elk_inst_set_exec_size(devinfo, inst, cvt(lower_size) - 1);
         elk_inst_set_flag_reg_nr(devinfo, inst, flag_subreg / 2);
         elk_inst_set_flag_subreg_nr(devinfo, inst, flag_subreg % 2);
      }

      /* Find the first bit set in the exec_size-wide portion of the flag
       * register that was updated by the last sequence of MOV
       * instructions.
       */
      const enum elk_reg_type type = elk_int_type(exec_size / 8, false);
      elk_set_default_exec_size(p, ELK_EXECUTE_1);
      if (!last) {
         inst = elk_FBL(p, vec1(dst), byte_offset(retype(flag, type), qtr_control));
      } else {
         inst = elk_LZD(p, vec1(dst), byte_offset(retype(flag, type), qtr_control));
         struct elk_reg neg = vec1(dst);
         neg.negate = true;
         inst = elk_ADD(p, vec1(dst), neg, elk_imm_uw(31));
      }
   } else {
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);

      /* Overwrite the destination without and with execution masking to
       * find out which of the channels is active.
       */
      elk_push_insn_state(p);
      elk_set_default_exec_size(p, ELK_EXECUTE_4);
      elk_MOV(p, elk_writemask(vec4(dst), WRITEMASK_X),
              elk_imm_ud(1));

      inst = elk_MOV(p, elk_writemask(vec4(dst), WRITEMASK_X),
                     elk_imm_ud(0));
      elk_pop_insn_state(p);
      elk_inst_set_mask_control(devinfo, inst, ELK_MASK_ENABLE);
   }

   elk_pop_insn_state(p);
}

void
elk_broadcast(struct elk_codegen *p,
              struct elk_reg dst,
              struct elk_reg src,
              struct elk_reg idx)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const bool align1 = elk_get_default_access_mode(p) == ELK_ALIGN_1;
   elk_inst *inst;

   elk_push_insn_state(p);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_set_default_exec_size(p, align1 ? ELK_EXECUTE_1 : ELK_EXECUTE_4);

   assert(src.file == ELK_GENERAL_REGISTER_FILE &&
          src.address_mode == ELK_ADDRESS_DIRECT);
   assert(!src.abs && !src.negate);

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

   if ((src.vstride == 0 && (src.hstride == 0 || !align1)) ||
       idx.file == ELK_IMMEDIATE_VALUE) {
      /* Trivial, the source is already uniform or the index is a constant.
       * We will typically not get here if the optimizer is doing its job, but
       * asserting would be mean.
       */
      const unsigned i = idx.file == ELK_IMMEDIATE_VALUE ? idx.ud : 0;
      src = align1 ? stride(suboffset(src, i), 0, 1, 0) :
                     stride(suboffset(src, 4 * i), 0, 4, 1);

      if (type_sz(src.type) > 4 && !devinfo->has_64bit_int) {
         elk_MOV(p, subscript(dst, ELK_REGISTER_TYPE_D, 0),
                    subscript(src, ELK_REGISTER_TYPE_D, 0));
         elk_MOV(p, subscript(dst, ELK_REGISTER_TYPE_D, 1),
                    subscript(src, ELK_REGISTER_TYPE_D, 1));
      } else {
         elk_MOV(p, dst, src);
      }
   } else {
      /* From the Haswell PRM section "Register Region Restrictions":
       *
       *    "The lower bits of the AddressImmediate must not overflow to
       *    change the register address.  The lower 5 bits of Address
       *    Immediate when added to lower 5 bits of address register gives
       *    the sub-register offset. The upper bits of Address Immediate
       *    when added to upper bits of address register gives the register
       *    address. Any overflow from sub-register offset is dropped."
       *
       * Fortunately, for broadcast, we never have a sub-register offset so
       * this isn't an issue.
       */
      assert(src.subnr == 0);

      if (align1) {
         const struct elk_reg addr =
            retype(elk_address_reg(0), ELK_REGISTER_TYPE_UD);
         unsigned offset = src.nr * REG_SIZE + src.subnr;
         /* Limit in bytes of the signed indirect addressing immediate. */
         const unsigned limit = 512;

         elk_push_insn_state(p);
         elk_set_default_mask_control(p, ELK_MASK_DISABLE);
         elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
         elk_set_default_flag_reg(p, 0, 0);

         /* Take into account the component size and horizontal stride. */
         assert(src.vstride == src.hstride + src.width);
         elk_SHL(p, addr, vec1(idx),
                 elk_imm_ud(util_logbase2(type_sz(src.type)) +
                            src.hstride - 1));

         /* We can only address up to limit bytes using the indirect
          * addressing immediate, account for the difference if the source
          * register is above this limit.
          */
         if (offset >= limit) {
            elk_ADD(p, addr, addr, elk_imm_ud(offset - offset % limit));
            offset = offset % limit;
         }

         elk_pop_insn_state(p);

         /* Use indirect addressing to fetch the specified component. */
         if (type_sz(src.type) > 4 &&
             (devinfo->platform == INTEL_PLATFORM_CHV || !devinfo->has_64bit_int)) {
            /* From the Cherryview PRM Vol 7. "Register Region Restrictions":
             *
             *    "When source or destination datatype is 64b or operation is
             *    integer DWord multiply, indirect addressing must not be
             *    used."
             *
             * To work around both of this issue, we do two integer MOVs
             * insead of one 64-bit MOV.  Because no double value should ever
             * cross a register boundary, it's safe to use the immediate
             * offset in the indirect here to handle adding 4 bytes to the
             * offset and avoid the extra ADD to the register file.
             */
            elk_MOV(p, subscript(dst, ELK_REGISTER_TYPE_D, 0),
                       retype(elk_vec1_indirect(addr.subnr, offset),
                              ELK_REGISTER_TYPE_D));
            elk_MOV(p, subscript(dst, ELK_REGISTER_TYPE_D, 1),
                       retype(elk_vec1_indirect(addr.subnr, offset + 4),
                              ELK_REGISTER_TYPE_D));
         } else {
            elk_MOV(p, dst,
                    retype(elk_vec1_indirect(addr.subnr, offset), src.type));
         }
      } else {
         /* In SIMD4x2 mode the index can be either zero or one, replicate it
          * to all bits of a flag register,
          */
         inst = elk_MOV(p,
                        elk_null_reg(),
                        stride(elk_swizzle(idx, ELK_SWIZZLE_XXXX), 4, 4, 1));
         elk_inst_set_pred_control(devinfo, inst, ELK_PREDICATE_NONE);
         elk_inst_set_cond_modifier(devinfo, inst, ELK_CONDITIONAL_NZ);
         elk_inst_set_flag_reg_nr(devinfo, inst, 1);

         /* and use predicated SEL to pick the right channel. */
         inst = elk_SEL(p, dst,
                        stride(suboffset(src, 4), 4, 4, 1),
                        stride(src, 4, 4, 1));
         elk_inst_set_pred_control(devinfo, inst, ELK_PREDICATE_NORMAL);
         elk_inst_set_flag_reg_nr(devinfo, inst, 1);
      }
   }

   elk_pop_insn_state(p);
}


/**
 * Emit the SEND message for a barrier
 */
void
elk_barrier(struct elk_codegen *p, struct elk_reg src)
{
   const struct intel_device_info *devinfo = p->devinfo;
   struct elk_inst *inst;

   assert(devinfo->ver >= 7);

   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   inst = next_insn(p, ELK_OPCODE_SEND);
   elk_set_dest(p, inst, retype(elk_null_reg(), ELK_REGISTER_TYPE_UW));
   elk_set_src0(p, inst, src);
   elk_set_src1(p, inst, elk_null_reg());
   elk_set_desc(p, inst, elk_message_desc(devinfo,
                                          1 * reg_unit(devinfo), 0, false));

   elk_inst_set_sfid(devinfo, inst, ELK_SFID_MESSAGE_GATEWAY);
   elk_inst_set_gateway_subfuncid(devinfo, inst,
                                  ELK_MESSAGE_GATEWAY_SFID_BARRIER_MSG);

   elk_inst_set_mask_control(devinfo, inst, ELK_MASK_DISABLE);
   elk_pop_insn_state(p);
}


/**
 * Emit the wait instruction for a barrier
 */
void
elk_WAIT(struct elk_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   struct elk_inst *insn;

   struct elk_reg src = elk_notification_reg();

   insn = next_insn(p, ELK_OPCODE_WAIT);
   elk_set_dest(p, insn, src);
   elk_set_src0(p, insn, src);
   elk_set_src1(p, insn, elk_null_reg());

   elk_inst_set_exec_size(devinfo, insn, ELK_EXECUTE_1);
   elk_inst_set_mask_control(devinfo, insn, ELK_MASK_DISABLE);
}

void
elk_float_controls_mode(struct elk_codegen *p,
                        unsigned mode, unsigned mask)
{
   assert(p->current->mask_control == ELK_MASK_DISABLE);

   /* From the Skylake PRM, Volume 7, page 760:
    *  "Implementation Restriction on Register Access: When the control
    *   register is used as an explicit source and/or destination, hardware
    *   does not ensure execution pipeline coherency. Software must set the
    *   thread control field to ‘switch’ for an instruction that uses
    *   control register as an explicit operand."
    */
   elk_inst *inst = elk_AND(p, elk_cr0_reg(0), elk_cr0_reg(0),
                            elk_imm_ud(~mask));
   elk_inst_set_exec_size(p->devinfo, inst, ELK_EXECUTE_1);
   elk_inst_set_thread_control(p->devinfo, inst, ELK_THREAD_SWITCH);

   if (mode) {
      elk_inst *inst_or = elk_OR(p, elk_cr0_reg(0), elk_cr0_reg(0),
                                 elk_imm_ud(mode));
      elk_inst_set_exec_size(p->devinfo, inst_or, ELK_EXECUTE_1);
      elk_inst_set_thread_control(p->devinfo, inst_or, ELK_THREAD_SWITCH);
   }
}

void
elk_update_reloc_imm(const struct elk_isa_info *isa,
                     elk_inst *inst,
                     uint32_t value)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   /* Sanity check that the instruction is a MOV of an immediate */
   assert(elk_inst_opcode(isa, inst) == ELK_OPCODE_MOV);
   assert(elk_inst_src0_reg_file(devinfo, inst) == ELK_IMMEDIATE_VALUE);

   /* If it was compacted, we can't safely rewrite */
   assert(elk_inst_cmpt_control(devinfo, inst) == 0);

   elk_inst_set_imm_ud(devinfo, inst, value);
}

/* A default value for constants that will be patched at run-time.
 * We pick an arbitrary value that prevents instruction compaction.
 */
#define DEFAULT_PATCH_IMM 0x4a7cc037

void
elk_MOV_reloc_imm(struct elk_codegen *p,
                  struct elk_reg dst,
                  enum elk_reg_type src_type,
                  uint32_t id)
{
   assert(type_sz(src_type) == 4);
   assert(type_sz(dst.type) == 4);

   elk_add_reloc(p, id, ELK_SHADER_RELOC_TYPE_MOV_IMM,
                 p->next_insn_offset, 0);

   elk_MOV(p, dst, retype(elk_imm_ud(DEFAULT_PATCH_IMM), src_type));
}
