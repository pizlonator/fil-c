/* Single-Precision vector (SVE) inverse sinpi function

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
  float32_t c1, c3, c5;
  float32_t c0, c2, c4, inv_pi;
} data = {
  /* Polynomial approximation of  (asin(sqrt(x)) - sqrt(x)) / (x * sqrt(x))  on
    [ 0x1p-24 0x1p-2 ] order = 4 rel error: 0x1.00a23bbp-29 .  */
  .c0 = 0x1.b2995ep-5f,	    .c1 = 0x1.8724ep-6f,  .c2 = 0x1.d1301ep-7f,
  .c3 = 0x1.446d3cp-7f,	    .c4 = 0x1.654848p-8f, .c5 = 0x1.5fdaa8p-7f,
  .inv_pi = 0x1.45f306p-2f,
};

/* Single-precision SVE implementation of vector asin(x).

   For |x| in [0, 0.5], use order 5 polynomial P such that the final
   approximation is an odd polynomial: asinpi(x) ~ x/pi + x^3 P(x^2).

    The largest observed error in this region is 1.96 ulps:
    _ZGVsMxv_asinpif (0x1.8e534ep-3) got 0x1.fe6ab4p-5
				    want 0x1.fe6ab8p-5.

    For |x| in [0.5, 1.0], use same approximation with a change of variable

    asinpi(x) = 1/2 - (y + y * z * P(z)), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 3.46 ulps:
   _ZGVsMxv_asinpif (0x1.0df892p-1) got 0x1.6a114cp-3
				   want 0x1.6a1146p-3.  */
svfloat32_t SV_NAME_F1 (asinpi) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t ptrue = svptrue_b32 ();

  svuint32_t sign = svand_x (pg, svreinterpret_u32 (x), 0x80000000);

  svfloat32_t ax = svabs_x (pg, x);
  svbool_t a_ge_half = svacge (pg, x, 0.5);

  /* Evaluate polynomial Q(x) = y + y * z * P(z) with
   z = x ^ 2 and y = |x|            , if |x| < 0.5
   z = (1 - |x|) / 2 and y = sqrt(z), if |x| >= 0.5.  */
  svfloat32_t z2 = svsel (a_ge_half, svmls_x (pg, sv_f32 (0.5), ax, 0.5),
			  svmul_x (pg, x, x));
  svfloat32_t z = svsqrt_m (ax, a_ge_half, z2);

  svfloat32_t z4 = svmul_x (ptrue, z2, z2);
  svfloat32_t c135_two = svld1rq (ptrue, &d->c1);

  /* Order-5 Pairwise Horner evaluation scheme.  */
  svfloat32_t p01 = svmla_lane (sv_f32 (d->c0), z2, c135_two, 0);
  svfloat32_t p23 = svmla_lane (sv_f32 (d->c2), z2, c135_two, 1);
  svfloat32_t p45 = svmla_lane (sv_f32 (d->c4), z2, c135_two, 2);

  svfloat32_t p25 = svmla_x (pg, p23, z4, p45);
  svfloat32_t p = svmla_x (pg, p01, z4, p25);

  /* Add 1/pi as final coeff.  */
  p = svmla_x (pg, sv_f32 (d->inv_pi), z2, p);
  p = svmul_x (pg, p, z);

  /*  asinpi(|x|) = Q(|x|), for |x| < 0.5
	       =  1/2 - 2 Q(|x|), for |x| >= 0.5.  */
  svfloat32_t y = svmsb_m (a_ge_half, p, sv_f32 (2.0), 0.5);

  /* Reinsert sign from argument.  */
  return svreinterpret_f32 (sveor_x (pg, svreinterpret_u32 (y), sign));
}
