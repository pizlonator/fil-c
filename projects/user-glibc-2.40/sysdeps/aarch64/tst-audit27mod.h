/* Check LD_AUDIT for aarch64 specific ABI.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#ifndef _TST_AUDIT27MOD_H
#define _TST_AUDIT27MOD_H 1

#include <float.h>

#define FUNC_FLOAT_ARG0 FLT_MIN
#define FUNC_FLOAT_ARG1 FLT_MAX
#define FUNC_FLOAT_ARG2 FLT_EPSILON
#define FUNC_FLOAT_ARG3 FLT_TRUE_MIN
#define FUNC_FLOAT_ARG4 0.0f
#define FUNC_FLOAT_ARG5 1.0f
#define FUNC_FLOAT_ARG6 2.0f
#define FUNC_FLOAT_ARG7 3.0f
#define FUNC_FLOAT_RET  4.0f

float
tst_audit27_func_float (float a0, float a1, float a2, float a3, float a4,
			float a5, float a6, float a7);

#define FUNC_DOUBLE_ARG0 DBL_MIN
#define FUNC_DOUBLE_ARG1 DBL_MAX
#define FUNC_DOUBLE_ARG2 DBL_EPSILON
#define FUNC_DOUBLE_ARG3 DBL_TRUE_MIN
#define FUNC_DOUBLE_ARG4 0.0
#define FUNC_DOUBLE_ARG5 1.0
#define FUNC_DOUBLE_ARG6 2.0
#define FUNC_DOUBLE_ARG7 3.0
#define FUNC_DOUBLE_RET  0x1.fffffe0000001p+127

double
tst_audit27_func_double (double a0, double a1, double a2, double a3, double a4,
			 double a5, double a6, double a7);

#define FUNC_LDOUBLE_ARG0 DBL_MAX + 1.0L
#define FUNC_LDOUBLE_ARG1 DBL_MAX + 2.0L
#define FUNC_LDOUBLE_ARG2 DBL_MAX + 3.0L
#define FUNC_LDOUBLE_ARG3 DBL_MAX + 4.0L
#define FUNC_LDOUBLE_ARG4 DBL_MAX + 5.0L
#define FUNC_LDOUBLE_ARG5 DBL_MAX + 6.0L
#define FUNC_LDOUBLE_ARG6 DBL_MAX + 7.0L
#define FUNC_LDOUBLE_ARG7 DBL_MAX + 8.0L
#define FUNC_LDOUBLE_RET  0x1.fffffffffffff000000000000001p+1023L

long double
tst_audit27_func_ldouble (long double a0, long double a1, long double a2,
			  long double a3, long double a4, long double a5,
			  long double a6, long double a7);

#endif
