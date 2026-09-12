/* Optimized frexp implementation for intel96 (x87 80-bit).
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <float.h>
#include <math.h>
#include <math_private.h>
#include <libm-alias-ldouble.h>

#define EXPONENT_BIAS 16383

static const long double two65 = 0x1p65L;

long double
__frexpl (long double x, int *eptr)
{
  uint32_t se, hx, lx;
  GET_LDOUBLE_WORDS (se, hx, lx, x);
  uint32_t ex = se & 0x7fff;

  /* Fast path for normal numbers.  */
  if (__glibc_likely ((ex - 1) < 0x7ffe))
    {
      ex -= EXPONENT_BIAS - 1;
      *eptr = ex;
      SET_LDOUBLE_EXP (x, se - ex);
      return x;
    }

  /* Handle zero, infinity, and NaN.  */
  if (__glibc_likely (ex == 0x7fff || (ex | hx | lx) == 0))
    {
      *eptr = 0;
      return x + x;
    }

  /* Subnormal.  */
  x *= two65;
  GET_LDOUBLE_EXP (se, x);

  ex = (se & 0x7fff) - EXPONENT_BIAS + 1;
  *eptr = ex - 65;
  SET_LDOUBLE_EXP (x, se - ex);
  return x;
}
libm_alias_ldouble (__frexp, frexp)
