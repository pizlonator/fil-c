/* Single-precision vector (SVE) atanh function

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

#include "sv_log1pf_inline.h"

#define One (0x3f800000)
#define Half (0x3f000000)

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  return sv_call_f32 (atanhf, x, y, special);
}

/* Approximation for vector single-precision atanh(x) using modified log1p.
   The maximum error is 2.28 ULP:
   _ZGVsMxv_atanhf(0x1.ff1194p-5) got 0x1.ffbbbcp-5
				 want 0x1.ffbbb6p-5.  */
svfloat32_t SV_NAME_F1 (atanh) (svfloat32_t x, const svbool_t pg)
{
  svfloat32_t ax = svabs_x (pg, x);
  svuint32_t iax = svreinterpret_u32 (ax);
  svuint32_t sign = sveor_x (pg, svreinterpret_u32 (x), iax);
  svfloat32_t halfsign = svreinterpret_f32 (svorr_x (pg, sign, Half));
  svbool_t special = svcmpge (pg, iax, One);

  /* Computation is performed based on the following sequence of equality:
   * (1+x)/(1-x) = 1 + 2x/(1-x).  */
  svfloat32_t y = svadd_x (pg, ax, ax);
  y = svdiv_x (pg, y, svsub_x (pg, sv_f32 (1), ax));
  /* ln((1+x)/(1-x)) = ln(1+2x/(1-x)) = ln(1 + y).  */
  y = sv_log1pf_inline (y, pg);

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, svmul_x (pg, halfsign, y), special);

  return svmul_x (pg, halfsign, y);
}
