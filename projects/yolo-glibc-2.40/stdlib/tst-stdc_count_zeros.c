/* Test stdc_count_zeros functions and macros.
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
    { 0x2ULL, 7, 15, 31, 63 },
    { 0x3ULL, 6, 14, 30, 62 },
    { 0x4ULL, 7, 15, 31, 63 },
    { 0x5ULL, 6, 14, 30, 62 },
    { 0x6ULL, 6, 14, 30, 62 },
    { 0x7ULL, 5, 13, 29, 61 },
    { 0x8ULL, 7, 15, 31, 63 },
    { 0x9ULL, 6, 14, 30, 62 },
    { 0xaULL, 6, 14, 30, 62 },
    { 0xbULL, 5, 13, 29, 61 },
    { 0xcULL, 6, 14, 30, 62 },
    { 0xdULL, 5, 13, 29, 61 },
    { 0xeULL, 5, 13, 29, 61 },
    { 0xfULL, 4, 12, 28, 60 },
    { 0x10ULL, 7, 15, 31, 63 },
    { 0x11ULL, 6, 14, 30, 62 },
    { 0x12ULL, 6, 14, 30, 62 },
    { 0x1fULL, 3, 11, 27, 59 },
    { 0x20ULL, 7, 15, 31, 63 },
    { 0x7fULL, 1, 9, 25, 57 },
    { 0x80ULL, 7, 15, 31, 63 },
    { 0x81ULL, 6, 14, 30, 62 },
    { 0x9aULL, 4, 12, 28, 60 },
    { 0xf3ULL, 2, 10, 26, 58 },
    { 0xffULL, 0, 8, 24, 56 },
    { 0x100ULL, 8, 15, 31, 63 },
    { 0x101ULL, 7, 14, 30, 62 },
    { 0x102ULL, 7, 14, 30, 62 },
    { 0x1feULL, 1, 8, 24, 56 },
    { 0x1ffULL, 0, 7, 23, 55 },
    { 0x200ULL, 8, 15, 31, 63 },
    { 0x234ULL, 5, 12, 28, 60 },
    { 0x4567ULL, 3, 8, 24, 56 },
    { 0x7fffULL, 0, 1, 17, 49 },
    { 0x8000ULL, 8, 15, 31, 63 },
    { 0x8001ULL, 7, 14, 30, 62 },
    { 0xfffeULL, 1, 1, 17, 49 },
    { 0xffffULL, 0, 0, 16, 48 },
    { 0x10000ULL, 8, 16, 31, 63 },
    { 0x10001ULL, 7, 15, 30, 62 },
    { 0xfedcba98ULL, 5, 8, 12, 44 },
    { 0xfffffefeULL, 1, 2, 2, 34 },
    { 0xffffffffULL, 0, 0, 0, 32 },
    { 0x100000000ULL, 8, 16, 32, 63 },
    { 0x100000001ULL, 7, 15, 31, 62 },
    { 0x123456789ULL, 5, 8, 18, 49 },
    { 0x123456789abcdefULL, 1, 4, 12, 32 },
    { 0x789abcdef0123456ULL, 4, 9, 19, 32 },
    { 0x8000000000000000ULL, 8, 16, 32, 63 },
    { 0x8000000000000001ULL, 7, 15, 31, 62 },
    { 0xfffffffffffffffeULL, 1, 1, 1, 1 },
    { 0xffffffffffffffffULL, 0, 0, 0, 0 },
  };

TEST_STDBIT_UI_TOPLEVEL (stdc_count_zeros);

static int
do_test (void)
{
  TEST_STDBIT_UI (stdc_count_zeros, inputs);
  return 0;
}

#include <support/test-driver.c>
