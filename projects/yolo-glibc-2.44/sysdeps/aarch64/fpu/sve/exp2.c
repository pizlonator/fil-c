/* Double-precision vector (SVE) exp2 function

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
#include "sv_exp_special_inline.h"

/* Value of |x| above which scale overflows without special treatment.
   log2(2^(1022 + 1/128)) ~ 1022.00.  */
#define SpecialBound 0x1.ff01p+9

/* Value of n above which scale overflows even with special treatment.  */
#define ScaleBound 1280

static const struct data
{
  double c2, c4;
  double c0, c1, c3;
  double shift, special_bound;
  struct sv_exp_special_data special_data;
} data = {
  .special_data = SV_EXP_SPECIAL_DATA,
  /* Coefficients are computed using Remez algorithm with
     minimisation of the absolute error.  */
  .c0 = 0x1.62e42fefa39efp-1,
  .c1 = 0x1.ebfbdff82a31bp-3,
  .c2 = 0x1.c6b08d706c8a5p-5,
  .c3 = 0x1.3b2ad2ff7d2f3p-7,
  .c4 = 0x1.5d8761184beb3p-10,
  .shift = 0x1.800000000ffc0p+46,
  .special_bound = SpecialBound,
};

static svfloat64_t NOINLINE
special_exp (svfloat64_t poly, svfloat64_t scale, svfloat64_t n, svfloat64_t z,
	     const struct sv_exp_special_data *ds)
{
  /* FEXPA zeroes the sign bit, however the sign is meaningful to the
  special case function so needs to be copied.
  e = sign bit of u << 46.  */
  svuint64_t u = svreinterpret_u64 (z);
  svuint64_t e = svand_x (svptrue_b64 (), svlsl_x (svptrue_b64 (), u, 46),
			  0x8000000000000000);
  /* Copy sign to scale.  */
  scale = svreinterpret_f64 (
      svadd_x (svptrue_b64 (), e, svreinterpret_u64 (scale)));
  return special_case (scale, poly, n, ds);
}

/* Fast vector implementation of exp2.
   Maximum measured error is 0.52 + 0.5 ulp.
   _ZGVsMxv_exp2 (0x1.3b72ad5b701bfp-1) got 0x1.8861641b49e08p+0
				       want 0x1.8861641b49e07p+0.  */
svfloat64_t SV_NAME_D1 (exp2) (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat64_t z = svadd_x (svptrue_b64 (), x, d->shift);
  svfloat64_t n = svsub_x (pg, z, d->shift);
  svfloat64_t r = svsub_x (pg, x, n);

  svfloat64_t scale = svexpa (svreinterpret_u64 (z));

  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t c24 = svld1rq (svptrue_b64 (), &d->c2);

  /* Approximate exp2(r) using polynomial.  */
  /* y = exp2(r) - 1 ~= r * (C0 + C1 r + C2 r^2 + C3 r^3 + C4 r^4).  */
  svfloat64_t p12 = svmla_lane (sv_f64 (d->c1), r, c24, 0);
  svfloat64_t p34 = svmla_lane (sv_f64 (d->c3), r, c24, 1);
  svfloat64_t p = svmla_x (pg, p12, p34, r2);
  p = svmad_x (pg, p, r, d->c0);
  svfloat64_t poly = svmul_x (svptrue_b64 (), r, p);

  svbool_t special = svacge (svptrue_b64 (), x, d->special_bound);
  /* Assemble result as exp(x) = 2^n * exp(r).  If |x| > Thresh the
     multiplication may overflow, so use special case routine.  */
  if (__glibc_unlikely (svptest_any (special, special)))
    return special_exp (poly, scale, n, z, &d->special_data);

  return svmla_x (pg, scale, scale, poly);
}
