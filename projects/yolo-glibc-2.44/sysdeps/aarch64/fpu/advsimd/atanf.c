/* Single-precision AdvSIMD inverse tan

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
#include "poly_advsimd_f32.h"

static const struct data
{
  uint32x4_t sign_mask, pi_over_2;
  float32x4_t neg_one;
  float32x4_t c0, c2, c4, c6;
  float c1, c3, c5, c7;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2**-128, 1.0].
     Generated using fpminimax between FLT_MIN and 1.  */
  .c0 = V4 (-0x1.5554dcp-2),	.c1 = 0x1.9978ecp-3,
  .c2 = V4 (-0x1.230a94p-3),	.c3 = 0x1.b4debp-4,
  .c4 = V4 (-0x1.3550dap-4),	.c5 = 0x1.61eebp-5,
  .c6 = V4 (-0x1.0c17d4p-6),	.c7 = 0x1.7ea694p-9,
  .pi_over_2 = V4 (0x3fc90fdb), .neg_one = V4 (-1.0f),
  .sign_mask = V4 (0x80000000),
};

/* Fast implementation of vector atanf based on
   atan(x) ~ shift + z + z^3 * P(z^2) with reduction to [0,1]
   using z=-1/x and shift = pi/2. Maximum observed error is 2.02 ulps:
   _ZGVnN4v_atanf (0x1.03d4cep+0) got 0x1.95ed3ap-1
				 want 0x1.95ed36p-1.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (atan) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t sign = vandq_u32 (ix, d->sign_mask);

  /* Argument reduction:
     y := arctan(x) for |x| < 1
     y := arctan(-1/x) + pi/2 for x > +1
     y := arctan(-1/x) - pi/2 for x < -1
     Hence, use z=-1/a if x>=|-1|, otherwise z=a.  */
  uint32x4_t red = vcagtq_f32 (x, d->neg_one);

  float32x4_t z = vbslq_f32 (red, vdivq_f32 (d->neg_one, x), x);

  /* Shift is calculated as +-pi/2 or 0, depending on the argument case.  */
  float32x4_t shift = vreinterpretq_f32_u32 (
      vandq_u32 (red, veorq_u32 (d->pi_over_2, sign)));

  float32x4_t z2 = vmulq_f32 (z, z);
  float32x4_t z3 = vmulq_f32 (z, z2);
  float32x4_t z4 = vmulq_f32 (z2, z2);
  float32x4_t z8 = vmulq_f32 (z4, z4);

  /* Uses an Estrin scheme for polynomial approximation.  */
  float32x4_t odd_coeffs = vld1q_f32 (&d->c1);

  float32x4_t p01 = vfmaq_laneq_f32 (d->c0, z2, odd_coeffs, 0);
  float32x4_t p23 = vfmaq_laneq_f32 (d->c2, z2, odd_coeffs, 1);
  float32x4_t p45 = vfmaq_laneq_f32 (d->c4, z2, odd_coeffs, 2);
  float32x4_t p67 = vfmaq_laneq_f32 (d->c6, z2, odd_coeffs, 3);

  float32x4_t p03 = vfmaq_f32 (p01, z4, p23);
  float32x4_t p47 = vfmaq_f32 (p45, z4, p67);

  float32x4_t y = vfmaq_f32 (p03, z8, p47);

  /* y = shift + z * P(z^2).  */
  return vfmaq_f32 (vaddq_f32 (shift, z), z3, y);
}
libmvec_hidden_def (V_NAME_F1 (atan))
HALF_WIDTH_ALIAS_F1 (atan)
