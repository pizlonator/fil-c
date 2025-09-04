/* Double-precision vector (SVE) acosh function

   Copyright (C) 2024 Free Software Foundation, Inc.
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

#define One (0x3ff0000000000000)
#define Thres (0x1ff0000000000000) /* asuint64 (0x1p511) - One.  */

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t special)
{
  return sv_call_f64 (acosh, x, y, special);
}

/* SVE approximation for double-precision acosh, based on log1p.
   The largest observed error is 3.19 ULP in the region where the
   argument to log1p falls in the k=0 interval, i.e. x close to 1:
   SV_NAME_D1 (acosh)(0x1.1e4388d4ca821p+0) got 0x1.ed23399f5137p-2
					   want 0x1.ed23399f51373p-2.  */
svfloat64_t SV_NAME_D1 (acosh) (svfloat64_t x, const svbool_t pg)
{
  /* (ix - One) >= (BigBound - One).  */
  svuint64_t ix = svreinterpret_u64 (x);
  svbool_t special = svcmpge (pg, svsub_x (pg, ix, One), Thres);

  svfloat64_t xm1 = svsub_x (pg, x, 1.0);
  svfloat64_t u = svmul_x (pg, xm1, svadd_x (pg, x, 1.0));
  svfloat64_t y = svadd_x (pg, xm1, svsqrt_x (pg, u));

  /* Fall back to scalar routine for special lanes.  */
  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, sv_log1p_inline (y, pg), special);
  return sv_log1p_inline (y, pg);
}
