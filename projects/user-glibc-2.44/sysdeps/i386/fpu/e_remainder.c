/* Floating-point remainder function.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
#include <libm-alias-double.h>
#include "math_config.h"

double
__remainder (double x, double y)
{
  uint64_t hx = asuint64 (x);
  uint64_t hy = asuint64 (y);

  /* fmod(+-Inf,y) or fmod(x,0) */
  if (__glibc_unlikely ((is_inf (hx) || y == 0.0)
			&& !is_nan (hy)
			&& !is_nan (hx)))
    return __math_invalid (x);

  return remainder_inline (x, y);
}
strong_alias (__remainder, __ieee754_remainder)
versioned_symbol (libm, __remainder, remainder, GLIBC_2_43);
libm_alias_double_other (__remainder, remainder)
libm_alias_finite (__ieee754_remainder, __remainder)
