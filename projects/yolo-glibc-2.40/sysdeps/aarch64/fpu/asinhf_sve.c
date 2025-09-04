/* Single-precision vector (SVE) asinh function

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

#include "sv_math.h"
#include "sv_log1pf_inline.h"

#define BigBound (0x5f800000)  /* asuint(0x1p64).  */

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  return sv_call_f32 (asinhf, x, y, special);
}

/* Single-precision SVE asinh(x) routine. Implements the same algorithm as
   vector asinhf and log1p.

   Maximum error is 2.48 ULPs:
   SV_NAME_F1 (asinh) (0x1.008864p-3) got 0x1.ffbbbcp-4
				     want 0x1.ffbbb8p-4.  */
svfloat32_t SV_NAME_F1 (asinh) (svfloat32_t x, const svbool_t pg)
{
  svfloat32_t ax = svabs_x (pg, x);
  svuint32_t iax = svreinterpret_u32 (ax);
  svuint32_t sign = sveor_x (pg, svreinterpret_u32 (x), iax);
  svbool_t special = svcmpge (pg, iax, BigBound);

  /* asinh(x) = log(x + sqrt(x * x + 1)).
     For positive x, asinh(x) = log1p(x + x * x / (1 + sqrt(x * x + 1))).  */
  svfloat32_t ax2 = svmul_x (pg, ax, ax);
  svfloat32_t d = svadd_x (pg, svsqrt_x (pg, svadd_x (pg, ax2, 1.0f)), 1.0f);
  svfloat32_t y
      = sv_log1pf_inline (svadd_x (pg, ax, svdiv_x (pg, ax2, d)), pg);

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (
	x, svreinterpret_f32 (svorr_x (pg, sign, svreinterpret_u32 (y))),
	special);
  return svreinterpret_f32 (svorr_x (pg, sign, svreinterpret_u32 (y)));
}
