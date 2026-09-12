/* Remainder function, double version.
   Copyright (C) 2008-2026 Free Software Foundation, Inc.
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
#include <libm-alias-finite.h>
#include <libm-alias-double.h>
#include <math-svid-compat.h>
#include "math_config.h"

double
__remainder (double x, double y)
{
  uint64_t hx = asuint64 (x);
  uint64_t hy = asuint64 (y);
  uint64_t sx = hx >> 63;

  hx &= ~SIGN_MASK;
  hy &= ~SIGN_MASK;

  /* |y| < DBL_MAX / 2 ? */
  y = fabs (y);
  if (__glibc_likely (hy < UINT64_C (0x7fe0000000000000)))
    {
      x = fabs (__fmod (x, y + y));
      if (isgreater (x + x, y))
	{
	  x -= y;
	  if (x + x >= y)
	    x -= y;
	  /* Make sure x is not -0. This can occur only when x = y
	     and rounding direction is towards negative infinity. */
	  else if (x == 0.0)
	    x = 0.0;
	}
    }
  else
    {
      /* |x| not finite or |y| is NaN */
      if (__glibc_unlikely (hx >= EXPONENT_MASK || hy > EXPONENT_MASK))
	return __math_invalid (x * y);

      x = fabs (x);
      double y_half = y * 0.5;
      if (x > y_half)
	{
	  x -= y;
	  if (x >= y_half)
	    x -= y;
	  else if (x == 0.0)
	    x = 0.0;
	}
    }

  return sx ? -x : x;
}
libm_alias_finite (__remainder, __remainder)
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __remainder, remainder, GLIBC_2_43);
libm_alias_double_other (__remainder, remainder)
#else
libm_alias_double (__remainder, remainder)
weak_alias (__remainder, drem)
# ifdef NO_LONG_DOUBLE
weak_alias (__remainder, dreml)
# endif
#endif
