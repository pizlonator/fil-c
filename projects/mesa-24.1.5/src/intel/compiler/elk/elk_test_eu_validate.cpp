/*
 * Copyright © 2016 Intel Corporation
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

#include <gtest/gtest.h>
#include "elk_disasm_info.h"
#include "elk_eu.h"
#include "elk_eu_defines.h"
#include "util/bitset.h"
#include "util/ralloc.h"

static const struct intel_gfx_info {
   const char *name;
} gfx_names[] = {
   { "brw", },
   { "g4x", },
   { "ilk", },
   { "snb", },
   { "ivb", },
   { "hsw", },
   { "byt", },
   { "bdw", },
   { "chv", },
};

class validation_test: public ::testing::TestWithParam<struct intel_gfx_info> {
   virtual void SetUp();

public:
   validation_test();
   virtual ~validation_test();

   struct elk_isa_info isa;
   struct elk_codegen *p;
   struct intel_device_info devinfo;
};

validation_test::validation_test()
{
   p = rzalloc(NULL, struct elk_codegen);
   memset(&devinfo, 0, sizeof(devinfo));
}

validation_test::~validation_test()
{
   ralloc_free(p);
}

void validation_test::SetUp()
{
   struct intel_gfx_info info = GetParam();
   int devid = intel_device_name_to_pci_device_id(info.name);

   intel_get_device_info_from_pci_id(devid, &devinfo);

   elk_init_isa_info(&isa, &devinfo);

   elk_init_codegen(&isa, p, p);
}

struct gfx_name {
   template <class ParamType>
   std::string
   operator()(const ::testing::TestParamInfo<ParamType>& info) const {
      return info.param.name;
   }
};

INSTANTIATE_TEST_SUITE_P(
   eu_assembly, validation_test,
   ::testing::ValuesIn(gfx_names),
   gfx_name()
);

static bool
validate(struct elk_codegen *p)
{
   const bool print = getenv("TEST_DEBUG");
   struct elk_disasm_info *disasm = elk_disasm_initialize(p->isa, NULL);

   if (print) {
      elk_disasm_new_inst_group(disasm, 0);
      elk_disasm_new_inst_group(disasm, p->next_insn_offset);
   }

   bool ret = elk_validate_instructions(p->isa, p->store, 0,
                                        p->next_insn_offset, disasm);

   if (print) {
      elk_dump_assembly(p->store, 0, p->next_insn_offset, disasm, NULL);
   }
   ralloc_free(disasm);

   return ret;
}

#define last_inst    (&p->store[p->nr_insn - 1])
#define g0           elk_vec8_grf(0, 0)
#define acc0         elk_acc_reg(8)
#define null         elk_null_reg()
#define zero         elk_imm_f(0.0f)

static void
clear_instructions(struct elk_codegen *p)
{
   p->next_insn_offset = 0;
   p->nr_insn = 0;
}

TEST_P(validation_test, sanity)
{
   elk_ADD(p, g0, g0, g0);

   EXPECT_TRUE(validate(p));
}

TEST_P(validation_test, src0_null_reg)
{
   elk_MOV(p, g0, null);

   EXPECT_FALSE(validate(p));
}

TEST_P(validation_test, src1_null_reg)
{
   elk_ADD(p, g0, g0, null);

   EXPECT_FALSE(validate(p));
}

TEST_P(validation_test, math_src0_null_reg)
{
   if (devinfo.ver >= 6) {
      elk_gfx6_math(p, g0, ELK_MATH_FUNCTION_SIN, null, null);
   } else {
      elk_gfx4_math(p, g0, ELK_MATH_FUNCTION_SIN, 0, null, ELK_MATH_PRECISION_FULL);
   }

   EXPECT_FALSE(validate(p));
}

TEST_P(validation_test, math_src1_null_reg)
{
   if (devinfo.ver >= 6) {
      elk_gfx6_math(p, g0, ELK_MATH_FUNCTION_POW, g0, null);
      EXPECT_FALSE(validate(p));
   } else {
      /* Math instructions on Gfx4/5 are actually SEND messages with payloads.
       * src1 is an immediate message descriptor set by elk_gfx4_math.
       */
   }
}

TEST_P(validation_test, opcode46)
{
   /* opcode 46 is "push" on Gen 4 and 5
    *              "fork" on Gen 6
    *              reserved on Gen 7
    *              "goto" on Gfx8+
    */
   elk_next_insn(p, elk_opcode_decode(&isa, 46));

   if (devinfo.ver == 7) {
      EXPECT_FALSE(validate(p));
   } else {
      EXPECT_TRUE(validate(p));
   }
}

TEST_P(validation_test, invalid_exec_size_encoding)
{
   const struct {
      enum elk_execution_size exec_size;
      bool expected_result;
   } test_case[] = {
      { ELK_EXECUTE_1,      true  },
      { ELK_EXECUTE_2,      true  },
      { ELK_EXECUTE_4,      true  },
      { ELK_EXECUTE_8,      true  },
      { ELK_EXECUTE_16,     true  },
      { ELK_EXECUTE_32,     true  },

      { (enum elk_execution_size)((int)ELK_EXECUTE_32 + 1), false },
      { (enum elk_execution_size)((int)ELK_EXECUTE_32 + 2), false },
   };

   for (unsigned i = 0; i < ARRAY_SIZE(test_case); i++) {
      elk_MOV(p, g0, g0);

      elk_inst_set_exec_size(&devinfo, last_inst, test_case[i].exec_size);
      elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
      elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);

      if (test_case[i].exec_size == ELK_EXECUTE_1) {
         elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_0);
         elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_1);
         elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_0);
      } else {
         elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_2);
         elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_2);
         elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);
      }

      EXPECT_EQ(test_case[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, invalid_file_encoding)
{
   elk_MOV(p, g0, g0);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_MESSAGE_REGISTER_FILE, ELK_REGISTER_TYPE_F);

   if (devinfo.ver > 6) {
      EXPECT_FALSE(validate(p));
   } else {
      EXPECT_TRUE(validate(p));
   }

   clear_instructions(p);

   if (devinfo.ver < 6) {
      elk_gfx4_math(p, g0, ELK_MATH_FUNCTION_SIN, 0, g0, ELK_MATH_PRECISION_FULL);
   } else {
      elk_gfx6_math(p, g0, ELK_MATH_FUNCTION_SIN, g0, null);
   }
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_MESSAGE_REGISTER_FILE, ELK_REGISTER_TYPE_F);

   if (devinfo.ver > 6) {
      EXPECT_FALSE(validate(p));
   } else {
      EXPECT_TRUE(validate(p));
   }
}

TEST_P(validation_test, invalid_type_encoding)
{
   enum elk_reg_file files[2] = {
      ELK_GENERAL_REGISTER_FILE,
      ELK_IMMEDIATE_VALUE,
   };

   for (unsigned i = 0; i < ARRAY_SIZE(files); i++) {
      const enum elk_reg_file file = files[i];
      const int num_bits = devinfo.ver >= 8 ? 4 : 3;
      const int num_encodings = 1 << num_bits;

      /* The data types are encoded into <num_bits> bits to be used in hardware
       * instructions, so keep a record in a bitset the invalid patterns so
       * they can be verified to be invalid when used.
       */
      BITSET_DECLARE(invalid_encodings, num_encodings);

      const struct {
         enum elk_reg_type type;
         bool expected_result;
      } test_case[] = {
         { ELK_REGISTER_TYPE_NF, devinfo.ver == 11 && file != IMM },
         { ELK_REGISTER_TYPE_DF, devinfo.has_64bit_float && (devinfo.ver >= 8 || file != IMM) },
         { ELK_REGISTER_TYPE_F,  true },
         { ELK_REGISTER_TYPE_HF, devinfo.ver >= 8 },
         { ELK_REGISTER_TYPE_VF, file == IMM },
         { ELK_REGISTER_TYPE_Q,  devinfo.has_64bit_int },
         { ELK_REGISTER_TYPE_UQ, devinfo.has_64bit_int },
         { ELK_REGISTER_TYPE_D,  true },
         { ELK_REGISTER_TYPE_UD, true },
         { ELK_REGISTER_TYPE_W,  true },
         { ELK_REGISTER_TYPE_UW, true },
         { ELK_REGISTER_TYPE_B,  file == FIXED_GRF },
         { ELK_REGISTER_TYPE_UB, file == FIXED_GRF },
         { ELK_REGISTER_TYPE_V,  file == IMM },
         { ELK_REGISTER_TYPE_UV, devinfo.ver >= 6 && file == IMM },
      };

      /* Initially assume all hardware encodings are invalid */
      BITSET_ONES(invalid_encodings);

      elk_set_default_exec_size(p, ELK_EXECUTE_4);

      for (unsigned i = 0; i < ARRAY_SIZE(test_case); i++) {
         if (test_case[i].expected_result) {
            unsigned hw_type = elk_reg_type_to_hw_type(&devinfo, file, test_case[i].type);
            if (hw_type != INVALID_REG_TYPE) {
               /* ... and remove valid encodings from the set */
               assert(BITSET_TEST(invalid_encodings, hw_type));
               BITSET_CLEAR(invalid_encodings, hw_type);
            }

            if (file == FIXED_GRF) {
               struct elk_reg g = retype(g0, test_case[i].type);
               elk_MOV(p, g, g);
               elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
               elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_4);
               elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);
            } else {
               enum elk_reg_type t;

               switch (test_case[i].type) {
               case ELK_REGISTER_TYPE_V:
                  t = ELK_REGISTER_TYPE_W;
                  break;
               case ELK_REGISTER_TYPE_UV:
                  t = ELK_REGISTER_TYPE_UW;
                  break;
               case ELK_REGISTER_TYPE_VF:
                  t = ELK_REGISTER_TYPE_F;
                  break;
               default:
                  t = test_case[i].type;
                  break;
               }

               struct elk_reg g = retype(g0, t);
               elk_MOV(p, g, retype(elk_imm_w(0), test_case[i].type));
            }

            EXPECT_TRUE(validate(p));

            clear_instructions(p);
         }
      }

      /* The remaining encodings in invalid_encodings do not have a mapping
       * from ELK_REGISTER_TYPE_* and must be invalid. Verify that invalid
       * encodings are rejected by the validator.
       */
      int e;
      BITSET_FOREACH_SET(e, invalid_encodings, num_encodings) {
         if (file == FIXED_GRF) {
            elk_MOV(p, g0, g0);
            elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
            elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_4);
            elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);
         } else {
            elk_MOV(p, g0, elk_imm_w(0));
         }
         elk_inst_set_dst_reg_hw_type(&devinfo, last_inst, e);
         elk_inst_set_src0_reg_hw_type(&devinfo, last_inst, e);

         EXPECT_FALSE(validate(p));

         clear_instructions(p);
      }
   }
}

