/* Fused multiply-add of long double (ldbl-96) value, narrowing the result
   to double.
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

#define f32xfmaf64x __hide_f32xfmaf64x
#define f64fmaf64x __hide_f64fmaf64x
#include <math.h>
#undef f32xfmaf64x
#undef f64fmaf64x

#include <math-narrow.h>

double
__dfmal (long double x, long double y, long double z)
{
  NARROW_FMA_ROUND_TO_ODD (x, y, z, double, union ieee854_long_double, l,
			   mantissa1, false);
}
libm_alias_double_ldouble (fma)
