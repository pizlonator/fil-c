/* Single-precision vector (SVE) asinh function

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

#define BigBound 0x5f800000 /* asuint(0x1p64).  */

static svfloat32_t NOINLINE
special_case (svfloat32_t ax, svfloat32_t y, svuint32_t sign, svbool_t pg,
	      svbool_t special, const struct sv_log1pf_data *d)
{
  /* For very large inputs (x > 2^64), asinh(x) ≈ ln(2x).
     In this range the +sqrt(x^2+1) term is negligible, so we compute
     asinh(x) as ln(x) + ln(2) later in this function.  */
  svfloat32_t log_ax = sv_log1pf_inline (ax, special);

  /* The only special cases that need considering are infinity and NaNs since
     0 will be handled by other calculations.  */
  svfloat32_t inf = svreinterpret_f32 (sv_u32 (d->inf));
  svbool_t is_inf = svcmpeq (special, ax, inf);
  svbool_t is_nan = svcmpne (special, ax, ax);
  svfloat32_t inf_ln2 = svsel (is_inf, inf, sv_f32 (d->ln2));
  svfloat32_t inf_nan_ln2
      = svsel (is_nan, svreinterpret_f32 (sv_u32 (d->nan)), inf_ln2);
  svfloat32_t asinh_x_res = svadd_x (special, log_ax, inf_nan_ln2);

  /* Now select (based on special) between x and y to change the type and,
     return either the positive or negative value, considering the input and
     its sign.  */
  svfloat32_t result = svsel (special, asinh_x_res, y);
  svuint32_t result_uint = svreinterpret_u32 (result);
  return svreinterpret_f32 (sveor_m (pg, result_uint, sign));
}

/* Single-precision SVE asinh(x) routine. Implements the same algorithm as
   vector asinhf and log1p.

   Maximum error is 1.92 ULPs:
   SV_NAME_F1 (asinh) (-0x1.0922ecp-1) got -0x1.fd0bccp-2
				      want -0x1.fd0bc8p-2.  */
svfloat32_t SV_NAME_F1 (asinh) (svfloat32_t x, const svbool_t pg)
{
  const struct sv_log1pf_data *d = ptr_barrier (&sv_log1pf_data);

  svfloat32_t ax = svabs_x (pg, x);
  svuint32_t iax = svreinterpret_u32 (ax);
  svuint32_t sign = sveor_x (pg, svreinterpret_u32 (x), iax);
  svbool_t special = svcmpge (pg, iax, BigBound);

  /* asinh(x) = log(x + sqrt(x * x + 1)).
     For positive x, asinh(x) = log1p(x + x * x / (1 + sqrt(x * x + 1))).  */
  svfloat32_t ax2 = svmul_x (pg, ax, ax);
  svfloat32_t dx = svadd_x (pg, svsqrt_x (pg, svadd_x (pg, ax2, 1.0f)), 1.0f);
  svfloat32_t y
      = sv_log1pf_inline (svadd_x (pg, ax, svdiv_x (pg, ax2, dx)), pg);

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (ax, y, sign, pg, special, d);
  return svreinterpret_f32 (svorr_x (pg, sign, svreinterpret_u32 (y)));
}
