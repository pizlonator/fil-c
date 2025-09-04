/* Measure stpcpy checking functions.
   Copyright (C) 2013-2024 Free Software Foundation, Inc.
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

#define STRCPY_RESULT(dst, len) ((dst) + (len))
#define TEST_MAIN
#define TEST_NAME "stpcpy_chk"
#include "bench-string.h"

extern void __attribute__ ((noreturn)) __chk_fail (void);
extern char *normal_stpcpy (char *, const char *, size_t)
  __asm ("stpcpy");
extern char *__stpcpy_chk (char *, const char *, size_t);

IMPL (normal_stpcpy, 1)
IMPL (__stpcpy_chk, 2)

#include "bench-strcpy_chk.c"
