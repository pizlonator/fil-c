/* Double-precision AdvSIMD inverse cos

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

static const struct data
{
  double c1, c3, c5, c7, c9, c11;
  float64x2_t c0, c2, c4, c6, c8, c10;
  uint64x2_t abs_mask;
  float64x2_t pi, pi_over_2;
} data = {
  /* Polynomial approximation of  (asin(sqrt(x)) - sqrt(x)) / (x * sqrt(x))
     on [ 0x1p-106, 0x1p-2 ], relative error: 0x1.c3d8e169p-57.  */
  .c0 = V2 (0x1.555555555554ep-3),     .c1 = 0x1.3333333337233p-4,
  .c2 = V2 (0x1.6db6db67f6d9fp-5),     .c3 = 0x1.f1c71fbd29fbbp-6,
  .c4 = V2 (0x1.6e8b264d467d6p-6),     .c5 = 0x1.1c5997c357e9dp-6,
  .c6 = V2 (0x1.c86a22cd9389dp-7),     .c7 = 0x1.856073c22ebbep-7,
  .c8 = V2 (0x1.fd1151acb6bedp-8),     .c9 = 0x1.087182f799c1dp-6,
  .c10 = V2 (-0x1.6602748120927p-7),   .c11 = 0x1.cfa0dd1f9478p-6,
  .pi = V2 (0x1.921fb54442d18p+1),     .pi_over_2 = V2 (0x1.921fb54442d18p+0),
  .abs_mask = V2 (0x7fffffffffffffff),
};

/* Double-precision implementation of vector acos(x).

   For |x| in [0, 0.5], use an order 11 polynomial P such that the final
   approximation of asin is an odd polynomial:

     acos(x) ~ pi/2 - (x + x^3 P(x^2)).

   The largest observed error in this region is 1.18 ulp:
   _ZGVnN2v_acos (0x1.fbab0a7c460f6p-2) got 0x1.0d54d1985c068p+0
				       want 0x1.0d54d1985c069p+0.

   For |x| in [0.5, 1.0], use same approximation with a change of variable

     acos(x) = y + y * z * P(z), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 1.50 ulp:
   _ZGVnN2v_acos (0x1.252a2cf3fb9acp-1) got 0x1.ec1a46aa82901p-1
				       want 0x1.ec1a46aa829p-1.  */
float64x2_t VPCS_ATTR V_NAME_D1 (acos) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  float64x2_t ax = vabsq_f64 (x);
  uint64x2_t a_le_half = vcleq_f64 (ax, v_f64 (0.5));

  /* Evaluate polynomial Q(x) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  float64x2_t z2 = vbslq_f64 (a_le_half, vmulq_f64 (x, x),
			      vfmaq_f64 (v_f64 (0.5), v_f64 (-0.5), ax));
  float64x2_t z = vbslq_f64 (a_le_half, ax, vsqrtq_f64 (z2));

  /* Use a single polynomial approximation P for both intervals.  */
  float64x2_t z3 = vmulq_f64 (z2, z);
  float64x2_t z4 = vmulq_f64 (z2, z2);
  float64x2_t z8 = vmulq_f64 (z4, z4);

  /* Order-11 Estrin.  */
  float64x2_t c13 = vld1q_f64 (&d->c1);
  float64x2_t c57 = vld1q_f64 (&d->c5);
  float64x2_t c911 = vld1q_f64 (&d->c9);

  float64x2_t p01 = vfmaq_laneq_f64 (d->c0, z2, c13, 0);
  float64x2_t p23 = vfmaq_laneq_f64 (d->c2, z2, c13, 1);
  float64x2_t p03 = vfmaq_f64 (p01, z4, p23);

  float64x2_t p45 = vfmaq_laneq_f64 (d->c4, z2, c57, 0);
  float64x2_t p67 = vfmaq_laneq_f64 (d->c6, z2, c57, 1);
  float64x2_t p47 = vfmaq_f64 (p45, z4, p67);

  float64x2_t p89 = vfmaq_laneq_f64 (d->c8, z2, c911, 0);
  float64x2_t p1011 = vfmaq_laneq_f64 (d->c10, z2, c911, 1);
  float64x2_t p811 = vfmaq_f64 (p89, z4, p1011);

  float64x2_t p411 = vfmaq_f64 (p47, z8, p811);
  float64x2_t p = vfmaq_f64 (p03, z8, p411);

  /* Finalize polynomial: z + z3 * P(z2).  */
  p = vfmaq_f64 (z, z3, p);

  /* acos(|x|) = pi/2 - sign(x) * Q(|x|), for  |x| < 0.5
	       = 2 Q(|x|)               , for  0.5 < x < 1.0
	       = pi - 2 Q(|x|)          , for -1.0 < x < -0.5.  */
  float64x2_t y = vbslq_f64 (d->abs_mask, p, x);
  uint64x2_t is_neg = vcltzq_f64 (x);
  float64x2_t off = vreinterpretq_f64_u64 (
      vandq_u64 (is_neg, vreinterpretq_u64_f64 (d->pi)));
  float64x2_t mul = vbslq_f64 (a_le_half, v_f64 (-1.0), v_f64 (2.0));
  float64x2_t add = vbslq_f64 (a_le_half, d->pi_over_2, off);

  return vfmaq_f64 (add, mul, y);
}
