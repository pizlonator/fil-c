/* Single-Precision vector (SVE) inverse tanpi function

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

#include "sv_math.h"

static const struct data
{
  float32_t c2, c4, c6, c8;
  float32_t c0, c1, c3, c5, c7;
  float32_t shift_val, neg_one;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
    [2**-128, 1.0].  */
  .c0 = 0x1.45f306p-2,	.c1 = -0x1.b2975ep-4, .c2 = 0x1.0490e4p-4,
  .c3 = -0x1.70c272p-5, .c4 = 0x1.0eef52p-5,  .c5 = -0x1.6abbbap-6,
  .c6 = 0x1.78157p-7,	.c7 = -0x1.f0b406p-9, .c8 = 0x1.2ae7fep-11,
  .shift_val = 1.5f,	.neg_one = -1.0f,
};

#define SignMask (0x80000000)

/* Fast implementation of SVE atanpif based on
   atan(x) ~ shift + z * P(z^2) with reduction to [0,1] using
   z=-1/x and shift = 1/2.
   Largest observed error is 2.59 ULP, close to +/-1.0:
   _ZGVsMxv_atanpif(0x1.f2a89cp-1) got 0x1.f76524p-3
				  want 0x1.f7651ep-3.  */
svfloat32_t SV_NAME_F1 (atanpi) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t ptrue = svptrue_b32 ();

  /* No need to trigger special case. Small cases, infs and nans
     are supported by our approximation technique.  */
  svuint32_t ix = svreinterpret_u32 (x);
  svuint32_t sign = svand_x (pg, ix, SignMask);

  /* Argument reduction:
     y := arctan(x) for x < 1
     y := arctan(-1/x) + 1/2 for x > +1
     y := arctan(-1/x) - 1/2 for x < -1
     Hence, use z=-1/a if |x|>=|-1|, otherwise z=a.  */
  svbool_t red = svacgt (pg, x, d->neg_one);
  svfloat32_t z = svsel (red, svdiv_x (ptrue, sv_f32 (d->neg_one), x), x);

  /* Reuse of -1.0f to reduce constant loads,
     We need a shift value of 1/2, which is created via -1 + (1 + 1/2).  */
  svfloat32_t shift
      = svadd_z (red, sv_f32 (d->neg_one), sv_f32 (d->shift_val));

  /* Reinserts the sign bit of the argument to handle the case of x < -1.  */
  shift = svreinterpret_f32 (sveor_x (pg, svreinterpret_u32 (shift), sign));

  svfloat32_t z2 = svmul_x (ptrue, z, z);
  svfloat32_t z4 = svmul_x (ptrue, z2, z2);
  svfloat32_t z8 = svmul_x (ptrue, z4, z4);

  svfloat32_t even_coeffs = svld1rq (ptrue, &d->c2);

  svfloat32_t p12 = svmla_lane (sv_f32 (d->c1), z2, even_coeffs, 0);
  svfloat32_t p34 = svmla_lane (sv_f32 (d->c3), z2, even_coeffs, 1);
  svfloat32_t p56 = svmla_lane (sv_f32 (d->c5), z2, even_coeffs, 2);
  svfloat32_t p78 = svmla_lane (sv_f32 (d->c7), z2, even_coeffs, 3);

  svfloat32_t p14 = svmad_x (pg, z4, p34, p12);
  svfloat32_t p58 = svmad_x (pg, z4, p78, p56);

  svfloat32_t p18 = svmad_x (pg, z8, p58, p14);
  svfloat32_t y = svmad_x (pg, z2, p18, d->c0);

  /* shift + z * P(z^2).  */
  return svmad_x (pg, y, z, shift);
}
