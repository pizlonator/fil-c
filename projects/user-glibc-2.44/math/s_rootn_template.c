/* Return the Yth root of X for integer Y.
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
#include <limits.h>
#include <math.h>
#include <math-narrow-eval.h>
#include <math_private.h>


FLOAT
M_DECL_FUNC (__rootn) (FLOAT x, long long int y)
{
  if (y == 0)
    {
      /* This is a domain error even if X is a NaN.  */
      __set_errno (EDOM);
      return M_LIT (0.0) / M_LIT (0.0);
    }
  if (isnan (x))
    return x + x;
  if (x < 0 && (y & 1) == 0)
    {
      __set_errno (EDOM);
      return (x - x) / (x - x);
    }
  if (isinf (x))
    /* If X is negative, then Y is odd.  */
    return y > 0 ? x : M_LIT (1.0) / x;
  if (x == M_LIT (0.0))
    {
      if (y > 0)
	return (y & 1) == 0 ? M_LIT (0.0) : x;
      else
	{
	  __set_errno (ERANGE);
	  return M_LIT (1.0) / ((y & 1) == 0 ? M_LIT (0.0) : x);
	}
    }
  if (y == 1)
    return x;
  if (y == -1)
    {
      /* Overflow is possible in this case (and underflow, though not
	 underflow to zero).  */
      FLOAT ret = math_narrow_eval (M_LIT (1.0) / x);
      if (isinf (ret))
	__set_errno (ERANGE);
      return ret;
    }
  /* Now X is finite and no overflow or underflow (or results even
     close to overflowing or underflowing) is possible.  If X is
     negative, then Y is odd; the result should have the sign of X.  */
  if (y >= 4 * M_MAX_EXP || y <= -4 * M_MAX_EXP)
    /* No extra precision is needed in computing the exponent; it is
       OK if Y cannot be exactly represented in type FLOAT.  */
    return M_COPYSIGN (M_SUF (__ieee754_pow) (M_FABS (x), M_LIT (1.0) / y), x);
  /* Compute 1 / Y with extra precision.  Y can be exactly represented
     in type FLOAT.  */
  FLOAT qhi, qlo;
  qhi = math_narrow_eval (M_LIT (1.0) / y);
  qlo = M_SUF (fma) (-qhi, y, 1.0) / y;
  return M_COPYSIGN (M_SUF (__ieee754_pow) (M_FABS (x), qhi)
		     * M_SUF (__ieee754_pow) (M_FABS (x), qlo), x);
}
declare_mgen_alias (__rootn, rootn);
