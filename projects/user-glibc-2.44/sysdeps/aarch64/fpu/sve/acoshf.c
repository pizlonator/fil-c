/* Single-precision vector (SVE) acosh function

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

#include "sv_math.h"
#include "sv_log1pf_inline.h"

#define One (0x3f800000U)
#define Thres (0x20000000U) /* asuint(0x1p64) - One.  */

/* Acosh is defined on [1, inf). Its formula can be re-written knowing that 1
   becomes negligible when x is a very large number. So for special numbers,
   where x >= 2^64, acosh ~= ln(2x). But, ln(2x) = ln(2) + ln(x) and below we
   calculate ln(x) and then add ln(2) to the result.

   Right before returning we check if x is infinity or if x is lower than 1,
   in which case we return infinity or NaN.  */
static inline svfloat32_t special_case (svfloat32_t x, svfloat32_t xm1,
					svfloat32_t y, svbool_t pg,
					svbool_t special,
					const struct sv_log1pf_data *d)
{
  svfloat32_t logv = sv_log1pf_inline (svsel (special, xm1, y), pg);
  svfloat32_t result = svadd_m (special, logv, sv_f32 (d->ln2));

  /* Catch x<1, and x==inf.
     Also catch x==nan using negation of fp comparison.  */
  svbool_t is_x_ge1 = svcmpge (special, xm1, 0.0f);
  svbool_t is_x_pinf
      = svcmpeq (special, x, svreinterpret_f32 (sv_u32 (d->inf)));

  svbool_t res_is_inf_nan = svorn_b_z (special, is_x_pinf, is_x_ge1);

  svuint32_t inf_or_nan = svsel (is_x_pinf, sv_u32 (d->inf), sv_u32 (d->nan));

  return svsel (res_is_inf_nan, svreinterpret_f32 (inf_or_nan), result);
}
/* Single-precision SVE acosh(x) routine. Implements the same algorithm as
   vector acoshf and log1p.

   Maximum error is 2.47 ULPs:
   SV_NAME_F1 (acosh) (0x1.01ca76p+0) got 0x1.e435a6p-4
				     want 0x1.e435a2p-4.  */
svfloat32_t SV_NAME_F1 (acosh) (svfloat32_t x, const svbool_t pg)
{
  const struct sv_log1pf_data *d = ptr_barrier (&sv_log1pf_data);

  svuint32_t ix = svreinterpret_u32 (x);
  svbool_t special = svcmpge (pg, svsub_x (pg, ix, One), Thres);

  svfloat32_t xm1 = svsub_x (pg, x, 1.0f);
  svfloat32_t u = svmul_x (pg, xm1, svadd_x (pg, x, 1.0f));
  svfloat32_t y = svadd_x (pg, xm1, svsqrt_x (pg, u));

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, xm1, y, pg, special, d);
  return sv_log1pf_inline (y, pg);
}
