/* Single-Precision vector (SVE) inverse tan2pi function

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
  float32_t c0, c1, c3, c5, c7;
  float32_t c2, c4, c6, c8;
  float32_t shift_val;
  uint32_t comp_const;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2**-128, 1.0].  */
  .c0 = 0x1.45f306p-2,
  .c1 = -0x1.b2975ep-4,
  .c2 = 0x1.0490e4p-4,
  .c3 = -0x1.70c272p-5,
  .c4 = 0x1.0eef52p-5,
  .c5 = -0x1.6abbbap-6,
  .c6 = 0x1.78157p-7,
  .c7 = -0x1.f0b406p-9,
  .c8 = 0x1.2ae7fep-11,
  .shift_val = 0.5f,
  .comp_const = 2 * 0x7f800000lu - 1,
};

#define OneOverPi sv_f32 (0x1.45f307p-2)

/* Special cases i.e. 0, infinity, nan (fall back to scalar calls).  */
static svfloat32_t NOINLINE
special_case (svfloat32_t y, svfloat32_t x, svfloat32_t ret,
	      const svbool_t cmp)
{
  ret = sv_call2_f32 (atan2f, y, x, ret, cmp);
  return svmul_f32_x (cmp, ret, OneOverPi);
}

/* Returns a predicate indicating true if the input is the bit representation
   of 0, infinity or nan.  */
static inline svbool_t
zeroinfnan (svuint32_t i, const svbool_t pg, const struct data *d)
{
  return svcmpge (pg, svsub_x (pg, svlsl_x (pg, i, 1), 1),
		  sv_u32 (d->comp_const));
}

/* Fast implementation of SVE atan2pif based on atan(x) ~ shift + z + z^3 *
   P(z^2) with reduction to [0,1] using z=1/x and shift = 1/2. Maximum
   observed error is 2.90 ULP:
   _ZGVsMxvv_atan2pif (0x1.a28542p+5, 0x1.adb7c6p+5) got 0x1.f76524p-3
						    want 0x1.f7651ep-3.  */
svfloat32_t SV_NAME_F2 (atan2pi) (svfloat32_t y, svfloat32_t x,
				  const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t ptrue = svptrue_b32 ();

  svuint32_t ix = svreinterpret_u32 (x);
  svuint32_t iy = svreinterpret_u32 (y);

  svbool_t cmp_x = zeroinfnan (ix, pg, d);
  svbool_t cmp_y = zeroinfnan (iy, pg, d);
  svbool_t cmp_xy = svorr_z (pg, cmp_x, cmp_y);

  svfloat32_t ax = svabs_x (pg, x);
  svfloat32_t ay = svabs_x (pg, y);
  svuint32_t iax = svreinterpret_u32 (ax);
  svuint32_t iay = svreinterpret_u32 (ay);

  svuint32_t sign_x = sveor_x (pg, ix, iax);
  svuint32_t sign_y = sveor_x (pg, iy, iay);
  svuint32_t sign_xy = sveor_x (pg, sign_x, sign_y);

  svbool_t pred_aygtax = svcmpgt (pg, ay, ax);

  /* Set up z for evaluation of atanpif.  */
  svfloat32_t num = svsel (pred_aygtax, svneg_x (pg, ax), ay);
  svfloat32_t den = svsel (pred_aygtax, ay, ax);
  svfloat32_t z = svdiv_x (ptrue, num, den);

  /* Work out the correct shift for atan2pi:
     -1.0 when x < 0  and ax < ay
     -0.5 when x < 0  and ax > ay
      0   when x >= 0 and ax < ay
      0.5 when x >= 0 and ax > ay.  */
  svfloat32_t shift = svreinterpret_f32 (svlsr_x (pg, sign_x, 1));
  shift = svmul_x (ptrue, shift, sv_f32 (d->shift_val));
  shift = svsel (pred_aygtax, sv_f32 (d->shift_val), shift);
  shift = svreinterpret_f32 (svorr_x (pg, sign_x, svreinterpret_u32 (shift)));

  /* Use pure Estrin scheme for P(z^2) with deg(P)=7.  */
  svfloat32_t z2 = svmul_x (pg, z, z);
  svfloat32_t z4 = svmul_x (pg, z2, z2);
  svfloat32_t z8 = svmul_x (pg, z4, z4);

  svfloat32_t even_coeffs = svld1rq (ptrue, &d->c2);

  svfloat32_t p12 = svmla_lane (sv_f32 (d->c1), z2, even_coeffs, 0);
  svfloat32_t p34 = svmla_lane (sv_f32 (d->c3), z2, even_coeffs, 1);
  svfloat32_t p56 = svmla_lane (sv_f32 (d->c5), z2, even_coeffs, 2);
  svfloat32_t p78 = svmla_lane (sv_f32 (d->c7), z2, even_coeffs, 3);

  svfloat32_t p14 = svmad_x (pg, z4, p34, p12);
  svfloat32_t p58 = svmad_x (pg, z4, p78, p56);

  svfloat32_t p18 = svmad_x (pg, z8, p58, p14);

  /* ret = shift + z + z^3 * P(z^2).  */
  svfloat32_t poly = svmad_x (pg, z2, p18, d->c0);
  svfloat32_t ret = svmad_x (pg, poly, z, shift);

  if (__glibc_unlikely (svptest_any (pg, cmp_xy)))
    return special_case (
	y, x,
	svreinterpret_f32 (sveor_x (pg, svreinterpret_u32 (ret), sign_xy)),
	cmp_xy);

  /* Account for the sign of x and y.  */
  return svreinterpret_f32 (sveor_x (pg, svreinterpret_u32 (ret), sign_xy));
}
