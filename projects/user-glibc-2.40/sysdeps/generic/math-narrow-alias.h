/* Alias macros for functions returning a narrower type.
   Copyright (C) 2018-2024 Free Software Foundation, Inc.
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

#ifndef	_MATH_NARROW_ALIAS_H
#define	_MATH_NARROW_ALIAS_H	1

#include <bits/floatn.h>
#include <bits/long-double.h>

/* The following macros declare aliases for a narrowing function.  The
   sole argument is the base name of a family of functions, such as
   "add".  If any platform changes long double format after the
   introduction of narrowing functions, in a way requiring symbol
   versioning compatibility, additional variants of these macros will
   be needed.  */

#define libm_alias_float_double_main(func)	\
  weak_alias (__f ## func, f ## func)		\
  weak_alias (__f ## func, f32 ## func ## f64)	\
  weak_alias (__f ## func, f32 ## func ## f32x)

#ifdef NO_LONG_DOUBLE
# define libm_alias_float_double(func)		\
  libm_alias_float_double_main (func)		\
  weak_alias (__f ## func, f ## func ## l)
#else
# define libm_alias_float_double(func)		\
  libm_alias_float_double_main (func)
#endif

#define libm_alias_float32x_float64_main(func)			\
  weak_alias (__f32x ## func ## f64, f32x ## func ## f64)

#ifdef NO_LONG_DOUBLE
# define libm_alias_float32x_float64(func)		\
  libm_alias_float32x_float64_main (func)		\
  weak_alias (__f32x ## func ## f64, d ## func ## l)
#elif defined __LONG_DOUBLE_MATH_OPTIONAL
# define libm_alias_float32x_float64(func)			\
  libm_alias_float32x_float64_main (func)			\
  weak_alias (__f32x ## func ## f64, __nldbl_d ## func ## l)
#else
# define libm_alias_float32x_float64(func)	\
  libm_alias_float32x_float64_main (func)
#endif

#if __HAVE_FLOAT128 && !__HAVE_DISTINCT_FLOAT128
# define libm_alias_float_ldouble_f128(func)		\
  weak_alias (__f ## func ## l, f32 ## func ## f128)
# define libm_alias_double_ldouble_f128(func)		\
  weak_alias (__d ## func ## l, f32x ## func ## f128)	\
  weak_alias (__d ## func ## l, f64 ## func ## f128)
#else
# define libm_alias_float_ldouble_f128(func)
# define libm_alias_double_ldouble_f128(func)
#endif

#if __HAVE_FLOAT64X_LONG_DOUBLE
# define libm_alias_float_ldouble_f64x(func)		\
  weak_alias (__f ## func ## l, f32 ## func ## f64x)
# define libm_alias_double_ldouble_f64x(func)		\
  weak_alias (__d ## func ## l, f32x ## func ## f64x)	\
  weak_alias (__d ## func ## l, f64 ## func ## f64x)
#else
# define libm_alias_float_ldouble_f64x(func)
# define libm_alias_double_ldouble_f64x(func)
#endif

#define libm_alias_float_ldouble(func)		\
  weak_alias (__f ## func ## l, f ## func ## l) \
  libm_alias_float_ldouble_f128 (func)		\
  libm_alias_float_ldouble_f64x (func)

#define libm_alias_double_ldouble(func)		\
  weak_alias (__d ## func ## l, d ## func ## l) \
  libm_alias_double_ldouble_f128 (func)		\
  libm_alias_double_ldouble_f64x (func)

#define libm_alias_float64x_float128(func)			\
  weak_alias (__f64x ## func ## f128, f64x ## func ## f128)

#define libm_alias_float32_float128_main(func)			\
  weak_alias (__f32 ## func ## f128, f32 ## func ## f128)

#define libm_alias_float64_float128_main(func)			\
  weak_alias (__f64 ## func ## f128, f64 ## func ## f128)	\
  weak_alias (__f64 ## func ## f128, f32x ## func ## f128)

#include <math-narrow-alias-float128.h>

/* The following macros declare narrowing-named aliases for a
   non-narrowing function.  */

#define libm_alias_double_narrow_main(from, to)	\
  weak_alias (from, f32x ## to ## f64)

#ifdef NO_LONG_DOUBLE
# define libm_alias_double_narrow(from, to)	\
  libm_alias_double_narrow_main (from, to)	\
  weak_alias (from, d ## to ## l)
#else
# define libm_alias_double_narrow(from, to)	\
  libm_alias_double_narrow_main (from, to)
#endif

#if __HAVE_FLOAT64X_LONG_DOUBLE && __HAVE_FLOAT128 && !__HAVE_DISTINCT_FLOAT128
# define libm_alias_ldouble_narrow(from, to)	\
  weak_alias (from ## l, f64x ## to ## f128)
#else
# define libm_alias_ldouble_narrow(from, to)
#endif

#if __HAVE_DISTINCT_FLOAT128 && __HAVE_FLOAT64X && !__HAVE_FLOAT64X_LONG_DOUBLE
# define libm_alias_float128_narrow(from, to)	\
  weak_alias (from ## f128, f64x ## to ## f128)
#else
# define libm_alias_float128_narrow(from, to)
#endif

#endif /* math-narrow-alias.h.  */
