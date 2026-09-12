/* Single-Precision vector (Advanced SIMD) inverse cospi function

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
  float32x4_t c0, c2, c4, inv_pi;
  float c1, c3, c5, null;
} data = {
  /* Coefficients of polynomial P such that asin(x)/pi~ x/pi + x^3 * poly(x^2)
     on [ 0x1p-126 0x1p-2 ]. rel error: 0x1.ef9f94b1p-33. Generated using
     iterative approach for minimisation of relative error in asinpif Sollya
     file.  */
  .c0 = V4 (0x1.b2995ep-5f),	 .c1 = 0x1.8724ep-6f,
  .c2 = V4 (0x1.d1301ep-7f),	 .c3 = 0x1.446d3cp-7f,
  .c4 = V4 (0x1.654848p-8f),	 .c5 = 0x1.5fdaa8p-7f,
  .inv_pi = V4 (0x1.45f306p-2f),
};

#define AbsMask 0x7fffffff

/* Single-precision implementation of vector acospi(x).

   For |x| in [0, 0.5], use order 5 polynomial P to approximate asinpi
   such that the final approximation of acospi is an odd polynomial:

     acospi(x) ~ 1/2 - (x/pi + x^3 P(x^2)).

   The largest observed error in this region is 1.23 ulps,
      _ZGVnN4v_acospif (0x1.fee13ep-2) got 0x1.55beb4p-2 want 0x1.55beb2p-2.

   For |x| in [0.5, 1.0], use same approximation with a change of variable

      acospi(x) = y/pi + y * z * P(z), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 2.53 ulps,
   _ZGVnN4v_acospif (0x1.6ad644p-1) got 0x1.fe8f96p-3
				   want 0x1.fe8f9cp-3.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (acospi) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t ia = vandq_u32 (ix, v_u32 (AbsMask));

  float32x4_t ax = vreinterpretq_f32_u32 (ia);
  uint32x4_t a_le_half = vcaltq_f32 (x, v_f32 (0.5f));

  /* Evaluate polynomial Q(x) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */

  float32x4_t z2 = vbslq_f32 (a_le_half, vmulq_f32 (x, x),
			      vfmsq_n_f32 (v_f32 (0.5f), ax, 0.5f));
  float32x4_t z = vbslq_f32 (a_le_half, ax, vsqrtq_f32 (z2));

  /* Use a single polynomial approximation P for both intervals.  */

  /* Order-5 Estrin evaluation scheme.  */
  float32x4_t z4 = vmulq_f32 (z2, z2);
  float32x4_t z8 = vmulq_f32 (z4, z4);
  float32x4_t c135 = vld1q_f32 (&d->c1);
  float32x4_t p01 = vfmaq_laneq_f32 (d->c0, z2, c135, 0);
  float32x4_t p23 = vfmaq_laneq_f32 (d->c2, z2, c135, 1);
  float32x4_t p03 = vfmaq_f32 (p01, z4, p23);
  float32x4_t p45 = vfmaq_laneq_f32 (d->c4, z2, c135, 2);
  float32x4_t p = vfmaq_f32 (p03, z8, p45);
  /* Add 1/pi as final coeff.  */
  p = vfmaq_f32 (d->inv_pi, z2, p);

  /* Finalize polynomial: z * P(z^2).  */
  p = vmulq_f32 (z, p);

  /* acospi(|x|)
			= 1/2 - sign(x) * Q(|x|), for       |x| < 0.5
			= 2 Q(|x|)              , for  0.5 < x < 1.0
			= 1 - 2 Q(|x|)          , for -1.0 < x < -0.5.  */

  float32x4_t y = vbslq_f32 (v_u32 (AbsMask), p, x);
  uint32x4_t is_neg = vcltzq_f32 (x);
  float32x4_t off = vreinterpretq_f32_u32 (
      vandq_u32 (vreinterpretq_u32_f32 (v_f32 (1.0f)), is_neg));
  float32x4_t mul = vbslq_f32 (a_le_half, v_f32 (1.0f), v_f32 (-2.0f));
  float32x4_t add = vbslq_f32 (a_le_half, v_f32 (0.5f), off);

  return vfmsq_f32 (add, mul, y);
}
libmvec_hidden_def (V_NAME_F1 (acospi))
HALF_WIDTH_ALIAS_F1 (acospi)
