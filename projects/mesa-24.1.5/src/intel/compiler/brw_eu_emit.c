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


#include "brw_eu_defines.h"
#include "brw_eu.h"

#include "util/ralloc.h"

void
brw_set_dest(struct brw_codegen *p, brw_inst *inst, struct brw_reg dest)
{
   const struct intel_device_info *devinfo = p->devinfo;

   if (dest.file == BRW_GENERAL_REGISTER_FILE)
      assert(dest.nr < XE2_MAX_GRF);

   /* The hardware has a restriction where a destination of size Byte with
    * a stride of 1 is only allowed for a packed byte MOV. For any other
    * instruction, the stride must be at least 2, even when the destination
    * is the NULL register.
    */
   if (dest.file == BRW_ARCHITECTURE_REGISTER_FILE &&
       dest.nr == BRW_ARF_NULL &&
       type_sz(dest.type) == 1 &&
       dest.hstride == BRW_HORIZONTAL_STRIDE_1) {
      dest.hstride = BRW_HORIZONTAL_STRIDE_2;
   }

   if (devinfo->ver >= 12 &&
       (brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SEND ||
        brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDC)) {
      assert(dest.file == BRW_GENERAL_REGISTER_FILE ||
             dest.file == BRW_ARCHITECTURE_REGISTER_FILE);
      assert(dest.address_mode == BRW_ADDRESS_DIRECT);
      assert(dest.subnr == 0);
      assert(brw_inst_exec_size(devinfo, inst) == BRW_EXECUTE_1 ||
             (dest.hstride == BRW_HORIZONTAL_STRIDE_1 &&
              dest.vstride == dest.width + 1));
      assert(!dest.negate && !dest.abs);
      brw_inst_set_dst_reg_file(devinfo, inst, dest.file);
      brw_inst_set_dst_da_reg_nr(devinfo, inst, phys_nr(devinfo, dest));

   } else if (brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDS ||
              brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDSC) {
      assert(devinfo->ver < 12);
      assert(dest.file == BRW_GENERAL_REGISTER_FILE ||
             dest.file == BRW_ARCHITECTURE_REGISTER_FILE);
      assert(dest.address_mode == BRW_ADDRESS_DIRECT);
      assert(dest.subnr % 16 == 0);
      assert(dest.hstride == BRW_HORIZONTAL_STRIDE_1 &&
             dest.vstride == dest.width + 1);
      assert(!dest.negate && !dest.abs);
      brw_inst_set_dst_da_reg_nr(devinfo, inst, dest.nr);
      brw_inst_set_dst_da16_subreg_nr(devinfo, inst, dest.subnr / 16);
      brw_inst_set_send_dst_reg_file(devinfo, inst, dest.file);
   } else {
      brw_inst_set_dst_file_type(devinfo, inst, dest.file, dest.type);
      brw_inst_set_dst_address_mode(devinfo, inst, dest.address_mode);

      if (dest.address_mode == BRW_ADDRESS_DIRECT) {
         brw_inst_set_dst_da_reg_nr(devinfo, inst, phys_nr(devinfo, dest));

         if (brw_inst_access_mode(devinfo, inst) == BRW_ALIGN_1) {
            brw_inst_set_dst_da1_subreg_nr(devinfo, inst, phys_subnr(devinfo, dest));
            if (dest.hstride == BRW_HORIZONTAL_STRIDE_0)
               dest.hstride = BRW_HORIZONTAL_STRIDE_1;
            brw_inst_set_dst_hstride(devinfo, inst, dest.hstride);
         } else {
            brw_inst_set_dst_da16_subreg_nr(devinfo, inst, dest.subnr / 16);
            brw_inst_set_da16_writemask(devinfo, inst, dest.writemask);
            if (dest.file == BRW_GENERAL_REGISTER_FILE) {
               assert(dest.writemask != 0);
            }
            /* From the Ivybridge PRM, Vol 4, Part 3, Section 5.2.4.1:
             *    Although Dst.HorzStride is a don't care for Align16, HW needs
             *    this to be programmed as "01".
             */
            brw_inst_set_dst_hstride(devinfo, inst, 1);
         }
      } else {
         brw_inst_set_dst_ia_subreg_nr(devinfo, inst, phys_subnr(devinfo, dest));

         /* These are different sizes in align1 vs align16:
          */
         if (brw_inst_access_mode(devinfo, inst) == BRW_ALIGN_1) {
            brw_inst_set_dst_ia1_addr_imm(devinfo, inst,
                                          dest.indirect_offset);
            if (dest.hstride == BRW_HORIZONTAL_STRIDE_0)
               dest.hstride = BRW_HORIZONTAL_STRIDE_1;
            brw_inst_set_dst_hstride(devinfo, inst, dest.hstride);
         } else {
            brw_inst_set_dst_ia16_addr_imm(devinfo, inst,
                                           dest.indirect_offset);
            /* even ignored in da16, still need to set as '01' */
            brw_inst_set_dst_hstride(devinfo, inst, 1);
         }
      }
   }
}

void
brw_set_src0(struct brw_codegen *p, brw_inst *inst, struct brw_reg reg)
{
   const struct intel_device_info *devinfo = p->devinfo;

   if (reg.file == BRW_GENERAL_REGISTER_FILE)
      assert(reg.nr < XE2_MAX_GRF);

   if (brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SEND  ||
       brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDC ||
       brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDS ||
       brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDSC) {
      /* Any source modifiers or regions will be ignored, since this just
       * identifies the GRF to start reading the message contents from.
       * Check for some likely failures.
       */
      assert(!reg.negate);
      assert(!reg.abs);
      assert(reg.address_mode == BRW_ADDRESS_DIRECT);
   }

   if (devinfo->ver >= 12 &&
       (brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SEND ||
        brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDC)) {
      assert(reg.file != BRW_IMMEDIATE_VALUE);
      assert(reg.address_mode == BRW_ADDRESS_DIRECT);
      assert(reg.subnr == 0);
      assert(has_scalar_region(reg) ||
             (reg.hstride == BRW_HORIZONTAL_STRIDE_1 &&
              reg.vstride == reg.width + 1));
      assert(!reg.negate && !reg.abs);
      brw_inst_set_send_src0_reg_file(devinfo, inst, reg.file);
      brw_inst_set_src0_da_reg_nr(devinfo, inst, phys_nr(devinfo, reg));

   } else if (brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDS ||
              brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDSC) {
      assert(reg.file == BRW_GENERAL_REGISTER_FILE);
      assert(reg.address_mode == BRW_ADDRESS_DIRECT);
      assert(reg.subnr % 16 == 0);
      assert(has_scalar_region(reg) ||
             (reg.hstride == BRW_HORIZONTAL_STRIDE_1 &&
              reg.vstride == reg.width + 1));
      assert(!reg.negate && !reg.abs);
      brw_inst_set_src0_da_reg_nr(devinfo, inst, reg.nr);
      brw_inst_set_src0_da16_subreg_nr(devinfo, inst, reg.subnr / 16);
   } else {
      brw_inst_set_src0_file_type(devinfo, inst, reg.file, reg.type);
      brw_inst_set_src0_abs(devinfo, inst, reg.abs);
      brw_inst_set_src0_negate(devinfo, inst, reg.negate);
      brw_inst_set_src0_address_mode(devinfo, inst, reg.address_mode);

      if (reg.file == BRW_IMMEDIATE_VALUE) {
         if (reg.type == BRW_REGISTER_TYPE_DF)
            brw_inst_set_imm_df(devinfo, inst, reg.df);
         else if (reg.type == BRW_REGISTER_TYPE_UQ ||
                  reg.type == BRW_REGISTER_TYPE_Q)
            brw_inst_set_imm_uq(devinfo, inst, reg.u64);
         else
            brw_inst_set_imm_ud(devinfo, inst, reg.ud);

         if (devinfo->ver < 12 && type_sz(reg.type) < 8) {
            brw_inst_set_src1_reg_file(devinfo, inst,
                                       BRW_ARCHITECTURE_REGISTER_FILE);
            brw_inst_set_src1_reg_hw_type(devinfo, inst,
                                          brw_inst_src0_reg_hw_type(devinfo, inst));
         }
      } else {
         if (reg.address_mode == BRW_ADDRESS_DIRECT) {
            brw_inst_set_src0_da_reg_nr(devinfo, inst, phys_nr(devinfo, reg));
            if (brw_inst_access_mode(devinfo, inst) == BRW_ALIGN_1) {
               brw_inst_set_src0_da1_subreg_nr(devinfo, inst, phys_subnr(devinfo, reg));
            } else {
               brw_inst_set_src0_da16_subreg_nr(devinfo, inst, reg.subnr / 16);
            }
         } else {
            brw_inst_set_src0_ia_subreg_nr(devinfo, inst, phys_subnr(devinfo, reg));

            if (brw_inst_access_mode(devinfo, inst) == BRW_ALIGN_1) {
               brw_inst_set_src0_ia1_addr_imm(devinfo, inst, reg.indirect_offset);
            } else {
               brw_inst_set_src0_ia16_addr_imm(devinfo, inst, reg.indirect_offset);
            }
         }

         if (brw_inst_access_mode(devinfo, inst) == BRW_ALIGN_1) {
            if (reg.width == BRW_WIDTH_1 &&
                brw_inst_exec_size(devinfo, inst) == BRW_EXECUTE_1) {
               brw_inst_set_src0_hstride(devinfo, inst, BRW_HORIZONTAL_STRIDE_0);
               brw_inst_set_src0_width(devinfo, inst, BRW_WIDTH_1);
               brw_inst_set_src0_vstride(devinfo, inst, BRW_VERTICAL_STRIDE_0);
            } else {
               brw_inst_set_src0_hstride(devinfo, inst, reg.hstride);
               brw_inst_set_src0_width(devinfo, inst, reg.width);
               brw_inst_set_src0_vstride(devinfo, inst, reg.vstride);
            }
         } else {
            brw_inst_set_src0_da16_swiz_x(devinfo, inst,
               BRW_GET_SWZ(reg.swizzle, BRW_CHANNEL_X));
            brw_inst_set_src0_da16_swiz_y(devinfo, inst,
               BRW_GET_SWZ(reg.swizzle, BRW_CHANNEL_Y));
            brw_inst_set_src0_da16_swiz_z(devinfo, inst,
               BRW_GET_SWZ(reg.swizzle, BRW_CHANNEL_Z));
            brw_inst_set_src0_da16_swiz_w(devinfo, inst,
               BRW_GET_SWZ(reg.swizzle, BRW_CHANNEL_W));

            if (reg.vstride == BRW_VERTICAL_STRIDE_8) {
               /* This is an oddity of the fact we're using the same
                * descriptions for registers in align_16 as align_1:
                */
               brw_inst_set_src0_vstride(devinfo, inst, BRW_VERTICAL_STRIDE_4);
            } else {
               brw_inst_set_src0_vstride(devinfo, inst, reg.vstride);
            }
         }
      }
   }
}


