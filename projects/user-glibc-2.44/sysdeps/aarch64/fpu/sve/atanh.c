/* Double-precision vector (SVE) atanh function

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

#define WANT_SV_LOG1P_K0_SHORTCUT 0
#include "sv_log1p_inline.h"

static const struct data
{
  uint64_t half;
  double inf;
  double nan;
} data = { .half = 0x3fe0000000000000, .inf = INFINITY, .nan = NAN };

static svfloat64_t NOINLINE
special_case (svfloat64_t ax, svfloat64_t y, svbool_t pg, svbool_t special,
	      svfloat64_t halfsign, const struct data *d)
{
  svfloat64_t res = svsel (special, sv_f64 (d->nan), y);
  res = svsel (svcmpeq (special, ax, sv_f64 (1.0)), sv_f64 (d->inf), res);
  return svmul_x (pg, res, halfsign);
}

/* SVE approximation for double-precision atanh, based on log1p.
   The greatest observed error is 3.3 ULP:
   _ZGVsMxv_atanh(0x1.ffae6288b601p-6) got 0x1.ffd8ff31b5019p-6
				      want 0x1.ffd8ff31b501cp-6.  */
svfloat64_t SV_NAME_D1 (atanh) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat64_t ax = svabs_x (pg, x);
  svuint64_t iax = svreinterpret_u64 (ax);
  svuint64_t sign = sveor_x (pg, svreinterpret_u64 (x), iax);
  svfloat64_t halfsign = svreinterpret_f64 (svorr_x (pg, sign, d->half));

  /* It is special if iax >= 1.  */
  svbool_t special = svacge (pg, ax, 1.0);

  /* Computation is performed based on the following sequence of equality:
	(1+x)/(1-x) = 1 + 2x/(1-x).  */
  svfloat64_t y;
  y = svadd_x (pg, ax, ax);
  y = svdiv_x (pg, y, svsub_x (pg, sv_f64 (1.0), ax));
  /* ln((1+x)/(1-x)) = ln(1+2x/(1-x)) = ln(1 + y).  */
  y = sv_log1p_inline (y, pg);

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (ax, y, pg, special, halfsign, d);
  return svmul_x (pg, halfsign, y);
}
