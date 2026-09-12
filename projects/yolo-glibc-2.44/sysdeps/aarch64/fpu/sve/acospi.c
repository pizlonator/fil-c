/* Double-Precision vector (SVE) inverse cospi function

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
  float64_t c1, c3, c5, c7, c9, c11;
  float64_t c0, c2, c4, c6, c8, c10;
  float64_t inv_pi, half;
} data = {
  /* Coefficients of polynomial P such that asin(x)/pi~ x/pi + x^3 * poly(x^2)
     on [ 0x1p-126 0x1p-2 ]. rel error: 0x1.ef9f94b1p-33. Generated using
     iterative approach for minimisation of relative error in asinpif Sollya
     file.  */
  .c0 = 0x1.b2995e7b7b5fbp-5,	  .c1 = 0x1.8723a1d58d83p-6,
  .c2 = 0x1.d1a452eacf2fep-7,	  .c3 = 0x1.3ce52c4d75582p-7,
  .c4 = 0x1.d2b2a0aea27d5p-8,	  .c5 = 0x1.6a0b9b92cad8bp-8,
  .c6 = 0x1.2290c84438caep-8,	  .c7 = 0x1.efba896580d02p-9,
  .c8 = 0x1.44446707af38p-9,	  .c9 = 0x1.5070b3e7aa03ep-8,
  .c10 = -0x1.c70015d0ebdafp-9,	  .c11 = 0x1.27029c383fed9p-7,
  .inv_pi = 0x1.45f306dc9c883p-2, .half = 0.5,
};

/* Double-precision SVE implementation of vector acospi(x).

   For |x| in [0, 0.5], use order 11 polynomial P to approximate asinpi
   such that the final approximation of acospi is:

     acospi(x) ~ 1/2 - (x/pi + x^3 P(x^2)).

   The largest observed error in this region is 1.35 ulp:
   _ZGVsMxv_acospi (0x1.fb014996aea18p-2) got 0x1.572a91755bbf6p-2
					 want 0x1.572a91755bbf7p-2.

   For |x| in [0.5, 1.0], use same approximation with a change of variable

      acospi(x) = y/pi + y * z * P(z), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 2.55 ulp:
   _ZGVsMxv_acospi(0x1.d90d50357410cp-1) got 0x1.ffd43d5dd3a9ep-4
					want 0x1.ffd43d5dd3a9bp-4.  */
svfloat64_t SV_NAME_D1 (acospi) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t ptrue = svptrue_b64 ();

  svuint64_t sign = svand_x (pg, svreinterpret_u64 (x), 0x8000000000000000);
  svfloat64_t ax = svabs_x (pg, x);
  svbool_t a_gt_half = svacgt (pg, x, 0.5f);

  /* Evaluate polynomial Q(x) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  svfloat64_t z2 = svsel (a_gt_half, svmls_x (pg, sv_f64 (0.5), ax, 0.5),
			  svmul_x (ptrue, x, x));
  svfloat64_t z = svsqrt_m (ax, a_gt_half, z2);

  /* Order-11 Estrin.  */
  svfloat64_t z4 = svmul_x (ptrue, z2, z2);
  svfloat64_t z8 = svmul_x (ptrue, z4, z4);

  svfloat64_t c13 = svld1rq (ptrue, &d->c1);
  svfloat64_t c57 = svld1rq (ptrue, &d->c5);
  svfloat64_t c911 = svld1rq (ptrue, &d->c9);

  svfloat64_t p01 = svmla_lane (sv_f64 (d->c0), z2, c13, 0);
  svfloat64_t p23 = svmla_lane (sv_f64 (d->c2), z2, c13, 1);
  svfloat64_t p03 = svmla_x (pg, p01, z4, p23);

  svfloat64_t p45 = svmla_lane (sv_f64 (d->c4), z2, c57, 0);
  svfloat64_t p67 = svmla_lane (sv_f64 (d->c6), z2, c57, 1);
  svfloat64_t p47 = svmla_x (pg, p45, z4, p67);

  svfloat64_t p89 = svmla_lane (sv_f64 (d->c8), z2, c911, 0);
  svfloat64_t p1011 = svmla_lane (sv_f64 (d->c10), z2, c911, 1);
  svfloat64_t p811 = svmla_x (pg, p89, z4, p1011);

  svfloat64_t p411 = svmla_x (pg, p47, z8, p811);
  svfloat64_t p = svmla_x (pg, p03, z8, p411);

  p = svmla_x (pg, sv_f64 (d->inv_pi), z2, p);
  p = svmul_x (ptrue, p, z);

  /* acospi(|x|) = 1/2 - sign(x) * Q(|x|), for       |x| < 0.5
		 = 2 Q(|x|)              , for  0.5 < x < 1.0
		 = 1 - 2 Q(|x|)          , for -1.0 < x < -0.5.  */
  svfloat64_t mul = svreinterpret_f64 (
      svlsl_m (a_gt_half, svreinterpret_u64 (sv_f64 (1.0)), 10));
  mul = svreinterpret_f64 (sveor_x (ptrue, svreinterpret_u64 (mul), sign));
  svfloat64_t add = svreinterpret_f64 (
      svorr_x (ptrue, sign, svreinterpret_u64 (sv_f64 (d->half))));
  add = svsub_m (a_gt_half, sv_f64 (d->half), add);

  return svmsb_x (pg, p, mul, add);
}
