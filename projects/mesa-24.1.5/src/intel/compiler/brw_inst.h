/*
 * Copyright © 2014 Intel Corporation
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

/**
 * @file brw_inst.h
 *
 * A representation of i965 EU assembly instructions, with helper methods to
 * get and set various fields.  This is the actual hardware format.
 */

#ifndef BRW_INST_H
#define BRW_INST_H

#include <assert.h>
#include <stdint.h>

#include "brw_eu_defines.h"
#include "brw_isa_info.h"
#include "brw_reg_type.h"
#include "dev/intel_device_info.h"

#ifdef __cplusplus
extern "C" {
#endif

/* brw_context.h has a forward declaration of brw_inst, so name the struct. */
typedef struct brw_inst {
   uint64_t data[2];
} brw_inst;

static inline uint64_t brw_inst_bits(const brw_inst *inst,
                                     unsigned high, unsigned low);
static inline void brw_inst_set_bits(brw_inst *inst,
                                     unsigned high, unsigned low,
                                     uint64_t value);

#define FC(name, hi9, lo9, hi12, lo12, assertions)            \
static inline void                                            \
brw_inst_set_##name(const struct intel_device_info *devinfo,  \
                    brw_inst *inst, uint64_t v)               \
{                                                             \
   assert(assertions);                                        \
   if (devinfo->ver >= 12)                                    \
      brw_inst_set_bits(inst, hi12, lo12, v);                 \
   else                                                       \
      brw_inst_set_bits(inst, hi9, lo9, v);                   \
}                                                             \
static inline uint64_t                                        \
brw_inst_##name(const struct intel_device_info *devinfo,      \
                const brw_inst *inst)                         \
{                                                             \
   assert(assertions);                                        \
   if (devinfo->ver >= 12)                                    \
      return brw_inst_bits(inst, hi12, lo12);                 \
   else                                                       \
      return brw_inst_bits(inst, hi9, lo9);                   \
}

/* A simple macro for fields which stay in the same place on all generations,
 * except for Gfx12!
 */
#define F(name, hi9, lo9, hi12, lo12) FC(name, hi9, lo9, hi12, lo12, true)

/* A simple macro for fields which stay in the same place on all generations,
 * except for Gfx12 and Gfx20.
 */
#define F20(name, hi9, lo9, hi12, lo12, hi20, lo20)                \
   static inline void                                              \
   brw_inst_set_##name(const struct intel_device_info *devinfo,    \
                       brw_inst *inst, uint64_t v)                 \
   {                                                               \
      if (devinfo->ver >= 20)                                      \
         brw_inst_set_bits(inst, hi20, lo20, v);                   \
      else if (devinfo->ver >= 12)                                 \
         brw_inst_set_bits(inst, hi12, lo12, v);                   \
      else                                                         \
         brw_inst_set_bits(inst, hi9, lo9, v);                     \
   }                                                               \
   static inline uint64_t                                          \
   brw_inst_##name(const struct intel_device_info *devinfo,        \
                   const brw_inst *inst)                           \
   {                                                               \
      if (devinfo->ver >= 20)                                      \
         return brw_inst_bits(inst, hi20, lo20);                   \
      else if (devinfo->ver >= 12)                                 \
         return brw_inst_bits(inst, hi12, lo12);                   \
      else                                                         \
         return brw_inst_bits(inst, hi9, lo9);                     \
   }

#define FV20(name, hi9, lo9, hi12, lo12, hi20, lo20)               \
   static inline void                                              \
   brw_inst_set_##name(const struct intel_device_info *devinfo,    \
                       brw_inst *inst, uint64_t v)                 \
   {                                                               \
      if (devinfo->ver >= 20)                                      \
         brw_inst_set_bits(inst, hi20, lo20, v & 0x7);             \
      else if (devinfo->ver >= 12)                                 \
         brw_inst_set_bits(inst, hi12, lo12, v);                   \
      else                                                         \
         brw_inst_set_bits(inst, hi9, lo9, v);                     \
   }                                                               \
   static inline uint64_t                                          \
   brw_inst_##name(const struct intel_device_info *devinfo,        \
                   const brw_inst *inst)                           \
   {                                                               \
      if (devinfo->ver >= 20)                                      \
         return brw_inst_bits(inst, hi20, lo20) == 0x7 ? 0xF :     \
                brw_inst_bits(inst, hi20, lo20);                   \
      else if (devinfo->ver >= 12)                                 \
         return brw_inst_bits(inst, hi12, lo12);                   \
      else                                                         \
         return brw_inst_bits(inst, hi9, lo9);                     \
   }

#define FD20(name, hi9, lo9, hi12, lo12, hi20, lo20, zero20)       \
   static inline void                                              \
   brw_inst_set_##name(const struct intel_device_info *devinfo,    \
                       brw_inst *inst, uint64_t v)                 \
   {                                                               \
      if (devinfo->ver >= 20) {                                    \
         brw_inst_set_bits(inst, hi20, lo20, v >> 1);              \
         if (zero20 == -1)                                         \
            assert((v & 1) == 0);                                  \
         else                                                      \
            brw_inst_set_bits(inst, zero20, zero20, v & 1);        \
      } else if (devinfo->ver >= 12)                               \
         brw_inst_set_bits(inst, hi12, lo12, v);                   \
      else                                                         \
         brw_inst_set_bits(inst, hi9, lo9, v);                     \
   }                                                               \
   static inline uint64_t                                          \
   brw_inst_##name(const struct intel_device_info *devinfo,        \
                   const brw_inst *inst)                           \
   {                                                               \
      if (devinfo->ver >= 20)                                      \
         return (brw_inst_bits(inst, hi20, lo20) << 1) |           \
                (zero20 == -1 ? 0 :                                \
                 brw_inst_bits(inst, zero20, zero20));             \
      else if (devinfo->ver >= 12)                                 \
         return brw_inst_bits(inst, hi12, lo12);                   \
      else                                                         \
         return brw_inst_bits(inst, hi9, lo9);                     \
   }

/* Macro for fields that gained extra discontiguous MSBs in Gfx12 (specified
 * by hi12ex-lo12ex).
 */
#define FFDC(name, hi9, lo9, hi12ex, lo12ex, hi12, lo12, assertions)          \
static inline void                                                            \
brw_inst_set_##name(const struct intel_device_info *devinfo,                  \
                    brw_inst *inst, uint64_t value)                           \
{                                                                             \
   assert(assertions);                                                        \
   if (devinfo->ver >= 12) {                                                  \
      const unsigned k = hi12 - lo12 + 1;                                     \
      if (hi12ex != -1 && lo12ex != -1)                                       \
         brw_inst_set_bits(inst, hi12ex, lo12ex, value >> k);                 \
      brw_inst_set_bits(inst, hi12, lo12, value & ((1ull << k) - 1));         \
   } else {                                                                   \
      brw_inst_set_bits(inst, hi9, lo9, value);                               \
   }                                                                          \
}                                                                             \
static inline uint64_t                                                        \
brw_inst_##name(const struct intel_device_info *devinfo, const brw_inst *inst)\
{                                                                             \
   assert(assertions);                                                        \
   if (devinfo->ver >= 12) {                                                  \
      const unsigned k = hi12 - lo12 + 1;                                     \
      return (hi12ex == -1 || lo12ex == -1 ? 0 :                              \
              brw_inst_bits(inst, hi12ex, lo12ex) << k) |                     \
             brw_inst_bits(inst, hi12, lo12);                                 \
   } else {                                                                   \
      return brw_inst_bits(inst, hi9, lo9);                                   \
   }                                                                          \
}

#define FD(name, hi9, lo9, hi12ex, lo12ex, hi12, lo12)            \
   FFDC(name, hi9, lo9, hi12ex, lo12ex, hi12, lo12, true)

/* Macro for fields that didn't move across generations until Gfx12, and then
 * gained extra discontiguous bits.
 */
#define FDC(name, hi9, lo9, hi12ex, lo12ex, hi12, lo12, assertions)     \
   FFDC(name, hi9, lo9, hi12ex, lo12ex, hi12, lo12, assertions)


/* Macro for the 2-bit register file field, which on Gfx12+ is stored as the
 * variable length combination of an IsImm (hi12) bit and an additional file
 * (lo12) bit.
 */
