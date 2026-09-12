/* Single-Precision vector (Advanced SIMD) inverse tanpi function

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
  uint32x4_t half;
  float32x4_t neg_one;
  float32x4_t c0, c1, c3, c5, c7;
  float c2, c4, c6, c8;
} data = {
  /* Polynomial coefficients generated using Remez algorithm,
     see atanpi.sollya for details.  */
  .c0 = V4 (0x1.45f306p-2), .c1 = V4 (-0x1.b2975ep-4),
  .c2 = 0x1.0490e4p-4,	    .c3 = V4 (-0x1.70c272p-5),
  .c4 = 0x1.0eef52p-5,	    .c5 = V4 (-0x1.6abbbap-6),
  .c6 = 0x1.78157p-7,	    .c7 = V4 (-0x1.f0b406p-9),
  .c8 = 0x1.2ae7fep-11,	    .half = V4 (0x3f000000),
  .neg_one = V4 (-1.0f),
};

#define SignMask v_u32 (0x80000000)

/* Fast implementation of vector atanpif based on
   atanpi(x) ~ shift + z * P(z^2) with reduction to [0,1]
   using z=-1/x and shift = +-1/2.
   Maximum observed error is 2.59ulps:
   _ZGVnN4v_atanpif (0x1.f2a89cp-1) got 0x1.f76524p-3
				   want 0x1.f7651ep-3.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (atanpi) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t sign = vandq_u32 (ix, SignMask);

  /* Argument Reduction:
     y := arctanpi(x) for |x| < 1
     y := arctanpi(-1/x) + 1/2 for x > 1
     y := arctanpi(-1/x) - 1/2 for x < -1
     Hence, use z=-1/a if |x|>=|-1|, otherwise z=a.  */
  uint32x4_t red = vcagtq_f32 (x, d->neg_one);

  float32x4_t z = vbslq_f32 (red, vdivq_f32 (d->neg_one, x), x);

  /* Shift is calculated as +1/2 or 0, depending on the argument case.  */
  float32x4_t shift = vreinterpretq_f32_u32 (vandq_u32 (red, d->half));

  /* Reinsert sign bit from argument into the shift value.  */
  shift = vreinterpretq_f32_u32 (
      veorq_u32 (vreinterpretq_u32_f32 (shift), sign));

  /* Uses an Estrin scheme for polynomial approximation.  */
  float32x4_t z2 = vmulq_f32 (z, z);
  float32x4_t z4 = vmulq_f32 (z2, z2);
  float32x4_t z8 = vmulq_f32 (z4, z4);

  float32x4_t even_coeffs = vld1q_f32 (&d->c2);

  float32x4_t p12 = vfmaq_laneq_f32 (d->c1, z2, even_coeffs, 0);
  float32x4_t p34 = vfmaq_laneq_f32 (d->c3, z2, even_coeffs, 1);
  float32x4_t p56 = vfmaq_laneq_f32 (d->c5, z2, even_coeffs, 2);
  float32x4_t p78 = vfmaq_laneq_f32 (d->c7, z2, even_coeffs, 3);

  float32x4_t p14 = vfmaq_f32 (p12, z4, p34);
  float32x4_t p58 = vfmaq_f32 (p56, z4, p78);

  float32x4_t y = vfmaq_f32 (p14, z8, p58);
  y = vfmaq_f32 (d->c0, z2, y);

  /* y = shift + z * P(z^2).  */
  return vfmaq_f32 (shift, z, y);
}
libmvec_hidden_def (V_NAME_F1 (atanpi))
HALF_WIDTH_ALIAS_F1 (atanpi)
