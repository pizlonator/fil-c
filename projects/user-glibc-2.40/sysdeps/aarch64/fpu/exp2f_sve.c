/* Single-precision vector (SVE) exp2 function

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

#include "sv_math.h"
#include "poly_sve_f32.h"

#define Thres 0x1.5d5e2ap+6f

static const struct data
{
  float poly[5];
  float shift, thres;
} data = {
  /* Coefficients copied from the polynomial in AdvSIMD variant, reversed for
     compatibility with polynomial helpers.  */
  .poly = { 0x1.62e422p-1f, 0x1.ebf9bcp-3f, 0x1.c6bd32p-5f, 0x1.3ce9e4p-7f,
	    0x1.59977ap-10f },
  /* 1.5*2^17 + 127.  */
  .shift = 0x1.903f8p17f,
  /* Roughly 87.3. For x < -Thres, the result is subnormal and not handled
     correctly by FEXPA.  */
  .thres = Thres,
};

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  return sv_call_f32 (exp2f, x, y, special);
}

/* Single-precision SVE exp2f routine. Implements the same algorithm
   as AdvSIMD exp2f.
   Worst case error is 1.04 ULPs.
   SV_NAME_F1 (exp2)(0x1.943b9p-1) got 0x1.ba7eb2p+0
				  want 0x1.ba7ebp+0.  */
svfloat32_t SV_NAME_F1 (exp2) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  /* exp2(x) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
    x = n + r, with r in [-1/2, 1/2].  */
  svfloat32_t shift = sv_f32 (d->shift);
  svfloat32_t z = svadd_x (pg, x, shift);
  svfloat32_t n = svsub_x (pg, z, shift);
  svfloat32_t r = svsub_x (pg, x, n);

  svbool_t special = svacgt (pg, x, d->thres);
  svfloat32_t scale = svexpa (svreinterpret_u32 (z));

  /* Polynomial evaluation: poly(r) ~ exp2(r)-1.
     Evaluate polynomial use hybrid scheme - offset ESTRIN by 1 for
     coefficients 1 to 4, and apply most significant coefficient directly.  */
  svfloat32_t r2 = svmul_x (pg, r, r);
  svfloat32_t p14 = sv_pairwise_poly_3_f32_x (pg, r, r2, d->poly + 1);
  svfloat32_t p0 = svmul_x (pg, r, d->poly[0]);
  svfloat32_t poly = svmla_x (pg, p0, r2, p14);

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, svmla_x (pg, scale, scale, poly), special);

  return svmla_x (pg, scale, scale, poly);
}
