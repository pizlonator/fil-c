/* Remainder function, float version.
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
#include <libm-alias-float.h>
#include <math-svid-compat.h>
#include "math_config.h"

float
__ieee754_remainderf(float x, float p)
{
  uint32_t hx = asuint (x);
  uint32_t hp = asuint (p);
  uint32_t sx = hx >> 31;

  hp &= ~SIGN_MASK;
  hx &= ~SIGN_MASK;

  /* |y| < DBL/MAX / 2 ? */
  p = fabsf (p);
  if (__glibc_likely (hp < 0x7f000000))
    {
      x = fabs (__fmodf (x, p + p)); /* now x < 2p */
      if (isgreater (x + x, p))
	{
	  x -= p;
	  if (x + x >= p)
	    x -= p;
	  /* Make sure x is not -0. This can occur only when x = p
	     and rounding direction is towards negative infinity. */
	  else if (x == 0.0f)
	    x = 0.0;
	}
    }
  else
    {
      /* |x| not finite or |y| is NaN */
      if (__glibc_unlikely (hx >= EXPONENT_MASK || hp > EXPONENT_MASK))
	return __math_invalidf (x * p);

      x = fabsf (x);
      float p_half = 0.5f * p;
      if (x > p_half)
	{
	  x -= p;
	  if (x >= p_half)
	    x -= p;
	  else if (x == 0.0f)
	    x = 0.0f;
	}
    }

  return sx ? -x : x;
}
libm_alias_finite (__ieee754_remainderf, __remainderf)
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __ieee754_remainderf, remainderf, GLIBC_2_43);
libm_alias_float_other (__ieee754_remainder, remainder)
#else
libm_alias_float (__ieee754_remainder, remainder)
weak_alias (__ieee754_remainderf, dremf)
#endif