#define FI(name, hi9, lo9, hi12, lo12)                                        \
static inline void                                                            \
brw_inst_set_##name(const struct intel_device_info *devinfo,                  \
                    brw_inst *inst, uint64_t value)                           \
{                                                                             \
   if (devinfo->ver >= 12) {                                                  \
      brw_inst_set_bits(inst, hi12, hi12, value >> 1);                        \
      if ((value >> 1) == 0)                                                  \
         brw_inst_set_bits(inst, lo12, lo12, value & 1);                      \
   } else {                                                                   \
      brw_inst_set_bits(inst, hi9, lo9, value);                               \
   }                                                                          \
}                                                                             \
static inline uint64_t                                                        \
brw_inst_##name(const struct intel_device_info *devinfo, const brw_inst *inst)\
{                                                                             \
   if (devinfo->ver >= 12) {                                                  \
      return (brw_inst_bits(inst, hi12, hi12) << 1) |                         \
             (brw_inst_bits(inst, hi12, hi12) == 0 ?                          \
              brw_inst_bits(inst, lo12, lo12) : 1);                           \
   } else {                                                                   \
      return brw_inst_bits(inst, hi9, lo9);                                   \
   }                                                                          \
}

/* Macro for fields that become a constant in Gfx12+ not actually represented
 * in the instruction.
 */
#define FK(name, hi9, lo9, const12)                           \
static inline void                                            \
brw_inst_set_##name(const struct intel_device_info *devinfo,  \
                    brw_inst *inst, uint64_t v)               \
{                                                             \
   if (devinfo->ver >= 12)                                    \
      assert(v == (const12));                                 \
   else                                                       \
      brw_inst_set_bits(inst, hi9, lo9, v);                   \
}                                                             \
static inline uint64_t                                        \
brw_inst_##name(const struct intel_device_info *devinfo,      \
                const brw_inst *inst)                         \
{                                                             \
   if (devinfo->ver >= 12)                                    \
      return (const12);                                       \
   else                                                       \
      return brw_inst_bits(inst, hi9, lo9);                   \
}

FV20(src1_vstride,     /* 9+ */ 120, 117, /* 12+ */ 119, 116, /* 20+ */ 118, 116)
F(src1_width,          /* 9+ */ 116, 114, /* 12+ */ 115, 113)
F(src1_da16_swiz_w,    /* 9+ */ 115, 114, /* 12+ */ -1, -1)
F(src1_da16_swiz_z,    /* 9+ */ 113, 112, /* 12+ */ -1, -1)
F(src1_hstride,        /* 9+ */ 113, 112, /* 12+ */ 97, 96)
F(src1_address_mode,   /* 9+ */ 111, 111, /* 12+ */ 112, 112)
/** Src1.SrcMod @{ */
F(src1_negate,         /* 9+ */ 110, 110, /* 12+ */ 121, 121)
F(src1_abs,            /* 9+ */ 109, 109, /* 12+ */ 120, 120)
/** @} */
F(src1_ia_subreg_nr,   /* 9+ */ 108, 105, /* 12+ */ 111, 108)
F(src1_da_reg_nr,      /* 9+ */ 108, 101, /* 12+ */ 111, 104)
F(src1_da16_subreg_nr, /* 9+ */ 100, 100, /* 12+ */ -1, -1)
FD20(src1_da1_subreg_nr, /* 9+ */ 100, 96, /* 12+ */ 103, 99, /* 20+ */ 103, 99, -1)
F(src1_da16_swiz_y,    /* 9+ */ 99,  98,  /* 12+ */ -1, -1)
F(src1_da16_swiz_x,    /* 9+ */ 97,  96,  /* 12+ */ -1, -1)
F(src1_reg_hw_type,    /* 9+ */ 94,  91,  /* 12+ */ 91, 88)
FI(src1_reg_file,      /* 9+ */ 90,  89,  /* 12+ */ 47, 98)
F(src1_is_imm,         /* 9+ */ -1,  -1,  /* 12+ */ 47, 47)
FV20(src0_vstride,     /* 9+ */ 88,  85,  /* 12+ */ 87, 84,  /* 20+ */ 86, 84)
F(src0_width,          /* 9+ */ 84,  82,  /* 12+ */ 83, 81)
F(src0_da16_swiz_w,    /* 9+ */ 83,  82,  /* 12+ */ -1, -1)
F(src0_da16_swiz_z,    /* 9+ */ 81,  80,  /* 12+ */ -1, -1)
F(src0_hstride,        /* 9+ */ 81,  80,  /* 12+ */ 65, 64)
F(src0_address_mode,   /* 9+ */ 79,  79,  /* 12+ */ 80, 80)
/** Src0.SrcMod @{ */
F(src0_negate,         /* 9+ */ 78,  78,  /* 12+ */ 45, 45)
F(src0_abs,            /* 9+ */ 77,  77,  /* 12+ */ 44, 44)
/** @} */
F(src0_ia_subreg_nr,   /* 9+ */ 76,  73,  /* 12+ */ 79, 76)
F(src0_da_reg_nr,      /* 9+ */ 76,  69,  /* 12+ */ 79, 72)
F(src0_da16_subreg_nr, /* 9+ */ 68,  68,  /* 12+ */ -1, -1)
FD20(src0_da1_subreg_nr, /* 9+ */ 68, 64, /* 12+ */ 71,  67, /* 20+ */ 71, 67, 87)
F(src0_da16_swiz_y,    /* 9+ */ 67,  66,  /* 12+ */ -1, -1)
F(src0_da16_swiz_x,    /* 9+ */ 65,  64,  /* 12+ */ -1, -1)
F(dst_address_mode,    /* 9+ */ 63,  63,  /* 12+ */ 35, 35)
F(dst_hstride,         /* 9+ */ 62,  61,  /* 12+ */ 49, 48)
F(dst_ia_subreg_nr,    /* 9+ */ 60,  57,  /* 12+ */ 63, 60)
F(dst_da_reg_nr,       /* 9+ */ 60,  53,  /* 12+ */ 63, 56)
F(dst_da16_subreg_nr,  /* 9+ */ 52,  52,  /* 12+ */ -1, -1)
FD20(dst_da1_subreg_nr, /* 9+ */ 52, 48,  /* 12+ */ 55, 51, /* 20+ */ 55, 51, 33)
F(da16_writemask,      /* 9+ */ 51,  48,  /* 12+ */ -1, -1) /* Dst.ChanEn */
F(src0_reg_hw_type,    /* 9+ */ 46,  43,  /* 12+ */ 43, 40)
FI(src0_reg_file,      /* 9+ */ 42,  41,  /* 12+ */ 46, 66)
F(src0_is_imm,         /* 9+ */ -1,  -1,  /* 12+ */ 46, 46)
F(dst_reg_hw_type,     /* 9+ */ 40,  37,  /* 12+ */ 39, 36)
F(dst_reg_file,        /* 9+ */ 36,  35,  /* 12+ */ 50, 50)
F(mask_control,        /* 9+ */ 34,  34,  /* 12+ */ 31, 31)
F20(flag_reg_nr,       /* 9+ */ 33,  33,  /* 12+ */ 23, 23,  /* 20+ */ 23, 22)
F20(flag_subreg_nr,    /* 9+ */ 32,  32,  /* 12+ */ 22, 22,  /* 20+ */ 21, 21)
F(saturate,            /* 9+ */ 31,  31,  /* 12+ */ 34, 34)
F(debug_control,       /* 9+ */ 30,  30,  /* 12+ */ 30, 30)
F(cmpt_control,        /* 9+ */ 29,  29,  /* 12+ */ 29, 29)
F(branch_control,      /* 9+ */ 28,  28,  /* 12+ */ 33, 33)
FC(acc_wr_control,     /* 9+ */ 28,  28,  /* 12+ */ 33, 33, devinfo->ver < 20)
F(cond_modifier,       /* 9+ */ 27,  24,  /* 12+ */ 95, 92)
F(math_function,       /* 9+ */ 27,  24,  /* 12+ */ 95, 92)
F20(exec_size,         /* 9+ */ 23,  21,  /* 12+ */ 18, 16,  /* 20+ */ 20, 18)
F(pred_inv,            /* 9+ */ 20,  20,  /* 12+ */ 28, 28)
F20(pred_control,      /* 9+ */ 19,  16,  /* 12+ */ 27, 24,  /* 20+ */ 27, 26)
F(thread_control,      /* 9+ */ 15,  14,  /* 12+ */ -1, -1)
F(atomic_control,      /* 9+ */ -1,  -1,  /* 12+ */ 32, 32)
F20(qtr_control,       /* 9+ */ 13,  12,  /* 12+ */ 21, 20,  /* 20+ */ 25, 24)
F20(nib_control,       /* 9+ */ 11,  11,  /* 12+ */ 19, 19,  /* 20+ */ -1, -1)
F(no_dd_check,         /* 9+ */ 10,  10,  /* 12+ */ -1, -1)
F(no_dd_clear,         /* 9+ */  9,   9,  /* 12+ */ -1, -1)
F20(swsb,              /* 9+ */  -1, -1,  /* 12+ */ 15,   8, /* 20+ */ 17, 8)
FK(access_mode,        /* 9+ */   8,  8,  /* 12+ */ BRW_ALIGN_1)
/* Bit 7 is Reserved (for future Opcode expansion) */
F(hw_opcode,           /* 9+ */   6,  0,  /* 12+ */ 6,  0)

