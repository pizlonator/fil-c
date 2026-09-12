/* SVE helper for single-precision expBm1 special routines

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

#ifndef AARCH64_FPU_SV_EXPF_SPECIAL_INLINE_H
#define AARCH64_FPU_SV_EXPF_SPECIAL_INLINE_H

#include "sv_math.h"

static const struct sv_expf_special_data
{
  uint32_t exponent_bias, special_offset, special_bias;
  float32_t scale_thresh;
} SV_EXPF_SPECIAL_DATA = {
  .special_offset = 0x82000000,
  .special_bias = 0x7f000000,
};

/* Special case routine shared with other expBm1 routines.  */
static inline svfloat32_t
special_exp (svfloat32_t poly, svfloat32_t n, svuint32_t e, svbool_t cmp1,
	     svfloat32_t scale, const struct sv_expf_special_data *ds)
{
  svbool_t b = svcmple (svptrue_b32 (), n, 0.0f);
  svfloat32_t s1 = svreinterpret_f32 (
      svsel (b, sv_u32 (ds->special_offset + ds->special_bias),
	     sv_u32 (ds->special_bias)));
  svfloat32_t s2
      = svreinterpret_f32 (svsub_m (b, e, sv_u32 (ds->special_offset)));
  /* Value of n above which scale overflows even with special treatment.  */
  svbool_t cmp2 = svacgt (svptrue_b32 (), n, 192.0);
  svfloat32_t r2 = svmul_x (svptrue_b32 (), s1, s1);
  svfloat32_t r1
      = svmul_x (svptrue_b32 (), svmla_x (svptrue_b32 (), s2, poly, s2), s1);
  svfloat32_t r0 = svmla_x (svptrue_b32 (), scale, poly, scale);
  svfloat32_t r = svsel (cmp1, r1, r0);
  return svsel (cmp2, r2, r);
}

/* Special case routine for expBm1.  */
static svfloat32_t NOINLINE
special_case (svfloat32_t poly, svfloat32_t n, svfloat32_t scale,
	      svbool_t cmp1, const struct sv_expf_special_data *ds)
{
  /* Compute unbiased exponent of scale.  */
  svuint32_t e = svlsl_x (
      svptrue_b32 (), svreinterpret_u32 (svcvt_s32_x (svptrue_b32 (), n)), 23);
  /* compute special exp and subtract 1.  */
  svfloat32_t special = svsub_x (
      svptrue_b32 (), special_exp (poly, n, e, cmp1, scale, ds), 1.0f);
  /* compute non-special output.  */
  svfloat32_t y = svmla_x (svptrue_b32 (),
			   svsub_x (svptrue_b32 (), scale, 1.0f), poly, scale);
  return svsel_f32 (cmp1, special, y);
}

#endif
