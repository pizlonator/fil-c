/* Return arc tangent of Y/X, divided by pi.
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
#include <math-narrow-eval.h>
#include <math_private.h>
#include <math-underflow.h>

FLOAT
M_DECL_FUNC (__atan2pi) (FLOAT y, FLOAT x)
{
  FLOAT ret = math_narrow_eval (M_SUF (__ieee754_atan2) (y, x)
				/ M_MLIT (M_PI));
  if (__glibc_likely (!isnan (ret)))
    math_check_force_underflow (ret);
  if (ret == 0 && y != 0 && isfinite (x))
    __set_errno (ERANGE);
  /* Ensure that rounding away from zero for both atan2 and the
     division cannot yield a return value from atan2pi with absolute
     value greater than 1.  */
  return (isgreater (M_FABS (ret), M_LIT (1.0))
	  ? M_COPYSIGN (M_LIT (1.0), ret)
	  : ret);
}
declare_mgen_alias (__atan2pi, atan2pi);