TEST_P(validation_test, invalid_type_encoding_3src_a16)
{
   /* 3-src instructions in align16 mode only supported on Gfx6-10 */
   if (devinfo.ver < 6)
      return;

   const int num_bits = devinfo.ver >= 8 ? 3 : 2;
   const int num_encodings = 1 << num_bits;

   /* The data types are encoded into <num_bits> bits to be used in hardware
    * instructions, so keep a record in a bitset the invalid patterns so
    * they can be verified to be invalid when used.
    */
   BITSET_DECLARE(invalid_encodings, num_encodings);

   const struct {
      enum elk_reg_type type;
      bool expected_result;
   } test_case[] = {
      { ELK_REGISTER_TYPE_DF, devinfo.ver >= 7  },
      { ELK_REGISTER_TYPE_F,  true },
      { ELK_REGISTER_TYPE_HF, devinfo.ver >= 8  },
      { ELK_REGISTER_TYPE_D,  devinfo.ver >= 7  },
      { ELK_REGISTER_TYPE_UD, devinfo.ver >= 7  },
   };

   /* Initially assume all hardware encodings are invalid */
   BITSET_ONES(invalid_encodings);

   elk_set_default_access_mode(p, ELK_ALIGN_16);
   elk_set_default_exec_size(p, ELK_EXECUTE_4);

   for (unsigned i = 0; i < ARRAY_SIZE(test_case); i++) {
      if (test_case[i].expected_result) {
         unsigned hw_type = elk_reg_type_to_a16_hw_3src_type(&devinfo, test_case[i].type);
         if (hw_type != INVALID_HW_REG_TYPE) {
            /* ... and remove valid encodings from the set */
            assert(BITSET_TEST(invalid_encodings, hw_type));
            BITSET_CLEAR(invalid_encodings, hw_type);
         }

         struct elk_reg g = retype(g0, test_case[i].type);
         if (!elk_reg_type_is_integer(test_case[i].type)) {
            elk_MAD(p, g, g, g, g);
         } else {
            elk_BFE(p, g, g, g, g);
         }

         EXPECT_TRUE(validate(p));

         clear_instructions(p);
      }
   }

   /* The remaining encodings in invalid_encodings do not have a mapping
    * from ELK_REGISTER_TYPE_* and must be invalid. Verify that invalid
    * encodings are rejected by the validator.
    */
   int e;
   BITSET_FOREACH_SET(e, invalid_encodings, num_encodings) {
      for (unsigned i = 0; i < 2; i++) {
         if (i == 0) {
            elk_MAD(p, g0, g0, g0, g0);
         } else {
            elk_BFE(p, g0, g0, g0, g0);
         }

         elk_inst_set_3src_a16_dst_hw_type(&devinfo, last_inst, e);
         elk_inst_set_3src_a16_src_hw_type(&devinfo, last_inst, e);

         EXPECT_FALSE(validate(p));

         clear_instructions(p);

         if (devinfo.ver == 6)
            break;
      }
   }
}

TEST_P(validation_test, 3src_inst_access_mode)
{
   /* 3-src instructions only supported on Gfx6+ */
   if (devinfo.ver < 6)
      return;

   const struct {
      unsigned mode;
      bool expected_result;
   } test_case[] = {
      { ELK_ALIGN_1,  false},
      { ELK_ALIGN_16, true },
   };

   for (unsigned i = 0; i < ARRAY_SIZE(test_case); i++) {
      elk_set_default_access_mode(p, ELK_ALIGN_16);

      elk_MAD(p, g0, g0, g0, g0);
      elk_inst_set_access_mode(&devinfo, last_inst, test_case[i].mode);

      EXPECT_EQ(test_case[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

/* When the Execution Data Type is wider than the destination data type, the
 * destination must [...] specify a HorzStride equal to the ratio in sizes of
 * the two data types.
 */
TEST_P(validation_test, dest_stride_must_be_equal_to_the_ratio_of_exec_size_to_dest_size)
{
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);

   EXPECT_TRUE(validate(p));
}

/* When the Execution Data Type is wider than the destination data type, the
 * destination must be aligned as required by the wider execution data type
 * [...]
 */
TEST_P(validation_test, dst_subreg_must_be_aligned_to_exec_type_size)
{
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, 2);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_4);
   elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, 8);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_4);
   elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_4);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);

   EXPECT_TRUE(validate(p));
}

/* ExecSize must be greater than or equal to Width. */
TEST_P(validation_test, exec_size_less_than_width)
{
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_16);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_16);

   EXPECT_FALSE(validate(p));
}

/* If ExecSize = Width and HorzStride ≠ 0,
 * VertStride must be set to Width * HorzStride.
 */
TEST_P(validation_test, vertical_stride_is_width_by_horizontal_stride)
{
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);

   EXPECT_FALSE(validate(p));
}

/* If Width = 1, HorzStride must be 0 regardless of the values
 * of ExecSize and VertStride.
 */
TEST_P(validation_test, horizontal_stride_must_be_0_if_width_is_1)
{
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_0);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_1);
   elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_0);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_1);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);

   EXPECT_FALSE(validate(p));
}

/* If ExecSize = Width = 1, both VertStride and HorzStride must be 0. */
TEST_P(validation_test, scalar_region_must_be_0_1_0)
{
   struct elk_reg g0_0 = elk_vec1_grf(0, 0);

   elk_ADD(p, g0, g0, g0_0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_1);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_1);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_1);
   elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_0);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0_0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_1);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_1);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_1);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_0);

   EXPECT_FALSE(validate(p));
}

/* If VertStride = HorzStride = 0, Width must be 1 regardless of the value
 * of ExecSize.
 */
TEST_P(validation_test, zero_stride_implies_0_1_0)
{
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_0);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_2);
   elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_0);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_0);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_2);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_0);

   EXPECT_FALSE(validate(p));
}

/* Dst.HorzStride must not be 0. */
TEST_P(validation_test, dst_horizontal_stride_0)
{
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_0);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_set_default_access_mode(p, ELK_ALIGN_16);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_0);

   EXPECT_FALSE(validate(p));
}

/* VertStride must be used to cross ELK_GENERAL_REGISTER_FILE register boundaries. This rule implies
 * that elements within a 'Width' cannot cross ELK_GENERAL_REGISTER_FILE boundaries.
 */
TEST_P(validation_test, must_not_cross_grf_boundary_in_a_width)
{
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_src0_da1_subreg_nr(&devinfo, last_inst, 4);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_src1_da1_subreg_nr(&devinfo, last_inst, 4);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_4);
   elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_4);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);

   EXPECT_FALSE(validate(p));
}

/* Destination Horizontal must be 1 in Align16 */
TEST_P(validation_test, dst_hstride_on_align16_must_be_1)
{
   elk_set_default_access_mode(p, ELK_ALIGN_16);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);

   EXPECT_TRUE(validate(p));
}

/* VertStride must be 0 or 4 in Align16 */
TEST_P(validation_test, vstride_on_align16_must_be_0_or_4)
{
   const struct {
      enum elk_vertical_stride vstride;
      bool expected_result;
   } vstride[] = {
      { ELK_VERTICAL_STRIDE_0, true },
      { ELK_VERTICAL_STRIDE_1, false },
      { ELK_VERTICAL_STRIDE_2, devinfo.verx10 >= 75 },
      { ELK_VERTICAL_STRIDE_4, true },
      { ELK_VERTICAL_STRIDE_8, false },
      { ELK_VERTICAL_STRIDE_16, false },
      { ELK_VERTICAL_STRIDE_32, false },
      { ELK_VERTICAL_STRIDE_ONE_DIMENSIONAL, false },
   };

   elk_set_default_access_mode(p, ELK_ALIGN_16);

   for (unsigned i = 0; i < ARRAY_SIZE(vstride); i++) {
      elk_ADD(p, g0, g0, g0);
      elk_inst_set_src0_vstride(&devinfo, last_inst, vstride[i].vstride);

      EXPECT_EQ(vstride[i].expected_result, validate(p));

      clear_instructions(p);
   }

   for (unsigned i = 0; i < ARRAY_SIZE(vstride); i++) {
      elk_ADD(p, g0, g0, g0);
      elk_inst_set_src1_vstride(&devinfo, last_inst, vstride[i].vstride);

      EXPECT_EQ(vstride[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

/* In Direct Addressing mode, a source cannot span more than 2 adjacent ELK_GENERAL_REGISTER_FILE
 * registers.
 */
TEST_P(validation_test, source_cannot_span_more_than_2_registers)
{
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_32);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_16);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_8);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_16);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_16);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_8);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);
   elk_inst_set_src1_da1_subreg_nr(&devinfo, last_inst, 2);

   EXPECT_TRUE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_16);

   EXPECT_TRUE(validate(p));
}

/* A destination cannot span more than 2 adjacent ELK_GENERAL_REGISTER_FILE registers. */
TEST_P(validation_test, destination_cannot_span_more_than_2_registers)
{
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_32);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_8);
   elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, 6);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_4);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_16);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_4);
   elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_16);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_4);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);

   EXPECT_TRUE(validate(p));
}

TEST_P(validation_test, src_region_spans_two_regs_dst_region_spans_one)
{
   /* Writes to dest are to the lower OWord */
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_16);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_4);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);

   EXPECT_TRUE(validate(p));

   clear_instructions(p);

   /* Writes to dest are to the upper OWord */
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, 16);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_16);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_4);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);

   EXPECT_TRUE(validate(p));

   clear_instructions(p);

   /* Writes to dest are evenly split between OWords */
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_16);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_16);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_8);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);

   EXPECT_TRUE(validate(p));

   clear_instructions(p);

   /* Writes to dest are uneven between OWords */
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_4);
   elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, 10);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_4);
   elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_16);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_2);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);

   EXPECT_FALSE(validate(p));
}

TEST_P(validation_test, dst_elements_must_be_evenly_split_between_registers)
{
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, 4);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_16);

   EXPECT_TRUE(validate(p));

   clear_instructions(p);

   if (devinfo.ver >= 6) {
      elk_gfx6_math(p, g0, ELK_MATH_FUNCTION_SIN, g0, null);

      EXPECT_TRUE(validate(p));

      clear_instructions(p);

      elk_gfx6_math(p, g0, ELK_MATH_FUNCTION_SIN, g0, null);
      elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, 4);

      EXPECT_FALSE(validate(p));
   }
}

TEST_P(validation_test, two_src_two_dst_source_offsets_must_be_same)
{
   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_4);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_4);
   elk_inst_set_src0_da1_subreg_nr(&devinfo, last_inst, 16);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_2);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_1);
   elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_0);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_4);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);

  if (devinfo.ver <= 7) {
      EXPECT_FALSE(validate(p));
   } else {
      EXPECT_TRUE(validate(p));
   }

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_4);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_4);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_1);
   elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_0);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_8);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_2);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);

   EXPECT_TRUE(validate(p));
}

TEST_P(validation_test, two_src_two_dst_each_dst_must_be_derived_from_one_src)
{
   elk_MOV(p, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_16);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_da1_subreg_nr(&devinfo, last_inst, 8);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_4);
   elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);

   if (devinfo.ver <= 7) {
      EXPECT_FALSE(validate(p));
   } else {
      EXPECT_TRUE(validate(p));
   }

   clear_instructions(p);

   elk_MOV(p, g0, g0);
   elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, 16);
   elk_inst_set_src0_da1_subreg_nr(&devinfo, last_inst, 8);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_2);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_2);
   elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);

   if (devinfo.ver <= 7) {
      EXPECT_FALSE(validate(p));
   } else {
      EXPECT_TRUE(validate(p));
   }
}

TEST_P(validation_test, one_src_two_dst)
{
   struct elk_reg g0_0 = elk_vec1_grf(0, 0);

   elk_ADD(p, g0, g0_0, g0_0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_16);

   EXPECT_TRUE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_16);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);

   EXPECT_TRUE(validate(p));

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_16);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);

   if (devinfo.ver >= 8) {
      EXPECT_TRUE(validate(p));
   } else {
      EXPECT_FALSE(validate(p));
   }

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_16);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_D);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);

   if (devinfo.ver >= 8) {
      EXPECT_TRUE(validate(p));
   } else {
      EXPECT_FALSE(validate(p));
   }

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_16);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_0);
   elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_1);
   elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_0);

   if (devinfo.ver >= 8) {
      EXPECT_TRUE(validate(p));
   } else {
      EXPECT_FALSE(validate(p));
   }

   clear_instructions(p);

   elk_ADD(p, g0, g0, g0);
   elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_16);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);
   elk_inst_set_dst_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);
   elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_0);
   elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_1);
   elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_0);
   elk_inst_set_src1_file_type(&devinfo, last_inst, ELK_GENERAL_REGISTER_FILE, ELK_REGISTER_TYPE_W);

   if (devinfo.ver >= 8) {
      EXPECT_TRUE(validate(p));
   } else {
      EXPECT_FALSE(validate(p));
   }
}

