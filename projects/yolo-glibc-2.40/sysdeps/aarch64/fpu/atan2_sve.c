/* Double-precision SVE atan2

   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
#include "poly_sve_f64.h"

static const struct data
{
  float64_t poly[20];
  float64_t pi_over_2;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2**-1022, 1.0].  */
  .poly = { -0x1.5555555555555p-2,  0x1.99999999996c1p-3, -0x1.2492492478f88p-3,
            0x1.c71c71bc3951cp-4,   -0x1.745d160a7e368p-4, 0x1.3b139b6a88ba1p-4,
            -0x1.11100ee084227p-4,  0x1.e1d0f9696f63bp-5, -0x1.aebfe7b418581p-5,
            0x1.842dbe9b0d916p-5,   -0x1.5d30140ae5e99p-5, 0x1.338e31eb2fbbcp-5,
            -0x1.00e6eece7de8p-5,   0x1.860897b29e5efp-6, -0x1.0051381722a59p-6,
            0x1.14e9dc19a4a4ep-7,  -0x1.d0062b42fe3bfp-9, 0x1.17739e210171ap-10,
            -0x1.ab24da7be7402p-13, 0x1.358851160a528p-16, },
  .pi_over_2 = 0x1.921fb54442d18p+0,
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
   x are reasonably close together. The greatest observed error is 2.28 ULP:
   _ZGVsMxvv_atan2 (-0x1.5915b1498e82fp+732, 0x1.54d11ef838826p+732)
   got -0x1.954f42f1fa841p-1 want -0x1.954f42f1fa843p-1.  */
svfloat64_t SV_NAME_D2 (atan2) (svfloat64_t y, svfloat64_t x, const svbool_t pg)
{
  const struct data *data_ptr = ptr_barrier (&data);

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

  /* Set up z for call to atan.  */
  svfloat64_t n = svsel (pred_aygtax, svneg_x (pg, ax), ay);
  svfloat64_t d = svsel (pred_aygtax, ay, ax);
  svfloat64_t z = svdiv_x (pg, n, d);

  /* Work out the correct shift.  */
  svfloat64_t shift = svreinterpret_f64 (svlsr_x (pg, sign_x, 1));
  shift = svsel (pred_aygtax, sv_f64 (1.0), shift);
  shift = svreinterpret_f64 (svorr_x (pg, sign_x, svreinterpret_u64 (shift)));
  shift = svmul_x (pg, shift, data_ptr->pi_over_2);

  /* Use split Estrin scheme for P(z^2) with deg(P)=19.  */
  svfloat64_t z2 = svmul_x (pg, z, z);
  svfloat64_t x2 = svmul_x (pg, z2, z2);
  svfloat64_t x4 = svmul_x (pg, x2, x2);
  svfloat64_t x8 = svmul_x (pg, x4, x4);

  svfloat64_t ret = svmla_x (
      pg, sv_estrin_7_f64_x (pg, z2, x2, x4, data_ptr->poly),
      sv_estrin_11_f64_x (pg, z2, x2, x4, x8, data_ptr->poly + 8), x8);

  /* y = shift + z + z^3 * P(z^2).  */
  svfloat64_t z3 = svmul_x (pg, z2, z);
  ret = svmla_x (pg, z, z3, ret);

  ret = svadd_m (pg, ret, shift);

  /* Account for the sign of x and y.  */
  if (__glibc_unlikely (svptest_any (pg, cmp_xy)))
    return special_case (
	y, x,
	svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (ret), sign_xy)),
	cmp_xy);
  return svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (ret), sign_xy));
}
