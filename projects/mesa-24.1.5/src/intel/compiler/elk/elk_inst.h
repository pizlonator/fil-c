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
 * @file elk_inst.h
 *
 * A representation of i965 EU assembly instructions, with helper methods to
 * get and set various fields.  This is the actual hardware format.
 */

#ifndef ELK_INST_H
#define ELK_INST_H

#include <assert.h>
#include <stdint.h>

#include "elk_eu_defines.h"
#include "elk_isa_info.h"
#include "elk_reg_type.h"
#include "dev/intel_device_info.h"

#ifdef __cplusplus
extern "C" {
#endif

/* elk_context.h has a forward declaration of elk_inst, so name the struct. */
typedef struct elk_inst {
   uint64_t data[2];
} elk_inst;

static inline uint64_t elk_inst_bits(const elk_inst *inst,
                                     unsigned high, unsigned low);
static inline void elk_inst_set_bits(elk_inst *inst,
                                     unsigned high, unsigned low,
                                     uint64_t value);

#define FC(name, hi4, lo4, assertions)                        \
static inline void                                            \
elk_inst_set_##name(const struct intel_device_info *devinfo,  \
                    elk_inst *inst, uint64_t v)               \
{                                                             \
   assert(assertions);                                        \
   elk_inst_set_bits(inst, hi4, lo4, v);                      \
}                                                             \
static inline uint64_t                                        \
elk_inst_##name(const struct intel_device_info *devinfo,      \
                const elk_inst *inst)                         \
{                                                             \
   assert(assertions);                                        \
   return elk_inst_bits(inst, hi4, lo4);                      \
}

/* A simple macro for fields which stay in the same place on all generations. */
#define F(name, hi4, lo4) FC(name, hi4, lo4, true)

#define BOUNDS(hi4, lo4, hi45, lo45, hi5, lo5, hi6, lo6,                     \
               hi7, lo7, hi8, lo8)                                           \
   unsigned high, low;                                                       \
   if (devinfo->ver >= 8) {                                                  \
      high = hi8;  low = lo8;                                                \
   } else if (devinfo->ver >= 7) {                                           \
      high = hi7;  low = lo7;                                                \
   } else if (devinfo->ver >= 6) {                                           \
      high = hi6;  low = lo6;                                                \
   } else if (devinfo->ver >= 5) {                                           \
      high = hi5;  low = lo5;                                                \
   } else if (devinfo->verx10 >= 45) {                                       \
      high = hi45; low = lo45;                                               \
   } else {                                                                  \
      high = hi4;  low = lo4;                                                \
   }                                                                         \
   assert(((int) high) != -1 && ((int) low) != -1);

/* A general macro for cases where the field has moved to several different
 * bit locations across generations.  GCC appears to combine cases where the
 * bits are identical, removing some of the inefficiency.
 */
#define FF(name, hi4, lo4, hi45, lo45, hi5, lo5, hi6, lo6,                    \
           hi7, lo7, hi8, lo8)                                                \
static inline void                                                            \
elk_inst_set_##name(const struct intel_device_info *devinfo,                  \
                    elk_inst *inst, uint64_t value)                           \
{                                                                             \
   BOUNDS(hi4, lo4, hi45, lo45, hi5, lo5, hi6, lo6,                           \
          hi7, lo7, hi8, lo8)                                                 \
   elk_inst_set_bits(inst, high, low, value);                                 \
}                                                                             \
static inline uint64_t                                                        \
elk_inst_##name(const struct intel_device_info *devinfo, const elk_inst *inst)\
{                                                                             \
   BOUNDS(hi4, lo4, hi45, lo45, hi5, lo5, hi6, lo6,                           \
          hi7, lo7, hi8, lo8)                                                 \
   return elk_inst_bits(inst, high, low);                                     \
}

/* A macro for fields which moved as of Gfx8+. */
#define F8(name, gfx4_high, gfx4_low, gfx8_high, gfx8_low) \
FF(name,                                                   \
   /* 4:   */ gfx4_high, gfx4_low,                         \
   /* 4.5: */ gfx4_high, gfx4_low,                         \
   /* 5:   */ gfx4_high, gfx4_low,                         \
   /* 6:   */ gfx4_high, gfx4_low,                         \
   /* 7:   */ gfx4_high, gfx4_low,                         \
   /* 8:   */ gfx8_high, gfx8_low);