TEST_P(validation_test, packed_byte_destination)
{
   static const struct {
      enum elk_reg_type dst_type;
      enum elk_reg_type src_type;
      bool neg, abs, sat;
      bool expected_result;
   } move[] = {
      { ELK_REGISTER_TYPE_UB, ELK_REGISTER_TYPE_UB, 0, 0, 0, true },
      { ELK_REGISTER_TYPE_B , ELK_REGISTER_TYPE_B , 0, 0, 0, true },
      { ELK_REGISTER_TYPE_UB, ELK_REGISTER_TYPE_B , 0, 0, 0, true },
      { ELK_REGISTER_TYPE_B , ELK_REGISTER_TYPE_UB, 0, 0, 0, true },

      { ELK_REGISTER_TYPE_UB, ELK_REGISTER_TYPE_UB, 1, 0, 0, false },
      { ELK_REGISTER_TYPE_B , ELK_REGISTER_TYPE_B , 1, 0, 0, false },
      { ELK_REGISTER_TYPE_UB, ELK_REGISTER_TYPE_B , 1, 0, 0, false },
      { ELK_REGISTER_TYPE_B , ELK_REGISTER_TYPE_UB, 1, 0, 0, false },

      { ELK_REGISTER_TYPE_UB, ELK_REGISTER_TYPE_UB, 0, 1, 0, false },
      { ELK_REGISTER_TYPE_B , ELK_REGISTER_TYPE_B , 0, 1, 0, false },
      { ELK_REGISTER_TYPE_UB, ELK_REGISTER_TYPE_B , 0, 1, 0, false },
      { ELK_REGISTER_TYPE_B , ELK_REGISTER_TYPE_UB, 0, 1, 0, false },

      { ELK_REGISTER_TYPE_UB, ELK_REGISTER_TYPE_UB, 0, 0, 1, false },
      { ELK_REGISTER_TYPE_B , ELK_REGISTER_TYPE_B , 0, 0, 1, false },
      { ELK_REGISTER_TYPE_UB, ELK_REGISTER_TYPE_B , 0, 0, 1, false },
      { ELK_REGISTER_TYPE_B , ELK_REGISTER_TYPE_UB, 0, 0, 1, false },

      { ELK_REGISTER_TYPE_UB, ELK_REGISTER_TYPE_UW, 0, 0, 0, false },
      { ELK_REGISTER_TYPE_B , ELK_REGISTER_TYPE_W , 0, 0, 0, false },
      { ELK_REGISTER_TYPE_UB, ELK_REGISTER_TYPE_UD, 0, 0, 0, false },
      { ELK_REGISTER_TYPE_B , ELK_REGISTER_TYPE_D , 0, 0, 0, false },
   };

   for (unsigned i = 0; i < ARRAY_SIZE(move); i++) {
      elk_MOV(p, retype(g0, move[i].dst_type), retype(g0, move[i].src_type));
      elk_inst_set_src0_negate(&devinfo, last_inst, move[i].neg);
      elk_inst_set_src0_abs(&devinfo, last_inst, move[i].abs);
      elk_inst_set_saturate(&devinfo, last_inst, move[i].sat);

      EXPECT_EQ(move[i].expected_result, validate(p));

      clear_instructions(p);
   }

   elk_SEL(p, retype(g0, ELK_REGISTER_TYPE_UB),
              retype(g0, ELK_REGISTER_TYPE_UB),
              retype(g0, ELK_REGISTER_TYPE_UB));
   elk_inst_set_pred_control(&devinfo, last_inst, ELK_PREDICATE_NORMAL);

   EXPECT_FALSE(validate(p));

   clear_instructions(p);

   elk_SEL(p, retype(g0, ELK_REGISTER_TYPE_B),
              retype(g0, ELK_REGISTER_TYPE_B),
              retype(g0, ELK_REGISTER_TYPE_B));
   elk_inst_set_pred_control(&devinfo, last_inst, ELK_PREDICATE_NORMAL);

   EXPECT_FALSE(validate(p));
}

TEST_P(validation_test, byte_destination_relaxed_alignment)
{
   elk_SEL(p, retype(g0, ELK_REGISTER_TYPE_B),
              retype(g0, ELK_REGISTER_TYPE_W),
              retype(g0, ELK_REGISTER_TYPE_W));
   elk_inst_set_pred_control(&devinfo, last_inst, ELK_PREDICATE_NORMAL);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);

   EXPECT_TRUE(validate(p));

   clear_instructions(p);

   elk_SEL(p, retype(g0, ELK_REGISTER_TYPE_B),
              retype(g0, ELK_REGISTER_TYPE_W),
              retype(g0, ELK_REGISTER_TYPE_W));
   elk_inst_set_pred_control(&devinfo, last_inst, ELK_PREDICATE_NORMAL);
   elk_inst_set_dst_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);
   elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, 1);

   if (devinfo.verx10 >= 45) {
      EXPECT_TRUE(validate(p));
   } else {
      EXPECT_FALSE(validate(p));
   }
}

TEST_P(validation_test, byte_64bit_conversion)
{
   static const struct {
      enum elk_reg_type dst_type;
      enum elk_reg_type src_type;
      unsigned dst_stride;
      bool expected_result;
   } inst[] = {
#define INST(dst_type, src_type, dst_stride, expected_result)             \
      {                                                                   \
         ELK_REGISTER_TYPE_##dst_type,                                    \
         ELK_REGISTER_TYPE_##src_type,                                    \
         ELK_HORIZONTAL_STRIDE_##dst_stride,                              \
         expected_result,                                                 \
      }

      INST(B,   Q, 1, false),
      INST(B,  UQ, 1, false),
      INST(B,  DF, 1, false),
      INST(UB,  Q, 1, false),
      INST(UB, UQ, 1, false),
      INST(UB, DF, 1, false),

      INST(B,   Q, 2, false),
      INST(B,  UQ, 2, false),
      INST(B , DF, 2, false),
      INST(UB,  Q, 2, false),
      INST(UB, UQ, 2, false),
      INST(UB, DF, 2, false),

      INST(B,   Q, 4, false),
      INST(B,  UQ, 4, false),
      INST(B,  DF, 4, false),
      INST(UB,  Q, 4, false),
      INST(UB, UQ, 4, false),
      INST(UB, DF, 4, false),

#undef INST
   };

   if (devinfo.ver < 8)
      return;

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      if (!devinfo.has_64bit_float &&
          inst[i].src_type == ELK_REGISTER_TYPE_DF)
         continue;

      if (!devinfo.has_64bit_int &&
          (inst[i].src_type == ELK_REGISTER_TYPE_Q ||
           inst[i].src_type == ELK_REGISTER_TYPE_UQ))
         continue;

      elk_MOV(p, retype(g0, inst[i].dst_type), retype(g0, inst[i].src_type));
      elk_inst_set_dst_hstride(&devinfo, last_inst, inst[i].dst_stride);
      EXPECT_EQ(inst[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, half_float_conversion)
{
   static const struct {
      enum elk_reg_type dst_type;
      enum elk_reg_type src_type;
      unsigned dst_stride;
      unsigned dst_subnr;
      bool expected_result_bdw;
      bool expected_result_chv;
   } inst[] = {
#define INST(dst_type, src_type, dst_stride, dst_subnr,                     \
             expected_result_bdw, expected_result_chv)                      \
      {                                                                     \
         ELK_REGISTER_TYPE_##dst_type,                                      \
         ELK_REGISTER_TYPE_##src_type,                                      \
         ELK_HORIZONTAL_STRIDE_##dst_stride,                                \
         dst_subnr,                                                         \
         expected_result_bdw,                                               \
         expected_result_chv,                                               \
      }

      /* MOV to half-float destination */
      INST(HF,  B, 1, 0, false, false), /* 0 */
      INST(HF,  W, 1, 0, false, false),
      INST(HF, HF, 1, 0, true,  true),
      INST(HF, HF, 1, 2, true,  true),
      INST(HF,  D, 1, 0, false, false),
      INST(HF,  F, 1, 0, false, true),
      INST(HF,  Q, 1, 0, false, false),
      INST(HF,  B, 2, 0, true,  true),
      INST(HF,  B, 2, 2, false, false),
      INST(HF,  W, 2, 0, true,  true),
      INST(HF,  W, 2, 2, false, false), /* 10 */
      INST(HF, HF, 2, 0, true,  true),
      INST(HF, HF, 2, 2, true,  true),
      INST(HF,  D, 2, 0, true,  true),
      INST(HF,  D, 2, 2, false, false),
      INST(HF,  F, 2, 0, true,  true),
      INST(HF,  F, 2, 2, false, true),
      INST(HF,  Q, 2, 0, false, false),
      INST(HF, DF, 2, 0, false, false),
      INST(HF,  B, 4, 0, false, false),
      INST(HF,  W, 4, 0, false, false), /* 20 */
      INST(HF, HF, 4, 0, true,  true),
      INST(HF, HF, 4, 2, true,  true),
      INST(HF,  D, 4, 0, false, false),
      INST(HF,  F, 4, 0, false, false),
      INST(HF,  Q, 4, 0, false, false),
      INST(HF, DF, 4, 0, false, false),

      /* MOV from half-float source */
      INST( B, HF, 1, 0, false, false),
      INST( W, HF, 1, 0, false, false),
      INST( D, HF, 1, 0, true,  true),
      INST( D, HF, 1, 4, true,  true),  /* 30 */
      INST( F, HF, 1, 0, true,  true),
      INST( F, HF, 1, 4, true,  true),
      INST( Q, HF, 1, 0, false, false),
      INST(DF, HF, 1, 0, false, false),
      INST( B, HF, 2, 0, false, false),
      INST( W, HF, 2, 0, true,  true),
      INST( W, HF, 2, 2, false, false),
      INST( D, HF, 2, 0, false, false),
      INST( F, HF, 2, 0, true,  true),
      INST( B, HF, 4, 0, true,  true),  /* 40 */
      INST( B, HF, 4, 1, false, false),
      INST( W, HF, 4, 0, false, false),

#undef INST
   };

   if (devinfo.ver < 8)
      return;

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      if (!devinfo.has_64bit_float &&
          (inst[i].dst_type == ELK_REGISTER_TYPE_DF ||
           inst[i].src_type == ELK_REGISTER_TYPE_DF))
         continue;

      if (!devinfo.has_64bit_int &&
          (inst[i].dst_type == ELK_REGISTER_TYPE_Q ||
           inst[i].dst_type == ELK_REGISTER_TYPE_UQ ||
           inst[i].src_type == ELK_REGISTER_TYPE_Q ||
           inst[i].src_type == ELK_REGISTER_TYPE_UQ))
         continue;

      elk_MOV(p, retype(g0, inst[i].dst_type), retype(g0, inst[i].src_type));

      elk_inst_set_exec_size(&devinfo, last_inst, ELK_EXECUTE_4);

      elk_inst_set_dst_hstride(&devinfo, last_inst, inst[i].dst_stride);
      elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, inst[i].dst_subnr);

      if (inst[i].src_type == ELK_REGISTER_TYPE_B) {
         elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
         elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_2);
         elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_2);
      } else {
         elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
         elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_4);
         elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);
      }

      if (devinfo.platform == INTEL_PLATFORM_CHV) {
         EXPECT_EQ(inst[i].expected_result_chv, validate(p)) <<
            "Failing test is: " << i;
      } else {
         EXPECT_EQ(inst[i].expected_result_bdw, validate(p)) <<
            "Failing test is: " << i;
      }

      clear_instructions(p);
   }
}

