/* Test stdc_leading_zeros functions and macros.
   Copyright (C) 2024 Free Software Foundation, Inc.
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

#include <tst-stdbit.h>

static const struct stdbit_test inputs[] =
  {
    { 0ULL, 8, 16, 32, 64 },
    { 0x1ULL, 7, 15, 31, 63 },
    { 0x2ULL, 6, 14, 30, 62 },
    { 0x3ULL, 6, 14, 30, 62 },
    { 0x4ULL, 5, 13, 29, 61 },
    { 0x5ULL, 5, 13, 29, 61 },
    { 0x6ULL, 5, 13, 29, 61 },
    { 0x7ULL, 5, 13, 29, 61 },
    { 0x8ULL, 4, 12, 28, 60 },
    { 0x9ULL, 4, 12, 28, 60 },
    { 0xaULL, 4, 12, 28, 60 },
    { 0xbULL, 4, 12, 28, 60 },
    { 0xcULL, 4, 12, 28, 60 },
    { 0xdULL, 4, 12, 28, 60 },
    { 0xeULL, 4, 12, 28, 60 },
    { 0xfULL, 4, 12, 28, 60 },
    { 0x10ULL, 3, 11, 27, 59 },
    { 0x11ULL, 3, 11, 27, 59 },
    { 0x12ULL, 3, 11, 27, 59 },
    { 0x1fULL, 3, 11, 27, 59 },
    { 0x20ULL, 2, 10, 26, 58 },
    { 0x7fULL, 1, 9, 25, 57 },
    { 0x80ULL, 0, 8, 24, 56 },
    { 0x81ULL, 0, 8, 24, 56 },
    { 0x9aULL, 0, 8, 24, 56 },
    { 0xf3ULL, 0, 8, 24, 56 },
    { 0xffULL, 0, 8, 24, 56 },
    { 0x100ULL, 8, 7, 23, 55 },
    { 0x101ULL, 7, 7, 23, 55 },
    { 0x102ULL, 6, 7, 23, 55 },
    { 0x1feULL, 0, 7, 23, 55 },
    { 0x1ffULL, 0, 7, 23, 55 },
    { 0x200ULL, 8, 6, 22, 54 },
    { 0x234ULL, 2, 6, 22, 54 },
    { 0x4567ULL, 1, 1, 17, 49 },
    { 0x7fffULL, 0, 1, 17, 49 },
    { 0x8000ULL, 8, 0, 16, 48 },
    { 0x8001ULL, 7, 0, 16, 48 },
    { 0xfffeULL, 0, 0, 16, 48 },
    { 0xffffULL, 0, 0, 16, 48 },
    { 0x10000ULL, 8, 16, 15, 47 },
    { 0x10001ULL, 7, 15, 15, 47 },
    { 0xfedcba98ULL, 0, 0, 0, 32 },
    { 0xfffffefeULL, 0, 0, 0, 32 },
    { 0xffffffffULL, 0, 0, 0, 32 },
    { 0x100000000ULL, 8, 16, 32, 31 },
    { 0x100000001ULL, 7, 15, 31, 31 },
    { 0x123456789ULL, 0, 1, 2, 31 },
    { 0x123456789abcdefULL, 0, 0, 0, 7 },
    { 0x789abcdef0123456ULL, 1, 2, 0, 1 },
    { 0x8000000000000000ULL, 8, 16, 32, 0 },
    { 0x8000000000000001ULL, 7, 15, 31, 0 },
    { 0xfffffffffffffffeULL, 0, 0, 0, 0 },
    { 0xffffffffffffffffULL, 0, 0, 0, 0 },
  };

TEST_STDBIT_UI_TOPLEVEL (stdc_leading_zeros);

static int
do_test (void)
{
  TEST_STDBIT_UI (stdc_leading_zeros, inputs);
  return 0;
}

#include <support/test-driver.c>
