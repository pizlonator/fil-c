/*
 * Copyright © 2010 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "brw_fs.h"
#include "brw_fs_builder.h"

using namespace brw;

static uint64_t
src_as_uint(const fs_reg &src)
{
   assert(src.file == IMM);

   switch (src.type) {
   case BRW_REGISTER_TYPE_W:
      return (uint64_t)(int16_t)(src.ud & 0xffff);

   case BRW_REGISTER_TYPE_UW:
      return (uint64_t)(uint16_t)(src.ud & 0xffff);

   case BRW_REGISTER_TYPE_D:
      return (uint64_t)src.d;

   case BRW_REGISTER_TYPE_UD:
      return (uint64_t)src.ud;

   case BRW_REGISTER_TYPE_Q:
      return src.d64;

   case BRW_REGISTER_TYPE_UQ:
      return src.u64;

   default:
      unreachable("Invalid integer type.");
   }
}

static fs_reg
brw_imm_for_type(uint64_t value, enum brw_reg_type type)
{
   switch (type) {
   case BRW_REGISTER_TYPE_W:
      return brw_imm_w(value);

   case BRW_REGISTER_TYPE_UW:
      return brw_imm_uw(value);

   case BRW_REGISTER_TYPE_D:
      return brw_imm_d(value);

   case BRW_REGISTER_TYPE_UD:
      return brw_imm_ud(value);

   case BRW_REGISTER_TYPE_Q:
      return brw_imm_d(value);

   case BRW_REGISTER_TYPE_UQ:
      return brw_imm_uq(value);

   default:
      unreachable("Invalid integer type.");
   }
}

bool
brw_fs_opt_algebraic(fs_visitor &s)
{
   const intel_device_info *devinfo = s.devinfo;
   bool progress = false;

   foreach_block_and_inst_safe(block, fs_inst, inst, s.cfg) {
      switch (inst->opcode) {
      case BRW_OPCODE_MOV:
         if ((inst->conditional_mod == BRW_CONDITIONAL_Z ||
              inst->conditional_mod == BRW_CONDITIONAL_NZ) &&
             inst->dst.is_null() &&
             (inst->src[0].abs || inst->src[0].negate)) {
            inst->src[0].abs = false;
            inst->src[0].negate = false;
            progress = true;
            break;
         }

         if (inst->src[0].file != IMM)
            break;

         if (inst->saturate) {
            /* Full mixed-type saturates don't happen.  However, we can end up
             * with things like:
             *
             *    mov.sat(8) g21<1>DF       -1F
             *
             * Other mixed-size-but-same-base-type cases may also be possible.
             */
            if (inst->dst.type != inst->src[0].type &&
                inst->dst.type != BRW_REGISTER_TYPE_DF &&
                inst->src[0].type != BRW_REGISTER_TYPE_F)
               assert(!"unimplemented: saturate mixed types");

            if (fs_reg_saturate_immediate(&inst->src[0])) {
               inst->saturate = false;
               progress = true;
            }
         }
         break;

      case BRW_OPCODE_MUL:
         if (inst->src[0].file != IMM && inst->src[1].file != IMM)
            continue;

         if (brw_reg_type_is_floating_point(inst->src[1].type))
            break;

         /* From the BDW PRM, Vol 2a, "mul - Multiply":
          *
          *    "When multiplying integer datatypes, if src0 is DW and src1
          *    is W, irrespective of the destination datatype, the
          *    accumulator maintains full 48-bit precision."
          *    ...
          *    "When multiplying integer data types, if one of the sources
          *    is a DW, the resulting full precision data is stored in
          *    the accumulator."
          *
          * There are also similar notes in earlier PRMs.
          *
          * The MOV instruction can copy the bits of the source, but it
          * does not clear the higher bits of the accumulator. So, because
          * we might use the full accumulator in the MUL/MACH macro, we
          * shouldn't replace such MULs with MOVs.
          */
         if ((brw_reg_type_to_size(inst->src[0].type) == 4 ||
              brw_reg_type_to_size(inst->src[1].type) == 4) &&
             (inst->dst.is_accumulator() ||
              inst->writes_accumulator_implicitly(devinfo)))
            break;

         if (inst->src[0].is_zero() || inst->src[1].is_zero()) {
            inst->opcode = BRW_OPCODE_MOV;
            inst->sources = 1;
            inst->src[0] = brw_imm_d(0);
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }

         /* a * 1.0 = a */
         if (inst->src[1].is_one()) {
            inst->opcode = BRW_OPCODE_MOV;
            inst->sources = 1;
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }

         /* a * -1.0 = -a */
         if (inst->src[0].is_negative_one()) {
            inst->opcode = BRW_OPCODE_MOV;
            inst->sources = 1;
            inst->src[0] = inst->src[1];
            inst->src[0].negate = !inst->src[0].negate;
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }

         if (inst->src[1].is_negative_one()) {
            inst->opcode = BRW_OPCODE_MOV;
            inst->sources = 1;
            inst->src[0].negate = !inst->src[0].negate;
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }

         break;
      case BRW_OPCODE_ADD:
         if (inst->src[1].file != IMM)
            continue;

         if (brw_reg_type_is_integer(inst->src[1].type) &&
             inst->src[1].is_zero()) {
            inst->opcode = BRW_OPCODE_MOV;
            inst->sources = 1;
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }

         if (inst->src[0].file == IMM) {
            assert(inst->src[0].type == BRW_REGISTER_TYPE_F);
            inst->opcode = BRW_OPCODE_MOV;
            inst->sources = 1;
            inst->src[0].f += inst->src[1].f;
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }
         break;

      case BRW_OPCODE_AND:
         if (inst->src[0].file == IMM && inst->src[1].file == IMM) {
            const uint64_t src0 = src_as_uint(inst->src[0]);
            const uint64_t src1 = src_as_uint(inst->src[1]);

            inst->opcode = BRW_OPCODE_MOV;
            inst->sources = 1;
            inst->src[0] = brw_imm_for_type(src0 & src1, inst->dst.type);
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }

         break;

      case BRW_OPCODE_OR:
         if (inst->src[0].file == IMM && inst->src[1].file == IMM) {
            const uint64_t src0 = src_as_uint(inst->src[0]);
            const uint64_t src1 = src_as_uint(inst->src[1]);

            inst->opcode = BRW_OPCODE_MOV;
            inst->sources = 1;
            inst->src[0] = brw_imm_for_type(src0 | src1, inst->dst.type);
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }

         if (inst->src[0].equals(inst->src[1]) ||
             inst->src[1].is_zero()) {
            /* On Gfx8+, the OR instruction can have a source modifier that
             * performs logical not on the operand.  Cases of 'OR r0, ~r1, 0'
             * or 'OR r0, ~r1, ~r1' should become a NOT instead of a MOV.
             */
            if (inst->src[0].negate) {
               inst->opcode = BRW_OPCODE_NOT;
               inst->sources = 1;
               inst->src[0].negate = false;
            } else {
               inst->opcode = BRW_OPCODE_MOV;
               inst->sources = 1;
            }
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }
         break;
      case BRW_OPCODE_CMP:
         if ((inst->conditional_mod == BRW_CONDITIONAL_Z ||
              inst->conditional_mod == BRW_CONDITIONAL_NZ) &&
             inst->src[1].is_zero() &&
             (inst->src[0].abs || inst->src[0].negate)) {
            inst->src[0].abs = false;
            inst->src[0].negate = false;
            progress = true;
            break;
         }
         break;
      case BRW_OPCODE_SEL:
         if (inst->src[0].equals(inst->src[1])) {
            inst->opcode = BRW_OPCODE_MOV;
            inst->sources = 1;
            inst->src[1] = reg_undef;
            inst->predicate = BRW_PREDICATE_NONE;
            inst->predicate_inverse = false;
            progress = true;
         } else if (inst->saturate && inst->src[1].file == IMM) {
            switch (inst->conditional_mod) {
            case BRW_CONDITIONAL_LE:
            case BRW_CONDITIONAL_L:
               switch (inst->src[1].type) {
               case BRW_REGISTER_TYPE_F:
                  if (inst->src[1].f >= 1.0f) {
                     inst->opcode = BRW_OPCODE_MOV;
                     inst->sources = 1;
                     inst->src[1] = reg_undef;
                     inst->conditional_mod = BRW_CONDITIONAL_NONE;
                     progress = true;
                  }
                  break;
               default:
                  break;
               }
               break;
            case BRW_CONDITIONAL_GE:
            case BRW_CONDITIONAL_G:
               switch (inst->src[1].type) {
               case BRW_REGISTER_TYPE_F:
                  if (inst->src[1].f <= 0.0f) {
                     inst->opcode = BRW_OPCODE_MOV;
                     inst->sources = 1;
                     inst->src[1] = reg_undef;
                     inst->conditional_mod = BRW_CONDITIONAL_NONE;
                     progress = true;
                  }
                  break;
               default:
                  break;
               }
            default:
               break;
            }
         }
         break;
      case BRW_OPCODE_MAD:
         if (inst->src[0].type != BRW_REGISTER_TYPE_F ||
             inst->src[1].type != BRW_REGISTER_TYPE_F ||
             inst->src[2].type != BRW_REGISTER_TYPE_F)
            break;
         if (inst->src[1].is_one()) {
            inst->opcode = BRW_OPCODE_ADD;
            inst->sources = 2;
            inst->src[1] = inst->src[2];
            inst->src[2] = reg_undef;
            progress = true;
         } else if (inst->src[2].is_one()) {
            inst->opcode = BRW_OPCODE_ADD;
            inst->sources = 2;
            inst->src[2] = reg_undef;
            progress = true;
         }
         break;
      case BRW_OPCODE_SHL:
         if (inst->src[0].file == IMM && inst->src[1].file == IMM) {
            /* It's not currently possible to generate this, and this constant
             * folding does not handle it.
             */
            assert(!inst->saturate);

            fs_reg result;

            switch (type_sz(inst->src[0].type)) {
            case 2:
               result = brw_imm_uw(0x0ffff & (inst->src[0].ud << (inst->src[1].ud & 0x1f)));
               break;
            case 4:
               result = brw_imm_ud(inst->src[0].ud << (inst->src[1].ud & 0x1f));
               break;
            case 8:
               result = brw_imm_uq(inst->src[0].u64 << (inst->src[1].ud & 0x3f));
               break;
            default:
               /* Just in case a future platform re-enables B or UB types. */
               unreachable("Invalid source size.");
            }

            inst->opcode = BRW_OPCODE_MOV;
            inst->src[0] = retype(result, inst->dst.type);
            inst->src[1] = reg_undef;
            inst->sources = 1;

            progress = true;
         }
         break;

      case SHADER_OPCODE_BROADCAST:
         if (is_uniform(inst->src[0])) {
            inst->opcode = BRW_OPCODE_MOV;
            inst->sources = 1;
            inst->force_writemask_all = true;
            progress = true;
         } else if (inst->src[1].file == IMM) {
            inst->opcode = BRW_OPCODE_MOV;
            /* It's possible that the selected component will be too large and
             * overflow the register.  This can happen if someone does a
             * readInvocation() from GLSL or SPIR-V and provides an OOB
             * invocationIndex.  If this happens and we some how manage
             * to constant fold it in and get here, then component() may cause
             * us to start reading outside of the VGRF which will lead to an
             * assert later.  Instead, just let it wrap around if it goes over
             * exec_size.
             */
            const unsigned comp = inst->src[1].ud & (inst->exec_size - 1);
            inst->src[0] = component(inst->src[0], comp);
            inst->sources = 1;
            inst->force_writemask_all = true;
            progress = true;
         }
         break;

      case SHADER_OPCODE_SHUFFLE:
         if (is_uniform(inst->src[0])) {
            inst->opcode = BRW_OPCODE_MOV;
            inst->sources = 1;
            progress = true;
         } else if (inst->src[1].file == IMM) {
            inst->opcode = BRW_OPCODE_MOV;
            inst->src[0] = component(inst->src[0],
                                     inst->src[1].ud);
            inst->sources = 1;
            progress = true;
         }
         break;

      default:
	 break;
      }

      /* Ensure that the correct source has the immediate value. 2-source
       * instructions must have the immediate in src[1]. On Gfx12 and later,
       * some 3-source instructions can have the immediate in src[0] or
       * src[2]. It's complicated, so don't mess with 3-source instructions
       * here.
       */
      if (progress && inst->sources == 2 && inst->is_commutative()) {
         if (inst->src[0].file == IMM) {
            fs_reg tmp = inst->src[1];
            inst->src[1] = inst->src[0];
            inst->src[0] = tmp;
         }
      }
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTION_DATA_FLOW |
                            DEPENDENCY_INSTRUCTION_DETAIL);

   return progress;
}