TEST_P(validation_test, mixed_float_source_indirect_addressing)
{
   static const struct {
      enum elk_reg_type dst_type;
      enum elk_reg_type src0_type;
      enum elk_reg_type src1_type;
      unsigned dst_stride;
      bool dst_indirect;
      bool src0_indirect;
      bool expected_result;
      bool gfx125_expected_result;
   } inst[] = {
#define INST(dst_type, src0_type, src1_type,                              \
             dst_stride, dst_indirect, src0_indirect, expected_result,    \
             gfx125_expected_result)                                      \
      {                                                                   \
         ELK_REGISTER_TYPE_##dst_type,                                    \
         ELK_REGISTER_TYPE_##src0_type,                                   \
         ELK_REGISTER_TYPE_##src1_type,                                   \
         ELK_HORIZONTAL_STRIDE_##dst_stride,                              \
         dst_indirect,                                                    \
         src0_indirect,                                                   \
         expected_result,                                                 \
         gfx125_expected_result,                                          \
      }

      /* Source and dest are mixed float: indirect src addressing not allowed */
      INST(HF,  F,  F, 2, false, false, true,  true),
      INST(HF,  F,  F, 2, true,  false, true,  true),
      INST(HF,  F,  F, 2, false, true,  false, false),
      INST(HF,  F,  F, 2, true,  true,  false, false),
      INST( F, HF,  F, 1, false, false, true,  false),
      INST( F, HF,  F, 1, true,  false, true,  false),
      INST( F, HF,  F, 1, false, true,  false, false),
      INST( F, HF,  F, 1, true,  true,  false, false),

      INST(HF, HF,  F, 2, false, false, true,  false),
      INST(HF, HF,  F, 2, true,  false, true,  false),
      INST(HF, HF,  F, 2, false, true,  false, false),
      INST(HF, HF,  F, 2, true,  true,  false, false),
      INST( F,  F, HF, 1, false, false, true,  false),
      INST( F,  F, HF, 1, true,  false, true,  false),
      INST( F,  F, HF, 1, false, true,  false, false),
      INST( F,  F, HF, 1, true,  true,  false, false),

#undef INST
   };

   if (devinfo.ver < 8)
      return;

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      elk_ADD(p, retype(g0, inst[i].dst_type),
                 retype(g0, inst[i].src0_type),
                 retype(g0, inst[i].src1_type));

      elk_inst_set_dst_address_mode(&devinfo, last_inst, inst[i].dst_indirect);
      elk_inst_set_dst_hstride(&devinfo, last_inst, inst[i].dst_stride);
      elk_inst_set_src0_address_mode(&devinfo, last_inst, inst[i].src0_indirect);

      EXPECT_EQ(inst[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, mixed_float_align1_simd16)
{
   static const struct {
      unsigned exec_size;
      enum elk_reg_type dst_type;
      enum elk_reg_type src0_type;
      enum elk_reg_type src1_type;
      unsigned dst_stride;
      bool expected_result;
      bool gfx125_expected_result;
   } inst[] = {
#define INST(exec_size, dst_type, src0_type, src1_type,                   \
             dst_stride, expected_result, gfx125_expected_result)         \
      {                                                                   \
         ELK_EXECUTE_##exec_size,                                         \
         ELK_REGISTER_TYPE_##dst_type,                                    \
         ELK_REGISTER_TYPE_##src0_type,                                   \
         ELK_REGISTER_TYPE_##src1_type,                                   \
         ELK_HORIZONTAL_STRIDE_##dst_stride,                              \
         expected_result,                                                 \
         gfx125_expected_result,                                          \
      }

      /* No SIMD16 in mixed mode when destination is packed f16 */
      INST( 8, HF,  F, HF, 2, true,  false),
      INST(16, HF, HF,  F, 2, true,  false),
      INST(16, HF, HF,  F, 1, false, false),
      INST(16, HF,  F, HF, 1, false, false),

      /* No SIMD16 in mixed mode when destination is f32 */
      INST( 8,  F, HF,  F, 1, true,  false),
      INST( 8,  F,  F, HF, 1, true,  false),
      INST(16,  F, HF,  F, 1, false, false),
      INST(16,  F,  F, HF, 1, false, false),

#undef INST
   };

   if (devinfo.ver < 8)
      return;

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      elk_ADD(p, retype(g0, inst[i].dst_type),
                 retype(g0, inst[i].src0_type),
                 retype(g0, inst[i].src1_type));

      elk_inst_set_exec_size(&devinfo, last_inst, inst[i].exec_size);

      elk_inst_set_dst_hstride(&devinfo, last_inst, inst[i].dst_stride);

      EXPECT_EQ(inst[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, mixed_float_align1_packed_fp16_dst_acc_read_offset_0)
{
   static const struct {
      enum elk_reg_type dst_type;
      enum elk_reg_type src0_type;
      enum elk_reg_type src1_type;
      unsigned dst_stride;
      bool read_acc;
      unsigned subnr;
      bool expected_result_bdw;
      bool expected_result_chv_skl;
      bool expected_result_gfx125;
   } inst[] = {
#define INST(dst_type, src0_type, src1_type, dst_stride, read_acc, subnr,   \
             expected_result_bdw, expected_result_chv_skl,                  \
             expected_result_gfx125)                                        \
      {                                                                     \
         ELK_REGISTER_TYPE_##dst_type,                                      \
         ELK_REGISTER_TYPE_##src0_type,                                     \
         ELK_REGISTER_TYPE_##src1_type,                                     \
         ELK_HORIZONTAL_STRIDE_##dst_stride,                                \
         read_acc,                                                          \
         subnr,                                                             \
         expected_result_bdw,                                               \
         expected_result_chv_skl,                                           \
         expected_result_gfx125,                                            \
      }

      /* Destination is not packed */
      INST(HF, HF,  F, 2, true,  0, true, true, false),
      INST(HF, HF,  F, 2, true,  2, true, true, false),
      INST(HF, HF,  F, 2, true,  4, true, true, false),
      INST(HF, HF,  F, 2, true,  8, true, true, false),
      INST(HF, HF,  F, 2, true, 16, true, true, false),

      /* Destination is packed, we don't read acc */
      INST(HF, HF,  F, 1, false,  0, false, true, false),
      INST(HF, HF,  F, 1, false,  2, false, true, false),
      INST(HF, HF,  F, 1, false,  4, false, true, false),
      INST(HF, HF,  F, 1, false,  8, false, true, false),
      INST(HF, HF,  F, 1, false, 16, false, true, false),

      /* Destination is packed, we read acc */
      INST(HF, HF,  F, 1, true,  0, false, false, false),
      INST(HF, HF,  F, 1, true,  2, false, false, false),
      INST(HF, HF,  F, 1, true,  4, false, false, false),
      INST(HF, HF,  F, 1, true,  8, false, false, false),
      INST(HF, HF,  F, 1, true, 16, false, false, false),

#undef INST
   };

   if (devinfo.ver < 8)
      return;

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      elk_ADD(p, retype(g0, inst[i].dst_type),
                 retype(inst[i].read_acc ? acc0 : g0, inst[i].src0_type),
                 retype(g0, inst[i].src1_type));

      elk_inst_set_dst_hstride(&devinfo, last_inst, inst[i].dst_stride);

      elk_inst_set_src0_da1_subreg_nr(&devinfo, last_inst, inst[i].subnr);

      if (devinfo.verx10 >= 125)
         EXPECT_EQ(inst[i].expected_result_gfx125, validate(p));
      else if (devinfo.platform == INTEL_PLATFORM_CHV || devinfo.ver >= 9)
         EXPECT_EQ(inst[i].expected_result_chv_skl, validate(p));
      else
         EXPECT_EQ(inst[i].expected_result_bdw, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, mixed_float_fp16_dest_with_acc)
{
   static const struct {
      unsigned exec_size;
      unsigned opcode;
      enum elk_reg_type dst_type;
      enum elk_reg_type src0_type;
      enum elk_reg_type src1_type;
      unsigned dst_stride;
      bool read_acc;
      bool expected_result_bdw;
      bool expected_result_chv_skl;
      bool expected_result_gfx125;
   } inst[] = {
#define INST(exec_size, opcode, dst_type, src0_type, src1_type,           \
             dst_stride, read_acc,expected_result_bdw,                    \
             expected_result_chv_skl, expected_result_gfx125)             \
      {                                                                   \
         ELK_EXECUTE_##exec_size,                                         \
         ELK_OPCODE_##opcode,                                             \
         ELK_REGISTER_TYPE_##dst_type,                                    \
         ELK_REGISTER_TYPE_##src0_type,                                   \
         ELK_REGISTER_TYPE_##src1_type,                                   \
         ELK_HORIZONTAL_STRIDE_##dst_stride,                              \
         read_acc,                                                        \
         expected_result_bdw,                                             \
         expected_result_chv_skl,                                         \
         expected_result_gfx125,                                          \
      }

      /* Packed fp16 dest with implicit acc needs hstride=2 */
      INST(8, MAC, HF, HF,  F, 1, false, false, false, false),
      INST(8, MAC, HF, HF,  F, 2, false, true,  true,  false),
      INST(8, MAC, HF,  F, HF, 1, false, false, false, false),
      INST(8, MAC, HF,  F, HF, 2, false, true,  true,  false),

      /* Packed fp16 dest with explicit acc needs hstride=2 */
      INST(8, ADD, HF, HF,  F, 1, true,  false, false, false),
      INST(8, ADD, HF, HF,  F, 2, true,  true,  true,  false),
      INST(8, ADD, HF,  F, HF, 1, true,  false, false, false),
      INST(8, ADD, HF,  F, HF, 2, true,  true,  true,  false),

      /* If destination is not fp16, restriction doesn't apply */
      INST(8, MAC,  F, HF,  F, 1, false, true, true, false),
      INST(8, MAC,  F, HF,  F, 2, false, true, true, false),

      /* If there is no implicit/explicit acc, restriction doesn't apply */
      INST(8, ADD, HF, HF,  F, 1, false, false, true, false),
      INST(8, ADD, HF, HF,  F, 2, false, true,  true, false),
      INST(8, ADD, HF,  F, HF, 1, false, false, true, false),
      INST(8, ADD, HF,  F, HF, 2, false, true,  true, false),
      INST(8, ADD,  F, HF,  F, 1, false, true,  true, false),
      INST(8, ADD,  F, HF,  F, 2, false, true,  true, false),

#undef INST
   };

   if (devinfo.ver < 8)
      return;

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      if (inst[i].opcode == ELK_OPCODE_MAC) {
         elk_MAC(p, retype(g0, inst[i].dst_type),
                    retype(g0, inst[i].src0_type),
                    retype(g0, inst[i].src1_type));
      } else {
         assert(inst[i].opcode == ELK_OPCODE_ADD);
         elk_ADD(p, retype(g0, inst[i].dst_type),
                    retype(inst[i].read_acc ? acc0: g0, inst[i].src0_type),
                    retype(g0, inst[i].src1_type));
      }

      elk_inst_set_exec_size(&devinfo, last_inst, inst[i].exec_size);

      elk_inst_set_dst_hstride(&devinfo, last_inst, inst[i].dst_stride);

      if (devinfo.verx10 >= 125)
         EXPECT_EQ(inst[i].expected_result_gfx125, validate(p));
      else if (devinfo.platform == INTEL_PLATFORM_CHV || devinfo.ver >= 9)
         EXPECT_EQ(inst[i].expected_result_chv_skl, validate(p));
      else
         EXPECT_EQ(inst[i].expected_result_bdw, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, mixed_float_align1_math_strided_fp16_inputs)
{
   static const struct {
      enum elk_reg_type dst_type;
      enum elk_reg_type src0_type;
      enum elk_reg_type src1_type;
      unsigned dst_stride;
      unsigned src0_stride;
      unsigned src1_stride;
      bool expected_result;
      bool expected_result_gfx125;
   } inst[] = {
#define INST(dst_type, src0_type, src1_type,                              \
             dst_stride, src0_stride, src1_stride, expected_result,       \
             expected_result_125)                                         \
      {                                                                   \
         ELK_REGISTER_TYPE_##dst_type,                                    \
         ELK_REGISTER_TYPE_##src0_type,                                   \
         ELK_REGISTER_TYPE_##src1_type,                                   \
         ELK_HORIZONTAL_STRIDE_##dst_stride,                              \
         ELK_HORIZONTAL_STRIDE_##src0_stride,                             \
         ELK_HORIZONTAL_STRIDE_##src1_stride,                             \
         expected_result,                                                 \
         expected_result_125,                                             \
      }

      INST(HF, HF,  F, 2, 2, 1, true,  false),
      INST(HF,  F, HF, 2, 1, 2, true,  false),
      INST(HF,  F, HF, 1, 1, 2, true,  false),
      INST(HF,  F, HF, 2, 1, 1, false, false),
      INST(HF, HF,  F, 2, 1, 1, false, false),
      INST(HF, HF,  F, 1, 1, 1, false, false),
      INST(HF, HF,  F, 2, 1, 1, false, false),
      INST( F, HF,  F, 1, 1, 1, false, false),
      INST( F,  F, HF, 1, 1, 2, true,  false),
      INST( F, HF, HF, 1, 2, 1, false, false),
      INST( F, HF, HF, 1, 2, 2, true,  false),

#undef INST
   };

   /* No half-float math in gfx8 */
   if (devinfo.ver < 9)
      return;

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      elk_gfx6_math(p, retype(g0, inst[i].dst_type),
                   ELK_MATH_FUNCTION_POW,
                   retype(g0, inst[i].src0_type),
                   retype(g0, inst[i].src1_type));

      elk_inst_set_dst_hstride(&devinfo, last_inst, inst[i].dst_stride);

      elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
      elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_4);
      elk_inst_set_src0_hstride(&devinfo, last_inst, inst[i].src0_stride);

      elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
      elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_4);
      elk_inst_set_src1_hstride(&devinfo, last_inst, inst[i].src1_stride);

      if (devinfo.verx10 >= 125)
         EXPECT_EQ(inst[i].expected_result_gfx125, validate(p));
      else
         EXPECT_EQ(inst[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, mixed_float_align1_packed_fp16_dst)
{
   static const struct {
      unsigned exec_size;
      enum elk_reg_type dst_type;
      enum elk_reg_type src0_type;
      enum elk_reg_type src1_type;
      unsigned dst_stride;
      unsigned dst_subnr;
      bool expected_result_bdw;
      bool expected_result_chv_skl;
      bool expected_result_gfx125;
   } inst[] = {
#define INST(exec_size, dst_type, src0_type, src1_type, dst_stride, dst_subnr, \
             expected_result_bdw, expected_result_chv_skl,                     \
             expected_result_gfx125)                                           \
      {                                                                        \
         ELK_EXECUTE_##exec_size,                                              \
         ELK_REGISTER_TYPE_##dst_type,                                         \
         ELK_REGISTER_TYPE_##src0_type,                                        \
         ELK_REGISTER_TYPE_##src1_type,                                        \
         ELK_HORIZONTAL_STRIDE_##dst_stride,                                   \
         dst_subnr,                                                            \
         expected_result_bdw,                                                  \
         expected_result_chv_skl,                                              \
         expected_result_gfx125                                                \
      }

      /* SIMD8 packed fp16 dst won't cross oword boundaries if region is
       * oword-aligned
       */
      INST( 8, HF, HF,  F, 1,  0, false, true,  false),
      INST( 8, HF, HF,  F, 1,  2, false, false, false),
      INST( 8, HF, HF,  F, 1,  4, false, false, false),
      INST( 8, HF, HF,  F, 1,  8, false, false, false),
      INST( 8, HF, HF,  F, 1, 16, false, true,  false),

      /* SIMD16 packed fp16 always crosses oword boundaries */
      INST(16, HF, HF,  F, 1,  0, false, false, false),
      INST(16, HF, HF,  F, 1,  2, false, false, false),
      INST(16, HF, HF,  F, 1,  4, false, false, false),
      INST(16, HF, HF,  F, 1,  8, false, false, false),
      INST(16, HF, HF,  F, 1, 16, false, false, false),

      /* If destination is not packed (or not fp16) we can cross oword
       * boundaries
       */
      INST( 8, HF, HF,  F, 2,  0, true, true, false),
      INST( 8,  F, HF,  F, 1,  0, true, true, false),

#undef INST
   };

   if (devinfo.ver < 8)
      return;

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      elk_ADD(p, retype(g0, inst[i].dst_type),
                 retype(g0, inst[i].src0_type),
                 retype(g0, inst[i].src1_type));

      elk_inst_set_dst_hstride(&devinfo, last_inst, inst[i].dst_stride);
      elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, inst[i].dst_subnr);

      elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
      elk_inst_set_src0_width(&devinfo, last_inst, ELK_WIDTH_4);
      elk_inst_set_src0_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);

      elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
      elk_inst_set_src1_width(&devinfo, last_inst, ELK_WIDTH_4);
      elk_inst_set_src1_hstride(&devinfo, last_inst, ELK_HORIZONTAL_STRIDE_1);

      elk_inst_set_exec_size(&devinfo, last_inst, inst[i].exec_size);

      if (devinfo.verx10 >= 125)
         EXPECT_EQ(inst[i].expected_result_gfx125, validate(p));
      else if (devinfo.platform == INTEL_PLATFORM_CHV || devinfo.ver >= 9)
         EXPECT_EQ(inst[i].expected_result_chv_skl, validate(p));
      else
         EXPECT_EQ(inst[i].expected_result_bdw, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, mixed_float_align16_packed_data)
{
   static const struct {
      enum elk_reg_type dst_type;
      enum elk_reg_type src0_type;
      enum elk_reg_type src1_type;
      unsigned src0_vstride;
      unsigned src1_vstride;
      bool expected_result;
   } inst[] = {
#define INST(dst_type, src0_type, src1_type,                              \
             src0_vstride, src1_vstride, expected_result)                 \
      {                                                                   \
         ELK_REGISTER_TYPE_##dst_type,                                    \
         ELK_REGISTER_TYPE_##src0_type,                                   \
         ELK_REGISTER_TYPE_##src1_type,                                   \
         ELK_VERTICAL_STRIDE_##src0_vstride,                              \
         ELK_VERTICAL_STRIDE_##src1_vstride,                              \
         expected_result,                                                 \
      }

      /* We only test with F destination because there is a restriction
       * by which F->HF conversions need to be DWord aligned but Align16 also
       * requires that destination horizontal stride is 1.
       */
      INST(F,  F, HF, 4, 4, true),
      INST(F,  F, HF, 2, 4, false),
      INST(F,  F, HF, 4, 2, false),
      INST(F,  F, HF, 0, 4, false),
      INST(F,  F, HF, 4, 0, false),
      INST(F, HF,  F, 4, 4, true),
      INST(F, HF,  F, 4, 2, false),
      INST(F, HF,  F, 2, 4, false),
      INST(F, HF,  F, 0, 4, false),
      INST(F, HF,  F, 4, 0, false),

#undef INST
   };

   if (devinfo.ver < 8 || devinfo.ver >= 11)
      return;

   elk_set_default_access_mode(p, ELK_ALIGN_16);

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      elk_ADD(p, retype(g0, inst[i].dst_type),
                 retype(g0, inst[i].src0_type),
                 retype(g0, inst[i].src1_type));

      elk_inst_set_src0_vstride(&devinfo, last_inst, inst[i].src0_vstride);
      elk_inst_set_src1_vstride(&devinfo, last_inst, inst[i].src1_vstride);

      EXPECT_EQ(inst[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, mixed_float_align16_no_simd16)
{
   static const struct {
      unsigned exec_size;
      enum elk_reg_type dst_type;
      enum elk_reg_type src0_type;
      enum elk_reg_type src1_type;
      bool expected_result;
   } inst[] = {
#define INST(exec_size, dst_type, src0_type, src1_type, expected_result)  \
      {                                                                   \
         ELK_EXECUTE_##exec_size,                                         \
         ELK_REGISTER_TYPE_##dst_type,                                    \
         ELK_REGISTER_TYPE_##src0_type,                                   \
         ELK_REGISTER_TYPE_##src1_type,                                   \
         expected_result,                                                 \
      }

      /* We only test with F destination because there is a restriction
       * by which F->HF conversions need to be DWord aligned but Align16 also
       * requires that destination horizontal stride is 1.
       */
      INST( 8,  F,  F, HF, true),
      INST( 8,  F, HF,  F, true),
      INST( 8,  F,  F, HF, true),
      INST(16,  F,  F, HF, false),
      INST(16,  F, HF,  F, false),
      INST(16,  F,  F, HF, false),

#undef INST
   };

   if (devinfo.ver < 8 || devinfo.ver >= 11)
      return;

   elk_set_default_access_mode(p, ELK_ALIGN_16);

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      elk_ADD(p, retype(g0, inst[i].dst_type),
                 retype(g0, inst[i].src0_type),
                 retype(g0, inst[i].src1_type));

      elk_inst_set_exec_size(&devinfo, last_inst, inst[i].exec_size);

      elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
      elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);

      EXPECT_EQ(inst[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, mixed_float_align16_no_acc_read)
{
   static const struct {
      enum elk_reg_type dst_type;
      enum elk_reg_type src0_type;
      enum elk_reg_type src1_type;
      bool read_acc;
      bool expected_result;
   } inst[] = {
#define INST(dst_type, src0_type, src1_type, read_acc, expected_result)   \
      {                                                                   \
         ELK_REGISTER_TYPE_##dst_type,                                    \
         ELK_REGISTER_TYPE_##src0_type,                                   \
         ELK_REGISTER_TYPE_##src1_type,                                   \
         read_acc,                                                        \
         expected_result,                                                 \
      }

      /* We only test with F destination because there is a restriction
       * by which F->HF conversions need to be DWord aligned but Align16 also
       * requires that destination horizontal stride is 1.
       */
      INST( F,  F, HF, false, true),
      INST( F,  F, HF, true,  false),
      INST( F, HF,  F, false, true),
      INST( F, HF,  F, true,  false),

#undef INST
   };

   if (devinfo.ver < 8 || devinfo.ver >= 11)
      return;

   elk_set_default_access_mode(p, ELK_ALIGN_16);

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      elk_ADD(p, retype(g0, inst[i].dst_type),
                 retype(inst[i].read_acc ? acc0 : g0, inst[i].src0_type),
                 retype(g0, inst[i].src1_type));

      elk_inst_set_src0_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);
      elk_inst_set_src1_vstride(&devinfo, last_inst, ELK_VERTICAL_STRIDE_4);

      EXPECT_EQ(inst[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, mixed_float_align16_math_packed_format)
{
   static const struct {
      enum elk_reg_type dst_type;
      enum elk_reg_type src0_type;
      enum elk_reg_type src1_type;
      unsigned src0_vstride;
      unsigned src1_vstride;
      bool expected_result;
   } inst[] = {
#define INST(dst_type, src0_type, src1_type,                              \
             src0_vstride, src1_vstride, expected_result)                 \
      {                                                                   \
         ELK_REGISTER_TYPE_##dst_type,                                    \
         ELK_REGISTER_TYPE_##src0_type,                                   \
         ELK_REGISTER_TYPE_##src1_type,                                   \
         ELK_VERTICAL_STRIDE_##src0_vstride,                              \
         ELK_VERTICAL_STRIDE_##src1_vstride,                              \
         expected_result,                                                 \
      }

      /* We only test with F destination because there is a restriction
       * by which F->HF conversions need to be DWord aligned but Align16 also
       * requires that destination horizontal stride is 1.
       */
      INST( F, HF,  F, 4, 0, false),
      INST( F, HF, HF, 4, 4, true),
      INST( F,  F, HF, 4, 0, false),
      INST( F,  F, HF, 2, 4, false),
      INST( F,  F, HF, 4, 2, false),
      INST( F, HF, HF, 0, 4, false),

#undef INST
   };

   /* Align16 Math for mixed float mode is not supported in gfx8 */
   if (devinfo.ver < 9 || devinfo.ver >= 11)
      return;

   elk_set_default_access_mode(p, ELK_ALIGN_16);

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      elk_gfx6_math(p, retype(g0, inst[i].dst_type),
                   ELK_MATH_FUNCTION_POW,
                   retype(g0, inst[i].src0_type),
                   retype(g0, inst[i].src1_type));

      elk_inst_set_src0_vstride(&devinfo, last_inst, inst[i].src0_vstride);
      elk_inst_set_src1_vstride(&devinfo, last_inst, inst[i].src1_vstride);

      EXPECT_EQ(inst[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, vector_immediate_destination_alignment)
{
   static const struct {
      enum elk_reg_type dst_type;
      enum elk_reg_type src_type;
      unsigned subnr;
      unsigned exec_size;
      bool expected_result;
   } move[] = {
      { ELK_REGISTER_TYPE_F, ELK_REGISTER_TYPE_VF,  0, ELK_EXECUTE_4, true  },
      { ELK_REGISTER_TYPE_F, ELK_REGISTER_TYPE_VF, 16, ELK_EXECUTE_4, true  },
      { ELK_REGISTER_TYPE_F, ELK_REGISTER_TYPE_VF,  1, ELK_EXECUTE_4, false },

      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_V,   0, ELK_EXECUTE_8, true  },
      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_V,  16, ELK_EXECUTE_8, true  },
      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_V,   1, ELK_EXECUTE_8, false },

      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_UV,  0, ELK_EXECUTE_8, true  },
      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_UV, 16, ELK_EXECUTE_8, true  },
      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_UV,  1, ELK_EXECUTE_8, false },
   };

   for (unsigned i = 0; i < ARRAY_SIZE(move); i++) {
      /* UV type is Gfx6+ */
      if (devinfo.ver < 6 &&
          move[i].src_type == ELK_REGISTER_TYPE_UV)
         continue;

      elk_MOV(p, retype(g0, move[i].dst_type), retype(zero, move[i].src_type));
      elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, move[i].subnr);
      elk_inst_set_exec_size(&devinfo, last_inst, move[i].exec_size);

      EXPECT_EQ(move[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, vector_immediate_destination_stride)
{
   static const struct {
      enum elk_reg_type dst_type;
      enum elk_reg_type src_type;
      unsigned stride;
      bool expected_result;
   } move[] = {
      { ELK_REGISTER_TYPE_F, ELK_REGISTER_TYPE_VF, ELK_HORIZONTAL_STRIDE_1, true  },
      { ELK_REGISTER_TYPE_F, ELK_REGISTER_TYPE_VF, ELK_HORIZONTAL_STRIDE_2, false },
      { ELK_REGISTER_TYPE_D, ELK_REGISTER_TYPE_VF, ELK_HORIZONTAL_STRIDE_1, true  },
      { ELK_REGISTER_TYPE_D, ELK_REGISTER_TYPE_VF, ELK_HORIZONTAL_STRIDE_2, false },
      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_VF, ELK_HORIZONTAL_STRIDE_2, true  },
      { ELK_REGISTER_TYPE_B, ELK_REGISTER_TYPE_VF, ELK_HORIZONTAL_STRIDE_4, true  },

      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_V,  ELK_HORIZONTAL_STRIDE_1, true  },
      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_V,  ELK_HORIZONTAL_STRIDE_2, false },
      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_V,  ELK_HORIZONTAL_STRIDE_4, false },
      { ELK_REGISTER_TYPE_B, ELK_REGISTER_TYPE_V,  ELK_HORIZONTAL_STRIDE_2, true  },

      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_UV, ELK_HORIZONTAL_STRIDE_1, true  },
      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_UV, ELK_HORIZONTAL_STRIDE_2, false },
      { ELK_REGISTER_TYPE_W, ELK_REGISTER_TYPE_UV, ELK_HORIZONTAL_STRIDE_4, false },
      { ELK_REGISTER_TYPE_B, ELK_REGISTER_TYPE_UV, ELK_HORIZONTAL_STRIDE_2, true  },
   };

   for (unsigned i = 0; i < ARRAY_SIZE(move); i++) {
      /* UV type is Gfx6+ */
      if (devinfo.ver < 6 &&
          move[i].src_type == ELK_REGISTER_TYPE_UV)
         continue;

      elk_MOV(p, retype(g0, move[i].dst_type), retype(zero, move[i].src_type));
      elk_inst_set_dst_hstride(&devinfo, last_inst, move[i].stride);

      EXPECT_EQ(move[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, qword_low_power_align1_regioning_restrictions)
{
   static const struct {
      enum elk_opcode opcode;
      unsigned exec_size;

      enum elk_reg_type dst_type;
      unsigned dst_subreg;
      unsigned dst_stride;

      enum elk_reg_type src_type;
      unsigned src_subreg;
      unsigned src_vstride;
      unsigned src_width;
      unsigned src_hstride;

      bool expected_result;
   } inst[] = {
#define INST(opcode, exec_size, dst_type, dst_subreg, dst_stride, src_type,    \
             src_subreg, src_vstride, src_width, src_hstride, expected_result) \
      {                                                                        \
         ELK_OPCODE_##opcode,                                                  \
         ELK_EXECUTE_##exec_size,                                              \
         ELK_REGISTER_TYPE_##dst_type,                                         \
         dst_subreg,                                                           \
         ELK_HORIZONTAL_STRIDE_##dst_stride,                                   \
         ELK_REGISTER_TYPE_##src_type,                                         \
         src_subreg,                                                           \
         ELK_VERTICAL_STRIDE_##src_vstride,                                    \
         ELK_WIDTH_##src_width,                                                \
         ELK_HORIZONTAL_STRIDE_##src_hstride,                                  \
         expected_result,                                                      \
      }

      /* Some instruction that violate no restrictions, as a control */
      INST(MOV, 4, DF, 0, 1, DF, 0, 4, 4, 1, true ),
      INST(MOV, 4, Q,  0, 1, Q,  0, 4, 4, 1, true ),
      INST(MOV, 4, UQ, 0, 1, UQ, 0, 4, 4, 1, true ),

      INST(MOV, 4, DF, 0, 1, F,  0, 8, 4, 2, true ),
      INST(MOV, 4, Q,  0, 1, D,  0, 8, 4, 2, true ),
      INST(MOV, 4, UQ, 0, 1, UD, 0, 8, 4, 2, true ),

      INST(MOV, 4, F,  0, 2, DF, 0, 4, 4, 1, true ),
      INST(MOV, 4, D,  0, 2, Q,  0, 4, 4, 1, true ),
      INST(MOV, 4, UD, 0, 2, UQ, 0, 4, 4, 1, true ),

      INST(MUL, 8, D,  0, 2, D,  0, 8, 4, 2, true ),
      INST(MUL, 8, UD, 0, 2, UD, 0, 8, 4, 2, true ),

      /* Something with subreg nrs */
      INST(MOV, 2, DF, 8, 1, DF, 8, 2, 2, 1, true ),
      INST(MOV, 2, Q,  8, 1, Q,  8, 2, 2, 1, true ),
      INST(MOV, 2, UQ, 8, 1, UQ, 8, 2, 2, 1, true ),

      INST(MUL, 2, D,  4, 2, D,  4, 4, 2, 2, true ),
      INST(MUL, 2, UD, 4, 2, UD, 4, 4, 2, 2, true ),

      /* The PRMs say that for CHV, BXT:
       *
       *    When source or destination datatype is 64b or operation is integer
       *    DWord multiply, regioning in Align1 must follow these rules:
       *
       *    1. Source and Destination horizontal stride must be aligned to the
       *       same qword.
       */
      INST(MOV, 4, DF, 0, 2, DF, 0, 4, 4, 1, false),
      INST(MOV, 4, Q,  0, 2, Q,  0, 4, 4, 1, false),
      INST(MOV, 4, UQ, 0, 2, UQ, 0, 4, 4, 1, false),

      INST(MOV, 4, DF, 0, 2, F,  0, 8, 4, 2, false),
      INST(MOV, 4, Q,  0, 2, D,  0, 8, 4, 2, false),
      INST(MOV, 4, UQ, 0, 2, UD, 0, 8, 4, 2, false),

      INST(MOV, 4, DF, 0, 2, F,  0, 4, 4, 1, false),
      INST(MOV, 4, Q,  0, 2, D,  0, 4, 4, 1, false),
      INST(MOV, 4, UQ, 0, 2, UD, 0, 4, 4, 1, false),

      INST(MUL, 4, D,  0, 2, D,  0, 4, 4, 1, false),
      INST(MUL, 4, UD, 0, 2, UD, 0, 4, 4, 1, false),

      INST(MUL, 4, D,  0, 1, D,  0, 8, 4, 2, false),
      INST(MUL, 4, UD, 0, 1, UD, 0, 8, 4, 2, false),

      /*    2. Regioning must ensure Src.Vstride = Src.Width * Src.Hstride. */
      INST(MOV, 4, DF, 0, 1, DF, 0, 0, 2, 1, false),
      INST(MOV, 4, Q,  0, 1, Q,  0, 0, 2, 1, false),
      INST(MOV, 4, UQ, 0, 1, UQ, 0, 0, 2, 1, false),

      INST(MOV, 4, DF, 0, 1, F,  0, 0, 2, 2, false),
      INST(MOV, 4, Q,  0, 1, D,  0, 0, 2, 2, false),
      INST(MOV, 4, UQ, 0, 1, UD, 0, 0, 2, 2, false),

      INST(MOV, 8, F,  0, 2, DF, 0, 0, 2, 1, false),
      INST(MOV, 8, D,  0, 2, Q,  0, 0, 2, 1, false),
      INST(MOV, 8, UD, 0, 2, UQ, 0, 0, 2, 1, false),

      INST(MUL, 8, D,  0, 2, D,  0, 0, 4, 2, false),
      INST(MUL, 8, UD, 0, 2, UD, 0, 0, 4, 2, false),

      INST(MUL, 8, D,  0, 2, D,  0, 0, 4, 2, false),
      INST(MUL, 8, UD, 0, 2, UD, 0, 0, 4, 2, false),

      /*    3. Source and Destination offset must be the same, except the case
       *       of scalar source.
       */
      INST(MOV, 2, DF, 8, 1, DF, 0, 2, 2, 1, false),
      INST(MOV, 2, Q,  8, 1, Q,  0, 2, 2, 1, false),
      INST(MOV, 2, UQ, 8, 1, UQ, 0, 2, 2, 1, false),

      INST(MOV, 2, DF, 0, 1, DF, 8, 2, 2, 1, false),
      INST(MOV, 2, Q,  0, 1, Q,  8, 2, 2, 1, false),
      INST(MOV, 2, UQ, 0, 1, UQ, 8, 2, 2, 1, false),

      INST(MUL, 4, D,  4, 2, D,  0, 4, 2, 2, false),
      INST(MUL, 4, UD, 4, 2, UD, 0, 4, 2, 2, false),

      INST(MUL, 4, D,  0, 2, D,  4, 4, 2, 2, false),
      INST(MUL, 4, UD, 0, 2, UD, 4, 4, 2, 2, false),

      INST(MOV, 2, DF, 8, 1, DF, 0, 0, 1, 0, true ),
      INST(MOV, 2, Q,  8, 1, Q,  0, 0, 1, 0, true ),
      INST(MOV, 2, UQ, 8, 1, UQ, 0, 0, 1, 0, true ),

      INST(MOV, 2, DF, 8, 1, F,  4, 0, 1, 0, true ),
      INST(MOV, 2, Q,  8, 1, D,  4, 0, 1, 0, true ),
      INST(MOV, 2, UQ, 8, 1, UD, 4, 0, 1, 0, true ),

      INST(MUL, 4, D,  4, 1, D,  0, 0, 1, 0, true ),
      INST(MUL, 4, UD, 4, 1, UD, 0, 0, 1, 0, true ),

      INST(MUL, 4, D,  0, 1, D,  4, 0, 1, 0, true ),
      INST(MUL, 4, UD, 0, 1, UD, 4, 0, 1, 0, true ),

#undef INST
   };

   /* These restrictions only apply to Gfx8+ */
   if (devinfo.ver < 8)
      return;

   /* NoDDChk/NoDDClr does not exist on Gfx12+ */
   if (devinfo.ver >= 12)
      return;

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      if (!devinfo.has_64bit_float &&
          (inst[i].dst_type == ELK_REGISTER_TYPE_DF ||
           inst[i].src_type == ELK_REGISTER_TYPE_DF))
         continue;

      if (!devinfo.has_64bit_int &&
          (inst[i].dst_type == ELK_REGISTER_TYPE_Q ||
           inst[i].dst_type == ELK_REGISTER_TYPE_UQ ||
           inst[i].src_type == ELK_REGISTER_TYPE_Q ||
           inst[i].src_type == ELK_REGISTER_TYPE_UQ))
         continue;

      if (inst[i].opcode == ELK_OPCODE_MOV) {
         elk_MOV(p, retype(g0, inst[i].dst_type),
                    retype(g0, inst[i].src_type));
      } else {
         assert(inst[i].opcode == ELK_OPCODE_MUL);
         elk_MUL(p, retype(g0, inst[i].dst_type),
                    retype(g0, inst[i].src_type),
                    retype(zero, inst[i].src_type));
      }
      elk_inst_set_exec_size(&devinfo, last_inst, inst[i].exec_size);

      elk_inst_set_dst_da1_subreg_nr(&devinfo, last_inst, inst[i].dst_subreg);
      elk_inst_set_src0_da1_subreg_nr(&devinfo, last_inst, inst[i].src_subreg);

      elk_inst_set_dst_hstride(&devinfo, last_inst, inst[i].dst_stride);

      elk_inst_set_src0_vstride(&devinfo, last_inst, inst[i].src_vstride);
      elk_inst_set_src0_width(&devinfo, last_inst, inst[i].src_width);
      elk_inst_set_src0_hstride(&devinfo, last_inst, inst[i].src_hstride);

      if (devinfo.platform == INTEL_PLATFORM_CHV) {
         EXPECT_EQ(inst[i].expected_result, validate(p));
      } else {
         EXPECT_TRUE(validate(p));
      }

      clear_instructions(p);
   }
}