void
brw_set_src1(struct brw_codegen *p, brw_inst *inst, struct brw_reg reg)
{
   const struct intel_device_info *devinfo = p->devinfo;

   if (reg.file == BRW_GENERAL_REGISTER_FILE)
      assert(reg.nr < XE2_MAX_GRF);

   if (brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDS ||
       brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDSC ||
       (devinfo->ver >= 12 &&
        (brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SEND ||
         brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDC))) {
      assert(reg.file == BRW_GENERAL_REGISTER_FILE ||
             reg.file == BRW_ARCHITECTURE_REGISTER_FILE);
      assert(reg.address_mode == BRW_ADDRESS_DIRECT);
      assert(reg.subnr == 0);
      assert(has_scalar_region(reg) ||
             (reg.hstride == BRW_HORIZONTAL_STRIDE_1 &&
              reg.vstride == reg.width + 1));
      assert(!reg.negate && !reg.abs);
      brw_inst_set_send_src1_reg_nr(devinfo, inst, phys_nr(devinfo, reg));
      brw_inst_set_send_src1_reg_file(devinfo, inst, reg.file);
   } else {
      /* From the IVB PRM Vol. 4, Pt. 3, Section 3.3.3.5:
       *
       *    "Accumulator registers may be accessed explicitly as src0
       *    operands only."
       */
      assert(reg.file != BRW_ARCHITECTURE_REGISTER_FILE ||
             (reg.nr & 0xF0) != BRW_ARF_ACCUMULATOR);

      brw_inst_set_src1_file_type(devinfo, inst, reg.file, reg.type);
      brw_inst_set_src1_abs(devinfo, inst, reg.abs);
      brw_inst_set_src1_negate(devinfo, inst, reg.negate);

      /* Only src1 can be immediate in two-argument instructions.
       */
      assert(brw_inst_src0_reg_file(devinfo, inst) != BRW_IMMEDIATE_VALUE);

      if (reg.file == BRW_IMMEDIATE_VALUE) {
         /* two-argument instructions can only use 32-bit immediates */
         assert(type_sz(reg.type) < 8);
         brw_inst_set_imm_ud(devinfo, inst, reg.ud);
      } else {
         /* This is a hardware restriction, which may or may not be lifted
          * in the future:
          */
         assert (reg.address_mode == BRW_ADDRESS_DIRECT);
         /* assert (reg.file == BRW_GENERAL_REGISTER_FILE); */

         brw_inst_set_src1_da_reg_nr(devinfo, inst, phys_nr(devinfo, reg));
         if (brw_inst_access_mode(devinfo, inst) == BRW_ALIGN_1) {
            brw_inst_set_src1_da1_subreg_nr(devinfo, inst, phys_subnr(devinfo, reg));
         } else {
            brw_inst_set_src1_da16_subreg_nr(devinfo, inst, reg.subnr / 16);
         }

         if (brw_inst_access_mode(devinfo, inst) == BRW_ALIGN_1) {
            if (reg.width == BRW_WIDTH_1 &&
                brw_inst_exec_size(devinfo, inst) == BRW_EXECUTE_1) {
               brw_inst_set_src1_hstride(devinfo, inst, BRW_HORIZONTAL_STRIDE_0);
               brw_inst_set_src1_width(devinfo, inst, BRW_WIDTH_1);
               brw_inst_set_src1_vstride(devinfo, inst, BRW_VERTICAL_STRIDE_0);
            } else {
               brw_inst_set_src1_hstride(devinfo, inst, reg.hstride);
               brw_inst_set_src1_width(devinfo, inst, reg.width);
               brw_inst_set_src1_vstride(devinfo, inst, reg.vstride);
            }
         } else {
            brw_inst_set_src1_da16_swiz_x(devinfo, inst,
               BRW_GET_SWZ(reg.swizzle, BRW_CHANNEL_X));
            brw_inst_set_src1_da16_swiz_y(devinfo, inst,
               BRW_GET_SWZ(reg.swizzle, BRW_CHANNEL_Y));
            brw_inst_set_src1_da16_swiz_z(devinfo, inst,
               BRW_GET_SWZ(reg.swizzle, BRW_CHANNEL_Z));
            brw_inst_set_src1_da16_swiz_w(devinfo, inst,
               BRW_GET_SWZ(reg.swizzle, BRW_CHANNEL_W));

            if (reg.vstride == BRW_VERTICAL_STRIDE_8) {
               /* This is an oddity of the fact we're using the same
                * descriptions for registers in align_16 as align_1:
                */
               brw_inst_set_src1_vstride(devinfo, inst, BRW_VERTICAL_STRIDE_4);
            } else {
               brw_inst_set_src1_vstride(devinfo, inst, reg.vstride);
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
brw_set_desc_ex(struct brw_codegen *p, brw_inst *inst,
                unsigned desc, unsigned ex_desc)
{
   const struct intel_device_info *devinfo = p->devinfo;
   assert(brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SEND ||
          brw_inst_opcode(p->isa, inst) == BRW_OPCODE_SENDC);
   if (devinfo->ver < 12)
      brw_inst_set_src1_file_type(devinfo, inst,
                                  BRW_IMMEDIATE_VALUE, BRW_REGISTER_TYPE_UD);
   brw_inst_set_send_desc(devinfo, inst, desc);
   if (devinfo->ver >= 9)
      brw_inst_set_send_ex_desc(devinfo, inst, ex_desc);
}

static void
brw_inst_set_state(const struct brw_isa_info *isa,
                   brw_inst *insn,
                   const struct brw_insn_state *state)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   brw_inst_set_exec_size(devinfo, insn, state->exec_size);
   brw_inst_set_group(devinfo, insn, state->group);
   brw_inst_set_access_mode(devinfo, insn, state->access_mode);
   brw_inst_set_mask_control(devinfo, insn, state->mask_control);
   if (devinfo->ver >= 12)
      brw_inst_set_swsb(devinfo, insn, tgl_swsb_encode(devinfo, state->swsb));
   brw_inst_set_saturate(devinfo, insn, state->saturate);
   brw_inst_set_pred_control(devinfo, insn, state->predicate);
   brw_inst_set_pred_inv(devinfo, insn, state->pred_inv);

   if (is_3src(isa, brw_inst_opcode(isa, insn)) &&
       state->access_mode == BRW_ALIGN_16) {
      brw_inst_set_3src_a16_flag_subreg_nr(devinfo, insn, state->flag_subreg % 2);
      brw_inst_set_3src_a16_flag_reg_nr(devinfo, insn, state->flag_subreg / 2);
   } else {
      brw_inst_set_flag_subreg_nr(devinfo, insn, state->flag_subreg % 2);
      brw_inst_set_flag_reg_nr(devinfo, insn, state->flag_subreg / 2);
   }

   if (devinfo->ver < 20)
      brw_inst_set_acc_wr_control(devinfo, insn, state->acc_wr_control);
}

static brw_inst *
brw_append_insns(struct brw_codegen *p, unsigned nr_insn, unsigned alignment)
{
   assert(util_is_power_of_two_or_zero(sizeof(brw_inst)));
   assert(util_is_power_of_two_or_zero(alignment));
   const unsigned align_insn = MAX2(alignment / sizeof(brw_inst), 1);
   const unsigned start_insn = ALIGN(p->nr_insn, align_insn);
   const unsigned new_nr_insn = start_insn + nr_insn;

   if (p->store_size < new_nr_insn) {
      p->store_size = util_next_power_of_two(new_nr_insn * sizeof(brw_inst));
      p->store = reralloc(p->mem_ctx, p->store, brw_inst, p->store_size);
   }

   /* Memset any padding due to alignment to 0.  We don't want to be hashing
    * or caching a bunch of random bits we got from a memory allocation.
    */
   if (p->nr_insn < start_insn) {
      memset(&p->store[p->nr_insn], 0,
             (start_insn - p->nr_insn) * sizeof(brw_inst));
   }

   assert(p->next_insn_offset == p->nr_insn * sizeof(brw_inst));
   p->nr_insn = new_nr_insn;
   p->next_insn_offset = new_nr_insn * sizeof(brw_inst);

   return &p->store[start_insn];
}

void
brw_realign(struct brw_codegen *p, unsigned alignment)
{
   brw_append_insns(p, 0, alignment);
}

int
brw_append_data(struct brw_codegen *p, void *data,
                unsigned size, unsigned alignment)
{
   unsigned nr_insn = DIV_ROUND_UP(size, sizeof(brw_inst));
   void *dst = brw_append_insns(p, nr_insn, alignment);
   memcpy(dst, data, size);

   /* If it's not a whole number of instructions, memset the end */
   if (size < nr_insn * sizeof(brw_inst))
      memset(dst + size, 0, nr_insn * sizeof(brw_inst) - size);

   return dst - (void *)p->store;
}

#define next_insn brw_next_insn
brw_inst *
brw_next_insn(struct brw_codegen *p, unsigned opcode)
{
   brw_inst *insn = brw_append_insns(p, 1, sizeof(brw_inst));

   memset(insn, 0, sizeof(*insn));
   brw_inst_set_opcode(p->isa, insn, opcode);

   /* Apply the default instruction state */
   brw_inst_set_state(p->isa, insn, p->current);

   return insn;
}

void
brw_add_reloc(struct brw_codegen *p, uint32_t id,
              enum brw_shader_reloc_type type,
              uint32_t offset, uint32_t delta)
{
   if (p->num_relocs + 1 > p->reloc_array_size) {
      p->reloc_array_size = MAX2(16, p->reloc_array_size * 2);
      p->relocs = reralloc(p->mem_ctx, p->relocs,
                           struct brw_shader_reloc, p->reloc_array_size);
   }

   p->relocs[p->num_relocs++] = (struct brw_shader_reloc) {
      .id = id,
      .type = type,
      .offset = offset,
      .delta = delta,
   };
}

static brw_inst *
brw_alu1(struct brw_codegen *p, unsigned opcode,
         struct brw_reg dest, struct brw_reg src)
{
   brw_inst *insn = next_insn(p, opcode);
   brw_set_dest(p, insn, dest);
   brw_set_src0(p, insn, src);
   return insn;
}

static brw_inst *
brw_alu2(struct brw_codegen *p, unsigned opcode,
         struct brw_reg dest, struct brw_reg src0, struct brw_reg src1)
{
   /* 64-bit immediates are only supported on 1-src instructions */
   assert(src0.file != BRW_IMMEDIATE_VALUE || type_sz(src0.type) <= 4);
   assert(src1.file != BRW_IMMEDIATE_VALUE || type_sz(src1.type) <= 4);

   brw_inst *insn = next_insn(p, opcode);
   brw_set_dest(p, insn, dest);
   brw_set_src0(p, insn, src0);
   brw_set_src1(p, insn, src1);
   return insn;
}

static int
get_3src_subreg_nr(struct brw_reg reg)
{
   /* Normally, SubRegNum is in bytes (0..31).  However, 3-src instructions
    * use 32-bit units (components 0..7).  Since they only support F/D/UD
    * types, this doesn't lose any flexibility, but uses fewer bits.
    */
   return reg.subnr / 4;
}

static enum gfx10_align1_3src_vertical_stride
to_3src_align1_vstride(const struct intel_device_info *devinfo,
                       enum brw_vertical_stride vstride)
{
   switch (vstride) {
   case BRW_VERTICAL_STRIDE_0:
      return BRW_ALIGN1_3SRC_VERTICAL_STRIDE_0;
   case BRW_VERTICAL_STRIDE_1:
      assert(devinfo->ver >= 12);
      return BRW_ALIGN1_3SRC_VERTICAL_STRIDE_1;
   case BRW_VERTICAL_STRIDE_2:
      assert(devinfo->ver < 12);
      return BRW_ALIGN1_3SRC_VERTICAL_STRIDE_2;
   case BRW_VERTICAL_STRIDE_4:
      return BRW_ALIGN1_3SRC_VERTICAL_STRIDE_4;
   case BRW_VERTICAL_STRIDE_8:
   case BRW_VERTICAL_STRIDE_16:
      return BRW_ALIGN1_3SRC_VERTICAL_STRIDE_8;
   default:
      unreachable("invalid vstride");
   }
}


static enum gfx10_align1_3src_src_horizontal_stride
to_3src_align1_hstride(enum brw_horizontal_stride hstride)
{
   switch (hstride) {
   case BRW_HORIZONTAL_STRIDE_0:
      return BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_0;
   case BRW_HORIZONTAL_STRIDE_1:
      return BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_1;
   case BRW_HORIZONTAL_STRIDE_2:
      return BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_2;
   case BRW_HORIZONTAL_STRIDE_4:
      return BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_4;
   default:
      unreachable("invalid hstride");
   }
}

static brw_inst *
brw_alu3(struct brw_codegen *p, unsigned opcode, struct brw_reg dest,
         struct brw_reg src0, struct brw_reg src1, struct brw_reg src2)
{
   const struct intel_device_info *devinfo = p->devinfo;
   brw_inst *inst = next_insn(p, opcode);

   assert(dest.nr < XE2_MAX_GRF);

   if (devinfo->ver >= 10)
      assert(!(src0.file == BRW_IMMEDIATE_VALUE &&
               src2.file == BRW_IMMEDIATE_VALUE));

   assert(src0.file == BRW_IMMEDIATE_VALUE || src0.nr < XE2_MAX_GRF);
   assert(src1.file != BRW_IMMEDIATE_VALUE && src1.nr < XE2_MAX_GRF);
   assert(src2.file == BRW_IMMEDIATE_VALUE || src2.nr < XE2_MAX_GRF);
   assert(dest.address_mode == BRW_ADDRESS_DIRECT);
   assert(src0.address_mode == BRW_ADDRESS_DIRECT);
   assert(src1.address_mode == BRW_ADDRESS_DIRECT);
   assert(src2.address_mode == BRW_ADDRESS_DIRECT);

   if (brw_inst_access_mode(devinfo, inst) == BRW_ALIGN_1) {
      assert(dest.file == BRW_GENERAL_REGISTER_FILE ||
             (dest.file == BRW_ARCHITECTURE_REGISTER_FILE &&
              (dest.nr & 0xF0) == BRW_ARF_ACCUMULATOR));

      STATIC_ASSERT((BRW_ARCHITECTURE_REGISTER_FILE ^ 1) == BRW_ALIGN1_3SRC_ACCUMULATOR);
      STATIC_ASSERT((BRW_GENERAL_REGISTER_FILE ^ 1) == BRW_ALIGN1_3SRC_GENERAL_REGISTER_FILE);

      /* Gfx10 and Gfx11 bit encoding for the register file is the inversion of
       * the actual register file (see the STATIC_ASSERTs above).
       */
      unsigned dst_reg_file = devinfo->ver >= 12 ? dest.file : dest.file ^ 1;

      brw_inst_set_3src_a1_dst_reg_file(devinfo, inst, dst_reg_file);
      brw_inst_set_3src_dst_reg_nr(devinfo, inst, phys_nr(devinfo, dest));
      brw_inst_set_3src_a1_dst_subreg_nr(devinfo, inst, phys_subnr(devinfo, dest) / 8);
      brw_inst_set_3src_a1_dst_hstride(devinfo, inst, BRW_ALIGN1_3SRC_DST_HORIZONTAL_STRIDE_1);

      if (brw_reg_type_is_floating_point(dest.type)) {
         brw_inst_set_3src_a1_exec_type(devinfo, inst,
                                        BRW_ALIGN1_3SRC_EXEC_TYPE_FLOAT);
      } else {
         brw_inst_set_3src_a1_exec_type(devinfo, inst,
                                        BRW_ALIGN1_3SRC_EXEC_TYPE_INT);
      }

      brw_inst_set_3src_a1_dst_type(devinfo, inst, dest.type);
      brw_inst_set_3src_a1_src0_type(devinfo, inst, src0.type);
      brw_inst_set_3src_a1_src1_type(devinfo, inst, src1.type);
      brw_inst_set_3src_a1_src2_type(devinfo, inst, src2.type);

      if (src0.file == BRW_IMMEDIATE_VALUE) {
         brw_inst_set_3src_a1_src0_imm(devinfo, inst, src0.ud);
      } else {
         brw_inst_set_3src_a1_src0_vstride(
            devinfo, inst, to_3src_align1_vstride(devinfo, src0.vstride));
         brw_inst_set_3src_a1_src0_hstride(devinfo, inst,
                                           to_3src_align1_hstride(src0.hstride));
         brw_inst_set_3src_a1_src0_subreg_nr(devinfo, inst, phys_subnr(devinfo, src0));
         if (src0.type == BRW_REGISTER_TYPE_NF) {
            brw_inst_set_3src_src0_reg_nr(devinfo, inst, BRW_ARF_ACCUMULATOR);
         } else {
            brw_inst_set_3src_src0_reg_nr(devinfo, inst, phys_nr(devinfo, src0));
         }
         brw_inst_set_3src_src0_abs(devinfo, inst, src0.abs);
         brw_inst_set_3src_src0_negate(devinfo, inst, src0.negate);
      }
      brw_inst_set_3src_a1_src1_vstride(
         devinfo, inst, to_3src_align1_vstride(devinfo, src1.vstride));
      brw_inst_set_3src_a1_src1_hstride(devinfo, inst,
                                        to_3src_align1_hstride(src1.hstride));

      brw_inst_set_3src_a1_src1_subreg_nr(devinfo, inst, phys_subnr(devinfo, src1));
      if (src1.file == BRW_ARCHITECTURE_REGISTER_FILE) {
         brw_inst_set_3src_src1_reg_nr(devinfo, inst, BRW_ARF_ACCUMULATOR);
      } else {
         brw_inst_set_3src_src1_reg_nr(devinfo, inst, phys_nr(devinfo, src1));
      }
      brw_inst_set_3src_src1_abs(devinfo, inst, src1.abs);
      brw_inst_set_3src_src1_negate(devinfo, inst, src1.negate);

      if (src2.file == BRW_IMMEDIATE_VALUE) {
         brw_inst_set_3src_a1_src2_imm(devinfo, inst, src2.ud);
      } else {
         brw_inst_set_3src_a1_src2_hstride(devinfo, inst,
                                           to_3src_align1_hstride(src2.hstride));
         /* no vstride on src2 */
         brw_inst_set_3src_a1_src2_subreg_nr(devinfo, inst, phys_subnr(devinfo, src2));
         brw_inst_set_3src_src2_reg_nr(devinfo, inst, phys_nr(devinfo, src2));
         brw_inst_set_3src_src2_abs(devinfo, inst, src2.abs);
         brw_inst_set_3src_src2_negate(devinfo, inst, src2.negate);
      }

      assert(src0.file == BRW_GENERAL_REGISTER_FILE ||
             src0.file == BRW_IMMEDIATE_VALUE ||
             (src0.file == BRW_ARCHITECTURE_REGISTER_FILE &&
              src0.type == BRW_REGISTER_TYPE_NF));
      assert(src1.file == BRW_GENERAL_REGISTER_FILE ||
             (src1.file == BRW_ARCHITECTURE_REGISTER_FILE &&
              src1.nr == BRW_ARF_ACCUMULATOR));
      assert(src2.file == BRW_GENERAL_REGISTER_FILE ||
             src2.file == BRW_IMMEDIATE_VALUE);

      if (devinfo->ver >= 12) {
         if (src0.file == BRW_IMMEDIATE_VALUE) {
            brw_inst_set_3src_a1_src0_is_imm(devinfo, inst, 1);
         } else {
            brw_inst_set_3src_a1_src0_reg_file(devinfo, inst, src0.file);
         }

         brw_inst_set_3src_a1_src1_reg_file(devinfo, inst, src1.file);

         if (src2.file == BRW_IMMEDIATE_VALUE) {
            brw_inst_set_3src_a1_src2_is_imm(devinfo, inst, 1);
         } else {
            brw_inst_set_3src_a1_src2_reg_file(devinfo, inst, src2.file);
         }
      } else {
         brw_inst_set_3src_a1_src0_reg_file(devinfo, inst,
                                            src0.file == BRW_GENERAL_REGISTER_FILE ?
                                            BRW_ALIGN1_3SRC_GENERAL_REGISTER_FILE :
                                            BRW_ALIGN1_3SRC_IMMEDIATE_VALUE);
         brw_inst_set_3src_a1_src1_reg_file(devinfo, inst,
                                            src1.file == BRW_GENERAL_REGISTER_FILE ?
                                            BRW_ALIGN1_3SRC_GENERAL_REGISTER_FILE :
                                            BRW_ALIGN1_3SRC_ACCUMULATOR);
         brw_inst_set_3src_a1_src2_reg_file(devinfo, inst,
                                            src2.file == BRW_GENERAL_REGISTER_FILE ?
                                            BRW_ALIGN1_3SRC_GENERAL_REGISTER_FILE :
                                            BRW_ALIGN1_3SRC_IMMEDIATE_VALUE);
      }

   } else {
      assert(dest.file == BRW_GENERAL_REGISTER_FILE);
      assert(dest.type == BRW_REGISTER_TYPE_F  ||
             dest.type == BRW_REGISTER_TYPE_DF ||
             dest.type == BRW_REGISTER_TYPE_D  ||
             dest.type == BRW_REGISTER_TYPE_UD ||
             dest.type == BRW_REGISTER_TYPE_HF);
      brw_inst_set_3src_dst_reg_nr(devinfo, inst, dest.nr);
      brw_inst_set_3src_a16_dst_subreg_nr(devinfo, inst, dest.subnr / 4);
      brw_inst_set_3src_a16_dst_writemask(devinfo, inst, dest.writemask);

      assert(src0.file == BRW_GENERAL_REGISTER_FILE);
      brw_inst_set_3src_a16_src0_swizzle(devinfo, inst, src0.swizzle);
      brw_inst_set_3src_a16_src0_subreg_nr(devinfo, inst, get_3src_subreg_nr(src0));
      brw_inst_set_3src_src0_reg_nr(devinfo, inst, src0.nr);
      brw_inst_set_3src_src0_abs(devinfo, inst, src0.abs);
      brw_inst_set_3src_src0_negate(devinfo, inst, src0.negate);
      brw_inst_set_3src_a16_src0_rep_ctrl(devinfo, inst,
                                          src0.vstride == BRW_VERTICAL_STRIDE_0);

      assert(src1.file == BRW_GENERAL_REGISTER_FILE);
      brw_inst_set_3src_a16_src1_swizzle(devinfo, inst, src1.swizzle);
      brw_inst_set_3src_a16_src1_subreg_nr(devinfo, inst, get_3src_subreg_nr(src1));
      brw_inst_set_3src_src1_reg_nr(devinfo, inst, src1.nr);
      brw_inst_set_3src_src1_abs(devinfo, inst, src1.abs);
      brw_inst_set_3src_src1_negate(devinfo, inst, src1.negate);
      brw_inst_set_3src_a16_src1_rep_ctrl(devinfo, inst,
                                          src1.vstride == BRW_VERTICAL_STRIDE_0);

      assert(src2.file == BRW_GENERAL_REGISTER_FILE);
      brw_inst_set_3src_a16_src2_swizzle(devinfo, inst, src2.swizzle);
      brw_inst_set_3src_a16_src2_subreg_nr(devinfo, inst, get_3src_subreg_nr(src2));
      brw_inst_set_3src_src2_reg_nr(devinfo, inst, src2.nr);
      brw_inst_set_3src_src2_abs(devinfo, inst, src2.abs);
      brw_inst_set_3src_src2_negate(devinfo, inst, src2.negate);
      brw_inst_set_3src_a16_src2_rep_ctrl(devinfo, inst,
                                          src2.vstride == BRW_VERTICAL_STRIDE_0);

      /* Set both the source and destination types based on dest.type,
       * ignoring the source register types.  The MAD and LRP emitters ensure
       * that all four types are float.  The BFE and BFI2 emitters, however,
       * may send us mixed D and UD types and want us to ignore that and use
       * the destination type.
       */
      brw_inst_set_3src_a16_src_type(devinfo, inst, dest.type);
      brw_inst_set_3src_a16_dst_type(devinfo, inst, dest.type);

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
      if (src1.type == BRW_REGISTER_TYPE_HF)
         brw_inst_set_3src_a16_src1_type(devinfo, inst, 1);

      if (src2.type == BRW_REGISTER_TYPE_HF)
         brw_inst_set_3src_a16_src2_type(devinfo, inst, 1);
   }

   return inst;
}

static brw_inst *
brw_dpas_three_src(struct brw_codegen *p, enum opcode opcode,
                   enum gfx12_systolic_depth sdepth, unsigned rcount, struct brw_reg dest,
                   struct brw_reg src0, struct brw_reg src1, struct brw_reg src2)
{
   const struct intel_device_info *devinfo = p->devinfo;
   brw_inst *inst = next_insn(p, opcode);

   assert(dest.file == BRW_GENERAL_REGISTER_FILE);
   brw_inst_set_dpas_3src_dst_reg_file(devinfo, inst,
                                       BRW_GENERAL_REGISTER_FILE);
   brw_inst_set_dpas_3src_dst_reg_nr(devinfo, inst, phys_nr(devinfo, dest));
   brw_inst_set_dpas_3src_dst_subreg_nr(devinfo, inst, phys_subnr(devinfo, dest));

   if (brw_reg_type_is_floating_point(dest.type)) {
      brw_inst_set_dpas_3src_exec_type(devinfo, inst,
                                       BRW_ALIGN1_3SRC_EXEC_TYPE_FLOAT);
   } else {
      brw_inst_set_dpas_3src_exec_type(devinfo, inst,
                                       BRW_ALIGN1_3SRC_EXEC_TYPE_INT);
   }

   brw_inst_set_dpas_3src_sdepth(devinfo, inst, sdepth);
   brw_inst_set_dpas_3src_rcount(devinfo, inst, rcount - 1);

   brw_inst_set_dpas_3src_dst_type(devinfo, inst, dest.type);
   brw_inst_set_dpas_3src_src0_type(devinfo, inst, src0.type);
   brw_inst_set_dpas_3src_src1_type(devinfo, inst, src1.type);
   brw_inst_set_dpas_3src_src2_type(devinfo, inst, src2.type);

   assert(src0.file == BRW_GENERAL_REGISTER_FILE ||
          (src0.file == BRW_ARCHITECTURE_REGISTER_FILE &&
           src0.nr == BRW_ARF_NULL));

   brw_inst_set_dpas_3src_src0_reg_file(devinfo, inst, src0.file);
   brw_inst_set_dpas_3src_src0_reg_nr(devinfo, inst, phys_nr(devinfo, src0));
   brw_inst_set_dpas_3src_src0_subreg_nr(devinfo, inst, phys_subnr(devinfo, src0));

   assert(src1.file == BRW_GENERAL_REGISTER_FILE);

   brw_inst_set_dpas_3src_src1_reg_file(devinfo, inst, src1.file);
   brw_inst_set_dpas_3src_src1_reg_nr(devinfo, inst, phys_nr(devinfo, src1));
   brw_inst_set_dpas_3src_src1_subreg_nr(devinfo, inst, phys_subnr(devinfo, src1));
   brw_inst_set_dpas_3src_src1_subbyte(devinfo, inst, BRW_SUB_BYTE_PRECISION_NONE);

   assert(src2.file == BRW_GENERAL_REGISTER_FILE);

   brw_inst_set_dpas_3src_src2_reg_file(devinfo, inst, src2.file);
   brw_inst_set_dpas_3src_src2_reg_nr(devinfo, inst, phys_nr(devinfo, src2));
   brw_inst_set_dpas_3src_src2_subreg_nr(devinfo, inst, phys_subnr(devinfo, src2));
   brw_inst_set_dpas_3src_src2_subbyte(devinfo, inst, BRW_SUB_BYTE_PRECISION_NONE);

   return inst;
}

/***********************************************************************
 * Convenience routines.
 */
#define ALU1(OP)					\
brw_inst *brw_##OP(struct brw_codegen *p,		\
	      struct brw_reg dest,			\
	      struct brw_reg src0)   			\
{							\
   return brw_alu1(p, BRW_OPCODE_##OP, dest, src0);    	\
}

#define ALU2(OP)					\
brw_inst *brw_##OP(struct brw_codegen *p,		\
	      struct brw_reg dest,			\
	      struct brw_reg src0,			\
	      struct brw_reg src1)   			\
{							\
   return brw_alu2(p, BRW_OPCODE_##OP, dest, src0, src1);	\
}

#define ALU3(OP)					\
brw_inst *brw_##OP(struct brw_codegen *p,		\
	      struct brw_reg dest,			\
	      struct brw_reg src0,			\
	      struct brw_reg src1,			\
	      struct brw_reg src2)   			\
{                                                       \
   if (p->current->access_mode == BRW_ALIGN_16) {       \
      if (src0.vstride == BRW_VERTICAL_STRIDE_0)        \
         src0.swizzle = BRW_SWIZZLE_XXXX;               \
      if (src1.vstride == BRW_VERTICAL_STRIDE_0)        \
         src1.swizzle = BRW_SWIZZLE_XXXX;               \
      if (src2.vstride == BRW_VERTICAL_STRIDE_0)        \
         src2.swizzle = BRW_SWIZZLE_XXXX;               \
   }                                                    \
   return brw_alu3(p, BRW_OPCODE_##OP, dest, src0, src1, src2);	\
}

#define ALU3F(OP)                                               \
brw_inst *brw_##OP(struct brw_codegen *p,         \
                                 struct brw_reg dest,           \
                                 struct brw_reg src0,           \
                                 struct brw_reg src1,           \
                                 struct brw_reg src2)           \
{                                                               \
   assert(dest.type == BRW_REGISTER_TYPE_F ||                   \
          dest.type == BRW_REGISTER_TYPE_DF);                   \
   if (dest.type == BRW_REGISTER_TYPE_F) {                      \
      assert(src0.type == BRW_REGISTER_TYPE_F);                 \
      assert(src1.type == BRW_REGISTER_TYPE_F);                 \
      assert(src2.type == BRW_REGISTER_TYPE_F);                 \
   } else if (dest.type == BRW_REGISTER_TYPE_DF) {              \
      assert(src0.type == BRW_REGISTER_TYPE_DF);                \
      assert(src1.type == BRW_REGISTER_TYPE_DF);                \
      assert(src2.type == BRW_REGISTER_TYPE_DF);                \
   }                                                            \
                                                                \
   if (p->current->access_mode == BRW_ALIGN_16) {               \
      if (src0.vstride == BRW_VERTICAL_STRIDE_0)                \
         src0.swizzle = BRW_SWIZZLE_XXXX;                       \
      if (src1.vstride == BRW_VERTICAL_STRIDE_0)                \
         src1.swizzle = BRW_SWIZZLE_XXXX;                       \
      if (src2.vstride == BRW_VERTICAL_STRIDE_0)                \
         src2.swizzle = BRW_SWIZZLE_XXXX;                       \
   }                                                            \
   return brw_alu3(p, BRW_OPCODE_##OP, dest, src0, src1, src2); \
}

ALU2(SEL)
ALU1(NOT)
ALU2(AND)
ALU2(OR)
ALU2(XOR)
ALU2(SHR)
ALU2(SHL)
ALU2(ASR)
ALU2(ROL)
ALU2(ROR)
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
ALU3(DP4A)
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
ALU3(ADD3)
ALU1(MOV)

brw_inst *
brw_ADD(struct brw_codegen *p, struct brw_reg dest,
        struct brw_reg src0, struct brw_reg src1)
{
   /* 6.2.2: add */
   if (src0.type == BRW_REGISTER_TYPE_F ||
       (src0.file == BRW_IMMEDIATE_VALUE &&
	src0.type == BRW_REGISTER_TYPE_VF)) {
      assert(src1.type != BRW_REGISTER_TYPE_UD);
      assert(src1.type != BRW_REGISTER_TYPE_D);
   }

   if (src1.type == BRW_REGISTER_TYPE_F ||
       (src1.file == BRW_IMMEDIATE_VALUE &&
	src1.type == BRW_REGISTER_TYPE_VF)) {
      assert(src0.type != BRW_REGISTER_TYPE_UD);
      assert(src0.type != BRW_REGISTER_TYPE_D);
   }

   return brw_alu2(p, BRW_OPCODE_ADD, dest, src0, src1);
}

brw_inst *
brw_AVG(struct brw_codegen *p, struct brw_reg dest,
        struct brw_reg src0, struct brw_reg src1)
{
   assert(dest.type == src0.type);
   assert(src0.type == src1.type);
   switch (src0.type) {
   case BRW_REGISTER_TYPE_B:
   case BRW_REGISTER_TYPE_UB:
   case BRW_REGISTER_TYPE_W:
   case BRW_REGISTER_TYPE_UW:
   case BRW_REGISTER_TYPE_D:
   case BRW_REGISTER_TYPE_UD:
      break;
   default:
      unreachable("Bad type for brw_AVG");
   }

   return brw_alu2(p, BRW_OPCODE_AVG, dest, src0, src1);
}

brw_inst *
brw_MUL(struct brw_codegen *p, struct brw_reg dest,
        struct brw_reg src0, struct brw_reg src1)
{
   /* 6.32.38: mul */
   if (src0.type == BRW_REGISTER_TYPE_D ||
       src0.type == BRW_REGISTER_TYPE_UD ||
       src1.type == BRW_REGISTER_TYPE_D ||
       src1.type == BRW_REGISTER_TYPE_UD) {
      assert(dest.type != BRW_REGISTER_TYPE_F);
   }

   if (src0.type == BRW_REGISTER_TYPE_F ||
       (src0.file == BRW_IMMEDIATE_VALUE &&
	src0.type == BRW_REGISTER_TYPE_VF)) {
      assert(src1.type != BRW_REGISTER_TYPE_UD);
      assert(src1.type != BRW_REGISTER_TYPE_D);
   }

   if (src1.type == BRW_REGISTER_TYPE_F ||
       (src1.file == BRW_IMMEDIATE_VALUE &&
	src1.type == BRW_REGISTER_TYPE_VF)) {
      assert(src0.type != BRW_REGISTER_TYPE_UD);
      assert(src0.type != BRW_REGISTER_TYPE_D);
   }

   assert(src0.file != BRW_ARCHITECTURE_REGISTER_FILE ||
	  src0.nr != BRW_ARF_ACCUMULATOR);
   assert(src1.file != BRW_ARCHITECTURE_REGISTER_FILE ||
	  src1.nr != BRW_ARF_ACCUMULATOR);

   return brw_alu2(p, BRW_OPCODE_MUL, dest, src0, src1);
}

brw_inst *
brw_LINE(struct brw_codegen *p, struct brw_reg dest,
         struct brw_reg src0, struct brw_reg src1)
{
   src0.vstride = BRW_VERTICAL_STRIDE_0;
   src0.width = BRW_WIDTH_1;
   src0.hstride = BRW_HORIZONTAL_STRIDE_0;
   return brw_alu2(p, BRW_OPCODE_LINE, dest, src0, src1);
}

brw_inst *
brw_PLN(struct brw_codegen *p, struct brw_reg dest,
        struct brw_reg src0, struct brw_reg src1)
{
   src0.vstride = BRW_VERTICAL_STRIDE_0;
   src0.width = BRW_WIDTH_1;
   src0.hstride = BRW_HORIZONTAL_STRIDE_0;
   src1.vstride = BRW_VERTICAL_STRIDE_8;
   src1.width = BRW_WIDTH_8;
   src1.hstride = BRW_HORIZONTAL_STRIDE_1;
   return brw_alu2(p, BRW_OPCODE_PLN, dest, src0, src1);
}

brw_inst *
brw_DPAS(struct brw_codegen *p, enum gfx12_systolic_depth sdepth,
         unsigned rcount, struct brw_reg dest, struct brw_reg src0,
         struct brw_reg src1, struct brw_reg src2)
{
   return brw_dpas_three_src(p, BRW_OPCODE_DPAS, sdepth, rcount, dest, src0,
                             src1, src2);
}

void brw_NOP(struct brw_codegen *p)
{
   brw_inst *insn = next_insn(p, BRW_OPCODE_NOP);
   memset(insn, 0, sizeof(*insn));
   brw_inst_set_opcode(p->isa, insn, BRW_OPCODE_NOP);
}

void brw_SYNC(struct brw_codegen *p, enum tgl_sync_function func)
{
   brw_inst *insn = next_insn(p, BRW_OPCODE_SYNC);
   brw_inst_set_cond_modifier(p->devinfo, insn, func);
}

/***********************************************************************
 * Comparisons, if/else/endif
 */

brw_inst *
brw_JMPI(struct brw_codegen *p, struct brw_reg index,
         unsigned predicate_control)
{
   const struct intel_device_info *devinfo = p->devinfo;
   struct brw_reg ip = brw_ip_reg();
   brw_inst *inst = brw_alu2(p, BRW_OPCODE_JMPI, ip, ip, index);

   brw_inst_set_exec_size(devinfo, inst, BRW_EXECUTE_1);
   brw_inst_set_qtr_control(devinfo, inst, BRW_COMPRESSION_NONE);
   brw_inst_set_mask_control(devinfo, inst, BRW_MASK_DISABLE);
   brw_inst_set_pred_control(devinfo, inst, predicate_control);

   return inst;
}

static void
push_if_stack(struct brw_codegen *p, brw_inst *inst)
{
   p->if_stack[p->if_stack_depth] = inst - p->store;

   p->if_stack_depth++;
   if (p->if_stack_array_size <= p->if_stack_depth) {
      p->if_stack_array_size *= 2;
      p->if_stack = reralloc(p->mem_ctx, p->if_stack, int,
			     p->if_stack_array_size);
   }
}

static brw_inst *
pop_if_stack(struct brw_codegen *p)
{
   p->if_stack_depth--;
   return &p->store[p->if_stack[p->if_stack_depth]];
}

static void
push_loop_stack(struct brw_codegen *p, brw_inst *inst)
{
   if (p->loop_stack_array_size <= (p->loop_stack_depth + 1)) {
      p->loop_stack_array_size *= 2;
      p->loop_stack = reralloc(p->mem_ctx, p->loop_stack, int,
			       p->loop_stack_array_size);
   }

   p->loop_stack[p->loop_stack_depth] = inst - p->store;
   p->loop_stack_depth++;
}

static brw_inst *
get_inner_do_insn(struct brw_codegen *p)
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
brw_inst *
brw_IF(struct brw_codegen *p, unsigned execute_size)
{
   const struct intel_device_info *devinfo = p->devinfo;
   brw_inst *insn;

   insn = next_insn(p, BRW_OPCODE_IF);

   /* Override the defaults for this instruction:
    */
   brw_set_dest(p, insn, vec1(retype(brw_null_reg(), BRW_REGISTER_TYPE_D)));
   if (devinfo->ver < 12)
      brw_set_src0(p, insn, brw_imm_d(0));
   brw_inst_set_jip(devinfo, insn, 0);
   brw_inst_set_uip(devinfo, insn, 0);

   brw_inst_set_exec_size(devinfo, insn, execute_size);
   brw_inst_set_qtr_control(devinfo, insn, BRW_COMPRESSION_NONE);
   brw_inst_set_pred_control(devinfo, insn, BRW_PREDICATE_NORMAL);
   brw_inst_set_mask_control(devinfo, insn, BRW_MASK_ENABLE);

   push_if_stack(p, insn);
   return insn;
}

/**
 * Patch IF and ELSE instructions with appropriate jump targets.
 */
static void
patch_IF_ELSE(struct brw_codegen *p,
              brw_inst *if_inst, brw_inst *else_inst, brw_inst *endif_inst)
{
   const struct intel_device_info *devinfo = p->devinfo;

   assert(if_inst != NULL && brw_inst_opcode(p->isa, if_inst) == BRW_OPCODE_IF);
   assert(endif_inst != NULL);
   assert(else_inst == NULL || brw_inst_opcode(p->isa, else_inst) == BRW_OPCODE_ELSE);

   unsigned br = brw_jump_scale(devinfo);

   assert(brw_inst_opcode(p->isa, endif_inst) == BRW_OPCODE_ENDIF);
   brw_inst_set_exec_size(devinfo, endif_inst, brw_inst_exec_size(devinfo, if_inst));

   if (else_inst == NULL) {
      /* Patch IF -> ENDIF */
      brw_inst_set_uip(devinfo, if_inst, br * (endif_inst - if_inst));
      brw_inst_set_jip(devinfo, if_inst, br * (endif_inst - if_inst));
   } else {
      brw_inst_set_exec_size(devinfo, else_inst, brw_inst_exec_size(devinfo, if_inst));

      /* Patch ELSE -> ENDIF */
      /* The IF instruction's JIP should point just past the ELSE */
      brw_inst_set_jip(devinfo, if_inst, br * (else_inst - if_inst + 1));
      /* The IF instruction's UIP and ELSE's JIP should point to ENDIF */
      brw_inst_set_uip(devinfo, if_inst, br * (endif_inst - if_inst));

      if (devinfo->ver < 11) {
         /* Set the ELSE instruction to use branch_ctrl with a join
          * jump target pointing at the NOP inserted right before
          * the ENDIF instruction in order to make sure it is
          * executed in all cases, since attempting to do the same
          * as on other generations could cause the EU to jump at
          * the instruction immediately after the ENDIF due to
          * Wa_220160235, which could cause the program to continue
          * running with all channels disabled.
          */
         brw_inst_set_jip(devinfo, else_inst, br * (endif_inst - else_inst - 1));
         brw_inst_set_branch_control(devinfo, else_inst, true);
      } else {
         brw_inst_set_jip(devinfo, else_inst, br * (endif_inst - else_inst));
      }

      /* Since we don't set branch_ctrl on Gfx11+, the ELSE's
       * JIP and UIP both should point to ENDIF on those
       * platforms.
       */
      brw_inst_set_uip(devinfo, else_inst, br * (endif_inst - else_inst));
   }
}

void
brw_ELSE(struct brw_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   brw_inst *insn;

   insn = next_insn(p, BRW_OPCODE_ELSE);

   brw_set_dest(p, insn, retype(brw_null_reg(), BRW_REGISTER_TYPE_D));
   if (devinfo->ver < 12)
      brw_set_src0(p, insn, brw_imm_d(0));
   brw_inst_set_jip(devinfo, insn, 0);
   brw_inst_set_uip(devinfo, insn, 0);

   brw_inst_set_qtr_control(devinfo, insn, BRW_COMPRESSION_NONE);
   brw_inst_set_mask_control(devinfo, insn, BRW_MASK_ENABLE);

   push_if_stack(p, insn);
}

void
brw_ENDIF(struct brw_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   brw_inst *insn = NULL;
   brw_inst *else_inst = NULL;
   brw_inst *if_inst = NULL;
   brw_inst *tmp;

   assert(p->if_stack_depth > 0);

   if (devinfo->ver < 11 &&
       brw_inst_opcode(p->isa, &p->store[p->if_stack[
                             p->if_stack_depth - 1]]) == BRW_OPCODE_ELSE) {
      /* Insert a NOP to be specified as join instruction within the
       * ELSE block, which is valid for an ELSE instruction with
       * branch_ctrl on.  The ELSE instruction will be set to jump
       * here instead of to the ENDIF instruction, since attempting to
       * do the latter would prevent the ENDIF from being executed in
       * some cases due to Wa_220160235, which could cause the program
       * to continue running with all channels disabled.
       */
      brw_NOP(p);
   }

   /*
    * A single next_insn() may change the base address of instruction store
    * memory(p->store), so call it first before referencing the instruction
    * store pointer from an index
    */
   insn = next_insn(p, BRW_OPCODE_ENDIF);

   /* Pop the IF and (optional) ELSE instructions from the stack */
   tmp = pop_if_stack(p);
   if (brw_inst_opcode(p->isa, tmp) == BRW_OPCODE_ELSE) {
      else_inst = tmp;
      tmp = pop_if_stack(p);
   }
   if_inst = tmp;

   brw_set_src0(p, insn, brw_imm_d(0));

   brw_inst_set_qtr_control(devinfo, insn, BRW_COMPRESSION_NONE);
   brw_inst_set_mask_control(devinfo, insn, BRW_MASK_ENABLE);

   brw_inst_set_jip(devinfo, insn, 2);
   patch_IF_ELSE(p, if_inst, else_inst, insn);
}

brw_inst *
brw_BREAK(struct brw_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   brw_inst *insn;

   insn = next_insn(p, BRW_OPCODE_BREAK);
   brw_set_dest(p, insn, retype(brw_null_reg(), BRW_REGISTER_TYPE_D));
   brw_set_src0(p, insn, brw_imm_d(0x0));
   brw_inst_set_qtr_control(devinfo, insn, BRW_COMPRESSION_NONE);
   brw_inst_set_exec_size(devinfo, insn, brw_get_default_exec_size(p));

   return insn;
}

brw_inst *
brw_CONT(struct brw_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   brw_inst *insn;

   insn = next_insn(p, BRW_OPCODE_CONTINUE);
   brw_set_dest(p, insn, brw_ip_reg());
   brw_set_src0(p, insn, brw_imm_d(0x0));

   brw_inst_set_qtr_control(devinfo, insn, BRW_COMPRESSION_NONE);
   brw_inst_set_exec_size(devinfo, insn, brw_get_default_exec_size(p));
   return insn;
}

brw_inst *
brw_HALT(struct brw_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   brw_inst *insn;

   insn = next_insn(p, BRW_OPCODE_HALT);
   brw_set_dest(p, insn, retype(brw_null_reg(), BRW_REGISTER_TYPE_D));
   if (devinfo->ver < 12) {
      brw_set_src0(p, insn, brw_imm_d(0x0));
   }

   brw_inst_set_qtr_control(devinfo, insn, BRW_COMPRESSION_NONE);
   brw_inst_set_exec_size(devinfo, insn, brw_get_default_exec_size(p));
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
 * For gfx6, there's no more mask stack, so no need for DO.  WHILE
 * just points back to the first instruction of the loop.
 */
brw_inst *
brw_DO(struct brw_codegen *p, unsigned execute_size)
{
   push_loop_stack(p, &p->store[p->nr_insn]);
   return &p->store[p->nr_insn];
}

brw_inst *
brw_WHILE(struct brw_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   brw_inst *insn, *do_insn;
   unsigned br = brw_jump_scale(devinfo);

   insn = next_insn(p, BRW_OPCODE_WHILE);
   do_insn = get_inner_do_insn(p);

   brw_set_dest(p, insn, retype(brw_null_reg(), BRW_REGISTER_TYPE_D));
   if (devinfo->ver < 12)
      brw_set_src0(p, insn, brw_imm_d(0));
   brw_inst_set_jip(devinfo, insn, br * (do_insn - insn));

   brw_inst_set_exec_size(devinfo, insn, brw_get_default_exec_size(p));

   brw_inst_set_qtr_control(devinfo, insn, BRW_COMPRESSION_NONE);

   p->loop_stack_depth--;

   return insn;
}

void brw_CMP(struct brw_codegen *p,
	     struct brw_reg dest,
	     unsigned conditional,
	     struct brw_reg src0,
	     struct brw_reg src1)
{
   const struct intel_device_info *devinfo = p->devinfo;
   brw_inst *insn = next_insn(p, BRW_OPCODE_CMP);

   brw_inst_set_cond_modifier(devinfo, insn, conditional);
   brw_set_dest(p, insn, dest);
   brw_set_src0(p, insn, src0);
   brw_set_src1(p, insn, src1);
}

void brw_CMPN(struct brw_codegen *p,
              struct brw_reg dest,
              unsigned conditional,
              struct brw_reg src0,
              struct brw_reg src1)
{
   const struct intel_device_info *devinfo = p->devinfo;
   brw_inst *insn = next_insn(p, BRW_OPCODE_CMPN);

   brw_inst_set_cond_modifier(devinfo, insn, conditional);
   brw_set_dest(p, insn, dest);
   brw_set_src0(p, insn, src0);
   brw_set_src1(p, insn, src1);
}

/***********************************************************************
 * Helpers for the various SEND message types:
 */

void gfx6_math(struct brw_codegen *p,
	       struct brw_reg dest,
	       unsigned function,
	       struct brw_reg src0,
	       struct brw_reg src1)
{
   const struct intel_device_info *devinfo = p->devinfo;
   brw_inst *insn = next_insn(p, BRW_OPCODE_MATH);

   assert(dest.file == BRW_GENERAL_REGISTER_FILE);

   assert(dest.hstride == BRW_HORIZONTAL_STRIDE_1);

   if (function == BRW_MATH_FUNCTION_INT_DIV_QUOTIENT ||
       function == BRW_MATH_FUNCTION_INT_DIV_REMAINDER ||
       function == BRW_MATH_FUNCTION_INT_DIV_QUOTIENT_AND_REMAINDER) {
      assert(src0.type != BRW_REGISTER_TYPE_F);
      assert(src1.type != BRW_REGISTER_TYPE_F);
      assert(src1.file == BRW_GENERAL_REGISTER_FILE ||
             src1.file == BRW_IMMEDIATE_VALUE);
      /* From BSpec 6647/47428 "[Instruction] Extended Math Function":
       *     INT DIV function does not support source modifiers.
       */
      assert(!src0.negate);
      assert(!src0.abs);
      assert(!src1.negate);
      assert(!src1.abs);
   } else {
      assert(src0.type == BRW_REGISTER_TYPE_F ||
             (src0.type == BRW_REGISTER_TYPE_HF && devinfo->ver >= 9));
      assert(src1.type == BRW_REGISTER_TYPE_F ||
             (src1.type == BRW_REGISTER_TYPE_HF && devinfo->ver >= 9));
   }

   brw_inst_set_math_function(devinfo, insn, function);

   brw_set_dest(p, insn, dest);
   brw_set_src0(p, insn, src0);
   brw_set_src1(p, insn, src1);
}

void
brw_send_indirect_message(struct brw_codegen *p,
                          unsigned sfid,
                          struct brw_reg dst,
                          struct brw_reg payload,
                          struct brw_reg desc,
                          unsigned desc_imm,
                          bool eot)
{
   const struct intel_device_info *devinfo = p->devinfo;
   struct brw_inst *send;

   dst = retype(dst, BRW_REGISTER_TYPE_UW);

   assert(desc.type == BRW_REGISTER_TYPE_UD);

   if (desc.file == BRW_IMMEDIATE_VALUE) {
      send = next_insn(p, BRW_OPCODE_SEND);
      brw_set_src0(p, send, retype(payload, BRW_REGISTER_TYPE_UD));
      brw_set_desc(p, send, desc.ud | desc_imm);
   } else {
      const struct tgl_swsb swsb = brw_get_default_swsb(p);
      struct brw_reg addr = retype(brw_address_reg(0), BRW_REGISTER_TYPE_UD);

      brw_push_insn_state(p);
      brw_set_default_access_mode(p, BRW_ALIGN_1);
      brw_set_default_mask_control(p, BRW_MASK_DISABLE);
      brw_set_default_exec_size(p, BRW_EXECUTE_1);
      brw_set_default_predicate_control(p, BRW_PREDICATE_NONE);
      brw_set_default_flag_reg(p, 0, 0);
      brw_set_default_swsb(p, tgl_swsb_src_dep(swsb));

      /* Load the indirect descriptor to an address register using OR so the
       * caller can specify additional descriptor bits with the desc_imm
       * immediate.
       */
      brw_OR(p, addr, desc, brw_imm_ud(desc_imm));

      brw_pop_insn_state(p);

      brw_set_default_swsb(p, tgl_swsb_dst_dep(swsb, 1));
      send = next_insn(p, BRW_OPCODE_SEND);
      brw_set_src0(p, send, retype(payload, BRW_REGISTER_TYPE_UD));

      if (devinfo->ver >= 12)
         brw_inst_set_send_sel_reg32_desc(devinfo, send, true);
      else
         brw_set_src1(p, send, addr);
   }

   brw_set_dest(p, send, dst);
   brw_inst_set_sfid(devinfo, send, sfid);
   brw_inst_set_eot(devinfo, send, eot);
}

void
brw_send_indirect_split_message(struct brw_codegen *p,
                                unsigned sfid,
                                struct brw_reg dst,
                                struct brw_reg payload0,
                                struct brw_reg payload1,
                                struct brw_reg desc,
                                unsigned desc_imm,
                                struct brw_reg ex_desc,
                                unsigned ex_desc_imm,
                                bool ex_desc_scratch,
                                bool ex_bso,
                                bool eot)
{
   const struct intel_device_info *devinfo = p->devinfo;
   struct brw_inst *send;

   dst = retype(dst, BRW_REGISTER_TYPE_UW);

   assert(desc.type == BRW_REGISTER_TYPE_UD);

   if (desc.file == BRW_IMMEDIATE_VALUE) {
      desc.ud |= desc_imm;
   } else {
      const struct tgl_swsb swsb = brw_get_default_swsb(p);
      struct brw_reg addr = retype(brw_address_reg(0), BRW_REGISTER_TYPE_UD);

      brw_push_insn_state(p);
      brw_set_default_access_mode(p, BRW_ALIGN_1);
      brw_set_default_mask_control(p, BRW_MASK_DISABLE);
      brw_set_default_exec_size(p, BRW_EXECUTE_1);
      brw_set_default_predicate_control(p, BRW_PREDICATE_NONE);
      brw_set_default_flag_reg(p, 0, 0);
      brw_set_default_swsb(p, tgl_swsb_src_dep(swsb));

      /* Load the indirect descriptor to an address register using OR so the
       * caller can specify additional descriptor bits with the desc_imm
       * immediate.
       */
      brw_OR(p, addr, desc, brw_imm_ud(desc_imm));

      brw_pop_insn_state(p);
      desc = addr;

      brw_set_default_swsb(p, tgl_swsb_dst_dep(swsb, 1));
   }

   if (ex_desc.file == BRW_IMMEDIATE_VALUE &&
       !ex_desc_scratch &&
       (devinfo->ver >= 12 ||
        ((ex_desc.ud | ex_desc_imm) & INTEL_MASK(15, 12)) == 0)) {
      /* ATS-M PRMs, Volume 2d: Command Reference: Structures,
       * EU_INSTRUCTION_SEND instruction
       *
       *    "ExBSO: Exists If: ([ExDesc.IsReg]==true)"
       */
      assert(!ex_bso);
      ex_desc.ud |= ex_desc_imm;
   } else {
      const struct tgl_swsb swsb = brw_get_default_swsb(p);
      struct brw_reg addr = retype(brw_address_reg(2), BRW_REGISTER_TYPE_UD);

      brw_push_insn_state(p);
      brw_set_default_access_mode(p, BRW_ALIGN_1);
      brw_set_default_mask_control(p, BRW_MASK_DISABLE);
      brw_set_default_exec_size(p, BRW_EXECUTE_1);
      brw_set_default_predicate_control(p, BRW_PREDICATE_NONE);
      brw_set_default_flag_reg(p, 0, 0);
      brw_set_default_swsb(p, tgl_swsb_src_dep(swsb));

      /* Load the indirect extended descriptor to an address register using OR
       * so the caller can specify additional descriptor bits with the
       * desc_imm immediate.
       *
       * Even though the instruction dispatcher always pulls the SFID and EOT
       * fields from the instruction itself, actual external unit which
       * processes the message gets the SFID and EOT from the extended
       * descriptor which comes from the address register.  If we don't OR
       * those two bits in, the external unit may get confused and hang.
       */
      unsigned imm_part = ex_bso ? 0 : (ex_desc_imm | sfid | eot << 5);

      if (ex_desc_scratch) {
         /* Or the scratch surface offset together with the immediate part of
          * the extended descriptor.
          */
         assert(devinfo->verx10 >= 125);
         brw_AND(p, addr,
                 retype(brw_vec1_grf(0, 5), BRW_REGISTER_TYPE_UD),
                 brw_imm_ud(INTEL_MASK(31, 10)));
         brw_OR(p, addr, addr, brw_imm_ud(imm_part));
      } else if (ex_desc.file == BRW_IMMEDIATE_VALUE) {
         /* ex_desc bits 15:12 don't exist in the instruction encoding prior
          * to Gfx12, so we may have fallen back to an indirect extended
          * descriptor.
          */
         brw_MOV(p, addr, brw_imm_ud(ex_desc.ud | imm_part));
      } else {
         brw_OR(p, addr, ex_desc, brw_imm_ud(imm_part));
      }

      brw_pop_insn_state(p);
      ex_desc = addr;

      brw_set_default_swsb(p, tgl_swsb_dst_dep(swsb, 1));
   }

   send = next_insn(p, devinfo->ver >= 12 ? BRW_OPCODE_SEND : BRW_OPCODE_SENDS);
   brw_set_dest(p, send, dst);
   brw_set_src0(p, send, retype(payload0, BRW_REGISTER_TYPE_UD));
   brw_set_src1(p, send, retype(payload1, BRW_REGISTER_TYPE_UD));

   if (desc.file == BRW_IMMEDIATE_VALUE) {
      brw_inst_set_send_sel_reg32_desc(devinfo, send, 0);
      brw_inst_set_send_desc(devinfo, send, desc.ud);
   } else {
      assert(desc.file == BRW_ARCHITECTURE_REGISTER_FILE);
      assert(desc.nr == BRW_ARF_ADDRESS);
      assert(desc.subnr == 0);
      brw_inst_set_send_sel_reg32_desc(devinfo, send, 1);
   }

   if (ex_desc.file == BRW_IMMEDIATE_VALUE) {
      brw_inst_set_send_sel_reg32_ex_desc(devinfo, send, 0);
      brw_inst_set_sends_ex_desc(devinfo, send, ex_desc.ud);
   } else {
      assert(ex_desc.file == BRW_ARCHITECTURE_REGISTER_FILE);
      assert(ex_desc.nr == BRW_ARF_ADDRESS);
      assert((ex_desc.subnr & 0x3) == 0);
      brw_inst_set_send_sel_reg32_ex_desc(devinfo, send, 1);
      brw_inst_set_send_ex_desc_ia_subreg_nr(devinfo, send, phys_subnr(devinfo, ex_desc) >> 2);
   }

   if (ex_bso) {
      /* The send instruction ExBSO field does not exist with UGM on Gfx20+,
       * it is assumed.
       *
       * BSpec 56890
       */
      if (devinfo->ver < 20 || sfid != GFX12_SFID_UGM)
         brw_inst_set_send_ex_bso(devinfo, send, true);
      brw_inst_set_send_src1_len(devinfo, send, GET_BITS(ex_desc_imm, 10, 6));
   }
   brw_inst_set_sfid(devinfo, send, sfid);
   brw_inst_set_eot(devinfo, send, eot);
}

static bool
while_jumps_before_offset(const struct intel_device_info *devinfo,
                          brw_inst *insn, int while_offset, int start_offset)
{
   int scale = 16 / brw_jump_scale(devinfo);
   int jip = brw_inst_jip(devinfo, insn);
   assert(jip < 0);
   return while_offset + jip * scale <= start_offset;
}


static int
brw_find_next_block_end(struct brw_codegen *p, int start_offset)
{
   int offset;
   void *store = p->store;
   const struct intel_device_info *devinfo = p->devinfo;

   int depth = 0;

   for (offset = next_offset(devinfo, store, start_offset);
        offset < p->next_insn_offset;
        offset = next_offset(devinfo, store, offset)) {
      brw_inst *insn = store + offset;

      switch (brw_inst_opcode(p->isa, insn)) {
      case BRW_OPCODE_IF:
         depth++;
         break;
      case BRW_OPCODE_ENDIF:
         if (depth == 0)
            return offset;
         depth--;
         break;
      case BRW_OPCODE_WHILE:
         /* If the while doesn't jump before our instruction, it's the end
          * of a sibling do...while loop.  Ignore it.
          */
         if (!while_jumps_before_offset(devinfo, insn, offset, start_offset))
            continue;
         FALLTHROUGH;
      case BRW_OPCODE_ELSE:
      case BRW_OPCODE_HALT:
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
brw_find_loop_end(struct brw_codegen *p, int start_offset)
{
   const struct intel_device_info *devinfo = p->devinfo;
   int offset;
   void *store = p->store;

   /* Always start after the instruction (such as a WHILE) we're trying to fix
    * up.
    */
   for (offset = next_offset(devinfo, store, start_offset);
        offset < p->next_insn_offset;
        offset = next_offset(devinfo, store, offset)) {
      brw_inst *insn = store + offset;

      if (brw_inst_opcode(p->isa, insn) == BRW_OPCODE_WHILE) {
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
brw_set_uip_jip(struct brw_codegen *p, int start_offset)
{
   const struct intel_device_info *devinfo = p->devinfo;
   int offset;
   int br = brw_jump_scale(devinfo);
   int scale = 16 / br;
   void *store = p->store;

   for (offset = start_offset; offset < p->next_insn_offset; offset += 16) {
      brw_inst *insn = store + offset;
      assert(brw_inst_cmpt_control(devinfo, insn) == 0);

      switch (brw_inst_opcode(p->isa, insn)) {
      case BRW_OPCODE_BREAK: {
         int block_end_offset = brw_find_next_block_end(p, offset);
         assert(block_end_offset != 0);
         brw_inst_set_jip(devinfo, insn, (block_end_offset - offset) / scale);
	 /* Gfx7 UIP points to WHILE; Gfx6 points just after it */
         brw_inst_set_uip(devinfo, insn,
	    (brw_find_loop_end(p, offset) - offset) / scale);
	 break;
      }

      case BRW_OPCODE_CONTINUE: {
         int block_end_offset = brw_find_next_block_end(p, offset);
         assert(block_end_offset != 0);
         brw_inst_set_jip(devinfo, insn, (block_end_offset - offset) / scale);
         brw_inst_set_uip(devinfo, insn,
            (brw_find_loop_end(p, offset) - offset) / scale);

         assert(brw_inst_uip(devinfo, insn) != 0);
         assert(brw_inst_jip(devinfo, insn) != 0);
	 break;
      }

      case BRW_OPCODE_ENDIF: {
         int block_end_offset = brw_find_next_block_end(p, offset);
         int32_t jump = (block_end_offset == 0) ?
                        1 * br : (block_end_offset - offset) / scale;
         brw_inst_set_jip(devinfo, insn, jump);
	 break;
      }

      case BRW_OPCODE_HALT: {
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
         int block_end_offset = brw_find_next_block_end(p, offset);
	 if (block_end_offset == 0) {
            brw_inst_set_jip(devinfo, insn, brw_inst_uip(devinfo, insn));
	 } else {
            brw_inst_set_jip(devinfo, insn, (block_end_offset - offset) / scale);
	 }
         assert(brw_inst_uip(devinfo, insn) != 0);
         assert(brw_inst_jip(devinfo, insn) != 0);
	 break;
      }

      default:
         break;
      }
   }
}

static void
brw_set_memory_fence_message(struct brw_codegen *p,
                             struct brw_inst *insn,
                             enum brw_message_target sfid,
                             bool commit_enable,
                             unsigned bti)
{
   const struct intel_device_info *devinfo = p->devinfo;

   brw_set_desc(p, insn, brw_message_desc(
                   devinfo, 1, (commit_enable ? 1 : 0), true));

   brw_inst_set_sfid(devinfo, insn, sfid);

   switch (sfid) {
   case GFX6_SFID_DATAPORT_RENDER_CACHE:
      brw_inst_set_dp_msg_type(devinfo, insn, GFX7_DATAPORT_RC_MEMORY_FENCE);
      break;
   case GFX7_SFID_DATAPORT_DATA_CACHE:
      brw_inst_set_dp_msg_type(devinfo, insn, GFX7_DATAPORT_DC_MEMORY_FENCE);
      break;
   default:
      unreachable("Not reached");
   }

   if (commit_enable)
      brw_inst_set_dp_msg_control(devinfo, insn, 1 << 5);

   assert(devinfo->ver >= 11 || bti == 0);
   brw_inst_set_binding_table_index(devinfo, insn, bti);
}

static void
gfx12_set_memory_fence_message(struct brw_codegen *p,
                               struct brw_inst *insn,
                               enum brw_message_target sfid,
                               uint32_t desc)
{
   const unsigned mlen = 1 * reg_unit(p->devinfo); /* g0 header */
    /* Completion signaled by write to register. No data returned. */
   const unsigned rlen = 1 * reg_unit(p->devinfo);

   brw_inst_set_sfid(p->devinfo, insn, sfid);

   if (sfid == BRW_SFID_URB && p->devinfo->ver < 20) {
      brw_set_desc(p, insn, brw_urb_fence_desc(p->devinfo) |
                            brw_message_desc(p->devinfo, mlen, rlen, true));
   } else {
      enum lsc_fence_scope scope = lsc_fence_msg_desc_scope(p->devinfo, desc);
      enum lsc_flush_type flush_type = lsc_fence_msg_desc_flush_type(p->devinfo, desc);

      if (sfid == GFX12_SFID_TGM) {
         scope = LSC_FENCE_TILE;
         flush_type = LSC_FLUSH_TYPE_EVICT;
      }

      /* Wa_14012437816:
       *
       *   "For any fence greater than local scope, always set flush type to
       *    at least invalidate so that fence goes on properly."
       *
       *   "The bug is if flush_type is 'None', the scope is always downgraded
       *    to 'local'."
       *
       * Here set scope to NONE_6 instead of NONE, which has the same effect
       * as NONE but avoids the downgrade to scope LOCAL.
       */
      if (intel_needs_workaround(p->devinfo, 14012437816) &&
          scope > LSC_FENCE_LOCAL &&
          flush_type == LSC_FLUSH_TYPE_NONE) {
         flush_type = LSC_FLUSH_TYPE_NONE_6;
      }

      brw_set_desc(p, insn, lsc_fence_msg_desc(p->devinfo, scope,
                                               flush_type, false) |
                            brw_message_desc(p->devinfo, mlen, rlen, false));
   }
}

void
brw_memory_fence(struct brw_codegen *p,
                 struct brw_reg dst,
                 struct brw_reg src,
                 enum opcode send_op,
                 enum brw_message_target sfid,
                 uint32_t desc,
                 bool commit_enable,
                 unsigned bti)
{
   const struct intel_device_info *devinfo = p->devinfo;

   dst = retype(vec1(dst), BRW_REGISTER_TYPE_UW);
   src = retype(vec1(src), BRW_REGISTER_TYPE_UD);

   /* Set dst as destination for dependency tracking, the MEMORY_FENCE
    * message doesn't write anything back.
    */
   struct brw_inst *insn = next_insn(p, send_op);
   brw_inst_set_mask_control(devinfo, insn, BRW_MASK_DISABLE);
   brw_inst_set_exec_size(devinfo, insn, BRW_EXECUTE_1);
   brw_set_dest(p, insn, dst);
   brw_set_src0(p, insn, src);

   /* All DG2 hardware requires LSC for fence messages, even A-step */
   if (devinfo->has_lsc)
      gfx12_set_memory_fence_message(p, insn, sfid, desc);
   else
      brw_set_memory_fence_message(p, insn, sfid, commit_enable, bti);
}

void
brw_broadcast(struct brw_codegen *p,
              struct brw_reg dst,
              struct brw_reg src,
              struct brw_reg idx)
{
   const struct intel_device_info *devinfo = p->devinfo;
   assert(brw_get_default_access_mode(p) == BRW_ALIGN_1);

   brw_push_insn_state(p);
   brw_set_default_mask_control(p, BRW_MASK_DISABLE);
   brw_set_default_exec_size(p, BRW_EXECUTE_1);

   assert(src.file == BRW_GENERAL_REGISTER_FILE &&
          src.address_mode == BRW_ADDRESS_DIRECT);
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
   src.type = dst.type = brw_reg_type_from_bit_size(type_sz(src.type) * 8,
                                                    BRW_REGISTER_TYPE_UD);

   if ((src.vstride == 0 && src.hstride == 0) ||
       idx.file == BRW_IMMEDIATE_VALUE) {
      /* Trivial, the source is already uniform or the index is a constant.
       * We will typically not get here if the optimizer is doing its job, but
       * asserting would be mean.
       */
      const unsigned i = idx.file == BRW_IMMEDIATE_VALUE ? idx.ud : 0;
      src = stride(suboffset(src, i), 0, 1, 0);

      if (type_sz(src.type) > 4 && !devinfo->has_64bit_int) {
         brw_MOV(p, subscript(dst, BRW_REGISTER_TYPE_D, 0),
                    subscript(src, BRW_REGISTER_TYPE_D, 0));
         brw_set_default_swsb(p, tgl_swsb_null());
         brw_MOV(p, subscript(dst, BRW_REGISTER_TYPE_D, 1),
                    subscript(src, BRW_REGISTER_TYPE_D, 1));
      } else {
         brw_MOV(p, dst, src);
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

      const struct brw_reg addr =
         retype(brw_address_reg(0), BRW_REGISTER_TYPE_UD);
      unsigned offset = src.nr * REG_SIZE + src.subnr;
      /* Limit in bytes of the signed indirect addressing immediate. */
      const unsigned limit = 512;

      brw_push_insn_state(p);
      brw_set_default_mask_control(p, BRW_MASK_DISABLE);
      brw_set_default_predicate_control(p, BRW_PREDICATE_NONE);
      brw_set_default_flag_reg(p, 0, 0);

      /* Take into account the component size and horizontal stride. */
      assert(src.vstride == src.hstride + src.width);
      brw_SHL(p, addr, vec1(idx),
              brw_imm_ud(util_logbase2(type_sz(src.type)) +
                         src.hstride - 1));

      /* We can only address up to limit bytes using the indirect
       * addressing immediate, account for the difference if the source
       * register is above this limit.
       */
      if (offset >= limit) {
         brw_set_default_swsb(p, tgl_swsb_regdist(1));
         brw_ADD(p, addr, addr, brw_imm_ud(offset - offset % limit));
         offset = offset % limit;
      }

      brw_pop_insn_state(p);

      brw_set_default_swsb(p, tgl_swsb_regdist(1));

      /* Use indirect addressing to fetch the specified component. */
      if (type_sz(src.type) > 4 &&
          (intel_device_info_is_9lp(devinfo) || !devinfo->has_64bit_int)) {
         /* From the Cherryview PRM Vol 7. "Register Region Restrictions":
          *
          *   "When source or destination datatype is 64b or operation is
          *    integer DWord multiply, indirect addressing must not be
          *    used."
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
                    retype(brw_vec1_indirect(addr.subnr, offset),
                           BRW_REGISTER_TYPE_D));
         brw_set_default_swsb(p, tgl_swsb_null());
         brw_MOV(p, subscript(dst, BRW_REGISTER_TYPE_D, 1),
                    retype(brw_vec1_indirect(addr.subnr, offset + 4),
                           BRW_REGISTER_TYPE_D));
      } else {
         brw_MOV(p, dst,
                 retype(brw_vec1_indirect(addr.subnr, offset), src.type));
      }
   }

   brw_pop_insn_state(p);
}


/**
 * Emit the SEND message for a barrier
 */
void
brw_barrier(struct brw_codegen *p, struct brw_reg src)
{
   const struct intel_device_info *devinfo = p->devinfo;
   struct brw_inst *inst;

   brw_push_insn_state(p);
   brw_set_default_access_mode(p, BRW_ALIGN_1);
   inst = next_insn(p, BRW_OPCODE_SEND);
   brw_set_dest(p, inst, retype(brw_null_reg(), BRW_REGISTER_TYPE_UW));
   brw_set_src0(p, inst, src);
   brw_set_src1(p, inst, brw_null_reg());
   brw_set_desc(p, inst, brw_message_desc(devinfo,
                                          1 * reg_unit(devinfo), 0, false));

   brw_inst_set_sfid(devinfo, inst, BRW_SFID_MESSAGE_GATEWAY);
   brw_inst_set_gateway_subfuncid(devinfo, inst,
                                  BRW_MESSAGE_GATEWAY_SFID_BARRIER_MSG);

   brw_inst_set_mask_control(devinfo, inst, BRW_MASK_DISABLE);
   brw_pop_insn_state(p);
}


/**
 * Emit the wait instruction for a barrier
 */
void
brw_WAIT(struct brw_codegen *p)
{
   const struct intel_device_info *devinfo = p->devinfo;
   struct brw_inst *insn;

   struct brw_reg src = brw_notification_reg();

   insn = next_insn(p, BRW_OPCODE_WAIT);
   brw_set_dest(p, insn, src);
   brw_set_src0(p, insn, src);
   brw_set_src1(p, insn, brw_null_reg());

   brw_inst_set_exec_size(devinfo, insn, BRW_EXECUTE_1);
   brw_inst_set_mask_control(devinfo, insn, BRW_MASK_DISABLE);
}

void
brw_float_controls_mode(struct brw_codegen *p,
                        unsigned mode, unsigned mask)
{
   assert(p->current->mask_control == BRW_MASK_DISABLE);

   /* From the Skylake PRM, Volume 7, page 760:
    *  "Implementation Restriction on Register Access: When the control
    *   register is used as an explicit source and/or destination, hardware
    *   does not ensure execution pipeline coherency. Software must set the
    *   thread control field to ‘switch’ for an instruction that uses
    *   control register as an explicit operand."
    *
    * On Gfx12+ this is implemented in terms of SWSB annotations instead.
    */
   brw_set_default_swsb(p, tgl_swsb_regdist(1));

   brw_inst *inst = brw_AND(p, brw_cr0_reg(0), brw_cr0_reg(0),
                            brw_imm_ud(~mask));
   brw_inst_set_exec_size(p->devinfo, inst, BRW_EXECUTE_1);
   if (p->devinfo->ver < 12)
      brw_inst_set_thread_control(p->devinfo, inst, BRW_THREAD_SWITCH);

   if (mode) {
      brw_inst *inst_or = brw_OR(p, brw_cr0_reg(0), brw_cr0_reg(0),
                                 brw_imm_ud(mode));
      brw_inst_set_exec_size(p->devinfo, inst_or, BRW_EXECUTE_1);
      if (p->devinfo->ver < 12)
         brw_inst_set_thread_control(p->devinfo, inst_or, BRW_THREAD_SWITCH);
   }

   if (p->devinfo->ver >= 12)
      brw_SYNC(p, TGL_SYNC_NOP);
}

void
brw_update_reloc_imm(const struct brw_isa_info *isa,
                     brw_inst *inst,
                     uint32_t value)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   /* Sanity check that the instruction is a MOV of an immediate */
   assert(brw_inst_opcode(isa, inst) == BRW_OPCODE_MOV);
   assert(brw_inst_src0_reg_file(devinfo, inst) == BRW_IMMEDIATE_VALUE);

   /* If it was compacted, we can't safely rewrite */
   assert(brw_inst_cmpt_control(devinfo, inst) == 0);

   brw_inst_set_imm_ud(devinfo, inst, value);
}

/* A default value for constants that will be patched at run-time.
 * We pick an arbitrary value that prevents instruction compaction.
 */
#define DEFAULT_PATCH_IMM 0x4a7cc037

void
brw_MOV_reloc_imm(struct brw_codegen *p,
                  struct brw_reg dst,
                  enum brw_reg_type src_type,
                  uint32_t id)
{
   assert(type_sz(src_type) == 4);
   assert(type_sz(dst.type) == 4);

   brw_add_reloc(p, id, BRW_SHADER_RELOC_TYPE_MOV_IMM,
                 p->next_insn_offset, 0);

   brw_MOV(p, dst, retype(brw_imm_ud(DEFAULT_PATCH_IMM), src_type));
}
