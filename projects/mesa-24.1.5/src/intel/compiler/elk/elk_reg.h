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

/** @file elk_reg.h
 *
 * This file defines struct elk_reg, which is our representation for EU
 * registers.  They're not a hardware specific format, just an abstraction
 * that intends to capture the full flexibility of the hardware registers.
 *
 * The elk_eu_emit.c layer's elk_set_dest/elk_set_src[01] functions encode
 * the abstract elk_reg type into the actual hardware instruction encoding.
 */

#ifndef ELK_REG_H
#define ELK_REG_H

#include <stdbool.h>
#include "util/compiler.h"
#include "util/glheader.h"
#include "util/macros.h"
#include "util/rounding.h"
#include "util/u_math.h"
#include "elk_eu_defines.h"
#include "elk_reg_type.h"

#ifdef __cplusplus
extern "C" {
#endif

struct intel_device_info;

/** Size of general purpose register space in REG_SIZE units */
#define ELK_MAX_GRF 128
#define XE2_MAX_GRF 256

/**
 * First GRF used for the MRF hack.
 *
 * On gfx7, MRFs are no longer used, and contiguous GRFs are used instead.  We
 * haven't converted our compiler to be aware of this, so it asks for MRFs and
 * elk_eu_emit.c quietly converts them to be accesses of the top GRFs.  The
 * register allocators have to be careful of this to avoid corrupting the "MRF"s
 * with actual GRF allocations.
 */
#define GFX7_MRF_HACK_START 112

/**
 * BRW hardware swizzles.
 * Only defines XYZW to ensure it can be contained in 2 bits
 */
#define ELK_SWIZZLE_X 0
#define ELK_SWIZZLE_Y 1
#define ELK_SWIZZLE_Z 2
#define ELK_SWIZZLE_W 3

/** Number of message register file registers */
#define ELK_MAX_MRF(gen) (gen == 6 ? 24 : 16)

#define ELK_SWIZZLE4(a,b,c,d) (((a)<<0) | ((b)<<2) | ((c)<<4) | ((d)<<6))
#define ELK_GET_SWZ(swz, idx) (((swz) >> ((idx)*2)) & 0x3)

#define ELK_SWIZZLE_NOOP      ELK_SWIZZLE4(0,1,2,3)
#define ELK_SWIZZLE_XYZW      ELK_SWIZZLE4(0,1,2,3)
#define ELK_SWIZZLE_XXXX      ELK_SWIZZLE4(0,0,0,0)
#define ELK_SWIZZLE_YYYY      ELK_SWIZZLE4(1,1,1,1)
#define ELK_SWIZZLE_ZZZZ      ELK_SWIZZLE4(2,2,2,2)
#define ELK_SWIZZLE_WWWW      ELK_SWIZZLE4(3,3,3,3)
#define ELK_SWIZZLE_XYXY      ELK_SWIZZLE4(0,1,0,1)
#define ELK_SWIZZLE_YXYX      ELK_SWIZZLE4(1,0,1,0)
#define ELK_SWIZZLE_XZXZ      ELK_SWIZZLE4(0,2,0,2)
#define ELK_SWIZZLE_YZXW      ELK_SWIZZLE4(1,2,0,3)
#define ELK_SWIZZLE_YWYW      ELK_SWIZZLE4(1,3,1,3)
#define ELK_SWIZZLE_ZXYW      ELK_SWIZZLE4(2,0,1,3)
#define ELK_SWIZZLE_ZWZW      ELK_SWIZZLE4(2,3,2,3)
#define ELK_SWIZZLE_WZWZ      ELK_SWIZZLE4(3,2,3,2)
#define ELK_SWIZZLE_WZYX      ELK_SWIZZLE4(3,2,1,0)
#define ELK_SWIZZLE_XXZZ      ELK_SWIZZLE4(0,0,2,2)
#define ELK_SWIZZLE_YYWW      ELK_SWIZZLE4(1,1,3,3)
#define ELK_SWIZZLE_YXWZ      ELK_SWIZZLE4(1,0,3,2)

#define ELK_SWZ_COMP_INPUT(comp) (ELK_SWIZZLE_XYZW >> ((comp)*2))
#define ELK_SWZ_COMP_OUTPUT(comp) (ELK_SWIZZLE_XYZW << ((comp)*2))

static inline bool
elk_is_single_value_swizzle(unsigned swiz)
{
   return (swiz == ELK_SWIZZLE_XXXX ||
           swiz == ELK_SWIZZLE_YYYY ||
           swiz == ELK_SWIZZLE_ZZZZ ||
           swiz == ELK_SWIZZLE_WWWW);
}

/**
 * Compute the swizzle obtained from the application of \p swz0 on the result
 * of \p swz1.  The argument ordering is expected to match function
 * composition.
 */
static inline unsigned
elk_compose_swizzle(unsigned swz0, unsigned swz1)
{
   return ELK_SWIZZLE4(
      ELK_GET_SWZ(swz1, ELK_GET_SWZ(swz0, 0)),
      ELK_GET_SWZ(swz1, ELK_GET_SWZ(swz0, 1)),
      ELK_GET_SWZ(swz1, ELK_GET_SWZ(swz0, 2)),
      ELK_GET_SWZ(swz1, ELK_GET_SWZ(swz0, 3)));
}

/**
 * Return the result of applying swizzle \p swz to shuffle the bits of \p mask
 * (AKA image).
 */
static inline unsigned
elk_apply_swizzle_to_mask(unsigned swz, unsigned mask)
{
   unsigned result = 0;

   for (unsigned i = 0; i < 4; i++) {
      if (mask & (1 << ELK_GET_SWZ(swz, i)))
         result |= 1 << i;
   }

   return result;
}

/**
 * Return the result of applying the inverse of swizzle \p swz to shuffle the
 * bits of \p mask (AKA preimage).  Useful to find out which components are
 * read from a swizzled source given the instruction writemask.
 */
static inline unsigned
elk_apply_inv_swizzle_to_mask(unsigned swz, unsigned mask)
{
   unsigned result = 0;

   for (unsigned i = 0; i < 4; i++) {
      if (mask & (1 << i))
         result |= 1 << ELK_GET_SWZ(swz, i);
   }

   return result;
}

/**
 * Construct an identity swizzle for the set of enabled channels given by \p
 * mask.  The result will only reference channels enabled in the provided \p
 * mask, assuming that \p mask is non-zero.  The constructed swizzle will
 * satisfy the property that for any instruction OP and any mask:
 *
 *    elk_OP(p, elk_writemask(dst, mask),
 *           elk_swizzle(src, elk_swizzle_for_mask(mask)));
 *
 * will be equivalent to the same instruction without swizzle:
 *
 *    elk_OP(p, elk_writemask(dst, mask), src);
 */
static inline unsigned
elk_swizzle_for_mask(unsigned mask)
{
   unsigned last = (mask ? ffs(mask) - 1 : 0);
   unsigned swz[4];

   for (unsigned i = 0; i < 4; i++)
      last = swz[i] = (mask & (1 << i) ? i : last);

   return ELK_SWIZZLE4(swz[0], swz[1], swz[2], swz[3]);
}

/**
 * Construct an identity swizzle for the first \p n components of a vector.
 * When only a subset of channels of a vec4 are used we don't want to
 * reference the other channels, as that will tell optimization passes that
 * those other channels are used.
 */
static inline unsigned
elk_swizzle_for_size(unsigned n)
{
   return elk_swizzle_for_mask((1 << n) - 1);
}

/**
 * Converse of elk_swizzle_for_mask().  Returns the mask of components
 * accessed by the specified swizzle \p swz.
 */
static inline unsigned
elk_mask_for_swizzle(unsigned swz)
{
   return elk_apply_inv_swizzle_to_mask(swz, ~0);
}

uint32_t elk_swizzle_immediate(enum elk_reg_type type, uint32_t x, unsigned swz);

#define REG_SIZE (8*4)

/* These aren't hardware structs, just something useful for us to pass around:
 *
 * Align1 operation has a lot of control over input ranges.  Used in
 * WM programs to implement shaders decomposed into "channel serial"
 * or "structure of array" form:
 */
struct elk_reg {
   union {
      struct {
         enum elk_reg_type type:4;
         enum elk_reg_file file:3;      /* :2 hardware format */
         unsigned negate:1;             /* source only */
         unsigned abs:1;                /* source only */
         unsigned address_mode:1;       /* relative addressing, hopefully! */
         unsigned pad0:17;
         unsigned subnr:5;              /* :1 in align16 */
      };
      uint32_t bits;
   };

   union {
      struct {
         unsigned nr;
         unsigned swizzle:8;      /* src only, align16 only */
         unsigned writemask:4;    /* dest only, align16 only */
         int  indirect_offset:10; /* relative addressing offset */
         unsigned vstride:4;      /* source only */
         unsigned width:3;        /* src only, align1 only */
         unsigned hstride:2;      /* align1 only */
         unsigned pad1:1;
      };

      double df;
      uint64_t u64;
      int64_t d64;
      float f;
      int   d;
      unsigned ud;
   };
};

static inline unsigned
phys_nr(const struct intel_device_info *devinfo, const struct elk_reg reg)
{
   return reg.nr;
}

static inline unsigned
phys_subnr(const struct intel_device_info *devinfo, const struct elk_reg reg)
{
   return reg.subnr;
}

static inline bool
elk_regs_equal(const struct elk_reg *a, const struct elk_reg *b)
{
   return a->bits == b->bits && a->u64 == b->u64;
}

static inline bool
elk_regs_negative_equal(const struct elk_reg *a, const struct elk_reg *b)
{
   if (a->file == IMM) {
      if (a->bits != b->bits)
         return false;

      switch ((enum elk_reg_type) a->type) {
      case ELK_REGISTER_TYPE_UQ:
      case ELK_REGISTER_TYPE_Q:
         return a->d64 == -b->d64;
      case ELK_REGISTER_TYPE_DF:
         return a->df == -b->df;
      case ELK_REGISTER_TYPE_UD:
      case ELK_REGISTER_TYPE_D:
         return a->d == -b->d;
      case ELK_REGISTER_TYPE_F:
         return a->f == -b->f;
      case ELK_REGISTER_TYPE_VF:
         /* It is tempting to treat 0 as a negation of 0 (and -0 as a negation
          * of -0).  There are occasions where 0 or -0 is used and the exact
          * bit pattern is desired.  At the very least, changing this to allow
          * 0 as a negation of 0 causes some fp64 tests to fail on IVB.
          */
         return a->ud == (b->ud ^ 0x80808080);
      case ELK_REGISTER_TYPE_UW:
      case ELK_REGISTER_TYPE_W:
      case ELK_REGISTER_TYPE_UV:
      case ELK_REGISTER_TYPE_V:
      case ELK_REGISTER_TYPE_HF:
         /* FINISHME: Implement support for these types once there is
          * something in the compiler that can generate them.  Until then,
          * they cannot be tested.
          */
         return false;
      case ELK_REGISTER_TYPE_UB:
      case ELK_REGISTER_TYPE_B:
      case ELK_REGISTER_TYPE_NF:
      default:
         unreachable("not reached");
      }
   } else {
      struct elk_reg tmp = *a;

      tmp.negate = !tmp.negate;

      return elk_regs_equal(&tmp, b);
   }
}

struct elk_indirect {
   unsigned addr_subnr:4;
   int addr_offset:10;
   unsigned pad:18;
};


static inline unsigned
type_sz(unsigned type)
{
   switch(type) {
   case ELK_REGISTER_TYPE_UQ:
   case ELK_REGISTER_TYPE_Q:
   case ELK_REGISTER_TYPE_DF:
   case ELK_REGISTER_TYPE_NF:
      return 8;
   case ELK_REGISTER_TYPE_UD:
   case ELK_REGISTER_TYPE_D:
   case ELK_REGISTER_TYPE_F:
   case ELK_REGISTER_TYPE_VF:
      return 4;
   case ELK_REGISTER_TYPE_UW:
   case ELK_REGISTER_TYPE_W:
   case ELK_REGISTER_TYPE_HF:
   /* [U]V components are 4-bit, but HW unpacks them to 16-bit (2 bytes) */
   case ELK_REGISTER_TYPE_UV:
   case ELK_REGISTER_TYPE_V:
      return 2;
   case ELK_REGISTER_TYPE_UB:
   case ELK_REGISTER_TYPE_B:
      return 1;
   default:
      unreachable("not reached");
   }
}

static inline enum elk_reg_type
get_exec_type(const enum elk_reg_type type)
{
   switch (type) {
   case ELK_REGISTER_TYPE_B:
   case ELK_REGISTER_TYPE_V:
      return ELK_REGISTER_TYPE_W;
   case ELK_REGISTER_TYPE_UB:
   case ELK_REGISTER_TYPE_UV:
      return ELK_REGISTER_TYPE_UW;
   case ELK_REGISTER_TYPE_VF:
      return ELK_REGISTER_TYPE_F;
   default:
      return type;
   }
}

/**
 * Return an integer type of the requested size and signedness.
 */
static inline enum elk_reg_type
elk_int_type(unsigned sz, bool is_signed)
{
   switch (sz) {
   case 1:
      return (is_signed ? ELK_REGISTER_TYPE_B : ELK_REGISTER_TYPE_UB);
   case 2:
      return (is_signed ? ELK_REGISTER_TYPE_W : ELK_REGISTER_TYPE_UW);
   case 4:
      return (is_signed ? ELK_REGISTER_TYPE_D : ELK_REGISTER_TYPE_UD);
   case 8:
      return (is_signed ? ELK_REGISTER_TYPE_Q : ELK_REGISTER_TYPE_UQ);
   default:
      unreachable("Not reached.");
   }
}

/**
 * Construct a elk_reg.
 * \param file      one of the ELK_x_REGISTER_FILE values
 * \param nr        register number/index
 * \param subnr     register sub number
 * \param negate    register negate modifier
 * \param abs       register abs modifier
 * \param type      one of ELK_REGISTER_TYPE_x
 * \param vstride   one of ELK_VERTICAL_STRIDE_x
 * \param width     one of ELK_WIDTH_x
 * \param hstride   one of ELK_HORIZONTAL_STRIDE_x
 * \param swizzle   one of ELK_SWIZZLE_x
 * \param writemask WRITEMASK_X/Y/Z/W bitfield
 */
static inline struct elk_reg
elk_reg(enum elk_reg_file file,
        unsigned nr,
        unsigned subnr,
        unsigned negate,
        unsigned abs,
        enum elk_reg_type type,
        unsigned vstride,
        unsigned width,
        unsigned hstride,
        unsigned swizzle,
        unsigned writemask)
{
   struct elk_reg reg;
   if (file == ELK_GENERAL_REGISTER_FILE)
      assert(nr < XE2_MAX_GRF);
   else if (file == ELK_ARCHITECTURE_REGISTER_FILE)
      assert(nr <= ELK_ARF_TIMESTAMP);
   /* Asserting on the MRF register number requires to know the hardware gen
    * (gfx6 has 24 MRF registers), which we don't know here, so we assert
    * for that in the generators and in elk_eu_emit.c
    */

   reg.type = type;
   reg.file = file;
   reg.negate = negate;
   reg.abs = abs;
   reg.address_mode = ELK_ADDRESS_DIRECT;
   reg.pad0 = 0;
   reg.subnr = subnr * type_sz(type);
   reg.nr = nr;

   /* Could do better: If the reg is r5.3<0;1,0>, we probably want to
    * set swizzle and writemask to W, as the lower bits of subnr will
    * be lost when converted to align16.  This is probably too much to
    * keep track of as you'd want it adjusted by suboffset(), etc.
    * Perhaps fix up when converting to align16?
    */
   reg.swizzle = swizzle;
   reg.writemask = writemask;
   reg.indirect_offset = 0;
   reg.vstride = vstride;
   reg.width = width;
   reg.hstride = hstride;
   reg.pad1 = 0;
   return reg;
}

/** Construct float[16] register */
static inline struct elk_reg
elk_vec16_reg(enum elk_reg_file file, unsigned nr, unsigned subnr)
{
   return elk_reg(file,
                  nr,
                  subnr,
                  0,
                  0,
                  ELK_REGISTER_TYPE_F,
                  ELK_VERTICAL_STRIDE_16,
                  ELK_WIDTH_16,
                  ELK_HORIZONTAL_STRIDE_1,
                  ELK_SWIZZLE_XYZW,
                  WRITEMASK_XYZW);
}

/** Construct float[8] register */
static inline struct elk_reg
elk_vec8_reg(enum elk_reg_file file, unsigned nr, unsigned subnr)
{
   return elk_reg(file,
                  nr,
                  subnr,
                  0,
                  0,
                  ELK_REGISTER_TYPE_F,
                  ELK_VERTICAL_STRIDE_8,
                  ELK_WIDTH_8,
                  ELK_HORIZONTAL_STRIDE_1,
                  ELK_SWIZZLE_XYZW,
                  WRITEMASK_XYZW);
}

/** Construct float[4] register */
static inline struct elk_reg
elk_vec4_reg(enum elk_reg_file file, unsigned nr, unsigned subnr)
{
   return elk_reg(file,
                  nr,
                  subnr,
                  0,
                  0,
                  ELK_REGISTER_TYPE_F,
                  ELK_VERTICAL_STRIDE_4,
                  ELK_WIDTH_4,
                  ELK_HORIZONTAL_STRIDE_1,
                  ELK_SWIZZLE_XYZW,
                  WRITEMASK_XYZW);
}

/** Construct float[2] register */
static inline struct elk_reg
elk_vec2_reg(enum elk_reg_file file, unsigned nr, unsigned subnr)
{
   return elk_reg(file,
                  nr,
                  subnr,
                  0,
                  0,
                  ELK_REGISTER_TYPE_F,
                  ELK_VERTICAL_STRIDE_2,
                  ELK_WIDTH_2,
                  ELK_HORIZONTAL_STRIDE_1,
                  ELK_SWIZZLE_XYXY,
                  WRITEMASK_XY);
}

/** Construct float[1] register */
static inline struct elk_reg
elk_vec1_reg(enum elk_reg_file file, unsigned nr, unsigned subnr)
{
   return elk_reg(file,
                  nr,
                  subnr,
                  0,
                  0,
                  ELK_REGISTER_TYPE_F,
                  ELK_VERTICAL_STRIDE_0,
                  ELK_WIDTH_1,
                  ELK_HORIZONTAL_STRIDE_0,
                  ELK_SWIZZLE_XXXX,
                  WRITEMASK_X);
}

static inline struct elk_reg
elk_vecn_reg(unsigned width, enum elk_reg_file file,
             unsigned nr, unsigned subnr)
{
   switch (width) {
   case 1:
      return elk_vec1_reg(file, nr, subnr);
   case 2:
      return elk_vec2_reg(file, nr, subnr);
   case 4:
      return elk_vec4_reg(file, nr, subnr);
   case 8:
      return elk_vec8_reg(file, nr, subnr);
   case 16:
      return elk_vec16_reg(file, nr, subnr);
   default:
      unreachable("Invalid register width");
   }
}

static inline struct elk_reg
retype(struct elk_reg reg, enum elk_reg_type type)
{
   reg.type = type;
   return reg;
}

static inline struct elk_reg
firsthalf(struct elk_reg reg)
{
   return reg;
}

static inline struct elk_reg
sechalf(struct elk_reg reg)
{
   if (reg.vstride)
      reg.nr++;
   return reg;
}

static inline struct elk_reg
offset(struct elk_reg reg, unsigned delta)
{
   reg.nr += delta;
   return reg;
}


static inline struct elk_reg
byte_offset(struct elk_reg reg, unsigned bytes)
{
   unsigned newoffset = reg.nr * REG_SIZE + reg.subnr + bytes;
   reg.nr = newoffset / REG_SIZE;
   reg.subnr = newoffset % REG_SIZE;
   return reg;
}

static inline struct elk_reg
suboffset(struct elk_reg reg, unsigned delta)
{
   return byte_offset(reg, delta * type_sz(reg.type));
}

/** Construct unsigned word[16] register */
static inline struct elk_reg
elk_uw16_reg(enum elk_reg_file file, unsigned nr, unsigned subnr)
{
   return suboffset(retype(elk_vec16_reg(file, nr, 0), ELK_REGISTER_TYPE_UW), subnr);
}

/** Construct unsigned word[8] register */
static inline struct elk_reg
elk_uw8_reg(enum elk_reg_file file, unsigned nr, unsigned subnr)
{
   return suboffset(retype(elk_vec8_reg(file, nr, 0), ELK_REGISTER_TYPE_UW), subnr);
}

/** Construct unsigned word[1] register */
static inline struct elk_reg
elk_uw1_reg(enum elk_reg_file file, unsigned nr, unsigned subnr)
{
   return suboffset(retype(elk_vec1_reg(file, nr, 0), ELK_REGISTER_TYPE_UW), subnr);
}

static inline struct elk_reg
elk_ud8_reg(enum elk_reg_file file, unsigned nr, unsigned subnr)
{
   return retype(elk_vec8_reg(file, nr, subnr), ELK_REGISTER_TYPE_UD);
}

static inline struct elk_reg
elk_ud1_reg(enum elk_reg_file file, unsigned nr, unsigned subnr)
{
   return retype(elk_vec1_reg(file, nr, subnr), ELK_REGISTER_TYPE_UD);
}

static inline struct elk_reg
elk_imm_reg(enum elk_reg_type type)
{
   return elk_reg(ELK_IMMEDIATE_VALUE,
                  0,
                  0,
                  0,
                  0,
                  type,
                  ELK_VERTICAL_STRIDE_0,
                  ELK_WIDTH_1,
                  ELK_HORIZONTAL_STRIDE_0,
                  0,
                  0);
}

/** Construct float immediate register */
static inline struct elk_reg
elk_imm_df(double df)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_DF);
   imm.df = df;
   return imm;
}