/**
 * Three-source instructions:
 *  @{
 */
F(3src_src2_reg_nr,         /* 9+ */ 125, 118, /* 12+ */ 127, 120) /* same in align1 */
F(3src_a16_src2_subreg_nr,  /* 9+ */ 117, 115, /* 12+ */ -1, -1) /* Extra discontiguous bit on CHV? */
F(3src_a16_src2_swizzle,    /* 9+ */ 114, 107, /* 12+ */ -1, -1)
F(3src_a16_src2_rep_ctrl,   /* 9+ */ 106, 106, /* 12+ */ -1, -1)
F(3src_src1_reg_nr,         /* 9+ */ 104,  97, /* 12+ */ 111, 104) /* same in align1 */
F(3src_a16_src1_subreg_nr,  /* 9+ */ 96,  94,  /* 12+ */ -1, -1) /* Extra discontiguous bit on CHV? */
F(3src_a16_src1_swizzle,    /* 9+ */ 93,  86,  /* 12+ */ -1, -1)
F(3src_a16_src1_rep_ctrl,   /* 9+ */ 85,  85,  /* 12+ */ -1, -1)
F(3src_src0_reg_nr,         /* 9+ */ 83,  76,  /* 12+ */ 79, 72) /* same in align1 */
F(3src_a16_src0_subreg_nr,  /* 9+ */ 75,  73,  /* 12+ */ -1, -1) /* Extra discontiguous bit on CHV? */
F(3src_a16_src0_swizzle,    /* 9+ */ 72,  65,  /* 12+ */ -1, -1)
F(3src_a16_src0_rep_ctrl,   /* 9+ */ 64,  64,  /* 12+ */ -1, -1)
F(3src_dst_reg_nr,          /* 9+ */ 63,  56,  /* 12+ */ 63, 56) /* same in align1 */
F(3src_a16_dst_subreg_nr,   /* 9+ */ 55,  53,  /* 12+ */ -1, -1)
F(3src_a16_dst_writemask,   /* 9+ */ 52,  49,  /* 12+ */ -1, -1)
F(3src_a16_nib_ctrl,        /* 9+ */ 11,  11,  /* 12+ */ -1, -1) /* only exists on IVB+ */
F(3src_a16_dst_hw_type,     /* 9+ */ 48,  46,  /* 12+ */ -1, -1) /* only exists on IVB+ */
F(3src_a16_src_hw_type,     /* 9+ */ 45,  43,  /* 12+ */ -1, -1)
F(3src_src2_negate,         /* 9+ */ 42,  42,  /* 12+ */ 85, 85)
F(3src_src2_abs,            /* 9+ */ 41,  41,  /* 12+ */ 84, 84)
F(3src_src1_negate,         /* 9+ */ 40,  40,  /* 12+ */ 87, 87)
F(3src_src1_abs,            /* 9+ */ 39,  39,  /* 12+ */ 86, 86)
F(3src_src0_negate,         /* 9+ */ 38,  38,  /* 12+ */ 45, 45)
F(3src_src0_abs,            /* 9+ */ 37,  37,  /* 12+ */ 44, 44)
F(3src_a16_src1_type,       /* 9+ */ 36,  36,  /* 12+ */ -1, -1)
F(3src_a16_src2_type,       /* 9+ */ 35,  35,  /* 12+ */ -1, -1)
F(3src_a16_flag_reg_nr,     /* 9+ */ 33,  33,  /* 12+ */ -1, -1)
F(3src_a16_flag_subreg_nr,  /* 9+ */ 32,  32,  /* 12+ */ -1, -1)
F(3src_saturate,            /* 9+ */ 31, 31,   /* 12+ */ 34, 34)
F(3src_debug_control,       /* 9+ */ 30, 30,   /* 12+ */ 30, 30)
F(3src_cmpt_control,        /* 9+ */ 29, 29,   /* 12+ */ 29, 29)
FC(3src_acc_wr_control,     /* 9+ */ 28, 28,   /* 12+ */ 33, 33, devinfo->ver < 20)
F(3src_cond_modifier,       /* 9+ */ 27, 24,   /* 12+ */ 95, 92)
F(3src_exec_size,           /* 9+ */ 23, 21,   /* 12+ */ 18, 16)
F(3src_pred_inv,            /* 9+ */ 20, 20,   /* 12+ */ 28, 28)
F20(3src_pred_control,      /* 9+ */ 19, 16,   /* 12+ */ 27, 24, /* 20+ */ 27, 26)
F(3src_thread_control,      /* 9+ */ 15, 14,   /* 12+ */ -1, -1)
F(3src_atomic_control,      /* 9+ */ -1, -1,   /* 12+ */ 32, 32)
F20(3src_qtr_control,       /* 9+ */ 13, 12,   /* 12+ */ 21, 20, /* 20+ */ 25, 24)
F(3src_no_dd_check,         /* 9+ */ 10, 10,   /* 12+ */ -1, -1)
F(3src_no_dd_clear,         /* 9+ */  9,  9,   /* 12+ */ -1, -1)
F(3src_mask_control,        /* 9+ */ 34, 34,   /* 12+ */ 31, 31)
FK(3src_access_mode,        /* 9+ */  8,  8,   /* 12+ */ BRW_ALIGN_1)
F(3src_swsb,                /* 9+ */ -1, -1,   /* 12+ */ 15,  8)
/* Bit 7 is Reserved (for future Opcode expansion) */
F(3src_hw_opcode,           /* 9+ */ 6,  0,    /* 12+ */ 6, 0)
/** @} */

#define REG_TYPE(reg)                                                         \
static inline void                                                            \
brw_inst_set_3src_a16_##reg##_type(const struct intel_device_info *devinfo,   \
                                   brw_inst *inst, enum brw_reg_type type)    \
{                                                                             \
   unsigned hw_type = brw_reg_type_to_a16_hw_3src_type(devinfo, type);        \
   brw_inst_set_3src_a16_##reg##_hw_type(devinfo, inst, hw_type);             \
}                                                                             \
                                                                              \
static inline enum brw_reg_type                                               \
brw_inst_3src_a16_##reg##_type(const struct intel_device_info *devinfo,       \
                               const brw_inst *inst)                          \
{                                                                             \
   unsigned hw_type = brw_inst_3src_a16_##reg##_hw_type(devinfo, inst);       \
   return brw_a16_hw_3src_type_to_reg_type(devinfo, hw_type);                 \
}

REG_TYPE(dst)
REG_TYPE(src)
#undef REG_TYPE

/**
 * Three-source align1 instructions:
 *  @{
 */
/* Reserved 127:126 */
/* src2_reg_nr same in align16 */
FD20(3src_a1_src2_subreg_nr,/* 9+ */   117, 113, /* 12+ */ 119, 115, /* 20+ */ 119, 115, -1)
FC(3src_a1_src2_hstride,    /* 9+ */   112, 111, /* 12+ */ 113, 112, devinfo->ver >= 10)
/* Reserved 110:109. src2 vstride is an implied parameter */
FC(3src_a1_src2_hw_type,    /* 9+ */   108, 106, /* 12+ */ 82, 80, devinfo->ver >= 10)
/* Reserved 105 */
/* src1_reg_nr same in align16 */
FD20(3src_a1_src1_subreg_nr, /* 9+ */   96,  92, /* 12+ */ 103, 99, /* 20+ */ 103, 99, -1)
FC(3src_a1_src1_hstride,    /* 9+ */   91,  90,  /* 12+ */ 97, 96, devinfo->ver >= 10)
FDC(3src_a1_src1_vstride,   /* 9+ */   89,  88,  /* 12+ */ 91, 91, 83, 83, devinfo->ver >= 10)
FC(3src_a1_src1_hw_type,    /* 9+ */   87,  85,  /* 12+ */ 90, 88, devinfo->ver >= 10)
/* Reserved 84 */
/* src0_reg_nr same in align16 */
FD20(3src_a1_src0_subreg_nr, /* 9+ */   75,  71, /* 12+ */ 71, 67, /* 20+ */ 71, 67, -1)
FC(3src_a1_src0_hstride,    /* 9+ */   70,  69,  /* 12+ */ 65, 64, devinfo->ver >= 10)
FDC(3src_a1_src0_vstride,   /* 9+ */   68,  67,  /* 12+ */ 43, 43, 35, 35, devinfo->ver >= 10)
FC(3src_a1_src0_hw_type,    /* 9+ */   66,  64,  /* 12+ */ 42, 40, devinfo->ver >= 10)
/* dst_reg_nr same in align16 */
FC(3src_a1_dst_subreg_nr,   /* 9+ */   55,  54,  /* 12+ */ 55, 54, devinfo->ver >= 10)
FC(3src_a1_special_acc,     /* 9+ */   55,  52,  /* 12+ */ 54, 51, devinfo->ver >= 10) /* aliases dst_subreg_nr */
/* Reserved 51:50 */
FC(3src_a1_dst_hstride,     /* 9+ */   49,  49,  /* 12+ */ 48, 48, devinfo->ver >= 10)
FC(3src_a1_dst_hw_type,     /* 9+ */   48,  46,  /* 12+ */ 38, 36, devinfo->ver >= 10)
FI(3src_a1_src2_reg_file,   /* 9+ */   45,  45,  /* 12+ */ 47, 114)
FC(3src_a1_src1_reg_file,   /* 9+ */   44,  44,  /* 12+ */ 98, 98, devinfo->ver >= 10)
FI(3src_a1_src0_reg_file,   /* 9+ */   43,  43,  /* 12+ */ 46, 66)

