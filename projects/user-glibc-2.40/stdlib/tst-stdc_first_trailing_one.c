/* Test stdc_first_trailing_one functions and macros.
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
    { 0x3ULL, 1, 1, 1, 1 },
    { 0x4ULL, 3, 3, 3, 3 },
    { 0x5ULL, 1, 1, 1, 1 },
    { 0x6ULL, 2, 2, 2, 2 },
    { 0x7ULL, 1, 1, 1, 1 },
    { 0x8ULL, 4, 4, 4, 4 },
    { 0x9ULL, 1, 1, 1, 1 },
    { 0xaULL, 2, 2, 2, 2 },
    { 0xbULL, 1, 1, 1, 1 },
    { 0xcULL, 3, 3, 3, 3 },
    { 0xdULL, 1, 1, 1, 1 },
    { 0xeULL, 2, 2, 2, 2 },
    { 0xfULL, 1, 1, 1, 1 },
    { 0x10ULL, 5, 5, 5, 5 },
    { 0x11ULL, 1, 1, 1, 1 },
    { 0x12ULL, 2, 2, 2, 2 },
    { 0x1fULL, 1, 1, 1, 1 },
    { 0x20ULL, 6, 6, 6, 6 },
    { 0x7fULL, 1, 1, 1, 1 },
    { 0x80ULL, 8, 8, 8, 8 },
    { 0x81ULL, 1, 1, 1, 1 },
    { 0x9aULL, 2, 2, 2, 2 },
    { 0xf3ULL, 1, 1, 1, 1 },
    { 0xffULL, 1, 1, 1, 1 },
    { 0x100ULL, 0, 9, 9, 9 },
    { 0x101ULL, 1, 1, 1, 1 },
    { 0x102ULL, 2, 2, 2, 2 },
    { 0x1feULL, 2, 2, 2, 2 },
    { 0x1ffULL, 1, 1, 1, 1 },
    { 0x200ULL, 0, 10, 10, 10 },
    { 0x234ULL, 3, 3, 3, 3 },
    { 0x4567ULL, 1, 1, 1, 1 },
    { 0x7fffULL, 1, 1, 1, 1 },
    { 0x8000ULL, 0, 16, 16, 16 },
    { 0x8001ULL, 1, 1, 1, 1 },
    { 0xfffeULL, 2, 2, 2, 2 },
    { 0xffffULL, 1, 1, 1, 1 },
    { 0x10000ULL, 0, 0, 17, 17 },
    { 0x10001ULL, 1, 1, 1, 1 },
    { 0xfedcba98ULL, 4, 4, 4, 4 },
    { 0xfffffefeULL, 2, 2, 2, 2 },
    { 0xffffffffULL, 1, 1, 1, 1 },
    { 0x100000000ULL, 0, 0, 0, 33 },
    { 0x100000001ULL, 1, 1, 1, 1 },
    { 0x123456789ULL, 1, 1, 1, 1 },
    { 0x123456789abcdefULL, 1, 1, 1, 1 },
    { 0x789abcdef0123456ULL, 2, 2, 2, 2 },
    { 0x8000000000000000ULL, 0, 0, 0, 64 },
    { 0x8000000000000001ULL, 1, 1, 1, 1 },
    { 0xfffffffffffffffeULL, 2, 2, 2, 2 },
    { 0xffffffffffffffffULL, 1, 1, 1, 1 },
  };

TEST_STDBIT_UI_TOPLEVEL (stdc_first_trailing_one);

static int
do_test (void)
{
  TEST_STDBIT_UI (stdc_first_trailing_one, inputs);
  return 0;
}

#include <support/test-driver.c>
