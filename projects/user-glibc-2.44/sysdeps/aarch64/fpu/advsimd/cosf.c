/* Single-precision vector (Advanced SIMD) cos function.

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
#include "v_trigf_fallback.h"

static const struct data
{
  float inv_pi, pi_1, pi_2, pi_3;
  float32x4_t c0, c1, c2, c3;
  float32x4_t range_val;
} data = {
  .c0 = V4 (-0x1.55554ep-3f),
  .c1 = V4 (0x1.110f22p-7f),
  .c2 = V4 (-0x1.9f7cd6p-13f),
  .c3 = V4 (0x1.5e6364p-19f),

  .inv_pi = 0x1.45f306p-2f,
  .pi_1 = 0x1.921fb6p+1f,
  .pi_2 = -0x1.777a5cp-24f,
  .pi_3 = -0x1.ee59dap-49f,

  .range_val = V4 (0x1p20f),
};

static inline float32x4_t VPCS_ATTR
cos_fallback (float32x4_t x)
{
  struct reduction_result_t r = large_range_reduction (x);
  float32x4x2_t lookup = sin_cos_lookup (r.octant);
  float32x4x2_t eval_fast = sincos_eval (r.remainder);

  /* Construct cos(x) from k and r, using angle addition formula, with
     approximations of sin(r) and cos(r) - 1 to reduce rounding errors.
     cos(x) = cos(k + r)
	     = cos(k)*cos(r) - sin(k)*sin(r)
	     = cos(k)*cosm1(r) - sin(k)*sin(r) + cos(k).  */

  /* lookup = { sin(k), cos(k) }, eval_fast = { sin(r), cos(r) - 1 }.  */
  float32x4_t sin_k = lookup.val[0];
  float32x4_t cos_k = lookup.val[1];
  float32x4_t sin_r = eval_fast.val[0];
  float32x4_t cosm1_r = eval_fast.val[1];

  float32x4_t cos_k_cosm1_r = vmulq_f32 (cos_k, cosm1_r);
  float32x4_t result = vfmsq_f32 (cos_k_cosm1_r, sin_k, sin_r);
  return vaddq_f32 (result, cos_k);
}

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, uint32x4_t odd, uint32x4_t cmp)
{
  uint32x4_t is_inf = vcageq_f32 (x, v_f32 (INFINITY));
  float32x4_t large = cos_fallback (x);
  large = vbslq_f32 (is_inf, v_f32 (NAN), large);
  y = vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (y), odd));
  return vbslq_f32 (cmp, large, y);
}

/* Vector version of cosf.
   The maximum observed error is 1.31 + 0.5 ULP if |x| < 0x1p20.
   _ZGVnN4v_cosf (0x1.35fb0cp-2)
    got 0x1.e8b83cp-1
   want 0x1.e8b838p-1.
   The special domain has a slightly higher maximum error than the fast path:
   Maximum observed error is 1.43 + 0.5ULP
   _ZGVnN4v_cosf (0x1.a2785ap+108)
    got -0x1.dc17fp-2
   want -0x1.dc17f4p-2.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (cos) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  float32x4_t pi_vals = vld1q_f32 (&d->inv_pi);
  uint32x4_t cmp = vcageq_f32 (x, d->range_val);

  /* n = rint((|x|+pi/2)/pi) - 0.5.  */
  float32x4_t n = vrndaq_f32 (vfmaq_laneq_f32 (v_f32 (0.5f), x, pi_vals, 0));
  uint32x4_t odd = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtq_s32_f32 (n)), 31);
  n = vsubq_f32 (n, v_f32 (0.5f));

  /* r = |x| - n*pi  (range reduction into -pi/2 .. pi/2).  */
  float32x4_t r = x;
  r = vfmsq_laneq_f32 (r, n, pi_vals, 1);
  r = vfmsq_laneq_f32 (r, n, pi_vals, 2);
  r = vfmsq_laneq_f32 (r, n, pi_vals, 3);

  /* y = sin(r).  */
  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t r3 = vmulq_f32 (r2, r);

  float32x4_t y;
  y = vfmaq_f32 (d->c2, r2, d->c3);
  y = vfmaq_f32 (d->c1, r2, y);
  y = vfmaq_f32 (d->c0, r2, y);
  y = vfmaq_f32 (r, r3, y);

  if (__glibc_unlikely (v_any_u32 (cmp)))
    return special_case (x, y, odd, cmp);
  return vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (y), odd));
}
libmvec_hidden_def (V_NAME_F1 (cos))
HALF_WIDTH_ALIAS_F1 (cos)
