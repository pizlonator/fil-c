/* Double-precision SVE inverse cos

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
  float64_t c1, c3, c5, c7, c9, c11;
  float64_t c0, c2, c4, c6, c8, c10;
  float64_t pi_over_2;
} data = {
  /* Polynomial approximation of  (asin(sqrt(x)) - sqrt(x)) / (x * sqrt(x))
     on [ 0x1p-106, 0x1p-2 ], relative error: 0x1.c3d8e169p-57.  */
  .c0 = 0x1.555555555554ep-3,	     .c1 = 0x1.3333333337233p-4,
  .c2 = 0x1.6db6db67f6d9fp-5,	     .c3 = 0x1.f1c71fbd29fbbp-6,
  .c4 = 0x1.6e8b264d467d6p-6,	     .c5 = 0x1.1c5997c357e9dp-6,
  .c6 = 0x1.c86a22cd9389dp-7,	     .c7 = 0x1.856073c22ebbep-7,
  .c8 = 0x1.fd1151acb6bedp-8,	     .c9 = 0x1.087182f799c1dp-6,
  .c10 = -0x1.6602748120927p-7,	     .c11 = 0x1.cfa0dd1f9478p-6,
  .pi_over_2 = 0x1.921fb54442d18p+0,
};

/* Double-precision SVE implementation of vector acos(x).

   For |x| in [0, 0.5], use an order 11 polynomial P such that the final
   approximation of asin is an odd polynomial:

     acos(x) ~ pi/2 - (x + x^3 P(x^2)).

   The largest observed error in this region is 1.18 ulp:
   _ZGVsMxv_acos (0x1.fbb7c9079b429p-2) got 0x1.0d51266607582p+0
				       want 0x1.0d51266607583p+0.

   For |x| in [0.5, 1.0], use same approximation with a change of variable

     acos(x) = y + y * z * P(z), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 1.50 ulp:
   _ZGVsMxv_acos (0x1.252a2cf3fb9acp-1) got 0x1.ec1a46aa82901p-1
				       want 0x1.ec1a46aa829p-1.  */
svfloat64_t SV_NAME_D1 (acos) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t ptrue = svptrue_b64 ();

  svuint64_t sign = svand_x (pg, svreinterpret_u64 (x), 0x8000000000000000);
  svfloat64_t ax = svabs_x (pg, x);

  svbool_t a_gt_half = svacgt (pg, x, 0.5);

  /* Evaluate polynomial Q(x) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  svfloat64_t z2 = svsel (a_gt_half, svmls_x (pg, sv_f64 (0.5), ax, 0.5),
			  svmul_x (pg, x, x));
  svfloat64_t z = svsqrt_m (ax, a_gt_half, z2);

  /* Use a single polynomial approximation P for both intervals.  */
  svfloat64_t z3 = svmul_x (ptrue, z2, z);
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
  svfloat64_t p = svmad_x (pg, p411, z8, p03);

  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = svmad_x (pg, p, z3, z);

  /* acos(|x|) = pi/2 - sign(x) * Q(|x|), for  |x| < 0.5
	       = 2 Q(|x|)               , for  0.5 < x < 1.0
	       = pi - 2 Q(|x|)          , for -1.0 < x < -0.5.  */
  svfloat64_t mul = svreinterpret_f64 (
      svlsl_m (a_gt_half, svreinterpret_u64 (sv_f64 (1.0)), 10));
  mul = svreinterpret_f64 (sveor_x (ptrue, svreinterpret_u64 (mul), sign));
  svfloat64_t add = svreinterpret_f64 (
      svorr_x (ptrue, sign, svreinterpret_u64 (sv_f64 (d->pi_over_2))));
  add = svsub_m (a_gt_half, sv_f64 (d->pi_over_2), add);

  return svmsb_x (pg, p, mul, add);
}
