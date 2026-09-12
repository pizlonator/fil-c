/* Return exp10(X) - 1.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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
#include <math-underflow.h>

FLOAT
M_DECL_FUNC (__exp10m1) (FLOAT x)
{
  if (isgreaterequal (x, M_LIT (-0.5)) && islessequal (x, M_LIT (0.5)))
    {
      FLOAT ret = M_SUF (__expm1) (M_MLIT (M_LN10) * x);
      math_check_force_underflow (ret);
      return ret;
    }
  else if (isgreater (x, M_MANT_DIG / 3 + M_LIT (2.0)))
    {
      FLOAT ret = M_SUF (__ieee754_exp10) (x);
      if (!isfinite (ret) && isfinite (x))
	__set_errno (ERANGE);
      return ret;
    }
  else if (isless (x, -(M_MANT_DIG / 3) - M_LIT (2.0)))
    return M_LIT (-1.0);
  else
    return M_SUF (__ieee754_exp10) (x) - M_LIT (1.0);
}
declare_mgen_alias (__exp10m1, exp10m1);
