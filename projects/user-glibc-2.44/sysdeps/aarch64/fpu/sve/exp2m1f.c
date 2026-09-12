/* Single-precision (SVE) exp2m1 function

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

#include "sv_math.h"
#include "sv_expf_special_inline.h"

/* Value of |x| above which scale overflows without special treatment.
   log2(2^(127 + 0.5)) = 127.5.  */
#define SpecialBound 0x1.fep+6

static const struct data
{
  struct sv_expf_special_data special_data;
  float32_t special_bound;
  float log2_lo, c2, c4, c6;
  float log2_hi, c1, c3, c5, shift;
  uint32_t exponent_bias;
} data = {
  .special_data = SV_EXPF_SPECIAL_DATA,
  /* Coefficients generated using remez's algorithm for exp2m1f(x).  */
  .log2_hi = 0x1.62e43p-1,
  .log2_lo = -0x1.05c610p-29,
  .c1 = 0x1.ebfbep-3,
  .c2 = 0x1.c6b06ep-5,
  .c3 = 0x1.3b2a5cp-7,
  .c4 = 0x1.5da59ep-10,
  .c5 = 0x1.440dccp-13,
  .c6 = 0x1.e081d6p-17,
  .exponent_bias = 0x3f800000,
  .special_bound = SpecialBound,
};

/* Single-precision vector exp2(x) - 1 function.
   The maximum error is  1.76 + 0.5 ULP.
   _ZGVsMxv_exp2m1f (0x1.018af8p-1) got 0x1.ab2ebcp-2
				   want 0x1.ab2ecp-2.  */
svfloat32_t SV_NAME_F1 (exp2m1) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat32_t n = svrinta_x (pg, x);
  svfloat32_t r = svsub_x (pg, x, n);

  svfloat32_t scale = svscale_x (pg, sv_f32 (1.0f), svcvt_s32_x (pg, n));

  svfloat32_t r2 = svmul_x (pg, r, r);

  svfloat32_t log2lo_c246 = svld1rq (svptrue_b32 (), &d->log2_lo);
  svfloat32_t p56 = svmla_lane (sv_f32 (d->c5), r, log2lo_c246, 3);
  svfloat32_t p34 = svmla_lane (sv_f32 (d->c3), r, log2lo_c246, 2);
  svfloat32_t p12 = svmla_lane (sv_f32 (d->c1), r, log2lo_c246, 1);

  svfloat32_t p36 = svmla_x (pg, p34, p56, r2);
  svfloat32_t p16 = svmla_x (pg, p12, p36, r2);

  svfloat32_t poly = svmla_lane (
      svmul_x (svptrue_b32 (), r, sv_f32 (d->log2_hi)), r, log2lo_c246, 0);
  poly = svmla_x (pg, poly, p16, r2);

  svbool_t cmp = svacge_n_f32 (svptrue_b32 (), x, d->special_bound);
  /* Fallback to special case for lanes with overflow.  */
  if (__glibc_unlikely (svptest_any (cmp, cmp)))
    return special_case (poly, n, scale, cmp, &d->special_data);

  return svmla_x (pg, svsub_x (pg, scale, 1.0f), poly, scale);
}
