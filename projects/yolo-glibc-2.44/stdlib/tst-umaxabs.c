/* Basic tests for umaxabs.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>

#include <support/check.h>

#define LARGE_PRIME 49999

static int do_test (void)
{
  intmax_t i;

  TEST_COMPARE (umaxabs (INTMAX_MIN), (uintmax_t)INTMAX_MAX + 1);
  TEST_COMPARE (umaxabs (-1), 1);
  TEST_COMPARE (umaxabs (0), 0);
  TEST_COMPARE (umaxabs (1), 1);

  for (i = INTMAX_MIN + 1; i < INTMAX_MIN + INT_MAX; i += LARGE_PRIME)
    TEST_COMPARE (umaxabs (i), -i);

  for (i = INTMAX_MAX - INT_MAX; i < INTMAX_MAX - LARGE_PRIME; i += LARGE_PRIME)
    TEST_COMPARE (umaxabs (i), i);

  for (i = INT_MIN + 1; i < 0; i += LARGE_PRIME)
    TEST_COMPARE (umaxabs (i), -i);

  for (i = 0; i < INT_MAX; i += LARGE_PRIME)
    TEST_COMPARE (umaxabs (i), i);

  return EXIT_SUCCESS;
}

#include <support/test-driver.c>
