/* Return maximum numeric value of X and Y.
   Copyright (C) 1997-2026 Free Software Foundation, Inc.
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

FLOAT
M_DECL_FUNC (__fmax) (FLOAT x, FLOAT y)
{
#if M_USE_BUILTIN (FMAX)
  return M_SUF (__builtin_fmax) (x, y);
#else
  if (__glibc_likely (!isunordered (x, y)))
    {
      if (__glibc_unlikely (x == y))
	return signbit (x) ? y : x;
      return x > y ? x : y;
    }
  else if (issignaling (x) || issignaling (y))
    return x + y;
  else
    return isnan (y) ? x : y;
#endif
}

declare_mgen_alias (__fmax, fmax);
