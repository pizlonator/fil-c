/* Return base-2 logarithm of 1 + X.
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
#include <math-underflow.h>

FLOAT
M_DECL_FUNC (__log2p1) (FLOAT x)
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
      /* Avoid spurious underflows when log1p underflows but log2p1
	 should not.  */
      FLOAT ret = M_MLIT (M_LOG2E) * x;
      math_check_force_underflow (ret);
      return ret;
    }
  return M_MLIT (M_LOG2E) * M_SUF (__log1p) (x);
}
declare_mgen_alias (__log2p1, log2p1);
