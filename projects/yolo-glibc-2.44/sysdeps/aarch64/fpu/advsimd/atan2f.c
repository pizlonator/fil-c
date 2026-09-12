/* Single-precision AdvSIMD atan2

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
  float32x4_t c0, c4, c6, c2;
  float c1, c3, c5, c7;
  uint32x4_t comp_const;
  float32x4_t pi;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2**-128, 1.0].
     Generated using fpminimax between FLT_MIN and 1.  */
  .c0 = V4 (-0x1.5554dcp-2), .c1 = 0x1.9978ecp-3,
  .c2 = V4 (-0x1.230a94p-3), .c3 = 0x1.b4debp-4,
  .c4 = V4 (-0x1.3550dap-4), .c5 = 0x1.61eebp-5,
  .c6 = V4 (-0x1.0c17d4p-6), .c7 = 0x1.7ea694p-9,
  .pi = V4 (0x1.921fb6p+1f), .comp_const = V4 (2 * 0x7f800000lu - 1),
};

#define SignMask v_u32 (0x80000000)

/* Special cases i.e. 0, infinity and nan (fall back to scalar calls).  */
static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t y, float32x4_t x, float32x4_t ret,
	      uint32x4_t sign_xy, uint32x4_t cmp)
{
  /* Account for the sign of y.  */
  ret = vreinterpretq_f32_u32 (
      veorq_u32 (vreinterpretq_u32_f32 (ret), sign_xy));
  return v_call2_f32 (atan2f, y, x, ret, cmp);
}

/* Returns 1 if input is the bit representation of 0, infinity or nan.  */
static inline uint32x4_t
zeroinfnan (uint32x4_t i, const struct data *d)
{
  /* 2 * i - 1 >= 2 * 0x7f800000lu - 1.  */
  return vcgeq_u32 (vsubq_u32 (vshlq_n_u32 (i, 1), v_u32 (1)), d->comp_const);
}

/* Fast implementation of vector atan2f. Maximum observed error is
   2.13 ULP in [0x1.9300d6p+6 0x1.93c0c6p+6] x [0x1.8c2dbp+6 0x1.8cea6p+6]:
   _ZGVnN4vv_atan2f (0x1.14a9d4p-87, 0x1.0eb886p-87) got 0x1.97aea2p-1
						    want 0x1.97ae9ep-1.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F2 (atan2) (float32x4_t y, float32x4_t x)
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

  /* Set up z for evaluation of atanf.  */
  float32x4_t num = vbslq_f32 (pred_aygtax, vnegq_f32 (ax), ay);
  float32x4_t den = vbslq_f32 (pred_aygtax, ay, ax);
  float32x4_t z = vdivq_f32 (num, den);

  /* Work out the correct shift for atan2:
     Multiplication by pi is done later.
     -pi   when x < 0  and ax < ay
     -pi/2 when x < 0  and ax > ay
      0    when x >= 0 and ax < ay
      pi/2 when x >= 0 and ax > ay.  */
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

  float32x4_t c1357 = vld1q_f32 (&d->c1);

  float32x4_t p01 = vfmaq_laneq_f32 (d->c0, z2, c1357, 0);
  float32x4_t p23 = vfmaq_laneq_f32 (d->c2, z2, c1357, 1);
  float32x4_t p45 = vfmaq_laneq_f32 (d->c4, z2, c1357, 2);
  float32x4_t p67 = vfmaq_laneq_f32 (d->c6, z2, c1357, 3);
  float32x4_t p03 = vfmaq_f32 (p01, z4, p23);
  float32x4_t p47 = vfmaq_f32 (p45, z4, p67);

  float32x4_t poly = vfmaq_f32 (p03, z8, p47);

  /* y = shift + z * P(z^2).  */
  float32x4_t ret = vfmaq_f32 (z, shift, d->pi);
  ret = vfmaq_f32 (ret, z3, poly);

  if (__glibc_unlikely (v_any_u32 (special_cases)))
    {
      return special_case (y, x, ret, sign_xy, special_cases);
    }

  /* Account for the sign of y.  */
  return vreinterpretq_f32_u32 (
      veorq_u32 (vreinterpretq_u32_f32 (ret), sign_xy));
}
libmvec_hidden_def (V_NAME_F2 (atan2))
HALF_WIDTH_ALIAS_F2(atan2)
