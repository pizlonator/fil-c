/* Test and measure mempcpy functions.
   Copyright (C) 1999-2024 Free Software Foundation, Inc.
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

#define MEMCPY_RESULT(dst, len) (dst) + (len)
#define MIN_PAGE_SIZE 131072
#define TEST_MAIN
#define TEST_NAME "mempcpy"
#include "test-string.h"

IMPL (mempcpy, 1)

/* Naive implementation to verify results.  */
char *
simple_mempcpy (char *dst, const char *src, size_t n)
{
  while (n--)
    *dst++ = *src++;
  return dst;
}

#include "test-memcpy.c"
