/* Double-Precision vector (SVE) inverse tanpi function

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
  float64_t c2, c4, c6, c8, c10, c12, c14, c16, c18, c20;
  float64_t c0, c1, c3, c5, c7, c9, c11, c13, c15, c17, c19;
  float64_t shift_val, neg_one;
} data = {
  /* Coefficients of polnomial P such that atan(x)~x+x*P(x^2) on
     [2^-1022, 1.0].  */
  .c0 = 0x1.45f306dc9c883p-2,
  .c1 = -0x1.b2995e7b7ba4ap-4,
  .c2 = 0x1.04c26be3d2c1p-4,
  .c3 = -0x1.7483759c17ea1p-5,
  .c4 = 0x1.21bb95c315d57p-5,
  .c5 = -0x1.da1bdc3d453f3p-6,
  .c6 = 0x1.912d20459b4bfp-6,
  .c7 = -0x1.5bbd4545cad1fp-6,
  .c8 = 0x1.331b83bec30a1p-6,
  .c9 = -0x1.13d6457f44de3p-6,
  .c10 = 0x1.f8e802974db94p-7,
  .c11 = -0x1.d7e173ab04a1ap-7,
  .c12 = 0x1.bdfa47d6a4f28p-7,
  .c13 = -0x1.9ba78f3232ceep-7,
  .c14 = 0x1.5e6044590ab4fp-7,
  .c15 = -0x1.01ccfdeb9f77fp-7,
  .c16 = 0x1.345cf0d4eb1c1p-8,
  .c17 = -0x1.19e5f00f67e3ap-9,
  .c18 = 0x1.6d3035ac7625bp-11,
  .c19 = -0x1.286bb9ae4ed79p-13,
  .c20 = 0x1.c37ec36da0e1ap-17,
  .shift_val = 1.5,
  .neg_one = -1,
};

/* Fast implementation of SVE atan.
   Based on atan(x) ~ shift + z + z^3 * P(z^2) with reduction to 0,1 using
   z=1/x and shift = pi/2. Largest errors are close to 1. The maximum observed
   error is 2.80 ulps:
   _ZGVsMxv_atanpi(0x1.f19587d63c76fp-1) got 0x1.f6b1304817d02p-3
					want 0x1.f6b1304817d05p-3.  */
svfloat64_t SV_NAME_D1 (atanpi) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svbool_t ptrue = svptrue_b64 ();
  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t sign = svand_x (pg, ix, 0x8000000000000000);

  /* Argument reduction:
     y := arctan(x) for x < 1
     y := pi/2 + arctan(-1/x) for x > 1
     Hence, use z=-1/a if x>=1, otherwise z=a.  */
  svbool_t red = svacgt (pg, x, d->neg_one);
  svfloat64_t z = svsel (red, svdiv_x (pg, sv_f64 (d->neg_one), x), x);

  /* Reuse of -1.0f to reduce constant loads,
     We need a shift value of 1/2, which is created via -1 + (1 + 1/2).  */
  svfloat64_t shift
      = svadd_z (red, sv_f64 (d->neg_one), sv_f64 (d->shift_val));

  /* Reinserts the sign bit of the argument to handle the case of x < -1.  */
  shift = svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (shift), sign));

  /* Use split Estrin scheme for P(z^2) with deg(P)=19.  */
  svfloat64_t z2 = svmul_x (ptrue, z, z);
  svfloat64_t z4 = svmul_x (ptrue, z2, z2);
  svfloat64_t z8 = svmul_x (ptrue, z4, z4);
  svfloat64_t z16 = svmul_x (ptrue, z8, z8);

  /* Order-7 Estrin.  */
  svfloat64_t c24 = svld1rq (ptrue, &d->c2);
  svfloat64_t c68 = svld1rq (ptrue, &d->c6);

  svfloat64_t p12 = svmla_lane (sv_f64 (d->c1), z2, c24, 0);
  svfloat64_t p34 = svmla_lane (sv_f64 (d->c3), z2, c24, 1);
  svfloat64_t p56 = svmla_lane (sv_f64 (d->c5), z2, c68, 0);
  svfloat64_t p78 = svmla_lane (sv_f64 (d->c7), z2, c68, 1);

  svfloat64_t p14 = svmla_x (pg, p12, z4, p34);
  svfloat64_t p58 = svmla_x (pg, p56, z4, p78);
  svfloat64_t p18 = svmla_x (pg, p14, z8, p58);

  /* Order-11 Estrin.  */
  svfloat64_t c1012 = svld1rq (ptrue, &d->c10);
  svfloat64_t c1416 = svld1rq (ptrue, &d->c14);
  svfloat64_t c1820 = svld1rq (ptrue, &d->c18);

  svfloat64_t p910 = svmla_lane (sv_f64 (d->c9), z2, c1012, 0);
  svfloat64_t p1112 = svmla_lane (sv_f64 (d->c11), z2, c1012, 1);
  svfloat64_t p912 = svmla_x (pg, p910, z4, p1112);

  svfloat64_t p1314 = svmla_lane (sv_f64 (d->c13), z2, c1416, 0);
  svfloat64_t p1516 = svmla_lane (sv_f64 (d->c15), z2, c1416, 1);
  svfloat64_t p1316 = svmla_x (pg, p1314, z4, p1516);

  svfloat64_t p1718 = svmla_lane (sv_f64 (d->c17), z2, c1820, 0);
  svfloat64_t p1920 = svmla_lane (sv_f64 (d->c19), z2, c1820, 1);
  svfloat64_t p1720 = svmla_x (pg, p1718, z4, p1920);

  svfloat64_t p916 = svmla_x (pg, p912, z8, p1316);
  svfloat64_t p920 = svmla_x (pg, p916, z16, p1720);

  svfloat64_t y = svmla_x (pg, p18, z16, p920);

  y = svmla_x (pg, sv_f64 (d->c0), z2, y);
  return svmla_x (pg, shift, z, y);
}
