/* Double-precision SVE atan2

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

#include "math_config.h"
#include "sv_math.h"

static const struct data
{
  float64_t c0, c2, c4, c6, c8, c10, c12, c14, c16, c18;
  float64_t c1, c3, c5, c7, c9, c11, c13, c15, c17, c19;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2**-1022, 1.0].  */
  .c0 = -0x1.555555555552ap-2,	 .c1 = 0x1.9999999995aebp-3,
  .c2 = -0x1.24924923923f6p-3,	 .c3 = 0x1.c71c7184288a2p-4,
  .c4 = -0x1.745d11fb3d32bp-4,	 .c5 = 0x1.3b136a18051b9p-4,
  .c6 = -0x1.110e6d985f496p-4,	 .c7 = 0x1.e1bcf7f08801dp-5,
  .c8 = -0x1.ae644e28058c3p-5,	 .c9 = 0x1.82eeb1fed85c6p-5,
  .c10 = -0x1.59d7f901566cbp-5,	 .c11 = 0x1.2c982855ab069p-5,
  .c12 = -0x1.eb49592998177p-6,	 .c13 = 0x1.69d8b396e3d38p-6,
  .c14 = -0x1.ca980345c4204p-7,	 .c15 = 0x1.dc050eafde0b3p-8,
  .c16 = -0x1.7ea70755b8eccp-9,	 .c17 = 0x1.ba3da3de903e8p-11,
  .c18 = -0x1.44a4b059b6f67p-13, .c19 = 0x1.c4a45029e5a91p-17,
};
/* Special cases i.e. 0, infinity, nan (fall back to scalar calls).  */
static svfloat64_t NOINLINE
special_case (svfloat64_t y, svfloat64_t x, svfloat64_t ret,
	      const svbool_t cmp)
{
  return sv_call2_f64 (atan2, y, x, ret, cmp);
}

/* Returns a predicate indicating true if the input is the bit representation
   of 0, infinity or nan.  */
static inline svbool_t
zeroinfnan (svuint64_t i, const svbool_t pg)
{
  return svcmpge (pg, svsub_x (pg, svlsl_x (pg, i, 1), 1),
		  sv_u64 (2 * asuint64 (INFINITY) - 1));
}

/* Fast implementation of SVE atan2. Errors are greatest when y and
   x are reasonably close together. The greatest observed error is 1.94 ULP:
   _ZGVsMxvv_atan2 (0x1.8a4bf7167228ap+5, 0x1.84971226bb57bp+5)
   got 0x1.95db19dfef9ccp-1 want 0x1.95db19dfef9cep-1.  */
svfloat64_t SV_NAME_D2 (atan2) (svfloat64_t y, svfloat64_t x,
				const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t iy = svreinterpret_u64 (y);
  svbool_t ptrue = svptrue_b64 ();

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

  /* Set up z for evaluation of atan.  */
  svfloat64_t num = svsel (pred_aygtax, svneg_x (pg, ax), ay);
  svfloat64_t den = svsel (pred_aygtax, ay, ax);
  svfloat64_t z = svdiv_x (pg, num, den);

  /* Work out the correct shift for atan2:
     Multiplication by pi is done later.
     -pi   when x < 0  and ax < ay
     -pi/2 when x < 0  and ax > ay
      0    when x >= 0 and ax < ay
      pi/2 when x >= 0 and ax > ay.  */
  svfloat64_t shift = svreinterpret_f64 (svlsr_x (pg, sign_x, 1));
  svfloat64_t shift_mul = svreinterpret_f64 (
      svorr_x (pg, sign_x, svreinterpret_u64 (sv_f64 (0x1.921fb54442d18p+0))));
  shift = svsel (pred_aygtax, sv_f64 (1.0), shift);
  shift = svmla_x (pg, z, shift, shift_mul);

  /* Use split Estrin scheme for P(z^2) with deg(P)=19.  */
  svfloat64_t z2 = svmul_x (pg, z, z);
  svfloat64_t z3 = svmul_x (pg, z2, z);
  svfloat64_t z4 = svmul_x (pg, z2, z2);
  svfloat64_t z8 = svmul_x (pg, z4, z4);
  svfloat64_t z16 = svmul_x (pg, z8, z8);

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

  svfloat64_t poly = svmla_x (pg, p07, z16, p819);

  /* y = shift + z + z^3 * P(z^2).  */
  svfloat64_t ret = svmla_x (pg, shift, z3, poly);

  /* Account for the sign of x and y.  */
  if (__glibc_unlikely (svptest_any (pg, cmp_xy)))
    return special_case (
	y, x,
	svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (ret), sign_xy)),
	cmp_xy);
  return svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (ret), sign_xy));
}
