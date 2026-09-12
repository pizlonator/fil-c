/* Return base-10 logarithm of 1 + X.
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
#include <math-underflow.h>

FLOAT
M_DECL_FUNC (__log10p1) (FLOAT x)
{
  if (__glibc_unlikely (islessequal (x, M_LIT (-1.0))))
    {
      if (x == -1)
	__set_errno (ERANGE);
      else
	__set_errno (EDOM);
    }
  if (isless (M_FABS (x), M_EPSILON / 4))
    {
      /* Ensure appropriate underflows (a wider range than for log1p,
	 with potential for zero results from nonzero arguments, in
	 which case errno should be set based on the result with any
	 excess range and precision removed) even if the result of
	 multiplying by M_MLIT (M_LOG10E) is exact.  */
      FLOAT ret = math_narrow_eval (M_MLIT (M_LOG10E) * x);
      math_check_force_underflow (ret);
      if (x != 0 && ret == 0)
	__set_errno (ERANGE);
      return ret;
    }
  return M_MLIT (M_LOG10E) * M_SUF (__log1p) (x);
}
declare_mgen_alias (__log10p1, log10p1);
