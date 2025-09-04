/* Test stdc_first_trailing_zero functions and macros.
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
    { 0ULL, 1, 1, 1, 1 },
    { 0x1ULL, 2, 2, 2, 2 },
    { 0x2ULL, 1, 1, 1, 1 },
    { 0x3ULL, 3, 3, 3, 3 },
    { 0x4ULL, 1, 1, 1, 1 },
    { 0x5ULL, 2, 2, 2, 2 },
    { 0x6ULL, 1, 1, 1, 1 },
    { 0x7ULL, 4, 4, 4, 4 },
    { 0x8ULL, 1, 1, 1, 1 },
    { 0x9ULL, 2, 2, 2, 2 },
    { 0xaULL, 1, 1, 1, 1 },
    { 0xbULL, 3, 3, 3, 3 },
    { 0xcULL, 1, 1, 1, 1 },
    { 0xdULL, 2, 2, 2, 2 },
    { 0xeULL, 1, 1, 1, 1 },
    { 0xfULL, 5, 5, 5, 5 },
    { 0x10ULL, 1, 1, 1, 1 },
    { 0x11ULL, 2, 2, 2, 2 },
    { 0x12ULL, 1, 1, 1, 1 },
    { 0x1fULL, 6, 6, 6, 6 },
    { 0x20ULL, 1, 1, 1, 1 },
    { 0x7fULL, 8, 8, 8, 8 },
    { 0x80ULL, 1, 1, 1, 1 },
    { 0x81ULL, 2, 2, 2, 2 },
    { 0x9aULL, 1, 1, 1, 1 },
    { 0xf3ULL, 3, 3, 3, 3 },
    { 0xffULL, 0, 9, 9, 9 },
    { 0x100ULL, 1, 1, 1, 1 },
    { 0x101ULL, 2, 2, 2, 2 },
    { 0x102ULL, 1, 1, 1, 1 },
    { 0x1feULL, 1, 1, 1, 1 },
    { 0x1ffULL, 0, 10, 10, 10 },
    { 0x200ULL, 1, 1, 1, 1 },
    { 0x234ULL, 1, 1, 1, 1 },
    { 0x4567ULL, 4, 4, 4, 4 },
    { 0x7fffULL, 0, 16, 16, 16 },
    { 0x8000ULL, 1, 1, 1, 1 },
    { 0x8001ULL, 2, 2, 2, 2 },
    { 0xfffeULL, 1, 1, 1, 1 },
    { 0xffffULL, 0, 0, 17, 17 },
    { 0x10000ULL, 1, 1, 1, 1 },
    { 0x10001ULL, 2, 2, 2, 2 },
    { 0xfedcba98ULL, 1, 1, 1, 1 },
    { 0xfffffefeULL, 1, 1, 1, 1 },
    { 0xffffffffULL, 0, 0, 0, 33 },
    { 0x100000000ULL, 1, 1, 1, 1 },
    { 0x100000001ULL, 2, 2, 2, 2 },
    { 0x123456789ULL, 2, 2, 2, 2 },
    { 0x123456789abcdefULL, 5, 5, 5, 5 },
    { 0x789abcdef0123456ULL, 1, 1, 1, 1 },
    { 0x8000000000000000ULL, 1, 1, 1, 1 },
    { 0x8000000000000001ULL, 2, 2, 2, 2 },
    { 0xfffffffffffffffeULL, 1, 1, 1, 1 },
    { 0xffffffffffffffffULL, 0, 0, 0, 0 },
  };

TEST_STDBIT_UI_TOPLEVEL (stdc_first_trailing_zero);

static int
do_test (void)
{
  TEST_STDBIT_UI (stdc_first_trailing_zero, inputs);
  return 0;
}

#include <support/test-driver.c>
