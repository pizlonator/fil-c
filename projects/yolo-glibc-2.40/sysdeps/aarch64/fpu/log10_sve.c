/* Double-precision vector (SVE) log10 function

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
#include "poly_sve_f64.h"

#define Min 0x0010000000000000
#define Max 0x7ff0000000000000
#define Thres 0x7fe0000000000000 /* Max - Min.  */
#define Off 0x3fe6900900000000
#define N (1 << V_LOG10_TABLE_BITS)

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t special)
{
  return sv_call_f64 (log10, x, y, special);
}

/* SVE log10 algorithm.
   Maximum measured error is 2.46 ulps.
   SV_NAME_D1 (log10)(0x1.131956cd4b627p+0) got 0x1.fffbdf6eaa669p-6
					   want 0x1.fffbdf6eaa667p-6.  */
svfloat64_t SV_NAME_D1 (log10) (svfloat64_t x, const svbool_t pg)
{
  svuint64_t ix = svreinterpret_u64 (x);
  svbool_t special = svcmpge (pg, svsub_x (pg, ix, Min), Thres);

  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  svuint64_t tmp = svsub_x (pg, ix, Off);
  svuint64_t i = svlsr_x (pg, tmp, 51 - V_LOG10_TABLE_BITS);
  i = svand_x (pg, i, (N - 1) << 1);
  svfloat64_t k = svcvt_f64_x (pg, svasr_x (pg, svreinterpret_s64 (tmp), 52));
  svfloat64_t z = svreinterpret_f64 (
      svsub_x (pg, ix, svand_x (pg, tmp, 0xfffULL << 52)));

  /* log(x) = k*log(2) + log(c) + log(z/c).  */
  svfloat64_t invc = svld1_gather_index (pg, &__v_log10_data.table[0].invc, i);
  svfloat64_t logc
      = svld1_gather_index (pg, &__v_log10_data.table[0].log10c, i);

  /* We approximate log(z/c) with a polynomial P(x) ~= log(x + 1):
     r = z/c - 1 (we look up precomputed 1/c)
     log(z/c) ~= P(r).  */
  svfloat64_t r = svmad_x (pg, invc, z, -1.0);

  /* hi = log(c) + k*log(2).  */
  svfloat64_t w = svmla_x (pg, logc, r, __v_log10_data.invln10);
  svfloat64_t hi = svmla_x (pg, w, k, __v_log10_data.log10_2);

  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  svfloat64_t r2 = svmul_x (pg, r, r);
  svfloat64_t y = sv_pw_horner_4_f64_x (pg, r, r2, __v_log10_data.poly);

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, svmla_x (svnot_z (pg, special), hi, r2, y),
			 special);
  return svmla_x (pg, hi, r2, y);
}
