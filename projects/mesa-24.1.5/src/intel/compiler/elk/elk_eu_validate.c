/*
 * Copyright © 2015-2019 Intel Corporation
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

/** @file elk_eu_validate.c
 *
 * This file implements a pass that validates shader assembly.
 *
 * The restrictions implemented herein are intended to verify that instructions
 * in shader assembly do not violate restrictions documented in the graphics
 * programming reference manuals.
 *
 * The restrictions are difficult for humans to quickly verify due to their
 * complexity and abundance.
 *
 * It is critical that this code is thoroughly unit tested because false
 * results will lead developers astray, which is worse than having no validator
 * at all. Functional changes to this file without corresponding unit tests (in
 * test_eu_validate.cpp) will be rejected.
 */

#include <stdlib.h>
#include "elk_eu.h"
#include "elk_disasm_info.h"

/* We're going to do lots of string concatenation, so this should help. */
struct string {
   char *str;
   size_t len;
};

static void
cat(struct string *dest, const struct string src)
{
   dest->str = realloc(dest->str, dest->len + src.len + 1);
   memcpy(dest->str + dest->len, src.str, src.len);
   dest->str[dest->len + src.len] = '\0';
   dest->len = dest->len + src.len;
}
#define CAT(dest, src) cat(&dest, (struct string){src, strlen(src)})

static bool
contains(const struct string haystack, const struct string needle)
{
   return haystack.str && memmem(haystack.str, haystack.len,
                                 needle.str, needle.len) != NULL;
}
#define CONTAINS(haystack, needle) \
   contains(haystack, (struct string){needle, strlen(needle)})

#define error(str)   "\tERROR: " str "\n"
#define ERROR_INDENT "\t       "

#define ERROR(msg) ERROR_IF(true, msg)
#define ERROR_IF(cond, msg)                             \
   do {                                                 \
      if ((cond) && !CONTAINS(error_msg, error(msg))) { \
         CAT(error_msg, error(msg));                    \
      }                                                 \
   } while(0)

#define CHECK(func, args...)                             \
   do {                                                  \
      struct string __msg = func(isa, inst, ##args); \
      if (__msg.str) {                                   \
         cat(&error_msg, __msg);                         \
         free(__msg.str);                                \
      }                                                  \
   } while (0)

#define STRIDE(stride) (stride != 0 ? 1 << ((stride) - 1) : 0)
#define WIDTH(width)   (1 << (width))

static bool
inst_is_send(const struct elk_isa_info *isa, const elk_inst *inst)
{
   switch (elk_inst_opcode(isa, inst)) {
   case ELK_OPCODE_SEND:
   case ELK_OPCODE_SENDC:
      return true;
   default:
      return false;
   }
}

static unsigned
signed_type(unsigned type)
{
   switch (type) {
   case ELK_REGISTER_TYPE_UD: return ELK_REGISTER_TYPE_D;
   case ELK_REGISTER_TYPE_UW: return ELK_REGISTER_TYPE_W;
   case ELK_REGISTER_TYPE_UB: return ELK_REGISTER_TYPE_B;
   case ELK_REGISTER_TYPE_UQ: return ELK_REGISTER_TYPE_Q;
   default:                   return type;
   }
}

static bool
inst_is_raw_move(const struct elk_isa_info *isa, const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   unsigned dst_type = signed_type(elk_inst_dst_type(devinfo, inst));
   unsigned src_type = signed_type(elk_inst_src0_type(devinfo, inst));

   if (elk_inst_src0_reg_file(devinfo, inst) == ELK_IMMEDIATE_VALUE) {
      /* FIXME: not strictly true */
      if (elk_inst_src0_type(devinfo, inst) == ELK_REGISTER_TYPE_VF ||
          elk_inst_src0_type(devinfo, inst) == ELK_REGISTER_TYPE_UV ||
          elk_inst_src0_type(devinfo, inst) == ELK_REGISTER_TYPE_V) {
         return false;
      }
   } else if (elk_inst_src0_negate(devinfo, inst) ||
              elk_inst_src0_abs(devinfo, inst)) {
      return false;
   }

   return elk_inst_opcode(isa, inst) == ELK_OPCODE_MOV &&
          elk_inst_saturate(devinfo, inst) == 0 &&
          dst_type == src_type;
}

static bool
dst_is_null(const struct intel_device_info *devinfo, const elk_inst *inst)
{
   return elk_inst_dst_reg_file(devinfo, inst) == ELK_ARCHITECTURE_REGISTER_FILE &&
          elk_inst_dst_da_reg_nr(devinfo, inst) == ELK_ARF_NULL;
}

static bool
src0_is_null(const struct intel_device_info *devinfo, const elk_inst *inst)
{
   return elk_inst_src0_address_mode(devinfo, inst) == ELK_ADDRESS_DIRECT &&
          elk_inst_src0_reg_file(devinfo, inst) == ELK_ARCHITECTURE_REGISTER_FILE &&
          elk_inst_src0_da_reg_nr(devinfo, inst) == ELK_ARF_NULL;
}

static bool
src1_is_null(const struct intel_device_info *devinfo, const elk_inst *inst)
{
   return elk_inst_src1_reg_file(devinfo, inst) == ELK_ARCHITECTURE_REGISTER_FILE &&
          elk_inst_src1_da_reg_nr(devinfo, inst) == ELK_ARF_NULL;
}

static bool
src0_is_acc(const struct intel_device_info *devinfo, const elk_inst *inst)
{
   return elk_inst_src0_reg_file(devinfo, inst) == ELK_ARCHITECTURE_REGISTER_FILE &&
          (elk_inst_src0_da_reg_nr(devinfo, inst) & 0xF0) == ELK_ARF_ACCUMULATOR;
}

static bool
src1_is_acc(const struct intel_device_info *devinfo, const elk_inst *inst)
{
   return elk_inst_src1_reg_file(devinfo, inst) == ELK_ARCHITECTURE_REGISTER_FILE &&
          (elk_inst_src1_da_reg_nr(devinfo, inst) & 0xF0) == ELK_ARF_ACCUMULATOR;
}

static bool
src0_has_scalar_region(const struct intel_device_info *devinfo,
                       const elk_inst *inst)
{
   return elk_inst_src0_vstride(devinfo, inst) == ELK_VERTICAL_STRIDE_0 &&
          elk_inst_src0_width(devinfo, inst) == ELK_WIDTH_1 &&
          elk_inst_src0_hstride(devinfo, inst) == ELK_HORIZONTAL_STRIDE_0;
}

static bool
src1_has_scalar_region(const struct intel_device_info *devinfo,
                       const elk_inst *inst)
{
   return elk_inst_src1_vstride(devinfo, inst) == ELK_VERTICAL_STRIDE_0 &&
          elk_inst_src1_width(devinfo, inst) == ELK_WIDTH_1 &&
          elk_inst_src1_hstride(devinfo, inst) == ELK_HORIZONTAL_STRIDE_0;
}

static struct string
invalid_values(const struct elk_isa_info *isa, const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   struct string error_msg = { .str = NULL, .len = 0 };

   switch ((enum elk_execution_size) elk_inst_exec_size(devinfo, inst)) {
   case ELK_EXECUTE_1:
   case ELK_EXECUTE_2:
   case ELK_EXECUTE_4:
   case ELK_EXECUTE_8:
   case ELK_EXECUTE_16:
   case ELK_EXECUTE_32:
      break;
   default:
      ERROR("invalid execution size");
      break;
   }

   if (error_msg.str)
      return error_msg;

   if (inst_is_send(isa, inst))
      return error_msg;

   if (num_sources == 3) {
      /* Nothing to test:
       *    No 3-src instructions on Gfx4-5
       *    No reg file bits on Gfx6-10 (align16)
       *    No invalid encodings on Gfx10-12 (align1)
       */
   } else {
      if (devinfo->ver > 6) {
         ERROR_IF(elk_inst_dst_reg_file(devinfo, inst) == MRF ||
                  (num_sources > 0 &&
                   elk_inst_src0_reg_file(devinfo, inst) == MRF) ||
                  (num_sources > 1 &&
                   elk_inst_src1_reg_file(devinfo, inst) == MRF),
                  "invalid register file encoding");
      }
   }

   if (error_msg.str)
      return error_msg;

   if (num_sources == 3) {
      if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
         ERROR("Align1 mode not allowed on Gen < 10");
      } else {
         ERROR_IF(elk_inst_3src_a16_dst_type(devinfo, inst) == INVALID_REG_TYPE ||
                  elk_inst_3src_a16_src_type(devinfo, inst) == INVALID_REG_TYPE,
                  "invalid register type encoding");
      }
   } else {
      ERROR_IF(elk_inst_dst_type (devinfo, inst) == INVALID_REG_TYPE ||
               (num_sources > 0 &&
                elk_inst_src0_type(devinfo, inst) == INVALID_REG_TYPE) ||
               (num_sources > 1 &&
                elk_inst_src1_type(devinfo, inst) == INVALID_REG_TYPE),
               "invalid register type encoding");
   }

   return error_msg;
}

static struct string
sources_not_null(const struct elk_isa_info *isa,
                 const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;
   unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   struct string error_msg = { .str = NULL, .len = 0 };

   /* Nothing to test. 3-src instructions can only have GRF sources, and
    * there's no bit to control the file.
    */
   if (num_sources == 3)
      return (struct string){};

   if (num_sources >= 1)
      ERROR_IF(src0_is_null(devinfo, inst), "src0 is null");

   if (num_sources == 2)
      ERROR_IF(src1_is_null(devinfo, inst), "src1 is null");

   return error_msg;
}

static bool
inst_uses_src_acc(const struct elk_isa_info *isa,
                  const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   /* Check instructions that use implicit accumulator sources */
   switch (elk_inst_opcode(isa, inst)) {
   case ELK_OPCODE_MAC:
   case ELK_OPCODE_MACH:
   case ELK_OPCODE_SADA2:
      return true;
   default:
      break;
   }

   /* FIXME: support 3-src instructions */
   unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   assert(num_sources < 3);

   return src0_is_acc(devinfo, inst) || (num_sources > 1 && src1_is_acc(devinfo, inst));
}

static struct string
send_restrictions(const struct elk_isa_info *isa,
                  const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   struct string error_msg = { .str = NULL, .len = 0 };

   if (inst_is_send(isa, inst)) {
      ERROR_IF(elk_inst_src0_address_mode(devinfo, inst) != ELK_ADDRESS_DIRECT,
               "send must use direct addressing");

      if (devinfo->ver >= 7) {
         ERROR_IF(elk_inst_send_src0_reg_file(devinfo, inst) != ELK_GENERAL_REGISTER_FILE,
                  "send from non-GRF");
         ERROR_IF(elk_inst_eot(devinfo, inst) &&
                  elk_inst_src0_da_reg_nr(devinfo, inst) < 112,
                  "send with EOT must use g112-g127");
      }

      if (devinfo->ver >= 8) {
         ERROR_IF(!dst_is_null(devinfo, inst) &&
                  (elk_inst_dst_da_reg_nr(devinfo, inst) +
                   elk_inst_rlen(devinfo, inst) > 127) &&
                  (elk_inst_src0_da_reg_nr(devinfo, inst) +
                   elk_inst_mlen(devinfo, inst) >
                   elk_inst_dst_da_reg_nr(devinfo, inst)),
                  "r127 must not be used for return address when there is "
                  "a src and dest overlap");
      }
   }

   return error_msg;
}

