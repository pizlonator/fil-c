/* Helper for single-precision AdvSIMD expB special cases

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

#ifndef AARCH64_FPU_V_EXPF_SPECIAL_INLINE_H
#define AARCH64_FPU_V_EXPF_SPECIAL_INLINE_H

#include "v_math.h"

static const struct v_expf_special_data
{
  uint32x4_t special_offset, special_bias;
  float32x4_t scale_bound;
} V_EXPF_SPECIAL_DATA = {
  .special_offset = V4 (0x82000000),
  .special_bias = V4 (0x7f000000),
  /* Value of n above which scale overflows even with special treatment.  */
  .scale_bound = V4 (0x1.8p+7), /* 192.0f.  */
};

static inline float32x4_t VPCS_ATTR
expf_special (float32x4_t poly, float32x4_t n, uint32x4_t e, uint32x4_t cmp1,
	      float32x4_t scale, const struct v_expf_special_data *ds)
{
  /* 2^n may overflow, break it up into s1*s2.  */
  uint32x4_t b = vandq_u32 (vclezq_f32 (n), ds->special_offset);
  float32x4_t s1 = vreinterpretq_f32_u32 (vaddq_u32 (b, ds->special_bias));
  float32x4_t s2 = vreinterpretq_f32_u32 (vsubq_u32 (e, b));
  uint32x4_t cmp2 = vcagtq_f32 (n, ds->scale_bound);
  float32x4_t r2 = vmulq_f32 (s1, s1);
  float32x4_t r1 = vmulq_f32 (vfmaq_f32 (s2, poly, s2), s1);
  /* Similar to r1 but avoids double rounding in the subnormal range.  */
  float32x4_t r0 = vfmaq_f32 (scale, poly, scale);
  float32x4_t r = vbslq_f32 (cmp1, r1, r0);
  return vbslq_f32 (cmp2, r2, r);
}

#endif