F(src1_vstride,        /* 4+ */ 120, 117)
F(src1_width,          /* 4+ */ 116, 114)
F(src1_da16_swiz_w,    /* 4+ */ 115, 114)
F(src1_da16_swiz_z,    /* 4+ */ 113, 112)
F(src1_hstride,        /* 4+ */ 113, 112)
F(src1_address_mode,   /* 4+ */ 111, 111)
/** Src1.SrcMod @{ */
F(src1_negate,         /* 4+ */ 110, 110)
F(src1_abs,            /* 4+ */ 109, 109)
/** @} */
F8(src1_ia_subreg_nr,  /* 4+ */ 108, 106, /* 8+ */  108, 105)
F(src1_da_reg_nr,      /* 4+ */ 108, 101)
F(src1_da16_subreg_nr, /* 4+ */ 100, 100)
F(src1_da1_subreg_nr,  /* 4+ */ 100, 96)
F(src1_da16_swiz_y,    /* 4+ */ 99,  98)
F(src1_da16_swiz_x,    /* 4+ */ 97,  96)
F8(src1_reg_hw_type,   /* 4+ */ 46,  44,  /* 8+ */  94,  91)
F8(src1_reg_file,      /* 4+ */ 43,  42,  /* 8+ */  90,  89)
F(src0_vstride,        /* 4+ */ 88,  85)
F(src0_width,          /* 4+ */ 84,  82)
F(src0_da16_swiz_w,    /* 4+ */ 83,  82)
F(src0_da16_swiz_z,    /* 4+ */ 81,  80)
F(src0_hstride,        /* 4+ */ 81,  80)
F(src0_address_mode,   /* 4+ */ 79,  79)
/** Src0.SrcMod @{ */
F(src0_negate,         /* 4+ */ 78,  78)
F(src0_abs,            /* 4+ */ 77,  77)
/** @} */
F8(src0_ia_subreg_nr,  /* 4+ */ 76,  74,  /* 8+ */  76,  73)
F(src0_da_reg_nr,      /* 4+ */ 76,  69)
F(src0_da16_subreg_nr, /* 4+ */ 68,  68)
F(src0_da1_subreg_nr,  /* 4+ */ 68,  64)
F(src0_da16_swiz_y,    /* 4+ */ 67,  66)
F(src0_da16_swiz_x,    /* 4+ */ 65,  64)
F(dst_address_mode,    /* 4+ */ 63,  63)
F(dst_hstride,         /* 4+ */ 62,  61)
F8(dst_ia_subreg_nr,   /* 4+ */ 60,  58,  /* 8+ */  60,  57)
F(dst_da_reg_nr,       /* 4+ */ 60,  53)
F(dst_da16_subreg_nr,  /* 4+ */ 52,  52)
F(dst_da1_subreg_nr,   /* 4+ */ 52,  48)
F(da16_writemask,      /* 4+ */ 51,  48) /* Dst.ChanEn */
F8(src0_reg_hw_type,   /* 4+ */ 41,  39,  /* 8+ */  46,  43)
F8(src0_reg_file,      /* 4+ */ 38,  37,  /* 8+ */  42,  41)
F(src0_is_imm,         /* 4+ */ -1,  -1)
F8(dst_reg_hw_type,    /* 4+ */ 36,  34,  /* 8+ */  40,  37)
F8(dst_reg_file,       /* 4+ */ 33,  32,  /* 8+ */  36,  35)
F8(mask_control,       /* 4+ */  9,   9,  /* 8+ */  34,  34)
FF(flag_reg_nr,
   /* 4-6: doesn't exist */ -1, -1, -1, -1, -1, -1, -1, -1,
   /* 7: */ 90, 90,
   /* 8: */ 33, 33)
F8(flag_subreg_nr,     /* 4+ */ 89,  89,  /* 8+ */  32,  32)
F(saturate,            /* 4+ */ 31,  31)
F(debug_control,       /* 4+ */ 30,  30)
F(cmpt_control,        /* 4+ */ 29,  29)
FC(branch_control,     /* 4+ */ 28,  28, devinfo->ver >= 8)
FC(acc_wr_control,     /* 4+ */ 28,  28, devinfo->ver >= 6)
FC(mask_control_ex,    /* 4+ */ 28,  28, devinfo->verx10 == 45 ||
                                         devinfo->ver == 5)
F(cond_modifier,       /* 4+ */ 27,  24)
FC(math_function,      /* 4+ */ 27,  24, devinfo->ver >= 6)
F(exec_size,           /* 4+ */ 23,  21)
F(pred_inv,            /* 4+ */ 20,  20)
F(pred_control,        /* 4+ */ 19,  16)
F(thread_control,      /* 4+ */ 15,  14)
F(atomic_control,      /* 4+ */ -1,  -1)
F(qtr_control,         /* 4+ */ 13,  12)
FF(nib_control,
   /* 4-6: doesn't exist */ -1, -1, -1, -1, -1, -1, -1, -1,
   /* 7: */ 47, 47,
   /* 8: */ 11, 11);
F8(no_dd_check,        /* 4+ */  11, 11,  /* 8+ */  10,  10)
F8(no_dd_clear,        /* 4+ */  10, 10,  /* 8+ */   9,   9)
F(access_mode,         /* 4+ */   8,  8)
/* Bit 7 is Reserved (for future Opcode expansion) */
F(hw_opcode,           /* 4+ */   6,  0)

/**
 * Three-source instructions:
 *  @{
 */
