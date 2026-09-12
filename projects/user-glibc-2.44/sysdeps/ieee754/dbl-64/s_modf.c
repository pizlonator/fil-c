/* Extract signed integral and fractional values.
   Copyright (C) 1993-2026 Free Software Foundation, Inc.
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
#include <libm-alias-double.h>
#include "math_config.h"

double
__modf (double x, double *iptr)
{
  uint64_t t = asuint64 (x);
#if USE_TRUNC_BUILTIN
# ifndef TRUNC
#  define TRUNC trunc
# endif
  if (is_inf (t))
    {
      *iptr = x;
      return copysign (0.0, x);
    }
  *iptr = TRUNC (x);
  return copysign (x - *iptr, x);
#else
  int e = get_exponent (t);
  /* No fraction part.  */
  if (e < MANTISSA_WIDTH)
    {
      if (e < 0)
	{
	  /* |x|<1 -> *iptr = +-0 */
	  *iptr = asdouble (t & SIGN_MASK);
	  return x;
	}

      uint64_t i = MANTISSA_MASK >> e;
      if ((t & i) == 0)
	{
	  /* x in integral, return +-0  */
	  *iptr = x;
	  return asdouble (t & SIGN_MASK);
	}

      *iptr = asdouble (t & ~i);
      return x - *iptr;
    }

  /* Set invalid operation for sNaN.  */
  *iptr = x * 1.0;
  if ((e == 0x400) && (t & MANTISSA_MASK))
    return *iptr;
  return asdouble (t & SIGN_MASK);
#endif
}
#ifndef __modf
libm_alias_double (__modf, modf)
#endif
