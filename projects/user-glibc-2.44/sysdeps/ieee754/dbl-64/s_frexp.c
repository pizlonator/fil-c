/* Copyright (C) 2011-2026 Free Software Foundation, Inc.
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

#include <inttypes.h>
#include <math.h>
#include <math_private.h>
#include <stdbit.h>
#include "math_config.h"
#include <libm-alias-double.h>

double
__frexp (double x, int *eptr)
{
  uint64_t ix = asuint64 (x);
  uint32_t ex = (ix >> MANTISSA_WIDTH) & 0x7ff;

  /* Fast path for normal numbers.  */
  if (__glibc_likely ((ex - 1) < 0x7fe))
    {
      int e = ex - EXPONENT_BIAS + 1;
      *eptr = e;
      return asdouble (ix - ((uint64_t) e << MANTISSA_WIDTH));
    }

  /* Handle zero, infinity, and NaN.  */
  if (__glibc_likely ((int64_t) (ix << 1) <= 0))
    {
      *eptr = 0;
      return x + x;
    }

  /* Subnormal.  */
  uint64_t sign = ix & SIGN_MASK;
  int lz = stdc_leading_zeros (ix << (64 - MANTISSA_WIDTH - 1));
  ix <<= lz;
  *eptr = -(EXPONENT_BIAS - 2) - lz;
  return asdouble ((ix & MANTISSA_MASK) | sign
		   | (((uint64_t) (EXPONENT_BIAS - 1)) << MANTISSA_WIDTH));
}
libm_alias_double (__frexp, frexp)