static inline struct elk_reg
elk_imm_u64(uint64_t u64)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_UQ);
   imm.u64 = u64;
   return imm;
}

static inline struct elk_reg
elk_imm_f(float f)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_F);
   imm.f = f;
   return imm;
}

/** Construct int64_t immediate register */
static inline struct elk_reg
elk_imm_q(int64_t q)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_Q);
   imm.d64 = q;
   return imm;
}

/** Construct int64_t immediate register */
static inline struct elk_reg
elk_imm_uq(uint64_t uq)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_UQ);
   imm.u64 = uq;
   return imm;
}

/** Construct integer immediate register */
static inline struct elk_reg
elk_imm_d(int d)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_D);
   imm.d = d;
   return imm;
}

/** Construct uint immediate register */
static inline struct elk_reg
elk_imm_ud(unsigned ud)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_UD);
   imm.ud = ud;
   return imm;
}

/** Construct ushort immediate register */
static inline struct elk_reg
elk_imm_uw(uint16_t uw)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_UW);
   imm.ud = uw | (uw << 16);
   return imm;
}

/** Construct short immediate register */
static inline struct elk_reg
elk_imm_w(int16_t w)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_W);
   imm.ud = (uint16_t)w | (uint32_t)(uint16_t)w << 16;
   return imm;
}

