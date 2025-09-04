/* Test stdc_count_ones functions and macros.
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
    { 0x1ULL, 1, 1, 1, 1 },
    { 0x2ULL, 1, 1, 1, 1 },
    { 0x3ULL, 2, 2, 2, 2 },
    { 0x4ULL, 1, 1, 1, 1 },
    { 0x5ULL, 2, 2, 2, 2 },
    { 0x6ULL, 2, 2, 2, 2 },
    { 0x7ULL, 3, 3, 3, 3 },
    { 0x8ULL, 1, 1, 1, 1 },
    { 0x9ULL, 2, 2, 2, 2 },
    { 0xaULL, 2, 2, 2, 2 },
    { 0xbULL, 3, 3, 3, 3 },
    { 0xcULL, 2, 2, 2, 2 },
    { 0xdULL, 3, 3, 3, 3 },
    { 0xeULL, 3, 3, 3, 3 },
    { 0xfULL, 4, 4, 4, 4 },
    { 0x10ULL, 1, 1, 1, 1 },
    { 0x11ULL, 2, 2, 2, 2 },
    { 0x12ULL, 2, 2, 2, 2 },
    { 0x1fULL, 5, 5, 5, 5 },
    { 0x20ULL, 1, 1, 1, 1 },
    { 0x7fULL, 7, 7, 7, 7 },
    { 0x80ULL, 1, 1, 1, 1 },
    { 0x81ULL, 2, 2, 2, 2 },
    { 0x9aULL, 4, 4, 4, 4 },
    { 0xf3ULL, 6, 6, 6, 6 },
    { 0xffULL, 8, 8, 8, 8 },
    { 0x100ULL, 0, 1, 1, 1 },
    { 0x101ULL, 1, 2, 2, 2 },
    { 0x102ULL, 1, 2, 2, 2 },
    { 0x1feULL, 7, 8, 8, 8 },
    { 0x1ffULL, 8, 9, 9, 9 },
    { 0x200ULL, 0, 1, 1, 1 },
    { 0x234ULL, 3, 4, 4, 4 },
    { 0x4567ULL, 5, 8, 8, 8 },
    { 0x7fffULL, 8, 15, 15, 15 },
    { 0x8000ULL, 0, 1, 1, 1 },
    { 0x8001ULL, 1, 2, 2, 2 },
    { 0xfffeULL, 7, 15, 15, 15 },
    { 0xffffULL, 8, 16, 16, 16 },
    { 0x10000ULL, 0, 0, 1, 1 },
    { 0x10001ULL, 1, 1, 2, 2 },
    { 0xfedcba98ULL, 3, 8, 20, 20 },
    { 0xfffffefeULL, 7, 14, 30, 30 },
    { 0xffffffffULL, 8, 16, 32, 32 },
    { 0x100000000ULL, 0, 0, 0, 1 },
    { 0x100000001ULL, 1, 1, 1, 2 },
    { 0x123456789ULL, 3, 8, 14, 15 },
    { 0x123456789abcdefULL, 7, 12, 20, 32 },
    { 0x789abcdef0123456ULL, 4, 7, 13, 32 },
    { 0x8000000000000000ULL, 0, 0, 0, 1 },
    { 0x8000000000000001ULL, 1, 1, 1, 2 },
    { 0xfffffffffffffffeULL, 7, 15, 31, 63 },
    { 0xffffffffffffffffULL, 8, 16, 32, 64 },
  };

TEST_STDBIT_UI_TOPLEVEL (stdc_count_ones);

static int
do_test (void)
{
  TEST_STDBIT_UI (stdc_count_ones, inputs);
  return 0;
}

#include <support/test-driver.c>
