/* Test the %m, %#m printf specifiers via asprintf.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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
#include <libc-diag.h>
#include <stdio.h>
#include <support/check.h>
#include <support/support.h>

/* GCC does not yet know about the %#m specifier.  */
DIAG_PUSH_NEEDS_COMMENT;
DIAG_IGNORE_NEEDS_COMMENT (11, "-Wformat=");

static int
do_test (void)
{
  char buf[64];

  errno = EINVAL;
  TEST_COMPARE (sprintf (buf, "%m"), 16);
  TEST_COMPARE_STRING (buf, "Invalid argument");

  errno = EINVAL;
  TEST_COMPARE (sprintf (buf, "%#m"), 6);
  TEST_COMPARE_STRING (buf, "EINVAL");

  errno = 0;
  TEST_COMPARE (sprintf (buf, "%m"), 7);
  TEST_COMPARE_STRING (buf, "Success");

  errno = 0;
  TEST_COMPARE (sprintf (buf, "%#m"), 1);
  TEST_COMPARE_STRING (buf, "0");

  errno = -1;
#ifdef __GNU__
  TEST_COMPARE (sprintf (buf, "%m"), 39);
  TEST_COMPARE_STRING (buf, "Error in unknown error system: FFFFFFFF");
#else
  TEST_COMPARE (sprintf (buf, "%m"), 16);
  TEST_COMPARE_STRING (buf, "Unknown error -1");
#endif

  errno = -1;
  TEST_COMPARE (sprintf (buf, "%#m"), 2);
  TEST_COMPARE_STRING (buf, "-1");

  errno = 1002003;
#ifdef __GNU__
  TEST_COMPARE (sprintf (buf, "%m"), 42);
  TEST_COMPARE_STRING (buf, "(system kern) error with unknown subsystem");
#else
  TEST_COMPARE (sprintf (buf, "%m"), 21);
  TEST_COMPARE_STRING (buf, "Unknown error 1002003");
#endif

  errno = 1002003;
  TEST_COMPARE (sprintf (buf, "%#m"), 7);
  TEST_COMPARE_STRING (buf, "1002003");

  errno = EINVAL;
  TEST_COMPARE (sprintf (buf, "%20m"), 20);
  TEST_COMPARE_STRING (buf, "    Invalid argument");

  errno = EINVAL;
  TEST_COMPARE (sprintf (buf, "%#20m"), 20);
  TEST_COMPARE_STRING (buf, "              EINVAL");

  errno = EINVAL;
  TEST_COMPARE (sprintf (buf, "%-20m"), 20);
  TEST_COMPARE_STRING (buf, "Invalid argument    ");

  errno = EINVAL;
  TEST_COMPARE (sprintf (buf, "%-#20m"), 20);
  TEST_COMPARE_STRING (buf, "EINVAL              ");

  errno = 0;
  TEST_COMPARE (sprintf (buf, "%-20m"), 20);
  TEST_COMPARE_STRING (buf, "Success             ");

  errno = 0;
  TEST_COMPARE (sprintf (buf, "%-#20m"), 20);
  TEST_COMPARE_STRING (buf, "0                   ");

  return 0;
}

#include <support/test-driver.c>