TEST_P(validation_test, qword_low_power_no_indirect_addressing)
{
   static const struct {
      enum elk_opcode opcode;
      unsigned exec_size;

      enum elk_reg_type dst_type;
      bool dst_is_indirect;
      unsigned dst_stride;

      enum elk_reg_type src_type;
      bool src_is_indirect;
      unsigned src_vstride;
      unsigned src_width;
      unsigned src_hstride;

      bool expected_result;
   } inst[] = {
#define INST(opcode, exec_size, dst_type, dst_is_indirect, dst_stride,         \
             src_type, src_is_indirect, src_vstride, src_width, src_hstride,   \
             expected_result)                                                  \
      {                                                                        \
         ELK_OPCODE_##opcode,                                                  \
         ELK_EXECUTE_##exec_size,                                              \
         ELK_REGISTER_TYPE_##dst_type,                                         \
         dst_is_indirect,                                                      \
         ELK_HORIZONTAL_STRIDE_##dst_stride,                                   \
         ELK_REGISTER_TYPE_##src_type,                                         \
         src_is_indirect,                                                      \
         ELK_VERTICAL_STRIDE_##src_vstride,                                    \
         ELK_WIDTH_##src_width,                                                \
         ELK_HORIZONTAL_STRIDE_##src_hstride,                                  \
         expected_result,                                                      \
      }

      /* Some instruction that violate no restrictions, as a control */
      INST(MOV, 4, DF, 0, 1, DF, 0, 4, 4, 1, true ),
      INST(MOV, 4, Q,  0, 1, Q,  0, 4, 4, 1, true ),
      INST(MOV, 4, UQ, 0, 1, UQ, 0, 4, 4, 1, true ),

      INST(MUL, 8, D,  0, 2, D,  0, 8, 4, 2, true ),
      INST(MUL, 8, UD, 0, 2, UD, 0, 8, 4, 2, true ),

      INST(MOV, 4, F,  1, 1, F,  0, 4, 4, 1, true ),
      INST(MOV, 4, F,  0, 1, F,  1, 4, 4, 1, true ),
      INST(MOV, 4, F,  1, 1, F,  1, 4, 4, 1, true ),

      /* The PRMs say that for CHV, BXT:
       *
       *    When source or destination datatype is 64b or operation is integer
       *    DWord multiply, indirect addressing must not be used.
       */
      INST(MOV, 4, DF, 1, 1, DF, 0, 4, 4, 1, false),
      INST(MOV, 4, Q,  1, 1, Q,  0, 4, 4, 1, false),
      INST(MOV, 4, UQ, 1, 1, UQ, 0, 4, 4, 1, false),

      INST(MOV, 4, DF, 0, 1, DF, 1, 4, 4, 1, false),
      INST(MOV, 4, Q,  0, 1, Q,  1, 4, 4, 1, false),
      INST(MOV, 4, UQ, 0, 1, UQ, 1, 4, 4, 1, false),

      INST(MOV, 4, DF, 1, 1, F,  0, 8, 4, 2, false),
      INST(MOV, 4, Q,  1, 1, D,  0, 8, 4, 2, false),
      INST(MOV, 4, UQ, 1, 1, UD, 0, 8, 4, 2, false),

      INST(MOV, 4, DF, 0, 1, F,  1, 8, 4, 2, false),
      INST(MOV, 4, Q,  0, 1, D,  1, 8, 4, 2, false),
      INST(MOV, 4, UQ, 0, 1, UD, 1, 8, 4, 2, false),

      INST(MOV, 4, F,  1, 2, DF, 0, 4, 4, 1, false),
      INST(MOV, 4, D,  1, 2, Q,  0, 4, 4, 1, false),
      INST(MOV, 4, UD, 1, 2, UQ, 0, 4, 4, 1, false),

      INST(MOV, 4, F,  0, 2, DF, 1, 4, 4, 1, false),
      INST(MOV, 4, D,  0, 2, Q,  1, 4, 4, 1, false),
      INST(MOV, 4, UD, 0, 2, UQ, 1, 4, 4, 1, false),

      INST(MUL, 8, D,  1, 2, D,  0, 8, 4, 2, false),
      INST(MUL, 8, UD, 1, 2, UD, 0, 8, 4, 2, false),

      INST(MUL, 8, D,  0, 2, D,  1, 8, 4, 2, false),
      INST(MUL, 8, UD, 0, 2, UD, 1, 8, 4, 2, false),

#undef INST
   };

   /* These restrictions only apply to Gfx8+ */
   if (devinfo.ver < 8)
      return;

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      if (!devinfo.has_64bit_float &&
          (inst[i].dst_type == ELK_REGISTER_TYPE_DF ||
           inst[i].src_type == ELK_REGISTER_TYPE_DF))
         continue;

      if (!devinfo.has_64bit_int &&
          (inst[i].dst_type == ELK_REGISTER_TYPE_Q ||
           inst[i].dst_type == ELK_REGISTER_TYPE_UQ ||
           inst[i].src_type == ELK_REGISTER_TYPE_Q ||
           inst[i].src_type == ELK_REGISTER_TYPE_UQ))
         continue;

      if (inst[i].opcode == ELK_OPCODE_MOV) {
         elk_MOV(p, retype(g0, inst[i].dst_type),
                    retype(g0, inst[i].src_type));
      } else {
         assert(inst[i].opcode == ELK_OPCODE_MUL);
         elk_MUL(p, retype(g0, inst[i].dst_type),
                    retype(g0, inst[i].src_type),
                    retype(zero, inst[i].src_type));
      }
      elk_inst_set_exec_size(&devinfo, last_inst, inst[i].exec_size);

      elk_inst_set_dst_address_mode(&devinfo, last_inst, inst[i].dst_is_indirect);
      elk_inst_set_src0_address_mode(&devinfo, last_inst, inst[i].src_is_indirect);

      elk_inst_set_dst_hstride(&devinfo, last_inst, inst[i].dst_stride);

      elk_inst_set_src0_vstride(&devinfo, last_inst, inst[i].src_vstride);
      elk_inst_set_src0_width(&devinfo, last_inst, inst[i].src_width);
      elk_inst_set_src0_hstride(&devinfo, last_inst, inst[i].src_hstride);

      if (devinfo.platform == INTEL_PLATFORM_CHV) {
         EXPECT_EQ(inst[i].expected_result, validate(p));
      } else {
         EXPECT_TRUE(validate(p));
      }

      clear_instructions(p);
   }
}

