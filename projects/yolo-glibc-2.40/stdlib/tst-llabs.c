/* Basic tests for llabs.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <limits.h>
#include <stdlib.h>

#include <support/check.h>

#define LARGE_PRIME 49999

static int do_test (void)
{
  long long int i;

  TEST_COMPARE (llabs (LLONG_MAX), LLONG_MAX);
  TEST_COMPARE (llabs (LLONG_MIN + 1), LLONG_MAX);
  TEST_COMPARE (llabs (0x00000000ffffffffL), 0x00000000ffffffffL);
  TEST_COMPARE (llabs (0x0000000100000000L), 0x0000000100000000L);
  TEST_COMPARE (llabs (0x80000000ffffffffL), 0x7fffffff00000001L);
  TEST_COMPARE (llabs (0x8000000100000000L), 0x7fffffff00000000L);
  TEST_COMPARE (llabs (-1), 1);
  TEST_COMPARE (llabs (0), 0);
  TEST_COMPARE (llabs (1), 1);

  for (i = LLONG_MIN + 1; i < LLONG_MIN + INT_MAX; i += LARGE_PRIME)
    TEST_COMPARE (llabs (i), -i);

  for (i = LLONG_MAX - INT_MAX; i < LLONG_MAX - LARGE_PRIME; i += LARGE_PRIME)
    TEST_COMPARE (llabs (i), i);

  for (i = INT_MIN + 1; i < 0; i += LARGE_PRIME)
    TEST_COMPARE (llabs (i), -i);

  for (i = 0; i < INT_MAX; i += LARGE_PRIME)
    TEST_COMPARE (llabs (i), i);

  return EXIT_SUCCESS;
}

#include <support/test-driver.c>