F(3src_src2_reg_nr,         /* 4+ */ 125, 118) /* same in align1 */
F(3src_a16_src2_subreg_nr,  /* 4+ */ 117, 115) /* Extra discontiguous bit on CHV? */
F(3src_a16_src2_swizzle,    /* 4+ */ 114, 107)
F(3src_a16_src2_rep_ctrl,   /* 4+ */ 106, 106)
F(3src_src1_reg_nr,         /* 4+ */ 104,  97) /* same in align1 */
F(3src_a16_src1_subreg_nr,  /* 4+ */ 96,  94) /* Extra discontiguous bit on CHV? */
F(3src_a16_src1_swizzle,    /* 4+ */ 93,  86)
F(3src_a16_src1_rep_ctrl,   /* 4+ */ 85,  85)
F(3src_src0_reg_nr,         /* 4+ */ 83,  76) /* same in align1 */
F(3src_a16_src0_subreg_nr,  /* 4+ */ 75,  73) /* Extra discontiguous bit on CHV? */
F(3src_a16_src0_swizzle,    /* 4+ */ 72,  65)
F(3src_a16_src0_rep_ctrl,   /* 4+ */ 64,  64)
F(3src_dst_reg_nr,          /* 4+ */ 63,  56) /* same in align1 */
F(3src_a16_dst_subreg_nr,   /* 4+ */ 55,  53)
F(3src_a16_dst_writemask,   /* 4+ */ 52,  49)
F8(3src_a16_nib_ctrl,       /* 4+ */ 47, 47,   /* 8+ */  11, 11) /* only exists on IVB+ */
F8(3src_a16_dst_hw_type,    /* 4+ */ 45, 44,   /* 8+ */  48, 46) /* only exists on IVB+ */
F8(3src_a16_src_hw_type,    /* 4+ */ 43, 42,   /* 8+ */  45, 43)
F8(3src_src2_negate,        /* 4+ */ 41, 41,   /* 8+ */  42, 42)
F8(3src_src2_abs,           /* 4+ */ 40, 40,   /* 8+ */  41, 41)
F8(3src_src1_negate,        /* 4+ */ 39, 39,   /* 8+ */  40, 40)
F8(3src_src1_abs,           /* 4+ */ 38, 38,   /* 8+ */  39, 39)
F8(3src_src0_negate,        /* 4+ */ 37, 37,   /* 8+ */  38, 38)
F8(3src_src0_abs,           /* 4+ */ 36, 36,   /* 8+ */  37, 37)
F8(3src_a16_src1_type,      /* 4+ */ -1, -1,   /* 8+ */  36, 36)
F8(3src_a16_src2_type,      /* 4+ */ -1, -1,   /* 8+ */  35, 35)
F8(3src_a16_flag_reg_nr,    /* 4+ */ 34, 34,   /* 8+ */  33, 33)
F8(3src_a16_flag_subreg_nr, /* 4+ */ 33, 33,   /* 8+ */  32, 32)
FF(3src_a16_dst_reg_file,
   /* 4-5: doesn't exist - no 3-source instructions */ -1, -1, -1, -1, -1, -1,
   /* 6: */ 32, 32,
   /* 7-8: doesn't exist - no MRFs */ -1, -1, -1, -1)
F(3src_saturate,            /* 4+ */ 31, 31)
F(3src_debug_control,       /* 4+ */ 30, 30)
F(3src_cmpt_control,        /* 4+ */ 29, 29)
F(3src_acc_wr_control,      /* 4+ */ 28, 28)
F(3src_cond_modifier,       /* 4+ */ 27, 24)
F(3src_exec_size,           /* 4+ */ 23, 21)
F(3src_pred_inv,            /* 4+ */ 20, 20)
F(3src_pred_control,        /* 4+ */ 19, 16)
F(3src_thread_control,      /* 4+ */ 15, 14)
F(3src_qtr_control,         /* 4+ */ 13, 12)
F8(3src_no_dd_check,        /* 4+ */ 11, 11,   /* 8+ */  10, 10)
F8(3src_no_dd_clear,        /* 4+ */ 10, 10,   /* 8+ */   9,  9)
F8(3src_mask_control,       /* 4+ */ 9,  9,    /* 8+ */  34, 34)
F(3src_access_mode,         /* 4+ */ 8,  8)
/* Bit 7 is Reserved (for future Opcode expansion) */
F(3src_hw_opcode,           /* 4+ */ 6,  0)
/** @} */

#define REG_TYPE(reg)                                                         \
static inline void                                                            \
elk_inst_set_3src_a16_##reg##_type(const struct intel_device_info *devinfo,   \
                                   elk_inst *inst, enum elk_reg_type type)    \
{                                                                             \
   unsigned hw_type = elk_reg_type_to_a16_hw_3src_type(devinfo, type);        \
   elk_inst_set_3src_a16_##reg##_hw_type(devinfo, inst, hw_type);             \
}                                                                             \
                                                                              \
static inline enum elk_reg_type                                               \
elk_inst_3src_a16_##reg##_type(const struct intel_device_info *devinfo,       \
                               const elk_inst *inst)                          \
{                                                                             \
   unsigned hw_type = elk_inst_3src_a16_##reg##_hw_type(devinfo, inst);       \
   return elk_a16_hw_3src_type_to_reg_type(devinfo, hw_type);                 \
}

REG_TYPE(dst)
REG_TYPE(src)
#undef REG_TYPE

/**
 * Three-source align1 instructions:
 *  @{
 */
F(3src_a1_src2_subreg_nr,    /* 4+ */   117, 113)
F(3src_a1_src1_subreg_nr,    /* 4+ */   96,  92)
F(3src_a1_src0_subreg_nr,    /* 4+ */   75,  71)
F8(3src_a1_src2_reg_file,    /* 4+ */   -1,  -1,  /* 8+ */  45, 45)

/** @} */

/**
 * Flow control instruction bits:
 *  @{
 */
static inline void
elk_inst_set_uip(const struct intel_device_info *devinfo,
                 elk_inst *inst, int32_t value)
{
   assert(devinfo->ver >= 6);

   if (devinfo->ver >= 8) {
      elk_inst_set_bits(inst, 95, 64, (uint32_t)value);
   } else {
      assert(value <= (1 << 16) - 1);
      assert(value > -(1 << 16));
      elk_inst_set_bits(inst, 127, 112, (uint16_t)value);
   }
}

static inline int32_t
elk_inst_uip(const struct intel_device_info *devinfo, const elk_inst *inst)
{
   assert(devinfo->ver >= 6);

   if (devinfo->ver >= 8) {
      return elk_inst_bits(inst, 95, 64);
   } else {
      return (int16_t)elk_inst_bits(inst, 127, 112);
   }
}

static inline void
elk_inst_set_jip(const struct intel_device_info *devinfo,
                 elk_inst *inst, int32_t value)
{
   assert(devinfo->ver >= 6);

   if (devinfo->ver >= 8) {
      elk_inst_set_bits(inst, 127, 96, (uint32_t)value);
   } else {
      assert(value <= (1 << 15) - 1);
      assert(value >= -(1 << 15));
      elk_inst_set_bits(inst, 111, 96, (uint16_t)value);
   }
}

