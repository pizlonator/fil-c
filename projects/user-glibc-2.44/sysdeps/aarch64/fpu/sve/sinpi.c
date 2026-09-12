/* Double-precision (SVE) sinpi function

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
#include "poly_sve_f64.h"

static const struct data
{
  double poly[10], range_val;
} data = {
  /* Polynomial coefficients generated using Remez algorithm,
     see sinpi.sollya for details.  */
  .poly = { 0x1.921fb54442d184p1, -0x1.4abbce625be53p2, 0x1.466bc6775ab16p1,
	    -0x1.32d2cce62dc33p-1, 0x1.507834891188ep-4, -0x1.e30750a28c88ep-8,
	    0x1.e8f48308acda4p-12, -0x1.6fc0032b3c29fp-16,
	    0x1.af86ae521260bp-21, -0x1.012a9870eeb7dp-25 },
  .range_val = 0x1p63,
};

/* A fast SVE implementation of sinpi.
   Maximum error 3.10 ULP:
   _ZGVsMxv_sinpi(0x1.df1a14f1b235p-2) got 0x1.fd64f541606cp-1
				      want 0x1.fd64f541606c3p-1.  */
svfloat64_t SV_NAME_D1 (sinpi) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* range reduction into -1/2 .. 1/2)
     with n = rint(x) and r = r - n.  */
  svfloat64_t n = svrinta_x (pg, x);
  svfloat64_t r = svsub_x (pg, x, n);

  /* Result should be negated based on if n is odd or not.  */
  svbool_t cmp = svaclt (pg, x, d->range_val);
  svuint64_t intn = svreinterpret_u64 (svcvt_s64_z (pg, n));
  svuint64_t sign = svlsl_z (cmp, intn, 63);

  /* y = sin(r).  */
  svfloat64_t r2 = svmul_x (pg, r, r);
  svfloat64_t r4 = svmul_x (pg, r2, r2);
  svfloat64_t y = sv_pw_horner_9_f64_x (pg, r2, r4, d->poly);
  y = svmul_x (pg, y, r);

  return svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (y), sign));
}