F(3src_a1_src2_is_imm,      /* 9+ */   -1,  -1,  /* 12+ */ 47, 47)
F(3src_a1_src0_is_imm,      /* 9+ */   -1,  -1,  /* 12+ */ 46, 46)

/* Source Modifier fields same in align16 */
FC(3src_a1_dst_reg_file,    /* 9+ */    36,  36, /* 12+ */ 50, 50, devinfo->ver >= 10)
FC(3src_a1_exec_type,       /* 9+ */    35,  35, /* 12+ */ 39, 39, devinfo->ver >= 10)
/* Fields below this same in align16 */
/** @} */

#define REG_TYPE(reg)                                                         \
static inline void                                                            \
brw_inst_set_3src_a1_##reg##_type(const struct intel_device_info *devinfo,    \
                                  brw_inst *inst, enum brw_reg_type type)     \
{                                                                             \
   UNUSED enum gfx10_align1_3src_exec_type exec_type =                        \
      (enum gfx10_align1_3src_exec_type) brw_inst_3src_a1_exec_type(devinfo,  \
                                                                    inst);    \
   if (brw_reg_type_is_floating_point(type)) {                                \
      assert(exec_type == BRW_ALIGN1_3SRC_EXEC_TYPE_FLOAT);                   \
   } else {                                                                   \
      assert(exec_type == BRW_ALIGN1_3SRC_EXEC_TYPE_INT);                     \
   }                                                                          \
   unsigned hw_type = brw_reg_type_to_a1_hw_3src_type(devinfo, type);         \
   brw_inst_set_3src_a1_##reg##_hw_type(devinfo, inst, hw_type);              \
}                                                                             \
                                                                              \
static inline enum brw_reg_type                                               \
brw_inst_3src_a1_##reg##_type(const struct intel_device_info *devinfo,        \
                              const brw_inst *inst)                           \
{                                                                             \
   enum gfx10_align1_3src_exec_type exec_type =                               \
      (enum gfx10_align1_3src_exec_type) brw_inst_3src_a1_exec_type(devinfo,  \
                                                                    inst);    \
   unsigned hw_type = brw_inst_3src_a1_##reg##_hw_type(devinfo, inst);        \
   return brw_a1_hw_3src_type_to_reg_type(devinfo, hw_type, exec_type);       \
}

REG_TYPE(dst)
REG_TYPE(src0)
REG_TYPE(src1)
REG_TYPE(src2)
#undef REG_TYPE

/**
 * Three-source align1 instruction immediates:
 *  @{
 */
static inline uint16_t
brw_inst_3src_a1_src0_imm(ASSERTED const struct intel_device_info *devinfo,
                          const brw_inst *insn)
{
   assert(devinfo->ver >= 10);
   if (devinfo->ver >= 12)
      return brw_inst_bits(insn, 79, 64);
   else
      return brw_inst_bits(insn, 82, 67);
}

static inline uint16_t
brw_inst_3src_a1_src2_imm(ASSERTED const struct intel_device_info *devinfo,
                          const brw_inst *insn)
{
   assert(devinfo->ver >= 10);
   if (devinfo->ver >= 12)
      return brw_inst_bits(insn, 127, 112);
   else
      return brw_inst_bits(insn, 124, 109);
}

static inline void
brw_inst_set_3src_a1_src0_imm(ASSERTED const struct intel_device_info *devinfo,
                              brw_inst *insn, uint16_t value)
{
   assert(devinfo->ver >= 10);
   if (devinfo->ver >= 12)
      brw_inst_set_bits(insn, 79, 64, value);
   else
      brw_inst_set_bits(insn, 82, 67, value);
}

static inline void
brw_inst_set_3src_a1_src2_imm(ASSERTED const struct intel_device_info *devinfo,
                              brw_inst *insn, uint16_t value)
{
   assert(devinfo->ver >= 10);
   if (devinfo->ver >= 12)
      brw_inst_set_bits(insn, 127, 112, value);
   else
      brw_inst_set_bits(insn, 124, 109, value);
}
/** @} */

/**
 * Three-source systolic instructions:
 *  @{
 */
F(dpas_3src_src2_reg_nr,    /* 9+ */ -1, -1,   /* 12+ */ 127, 120)
F(dpas_3src_src2_subreg_nr, /* 9+ */ -1, -1,   /* 12+ */ 119, 115)
F(dpas_3src_src2_reg_file,  /* 9+ */ -1, -1,   /* 12+ */ 114, 114)
F(dpas_3src_src1_reg_nr,    /* 9+ */ -1, -1,   /* 12+ */ 111, 104)
F(dpas_3src_src1_subreg_nr, /* 9+ */ -1, -1,   /* 12+ */ 103, 99)
F(dpas_3src_src1_reg_file,  /* 9+ */ -1, -1,   /* 12+ */ 98,  98)
F(dpas_3src_src1_hw_type,   /* 9+ */ -1, -1,   /* 12+ */ 90,  88)
F(dpas_3src_src1_subbyte,   /* 9+ */ -1, -1,   /* 12+ */ 87,  86)
F(dpas_3src_src2_subbyte,   /* 9+ */ -1, -1,   /* 12+ */ 85,  84)
F(dpas_3src_src2_hw_type,   /* 9+ */ -1, -1,   /* 12+ */ 82,  80)
F(dpas_3src_src0_reg_nr,    /* 9+ */ -1, -1,   /* 12+ */ 79,  72)
F(dpas_3src_src0_subreg_nr, /* 9+ */ -1, -1,   /* 12+ */ 71,  67)
F(dpas_3src_src0_reg_file,  /* 9+ */ -1, -1,   /* 12+ */ 66,  66)
F(dpas_3src_dst_reg_nr,     /* 9+ */ -1, -1,   /* 12+ */ 63,  56)
F(dpas_3src_dst_subreg_nr,  /* 9+ */ -1, -1,   /* 12+ */ 55,  51)
F(dpas_3src_dst_reg_file,   /* 9+ */ -1, -1,   /* 12+ */ 50,  50)
F(dpas_3src_sdepth,         /* 9+ */ -1, -1,   /* 12+ */ 49,  48)
F(dpas_3src_rcount,         /* 9+ */ -1, -1,   /* 12+ */ 45,  43)
F(dpas_3src_src0_hw_type,   /* 9+ */ -1, -1,   /* 12+ */ 42,  40)
F(dpas_3src_exec_type,      /* 9+ */ -1, -1,   /* 12+ */ 39,  39)
F(dpas_3src_dst_hw_type,    /* 9+ */ -1, -1,   /* 12+ */ 38,  36)
/** @} */

#define REG_TYPE(reg)                                                         \
static inline void                                                            \
brw_inst_set_dpas_3src_##reg##_type(const struct intel_device_info *devinfo,  \
                                    brw_inst *inst, enum brw_reg_type type)   \
{                                                                             \
   UNUSED enum gfx10_align1_3src_exec_type exec_type =                        \
      (enum gfx10_align1_3src_exec_type) brw_inst_dpas_3src_exec_type(devinfo,\
                                                                      inst);  \
   if (brw_reg_type_is_floating_point(type)) {                                \
      assert(exec_type == BRW_ALIGN1_3SRC_EXEC_TYPE_FLOAT);                   \
   } else {                                                                   \
      assert(exec_type == BRW_ALIGN1_3SRC_EXEC_TYPE_INT);                     \
   }                                                                          \
   unsigned hw_type = brw_reg_type_to_a1_hw_3src_type(devinfo, type);         \
   brw_inst_set_dpas_3src_##reg##_hw_type(devinfo, inst, hw_type);            \
}                                                                             \
                                                                              \
