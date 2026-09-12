/* Helper for double-precision AdvSIMD expB special cases

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

#ifndef AARCH64_FPU_V_EXP_SPECIAL_INLINE_H
#define AARCH64_FPU_V_EXP_SPECIAL_INLINE_H

#include "v_math.h"

static const struct v_exp_special_data
{
  uint64x2_t special_offset, special_bias1, special_bias2;
} V_EXP_SPECIAL_DATA = {
  .special_offset = V2 (0x6000000000000000), /* 0x1p513.  */
  .special_bias1 = V2 (0x7000000000000000),  /* 0x1p769.  */
  .special_bias2 = V2 (0x3010000000000000),  /* 0x1p-254.  */
};

static inline float64x2_t VPCS_ATTR
exp_special (float64x2_t poly, float64x2_t n, float64x2_t scale,
	     float64x2_t scale_bound, const struct v_exp_special_data *ds)
{
  /* 2^n may overflow, break it up into s1*s2.  */
  uint64x2_t b = vandq_u64 (vclezq_f64 (n), ds->special_offset);
  float64x2_t s1 = vreinterpretq_f64_u64 (vsubq_u64 (ds->special_bias1, b));
  float64x2_t s2 = vreinterpretq_f64_u64 (vaddq_u64 (
      vsubq_u64 (vreinterpretq_u64_f64 (scale), ds->special_bias2), b));
  uint64x2_t cmp2 = vcagtq_f64 (n, scale_bound);
  float64x2_t r1 = vmulq_f64 (s1, s1);
  float64x2_t r2 = vmulq_f64 (vfmaq_f64 (s2, s2, poly), s1);
  return vbslq_f64 (cmp2, r1, r2);
}

#endif
