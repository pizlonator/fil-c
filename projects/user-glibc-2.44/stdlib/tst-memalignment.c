/* Test memalignment.
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

#include <stdlib.h>
#include <array_length.h>
#include <support/check.h>

static int
do_test (void)
{
  void *null = NULL;
  TEST_COMPARE (memalignment (NULL), 0);
  TEST_COMPARE (memalignment (null), 0);
  char ca[256];
  array_foreach (p, ca)
    TEST_VERIFY (memalignment (p) >= 1);
  TEST_VERIFY (memalignment (&ca[0]) == 1 || memalignment (&ca[1]) == 1);
  TEST_VERIFY (memalignment (&ca[0]) == 2 || memalignment (&ca[1]) == 2
	       || memalignment (&ca[2]) == 2 || memalignment (&ca[3]) == 2);
  long long int lla[256];
  array_foreach (p, lla)
    TEST_VERIFY (memalignment (p) >= _Alignof (long long int));
  TEST_VERIFY (memalignment (&lla[0]) <= sizeof (long long int)
	       || memalignment (&lla[1]) <= sizeof (long long int));
  return 0;
}

#include <support/test-driver.c>
