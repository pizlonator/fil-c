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

#include <fenv.h>
#include <math.h>
#include <math_private.h>
#include <math-svid-compat.h>
#include <libm-alias-float.h>
#include <shlib-compat.h>


#if LIBM_SVID_COMPAT && SHLIB_COMPAT (libm, GLIBC_2_0, GLIBC_2_43)
/* wrapper j0f */
float
__j0f_svid (float x)
{
  if (__builtin_expect (isgreater (fabsf (x),
				   AS_FLOAT_CONSTANT (X_TLOSS)), 0)
      && _LIB_VERSION != _IEEE_ && _LIB_VERSION != _POSIX_)
    /* j0(|x|>X_TLOSS) */
    return __kernel_standard_f (x, x, 134);

  return __ieee754_j0f (x);
}
compat_symbol (libm, __j0f_svid, j0f, GLIBC_2_0);


/* wrapper y0f */
float
__y0f_svid (float x)
{
  if (__builtin_expect (islessequal (x, 0.0f)
                        || isgreater (x, AS_FLOAT_CONSTANT (X_TLOSS)), 0)
      && _LIB_VERSION != _IEEE_)
    {
      if (x < 0.0f)
	{
	  /* d = zero/(x-x) */
	  feraiseexcept (FE_INVALID);
	  return __kernel_standard_f (x, x, 109);
	}
      else if (x == 0.0f)
	{
	  /* d = -one/(x-x) */
	  feraiseexcept (FE_DIVBYZERO);
	  return __kernel_standard_f (x, x, 108);
	}
      else if (_LIB_VERSION != _POSIX_)
	/* y0(x>X_TLOSS) */
	return __kernel_standard_f (x, x, 135);
    }

  return __ieee754_y0f (x);
}
compat_symbol (libm, __y0f_svid, y0f, GLIBC_2_0);
#endif
