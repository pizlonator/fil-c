/* Return arc sine of X, divided by pi.
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
M_DECL_FUNC (__asinpi) (FLOAT x)
{
  if (__glibc_unlikely (!islessequal (M_FABS (x), M_LIT (1.0))))
    {
      if (!isnan (x))
	__set_errno (EDOM);
      return (x - x) / (x - x);
    }
  FLOAT ret = math_narrow_eval (M_SUF (__ieee754_asin) (x) / M_MLIT (M_PI));
  math_check_force_underflow (ret);
  if (x != 0 && ret == 0)
    __set_errno (ERANGE);
  /* Ensure that rounding away from zero for both asin and the
     division cannot yield a return value from asinpi with absolute
     value greater than 0.5.  */
  return (isgreater (M_FABS (ret), M_LIT (0.5))
	  ? M_COPYSIGN (M_LIT (0.5), ret)
	  : ret);
}
declare_mgen_alias (__asinpi, asinpi);
