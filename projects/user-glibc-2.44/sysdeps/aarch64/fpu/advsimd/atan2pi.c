/* Double-Precision vector (Advanced SIMD) inverse tan2pi function

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
  float64_t c2, c4, c6, c8, c10, c12, c14, c16, c18, c20;
  float64x2_t c0;
  uint64x2_t zeroinfnan;
  float64x2_t c1, c3, c5, c7, c9, c11, c13, c15, c17, c19;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
	      [2**-1022, 1.0].  */
  .c0 = V2 (0x1.45f306dc9c883p-2),
  .c1 = V2 (-0x1.b2995e7b7ba4ap-4),
  .c2 = 0x1.04c26be3d2c1p-4,
  .c3 = V2 (-0x1.7483759c17ea1p-5),
  .c4 = 0x1.21bb95c315d57p-5,
  .c5 = V2 (-0x1.da1bdc3d453f3p-6),
  .c6 = 0x1.912d20459b4bfp-6,
  .c7 = V2 (-0x1.5bbd4545cad1fp-6),
  .c8 = 0x1.331b83bec30a1p-6,
  .c9 = V2 (-0x1.13d6457f44de3p-6),
  .c10 = 0x1.f8e802974db94p-7,
  .c11 = V2 (-0x1.d7e173ab04a1ap-7),
  .c12 = 0x1.bdfa47d6a4f28p-7,
  .c13 = V2 (-0x1.9ba78f3232ceep-7),
  .c14 = 0x1.5e6044590ab4fp-7,
  .c15 = V2 (-0x1.01ccfdeb9f77fp-7),
  .c16 = 0x1.345cf0d4eb1c1p-8,
  .c17 = V2 (-0x1.19e5f00f67e3ap-9),
  .c18 = 0x1.6d3035ac7625bp-11,
  .c19 = V2 (-0x1.286bb9ae4ed79p-13),
  .c20 = 0x1.c37ec36da0e1ap-17,
  .zeroinfnan = V2 (2 * 0x7ff0000000000000ul - 1),
};

#define SignMask v_u64 (0x8000000000000000)
#define OneOverPi v_f64 (0x1.45f306dc9c883p-2)

/* Special cases i.e. 0, infinity, NaN (fall back to scalar calls).  */
static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t y, float64x2_t x, float64x2_t ret,
	      uint64x2_t sign_xy, uint64x2_t cmp)
{
  /* Account for the sign of x and y.  */
  ret = vreinterpretq_f64_u64 (
      veorq_u64 (vreinterpretq_u64_f64 (ret), sign_xy));

  /* Since we have no scalar fallback for atan2pi,
     we can instead make a call to atan2f and divide by pi.  */
  ret = v_call2_f64 (atan2, y, x, ret, cmp);

  /* Only divide the special cases by pi, and leave the rest unchanged.  */
  return vbslq_f64 (cmp, vmulq_f64 (ret, OneOverPi), ret);
}

/* Returns 1 if input is the bit representation of 0, infinity or nan.  */
static inline uint64x2_t
zeroinfnan (uint64x2_t i, const struct data *d)
{
  /* (2 * i - 1) >= (2 * asuint64 (INFINITY) - 1).  */
  return vcgeq_u64 (vsubq_u64 (vaddq_u64 (i, i), v_u64 (1)), d->zeroinfnan);
}

/* Fast implementation of vector atan2pi.
   Maximum observed error is 3.04 ulps:
   _ZGVnN2vv_atan2pi (0x1.1e0733532ce28p+5, 0x1.2d803379cca1fp+5)
   got 0x1.eed60c1e89317p-3 want 0x1.eed60c1e89314p-3.  */
