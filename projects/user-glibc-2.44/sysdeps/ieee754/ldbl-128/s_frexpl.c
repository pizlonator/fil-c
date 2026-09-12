/* Optimized frexp implementation for binary128 (IEEE quad precision).
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

#include <math.h>
#include <math_private.h>
#include <libm-alias-ldouble.h>

#define EXPONENT_BIAS 16383

static const _Float128 two114 = 0x1p114L;

_Float128
__frexpl (_Float128 x, int *eptr)
{
  uint64_t hx, lx;
  GET_LDOUBLE_WORDS64 (hx, lx, x);
  uint64_t ex = (hx >> 48) & 0x7fff;

  /* Fast path for normal numbers.  */
  if (__glibc_likely ((ex - 1U) < 0x7ffe))
    {
      ex -= EXPONENT_BIAS - 1;
      *eptr = ex;
      hx = hx - (ex << 48);
      SET_LDOUBLE_MSW64 (x, hx);
      return x;
    }

  /* Handle zero, infinity, and NaN.  */
  if (__glibc_likely (ex == 0x7fff || ((hx << 1) | lx) == 0))
    {
      *eptr = 0;
      return x + x;
    }

  /* Subnormal.  */
  x *= two114;
  GET_LDOUBLE_MSW64 (hx, x);
  ex = (hx >> 48) & 0x7fff;
  ex -= EXPONENT_BIAS - 1;
  *eptr = ex - 114;
  hx = hx - (ex << 48);
  SET_LDOUBLE_MSW64 (x, hx);
  return x;
}
libm_alias_ldouble (__frexp, frexp)
