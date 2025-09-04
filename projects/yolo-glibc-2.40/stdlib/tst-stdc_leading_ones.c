/* Test stdc_leading_ones functions and macros.
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
    { 0x1ULL, 0, 0, 0, 0 },
    { 0x2ULL, 0, 0, 0, 0 },
    { 0x3ULL, 0, 0, 0, 0 },
    { 0x4ULL, 0, 0, 0, 0 },
    { 0x5ULL, 0, 0, 0, 0 },
    { 0x6ULL, 0, 0, 0, 0 },
    { 0x7ULL, 0, 0, 0, 0 },
    { 0x8ULL, 0, 0, 0, 0 },
    { 0x9ULL, 0, 0, 0, 0 },
    { 0xaULL, 0, 0, 0, 0 },
    { 0xbULL, 0, 0, 0, 0 },
    { 0xcULL, 0, 0, 0, 0 },
    { 0xdULL, 0, 0, 0, 0 },
    { 0xeULL, 0, 0, 0, 0 },
    { 0xfULL, 0, 0, 0, 0 },
    { 0x10ULL, 0, 0, 0, 0 },
    { 0x11ULL, 0, 0, 0, 0 },
    { 0x12ULL, 0, 0, 0, 0 },
    { 0x1fULL, 0, 0, 0, 0 },
    { 0x20ULL, 0, 0, 0, 0 },
    { 0x7fULL, 0, 0, 0, 0 },
    { 0x80ULL, 1, 0, 0, 0 },
    { 0x81ULL, 1, 0, 0, 0 },
    { 0x9aULL, 1, 0, 0, 0 },
    { 0xf3ULL, 4, 0, 0, 0 },
    { 0xffULL, 8, 0, 0, 0 },
    { 0x100ULL, 0, 0, 0, 0 },
    { 0x101ULL, 0, 0, 0, 0 },
    { 0x102ULL, 0, 0, 0, 0 },
    { 0x1feULL, 7, 0, 0, 0 },
    { 0x1ffULL, 8, 0, 0, 0 },
    { 0x200ULL, 0, 0, 0, 0 },
    { 0x234ULL, 0, 0, 0, 0 },
    { 0x4567ULL, 0, 0, 0, 0 },
    { 0x7fffULL, 8, 0, 0, 0 },
    { 0x8000ULL, 0, 1, 0, 0 },
    { 0x8001ULL, 0, 1, 0, 0 },
    { 0xfffeULL, 7, 15, 0, 0 },
    { 0xffffULL, 8, 16, 0, 0 },
    { 0x10000ULL, 0, 0, 0, 0 },
    { 0x10001ULL, 0, 0, 0, 0 },
    { 0xfedcba98ULL, 1, 1, 7, 0 },
    { 0xfffffefeULL, 7, 7, 23, 0 },
    { 0xffffffffULL, 8, 16, 32, 0 },
    { 0x100000000ULL, 0, 0, 0, 0 },
    { 0x100000001ULL, 0, 0, 0, 0 },
    { 0x123456789ULL, 1, 0, 0, 0 },
    { 0x123456789abcdefULL, 3, 2, 1, 0 },
    { 0x789abcdef0123456ULL, 0, 0, 4, 0 },
    { 0x8000000000000000ULL, 0, 0, 0, 1 },
    { 0x8000000000000001ULL, 0, 0, 0, 1 },
    { 0xfffffffffffffffeULL, 7, 15, 31, 63 },
    { 0xffffffffffffffffULL, 8, 16, 32, 64 },
  };

TEST_STDBIT_UI_TOPLEVEL (stdc_leading_ones);

static int
do_test (void)
{
  TEST_STDBIT_UI (stdc_leading_ones, inputs);
  return 0;
}

#include <support/test-driver.c>
