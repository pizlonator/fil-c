/* Single-precision vector (Advanced SIMD) tanh function

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

#include "v_math.h"

static const struct data
{
  float32x4_t special_bound, two;
  float32x4_t c0, c2;
  int32x4_t exponent_bias;
  float c1, c3, two_over_ln2, c4;
  float ln2_hi, ln2_lo;
} data = {
  /* 9.01, above which tanhf rounds to 1 (or -1 for  negative).  */
  .special_bound = V4 (0x1.205966p+3),
  .two = V4 (0x1.0p+1), /* 2.0.  */
  /* Coefficients generated using fpminimax with degree=5 in [-log(2)/2,
    log(2)/2]. Exponent bias is asuint(1.0f).  */
  .c0 = V4 (0x1.fffffep-2),
  .c1 = 0x1.5554aep-3,
  .c2 = V4 (0x1.555736p-5),
  .c3 = 0x1.12287cp-7,
  .c4 = 0x1.6b55a2p-10,
  .exponent_bias = V4 (0x3f800000),
  .two_over_ln2 = 0x1.715476p+1f,
  .ln2_hi = 0x1.62e4p-1f,
  .ln2_lo = 0x1.7f7d1cp-20f,
};

/* e^2x - 1 inline helper.  */
static inline float32x4_t
e2xm1f_inline (float32x4_t x, const struct data *d)
{
  float32x2_t ln2 = vld1_f32 (&d->ln2_hi);
  float32x4_t lane_consts = vld1q_f32 (&d->c1);

  /* Reduce argument: f in [-ln2/2, ln2/2], i is exact.  */
  float32x4_t j = vrndaq_f32 (vmulq_laneq_f32 (x, lane_consts, 2));
  int32x4_t i = vcvtq_s32_f32 (j);
  float32x4_t f = vaddq_f32 (x, x);
  f = vfmsq_lane_f32 (f, j, ln2, 0);
  f = vfmsq_lane_f32 (f, j, ln2, 1);

  /* Approximate expm1(f) with polynomial P, expm1(f) ~= f + f^2 * P(f).  */
  float32x4_t f2 = vmulq_f32 (f, f);
  float32x4_t f4 = vmulq_f32 (f2, f2);
  float32x4_t p01 = vfmaq_laneq_f32 (d->c0, f, lane_consts, 0);
  float32x4_t p23 = vfmaq_laneq_f32 (d->c2, f, lane_consts, 1);
  float32x4_t poly = vfmaq_f32 (p01, f2, p23);
  poly = vfmaq_laneq_f32 (poly, f4, lane_consts, 3);
  poly = vfmaq_f32 (f, f2, poly);

  /* scale = 2^i.  */
  int32x4_t u = vaddq_s32 (vshlq_n_s32 (i, 23), d->exponent_bias);
  float32x4_t scale = vreinterpretq_f32_s32 (u);
  /* expm1(x) ~= poly * scale + (scale - 1).  */
  return vfmaq_f32 (vsubq_f32 (scale, v_f32 (1.0f)), poly, scale);
}

static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, float32x4_t q, uint32x4_t special)
{
  const struct data *d = ptr_barrier (&data);

  /* Complete fast path.  */
  float32x4_t y = vdivq_f32 (q, vaddq_f32 (q, d->two));

  uint32x4_t ix = vreinterpretq_u32_f32 (x);

  /* expm1 exponent bias is +1.0f.  */
  uint32x4_t one_bits = vreinterpretq_u32_s32 (d->exponent_bias);

  /* Mask selecting only the sign bit in each lane.  */
  uint32x4_t sign_mask = vdupq_n_u32 (0x80000000u);

  /* Produce signed 1 for return of special cases:
    sign bit taken from ix
    all other bits taken from +1.0f (one_bits).  */
  uint32x4_t special_bits = vbslq_u32 (sign_mask, ix, one_bits);
  float32x4_t special_y = vreinterpretq_f32_u32 (special_bits);

  /* Select between special case or regular case and return value.  */
  return vbslq_f32 (special, special_y, y);
}

/* Approximation for single-precision vector tanh(x), using a simplified
   version of expm1f. The maximum error is 2.08 + 0.5 ULP:
   _ZGVnN4v_tanhf (0x1.fa5eep-5) got 0x1.f9ba02p-5
				want 0x1.f9ba08p-5.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (tanh) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* tanh(x) = (e^2x - 1) / (e^2x + 1).  */
  float32x4_t q = e2xm1f_inline (x, d);

  /* Check for special cases.  */
  uint32x4_t special = vcagtq_f32 (x, d->special_bound);

  /* Fall back to vectorised special case for any lanes which would cause
     expm1 to overflow.  */
  if (__glibc_unlikely (v_any_u32 (special)))
    return special_case (x, q, special);

  /* Complete fast path if no special lanes.  */
  return vdivq_f32 (q, vaddq_f32 (q, d->two));
}
libmvec_hidden_def (V_NAME_F1 (tanh))
HALF_WIDTH_ALIAS_F1 (tanh)
