/* Single-precision vector (Advanced SIMD) sin function.

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

static inline VPCS_ATTR float32x4_t
sin_fallback (float32x4_t x)
{
  struct reduction_result_t r = large_range_reduction (x);
  float32x4x2_t lookup = sin_cos_lookup (r.octant);
  float32x4x2_t eval_fast = sincos_eval (r.remainder);

  /* Construct sin(x) from k and r, using angle addition formula, with
     approximations of sin(r) and cos(r) - 1 to reduce rounding errors.
     sin(x) = sin(k + r)
	    = cos(k)*sin(r) + sin(k)*cos(r)
	    = cos(k)*sin(r) + sin(k)*cosm1(r) + sin(k).  */

  float32x4_t sin_k = lookup.val[0];
  float32x4_t cos_k = lookup.val[1];
  float32x4_t sin_r = eval_fast.val[0];
  float32x4_t cosm1_r = eval_fast.val[1];

  float32x4_t sin_k_cosm1_r = vmulq_f32 (sin_k, cosm1_r);
  float32x4_t result = vfmaq_f32 (sin_k_cosm1_r, cos_k, sin_r);
  return vaddq_f32 (result, sin_k);
}

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, uint32x4_t odd, uint32x4_t cmp)
{
  uint32x4_t is_inf = vcageq_f32 (x, v_f32 (INFINITY));
  float32x4_t large = sin_fallback (x);
  large = vbslq_f32 (is_inf, v_f32 (NAN), large);
  y = vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (y), odd));
  return vbslq_f32 (cmp, large, y);
}

/* Vector version of sinf.
   Maximum observed error is 1.25 + 0.5 ULP if |x| < 0x1p20.
   _ZGVnN4v_sinf (0x1.f0ea62p-2)
    got 0x1.fdf15p-1
   want 0x1.fdf154p-1.
   The special domain has a slightly higher maximum error than the fast path:
   Maximum observed error is 1.42 + 0.5ULP
   _ZGVnN4v_sinf (0x1.dbe63cp+36)
    got -0x1.d76b4p-2
   want -0x1.d76b44p-2.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (sin) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint32x4_t cmp = vcageq_f32 (x, d->range_val);

  float32x4_t pi_vals = vld1q_f32 (&d->inv_pi);
  /* n = rint(|x|/pi).  */
  float32x4_t n = vrndaq_f32 (vmulq_laneq_f32 (x, pi_vals, 0));
  uint32x4_t odd = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtq_s32_f32 (n)), 31);

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
libmvec_hidden_def (V_NAME_F1 (sin))
HALF_WIDTH_ALIAS_F1 (sin)