static bool
is_unsupported_inst(const struct elk_isa_info *isa,
                    const elk_inst *inst)
{
   return elk_inst_opcode(isa, inst) == ELK_OPCODE_ILLEGAL;
}

/**
 * Returns whether a combination of two types would qualify as mixed float
 * operation mode
 */
static inline bool
types_are_mixed_float(enum elk_reg_type t0, enum elk_reg_type t1)
{
   return (t0 == ELK_REGISTER_TYPE_F && t1 == ELK_REGISTER_TYPE_HF) ||
          (t1 == ELK_REGISTER_TYPE_F && t0 == ELK_REGISTER_TYPE_HF);
}

static enum elk_reg_type
execution_type_for_type(enum elk_reg_type type)
{
   switch (type) {
   case ELK_REGISTER_TYPE_NF:
   case ELK_REGISTER_TYPE_DF:
   case ELK_REGISTER_TYPE_F:
   case ELK_REGISTER_TYPE_HF:
      return type;

   case ELK_REGISTER_TYPE_VF:
      return ELK_REGISTER_TYPE_F;

   case ELK_REGISTER_TYPE_Q:
   case ELK_REGISTER_TYPE_UQ:
      return ELK_REGISTER_TYPE_Q;

   case ELK_REGISTER_TYPE_D:
   case ELK_REGISTER_TYPE_UD:
      return ELK_REGISTER_TYPE_D;

   case ELK_REGISTER_TYPE_W:
   case ELK_REGISTER_TYPE_UW:
   case ELK_REGISTER_TYPE_B:
   case ELK_REGISTER_TYPE_UB:
   case ELK_REGISTER_TYPE_V:
   case ELK_REGISTER_TYPE_UV:
      return ELK_REGISTER_TYPE_W;
   }
   unreachable("not reached");
}

/**
 * Returns the execution type of an instruction \p inst
 */
static enum elk_reg_type
execution_type(const struct elk_isa_info *isa, const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   enum elk_reg_type src0_exec_type, src1_exec_type;

   /* Execution data type is independent of destination data type, except in
    * mixed F/HF instructions.
    */
   enum elk_reg_type dst_exec_type = elk_inst_dst_type(devinfo, inst);

   src0_exec_type = execution_type_for_type(elk_inst_src0_type(devinfo, inst));
   if (num_sources == 1) {
      if (src0_exec_type == ELK_REGISTER_TYPE_HF)
         return dst_exec_type;
      return src0_exec_type;
   }

   src1_exec_type = execution_type_for_type(elk_inst_src1_type(devinfo, inst));
   if (types_are_mixed_float(src0_exec_type, src1_exec_type) ||
       types_are_mixed_float(src0_exec_type, dst_exec_type) ||
       types_are_mixed_float(src1_exec_type, dst_exec_type)) {
      return ELK_REGISTER_TYPE_F;
   }

   if (src0_exec_type == src1_exec_type)
      return src0_exec_type;

   if (src0_exec_type == ELK_REGISTER_TYPE_NF ||
       src1_exec_type == ELK_REGISTER_TYPE_NF)
      return ELK_REGISTER_TYPE_NF;

   /* Mixed operand types where one is float is float on Gen < 6
    * (and not allowed on later platforms)
    */
   if (devinfo->ver < 6 &&
       (src0_exec_type == ELK_REGISTER_TYPE_F ||
        src1_exec_type == ELK_REGISTER_TYPE_F))
      return ELK_REGISTER_TYPE_F;

   if (src0_exec_type == ELK_REGISTER_TYPE_Q ||
       src1_exec_type == ELK_REGISTER_TYPE_Q)
      return ELK_REGISTER_TYPE_Q;

   if (src0_exec_type == ELK_REGISTER_TYPE_D ||
       src1_exec_type == ELK_REGISTER_TYPE_D)
      return ELK_REGISTER_TYPE_D;

   if (src0_exec_type == ELK_REGISTER_TYPE_W ||
       src1_exec_type == ELK_REGISTER_TYPE_W)
      return ELK_REGISTER_TYPE_W;

   if (src0_exec_type == ELK_REGISTER_TYPE_DF ||
       src1_exec_type == ELK_REGISTER_TYPE_DF)
      return ELK_REGISTER_TYPE_DF;

   unreachable("not reached");
}

/**
 * Returns whether a region is packed
 *
 * A region is packed if its elements are adjacent in memory, with no
 * intervening space, no overlap, and no replicated values.
 */
static bool
is_packed(unsigned vstride, unsigned width, unsigned hstride)
{
   if (vstride == width) {
      if (vstride == 1) {
         return hstride == 0;
      } else {
         return hstride == 1;
      }
   }

   return false;
}

/**
 * Returns whether a region is linear
 *
 * A region is linear if its elements do not overlap and are not replicated.
 * Unlike a packed region, intervening space (i.e. strided values) is allowed.
 */
static bool
is_linear(unsigned vstride, unsigned width, unsigned hstride)
{
   return vstride == width * hstride ||
          (hstride == 0 && width == 1);
}

/**
 * Returns whether an instruction is an explicit or implicit conversion
 * to/from half-float.
 */
static bool
is_half_float_conversion(const struct elk_isa_info *isa,
                         const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   enum elk_reg_type dst_type = elk_inst_dst_type(devinfo, inst);

   unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   enum elk_reg_type src0_type = elk_inst_src0_type(devinfo, inst);

   if (dst_type != src0_type &&
       (dst_type == ELK_REGISTER_TYPE_HF || src0_type == ELK_REGISTER_TYPE_HF)) {
      return true;
   } else if (num_sources > 1) {
      enum elk_reg_type src1_type = elk_inst_src1_type(devinfo, inst);
      return dst_type != src1_type &&
            (dst_type == ELK_REGISTER_TYPE_HF ||
             src1_type == ELK_REGISTER_TYPE_HF);
   }

   return false;
}

/*
 * Returns whether an instruction is using mixed float operation mode
 */
static bool
is_mixed_float(const struct elk_isa_info *isa, const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   if (devinfo->ver < 8)
      return false;

   if (inst_is_send(isa, inst))
      return false;

   unsigned opcode = elk_inst_opcode(isa, inst);
   const struct elk_opcode_desc *desc = elk_opcode_desc(isa, opcode);
   if (desc->ndst == 0)
      return false;

   /* FIXME: support 3-src instructions */
   unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   assert(num_sources < 3);

   enum elk_reg_type dst_type = elk_inst_dst_type(devinfo, inst);
   enum elk_reg_type src0_type = elk_inst_src0_type(devinfo, inst);

   if (num_sources == 1)
      return types_are_mixed_float(src0_type, dst_type);

   enum elk_reg_type src1_type = elk_inst_src1_type(devinfo, inst);

   return types_are_mixed_float(src0_type, src1_type) ||
          types_are_mixed_float(src0_type, dst_type) ||
          types_are_mixed_float(src1_type, dst_type);
}

/**
 * Returns whether an instruction is an explicit or implicit conversion
 * to/from byte.
 */
static bool
is_byte_conversion(const struct elk_isa_info *isa,
                   const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   enum elk_reg_type dst_type = elk_inst_dst_type(devinfo, inst);

   unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   enum elk_reg_type src0_type = elk_inst_src0_type(devinfo, inst);

   if (dst_type != src0_type &&
       (type_sz(dst_type) == 1 || type_sz(src0_type) == 1)) {
      return true;
   } else if (num_sources > 1) {
      enum elk_reg_type src1_type = elk_inst_src1_type(devinfo, inst);
      return dst_type != src1_type &&
            (type_sz(dst_type) == 1 || type_sz(src1_type) == 1);
   }

   return false;
}

/**
 * Checks restrictions listed in "General Restrictions Based on Operand Types"
 * in the "Register Region Restrictions" section.
 */
