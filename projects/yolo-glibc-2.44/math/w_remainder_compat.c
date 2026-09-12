/* Copyright (C) 2011-2026 Free Software Foundation, Inc.
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
#include <math_private.h>
#include <math-svid-compat.h>
#include <libm-alias-double.h>
#include <shlib-compat.h>


#if LIBM_SVID_COMPAT && (SHLIB_COMPAT (libm, GLIBC_2_0, GLIBC_2_43) \
			 || defined NO_LONG_DOUBLE \
			 || defined LONG_DOUBLE_COMPAT)
/* wrapper remainder */
double
__remainder_compat (double x, double y)
{
  if (((__builtin_expect (y == 0.0, 0) && ! isnan (x))
       || (__builtin_expect (isinf (x), 0) && ! isnan (y)))
      && _LIB_VERSION != _IEEE_)
    return __kernel_standard (x, y, 28); /* remainder domain */

  return __remainder (x, y);
}
compat_symbol (libm, __remainder_compat, remainder, GLIBC_2_0);
weak_alias (__remainder_compat, drem)
# ifdef NO_LONG_DOUBLE
weak_alias (__remainder_compat, dreml)
weak_alias (__remainder_compat, remainderl)
# endif
# ifdef LONG_DOUBLE_COMPAT
LONG_DOUBLE_COMPAT_CHOOSE_libm_remainderl (
  compat_symbol (libm, __remainder_compat, remainderl, \
		 FIRST_VERSION_libm_remainderl), );
# endif
#endif