static inline enum brw_reg_type                                               \
brw_inst_dpas_3src_##reg##_type(const struct intel_device_info *devinfo,      \
                              const brw_inst *inst)                           \
{                                                                             \
   enum gfx10_align1_3src_exec_type exec_type =                               \
      (enum gfx10_align1_3src_exec_type) brw_inst_dpas_3src_exec_type(devinfo,\
                                                                      inst);  \
   unsigned hw_type = brw_inst_dpas_3src_##reg##_hw_type(devinfo, inst);      \
   return brw_a1_hw_3src_type_to_reg_type(devinfo, hw_type, exec_type);       \
}

REG_TYPE(dst)
REG_TYPE(src0)
REG_TYPE(src1)
REG_TYPE(src2)
#undef REG_TYPE

/**
 * Flow control instruction bits:
 *  @{
 */
static inline void
brw_inst_set_uip(const struct intel_device_info *devinfo,
                 brw_inst *inst, int32_t value)
{
   if (devinfo->ver >= 12)
      brw_inst_set_src1_is_imm(devinfo, inst, 1);

   brw_inst_set_bits(inst, 95, 64, (uint32_t)value);
}

static inline int32_t
brw_inst_uip(const struct intel_device_info *devinfo, const brw_inst *inst)
{
   return brw_inst_bits(inst, 95, 64);
}

static inline void
brw_inst_set_jip(const struct intel_device_info *devinfo,
                 brw_inst *inst, int32_t value)
{
   if (devinfo->ver >= 12)
      brw_inst_set_src0_is_imm(devinfo, inst, 1);

   brw_inst_set_bits(inst, 127, 96, (uint32_t)value);
}

static inline int32_t
brw_inst_jip(const struct intel_device_info *devinfo, const brw_inst *inst)
{
   return brw_inst_bits(inst, 127, 96);
}
/** @} */

/**
 * SEND instructions:
 *  @{
 */
F(send_ex_desc_ia_subreg_nr,  /* 9+ */ 82, 80, /* 12+ */  42,  40)
F(send_src0_address_mode,     /* 9+ */ 79, 79, /* 12+ */  -1,  -1)
F(send_sel_reg32_desc,        /* 9+ */ 77, 77, /* 12+ */  48,  48)
F(send_sel_reg32_ex_desc,     /* 9+ */ 61, 61, /* 12+ */  49,  49)
F(send_src0_reg_file,         /* 9+ */ 42, 41, /* 12+ */   66, 66)
F(send_src1_reg_nr,           /* 9+ */ 51, 44, /* 12+ */ 111, 104)
FC(send_src1_len,             /* 9+ */ -1, -1, /* 12+ */ 103,  99, devinfo->verx10 >= 125)
F(send_src1_reg_file,         /* 9+ */ 36, 36, /* 12+ */  98,  98)
F(send_dst_reg_file,          /* 9+ */ 35, 35, /* 12+ */  50,  50)
FC(send_ex_bso,               /* 9+ */ -1, -1, /* 12+ */  39,  39, devinfo->verx10 >= 125)
/** @} */

/* Message descriptor bits */
#define MD(x) ((x) + 96)
#define MD12(x) ((x) >= 30 ? (x) - 30 + 122 :        \
                 (x) >= 25 ? (x) - 25 + 67 :         \
                 (x) >= 20 ? (x) - 20 + 51 :         \
                 (x) >= 11 ? (x) - 11 + 113 :        \
                 (x) - 0 + 81)

/**
 * Set the SEND(C) message descriptor immediate.
 *
 * This doesn't include the SFID nor the EOT field that were considered to be
 * part of the message descriptor by ancient versions of the BSpec, because
 * they are present in the instruction even if the message descriptor is
 * provided indirectly in the address register, so we want to specify them
 * separately.
 */
static inline void
brw_inst_set_send_desc(const struct intel_device_info *devinfo,
                       brw_inst *inst, uint32_t value)
{
   if (devinfo->ver >= 12) {
      brw_inst_set_bits(inst, 123, 122, GET_BITS(value, 31, 30));
      brw_inst_set_bits(inst, 71, 67, GET_BITS(value, 29, 25));
      brw_inst_set_bits(inst, 55, 51, GET_BITS(value, 24, 20));
      brw_inst_set_bits(inst, 121, 113, GET_BITS(value, 19, 11));
      brw_inst_set_bits(inst, 91, 81, GET_BITS(value, 10, 0));
   } else {
      brw_inst_set_bits(inst, 126, 96, value);
      assert(value >> 31 == 0);
   }
}

/**
 * Get the SEND(C) message descriptor immediate.
 *
 * \sa brw_inst_set_send_desc().
 */
static inline uint32_t
brw_inst_send_desc(const struct intel_device_info *devinfo,
                   const brw_inst *inst)
{
   if (devinfo->ver >= 12) {
      return (brw_inst_bits(inst, 123, 122) << 30 |
              brw_inst_bits(inst, 71, 67) << 25 |
              brw_inst_bits(inst, 55, 51) << 20 |
              brw_inst_bits(inst, 121, 113) << 11 |
              brw_inst_bits(inst, 91, 81));
   } else {
      return brw_inst_bits(inst, 126, 96);
   }
}

/**
 * Set the SEND(C) message extended descriptor immediate.
 *
 * This doesn't include the SFID nor the EOT field that were considered to be
 * part of the extended message descriptor by some versions of the BSpec,
 * because they are present in the instruction even if the extended message
 * descriptor is provided indirectly in a register, so we want to specify them
 * separately.
 */
static inline void
brw_inst_set_send_ex_desc(const struct intel_device_info *devinfo,
                          brw_inst *inst, uint32_t value)
{
   if (devinfo->ver >= 12) {
      brw_inst_set_bits(inst, 127, 124, GET_BITS(value, 31, 28));
      brw_inst_set_bits(inst, 97, 96, GET_BITS(value, 27, 26));
      brw_inst_set_bits(inst, 65, 64, GET_BITS(value, 25, 24));
      brw_inst_set_bits(inst, 47, 35, GET_BITS(value, 23, 11));
      brw_inst_set_bits(inst, 103, 99, GET_BITS(value, 10, 6));
      assert(GET_BITS(value, 5, 0) == 0);
   } else {
      assert(devinfo->ver >= 9);
      brw_inst_set_bits(inst, 94, 91, GET_BITS(value, 31, 28));
      brw_inst_set_bits(inst, 88, 85, GET_BITS(value, 27, 24));
      brw_inst_set_bits(inst, 83, 80, GET_BITS(value, 23, 20));
      brw_inst_set_bits(inst, 67, 64, GET_BITS(value, 19, 16));
      assert(GET_BITS(value, 15, 0) == 0);
   }
}

/**
 * Set the SENDS(C) message extended descriptor immediate.
 *
 * This doesn't include the SFID nor the EOT field that were considered to be
 * part of the extended message descriptor by some versions of the BSpec,
 * because they are present in the instruction even if the extended message
 * descriptor is provided indirectly in a register, so we want to specify them
 * separately.
 */
static inline void
brw_inst_set_sends_ex_desc(const struct intel_device_info *devinfo,
                           brw_inst *inst, uint32_t value)
{
   if (devinfo->ver >= 12) {
      brw_inst_set_send_ex_desc(devinfo, inst, value);
   } else {
      brw_inst_set_bits(inst, 95, 80, GET_BITS(value, 31, 16));
      assert(GET_BITS(value, 15, 10) == 0);
      brw_inst_set_bits(inst, 67, 64, GET_BITS(value, 9, 6));
      assert(GET_BITS(value, 5, 0) == 0);
   }
}

/**
 * Get the SEND(C) message extended descriptor immediate.
 *
 * \sa brw_inst_set_send_ex_desc().
 */
static inline uint32_t
brw_inst_send_ex_desc(const struct intel_device_info *devinfo,
                      const brw_inst *inst)
{
   if (devinfo->ver >= 12) {
      return (brw_inst_bits(inst, 127, 124) << 28 |
              brw_inst_bits(inst, 97, 96) << 26 |
              brw_inst_bits(inst, 65, 64) << 24 |
              brw_inst_bits(inst, 47, 35) << 11 |
              brw_inst_bits(inst, 103, 99) << 6);
   } else {
      assert(devinfo->ver >= 9);
      return (brw_inst_bits(inst, 94, 91) << 28 |
              brw_inst_bits(inst, 88, 85) << 24 |
              brw_inst_bits(inst, 83, 80) << 20 |
              brw_inst_bits(inst, 67, 64) << 16);
   }
}

/**
 * Get the SENDS(C) message extended descriptor immediate.
 *
 * \sa brw_inst_set_send_ex_desc().
 */
