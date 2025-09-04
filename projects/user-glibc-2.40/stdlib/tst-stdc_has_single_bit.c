/* Test stdc_has_single_bit functions and macros.
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
    { 0ULL, false, false, false, false },
    { 0x1ULL, true, true, true, true },
    { 0x2ULL, true, true, true, true },
    { 0x3ULL, false, false, false, false },
    { 0x4ULL, true, true, true, true },
    { 0x5ULL, false, false, false, false },
    { 0x6ULL, false, false, false, false },
    { 0x7ULL, false, false, false, false },
    { 0x8ULL, true, true, true, true },
    { 0x9ULL, false, false, false, false },
    { 0xaULL, false, false, false, false },
    { 0xbULL, false, false, false, false },
    { 0xcULL, false, false, false, false },
    { 0xdULL, false, false, false, false },
    { 0xeULL, false, false, false, false },
    { 0xfULL, false, false, false, false },
    { 0x10ULL, true, true, true, true },
    { 0x11ULL, false, false, false, false },
    { 0x12ULL, false, false, false, false },
    { 0x1fULL, false, false, false, false },
    { 0x20ULL, true, true, true, true },
    { 0x7fULL, false, false, false, false },
    { 0x80ULL, true, true, true, true },
    { 0x81ULL, false, false, false, false },
    { 0x9aULL, false, false, false, false },
    { 0xf3ULL, false, false, false, false },
    { 0xffULL, false, false, false, false },
    { 0x100ULL, false, true, true, true },
    { 0x101ULL, true, false, false, false },
    { 0x102ULL, true, false, false, false },
    { 0x1feULL, false, false, false, false },
    { 0x1ffULL, false, false, false, false },
    { 0x200ULL, false, true, true, true },
    { 0x234ULL, false, false, false, false },
    { 0x4567ULL, false, false, false, false },
    { 0x7fffULL, false, false, false, false },
    { 0x8000ULL, false, true, true, true },
    { 0x8001ULL, true, false, false, false },
    { 0xfffeULL, false, false, false, false },
    { 0xffffULL, false, false, false, false },
    { 0x10000ULL, false, false, true, true },
    { 0x10001ULL, true, true, false, false },
    { 0xfedcba98ULL, false, false, false, false },
    { 0xfffffefeULL, false, false, false, false },
    { 0xffffffffULL, false, false, false, false },
    { 0x100000000ULL, false, false, false, true },
    { 0x100000001ULL, true, true, true, false },
    { 0x123456789ULL, false, false, false, false },
    { 0x123456789abcdefULL, false, false, false, false },
    { 0x789abcdef0123456ULL, false, false, false, false },
    { 0x8000000000000000ULL, false, false, false, true },
    { 0x8000000000000001ULL, true, true, true, false },
    { 0xfffffffffffffffeULL, false, false, false, false },
    { 0xffffffffffffffffULL, false, false, false, false },
  };

TEST_STDBIT_BOOL_TOPLEVEL (stdc_has_single_bit);

static int
do_test (void)
{
  TEST_STDBIT_BOOL (stdc_has_single_bit, inputs);
  return 0;
}

#include <support/test-driver.c>
