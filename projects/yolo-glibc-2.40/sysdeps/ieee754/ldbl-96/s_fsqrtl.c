/* Square root of long double (ldbl-96) value, narrowing the result to float.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#define f32sqrtf64x __hide_f32sqrtf64x
#include <math.h>
#undef f32sqrtf64x

#include <math-narrow.h>

float
__fsqrtl (long double x)
{
  NARROW_SQRT_ROUND_TO_ODD (x, float, union ieee854_long_double, l,
			    mantissa1);
}
libm_alias_float_ldouble (sqrt)