/* elk_imm_b and elk_imm_ub aren't supported by hardware - the type
 * numbers alias with _V and _VF below:
 */

/** Construct vector of eight signed half-byte values */
static inline struct elk_reg
elk_imm_v(unsigned v)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_V);
   imm.ud = v;
   return imm;
}

/** Construct vector of eight unsigned half-byte values */
static inline struct elk_reg
elk_imm_uv(unsigned uv)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_UV);
   imm.ud = uv;
   return imm;
}

/** Construct vector of four 8-bit float values */
static inline struct elk_reg
elk_imm_vf(unsigned v)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_VF);
   imm.ud = v;
   return imm;
}

static inline struct elk_reg
elk_imm_vf4(unsigned v0, unsigned v1, unsigned v2, unsigned v3)
{
   struct elk_reg imm = elk_imm_reg(ELK_REGISTER_TYPE_VF);
   imm.vstride = ELK_VERTICAL_STRIDE_0;
   imm.width = ELK_WIDTH_4;
   imm.hstride = ELK_HORIZONTAL_STRIDE_1;
   imm.ud = ((v0 << 0) | (v1 << 8) | (v2 << 16) | (v3 << 24));
   return imm;
}


static inline struct elk_reg
elk_address(struct elk_reg reg)
{
   return elk_imm_uw(reg.nr * REG_SIZE + reg.subnr);
}

