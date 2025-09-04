/* Test stdc_first_leading_one functions and macros.
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
    { 0ULL, 0, 0, 0, 0 },
    { 0x1ULL, 8, 16, 32, 64 },
    { 0x2ULL, 7, 15, 31, 63 },
    { 0x3ULL, 7, 15, 31, 63 },
    { 0x4ULL, 6, 14, 30, 62 },
    { 0x5ULL, 6, 14, 30, 62 },
    { 0x6ULL, 6, 14, 30, 62 },
    { 0x7ULL, 6, 14, 30, 62 },
    { 0x8ULL, 5, 13, 29, 61 },
    { 0x9ULL, 5, 13, 29, 61 },
    { 0xaULL, 5, 13, 29, 61 },
    { 0xbULL, 5, 13, 29, 61 },
    { 0xcULL, 5, 13, 29, 61 },
    { 0xdULL, 5, 13, 29, 61 },
    { 0xeULL, 5, 13, 29, 61 },
    { 0xfULL, 5, 13, 29, 61 },
    { 0x10ULL, 4, 12, 28, 60 },
    { 0x11ULL, 4, 12, 28, 60 },
    { 0x12ULL, 4, 12, 28, 60 },
    { 0x1fULL, 4, 12, 28, 60 },
    { 0x20ULL, 3, 11, 27, 59 },
    { 0x7fULL, 2, 10, 26, 58 },
    { 0x80ULL, 1, 9, 25, 57 },
    { 0x81ULL, 1, 9, 25, 57 },
    { 0x9aULL, 1, 9, 25, 57 },
    { 0xf3ULL, 1, 9, 25, 57 },
    { 0xffULL, 1, 9, 25, 57 },
    { 0x100ULL, 0, 8, 24, 56 },
    { 0x101ULL, 8, 8, 24, 56 },
    { 0x102ULL, 7, 8, 24, 56 },
    { 0x1feULL, 1, 8, 24, 56 },
    { 0x1ffULL, 1, 8, 24, 56 },
    { 0x200ULL, 0, 7, 23, 55 },
    { 0x234ULL, 3, 7, 23, 55 },
    { 0x4567ULL, 2, 2, 18, 50 },
    { 0x7fffULL, 1, 2, 18, 50 },
    { 0x8000ULL, 0, 1, 17, 49 },
    { 0x8001ULL, 8, 1, 17, 49 },
    { 0xfffeULL, 1, 1, 17, 49 },
    { 0xffffULL, 1, 1, 17, 49 },
    { 0x10000ULL, 0, 0, 16, 48 },
    { 0x10001ULL, 8, 16, 16, 48 },
    { 0xfedcba98ULL, 1, 1, 1, 33 },
    { 0xfffffefeULL, 1, 1, 1, 33 },
    { 0xffffffffULL, 1, 1, 1, 33 },
    { 0x100000000ULL, 0, 0, 0, 32 },
    { 0x100000001ULL, 8, 16, 32, 32 },
    { 0x123456789ULL, 1, 2, 3, 32 },
    { 0x123456789abcdefULL, 1, 1, 1, 8 },
    { 0x789abcdef0123456ULL, 2, 3, 1, 2 },
    { 0x8000000000000000ULL, 0, 0, 0, 1 },
    { 0x8000000000000001ULL, 8, 16, 32, 1 },
    { 0xfffffffffffffffeULL, 1, 1, 1, 1 },
    { 0xffffffffffffffffULL, 1, 1, 1, 1 },
  };

TEST_STDBIT_UI_TOPLEVEL (stdc_first_leading_one);

static int
do_test (void)
{
  TEST_STDBIT_UI (stdc_first_leading_one, inputs);
  return 0;
}

#include <support/test-driver.c>
