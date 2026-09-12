/* Test that glob does not overflow the stack with many slashes.
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

#include <glob.h>
#include <string.h>
#include <support/test-driver.h>
#include <support/support.h>
#include <support/check.h>
#include <sys/resource.h>

/* Allocation size.  */
#define SIZE 4096

/* This test would cause a stack overflow on glibc 2.42 and earlier.  */
#define MAX_STACK_SIZE (8 * 1024 * 1024)

static int
do_test (void)
{
  struct rlimit stack_limit = { MAX_STACK_SIZE, MAX_STACK_SIZE };
  TEST_VERIFY_EXIT (setrlimit (RLIMIT_STACK, &stack_limit) == 0);
  glob_t g;
  char *pattern = xmalloc (SIZE);

  /* Test based on BZ #30635.  "/////////".  */
  memset (pattern, '/', SIZE - 1);
  pattern[SIZE - 1] = '\0';
  TEST_VERIFY (glob (pattern, 0, NULL, &g) == 0);
  globfree (&g);

  /* Test based on BZ #33537.  "/*/////sh".  */
  pattern[1] = '*';
  pattern[SIZE - 3] = 's';
  pattern[SIZE - 2] = 'h';
  TEST_VERIFY (glob (pattern, 0, NULL, &g) == 0);
  globfree (&g);

  return 0;
}

#include <support/test-driver.c>