/** Construct float[1] general-purpose register */
static inline struct elk_reg
elk_vec1_grf(unsigned nr, unsigned subnr)
{
   return elk_vec1_reg(ELK_GENERAL_REGISTER_FILE, nr, subnr);
}

static inline struct elk_reg
xe2_vec1_grf(unsigned nr, unsigned subnr)
{
   return elk_vec1_reg(ELK_GENERAL_REGISTER_FILE, 2 * nr + subnr / 8, subnr % 8);
}

/** Construct float[2] general-purpose register */
static inline struct elk_reg
elk_vec2_grf(unsigned nr, unsigned subnr)
{
   return elk_vec2_reg(ELK_GENERAL_REGISTER_FILE, nr, subnr);
}

static inline struct elk_reg
xe2_vec2_grf(unsigned nr, unsigned subnr)
{
   return elk_vec2_reg(ELK_GENERAL_REGISTER_FILE, 2 * nr + subnr / 8, subnr % 8);
}

/** Construct float[4] general-purpose register */
static inline struct elk_reg
elk_vec4_grf(unsigned nr, unsigned subnr)
{
   return elk_vec4_reg(ELK_GENERAL_REGISTER_FILE, nr, subnr);
}

static inline struct elk_reg
xe2_vec4_grf(unsigned nr, unsigned subnr)
{
   return elk_vec4_reg(ELK_GENERAL_REGISTER_FILE, 2 * nr + subnr / 8, subnr % 8);
}