static inline uint32_t
brw_inst_sends_ex_desc(const struct intel_device_info *devinfo,
                       const brw_inst *inst)
{
   if (devinfo->ver >= 12) {
      return brw_inst_send_ex_desc(devinfo, inst);
   } else {
      return (brw_inst_bits(inst, 95, 80) << 16 |
              brw_inst_bits(inst, 67, 64) << 6);
   }
}

/**
 * Fields for SEND messages:
 *  @{
 */
F(eot,                 /* 9+ */ 127, 127,       /* 12+ */ 34, 34)
F(mlen,                /* 9+ */ 124, 121,       /* 12+ */ MD12(28), MD12(25))
F(rlen,                /* 9+ */ 120, 116,       /* 12+ */ MD12(24), MD12(20))
F(header_present,      /* 9+ */ 115, 115,       /* 12+ */ MD12(19), MD12(19))
F(gateway_notify,      /* 9+ */ MD(16), MD(15), /* 12+ */ -1, -1)
FD(function_control,   /* 9+ */ 114,  96,       /* 12+ */ MD12(18), MD12(11), MD12(10), MD12(0))
F(gateway_subfuncid,   /* 9+ */ MD(2), MD(0),   /* 12+ */ MD12(2),  MD12(0))
F(sfid,                /* 9+ */  27,  24,       /* 12+ */ 95, 92)
F(null_rt,             /* 9+ */  80,  80,       /* 12+ */ 44, 44) /* actually only Gfx11+ */
F(send_rta_index,      /* 9+ */  -1,  -1,       /* 12+ */  38,  36)
/** @} */

/**
 * URB message function control bits:
 *  @{
 */
F(urb_per_slot_offset,      /* 9+ */ MD(17), MD(17), /* 12+ */ MD12(17), MD12(17))
F(urb_channel_mask_present, /* 9+ */ MD(15), MD(15), /* 12+ */ MD12(15), MD12(15))
F(urb_swizzle_control,      /* 9+ */ MD(15), MD(15), /* 12+ */ -1, -1)
FD(urb_global_offset,       /* 9+ */ MD(14), MD(4),  /* 12+ */ MD12(14), MD12(11), MD12(10), MD12(4))
F(urb_opcode,               /* 9+ */ MD( 3), MD(0),  /* 12+ */ MD12(3), MD12(0))
/** @} */

/**
 * Sampler message function control bits:
 *  @{
 */
F(sampler_simd_mode,      /* 9+ */ MD(18), MD(17), /* 12+ */ MD12(18), MD12(17))
F(sampler_msg_type,       /* 9+ */ MD(16), MD(12), /* 12+ */ MD12(16), MD12(12))
FD(sampler,               /* 9+ */ MD(11), MD(8),  /* 12+ */ MD12(11), MD12(11), MD12(10), MD12(8))
F(binding_table_index,    /* 9+ */ MD(7), MD(0),   /* 12+ */ MD12(7), MD12(0)) /* also used by other messages */
/** @} */

/**
 * Data port message function control bits:
 *  @{
 */
F(dp_category,            /* 9+ */ MD(18), MD(18), /* 12+ */ MD12(18), MD12(18))

F(dp_read_msg_type,       /* 9+ */ MD(17), MD(14), /* 12+ */ MD12(17), MD12(14))
F(dp_write_msg_type,      /* 9+ */ MD(17), MD(14), /* 12+ */ MD12(17), MD12(14))
FD(dp_read_msg_control,   /* 9+ */ MD(13), MD( 8), /* 12+ */ MD12(13), MD12(11), MD12(10), MD12(8))
FD(dp_write_msg_control,  /* 9+ */ MD(13), MD( 8), /* 12+ */ MD12(13), MD12(11), MD12(10), MD12(8))

F(dp_msg_type,            /* 9+ */ MD(18), MD(14), /* 12+ */ MD12(18), MD12(14))
FD(dp_msg_control,        /* 9+ */ MD(13), MD( 8), /* 12+ */ MD12(13), MD12(11), MD12(10), MD12(8))
/** @} */

/**
 * Scratch message bits:
 *  @{
 */
F(scratch_read_write,  /* 9+ */ MD(17), MD(17), /* 12+ */ MD12(17), MD12(17)) /* 0 = read,  1 = write */
F(scratch_type,        /* 9+ */ MD(16), MD(16), /* 12+ */ -1, -1) /* 0 = OWord, 1 = DWord */
F(scratch_invalidate_after_read, /* 9+ */ MD(15), MD(15), /* 12+ */ MD12(15), MD12(15))
F(scratch_block_size,  /* 9+ */ MD(13), MD(12), /* 12+ */ MD12(13), MD12(12))
FD(scratch_addr_offset,
   /* 9:   */ MD(11), MD(0),
   /* 12:  */ MD12(11), MD12(11), MD12(10), MD12(0))
/** @} */

/**
 * Render Target message function control bits:
 *  @{
 */
F(rt_last,             /* 9+ */ MD(12), MD(12),  /* 12+ */ MD12(12), MD12(12))
F(rt_slot_group,       /* 9+ */ MD(11),  MD(11), /* 12+ */ MD12(11), MD12(11))
F(rt_message_type,     /* 9+ */ MD(10),  MD( 8), /* 12+ */ MD12(10), MD12(8))
/** @} */

/**
 * Pixel Interpolator message function control bits:
 *  @{
 */
F(pi_simd_mode,        /* 9+ */ MD(16),  MD(16), /* 12+ */ MD12(16), MD12(16))
F(pi_nopersp,          /* 9+ */ MD(14),  MD(14), /* 12+ */ MD12(14), MD12(14))
F(pi_message_type,     /* 9+ */ MD(13),  MD(12), /* 12+ */ MD12(13), MD12(12))
F(pi_slot_group,       /* 9+ */ MD(11),  MD(11), /* 12+ */ MD12(11), MD12(11))
F(pi_message_data,     /* 9+ */ MD(7),   MD(0),  /* 12+ */  MD12(7), MD12(0))
/** @} */

/**
 * Immediates:
 *  @{
 */
static inline int
brw_inst_imm_d(const struct intel_device_info *devinfo, const brw_inst *insn)
{
   (void) devinfo;
   return brw_inst_bits(insn, 127, 96);
}

static inline unsigned
brw_inst_imm_ud(const struct intel_device_info *devinfo, const brw_inst *insn)
{
   (void) devinfo;
   return brw_inst_bits(insn, 127, 96);
}

static inline uint64_t
brw_inst_imm_uq(const struct intel_device_info *devinfo,
                const brw_inst *insn)
{
   if (devinfo->ver >= 12) {
      return brw_inst_bits(insn, 95, 64) << 32 |
             brw_inst_bits(insn, 127, 96);
   } else {
      return brw_inst_bits(insn, 127, 64);
   }
}

static inline float
brw_inst_imm_f(const struct intel_device_info *devinfo, const brw_inst *insn)
{
   union {
      float f;
      uint32_t u;
   } ft;
   (void) devinfo;
   ft.u = brw_inst_bits(insn, 127, 96);
   return ft.f;
}

static inline double
brw_inst_imm_df(const struct intel_device_info *devinfo, const brw_inst *insn)
{
   union {
      double d;
      uint64_t u;
   } dt;
   dt.u = brw_inst_imm_uq(devinfo, insn);
   return dt.d;
}

static inline void
brw_inst_set_imm_d(const struct intel_device_info *devinfo,
                   brw_inst *insn, int value)
{
   (void) devinfo;
   return brw_inst_set_bits(insn, 127, 96, value);
}

static inline void
brw_inst_set_imm_ud(const struct intel_device_info *devinfo,
                    brw_inst *insn, unsigned value)
{
   (void) devinfo;
   return brw_inst_set_bits(insn, 127, 96, value);
}

static inline void
brw_inst_set_imm_f(const struct intel_device_info *devinfo,
                   brw_inst *insn, float value)
{
   union {
      float f;
      uint32_t u;
   } ft;
   (void) devinfo;
   ft.f = value;
   brw_inst_set_bits(insn, 127, 96, ft.u);
}

static inline void
brw_inst_set_imm_df(const struct intel_device_info *devinfo,
                    brw_inst *insn, double value)
{
   union {
      double d;
      uint64_t u;
   } dt;
   (void) devinfo;
   dt.d = value;

   if (devinfo->ver >= 12) {
      brw_inst_set_bits(insn, 95, 64, dt.u >> 32);
      brw_inst_set_bits(insn, 127, 96, dt.u & 0xFFFFFFFF);
   } else {
      brw_inst_set_bits(insn, 127, 64, dt.u);
   }
}

static inline void
brw_inst_set_imm_uq(const struct intel_device_info *devinfo,
                    brw_inst *insn, uint64_t value)
{
   (void) devinfo;
   if (devinfo->ver >= 12) {
      brw_inst_set_bits(insn, 95, 64, value >> 32);
      brw_inst_set_bits(insn, 127, 96, value & 0xFFFFFFFF);
   } else {
      brw_inst_set_bits(insn, 127, 64, value);
   }
}

