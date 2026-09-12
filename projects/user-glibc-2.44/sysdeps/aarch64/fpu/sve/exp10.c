/* Double-precision vector (SVE) exp10 function.

   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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
#include "sv_exp_special_inline.h"

/* Value of |x| above which scale overflows without special treatment.
   log10(2^1022 + 1/128) ~ 307.65.  */
#define SpecialBound 0x1.33a7ae900b507p+8

static const struct data
{
  struct sv_exp_special_data special_data;
  double log2_10_hi, log2_10_lo;
  double log10_2, c0;
  double c1, c3, c2, c4;
  double shift, special_bound;
} data = {
  .special_data = SV_EXP_SPECIAL_DATA,
  /* Coefficients generated using Remez algorithm.
     rel error: 0x1.9fcb9b3p-60
     abs error: 0x1.a20d9598p-60 in [ -log10(2)/128, log10(2)/128 ]
     max ulp err 0.52 +0.5.  */
  .c0 = 0x1.26bb1bbb55516p1,
  .c1 = 0x1.53524c73cd32ap1,
  .c2 = 0x1.0470591daeafbp1,
  .c3 = 0x1.2bd77b1361ef6p0,
  .c4 = 0x1.142b5d54e9621p-1,
  /* 1.5*2^46+1023. This value is further explained below.  */
  .shift = 0x1.800000000ffc0p+46,
  .log10_2 = 0x1.a934f0979a371p1,     /* 1/log2(10).  */
  .log2_10_hi = 0x1.34413509f79ffp-2, /* log2(10).  */
  .log2_10_lo = -0x1.9dc1da994fd21p-59,
  .special_bound = SpecialBound,
};

static svfloat64_t NOINLINE
special_exp (svfloat64_t scale, svfloat64_t poly, svfloat64_t n, svuint64_t u,
	     const struct sv_exp_special_data *ds)
{
  /* FEXPA zeroes the sign bit, however the sign is meaningful to the
  special case function so needs to be copied.
  e = sign bit of u << 46.  */
  svuint64_t e = svand_x (svptrue_b64 (), svlsl_x (svptrue_b64 (), u, 46),
			  0x8000000000000000);
  /* Copy sign to scale.  */
  scale = svreinterpret_f64 (
      svadd_x (svptrue_b64 (), e, svreinterpret_u64 (scale)));
  return special_case (scale, poly, n, ds);
}

/* Fast vector implementation of exp10 using FEXPA instruction.
   Maximum measured error is 1.02 ulp.
   SV_NAME_D1 (exp10)(-0x1.2862fec805e58p+2) got 0x1.885a89551d782p-16
					    want 0x1.885a89551d781p-16.  */
svfloat64_t SV_NAME_D1 (exp10) (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* n = round(x/(log10(2)/N)).  */
  svfloat64_t shift = sv_f64 (d->shift);
  svfloat64_t log10_2_c0 = svld1rq (svptrue_b64 (), &d->log10_2);
  svfloat64_t z = svmla_lane (shift, x, log10_2_c0, 0);
  svfloat64_t n = svsub_x (pg, z, shift);

  /* r = x - n*log10(2)/N.  */
  svfloat64_t log2_10 = svld1rq (svptrue_b64 (), &d->log2_10_hi);
  svfloat64_t r = x;
  r = svmls_lane (r, n, log2_10, 0);
  r = svmls_lane (r, n, log2_10, 1);

  /* scale = 2^(n/N), computed using FEXPA. FEXPA does not propagate NaNs, so
     for consistent NaN handling we have to manually propagate them. This
     comes at significant performance cost.  */
  svuint64_t u = svreinterpret_u64 (z);
  svfloat64_t scale = svexpa (u);
  svfloat64_t c24 = svld1rq (svptrue_b64 (), &d->c2);
  /* Approximate exp10(r) using polynomial.  */
  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t p12 = svmla_lane (sv_f64 (d->c1), r, c24, 0);
  svfloat64_t p34 = svmla_lane (sv_f64 (d->c3), r, c24, 1);
  svfloat64_t p14 = svmla_x (pg, p12, p34, r2);

  svfloat64_t poly = svmla_x (pg, svmul_lane (r, log10_2_c0, 1), r2, p14);

  svbool_t special = svacge (svptrue_b64 (), x, d->special_bound);
  /* Assemble result as exp(x) = 2^n * exp(r).  If |x| > Thresh the
     multiplication may overflow, so use special case routine.  */
  if (__glibc_unlikely (svptest_any (special, special)))
    return special_exp (scale, poly, n, u, &d->special_data);

  /* No special case.  */
  return svmla_x (pg, scale, scale, poly);
}