/** Construct float[8] general-purpose register */
static inline struct elk_reg
elk_vec8_grf(unsigned nr, unsigned subnr)
{
   return elk_vec8_reg(ELK_GENERAL_REGISTER_FILE, nr, subnr);
}

static inline struct elk_reg
xe2_vec8_grf(unsigned nr, unsigned subnr)
{
   return elk_vec8_reg(ELK_GENERAL_REGISTER_FILE, 2 * nr + subnr / 8, subnr % 8);
}

/** Construct float[16] general-purpose register */
static inline struct elk_reg
elk_vec16_grf(unsigned nr, unsigned subnr)
{
   return elk_vec16_reg(ELK_GENERAL_REGISTER_FILE, nr, subnr);
}

static inline struct elk_reg
xe2_vec16_grf(unsigned nr, unsigned subnr)
{
   return elk_vec16_reg(ELK_GENERAL_REGISTER_FILE, 2 * nr + subnr / 8, subnr % 8);
}

static inline struct elk_reg
elk_vecn_grf(unsigned width, unsigned nr, unsigned subnr)
{
   return elk_vecn_reg(width, ELK_GENERAL_REGISTER_FILE, nr, subnr);
}

static inline struct elk_reg
xe2_vecn_grf(unsigned width, unsigned nr, unsigned subnr)
{
   return elk_vecn_reg(width, ELK_GENERAL_REGISTER_FILE, nr + subnr / 8, subnr % 8);
}

