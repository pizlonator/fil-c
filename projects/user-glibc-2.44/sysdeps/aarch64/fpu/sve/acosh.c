/* Double-precision vector (SVE) acosh function

   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#define WANT_SV_LOG1P_K0_SHORTCUT 1
#include "sv_log1p_inline.h"

#define One (0x3ff0000000000000ULL)
#define Thres (0x1ff0000000000000ULL)

const static struct data
{
  double ln2;
} data = { .ln2 = 0x1.62e42fefa39efp-1 };

/* Acosh is define on [1, inf). Its formula can be re-written knowing that 1
   becomes negligible when x is a very large number. So for special numbers,
   where x >= 2^511, acosh ~= ln(2x). But, ln(2x) = ln(2) + ln(x) and below we
   calculate ln(x) and then add ln(2) to the result.

   Right before returning we check if x is infinity or if x is lower than 1,
   in which case we return infinity or NaN.  */
static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t xm1, svfloat64_t y, svbool_t special,
	      svbool_t pg, const struct data *d)
{
  svfloat64_t inf = sv_f64 (INFINITY);
  svfloat64_t nan = sv_f64 (NAN);
  svfloat64_t log = sv_log1p_inline (svsel (special, xm1, y), pg);
  svfloat64_t result = svadd_m (special, log, sv_f64 (d->ln2));
  svbool_t is_inf_nan = svorr_b_z (special, svcmpge (special, x, inf),
				   svcmplt (special, x, sv_f64 (1.0)));
  svfloat64_t inf_or_nan = svsel (svcmpge (special, x, inf), inf, nan);
  return svsel (is_inf_nan, inf_or_nan, result);
}

/* SVE approximation for double-precision acosh, based on log1p.
   The largest observed error is 3.14 ULP in the region where the
   argument to log1p falls in the k=0 interval, i.e. x close to 1:
   SV_NAME_D1 (acosh)(0x1.1e80ed12f0ad1p+0) got 0x1.ef0cee7c33ce1p-2
					   want 0x1.ef0cee7c33ce4p-2.  */
svfloat64_t SV_NAME_D1 (acosh) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* (ix - One) >= (BigBound - One).  */
  svuint64_t ix = svreinterpret_u64 (x);
  svbool_t special = svcmpge (pg, svsub_x (pg, ix, One), Thres);

  svfloat64_t xm1 = svsub_x (pg, x, 1.0);
  svfloat64_t u = svmul_x (pg, xm1, svadd_x (pg, x, 1.0));
  svfloat64_t y = svadd_x (pg, xm1, svsqrt_x (pg, u));

  /* Fall back to scalar routine for special lanes.  */
  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, xm1, y, special, pg, d);
  return sv_log1p_inline (y, pg);
}
