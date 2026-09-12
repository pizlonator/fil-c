/* Multiprecision generic functions.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef __GMP_ARCH_H
#define __GMP_ARCH_H

#include <stdint.h>
#include <gmp.h>
#include <math_uint128.h>

#define LL_B ((mp_limb_t) 1 << (BITS_PER_MP_LIMB / 2))

static __always_inline mp_limb_t
ll_lowpart (mp_limb_t t)
{
  return t & (LL_B - 1);
}

static __always_inline mp_limb_t
ll_highpart (mp_limb_t t)
{
  return t >> (BITS_PER_MP_LIMB / 2);
}

/* umul_ppmm(high_prod, low_prod, multiplier, multiplicand) multiplies two
   UWtype integers MULTIPLIER and MULTIPLICAND, and generates a two UWtype
   word product in HIGH_PROD and LOW_PROD.  */
static __always_inline void
umul_ppmm_generic (mp_limb_t *w1, mp_limb_t *w0, mp_limb_t u, mp_limb_t v)
{
#if BITS_PER_MP_LIMB == 32
  uint64_t t0 = (uint64_t)u * v;
  *w1 = t0 >> 32;
  *w0 = t0;
#else
  u128 r = u128_mul(u128_from_u64 (u), u128_from_u64 (v));
  *w1 = u128_high (r);
  *w0 = u128_low (r);
#endif
}
#define umul_ppmm(__w1, __w0, __u, __v)			\
  ({							\
    __typeof (__w0) __w0t;				\
    __typeof (__w1) __w1t;				\
    umul_ppmm_generic (&__w1t, &__w0t, __u, __v);	\
    __w1 = __w1t;					\
    __w0 = __w0t;					\
  })

/* udiv_qrnnd(quotient, remainder, high_numerator, low_numerator,
   denominator) divides a UDWtype, composed by the UWtype integers
   HIGH_NUMERATOR and LOW_NUMERATOR, by DENOMINATOR and places the quotient
   in QUOTIENT and the remainder in REMAINDER.  HIGH_NUMERATOR must be less
   than DENOMINATOR for correct operation.  If, in addition, the most
   significant bit of DENOMINATOR must be 1, then the pre-processor symbol
   UDIV_NEEDS_NORMALIZATION is defined to 1.  */
#ifndef udiv_qrnnd
static __always_inline void
udiv_qrnnd_generic (mp_limb_t *q, mp_limb_t *r, mp_limb_t n1, mp_limb_t n0,
		    mp_limb_t d)
{
  mp_limb_t d1 = ll_highpart (d),
            d0 = ll_lowpart (d),
            q1, q0;
  mp_limb_t r1, r0, m;

  r1 = n1 % d1;
  q1 = n1 / d1;
  m = q1 * d0;
  r1 = r1 * LL_B | ll_highpart (n0);
  if (r1 < m)
    {
      q1--;
      r1 += d;
      if (r1 >= d)
        if (r1 < m)
          {
            q1--;
            r1 += d;
          }
    }
  r1 -= m;

  r0 = r1 % d1;
  q0 = r1 / d1;
  m = q0 * d0;
  r0 = r0 * LL_B | ll_lowpart (n0);
  if (r0 < m)
    {
      q0--;
      r0 += d;
      if (r0 >= d)
        if (r0 < m)
          {
            q0--;
            r0 += d;
          }
    }
  r0 -= m;

  *q = q1 * LL_B | q0;
  *r = r0;
}
# define UDIV_NEEDS_NORMALIZATION 1
# define udiv_qrnnd(__q, __r, __n1, __n0, __d) \
  udiv_qrnnd_generic (&__q, &__r, __n1, __n0, __d)
#endif


/* add_ssaaaa(high_sum, low_sum, high_addend_1, low_addend_1,
   high_addend_2, low_addend_2) adds two UWtype integers, composed by
   HIGH_ADDEND_1 and LOW_ADDEND_1, and HIGH_ADDEND_2 and LOW_ADDEND_2
   respectively.  The result is placed in HIGH_SUM and LOW_SUM.  Overflow
   (i.e. carry out) is not stored anywhere, and is lost.  */
static __always_inline void
add_ssaaaa_generic (mp_limb_t *sh, mp_limb_t *sl, mp_limb_t ah,
		    mp_limb_t al,  mp_limb_t bh,  mp_limb_t bl)
{
#if BITS_PER_MP_LIMB == 32
  uint64_t a = (uint64_t)ah << 32 | al;
  uint64_t b = (uint64_t)bh << 32 | bl;
  uint64_t r = a + b;
  *sh = r >> 32;
  *sl = r & 0xFFFFFFFF;
#else
  u128 r = u128_add (u128_from_hl (ah, al),
                     u128_from_hl (bh, bl));
  *sh = u128_high (r);
  *sl = u128_low (r);
#endif
}
#define add_ssaaaa(sh, sl, ah, al, bh, bl) \
  add_ssaaaa_generic (&sh, &sl, ah, al, bh, bl)

/* sub_ddmmss(high_difference, low_difference, high_minuend, low_minuend,
   high_subtrahend, low_subtrahend) subtracts two two-word UWtype integers,
   composed by HIGH_MINUEND_1 and LOW_MINUEND_1, and HIGH_SUBTRAHEND_2 and
   LOW_SUBTRAHEND_2 respectively.  The result is placed in HIGH_DIFFERENCE
   and LOW_DIFFERENCE.  Overflow (i.e. carry out) is not stored anywhere,
   and is lost.  */
static __always_inline void
sub_ddmmss_generic (mp_limb_t *sh, mp_limb_t *sl, mp_limb_t ah,
		    mp_limb_t al,  mp_limb_t bh,  mp_limb_t bl)
{
#if BITS_PER_MP_LIMB == 32
  uint64_t a = (uint64_t)ah << 32 | al;
  uint64_t b = (uint64_t)bh << 32 | bl;
  uint64_t r = a - b;
  *sh = r >> 32;
  *sl = r & 0xFFFFFFFF;
#else
  u128 r = u128_sub (u128_from_hl (ah, al),
                     u128_from_hl (bh, bl));
  *sh = u128_high (r);
  *sl = u128_low (r);
#endif
}
#define sub_ddmmss(sh, sl, ah, al, bh, bl) \
  sub_ddmmss_generic (&sh, &sl, ah, al, bh, bl)

#endif /* __GMP_ARCH_H */