static inline struct elk_reg
elk_uw1_grf(unsigned nr, unsigned subnr)
{
   return elk_uw1_reg(ELK_GENERAL_REGISTER_FILE, nr, subnr);
}

static inline struct elk_reg
elk_uw8_grf(unsigned nr, unsigned subnr)
{
   return elk_uw8_reg(ELK_GENERAL_REGISTER_FILE, nr, subnr);
}

static inline struct elk_reg
elk_uw16_grf(unsigned nr, unsigned subnr)
{
   return elk_uw16_reg(ELK_GENERAL_REGISTER_FILE, nr, subnr);
}

static inline struct elk_reg
elk_ud8_grf(unsigned nr, unsigned subnr)
{
   return elk_ud8_reg(ELK_GENERAL_REGISTER_FILE, nr, subnr);
}

static inline struct elk_reg
elk_ud1_grf(unsigned nr, unsigned subnr)
{
   return elk_ud1_reg(ELK_GENERAL_REGISTER_FILE, nr, subnr);
}


/** Construct null register (usually used for setting condition codes) */
static inline struct elk_reg
elk_null_reg(void)
{
   return elk_vec8_reg(ELK_ARCHITECTURE_REGISTER_FILE, ELK_ARF_NULL, 0);
}

static inline struct elk_reg
elk_null_vec(unsigned width)
{
   return elk_vecn_reg(width, ELK_ARCHITECTURE_REGISTER_FILE, ELK_ARF_NULL, 0);
}

static inline struct elk_reg
elk_address_reg(unsigned subnr)
{
   return elk_uw1_reg(ELK_ARCHITECTURE_REGISTER_FILE, ELK_ARF_ADDRESS, subnr);
}

static inline struct elk_reg
elk_tdr_reg(void)
{
   return elk_uw1_reg(ELK_ARCHITECTURE_REGISTER_FILE, ELK_ARF_TDR, 0);
}

/* If/else instructions break in align16 mode if writemask & swizzle
 * aren't xyzw.  This goes against the convention for other scalar
 * regs:
 */
static inline struct elk_reg
elk_ip_reg(void)
{
   return elk_reg(ELK_ARCHITECTURE_REGISTER_FILE,
                  ELK_ARF_IP,
                  0,
                  0,
                  0,
                  ELK_REGISTER_TYPE_UD,
                  ELK_VERTICAL_STRIDE_4, /* ? */
                  ELK_WIDTH_1,
                  ELK_HORIZONTAL_STRIDE_0,
                  ELK_SWIZZLE_XYZW, /* NOTE! */
                  WRITEMASK_XYZW); /* NOTE! */
}

static inline struct elk_reg
elk_notification_reg(void)
{
   return elk_reg(ELK_ARCHITECTURE_REGISTER_FILE,
                  ELK_ARF_NOTIFICATION_COUNT,
                  0,
                  0,
                  0,
                  ELK_REGISTER_TYPE_UD,
                  ELK_VERTICAL_STRIDE_0,
                  ELK_WIDTH_1,
                  ELK_HORIZONTAL_STRIDE_0,
                  ELK_SWIZZLE_XXXX,
                  WRITEMASK_X);
}

static inline struct elk_reg
elk_cr0_reg(unsigned subnr)
{
   return elk_ud1_reg(ELK_ARCHITECTURE_REGISTER_FILE, ELK_ARF_CONTROL, subnr);
}

static inline struct elk_reg
elk_sr0_reg(unsigned subnr)
{
   return elk_ud1_reg(ELK_ARCHITECTURE_REGISTER_FILE, ELK_ARF_STATE, subnr);
}

static inline struct elk_reg
elk_acc_reg(unsigned width)
{
   return elk_vecn_reg(width, ELK_ARCHITECTURE_REGISTER_FILE,
                       ELK_ARF_ACCUMULATOR, 0);
}

static inline struct elk_reg
elk_flag_reg(int reg, int subreg)
{
   return elk_uw1_reg(ELK_ARCHITECTURE_REGISTER_FILE,
                      ELK_ARF_FLAG + reg, subreg);
}