static inline int32_t
elk_inst_jip(const struct intel_device_info *devinfo, const elk_inst *inst)
{
   assert(devinfo->ver >= 6);

   if (devinfo->ver >= 8) {
      return elk_inst_bits(inst, 127, 96);
   } else {
      return (int16_t)elk_inst_bits(inst, 111, 96);
   }
}

/** Like FC, but using int16_t to handle negative jump targets. */
#define FJ(name, high, low, assertions)                                       \
static inline void                                                            \
elk_inst_set_##name(const struct intel_device_info *devinfo, elk_inst *inst, int16_t v) \
{                                                                             \
   assert(assertions);                                                        \
   (void) devinfo;                                                            \
   elk_inst_set_bits(inst, high, low, (uint16_t) v);                          \
}                                                                             \
static inline int16_t                                                         \
elk_inst_##name(const struct intel_device_info *devinfo, const elk_inst *inst)\
{                                                                             \
   assert(assertions);                                                        \
   (void) devinfo;                                                            \
   return elk_inst_bits(inst, high, low);                                     \
}

FJ(gfx6_jump_count,  63,  48, devinfo->ver == 6)
FJ(gfx4_jump_count, 111,  96, devinfo->ver < 6)
FC(gfx4_pop_count,  /* 4+ */ 115, 112, devinfo->ver < 6)
/** @} */

/**
 * SEND instructions:
 *  @{
 */
F8(send_src0_reg_file,        /* 4+ */ 38, 37, /* 8+ */   42,  41)
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
elk_inst_set_send_desc(const struct intel_device_info *devinfo,
                       elk_inst *inst, uint32_t value)
{
   if (devinfo->ver >= 5) {
      elk_inst_set_bits(inst, 124, 96, value);
      assert(value >> 29 == 0);
   } else {
      elk_inst_set_bits(inst, 119, 96, value);
      assert(value >> 24 == 0);
   }
}

/**
 * Get the SEND(C) message descriptor immediate.
 *
 * \sa elk_inst_set_send_desc().
 */
static inline uint32_t
elk_inst_send_desc(const struct intel_device_info *devinfo,
                   const elk_inst *inst)
{
   if (devinfo->ver >= 5) {
      return elk_inst_bits(inst, 124, 96);
   } else {
      return elk_inst_bits(inst, 119, 96);
   }
}

/**
 * Fields for SEND messages:
 *  @{
 */
F(eot,                 /* 4+ */ 127, 127)
FF(mlen,
   /* 4:   */ 119, 116,
   /* 4.5: */ 119, 116,
   /* 5:   */ 124, 121,
   /* 6:   */ 124, 121,
   /* 7:   */ 124, 121,
   /* 8:   */ 124, 121)
FF(rlen,
   /* 4:   */ 115, 112,
   /* 4.5: */ 115, 112,
   /* 5:   */ 120, 116,
   /* 6:   */ 120, 116,
   /* 7:   */ 120, 116,
   /* 8:   */ 120, 116)
FF(header_present,
   /* 4: doesn't exist */ -1, -1, -1, -1,
   /* 5:   */ 115, 115,
   /* 6:   */ 115, 115,
   /* 7:   */ 115, 115,
   /* 8:   */ 115, 115)
F(gateway_notify, /* 4+ */ MD(16), MD(15))
FF(function_control,
   /* 4:   */ 111,  96,
   /* 4.5: */ 111,  96,
   /* 5:   */ 114,  96,
   /* 6:   */ 114,  96,
   /* 7:   */ 114,  96,
   /* 8:   */ 114,  96)
FF(gateway_subfuncid,
   /* 4:   */ MD(1), MD(0),
   /* 4.5: */ MD(1), MD(0),
   /* 5:   */ MD(1), MD(0), /* 2:0, but bit 2 is reserved MBZ */
   /* 6:   */ MD(2), MD(0),
   /* 7:   */ MD(2), MD(0),
   /* 8:   */ MD(2), MD(0))
FF(sfid,
   /* 4:   */ 123, 120, /* called msg_target */
   /* 4.5  */ 123, 120,
   /* 5:   */  95,  92,
   /* 6:   */  27,  24,
   /* 7:   */  27,  24,
   /* 8:   */  27,  24)
F8(null_rt,    /* 4+ */ -1,  -1, /* 8+ */  80, 80)
FC(base_mrf,   /* 4+ */ 27,  24, devinfo->ver < 6);
/** @} */

/**
 * URB message function control bits:
 *  @{
 */
FF(urb_per_slot_offset,
   /* 4-6: */ -1, -1, -1, -1, -1, -1, -1, -1,
   /* 7:   */ MD(16), MD(16),
   /* 8:   */ MD(17), MD(17))
FC(urb_channel_mask_present, /* 4+ */ MD(15), MD(15), devinfo->ver >= 8)
FC(urb_complete, /* 4+ */ MD(15), MD(15), devinfo->ver < 8)
FC(urb_used,     /* 4+ */ MD(14), MD(14), devinfo->ver < 7)
FC(urb_allocate, /* 4+ */ MD(13), MD(13), devinfo->ver < 7)
FF(urb_swizzle_control,
   /* 4:   */ MD(11), MD(10),
   /* 4.5: */ MD(11), MD(10),
   /* 5:   */ MD(11), MD(10),
   /* 6:   */ MD(11), MD(10),
   /* 7:   */ MD(14), MD(14),
   /* 8:   */ MD(15), MD(15))
