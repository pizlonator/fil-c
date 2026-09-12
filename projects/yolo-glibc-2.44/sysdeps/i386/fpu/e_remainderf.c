/* Floating-point remainder function.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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
#include <libm-alias-finite.h>
#include <libm-alias-float.h>
#include "sysdeps/ieee754/flt-32/math_config.h"

float
__remainderf (float x, float y)
{
  uint32_t hx = asuint (x);
  uint32_t hy = asuint (y);

  /* fmod(+-Inf,y) or fmod(x,0) */
  if (__glibc_unlikely ((is_inf (hx) || y == 0.0f)
			&& !is_nan (hy)
			&& !is_nan (hx)))
    return __math_invalidf (x);

  return remainderf_inline (x, y);
}
strong_alias (__remainderf, __ieee754_remainderf)
versioned_symbol (libm, __remainderf, remainderf, GLIBC_2_43);
libm_alias_float_other (__remainder, remainder)
libm_alias_finite (__ieee754_remainderf, __remainderf)
