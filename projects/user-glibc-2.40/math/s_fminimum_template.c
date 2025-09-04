/* Return minimum of X and Y.
   Copyright (C) 1997-2024 Free Software Foundation, Inc.
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
  if (isless (x, y))
    return x;
  else if (isgreater (x, y))
    return y;
  else if (x == y)
    return (M_COPYSIGN (1, x) <= M_COPYSIGN (1, y) ? x : y);
  else
    return x + y;
}
declare_mgen_alias (__fminimum, fminimum);
