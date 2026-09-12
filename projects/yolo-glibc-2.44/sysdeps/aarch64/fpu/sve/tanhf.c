/* Single-precision vector (SVE) tanh function

   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include "sv_math.h"

/* Largest value of x for which tanhf(x) rounds to 1 (or -1 for negative).  */
#define SpecialBound 0x1.205966p+3f /* ~9.01.  */

static const struct data
{
  /* These 4 are grouped together so they can be loaded as one quadword, then
   used with _lane forms of svmla/svmls.  */
  float32_t c2, c4, ln2_hi, ln2_lo;
  float c0, two_over_ln2, c1, c3, special_bound;
} data = {
  .special_bound = SpecialBound,
  /* Coefficients generated using fpminimax.  */
  .c0 = 0x1.fffffep-2,
  .c1 = 0x1.5554aep-3,
  /* 2/ln2.  */
  .two_over_ln2 = 0x1.715476p+1f,
  .c2 = 0x1.555736p-5,
  .c3 = 0x1.12287cp-7,
  .c4 = 0x1.6b55a2p-10,
  .ln2_lo = 0x1.7f7d1cp-20f,
  .ln2_hi = 0x1.62e4p-1f,
};

/* An expm1 inspired helper function that returns an accurate
   estimate for e^2x - 1.  */
static inline svfloat32_t
e2xm1f_inline (svfloat32_t x, svbool_t pg, const struct data *d)
{
  /* This vector is reliant on layout of data - it contains constants
   that can be used with _lane forms of svmla/svmls. Values are:
   [ coeff_2, coeff_4, ln2_hi, ln2_lo ].  */
  svfloat32_t lane_constants = svld1rq (svptrue_b32 (), &d->c2);

  /* Reduce argument to smaller range:
     Let i = round(x / (2 * ln2))
     and f = (x + x) - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  svfloat32_t j = svmul_x (svptrue_b32 (), x, d->two_over_ln2);
  j = svrinta_x (pg, j);
  svfloat32_t f = svadd_x (pg, x, x);
  f = svmls_lane (f, j, lane_constants, 2);
  f = svmls_lane (f, j, lane_constants, 3);

  /* Approximate expm1(f) using polynomial.
     Taylor expansion for expm1(x) has the form:
	 x + ax^2 + bx^3 + cx^4 ....
     So we calculate the polynomial P(f) = a + bf + cf^2 + ...
     and assemble the approximation expm1(f) ~= f + f^2 * P(f).  */
  svfloat32_t p12 = svmla_lane (sv_f32 (d->c1), f, lane_constants, 0);
  svfloat32_t p34 = svmla_lane (sv_f32 (d->c3), f, lane_constants, 1);
  svfloat32_t f2 = svmul_x (svptrue_b32 (), f, f);
  svfloat32_t p = svmla_x (pg, p12, f2, p34);
  p = svmla_x (pg, sv_f32 (d->c0), f, p);
  p = svmla_x (pg, f, f2, p);

  /* Assemble the result.
     expm1(x) ~= 2^i * (p + 1) - 1
     Let t = 2^i.  */
  svfloat32_t t = svscale_x (pg, sv_f32 (1.0f), svcvt_s32_x (pg, j));
  return svmla_x (pg, svsub_x (pg, t, 1.0f), p, t);
}

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svbool_t pg, svbool_t special, svfloat32_t q)
{
  /* Finish fastpass to compute values for non-special cases.  */
  svfloat32_t y = svdiv_x (pg, q, svadd_x (pg, q, 2.0));

  /* Make special values positive for best accuracy.  */
  svfloat32_t ax = svabs_x (svptrue_b32 (), x);
  svuint32_t iax = svreinterpret_u32 (ax);

  /* Preserve the sign bit to return final calcualtion to correct sign.  */
  svuint32_t sign = sveor_x (svptrue_b32 (), svreinterpret_u32 (x), iax);

  /* Set overflowing lanes to signed 1.  */
  svfloat32_t special_y = svreinterpret_f32 (
      svorr_x (svptrue_b32 (), sign, sv_u32 (0x3f800000)));

  /* Return special_y for special lanes and y for none special lanes.  */
  return svsel_f32 (special, special_y, y);
}

/* Approximation for single-precision SVE tanh(x), using a simplified
   version of expm1f.
   Maximum error is 2.06 +0.5 ULP:
   _ZGVsMxv_tanhf (0x1.fc1832p-5) got 0x1.fb71a4p-5
				 want 0x1.fb71aap-5.  */
svfloat32_t SV_NAME_F1 (tanh) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* tanh(x) = (e^2x - 1) / (e^2x + 1).  */
  svfloat32_t q = e2xm1f_inline (x, pg, d);

  svbool_t special = svacgt (pg, x, d->special_bound);
  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, pg, special, q);

  return svdiv_x (pg, q, svadd_x (pg, q, 2.0));
}
