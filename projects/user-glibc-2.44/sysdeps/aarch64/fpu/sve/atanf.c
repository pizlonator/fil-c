/* Single-precision SVE inverse tan

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

#include "sv_math.h"

static const struct data
{
  float32_t c1, c3, c5, c7;
  float32_t c0, c2, c4, c6;
  float32_t shift_val, neg_one;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
    [2**-128, 1.0].  */
  .c0 = -0x1.5554dcp-2,
  .c1 = 0x1.9978ecp-3,
  .c2 = -0x1.230a94p-3,
  .c3 = 0x1.b4debp-4,
  .c4 = -0x1.3550dap-4,
  .c5 = 0x1.61eebp-5,
  .c6 = -0x1.0c17d4p-6,
  .c7 = 0x1.7ea694p-9,
  /*  pi/2, used as a shift value after reduction.  */
  .shift_val = 0x1.921fb54442d18p+0,
  .neg_one = -1.0f,
};

#define SignMask (0x80000000)

/* Fast implementation of SVE atanf based on
   atan(x) ~ shift + z + z^3 * P(z^2) with reduction to [0,1] using
   z=-1/x and shift = pi/2.
   Largest observed error is 2.12 ULP:
   _ZGVsMxv_atanf (0x1.03d4cep+0) got 0x1.95ed3ap-1
				 want 0x1.95ed36p-1.  */
svfloat32_t SV_NAME_F1 (atan) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t ptrue = svptrue_b32 ();

  /* No need to trigger special case. Small cases, infs and nans
     are supported by our approximation technique.  */
  svuint32_t ix = svreinterpret_u32 (x);
  svuint32_t sign = svand_x (ptrue, ix, SignMask);

  /* Argument reduction:
     y := arctan(x) for x < 1
     y := arctan(-1/x) + pi/2 for x > +1
     y := arctan(-1/x) - pi/2 for x < -1
     Hence, use z=-1/a if |x|>=|-1|, otherwise z=a.  */
  svbool_t red = svacgt (pg, x, d->neg_one);
  svfloat32_t z = svsel (red, svdiv_x (pg, sv_f32 (d->neg_one), x), x);

  /* Reinserts the sign bit of the argument to handle the case of x < -1.  */
  svfloat32_t shift = svreinterpret_f32 (
      sveor_x (red, svreinterpret_u32 (sv_f32 (d->shift_val)), sign));

  svfloat32_t z2 = svmul_x (ptrue, z, z);
  svfloat32_t z3 = svmul_x (ptrue, z2, z);
  svfloat32_t z4 = svmul_x (ptrue, z2, z2);
  svfloat32_t z8 = svmul_x (ptrue, z4, z4);

  svfloat32_t odd_coeffs = svld1rq (ptrue, &d->c1);

  svfloat32_t p01 = svmla_lane (sv_f32 (d->c0), z2, odd_coeffs, 0);
  svfloat32_t p23 = svmla_lane (sv_f32 (d->c2), z2, odd_coeffs, 1);
  svfloat32_t p45 = svmla_lane (sv_f32 (d->c4), z2, odd_coeffs, 2);
  svfloat32_t p67 = svmla_lane (sv_f32 (d->c6), z2, odd_coeffs, 3);

  svfloat32_t p03 = svmla_x (pg, p01, z4, p23);
  svfloat32_t p47 = svmla_x (pg, p45, z4, p67);

  svfloat32_t y = svmla_x (pg, p03, z8, p47);

  /* shift + z + z^3 * P(z^2).  */
  shift = svadd_m (red, z, shift);
  return svmla_x (pg, shift, z3, y);
}
