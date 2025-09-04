/* Test stdc_bit_width functions and macros.
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
    { 0x2ULL, 2, 2, 2, 2 },
    { 0x3ULL, 2, 2, 2, 2 },
    { 0x4ULL, 3, 3, 3, 3 },
    { 0x5ULL, 3, 3, 3, 3 },
    { 0x6ULL, 3, 3, 3, 3 },
    { 0x7ULL, 3, 3, 3, 3 },
    { 0x8ULL, 4, 4, 4, 4 },
    { 0x9ULL, 4, 4, 4, 4 },
    { 0xaULL, 4, 4, 4, 4 },
    { 0xbULL, 4, 4, 4, 4 },
    { 0xcULL, 4, 4, 4, 4 },
    { 0xdULL, 4, 4, 4, 4 },
    { 0xeULL, 4, 4, 4, 4 },
    { 0xfULL, 4, 4, 4, 4 },
    { 0x10ULL, 5, 5, 5, 5 },
    { 0x11ULL, 5, 5, 5, 5 },
    { 0x12ULL, 5, 5, 5, 5 },
    { 0x1fULL, 5, 5, 5, 5 },
    { 0x20ULL, 6, 6, 6, 6 },
    { 0x7fULL, 7, 7, 7, 7 },
    { 0x80ULL, 8, 8, 8, 8 },
    { 0x81ULL, 8, 8, 8, 8 },
    { 0x9aULL, 8, 8, 8, 8 },
    { 0xf3ULL, 8, 8, 8, 8 },
    { 0xffULL, 8, 8, 8, 8 },
    { 0x100ULL, 0, 9, 9, 9 },
    { 0x101ULL, 1, 9, 9, 9 },
    { 0x102ULL, 2, 9, 9, 9 },
    { 0x1feULL, 8, 9, 9, 9 },
    { 0x1ffULL, 8, 9, 9, 9 },
    { 0x200ULL, 0, 10, 10, 10 },
    { 0x234ULL, 6, 10, 10, 10 },
    { 0x4567ULL, 7, 15, 15, 15 },
    { 0x7fffULL, 8, 15, 15, 15 },
    { 0x8000ULL, 0, 16, 16, 16 },
    { 0x8001ULL, 1, 16, 16, 16 },
    { 0xfffeULL, 8, 16, 16, 16 },
    { 0xffffULL, 8, 16, 16, 16 },
    { 0x10000ULL, 0, 0, 17, 17 },
    { 0x10001ULL, 1, 1, 17, 17 },
    { 0xfedcba98ULL, 8, 16, 32, 32 },
    { 0xfffffefeULL, 8, 16, 32, 32 },
    { 0xffffffffULL, 8, 16, 32, 32 },
    { 0x100000000ULL, 0, 0, 0, 33 },
    { 0x100000001ULL, 1, 1, 1, 33 },
    { 0x123456789ULL, 8, 15, 30, 33 },
    { 0x123456789abcdefULL, 8, 16, 32, 57 },
    { 0x789abcdef0123456ULL, 7, 14, 32, 63 },
    { 0x8000000000000000ULL, 0, 0, 0, 64 },
    { 0x8000000000000001ULL, 1, 1, 1, 64 },
    { 0xfffffffffffffffeULL, 8, 16, 32, 64 },
    { 0xffffffffffffffffULL, 8, 16, 32, 64 },
  };

TEST_STDBIT_UI_TOPLEVEL (stdc_bit_width);

static int
do_test (void)
{
  TEST_STDBIT_UI (stdc_bit_width, inputs);
  return 0;
}

#include <support/test-driver.c>