FF(urb_global_offset,
   /* 4:   */ MD( 9), MD(4),
   /* 4.5: */ MD( 9), MD(4),
   /* 5:   */ MD( 9), MD(4),
   /* 6:   */ MD( 9), MD(4),
   /* 7:   */ MD(13), MD(3),
   /* 8:   */ MD(14), MD(4))
FF(urb_opcode,
   /* 4:   */ MD( 3), MD(0),
   /* 4.5: */ MD( 3), MD(0),
   /* 5:   */ MD( 3), MD(0),
   /* 6:   */ MD( 3), MD(0),
   /* 7:   */ MD( 2), MD(0),
   /* 8:   */ MD( 3), MD(0))
/** @} */

/**
 * Gfx4-5 math messages:
 *  @{
 */
FC(math_msg_data_type,  /* 4+ */ MD(7), MD(7), devinfo->ver < 6)
FC(math_msg_saturate,   /* 4+ */ MD(6), MD(6), devinfo->ver < 6)
FC(math_msg_precision,  /* 4+ */ MD(5), MD(5), devinfo->ver < 6)
FC(math_msg_signed_int, /* 4+ */ MD(4), MD(4), devinfo->ver < 6)
FC(math_msg_function,   /* 4+ */ MD(3), MD(0), devinfo->ver < 6)
/** @} */

/**
 * Sampler message function control bits:
 *  @{
 */
FF(sampler_simd_mode,
   /* 4: doesn't exist */ -1, -1, -1, -1,
   /* 5:   */ MD(17), MD(16),
   /* 6:   */ MD(17), MD(16),
   /* 7:   */ MD(18), MD(17),
   /* 8:   */ MD(18), MD(17))
FF(sampler_msg_type,
   /* 4:   */ MD(15), MD(14),
   /* 4.5: */ MD(15), MD(12),
   /* 5:   */ MD(15), MD(12),
   /* 6:   */ MD(15), MD(12),
   /* 7:   */ MD(16), MD(12),
   /* 8:   */ MD(16), MD(12))
FC(sampler_return_format, /* 4+ */ MD(13), MD(12), devinfo->verx10 == 40)
FF(sampler,
   /* 4:   */ MD(11), MD(8),
   /* 4.5: */ MD(11), MD(8),
   /* 5:   */ MD(11), MD(8),
   /* 6:   */ MD(11), MD(8),
   /* 7:   */ MD(11), MD(8),
   /* 8:   */ MD(11), MD(8))
F(binding_table_index,    /* 4+ */ MD(7), MD(0)) /* also used by other messages */
/** @} */

/**
 * Data port message function control bits:
 *  @{
 */
FC(dp_category,           /* 4+ */ MD(18), MD(18), devinfo->ver >= 7)

/* Gfx4-5 store fields in different bits for read/write messages. */
FF(dp_read_msg_type,
   /* 4:   */ MD(13), MD(12),
   /* 4.5: */ MD(13), MD(11),
   /* 5:   */ MD(13), MD(11),
   /* 6:   */ MD(16), MD(13),
   /* 7:   */ MD(17), MD(14),
   /* 8:   */ MD(17), MD(14))
FF(dp_write_msg_type,
   /* 4:   */ MD(14), MD(12),
   /* 4.5: */ MD(14), MD(12),
   /* 5:   */ MD(14), MD(12),
   /* 6:   */ MD(16), MD(13),
   /* 7:   */ MD(17), MD(14),
   /* 8:   */ MD(17), MD(14))
FF(dp_read_msg_control,
   /* 4:   */ MD(11), MD( 8),
   /* 4.5: */ MD(10), MD( 8),
   /* 5:   */ MD(10), MD( 8),
   /* 6:   */ MD(12), MD( 8),
   /* 7:   */ MD(13), MD( 8),
   /* 8:   */ MD(13), MD( 8))
FF(dp_write_msg_control,
   /* 4:   */ MD(11), MD( 8),
   /* 4.5: */ MD(11), MD( 8),
   /* 5:   */ MD(11), MD( 8),
   /* 6:   */ MD(12), MD( 8),
   /* 7:   */ MD(13), MD( 8),
   /* 8:   */ MD(13), MD( 8))
FC(dp_read_target_cache, /* 4+ */ MD(15), MD(14), devinfo->ver < 6);

FF(dp_write_commit,
   /* 4:   */ MD(15),  MD(15),
   /* 4.5: */ MD(15),  MD(15),
   /* 5:   */ MD(15),  MD(15),
   /* 6:   */ MD(17),  MD(17),
   /* 7+: does not exist */ -1, -1, -1, -1)

/* Gfx6+ use the same bit locations for everything. */
FF(dp_msg_type,
   /* 4-5: use dp_read_msg_type or dp_write_msg_type instead */
   -1, -1, -1, -1, -1, -1,
   /* 6:   */ MD(16), MD(13),
   /* 7:   */ MD(17), MD(14),
   /* 8:   */ MD(18), MD(14))
FF(dp_msg_control,
   /* 4:   */ MD(11), MD( 8),
   /* 4.5-5: use dp_read_msg_control or dp_write_msg_control */ -1, -1, -1, -1,
   /* 6:   */ MD(12), MD( 8),
   /* 7:   */ MD(13), MD( 8),
   /* 8:   */ MD(13), MD( 8))
/** @} */

/**
 * Scratch message bits (Gfx7+):
 *  @{
 */