static inline struct elk_reg
elk_flag_subreg(unsigned subreg)
{
   return elk_uw1_reg(ELK_ARCHITECTURE_REGISTER_FILE,
                      ELK_ARF_FLAG + subreg / 2, subreg % 2);
}

/**
 * Return the mask register present in Gfx4-5, or the related register present
 * in Gfx7.5 and later hardware referred to as "channel enable" register in
 * the documentation.
 */
static inline struct elk_reg
elk_mask_reg(unsigned subnr)
{
   return elk_uw1_reg(ELK_ARCHITECTURE_REGISTER_FILE, ELK_ARF_MASK, subnr);
}

static inline struct elk_reg
elk_vmask_reg()
{
   return elk_sr0_reg(3);
}

static inline struct elk_reg
elk_dmask_reg()
{
   return elk_sr0_reg(2);
}

static inline struct elk_reg
elk_mask_stack_reg(unsigned subnr)
{
   return suboffset(retype(elk_vec16_reg(ELK_ARCHITECTURE_REGISTER_FILE,
                                         ELK_ARF_MASK_STACK, 0),
                           ELK_REGISTER_TYPE_UB), subnr);
}

static inline struct elk_reg
elk_mask_stack_depth_reg(unsigned subnr)
{
   return elk_uw1_reg(ELK_ARCHITECTURE_REGISTER_FILE,
                      ELK_ARF_MASK_STACK_DEPTH, subnr);
}

static inline struct elk_reg
elk_message_reg(unsigned nr)
{
   return elk_vec8_reg(ELK_MESSAGE_REGISTER_FILE, nr, 0);
}

static inline struct elk_reg
elk_uvec_mrf(unsigned width, unsigned nr, unsigned subnr)
{
   return retype(elk_vecn_reg(width, ELK_MESSAGE_REGISTER_FILE, nr, subnr),
                 ELK_REGISTER_TYPE_UD);
}

/* This is almost always called with a numeric constant argument, so
 * make things easy to evaluate at compile time:
 */
static inline unsigned cvt(unsigned val)
{
   switch (val) {
   case 0: return 0;
   case 1: return 1;
   case 2: return 2;
   case 4: return 3;
   case 8: return 4;
   case 16: return 5;
   case 32: return 6;
   }
   return 0;
}

static inline struct elk_reg
stride(struct elk_reg reg, unsigned vstride, unsigned width, unsigned hstride)
{
   reg.vstride = cvt(vstride);
   reg.width = cvt(width) - 1;
   reg.hstride = cvt(hstride);
   return reg;
}

/**
 * Multiply the vertical and horizontal stride of a register by the given
 * factor \a s.
 */
static inline struct elk_reg
spread(struct elk_reg reg, unsigned s)
{
   if (s) {
      assert(util_is_power_of_two_nonzero(s));

      if (reg.hstride)
         reg.hstride += cvt(s) - 1;

      if (reg.vstride)
         reg.vstride += cvt(s) - 1;

      return reg;
   } else {
      return stride(reg, 0, 1, 0);
   }
}

/**
 * Reinterpret each channel of register \p reg as a vector of values of the
 * given smaller type and take the i-th subcomponent from each.
 */
static inline struct elk_reg
subscript(struct elk_reg reg, enum elk_reg_type type, unsigned i)
{
   unsigned scale = type_sz(reg.type) / type_sz(type);
   assert(scale >= 1 && i < scale);

   if (reg.file == IMM) {
      unsigned bit_size = type_sz(type) * 8;
      reg.u64 >>= i * bit_size;
      reg.u64 &= BITFIELD64_MASK(bit_size);
      if (bit_size <= 16)
         reg.u64 |= reg.u64 << 16;
      return retype(reg, type);
   }

   return suboffset(retype(spread(reg, scale), type), i);
}

static inline struct elk_reg
vec16(struct elk_reg reg)
{
   return stride(reg, 16,16,1);
}

static inline struct elk_reg
vec8(struct elk_reg reg)
{
   return stride(reg, 8,8,1);
}

static inline struct elk_reg
vec4(struct elk_reg reg)
{
   return stride(reg, 4,4,1);
}

static inline struct elk_reg
vec2(struct elk_reg reg)
{
   return stride(reg, 2,2,1);
}

static inline struct elk_reg
vec1(struct elk_reg reg)
{
   return stride(reg, 0,1,0);
}


static inline struct elk_reg
get_element(struct elk_reg reg, unsigned elt)
{
   return vec1(suboffset(reg, elt));
}

static inline struct elk_reg
get_element_ud(struct elk_reg reg, unsigned elt)
{
   return vec1(suboffset(retype(reg, ELK_REGISTER_TYPE_UD), elt));
}

static inline struct elk_reg
get_element_d(struct elk_reg reg, unsigned elt)
{
   return vec1(suboffset(retype(reg, ELK_REGISTER_TYPE_D), elt));
}

static inline struct elk_reg
elk_swizzle(struct elk_reg reg, unsigned swz)
{
   if (reg.file == ELK_IMMEDIATE_VALUE)
      reg.ud = elk_swizzle_immediate(reg.type, reg.ud, swz);
   else
      reg.swizzle = elk_compose_swizzle(swz, reg.swizzle);

   return reg;
}

