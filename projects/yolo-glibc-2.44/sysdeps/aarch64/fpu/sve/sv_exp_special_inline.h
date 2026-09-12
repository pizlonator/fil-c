/* SVE helper for double-precision expB special routines

   Copyright (C) 2026 Free Software Foundation, Inc.
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

#ifndef AARCH64_FPU_SV_EXP_SPECIAL_INLINE_H
#define AARCH64_FPU_SV_EXP_SPECIAL_INLINE_H

#include "sv_math.h"

static const struct sv_exp_special_data
{
  uint64_t special_offset, special_bias1, special_bias2;
} SV_EXP_SPECIAL_DATA = {
  .special_offset = 0x6000000000000000,
  .special_bias1 = 0x7000000000000000, /* 0x1p769.  */
  .special_bias2 = 0x3010000000000000, /* 0x1p-254.  */
};

static inline svfloat64_t
special_case (svfloat64_t scale, svfloat64_t poly, svfloat64_t n,
	      const struct sv_exp_special_data *ds)
{
  /* scale = 2^n may overflow, break it up into s=s1*s2,
     such that exp = scale + scale*poly can be computed as s1*(s2+s2*poly)
     and s1*s1 overflows only if n>0.  */

  /* If n<=0 then set b to 0x6, 0 otherwise.  */
  svbool_t p_sign = svcmple (svptrue_b64 (), n, 0.0); /* n <= 0.  */
  svuint64_t b = svdup_u64_z (
      p_sign, ds->special_offset); /* Inactive lanes set to 0.  */
  /* Set s1 to generate overflow depending on sign of exponent n,
     ie. s1 = 0x70...0 - b.  */
  svfloat64_t s1
      = svreinterpret_f64 (svsubr_x (svptrue_b64 (), b, ds->special_bias1));
  /* Offset s to avoid overflow in final result if n is below threshold.
     ie. s2 = as_u64 (s) - 0x3010...0 + b.  */
  svuint64_t biased_scale
      = svsub_x (svptrue_b64 (), svreinterpret_u64 (scale), ds->special_bias2);
  svfloat64_t s2
      = svreinterpret_f64 (svadd_x (svptrue_b64 (), biased_scale, b));

  /* |n| > 1280 => 2^(n) overflows.  */
  svbool_t p_cmp = svacgt (svptrue_b64 (), n, 1280);

  svfloat64_t r1 = svmul_x (svptrue_b64 (), s1, s1);
  svfloat64_t r2 = svmla_x (svptrue_b64 (), s2, s2, poly);
  svfloat64_t r0 = svmul_x (svptrue_b64 (), r2, s1);

  return svsel (p_cmp, r1, r0);
}

#endif
