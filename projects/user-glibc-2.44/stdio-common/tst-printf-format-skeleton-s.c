/* Test skeleton for formatted printf output for the 's' conversion.
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

#define MID_WIDTH 5
#define HUGE_WIDTH 10
#define REF_FMT "s"
#define REF_VAL(v) (v)
typedef const char *type_t;
static const type_t vals[] =
  { "", "The", "quick", "brown fox", "jumps over the lazy dog" };
static const char length[] = "";

#include "tst-printf-format-skeleton.c"
