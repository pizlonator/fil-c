/* Return X^Y, with special cases based on exp(Y*log(X)).
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
#include <math.h>
#include <math_private.h>

FLOAT
M_DECL_FUNC (__powr) (FLOAT x, FLOAT y)
{
  if (isless (x, M_LIT (0.0))
      || (x == M_LIT (0.0) && y == M_LIT (0.0))
      || (x == M_LIT (1.0) && isinf (y))
      || (isinf (x) && y == M_LIT (0.0)))
    {
      __set_errno (EDOM);
      return (x - x) / (x - x);
    }
  if (isnan (x) || isnan (y))
    return x + y;
  x = M_FABS (x);
  FLOAT ret = M_SUF (__ieee754_pow) (x, y);
  if (!isfinite (ret))
    {
      if (isfinite (x) && isfinite (y))
	__set_errno (ERANGE);
    }
  else if (ret == 0
	   && isfinite (x)
	   && x != 0
	   && isfinite (y))
    __set_errno (ERANGE);
  return ret;
}
declare_mgen_alias (__powr, powr);
