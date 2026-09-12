/* Double-precision vector (SVE) cosh function

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

static const struct data
{
  double c0, c2;
  double c1, c3;
  double special_bound;
  float64_t inv_ln2, ln2_hi, ln2_lo, shift;
  float64_t exp_9;
} data = {
  /* Generated using Remez, in [-log(2)/128, log(2)/128].  */
  .c0 = 0x1.fffffffffdbcdp-2,
  .c1 = 0x1.555555555444cp-3,
  .c2 = 0x1.555573c6a9f7dp-5,
  .c3 = 0x1.1111266d28935p-7,
  .ln2_hi = 0x1.62e42fefa3800p-1,
  .ln2_lo = 0x1.ef35793c76730p-45,
  /* 1/ln2.  */
  .inv_ln2 = 0x1.71547652b82fep+0,
  .shift = 0x1.800000000ff80p+46, /* 1.5*2^46+1022.  */
  /* (ln(2^(1021 + 1/128))), above which exp overflows.  */
  .special_bound = 0x1.61dab63dc7dc1p+9, /* ~ 707.71.  */
  .exp_9 = 0x1.fa7157c470f82p+12,	 /* exp(9) ~ 8103.08.  */
};

/* Helper for approximating exp(x)/2.
   Functionally identical to FEXPA exp(x), but an adjustment in
   the shift value which leads to a reduction in the exponent of scale by 1,
   thus halving the result at no cost.  */
static inline svfloat64_t
exp_over_two_inline (const svbool_t pg, svfloat64_t x, const struct data *d)
{
  /* Calculate exp(x).  */
  svfloat64_t z = svmla_x (pg, sv_f64 (d->shift), x, d->inv_ln2);
  svuint64_t u = svreinterpret_u64 (z);
  svfloat64_t n = svsub_x (pg, z, d->shift);

  svfloat64_t c13 = svld1rq (svptrue_b64 (), &d->c1);
  svfloat64_t ln2 = svld1rq (svptrue_b64 (), &d->ln2_hi);

  svfloat64_t r = x;
  r = svmls_lane (r, n, ln2, 0);
  r = svmls_lane (r, n, ln2, 1);

  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t p01 = svmla_lane (sv_f64 (d->c0), r, c13, 0);
  svfloat64_t p23 = svmla_lane (sv_f64 (d->c2), r, c13, 1);
  svfloat64_t p04 = svmla_x (pg, p01, p23, r2);
  svfloat64_t p = svmla_x (pg, r, p04, r2);

  svfloat64_t scale = svexpa (u);

  return svmla_x (pg, scale, scale, p);
}

/* Uses the compound angle formula to adjust x back into an approximable range:
   cosh (A + B) = cosh(A)cosh(B) + sinh(A)sinh(B)
   By choosing sufficiently large values whereby after rounding cosh == sinh,
   this can be simplified into: cosh (A + B) = cosh(A) * e^B.  */
static svfloat64_t NOINLINE
special_case (svfloat64_t x, svbool_t pg, svbool_t special, svfloat64_t t,
	      const struct data *d)
{
  /* Finish fast path to compute values for non-special cases.  */
  svfloat64_t inv_twoexp = svdivr_x (pg, t, 0.25);
  svfloat64_t y = svadd_x (pg, t, inv_twoexp);

  /* Absolute x so we can subtract 9.0 without worrying about signing.  */
  svfloat64_t ax = svabs_x (svptrue_b64 (), x);
  /* The input `x` is reduced by an offset of 9.0 to allow for accurate
     approximation on the interval x > SpecialBound ~ 710.47.  */
  ax = svsub_x (svptrue_b64 (), ax, 9.0);

  svfloat64_t half_exp = exp_over_two_inline (svptrue_b64 (), ax, d);

  /* Multiply the result by exp(9) for special lanes only.  */
  svfloat64_t cosh_sum = svmul_x (svptrue_b64 (), half_exp, d->exp_9);

  /* Check for overflowing special lanes and return inf for these lanes.  */
  svbool_t is_inf = svcmpgt (special, ax, d->special_bound);
  /* Return inf for overflowing lanes.  */
  svfloat64_t special_y = svsel (is_inf, sv_f64 (INFINITY), cosh_sum);

  return svsel (special, special_y, y);
}

/* Approximation for SVE double-precision cosh(x) using exp_inline.
   cosh(x) = (exp(x) + exp(-x)) / 2.
   The greatest observed error is 2.10 + 0.5 ULP:
   _ZGVsMxv_cosh (-0x1.2acb2978bd15ep+4) got 0x1.ebbd8806ea342p+25
					want 0x1.ebbd8806ea33fp+25.  */
svfloat64_t SV_NAME_D1 (cosh) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* Up to the point that exp overflows, we can use it to calculate cosh by
     (exp(|x|)/2 + 1) / (2 * exp(|x|)).  */
  svfloat64_t half_exp = exp_over_two_inline (pg, x, d);

  /* Fall back to vectorised special case for any lanes which would cause
     exp to overflow.  */
  svbool_t special = svacge (pg, x, d->special_bound);
  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, pg, special, half_exp, d);

  svfloat64_t inv_twoexp = svdivr_x (pg, half_exp, 0.25);
  return svadd_x (pg, half_exp, inv_twoexp);
}
