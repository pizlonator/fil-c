/* Single-Precision vector (Advanced SIMD) inverse tan2pi function

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
  float32x4_t c1, c3, c5, c7;
  float c2, c4, c6, c8;
  float32x4_t c0;
  uint32x4_t comp_const;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2^-128, 1.0].
     Generated using fpminimax between FLT_MIN and 1.  */
  .c0 = V4 (0x1.45f306p-2), .c1 = V4 (-0x1.b2975ep-4),
  .c2 = 0x1.0490e4p-4,	    .c3 = V4 (-0x1.70c272p-5),
  .c4 = 0x1.0eef52p-5,	    .c5 = V4 (-0x1.6abbbap-6),
  .c6 = 0x1.78157p-7,	    .c7 = V4 (-0x1.f0b406p-9),
  .c8 = 0x1.2ae7fep-11,	    .comp_const = V4 (2 * 0x7f800000lu - 1),
};

#define SignMask v_u32 (0x80000000)
#define OneOverPi v_f32 (0x1.45f307p-2)

/* Special cases i.e. 0, infinity and nan (fall back to scalar calls).  */
static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t y, float32x4_t x, float32x4_t ret,
	      uint32x4_t sign_xy, uint32x4_t cmp)
{
  /* Account for the sign of y.  */
  ret = vreinterpretq_f32_u32 (
      veorq_u32 (vreinterpretq_u32_f32 (ret), sign_xy));

  /* Since we have no scalar fallback for atan2pif,
     we can instead make a call to atan2f and divide by pi.  */
  ret = v_call2_f32 (atan2f, y, x, ret, cmp);

  /* Only divide the special cases by pi, and leave the rest unchanged.  */
  return vbslq_f32 (cmp, vmulq_f32 (ret, OneOverPi), ret);
}

/* Returns 1 if input is the bit representation of 0, infinity or nan.  */
static inline uint32x4_t
zeroinfnan (uint32x4_t i, const struct data *d)
{
  /* 2 * i - 1 >= 2 * 0x7f800000lu - 1.  */
  return vcgeq_u32 (vsubq_u32 (vshlq_n_u32 (i, 1), v_u32 (1)), d->comp_const);
}

/* Fast implementation of vector atan2f. Maximum observed error is 2.89 ULP:
   _ZGVnN4vv_atan2pif (0x1.bd397p+54, 0x1.e79a4ap+54) got 0x1.e2678ep-3
						     want 0x1.e26794p-3.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F2 (atan2pi) (float32x4_t y,
						    float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t iy = vreinterpretq_u32_f32 (y);

  uint32x4_t special_cases
      = vorrq_u32 (zeroinfnan (ix, d), zeroinfnan (iy, d));

  uint32x4_t sign_x = vandq_u32 (ix, SignMask);
  uint32x4_t sign_y = vandq_u32 (iy, SignMask);
  uint32x4_t sign_xy = veorq_u32 (sign_x, sign_y);

  float32x4_t ax = vabsq_f32 (x);
  float32x4_t ay = vabsq_f32 (y);

  uint32x4_t pred_xlt0 = vcltzq_f32 (x);
  uint32x4_t pred_aygtax = vcgtq_f32 (ay, ax);

  /* Set up z for evaluation of atanpif.  */
  float32x4_t num = vbslq_f32 (pred_aygtax, vnegq_f32 (ax), ay);
  float32x4_t den = vbslq_f32 (pred_aygtax, ay, ax);
  float32x4_t z = vdivq_f32 (num, den);

  /* Work out the correct shift for atan2pi:
     -1.0 when x < 0  and ax < ay
     -0.5 when x < 0  and ax > ay
      0   when x >= 0 and ax < ay
      0.5 when x >= 0 and ax > ay.  */
  float32x4_t shift = vreinterpretq_f32_u32 (
      vandq_u32 (pred_xlt0, vreinterpretq_u32_f32 (v_f32 (-1.0f))));
  float32x4_t shift2 = vreinterpretq_f32_u32 (
      vandq_u32 (pred_aygtax, vreinterpretq_u32_f32 (v_f32 (0.5f))));
  shift = vaddq_f32 (shift, shift2);

  /* Calculate the polynomial approximation.  */
  float32x4_t z2 = vmulq_f32 (z, z);
  float32x4_t z3 = vmulq_f32 (z2, z);
  float32x4_t z4 = vmulq_f32 (z2, z2);
  float32x4_t z8 = vmulq_f32 (z4, z4);

  float32x4_t c2468 = vld1q_f32 (&d->c2);

  float32x4_t p12 = vfmaq_laneq_f32 (d->c1, z2, c2468, 0);
  float32x4_t p34 = vfmaq_laneq_f32 (d->c3, z2, c2468, 1);
  float32x4_t p56 = vfmaq_laneq_f32 (d->c5, z2, c2468, 2);
  float32x4_t p78 = vfmaq_laneq_f32 (d->c7, z2, c2468, 3);
  float32x4_t p14 = vfmaq_f32 (p12, z4, p34);
  float32x4_t p58 = vfmaq_f32 (p56, z4, p78);

  float32x4_t poly = vfmaq_f32 (p14, z8, p58);

  /* y = shift + z * P(z^2).  */
  float32x4_t ret = vfmaq_f32 (shift, z, d->c0);
  ret = vfmaq_f32 (ret, z3, poly);

  if (__glibc_unlikely (v_any_u32 (special_cases)))
    {
      return special_case (y, x, ret, sign_xy, special_cases);
    }

  /* Account for the sign of y.  */
  return vreinterpretq_f32_u32 (
      veorq_u32 (vreinterpretq_u32_f32 (ret), sign_xy));
}
libmvec_hidden_def (V_NAME_F2 (atan2pi))
HALF_WIDTH_ALIAS_F2 (atan2pi)
