/* Test skeleton for formatted printf output for unsigned char conversions.
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
#define HUGE_WIDTH 4
#define REF_FMT "u"
#define REF_VAL(v) ((v) & 0xff)
typedef unsigned int type_t;
static const type_t vals[] =
  { 0, 1, 42, UCHAR_MAX, UCHAR_MAX + 1, UCHAR_MAX + 42 };
static const char length[] = "hh";

#include "tst-printf-format-skeleton.c"
