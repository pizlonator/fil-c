/* Double-precision SVE inverse tan

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
  float64_t c0, c2, c4, c6, c8, c10, c12, c14, c16, c18;
  float64_t c1, c3, c5, c7, c9, c11, c13, c15, c17, c19;
  float64_t shift_val, neg_one;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2**-1022, 1.0].  */
  .c0 = -0x1.555555555552ap-2,	     .c1 = 0x1.9999999995aebp-3,
  .c2 = -0x1.24924923923f6p-3,	     .c3 = 0x1.c71c7184288a2p-4,
  .c4 = -0x1.745d11fb3d32bp-4,	     .c5 = 0x1.3b136a18051b9p-4,
  .c6 = -0x1.110e6d985f496p-4,	     .c7 = 0x1.e1bcf7f08801dp-5,
  .c8 = -0x1.ae644e28058c3p-5,	     .c9 = 0x1.82eeb1fed85c6p-5,
  .c10 = -0x1.59d7f901566cbp-5,	     .c11 = 0x1.2c982855ab069p-5,
  .c12 = -0x1.eb49592998177p-6,	     .c13 = 0x1.69d8b396e3d38p-6,
  .c14 = -0x1.ca980345c4204p-7,	     .c15 = 0x1.dc050eafde0b3p-8,
  .c16 = -0x1.7ea70755b8eccp-9,	     .c17 = 0x1.ba3da3de903e8p-11,
  .c18 = -0x1.44a4b059b6f67p-13,     .c19 = 0x1.c4a45029e5a91p-17,
  .shift_val = 0x1.490fdaa22168cp+1, .neg_one = -1,
};

/* Useful constants.  */
#define SignMask (0x8000000000000000)

/* Fast implementation of SVE atan.
   Based on atan(x) ~ shift + z + z^3 * P(z^2) with reduction to [0,1] using
   z=1/x and shift = pi/2. Largest errors are close to 1. The maximum observed
   error is 2.08 ulps:
   _ZGVsMxv_atan (0x1.000a7c56975e8p+0) got 0x1.922a3163e15c2p-1
				       want 0x1.922a3163e15c4p-1.  */
svfloat64_t SV_NAME_D1 (atan) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svbool_t ptrue = svptrue_b64 ();
  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t sign = svand_x (pg, ix, SignMask);

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
  svfloat64_t c13 = svld1rq (ptrue, &d->c1);
  svfloat64_t c57 = svld1rq (ptrue, &d->c5);

  svfloat64_t p01 = svmla_lane (sv_f64 (d->c0), z2, c13, 0);
  svfloat64_t p23 = svmla_lane (sv_f64 (d->c2), z2, c13, 1);
  svfloat64_t p45 = svmla_lane (sv_f64 (d->c4), z2, c57, 0);
  svfloat64_t p67 = svmla_lane (sv_f64 (d->c6), z2, c57, 1);

  svfloat64_t p03 = svmla_x (pg, p01, z4, p23);
  svfloat64_t p47 = svmla_x (pg, p45, z4, p67);
  svfloat64_t p07 = svmla_x (pg, p03, z8, p47);

  /* Order-11 Estrin.  */
  svfloat64_t c911 = svld1rq (ptrue, &d->c9);
  svfloat64_t c1315 = svld1rq (ptrue, &d->c13);
  svfloat64_t c1719 = svld1rq (ptrue, &d->c17);

  svfloat64_t p89 = svmla_lane (sv_f64 (d->c8), z2, c911, 0);
  svfloat64_t p1011 = svmla_lane (sv_f64 (d->c10), z2, c911, 1);
  svfloat64_t p811 = svmla_x (pg, p89, z4, p1011);

  svfloat64_t p1213 = svmla_lane (sv_f64 (d->c12), z2, c1315, 0);
  svfloat64_t p1415 = svmla_lane (sv_f64 (d->c14), z2, c1315, 1);
  svfloat64_t p1215 = svmla_x (pg, p1213, z4, p1415);

  svfloat64_t p1617 = svmla_lane (sv_f64 (d->c16), z2, c1719, 0);
  svfloat64_t p1819 = svmla_lane (sv_f64 (d->c18), z2, c1719, 1);
  svfloat64_t p1619 = svmla_x (pg, p1617, z4, p1819);

  svfloat64_t p815 = svmla_x (pg, p811, z8, p1215);
  svfloat64_t p819 = svmla_x (pg, p815, z16, p1619);

  svfloat64_t y = svmla_x (pg, p07, z16, p819);

  /* y = shift + z + z^3 * P(z^2).  */
  shift = svadd_m (red, z, shift);
  y = svmul_x (pg, z2, y);
  return svmla_x (pg, shift, z, y);
}
