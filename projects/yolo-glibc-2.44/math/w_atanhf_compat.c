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
#include <libm-alias-float.h>


#if LIBM_SVID_COMPAT && SHLIB_COMPAT (libm, GLIBC_2_0, GLIBC_2_43)
/* wrapper atanhf */
float
__atanh_compatf (float x)
{
  if (__builtin_expect (isgreaterequal (fabsf (x), 1.0f), 0)
      && _LIB_VERSION != _IEEE_)
    return __kernel_standard_f (x, x,
				fabsf (x) > 1.0f
				? 130		/* atanh(|x|>1) */
				: 131);		/* atanh(|x|==1) */

  return __ieee754_atanhf (x);
}
# ifdef NO_COMPAT_NEEDED
strong_alias (__atanh_compatf, __atanhf)
libm_alias_float (__atanh_compat, atanh)
# else
compat_symbol (libm, __atanh_compatf, atanhf, GLIBC_2_0);
# endif
#endif