/** @} */

#define REG_TYPE(reg)                                                         \
static inline void                                                            \
brw_inst_set_##reg##_file_type(const struct intel_device_info *devinfo,       \
                               brw_inst *inst, enum brw_reg_file file,        \
                               enum brw_reg_type type)                        \
{                                                                             \
   assert(file <= BRW_IMMEDIATE_VALUE);                                       \
   unsigned hw_type = brw_reg_type_to_hw_type(devinfo, file, type);           \
   brw_inst_set_##reg##_reg_file(devinfo, inst, file);                        \
   brw_inst_set_##reg##_reg_hw_type(devinfo, inst, hw_type);                  \
}                                                                             \
                                                                              \
static inline enum brw_reg_type                                               \
brw_inst_##reg##_type(const struct intel_device_info *devinfo,                \
                      const brw_inst *inst)                                   \
{                                                                             \
   unsigned file = __builtin_strcmp("dst", #reg) == 0 ?                       \
                   (unsigned) BRW_GENERAL_REGISTER_FILE :                     \
                   brw_inst_##reg##_reg_file(devinfo, inst);                  \
   unsigned hw_type = brw_inst_##reg##_reg_hw_type(devinfo, inst);            \
   return brw_hw_type_to_reg_type(devinfo, (enum brw_reg_file)file, hw_type); \
}

REG_TYPE(dst)
REG_TYPE(src0)
REG_TYPE(src1)
#undef REG_TYPE


/* The AddrImm fields are split into two discontiguous sections on Gfx9+ */
#define BRW_IA1_ADDR_IMM(reg, g9_nine, g9_high, g9_low,                  \
                         g12_high, g12_low, g20_high, g20_low, g20_zero) \
static inline void                                                       \
brw_inst_set_##reg##_ia1_addr_imm(const struct                           \
                                  intel_device_info *devinfo,            \
                                  brw_inst *inst,                        \
                                  unsigned value)                        \
{                                                                        \
   if (devinfo->ver >= 20) {                                             \
      assert((value & ~0x7ff) == 0);                                     \
      brw_inst_set_bits(inst, g20_high, g20_low, value >> 1);            \
      if (g20_zero == -1)                                                \
         assert((value & 1) == 0);                                       \
      else                                                               \
         brw_inst_set_bits(inst, g20_zero, g20_zero, value & 1);         \
   } else if (devinfo->ver >= 12) {                                      \
      assert((value & ~0x3ff) == 0);                                     \
      brw_inst_set_bits(inst, g12_high, g12_low, value);                 \
   } else {                                                              \
      assert((value & ~0x3ff) == 0);                                     \
      brw_inst_set_bits(inst, g9_high, g9_low, value & 0x1ff);           \
      brw_inst_set_bits(inst, g9_nine, g9_nine, value >> 9);             \
   }                                                                     \
}                                                                        \
static inline unsigned                                                   \
brw_inst_##reg##_ia1_addr_imm(const struct intel_device_info *devinfo,   \
                              const brw_inst *inst)                      \
{                                                                        \
   if (devinfo->ver >= 20) {                                             \
      return brw_inst_bits(inst, g20_high, g20_low) << 1 |               \
             (g20_zero == -1 ? 0 :                                       \
              brw_inst_bits(inst, g20_zero, g20_zero));                  \
   } else if (devinfo->ver >= 12) {                                      \
      return brw_inst_bits(inst, g12_high, g12_low);                     \
   } else {                                                              \
      return brw_inst_bits(inst, g9_high, g9_low) |                      \
             (brw_inst_bits(inst, g9_nine, g9_nine) << 9);               \
   }                                                                     \
}

/* AddrImm for Align1 Indirect Addressing                 */
/*                     ----Gfx9----  -Gfx12-  ---Gfx20--- */
BRW_IA1_ADDR_IMM(src1, 121, 104, 96, 107, 98, 107, 98, -1)
BRW_IA1_ADDR_IMM(src0,  95,  72, 64,  75, 66,  75, 66, 87)
BRW_IA1_ADDR_IMM(dst,   47,  56, 48,  59, 50,  59, 50, 33)

#define BRW_IA16_ADDR_IMM(reg, g9_nine, g9_high, g9_low)                  \
static inline void                                                        \
brw_inst_set_##reg##_ia16_addr_imm(const struct                           \
                                   intel_device_info *devinfo,            \
                                   brw_inst *inst, unsigned value)        \
{                                                                         \
   assert(devinfo->ver < 12);                                             \
   assert((value & ~0x3ff) == 0);                                         \
   assert(GET_BITS(value, 3, 0) == 0);                                    \
   brw_inst_set_bits(inst, g9_high, g9_low, GET_BITS(value, 8, 4));       \
   brw_inst_set_bits(inst, g9_nine, g9_nine, GET_BITS(value, 9, 9));      \
}                                                                         \
static inline unsigned                                                    \
brw_inst_##reg##_ia16_addr_imm(const struct intel_device_info *devinfo,   \
                               const brw_inst *inst)                      \
{                                                                         \
   assert(devinfo->ver < 12);                                             \
   return (brw_inst_bits(inst, g9_high, g9_low) << 4) |                   \
          (brw_inst_bits(inst, g9_nine, g9_nine) << 9);                   \
}

/* AddrImm[9:0] for Align16 Indirect Addressing:
 * Compared to Align1, these are missing the low 4 bits.
 *                             ----Gfx9----
 */
BRW_IA16_ADDR_IMM(src1,        121, 104, 100)
BRW_IA16_ADDR_IMM(src0,         95,  72,  68)
BRW_IA16_ADDR_IMM(dst,          47,  56,  52)
BRW_IA16_ADDR_IMM(send_src0,    78,  72,  68)
BRW_IA16_ADDR_IMM(send_dst,     62,  56,  52)

/**
 * Fetch a set of contiguous bits from the instruction.
 *
 * Bits indices range from 0..127; fields may not cross 64-bit boundaries.
 */
static inline uint64_t
brw_inst_bits(const brw_inst *inst, unsigned high, unsigned low)
{
   assume(high < 128);
   assume(high >= low);
   /* We assume the field doesn't cross 64-bit boundaries. */
   const unsigned word = high / 64;
   assert(word == low / 64);

   high %= 64;
   low %= 64;

   const uint64_t mask = (~0ull >> (64 - (high - low + 1)));

   return (inst->data[word] >> low) & mask;
}

/**
 * Set bits in the instruction, with proper shifting and masking.
 *
 * Bits indices range from 0..127; fields may not cross 64-bit boundaries.
 */
static inline void
brw_inst_set_bits(brw_inst *inst, unsigned high, unsigned low, uint64_t value)
{
   assume(high < 128);
   assume(high >= low);
   const unsigned word = high / 64;
   assert(word == low / 64);

   high %= 64;
   low %= 64;

   const uint64_t mask = (~0ull >> (64 - (high - low + 1))) << low;

   /* Make sure the supplied value actually fits in the given bitfield. */
   assert((value & (mask >> low)) == value);

   inst->data[word] = (inst->data[word] & ~mask) | (value << low);
}

#undef BRW_IA16_ADDR_IMM
#undef BRW_IA1_ADDR_IMM
#undef MD
#undef F
#undef FC
#undef F20
#undef FD20

typedef struct {
   uint64_t data;
} brw_compact_inst;

/**
 * Fetch a set of contiguous bits from the compacted instruction.
 *
 * Bits indices range from 0..63.
 */
static inline unsigned
brw_compact_inst_bits(const brw_compact_inst *inst, unsigned high, unsigned low)
{
   assume(high < 64);
   assume(high >= low);
   const uint64_t mask = (1ull << (high - low + 1)) - 1;

   return (inst->data >> low) & mask;
}

/**
 * Set bits in the compacted instruction.
 *
 * Bits indices range from 0..63.
 */
static inline void
brw_compact_inst_set_bits(brw_compact_inst *inst, unsigned high, unsigned low,
                          uint64_t value)
{
   assume(high < 64);
   assume(high >= low);
   const uint64_t mask = ((1ull << (high - low + 1)) - 1) << low;

   /* Make sure the supplied value actually fits in the given bitfield. */
   assert((value & (mask >> low)) == value);

   inst->data = (inst->data & ~mask) | (value << low);
}

