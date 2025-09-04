/* Single-precision vector (SVE) exp function.

   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
  float poly[5];
  float inv_ln2, ln2_hi, ln2_lo, shift, thres;
} data = {
  /* Coefficients copied from the polynomial in AdvSIMD variant, reversed for
     compatibility with polynomial helpers.  */
  .poly = { 0x1.ffffecp-1f, 0x1.fffdb6p-2f, 0x1.555e66p-3f, 0x1.573e2ep-5f,
	    0x1.0e4020p-7f },
  .inv_ln2 = 0x1.715476p+0f,
  .ln2_hi = 0x1.62e4p-1f,
  .ln2_lo = 0x1.7f7d1cp-20f,
  /* 1.5*2^17 + 127.  */
  .shift = 0x1.903f8p17f,
  /* Roughly 87.3. For x < -Thres, the result is subnormal and not handled
     correctly by FEXPA.  */
  .thres = 0x1.5d5e2ap+6f,
};

#define C(i) sv_f32 (d->poly[i])
#define ExponentBias 0x3f800000

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  return sv_call_f32 (expf, x, y, special);
}

/* Optimised single-precision SVE exp function.
   Worst-case error is 1.04 ulp:
   SV_NAME_F1 (exp)(0x1.a8eda4p+1) got 0x1.ba74bcp+4
				  want 0x1.ba74bap+4.  */
svfloat32_t SV_NAME_F1 (exp) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* exp(x) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
     x = ln2*n + r, with r in [-ln2/2, ln2/2].  */

  /* Load some constants in quad-word chunks to minimise memory access (last
     lane is wasted).  */
  svfloat32_t invln2_and_ln2 = svld1rq (svptrue_b32 (), &d->inv_ln2);

  /* n = round(x/(ln2/N)).  */
  svfloat32_t z = svmla_lane (sv_f32 (d->shift), x, invln2_and_ln2, 0);
  svfloat32_t n = svsub_x (pg, z, d->shift);

  /* r = x - n*ln2/N.  */
  svfloat32_t r = svmls_lane (x, n, invln2_and_ln2, 1);
  r = svmls_lane (r, n, invln2_and_ln2, 2);

  /* scale = 2^(n/N).  */
  svbool_t is_special_case = svacgt (pg, x, d->thres);
  svfloat32_t scale = svexpa (svreinterpret_u32 (z));

  /* y = exp(r) - 1 ~= r + C0 r^2 + C1 r^3 + C2 r^4 + C3 r^5 + C4 r^6.  */
  svfloat32_t p12 = svmla_x (pg, C (1), C (2), r);
  svfloat32_t p34 = svmla_x (pg, C (3), C (4), r);
  svfloat32_t r2 = svmul_x (pg, r, r);
  svfloat32_t p14 = svmla_x (pg, p12, p34, r2);
  svfloat32_t p0 = svmul_x (pg, r, C (0));
  svfloat32_t poly = svmla_x (pg, p0, r2, p14);

  if (__glibc_unlikely (svptest_any (pg, is_special_case)))
    return special_case (x, svmla_x (pg, scale, scale, poly), is_special_case);

  return svmla_x (pg, scale, scale, poly);
}
