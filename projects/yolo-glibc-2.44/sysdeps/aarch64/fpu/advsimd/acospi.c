/* Double-Precision vector (Advanced SIMD) inverse cospi function

   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
  float64x2_t c0, c2, c4, c6, c8, c10;
  uint64x2_t abs_mask;
  float64x2_t one, inv_pi;
  double c1, c3, c5, c7, c9, c11;
} data = {
  /* Coefficients of polynomial P such that asin(x)/pi~ x/pi + x^3 * poly(x^2)
     on [ 0x1p-126 0x1p-2 ]. rel error: 0x1.ef9f94b1p-33. Generated using
     iterative approach for minimisation of relative error in asinpif Sollya
     file.  */
  .c0 = V2 (0x1.b2995e7b7b5fbp-5),     .c1 = 0x1.8723a1d58d83p-6,
  .c2 = V2 (0x1.d1a452eacf2fep-7),     .c3 = 0x1.3ce52c4d75582p-7,
  .c4 = V2 (0x1.d2b2a0aea27d5p-8),     .c5 = 0x1.6a0b9b92cad8bp-8,
  .c6 = V2 (0x1.2290c84438caep-8),     .c7 = 0x1.efba896580d02p-9,
  .c8 = V2 (0x1.44446707af38p-9),      .c9 = 0x1.5070b3e7aa03ep-8,
  .c10 = V2 (-0x1.c70015d0ebdafp-9),   .c11 = 0x1.27029c383fed9p-7,
  .abs_mask = V2 (0x7fffffffffffffff), .one = V2 (1.0),
  .inv_pi = V2 (0x1.45f306dc9c883p-2),
};

/* Double-precision implementation of vector acospi(x).

   For |x| in [0, 0.5], use order-11 polynomial P to approximate asinpi
   such that the final approximation of acospi is an odd polynomial:

     acospi(x) ~ 1/2 - (x/pi + x^3 P(x^2)).

   The largest observed error in this region is 1.35 ulp:
   _ZGVnN2v_acospi (0x1.fb16ed35a6d64p-2) got 0x1.5722a3dbcafb4p-2
					 want 0x1.5722a3dbcafb5p-2.

   For |x| in [0.5, 1.0], use same approximation with a change of variable

      acospi(x) = y/pi + y * z * P(z), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 2.55 ulp:
   _ZGVnN2v_acospi (0x1.d90d50357410cp-1) got 0x1.ffd43d5dd3a9ep-4
					 want 0x1.ffd43d5dd3a9bp-4.  */
float64x2_t VPCS_ATTR NOINLINE V_NAME_D1 (acospi) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  uint64x2_t ia = vandq_u64 (ix, d->abs_mask);

  float64x2_t ax = vreinterpretq_f64_u64 (ia);
  uint64x2_t a_le_half = vcaltq_f64 (x, v_f64 (0.5));

  /* Evaluate polynomial Q(x) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  float64x2_t z2 = vbslq_f64 (a_le_half, vmulq_f64 (x, x),
			      vfmsq_n_f64 (v_f64 (0.5), ax, 0.5));
  float64x2_t z = vbslq_f64 (a_le_half, ax, vsqrtq_f64 (z2));

  /* Use a single polynomial approximation P for both intervals.  */
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

  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = vfmaq_f64 (d->inv_pi, z2, p);
  p = vmulq_f64 (p, z);

  /* acospi(|x|)
		= 1/2 - sign(x) * Q(|x|), for       |x| < 0.5
		= 2 Q(|x|)              , for  0.5 < x < 1.0
		= 1 - 2 Q(|x|)          , for -1.0 < x < -0.5.  */
  float64x2_t y = vbslq_f64 (d->abs_mask, p, x);
  uint64x2_t is_neg = vcltzq_f64 (x);
  float64x2_t off = vreinterpretq_f64_u64 (
      vandq_u64 (is_neg, vreinterpretq_u64_f64 (d->one)));
  float64x2_t mul = vbslq_f64 (a_le_half, d->one, v_f64 (-2.0));
  float64x2_t add = vbslq_f64 (a_le_half, v_f64 (0.5), off);

  return vfmsq_f64 (add, mul, y);
}