#define FC(name, hi9, lo9, hi12, lo12, assertions)                 \
static inline void                                                 \
brw_compact_inst_set_##name(const struct                           \
                            intel_device_info *devinfo,            \
                            brw_compact_inst *inst, unsigned v)    \
{                                                                  \
   assert(assertions);                                             \
   if (devinfo->ver >= 12)                                         \
      brw_compact_inst_set_bits(inst, hi12, lo12, v);              \
   else                                                            \
      brw_compact_inst_set_bits(inst, hi9, lo9, v);                \
}                                                                  \
static inline unsigned                                             \
brw_compact_inst_##name(const struct intel_device_info *devinfo,   \
                        const brw_compact_inst *inst)              \
{                                                                  \
   assert(assertions);                                             \
   if (devinfo->ver >= 12)                                         \
      return brw_compact_inst_bits(inst, hi12, lo12);              \
   else                                                            \
      return brw_compact_inst_bits(inst, hi9, lo9);                \
}

/* A simple macro for fields which stay in the same place on all generations
 * except for Gfx12.
 */
#define F(name, hi9, lo9, hi12, lo12) FC(name, hi9, lo9, hi12, lo12, true)

/* A macro for fields which moved to several different locations
 * across generations.
 */
#define F20(name, hi9, lo9, hi12, lo12, hi20, lo20)                \
static inline void                                                 \
brw_compact_inst_set_##name(const struct                           \
                            intel_device_info *devinfo,            \
                            brw_compact_inst *inst, unsigned v)    \
{                                                                  \
   if (devinfo->ver >= 20)                                         \
      brw_compact_inst_set_bits(inst, hi20, lo20, v);              \
   else if (devinfo->ver >= 12)                                    \
      brw_compact_inst_set_bits(inst, hi12, lo12, v);              \
   else                                                            \
      brw_compact_inst_set_bits(inst, hi9, lo9, v);                \
}                                                                  \
static inline unsigned                                             \
brw_compact_inst_##name(const struct intel_device_info *devinfo,   \
                        const brw_compact_inst *inst)              \
{                                                                  \
   if (devinfo->ver >= 20)                                         \
      return brw_compact_inst_bits(inst, hi20, lo20);              \
   else if (devinfo->ver >= 12)                                    \
      return brw_compact_inst_bits(inst, hi12, lo12);              \
   else                                                            \
      return brw_compact_inst_bits(inst, hi9, lo9);                \
}

/* A macro for fields which gained extra discontiguous bits in Gfx20
 * (specified by hi20ex-lo20ex).
 */
#define FD20(name, hi9, lo9, hi12, lo12,                                \
             hi20, lo20, hi20ex, lo20ex)                                \
   static inline void                                                   \
brw_compact_inst_set_##name(const struct                                \
                            intel_device_info *devinfo,                 \
                            brw_compact_inst *inst, unsigned v)         \
{                                                                       \
   if (devinfo->ver >= 20) {                                            \
      const unsigned k = hi20 - lo20 + 1;                               \
      brw_compact_inst_set_bits(inst, hi20ex, lo20ex, v >> k);          \
      brw_compact_inst_set_bits(inst, hi20, lo20, v & ((1u << k) - 1)); \
   } else if (devinfo->ver >= 12) {                                     \
      brw_compact_inst_set_bits(inst, hi12, lo12, v);                   \
   } else {                                                             \
      brw_compact_inst_set_bits(inst, hi9, lo9, v);                     \
   }                                                                    \
}                                                                       \
static inline unsigned                                                  \
brw_compact_inst_##name(const struct intel_device_info *devinfo,        \
                        const brw_compact_inst *inst)                   \
{                                                                       \
   if (devinfo->ver >= 20) {                                            \
      const unsigned k = hi20 - lo20 + 1;                               \
      return (brw_compact_inst_bits(inst, hi20ex, lo20ex) << k |        \
              brw_compact_inst_bits(inst, hi20, lo20));                 \
   } else if (devinfo->ver >= 12) {                                     \
      return brw_compact_inst_bits(inst, hi12, lo12);                   \
   } else {                                                             \
      return brw_compact_inst_bits(inst, hi9, lo9);                     \
   }                                                                    \
}

F(src1_reg_nr,       /* 9+ */ 63, 56, /* 12+ */ 63, 56)
F(src0_reg_nr,       /* 9+ */ 55, 48, /* 12+ */ 47, 40)
F20(dst_reg_nr,      /* 9+ */ 47, 40, /* 12+ */ 23, 16, /* 20+ */ 39, 32)
F(src1_index,        /* 9+ */ 39, 35, /* 12+ */ 55, 52)
F20(src0_index,      /* 9+ */ 34, 30, /* 12+ */ 51, 48, /* 20+ */ 25, 23)
F(cmpt_control,      /* 9+ */ 29, 29, /* 12+ */ 29, 29) /* Same location as brw_inst */
F(cond_modifier,     /* 9+ */ 27, 24, /* 12+ */ -1, -1) /* Same location as brw_inst */
F(acc_wr_control,    /* 9+ */ 23, 23, /* 12+ */ -1, -1)
F20(subreg_index,    /* 9+ */ 22, 18, /* 12+ */ 39, 35, /* 20+ */ 51, 48)
FD20(datatype_index, /* 9+ */ 17, 13, /* 12+ */ 34, 30, /* 20+ */ 28, 26, 31, 30)
F20(control_index,   /* 9+ */ 12,  8, /* 12+ */ 28, 24, /* 20+ */ 22, 18)
F20(swsb,            /* 9+ */ -1, -1, /* 12+ */ 15,  8, /* 20+ */ 17,  8)
F(debug_control,     /* 9+ */  7,  7, /* 12+ */  7,  7)
F(hw_opcode,         /* 9+ */  6,  0, /* 12+ */  6,  0) /* Same location as brw_inst */

static inline unsigned
brw_compact_inst_imm(const struct intel_device_info *devinfo,
                     const brw_compact_inst *inst)
{
   if (devinfo->ver >= 12) {
      return brw_compact_inst_bits(inst, 63, 52);
   } else {
      return (brw_compact_inst_bits(inst, 39, 35) << 8) |
             (brw_compact_inst_bits(inst, 63, 56));
   }
}

/**
 * Compacted three-source instructions:
 *  @{
 */
F(3src_src2_reg_nr,     /* 9+ */ 63, 57, /* 12+ */ 55, 48)
F(3src_src1_reg_nr,     /* 9+ */ 56, 50, /* 12+ */ 63, 56)
F(3src_src0_reg_nr,     /* 9+ */ 49, 43, /* 12+ */ 47, 40)
F(3src_src2_subreg_nr,  /* 9+ */ 42, 40, /* 12+ */ -1, -1)
F(3src_src1_subreg_nr,  /* 9+ */ 39, 37, /* 12+ */ -1, -1)
F(3src_src0_subreg_nr,  /* 9+ */ 36, 34, /* 12+ */ -1, -1)
F(3src_src2_rep_ctrl,   /* 9+ */ 33, 33, /* 12+ */ -1, -1)
F(3src_src1_rep_ctrl,   /* 9+ */ 32, 32, /* 12+ */ -1, -1)
F(3src_saturate,        /* 9+ */ 31, 31, /* 12+ */ -1, -1)
F(3src_debug_control,   /* 9+ */ 30, 30, /* 12+ */  7,  7)
F(3src_cmpt_control,    /* 9+ */ 29, 29, /* 12+ */ 29, 29)
F(3src_src0_rep_ctrl,   /* 9+ */ 28, 28, /* 12+ */ -1, -1)
/* Reserved */
F20(3src_dst_reg_nr,    /* 9+ */ 18, 12, /* 12+ */ 23, 16, /* 20+ */ 39, 32)
F20(3src_source_index,  /* 9+ */ 11, 10, /* 12+ */ 34, 30, /* 20+ */ 25, 22)
FD20(3src_subreg_index, /* 9+ */ -1, -1, /* 12+ */ 39, 35, /* 20+ */ 28, 26, 31, 30)
F20(3src_control_index, /* 9+ */  9,  8, /* 12+ */ 28, 24, /* 20+ */ 21, 18)
F20(3src_swsb,          /* 9+ */ -1, -1, /* 12+ */ 15,  8, /* 20+ */ 17,  8)
/* Bit 7 is Reserved (for future Opcode expansion) */
F(3src_hw_opcode,       /* 9+ */  6,  0, /* 12+ */  6,  0)
/** @} */

#undef F

static inline void
brw_inst_set_opcode(const struct brw_isa_info *isa,
                    struct brw_inst *inst, enum opcode opcode)
{
   brw_inst_set_hw_opcode(isa->devinfo, inst, brw_opcode_encode(isa, opcode));
}

static inline enum opcode
brw_inst_opcode(const struct brw_isa_info *isa,
                const struct brw_inst *inst)
{
   return brw_opcode_decode(isa, brw_inst_hw_opcode(isa->devinfo, inst));
}

#ifdef __cplusplus
}
#endif

#endif
