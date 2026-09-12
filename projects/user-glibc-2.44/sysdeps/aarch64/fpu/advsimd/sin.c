/* Double-precision vector (Advanced SIMD) sin function.

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
#include "v_trig_fallback.h"

static const struct data
{
  double c1, c3, c5, c6;
  double inv_pi, pi_1, pi_2, pi_3;
  float64x2_t c0, c2, c4;
  float64x2_t range_val;
} data = { .c0 = V2 (-0x1.555555555547bp-3),
	   .c1 = 0x1.1111111108a4dp-7,
	   .c2 = V2 (-0x1.a01a019936f27p-13),
	   .c3 = 0x1.71de37a97d93ep-19,
	   .c4 = V2 (-0x1.ae633919987c6p-26),
	   .c5 = 0x1.60e277ae07cecp-33,
	   .c6 = -0x1.9e9540300a1p-41,
	   .inv_pi = 0x1.45f306dc9c883p-2,
	   .pi_1 = 0x1.921fb54442d18p+1,
	   .pi_2 = 0x1.1a62633145c06p-53,
	   .pi_3 = 0x1.c1cd129024e09p-106,
	   .range_val = V2 (0x1p23) };

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t odd, uint64x2_t cmp)
{
  const struct reduction_data *d = ptr_barrier (&reduction_data);
  struct reduction_result_t r = v_large_range_reduction (x, d);

  float64x2x2_t eval = v_sincos_eval (r.remainder, d);
  float64x2x2_t lookup = v_sincos_lookup (r.quadrant);

  float64x2_t sin_r = eval.val[0];
  float64x2_t cosm1_r = eval.val[1];
  float64x2_t sin_k = lookup.val[0];
  float64x2_t cos_k = lookup.val[1];

  /* Construct sin(x) from k and r, using angle addition formula, with
     approximations of sin(r) and cos(r) - 1 to reduce rounding errors.
     sin(x) = sin(k + r)
	    = cos(k)*sin(r) + sin(k)*cos(r)
	    = cos(k)*sin(r) + sin(k)*cosm1(r) + sin(k).  */

  float64x2_t large_sin = vfmaq_f64 (sin_k, cos_k, sin_r);
  large_sin = vfmaq_f64 (large_sin, sin_k, cosm1_r);
  float64x2_t small_sin
      = vreinterpretq_f64_u64 (veorq_u64 (vreinterpretq_u64_f64 (y), odd));

  /* Inf cases are handled correctly by the fast path, and incorrectly
     by the slow path. However, it's less costly to the fast path to
     handle them separately. So we do want to branch here for inf cases,
     but then use the fast path value anyway.  */
  uint64x2_t is_finite = vcaltq_f64 (x, v_f64 (INFINITY));
  cmp = vandq_u64 (cmp, is_finite);
  return vbslq_f64 (cmp, large_sin, small_sin);
}

/* Vector version of sin.
   Maximum observed error is 2.15 + 0.5ULP when |x| >= 0x1p23.
   _ZGVnN2v_sin (0x1.3d4ded894041ep+784)
    got -0x1.fffa6b28930b5p-7
   want -0x1.fffa6b28930b2p-7
   The fast path has a higher maximum error than the special domain:
   The maximum observed error is 2.72 + 0.5 ULP when |x| < 0x1p23.
   _ZGVnN2v_sin (0x1.5702447b6f17bp+22)
    got 0x1.ffdcd125c84fbp-3
   want 0x1.ffdcd125c84f8p-3.  */
float64x2_t VPCS_ATTR V_NAME_D1 (sin) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint64x2_t cmp = vcageq_f64 (x, d->range_val);

  float64x2_t invpi_pi_1 = vld1q_f64 (&d->inv_pi);
  float64x2_t pi_2_3 = vld1q_f64 (&d->pi_2);

  /* n = rint(x/pi).  */
  float64x2_t n = vrndaq_f64 (vmulq_laneq_f64 (x, invpi_pi_1, 0));
  uint64x2_t odd = vshlq_n_u64 (vreinterpretq_u64_s64 (vcvtq_s64_f64 (n)), 63);

  /* r = x - n*pi  (range reduction into -pi/2 .. pi/2).  */
  float64x2_t r = x;
  r = vfmsq_laneq_f64 (r, n, invpi_pi_1, 1);
  r = vfmsq_laneq_f64 (r, n, pi_2_3, 0);
  r = vfmsq_laneq_f64 (r, n, pi_2_3, 1);

  /* sin(r) poly approx.  */
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t r3 = vmulq_f64 (r2, r);
  float64x2_t r4 = vmulq_f64 (r2, r2);

  float64x2_t c13 = vld1q_f64 (&d->c1);
  float64x2_t c56 = vld1q_f64 (&d->c5);

  float64x2_t p01 = vfmaq_laneq_f64 (d->c0, r2, c13, 0);
  float64x2_t p23 = vfmaq_laneq_f64 (d->c2, r2, c13, 1);
  float64x2_t p45 = vfmaq_laneq_f64 (d->c4, r2, c56, 0);
  float64x2_t p46 = vfmaq_laneq_f64 (p45, r4, c56, 1);

  float64x2_t p26 = vfmaq_f64 (p23, r4, p46);
  float64x2_t p06 = vfmaq_f64 (p01, r4, p26);
  float64x2_t y = vfmaq_f64 (r, r3, p06);

  if (__glibc_unlikely (v_any_u64 (cmp)))
    return special_case (x, y, odd, cmp);

  return vreinterpretq_f64_u64 (veorq_u64 (vreinterpretq_u64_f64 (y), odd));
}
