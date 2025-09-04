/* Test stdc_trailing_zeros functions and macros.
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
    { 0x1ULL, 0, 0, 0, 0 },
    { 0x2ULL, 1, 1, 1, 1 },
    { 0x3ULL, 0, 0, 0, 0 },
    { 0x4ULL, 2, 2, 2, 2 },
    { 0x5ULL, 0, 0, 0, 0 },
    { 0x6ULL, 1, 1, 1, 1 },
    { 0x7ULL, 0, 0, 0, 0 },
    { 0x8ULL, 3, 3, 3, 3 },
    { 0x9ULL, 0, 0, 0, 0 },
    { 0xaULL, 1, 1, 1, 1 },
    { 0xbULL, 0, 0, 0, 0 },
    { 0xcULL, 2, 2, 2, 2 },
    { 0xdULL, 0, 0, 0, 0 },
    { 0xeULL, 1, 1, 1, 1 },
    { 0xfULL, 0, 0, 0, 0 },
    { 0x10ULL, 4, 4, 4, 4 },
    { 0x11ULL, 0, 0, 0, 0 },
    { 0x12ULL, 1, 1, 1, 1 },
    { 0x1fULL, 0, 0, 0, 0 },
    { 0x20ULL, 5, 5, 5, 5 },
    { 0x7fULL, 0, 0, 0, 0 },
    { 0x80ULL, 7, 7, 7, 7 },
    { 0x81ULL, 0, 0, 0, 0 },
    { 0x9aULL, 1, 1, 1, 1 },
    { 0xf3ULL, 0, 0, 0, 0 },
    { 0xffULL, 0, 0, 0, 0 },
    { 0x100ULL, 8, 8, 8, 8 },
    { 0x101ULL, 0, 0, 0, 0 },
    { 0x102ULL, 1, 1, 1, 1 },
    { 0x1feULL, 1, 1, 1, 1 },
    { 0x1ffULL, 0, 0, 0, 0 },
    { 0x200ULL, 8, 9, 9, 9 },
    { 0x234ULL, 2, 2, 2, 2 },
    { 0x4567ULL, 0, 0, 0, 0 },
    { 0x7fffULL, 0, 0, 0, 0 },
    { 0x8000ULL, 8, 15, 15, 15 },
    { 0x8001ULL, 0, 0, 0, 0 },
    { 0xfffeULL, 1, 1, 1, 1 },
    { 0xffffULL, 0, 0, 0, 0 },
    { 0x10000ULL, 8, 16, 16, 16 },
    { 0x10001ULL, 0, 0, 0, 0 },
    { 0xfedcba98ULL, 3, 3, 3, 3 },
    { 0xfffffefeULL, 1, 1, 1, 1 },
    { 0xffffffffULL, 0, 0, 0, 0 },
    { 0x100000000ULL, 8, 16, 32, 32 },
    { 0x100000001ULL, 0, 0, 0, 0 },
    { 0x123456789ULL, 0, 0, 0, 0 },
    { 0x123456789abcdefULL, 0, 0, 0, 0 },
    { 0x789abcdef0123456ULL, 1, 1, 1, 1 },
    { 0x8000000000000000ULL, 8, 16, 32, 63 },
    { 0x8000000000000001ULL, 0, 0, 0, 0 },
    { 0xfffffffffffffffeULL, 1, 1, 1, 1 },
    { 0xffffffffffffffffULL, 0, 0, 0, 0 },
  };

TEST_STDBIT_UI_TOPLEVEL (stdc_trailing_zeros);

static int
do_test (void)
{
  TEST_STDBIT_UI (stdc_trailing_zeros, inputs);
  return 0;
}

#include <support/test-driver.c>
