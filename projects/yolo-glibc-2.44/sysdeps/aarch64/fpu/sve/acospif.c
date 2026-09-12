/* Single-Precision vector (SVE) inverse cospi function

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
  float32_t c0, c1, c2, c3, c4, inv_pi, half;
} data = {
  /* Coefficients of polynomial P such that asin(x)/pi~ x/pi + x^3 * poly(x^2)
     on [ 0x1p-126 0x1p-2 ]. rel error: 0x1.ef9f94b1p-33. Generated using
     iterative approach for minimisation of relative error.  */
  .c0 = 0x1.b29968p-5f, .c1 = 0x1.871424p-6f, .c2 = 0x1.d56e44p-7f,
  .c3 = 0x1.149bb8p-7f, .c4 = 0x1.8e07fep-7f, .inv_pi = 0x1.45f306p-2f,
  .half = 0.5f,
};

/* Single-precision SVE implementation of vector acospi(x).

   For |x| in [0, 0.5], use order 5 polynomial P to approximate asinpi
   such that the final approximation of acospi is:

     acospi(x) ~ 1/2 - (x/pi + x^3 P(x^2)).

    The largest observed error in this region is 1.3 ulps,
      _ZGVsMxv_acospif(0x1.ffa9d2p-2) got 0x1.557504p-2
				     want 0x1.557502p-2.

   For |x| in [0.5, 1.0], use same approximation with a change of variable

      acospi(x) = y/pi + y * z * P(z), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 2.61 ulps,
   _ZGVsMxv_acospif (0x1.6b232ep-1) got 0x1.fe04bap-3
				   want 0x1.fe04cp-3.  */
svfloat32_t SV_NAME_F1 (acospi) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svbool_t ptrue = svptrue_b32 ();

  svuint32_t sign = svand_x (pg, svreinterpret_u32 (x), 0x80000000);
  svfloat32_t ax = svabs_x (pg, x);
  svbool_t a_gt_half = svacgt (pg, x, 0.5f);

  /* Evaluate polynomial Q(x) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  svfloat32_t z2 = svsel (a_gt_half, svmls_x (pg, sv_f32 (0.5f), ax, 0.5f),
			  svmul_x (ptrue, x, x));
  svfloat32_t z = svsqrt_m (ax, a_gt_half, z2);

  /* Use a single polynomial approximation P for both intervals.  */
  svfloat32_t p = svmla_x (pg, sv_f32 (d->c3), z2, d->c4);
  p = svmad_x (pg, z2, p, d->c2);
  p = svmad_x (pg, z2, p, d->c1);
  p = svmad_x (pg, z2, p, d->c0);
  /* Add 1/pi as final coeff.  */
  p = svmla_x (pg, sv_f32 (d->inv_pi), z2, p);
  /* Finalize polynomial: z * P(z^2).  */
  p = svmul_x (ptrue, z, p);

  /* acospi(|x|)
			  = 1/2 - sign(x) * Q(|x|), for       |x| < 0.5
			  = 2 Q(|x|)              , for  0.5 < x < 1.0
			  = 1 - 2 Q(|x|)          , for -1.0 < x < -0.5.  */
  svfloat32_t y
      = svreinterpret_f32 (svorr_x (ptrue, svreinterpret_u32 (p), sign));
  svfloat32_t mul = svsel (a_gt_half, sv_f32 (2.0f), sv_f32 (-1.0f));
  svfloat32_t add = svreinterpret_f32 (
      svorr_x (ptrue, sign, svreinterpret_u32 (sv_f32 (d->half))));
  add = svsub_m (a_gt_half, sv_f32 (d->half), add);

  return svmad_x (pg, y, mul, add);
}
