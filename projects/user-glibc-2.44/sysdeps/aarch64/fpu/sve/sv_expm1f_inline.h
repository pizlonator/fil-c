/* Single-precision inline helper for vector (SVE) expm1 function

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

#ifndef AARCH64_FPU_SV_EXPM1F_INLINE_H
#define AARCH64_FPU_SV_EXPM1F_INLINE_H

#include "sv_math.h"

struct sv_expm1f_data
{
  /* These 4 are grouped together so they can be loaded as one quadword, then
   used with _lane forms of svmla/svmls.  */
  float32_t c2, c4, ln2_hi, ln2_lo;
  float c0, inv_ln2, c1, c3, special_bound;
};

/* Coefficients generated using fpminimax.  */
#define SV_EXPM1F_DATA                                                        \
  {                                                                           \
    .c0 = 0x1.fffffep-2, .c1 = 0x1.5554aep-3, .inv_ln2 = 0x1.715476p+0f,      \
    .c2 = 0x1.555736p-5, .c3 = 0x1.12287cp-7,                                 \
                                                                              \
    .c4 = 0x1.6b55a2p-10, .ln2_lo = 0x1.7f7d1cp-20f, .ln2_hi = 0x1.62e4p-1f,  \
  }

static inline svfloat32_t
expm1f_inline (svfloat32_t x, svbool_t pg, const struct sv_expm1f_data *d)
{
  /* This vector is reliant on layout of data - it contains constants
   that can be used with _lane forms of svmla/svmls. Values are:
   [ coeff_2, coeff_4, ln2_hi, ln2_lo ].  */
  svfloat32_t lane_constants = svld1rq (svptrue_b32 (), &d->c2);

  /* Reduce argument to smaller range:
     Let i = round(x / ln2)
     and f = x - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  svfloat32_t j = svmul_x (svptrue_b32 (), x, d->inv_ln2);
  j = svrinta_x (pg, j);

  svfloat32_t f = svmls_lane (x, j, lane_constants, 2);
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

#endif