float64x2_t VPCS_ATTR V_NAME_D2 (atan2pi) (float64x2_t y, float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  uint64x2_t iy = vreinterpretq_u64_f64 (y);

  uint64x2_t special_cases
      = vorrq_u64 (zeroinfnan (ix, d), zeroinfnan (iy, d));

  uint64x2_t sign_x = vandq_u64 (ix, SignMask);
  uint64x2_t sign_y = vandq_u64 (iy, SignMask);
  uint64x2_t sign_xy = veorq_u64 (sign_x, sign_y);

  float64x2_t ax = vabsq_f64 (x);
  float64x2_t ay = vabsq_f64 (y);

  uint64x2_t pred_xlt0 = vcltzq_f64 (x);
  uint64x2_t pred_aygtax = vcgtq_f64 (ay, ax);

  /* Set up z for evaluation of atanpi.  */
  float64x2_t num = vbslq_f64 (pred_aygtax, vnegq_f64 (ax), ay);
  float64x2_t den = vbslq_f64 (pred_aygtax, ay, ax);
  float64x2_t z = vdivq_f64 (num, den);

  /* Work out the correct shift for atan2pi:
     -1.0 when x < 0  and ax < ay
     -0.5 when x < 0  and ax > ay
      0   when x >= 0 and ax < ay
      0.5 when x >= 0 and ax > ay.  */
  float64x2_t shift = vreinterpretq_f64_u64 (
      vandq_u64 (pred_xlt0, vreinterpretq_u64_f64 (v_f64 (-1.0))));
  float64x2_t shift2 = vreinterpretq_f64_u64 (
      vandq_u64 (pred_aygtax, vreinterpretq_u64_f64 (v_f64 (0.5))));
  shift = vaddq_f64 (shift, shift2);

  /* Calculate the polynomial approximation.  */
  float64x2_t z2 = vmulq_f64 (z, z);
  float64x2_t z3 = vmulq_f64 (z2, z);
  float64x2_t z4 = vmulq_f64 (z2, z2);
  float64x2_t z8 = vmulq_f64 (z4, z4);
  float64x2_t z16 = vmulq_f64 (z8, z8);

  float64x2_t c24 = vld1q_f64 (&d->c2);
  float64x2_t c68 = vld1q_f64 (&d->c6);

  /* Order-7 Estrin.  */
  float64x2_t p12 = vfmaq_laneq_f64 (d->c1, z2, c24, 0);
  float64x2_t p34 = vfmaq_laneq_f64 (d->c3, z2, c24, 1);
  float64x2_t p56 = vfmaq_laneq_f64 (d->c5, z2, c68, 0);
  float64x2_t p78 = vfmaq_laneq_f64 (d->c7, z2, c68, 1);

  float64x2_t p14 = vfmaq_f64 (p12, z4, p34);
  float64x2_t p58 = vfmaq_f64 (p56, z4, p78);
  float64x2_t p18 = vfmaq_f64 (p14, z8, p58);

  /* Order-11 Estrin.  */
  float64x2_t c1012 = vld1q_f64 (&d->c10);
  float64x2_t c1416 = vld1q_f64 (&d->c14);
  float64x2_t c1820 = vld1q_f64 (&d->c18);

  float64x2_t p910 = vfmaq_laneq_f64 (d->c9, z2, c1012, 0);
  float64x2_t p1112 = vfmaq_laneq_f64 (d->c11, z2, c1012, 1);
  float64x2_t p912 = vfmaq_f64 (p910, z4, p1112);

  float64x2_t p1314 = vfmaq_laneq_f64 (d->c13, z2, c1416, 0);
  float64x2_t p1516 = vfmaq_laneq_f64 (d->c15, z2, c1416, 1);
  float64x2_t p1316 = vfmaq_f64 (p1314, z4, p1516);

  float64x2_t p1718 = vfmaq_laneq_f64 (d->c17, z2, c1820, 0);
  float64x2_t p1920 = vfmaq_laneq_f64 (d->c19, z2, c1820, 1);
  float64x2_t p1720 = vfmaq_f64 (p1718, z4, p1920);

  float64x2_t p916 = vfmaq_f64 (p912, z8, p1316);
  float64x2_t p920 = vfmaq_f64 (p916, z16, p1720);

  float64x2_t poly = vfmaq_f64 (p18, z16, p920);

  /* y = shift + z * P(z^2).  */
  float64x2_t ret = vfmaq_f64 (shift, z, d->c0);
  ret = vfmaq_f64 (ret, z3, poly);

  if (__glibc_unlikely (v_any_u64 (special_cases)))
    return special_case (y, x, ret, sign_xy, special_cases);

  /* Account for the sign of x and y.  */
  return vreinterpretq_f64_u64 (
      veorq_u64 (vreinterpretq_u64_f64 (ret), sign_xy));
}