FC(scratch_read_write, /* 4+ */ MD(17), MD(17), devinfo->ver >= 7) /* 0 = read,  1 = write */
FC(scratch_type,       /* 4+ */ MD(16), MD(16), devinfo->ver >= 7) /* 0 = OWord, 1 = DWord */
FC(scratch_invalidate_after_read, /* 4+ */ MD(15), MD(15), devinfo->ver >= 7)
FC(scratch_block_size, /* 4+ */ MD(13), MD(12), devinfo->ver >= 7)
FF(scratch_addr_offset,
   /* 4:   */ -1, -1,
   /* 4.5: */ -1, -1,
   /* 5:   */ -1, -1,
   /* 6:   */ -1, -1,
   /* 7:   */ MD(11), MD(0),
   /* 8:   */ MD(11), MD(0))
/** @} */

/**
 * Render Target message function control bits:
 *  @{
 */
FF(rt_last,
   /* 4:   */ MD(11), MD(11),
   /* 4.5: */ MD(11), MD(11),
   /* 5:   */ MD(11), MD(11),
   /* 6:   */ MD(12), MD(12),
   /* 7:   */ MD(12), MD(12),
   /* 8:   */ MD(12), MD(12))
FC(rt_slot_group,      /* 4+ */ MD(11),  MD(11), devinfo->ver >= 6)
F(rt_message_type,     /* 4+ */ MD(10),  MD( 8))
/** @} */

/**
 * Thread Spawn message function control bits:
 *  @{
 */
F(ts_resource_select,  /* 4+ */ MD( 4),   MD( 4))
F(ts_request_type,     /* 4+ */ MD( 1),   MD( 1))
F(ts_opcode,            /* 4+ */ MD( 0),  MD( 0))
/** @} */

/**
 * Pixel Interpolator message function control bits:
 *  @{
 */
F(pi_simd_mode,        /* 4+ */ MD(16),  MD(16))
F(pi_nopersp,          /* 4+ */ MD(14),  MD(14))
F(pi_message_type,     /* 4+ */ MD(13),  MD(12))
F(pi_slot_group,       /* 4+ */ MD(11),  MD(11))
F(pi_message_data,     /* 4+ */ MD(7),   MD(0))
/** @} */

/**
 * Immediates:
 *  @{
 */
static inline int
elk_inst_imm_d(const struct intel_device_info *devinfo, const elk_inst *insn)
{
   (void) devinfo;
   return elk_inst_bits(insn, 127, 96);
}

static inline unsigned
elk_inst_imm_ud(const struct intel_device_info *devinfo, const elk_inst *insn)
{
   (void) devinfo;
   return elk_inst_bits(insn, 127, 96);
}

static inline uint64_t
elk_inst_imm_uq(const struct intel_device_info *devinfo,
                const elk_inst *insn)
{
   assert(devinfo->ver >= 8);
   return elk_inst_bits(insn, 127, 64);
}

static inline float
elk_inst_imm_f(const struct intel_device_info *devinfo, const elk_inst *insn)
{
   union {
      float f;
      uint32_t u;
   } ft;
   (void) devinfo;
   ft.u = elk_inst_bits(insn, 127, 96);
   return ft.f;
}

static inline double
elk_inst_imm_df(const struct intel_device_info *devinfo, const elk_inst *insn)
{
   union {
      double d;
      uint64_t u;
   } dt;
   dt.u = elk_inst_imm_uq(devinfo, insn);
   return dt.d;
}

static inline void
elk_inst_set_imm_d(const struct intel_device_info *devinfo,
                   elk_inst *insn, int value)
{
   (void) devinfo;
   return elk_inst_set_bits(insn, 127, 96, value);
}

static inline void
elk_inst_set_imm_ud(const struct intel_device_info *devinfo,
                    elk_inst *insn, unsigned value)
{
   (void) devinfo;
   return elk_inst_set_bits(insn, 127, 96, value);
}

static inline void
elk_inst_set_imm_f(const struct intel_device_info *devinfo,
                   elk_inst *insn, float value)
{
   union {
      float f;
      uint32_t u;
   } ft;
   (void) devinfo;
   ft.f = value;
   elk_inst_set_bits(insn, 127, 96, ft.u);
}

static inline void
elk_inst_set_imm_df(const struct intel_device_info *devinfo,
                    elk_inst *insn, double value)
{
   union {
      double d;
      uint64_t u;
   } dt;
   (void) devinfo;
   dt.d = value;

   elk_inst_set_bits(insn, 127, 64, dt.u);
}

static inline void
elk_inst_set_imm_uq(const struct intel_device_info *devinfo,
                    elk_inst *insn, uint64_t value)
{
   (void) devinfo;
   elk_inst_set_bits(insn, 127, 64, value);
}

/** @} */

#define REG_TYPE(reg)                                                         \
static inline void                                                            \
elk_inst_set_##reg##_file_type(const struct intel_device_info *devinfo,       \
                               elk_inst *inst, enum elk_reg_file file,        \
                               enum elk_reg_type type)                        \
{                                                                             \
   assert(file <= ELK_IMMEDIATE_VALUE);                                       \
   unsigned hw_type = elk_reg_type_to_hw_type(devinfo, file, type);           \
   elk_inst_set_##reg##_reg_file(devinfo, inst, file);                        \
   elk_inst_set_##reg##_reg_hw_type(devinfo, inst, hw_type);                  \
}                                                                             \
                                                                              \
