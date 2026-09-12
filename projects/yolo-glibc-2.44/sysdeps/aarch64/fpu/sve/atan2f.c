/* Single-precision SVE atan2

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
  float32_t c0, c2, c4, c6;
  float32_t c1, c3, c5, c7;
  float32_t pi_over_2;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2**-128, 1.0].  */
  .c0 = -0x1.5554dcp-2, .c1 = 0x1.9978ecp-3,  .c2 = -0x1.230a94p-3,
  .c3 = 0x1.b4debp-4,	.c4 = -0x1.3550dap-4, .c5 = 0x1.61eebp-5,
  .c6 = -0x1.0c17d4p-6, .c7 = 0x1.7ea694p-9,  .pi_over_2 = 0x1.921fb6p+0f,
};

/* Special cases i.e. 0, infinity, nan (fall back to scalar calls).  */
static svfloat32_t NOINLINE
special_case (svfloat32_t y, svfloat32_t x, svfloat32_t ret,
	      const svbool_t cmp)
{
  return sv_call2_f32 (atan2f, y, x, ret, cmp);
}

/* Returns a predicate indicating true if the input is the bit representation
   of 0, infinity or nan.  */
static inline svbool_t
zeroinfnan (svuint32_t i, const svbool_t pg)
{
  return svcmpge (pg, svsub_x (pg, svlsl_x (pg, i, 1), 1),
		  sv_u32 (2 * 0x7f800000lu - 1));
}

/* Fast implementation of SVE atan2f based on atan(x) ~ shift + z + z^3 *
   P(z^2) with reduction to [0,1] using z=1/x and shift = pi/2. Maximum
   observed error is 2.21 ULP:
   _ZGVnN4vv_atan2f (0x1.a04aa8p+6, 0x1.9a274p+6) got 0x1.95ed3ap-1
						 want 0x1.95ed36p-1.  */
svfloat32_t SV_NAME_F2 (atan2) (svfloat32_t y, svfloat32_t x,
				const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t ptrue = svptrue_b32 ();

  svuint32_t ix = svreinterpret_u32 (x);
  svuint32_t iy = svreinterpret_u32 (y);

  svbool_t cmp_x = zeroinfnan (ix, pg);
  svbool_t cmp_y = zeroinfnan (iy, pg);
  svbool_t cmp_xy = svorr_z (pg, cmp_x, cmp_y);

  svfloat32_t ax = svabs_x (pg, x);
  svfloat32_t ay = svabs_x (pg, y);
  svuint32_t iax = svreinterpret_u32 (ax);
  svuint32_t iay = svreinterpret_u32 (ay);

  svuint32_t sign_x = sveor_x (pg, ix, iax);
  svuint32_t sign_y = sveor_x (pg, iy, iay);
  svuint32_t sign_xy = sveor_x (pg, sign_x, sign_y);

  svbool_t pred_aygtax = svcmpgt (pg, ay, ax);

  /* Set up z for evaluation of atanf.  */
  svfloat32_t num = svsel (pred_aygtax, svneg_x (pg, ax), ay);
  svfloat32_t den = svsel (pred_aygtax, ay, ax);
  svfloat32_t z = svdiv_x (ptrue, num, den);

  /* Work out the correct shift for atan2:
     Multiplication by pi is done later.
     -pi   when x < 0  and ax < ay
     -pi/2 when x < 0  and ax > ay
      0    when x >= 0 and ax < ay
      pi/2 when x >= 0 and ax > ay.  */
  svfloat32_t shift = svreinterpret_f32 (svlsr_x (pg, sign_x, 1));
  shift = svsel (pred_aygtax, sv_f32 (1.0), shift);
  shift = svreinterpret_f32 (svorr_x (pg, sign_x, svreinterpret_u32 (shift)));

  /* Use pure Estrin scheme for P(z^2) with deg(P)=7.  */
  svfloat32_t z2 = svmul_x (ptrue, z, z);
  svfloat32_t z3 = svmul_x (pg, z2, z);
  svfloat32_t z4 = svmul_x (pg, z2, z2);
  svfloat32_t z8 = svmul_x (pg, z4, z4);

  svfloat32_t odd_coeffs = svld1rq (ptrue, &d->c1);

  svfloat32_t p01 = svmla_lane (sv_f32 (d->c0), z2, odd_coeffs, 0);
  svfloat32_t p23 = svmla_lane (sv_f32 (d->c2), z2, odd_coeffs, 1);
  svfloat32_t p45 = svmla_lane (sv_f32 (d->c4), z2, odd_coeffs, 2);
  svfloat32_t p67 = svmla_lane (sv_f32 (d->c6), z2, odd_coeffs, 3);

  svfloat32_t p03 = svmla_x (pg, p01, z4, p23);
  svfloat32_t p47 = svmla_x (pg, p45, z4, p67);

  svfloat32_t poly = svmla_x (pg, p03, z8, p47);

  /* ret = shift + z + z^3 * P(z^2).  */
  svfloat32_t ret = svmla_x (pg, z, shift, sv_f32 (d->pi_over_2));
  ret = svmla_x (pg, ret, z3, poly);

  /* Account for the sign of x and y.  */

  if (__glibc_unlikely (svptest_any (pg, cmp_xy)))
    return special_case (
	y, x,
	svreinterpret_f32 (sveor_x (pg, svreinterpret_u32 (ret), sign_xy)),
	cmp_xy);

  return svreinterpret_f32 (sveor_x (pg, svreinterpret_u32 (ret), sign_xy));
}