TEST_P(validation_test, qword_low_power_no_64bit_arf)
{
   static const struct {
      enum elk_opcode opcode;
      unsigned exec_size;

      struct elk_reg dst;
      enum elk_reg_type dst_type;
      unsigned dst_stride;

      struct elk_reg src;
      enum elk_reg_type src_type;
      unsigned src_vstride;
      unsigned src_width;
      unsigned src_hstride;

      bool acc_wr;
      bool expected_result;
   } inst[] = {
#define INST(opcode, exec_size, dst, dst_type, dst_stride,                     \
             src, src_type, src_vstride, src_width, src_hstride,               \
             acc_wr, expected_result)                                          \
      {                                                                        \
         ELK_OPCODE_##opcode,                                                  \
         ELK_EXECUTE_##exec_size,                                              \
         dst,                                                                  \
         ELK_REGISTER_TYPE_##dst_type,                                         \
         ELK_HORIZONTAL_STRIDE_##dst_stride,                                   \
         src,                                                                  \
         ELK_REGISTER_TYPE_##src_type,                                         \
         ELK_VERTICAL_STRIDE_##src_vstride,                                    \
         ELK_WIDTH_##src_width,                                                \
         ELK_HORIZONTAL_STRIDE_##src_hstride,                                  \
         acc_wr,                                                               \
         expected_result,                                                      \
      }

      /* Some instruction that violate no restrictions, as a control */
      INST(MOV, 4, g0,   DF, 1, g0,   F,  4, 2, 2, 0, true ),
      INST(MOV, 4, g0,   F,  2, g0,   DF, 4, 4, 1, 0, true ),

      INST(MOV, 4, g0,   Q,  1, g0,   D,  4, 2, 2, 0, true ),
      INST(MOV, 4, g0,   D,  2, g0,   Q,  4, 4, 1, 0, true ),

      INST(MOV, 4, g0,   UQ, 1, g0,   UD, 4, 2, 2, 0, true ),
      INST(MOV, 4, g0,   UD, 2, g0,   UQ, 4, 4, 1, 0, true ),

      INST(MOV, 4, null, F,  1, g0,   F,  4, 4, 1, 0, true ),
      INST(MOV, 4, acc0, F,  1, g0,   F,  4, 4, 1, 0, true ),
      INST(MOV, 4, g0,   F,  1, acc0, F,  4, 4, 1, 0, true ),

      INST(MOV, 4, null, D,  1, g0,   D,  4, 4, 1, 0, true ),
      INST(MOV, 4, acc0, D,  1, g0,   D,  4, 4, 1, 0, true ),
      INST(MOV, 4, g0,   D,  1, acc0, D,  4, 4, 1, 0, true ),

      INST(MOV, 4, null, UD, 1, g0,   UD, 4, 4, 1, 0, true ),
      INST(MOV, 4, acc0, UD, 1, g0,   UD, 4, 4, 1, 0, true ),
      INST(MOV, 4, g0,   UD, 1, acc0, UD, 4, 4, 1, 0, true ),

      INST(MUL, 4, g0,   D,  2, g0,   D,  4, 2, 2, 0, true ),
      INST(MUL, 4, g0,   UD, 2, g0,   UD, 4, 2, 2, 0, true ),

      /* The PRMs say that for CHV, BXT:
       *
       *    ARF registers must never be used with 64b datatype or when
       *    operation is integer DWord multiply.
       */
      INST(MOV, 4, acc0, DF, 1, g0,   F,  4, 2, 2, 0, false),
      INST(MOV, 4, g0,   DF, 1, acc0, F,  4, 2, 2, 0, false),

      INST(MOV, 4, acc0, Q,  1, g0,   D,  4, 2, 2, 0, false),
      INST(MOV, 4, g0,   Q,  1, acc0, D,  4, 2, 2, 0, false),

      INST(MOV, 4, acc0, UQ, 1, g0,   UD, 4, 2, 2, 0, false),
      INST(MOV, 4, g0,   UQ, 1, acc0, UD, 4, 2, 2, 0, false),

      INST(MOV, 4, acc0, F,  2, g0,   DF, 4, 4, 1, 0, false),
      INST(MOV, 4, g0,   F,  2, acc0, DF, 4, 4, 1, 0, false),

      INST(MOV, 4, acc0, D,  2, g0,   Q,  4, 4, 1, 0, false),
      INST(MOV, 4, g0,   D,  2, acc0, Q,  4, 4, 1, 0, false),

      INST(MOV, 4, acc0, UD, 2, g0,   UQ, 4, 4, 1, 0, false),
      INST(MOV, 4, g0,   UD, 2, acc0, UQ, 4, 4, 1, 0, false),

      INST(MUL, 4, acc0, D,  2, g0,   D,  4, 2, 2, 0, false),
      INST(MUL, 4, acc0, UD, 2, g0,   UD, 4, 2, 2, 0, false),
      /* MUL cannot have integer accumulator sources, so don't test that */

      /* We assume that the restriction does not apply to the null register */
      INST(MOV, 4, null, DF, 1, g0,   F,  4, 2, 2, 0, true ),
      INST(MOV, 4, null, Q,  1, g0,   D,  4, 2, 2, 0, true ),
      INST(MOV, 4, null, UQ, 1, g0,   UD, 4, 2, 2, 0, true ),

      /* Check implicit accumulator write control */
      INST(MOV, 4, null, DF, 1, g0,   F,  4, 2, 2, 1, false),
      INST(MUL, 4, null, DF, 1, g0,   F,  4, 2, 2, 1, false),

#undef INST
   };

   /* These restrictions only apply to Gfx8+ */
   if (devinfo.ver < 8)
      return;

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      if (!devinfo.has_64bit_float &&
          (inst[i].dst_type == ELK_REGISTER_TYPE_DF ||
           inst[i].src_type == ELK_REGISTER_TYPE_DF))
         continue;

      if (!devinfo.has_64bit_int &&
          (inst[i].dst_type == ELK_REGISTER_TYPE_Q ||
           inst[i].dst_type == ELK_REGISTER_TYPE_UQ ||
           inst[i].src_type == ELK_REGISTER_TYPE_Q ||
           inst[i].src_type == ELK_REGISTER_TYPE_UQ))
         continue;

      if (inst[i].opcode == ELK_OPCODE_MOV) {
         elk_MOV(p, retype(inst[i].dst, inst[i].dst_type),
                    retype(inst[i].src, inst[i].src_type));
      } else {
         assert(inst[i].opcode == ELK_OPCODE_MUL);
         elk_MUL(p, retype(inst[i].dst, inst[i].dst_type),
                    retype(inst[i].src, inst[i].src_type),
                    retype(zero, inst[i].src_type));
         elk_inst_set_opcode(&isa, last_inst, inst[i].opcode);
      }
      elk_inst_set_exec_size(&devinfo, last_inst, inst[i].exec_size);
      elk_inst_set_acc_wr_control(&devinfo, last_inst, inst[i].acc_wr);

      elk_inst_set_dst_hstride(&devinfo, last_inst, inst[i].dst_stride);

      elk_inst_set_src0_vstride(&devinfo, last_inst, inst[i].src_vstride);
      elk_inst_set_src0_width(&devinfo, last_inst, inst[i].src_width);
      elk_inst_set_src0_hstride(&devinfo, last_inst, inst[i].src_hstride);

      /* Note: The Broadwell PRM also lists the restriction that destination
       * of DWord multiplication cannot be the accumulator.
       */
      if (devinfo.platform == INTEL_PLATFORM_CHV ||
          (devinfo.ver == 8 &&
           inst[i].opcode == ELK_OPCODE_MUL &&
           elk_inst_dst_reg_file(&devinfo, last_inst) == ELK_ARCHITECTURE_REGISTER_FILE &&
           elk_inst_dst_da_reg_nr(&devinfo, last_inst) != ELK_ARF_NULL)) {
         EXPECT_EQ(inst[i].expected_result, validate(p));
      } else {
         EXPECT_TRUE(validate(p));
      }

      clear_instructions(p);
   }

   if (!devinfo.has_64bit_float)
      return;

   /* MAC implicitly reads the accumulator */
   elk_MAC(p, retype(g0, ELK_REGISTER_TYPE_DF),
              retype(stride(g0, 4, 4, 1), ELK_REGISTER_TYPE_DF),
              retype(stride(g0, 4, 4, 1), ELK_REGISTER_TYPE_DF));
   if (devinfo.platform == INTEL_PLATFORM_CHV) {
      EXPECT_FALSE(validate(p));
   } else {
      EXPECT_TRUE(validate(p));
   }
}

