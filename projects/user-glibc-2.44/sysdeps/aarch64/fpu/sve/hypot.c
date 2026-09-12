/* Double-precision vector (SVE) hypot function

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

static const struct data
{
  uint64_t tiny_bound, thres;
} data = {
  .tiny_bound = 0x0c80000000000000, /* asuint (0x1p-102).  */
  .thres = 0x7300000000000000,	    /* asuint (inf) - tiny_bound.  */
};

static svfloat64_t NOINLINE
special_case (svfloat64_t sqsum, svfloat64_t x, svfloat64_t y, svbool_t pg,
	      svbool_t special)
{
  return sv_call2_f64 (hypot, x, y, svsqrt_x (pg, sqsum), special);
}

/* SVE implementation of double-precision hypot.
   Maximum error observed is 1.21 ULP:
   _ZGVsMxvv_hypot (-0x1.6a22d0412cdd3p+352, 0x1.d3d89bd66fb1ap+330)
    got 0x1.6a22d0412cfp+352
   want 0x1.6a22d0412cf01p+352.  */
svfloat64_t SV_NAME_D2 (hypot) (svfloat64_t x, svfloat64_t y, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat64_t sqsum = svmla_x (pg, svmul_x (pg, x, x), y, y);

  svbool_t special = svcmpge (
      pg, svsub_x (pg, svreinterpret_u64 (sqsum), d->tiny_bound), d->thres);

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (sqsum, x, y, pg, special);
  return svsqrt_x (pg, sqsum);
}
