/* Test that asprintf sets the buffer pointer to NULL on failure.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <stdio.h>
#include <support/check.h>
#include <sys/resource.h>

static int
do_test (void)
{
  static const char sentinel[] = "sentinel";
  char *buf = (char *) sentinel;
  {
    /* Avoid -Wformat-overflow warning.  */
    const char *volatile format = "%2000000000d %2000000000d";
    TEST_COMPARE (asprintf (&buf, format, 1, 2), -1);
  }
  if (errno != ENOMEM)
    TEST_COMPARE (errno, EOVERFLOW);
  TEST_VERIFY (buf == NULL);

  /* Force ENOMEM in the test below.  */
  struct rlimit rl;
  TEST_COMPARE (getrlimit (RLIMIT_AS, &rl), 0);
  rl.rlim_cur = 10 * 1024 * 1024;
  TEST_COMPARE (setrlimit (RLIMIT_AS, &rl), 0);

  buf = (char *) sentinel;
  TEST_COMPARE (asprintf (&buf, "%20000000d", 1), -1);
  TEST_COMPARE (errno, ENOMEM);
  TEST_VERIFY (buf == NULL);
  return 0;
}

#include <support/test-driver.c>
