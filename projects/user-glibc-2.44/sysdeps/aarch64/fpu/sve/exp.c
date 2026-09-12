/* Double-precision vector (SVE) exp function.

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
   ln(2^(1022 + 1/128)) ~ 708.40.  */
#define SpecialBound 0x1.62336f49c49a2p+9

static const struct data
{
  struct sv_exp_special_data special_data;
  double c0, c2;
  double c1, c3;
  double ln2_hi, ln2_lo;
  double inv_ln2, shift, special_bound;
} data = {
  .special_data = SV_EXP_SPECIAL_DATA,
  .c0 = 0x1.fffffffffdbcdp-2,
  .c1 = 0x1.555555555444cp-3,
  .c2 = 0x1.555573c6a9f7dp-5,
  .c3 = 0x1.1111266d28935p-7,
  .ln2_hi = 0x1.62e42fefa3800p-1,
  .ln2_lo = 0x1.ef35793c76730p-45,
  /* 1/ln2.  */
  .inv_ln2 = 0x1.71547652b82fep+0,
  /* 1.5*2^46+1023. This value is further explained below.  */
  .shift = 0x1.800000000ffc0p+46,
  .special_bound = SpecialBound,
};

static svfloat64_t NOINLINE
special_exp (svfloat64_t poly, svfloat64_t scale, svfloat64_t n, svuint64_t u,
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

/* SVE exp algorithm. Maximum measured error is 1.01ulps:
   SV_NAME_D1 (exp)(0x1.4619d7b04da41p+6) got 0x1.885d9acc41da7p+117
					 want 0x1.885d9acc41da6p+117.  */
svfloat64_t SV_NAME_D1 (exp) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* Use a modifed version of the shift used for flooring, such that x/ln2 is
     rounded to a multiple of 2^-6=1/64, shift = 1.5 * 2^52 * 2^-6 = 1.5 *
     2^46.

     n is not an integer but can be written as n = m + i/64, with i and m
     integer, 0 <= i < 64 and m <= n.

     Bits 5:0 of z will be null every time x/ln2 reaches a new integer value
     (n=m, i=0), and is incremented every time z (or n) is incremented by 1/64.
     FEXPA expects i in bits 5:0 of the input so it can be used as index into
     FEXPA hardwired table T[i] = 2^(i/64) for i = 0:63, that will in turn
     populate the mantissa of the output. Therefore, we use u=asuint(z) as
     input to FEXPA.

     We add 1023 to the modified shift value in order to set bits 16:6 of u to
     1, such that once these bits are moved to the exponent of the output of
     FEXPA, we get the exponent of 2^n right, i.e. we get 2^m.  */
  svfloat64_t z = svmla_x (pg, sv_f64 (d->shift), x, d->inv_ln2);
  svuint64_t u = svreinterpret_u64 (z);
  svfloat64_t n = svsub_x (pg, z, d->shift);
  svfloat64_t c13 = svld1rq (svptrue_b64 (), &d->c1);
  /* r = x - n * ln2, r is in [-ln2/(2N), ln2/(2N)].  */
  svfloat64_t ln2_hi_lo = svld1rq (svptrue_b64 (), &d->ln2_hi);
  svfloat64_t r = x;
  r = svmls_lane (r, n, ln2_hi_lo, 0);
  r = svmls_lane (r, n, ln2_hi_lo, 1);

  /* poly = exp(r) - 1 ~= r + C0 r^2 + C1 r^3 + C2 r^4 + C3 r^5.  */
  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t p01 = svmla_lane (sv_f64 (d->c0), r, c13, 0);
  svfloat64_t p23 = svmla_lane (sv_f64 (d->c2), r, c13, 1);
  svfloat64_t p04 = svmla_x (pg, p01, p23, r2);
  svfloat64_t poly = svmla_x (pg, r, p04, r2);

  /* scale = 2^n, computed using FEXPA. FEXPA does not propagate NaNs, so for
     consistent NaN handling we have to manually propagate them. This comes at
     significant performance cost.  */
  svfloat64_t scale = svexpa (u);

  svbool_t special = svacge (svptrue_b64 (), x, d->special_bound);
  /* Assemble result as exp(x) = 2^n * exp(r).  If |x| > Thresh the
     multiplication may overflow, so use special case routine.  */
  if (__glibc_unlikely (svptest_any (special, special)))
    return special_exp (poly, scale, n, u, &d->special_data);

  /* No special case.  */
  return svmla_x (pg, scale, scale, poly);
}
