/* Test skeleton for formatted printf output for signed char conversions.
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

#include <limits.h>

#define MID_WIDTH 3
#define HUGE_WIDTH 5
#define REF_FMT "i"
#define REF_VAL(v) ((((v) & 0xff) ^ 0x80) - 0x80)
typedef int type_t;
static const type_t vals[] =
  { SCHAR_MIN - 123, SCHAR_MIN - 1, SCHAR_MIN, -123, -1, 0, 1, 42, SCHAR_MAX,
    SCHAR_MAX + 1, SCHAR_MAX + 42 };
static const char length[] = "hh";

#include "tst-printf-format-skeleton.c"
