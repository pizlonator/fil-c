/* Single-precision SVE expm1

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
#include "sv_expf_special_inline.h"

/* Value of |x| above which scale overflows without special treatment.
   ln(2^(127 + 0.5)) ~ 88.38.  */
#define SpecialBound 0x1.61814bbe4473dp+6

static const struct data
{
  /* These 4 are grouped together so they can be loaded as one quadword, then
     used with _lane forms of svmla/svmls.  */
  struct sv_expf_special_data special_data;
  float c0, c1, c3, inv_ln2;
  float ln2_hi, ln2_lo, c2, c4;
  float special_bound;
} data = {
  .special_data = SV_EXPF_SPECIAL_DATA,
  /* Generated using fpminimax.  */
  .ln2_hi = 0x1.62e4p-1f,
  .ln2_lo = 0x1.7f7d1cp-20f,
  .c0 = 0x1.fffffep-2,
  .c1 = 0x1.5554aep-3,
  .c2 = 0x1.555736p-5,
  .c3 = 0x1.12287cp-7,
  .c4 = 0x1.6b55a2p-10,
  .inv_ln2 = 0x1.715476p+0f,
  .special_bound = SpecialBound,
};

/* Single-precision SVE exp(x) - 1.
   Maximum error is 1.02 +0.5ULP:
   _ZGVsMxv_expm1f(0x1.8f4ebcp-2) got 0x1.e859dp-2
				 want 0x1.e859d4p-2.  */
svfloat32_t SV_NAME_F1 (expm1) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* This vector is reliant on layout of data - it contains constants
     that can be used with _lane forms of svmla/svmls. Values are:
     [ ln2_hi, ln2_lo, coeff_2, coeff_4 ].  */
  svfloat32_t lane_constants = svld1rq (svptrue_b32 (), &d->ln2_hi);

  /* Reduce argument to smaller range:
     Let i = round(x / ln2)
     and f = x - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  svfloat32_t n = svrinta_x (pg, svmul_x (svptrue_b32 (), x, d->inv_ln2));

  svfloat32_t f = svmls_lane_f32 (x, n, lane_constants, 0);
  f = svmls_lane_f32 (f, n, lane_constants, 1);

  /* Assemble the result.
     expm1(x) ~= scale * (poly + 1) - 1
     with scale = 2^i.  */
  svfloat32_t scale = svscale_x (pg, sv_f32 (1.0f), svcvt_s32_x (pg, n));

  /* Approximate expm1(f) using polynomial.
     Taylor expansion for expm1(x) has the form:
     x + ax^2 + bx^3 + cx^4 ....
     So we calculate the polynomial P(f) = a + bf + cf^2 + ...
     and assemble the approximation expm1(f) ~= f + f^2 * P(f).  */
  svfloat32_t f2 = svmul_x (svptrue_b32 (), f, f);
  svfloat32_t p12 = svmla_lane (sv_f32 (d->c1), f, lane_constants, 2);
  svfloat32_t p34 = svmla_lane (sv_f32 (d->c3), f, lane_constants, 3);

  svfloat32_t poly = svmla_x (pg, p12, f2, p34);
  poly = svmla_x (pg, sv_f32 (d->c0), f, poly);
  poly = svmla_x (pg, f, f2, poly);

  /* Large, NaN/Inf.  */
  svbool_t cmp = svacge_n_f32 (svptrue_b32 (), x, d->special_bound);
  /* Fallback to special case for lanes with overflow.  */
  if (__glibc_unlikely (svptest_any (cmp, cmp)))
    return special_case (poly, n, scale, cmp, &d->special_data);

  return svmla_x (pg, svsub_x (pg, scale, 1.0f), poly, scale);
}
