/* Test skeleton for formatted printf output for long double conversions.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <float.h>
#include <math.h>
#include <support/test-driver.h>

#define MID_WIDTH 20
#define HUGE_WIDTH 4950
#define REF_FMT ".35Le"
#define REF_VAL(v) (v)
#define PREC LDBL_MANT_DIG
typedef long double type_t;
static const type_t vals[] =
  { -HUGE_VAL, -LDBL_MAX, -LDBL_MIN, -0.0, -NAN, NAN, 0, LDBL_MIN,
    LDBL_MAX, HUGE_VAL };
static const char length[] = "L";

#ifndef TIMEOUT
# define TIMEOUT (DEFAULT_TIMEOUT * 64)
#endif

#include "tst-printf-format-skeleton.c"
