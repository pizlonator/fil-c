/* Double-Precision vector (SVE) inverse tan2pi function

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

#include "math_config.h"
#include "sv_math.h"

static const struct data
{
  float64_t c2, c4, c6, c8, c10, c12, c14, c16, c18, c20;
  float64_t c0, c1, c3, c5, c7, c9, c11, c13, c15, c17, c19;
  float64_t shift_val;
} data = {
  /* Coefficients of polnomial P such that atan(x)~x+x*P(x^2) on
     [2^-1022, 1.0].  */
  .c0 = 0x1.45f306dc9c883p-2,	.c1 = -0x1.b2995e7b7ba4ap-4,
  .c2 = 0x1.04c26be3d2c1p-4,	.c3 = -0x1.7483759c17ea1p-5,
  .c4 = 0x1.21bb95c315d57p-5,	.c5 = -0x1.da1bdc3d453f3p-6,
  .c6 = 0x1.912d20459b4bfp-6,	.c7 = -0x1.5bbd4545cad1fp-6,
  .c8 = 0x1.331b83bec30a1p-6,	.c9 = -0x1.13d6457f44de3p-6,
  .c10 = 0x1.f8e802974db94p-7,	.c11 = -0x1.d7e173ab04a1ap-7,
  .c12 = 0x1.bdfa47d6a4f28p-7,	.c13 = -0x1.9ba78f3232ceep-7,
  .c14 = 0x1.5e6044590ab4fp-7,	.c15 = -0x1.01ccfdeb9f77fp-7,
  .c16 = 0x1.345cf0d4eb1c1p-8,	.c17 = -0x1.19e5f00f67e3ap-9,
  .c18 = 0x1.6d3035ac7625bp-11, .c19 = -0x1.286bb9ae4ed79p-13,
  .c20 = 0x1.c37ec36da0e1ap-17, .shift_val = 0.5,
};

#define OneOverPi sv_f64 (0x1.45f306dc9c883p-2)

/* Special cases i.e. 0, infinity, nan (fall back to scalar calls).  */
static svfloat64_t NOINLINE
special_case (svfloat64_t y, svfloat64_t x, svfloat64_t ret,
	      const svbool_t cmp)
{
  ret = sv_call2_f64 (atan2, y, x, ret, cmp);
  return svmul_f64_m (cmp, ret, OneOverPi);
}

/* Returns a predicate indicating true if the input is the bit representation
   of 0, infinity or nan.  */
static inline svbool_t
zeroinfnan (svuint64_t i, const svbool_t pg)
{
  return svcmpge (pg, svsub_x (pg, svlsl_x (pg, i, 1), 1),
		  sv_u64 (2 * asuint64 (INFINITY) - 1));
}

/* Fast implementation of SVE atan2pi.
   Maximum observed error is 3.11 ulps:
   _ZGVsMxvv_atan2pi (0x1.ef284a877f6b5p+6, 0x1.03fdde8242b17p+7)
   got 0x1.f00f800163079p-3 want 0x1.f00f800163076p-3.  */
svfloat64_t SV_NAME_D2 (atan2pi) (svfloat64_t y, svfloat64_t x,
				  const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t ptrue = svptrue_b64 ();

  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t iy = svreinterpret_u64 (y);

  svbool_t cmp_x = zeroinfnan (ix, pg);
  svbool_t cmp_y = zeroinfnan (iy, pg);
  svbool_t cmp_xy = svorr_z (pg, cmp_x, cmp_y);

  svfloat64_t ax = svabs_x (pg, x);
  svfloat64_t ay = svabs_x (pg, y);
  svuint64_t iax = svreinterpret_u64 (ax);
  svuint64_t iay = svreinterpret_u64 (ay);

  svuint64_t sign_x = sveor_x (pg, ix, iax);
  svuint64_t sign_y = sveor_x (pg, iy, iay);
  svuint64_t sign_xy = sveor_x (pg, sign_x, sign_y);

  svbool_t pred_aygtax = svcmpgt (pg, ay, ax);

  /* Set up z for evaluation of atanpi.  */
  svfloat64_t num = svsel (pred_aygtax, svneg_x (pg, ax), ay);
  svfloat64_t den = svsel (pred_aygtax, ay, ax);
  svfloat64_t z = svdiv_x (pg, num, den);

  /* Work out the correct shift for atan2pi:
     -1.0 when x < 0  and ax < ay
     -0.5 when x < 0  and ax > ay
      0   when x >= 0 and ax < ay
      0.5 when x >= 0 and ax > ay.  */
  svfloat64_t shift = svreinterpret_f64 (svlsr_x (pg, sign_x, 1));
  shift = svmul_x (ptrue, shift, sv_f64 (d->shift_val));
  shift = svsel (pred_aygtax, sv_f64 (d->shift_val), shift);
  shift = svreinterpret_f64 (svorr_x (pg, sign_x, svreinterpret_u64 (shift)));

  /* Use split Estrin scheme for P(z^2) with deg(P)=19.  */
  svfloat64_t z2 = svmul_x (pg, z, z);
  svfloat64_t z3 = svmul_x (pg, z2, z);
  svfloat64_t z4 = svmul_x (pg, z2, z2);
  svfloat64_t z8 = svmul_x (pg, z4, z4);
  svfloat64_t z16 = svmul_x (pg, z8, z8);

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

  svfloat64_t poly = svmla_x (pg, p18, z16, p920);

  svfloat64_t ret = svmla_x (pg, shift, z, sv_f64 (d->c0));
  ret = svmla_x (pg, ret, z3, poly);

  /* Account for the sign of x and y.  */
  if (__glibc_unlikely (svptest_any (pg, cmp_xy)))
    return special_case (
	y, x,
	svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (ret), sign_xy)),
	cmp_xy);
  return svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (ret), sign_xy));
}
