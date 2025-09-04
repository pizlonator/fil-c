/* Compute radix independent exponent.
   Copyright (C) 2011-2024 Free Software Foundation, Inc.
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
#include <libm-alias-double.h>
#include <fix-int-fp-convert-zero.h>
#include <math-use-builtins.h>

double
__logb (double x)
{
#if USE_LOGB_BUILTIN
  return __builtin_logb (x);
#else
  int64_t ix, ex;

  EXTRACT_WORDS64 (ix, x);
  ix &= UINT64_C(0x7fffffffffffffff);
  if (ix == 0)
    return -1.0 / fabs (x);
  ex = ix >> 52;
  if (ex == 0x7ff)
    return x * x;
  if (__glibc_unlikely (ex == 0))
    {
      int m = __builtin_clzll (ix);
      ex -= m - 12;
    }
  if (FIX_INT_FP_CONVERT_ZERO && ex == 1023)
    return 0.0;
  return (double) (ex - 1023);
#endif /* !USE_LOGB_BUILTIN  */
}
#ifndef __logb
libm_alias_double (__logb, logb)
#endif