static struct string
general_restrictions_based_on_operand_types(const struct elk_isa_info *isa,
                                            const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   const struct elk_opcode_desc *desc =
      elk_opcode_desc(isa, elk_inst_opcode(isa, inst));
   unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   unsigned exec_size = 1 << elk_inst_exec_size(devinfo, inst);
   struct string error_msg = { .str = NULL, .len = 0 };

   if (inst_is_send(isa, inst))
      return error_msg;

   enum elk_reg_type dst_type;

   if (num_sources == 3) {
      dst_type = elk_inst_3src_a16_dst_type(devinfo, inst);
   } else {
      dst_type = elk_inst_dst_type(devinfo, inst);
   }

   ERROR_IF(dst_type == ELK_REGISTER_TYPE_DF &&
            !devinfo->has_64bit_float,
            "64-bit float destination, but platform does not support it");

   ERROR_IF((dst_type == ELK_REGISTER_TYPE_Q ||
             dst_type == ELK_REGISTER_TYPE_UQ) &&
            !devinfo->has_64bit_int,
            "64-bit int destination, but platform does not support it");

   for (unsigned s = 0; s < num_sources; s++) {
      enum elk_reg_type src_type;
      if (num_sources == 3) {
         src_type = elk_inst_3src_a16_src_type(devinfo, inst);
      } else {
         switch (s) {
         case 0: src_type = elk_inst_src0_type(devinfo, inst); break;
         case 1: src_type = elk_inst_src1_type(devinfo, inst); break;
         default: unreachable("invalid src");
         }
      }

      ERROR_IF(src_type == ELK_REGISTER_TYPE_DF &&
               !devinfo->has_64bit_float,
               "64-bit float source, but platform does not support it");

      ERROR_IF((src_type == ELK_REGISTER_TYPE_Q ||
                src_type == ELK_REGISTER_TYPE_UQ) &&
               !devinfo->has_64bit_int,
               "64-bit int source, but platform does not support it");
   }

   if (num_sources == 3)
      return error_msg;

   if (exec_size == 1)
      return error_msg;

   if (desc->ndst == 0)
      return error_msg;

   /* The PRMs say:
    *
    *    Where n is the largest element size in bytes for any source or
    *    destination operand type, ExecSize * n must be <= 64.
    *
    * But we do not attempt to enforce it, because it is implied by other
    * rules:
    *
    *    - that the destination stride must match the execution data type
    *    - sources may not span more than two adjacent GRF registers
    *    - destination may not span more than two adjacent GRF registers
    *
    * In fact, checking it would weaken testing of the other rules.
    */

   unsigned dst_stride = STRIDE(elk_inst_dst_hstride(devinfo, inst));
   bool dst_type_is_byte =
      elk_inst_dst_type(devinfo, inst) == ELK_REGISTER_TYPE_B ||
      elk_inst_dst_type(devinfo, inst) == ELK_REGISTER_TYPE_UB;

   if (dst_type_is_byte) {
      if (is_packed(exec_size * dst_stride, exec_size, dst_stride)) {
         if (!inst_is_raw_move(isa, inst))
            ERROR("Only raw MOV supports a packed-byte destination");
         return error_msg;
      }
   }

   unsigned exec_type = execution_type(isa, inst);
   unsigned exec_type_size = elk_reg_type_to_size(exec_type);
   unsigned dst_type_size = elk_reg_type_to_size(dst_type);

   /* On IVB/BYT, region parameters and execution size for DF are in terms of
    * 32-bit elements, so they are doubled. For evaluating the validity of an
    * instruction, we halve them.
    */
   if (devinfo->verx10 == 70 &&
       exec_type_size == 8 && dst_type_size == 4)
      dst_type_size = 8;

   if (is_byte_conversion(isa, inst)) {
      /* From the BDW+ PRM, Volume 2a, Command Reference, Instructions - MOV:
       *
       *    "There is no direct conversion from B/UB to DF or DF to B/UB.
       *     There is no direct conversion from B/UB to Q/UQ or Q/UQ to B/UB."
       *
       * Even if these restrictions are listed for the MOV instruction, we
       * validate this more generally, since there is the possibility
       * of implicit conversions from other instructions.
       */
      enum elk_reg_type src0_type = elk_inst_src0_type(devinfo, inst);
      enum elk_reg_type src1_type = num_sources > 1 ?
                                    elk_inst_src1_type(devinfo, inst) : 0;

      ERROR_IF(type_sz(dst_type) == 1 &&
               (type_sz(src0_type) == 8 ||
                (num_sources > 1 && type_sz(src1_type) == 8)),
               "There are no direct conversions between 64-bit types and B/UB");

      ERROR_IF(type_sz(dst_type) == 8 &&
               (type_sz(src0_type) == 1 ||
                (num_sources > 1 && type_sz(src1_type) == 1)),
               "There are no direct conversions between 64-bit types and B/UB");
   }

   if (is_half_float_conversion(isa, inst)) {
      /**
       * A helper to validate used in the validation of the following restriction
       * from the BDW+ PRM, Volume 2a, Command Reference, Instructions - MOV:
       *
       *    "There is no direct conversion from HF to DF or DF to HF.
       *     There is no direct conversion from HF to Q/UQ or Q/UQ to HF."
       *
       * Even if these restrictions are listed for the MOV instruction, we
       * validate this more generally, since there is the possibility
       * of implicit conversions from other instructions, such us implicit
       * conversion from integer to HF with the ADD instruction in SKL+.
       */
      enum elk_reg_type src0_type = elk_inst_src0_type(devinfo, inst);
      enum elk_reg_type src1_type = num_sources > 1 ?
                                    elk_inst_src1_type(devinfo, inst) : 0;
      ERROR_IF(dst_type == ELK_REGISTER_TYPE_HF &&
               (type_sz(src0_type) == 8 ||
                (num_sources > 1 && type_sz(src1_type) == 8)),
               "There are no direct conversions between 64-bit types and HF");

      ERROR_IF(type_sz(dst_type) == 8 &&
               (src0_type == ELK_REGISTER_TYPE_HF ||
                (num_sources > 1 && src1_type == ELK_REGISTER_TYPE_HF)),
               "There are no direct conversions between 64-bit types and HF");

      /* From the BDW+ PRM:
       *
       *   "Conversion between Integer and HF (Half Float) must be
       *    DWord-aligned and strided by a DWord on the destination."
       *
       * Also, the above restrictions seems to be expanded on CHV and SKL+ by:
       *
       *   "There is a relaxed alignment rule for word destinations. When
       *    the destination type is word (UW, W, HF), destination data types
       *    can be aligned to either the lowest word or the second lowest
       *    word of the execution channel. This means the destination data
       *    words can be either all in the even word locations or all in the
       *    odd word locations."
       *
       * We do not implement the second rule as is though, since empirical
       * testing shows inconsistencies:
       *   - It suggests that packed 16-bit is not allowed, which is not true.
       *   - It suggests that conversions from Q/DF to W (which need to be
       *     64-bit aligned on the destination) are not possible, which is
       *     not true.
       *
       * So from this rule we only validate the implication that conversions
       * from F to HF need to be DWord strided (except in Align1 mixed
       * float mode where packed fp16 destination is allowed so long as the
       * destination is oword-aligned).
       *
       * Finally, we only validate this for Align1 because Align16 always
       * requires packed destinations, so these restrictions can't possibly
       * apply to Align16 mode.
       */
      if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
         if ((dst_type == ELK_REGISTER_TYPE_HF &&
              (elk_reg_type_is_integer(src0_type) ||
               (num_sources > 1 && elk_reg_type_is_integer(src1_type)))) ||
             (elk_reg_type_is_integer(dst_type) &&
              (src0_type == ELK_REGISTER_TYPE_HF ||
               (num_sources > 1 && src1_type == ELK_REGISTER_TYPE_HF)))) {
            ERROR_IF(dst_stride * dst_type_size != 4,
                     "Conversions between integer and half-float must be "
                     "strided by a DWord on the destination");

            unsigned subreg = elk_inst_dst_da1_subreg_nr(devinfo, inst);
            ERROR_IF(subreg % 4 != 0,
                     "Conversions between integer and half-float must be "
                     "aligned to a DWord on the destination");
         } else if (devinfo->platform == INTEL_PLATFORM_CHV &&
                    dst_type == ELK_REGISTER_TYPE_HF) {
            unsigned subreg = elk_inst_dst_da1_subreg_nr(devinfo, inst);
            ERROR_IF(dst_stride != 2 &&
                     !(is_mixed_float(isa, inst) &&
                       dst_stride == 1 && subreg % 16 == 0),
                     "Conversions to HF must have either all words in even "
                     "word locations or all words in odd word locations or "
                     "be mixed-float with Oword-aligned packed destination");
         }
      }
   }

   /* There are special regioning rules for mixed-float mode in CHV and SKL that
    * override the general rule for the ratio of sizes of the destination type
    * and the execution type. We will add validation for those in a later patch.
    */
   bool validate_dst_size_and_exec_size_ratio =
      !is_mixed_float(isa, inst) ||
      !(devinfo->platform == INTEL_PLATFORM_CHV);

   if (validate_dst_size_and_exec_size_ratio &&
       exec_type_size > dst_type_size) {
      if (!(dst_type_is_byte && inst_is_raw_move(isa, inst))) {
         ERROR_IF(dst_stride * dst_type_size != exec_type_size,
                  "Destination stride must be equal to the ratio of the sizes "
                  "of the execution data type to the destination type");
      }

      unsigned subreg = elk_inst_dst_da1_subreg_nr(devinfo, inst);

      if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1 &&
          elk_inst_dst_address_mode(devinfo, inst) == ELK_ADDRESS_DIRECT) {
         /* The i965 PRM says:
          *
          *    Implementation Restriction: The relaxed alignment rule for byte
          *    destination (#10.5) is not supported.
          */
         if (devinfo->verx10 >= 45 && dst_type_is_byte) {
            ERROR_IF(subreg % exec_type_size != 0 &&
                     subreg % exec_type_size != 1,
                     "Destination subreg must be aligned to the size of the "
                     "execution data type (or to the next lowest byte for byte "
                     "destinations)");
         } else {
            ERROR_IF(subreg % exec_type_size != 0,
                     "Destination subreg must be aligned to the size of the "
                     "execution data type");
         }
      }
   }

   return error_msg;
}

/**
 * Checks restrictions listed in "General Restrictions on Regioning Parameters"
 * in the "Register Region Restrictions" section.
 */
