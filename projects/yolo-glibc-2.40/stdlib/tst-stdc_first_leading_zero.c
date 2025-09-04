/* Test stdc_first_leading_zero functions and macros.
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
    { 0x1ULL, 1, 1, 1, 1 },
    { 0x2ULL, 1, 1, 1, 1 },
    { 0x3ULL, 1, 1, 1, 1 },
    { 0x4ULL, 1, 1, 1, 1 },
    { 0x5ULL, 1, 1, 1, 1 },
    { 0x6ULL, 1, 1, 1, 1 },
    { 0x7ULL, 1, 1, 1, 1 },
    { 0x8ULL, 1, 1, 1, 1 },
    { 0x9ULL, 1, 1, 1, 1 },
    { 0xaULL, 1, 1, 1, 1 },
    { 0xbULL, 1, 1, 1, 1 },
    { 0xcULL, 1, 1, 1, 1 },
    { 0xdULL, 1, 1, 1, 1 },
    { 0xeULL, 1, 1, 1, 1 },
    { 0xfULL, 1, 1, 1, 1 },
    { 0x10ULL, 1, 1, 1, 1 },
    { 0x11ULL, 1, 1, 1, 1 },
    { 0x12ULL, 1, 1, 1, 1 },
    { 0x1fULL, 1, 1, 1, 1 },
    { 0x20ULL, 1, 1, 1, 1 },
    { 0x7fULL, 1, 1, 1, 1 },
    { 0x80ULL, 2, 1, 1, 1 },
    { 0x81ULL, 2, 1, 1, 1 },
    { 0x9aULL, 2, 1, 1, 1 },
    { 0xf3ULL, 5, 1, 1, 1 },
    { 0xffULL, 0, 1, 1, 1 },
    { 0x100ULL, 1, 1, 1, 1 },
    { 0x101ULL, 1, 1, 1, 1 },
    { 0x102ULL, 1, 1, 1, 1 },
    { 0x1feULL, 8, 1, 1, 1 },
    { 0x1ffULL, 0, 1, 1, 1 },
    { 0x200ULL, 1, 1, 1, 1 },
    { 0x234ULL, 1, 1, 1, 1 },
    { 0x4567ULL, 1, 1, 1, 1 },
    { 0x7fffULL, 0, 1, 1, 1 },
    { 0x8000ULL, 1, 2, 1, 1 },
    { 0x8001ULL, 1, 2, 1, 1 },
    { 0xfffeULL, 8, 16, 1, 1 },
    { 0xffffULL, 0, 0, 1, 1 },
    { 0x10000ULL, 1, 1, 1, 1 },
    { 0x10001ULL, 1, 1, 1, 1 },
    { 0xfedcba98ULL, 2, 2, 8, 1 },
    { 0xfffffefeULL, 8, 8, 24, 1 },
    { 0xffffffffULL, 0, 0, 0, 1 },
    { 0x100000000ULL, 1, 1, 1, 1 },
    { 0x100000001ULL, 1, 1, 1, 1 },
    { 0x123456789ULL, 2, 1, 1, 1 },
    { 0x123456789abcdefULL, 4, 3, 2, 1 },
    { 0x789abcdef0123456ULL, 1, 1, 5, 1 },
    { 0x8000000000000000ULL, 1, 1, 1, 2 },
    { 0x8000000000000001ULL, 1, 1, 1, 2 },
    { 0xfffffffffffffffeULL, 8, 16, 32, 64 },
    { 0xffffffffffffffffULL, 0, 0, 0, 0 },
  };

TEST_STDBIT_UI_TOPLEVEL (stdc_first_leading_zero);

static int
do_test (void)
{
  TEST_STDBIT_UI (stdc_first_leading_zero, inputs);
  return 0;
}

#include <support/test-driver.c>