static inline struct elk_reg
elk_writemask(struct elk_reg reg, unsigned mask)
{
   assert(reg.file != ELK_IMMEDIATE_VALUE);
   reg.writemask &= mask;
   return reg;
}

static inline struct elk_reg
elk_set_writemask(struct elk_reg reg, unsigned mask)
{
   assert(reg.file != ELK_IMMEDIATE_VALUE);
   reg.writemask = mask;
   return reg;
}

static inline unsigned
elk_writemask_for_size(unsigned n)
{
   return (1 << n) - 1;
}

static inline unsigned
elk_writemask_for_component_packing(unsigned n, unsigned first_component)
{
   assert(first_component + n <= 4);
   return (((1 << n) - 1) << first_component);
}

static inline struct elk_reg
negate(struct elk_reg reg)
{
   reg.negate ^= 1;
   return reg;
}

static inline struct elk_reg
elk_abs(struct elk_reg reg)
{
   reg.abs = 1;
   reg.negate = 0;
   return reg;
}

/************************************************************************/

static inline struct elk_reg
elk_vec4_indirect(unsigned subnr, int offset)
{
   struct elk_reg reg =  elk_vec4_grf(0, 0);
   reg.subnr = subnr;
   reg.address_mode = ELK_ADDRESS_REGISTER_INDIRECT_REGISTER;
   reg.indirect_offset = offset;
   return reg;
}

static inline struct elk_reg
elk_vec1_indirect(unsigned subnr, int offset)
{
   struct elk_reg reg =  elk_vec1_grf(0, 0);
   reg.subnr = subnr;
   reg.address_mode = ELK_ADDRESS_REGISTER_INDIRECT_REGISTER;
   reg.indirect_offset = offset;
   return reg;
}

static inline struct elk_reg
elk_VxH_indirect(unsigned subnr, int offset)
{
   struct elk_reg reg = elk_vec1_grf(0, 0);
   reg.vstride = ELK_VERTICAL_STRIDE_ONE_DIMENSIONAL;
   reg.subnr = subnr;
   reg.address_mode = ELK_ADDRESS_REGISTER_INDIRECT_REGISTER;
   reg.indirect_offset = offset;
   return reg;
}

static inline struct elk_reg
deref_4f(struct elk_indirect ptr, int offset)
{
   return elk_vec4_indirect(ptr.addr_subnr, ptr.addr_offset + offset);
}

static inline struct elk_reg
deref_1f(struct elk_indirect ptr, int offset)
{
   return elk_vec1_indirect(ptr.addr_subnr, ptr.addr_offset + offset);
}

static inline struct elk_reg
deref_4b(struct elk_indirect ptr, int offset)
{
   return retype(deref_4f(ptr, offset), ELK_REGISTER_TYPE_B);
}

static inline struct elk_reg
deref_1uw(struct elk_indirect ptr, int offset)
{
   return retype(deref_1f(ptr, offset), ELK_REGISTER_TYPE_UW);
}

static inline struct elk_reg
deref_1d(struct elk_indirect ptr, int offset)
{
   return retype(deref_1f(ptr, offset), ELK_REGISTER_TYPE_D);
}

static inline struct elk_reg
deref_1ud(struct elk_indirect ptr, int offset)
{
   return retype(deref_1f(ptr, offset), ELK_REGISTER_TYPE_UD);
}

static inline struct elk_reg
get_addr_reg(struct elk_indirect ptr)
{
   return elk_address_reg(ptr.addr_subnr);
}

static inline struct elk_indirect
elk_indirect_offset(struct elk_indirect ptr, int offset)
{
   ptr.addr_offset += offset;
   return ptr;
}

static inline struct elk_indirect
elk_indirect(unsigned addr_subnr, int offset)
{
   struct elk_indirect ptr;
   ptr.addr_subnr = addr_subnr;
   ptr.addr_offset = offset;
   ptr.pad = 0;
   return ptr;
}

static inline bool
region_matches(struct elk_reg reg, enum elk_vertical_stride v,
               enum elk_width w, enum elk_horizontal_stride h)
{
   return reg.vstride == v &&
          reg.width == w &&
          reg.hstride == h;
}

#define has_scalar_region(reg) \
   region_matches(reg, ELK_VERTICAL_STRIDE_0, ELK_WIDTH_1, \
                  ELK_HORIZONTAL_STRIDE_0)

/**
 * Return the size in bytes per data element of register \p reg on the
 * corresponding register file.
 */
static inline unsigned
element_sz(struct elk_reg reg)
{
   if (reg.file == ELK_IMMEDIATE_VALUE || has_scalar_region(reg)) {
      return type_sz(reg.type);

   } else if (reg.width == ELK_WIDTH_1 &&
              reg.hstride == ELK_HORIZONTAL_STRIDE_0) {
      assert(reg.vstride != ELK_VERTICAL_STRIDE_0);
      return type_sz(reg.type) << (reg.vstride - 1);

   } else {
      assert(reg.hstride != ELK_HORIZONTAL_STRIDE_0);
      assert(reg.vstride == reg.hstride + reg.width);
      return type_sz(reg.type) << (reg.hstride - 1);
   }
}

/* elk_packed_float.c */
int elk_float_to_vf(float f);
float elk_vf_to_float(unsigned char vf);

#ifdef __cplusplus
}
#endif

#endif