static inline enum elk_reg_type                                               \
elk_inst_##reg##_type(const struct intel_device_info *devinfo,                \
                      const elk_inst *inst)                                   \
{                                                                             \
   unsigned file = __builtin_strcmp("dst", #reg) == 0 ?                       \
                   (unsigned) ELK_GENERAL_REGISTER_FILE :                     \
                   elk_inst_##reg##_reg_file(devinfo, inst);                  \
   unsigned hw_type = elk_inst_##reg##_reg_hw_type(devinfo, inst);            \
   return elk_hw_type_to_reg_type(devinfo, (enum elk_reg_file)file, hw_type); \
}

REG_TYPE(dst)
REG_TYPE(src0)
REG_TYPE(src1)
#undef REG_TYPE


/* The AddrImm fields are split into two discontiguous sections on Gfx8+ */
#define ELK_IA1_ADDR_IMM(reg, g4_high, g4_low, g8_nine, g8_high, g8_low) \
static inline void                                                       \
elk_inst_set_##reg##_ia1_addr_imm(const struct                           \
                                  intel_device_info *devinfo,            \
                                  elk_inst *inst,                        \
                                  unsigned value)                        \
{                                                                        \
   if (devinfo->ver >= 8) {                                              \
      assert((value & ~0x3ff) == 0);                                     \
      elk_inst_set_bits(inst, g8_high, g8_low, value & 0x1ff);           \
      elk_inst_set_bits(inst, g8_nine, g8_nine, value >> 9);             \
   } else {                                                              \
      assert((value & ~0x3ff) == 0);                                     \
      elk_inst_set_bits(inst, g4_high, g4_low, value);                   \
   }                                                                     \
}                                                                        \
static inline unsigned                                                   \
elk_inst_##reg##_ia1_addr_imm(const struct intel_device_info *devinfo,   \
                              const elk_inst *inst)                      \
{                                                                        \
   if (devinfo->ver >= 8) {                                              \
      return elk_inst_bits(inst, g8_high, g8_low) |                      \
             (elk_inst_bits(inst, g8_nine, g8_nine) << 9);               \
   } else {                                                              \
      return elk_inst_bits(inst, g4_high, g4_low);                       \
   }                                                                     \
}

/* AddrImm for Align1 Indirect Addressing    */
/*                     -Gen 4-  ----Gfx8---- */
ELK_IA1_ADDR_IMM(src1, 105, 96, 121, 104, 96)
ELK_IA1_ADDR_IMM(src0,  73, 64,  95,  72, 64)
ELK_IA1_ADDR_IMM(dst,   57, 48,  47,  56, 48)

#define ELK_IA16_ADDR_IMM(reg, g4_high, g4_low, g8_nine, g8_high, g8_low) \
static inline void                                                        \
elk_inst_set_##reg##_ia16_addr_imm(const struct                           \
                                   intel_device_info *devinfo,            \
                                   elk_inst *inst, unsigned value)        \
{                                                                         \
   assert((value & ~0x3ff) == 0);                                         \
   if (devinfo->ver >= 8) {                                               \
      assert(GET_BITS(value, 3, 0) == 0);                                 \
      elk_inst_set_bits(inst, g8_high, g8_low, GET_BITS(value, 8, 4));    \
      elk_inst_set_bits(inst, g8_nine, g8_nine, GET_BITS(value, 9, 9));   \
   } else {                                                               \
      elk_inst_set_bits(inst, g4_high, g4_low, value);                    \
   }                                                                      \
}                                                                         \
static inline unsigned                                                    \
elk_inst_##reg##_ia16_addr_imm(const struct intel_device_info *devinfo,   \
                               const elk_inst *inst)                      \
{                                                                         \
   if (devinfo->ver >= 8) {                                               \
      return (elk_inst_bits(inst, g8_high, g8_low) << 4) |                \
             (elk_inst_bits(inst, g8_nine, g8_nine) << 9);                \
   } else {                                                               \
      return elk_inst_bits(inst, g4_high, g4_low);                        \
   }                                                                      \
}

/* AddrImm[9:0] for Align16 Indirect Addressing:
 * Compared to Align1, these are missing the low 4 bits.
 *                     -Gen 4-  ----Gfx8----
 */
ELK_IA16_ADDR_IMM(src1,       105, 96, 121, 104, 100)
ELK_IA16_ADDR_IMM(src0,        73, 64,  95,  72,  68)
ELK_IA16_ADDR_IMM(dst,         57, 52,  47,  56,  52)
ELK_IA16_ADDR_IMM(send_src0,   -1, -1,  78,  72,  68)
ELK_IA16_ADDR_IMM(send_dst,    -1, -1,  62,  56,  52)

/**
 * Fetch a set of contiguous bits from the instruction.
 *
 * Bits indices range from 0..127; fields may not cross 64-bit boundaries.
 */
static inline uint64_t
elk_inst_bits(const elk_inst *inst, unsigned high, unsigned low)
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
elk_inst_set_bits(elk_inst *inst, unsigned high, unsigned low, uint64_t value)
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

#undef ELK_IA16_ADDR_IMM
#undef ELK_IA1_ADDR_IMM
#undef MD
#undef F8
#undef FF
#undef BOUNDS
#undef F
#undef FC
#undef F20
#undef FD20

typedef struct {
   uint64_t data;
} elk_compact_inst;

/**
 * Fetch a set of contiguous bits from the compacted instruction.
 *
 * Bits indices range from 0..63.
 */
static inline unsigned
elk_compact_inst_bits(const elk_compact_inst *inst, unsigned high, unsigned low)
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
elk_compact_inst_set_bits(elk_compact_inst *inst, unsigned high, unsigned low,
                          uint64_t value)
{
   assume(high < 64);
   assume(high >= low);
   const uint64_t mask = ((1ull << (high - low + 1)) - 1) << low;

   /* Make sure the supplied value actually fits in the given bitfield. */
   assert((value & (mask >> low)) == value);

   inst->data = (inst->data & ~mask) | (value << low);
}

