/* Return arc cosine of X, divided by pi.
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

FLOAT
M_DECL_FUNC (__acospi) (FLOAT x)
{
  if (__glibc_unlikely (isgreater (M_FABS (x), M_LIT (1.0))))
    {
      __set_errno (EDOM);
      return (x - x) / (x - x);
    }
  FLOAT ret = M_SUF (__ieee754_acos) (x) / M_MLIT (M_PI);
  /* Ensure that rounding upward for both acos and the division cannot
     yield a return value from acospi greater than 1.  */
  return isgreater (ret, M_LIT (1.0)) ? M_LIT (1.0) : ret;
}
declare_mgen_alias (__acospi, acospi);