TEST_P(validation_test, align16_64_bit_integer)
{
   static const struct {
      enum elk_opcode opcode;
      unsigned exec_size;

      enum elk_reg_type dst_type;
      enum elk_reg_type src_type;

      bool expected_result;
   } inst[] = {
#define INST(opcode, exec_size, dst_type, src_type, expected_result)           \
      {                                                                        \
         ELK_OPCODE_##opcode,                                                  \
         ELK_EXECUTE_##exec_size,                                              \
         ELK_REGISTER_TYPE_##dst_type,                                         \
         ELK_REGISTER_TYPE_##src_type,                                         \
         expected_result,                                                      \
      }

      /* Some instruction that violate no restrictions, as a control */
      INST(MOV, 2, Q,  D,  true ),
      INST(MOV, 2, UQ, UD, true ),
      INST(MOV, 2, DF, F,  true ),

      INST(ADD, 2, Q,  D,  true ),
      INST(ADD, 2, UQ, UD, true ),
      INST(ADD, 2, DF, F,  true ),

      /* The PRMs say that for BDW, SKL:
       *
       *    If Align16 is required for an operation with QW destination and non-QW
       *    source datatypes, the execution size cannot exceed 2.
       */

      INST(MOV, 4, Q,  D,  false),
      INST(MOV, 4, UQ, UD, false),
      INST(MOV, 4, DF, F,  false),

      INST(ADD, 4, Q,  D,  false),
      INST(ADD, 4, UQ, UD, false),
      INST(ADD, 4, DF, F,  false),

#undef INST
   };

   /* 64-bit integer types exist on Gfx8+ */
   if (devinfo.ver < 8)
      return;

   /* Align16 does not exist on Gfx11+ */
   if (devinfo.ver >= 11)
      return;

   elk_set_default_access_mode(p, ELK_ALIGN_16);

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      if (inst[i].opcode == ELK_OPCODE_MOV) {
         elk_MOV(p, retype(g0, inst[i].dst_type),
                    retype(g0, inst[i].src_type));
      } else {
         assert(inst[i].opcode == ELK_OPCODE_ADD);
         elk_ADD(p, retype(g0, inst[i].dst_type),
                    retype(g0, inst[i].src_type),
                    retype(g0, inst[i].src_type));
      }
      elk_inst_set_exec_size(&devinfo, last_inst, inst[i].exec_size);

      EXPECT_EQ(inst[i].expected_result, validate(p));

      clear_instructions(p);
   }
}