#define FC(name, high, low, assertions)                            \
static inline void                                                 \
elk_compact_inst_set_##name(const struct                           \
                            intel_device_info *devinfo,            \
                            elk_compact_inst *inst, unsigned v)    \
{                                                                  \
   assert(assertions);                                             \
   elk_compact_inst_set_bits(inst, high, low, v);                  \
}                                                                  \
static inline unsigned                                             \
elk_compact_inst_##name(const struct intel_device_info *devinfo,   \
                        const elk_compact_inst *inst)              \
{                                                                  \
   assert(assertions);                                             \
   return elk_compact_inst_bits(inst, high, low);                  \
}

/* A simple macro for fields which stay in the same place on all generations. */
#define F(name, high, low) FC(name, high, low, true)

/* A macro for fields which moved as of Gfx8+. */
#define F8(name, high, low, hi8, lo8)                              \
static inline void                                                 \
elk_compact_inst_set_##name(const struct                           \
                            intel_device_info *devinfo,            \
                            elk_compact_inst *inst, unsigned v)    \
{                                                                  \
   if (devinfo->ver >= 8)                                          \
      elk_compact_inst_set_bits(inst, hi8, lo8, v);                \
   else                                                            \
      elk_compact_inst_set_bits(inst, high, low, v);               \
}                                                                  \
static inline unsigned                                             \
elk_compact_inst_##name(const struct intel_device_info *devinfo,   \
                        const elk_compact_inst *inst)              \
{                                                                  \
   if (devinfo->ver >= 8)                                          \
      return elk_compact_inst_bits(inst, hi8, lo8);                \
   else                                                            \
      return elk_compact_inst_bits(inst, high, low);               \
}

F(src1_reg_nr,       /* 4+ */ 63, 56)
F(src0_reg_nr,       /* 4+ */ 55, 48)
F8(dst_reg_nr,       /* 4+ */ 47, 40, /*  8+ */ 47, 40)
F(src1_index,        /* 4+ */ 39, 35)
F8(src0_index,       /* 4+ */ 34, 30, /*  8+ */ 34, 30)
F(cmpt_control,      /* 4+ */ 29, 29) /* Same location as elk_inst */
FC(flag_subreg_nr,   /* 4+ */ 28, 28, devinfo->ver <= 6)
F(cond_modifier,     /* 4+ */ 27, 24) /* Same location as elk_inst */
FC(acc_wr_control,   /* 4+ */ 23, 23, devinfo->ver >= 6)
FC(mask_control_ex,  /* 4+ */ 23, 23, devinfo->verx10 == 45 || devinfo->ver == 5)
F8(subreg_index,     /* 4+ */ 22, 18, /*  8+ */ 22, 18)
F8(datatype_index,   /* 4+ */ 17, 13, /*  8+ */ 17, 13)
F8(control_index,    /* 4+ */ 12,  8, /*  8+ */ 12,  8)
F(debug_control,     /* 4+ */  7,  7)
F(hw_opcode,         /* 4+ */  6,  0) /* Same location as elk_inst */

static inline unsigned
elk_compact_inst_imm(const struct intel_device_info *devinfo,
                     const elk_compact_inst *inst)
{
   return (elk_compact_inst_bits(inst, 39, 35) << 8) |
          (elk_compact_inst_bits(inst, 63, 56));
}

/**
 * (Gfx8+) Compacted three-source instructions:
 *  @{
 */
FC(3src_src2_reg_nr,    /* 4+ */ 63, 57, devinfo->ver >= 8)
FC(3src_src1_reg_nr,    /* 4+ */ 56, 50, devinfo->ver >= 8)
FC(3src_src0_reg_nr,    /* 4+ */ 49, 43, devinfo->ver >= 8)
FC(3src_src2_subreg_nr, /* 4+ */ 42, 40, devinfo->ver >= 8)
FC(3src_src1_subreg_nr, /* 4+ */ 39, 37, devinfo->ver >= 8)
FC(3src_src0_subreg_nr, /* 4+ */ 36, 34, devinfo->ver >= 8)
FC(3src_src2_rep_ctrl,  /* 4+ */ 33, 33, devinfo->ver >= 8)
FC(3src_src1_rep_ctrl,  /* 4+ */ 32, 32, devinfo->ver >= 8)
FC(3src_saturate,       /* 4+ */ 31, 31, devinfo->ver >= 8)
FC(3src_debug_control,  /* 4+ */ 30, 30, devinfo->ver >= 8)
FC(3src_cmpt_control,   /* 4+ */ 29, 29, devinfo->ver >= 8)
FC(3src_src0_rep_ctrl,  /* 4+ */ 28, 28, devinfo->ver >= 8)
/* Reserved */
F8(3src_dst_reg_nr,     /* 4+ */ 18, 12, /*  8+ */ 18, 12)
F8(3src_source_index,   /* 4+ */ -1, -1, /*  8+ */ 11, 10)
F8(3src_control_index,  /* 4+ */ -1, -1, /*  8+ */  9,  8)
/* Bit 7 is Reserved (for future Opcode expansion) */
FC(3src_hw_opcode,      /* 4+ */  6,  0, devinfo->ver >= 8)
/** @} */

#undef F

static inline void
elk_inst_set_opcode(const struct elk_isa_info *isa,
                    struct elk_inst *inst, enum elk_opcode opcode)
{
   elk_inst_set_hw_opcode(isa->devinfo, inst, elk_opcode_encode(isa, opcode));
}

static inline enum elk_opcode
elk_inst_opcode(const struct elk_isa_info *isa,
                const struct elk_inst *inst)
{
   return elk_opcode_decode(isa, elk_inst_hw_opcode(isa->devinfo, inst));
}

#ifdef __cplusplus
}
#endif

#endif
