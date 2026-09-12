/* Optimized frexp implementation for binary32.
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
#include <stdbit.h>
#include "math_config.h"
#include <libm-alias-float.h>

float
__frexpf (float x, int *eptr)
{
  uint32_t hx = asuint (x);
  uint32_t ex = (hx >> MANTISSA_WIDTH) & 0xff;

  /* Fast path for normal numbers.  */
  if (__glibc_likely ((ex - 1) < 0xfe))
    {
      int e = ex - EXPONENT_BIAS + 1;
      *eptr = e;
      return asfloat (hx - (e << MANTISSA_WIDTH));
    }

  /* Handle zero, infinity, and NaN.  */
  if (__glibc_likely ((int32_t) (hx << 1) <= 0))
    {
      *eptr = 0;
      return x + x;
    }

  /* Subnormal.  */
  uint32_t sign = hx & SIGN_MASK;
  int lz = stdc_leading_zeros (hx << (32 - MANTISSA_WIDTH - 1));
  hx <<= lz;
  *eptr = -(EXPONENT_BIAS - 2) - lz;
  return asfloat ((hx & MANTISSA_MASK) | sign
		  | (((uint32_t) (EXPONENT_BIAS - 1)) << MANTISSA_WIDTH));
}
libm_alias_float (__frexp, frexp)
