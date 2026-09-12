/* Single-precision AdvSIMD inverse sin

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
  float32x4_t c0, c2, c4;
  float c1, c3;
  float32x4_t pi_over_2f;
} data = {
  /* Polynomial approximation of  (asin(sqrt(x)) - sqrt(x)) / (x * sqrt(x))  on
     [ 0x1p-24 0x1p-2 ] order = 4 rel error: 0x1.00a23bbp-29 .  */
  .c0 = V4 (0x1.55555ep-3f), .c1 = 0x1.33261ap-4f,
  .c2 = V4 (0x1.70d7dcp-5f), .c3 = 0x1.b059dp-6f,
  .c4 = V4 (0x1.3af7d8p-5f), .pi_over_2f = V4 (0x1.921fb6p+0f),
};

#define AbsMask 0x7fffffff

/* Single-precision implementation of vector asin(x).

   For |x| in [0, 0.5), use order 4 polynomial P such that the final
   approximation is an odd polynomial: asin(x) ~ x + x^3 P(x^2).

    The largest observed error in this region is 0.83 ulps,
      _ZGVnN4v_asinf (0x1.ea00f4p-2) got 0x1.fef15ep-2 want 0x1.fef15cp-2.

    For |x| in [0.5, 1.0], use same approximation with a change of variable

    asin(x) = pi/2 - (y + y * z * P(z)), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 2.41 ulps,
     _ZGVnN4v_asinf (0x1.00203ep-1) got 0x1.0c3a64p-1 want 0x1.0c3a6p-1.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (asin) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t ia = vandq_u32 (ix, v_u32 (AbsMask));
  float32x4_t ax = vreinterpretq_f32_u32 (ia);
  uint32x4_t a_lt_half = vcaltq_f32 (x, v_f32 (0.5f));

  /* Evaluate polynomial Q(x) = y + y * z * P(z) with
     z = x ^ 2 and y = |x|            , if |x| < 0.5
     z = (1 - |x|) / 2 and y = sqrt(z), if |x| >= 0.5.  */
  float32x4_t z2 = vbslq_f32 (a_lt_half, vmulq_f32 (x, x),
			      vfmsq_n_f32 (v_f32 (0.5f), ax, 0.5f));
  float32x4_t z = vbslq_f32 (a_lt_half, ax, vsqrtq_f32 (z2));

  /* Use a single polynomial approximation P for both intervals.  */

  /* PW Horner 3 evaluation scheme.  */
  float32x4_t z4 = vmulq_f32 (z2, z2);
  float32x4_t c13 = vld1q_f32 (&d->c1);
  float32x4_t p01 = vfmaq_laneq_f32 (d->c0, z2, c13, 0);
  float32x4_t p23 = vfmaq_laneq_f32 (d->c2, z2, c13, 1);
  float32x4_t p = vfmaq_f32 (p23, d->c4, z4);
  p = vfmaq_f32 (p01, p, z4);
  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = vfmaq_f32 (z, vmulq_f32 (z, z2), p);

  /* asin(|x|) = Q(|x|)         , for |x| < 0.5
	       = pi/2 - 2 Q(|x|), for |x| >= 0.5.  */
  float32x4_t y
      = vbslq_f32 (a_lt_half, p, vfmsq_n_f32 (d->pi_over_2f, p, 2.0f));

  /* Copy sign.  */
  return vbslq_f32 (v_u32 (AbsMask), y, x);
}
libmvec_hidden_def (V_NAME_F1 (asin))
HALF_WIDTH_ALIAS_F1 (asin)