static struct string
general_restrictions_on_region_parameters(const struct elk_isa_info *isa,
                                          const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   const struct elk_opcode_desc *desc =
      elk_opcode_desc(isa, elk_inst_opcode(isa, inst));
   unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   unsigned exec_size = 1 << elk_inst_exec_size(devinfo, inst);
   struct string error_msg = { .str = NULL, .len = 0 };

   if (num_sources == 3)
      return (struct string){};

   if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_16) {
      if (desc->ndst != 0 && !dst_is_null(devinfo, inst))
         ERROR_IF(elk_inst_dst_hstride(devinfo, inst) != ELK_HORIZONTAL_STRIDE_1,
                  "Destination Horizontal Stride must be 1");

      if (num_sources >= 1) {
         if (devinfo->verx10 >= 75) {
            ERROR_IF(elk_inst_src0_reg_file(devinfo, inst) != ELK_IMMEDIATE_VALUE &&
                     elk_inst_src0_vstride(devinfo, inst) != ELK_VERTICAL_STRIDE_0 &&
                     elk_inst_src0_vstride(devinfo, inst) != ELK_VERTICAL_STRIDE_2 &&
                     elk_inst_src0_vstride(devinfo, inst) != ELK_VERTICAL_STRIDE_4,
                     "In Align16 mode, only VertStride of 0, 2, or 4 is allowed");
         } else {
            ERROR_IF(elk_inst_src0_reg_file(devinfo, inst) != ELK_IMMEDIATE_VALUE &&
                     elk_inst_src0_vstride(devinfo, inst) != ELK_VERTICAL_STRIDE_0 &&
                     elk_inst_src0_vstride(devinfo, inst) != ELK_VERTICAL_STRIDE_4,
                     "In Align16 mode, only VertStride of 0 or 4 is allowed");
         }
      }

      if (num_sources == 2) {
         if (devinfo->verx10 >= 75) {
            ERROR_IF(elk_inst_src1_reg_file(devinfo, inst) != ELK_IMMEDIATE_VALUE &&
                     elk_inst_src1_vstride(devinfo, inst) != ELK_VERTICAL_STRIDE_0 &&
                     elk_inst_src1_vstride(devinfo, inst) != ELK_VERTICAL_STRIDE_2 &&
                     elk_inst_src1_vstride(devinfo, inst) != ELK_VERTICAL_STRIDE_4,
                     "In Align16 mode, only VertStride of 0, 2, or 4 is allowed");
         } else {
            ERROR_IF(elk_inst_src1_reg_file(devinfo, inst) != ELK_IMMEDIATE_VALUE &&
                     elk_inst_src1_vstride(devinfo, inst) != ELK_VERTICAL_STRIDE_0 &&
                     elk_inst_src1_vstride(devinfo, inst) != ELK_VERTICAL_STRIDE_4,
                     "In Align16 mode, only VertStride of 0 or 4 is allowed");
         }
      }

      return error_msg;
   }

   for (unsigned i = 0; i < num_sources; i++) {
      unsigned vstride, width, hstride, element_size, subreg;
      enum elk_reg_type type;

#define DO_SRC(n)                                                              \
      if (elk_inst_src ## n ## _reg_file(devinfo, inst) ==                     \
          ELK_IMMEDIATE_VALUE)                                                 \
         continue;                                                             \
                                                                               \
      vstride = STRIDE(elk_inst_src ## n ## _vstride(devinfo, inst));          \
      width = WIDTH(elk_inst_src ## n ## _width(devinfo, inst));               \
      hstride = STRIDE(elk_inst_src ## n ## _hstride(devinfo, inst));          \
      type = elk_inst_src ## n ## _type(devinfo, inst);                        \
      element_size = elk_reg_type_to_size(type);                               \
      subreg = elk_inst_src ## n ## _da1_subreg_nr(devinfo, inst)

      if (i == 0) {
         DO_SRC(0);
      } else {
         DO_SRC(1);
      }
#undef DO_SRC

      /* On IVB/BYT, region parameters and execution size for DF are in terms of
       * 32-bit elements, so they are doubled. For evaluating the validity of an
       * instruction, we halve them.
       */
      if (devinfo->verx10 == 70 &&
          element_size == 8)
         element_size = 4;

      /* ExecSize must be greater than or equal to Width. */
      ERROR_IF(exec_size < width, "ExecSize must be greater than or equal "
                                  "to Width");

      /* If ExecSize = Width and HorzStride ≠ 0,
       * VertStride must be set to Width * HorzStride.
       */
      if (exec_size == width && hstride != 0) {
         ERROR_IF(vstride != width * hstride,
                  "If ExecSize = Width and HorzStride ≠ 0, "
                  "VertStride must be set to Width * HorzStride");
      }

      /* If Width = 1, HorzStride must be 0 regardless of the values of
       * ExecSize and VertStride.
       */
      if (width == 1) {
         ERROR_IF(hstride != 0,
                  "If Width = 1, HorzStride must be 0 regardless "
                  "of the values of ExecSize and VertStride");
      }

      /* If ExecSize = Width = 1, both VertStride and HorzStride must be 0. */
      if (exec_size == 1 && width == 1) {
         ERROR_IF(vstride != 0 || hstride != 0,
                  "If ExecSize = Width = 1, both VertStride "
                  "and HorzStride must be 0");
      }

      /* If VertStride = HorzStride = 0, Width must be 1 regardless of the
       * value of ExecSize.
       */
      if (vstride == 0 && hstride == 0) {
         ERROR_IF(width != 1,
                  "If VertStride = HorzStride = 0, Width must be "
                  "1 regardless of the value of ExecSize");
      }

      /* VertStride must be used to cross GRF register boundaries. This rule
       * implies that elements within a 'Width' cannot cross GRF boundaries.
       */
      const uint64_t mask = (1ULL << element_size) - 1;
      unsigned rowbase = subreg;

      for (int y = 0; y < exec_size / width; y++) {
         uint64_t access_mask = 0;
         unsigned offset = rowbase;

         for (int x = 0; x < width; x++) {
            access_mask |= mask << (offset % 64);
            offset += hstride * element_size;
         }

         rowbase += vstride * element_size;

         if ((uint32_t)access_mask != 0 && (access_mask >> 32) != 0) {
            ERROR("VertStride must be used to cross GRF register boundaries");
            break;
         }
      }
   }

   /* Dst.HorzStride must not be 0. */
   if (desc->ndst != 0 && !dst_is_null(devinfo, inst)) {
      ERROR_IF(elk_inst_dst_hstride(devinfo, inst) == ELK_HORIZONTAL_STRIDE_0,
               "Destination Horizontal Stride must not be 0");
   }

   return error_msg;
}

static struct string
special_restrictions_for_mixed_float_mode(const struct elk_isa_info *isa,
                                          const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   struct string error_msg = { .str = NULL, .len = 0 };

   const unsigned opcode = elk_inst_opcode(isa, inst);
   const unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   if (num_sources >= 3)
      return error_msg;

   if (!is_mixed_float(isa, inst))
      return error_msg;

   unsigned exec_size = 1 << elk_inst_exec_size(devinfo, inst);
   bool is_align16 = elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_16;

   enum elk_reg_type src0_type = elk_inst_src0_type(devinfo, inst);
   enum elk_reg_type src1_type = num_sources > 1 ?
                                 elk_inst_src1_type(devinfo, inst) : 0;
   enum elk_reg_type dst_type = elk_inst_dst_type(devinfo, inst);

   unsigned dst_stride = STRIDE(elk_inst_dst_hstride(devinfo, inst));
   bool dst_is_packed = is_packed(exec_size * dst_stride, exec_size, dst_stride);

   /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
    * Float Operations:
    *
    *    "Indirect addressing on source is not supported when source and
    *     destination data types are mixed float."
    */
   ERROR_IF(elk_inst_src0_address_mode(devinfo, inst) != ELK_ADDRESS_DIRECT ||
            (num_sources > 1 &&
             elk_inst_src1_address_mode(devinfo, inst) != ELK_ADDRESS_DIRECT),
            "Indirect addressing on source is not supported when source and "
            "destination data types are mixed float");

   /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
    * Float Operations:
    *
    *    "No SIMD16 in mixed mode when destination is f32. Instruction
    *     execution size must be no more than 8."
    */
   ERROR_IF(exec_size > 8 && dst_type == ELK_REGISTER_TYPE_F,
            "Mixed float mode with 32-bit float destination is limited "
            "to SIMD8");

   if (is_align16) {
      /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
       * Float Operations:
       *
       *   "In Align16 mode, when half float and float data types are mixed
       *    between source operands OR between source and destination operands,
       *    the register content are assumed to be packed."
       *
       * Since Align16 doesn't have a concept of horizontal stride (or width),
       * it means that vertical stride must always be 4, since 0 and 2 would
       * lead to replicated data, and any other value is disallowed in Align16.
       */
      ERROR_IF(elk_inst_src0_vstride(devinfo, inst) != ELK_VERTICAL_STRIDE_4,
               "Align16 mixed float mode assumes packed data (vstride must be 4");

      ERROR_IF(num_sources >= 2 &&
               elk_inst_src1_vstride(devinfo, inst) != ELK_VERTICAL_STRIDE_4,
               "Align16 mixed float mode assumes packed data (vstride must be 4");

      /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
       * Float Operations:
       *
       *   "For Align16 mixed mode, both input and output packed f16 data
       *    must be oword aligned, no oword crossing in packed f16."
       *
       * The previous rule requires that Align16 operands are always packed,
       * and since there is only one bit for Align16 subnr, which represents
       * offsets 0B and 16B, this rule is always enforced and we don't need to
       * validate it.
       */

      /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
       * Float Operations:
       *
       *    "No SIMD16 in mixed mode when destination is packed f16 for both
       *     Align1 and Align16."
       *
       * And:
       *
       *   "In Align16 mode, when half float and float data types are mixed
       *    between source operands OR between source and destination operands,
       *    the register content are assumed to be packed."
       *
       * Which implies that SIMD16 is not available in Align16. This is further
       * confirmed by:
       *
       *    "For Align16 mixed mode, both input and output packed f16 data
       *     must be oword aligned, no oword crossing in packed f16"
       *
       * Since oword-aligned packed f16 data would cross oword boundaries when
       * the execution size is larger than 8.
       */
      ERROR_IF(exec_size > 8, "Align16 mixed float mode is limited to SIMD8");

      /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
       * Float Operations:
       *
       *    "No accumulator read access for Align16 mixed float."
       */
      ERROR_IF(inst_uses_src_acc(isa, inst),
               "No accumulator read access for Align16 mixed float");
   } else {
      assert(!is_align16);

      /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
       * Float Operations:
       *
       *    "No SIMD16 in mixed mode when destination is packed f16 for both
       *     Align1 and Align16."
       */
      ERROR_IF(exec_size > 8 && dst_is_packed &&
               dst_type == ELK_REGISTER_TYPE_HF,
               "Align1 mixed float mode is limited to SIMD8 when destination "
               "is packed half-float");

      /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
       * Float Operations:
       *
       *    "Math operations for mixed mode:
       *     - In Align1, f16 inputs need to be strided"
       */
      if (opcode == ELK_OPCODE_MATH) {
         if (src0_type == ELK_REGISTER_TYPE_HF) {
            ERROR_IF(STRIDE(elk_inst_src0_hstride(devinfo, inst)) <= 1,
                     "Align1 mixed mode math needs strided half-float inputs");
         }

         if (num_sources >= 2 && src1_type == ELK_REGISTER_TYPE_HF) {
            ERROR_IF(STRIDE(elk_inst_src1_hstride(devinfo, inst)) <= 1,
                     "Align1 mixed mode math needs strided half-float inputs");
         }
      }

      if (dst_type == ELK_REGISTER_TYPE_HF && dst_stride == 1) {
         /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
          * Float Operations:
          *
          *    "In Align1, destination stride can be smaller than execution
          *     type. When destination is stride of 1, 16 bit packed data is
          *     updated on the destination. However, output packed f16 data
          *     must be oword aligned, no oword crossing in packed f16."
          *
          * The requirement of not crossing oword boundaries for 16-bit oword
          * aligned data means that execution size is limited to 8.
          */
         unsigned subreg;
         if (elk_inst_dst_address_mode(devinfo, inst) == ELK_ADDRESS_DIRECT)
            subreg = elk_inst_dst_da1_subreg_nr(devinfo, inst);
         else
            subreg = elk_inst_dst_ia_subreg_nr(devinfo, inst);
         ERROR_IF(subreg % 16 != 0,
                  "Align1 mixed mode packed half-float output must be "
                  "oword aligned");
         ERROR_IF(exec_size > 8,
                  "Align1 mixed mode packed half-float output must not "
                  "cross oword boundaries (max exec size is 8)");

         /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
          * Float Operations:
          *
          *    "When source is float or half float from accumulator register and
          *     destination is half float with a stride of 1, the source must
          *     register aligned. i.e., source must have offset zero."
          *
          * Align16 mixed float mode doesn't allow accumulator access on sources,
          * so we only need to check this for Align1.
          */
         if (src0_is_acc(devinfo, inst) &&
             (src0_type == ELK_REGISTER_TYPE_F ||
              src0_type == ELK_REGISTER_TYPE_HF)) {
            ERROR_IF(elk_inst_src0_da1_subreg_nr(devinfo, inst) != 0,
                     "Mixed float mode requires register-aligned accumulator "
                     "source reads when destination is packed half-float");

         }

         if (num_sources > 1 &&
             src1_is_acc(devinfo, inst) &&
             (src1_type == ELK_REGISTER_TYPE_F ||
              src1_type == ELK_REGISTER_TYPE_HF)) {
            ERROR_IF(elk_inst_src1_da1_subreg_nr(devinfo, inst) != 0,
                     "Mixed float mode requires register-aligned accumulator "
                     "source reads when destination is packed half-float");
         }
      }

      /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
       * Float Operations:
       *
       *    "No swizzle is allowed when an accumulator is used as an implicit
       *     source or an explicit source in an instruction. i.e. when
       *     destination is half float with an implicit accumulator source,
       *     destination stride needs to be 2."
       *
       * FIXME: it is not quite clear what the first sentence actually means
       *        or its link to the implication described after it, so we only
       *        validate the explicit implication, which is clearly described.
       */
      if (dst_type == ELK_REGISTER_TYPE_HF &&
          inst_uses_src_acc(isa, inst)) {
         ERROR_IF(dst_stride != 2,
                  "Mixed float mode with implicit/explicit accumulator "
                  "source and half-float destination requires a stride "
                  "of 2 on the destination");
      }
   }

   return error_msg;
}

/**
 * Creates an \p access_mask for an \p exec_size, \p element_size, and a region
 *
 * An \p access_mask is a 32-element array of uint64_t, where each uint64_t is
 * a bitmask of bytes accessed by the region.
 *
 * For instance the access mask of the source gX.1<4,2,2>F in an exec_size = 4
 * instruction would be
 *
 *    access_mask[0] = 0x00000000000000F0
 *    access_mask[1] = 0x000000000000F000
 *    access_mask[2] = 0x0000000000F00000
 *    access_mask[3] = 0x00000000F0000000
 *    access_mask[4-31] = 0
 *
 * because the first execution channel accesses bytes 7-4 and the second
 * execution channel accesses bytes 15-12, etc.
 */
static void
align1_access_mask(uint64_t access_mask[static 32],
                   unsigned exec_size, unsigned element_size, unsigned subreg,
                   unsigned vstride, unsigned width, unsigned hstride)
{
   const uint64_t mask = (1ULL << element_size) - 1;
   unsigned rowbase = subreg;
   unsigned element = 0;

   for (int y = 0; y < exec_size / width; y++) {
      unsigned offset = rowbase;

      for (int x = 0; x < width; x++) {
         access_mask[element++] = mask << (offset % 64);
         offset += hstride * element_size;
      }

      rowbase += vstride * element_size;
   }

   assert(element == 0 || element == exec_size);
}

/**
 * Returns the number of registers accessed according to the \p access_mask
 */
static int
registers_read(const uint64_t access_mask[static 32])
{
   int regs_read = 0;

   for (unsigned i = 0; i < 32; i++) {
      if (access_mask[i] > 0xFFFFFFFF) {
         return 2;
      } else if (access_mask[i]) {
         regs_read = 1;
      }
   }

   return regs_read;
}

/**
 * Checks restrictions listed in "Region Alignment Rules" in the "Register
 * Region Restrictions" section.
 */
static struct string
region_alignment_rules(const struct elk_isa_info *isa,
                       const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;
   const struct elk_opcode_desc *desc =
      elk_opcode_desc(isa, elk_inst_opcode(isa, inst));
   unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   unsigned exec_size = 1 << elk_inst_exec_size(devinfo, inst);
   uint64_t dst_access_mask[32], src0_access_mask[32], src1_access_mask[32];
   struct string error_msg = { .str = NULL, .len = 0 };

   if (num_sources == 3)
      return (struct string){};

   if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_16)
      return (struct string){};

   if (inst_is_send(isa, inst))
      return (struct string){};

   memset(dst_access_mask, 0, sizeof(dst_access_mask));
   memset(src0_access_mask, 0, sizeof(src0_access_mask));
   memset(src1_access_mask, 0, sizeof(src1_access_mask));

   for (unsigned i = 0; i < num_sources; i++) {
      unsigned vstride, width, hstride, element_size, subreg;
      enum elk_reg_type type;

      /* In Direct Addressing mode, a source cannot span more than 2 adjacent
       * GRF registers.
       */

#define DO_SRC(n)                                                              \
      if (elk_inst_src ## n ## _address_mode(devinfo, inst) !=                 \
          ELK_ADDRESS_DIRECT)                                                  \
         continue;                                                             \
                                                                               \
      if (elk_inst_src ## n ## _reg_file(devinfo, inst) ==                     \
          ELK_IMMEDIATE_VALUE)                                                 \
         continue;                                                             \
                                                                               \
      vstride = STRIDE(elk_inst_src ## n ## _vstride(devinfo, inst));          \
      width = WIDTH(elk_inst_src ## n ## _width(devinfo, inst));               \
      hstride = STRIDE(elk_inst_src ## n ## _hstride(devinfo, inst));          \
      type = elk_inst_src ## n ## _type(devinfo, inst);                        \
      element_size = elk_reg_type_to_size(type);                               \
      subreg = elk_inst_src ## n ## _da1_subreg_nr(devinfo, inst);             \
      align1_access_mask(src ## n ## _access_mask,                             \
                         exec_size, element_size, subreg,                      \
                         vstride, width, hstride)

      if (i == 0) {
         DO_SRC(0);
      } else {
         DO_SRC(1);
      }
#undef DO_SRC

      unsigned num_vstride = exec_size / width;
      unsigned num_hstride = width;
      unsigned vstride_elements = (num_vstride - 1) * vstride;
      unsigned hstride_elements = (num_hstride - 1) * hstride;
      unsigned offset = (vstride_elements + hstride_elements) * element_size +
                        subreg;
      ERROR_IF(offset >= 64 * reg_unit(devinfo),
               "A source cannot span more than 2 adjacent GRF registers");
   }

   if (desc->ndst == 0 || dst_is_null(devinfo, inst))
      return error_msg;

   unsigned stride = STRIDE(elk_inst_dst_hstride(devinfo, inst));
   enum elk_reg_type dst_type = elk_inst_dst_type(devinfo, inst);
   unsigned element_size = elk_reg_type_to_size(dst_type);
   unsigned subreg = elk_inst_dst_da1_subreg_nr(devinfo, inst);
   unsigned offset = ((exec_size - 1) * stride * element_size) + subreg;
   ERROR_IF(offset >= 64 * reg_unit(devinfo),
            "A destination cannot span more than 2 adjacent GRF registers");

   if (error_msg.str)
      return error_msg;

   /* On IVB/BYT, region parameters and execution size for DF are in terms of
    * 32-bit elements, so they are doubled. For evaluating the validity of an
    * instruction, we halve them.
    */
   if (devinfo->verx10 == 70 &&
       element_size == 8)
      element_size = 4;

   align1_access_mask(dst_access_mask, exec_size, element_size, subreg,
                      exec_size == 1 ? 0 : exec_size * stride,
                      exec_size == 1 ? 1 : exec_size,
                      exec_size == 1 ? 0 : stride);

   unsigned dst_regs = registers_read(dst_access_mask);
   unsigned src0_regs = registers_read(src0_access_mask);
   unsigned src1_regs = registers_read(src1_access_mask);

   /* The SNB, IVB, HSW, BDW, and CHV PRMs say:
    *
    *    When an instruction has a source region spanning two registers and a
    *    destination region contained in one register, the number of elements
    *    must be the same between two sources and one of the following must be
    *    true:
    *
    *       1. The destination region is entirely contained in the lower OWord
    *          of a register.
    *       2. The destination region is entirely contained in the upper OWord
    *          of a register.
    *       3. The destination elements are evenly split between the two OWords
    *          of a register.
    */
   if (devinfo->ver <= 8) {
      if (dst_regs == 1 && (src0_regs == 2 || src1_regs == 2)) {
         unsigned upper_oword_writes = 0, lower_oword_writes = 0;

         for (unsigned i = 0; i < exec_size; i++) {
            if (dst_access_mask[i] > 0x0000FFFF) {
               upper_oword_writes++;
            } else {
               assert(dst_access_mask[i] != 0);
               lower_oword_writes++;
            }
         }

         ERROR_IF(lower_oword_writes != 0 &&
                  upper_oword_writes != 0 &&
                  upper_oword_writes != lower_oword_writes,
                  "Writes must be to only one OWord or "
                  "evenly split between OWords");
      }
   }

   /* The IVB and HSW PRMs say:
    *
    *    When an instruction has a source region that spans two registers and
    *    the destination spans two registers, the destination elements must be
    *    evenly split between the two registers [...]
    *
    * The SNB PRM contains similar wording (but written in a much more
    * confusing manner).
    *
    * The BDW PRM says:
    *
    *    When destination spans two registers, the source may be one or two
    *    registers. The destination elements must be evenly split between the
    *    two registers.
    *
    * The SKL PRM says:
    *
    *    When destination of MATH instruction spans two registers, the
    *    destination elements must be evenly split between the two registers.
    *
    * It is not known whether this restriction applies to KBL other Gens after
    * SKL.
    */
   if (devinfo->ver <= 8 ||
       elk_inst_opcode(isa, inst) == ELK_OPCODE_MATH) {

      /* Nothing explicitly states that on Gen < 8 elements must be evenly
       * split between two destination registers in the two exceptional
       * source-region-spans-one-register cases, but since Broadwell requires
       * evenly split writes regardless of source region, we assume that it was
       * an oversight and require it.
       */
      if (dst_regs == 2) {
         unsigned upper_reg_writes = 0, lower_reg_writes = 0;

         for (unsigned i = 0; i < exec_size; i++) {
            if (dst_access_mask[i] > 0xFFFFFFFF) {
               upper_reg_writes++;
            } else {
               assert(dst_access_mask[i] != 0);
               lower_reg_writes++;
            }
         }

         ERROR_IF(upper_reg_writes != lower_reg_writes,
                  "Writes must be evenly split between the two "
                  "destination registers");
      }
   }

   /* The IVB and HSW PRMs say:
    *
    *    When an instruction has a source region that spans two registers and
    *    the destination spans two registers, the destination elements must be
    *    evenly split between the two registers and each destination register
    *    must be entirely derived from one source register.
    *
    *    Note: In such cases, the regioning parameters must ensure that the
    *    offset from the two source registers is the same.
    *
    * The SNB PRM contains similar wording (but written in a much more
    * confusing manner).
    *
    * There are effectively three rules stated here:
    *
    *    For an instruction with a source and a destination spanning two
    *    registers,
    *
    *       (1) destination elements must be evenly split between the two
    *           registers
    *       (2) all destination elements in a register must be derived
    *           from one source register
    *       (3) the offset (i.e. the starting location in each of the two
    *           registers spanned by a region) must be the same in the two
    *           registers spanned by a region
    *
    * It is impossible to violate rule (1) without violating (2) or (3), so we
    * do not attempt to validate it.
    */
   if (devinfo->ver <= 7 && dst_regs == 2) {
      for (unsigned i = 0; i < num_sources; i++) {
#define DO_SRC(n)                                                             \
         if (src ## n ## _regs <= 1)                                          \
            continue;                                                         \
                                                                              \
         for (unsigned i = 0; i < exec_size; i++) {                           \
            if ((dst_access_mask[i] > 0xFFFFFFFF) !=                          \
                (src ## n ## _access_mask[i] > 0xFFFFFFFF)) {                 \
               ERROR("Each destination register must be entirely derived "    \
                     "from one source register");                             \
               break;                                                         \
            }                                                                 \
         }                                                                    \
                                                                              \
         unsigned offset_0 =                                                  \
            elk_inst_src ## n ## _da1_subreg_nr(devinfo, inst);               \
         unsigned offset_1 = offset_0;                                        \
                                                                              \
         for (unsigned i = 0; i < exec_size; i++) {                           \
            if (src ## n ## _access_mask[i] > 0xFFFFFFFF) {                   \
               offset_1 = __builtin_ctzll(src ## n ## _access_mask[i]) - 32;  \
               break;                                                         \
            }                                                                 \
         }                                                                    \
                                                                              \
         ERROR_IF(num_sources == 2 && offset_0 != offset_1,                   \
                  "The offset from the two source registers "                 \
                  "must be the same")

         if (i == 0) {
            DO_SRC(0);
         } else {
            DO_SRC(1);
         }
#undef DO_SRC
      }
   }

   /* The IVB and HSW PRMs say:
    *
    *    When destination spans two registers, the source MUST span two
    *    registers. The exception to the above rule:
    *        1. When source is scalar, the source registers are not
    *           incremented.
    *        2. When source is packed integer Word and destination is packed
    *           integer DWord, the source register is not incremented by the
    *           source sub register is incremented.
    *
    * The SNB PRM does not contain this rule, but the internal documentation
    * indicates that it applies to SNB as well. We assume that the rule applies
    * to Gen <= 5 although their PRMs do not state it.
    *
    * While the documentation explicitly says in exception (2) that the
    * destination must be an integer DWord, the hardware allows at least a
    * float destination type as well. We emit such instructions from
    *
    *    elk_fs_visitor::emit_interpolation_setup_gfx6
    *    elk_fs_visitor::emit_fragcoord_interpolation
    *
    * and have for years with no ill effects.
    *
    * Additionally the simulator source code indicates that the real condition
    * is that the size of the destination type is 4 bytes.
    *
    * HSW PRMs also add a note to the second exception:
    *  "When lower 8 channels are disabled, the sub register of source1
    *   operand is not incremented. If the lower 8 channels are expected
    *   to be disabled, say by predication, the instruction must be split
    *   into pair of simd8 operations."
    *
    * We can't reliably know if the channels won't be disabled due to,
    * for example, IMASK. So, play it safe and disallow packed-word exception
    * for src1.
    */
   if (devinfo->ver <= 7 && dst_regs == 2) {
      enum elk_reg_type dst_type = elk_inst_dst_type(devinfo, inst);
      bool dst_is_packed_dword =
         is_packed(exec_size * stride, exec_size, stride) &&
         elk_reg_type_to_size(dst_type) == 4;

      for (unsigned i = 0; i < num_sources; i++) {
#define DO_SRC(n)                                                                  \
         unsigned vstride, width, hstride;                                         \
         vstride = STRIDE(elk_inst_src ## n ## _vstride(devinfo, inst));           \
         width = WIDTH(elk_inst_src ## n ## _width(devinfo, inst));                \
         hstride = STRIDE(elk_inst_src ## n ## _hstride(devinfo, inst));           \
         bool src ## n ## _is_packed_word =                                        \
            n != 1 && is_packed(vstride, width, hstride) &&                        \
            (elk_inst_src ## n ## _type(devinfo, inst) == ELK_REGISTER_TYPE_W ||   \
             elk_inst_src ## n ## _type(devinfo, inst) == ELK_REGISTER_TYPE_UW);   \
                                                                                   \
         ERROR_IF(src ## n ## _regs == 1 &&                                        \
                  !src ## n ## _has_scalar_region(devinfo, inst) &&                \
                  !(dst_is_packed_dword && src ## n ## _is_packed_word),           \
                  "When the destination spans two registers, the source must "     \
                  "span two registers\n" ERROR_INDENT "(exceptions for scalar "    \
                  "sources, and packed-word to packed-dword expansion for src0)")

         if (i == 0) {
            DO_SRC(0);
         } else {
            DO_SRC(1);
         }
#undef DO_SRC
      }
   }

   return error_msg;
}

static struct string
vector_immediate_restrictions(const struct elk_isa_info *isa,
                              const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   struct string error_msg = { .str = NULL, .len = 0 };

   if (num_sources == 3 || num_sources == 0)
      return (struct string){};

   unsigned file = num_sources == 1 ?
                   elk_inst_src0_reg_file(devinfo, inst) :
                   elk_inst_src1_reg_file(devinfo, inst);
   if (file != ELK_IMMEDIATE_VALUE)
      return (struct string){};

   enum elk_reg_type dst_type = elk_inst_dst_type(devinfo, inst);
   unsigned dst_type_size = elk_reg_type_to_size(dst_type);
   unsigned dst_subreg = elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1 ?
                         elk_inst_dst_da1_subreg_nr(devinfo, inst) : 0;
   unsigned dst_stride = STRIDE(elk_inst_dst_hstride(devinfo, inst));
   enum elk_reg_type type = num_sources == 1 ?
                            elk_inst_src0_type(devinfo, inst) :
                            elk_inst_src1_type(devinfo, inst);

   /* The PRMs say:
    *
    *    When an immediate vector is used in an instruction, the destination
    *    must be 128-bit aligned with destination horizontal stride equivalent
    *    to a word for an immediate integer vector (v) and equivalent to a
    *    DWord for an immediate float vector (vf).
    *
    * The text has not been updated for the addition of the immediate unsigned
    * integer vector type (uv) on SNB, but presumably the same restriction
    * applies.
    */
   switch (type) {
   case ELK_REGISTER_TYPE_V:
   case ELK_REGISTER_TYPE_UV:
   case ELK_REGISTER_TYPE_VF:
      ERROR_IF(dst_subreg % (128 / 8) != 0,
               "Destination must be 128-bit aligned in order to use immediate "
               "vector types");

      if (type == ELK_REGISTER_TYPE_VF) {
         ERROR_IF(dst_type_size * dst_stride != 4,
                  "Destination must have stride equivalent to dword in order "
                  "to use the VF type");
      } else {
         ERROR_IF(dst_type_size * dst_stride != 2,
                  "Destination must have stride equivalent to word in order "
                  "to use the V or UV type");
      }
      break;
   default:
      break;
   }

   return error_msg;
}

static struct string
special_requirements_for_handling_double_precision_data_types(
                                       const struct elk_isa_info *isa,
                                       const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   unsigned num_sources = elk_num_sources_from_inst(isa, inst);
   struct string error_msg = { .str = NULL, .len = 0 };

   if (num_sources == 3 || num_sources == 0)
      return (struct string){};

   enum elk_reg_type exec_type = execution_type(isa, inst);
   unsigned exec_type_size = elk_reg_type_to_size(exec_type);

   enum elk_reg_file dst_file = elk_inst_dst_reg_file(devinfo, inst);
   enum elk_reg_type dst_type = elk_inst_dst_type(devinfo, inst);
   unsigned dst_type_size = elk_reg_type_to_size(dst_type);
   unsigned dst_hstride = STRIDE(elk_inst_dst_hstride(devinfo, inst));
   unsigned dst_reg = elk_inst_dst_da_reg_nr(devinfo, inst);
   unsigned dst_subreg = elk_inst_dst_da1_subreg_nr(devinfo, inst);
   unsigned dst_address_mode = elk_inst_dst_address_mode(devinfo, inst);

   bool is_integer_dword_multiply =
      devinfo->ver >= 8 &&
      elk_inst_opcode(isa, inst) == ELK_OPCODE_MUL &&
      (elk_inst_src0_type(devinfo, inst) == ELK_REGISTER_TYPE_D ||
       elk_inst_src0_type(devinfo, inst) == ELK_REGISTER_TYPE_UD) &&
      (elk_inst_src1_type(devinfo, inst) == ELK_REGISTER_TYPE_D ||
       elk_inst_src1_type(devinfo, inst) == ELK_REGISTER_TYPE_UD);

   const bool is_double_precision =
      dst_type_size == 8 || exec_type_size == 8 || is_integer_dword_multiply;

   for (unsigned i = 0; i < num_sources; i++) {
      unsigned vstride, width, hstride, type_size, reg, subreg, address_mode;
      bool is_scalar_region;
      enum elk_reg_file file;
      enum elk_reg_type type;

#define DO_SRC(n)                                                              \
      if (elk_inst_src ## n ## _reg_file(devinfo, inst) ==                     \
          ELK_IMMEDIATE_VALUE)                                                 \
         continue;                                                             \
                                                                               \
      is_scalar_region = src ## n ## _has_scalar_region(devinfo, inst);        \
      vstride = STRIDE(elk_inst_src ## n ## _vstride(devinfo, inst));          \
      width = WIDTH(elk_inst_src ## n ## _width(devinfo, inst));               \
      hstride = STRIDE(elk_inst_src ## n ## _hstride(devinfo, inst));          \
      file = elk_inst_src ## n ## _reg_file(devinfo, inst);                    \
      type = elk_inst_src ## n ## _type(devinfo, inst);                        \
      type_size = elk_reg_type_to_size(type);                                  \
      reg = elk_inst_src ## n ## _da_reg_nr(devinfo, inst);                    \
      subreg = elk_inst_src ## n ## _da1_subreg_nr(devinfo, inst);             \
      address_mode = elk_inst_src ## n ## _address_mode(devinfo, inst)

      if (i == 0) {
         DO_SRC(0);
      } else {
         DO_SRC(1);
      }
#undef DO_SRC

      const unsigned src_stride = (hstride ? hstride : vstride) * type_size;
      const unsigned dst_stride = dst_hstride * dst_type_size;

      /* The PRMs say that for CHV, BXT:
       *
       *    When source or destination datatype is 64b or operation is integer
       *    DWord multiply, regioning in Align1 must follow these rules:
       *
       *    1. Source and Destination horizontal stride must be aligned to the
       *       same qword.
       *    2. Regioning must ensure Src.Vstride = Src.Width * Src.Hstride.
       *    3. Source and Destination offset must be the same, except the case
       *       of scalar source.
       */
      if (is_double_precision &&
          elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1 &&
          devinfo->platform == INTEL_PLATFORM_CHV) {
         ERROR_IF(!is_scalar_region &&
                  (src_stride % 8 != 0 ||
                   dst_stride % 8 != 0 ||
                   src_stride != dst_stride),
                  "Source and destination horizontal stride must equal and a "
                  "multiple of a qword when the execution type is 64-bit");

         ERROR_IF(vstride != width * hstride,
                  "Vstride must be Width * Hstride when the execution type is "
                  "64-bit");

         ERROR_IF(!is_scalar_region && dst_subreg != subreg,
                  "Source and destination offset must be the same when the "
                  "execution type is 64-bit");
      }

      /* The PRMs say that for CHV, BXT:
       *
       *    When source or destination datatype is 64b or operation is integer
       *    DWord multiply, indirect addressing must not be used.
       */
      if (is_double_precision && devinfo->platform == INTEL_PLATFORM_CHV) {
         ERROR_IF(ELK_ADDRESS_REGISTER_INDIRECT_REGISTER == address_mode ||
                  ELK_ADDRESS_REGISTER_INDIRECT_REGISTER == dst_address_mode,
                  "Indirect addressing is not allowed when the execution type "
                  "is 64-bit");
      }

      /* The PRMs say that for CHV, BXT:
       *
       *    ARF registers must never be used with 64b datatype or when
       *    operation is integer DWord multiply.
       *
       * We assume that the restriction does not apply to the null register.
       */
      if (is_double_precision && devinfo->platform == INTEL_PLATFORM_CHV) {
         ERROR_IF(elk_inst_opcode(isa, inst) == ELK_OPCODE_MAC ||
                  elk_inst_acc_wr_control(devinfo, inst) ||
                  (ELK_ARCHITECTURE_REGISTER_FILE == file &&
                   reg != ELK_ARF_NULL) ||
                  (ELK_ARCHITECTURE_REGISTER_FILE == dst_file &&
                   dst_reg != ELK_ARF_NULL),
                  "Architecture registers cannot be used when the execution "
                  "type is 64-bit");
      }
   }

   /* The PRMs say that for BDW, SKL:
    *
    *    If Align16 is required for an operation with QW destination and non-QW
    *    source datatypes, the execution size cannot exceed 2.
    *
    * We assume that the restriction applies to all Gfx8+ parts.
    */
   if (is_double_precision && devinfo->ver >= 8) {
      enum elk_reg_type src0_type = elk_inst_src0_type(devinfo, inst);
      enum elk_reg_type src1_type =
         num_sources > 1 ? elk_inst_src1_type(devinfo, inst) : src0_type;
      unsigned src0_type_size = elk_reg_type_to_size(src0_type);
      unsigned src1_type_size = elk_reg_type_to_size(src1_type);

      ERROR_IF(elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_16 &&
               dst_type_size == 8 &&
               (src0_type_size != 8 || src1_type_size != 8) &&
               elk_inst_exec_size(devinfo, inst) > ELK_EXECUTE_2,
               "In Align16 exec size cannot exceed 2 with a QWord destination "
               "and a non-QWord source");
   }

   /* The PRMs say that for CHV, BXT:
    *
    *    When source or destination datatype is 64b or operation is integer
    *    DWord multiply, DepCtrl must not be used.
    */
   if (is_double_precision && devinfo->platform == INTEL_PLATFORM_CHV) {
      ERROR_IF(elk_inst_no_dd_check(devinfo, inst) ||
               elk_inst_no_dd_clear(devinfo, inst),
               "DepCtrl is not allowed when the execution type is 64-bit");
   }

   return error_msg;
}

static struct string
instruction_restrictions(const struct elk_isa_info *isa,
                         const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;
   struct string error_msg = { .str = NULL, .len = 0 };

   if (elk_inst_opcode(isa, inst) == ELK_OPCODE_CMP ||
       elk_inst_opcode(isa, inst) == ELK_OPCODE_CMPN) {
      if (devinfo->ver <= 7) {
         /* Page 166 of the Ivy Bridge PRM Volume 4 part 3 (Execution Unit
          * ISA) says:
          *
          *    Accumulator cannot be destination, implicit or explicit. The
          *    destination must be a general register or the null register.
          *
          * Page 77 of the Haswell PRM Volume 2b contains the same text.  The
          * 965G PRMs contain similar text.
          *
          * Page 864 (page 880 of the PDF) of the Broadwell PRM Volume 7 says:
          *
          *    For the cmp and cmpn instructions, remove the accumulator
          *    restrictions.
          */
         ERROR_IF(elk_inst_dst_reg_file(devinfo, inst) == ELK_ARCHITECTURE_REGISTER_FILE &&
                  elk_inst_dst_da_reg_nr(devinfo, inst) != ELK_ARF_NULL,
                  "Accumulator cannot be destination, implicit or explicit.");
      }

      /* Page 166 of the Ivy Bridge PRM Volume 4 part 3 (Execution Unit ISA)
       * says:
       *
       *    If the destination is the null register, the {Switch} instruction
       *    option must be used.
       *
       * Page 77 of the Haswell PRM Volume 2b contains the same text.
       */
      if (devinfo->ver == 7) {
         ERROR_IF(dst_is_null(devinfo, inst) &&
                  elk_inst_thread_control(devinfo, inst) != ELK_THREAD_SWITCH,
                  "If the destination is the null register, the {Switch} "
                  "instruction option must be used.");
      }

      ERROR_IF(elk_inst_cond_modifier(devinfo, inst) == ELK_CONDITIONAL_NONE,
               "CMP (or CMPN) must have a condition.");
   }

   if (elk_inst_opcode(isa, inst) == ELK_OPCODE_SEL) {
      if (devinfo->ver < 6) {
         ERROR_IF(elk_inst_cond_modifier(devinfo, inst) != ELK_CONDITIONAL_NONE,
                  "SEL must not have a condition modifier");
         ERROR_IF(elk_inst_pred_control(devinfo, inst) == ELK_PREDICATE_NONE,
                  "SEL must be predicated");
      } else {
         ERROR_IF((elk_inst_cond_modifier(devinfo, inst) != ELK_CONDITIONAL_NONE) ==
                  (elk_inst_pred_control(devinfo, inst) != ELK_PREDICATE_NONE),
                  "SEL must either be predicated or have a condition modifiers");
      }
   }

   if (elk_inst_opcode(isa, inst) == ELK_OPCODE_MUL) {
      const enum elk_reg_type src0_type = elk_inst_src0_type(devinfo, inst);
      const enum elk_reg_type src1_type = elk_inst_src1_type(devinfo, inst);
      const enum elk_reg_type dst_type = elk_inst_dst_type(devinfo, inst);

      if (devinfo->ver == 6) {
         /* Page 223 of the Sandybridge PRM volume 4 part 2 says:
          *
          *    [DevSNB]: When multiple (sic) a DW and a W, the W has to be on
          *    src0, and the DW has to be on src1.
          *
          * This text appears only in the Sandybridge PRMw.
          */
         ERROR_IF(elk_reg_type_is_integer(src0_type) &&
                  type_sz(src0_type) == 4 && type_sz(src1_type) < 4,
                  "When multiplying a DW and any lower precision integer, the "
                  "DW operand must be src1.");
      } else if (devinfo->ver >= 7) {
         /* Page 966 (page 982 of the PDF) of Broadwell PRM volume 2a says:
          *
          *    When multiplying a DW and any lower precision integer, the DW
          *    operand must on src0.
          *
          * Ivy Bridge, Haswell, Skylake, and Ice Lake PRMs contain the same
          * text.
          */
         ERROR_IF(elk_reg_type_is_integer(src1_type) &&
                  type_sz(src0_type) < 4 && type_sz(src1_type) == 4,
                  "When multiplying a DW and any lower precision integer, the "
                  "DW operand must be src0.");
      }

      if (devinfo->ver <= 7) {
         /* Section 14.2.28 of Intel 965 Express Chipset PRM volume 4 says:
          *
          *    Source operands cannot be an accumulator register.
          *
          * Iron Lake, Sandybridge, and Ivy Bridge PRMs have the same text.
          * Haswell does not.  Given that later PRMs have different
          * restrictions on accumulator sources (see below), it seems most
          * likely that Haswell shares the Ivy Bridge restriction.
          */
         ERROR_IF(src0_is_acc(devinfo, inst) || src1_is_acc(devinfo, inst),
                  "Source operands cannot be an accumulator register.");
      } else {
         /* Page 971 (page 987 of the PDF), section "Accumulator
          * Restrictions," of the Broadwell PRM volume 7 says:
          *
          *    Integer source operands cannot be accumulators.
          *
          * The Skylake and Ice Lake PRMs contain the same text.
          */
         ERROR_IF((src0_is_acc(devinfo, inst) &&
                   elk_reg_type_is_integer(src0_type)) ||
                  (src1_is_acc(devinfo, inst) &&
                   elk_reg_type_is_integer(src1_type)),
                  "Integer source operands cannot be accumulators.");
      }

      if (devinfo->ver <= 6) {
         /* Page 223 of the Sandybridge PRM volume 4 part 2 says:
          *
          *    Dword integer source is not allowed for this instruction in
          *    float execution mode.  In other words, if one source is of type
          *    float (:f, :vf), the other source cannot be of type dword
          *    integer (:ud or :d).
          *
          * G965 and Iron Lake PRMs have similar text.  Later GPUs do not
          * allow mixed source types at all, but that restriction should be
          * handled elsewhere.
          */
         ERROR_IF(execution_type(isa, inst) == ELK_REGISTER_TYPE_F &&
                  (src0_type == ELK_REGISTER_TYPE_UD ||
                   src0_type == ELK_REGISTER_TYPE_D ||
                   src1_type == ELK_REGISTER_TYPE_UD ||
                   src1_type == ELK_REGISTER_TYPE_D),
                  "Dword integer source is not allowed for this instruction in"
                  "float execution mode.");
      }

      if (devinfo->ver <= 7) {
         /* Page 118 of the Haswell PRM volume 2b says:
          *
          *    When operating on integers with at least one of the source
          *    being a DWord type (signed or unsigned), the destination cannot
          *    be floating-point (implementation note: the data converter only
          *    looks at the low 34 bits of the result).
          *
          * G965, Iron Lake, Sandybridge, and Ivy Bridge have similar text.
          * Later GPUs do not allow mixed source and destination types at all,
          * but that restriction should be handled elsewhere.
          */
         ERROR_IF(dst_type == ELK_REGISTER_TYPE_F &&
                  (src0_type == ELK_REGISTER_TYPE_UD ||
                   src0_type == ELK_REGISTER_TYPE_D ||
                   src1_type == ELK_REGISTER_TYPE_UD ||
                   src1_type == ELK_REGISTER_TYPE_D),
                  "Float destination type not allowed with DWord source type.");
      }

      if (devinfo->ver == 8) {
         /* Page 966 (page 982 of the PDF) of the Broadwell PRM volume 2a
          * says:
          *
          *    When multiplying DW x DW, the dst cannot be accumulator.
          *
          * This text also appears in the Cherry Trail / Braswell PRM, but it
          * does not appear in any other PRM.
          */
         ERROR_IF((src0_type == ELK_REGISTER_TYPE_UD ||
                   src0_type == ELK_REGISTER_TYPE_D) &&
                  (src1_type == ELK_REGISTER_TYPE_UD ||
                   src1_type == ELK_REGISTER_TYPE_D) &&
                  elk_inst_dst_reg_file(devinfo, inst) == ELK_ARCHITECTURE_REGISTER_FILE &&
                  elk_inst_dst_da_reg_nr(devinfo, inst) != ELK_ARF_NULL,
                  "When multiplying DW x DW, the dst cannot be accumulator.");
      }

      /* Page 935 (page 951 of the PDF) of the Ice Lake PRM volume 2a says:
       *
       *    When multiplying integer data types, if one of the sources is a
       *    DW, the resulting full precision data is stored in the
       *    accumulator. However, if the destination data type is either W or
       *    DW, the low bits of the result are written to the destination
       *    register and the remaining high bits are discarded. This results
       *    in undefined Overflow and Sign flags. Therefore, conditional
       *    modifiers and saturation (.sat) cannot be used in this case.
       *
       * Similar text appears in every version of the PRM.
       *
       * The wording of the last sentence is not very clear.  It could either
       * be interpreted as "conditional modifiers combined with saturation
       * cannot be used" or "neither conditional modifiers nor saturation can
       * be used."  I have interpreted it as the latter primarily because that
       * is the more restrictive interpretation.
       */
      ERROR_IF((src0_type == ELK_REGISTER_TYPE_UD ||
                src0_type == ELK_REGISTER_TYPE_D ||
                src1_type == ELK_REGISTER_TYPE_UD ||
                src1_type == ELK_REGISTER_TYPE_D) &&
               (dst_type == ELK_REGISTER_TYPE_UD ||
                dst_type == ELK_REGISTER_TYPE_D ||
                dst_type == ELK_REGISTER_TYPE_UW ||
                dst_type == ELK_REGISTER_TYPE_W) &&
               (elk_inst_saturate(devinfo, inst) != 0 ||
                elk_inst_cond_modifier(devinfo, inst) != ELK_CONDITIONAL_NONE),
               "Neither Saturate nor conditional modifier allowed with DW "
               "integer multiply.");
   }

   if (elk_inst_opcode(isa, inst) == ELK_OPCODE_MATH) {
      unsigned math_function = elk_inst_math_function(devinfo, inst);
      switch (math_function) {
      case ELK_MATH_FUNCTION_INT_DIV_QUOTIENT_AND_REMAINDER:
      case ELK_MATH_FUNCTION_INT_DIV_QUOTIENT:
      case ELK_MATH_FUNCTION_INT_DIV_REMAINDER: {
         /* Page 442 of the Broadwell PRM Volume 2a "Extended Math Function" says:
          *    INT DIV function does not support source modifiers.
          * Bspec 6647 extends it back to Ivy Bridge.
          */
         bool src0_valid = !elk_inst_src0_negate(devinfo, inst) &&
                           !elk_inst_src0_abs(devinfo, inst);
         bool src1_valid = !elk_inst_src1_negate(devinfo, inst) &&
                           !elk_inst_src1_abs(devinfo, inst);
         ERROR_IF(!src0_valid || !src1_valid,
                  "INT DIV function does not support source modifiers.");
         break;
      }
      default:
         break;
      }
   }

   if (elk_inst_opcode(isa, inst) == ELK_OPCODE_OR ||
       elk_inst_opcode(isa, inst) == ELK_OPCODE_AND ||
       elk_inst_opcode(isa, inst) == ELK_OPCODE_XOR ||
       elk_inst_opcode(isa, inst) == ELK_OPCODE_NOT) {
      if (devinfo->ver >= 8) {
         /* While the behavior of the negate source modifier is defined as
          * logical not, the behavior of abs source modifier is not
          * defined. Disallow it to be safe.
          */
         ERROR_IF(elk_inst_src0_abs(devinfo, inst),
                  "Behavior of abs source modifier in logic ops is undefined.");
         ERROR_IF(elk_inst_opcode(isa, inst) != ELK_OPCODE_NOT &&
                  elk_inst_src1_reg_file(devinfo, inst) != ELK_IMMEDIATE_VALUE &&
                  elk_inst_src1_abs(devinfo, inst),
                  "Behavior of abs source modifier in logic ops is undefined.");

         /* Page 479 (page 495 of the PDF) of the Broadwell PRM volume 2a says:
          *
          *    Source modifier is not allowed if source is an accumulator.
          *
          * The same text also appears for OR, NOT, and XOR instructions.
          */
         ERROR_IF((elk_inst_src0_abs(devinfo, inst) ||
                   elk_inst_src0_negate(devinfo, inst)) &&
                  src0_is_acc(devinfo, inst),
                  "Source modifier is not allowed if source is an accumulator.");
         ERROR_IF(elk_num_sources_from_inst(isa, inst) > 1 &&
                  (elk_inst_src1_abs(devinfo, inst) ||
                   elk_inst_src1_negate(devinfo, inst)) &&
                  src1_is_acc(devinfo, inst),
                  "Source modifier is not allowed if source is an accumulator.");
      }

      /* Page 479 (page 495 of the PDF) of the Broadwell PRM volume 2a says:
       *
       *    This operation does not produce sign or overflow conditions. Only
       *    the .e/.z or .ne/.nz conditional modifiers should be used.
       *
       * The same text also appears for OR, NOT, and XOR instructions.
       *
       * Per the comment around nir_op_imod in elk_fs_nir.cpp, we have
       * determined this to not be true. The only conditions that seem
       * absolutely sketchy are O, R, and U.  Some OpenGL shaders from Doom
       * 2016 have been observed to generate and.g and operate correctly.
       */
      const enum elk_conditional_mod cmod =
         elk_inst_cond_modifier(devinfo, inst);
      ERROR_IF(cmod == ELK_CONDITIONAL_O ||
               cmod == ELK_CONDITIONAL_R ||
               cmod == ELK_CONDITIONAL_U,
               "O, R, and U conditional modifiers should not be used.");
   }

   if (elk_inst_opcode(isa, inst) == ELK_OPCODE_BFI2) {
      ERROR_IF(elk_inst_cond_modifier(devinfo, inst) != ELK_CONDITIONAL_NONE,
               "BFI2 cannot have conditional modifier");

      ERROR_IF(elk_inst_saturate(devinfo, inst),
               "BFI2 cannot have saturate modifier");

      ERROR_IF(elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1,
               "BFI2 cannot have Align1");

      enum elk_reg_type dst_type = elk_inst_3src_a16_dst_type(devinfo, inst);

      ERROR_IF(dst_type != ELK_REGISTER_TYPE_D &&
               dst_type != ELK_REGISTER_TYPE_UD,
               "BFI2 destination type must be D or UD");

      for (unsigned s = 0; s < 3; s++) {
         enum elk_reg_type src_type = elk_inst_3src_a16_src_type(devinfo, inst);

         ERROR_IF(src_type != dst_type,
                  "BFI2 source type must match destination type");
      }
   }

   if (elk_inst_opcode(isa, inst) == ELK_OPCODE_CSEL) {
      ERROR_IF(elk_inst_pred_control(devinfo, inst) != ELK_PREDICATE_NONE,
               "CSEL cannot be predicated");

      /* CSEL is CMP and SEL fused into one. The condition modifier, which
       * does not actually modify the flags, controls the built-in comparison.
       */
      ERROR_IF(elk_inst_cond_modifier(devinfo, inst) == ELK_CONDITIONAL_NONE,
               "CSEL must have a condition.");

      ERROR_IF(elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1,
               "CSEL cannot have Align1.");
      enum elk_reg_type dst_type = elk_inst_3src_a16_dst_type(devinfo, inst);

      if (devinfo->ver < 8) {
         ERROR_IF(devinfo->ver < 8, "CSEL not supported before Gfx8");
      } else {
         ERROR_IF(dst_type != ELK_REGISTER_TYPE_F &&
                  dst_type != ELK_REGISTER_TYPE_HF &&
                  dst_type != ELK_REGISTER_TYPE_D &&
                  dst_type != ELK_REGISTER_TYPE_W,
                  "CSEL destination type must be F, HF, D, or W");
      }

      for (unsigned s = 0; s < 3; s++) {
         enum elk_reg_type src_type = elk_inst_3src_a16_src_type(devinfo, inst);

         ERROR_IF(src_type != dst_type,
                  "CSEL source type must match destination type");
      }
   }

   return error_msg;
}

static struct string
send_descriptor_restrictions(const struct elk_isa_info *isa,
                             const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;
   struct string error_msg = { .str = NULL, .len = 0 };

   if (inst_is_send(isa, inst)) {
      /* We can only validate immediate descriptors */
      if (elk_inst_src1_reg_file(devinfo, inst) != ELK_IMMEDIATE_VALUE)
         return error_msg;
   } else {
      return error_msg;
   }

   if (elk_inst_sfid(devinfo, inst) == ELK_SFID_URB) {
      /* Gfx4 doesn't have a "header present" bit in the SEND message. */
      ERROR_IF(devinfo->ver > 4 && !elk_inst_header_present(devinfo, inst),
               "Header must be present for all URB messages.");

      switch (elk_inst_urb_opcode(devinfo, inst)) {
      case ELK_URB_OPCODE_WRITE_HWORD:
         break;

      /* case FF_SYNC: */
      case ELK_URB_OPCODE_WRITE_OWORD:
         /* Gfx5 / Gfx6 FF_SYNC message and Gfx7+ URB_WRITE_OWORD have the
          * same opcode value.
          */
         if (devinfo->ver == 5 || devinfo->ver == 6) {
            ERROR_IF(elk_inst_urb_global_offset(devinfo, inst) != 0,
                     "FF_SYNC global offset must be zero.");
            ERROR_IF(elk_inst_urb_swizzle_control(devinfo, inst) != 0,
                     "FF_SYNC swizzle control must be zero.");
            ERROR_IF(elk_inst_urb_used(devinfo, inst) != 0,
                     "FF_SYNC used must be zero.");
            ERROR_IF(elk_inst_urb_complete(devinfo, inst) != 0,
                     "FF_SYNC complete must be zero.");

            /* Volume 4 part 2 of the Sandybridge PRM (page 28) says:
             *
             *    A message response (writeback) length of 1 GRF will be
             *    indicated on the ‘send’ instruction if the thread requires
             *    response data and/or synchronization.
             */
            ERROR_IF((unsigned)elk_inst_rlen(devinfo, inst) > 1,
                     "FF_SYNC read length must be 0 or 1.");
         } else {
            ERROR_IF(devinfo->ver < 7,
                     "URB OWORD write messages only valid on gfx >= 7");
         }
         break;

      case ELK_URB_OPCODE_READ_HWORD:
      case ELK_URB_OPCODE_READ_OWORD:
         ERROR_IF(devinfo->ver < 7,
                  "URB read messages only valid on gfx >= 7");
         break;

      case GFX7_URB_OPCODE_ATOMIC_MOV:
      case GFX7_URB_OPCODE_ATOMIC_INC:
         ERROR_IF(devinfo->ver < 7,
                  "URB atomic move and increment messages only valid on gfx >= 7");
         break;

      case GFX8_URB_OPCODE_ATOMIC_ADD:
         /* The Haswell PRM lists this opcode as valid on page 317. */
         ERROR_IF(devinfo->verx10 < 75,
                  "URB atomic add message only valid on gfx >= 7.5");
         break;

      case GFX8_URB_OPCODE_SIMD8_READ:
         ERROR_IF(elk_inst_rlen(devinfo, inst) == 0,
                  "URB SIMD8 read message must read some data.");
         FALLTHROUGH;

      case GFX8_URB_OPCODE_SIMD8_WRITE:
         ERROR_IF(devinfo->ver < 8,
                  "URB SIMD8 messages only valid on gfx >= 8");
         break;

      default:
         ERROR_IF(true, "Invalid URB message");
         break;
      }
   }

   return error_msg;
}

bool
elk_validate_instruction(const struct elk_isa_info *isa,
                         const elk_inst *inst, int offset,
                         unsigned inst_size,
                         struct elk_disasm_info *disasm)
{
   struct string error_msg = { .str = NULL, .len = 0 };

   if (is_unsupported_inst(isa, inst)) {
      ERROR("Instruction not supported on this Gen");
   } else {
      CHECK(invalid_values);

      if (error_msg.str == NULL) {
         CHECK(sources_not_null);
         CHECK(send_restrictions);
         CHECK(general_restrictions_based_on_operand_types);
         CHECK(general_restrictions_on_region_parameters);
         CHECK(special_restrictions_for_mixed_float_mode);
         CHECK(region_alignment_rules);
         CHECK(vector_immediate_restrictions);
         CHECK(special_requirements_for_handling_double_precision_data_types);
         CHECK(instruction_restrictions);
         CHECK(send_descriptor_restrictions);
      }
   }

   if (error_msg.str && disasm) {
      elk_disasm_insert_error(disasm, offset, inst_size, error_msg.str);
   }
   free(error_msg.str);

   return error_msg.len == 0;
}

bool
elk_validate_instructions(const struct elk_isa_info *isa,
                          const void *assembly, int start_offset, int end_offset,
                          struct elk_disasm_info *disasm)
{
   const struct intel_device_info *devinfo = isa->devinfo;
   bool valid = true;

   for (int src_offset = start_offset; src_offset < end_offset;) {
      const elk_inst *inst = assembly + src_offset;
      bool is_compact = elk_inst_cmpt_control(devinfo, inst);
      unsigned inst_size = is_compact ? sizeof(elk_compact_inst)
                                      : sizeof(elk_inst);
      elk_inst uncompacted;

      if (is_compact) {
         elk_compact_inst *compacted = (void *)inst;
         elk_uncompact_instruction(isa, &uncompacted, compacted);
         inst = &uncompacted;
      }

      bool v = elk_validate_instruction(isa, inst, src_offset,
                                        inst_size, disasm);
      valid = valid && v;

      src_offset += inst_size;
   }

   return valid;
}
