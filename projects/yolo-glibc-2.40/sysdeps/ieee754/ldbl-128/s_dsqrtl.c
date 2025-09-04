/* Square root of long double (ldbl-128) value, narrowing the result to double.
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

#define f32xsqrtf64x __hide_f32xsqrtf64x
#define f32xsqrtf128 __hide_f32xsqrtf128
#define f64sqrtf64x __hide_f64sqrtf64x
#define f64sqrtf128 __hide_f64sqrtf128
#include <math.h>
#undef f32xsqrtf64x
#undef f32xsqrtf128
#undef f64sqrtf64x
#undef f64sqrtf128

#include <math-narrow.h>

double
__dsqrtl (_Float128 x)
{
  NARROW_SQRT_ROUND_TO_ODD (x, double, union ieee854_long_double, l,
			    mantissa3);
}
libm_alias_double_ldouble (sqrt)
