/* Return minimum of X and Y.
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
M_DECL_FUNC (__fminimum) (FLOAT x, FLOAT y)
{
  if (__glibc_likely (!isunordered (x, y)))
    {
#if M_USE_BUILTIN (FMIN)
      return M_SUF (__builtin_fmin) (x, y);
#else
      if (isless (x, y))
	return x;
      else if (isgreater (x, y))
	return y;
      return signbit (x) ? x : y;
#endif
    }
  else
    return x + y;
}
declare_mgen_alias (__fminimum, fminimum);
