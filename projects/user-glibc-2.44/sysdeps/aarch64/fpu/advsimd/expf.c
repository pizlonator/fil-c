/* Single-precision vector (Advanced SIMD) exp function.

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

#include "v_math.h"
#include "v_expf_special_inline.h"

/* Value of |x| above which scale overflows without special treatment.  */
#define SpecialBound 0x1.5ebb83cf2cf96p+6 /* ≈ 87.69.  */

static const struct data
{
  struct v_expf_special_data special_data;
  float32x4_t c1, c3, c4, inv_ln2;
  float ln2_hi, ln2_lo, c0, c2;
  uint32x4_t exponent_bias;
  float32x4_t special_bound;
} data = {
  .special_data = V_EXPF_SPECIAL_DATA,
  .c0 = 0x1.0e4020p-7f,
  .c1 = V4 (0x1.573e2ep-5f),
  .c2 = 0x1.555e66p-3f,
  .c3 = V4 (0x1.fffdb6p-2f),
  .c4 = V4 (0x1.ffffecp-1f),
  .inv_ln2 = V4 (0x1.715476p+0f),
  .ln2_hi = 0x1.62e4p-1f,
  .ln2_lo = 0x1.7f7d1cp-20f,
  .exponent_bias = V4 (0x3f800000),
  /* Implementation triggers special case handling as soon as the scale
     overflows, which is earlier than expf's overflow bound
     `log1p(FLT_MAX)~88.7` or underflow bound `log1p(FLT_MIN)~-103.28`.
     The absolute comparison catches all these cases efficiently, although
     there is a small window where special cases are triggered
     unnecessarily.  */
  .special_bound = V4 (SpecialBound),
};

/* Single-precision vector expf routine.
   The maximum error is 1.44 +0.5 ULP:
   _ZGVnN4v_expf(-0x1.86f03cp+5) got 0x1.69e27p-71
				want 0x1.69e274p-71.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (exp) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  float32x4_t ln2_c02 = vld1q_f32 (&d->ln2_hi);

  /* exp(x) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
     x = ln2*n + r, with r in [-ln2/2, ln2/2].  */
  float32x4_t n = vrndaq_f32 (vmulq_f32 (x, d->inv_ln2));
  float32x4_t r = vfmsq_laneq_f32 (x, n, ln2_c02, 0);
  r = vfmsq_laneq_f32 (r, n, ln2_c02, 1);
  uint32x4_t e = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtq_s32_f32 (n)), 23);
  float32x4_t scale = vreinterpretq_f32_u32 (vaddq_u32 (e, d->exponent_bias));

  uint32x4_t cmp = vcageq_f32 (x, d->special_bound);

  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t p = vfmaq_laneq_f32 (d->c1, r, ln2_c02, 2);
  float32x4_t q = vfmaq_laneq_f32 (d->c3, r, ln2_c02, 3);
  q = vfmaq_f32 (q, p, r2);
  p = vmulq_f32 (d->c4, r);
  float32x4_t poly = vfmaq_f32 (p, q, r2);

  if (__glibc_unlikely (v_any_u32 (cmp)))
    return expf_special (poly, n, e, cmp, scale, &d->special_data);

  return vfmaq_f32 (scale, poly, scale);
}
libmvec_hidden_def (V_NAME_F1 (exp))
HALF_WIDTH_ALIAS_F1 (exp)
