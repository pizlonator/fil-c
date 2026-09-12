/* Single-precision vector (SVE) hypot function

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

#define TinyBound 0x0c800000 /* asuint (0x1p-102).  */
#define Thres 0x73000000     /* 0x70000000 - TinyBound.  */

static svfloat32_t NOINLINE
special_case (svfloat32_t sqsum, svfloat32_t x, svfloat32_t y, svbool_t pg,
	      svbool_t special)
{
  return sv_call2_f32 (hypotf, x, y, svsqrt_x (pg, sqsum), special);
}

/* SVE implementation of single-precision hypot.
   Maximum error observed is 1.21 ULP:
   _ZGVsMxvv_hypotf (0x1.6a213cp-19, -0x1.32b982p-26) got 0x1.6a2346p-19
						     want 0x1.6a2344p-19.  */
svfloat32_t SV_NAME_F2 (hypot) (svfloat32_t x, svfloat32_t y,
				const svbool_t pg)
{
  svfloat32_t sqsum = svmla_x (pg, svmul_x (pg, x, x), y, y);

  svbool_t special = svcmpge (
      pg, svsub_x (pg, svreinterpret_u32 (sqsum), TinyBound), Thres);

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (sqsum, x, y, pg, special);

  return svsqrt_x (pg, sqsum);
}