TEST_P(validation_test, qword_low_power_no_depctrl)
{
   static const struct {
      enum elk_opcode opcode;
      unsigned exec_size;

      enum elk_reg_type dst_type;
      unsigned dst_stride;

      enum elk_reg_type src_type;
      unsigned src_vstride;
      unsigned src_width;
      unsigned src_hstride;

      bool no_dd_check;
      bool no_dd_clear;

      bool expected_result;
   } inst[] = {
#define INST(opcode, exec_size, dst_type, dst_stride,                          \
             src_type, src_vstride, src_width, src_hstride,                    \
             no_dd_check, no_dd_clear, expected_result)                        \
      {                                                                        \
         ELK_OPCODE_##opcode,                                                  \
         ELK_EXECUTE_##exec_size,                                              \
         ELK_REGISTER_TYPE_##dst_type,                                         \
         ELK_HORIZONTAL_STRIDE_##dst_stride,                                   \
         ELK_REGISTER_TYPE_##src_type,                                         \
         ELK_VERTICAL_STRIDE_##src_vstride,                                    \
         ELK_WIDTH_##src_width,                                                \
         ELK_HORIZONTAL_STRIDE_##src_hstride,                                  \
         no_dd_check,                                                          \
         no_dd_clear,                                                          \
         expected_result,                                                      \
      }

      /* Some instruction that violate no restrictions, as a control */
      INST(MOV, 4, DF, 1, F,  8, 4, 2, 0, 0, true ),
      INST(MOV, 4, Q,  1, D,  8, 4, 2, 0, 0, true ),
      INST(MOV, 4, UQ, 1, UD, 8, 4, 2, 0, 0, true ),

      INST(MOV, 4, F,  2, DF, 4, 4, 1, 0, 0, true ),
      INST(MOV, 4, D,  2, Q,  4, 4, 1, 0, 0, true ),
      INST(MOV, 4, UD, 2, UQ, 4, 4, 1, 0, 0, true ),

      INST(MUL, 8, D,  2, D,  8, 4, 2, 0, 0, true ),
      INST(MUL, 8, UD, 2, UD, 8, 4, 2, 0, 0, true ),

      INST(MOV, 4, F,  1, F,  4, 4, 1, 1, 1, true ),

      /* The PRMs say that for CHV, BXT:
       *
       *    When source or destination datatype is 64b or operation is integer
       *    DWord multiply, DepCtrl must not be used.
       */
      INST(MOV, 4, DF, 1, F,  8, 4, 2, 1, 0, false),
      INST(MOV, 4, Q,  1, D,  8, 4, 2, 1, 0, false),
      INST(MOV, 4, UQ, 1, UD, 8, 4, 2, 1, 0, false),

      INST(MOV, 4, F,  2, DF, 4, 4, 1, 1, 0, false),
      INST(MOV, 4, D,  2, Q,  4, 4, 1, 1, 0, false),
      INST(MOV, 4, UD, 2, UQ, 4, 4, 1, 1, 0, false),

      INST(MOV, 4, DF, 1, F,  8, 4, 2, 0, 1, false),
      INST(MOV, 4, Q,  1, D,  8, 4, 2, 0, 1, false),
      INST(MOV, 4, UQ, 1, UD, 8, 4, 2, 0, 1, false),

      INST(MOV, 4, F,  2, DF, 4, 4, 1, 0, 1, false),
      INST(MOV, 4, D,  2, Q,  4, 4, 1, 0, 1, false),
      INST(MOV, 4, UD, 2, UQ, 4, 4, 1, 0, 1, false),

      INST(MUL, 8, D,  2, D,  8, 4, 2, 1, 0, false),
      INST(MUL, 8, UD, 2, UD, 8, 4, 2, 1, 0, false),

      INST(MUL, 8, D,  2, D,  8, 4, 2, 0, 1, false),
      INST(MUL, 8, UD, 2, UD, 8, 4, 2, 0, 1, false),

#undef INST
   };

   /* These restrictions only apply to Gfx8+ */
   if (devinfo.ver < 8)
      return;

   /* NoDDChk/NoDDClr does not exist on Gfx12+ */
   if (devinfo.ver >= 12)
      return;

   for (unsigned i = 0; i < ARRAY_SIZE(inst); i++) {
      if (!devinfo.has_64bit_float &&
          (inst[i].dst_type == ELK_REGISTER_TYPE_DF ||
           inst[i].src_type == ELK_REGISTER_TYPE_DF))
         continue;

      if (!devinfo.has_64bit_int &&
          (inst[i].dst_type == ELK_REGISTER_TYPE_Q ||
           inst[i].dst_type == ELK_REGISTER_TYPE_UQ ||
           inst[i].src_type == ELK_REGISTER_TYPE_Q ||
           inst[i].src_type == ELK_REGISTER_TYPE_UQ))
         continue;

      if (inst[i].opcode == ELK_OPCODE_MOV) {
         elk_MOV(p, retype(g0, inst[i].dst_type),
                    retype(g0, inst[i].src_type));
      } else {
         assert(inst[i].opcode == ELK_OPCODE_MUL);
         elk_MUL(p, retype(g0, inst[i].dst_type),
                    retype(g0, inst[i].src_type),
                    retype(zero, inst[i].src_type));
      }
      elk_inst_set_exec_size(&devinfo, last_inst, inst[i].exec_size);

      elk_inst_set_dst_hstride(&devinfo, last_inst, inst[i].dst_stride);

      elk_inst_set_src0_vstride(&devinfo, last_inst, inst[i].src_vstride);
      elk_inst_set_src0_width(&devinfo, last_inst, inst[i].src_width);
      elk_inst_set_src0_hstride(&devinfo, last_inst, inst[i].src_hstride);

      elk_inst_set_no_dd_check(&devinfo, last_inst, inst[i].no_dd_check);
      elk_inst_set_no_dd_clear(&devinfo, last_inst, inst[i].no_dd_clear);

      if (devinfo.platform == INTEL_PLATFORM_CHV) {
         EXPECT_EQ(inst[i].expected_result, validate(p));
      } else {
         EXPECT_TRUE(validate(p));
      }

      clear_instructions(p);
   }
}
