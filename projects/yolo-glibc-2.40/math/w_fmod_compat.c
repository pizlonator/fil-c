/* Copyright (C) 2011-2024 Free Software Foundation, Inc.
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

#if LIBM_SVID_COMPAT && SHLIB_COMPAT (libm, GLIBC_2_0, GLIBC_2_38)
/* wrapper fmod */
double
__fmod_compat (double x, double y)
{
  if (__builtin_expect (isinf (x) || y == 0.0, 0)
      && _LIB_VERSION != _IEEE_ && !isnan (y) && !isnan (x))
    /* fmod(+-Inf,y) or fmod(x,0) */
    return __kernel_standard (x, y, 27);

  return __ieee754_fmod (x, y);
}
compat_symbol (libm, __fmod_compat, fmod, GLIBC_2_0);
# ifdef NO_LONG_DOUBLE
weak_alias (__fmod_compat, fmodl)
# endif
# ifdef LONG_DOUBLE_COMPAT
LONG_DOUBLE_COMPAT_CHOOSE_libm_fmodl (
  compat_symbol (libm, __fmod_compat, fmodl, FIRST_VERSION_libm_fmodl), );
# endif
#endif
