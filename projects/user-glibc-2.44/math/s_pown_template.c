/* Return X^Y for integer Y.
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

#include <errno.h>
#include <fenv_private.h>
#include <limits.h>
#include <math.h>
#include <math-barriers.h>
#include <math-narrow-eval.h>
#include <math_private.h>
#include <stdlib.h>

FLOAT
M_DECL_FUNC (__pown) (FLOAT x, long long int y)
{
  FLOAT ret;
#if M_MANT_DIG >= LLONG_WIDTH - 1
  ret = M_SUF (__ieee754_pow) (x, y);
#else
  if ((y <= 1LL << M_MANT_DIG
       && y >= -(1LL << M_MANT_DIG))
      || y == LLONG_MIN)
    ret = M_SUF (__ieee754_pow) (x, y);
  else
    {
      {
	/* To avoid problems with overflow and underflow from
	   intermediate computations occurring in the wrong directed
	   rounding mode (when X is negative and Y is odd), do these
	   computations in round-to-nearest mode and correct
	   overflowing and underflowing results afterwards.  */
	M_SET_RESTORE_ROUND (FE_TONEAREST);
	ret = M_LIT (1.0);
	while (y != 0)
	  {
	    long long int absy = llabs (y);
	    int bits = LLONG_WIDTH - __builtin_clzll (absy);
	    if (bits <= M_MANT_DIG)
	      {
		ret *= M_SUF (__ieee754_pow) (x, y);
		y = 0;
	      }
	    else
	      {
		long long int absy_high
		  = absy & (((1ULL << M_MANT_DIG) - 1) << (bits - M_MANT_DIG));
		long long int absy_low = absy - absy_high;
		long long int y_high = y < 0 ? -absy_high : absy_high;
		ret *= M_SUF (__ieee754_pow) (x, y_high);
		y = y < 0 ? -absy_low : absy_low;
	      }
	  }
	ret = math_narrow_eval (ret);
	math_force_eval (ret);
      }
      if (isfinite (x) && x != M_LIT (0.0))
	{
	  if (isinf (ret))
	    ret = math_narrow_eval (M_COPYSIGN (M_MAX, ret) * M_MAX);
	  else if (ret == M_LIT (0.0))
	    ret = math_narrow_eval (M_COPYSIGN (M_MIN, ret) * M_MIN);
	}
    }
#endif
  if (!isfinite (ret))
    {
      if (isfinite (x))
	__set_errno (ERANGE);
    }
  else if (ret == 0
	   && isfinite (x)
	   && x != 0)
    __set_errno (ERANGE);
  return ret;
}
declare_mgen_alias (__pown, pown);
